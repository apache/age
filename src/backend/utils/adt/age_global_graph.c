/*
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 */

#include "postgres.h"

#include "fmgr.h"

#include "access/heapam.h"
#include "access/htup_details.h"
#include "access/xact.h"
#include "catalog/namespace.h"
#include "commands/trigger.h"
#include "common/hashfn.h"
#include "commands/label_commands.h"
#include "port/atomics.h"
#include "storage/lwlock.h"
#include "utils/datum.h"
#include "utils/lsyscache.h"
#include "utils/memutils.h"
#include "utils/snapmgr.h"
#include "utils/builtins.h"
#include "executor/spi.h"
#include "lib/stringinfo.h"
#include "miscadmin.h"
#include "utils/rls.h"
#include "utils/acl.h"
#include "utils/inval.h"
#include "utils/syscache.h"

#if PG_VERSION_NUM >= 170000
#include "storage/dsm_registry.h"
#else
#include "storage/ipc.h"
#include "storage/shmem.h"
#endif

#include "utils/age_global_graph.h"
#include "utils/agehash.h"
#include "catalog/ag_graph.h"
#include "catalog/ag_label.h"
#include "utils/ag_cache.h"
#include "utils/ag_guc.h"


/* defines */
#define VERTEX_HTAB_NAME "Vertex to edge lists " /* added a space at end for */
#define VERTEX_HTAB_INITIAL_SIZE 10000
#define EDGE_HTAB_INITIAL_SIZE 10000

/*
 * Maximum number of graphs tracked for version counting.
 *
 * There is no hard limit behind this number. An entry is 16 bytes, so the whole
 * table is about 4 KB of shared memory, and it is sized for headroom. Because a
 * slot is released when its graph is dropped, this bounds the graphs that exist
 * at one time rather than the graphs ever created. Lookups are linear scans of
 * the slots in use, which is the reason not to raise it much further without
 * replacing the scan with a hash.
 */
#define AGE_MAX_GRAPHS 256

/*
 * Graph version counter entry. Stored in shared memory (DSM or shmem)
 * so that all backends can see mutation events. The version counter is
 * incremented by Cypher mutations (CREATE/DELETE/SET/MERGE) and by
 * SQL triggers on label tables. VLE cache invalidation checks this
 * counter instead of snapshot xmin/xmax/curcid.
 */
typedef struct GraphVersionEntry
{
    Oid graph_oid;                 /* graph identifier (0 = unused slot) */
    pg_atomic_uint64 version;      /* monotonic change counter */
} GraphVersionEntry;

/*
 * Shared memory state for graph version tracking.
 * Contains a fixed-size array of per-graph version counters.
 */
typedef struct GraphVersionState
{
    LWLock lock;                   /* protects slot allocation only */
    int num_entries;               /* number of active entries */
    GraphVersionEntry entries[AGE_MAX_GRAPHS];
} GraphVersionState;

/*
 * Version mode detection — determined once per backend on first use.
 * DSM:      PG 17+ GetNamedDSMSegment (no shared_preload_libraries needed)
 * SHMEM:    PG < 17 with shared_preload_libraries
 * SNAPSHOT: PG < 17 without shared_preload_libraries (current behavior)
 */
typedef enum
{
    VERSION_MODE_UNKNOWN = 0,
    VERSION_MODE_DSM,
    VERSION_MODE_SHMEM,
    VERSION_MODE_SNAPSHOT
} VersionMode;

static VersionMode version_mode = VERSION_MODE_UNKNOWN;

/* For PG < 17 shmem path */
static GraphVersionState *shmem_version_state = NULL;

/* internal data structures implementation */

/* vertex entry for the vertex_hashtable */
typedef struct vertex_entry
{
    graphid vertex_id;             /* vertex id, it is also the hash key */
    VertexEdgeArray edges_in;      /* incoming edge graphids (flat array) */
    VertexEdgeArray edges_out;     /* outgoing edge graphids (flat array) */
    VertexEdgeArray edges_self;    /* self-loop edge graphids (flat array) */
    Oid vertex_label_table_oid;    /* the label table oid */
    ItemPointerData tid;           /* physical tuple location for lazy fetch */
} vertex_entry;

/*
 * edge entry for the edge_table.
 *
 * The edge_id is the hash key and is stored in the agehash slot header
 * (immediately before the payload). It is intentionally NOT a field on this
 * payload struct: duplicating it would add 8 bytes per edge to the slot,
 * which on SF10 (~175M edges) is over a gigabyte of overhead. Use
 * get_edge_entry_id(ee) when you need the id of an entry returned by
 * get_edge_entry / get_edge_entry_with_hash; that helper recovers the key
 * from the slot via agehash_key_from_payload.
 */
typedef struct edge_entry
{
    Oid edge_label_table_oid;      /* the label table oid */
    ItemPointerData tid;           /* physical tuple location for lazy fetch */
    graphid start_vertex_id;       /* start vertex */
    graphid end_vertex_id;         /* end vertex */
} edge_entry;

/*
 * GRAPH global context per graph. They are chained together via next.
 * Be aware that the global pointer will point to the root BUT that
 * the root will change as new graphs are added to the top.
 */
typedef struct GRAPH_global_context
{
    char *graph_name;              /* graph name */
    Oid graph_oid;                 /* graph oid for searching */
    HTAB *vertex_hashtable;        /* hashtable to hold vertex edge lists */
    AgeHashTable *edge_table;      /* edge to vertex map (Robin Hood) */
    MemoryContext edge_table_mcxt; /* private context owning edge_table */
    uint64 graph_version;          /* version counter for cache invalidation */
    TransactionId xmin;            /* snapshot fallback: transaction xmin */
    TransactionId xmax;            /* snapshot fallback: transaction xmax */
    CommandId curcid;              /* snapshot fallback: command id */
    Oid load_as_role;              /* role OID the cache was loaded as (RLS cache key) */
    bool loaded_rls_enforced;      /* age.enforce_rls_in_traversal value at load */
    bool vertices_rls_filtered;    /* true if any vertex label was RLS-filtered at load */
    bool loaded_with_rls;          /* true if any label was loaded through RLS (SPI) */
    bool security_invalidated;     /* set by ACL/RLS catalog inval callbacks */
    List *label_table_oids;        /* label table OIDs, for targeted security inval */
    int64 num_loaded_vertices;     /* number of loaded vertices in this graph */
    int64 num_loaded_edges;        /* number of loaded edges in this graph */
    ListGraphId *vertices;         /* vertices for vertex hashtable cleanup */
    struct GRAPH_global_context *next; /* next graph */
} GRAPH_global_context;

/* global variable to hold the per process GRAPH global contexts */
static GRAPH_global_context *global_graph_contexts = NULL;

/*
 * VertexEdgeArray helpers — flat-array adjacency container used by
 * vertex_entry's edges_in / edges_out / edges_self.
 *
 * Growth policy: start at 4 slots on first append, then double on each
 * overflow. This keeps the average cost of n appends amortised O(n) and
 * keeps the memory waste bounded by 2x.
 */
#define VEA_INITIAL_CAPACITY 4

static inline void vea_append(VertexEdgeArray *vea, graphid edge_id)
{
    if (vea->size == vea->capacity)
    {
        int32 new_capacity = (vea->capacity == 0)
                                 ? VEA_INITIAL_CAPACITY
                                 : vea->capacity * 2;

        if (vea->array == NULL)
        {
            vea->array = (graphid *) palloc(new_capacity * sizeof(graphid));
        }
        else
        {
            vea->array = (graphid *) repalloc(vea->array,
                                              new_capacity * sizeof(graphid));
        }

        vea->capacity = new_capacity;
    }
    vea->array[vea->size++] = edge_id;
}

static inline void vea_free(VertexEdgeArray *vea)
{
    if (vea->array != NULL)
    {
        pfree(vea->array);
        vea->array = NULL;
    }
    vea->size = 0;
    vea->capacity = 0;
}

/* declarations */
/* GRAPH global context functions */
static bool free_specific_GRAPH_global_context(GRAPH_global_context *ggctx);
static bool delete_specific_GRAPH_global_contexts(char *graph_name);
static bool delete_GRAPH_global_contexts(void);
static void create_GRAPH_global_hashtables(GRAPH_global_context *ggctx);
static void load_GRAPH_global_hashtables(GRAPH_global_context *ggctx);
static void load_vertex_hashtable(GRAPH_global_context *ggctx);
static void load_edge_hashtable(GRAPH_global_context *ggctx);
static void enforce_label_table_select_acl(Oid label_table_oid);
static void register_ggctx_security_callbacks(void);
static void ggctx_security_relcache_callback(Datum arg, Oid relid);
static void ggctx_security_syscache_callback(Datum arg, int cacheid,
                                             uint32 hashvalue);
static void freeze_GRAPH_global_hashtables(GRAPH_global_context *ggctx);
static List *get_ag_labels_names(Snapshot snapshot, Oid graph_oid,
                                 char label_type);
static bool insert_edge_entry(GRAPH_global_context *ggctx, graphid edge_id,
                              ItemPointerData tid, graphid start_vertex_id,
                              graphid end_vertex_id, Oid edge_label_table_oid);
static bool insert_vertex_edge(GRAPH_global_context *ggctx,
                               graphid start_vertex_id, graphid end_vertex_id,
                               graphid edge_id, char *edge_label_name);
static bool insert_vertex_entry(GRAPH_global_context *ggctx, graphid vertex_id,
                                Oid vertex_label_table_oid,
                                ItemPointerData tid);
static void load_vertex_label_rls(GRAPH_global_context *ggctx,
                                  Oid vertex_label_table_oid,
                                  char *vertex_label_name);
static void load_edge_label_rls(GRAPH_global_context *ggctx,
                                Oid edge_label_table_oid,
                                char *edge_label_name);
/* definitions */

/*
 * Helper function to determine validity of the passed GRAPH_global_context.
 *
 * Uses graph-specific version counters (via DSM or shmem) when available.
 * Falls back to snapshot-based invalidation when shared memory is not
 * initialized (PG < 17 without shared_preload_libraries).
 *
 * The version counter approach only invalidates when the specific graph
 * has been mutated (via Cypher operations or SQL triggers), avoiding false
 * invalidation from unrelated transactions on the server.
 */
bool is_ggctx_invalid(GRAPH_global_context *ggctx)
{
    /*
     * Security-relevant cache keys, checked before the version-counter
     * fast-path below. A cache that applied row-level security is never safe
     * to reuse across statements: RLS policies can depend on session state
     * (for example current_setting()) that is not part of the snapshot,
     * role, or GUC keyed on here. A cache is also role- and GUC-specific
     * because the SELECT ACL / RLS enforcement performed at load depends on
     * the current role and on age.enforce_rls_in_traversal. Only graphs that
     * actually enforced RLS pay the always-rebuild cost; ordinary graphs
     * fall through to the normal version / snapshot validation below.
     */
    if (ggctx->loaded_with_rls)
    {
        return true;
    }
    if (ggctx->load_as_role != GetUserId() ||
        ggctx->loaded_rls_enforced != age_enforce_rls_in_traversal)
    {
        return true;
    }

    /*
     * If enforcement was on at load and a label table's ACL/RLS metadata (or
     * the reading role's authorization) has since changed, an enforced cache
     * may reflect stale permissions. The version-counter fast path below only
     * tracks graph DATA changes, so GRANT/REVOKE, policy or RLS DDL, and role
     * (BYPASSRLS / membership) changes would otherwise go unnoticed. The
     * security-invalidation callbacks (register_ggctx_security_callbacks) set
     * this flag on the relevant catalog invalidations; rebuild so the SELECT
     * ACL / RLS state is re-evaluated. RLS-loaded caches already rebuild
     * unconditionally above; non-enforced (GUC off) caches are unaffected.
     */
    if (ggctx->loaded_rls_enforced && ggctx->security_invalidated)
    {
        return true;
    }

    /* use version counter if DSM or SHMEM mode is active */
    if (version_mode == VERSION_MODE_DSM || version_mode == VERSION_MODE_SHMEM)
    {
        uint64 current_version = get_graph_version(ggctx->graph_oid);

        /*
         * If current_version is 0, no mutations have been tracked through
         * the version counter system yet. Fall through to snapshot-based
         * checking for safety — the graph may have been mutated via paths
         * that don't increment the counter (e.g., before executor hooks
         * are in place, or via direct SQL without triggers).
         *
         * Once current_version > 0, we know the counter is actively
         * tracking this graph and can rely on it exclusively.
         */
        if (current_version > 0)
        {
            return (ggctx->graph_version != current_version);
        }
        /* fall through to snapshot check */
    }

    /* SNAPSHOT fallback: original behavior — check snapshot ids */
    {
        Snapshot snap = GetActiveSnapshot();

        return (ggctx->xmin != snap->xmin ||
                ggctx->xmax != snap->xmax ||
                ggctx->curcid != snap->curcid);
    }
}
/*
 * Fast hash function for graphid (int64) keys.
 *
 * Replaces dynahash's tag_hash (Jenkins lookup3 → ~17 mixing ops) with the
 * MurmurHash3 fmix64 finalizer (5 ops: 3 xorshifts + 2 multiplies).
 *
 * Quality: fmix64 is the avalanche stage of MurmurHash3 and passes all SMHasher
 * tests for 64-bit integer inputs. The output is truncated to uint32 to match
 * dynahash's HashValueFunc signature; bits 0..31 of fmix64 are well-mixed.
 *
 * Performance rationale: graphid lookups dominate hash_search_with_hash_value
 * time (≈41% IC1 on SF3). Reducing the per-call mixing cost cuts both insert
 * and lookup overhead in age_global_graph and age_vle hashtables.
 */
uint32 graphid_hash(const void *key, Size keysize)
{
    uint64 k;

    /* keysize is always sizeof(int64) for every graphid hashtable; assert in debug. */
    Assert(keysize == sizeof(int64));
    (void) keysize;

    /* graphid keys are stored as int64; load aligned (callers pass &graphid). */
    memcpy(&k, key, sizeof(uint64));

    /* MurmurHash3 fmix64 (Austin Appleby, public domain). */
    k ^= k >> 33;
    k *= UINT64CONST(0xff51afd7ed558ccd);
    k ^= k >> 33;
    k *= UINT64CONST(0xc4ceb9fe1a85ec53);
    k ^= k >> 33;

    return (uint32) k;
}

/*
 * agehash key-equality callback for graphid (int64) keys.
 *
 * graphid_hash collisions are rare but real (32-bit hash space, billions of
 * possible keys), so the equality check has to compare the full 8 bytes.
 * memcmp on a fixed 8-byte length compiles to a single load + cmp on x86,
 * which is just as fast as an int64 cast and avoids any alignment risk on
 * other architectures.
 */
bool graphid_keyeq(const void *a, const void *b, Size keysize)
{
    Assert(keysize == sizeof(int64));
    (void) keysize;
    return memcmp(a, b, sizeof(int64)) == 0;
}

/*
 * Helper function to create the global vertex and edge hashtables. One
 * hashtable will hold the vertex, its edges (both incoming and exiting) as a
 * list, and its properties datum. The other hashtable will hold the edge, its
 * properties datum, and its source and target vertex.
 */
static void create_GRAPH_global_hashtables(GRAPH_global_context *ggctx)
{
    HASHCTL vertex_ctl;
    char *graph_name = NULL;
    char *vhn = NULL;
    int glen;
    int vlen;

    /* get the graph name and length */
    graph_name = ggctx->graph_name;
    glen = strlen(graph_name);
    /* get the vertex htab name length */
    vlen = strlen(VERTEX_HTAB_NAME);
    /* allocate the space and build the name */
    vhn = palloc0(vlen + glen + 1);
    strcpy(vhn, VERTEX_HTAB_NAME);
    vhn = strncat(vhn, graph_name, glen);

    /* initialize the vertex hashtable */
    MemSet(&vertex_ctl, 0, sizeof(vertex_ctl));
    vertex_ctl.keysize = sizeof(int64);
    vertex_ctl.entrysize = sizeof(vertex_entry);
    vertex_ctl.hash = graphid_hash;
    ggctx->vertex_hashtable = hash_create(vhn, VERTEX_HTAB_INITIAL_SIZE,
                                          &vertex_ctl,
                                          HASH_ELEM | HASH_FUNCTION);
    pfree_if_not_null(vhn);

    /*
     * Initialize the edge_table (agehash, INLINE mode).
     *
     * Owns its own MemoryContext as a child of CurrentMemoryContext (which,
     * at the call site, is TopMemoryContext for the lifetime of the cached
     * GRAPH_global_context). Cleanup is a single MemoryContextDelete in
     * free_specific_GRAPH_global_context, so an elog during build cannot
     * leak slots.
     */
    ggctx->edge_table_mcxt =
        AllocSetContextCreate(CurrentMemoryContext,
                              "AGE edge_table",
                              ALLOCSET_DEFAULT_SIZES);
    ggctx->edge_table = agehash_create_inline(ggctx->edge_table_mcxt,
                                              sizeof(graphid),
                                              sizeof(edge_entry),
                                              EDGE_HTAB_INITIAL_SIZE,
                                              graphid_hash,
                                              graphid_keyeq);
}

/* helper function to get a List of all label names for the specified graph */
static List *get_ag_labels_names(Snapshot snapshot, Oid graph_oid,
                                 char label_type)
{
    List *labels = NIL;
    ScanKeyData scan_keys[2];
    Relation ag_label;
    TableScanDesc scan_desc;
    HeapTuple tuple;
    TupleDesc tupdesc;
    Oid index_oid = InvalidOid;

    /* we need a valid snapshot */
    Assert(snapshot != NULL);

    /* setup the table to be scanned, ag_label in this case */
    ag_label = table_open(ag_label_relation_id(), AccessShareLock);

    /* get the tupdesc - we don't need to release this one */
    tupdesc = RelationGetDescr(ag_label);
    /* bail if the number of columns differs - this table has 5 */
    Assert(tupdesc->natts == Natts_ag_label);

    /* 
     * Find a usable index whose first key column is ag_label.graph 
     * (Anum_ag_label_graph) 
     */
    index_oid = find_usable_btree_index_for_attr(ag_label, Anum_ag_label_graph);

    if (OidIsValid(index_oid))
    {
        Relation index_rel;
        IndexScanDesc idx_scan_desc;
        ScanKeyData key;
        TupleTableSlot *slot;

        index_rel = index_open(index_oid, AccessShareLock);
        slot = table_slot_create(ag_label, NULL);

        /* 
         * Setup ScanKey: ag_label.graph = graph_oid 
         * Note: We CANNOT filter by 'kind' here because it is not in the index.
         */
        ScanKeyInit(&key, 1, BTEqualStrategyNumber,
                    F_OIDEQ, ObjectIdGetDatum(graph_oid));

        idx_scan_desc = index_beginscan(ag_label, index_rel, snapshot, NULL, 1, 0);
        index_rescan(idx_scan_desc, &key, 1, NULL, 0);

        while (index_getnext_slot(idx_scan_desc, ForwardScanDirection, slot))
        {
            bool shouldFree;
            
            tuple = ExecFetchSlotHeapTuple(slot, true, &shouldFree);

            if (HeapTupleIsValid(tuple))
            {
                bool is_null;
                Datum kind_datum;

                /* 
                 * Since the index only gave us rows for the correct graph,
                 * we must now check if the label 'kind' matches (vertex 'v' or edge 'e').
                 */
                kind_datum = heap_getattr(tuple, Anum_ag_label_kind, tupdesc, &is_null);

                if (!is_null && DatumGetChar(kind_datum) == label_type)
                {
                    Datum name_datum = heap_getattr(tuple, Anum_ag_label_name, tupdesc, &is_null);
                    if (!is_null)
                    {
                        Name label_name_ptr;
                        Name lval;

                        label_name_ptr = DatumGetName(name_datum);
                        lval = (Name) palloc(NAMEDATALEN);
                        namestrcpy(lval, NameStr(*label_name_ptr));
                        labels = lappend(labels, lval);
                    }
                }
            }

            if (shouldFree)
            {
                heap_freetuple(tuple);
            }
            ExecClearTuple(slot);
        }

        ExecDropSingleTupleTableSlot(slot);
        index_endscan(idx_scan_desc);
        index_close(index_rel, AccessShareLock);
    } 
    else
    {
        /* setup scan keys to get all edges for the given graph oid */
        ScanKeyInit(&scan_keys[1], Anum_ag_label_graph, BTEqualStrategyNumber,
                    F_OIDEQ, ObjectIdGetDatum(graph_oid));
        ScanKeyInit(&scan_keys[0], Anum_ag_label_kind, BTEqualStrategyNumber,
                    F_CHAREQ, CharGetDatum(label_type));

        scan_desc = table_beginscan(ag_label, snapshot, 2, scan_keys);

        /* get all of the label names */
        while((tuple = heap_getnext(scan_desc, ForwardScanDirection)) != NULL)
        {
            Name label;
            Name lval;
            bool is_null = false;

            /* something is wrong if this tuple isn't valid */
            Assert(HeapTupleIsValid(tuple));
            /* get the label name */
            label = DatumGetName(heap_getattr(tuple, Anum_ag_label_name, tupdesc,
                                            &is_null));

            Assert(!is_null);
            /* add it to our list */
            lval = (Name) palloc(NAMEDATALEN);
            namestrcpy(lval, NameStr(*label));
            labels = lappend(labels, lval);
        }

        /* close up scan */
        table_endscan(scan_desc);
    }

    table_close(ag_label, AccessShareLock);

    return labels;
}

/*
 * Helper function to insert one edge/edge->vertex, key/value pair, in the
 * current GRAPH global edge hashtable.
 */
static bool insert_edge_entry(GRAPH_global_context *ggctx, graphid edge_id,
                              ItemPointerData tid, graphid start_vertex_id,
                              graphid end_vertex_id, Oid edge_label_table_oid)
{
    edge_entry *ee = NULL;
    bool found = false;

    /* search for the edge */
    ee = (edge_entry *) agehash_insert(ggctx->edge_table,
                                       (void *) &edge_id, &found);

    /* agehash never returns NULL on insert; a NULL would indicate a bug. */
    if (ee == NULL)
    {
        elog(ERROR, "insert_edge_entry: hash table returned NULL for ee");
    }

    /*
     * If we found the key, either we have a duplicate, or we made a mistake and
     * inserted it already. Either way, this isn't good so don't insert it and
     * return false.
     */
    if (found)
    {
        ereport(WARNING,
                (errcode(ERRCODE_DATA_EXCEPTION),
                 errmsg("edge: [id: %ld, start: %ld, end: %ld, label oid: %d] %s",
                        edge_id, start_vertex_id, end_vertex_id,
                        edge_label_table_oid, "duplicate edge found")));

        ereport(WARNING,
                (errcode(ERRCODE_DATA_EXCEPTION),
                 errmsg("previous edge: [id: %ld, start: %ld, end: %ld, label oid: %d]",
                        edge_id, ee->start_vertex_id, ee->end_vertex_id,
                        ee->edge_label_table_oid)));

        return false;
    }

    /*
     * agehash_insert zero-fills the payload on a fresh insert, so we can fill
     * in only the fields we care about. The hash key (edge_id) lives in the
     * slot header; recoverable via get_edge_entry_id() if needed.
     */
    ee->tid = tid;
    ee->start_vertex_id = start_vertex_id;
    ee->end_vertex_id = end_vertex_id;
    ee->edge_label_table_oid = edge_label_table_oid;

    /* increment the number of loaded edges */
    ggctx->num_loaded_edges++;

    return true;
}

/*
 * Helper function to insert an entire vertex into the current GRAPH global
 * vertex hashtable. It will return false if there is a duplicate.
 */
static bool insert_vertex_entry(GRAPH_global_context *ggctx, graphid vertex_id,
                                Oid vertex_label_table_oid,
                                ItemPointerData tid)
{
    vertex_entry *ve = NULL;
    bool found = false;

    /* search for the vertex */
    ve = (vertex_entry *)hash_search(ggctx->vertex_hashtable,
                                     (void *)&vertex_id, HASH_ENTER, &found);

    /* if the hash enter returned is NULL, error out */
    if (ve == NULL)
    {
        elog(ERROR, "insert_vertex_entry: hash table returned NULL for ve");
    }

    /* we should never have duplicates, warn and return false */
    if (found)
    {
        ereport(WARNING,
                (errcode(ERRCODE_DATA_EXCEPTION),
                 errmsg("vertex: [id: %ld, label oid: %d] %s",
                        vertex_id, vertex_label_table_oid,
                        "duplicate vertex found")));

        ereport(WARNING,
                (errcode(ERRCODE_DATA_EXCEPTION),
                 errmsg("previous vertex: [id: %ld, label oid: %d]",
                        ve->vertex_id, ve->vertex_label_table_oid)));

        return false;
    }

    /* again, MemSet may not be needed here */
    MemSet(ve, 0, sizeof(vertex_entry));

    /*
     * Set the vertex id - this is important as this is the hash key value
     * used for hash function collisions.
     */
    ve->vertex_id = vertex_id;
    /* set the label table oid for this vertex */
    ve->vertex_label_table_oid = vertex_label_table_oid;
    /* set the TID for lazy property fetch */
    ve->tid = tid;
    /*
     * MemSet above already zeroed the embedded VertexEdgeArray fields
     * (array=NULL, size=0, capacity=0); no explicit NIL assignment needed.
     */

    /* we also need to store the vertex id for clean up of vertex lists */
    ggctx->vertices = append_graphid(ggctx->vertices, vertex_id);

    /* increment the number of loaded vertices */
    ggctx->num_loaded_vertices++;

    return true;
}

/*
 * Helper function to append one edge to an existing vertex in the current
 * global vertex hashtable.
 */
static bool insert_vertex_edge(GRAPH_global_context *ggctx,
                               graphid start_vertex_id, graphid end_vertex_id,
                               graphid edge_id, char *edge_label_name)
{
    vertex_entry *value = NULL;
    bool start_found = false;
    bool end_found = false;
    bool is_selfloop = false;

    /* is it a self loop */
    is_selfloop = (start_vertex_id == end_vertex_id);

    /* search for the start vertex of the edge */
    value = (vertex_entry *)hash_search(ggctx->vertex_hashtable,
                                        (void *)&start_vertex_id, HASH_FIND,
                                        &start_found);

    /*
     * If we found the start_vertex_id and it is a self loop, add the edge to
     * edges_self and we're done
     */
    if (start_found && is_selfloop)
    {
        vea_append(&value->edges_self, edge_id);
        return true;
    }
    /*
     * Otherwise, if we found the start_vertex_id add the edge to the edges_out
     * list of the start vertex
     */
    else if (start_found)
    {
        vea_append(&value->edges_out, edge_id);
    }

    /* search for the end vertex of the edge */
    value = (vertex_entry *)hash_search(ggctx->vertex_hashtable,
                                        (void *)&end_vertex_id, HASH_FIND,
                                        &end_found);

    /*
     * If we found the start_vertex_id and the end_vertex_id add the edge to the
     * edges_in list of the end vertex
     */
    if (start_found && end_found)
    {
        vea_append(&value->edges_in, edge_id);
        return true;
    }
    /*
     * Otherwise we need to generate the appropriate warning message about the
     * dangling edge that we found.
     */
    else if (!start_found && end_found)
    {
        ereport(WARNING,
                (errcode(ERRCODE_DATA_EXCEPTION),
                 errmsg("edge: [id: %ld, start: %ld, end: %ld, label: %s] %s",
                        edge_id, start_vertex_id, end_vertex_id,
                        edge_label_name, "start vertex not found")));
    }
    else if (start_found && !end_found)
    {
        ereport(WARNING,
                (errcode(ERRCODE_DATA_EXCEPTION),
                 errmsg("edge: [id: %ld, start: %ld, end: %ld, label: %s] %s",
                        edge_id, start_vertex_id, end_vertex_id,
                        edge_label_name, "end vertex not found")));
    }
    else
    {
        ereport(WARNING,
                (errcode(ERRCODE_DATA_EXCEPTION),
                 errmsg("edge: [id: %ld, start: %ld, end: %ld, label: %s] %s",
                        edge_id, start_vertex_id, end_vertex_id,
                        edge_label_name, "start and end vertices not found")));
    }

    return false;
}

/*
 * RLS-aware loader for a single vertex label table.
 *
 * Loads (id, ctid) through SPI so the planner applies row-level security
 * policies (and table/column ACL) for the current role, then inserts each
 * visible row into the graph's global vertex hashtable. The physical tuple
 * location (ctid) is stored for the same lazy property fetch the direct-scan
 * path uses (get_vertex_entry_properties); because only RLS-visible rows are
 * loaded, that later heap_fetch only ever reads authorized tuples. Used in
 * place of the direct heap scan when age.enforce_rls_in_traversal is on and RLS
 * is active on the label table. Rows are fetched in batches so the entire label
 * table is never materialized at once.
 */
static void load_vertex_label_rls(GRAPH_global_context *ggctx,
                                  Oid vertex_label_table_oid,
                                  char *vertex_label_name)
{
    MemoryContext ggctx_cxt = CurrentMemoryContext;
    StringInfoData query;
    SPIPlanPtr plan;
    Portal portal;

    if (SPI_connect() != SPI_OK_CONNECT)
    {
        ereport(ERROR,
                (errcode(ERRCODE_INTERNAL_ERROR),
                 errmsg("load_vertex_label_rls: SPI_connect failed")));
    }

    /*
     * Build the query text AFTER SPI_connect(): SPI switches to its own
     * procedure memory context here, so the StringInfo buffer and the
     * quote_qualified_identifier() result are allocated in that context and
     * freed by SPI_finish(), instead of leaking into the caller's long-lived
     * (TopMemoryContext) cache-build context that is re-entered on every RLS
     * cache rebuild.
     */
    initStringInfo(&query);
    appendStringInfo(&query, "SELECT id, ctid FROM ONLY %s",
                     quote_qualified_identifier(ggctx->graph_name,
                                                vertex_label_name));

    plan = SPI_prepare(query.data, 0, NULL);
    if (plan == NULL)
    {
        ereport(ERROR,
                (errcode(ERRCODE_INTERNAL_ERROR),
                 errmsg("load_vertex_label_rls: SPI_prepare failed: %s",
                        SPI_result_code_string(SPI_result))));
    }

    /* read-only cursor: reuses the active snapshot; RLS applied by planner */
    portal = SPI_cursor_open(NULL, plan, NULL, NULL, true);

    for (;;)
    {
        uint64 row;

        SPI_cursor_fetch(portal, true, 10000);

        if (SPI_processed == 0)
        {
            break;
        }

        for (row = 0; row < SPI_processed; row++)
        {
            HeapTuple tuple = SPI_tuptable->vals[row];
            TupleDesc tupdesc = SPI_tuptable->tupdesc;
            bool isnull = false;
            graphid vertex_id;
            ItemPointer tidptr;
            ItemPointerData tid;
            MemoryContext oldctx;

            vertex_id = DatumGetInt64(SPI_getbinval(tuple, tupdesc, 1,
                                                    &isnull));
            tidptr = (ItemPointer) DatumGetPointer(SPI_getbinval(tuple, tupdesc,
                                                                 2, &isnull));
            /* ctid is never null for a scanned heap tuple; guard defensively */
            if (isnull || tidptr == NULL)
            {
                continue;
            }
            tid = *tidptr;

            /* build the entry in the cache's (persistent) memory context */
            oldctx = MemoryContextSwitchTo(ggctx_cxt);
            insert_vertex_entry(ggctx, vertex_id, vertex_label_table_oid, tid);
            MemoryContextSwitchTo(oldctx);
        }

        SPI_freetuptable(SPI_tuptable);
    }

    SPI_cursor_close(portal);
    SPI_finish();
}

/*
 * RLS-aware loader for a single edge label table. See load_vertex_label_rls;
 * this variant additionally loads start_id/end_id and populates the per-vertex
 * edge lists, exactly like the direct-scan path, storing the ctid for the same
 * lazy property fetch.
 */
static void load_edge_label_rls(GRAPH_global_context *ggctx,
                                Oid edge_label_table_oid,
                                char *edge_label_name)
{
    MemoryContext ggctx_cxt = CurrentMemoryContext;
    StringInfoData query;
    SPIPlanPtr plan;
    Portal portal;

    if (SPI_connect() != SPI_OK_CONNECT)
    {
        ereport(ERROR,
                (errcode(ERRCODE_INTERNAL_ERROR),
                 errmsg("load_edge_label_rls: SPI_connect failed")));
    }

    /*
     * Build the query text AFTER SPI_connect(); see load_vertex_label_rls for
     * why (SPI procedure memory context ownership avoids a per-rebuild leak).
     */
    initStringInfo(&query);
    appendStringInfo(&query,
                     "SELECT id, start_id, end_id, ctid FROM ONLY %s",
                     quote_qualified_identifier(ggctx->graph_name,
                                                edge_label_name));

    plan = SPI_prepare(query.data, 0, NULL);
    if (plan == NULL)
    {
        ereport(ERROR,
                (errcode(ERRCODE_INTERNAL_ERROR),
                 errmsg("load_edge_label_rls: SPI_prepare failed: %s",
                        SPI_result_code_string(SPI_result))));
    }

    /* read-only cursor: reuses the active snapshot; RLS applied by planner */
    portal = SPI_cursor_open(NULL, plan, NULL, NULL, true);

    for (;;)
    {
        uint64 row;

        SPI_cursor_fetch(portal, true, 10000);

        if (SPI_processed == 0)
        {
            break;
        }

        for (row = 0; row < SPI_processed; row++)
        {
            HeapTuple tuple = SPI_tuptable->vals[row];
            TupleDesc tupdesc = SPI_tuptable->tupdesc;
            bool isnull = false;
            graphid edge_id;
            graphid edge_vertex_start_id;
            graphid edge_vertex_end_id;
            ItemPointer tidptr;
            ItemPointerData tid;
            MemoryContext oldctx;

            edge_id = DatumGetInt64(SPI_getbinval(tuple, tupdesc, 1, &isnull));
            edge_vertex_start_id = DatumGetInt64(SPI_getbinval(tuple, tupdesc,
                                                               2, &isnull));
            edge_vertex_end_id = DatumGetInt64(SPI_getbinval(tuple, tupdesc, 3,
                                                             &isnull));
            tidptr = (ItemPointer) DatumGetPointer(SPI_getbinval(tuple, tupdesc,
                                                                 4, &isnull));
            /* ctid is never null for a scanned heap tuple; guard defensively */
            if (isnull || tidptr == NULL)
            {
                continue;
            }
            tid = *tidptr;

            /*
             * If a vertex label was RLS-filtered, drop any edge whose start or
             * end vertex is not visible. This mirrors MATCH's inner-join
             * behavior and avoids leaving a dangling edge in the cache.
             */
            if (ggctx->vertices_rls_filtered &&
                (get_vertex_entry(ggctx, edge_vertex_start_id) == NULL ||
                 get_vertex_entry(ggctx, edge_vertex_end_id) == NULL))
            {
                continue;
            }

            /* build the entries in the cache's (persistent) memory context */
            oldctx = MemoryContextSwitchTo(ggctx_cxt);
            insert_edge_entry(ggctx, edge_id, tid, edge_vertex_start_id,
                              edge_vertex_end_id, edge_label_table_oid);
            insert_vertex_edge(ggctx, edge_vertex_start_id,
                               edge_vertex_end_id, edge_id, edge_label_name);
            MemoryContextSwitchTo(oldctx);
        }

        SPI_freetuptable(SPI_tuptable);
    }

    SPI_cursor_close(portal);
    SPI_finish();
}

/*
 * Enforce table-level SELECT privilege on a graph label table before it is read
 * by the low-level heap scans in the loaders below.
 *
 * Those scans bypass the executor, which is what normally enforces SELECT ACLs.
 * Without this a role could read rows of a label table it has no SELECT
 * privilege on through a VLE / traversal. The RLS half of that is handled
 * separately by routing RLS-enabled labels through an SPI load
 * (which the planner filters). This helper runs before both load paths: it is
 * the only SELECT-ACL check on the raw fast path (used when RLS is not enabled),
 * and a harmless early check before the SPI path (which the planner ACL-checks
 * anyway). Gated by the same GUC as RLS enforcement so operators retain a
 * single escape hatch.
 */
static void enforce_label_table_select_acl(Oid label_table_oid)
{
    if (!age_enforce_rls_in_traversal)
    {
        return;
    }

    /*
     * table_open() / table_beginscan() below do not verify SELECT privilege -
     * the executor normally does - so check it explicitly here.
     *
     * pg_class_aclcheck() only covers relation-level SELECT. PostgreSQL also
     * allows column-level SELECT grants, which the executor (and therefore the
     * RLS/SPI load path) honors. The loaders read every user column of the
     * label table, so when the relation-level check fails, fall back to
     * requiring SELECT on each live user column; this keeps the fast-path check
     * consistent with what the executor would permit instead of over-denying a
     * role that was granted the columns individually.
     */
    if (pg_class_aclcheck(label_table_oid, GetUserId(), ACL_SELECT) !=
        ACLCHECK_OK)
    {
        Relation rel = table_open(label_table_oid, AccessShareLock);
        TupleDesc tupdesc = RelationGetDescr(rel);
        int i;

        for (i = 0; i < tupdesc->natts; i++)
        {
            Form_pg_attribute attr = TupleDescAttr(tupdesc, i);

            /* skip dropped and system columns */
            if (attr->attisdropped || attr->attnum <= 0)
            {
                continue;
            }

            if (pg_attribute_aclcheck(label_table_oid, attr->attnum,
                                      GetUserId(), ACL_SELECT) != ACLCHECK_OK)
            {
                table_close(rel, AccessShareLock);
                ereport(ERROR,
                        (errcode(ERRCODE_INSUFFICIENT_PRIVILEGE),
                         errmsg("permission denied for table %s",
                                get_rel_name(label_table_oid))));
            }
        }

        table_close(rel, AccessShareLock);
    }
}

/* helper routine to load all vertices into the GRAPH global vertex hashtable */
static void load_vertex_hashtable(GRAPH_global_context *ggctx)
{
    Oid graph_oid;
    Oid graph_namespace_oid;
    Snapshot snapshot;
    List *vertex_label_names = NIL;
    ListCell *lc;

    /* get the specific graph OID and namespace (schema) OID */
    graph_oid = ggctx->graph_oid;
    graph_namespace_oid = get_namespace_oid(ggctx->graph_name, false);
    /* get the active snapshot */
    snapshot = GetActiveSnapshot();
    /* get the names of all of the vertex label tables */
    vertex_label_names = get_ag_labels_names(snapshot, graph_oid,
                                             LABEL_TYPE_VERTEX);
    /*
     * The scan below enforces the SELECT ACL (and, when RLS is active on a
     * label, loads it via SPI) and can therefore throw. get_ag_labels_names()
     * built this list in TopMemoryContext (backend lifetime), so free it on
     * the error path as well; otherwise a repeatedly-failing rebuild - for
     * example a role retrying a VLE on a label it cannot read - leaks the
     * list on every attempt.
     */
    PG_TRY();
    {
        /* go through all vertex label tables in list */
        foreach (lc, vertex_label_names)
        {
            Relation graph_vertex_label;
            TableScanDesc scan_desc;
            HeapTuple tuple;
            char *vertex_label_name;
            Oid vertex_label_table_oid;
            TupleDesc tupdesc;

            /* get the vertex label name */
            vertex_label_name = lfirst(lc);
            /* get the vertex label name's OID */
            vertex_label_table_oid = get_relname_relid(vertex_label_name,
                                                       graph_namespace_oid);
            /*
             * Record this label table OID so the security-invalidation callbacks
             * can target this graph when the label's ACL/RLS metadata changes.
             */
            ggctx->label_table_oids =
                list_append_unique_oid(ggctx->label_table_oids,
                                       vertex_label_table_oid);
            /*
             * Enforce SELECT privilege before any read of this label table
             * (covers the raw heap-scan fast path below, which bypasses the
             * executor's ACL check). No-op when the enforcement GUC is off.
             */
            enforce_label_table_select_acl(vertex_label_table_oid);
            /*
             * When RLS is active on this label for the current role, load it via
             * SPI so the planner enforces the policies, then skip the direct
             * scan below.
             */
            if (age_enforce_rls_in_traversal &&
                check_enable_rls(vertex_label_table_oid, InvalidOid, true) ==
                    RLS_ENABLED)
            {
                load_vertex_label_rls(ggctx, vertex_label_table_oid,
                                      vertex_label_name);
                /*
                 * A vertex label was RLS-filtered, so the visible vertex set may
                 * be a subset of what the edges reference. Remember this so the
                 * edge loaders can drop edges whose endpoints are not visible.
                 */
                ggctx->vertices_rls_filtered = true;
                ggctx->loaded_with_rls = true;
                continue;
            }
            /* open the relation (table) and begin the scan */
            graph_vertex_label = table_open(vertex_label_table_oid, AccessShareLock);
            scan_desc = table_beginscan(graph_vertex_label, snapshot, 0, NULL);
            /* get the tupdesc - we don't need to release this one */
            tupdesc = RelationGetDescr(graph_vertex_label);
            /* bail if the number of columns differs */
            if (tupdesc->natts != 2)
            {
                ereport(ERROR,
                        (errcode(ERRCODE_UNDEFINED_TABLE),
                         errmsg("Invalid number of attributes for %s.%s",
                         ggctx->graph_name, vertex_label_name)));
            }
            /* get all tuples in table and insert them into graph hashtables */
            while((tuple = heap_getnext(scan_desc, ForwardScanDirection)) != NULL)
            {
                graphid vertex_id;
                bool inserted = false;

                /* something is wrong if this isn't true */
                if (!HeapTupleIsValid(tuple))
                {
                    elog(ERROR, "load_vertex_hashtable: !HeapTupleIsValid");
                }
                Assert(HeapTupleIsValid(tuple));

                /* get the vertex id */
                vertex_id = DatumGetInt64(column_get_datum(tupdesc, tuple, 0, "id",
                                                           GRAPHIDOID, true));

                /* insert vertex into vertex hashtable with TID (no property copy) */
                inserted = insert_vertex_entry(ggctx, vertex_id,
                                               vertex_label_table_oid,
                                               tuple->t_self);

                /* warn if there is a duplicate */
                if (!inserted)
                {
                     ereport(WARNING,
                             (errcode(ERRCODE_DATA_EXCEPTION),
                              errmsg("ignored duplicate vertex")));
                }
            }

            /* end the scan and close the relation */
            table_endscan(scan_desc);
            table_close(graph_vertex_label, AccessShareLock);
        }
    }
    PG_CATCH();
    {
        list_free_deep(vertex_label_names);
        PG_RE_THROW();
    }
    PG_END_TRY();

    /*
     * Free the transient list of label names (and the palloc'd Name entries)
     * returned by get_ag_labels_names(). The global graph context is built in
     * TopMemoryContext, so this list would otherwise persist for the life of
     * the backend and accumulate on every rebuild.
     */
    list_free_deep(vertex_label_names);
}

/*
 * Helper function to load all of the GRAPH global hashtables (vertex & edge)
 * for the current global context.
 */
static void load_GRAPH_global_hashtables(GRAPH_global_context *ggctx)
{
    /* initialize statistics */
    ggctx->num_loaded_vertices = 0;
    ggctx->num_loaded_edges = 0;

    /* insert all of our vertices */
    load_vertex_hashtable(ggctx);

    /* insert all of our edges */
    load_edge_hashtable(ggctx);
}

/*
 * Helper routine to load all edges into the GRAPH global edge and vertex
 * hashtables.
 */
static void load_edge_hashtable(GRAPH_global_context *ggctx)
{
    Oid graph_oid;
    Oid graph_namespace_oid;
    Snapshot snapshot;
    List *edge_label_names = NIL;
    ListCell *lc;

    /* get the specific graph OID and namespace (schema) OID */
    graph_oid = ggctx->graph_oid;
    graph_namespace_oid = get_namespace_oid(ggctx->graph_name, false);
    /* get the active snapshot */
    snapshot = GetActiveSnapshot();
    /* get the names of all of the edge label tables */
    edge_label_names = get_ag_labels_names(snapshot, graph_oid,
                                           LABEL_TYPE_EDGE);
    /*
     * The scan below enforces the SELECT ACL (and, when RLS is active on a
     * label, loads it via SPI) and can therefore throw. get_ag_labels_names()
     * built this list in TopMemoryContext (backend lifetime), so free it on
     * the error path as well; otherwise a repeatedly-failing rebuild - for
     * example a role retrying a VLE on a label it cannot read - leaks the
     * list on every attempt.
     */
    PG_TRY();
    {
        /* go through all edge label tables in list */
        foreach (lc, edge_label_names)
        {
            Relation graph_edge_label;
            TableScanDesc scan_desc;
            HeapTuple tuple;
            char *edge_label_name;
            Oid edge_label_table_oid;
            TupleDesc tupdesc;

            /* get the edge label name */
            edge_label_name = lfirst(lc);
            /* get the edge label name's OID */
            edge_label_table_oid = get_relname_relid(edge_label_name,
                                                     graph_namespace_oid);
            /*
             * Record this label table OID so the security-invalidation callbacks
             * can target this graph when the label's ACL/RLS metadata changes.
             */
            ggctx->label_table_oids =
                list_append_unique_oid(ggctx->label_table_oids,
                                       edge_label_table_oid);
            /*
             * Enforce SELECT privilege before any read of this label table
             * (covers the raw heap-scan fast path below, which bypasses the
             * executor's ACL check). No-op when the enforcement GUC is off.
             */
            enforce_label_table_select_acl(edge_label_table_oid);
            /*
             * When RLS is active on this label for the current role, load it via
             * SPI so the planner enforces the policies, then skip the direct
             * scan below.
             */
            if (age_enforce_rls_in_traversal &&
                check_enable_rls(edge_label_table_oid, InvalidOid, true) ==
                    RLS_ENABLED)
            {
                load_edge_label_rls(ggctx, edge_label_table_oid, edge_label_name);
                ggctx->loaded_with_rls = true;
                continue;
            }
            /* open the relation (table) and begin the scan */
            graph_edge_label = table_open(edge_label_table_oid, AccessShareLock);
            scan_desc = table_beginscan(graph_edge_label, snapshot, 0, NULL);
            /* get the tupdesc - we don't need to release this one */
            tupdesc = RelationGetDescr(graph_edge_label);
            /* bail if the number of columns differs */
            if (tupdesc->natts != 4)
            {
                ereport(ERROR,
                        (errcode(ERRCODE_UNDEFINED_TABLE),
                         errmsg("Invalid number of attributes for %s.%s",
                         ggctx->graph_name, edge_label_name)));
            }
            /* get all tuples in table and insert them into graph hashtables */
            while((tuple = heap_getnext(scan_desc, ForwardScanDirection)) != NULL)
            {
                graphid edge_id;
                graphid edge_vertex_start_id;
                graphid edge_vertex_end_id;
                bool inserted = false;

                /* something is wrong if this isn't true */
                if (!HeapTupleIsValid(tuple))
                {
                    elog(ERROR, "load_edge_hashtable: !HeapTupleIsValid");
                }
                Assert(HeapTupleIsValid(tuple));

                /* get the edge id */
                edge_id = DatumGetInt64(column_get_datum(tupdesc, tuple, 0, "id",
                                                         GRAPHIDOID, true));
                /* get the edge start_id (start vertex id) */
                edge_vertex_start_id = DatumGetInt64(column_get_datum(tupdesc,
                                                                      tuple, 1,
                                                                      "start_id",
                                                                      GRAPHIDOID,
                                                                      true));
                /* get the edge end_id (end vertex id)*/
                edge_vertex_end_id = DatumGetInt64(column_get_datum(tupdesc, tuple,
                                                                    2, "end_id",
                                                                    GRAPHIDOID,
                                                                    true));

                /*
                 * If a vertex label was RLS-filtered, drop any edge whose start
                 * or end vertex is not visible (mirrors MATCH's inner-join
                 * behavior).
                 */
                if (ggctx->vertices_rls_filtered &&
                    (get_vertex_entry(ggctx, edge_vertex_start_id) == NULL ||
                     get_vertex_entry(ggctx, edge_vertex_end_id) == NULL))
                {
                    continue;
                }

                /* insert edge into edge hashtable with TID (no property copy) */
                inserted = insert_edge_entry(ggctx, edge_id, tuple->t_self,
                                             edge_vertex_start_id,
                                             edge_vertex_end_id,
                                             edge_label_table_oid);

                /* warn if there is a duplicate */
                if (!inserted)
                {
                     ereport(WARNING,
                             (errcode(ERRCODE_DATA_EXCEPTION),
                              errmsg("ignored duplicate edge")));
                }

                /* insert the edge into the start and end vertices edge lists */
                inserted = insert_vertex_edge(ggctx, edge_vertex_start_id,
                                              edge_vertex_end_id, edge_id,
                                              edge_label_name);
                if (!inserted)
                {
                     ereport(WARNING,
                             (errcode(ERRCODE_DATA_EXCEPTION),
                              errmsg("ignored malformed or dangling edge")));
                }
            }

            /* end the scan and close the relation */
            table_endscan(scan_desc);
            table_close(graph_edge_label, AccessShareLock);
        }
    }
    PG_CATCH();
    {
        list_free_deep(edge_label_names);
        PG_RE_THROW();
    }
    PG_END_TRY();

    /*
     * Free the transient list of label names (and the palloc'd Name entries)
     * returned by get_ag_labels_names(). The global graph context is built in
     * TopMemoryContext, so this list would otherwise persist for the life of
     * the backend and accumulate on every rebuild.
     */
    list_free_deep(edge_label_names);
}

/*
 * Helper function to freeze the GRAPH global hashtables from additional
 * inserts. This may, or may not, be useful. Currently, these hashtables are
 * only seen by the creating process and only for reading.
 */
static void freeze_GRAPH_global_hashtables(GRAPH_global_context *ggctx)
{
    hash_freeze(ggctx->vertex_hashtable);
    agehash_freeze(ggctx->edge_table);
}

/*
 * Helper function to free the entire specified GRAPH global context. After
 * running this you should not use the pointer in ggctx.
 */
static bool free_specific_GRAPH_global_context(GRAPH_global_context *ggctx)
{
    GraphIdNode *curr_vertex = NULL;

    /* don't do anything if NULL */
    if (ggctx == NULL)
    {
        return true;
    }

    /* free the graph name */
    pfree_if_not_null(ggctx->graph_name);
    ggctx->graph_name = NULL;

    /* free the label table OID list used for security invalidation */
    list_free(ggctx->label_table_oids);
    ggctx->label_table_oids = NULL;

    ggctx->graph_oid = InvalidOid;
    ggctx->next = NULL;

    /* free the vertex edge lists and properties, starting with the head */
    curr_vertex = peek_stack_head(ggctx->vertices);
    while (curr_vertex != NULL)
    {
        GraphIdNode *next_vertex = NULL;
        vertex_entry *value = NULL;
        bool found = false;
        graphid vertex_id;

        /* get the next vertex in the list, if any */
        next_vertex = next_GraphIdNode(curr_vertex);

        /* get the current vertex id */
        vertex_id = get_graphid(curr_vertex);

        /* retrieve the vertex entry */
        value = (vertex_entry *)hash_search(ggctx->vertex_hashtable,
                                            (void *)&vertex_id, HASH_FIND,
                                            &found);
        /* this is bad if it isn't found, but leave that to the caller */
        if (found == false)
        {
            return false;
        }

        /* free the edge arrays associated with this vertex */
        vea_free(&value->edges_in);
        vea_free(&value->edges_out);
        vea_free(&value->edges_self);

        /* move to the next vertex */
        curr_vertex = next_vertex;
    }

    /* free the vertices list */
    free_ListGraphId(ggctx->vertices);
    ggctx->vertices = NULL;

    /* free the hashtables */
    hash_destroy(ggctx->vertex_hashtable);
    /*
     * The edge_table and all of its slots live entirely inside
     * edge_table_mcxt, so a single MemoryContextDelete reclaims them.
     */
    if (ggctx->edge_table_mcxt != NULL)
    {
        MemoryContextDelete(ggctx->edge_table_mcxt);
    }

    ggctx->vertex_hashtable = NULL;
    ggctx->edge_table = NULL;
    ggctx->edge_table_mcxt = NULL;

    /* free the context */
    pfree_if_not_null(ggctx);
    ggctx = NULL;

    return true;
}

/*
 * Security-invalidation callbacks for the global-graph cache.
 *
 * The version-counter fast path in is_ggctx_invalid only tracks graph DATA
 * changes, so a cache built under enforcement (direct scan + SELECT ACL, no RLS)
 * would keep serving rows after a GRANT/REVOKE, a policy/RLS DDL change, or a
 * role authorization change, because none of those bump the data version. These
 * callbacks mark such caches stale on the relevant catalog invalidations;
 * is_ggctx_invalid then rebuilds and re-evaluates authorization.
 */

/*
 * Relcache invalidation. Table-level (pg_class) and column-level (pg_attribute)
 * SELECT ACL changes, CREATE/ALTER/DROP POLICY, and ENABLE/DISABLE/FORCE ROW
 * LEVEL SECURITY all invalidate the affected relation's relcache. Mark every
 * cached graph whose label tables include relid; InvalidOid is a global reset.
 */
static void ggctx_security_relcache_callback(Datum arg, Oid relid)
{
    GRAPH_global_context *ggctx;

    for (ggctx = global_graph_contexts; ggctx != NULL; ggctx = ggctx->next)
    {
        if (!OidIsValid(relid) ||
            list_member_oid(ggctx->label_table_oids, relid))
        {
            ggctx->security_invalidated = true;
        }
    }
}

/*
 * Syscache invalidation on pg_authid (AUTHOID) and pg_auth_members
 * (AUTHMEMMEMROLE / AUTHMEMROLEMEM). Role attribute (for example BYPASSRLS) and
 * membership changes affect authorization but carry no relid we can map to a
 * label table, so mark all cached contexts stale. Such DDL is rare, so the
 * broad invalidation is inexpensive.
 */
static void ggctx_security_syscache_callback(Datum arg, int cacheid,
                                             uint32 hashvalue)
{
    GRAPH_global_context *ggctx;

    for (ggctx = global_graph_contexts; ggctx != NULL; ggctx = ggctx->next)
    {
        ggctx->security_invalidated = true;
    }
}

/*
 * Register the security-invalidation callbacks once per backend. Registered
 * lazily from manage_GRAPH_global_contexts, before the first cache is built, so
 * there is never a cache in existence that could miss an earlier invalidation.
 */
static void register_ggctx_security_callbacks(void)
{
    static bool registered = false;

    if (registered)
    {
        return;
    }

    CacheRegisterRelcacheCallback(ggctx_security_relcache_callback, (Datum) 0);
    CacheRegisterSyscacheCallback(AUTHOID, ggctx_security_syscache_callback,
                                  (Datum) 0);
    CacheRegisterSyscacheCallback(AUTHMEMMEMROLE,
                                  ggctx_security_syscache_callback, (Datum) 0);
    CacheRegisterSyscacheCallback(AUTHMEMROLEMEM,
                                  ggctx_security_syscache_callback, (Datum) 0);

    registered = true;
}

/*
 * Helper function to manage the GRAPH global contexts. It will create the
 * context for the graph specified, provided it isn't already built and valid.
 * During processing it will free (delete) all invalid GRAPH contexts. It
 * returns the GRAPH global context for the specified graph.
 */
GRAPH_global_context *manage_GRAPH_global_contexts(char *graph_name,
                                                   Oid graph_oid)
{
    GRAPH_global_context *new_ggctx = NULL;
    GRAPH_global_context *curr_ggctx = NULL;
    GRAPH_global_context *prev_ggctx = NULL;
    MemoryContext oldctx = NULL;

    /* we need a higher context, or one that isn't destroyed by SRF exit */
    oldctx = MemoryContextSwitchTo(TopMemoryContext);

    /*
     * Ensure the ACL/RLS security-invalidation callbacks are registered (once
     * per backend) before any cache is built, so the version-counter fast path
     * in is_ggctx_invalid cannot serve stale permissions after GRANT/REVOKE,
     * policy/RLS DDL, or role authorization changes.
     */
    register_ggctx_security_callbacks();

    /*
     * We need to see if any GRAPH global contexts already exist and if any do
     * for this particular graph. There are 5 possibilities -
     *
     *     1) There are no global contexts.
     *     2) One does exist for this graph but, is invalid.
     *     3) One does exist for this graph and is valid.
     *     4) One or more other contexts do exist and all are valid.
     *     5) One or more other contexts do exist but, one or more are invalid.
     */


    /* free the invalidated GRAPH global contexts first */
    prev_ggctx = NULL;
    curr_ggctx = global_graph_contexts;
    while (curr_ggctx != NULL)
    {
        GRAPH_global_context *next_ggctx = curr_ggctx->next;

        /* if the transaction ids have changed, we have an invalid graph */
        if (is_ggctx_invalid(curr_ggctx))
        {
            bool success = false;

            /*
             * If prev_ggctx is NULL then we are freeing the top of the
             * contexts. So, we need to point the contexts variable to the
             * new (next) top context, if there is one.
             */
            if (prev_ggctx == NULL)
            {
                global_graph_contexts = next_ggctx;
            }
            else
            {
                prev_ggctx->next = curr_ggctx->next;
            }

            /* free the current graph context */
            success = free_specific_GRAPH_global_context(curr_ggctx);

            /* if it wasn't successfull, there was a missing vertex entry */
            if (!success)
            {

                ereport(ERROR, (errcode(ERRCODE_DATA_EXCEPTION),
                                errmsg("missing vertex or edge entry during free")));
            }
        }
        else
        {
            prev_ggctx = curr_ggctx;
        }

        /* advance to the next context */
        curr_ggctx = next_ggctx;
    }

    /* find our graph's context. if it exists, we are done */
    curr_ggctx = global_graph_contexts;
    while (curr_ggctx != NULL)
    {
        if (curr_ggctx->graph_oid == graph_oid)
        {
            /* switch our context back */
            MemoryContextSwitchTo(oldctx);


            return curr_ggctx;
        }
        curr_ggctx = curr_ggctx->next;
    }

    /*
     * Otherwise we need to build a new context for this graph.
     *
     * Build it DETACHED from the shared list, then attach it after a
     * successful build. The RLS-aware load path runs an SPI query that
     * evaluates user-defined row-level security policy expressions, and such
     * an expression can call back into AGE (for example a policy that invokes
     * a function performing its own VLE / global-graph traversal), re-entering
     * this function on the same backend. If the half-built context were
     * already on the shared list, that re-entrant call could find and use it
     * before it was fully loaded. Building detached avoids that; the
     * re-entrant call builds its own context and the duplicate is discarded
     * on attach below. (This code path holds no lock: the context list is
     * per-backend.)
     */
    new_ggctx = palloc0(sizeof(GRAPH_global_context));
    new_ggctx->next = NULL;

    /* set the graph name and oid */
    new_ggctx->graph_name = pstrdup(graph_name);
    new_ggctx->graph_oid = graph_oid;

    /* set the graph version counter for cache invalidation */
    new_ggctx->graph_version = get_graph_version(graph_oid);

    /* set snapshot fields for SNAPSHOT fallback mode */
    new_ggctx->xmin = GetActiveSnapshot()->xmin;
    new_ggctx->xmax = GetActiveSnapshot()->xmax;
    new_ggctx->curcid = GetActiveSnapshot()->curcid;

    /*
     * Record the role and enforcement GUC the cache is loaded under. The
     * SELECT ACL / RLS enforcement applied during load is role- and
     * GUC-dependent, so a cache built as one role (or GUC state) must not be
     * reused for another (see is_ggctx_invalid).
     */
    new_ggctx->load_as_role = GetUserId();
    new_ggctx->loaded_rls_enforced = age_enforce_rls_in_traversal;

    /* initialize our vertices list */
    new_ggctx->vertices = NULL;

    /* build the hashtables for this graph (detached from the shared list) */
    PG_TRY();
    {
        create_GRAPH_global_hashtables(new_ggctx);
        load_GRAPH_global_hashtables(new_ggctx);
        freeze_GRAPH_global_hashtables(new_ggctx);
    }
    PG_CATCH();
    {
        /*
         * The context is not attached to the shared list, so on a failed
         * load (for example an RLS or ACL error) just free the
         * partially-built context and re-throw. Its allocations live in
         * TopMemoryContext (backend lifetime), so without this a role that
         * repeatedly triggers a load failure - for example retrying a VLE on
         * a table it cannot read - would leak memory until the backend exits.
         * free_specific_GRAPH_global_context is safe on a partially-built
         * context: the vertices list and vertex_hashtable are kept in sync by
         * insert_vertex_entry, the edge_table lives in its own memory context,
         * and it only uses pfree / hash_search / hash_destroy /
         * MemoryContextDelete, none of which re-throw.
         */
        (void) free_specific_GRAPH_global_context(new_ggctx);
        MemoryContextSwitchTo(oldctx);
        PG_RE_THROW();
    }
    PG_END_TRY();

    /*
     * Attach. While we were building, a re-entrant call from an RLS policy on
     * this same backend may already have created and attached a context for
     * this graph. Reuse it only when it is still valid for the current
     * security context - same role, same enforcement GUC, and not loaded under
     * RLS - as tested by is_ggctx_invalid(). A re-entrant traversal reached
     * through a SECURITY DEFINER RLS policy can attach a context loaded as a
     * different role; returning it here would expose rows the current role may
     * not be allowed to see. If the existing context is not reusable, detach
     * and discard it so the list keeps a single, correct context per graph.
     */
    prev_ggctx = NULL;
    curr_ggctx = global_graph_contexts;
    while (curr_ggctx != NULL)
    {
        if (curr_ggctx->graph_oid == graph_oid)
        {
            if (!is_ggctx_invalid(curr_ggctx))
            {
                (void) free_specific_GRAPH_global_context(new_ggctx);
                MemoryContextSwitchTo(oldctx);
                return curr_ggctx;
            }

            /* detach the incompatible context and discard it */
            if (prev_ggctx == NULL)
            {
                global_graph_contexts = curr_ggctx->next;
            }
            else
            {
                prev_ggctx->next = curr_ggctx->next;
            }
            if (!free_specific_GRAPH_global_context(curr_ggctx))
            {
                /*
                 * The detached context was internally inconsistent (a
                 * vertex or edge entry went missing). Free our freshly-
                 * built context first so it does not leak, then surface
                 * the corruption as the other cleanup paths in this file
                 * do.
                 */
                (void) free_specific_GRAPH_global_context(new_ggctx);
                ereport(ERROR, (errcode(ERRCODE_DATA_EXCEPTION),
                                errmsg("missing vertex or edge entry during free")));
            }
            break;
        }
        prev_ggctx = curr_ggctx;
        curr_ggctx = curr_ggctx->next;
    }

    /* prepend our freshly-built context to the shared list */
    new_ggctx->next = global_graph_contexts;
    global_graph_contexts = new_ggctx;

    /* switch back to the previous memory context */
    MemoryContextSwitchTo(oldctx);

    return new_ggctx;
}

/*
 * Helper function to delete all of the global graph contexts used by the
 * process. When done the global global_graph_contexts will be NULL.
 *
 *
 */
static bool delete_GRAPH_global_contexts(void)
{
    GRAPH_global_context *curr_ggctx = NULL;
    bool retval = false;


    /* get the first context, if any */
    curr_ggctx = global_graph_contexts;

    /* free all GRAPH global contexts */
    while (curr_ggctx != NULL)
    {
        GRAPH_global_context *next_ggctx = curr_ggctx->next;
        bool success = false;

        /* free the current graph context */
        success = free_specific_GRAPH_global_context(curr_ggctx);

        /* if it wasn't successfull, there was a missing vertex entry */
        if (!success)
        {

            ereport(ERROR, (errcode(ERRCODE_DATA_EXCEPTION),
                            errmsg("missing vertex or edge entry during free")));
        }

        /* advance to the next context */
        curr_ggctx = next_ggctx;

        retval = true;
    }

    /* reset the head of the contexts to NULL */
    global_graph_contexts = NULL;


    return retval;
}

/*
 * Helper function to delete a specific global graph context used by the
 * process.
 */
static bool delete_specific_GRAPH_global_contexts(char *graph_name)
{
    GRAPH_global_context *prev_ggctx = NULL;
    GRAPH_global_context *curr_ggctx = NULL;
    Oid graph_oid = InvalidOid;

    if (graph_name == NULL)
    {
        return false;
    }

    /* get the graph oid */
    graph_oid = get_graph_oid(graph_name);


    /* get the first context, if any */
    curr_ggctx = global_graph_contexts;

    /* find the specified GRAPH global context */
    while (curr_ggctx != NULL)
    {
        GRAPH_global_context *next_ggctx = curr_ggctx->next;

        if (curr_ggctx->graph_oid == graph_oid)
        {
            bool success = false;
            /*
             * If prev_ggctx is NULL then we are freeing the top of the
             * contexts. So, we need to point the global variable to the
             * new (next) top context, if there is one.
             */
            if (prev_ggctx == NULL)
            {
                global_graph_contexts = next_ggctx;
            }
            else
            {
                prev_ggctx->next = curr_ggctx->next;
            }

            /* free the current graph context */
            success = free_specific_GRAPH_global_context(curr_ggctx);


            /* if it wasn't successfull, there was a missing vertex entry */
            if (!success)
            {
                ereport(ERROR, (errcode(ERRCODE_DATA_EXCEPTION),
                                errmsg("missing vertex_entry during free")));
            }

            /* we found and freed it, return true */
            return true;
        }

        /* save the current as previous and advance to the next one */
        prev_ggctx = curr_ggctx;
        curr_ggctx = next_ggctx;
    }


    /* we didn't find it, return false */
    return false;
}

/*
 * Helper function to retrieve a vertex_entry from the graph's vertex hash
 * table. If there isn't one, it returns a NULL. The latter is necessary for
 * checking if the vsid and veid entries exist.
 */
vertex_entry *get_vertex_entry(GRAPH_global_context *ggctx, graphid vertex_id)
{
    vertex_entry *ve = NULL;
    bool found = false;

    /* retrieve the current vertex entry */
    ve = (vertex_entry *)hash_search(ggctx->vertex_hashtable,
                                     (void *)&vertex_id, HASH_FIND, &found);
    return ve;
}

/* helper function to retrieve an edge_entry from the graph's edge table */
edge_entry *get_edge_entry(GRAPH_global_context *ggctx, graphid edge_id)
{
    edge_entry *ee;

    ee = (edge_entry *) agehash_lookup(ggctx->edge_table, (void *) &edge_id);
    /* it should be found, otherwise we have problems */
    Assert(ee != NULL);

    return ee;
}

/*
 * Variant of get_edge_entry that uses a precomputed hash value to skip the
 * agehash internal hash callback. The caller is responsible for ensuring
 * hashvalue == graphid_hash(&edge_id, sizeof(int64)). Used by the VLE DFS
 * hot loop where the same edge_id is also looked up in edge_state_hashtable.
 */
edge_entry *get_edge_entry_with_hash(GRAPH_global_context *ggctx,
                                     graphid edge_id, uint32 hashvalue)
{
    edge_entry *ee;

    ee = (edge_entry *) agehash_lookup_with_hash(ggctx->edge_table,
                                                 (void *) &edge_id,
                                                 hashvalue);
    Assert(ee != NULL);

    return ee;
}

/*
 * Helper function to find the GRAPH_global_context used by the specified
 * graph_oid. If not found, it returns NULL.
 */
GRAPH_global_context *find_GRAPH_global_context(Oid graph_oid)
{
    GRAPH_global_context *ggctx = NULL;


    /* get the root */
    ggctx = global_graph_contexts;

    while(ggctx != NULL)
    {
        /* if we found it return it */
        if (ggctx->graph_oid == graph_oid)
        {

            return ggctx;
        }

        /* advance to the next context */
        ggctx = ggctx->next;
    }


    /* we did not find it so return NULL */
    return NULL;
}

/* graph vertices accessor */
ListGraphId *get_graph_vertices(GRAPH_global_context *ggctx)
{
    return ggctx->vertices;
}

/* vertex_entry accessor functions */
graphid get_vertex_entry_id(vertex_entry *ve)
{
    return ve->vertex_id;
}

VertexEdgeArray *get_vertex_entry_edges_in_array(vertex_entry *ve)
{
    return &ve->edges_in;
}

VertexEdgeArray *get_vertex_entry_edges_out_array(vertex_entry *ve)
{
    return &ve->edges_out;
}

VertexEdgeArray *get_vertex_entry_edges_self_array(vertex_entry *ve)
{
    return &ve->edges_self;
}


Oid get_vertex_entry_label_table_oid(vertex_entry *ve)
{
    return ve->vertex_label_table_oid;
}

/*
 * Outcome of fetch_entry_properties(). A missing row and a row whose properties
 * are NULL are different failures: the first means the cache is stale, the
 * second means the label table no longer satisfies the NOT NULL that AGE
 * creates it with. Reporting them apart keeps a schema problem from being
 * described as a cache problem.
 */
typedef enum entry_fetch_status
{
    ENTRY_FETCH_OK,
    ENTRY_FETCH_GONE,
    ENTRY_FETCH_NULL_PROPS
} entry_fetch_status;

/*
 * Read one cached entry's properties out of the heap.
 *
 * The stored TID normally resolves under the active snapshot. It does not once
 * this statement has deleted the tuple: cypher_delete() advances
 * es_snapshot->curcid past every delete, so a path bound by an earlier MATCH
 * can no longer see its own endpoints by the time it is projected (issue
 * #2549). That tuple is still physically present -- our transaction has not
 * committed, so nothing may prune it -- and the properties it carried when the
 * path was matched are what the path should report, so it is read anyway.
 *
 * The relaxation is deliberately narrow: only a tuple deleted by our own
 * transaction qualifies, and only while the row still holds the entity that was
 * cached, so a line pointer recycled by vacuum cannot be mistaken for the
 * original. Anything else leaves *found false and the caller reports a stale
 * entry, which is what keeps a genuine cache-invalidation bug visible.
 *
 * The value is detoasted here, under the buffer pin, so no caller is left
 * holding an external pointer into a tuple that is logically gone.
 */
static Datum fetch_entry_properties(Oid label_table_oid, ItemPointer tid,
                                    graphid expected_id, AttrNumber id_attnum,
                                    AttrNumber props_attnum,
                                    entry_fetch_status *status)
{
    Relation rel;
    TupleDesc tupdesc;
    HeapTupleData tuple;
    Buffer buffer = InvalidBuffer;
    Datum result = (Datum) 0;
    bool usable;
    bool isnull;

    *status = ENTRY_FETCH_GONE;

    rel = table_open(label_table_oid, AccessShareLock);
    tupdesc = RelationGetDescr(rel);
    tuple.t_self = *tid;

    /*
     * keep_buf leaves the tuple readable when the fetch fails on visibility
     * alone; a line pointer that is gone clears t_data and returns no buffer.
     */
    usable = heap_fetch(rel, GetActiveSnapshot(), &tuple, &buffer, true);

    if (!usable && BufferIsValid(buffer))
    {
        TransactionId xmax = HeapTupleHeaderGetUpdateXid(tuple.t_data);

        if (TransactionIdIsValid(xmax) &&
            TransactionIdIsCurrentTransactionId(xmax))
        {
            Datum id = heap_getattr(&tuple, id_attnum, tupdesc, &isnull);

            usable = !isnull && DATUM_GET_GRAPHID(id) == expected_id;
        }
    }

    if (usable)
    {
        Datum props = heap_getattr(&tuple, props_attnum, tupdesc, &isnull);

        if (isnull)
        {
            *status = ENTRY_FETCH_NULL_PROPS;
        }
        else
        {
            result = PointerGetDatum(PG_DETOAST_DATUM_COPY(props));
            *status = ENTRY_FETCH_OK;
        }
    }

    if (BufferIsValid(buffer))
    {
        ReleaseBuffer(buffer);
    }

    table_close(rel, AccessShareLock);

    return result;
}

/*
 * Fetch vertex properties on demand from the heap via stored TID.
 *
 * Returns a detoasted copy of the properties in the current memory context.
 * The caller does not need to free the result explicitly — it will be
 * freed when the memory context is reset (typically the SRF multi-call
 * context for VLE, which is cleaned up when the SRF completes).
 *
 * A tuple this transaction has already deleted is still reported, carrying the
 * properties it held when the path was matched; see fetch_entry_properties.
 * Any other unreachable TID means the version counter failed to invalidate the
 * cache, and is raised as an error.
 */
Datum get_vertex_entry_properties(vertex_entry *ve)
{
    Datum result;
    entry_fetch_status status;

    result = fetch_entry_properties(ve->vertex_label_table_oid, &ve->tid,
                                    ve->vertex_id,
                                    Anum_ag_label_vertex_table_id,
                                    Anum_ag_label_vertex_table_properties,
                                    &status);

    if (status == ENTRY_FETCH_NULL_PROPS)
    {
        elog(ERROR, "get_vertex_entry_properties: vertex " INT64_FORMAT
             " has null properties", ve->vertex_id);
    }

    if (status != ENTRY_FETCH_OK)
    {
        elog(ERROR, "get_vertex_entry_properties: stale TID - "
             "vertex entry references a tuple that is no longer visible");
    }

    return result;
}

/* edge_entry accessor functions */
graphid get_edge_entry_id(edge_entry *ee)
{
    /*
     * The edge_id is stored as the agehash slot key, immediately preceding
     * the payload pointer we hand back as `edge_entry *`. Recover it via
     * the public agehash_key_from_payload helper to avoid a redundant
     * 8-byte field on every entry (saves ~400MB on SF3, ~1.4GB on SF10).
     */
    graphid k;
    memcpy(&k, agehash_key_from_payload(ee, sizeof(graphid)), sizeof(graphid));
    return k;
}

Oid get_edge_entry_label_table_oid(edge_entry *ee)
{
    return ee->edge_label_table_oid;
}

/*
 * Fetch edge properties on demand from the heap via stored TID.
 * See get_vertex_entry_properties for memory and safety notes.
 */
Datum get_edge_entry_properties(edge_entry *ee)
{
    Datum result;
    entry_fetch_status status;

    result = fetch_entry_properties(ee->edge_label_table_oid, &ee->tid,
                                    get_edge_entry_id(ee),
                                    Anum_ag_label_edge_table_id,
                                    Anum_ag_label_edge_table_properties,
                                    &status);

    if (status == ENTRY_FETCH_NULL_PROPS)
    {
        elog(ERROR, "get_edge_entry_properties: edge " INT64_FORMAT
             " has null properties", get_edge_entry_id(ee));
    }

    if (status != ENTRY_FETCH_OK)
    {
        elog(ERROR, "get_edge_entry_properties: stale TID - "
             "edge entry references a tuple that is no longer visible");
    }

    return result;
}

graphid get_edge_entry_start_vertex_id(edge_entry *ee)
{
    return ee->start_vertex_id;
}

graphid get_edge_entry_end_vertex_id(edge_entry *ee)
{
    return ee->end_vertex_id;
}

/* PostgreSQL SQL facing functions */

/* PG wrapper function for age_delete_global_graphs */
PG_FUNCTION_INFO_V1(age_delete_global_graphs);

Datum age_delete_global_graphs(PG_FUNCTION_ARGS)
{
    agtype_value *agtv_temp = NULL;
    bool success = false;

    /* get the graph name if supplied */
    if (!PG_ARGISNULL(0))
    {
        agtv_temp = get_agtype_value("delete_global_graphs",
                                     AG_GET_ARG_AGTYPE_P(0),
                                     AGTV_STRING, false);
    }

    if (agtv_temp == NULL || agtv_temp->type == AGTV_NULL)
    {
        success = delete_GRAPH_global_contexts();
    }
    else if (agtv_temp->type == AGTV_STRING)
    {
        char *graph_name = NULL;

        graph_name = pnstrdup(agtv_temp->val.string.val,
                              agtv_temp->val.string.len);

        success = delete_specific_GRAPH_global_contexts(graph_name);
    }
    else
    {
        ereport(ERROR,
                (errcode(ERRCODE_INVALID_PARAMETER_VALUE),
                 errmsg("delete_global_graphs: invalid graph name type")));
    }

    PG_RETURN_BOOL(success);
}

/* PG wrapper function for age_vertex_degree */
PG_FUNCTION_INFO_V1(age_vertex_stats);

Datum age_vertex_stats(PG_FUNCTION_ARGS)
{
    GRAPH_global_context *ggctx = NULL;
    vertex_entry *ve = NULL;
    VertexEdgeArray *edges = NULL;
    agtype_value *agtv_vertex = NULL;
    agtype_value *agtv_temp = NULL;
    agtype_value agtv_integer;
    agtype_in_state result;
    char *graph_name = NULL;
    Oid graph_oid = InvalidOid;
    graphid vid = 0;
    int64 self_loops = 0;
    int64 degree = 0;

    /* the graph name is required, but this generally isn't user supplied */
    if (PG_ARGISNULL(0))
    {
        ereport(ERROR,
                (errcode(ERRCODE_INVALID_PARAMETER_VALUE),
                 errmsg("vertex_stats: graph name cannot be NULL")));
    }

    /* get the graph name */
    agtv_temp = get_agtype_value("vertex_stats", AG_GET_ARG_AGTYPE_P(0),
                                 AGTV_STRING, true);

    /* we need the vertex */
    if (PG_ARGISNULL(1))
    {
        ereport(ERROR,
                (errcode(ERRCODE_INVALID_PARAMETER_VALUE),
                 errmsg("vertex_stats: vertex cannot be NULL")));
    }

    /* get the vertex */
    agtv_vertex = get_agtype_value("vertex_stats", AG_GET_ARG_AGTYPE_P(1),
                                   AGTV_VERTEX, true);

    graph_name = pnstrdup(agtv_temp->val.string.val,
                          agtv_temp->val.string.len);

    /* get the graph oid */
    graph_oid = get_graph_oid(graph_name);

    /*
     * Create or retrieve the GRAPH global context for this graph. This function
     * will also purge off invalidated contexts.
     */
    ggctx = manage_GRAPH_global_contexts(graph_name, graph_oid);

    /* free the graph name */
    pfree_if_not_null(graph_name);

    /* get the id */
    agtv_temp = GET_AGTYPE_VALUE_OBJECT_VALUE(agtv_vertex, "id");
    vid = agtv_temp->val.int_value;

    /* get the vertex entry */
    ve = get_vertex_entry(ggctx, vid);

    /* zero the state */
    memset(&result, 0, sizeof(agtype_in_state));

    /* start the object */
    result.res = push_agtype_value(&result.parse_state, WAGT_BEGIN_OBJECT,
                                   NULL);
    /* store the id */
    result.res = push_agtype_value(&result.parse_state, WAGT_KEY,
                                   string_to_agtype_value("id"));
    result.res = push_agtype_value(&result.parse_state, WAGT_VALUE, agtv_temp);

    /* store the label */
    agtv_temp = GET_AGTYPE_VALUE_OBJECT_VALUE(agtv_vertex, "label");
    result.res = push_agtype_value(&result.parse_state, WAGT_KEY,
                                   string_to_agtype_value("label"));
    result.res = push_agtype_value(&result.parse_state, WAGT_VALUE, agtv_temp);

    /* set up an integer for returning values */
    agtv_temp = &agtv_integer;
    agtv_temp->type = AGTV_INTEGER;
    agtv_temp->val.int_value = 0;

    /* get and store the self_loops */
    edges = get_vertex_entry_edges_self_array(ve);
    self_loops = edges->size;
    agtv_temp->val.int_value = self_loops;
    result.res = push_agtype_value(&result.parse_state, WAGT_KEY,
                                   string_to_agtype_value("self_loops"));
    result.res = push_agtype_value(&result.parse_state, WAGT_VALUE, agtv_temp);

    /* get and store the in_degree */
    edges = get_vertex_entry_edges_in_array(ve);
    degree = edges->size;
    agtv_temp->val.int_value = degree + self_loops;
    result.res = push_agtype_value(&result.parse_state, WAGT_KEY,
                                   string_to_agtype_value("in_degree"));
    result.res = push_agtype_value(&result.parse_state, WAGT_VALUE, agtv_temp);

    /* get and store the out_degree */
    edges = get_vertex_entry_edges_out_array(ve);
    degree = edges->size;
    agtv_temp->val.int_value = degree + self_loops;
    result.res = push_agtype_value(&result.parse_state, WAGT_KEY,
                                   string_to_agtype_value("out_degree"));
    result.res = push_agtype_value(&result.parse_state, WAGT_VALUE, agtv_temp);

    /* close the object */
    result.res = push_agtype_value(&result.parse_state, WAGT_END_OBJECT, NULL);

    result.res->type = AGTV_OBJECT;

    PG_RETURN_POINTER(agtype_value_to_agtype(result.res));
}

/* PG wrapper function for age_graph_stats */
PG_FUNCTION_INFO_V1(age_graph_stats);

Datum age_graph_stats(PG_FUNCTION_ARGS)
{
    GRAPH_global_context *ggctx = NULL;
    agtype_value *agtv_temp = NULL;
    agtype_value agtv_integer;
    agtype_in_state result;
    char *graph_name = NULL;
    Oid graph_oid = InvalidOid;

    /* the graph name is required, but this generally isn't user supplied */
    if (PG_ARGISNULL(0))
    {
        ereport(ERROR,
                (errcode(ERRCODE_INVALID_PARAMETER_VALUE),
                 errmsg("graph_stats: graph name cannot be NULL")));
    }

    /* get the graph name */
    agtv_temp = get_agtype_value("graph_stats", AG_GET_ARG_AGTYPE_P(0),
                                 AGTV_STRING, true);

    graph_name = pnstrdup(agtv_temp->val.string.val,
                          agtv_temp->val.string.len);

    /*
     * Remove any context for this graph. This is done to allow graph_stats to
     * show any load issues.
     */
    delete_specific_GRAPH_global_contexts(graph_name);

    /* get the graph oid */
    graph_oid = get_graph_oid(graph_name);

    /*
     * Create or retrieve the GRAPH global context for this graph. This function
     * will also purge off invalidated contexts.
     */
    ggctx = manage_GRAPH_global_contexts(graph_name, graph_oid);

    /* free the graph name */
    pfree_if_not_null(graph_name);

    /* zero the state */
    memset(&result, 0, sizeof(agtype_in_state));

    /* start the object */
    result.res = push_agtype_value(&result.parse_state, WAGT_BEGIN_OBJECT,
                                   NULL);
    /* store the graph name */
    result.res = push_agtype_value(&result.parse_state, WAGT_KEY,
                                   string_to_agtype_value("graph"));
    result.res = push_agtype_value(&result.parse_state, WAGT_VALUE, agtv_temp);

    /* set up an integer for returning values */
    agtv_temp = &agtv_integer;
    agtv_temp->type = AGTV_INTEGER;
    agtv_temp->val.int_value = 0;

    /* get and store num_loaded_vertices */
    agtv_temp->val.int_value = ggctx->num_loaded_vertices;
    result.res = push_agtype_value(&result.parse_state, WAGT_KEY,
                                   string_to_agtype_value("num_loaded_vertices"));
    result.res = push_agtype_value(&result.parse_state, WAGT_VALUE, agtv_temp);

    /* get and store num_loaded_edges */
    agtv_temp->val.int_value = ggctx->num_loaded_edges;
    result.res = push_agtype_value(&result.parse_state, WAGT_KEY,
                                   string_to_agtype_value("num_loaded_edges"));
    result.res = push_agtype_value(&result.parse_state, WAGT_VALUE, agtv_temp);

    /* close the object */
    result.res = push_agtype_value(&result.parse_state, WAGT_END_OBJECT, NULL);

    result.res->type = AGTV_OBJECT;

    PG_RETURN_POINTER(agtype_value_to_agtype(result.res));
}

/*
 * ============================================================================
 * Graph Version Counter Implementation
 *
 * Provides per-graph monotonic version counters in shared memory for
 * cross-backend VLE cache invalidation. Three modes are supported:
 *
 * DSM (PG 17+):  Uses GetNamedDSMSegment — works without shared_preload_libs
 * SHMEM (PG <17): Uses shmem_request/startup hooks — needs shared_preload_libs
 * SNAPSHOT:       Falls back to original snapshot-based invalidation
 * ============================================================================
 */

#if PG_VERSION_NUM >= 170000
/*
 * DSM path: GetNamedDSMSegment init callback.
 * Called once when the DSM segment is first created.
 */
static void age_dsm_init_callback(void *ptr)
{
    GraphVersionState *state = (GraphVersionState *) ptr;

    LWLockInitialize(&state->lock,
                     LWLockNewTrancheId());
    LWLockRegisterTranche(state->lock.tranche, "age_graph_version");
    state->num_entries = 0;
    memset(state->entries, 0, sizeof(state->entries));
}

/*
 * Get the shared GraphVersionState via DSM registry.
 * The segment is created on first access and persists until server shutdown.
 */
static GraphVersionState *get_version_state_dsm(void)
{
    bool found;

    return (GraphVersionState *)
        GetNamedDSMSegment("age_graph_versions",
                           sizeof(GraphVersionState),
                           age_dsm_init_callback,
                           &found);
}
#endif /* PG_VERSION_NUM >= 170000 */

/*
 * SHMEM path: request and startup hooks for PG < 17.
 * These are registered in _PG_init when shared_preload_libraries is used.
 * On PG 17+, DSM is used instead and these functions are not called.
 */
#if PG_VERSION_NUM < 170000
void age_graph_version_shmem_request(void)
{
    RequestAddinShmemSpace(MAXALIGN(sizeof(GraphVersionState)));
}

void age_graph_version_shmem_startup(void)
{
    bool found;

    LWLockAcquire(AddinShmemInitLock, LW_EXCLUSIVE);

    shmem_version_state =
        (GraphVersionState *) ShmemInitStruct("AGE Graph Version State",
                                              sizeof(GraphVersionState),
                                              &found);
    if (!found)
    {
        LWLockInitialize(&shmem_version_state->lock,
                         LWLockNewTrancheId());
        LWLockRegisterTranche(shmem_version_state->lock.tranche,
                              "age_graph_version");
        shmem_version_state->num_entries = 0;
        memset(shmem_version_state->entries, 0,
               sizeof(shmem_version_state->entries));
    }

    LWLockRelease(AddinShmemInitLock);
}
#endif /* PG_VERSION_NUM < 170000 */

/*
 * Detect which version mode to use. Called once per backend on first access.
 * Emits a DEBUG1 log message indicating the chosen mode.
 */
static void detect_version_mode(void)
{
#if PG_VERSION_NUM >= 170000
    version_mode = VERSION_MODE_DSM;
    elog(DEBUG1, "AGE: VLE cache using DSM version counter");
#else
    if (shmem_version_state != NULL)
    {
        version_mode = VERSION_MODE_SHMEM;
        elog(DEBUG1, "AGE: VLE cache using SHMEM version counter");
    }
    else
    {
        version_mode = VERSION_MODE_SNAPSHOT;
        elog(DEBUG1, "AGE: VLE cache using snapshot-based invalidation "
             "(add AGE to shared_preload_libraries for better caching)");
    }
#endif
}

/*
 * Get a pointer to the GraphVersionState, regardless of mode.
 * Returns NULL only in SNAPSHOT mode (no shared memory available).
 */
static GraphVersionState *get_version_state(void)
{
    if (version_mode == VERSION_MODE_UNKNOWN)
    {
        detect_version_mode();
    }

#if PG_VERSION_NUM >= 170000
    if (version_mode == VERSION_MODE_DSM)
    {
        return get_version_state_dsm();
    }
#endif

    if (version_mode == VERSION_MODE_SHMEM)
    {
        return shmem_version_state;
    }

    return NULL;
}

/*
 * Get the current version counter for a graph.
 * Returns 0 if the graph has never been tracked or if shared memory
 * is not available. Lock-free read via pg_atomic_read_u64.
 */
uint64 get_graph_version(Oid graph_oid)
{
    GraphVersionState *state = get_version_state();
    int i;

    if (state == NULL)
    {
        return 0;
    }

    /* lock-free scan of the array */
    for (i = 0; i < state->num_entries; i++)
    {
        if (state->entries[i].graph_oid == graph_oid)
        {
            return pg_atomic_read_u64(&state->entries[i].version);
        }
    }

    return 0;
}

/*
 * Increment the version counter for a graph.
 * Called after any graph mutation (Cypher or SQL trigger).
 * Lock-free for existing entries; acquires LWLock only to allocate new slots.
 */
void increment_graph_version(Oid graph_oid)
{
    GraphVersionState *state = get_version_state();
    int i;

    if (state == NULL)
    {
        return;
    }

    /* try to find existing entry (lock-free) */
    for (i = 0; i < state->num_entries; i++)
    {
        if (state->entries[i].graph_oid == graph_oid)
        {
            pg_atomic_fetch_add_u64(&state->entries[i].version, 1);
            return;
        }
    }

    /* new graph — need lock to allocate slot */
    LWLockAcquire(&state->lock, LW_EXCLUSIVE);

    /* re-check after acquiring lock (another backend may have added it) */
    for (i = 0; i < state->num_entries; i++)
    {
        if (state->entries[i].graph_oid == graph_oid)
        {
            LWLockRelease(&state->lock);
            pg_atomic_fetch_add_u64(&state->entries[i].version, 1);
            return;
        }
    }

    /* take a slot, preferring one left behind by a dropped graph */
    {
        int idx = -1;
        bool appending;

        for (i = 0; i < state->num_entries; i++)
        {
            if (state->entries[i].graph_oid == InvalidOid)
            {
                idx = i;
                break;
            }
        }

        if (idx < 0 && state->num_entries < AGE_MAX_GRAPHS)
        {
            idx = state->num_entries;
        }

        if (idx < 0)
        {
            elog(WARNING, "AGE: graph version counter table full (%d graphs)",
                 AGE_MAX_GRAPHS);
            LWLockRelease(&state->lock);
            return;
        }

        appending = (idx == state->num_entries);

        /*
         * Seed above every version this table has ever issued, freed slots
         * included, so the sequence never repeats a value. A context cached
         * against this slot's previous occupant -- or against an earlier graph
         * that happened to reuse this OID -- then cannot compare equal by
         * coincidence and be mistaken for current.
         */
        {
            uint64 seed = 0;
            int j;

            for (j = 0; j < state->num_entries; j++)
            {
                uint64 v = pg_atomic_read_u64(&state->entries[j].version);

                if (v > seed)
                {
                    seed = v;
                }
            }

            if (appending)
            {
                pg_atomic_init_u64(&state->entries[idx].version, seed + 1);
            }
            else
            {
                pg_atomic_write_u64(&state->entries[idx].version, seed + 1);
            }
        }

        /*
         * Publish the version before the oid: readers match on the oid without
         * the lock and must never find a slot whose version is not yet set.
         */
        pg_write_barrier();
        state->entries[idx].graph_oid = graph_oid;

        if (appending)
        {
            /*
             * Write barrier ensures the entry fields are fully visible to
             * other backends before num_entries is incremented. This prevents
             * readers on weak memory-ordering architectures (e.g., ARM) from
             * seeing the incremented count before the entry is initialized.
             */
            pg_write_barrier();
            state->num_entries++;
        }
    }

    LWLockRelease(&state->lock);
}

/*
 * Release a dropped graph's slot so another graph can use it.
 *
 * Without this the table is a tally of every graph ever mutated, and a server
 * that cycles graphs eventually fills it: further graphs go untracked, every
 * mutation warns, and their contexts fall back to snapshot comparison, which is
 * correct but invalidates far more often.
 *
 * The version is deliberately left in the freed slot. It is part of the
 * high-water mark the next occupant seeds above, which is what stops a stale
 * context from matching a later graph.
 */
void release_graph_version(Oid graph_oid)
{
    GraphVersionState *state = get_version_state();
    int i;

    if (state == NULL || !OidIsValid(graph_oid))
    {
        return;
    }

    LWLockAcquire(&state->lock, LW_EXCLUSIVE);

    for (i = 0; i < state->num_entries; i++)
    {
        if (state->entries[i].graph_oid == graph_oid)
        {
            /*
             * Clearing the oid is enough to retire the slot: a lock-free
             * reader matches on it, so it stops finding this graph and falls
             * back to snapshot comparison until the graph is registered again.
             */
            state->entries[i].graph_oid = InvalidOid;
            break;
        }
    }

    LWLockRelease(&state->lock);
}

/*
 * Bump the version of every tracked graph.
 *
 * For commands that rewrite a heap without naming one: VACUUM FULL or CLUSTER
 * over a whole database. A graph absent from this table has never been mutated
 * through the counter, so its contexts are still validated by snapshot
 * comparison in is_ggctx_invalid() and need no bump.
 */
void increment_all_graph_versions(void)
{
    GraphVersionState *state = get_version_state();
    int i;

    if (state == NULL)
    {
        return;
    }

    /*
     * num_entries only grows, and entries are published with a write barrier
     * before it is incremented, so reading it without the lock can miss a
     * brand-new graph but never sees a half-built entry. A graph added after
     * this read has no cached context to invalidate yet.
     */
    for (i = 0; i < state->num_entries; i++)
    {
        if (state->entries[i].graph_oid != InvalidOid)
        {
            pg_atomic_fetch_add_u64(&state->entries[i].version, 1);
        }
    }
}

/*
 * Helper function to look up the graph OID for a given label table OID.
 * Uses AGE's label relation cache for fast lookup.
 * Returns InvalidOid if the table is not a graph label table.
 */
Oid get_graph_oid_for_table(Oid table_oid)
{
    label_cache_data *lcd = NULL;

    lcd = search_label_relation_cache(table_oid);

    if (lcd != NULL)
    {
        return lcd->graph;
    }

    return InvalidOid;
}

/*
 * SQL-callable trigger function for VLE cache invalidation.
 * Installed on graph label tables (AFTER INSERT/UPDATE/DELETE FOR EACH STATEMENT).
 * Looks up which graph the triggering table belongs to and increments
 * that graph's version counter.
 */
PG_FUNCTION_INFO_V1(age_invalidate_graph_cache);

Datum age_invalidate_graph_cache(PG_FUNCTION_ARGS)
{
    TriggerData *trigdata;
    Oid table_oid;
    Oid graph_oid;

    /* verify called as trigger */
    if (!CALLED_AS_TRIGGER(fcinfo))
    {
        ereport(ERROR,
                (errcode(ERRCODE_E_R_I_E_TRIGGER_PROTOCOL_VIOLATED),
                 errmsg("age_invalidate_graph_cache: not called as trigger")));
    }

    trigdata = (TriggerData *) fcinfo->context;
    table_oid = RelationGetRelid(trigdata->tg_relation);

    /* look up which graph this label table belongs to */
    graph_oid = get_graph_oid_for_table(table_oid);

    if (OidIsValid(graph_oid))
    {
        increment_graph_version(graph_oid);
    }

    /*
     * Trigger protocol: return a null pointer without setting fcinfo->isnull.
     * PG_RETURN_NULL() sets isnull=true, which violates the trigger protocol
     * and causes "trigger function returned null value" errors during COPY.
     */
    PG_RETURN_POINTER(NULL);
}
