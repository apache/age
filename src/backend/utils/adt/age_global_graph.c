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

#include "access/genam.h"
#include "access/heapam.h"
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

#if PG_VERSION_NUM >= 170000
#include "storage/dsm_registry.h"
#else
#include "storage/ipc.h"
#include "storage/shmem.h"
#endif

#include "utils/age_global_graph.h"
#include "catalog/ag_graph.h"
#include "catalog/ag_label.h"
#include "utils/ag_cache.h"

#include <pthread.h>

/* defines */
#define VERTEX_HTAB_NAME "Vertex to edge lists " /* added a space at end for */
#define EDGE_HTAB_NAME "Edge to vertex mapping " /* the graph name to follow */
#define VERTEX_HTAB_INITIAL_SIZE 1024
#define EDGE_HTAB_INITIAL_SIZE 1024

/* Maximum number of graphs tracked for version counting */
#define AGE_MAX_GRAPHS 128

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

typedef struct EntryPropertyRelationCacheEntry
{
    Oid relid;
    Relation rel;
} EntryPropertyRelationCacheEntry;

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
static Oid cached_version_graph_oid = InvalidOid;
static int cached_version_graph_index = -1;

/* For PG < 17 shmem path */
static GraphVersionState *shmem_version_state = NULL;

/* internal data structures implementation */

/* vertex entry for the vertex_hashtable */
typedef struct vertex_entry
{
    graphid vertex_id;             /* vertex id, it is also the hash key */
    ListGraphId *edges_in;         /* List of entering edges graphids (int64) */
    ListGraphId *edges_out;        /* List of exiting edges graphids (int64) */
    ListGraphId *edges_self;       /* List of selfloop edges graphids (int64) */
    Oid vertex_label_table_oid;    /* the label table oid */
    char *vertex_label_name;       /* the label name */
    ItemPointerData tid;           /* physical tuple location for lazy fetch */
} vertex_entry;

/* edge entry for the edge_hashtable */
typedef struct edge_entry
{
    graphid edge_id;               /* edge id, it is also the hash key */
    Oid edge_label_table_oid;      /* the label table oid */
    char *edge_label_name;         /* the label name */
    ItemPointerData tid;           /* physical tuple location for lazy fetch */
    graphid start_vertex_id;       /* start vertex */
    graphid end_vertex_id;         /* end vertex */
} edge_entry;

typedef struct graph_label_entry
{
    NameData name;
    Oid relation;
} graph_label_entry;

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
    HTAB *edge_hashtable;          /* hashtable to hold edge to vertex map */
    uint64 graph_version;          /* version counter for cache invalidation */
    TransactionId xmin;            /* snapshot fallback: transaction xmin */
    TransactionId xmax;            /* snapshot fallback: transaction xmax */
    CommandId curcid;              /* snapshot fallback: command id */
    int64 num_loaded_vertices;     /* number of loaded vertices in this graph */
    int64 num_loaded_edges;        /* number of loaded edges in this graph */
    ListGraphId *vertices;         /* vertices for vertex hashtable cleanup */
    struct GRAPH_global_context *next; /* next graph */
} GRAPH_global_context;

/* container for GRAPH_global_context and its mutex */
typedef struct GRAPH_global_context_container
{
    /* head of the list */
    GRAPH_global_context *contexts;

    /* mutex to protect the list */
    pthread_mutex_t mutex_lock;
} GRAPH_global_context_container;

/* global variable to hold the per process GRAPH global contexts */
static GRAPH_global_context_container global_graph_contexts_container = {0};

/* declarations */
/* GRAPH global context functions */
static bool free_specific_GRAPH_global_context(GRAPH_global_context *ggctx);
static GRAPH_global_context *manage_GRAPH_global_contexts_internal(
    const char *graph_name, int graph_name_len, Oid graph_oid);
static bool delete_specific_GRAPH_global_context_by_oid(Oid graph_oid);
static bool delete_specific_GRAPH_global_contexts_len(const char *graph_name,
                                                      int graph_name_len);
static bool delete_GRAPH_global_contexts(void);
static Oid get_cached_global_graph_oid_len(const char *graph_name,
                                           int graph_name_len);
static void get_global_graph_scalar_arg_no_copy(char *funcname,
                                                agtype *agt_arg,
                                                enum agtype_value_type type,
                                                bool error,
                                                agtype_value *result,
                                                bool *needs_free);
static void create_GRAPH_global_hashtables(GRAPH_global_context *ggctx);
static void load_GRAPH_global_hashtables(GRAPH_global_context *ggctx);
static void load_vertex_hashtable(GRAPH_global_context *ggctx,
                                  Snapshot snapshot, List *vertex_labels);
static void load_edge_hashtable(GRAPH_global_context *ggctx,
                                Snapshot snapshot, List *edge_labels);
static void freeze_GRAPH_global_hashtables(GRAPH_global_context *ggctx);
static void get_ag_labels_names(Snapshot snapshot, Oid graph_oid,
                                List **vertex_labels, List **edge_labels);
static void free_ag_label_names(List *labels);
static bool insert_edge_entry(GRAPH_global_context *ggctx, graphid edge_id,
                              ItemPointerData tid, graphid start_vertex_id,
                              graphid end_vertex_id, Oid edge_label_table_oid,
                              char *edge_label_name);
static bool insert_vertex_edge(GRAPH_global_context *ggctx,
                               graphid start_vertex_id, graphid end_vertex_id,
                               graphid edge_id, char *edge_label_name);
static bool insert_vertex_entry(GRAPH_global_context *ggctx, graphid vertex_id,
                                Oid vertex_label_table_oid,
                                char *vertex_label_name,
                                ItemPointerData tid);
static Datum fetch_entry_properties(Oid relid, ItemPointerData tid,
                                    AttrNumber property_attr,
                                    HTAB *relation_cache,
                                    const char *stale_msg);
static Relation get_entry_property_relation(Oid relid, HTAB *relation_cache);
static bool is_ggctx_invalid_with_snapshot(GRAPH_global_context *ggctx,
                                           Snapshot *snapshot);
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
    Snapshot snapshot = NULL;

    return is_ggctx_invalid_with_snapshot(ggctx, &snapshot);
}

static bool is_ggctx_invalid_with_snapshot(GRAPH_global_context *ggctx,
                                           Snapshot *snapshot)
{
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
    if (*snapshot == NULL)
    {
        *snapshot = GetActiveSnapshot();
    }

    return (ggctx->xmin != (*snapshot)->xmin ||
            ggctx->xmax != (*snapshot)->xmax ||
            ggctx->curcid != (*snapshot)->curcid);
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
    HASHCTL edge_ctl;
    char *graph_name = NULL;
    char *vhn = NULL;
    char *ehn = NULL;

    /* get the graph name */
    graph_name = ggctx->graph_name;
    vhn = psprintf("%s%s", VERTEX_HTAB_NAME, graph_name);
    ehn = psprintf("%s%s", EDGE_HTAB_NAME, graph_name);

    /* initialize the vertex hashtable */
    MemSet(&vertex_ctl, 0, sizeof(vertex_ctl));
    vertex_ctl.keysize = sizeof(int64);
    vertex_ctl.entrysize = sizeof(vertex_entry);
    vertex_ctl.hash = tag_hash;
    ggctx->vertex_hashtable = hash_create(vhn, VERTEX_HTAB_INITIAL_SIZE,
                                          &vertex_ctl,
                                          HASH_ELEM | HASH_FUNCTION);
    pfree_if_not_null(vhn);

    /* initialize the edge hashtable */
    MemSet(&edge_ctl, 0, sizeof(edge_ctl));
    edge_ctl.keysize = sizeof(int64);
    edge_ctl.entrysize = sizeof(edge_entry);
    edge_ctl.hash = tag_hash;
    ggctx->edge_hashtable = hash_create(ehn, EDGE_HTAB_INITIAL_SIZE, &edge_ctl,
                                        HASH_ELEM | HASH_FUNCTION);
    pfree_if_not_null(ehn);
}

/* helper function to get label names for the specified graph */
static void get_ag_labels_names(Snapshot snapshot, Oid graph_oid,
                                List **vertex_labels, List **edge_labels)
{
    ScanKeyData scan_key;
    Relation ag_label;
    SysScanDesc scan_desc;
    HeapTuple tuple;
    TupleDesc tupdesc;

    /* we need a valid snapshot */
    Assert(snapshot != NULL);

    /* setup the table to be scanned, ag_label in this case */
    ag_label = table_open(ag_label_relation_id(), AccessShareLock);

    /* get the tupdesc - we don't need to release this one */
    tupdesc = RelationGetDescr(ag_label);
    /* bail if the number of columns differs - this table has 5 */
    Assert(tupdesc->natts == Natts_ag_label);

    ScanKeyInit(&scan_key, Anum_ag_label_graph, BTEqualStrategyNumber,
                F_OIDEQ, ObjectIdGetDatum(graph_oid));

    scan_desc = systable_beginscan(ag_label, ag_label_graph_oid_index_id(),
                                   true, snapshot, 1, &scan_key);

    while (HeapTupleIsValid(tuple = systable_getnext(scan_desc)))
    {
        graph_label_entry *label;
        bool is_null;
        Datum datum;
        char label_type;

        datum = heap_getattr(tuple, Anum_ag_label_kind, tupdesc, &is_null);
        if (is_null)
        {
            continue;
        }

        label_type = DatumGetChar(datum);
        if (label_type == LABEL_TYPE_VERTEX || label_type == LABEL_TYPE_EDGE)
        {
            datum = heap_getattr(tuple, Anum_ag_label_name, tupdesc, &is_null);
            if (is_null)
            {
                continue;
            }

            label = palloc(sizeof(*label));
            namestrcpy(&label->name, NameStr(*DatumGetName(datum)));

            datum = heap_getattr(tuple, Anum_ag_label_relation, tupdesc,
                                 &is_null);
            Assert(!is_null);
            label->relation = DatumGetObjectId(datum);

            if (label_type == LABEL_TYPE_VERTEX)
            {
                *vertex_labels = lappend(*vertex_labels, label);
            }
            else
            {
                *edge_labels = lappend(*edge_labels, label);
            }
        }
    }

    systable_endscan(scan_desc);
    table_close(ag_label, AccessShareLock);
}

static void free_ag_label_names(List *labels)
{
    ListCell *lc;

    foreach(lc, labels)
    {
        pfree(lfirst(lc));
    }

    list_free(labels);
}

/*
 * Helper function to insert one edge/edge->vertex, key/value pair, in the
 * current GRAPH global edge hashtable.
 */
static bool insert_edge_entry(GRAPH_global_context *ggctx, graphid edge_id,
                              ItemPointerData tid, graphid start_vertex_id,
                              graphid end_vertex_id, Oid edge_label_table_oid,
                              char *edge_label_name)
{
    edge_entry *ee = NULL;
    bool found = false;

    /* search for the edge */
    ee = (edge_entry *)hash_search(ggctx->edge_hashtable, (void *)&edge_id,
                                      HASH_ENTER, &found);

    /* if the hash enter returned is NULL, error out */
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
                        ee->edge_id, ee->start_vertex_id, ee->end_vertex_id,
                        ee->edge_label_table_oid)));

        return false;
    }

    /* not sure if we really need to zero out the entry, as we set everything */
    MemSet(ee, 0, sizeof(edge_entry));

    /*
     * Set the edge id - this is important as this is the hash key value used
     * for hash function collisions.
     */
    ee->edge_id = edge_id;
    ee->tid = tid;
    ee->start_vertex_id = start_vertex_id;
    ee->end_vertex_id = end_vertex_id;
    ee->edge_label_table_oid = edge_label_table_oid;
    ee->edge_label_name = pstrdup(edge_label_name);

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
                                char *vertex_label_name,
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
    ve->vertex_label_name = pstrdup(vertex_label_name);
    /* set the TID for lazy property fetch */
    ve->tid = tid;
    /* set the NIL edge list */
    ve->edges_in = NULL;
    ve->edges_out = NULL;
    ve->edges_self = NULL;

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
        value->edges_self = append_graphid(value->edges_self, edge_id);
        return true;
    }
    /*
     * Otherwise, if we found the start_vertex_id add the edge to the edges_out
     * list of the start vertex
     */
    else if (start_found)
    {
        value->edges_out = append_graphid(value->edges_out, edge_id);
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
        value->edges_in = append_graphid(value->edges_in, edge_id);
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

/* helper routine to load all vertices into the GRAPH global vertex hashtable */
static void load_vertex_hashtable(GRAPH_global_context *ggctx,
                                  Snapshot snapshot, List *vertex_labels)
{
    ListCell *lc;

    /* go through all vertex label tables in list */
    foreach (lc, vertex_labels)
    {
        Relation graph_vertex_label;
        TableScanDesc scan_desc;
        HeapTuple tuple;
        graph_label_entry *label_entry;
        char *vertex_label_name;
        Oid vertex_label_table_oid;
        TupleDesc tupdesc;

        label_entry = lfirst(lc);
        vertex_label_name = NameStr(label_entry->name);
        vertex_label_table_oid = label_entry->relation;
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
            bool isnull;
            bool inserted = false;

            /* something is wrong if this isn't true */
            if (!HeapTupleIsValid(tuple))
            {
                elog(ERROR, "load_vertex_hashtable: !HeapTupleIsValid");
            }
            Assert(HeapTupleIsValid(tuple));

            vertex_id = DatumGetInt64(heap_getattr(tuple,
                                                   Anum_ag_label_vertex_table_id,
                                                   tupdesc, &isnull));
            if (isnull)
            {
                ereport(ERROR,
                        (errcode(ERRCODE_UNDEFINED_TABLE),
                         errmsg("vertex id is null for %s.%s",
                                ggctx->graph_name, vertex_label_name)));
            }

            /* insert vertex into vertex hashtable with TID (no property copy) */
            inserted = insert_vertex_entry(ggctx, vertex_id,
                                           vertex_label_table_oid,
                                           vertex_label_name,
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

/*
 * Helper function to load all of the GRAPH global hashtables (vertex & edge)
 * for the current global context.
 */
static void load_GRAPH_global_hashtables(GRAPH_global_context *ggctx)
{
    Snapshot snapshot = GetActiveSnapshot();
    List *vertex_labels = NIL;
    List *edge_labels = NIL;

    /* initialize statistics */
    ggctx->num_loaded_vertices = 0;
    ggctx->num_loaded_edges = 0;

    get_ag_labels_names(snapshot, ggctx->graph_oid, &vertex_labels,
                        &edge_labels);

    /* insert all of our vertices */
    load_vertex_hashtable(ggctx, snapshot, vertex_labels);

    /* insert all of our edges */
    load_edge_hashtable(ggctx, snapshot, edge_labels);

    free_ag_label_names(vertex_labels);
    free_ag_label_names(edge_labels);
}

/*
 * Helper routine to load all edges into the GRAPH global edge and vertex
 * hashtables.
 */
static void load_edge_hashtable(GRAPH_global_context *ggctx,
                                Snapshot snapshot, List *edge_labels)
{
    ListCell *lc;

    /* go through all edge label tables in list */
    foreach (lc, edge_labels)
    {
        Relation graph_edge_label;
        TableScanDesc scan_desc;
        HeapTuple tuple;
        graph_label_entry *label_entry;
        char *edge_label_name;
        Oid edge_label_table_oid;
        TupleDesc tupdesc;

        label_entry = lfirst(lc);
        edge_label_name = NameStr(label_entry->name);
        edge_label_table_oid = label_entry->relation;
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
            bool isnull;
            bool inserted = false;

            /* something is wrong if this isn't true */
            if (!HeapTupleIsValid(tuple))
            {
                elog(ERROR, "load_edge_hashtable: !HeapTupleIsValid");
            }
            Assert(HeapTupleIsValid(tuple));

            edge_id = DatumGetInt64(heap_getattr(tuple,
                                                 Anum_ag_label_edge_table_id,
                                                 tupdesc, &isnull));
            if (isnull)
            {
                ereport(ERROR,
                        (errcode(ERRCODE_UNDEFINED_TABLE),
                         errmsg("edge id is null for %s.%s",
                                ggctx->graph_name, edge_label_name)));
            }

            edge_vertex_start_id = DatumGetInt64(heap_getattr(
                tuple, Anum_ag_label_edge_table_start_id, tupdesc, &isnull));
            if (isnull)
            {
                ereport(ERROR,
                        (errcode(ERRCODE_UNDEFINED_TABLE),
                         errmsg("edge start_id is null for %s.%s",
                                ggctx->graph_name, edge_label_name)));
            }

            edge_vertex_end_id = DatumGetInt64(heap_getattr(
                tuple, Anum_ag_label_edge_table_end_id, tupdesc, &isnull));
            if (isnull)
            {
                ereport(ERROR,
                        (errcode(ERRCODE_UNDEFINED_TABLE),
                         errmsg("edge end_id is null for %s.%s",
                                ggctx->graph_name, edge_label_name)));
            }

            /* insert edge into edge hashtable with TID (no property copy) */
            inserted = insert_edge_entry(ggctx, edge_id, tuple->t_self,
                                         edge_vertex_start_id,
                                         edge_vertex_end_id,
                                         edge_label_table_oid,
                                         edge_label_name);

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

/*
 * Helper function to freeze the GRAPH global hashtables from additional
 * inserts. This may, or may not, be useful. Currently, these hashtables are
 * only seen by the creating process and only for reading.
 */
static void freeze_GRAPH_global_hashtables(GRAPH_global_context *ggctx)
{
    hash_freeze(ggctx->vertex_hashtable);
    hash_freeze(ggctx->edge_hashtable);
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

        /* free the edge list associated with this vertex */
        free_ListGraphId(value->edges_in);
        free_ListGraphId(value->edges_out);
        free_ListGraphId(value->edges_self);

        value->edges_in = NULL;
        value->edges_out = NULL;
        value->edges_self = NULL;

        /* move to the next vertex */
        curr_vertex = next_vertex;
    }

    /* free the vertices list */
    free_ListGraphId(ggctx->vertices);
    ggctx->vertices = NULL;

    /* free the hashtables */
    hash_destroy(ggctx->vertex_hashtable);
    hash_destroy(ggctx->edge_hashtable);

    ggctx->vertex_hashtable = NULL;
    ggctx->edge_hashtable = NULL;

    /* free the context */
    pfree_if_not_null(ggctx);
    ggctx = NULL;

    return true;
}

/*
 * Helper function to manage the GRAPH global contexts. It will create the
 * context for the graph specified, provided it isn't already built and valid.
 * During processing it will free (delete) all invalid GRAPH contexts. It
 * returns the GRAPH global context for the specified graph.
 *
 * NOTE: Function uses a MUTEX for global_graph_contexts
 *
 */
GRAPH_global_context *manage_GRAPH_global_contexts(char *graph_name,
                                                   Oid graph_oid)
{
    return manage_GRAPH_global_contexts_internal(graph_name, strlen(graph_name),
                                                graph_oid);
}

GRAPH_global_context *manage_GRAPH_global_contexts_len(const char *graph_name,
                                                       int graph_name_len,
                                                       Oid graph_oid)
{
    return manage_GRAPH_global_contexts_internal(graph_name, graph_name_len,
                                                graph_oid);
}

static GRAPH_global_context *manage_GRAPH_global_contexts_internal(
    const char *graph_name, int graph_name_len, Oid graph_oid)
{
    GRAPH_global_context *new_ggctx = NULL;
    GRAPH_global_context *curr_ggctx = NULL;
    GRAPH_global_context *prev_ggctx = NULL;
    GRAPH_global_context *found_ggctx = NULL;
    Snapshot invalidation_snapshot = NULL;
    MemoryContext oldctx = NULL;

    /* we need a higher context, or one that isn't destroyed by SRF exit */
    oldctx = MemoryContextSwitchTo(TopMemoryContext);

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

    /* lock the global contexts list */
    pthread_mutex_lock(&global_graph_contexts_container.mutex_lock);

    /* free the invalidated GRAPH global contexts first */
    prev_ggctx = NULL;
    curr_ggctx = global_graph_contexts_container.contexts;
    while (curr_ggctx != NULL)
    {
        GRAPH_global_context *next_ggctx = curr_ggctx->next;

        /* if the transaction ids have changed, we have an invalid graph */
        if (is_ggctx_invalid_with_snapshot(curr_ggctx,
                                           &invalidation_snapshot))
        {
            bool success = false;

            /*
             * If prev_ggctx is NULL then we are freeing the top of the
             * contexts. So, we need to point the contexts variable to the
             * new (next) top context, if there is one.
             */
            if (prev_ggctx == NULL)
            {
                global_graph_contexts_container.contexts = next_ggctx;
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
                /* unlock the mutex so we don't get a deadlock */
                pthread_mutex_unlock(&global_graph_contexts_container.mutex_lock);

                ereport(ERROR, (errcode(ERRCODE_DATA_EXCEPTION),
                                errmsg("missing vertex or edge entry during free")));
            }
        }
        else
        {
            if (found_ggctx == NULL && curr_ggctx->graph_oid == graph_oid)
            {
                found_ggctx = curr_ggctx;
            }
            prev_ggctx = curr_ggctx;
        }

        /* advance to the next context */
        curr_ggctx = next_ggctx;
    }

    if (found_ggctx != NULL)
    {
        /* switch our context back */
        MemoryContextSwitchTo(oldctx);

        /* we are done unlock the global contexts list */
        pthread_mutex_unlock(&global_graph_contexts_container.mutex_lock);

        return found_ggctx;
    }

    /* otherwise, we need to create one and possibly attach it */
    new_ggctx = palloc(sizeof(GRAPH_global_context));

    if (global_graph_contexts_container.contexts != NULL)
    {
        new_ggctx->next = global_graph_contexts_container.contexts;
    }
    else
    {
        new_ggctx->next = NULL;
    }

    /* set the global context variable */
    global_graph_contexts_container.contexts = new_ggctx;

    /* set the graph name and oid */
    new_ggctx->graph_name = pnstrdup(graph_name, graph_name_len);
    new_ggctx->graph_oid = graph_oid;

    /* set the graph version counter for cache invalidation */
    new_ggctx->graph_version = get_graph_version(graph_oid);

    /* set snapshot fields for SNAPSHOT fallback mode */
    {
        Snapshot snap = GetActiveSnapshot();

        new_ggctx->xmin = snap->xmin;
        new_ggctx->xmax = snap->xmax;
        new_ggctx->curcid = snap->curcid;
    }

    /* initialize our vertices list */
    new_ggctx->vertices = NULL;

    /* build the hashtables for this graph */
    create_GRAPH_global_hashtables(new_ggctx);
    load_GRAPH_global_hashtables(new_ggctx);
    freeze_GRAPH_global_hashtables(new_ggctx);

    /* unlock the global contexts list */
    pthread_mutex_unlock(&global_graph_contexts_container.mutex_lock);

    /* switch back to the previous memory context */
    MemoryContextSwitchTo(oldctx);

    return new_ggctx;
}

/*
 * Helper function to delete all of the global graph contexts used by the
 * process. When done the global global_graph_contexts will be NULL.
 *
 * NOTE: Function uses a MUTEX global_graph_contexts_mutex
 */
static bool delete_GRAPH_global_contexts(void)
{
    GRAPH_global_context *curr_ggctx = NULL;
    bool retval = false;

    /* lock contexts list */
    pthread_mutex_lock(&global_graph_contexts_container.mutex_lock);

    /* get the first context, if any */
    curr_ggctx = global_graph_contexts_container.contexts;

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
            /* unlock the mutex so we don't get a deadlock */
            pthread_mutex_unlock(&global_graph_contexts_container.mutex_lock);

            ereport(ERROR, (errcode(ERRCODE_DATA_EXCEPTION),
                            errmsg("missing vertex or edge entry during free")));
        }

        /* advance to the next context */
        curr_ggctx = next_ggctx;

        retval = true;
    }

    /* reset the head of the contexts to NULL */
    global_graph_contexts_container.contexts = NULL;

    /* unlock the global contexts list */
    pthread_mutex_unlock(&global_graph_contexts_container.mutex_lock);

    return retval;
}

/*
 * Helper function to delete a specific global graph context used by the
 * process.
 */
static bool delete_specific_GRAPH_global_contexts_len(const char *graph_name,
                                                      int graph_name_len)
{
    Oid graph_oid = InvalidOid;

    if (graph_name == NULL)
    {
        return false;
    }

    graph_oid = get_cached_global_graph_oid_len(graph_name, graph_name_len);

    return delete_specific_GRAPH_global_context_by_oid(graph_oid);
}

static Oid get_cached_global_graph_oid_len(const char *graph_name,
                                           int graph_name_len)
{
    static NameData cached_graph_name;
    static Oid cached_graph_oid = InvalidOid;
    static uint64 cached_generation = 0;
    uint64 current_generation = get_graph_cache_generation();
    char *graph_name_cstr;
    NameData graph_name_buf;
    bool free_graph_name = false;

    if (OidIsValid(cached_graph_oid) &&
        cached_generation == current_generation &&
        graph_name_len < NAMEDATALEN &&
        strncmp(NameStr(cached_graph_name), graph_name, graph_name_len) == 0 &&
        NameStr(cached_graph_name)[graph_name_len] == '\0')
    {
        return cached_graph_oid;
    }

    if (graph_name_len < NAMEDATALEN)
    {
        memcpy(NameStr(graph_name_buf), graph_name, graph_name_len);
        NameStr(graph_name_buf)[graph_name_len] = '\0';
        graph_name_cstr = NameStr(graph_name_buf);
    }
    else
    {
        graph_name_cstr = pnstrdup(graph_name, graph_name_len);
        free_graph_name = true;
    }

    cached_graph_oid = get_graph_oid(graph_name_cstr);
    if (OidIsValid(cached_graph_oid))
    {
        if (graph_name_len < NAMEDATALEN)
        {
            cached_graph_name = graph_name_buf;
        }
        else
        {
            namestrcpy(&cached_graph_name, graph_name_cstr);
        }
        cached_generation = current_generation;
    }
    if (free_graph_name)
    {
        pfree(graph_name_cstr);
    }

    return cached_graph_oid;
}

static void get_global_graph_scalar_arg_no_copy(char *funcname,
                                                agtype *agt_arg,
                                                enum agtype_value_type type,
                                                bool error,
                                                agtype_value *result,
                                                bool *needs_free)
{
    bool found;

    Assert(funcname != NULL);
    Assert(agt_arg != NULL);
    Assert(result != NULL);
    Assert(needs_free != NULL);

    if (!AGTYPE_CONTAINER_IS_SCALAR(&agt_arg->root))
    {
        ereport(ERROR,
                (errcode(ERRCODE_INVALID_PARAMETER_VALUE),
                 errmsg("%s: agtype argument must be a scalar", funcname)));
    }

    found = get_ith_agtype_value_from_container_no_copy(&agt_arg->root, 0,
                                                        result, needs_free);
    Assert(found);

    if (error && result->type == AGTV_NULL)
    {
        if (*needs_free)
        {
            pfree_agtype_value_content(result);
        }
        ereport(ERROR,
                (errcode(ERRCODE_INVALID_PARAMETER_VALUE),
                 errmsg("%s: agtype argument must not be AGTV_NULL",
                        funcname)));
    }

    if (error && result->type != type)
    {
        if (*needs_free)
        {
            pfree_agtype_value_content(result);
        }
        ereport(ERROR,
                (errcode(ERRCODE_INVALID_PARAMETER_VALUE),
                 errmsg("%s: agtype argument of wrong type", funcname)));
    }
}

static bool delete_specific_GRAPH_global_context_by_oid(Oid graph_oid)
{
    GRAPH_global_context *prev_ggctx = NULL;
    GRAPH_global_context *curr_ggctx = NULL;

    if (!OidIsValid(graph_oid))
    {
        return false;
    }

    /* lock the global contexts list */
    pthread_mutex_lock(&global_graph_contexts_container.mutex_lock);

    /* get the first context, if any */
    curr_ggctx = global_graph_contexts_container.contexts;

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
                global_graph_contexts_container.contexts = next_ggctx;
            }
            else
            {
                prev_ggctx->next = curr_ggctx->next;
            }

            /* free the current graph context */
            success = free_specific_GRAPH_global_context(curr_ggctx);

            /* unlock the global contexts list */
            pthread_mutex_unlock(&global_graph_contexts_container.mutex_lock);

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

    /* unlock the global contexts list */
    pthread_mutex_unlock(&global_graph_contexts_container.mutex_lock);

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

/* helper function to retrieve an edge_entry from the graph's edge hash table */
edge_entry *get_edge_entry(GRAPH_global_context *ggctx, graphid edge_id)
{
    edge_entry *ee = NULL;
    bool found = false;

    /* retrieve the current edge entry */
    ee = (edge_entry *)hash_search(ggctx->edge_hashtable, (void *)&edge_id,
                                   HASH_FIND, &found);
    /* it should be found, otherwise we have problems */
    Assert(found);

    return ee;
}

/*
 * Helper function to find the GRAPH_global_context used by the specified
 * graph_oid. If not found, it returns NULL.
 */
GRAPH_global_context *find_GRAPH_global_context(Oid graph_oid)
{
    GRAPH_global_context *ggctx = NULL;

    /* lock the global contexts lists */
    pthread_mutex_lock(&global_graph_contexts_container.mutex_lock);

    /* get the root */
    ggctx = global_graph_contexts_container.contexts;

    while(ggctx != NULL)
    {
        /* if we found it return it */
        if (ggctx->graph_oid == graph_oid)
        {
            /* unlock the global contexts lists */
            pthread_mutex_unlock(&global_graph_contexts_container.mutex_lock);

            return ggctx;
        }

        /* advance to the next context */
        ggctx = ggctx->next;
    }

    /* unlock the global contexts lists */
    pthread_mutex_unlock(&global_graph_contexts_container.mutex_lock);

    /* we did not find it so return NULL */
    return NULL;
}

/* graph vertices accessor */
ListGraphId *get_graph_vertices(GRAPH_global_context *ggctx)
{
    return ggctx->vertices;
}

int64 get_graph_num_loaded_edges(GRAPH_global_context *ggctx)
{
    return ggctx->num_loaded_edges;
}

/* vertex_entry accessor functions */
graphid get_vertex_entry_id(vertex_entry *ve)
{
    return ve->vertex_id;
}

ListGraphId *get_vertex_entry_edges_in(vertex_entry *ve)
{
    return ve->edges_in;
}

ListGraphId *get_vertex_entry_edges_out(vertex_entry *ve)
{
    return ve->edges_out;
}

ListGraphId *get_vertex_entry_edges_self(vertex_entry *ve)
{
    return ve->edges_self;
}


Oid get_vertex_entry_label_table_oid(vertex_entry *ve)
{
    return ve->vertex_label_table_oid;
}

char *get_vertex_entry_label_name(vertex_entry *ve)
{
    return ve->vertex_label_name;
}

/*
 * Fetch vertex properties on demand from the heap via stored TID.
 *
 * Returns a datumCopy of the properties in the current memory context.
 * The caller does not need to free the result explicitly — it will be
 * freed when the memory context is reset (typically the SRF multi-call
 * context for VLE, which is cleaned up when the SRF completes).
 *
 * If the tuple is no longer visible (e.g., concurrent mutation between
 * cache build and fetch), the version counter should have invalidated
 * the cache. If we get here with a stale TID, it indicates a bug in
 * the invalidation logic.
 */
Datum get_vertex_entry_properties(vertex_entry *ve)
{
    return get_vertex_entry_properties_with_cache(ve, NULL);
}

Datum get_vertex_entry_properties_with_cache(vertex_entry *ve,
                                             HTAB *relation_cache)
{
    return fetch_entry_properties(
        ve->vertex_label_table_oid, ve->tid, 2, relation_cache,
        "get_vertex_entry_properties: stale TID - "
        "vertex entry references a tuple that is no longer visible");
}

static Datum fetch_entry_properties(Oid relid, ItemPointerData tid,
                                    AttrNumber property_attr,
                                    HTAB *relation_cache,
                                    const char *stale_msg)
{
    Relation rel;
    HeapTupleData tuple;
    Buffer buffer;
    Datum result = (Datum) 0;
    bool close_rel = false;

    rel = get_entry_property_relation(relid, relation_cache);
    close_rel = relation_cache == NULL;
    tuple.t_self = tid;

    if (heap_fetch(rel, GetActiveSnapshot(), &tuple, &buffer, true))
    {
        TupleDesc tupdesc = RelationGetDescr(rel);
        bool isnull;
        Datum props;

        props = heap_getattr(&tuple, property_attr, tupdesc, &isnull);
        if (!isnull)
        {
            result = datumCopy(props, false, -1);
        }

        ReleaseBuffer(buffer);
    }

    if (close_rel)
    {
        table_close(rel, AccessShareLock);
    }

    /*
     * If heap_fetch failed, the tuple is no longer visible. This should
     * not happen under normal operation because the version counter
     * invalidates the cache when the graph is mutated.
     */
    if (result == (Datum) 0)
    {
        elog(ERROR, "%s", stale_msg);
    }

    return result;
}

/* edge_entry accessor functions */
graphid get_edge_entry_id(edge_entry *ee)
{
    return ee->edge_id;
}

Oid get_edge_entry_label_table_oid(edge_entry *ee)
{
    return ee->edge_label_table_oid;
}

char *get_edge_entry_label_name(edge_entry *ee)
{
    return ee->edge_label_name;
}

/*
 * Fetch edge properties on demand from the heap via stored TID.
 * See get_vertex_entry_properties for memory and safety notes.
 */
Datum get_edge_entry_properties(edge_entry *ee)
{
    return get_edge_entry_properties_with_cache(ee, NULL);
}

Datum get_edge_entry_properties_with_cache(edge_entry *ee,
                                           HTAB *relation_cache)
{
    return fetch_entry_properties(
        ee->edge_label_table_oid, ee->tid, 4, relation_cache,
        "get_edge_entry_properties: stale TID - "
        "edge entry references a tuple that is no longer visible");
}

HTAB *create_entry_property_relation_cache(const char *name)
{
    HASHCTL hash_ctl;

    MemSet(&hash_ctl, 0, sizeof(hash_ctl));
    hash_ctl.keysize = sizeof(Oid);
    hash_ctl.entrysize = sizeof(EntryPropertyRelationCacheEntry);
    hash_ctl.hcxt = CurrentMemoryContext;

    return hash_create(name, 8, &hash_ctl,
                       HASH_ELEM | HASH_BLOBS | HASH_CONTEXT);
}

void destroy_entry_property_relation_cache(HTAB *relation_cache)
{
    HASH_SEQ_STATUS hash_seq;
    EntryPropertyRelationCacheEntry *entry;

    if (relation_cache == NULL)
    {
        return;
    }

    hash_seq_init(&hash_seq, relation_cache);
    while ((entry = hash_seq_search(&hash_seq)) != NULL)
    {
        if (entry->rel != NULL)
        {
            table_close(entry->rel, AccessShareLock);
        }
    }

    hash_destroy(relation_cache);
}

static Relation get_entry_property_relation(Oid relid, HTAB *relation_cache)
{
    EntryPropertyRelationCacheEntry *entry;
    bool found;

    if (relation_cache == NULL)
    {
        return table_open(relid, AccessShareLock);
    }

    entry = hash_search(relation_cache, &relid, HASH_ENTER, &found);
    if (!found)
    {
        entry->relid = relid;
        entry->rel = table_open(relid, AccessShareLock);
    }

    return entry->rel;
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
    agtype_value agtv_temp;
    agtype_value *agtv_temp_ptr = NULL;
    bool agtv_temp_needs_free = false;
    bool success = false;

    /* get the graph name if supplied */
    if (!PG_ARGISNULL(0))
    {
        get_global_graph_scalar_arg_no_copy("delete_global_graphs",
                                            AG_GET_ARG_AGTYPE_P(0),
                                            AGTV_STRING, false,
                                            &agtv_temp,
                                            &agtv_temp_needs_free);
        agtv_temp_ptr = &agtv_temp;
    }

    if (agtv_temp_ptr == NULL || agtv_temp_ptr->type == AGTV_NULL)
    {
        success = delete_GRAPH_global_contexts();
    }
    else if (agtv_temp_ptr->type == AGTV_STRING)
    {
        success = delete_specific_GRAPH_global_contexts_len(
            agtv_temp_ptr->val.string.val, agtv_temp_ptr->val.string.len);
    }
    else
    {
        ereport(ERROR,
                (errcode(ERRCODE_INVALID_PARAMETER_VALUE),
                 errmsg("delete_global_graphs: invalid graph name type")));
    }

    if (agtv_temp_needs_free)
    {
        pfree_agtype_value_content(&agtv_temp);
    }

    PG_RETURN_BOOL(success);
}

/* PG wrapper function for age_vertex_degree */
PG_FUNCTION_INFO_V1(age_vertex_stats);

Datum age_vertex_stats(PG_FUNCTION_ARGS)
{
    GRAPH_global_context *ggctx = NULL;
    vertex_entry *ve = NULL;
    ListGraphId *edges = NULL;
    agtype_value agtv_vertex;
    agtype_value *agtv_temp = NULL;
    agtype_value graph_name_value;
    bool graph_name_needs_free = false;
    bool vertex_needs_free = false;
    agtype_value agtv_integer;
    agtype_in_state result;
    agtype *agt_result;
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
    get_global_graph_scalar_arg_no_copy("vertex_stats", AG_GET_ARG_AGTYPE_P(0),
                                        AGTV_STRING, true, &graph_name_value,
                                        &graph_name_needs_free);

    /* we need the vertex */
    if (PG_ARGISNULL(1))
    {
        ereport(ERROR,
                (errcode(ERRCODE_INVALID_PARAMETER_VALUE),
                 errmsg("vertex_stats: vertex cannot be NULL")));
    }

    /* get the vertex */
    get_global_graph_scalar_arg_no_copy("vertex_stats", AG_GET_ARG_AGTYPE_P(1),
                                        AGTV_VERTEX, true, &agtv_vertex,
                                        &vertex_needs_free);

    /* get the graph oid */
    graph_oid = get_cached_global_graph_oid_len(graph_name_value.val.string.val,
                                                graph_name_value.val.string.len);

    /*
     * Create or retrieve the GRAPH global context for this graph. This function
     * will also purge off invalidated contexts.
     */
    ggctx = manage_GRAPH_global_contexts_len(graph_name_value.val.string.val,
                                             graph_name_value.val.string.len,
                                             graph_oid);

    /* get the id */
    agtv_temp = AGTYPE_VERTEX_GET_ID(&agtv_vertex);
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
    agtv_temp = AGTYPE_VERTEX_GET_LABEL(&agtv_vertex);
    result.res = push_agtype_value(&result.parse_state, WAGT_KEY,
                                   string_to_agtype_value("label"));
    result.res = push_agtype_value(&result.parse_state, WAGT_VALUE, agtv_temp);

    /* set up an integer for returning values */
    agtv_temp = &agtv_integer;
    agtv_temp->type = AGTV_INTEGER;
    agtv_temp->val.int_value = 0;

    /* get and store the self_loops */
    edges = get_vertex_entry_edges_self(ve);
    self_loops = (edges != NULL) ? get_list_size(edges) : 0;
    agtv_temp->val.int_value = self_loops;
    result.res = push_agtype_value(&result.parse_state, WAGT_KEY,
                                   string_to_agtype_value("self_loops"));
    result.res = push_agtype_value(&result.parse_state, WAGT_VALUE, agtv_temp);

    /* get and store the in_degree */
    edges = get_vertex_entry_edges_in(ve);
    degree = (edges != NULL) ? get_list_size(edges) : 0;
    agtv_temp->val.int_value = degree + self_loops;
    result.res = push_agtype_value(&result.parse_state, WAGT_KEY,
                                   string_to_agtype_value("in_degree"));
    result.res = push_agtype_value(&result.parse_state, WAGT_VALUE, agtv_temp);

    /* get and store the out_degree */
    edges = get_vertex_entry_edges_out(ve);
    degree = (edges != NULL) ? get_list_size(edges) : 0;
    agtv_temp->val.int_value = degree + self_loops;
    result.res = push_agtype_value(&result.parse_state, WAGT_KEY,
                                   string_to_agtype_value("out_degree"));
    result.res = push_agtype_value(&result.parse_state, WAGT_VALUE, agtv_temp);

    /* close the object */
    result.res = push_agtype_value(&result.parse_state, WAGT_END_OBJECT, NULL);

    result.res->type = AGTV_OBJECT;

    agt_result = agtype_value_to_agtype(result.res);
    if (graph_name_needs_free)
    {
        pfree_agtype_value_content(&graph_name_value);
    }
    if (vertex_needs_free)
    {
        pfree_agtype_value_content(&agtv_vertex);
    }

    PG_RETURN_POINTER(agt_result);
}

/* PG wrapper function for age_graph_stats */
PG_FUNCTION_INFO_V1(age_graph_stats);

Datum age_graph_stats(PG_FUNCTION_ARGS)
{
    GRAPH_global_context *ggctx = NULL;
    agtype_value *agtv_temp = NULL;
    agtype_value graph_name_value;
    bool graph_name_needs_free = false;
    agtype_value agtv_integer;
    agtype_in_state result;
    agtype *agt_result;
    Oid graph_oid = InvalidOid;

    /* the graph name is required, but this generally isn't user supplied */
    if (PG_ARGISNULL(0))
    {
        ereport(ERROR,
                (errcode(ERRCODE_INVALID_PARAMETER_VALUE),
                 errmsg("graph_stats: graph name cannot be NULL")));
    }

    /* get the graph name */
    get_global_graph_scalar_arg_no_copy("graph_stats", AG_GET_ARG_AGTYPE_P(0),
                                        AGTV_STRING, true, &graph_name_value,
                                        &graph_name_needs_free);

    /* get the graph oid */
    graph_oid = get_cached_global_graph_oid_len(graph_name_value.val.string.val,
                                                graph_name_value.val.string.len);

    /*
     * Remove any context for this graph. This is done to allow graph_stats to
     * show any load issues.
     */
    delete_specific_GRAPH_global_context_by_oid(graph_oid);

    /*
     * Create or retrieve the GRAPH global context for this graph. This function
     * will also purge off invalidated contexts.
     */
    ggctx = manage_GRAPH_global_contexts_len(graph_name_value.val.string.val,
                                             graph_name_value.val.string.len,
                                             graph_oid);

    /* zero the state */
    memset(&result, 0, sizeof(agtype_in_state));

    /* start the object */
    result.res = push_agtype_value(&result.parse_state, WAGT_BEGIN_OBJECT,
                                   NULL);
    /* store the graph name */
    result.res = push_agtype_value(&result.parse_state, WAGT_KEY,
                                   string_to_agtype_value("graph"));
    result.res = push_agtype_value(&result.parse_state, WAGT_VALUE,
                                   &graph_name_value);

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

    agt_result = agtype_value_to_agtype(result.res);
    if (graph_name_needs_free)
    {
        pfree_agtype_value_content(&graph_name_value);
    }

    PG_RETURN_POINTER(agt_result);
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

    if (cached_version_graph_oid == graph_oid &&
        cached_version_graph_index >= 0 &&
        cached_version_graph_index < state->num_entries &&
        state->entries[cached_version_graph_index].graph_oid == graph_oid)
    {
        return pg_atomic_read_u64(
            &state->entries[cached_version_graph_index].version);
    }

    /* lock-free scan of the array */
    for (i = 0; i < state->num_entries; i++)
    {
        if (state->entries[i].graph_oid == graph_oid)
        {
            cached_version_graph_oid = graph_oid;
            cached_version_graph_index = i;

            return pg_atomic_read_u64(&state->entries[i].version);
        }
    }

    cached_version_graph_oid = InvalidOid;
    cached_version_graph_index = -1;

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

    if (cached_version_graph_oid == graph_oid &&
        cached_version_graph_index >= 0 &&
        cached_version_graph_index < state->num_entries &&
        state->entries[cached_version_graph_index].graph_oid == graph_oid)
    {
        pg_atomic_fetch_add_u64(
            &state->entries[cached_version_graph_index].version, 1);
        return;
    }

    /* try to find existing entry (lock-free) */
    for (i = 0; i < state->num_entries; i++)
    {
        if (state->entries[i].graph_oid == graph_oid)
        {
            cached_version_graph_oid = graph_oid;
            cached_version_graph_index = i;

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
            cached_version_graph_oid = graph_oid;
            cached_version_graph_index = i;

            pg_atomic_fetch_add_u64(&state->entries[i].version, 1);
            return;
        }
    }

    /* add new entry */
    if (state->num_entries < AGE_MAX_GRAPHS)
    {
        int idx = state->num_entries;

        state->entries[idx].graph_oid = graph_oid;
        pg_atomic_init_u64(&state->entries[idx].version, 1);
        cached_version_graph_oid = graph_oid;
        cached_version_graph_index = idx;

        /*
         * Write barrier ensures the entry fields are fully visible to
         * other backends before num_entries is incremented. This prevents
         * readers on weak memory-ordering architectures (e.g., ARM) from
         * seeing the incremented count before the entry is initialized.
         */
        pg_write_barrier();
        state->num_entries++;
    }
    else
    {
        elog(WARNING, "AGE: graph version counter table full (%d graphs)",
             AGE_MAX_GRAPHS);
    }

    LWLockRelease(&state->lock);
}

/*
 * Helper function to look up the graph OID for a given label table OID.
 * Uses AGE's label relation cache for fast lookup.
 * Returns InvalidOid if the table is not a graph label table.
 */
Oid get_graph_oid_for_table(Oid table_oid)
{
    label_cache_data *lcd = NULL;

    lcd = search_label_relation_cache_cached(table_oid);

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
