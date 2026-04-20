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
#define VERTEX_HTAB_INITIAL_SIZE 10000
#define EDGE_HTAB_INITIAL_SIZE 10000

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
    ListGraphId *edges_in;         /* List of entering edges graphids (int64) */
    ListGraphId *edges_out;        /* List of exiting edges graphids (int64) */
    ListGraphId *edges_self;       /* List of selfloop edges graphids (int64) */
    Oid vertex_label_table_oid;    /* the label table oid */
    ItemPointerData tid;           /* physical tuple location for lazy fetch */
} vertex_entry;

/* edge entry for the edge_hashtable */
typedef struct edge_entry
{
    graphid edge_id;               /* edge id, it is also the hash key */
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
static bool delete_specific_GRAPH_global_contexts(char *graph_name);
static bool delete_GRAPH_global_contexts(void);
static void create_GRAPH_global_hashtables(GRAPH_global_context *ggctx);
static void load_GRAPH_global_hashtables(GRAPH_global_context *ggctx);
static void load_vertex_hashtable(GRAPH_global_context *ggctx);
static void load_edge_hashtable(GRAPH_global_context *ggctx);
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
    int glen;
    int vlen;
    int elen;

    /* get the graph name and length */
    graph_name = ggctx->graph_name;
    glen = strlen(graph_name);
    /* get the vertex htab name length */
    vlen = strlen(VERTEX_HTAB_NAME);
    /* get the edge htab name length */
    elen = strlen(EDGE_HTAB_NAME);
    /* allocate the space and build the names */
    vhn = palloc0(vlen + glen + 1);
    ehn = palloc0(elen + glen + 1);
    /* copy in the names */
    strcpy(vhn, VERTEX_HTAB_NAME);
    strcpy(ehn, EDGE_HTAB_NAME);
    /* add in the graph name */
    vhn = strncat(vhn, graph_name, glen);
    ehn = strncat(ehn, graph_name, glen);

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
    GRAPH_global_context *new_ggctx = NULL;
    GRAPH_global_context *curr_ggctx = NULL;
    GRAPH_global_context *prev_ggctx = NULL;
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
            prev_ggctx = curr_ggctx;
        }

        /* advance to the next context */
        curr_ggctx = next_ggctx;
    }

    /* find our graph's context. if it exists, we are done */
    curr_ggctx = global_graph_contexts_container.contexts;
    while (curr_ggctx != NULL)
    {
        if (curr_ggctx->graph_oid == graph_oid)
        {
            /* switch our context back */
            MemoryContextSwitchTo(oldctx);

            /* we are done unlock the global contexts list */
            pthread_mutex_unlock(&global_graph_contexts_container.mutex_lock);

            return curr_ggctx;
        }
        curr_ggctx = curr_ggctx->next;
    }

    /* otherwise, we need to create one and possibly attach it */
    new_ggctx = palloc0(sizeof(GRAPH_global_context));

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
    new_ggctx->graph_name = pstrdup(graph_name);
    new_ggctx->graph_oid = graph_oid;

    /* set the graph version counter for cache invalidation */
    new_ggctx->graph_version = get_graph_version(graph_oid);

    /* set snapshot fields for SNAPSHOT fallback mode */
    new_ggctx->xmin = GetActiveSnapshot()->xmin;
    new_ggctx->xmax = GetActiveSnapshot()->xmax;
    new_ggctx->curcid = GetActiveSnapshot()->curcid;

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
    Relation rel;
    HeapTupleData tuple;
    Buffer buffer;
    Datum result = (Datum) 0;

    rel = table_open(ve->vertex_label_table_oid, AccessShareLock);
    tuple.t_self = ve->tid;

    if (heap_fetch(rel, GetActiveSnapshot(), &tuple, &buffer, true))
    {
        TupleDesc tupdesc = RelationGetDescr(rel);
        bool isnull;
        Datum props;

        /* properties is column 2 (1-indexed) */
        props = heap_getattr(&tuple, 2, tupdesc, &isnull);
        if (!isnull)
        {
            result = datumCopy(props, false, -1);
        }

        ReleaseBuffer(buffer);
    }

    table_close(rel, AccessShareLock);

    /*
     * If heap_fetch failed, the tuple is no longer visible. This should
     * not happen under normal operation because the version counter
     * invalidates the cache when the graph is mutated.
     */
    if (result == (Datum) 0)
    {
        elog(ERROR, "get_vertex_entry_properties: stale TID - "
             "vertex entry references a tuple that is no longer visible");
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

/*
 * Fetch edge properties on demand from the heap via stored TID.
 * See get_vertex_entry_properties for memory and safety notes.
 */
Datum get_edge_entry_properties(edge_entry *ee)
{
    Relation rel;
    HeapTupleData tuple;
    Buffer buffer;
    Datum result = (Datum) 0;

    rel = table_open(ee->edge_label_table_oid, AccessShareLock);
    tuple.t_self = ee->tid;

    if (heap_fetch(rel, GetActiveSnapshot(), &tuple, &buffer, true))
    {
        TupleDesc tupdesc = RelationGetDescr(rel);
        bool isnull;
        Datum props;

        /* properties is column 4 (1-indexed) */
        props = heap_getattr(&tuple, 4, tupdesc, &isnull);
        if (!isnull)
        {
            result = datumCopy(props, false, -1);
        }

        ReleaseBuffer(buffer);
    }

    table_close(rel, AccessShareLock);

    if (result == (Datum) 0)
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
    ListGraphId *edges = NULL;
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

    /* add new entry */
    if (state->num_entries < AGE_MAX_GRAPHS)
    {
        int idx = state->num_entries;

        state->entries[idx].graph_oid = graph_oid;
        pg_atomic_init_u64(&state->entries[idx].version, 1);

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
