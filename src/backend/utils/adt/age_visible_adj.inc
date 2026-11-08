/* Experimental bridge: AGE adjacency storage + common PGQ endpoint runtime.
 * Reads the SAME PGQ heap relations, not the separately imported Cypher graph.
 * No graphid relabelling, path construction or traversal algorithm here.
 * Only a superuser, permanent heap tables, RR READ ONLY, non-subtransaction.
 * Private TopTransactionContext ownership: never borrow VLE global pointers.
 */
typedef struct AgeVisibleCache
{
    GRAPH_global_context graph;
    GgcFixedStagePlan mapping;
    Snapshot snapshot;             /* FirstXactSnapshot, registered by PG */
    CommandId cid;
    Oid role;
    int security;
    MemoryContext memory;
    MemoryContextCallback cleanup;
} AgeVisibleCache;

static AgeVisibleCache *age_visible_cache;
#define AGE_VISIBLE_MAX_BYTES ((Size) 128 * 1024 * 1024)
#define AGE_VISIBLE_MAX_ROWS UINT64CONST(1000000)

static void
age_visible_reset(void *arg)
{
    if (age_visible_cache == arg)
        age_visible_cache = NULL;
}

static bool
age_visible_same_snapshot(Snapshot a, Snapshot b)
{
    return a && b && a->snapshot_type == SNAPSHOT_MVCC &&
        b->snapshot_type == SNAPSHOT_MVCC &&
        a->xmin == b->xmin && a->xmax == b->xmax && a->curcid == b->curcid &&
        a->xcnt == b->xcnt && a->subxcnt == b->subxcnt &&
        a->suboverflowed == b->suboverflowed &&
        !a->takenDuringRecovery && !b->takenDuringRecovery &&
        (!a->xcnt || memcmp(a->xip, b->xip, a->xcnt * sizeof(TransactionId)) == 0) &&
        (!a->subxcnt || memcmp(a->subxip, b->subxip, a->subxcnt * sizeof(TransactionId)) == 0);
}

static void
age_visible_check_scope(Snapshot snapshot)
{
    Oid role;
    int security;
    GetUserIdAndSecContext(&role, &security);
    if (!superuser() || security != 0 || !XactReadOnly ||
        XactIsoLevel != XACT_REPEATABLE_READ || IsSubTransaction() ||
        TransactionIdIsValid(GetTopTransactionIdIfAny()) ||
        !age_visible_same_snapshot(snapshot, GetTransactionSnapshot()))
        ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
            errmsg("AGE visible adjacency requires superuser, unchanged REPEATABLE READ READ ONLY snapshot, no writes or subtransactions")));
    if (age_visible_cache &&
        (age_visible_cache->snapshot != GetTransactionSnapshot() ||
         age_visible_cache->cid != GetCurrentCommandId(false) ||
         age_visible_cache->role != role || age_visible_cache->security != security))
        elog(ERROR, "AGE visible adjacency transaction scope changed");
}

static Relation
age_visible_relation(Oid oid)
{
    Relation rel = table_open(oid, AccessShareLock);
    if (rel->rd_rel->relkind != RELKIND_RELATION ||
        rel->rd_rel->relpersistence != RELPERSISTENCE_PERMANENT ||
        rel->rd_rel->relam != HEAP_TABLE_AM_OID || rel->rd_rel->relrowsecurity ||
        rel->rd_rel->relforcerowsecurity || rel->rd_rel->relhassubclass)
        ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
            errmsg("AGE visible adjacency requires a permanent non-inherited heap without RLS")));
    return rel;
}

static void
age_visible_attribute(Relation rel, AttrNumber att)
{
    TupleDesc desc = RelationGetDescr(rel);
    if (att <= 0 || att > desc->natts ||
        TupleDescAttr(desc, att-1)->attisdropped ||
        TupleDescAttr(desc, att-1)->atttypid != INT8OID)
        elog(ERROR, "AGE visible adjacency requires plain bigint key attributes");
}

static int64
age_visible_key(HeapTuple tuple, Relation rel, AttrNumber att)
{
    bool isnull;
    Datum value = heap_getattr(tuple, att, RelationGetDescr(rel), &isnull);
    if (isnull)
        ereport(ERROR, (errcode(ERRCODE_NULL_VALUE_NOT_ALLOWED),
            errmsg("AGE visible adjacency prototype requires nonnull keys")));
    return DatumGetInt64(value);
}

static void
age_visible_budget(AgeVisibleCache *cache, uint64 rows)
{
    CHECK_FOR_INTERRUPTS();
    if (rows > AGE_VISIBLE_MAX_ROWS ||
        (rows % 1024 == 0 && MemoryContextMemAllocated(cache->memory, true) > AGE_VISIBLE_MAX_BYTES))
        ereport(ERROR, (errcode(ERRCODE_PROGRAM_LIMIT_EXCEEDED),
            errmsg("AGE visible adjacency cache budget exceeded (128 MiB / 1M rows)")));
}

static void *
age_visible_open(EState *estate, const GgcFixedStagePlan *stage)
{
    MemoryContext old, memory;
    AgeVisibleCache *cache;
    Relation vertices, edges;
    TableScanDesc scan;
    HeapTuple tuple;
    uint64 rows = 0;
    Snapshot snapshot = estate->es_snapshot;

    age_visible_check_scope(snapshot);
    if (age_visible_cache)
    {
        GgcFixedStagePlan *m = &age_visible_cache->mapping;
        if (stage->edge_relid != m->edge_relid || stage->vertex_relid != m->vertex_relid ||
            stage->edge_identity_attnum != m->edge_identity_attnum ||
            stage->source_attnum != m->source_attnum || stage->destination_attnum != m->destination_attnum ||
            stage->vertex_key_attnum != m->vertex_key_attnum)
            elog(ERROR, "AGE visible adjacency prototype permits one relation mapping per transaction");
        return age_visible_cache;
    }
    vertices = age_visible_relation(stage->vertex_relid);
    edges = age_visible_relation(stage->edge_relid);
    age_visible_attribute(vertices, stage->vertex_key_attnum);
    age_visible_attribute(edges, stage->edge_identity_attnum);
    age_visible_attribute(edges, stage->source_attnum);
    age_visible_attribute(edges, stage->destination_attnum);
    memory = AllocSetContextCreate(TopTransactionContext, "AGE PGQ visible adjacency", ALLOCSET_DEFAULT_SIZES);
    old = MemoryContextSwitchTo(memory);
    cache = palloc0(sizeof(*cache));
    cache->memory = memory;
    cache->mapping = *stage;
    cache->snapshot = GetTransactionSnapshot();
    cache->cid = GetCurrentCommandId(false);
    GetUserIdAndSecContext(&cache->role, &cache->security);
    cache->cleanup.func = age_visible_reset;
    cache->cleanup.arg = cache;
    MemoryContextRegisterResetCallback(memory, &cache->cleanup);
    cache->graph.graph_name = pstrdup("PGQ transaction snapshot");
    create_GRAPH_global_hashtables(&cache->graph);

    scan = table_beginscan(vertices, snapshot, 0, NULL, 0);
    while ((tuple = heap_getnext(scan, ForwardScanDirection)) != NULL)
    {
        int64 id = age_visible_key(tuple, vertices, stage->vertex_key_attnum);
        age_visible_budget(cache, ++rows);
        if (!insert_vertex_entry(&cache->graph, id, stage->vertex_relid, tuple->t_self))
            elog(ERROR, "AGE visible adjacency found duplicate vertex keys");
    }
    table_endscan(scan);
    scan = table_beginscan(edges, snapshot, 0, NULL, 0);
    while ((tuple = heap_getnext(scan, ForwardScanDirection)) != NULL)
    {
        int64 id = age_visible_key(tuple, edges, stage->edge_identity_attnum);
        int64 src = age_visible_key(tuple, edges, stage->source_attnum);
        int64 dst = age_visible_key(tuple, edges, stage->destination_attnum);
        age_visible_budget(cache, ++rows);
        if (!insert_edge_entry(&cache->graph, id, tuple->t_self, src, dst, stage->edge_relid))
            elog(ERROR, "AGE visible adjacency found duplicate edge keys");
        /* Inner vertex joins discard edges with a missing visible endpoint.
         * Do not let a dangling vertex become a traversable frontier node. */
        if (get_vertex_entry(&cache->graph, src) &&
            get_vertex_entry(&cache->graph, dst))
            (void) insert_vertex_edge(&cache->graph, src, dst, id, "PGQ");
    }
    table_endscan(scan);
    freeze_GRAPH_global_hashtables(&cache->graph);
    age_visible_budget(cache, 0);
    /* Keep the locks until transaction end, including warm statement reuse. */
    table_close(edges, NoLock);
    table_close(vertices, NoLock);
    MemoryContextSwitchTo(old);
    age_visible_cache = cache;      /* Publish only a complete frozen cache. */
    return cache;
}

/* The caller certifies the scope once per synchronous, bounded visit. The
 * common endpoint consumer only accounts and deduplicates keys; it cannot
 * execute user expressions or change the snapshot/role mid-batch. */
static void
age_visible_emit_neighbors(AgeVisibleCache *cache, int64 endpoint,
                           GgcDirection direction, GgcVisibleEmit emit, void *arg)
{
    vertex_entry *vertex;
    VertexEdgeArray *lists[2];
    int k;
    CHECK_FOR_INTERRUPTS();
    vertex = get_vertex_entry(&cache->graph, endpoint);
    if (!vertex)
        return;
    lists[0] = direction == GGC_OUT ? &vertex->edges_out : &vertex->edges_in;
    lists[1] = &vertex->edges_self;
    for (k = 0; k < 2; k++)
    {
        int32 i;
        for (i = 0; i < lists[k]->size; i++)
        {
            edge_entry *edge = get_edge_entry(&cache->graph, lists[k]->array[i]);
            CHECK_FOR_INTERRUPTS();
            if (!edge)
                elog(ERROR, "AGE visible adjacency contains a missing edge");
            emit(arg, direction == GGC_OUT ? edge->end_vertex_id : edge->start_vertex_id);
        }
    }
}

static void
age_visible_visit_batch(void *context, const int64 *endpoints, uint32 count,
                        GgcDirection direction, GgcVisibleEmit emit, void *arg)
{
    uint32 i;

    if (!context || context != age_visible_cache || !endpoints || !emit ||
        !count || count > GGC_VISIBLE_ADJ_BATCH_MAX ||
        (direction != GGC_OUT && direction != GGC_IN))
        elog(ERROR, "invalid AGE visible adjacency batch");
    age_visible_check_scope(GetActiveSnapshot());
    for (i = 0; i < count; i++)
        age_visible_emit_neighbors(context, endpoints[i], direction, emit, arg);
}

static void
age_visible_visit(void *context, int64 endpoint, GgcDirection direction,
                  GgcVisibleEmit emit, void *arg)
{
    age_visible_visit_batch(context, &endpoint, 1, direction, emit, arg);
}

void
age_visible_adj_init(void)
{
    static const GgcVisibleAdjOps ops = {GGC_VISIBLE_ADJ_ABI, sizeof(GgcVisibleAdjOps),
        "AGE transaction-private adjacency (experimental)", age_visible_open,
        age_visible_visit, age_visible_visit_batch};
    void **slot = find_rendezvous_variable(GGC_VISIBLE_ADJ_RENDEZVOUS);
    if (*slot && *slot != &ops)
        elog(ERROR, "visible adjacency provider already registered");
    *slot = (void *) &ops;
}
