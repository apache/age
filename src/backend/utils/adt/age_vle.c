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

/*
 * VLE (Variable-Length Edge) semantics and cost model
 * ---------------------------------------------------
 *
 * This file implements variable-length relationship matching for Cypher
 * patterns of the form (a)-[*min..max]->(b). The semantics and cost model
 * are often misunderstood; this note exists to prevent future
 * misdiagnoses (see issue #2349).
 *
 * Semantics: edge-isomorphism (openCypher-mandated)
 *
 *   A path is valid iff no edge appears in it more than once. Vertices MAY
 *   recur. This is "edge-isomorphism" (a.k.a. relationship-uniqueness) per
 *   the openCypher specification; it is NOT vertex-isomorphism.
 *
 *   Example: in the triangle A-[e1]->B-[e2]->C-[e3]->A, the query
 *     MATCH (a)-[*3]->(b) WHERE id(a) = id(A)
 *   MUST return the path (A, e1, B, e2, C, e3, A) with b = A. Switching
 *   to vertex-isomorphism would silently drop this path and violate the
 *   spec. Any "optimization" that tracks visited vertices as a filter
 *   rather than visited edges is therefore incorrect, not merely faster.
 *
 * Cost model
 *
 *   With E total edges in the traversal-reachable subgraph and a bounded
 *   pattern [*min..max], the number of enumerated paths is bounded by
 *   sum_{k=min..max} P(E, k) where P(E, k) = E! / (E - k)! -- polynomial
 *   in E for fixed max, but factorial in the depth bound.
 *
 *   Unbounded patterns ([*], [*1..]) have no termination guarantee other
 *   than edge-uniqueness depletion. On a cycle-rich graph the worst case
 *   is O(E!). This is inherent to edge-isomorphic path enumeration and
 *   cannot be reduced by algorithm change without changing semantics.
 *   Users who want reachability (not full enumeration) should bound the
 *   upper length or use a dedicated function such as shortestPath().
 *
 * Implementation pointer
 *
 *   Cycle prevention is enforced by edge_state_entry.used_in_path, set
 *   and cleared during DFS traversal in dfs_find_a_path_between() and
 *   dfs_find_a_path_from(). The helper is_edge_in_path() inspects the
 *   current path stack. See those functions for the enforcement site.
 */

#include "postgres.h"

#include "common/hashfn.h"
#include "funcapi.h"
#include "miscadmin.h"
#include "nodes/pg_list.h"
#include "utils/datum.h"
#include "utils/guc.h"
#include "utils/lsyscache.h"
#include "utils/memutils.h"

#include "utils/age_vle.h"
#include "catalog/ag_graph.h"
#include "catalog/ag_label.h"
#include "nodes/cypher_nodes.h"

/* defines */
#define GET_GRAPHID_ARRAY_FROM_CONTAINER(vpc) \
            (graphid *) (&vpc->graphid_array_data)
#define EDGE_STATE_HTAB_NAME "Edge state "
#define EXISTS_HTAB_NAME "known edges"
#define EXISTS_HTAB_NAME_INITIAL_SIZE 1000

/*
 * ---------------------------------------------------------------------
 * GUC-controlled memory knobs for the VLE caches
 * ---------------------------------------------------------------------
 */
int vle_edge_state_htab_initial_size = 16384;
int vle_vertex_edge_htab_initial_size = 1024;
int vle_edge_state_max_entries = 2000000;
int vle_vertex_edge_cache_max_entries = 200000;
int vle_vertex_edge_cache_max_kb = 65536;
int vle_reverse_dist_max_entries = 500000;
int vle_max_cached_contexts = 5;
bool vle_edge_state_eviction_enabled = true;

#define MAXIMUM_NUMBER_OF_CACHED_LOCAL_CONTEXTS (vle_max_cached_contexts)

void vle_define_guc_variables(void)
{
    DefineCustomIntVariable("age.vle_edge_state_htab_initial_size",
                            "Initial bucket count for the per-query VLE edge-state hash table.",
                            "Lower values reduce baseline memory for small VLE queries; "
                            "the table still grows on demand for larger ones.",
                            &vle_edge_state_htab_initial_size,
                            vle_edge_state_htab_initial_size, 16, 10000000,
                            PGC_USERSET, 0, NULL, NULL, NULL);

    DefineCustomIntVariable("age.vle_vertex_edge_htab_initial_size",
                            "Initial bucket count for the per-query VLE vertex->edges cache.",
                            NULL,
                            &vle_vertex_edge_htab_initial_size,
                            vle_vertex_edge_htab_initial_size, 16, 10000000,
                            PGC_USERSET, 0, NULL, NULL, NULL);

    DefineCustomIntVariable("age.vle_edge_state_max_entries",
                            "Soft cap on live entries in the VLE edge-state hash table "
                            "before background eviction of unreferenced entries kicks in.",
                            "Entries currently pinned (part of the active DFS path or "
                            "still sitting unresolved on the DFS edge stack) are never "
                            "evicted, so actual memory can still exceed this in the "
                            "worst case -- see age.vle_edge_state_eviction_enabled.",
                            &vle_edge_state_max_entries,
                            vle_edge_state_max_entries, 1000, INT_MAX,
                            PGC_USERSET, 0, NULL, NULL, NULL);

    DefineCustomIntVariable("age.vle_vertex_edge_cache_max_entries",
                            "Soft cap on the number of vertices whose statically-valid "
                            "adjacent edges are cached at once (VLE vertex_edge_cache).",
                            "Unlike the edge-state cache, this one is a pure "
                            "performance cache with no pinning constraints, so it is "
                            "always evicted down to the cap using an LRU/clock policy.",
                            &vle_vertex_edge_cache_max_entries,
                            vle_vertex_edge_cache_max_entries, 1000, INT_MAX,
                            PGC_USERSET, 0, NULL, NULL, NULL);

    DefineCustomIntVariable("age.vle_vertex_edge_cache_max_kb",
                            "Soft cap, in kilobytes, on the memory backing "
                            "VLE vertex_edge_cache's cached adjacency arrays.",
                            "A single high-degree (hub) vertex can hold far more "
                            "memory than age.vle_vertex_edge_cache_max_entries alone "
                            "would suggest; this bounds the cache by its actual "
                            "footprint.",
                            &vle_vertex_edge_cache_max_kb,
                            vle_vertex_edge_cache_max_kb, 1024, INT_MAX,
                            PGC_USERSET, 0, NULL, NULL, NULL);

    DefineCustomIntVariable("age.vle_reverse_dist_max_entries",
                            "Cap on the reverse-BFS distance table used to prune "
                            "PATHS_BETWEEN VLE queries.",
                            "Once reached, the reverse BFS simply stops advancing "
                            "(graceful degradation of pruning power, never a "
                            "correctness issue -- under-pruning only costs speed).",
                            &vle_reverse_dist_max_entries,
                            vle_reverse_dist_max_entries, 1000, INT_MAX,
                            PGC_USERSET, 0, NULL, NULL, NULL);

    DefineCustomIntVariable("age.vle_max_cached_contexts",
                            "Maximum number of VLE_local_context objects (one per "
                            "distinct VLE grammar node) kept cached per backend.",
                            NULL,
                            &vle_max_cached_contexts,
                            vle_max_cached_contexts, 1, 1000,
                            PGC_USERSET, 0, NULL, NULL, NULL);

    DefineCustomBoolVariable("age.vle_edge_state_eviction_enabled",
                            "Enable clock-style eviction of unreferenced entries in "
                            "the VLE edge-state hash table once "
                            "age.vle_edge_state_max_entries is exceeded.",
                            "When disabled, the edge-state cache grows without a "
                            "hard bound (still only ever holding matched edges).",
                            &vle_edge_state_eviction_enabled,
                            vle_edge_state_eviction_enabled,
                            PGC_USERSET, 0, NULL, NULL, NULL);
}

/*
 * Edge state entry for the edge_state_hashtable.
 *
 * An entry is created here only for an edge that passed
 * is_an_edge_match() -- see get_or_build_vertex_edge_cache() and
 * rdist_expand_vertex(). Presence in the table therefore implies
 * "matched"; there is no separate matched flag.
 *
 * pin_count is the number of times this edge_id currently sits,
 * unpopped, somewhere on vlelctx->dfs_edge_stack. Because vertices (not
 * just edges) can be revisited by the DFS, the SAME edge_id can be
 * pushed onto dfs_edge_stack more than once while an older, still-open
 * copy is buried deeper in the stack (see add_valid_vertex_edges() and
 * the single gid_stack_pop(edge_stack) sites in dfs_find_a_path_between/
 * from()). used_in_path alone is therefore NOT sufficient to know an
 * entry is safe to evict -- a pin_count of 0 is: it means no live
 * occurrence of this edge remains anywhere on the stack, in any state.
 */
typedef struct edge_state_entry
{
    graphid edge_id;               /* edge id, it is also the hash key */
    graphid start_vertex_id;       /* Topology cache for edge endpoints; */
    graphid end_vertex_id;         /* used for direction resolution in DFS. */
    uint32 state;               /* bits 0-29: live occurrences on dfs_edge_stack; */
                                    /* 0 <=> safe to evict (see above)    */
                                /* bit 30: used_in_path; bit 31: clock_ref (eviction) */
} edge_state_entry;


/*
 * Macros for manipulating edge_state_entry flags: used_in_path and
 * clock_ref, packed into a single uint8 field.
 */
#define PIN_COUNT_MASK 0b00111111111111111111111111111111U /* bits 0-29 */
#define USE_IN_PATH_FLAGS_MASK 0b01000000000000000000000000000000U /* bit 30 */
#define CLOCK_REF_FLAGS_MASK   0b10000000000000000000000000000000U /* bit 31 */

#define EDGE_STATE_ENTRY_USE_IN_PATH(ese) ((ese)->state & USE_IN_PATH_FLAGS_MASK)
#define EDGE_STATE_ENTRY_SET_USE_IN_PATH(ese) ((ese)->state |= USE_IN_PATH_FLAGS_MASK)
#define EDGE_STATE_ENTRY_UNSET_USE_IN_PATH(ese) ((ese)->state &= ~USE_IN_PATH_FLAGS_MASK)

#define EDGE_STATE_ENTRY_CLOCK_REF(ese) ((ese)->state & CLOCK_REF_FLAGS_MASK)
#define EDGE_STATE_ENTRY_SET_CLOCK_REF(ese) ((ese)->state |= CLOCK_REF_FLAGS_MASK)
#define EDGE_STATE_ENTRY_UNSET_CLOCK_REF(ese) ((ese)->state &= ~CLOCK_REF_FLAGS_MASK)

#define EDGE_STATE_ENTRY_PIN_COUNT(ese) ((ese)->state & PIN_COUNT_MASK)
#define EDGE_STATE_ENTRY_PIN_COUNT_INC(ese) ((ese)->state = ((ese)->state & ~PIN_COUNT_MASK) | (EDGE_STATE_ENTRY_PIN_COUNT(ese) + 1))
#define EDGE_STATE_ENTRY_PIN_COUNT_DEC(ese) ((ese)->state = ((ese)->state & ~PIN_COUNT_MASK) | (EDGE_STATE_ENTRY_PIN_COUNT(ese) - 1))
/*
 * Vertex-level cache of statically-valid adjacent edges (see
 * get_or_build_vertex_edge_cache). "Statically valid" means the edge passed
 * is_an_edge_match() -- it says nothing about whether the edge is currently
 * usable in the DFS (that is path-dependent and re-checked on every visit
 * by add_valid_vertex_edges()).
 */
typedef struct vertex_edge_cache_entry
{
    graphid vertex_id;              /* vertex id, it is also the hash key */
    int32 nvalid;                   /* number of entries in valid_edges */
    graphid *valid_edges;           /* palloc'd array (in
                                      * vlelctx->vertex_edge_cache_mcxt) of
                                      * edge ids that passed is_an_edge_match
                                      * for this vertex, in the same relative
                                      * order they were discovered (out, then
                                      * in, then self). Sized to exactly
                                      * nvalid, not to the raw adjacency
                                      * count -- see the repalloc in
                                      * get_or_build_vertex_edge_cache(). */
    bool clock_ref;                 /* eviction: touched since last sweep?
                                      * Unlike edge_state_entry, this cache
                                      * has NO pinning requirement -- the
                                      * returned pointer is only ever used
                                      * synchronously within a single
                                      * add_valid_vertex_edges() call, never
                                      * retained -- so any entry is always
                                      * safe to evict and rebuild later. */
} vertex_edge_cache_entry;

/*
 * Resumable FIFO frontier queue for the lazy reverse-BFS in
 * get_or_advance_reverse_dist().
 */
typedef struct rdist_queue
{
    graphid *data;
    int64 head;
    int64 tail;
    int64 cap;  /* power of two */
    int64 count;
    int64 max_count;
} rdist_queue;

/*
 * Entry in vlelctx->reverse_dist_table: vertex_id -> its reverse-BFS
 * distance to vlelctx->veid, for the CURRENT target only.
 */
typedef struct reverse_dist_entry
{
    graphid vertex_id;              /* hash key */
    int64 dist;
} reverse_dist_entry;

/*
 * VLE_path_function is an enum for the path function to use. This currently can
 * be one of two possibilities - where the target vertex is provided and where
 * it isn't.
 */
typedef enum
{                                  /* Given a path (u)-[e]-(v)                */
    VLE_FUNCTION_PATHS_FROM,       /* Paths from a (u) without a provided (v) */
    VLE_FUNCTION_PATHS_TO,         /* Paths to a (v) without a provided (u)   */
    VLE_FUNCTION_PATHS_BETWEEN,    /* Paths between a (u) and a provided (v)  */
    VLE_FUNCTION_PATHS_ALL,        /* All paths without a provided (u) or (v) */
    VLE_FUNCTION_NONE
} VLE_path_function;

/* VLE local context per each unique age_vle function activation */
typedef struct VLE_local_context
{
    char *graph_name;              /* name of the graph */
    Oid graph_oid;                 /* graph oid for searching */
    GRAPH_global_context *ggctx;   /* global graph context pointer */
    graphid vsid;                  /* starting vertex id */
    graphid veid;                  /* ending vertex id */
    char *edge_label_name;         /* edge label name for match */
    Oid edge_label_name_oid;       /* edge label name oid for match */
    agtype *edge_property_constraint; /* edge property constraint as agtype */
    Datum edge_property_constraint_datum; /* edge property constraint as Datum */
    uint32 edge_property_constraint_hash; /* edge property constraint hash */
    int64 lidx;                    /* lower (start) bound index */
    int64 uidx;                    /* upper (end) bound index */
    bool uidx_infinite;            /* flag if the upper bound is omitted */
    cypher_rel_dir edge_direction; /* the direction of the edge */
    HTAB *edge_state_hashtable;    /* local state hashtable for our edges */
    HTAB *vertex_edge_cache;       /* vertex_id -> statically-valid adjacent
                                     * edges (see get_or_build_vertex_edge_cache) */
    MemoryContext vertex_edge_cache_mcxt; /* Child context for valid_edges[]; managed explicitly. */

    /*
     * Lazy reverse-BFS state for VLE_FUNCTION_PATHS_BETWEEN pruning.
     *
     * This state is target-specific: reverse_dist_table is valid only
     * for the current veid and is rebuilt when veid changes.  It is
     * deliberately separate from vertex_edge_cache, whose entries are
     * valid for the whole VLE_local_context.
     */
    HTAB *reverse_dist_table;       /* vertex_id -> reverse_dist_entry,
                                      * valid only for the current veid */
    MemoryContext reverse_dist_mcxt; /* owns reverse_dist_table and
                                      * reverse_dist_queue.data */
    bool reverse_dist_initialized;  /* true after reverse_dist_table has
                                      * been created for this vlelctx */
    graphid reverse_dist_target;    /* veid for reverse_dist_table */
    bool reverse_dist_exhausted;    /* true once the reverse-BFS frontier
                                      * is fully drained (or past the uidx
                                      * budget) -- a PROOF that any vertex
                                      * not already in reverse_dist_table
                                      * is unreachable within scope. Only
                                      * this flag may make
                                      * get_or_advance_reverse_dist() return
                                      * PG_INT64_MAX for an unknown vertex. */
    bool reverse_dist_capped;       /* true once age.vle_reverse_dist_max_entries
                                      * stopped the reverse BFS from
                                      * admitting new frontier vertices.
                                      * This is NOT a reachability proof --
                                      * it just means we gave up early to
                                      * respect a memory budget -- so it
                                      * must never be treated the way
                                      * reverse_dist_exhausted is. */
    rdist_queue reverse_dist_queue; /* reverse-BFS frontier */
    GraphIdStack *dfs_vertex_stack; /* dfs stack for vertices (array-based) */
    GraphIdStack *dfs_edge_stack;   /* dfs stack for edges (array-based) */
    GraphIdStack *dfs_path_stack;   /* dfs stack containing the path (array-based) */
    VLE_path_function path_function; /* which path function to use */
    GraphIdNode *next_vertex;      /* for VLE_FUNCTION_PATHS_TO */
    int64 vle_grammar_node_id;     /* the unique VLE grammar assigned node id */
    bool use_cache;                /* are we using VLE_local_context cache */
    struct VLE_local_context *next;  /* the next chained VLE_local_context */
    bool is_dirty;                 /* is this VLE context reusable */
} VLE_local_context;

/*
 * Container to hold the graphid array that contains one valid path. This
 * structure will allow it to be easily passed as an AGTYPE pointer. The
 * structure is set up to contains a BINARY container that can be accessed by
 * functions that need to process the path.
 */
/*
 * Layout (offsets, with int64 alignment):
 *
 *     0:  vl_len_[4]              varlena length header (int32 + pad)
 *     4:  header                  AGT_FBINARY | AGT_FBINARY_TYPE_VLE_PATH
 *     8:  graph_oid               source graph oid
 *    12:  (4 bytes pad)           int64 alignment
 *    16:  graphid_array_size      number of graphids in the path
 *    24:  container_size_bytes    total bytes of this container
 *    32:  start_vid               redundant cache of graphid_array[0]
 *    40:  end_vid                 redundant cache of
 *                                 graphid_array[graphid_array_size - 1]
 *    48:  graphid_array_data      flexible array start
 *
 * start_vid / end_vid are populated whenever the container is built and let
 * downstream consumers (the age_vle SRF's start_id/end_id output columns)
 * read the join endpoints without traversing the (potentially toasted)
 * variadic payload.
 *
 * Persistence note: VLE_path_container is a transient SRF output. It is
 * consumed within the same query that produces it (by the planner-emitted
 * endpoint equalities and by age_materialize_vle_path / _vle_edges) and is
 * never written back to disk. Because no on-disk instance of this layout
 * can exist, adding fields to the struct does not require a binary
 * version bump or a backward-compatible decoder. If a future change ever
 * makes a VLE container persistable (e.g. by allowing it to be returned
 * directly as agtype and stored in a column), the AGT_FBINARY_TYPE_VLE_PATH
 * tag must be versioned and the readers (GET_GRAPHID_ARRAY_FROM_CONTAINER
 * etc.) must branch on the version.
 */
typedef struct VLE_path_container
{
    char vl_len_[4]; /* Do not touch this field! */
    uint32 header;
    uint32 graph_oid;
    int64 graphid_array_size;
    int64 container_size_bytes;
    graphid start_vid;
    graphid end_vid;
    graphid graphid_array_data;
} VLE_path_container;

/* declarations */

/* global variable to hold the per process global cached VLE_local contexts */
static VLE_local_context *global_vle_local_contexts = NULL;

/* agtype functions */
static bool is_an_edge_match(VLE_local_context *vlelctx, edge_entry *ee);
/* VLE local context functions */
static VLE_local_context *build_local_vle_context(FunctionCallInfo fcinfo,
                                                  FuncCallContext *funcctx);
static void create_VLE_local_state_hashtable(VLE_local_context *vlelctx);
static void free_VLE_local_context(VLE_local_context *vlelctx);
/* VLE graph traversal functions */
static edge_state_entry *get_edge_state_with_hash(VLE_local_context *vlelctx,
                                                  graphid edge_id,
                                                  uint32 hashvalue);
static edge_state_entry *find_edge_state_with_hash(VLE_local_context *vlelctx,
                                                    graphid edge_id,
                                                    uint32 hashvalue);
static edge_state_entry *insert_matched_edge_state(VLE_local_context *vlelctx,
                                                    graphid edge_id,
                                                    uint32 hashvalue,
                                                    edge_entry *ee);
static void evict_edge_state_entries_if_needed(VLE_local_context *vlelctx);
static void evict_vertex_edge_cache_entries_if_needed(VLE_local_context *vlelctx);
/* graphid data structures */
static void load_initial_dfs_stacks(VLE_local_context *vlelctx);
static bool dfs_find_a_path_between(VLE_local_context *vlelctx);
static bool dfs_find_a_path_from(VLE_local_context *vlelctx);
static bool do_vsid_and_veid_exist(VLE_local_context *vlelctx);
static void add_valid_vertex_edges(VLE_local_context *vlelctx,
                                   graphid vertex_id);
static graphid get_next_vertex(VLE_local_context *vlelctx, edge_entry *ee);
static graphid get_next_vertex_from_state(VLE_local_context *vlelctx,
                                          edge_state_entry *ese);
static vertex_edge_cache_entry *get_or_build_vertex_edge_cache(
                                                    VLE_local_context *vlelctx,
                                                    graphid vertex_id);
static bool is_edge_in_path(VLE_local_context *vlelctx, graphid edge_id);
/* reverse-BFS pruning for VLE_FUNCTION_PATHS_BETWEEN (see add_valid_vertex_edges) */
static cypher_rel_dir flip_edge_direction(cypher_rel_dir dir);
static void reset_reverse_dist_state_if_needed(VLE_local_context *vlelctx);
static void set_reverse_dist(VLE_local_context *vlelctx, graphid vertex_id,
                             int64 dist);
static bool try_get_reverse_dist(VLE_local_context *vlelctx, graphid vertex_id,
                                 int64 *dist);
static void rdist_expand_vertex(VLE_local_context *vlelctx, graphid u,
                                int64 du, cypher_rel_dir flipped_dir);
static int64 get_or_advance_reverse_dist(VLE_local_context *vlelctx,
                                         graphid w);
static void rdist_queue_push(VLE_local_context *vlelctx, rdist_queue *q,
                             graphid v);
static graphid rdist_queue_pop(rdist_queue *q);
static bool rdist_queue_is_empty(rdist_queue *q);
static void rdist_queue_reset(VLE_local_context *vlelctx, rdist_queue *q);
/* VLE path and edge building functions */
static VLE_path_container *create_VLE_path_container(int64 path_size);
static VLE_path_container *build_VLE_path_container(VLE_local_context *vlelctx);
static VLE_path_container *build_VLE_zero_container(VLE_local_context *vlelctx);
static agtype_value *build_path(VLE_path_container *vpc);
static agtype_value *build_edge_list(VLE_path_container *vpc);
/* VLE_local_context cache management */
static VLE_local_context *get_cached_VLE_local_context(int64 vle_node_id);
static void cache_VLE_local_context(VLE_local_context *vlelctx);

/* definitions */

/*
 * Helper function to retrieve a cached VLE local context. It will also purge
 * off any contexts beyond the maximum defined number of cached contexts. It
 * will promote (a very basic LRU) the recently fetched context to the head of
 * the list. If a context doesn't exist or is dirty, it will purge it off and
 * return NULL.
 */
static VLE_local_context *get_cached_VLE_local_context(int64 vle_grammar_node_id)
{
    VLE_local_context *vlelctx = global_vle_local_contexts;
    VLE_local_context *prev = NULL;
    VLE_local_context *next = NULL;
    int cache_size = 0;

    /* while we have contexts to check */
    while (vlelctx != NULL)
    {
        /* purge any contexts past the maximum cache size */
        if (cache_size >= MAXIMUM_NUMBER_OF_CACHED_LOCAL_CONTEXTS)
        {
            /* set the next pointer to the context that follows */
            next = vlelctx->next;

            /*
             * Clear (unlink) the previous context's next pointer, if needed.
             * Also clear prev as we are at the end of available cached contexts
             * and just purging them off. Remember, this forms a loop that will
             * exit the while after purging.
             */
            if (prev != NULL)
            {
                prev->next = NULL;
                prev = NULL;
            }

            /* free the context */
            free_VLE_local_context(vlelctx);

            /* set to the next one */
            vlelctx = next;

            /* if there is another context beyond the max, we will re-enter */
            continue;
        }

        /* if this context belongs to this grammar node */
        if (vlelctx->vle_grammar_node_id == vle_grammar_node_id)
        {
            /* and isn't dirty */
            if (vlelctx->is_dirty == false)
            {
                GRAPH_global_context *ggctx = NULL;

                /*
                 * Get the GRAPH global context associated with this local VLE
                 * context. We need to verify it still exists and that the
                 * pointer is valid.
                 */
                ggctx = find_GRAPH_global_context(vlelctx->graph_oid);

                /*
                 * If ggctx == NULL, vlelctx is bad and vlelctx needs to be
                 * removed.
                 * If ggctx == vlelctx->ggctx, then vlelctx is good.
                 * If ggctx != vlelctx->ggctx, then vlelctx needs to be updated.
                 * In the end, vlelctx->ggctx will be set to ggctx.
                 */

                /*
                 * If the returned ggctx isn't valid (there was some update to
                 * the underlying graph), then set it to NULL. This will force a
                 * rebuild of it.
                 */
                if (ggctx != NULL && is_ggctx_invalid(ggctx))
                {
                    ggctx = NULL;
                }

                vlelctx->ggctx = ggctx;

                /*
                 * If the context is good and isn't at the head of the cache,
                 * promote it to the head.
                 */
                if (ggctx != NULL && vlelctx != global_vle_local_contexts)
                {
                    /* adjust the links to cut out the node */
                    prev->next = vlelctx->next;
                    /* point the context to the old head of the list */
                    vlelctx->next = global_vle_local_contexts;
                    /* point the head to this context */
                    global_vle_local_contexts = vlelctx;
                }

                /* if we have a good one, return it. */
                if (ggctx != NULL)
                {
                    return vlelctx;
                }
            }

            /* otherwise, clean and remove it, and return NULL */

            /* set the top if necessary and unlink it */
            if (prev == NULL)
            {
                global_vle_local_contexts = vlelctx->next;
            }
            else
            {
                prev->next = vlelctx->next;
            }

            /* now free it and return NULL */
            free_VLE_local_context(vlelctx);
            return NULL;
        }
        /* save the previous context */
        prev = vlelctx;
        /* get the next context */
        vlelctx = vlelctx->next;
        /* keep track of cache size */
        cache_size++;
    }
    return vlelctx;
}

static void cache_VLE_local_context(VLE_local_context *vlelctx)
{
    /* if the context passed is null, just return */
    if (vlelctx == NULL)
    {
        return;
    }

    /* if the global link is null, just assign it the local context */
    if (global_vle_local_contexts == NULL)
    {
        global_vle_local_contexts = vlelctx;
        return;
    }

    /* if there is a global link, add the local context to the top */
    vlelctx->next = global_vle_local_contexts;
    global_vle_local_contexts = vlelctx;
}

/* helper function to create the local VLE edge state hashtable. */
static void create_VLE_local_state_hashtable(VLE_local_context *vlelctx)
{
    HASHCTL edge_state_ctl;
    char *graph_name = NULL;
    char *eshn = NULL;
    int glen;
    int elen;

    /* get the graph name and length */
    graph_name = vlelctx->graph_name;
    glen = strlen(graph_name);
    /* get the edge state htab name length */
    elen = strlen(EDGE_STATE_HTAB_NAME);
    /* allocate the space and build the name */
    eshn = palloc0(elen + glen + 1);
    /* copy in the name */
    strcpy(eshn, EDGE_STATE_HTAB_NAME);
    /* add in the graph name */
    eshn = strncat(eshn, graph_name, glen);

    /* initialize the edge state hashtable */
    MemSet(&edge_state_ctl, 0, sizeof(edge_state_ctl));
    edge_state_ctl.keysize = sizeof(int64);
    edge_state_ctl.entrysize = sizeof(edge_state_entry);
    edge_state_ctl.hash = graphid_hash;
    vlelctx->edge_state_hashtable = hash_create(eshn,
                                                vle_edge_state_htab_initial_size,
                                                &edge_state_ctl,
                                                HASH_ELEM | HASH_FUNCTION);
    pfree_if_not_null(eshn);

    /*
     * Create a dedicated child context to own every valid_edges[] array
     * that will ever be allocated by get_or_build_vertex_edge_cache().
     *
     * We deliberately do NOT just capture CurrentMemoryContext into a bare
     * MemoryContext field and palloc directly into it: this function is
     * called from three different sites (the LRU-cached path in
     * build_local_vle_context(), which runs under TopMemoryContext; the
     * uncached SRF path, which runs under funcctx->multi_call_memory_ctx;
     * and sp_minhops_fallback(), which runs under its own private scratch
     * context). All three currently do the right thing, but relying on
     * "whichever context the caller happened to switch to before calling
     * in here" is exactly the kind of ambient-context assumption that
     * silently breaks under refactoring. Creating our OWN child context
     * here means its lifetime is entirely our responsibility, symmetric
     * with hash_destroy() below: we explicitly MemoryContextDelete() it in
     * free_VLE_local_context(), the same way hash_destroy() explicitly
     * frees edge_state_hashtable's storage. Correctness no longer depends
     * on what CurrentMemoryContext happens to be, here or at any future
     * call site -- we still parent off it (so it is reclaimed for free if
     * an ancestor context is ever deleted first, e.g. via
     * sp_minhops_fallback()'s MemoryContextDelete(tmpctx)), but we never
     * depend on that parent to be the one doing the cleanup.
     */
    vlelctx->vertex_edge_cache_mcxt = AllocSetContextCreate(CurrentMemoryContext,
                                                            "VLE vertex edge cache",
                                                            ALLOCSET_DEFAULT_SIZES);

    /* initialize the per-vertex valid-edge cache (see get_or_build_vertex_edge_cache) */
    {
        HASHCTL vertex_edge_ctl;

        MemSet(&vertex_edge_ctl, 0, sizeof(vertex_edge_ctl));
        vertex_edge_ctl.keysize = sizeof(int64);
        vertex_edge_ctl.entrysize = sizeof(vertex_edge_cache_entry);
        vertex_edge_ctl.hash = graphid_hash;
        vlelctx->vertex_edge_cache = hash_create("VLE vertex edge cache",
                                                 vle_vertex_edge_htab_initial_size,
                                                 &vertex_edge_ctl,
                                                 HASH_ELEM | HASH_FUNCTION);
    }

    /*
     * Dedicated context for reverse-BFS pruning state.  The table itself
     * is created lazily because only PATHS_BETWEEN uses it.
     */
    vlelctx->reverse_dist_mcxt = AllocSetContextCreate(CurrentMemoryContext,
                                                       "VLE reverse distance",
                                                       ALLOCSET_DEFAULT_SIZES);
}

/*
 * Helper function to compare the edge constraint (properties we are looking
 * for in a matching edge) against an edge entry's property.
 */
static bool is_an_edge_match(VLE_local_context *vlelctx, edge_entry *ee)
{
    agtype *edge_property = NULL;
    agtype_container *agtc_edge_property = NULL;
    agtype_container *agtc_edge_property_constraint = NULL;
    agtype_iterator *constraint_it = NULL;
    agtype_iterator *property_it = NULL;
    Oid edge_label_name_oid = InvalidOid;
    int num_edge_property_constraints = 0;
    int num_edge_properties = 0;

    /* get the number of conditions from the prototype edge */
    num_edge_property_constraints = AGT_ROOT_COUNT(vlelctx->edge_property_constraint);

    /*
     * Issue #2382: If the user asked for a specific edge label but that label
     * does not exist in the graph (edge_label_name_oid == InvalidOid while
     * edge_label_name is non-NULL), no real edge can match. Returning false
     * here ensures that for VLE patterns like [:NOEXIST*0..N] we do not
     * traverse arbitrary other-label edges. Zero-hop self-binding is handled
     * separately via build_VLE_zero_container() so this does not break it.
     */
    if (vlelctx->edge_label_name != NULL &&
        vlelctx->edge_label_name_oid == InvalidOid)
    {
        return false;
    }

    /*
     * We only care about verifying that we have all of the property conditions.
     * We don't care about extra unmatched properties. If there aren't any edge
     * constraints, then the edge passes by default.
     */
    if (vlelctx->edge_label_name_oid == InvalidOid &&
        num_edge_property_constraints == 0)
    {
        return true;
    }

    /* get the edge label oid */
    edge_label_name_oid = get_edge_entry_label_table_oid(ee);

    /*
     * Check for a label constraint. Remember, if the constraint label oid is
     * InvalidOid, there isn't one. If there is one, they need to match.
     */
    if (vlelctx->edge_label_name_oid != InvalidOid &&
        vlelctx->edge_label_name_oid != edge_label_name_oid)
    {
        return false;
    }

    /*
     * Fast path: if the label matched (or wasn't constrained) and there
     * are no property constraints, the edge is a match. This avoids
     * accessing edge properties entirely for label-only VLE patterns
     * like [:KNOWS*1..3] which are the common case.
     */
    if (num_edge_property_constraints == 0)
    {
        return true;
    }

    /*
     * Fetch edge properties once and cache locally. With thin entries,
     * get_edge_entry_properties() does a heap_fetch, so we avoid calling
     * it multiple times for the same edge.
     */
    {
        Datum edge_props_datum = get_edge_entry_properties(ee);

        edge_property = DATUM_GET_AGTYPE_P(edge_props_datum);
        agtc_edge_property_constraint = &vlelctx->edge_property_constraint->root;
        agtc_edge_property = &edge_property->root;
        num_edge_properties = AGTYPE_CONTAINER_SIZE(agtc_edge_property);

        /*
         * Check to see if the edge_properties object has AT LEAST as many
         * pairs to compare as the edge_property_constraint object has pairs.
         * If not, it can't possibly match.
         */
        if (num_edge_property_constraints > num_edge_properties)
        {
            return false;
        }

        /*
         * If the number of constraints are the same as the number of
         * properties, then the datums would be the same if they match.
         */
        if (num_edge_property_constraints == num_edge_properties)
        {
            uint32 edge_props_hash = datum_image_hash(edge_props_datum,
                                                      false, -1);
            /* check the hash first */
            if (vlelctx->edge_property_constraint_hash == edge_props_hash)
            {
                /* if the hashes match, check the datum images */
                if (datum_image_eq(vlelctx->edge_property_constraint_datum,
                                   edge_props_datum, false, -1))
                {
                    return true;
                }
            }

            /* if we got here they aren't the same */
            return false;
        }

        /* get the iterators */
        constraint_it = agtype_iterator_init(agtc_edge_property_constraint);
        property_it = agtype_iterator_init(agtc_edge_property);

        /* return the value of deep contains */
        return agtype_deep_contains(&property_it, &constraint_it, false);
    }
}

/*
 * Helper function to free up the memory used by the VLE_local_context.
 *
 * Currently, the only structures that needs to be freed are the edge state
 * hashtable and the dfs stacks (vertex, edge, and path). The hashtable is easy
 * because hash_create packages everything into its own memory context. So, you
 * only need to do a destroy.
 */
static void free_VLE_local_context(VLE_local_context *vlelctx)
{
    /* if the VLE context is NULL, do nothing */
    if (vlelctx == NULL)
    {
        return;
    }

    /* free the stored graph name */
    if (vlelctx->graph_name != NULL)
    {
        pfree_if_not_null(vlelctx->graph_name);
        vlelctx->graph_name = NULL;
    }

    /* free the stored edge label name */
    if (vlelctx->edge_label_name != NULL)
    {
        pfree_if_not_null(vlelctx->edge_label_name);
        vlelctx->edge_label_name = NULL;
    }

    /* we need to free our state hashtable */
    hash_destroy(vlelctx->edge_state_hashtable);
    vlelctx->edge_state_hashtable = NULL;

    /*
     * Free the vertex edge cache's own hashtable storage (its entries --
     * fixed-size vertex_edge_cache_entry structs, held in dynahash's own
     * private child context). This does NOT free the valid_edges[] arrays
     * those entries point to.
     */
    if (vlelctx->vertex_edge_cache != NULL)
    {
        hash_destroy(vlelctx->vertex_edge_cache);
        vlelctx->vertex_edge_cache = NULL;
    }

    /*
     * Explicitly delete the dedicated context that owns every valid_edges[]
     * array. This is what actually reclaims that memory -- hash_destroy()
     * above never touches it. Symmetric, on purpose, with the hash_destroy()
     * calls: every allocator this function uses gets an explicit, owned
     * teardown call here, none of them are left to an ambient parent
     * context to clean up "eventually".
     */
    if (vlelctx->vertex_edge_cache_mcxt != NULL)
    {
        MemoryContextDelete(vlelctx->vertex_edge_cache_mcxt);
        vlelctx->vertex_edge_cache_mcxt = NULL;
    }

    /*
     * Free the reverse-BFS pruning state.  The dedicated context owns
     * both the distance table storage and the queue data.
     */
    if (vlelctx->reverse_dist_table != NULL)
    {
        hash_destroy(vlelctx->reverse_dist_table);
        vlelctx->reverse_dist_table = NULL;
    }
    if (vlelctx->reverse_dist_mcxt != NULL)
    {
        MemoryContextDelete(vlelctx->reverse_dist_mcxt);
        vlelctx->reverse_dist_mcxt = NULL;
    }
    vlelctx->reverse_dist_queue.data = NULL;
    vlelctx->reverse_dist_queue.head = 0;
    vlelctx->reverse_dist_queue.tail = 0;
    vlelctx->reverse_dist_queue.cap = 0;
    vlelctx->reverse_dist_queue.count = 0;
    vlelctx->reverse_dist_queue.max_count = 0;

    /*
     * Free the DFS stacks. When is_dirty is false, the stacks are in the
     * current context and need explicit cleanup. When is_dirty is true
     * (cached context), only free the containers — the contents were
     * allocated in a volatile SRF context that was already cleaned up.
     */
    if (vlelctx->dfs_vertex_stack != NULL)
    {
        free_gid_stack(vlelctx->dfs_vertex_stack);
    }
    if (vlelctx->dfs_edge_stack != NULL)
    {
        free_gid_stack(vlelctx->dfs_edge_stack);
    }
    if (vlelctx->dfs_path_stack != NULL)
    {
        free_gid_stack(vlelctx->dfs_path_stack);
    }
    vlelctx->dfs_vertex_stack = NULL;
    vlelctx->dfs_edge_stack = NULL;
    vlelctx->dfs_path_stack = NULL;

    /* and finally the context itself */
    pfree_if_not_null(vlelctx);
    vlelctx = NULL;
}

/* helper function to check if our start and end vertices exist */
static bool do_vsid_and_veid_exist(VLE_local_context *vlelctx)
{
    /* if we are only using the starting vertex */
    if (vlelctx->path_function == VLE_FUNCTION_PATHS_FROM ||
        vlelctx->path_function == VLE_FUNCTION_PATHS_ALL)
    {
        return (get_vertex_entry(vlelctx->ggctx, vlelctx->vsid) != NULL);
    }

    /* if we are only using the ending vertex */
    if (vlelctx->path_function == VLE_FUNCTION_PATHS_TO)
    {
        return (get_vertex_entry(vlelctx->ggctx, vlelctx->veid) != NULL);
    }

    /* if we are using both start and end */
    return ((get_vertex_entry(vlelctx->ggctx, vlelctx->vsid) != NULL) &&
            (get_vertex_entry(vlelctx->ggctx, vlelctx->veid) != NULL));
}

/* load the initial edges into the dfs_edge_stack */
static void load_initial_dfs_stacks(VLE_local_context *vlelctx)
{
    /*
     * If either the vsid or veid don't exist - don't load anything because
     * there won't be anything to find.
     */
    if (!do_vsid_and_veid_exist(vlelctx))
    {
        return;
    }

    /*
    * Every fresh traversal passes through here, so this is the correct
    * place to reset reverse-BFS pruning state if the target changed.
    */
    reset_reverse_dist_state_if_needed(vlelctx);

    /* add in the edges for the start vertex */
    add_valid_vertex_edges(vlelctx, vlelctx->vsid);
}

/*
 * Helper function to build the local VLE context. This is also the point
 * where, if necessary, the global GRAPH contexts are created and freed.
 */
static VLE_local_context *build_local_vle_context(FunctionCallInfo fcinfo,
                                                  FuncCallContext *funcctx)
{
    MemoryContext oldctx = NULL;
    GRAPH_global_context *ggctx = NULL;
    VLE_local_context *vlelctx = NULL;
    agtype_value *agtv_temp = NULL;
    agtype_value *agtv_object = NULL;
    agtype *agt_edge_property_constraint = NULL;
    Datum d_edge_property_constraint = 0;
    char *graph_name = NULL;
    Oid graph_oid = InvalidOid;
    int64 vle_grammar_node_id = 0;
    bool use_cache = false;

    /*
     * Get the VLE grammar node id, if it exists. Remember, we overload the
     * age_vle function, for now, for backwards compatibility
     */
    if (PG_NARGS() == 8)
    {
        /* get the VLE grammar node id */
        agtv_temp = get_agtype_value("age_vle", AG_GET_ARG_AGTYPE_P(7),
                                     AGTV_INTEGER, true);
        vle_grammar_node_id = agtv_temp->val.int_value;

        /* we are using the VLE local context cache, so set it */
        use_cache = true;
    }

    /* fetch the VLE_local_context if it is cached */
    vlelctx = get_cached_VLE_local_context(vle_grammar_node_id);

    /* if we are caching VLE_local_contexts and this grammar node is cached */
    if (use_cache && vlelctx != NULL)
    {
        /*
         * No context change is needed here as the cache entry is in the proper
         * context. Additionally, all of the modifications are either pointers
         * to objects already in the proper context or primitive types that will
         * be stored in that context since the memory is allocated there.
         */

        /* get and update the start vertex id */
        if (PG_ARGISNULL(1) || is_agtype_null(AG_GET_ARG_AGTYPE_P(1)))
        {
            /* if there are no more vertices to process, return NULL */
            if (vlelctx->next_vertex == NULL)
            {
                return NULL;
            }
            vlelctx->vsid = get_graphid(vlelctx->next_vertex);
            /* increment to the next vertex */
            vlelctx->next_vertex = next_GraphIdNode(vlelctx->next_vertex);
        }
        else
        {
            agtv_temp = get_agtype_value("age_vle", AG_GET_ARG_AGTYPE_P(1),
                                         AGTV_VERTEX, false);
            if (agtv_temp != NULL && agtv_temp->type == AGTV_VERTEX)
            {
                agtv_temp = GET_AGTYPE_VALUE_OBJECT_VALUE(agtv_temp, "id");
            }
            else if (agtv_temp == NULL || agtv_temp->type != AGTV_INTEGER)
            {
                ereport(ERROR,
                        (errcode(ERRCODE_INVALID_PARAMETER_VALUE),
                         errmsg("start vertex argument must be a vertex or the integer id")));
            }
            vlelctx->vsid = agtv_temp->val.int_value;
        }

        /* get and update the end vertex id */
        if (PG_ARGISNULL(2) || is_agtype_null(AG_GET_ARG_AGTYPE_P(2)))
        {
            vlelctx->veid = 0;
        }
        else
        {
            agtv_temp = get_agtype_value("age_vle", AG_GET_ARG_AGTYPE_P(2),
                                         AGTV_VERTEX, false);
            if (agtv_temp != NULL && agtv_temp->type == AGTV_VERTEX)
            {
                agtv_temp = GET_AGTYPE_VALUE_OBJECT_VALUE(agtv_temp, "id");
            }
            else if (agtv_temp == NULL || agtv_temp->type != AGTV_INTEGER)
            {
                ereport(ERROR,
                        (errcode(ERRCODE_INVALID_PARAMETER_VALUE),
                         errmsg("end vertex argument must be a vertex or the integer id")));
            }
            vlelctx->veid = agtv_temp->val.int_value;
        }
        vlelctx->is_dirty = true;

        /* we need the SRF context to add in the edges to the stacks */
        oldctx = MemoryContextSwitchTo(funcctx->multi_call_memory_ctx);

        /* load the initial edges into the dfs stacks */
        load_initial_dfs_stacks(vlelctx);

        /* switch back to the original context */
        MemoryContextSwitchTo(oldctx);

        /* return the context */
        return vlelctx;
    }

    /* we are not using a cached VLE_local_context, so create a new one */

    /*
     * If we are going to cache this context, we need to use TopMemoryContext
     * to save the contents of the context. Otherwise, we just use a regular
     * context for SRFs
     */
    if (use_cache == true)
    {
        oldctx = MemoryContextSwitchTo(TopMemoryContext);
    }
    else
    {
        oldctx = MemoryContextSwitchTo(funcctx->multi_call_memory_ctx);
    }

    /* get the graph name - this is a required argument */
    agtv_temp = get_agtype_value("age_vle", AG_GET_ARG_AGTYPE_P(0),
                                 AGTV_STRING, true);
    graph_name = pnstrdup(agtv_temp->val.string.val,
                          agtv_temp->val.string.len);
    /* get the graph oid */
    graph_oid = get_graph_oid(graph_name);

    /*
     * Create or retrieve the GRAPH global context for this graph. This function
     * will also purge off invalidated contexts.
    */
    ggctx = manage_GRAPH_global_contexts(graph_name, graph_oid);

    /* allocate and initialize local VLE context */
    vlelctx = palloc0(sizeof(VLE_local_context));

    /* store the cache usage */
    vlelctx->use_cache = use_cache;

    /* set the VLE grammar node id */
    vlelctx->vle_grammar_node_id = vle_grammar_node_id;

    /* set the graph name and id */
    vlelctx->graph_name = graph_name;
    vlelctx->graph_oid = graph_oid;

    /* set the global context referenced by this local VLE context */
    vlelctx->ggctx = ggctx;

    /* initialize the path function */
    vlelctx->path_function = VLE_FUNCTION_PATHS_BETWEEN;

    /* initialize the next vertex, in this case the first */
    vlelctx->next_vertex = peek_stack_head(get_graph_vertices(ggctx));

    /* if there isn't one, the graph is empty */
    if (vlelctx->next_vertex == NULL)
    {
        elog(ERROR, "age_vle: empty graph");
    }
    /*
     * Get the start vertex id - this is an optional parameter and determines
     * which path function is used. If a start vertex isn't provided, we
     * retrieve them incrementally from the vertices list.
     */
    if (PG_ARGISNULL(1) || is_agtype_null(AG_GET_ARG_AGTYPE_P(1)))
    {
        /* set _TO */
        vlelctx->path_function = VLE_FUNCTION_PATHS_TO;

        /* get the start vertex */
        vlelctx->vsid = get_graphid(vlelctx->next_vertex);
        /* increment to the next vertex */
        vlelctx->next_vertex = next_GraphIdNode(vlelctx->next_vertex);
    }
    else
    {
        agtv_temp = get_agtype_value("age_vle", AG_GET_ARG_AGTYPE_P(1),
                                     AGTV_VERTEX, false);
        if (agtv_temp != NULL && agtv_temp->type == AGTV_VERTEX)
        {
            agtv_temp = GET_AGTYPE_VALUE_OBJECT_VALUE(agtv_temp, "id");
        }
        else if (agtv_temp == NULL || agtv_temp->type != AGTV_INTEGER)
        {
            ereport(ERROR,
                    (errcode(ERRCODE_INVALID_PARAMETER_VALUE),
                     errmsg("start vertex argument must be a vertex or the integer id")));
        }
        vlelctx->vsid = agtv_temp->val.int_value;
    }

    /*
     * Get the end vertex id - this is an optional parameter and determines
     * which path function is used.
     */
    if (PG_ARGISNULL(2) || is_agtype_null(AG_GET_ARG_AGTYPE_P(2)))
    {
        if (vlelctx->path_function == VLE_FUNCTION_PATHS_TO)
        {
            vlelctx->path_function = VLE_FUNCTION_PATHS_ALL;
        }
        else
        {
            vlelctx->path_function = VLE_FUNCTION_PATHS_FROM;
        }
        vlelctx->veid = 0;
    }
    else
    {
        agtv_temp = get_agtype_value("age_vle", AG_GET_ARG_AGTYPE_P(2),
                                     AGTV_VERTEX, false);
        if (agtv_temp != NULL && agtv_temp->type == AGTV_VERTEX)
        {
            agtv_temp = GET_AGTYPE_VALUE_OBJECT_VALUE(agtv_temp, "id");
        }
        else if (agtv_temp == NULL || agtv_temp->type != AGTV_INTEGER)
        {
            ereport(ERROR,
                    (errcode(ERRCODE_INVALID_PARAMETER_VALUE),
                     errmsg("end vertex argument must be a vertex or the integer id")));
        }
        vlelctx->path_function = VLE_FUNCTION_PATHS_BETWEEN;
        vlelctx->veid = agtv_temp->val.int_value;
    }

    /* get the VLE edge prototype */
    agtv_temp = get_agtype_value("age_vle", AG_GET_ARG_AGTYPE_P(3),
                                 AGTV_EDGE, true);

    /* get the edge prototype's property conditions */
    agtv_object = GET_AGTYPE_VALUE_OBJECT_VALUE(agtv_temp, "properties");
    agt_edge_property_constraint = agtype_value_to_agtype(agtv_object);

    /* store the properties as an agtype */
    vlelctx->edge_property_constraint = agt_edge_property_constraint;

    d_edge_property_constraint = AGTYPE_P_GET_DATUM(agt_edge_property_constraint);
    vlelctx->edge_property_constraint_datum = d_edge_property_constraint;
    vlelctx->edge_property_constraint_hash = datum_image_hash(d_edge_property_constraint, false, -1);

    /* get the edge prototype's label name */
    agtv_temp = GET_AGTYPE_VALUE_OBJECT_VALUE(agtv_temp, "label");
    if (agtv_temp->type == AGTV_STRING &&
        agtv_temp->val.string.len != 0)
    {
        vlelctx->edge_label_name = pnstrdup(agtv_temp->val.string.val,
                                            agtv_temp->val.string.len);

        vlelctx->edge_label_name_oid = get_label_relation(vlelctx->edge_label_name,
                                                          graph_oid);
    }
    else
    {
        vlelctx->edge_label_name = NULL;
        vlelctx->edge_label_name_oid = InvalidOid;
    }

    /* get the left range index */
    if (PG_ARGISNULL(4) || is_agtype_null(AG_GET_ARG_AGTYPE_P(4)))
    {
        vlelctx->lidx = 1;
    }
    else
    {
        agtv_temp = get_agtype_value("age_vle", AG_GET_ARG_AGTYPE_P(4),
                                     AGTV_INTEGER, true);
        vlelctx->lidx = agtv_temp->val.int_value;
    }

    /* get the right range index. NULL means infinite */
    if (PG_ARGISNULL(5) || is_agtype_null(AG_GET_ARG_AGTYPE_P(5)))
    {
        vlelctx->uidx_infinite = true;
        vlelctx->uidx = 0;
    }
    else
    {
        agtv_temp = get_agtype_value("age_vle", AG_GET_ARG_AGTYPE_P(5),
                                     AGTV_INTEGER, true);
        vlelctx->uidx = agtv_temp->val.int_value;
        vlelctx->uidx_infinite = false;
    }
    /* get edge direction */
    agtv_temp = get_agtype_value("age_vle", AG_GET_ARG_AGTYPE_P(6),
                                 AGTV_INTEGER, true);
    vlelctx->edge_direction = agtv_temp->val.int_value;

    /* create the local state hashtable */
    create_VLE_local_state_hashtable(vlelctx);

    /* initialize the dfs stacks */
    vlelctx->dfs_vertex_stack = new_gid_stack();
    vlelctx->dfs_edge_stack = new_gid_stack();
    vlelctx->dfs_path_stack = new_gid_stack();

    /* load in the starting edge(s) */
    load_initial_dfs_stacks(vlelctx);

    /* this is a new one so nothing follows it */
    vlelctx->next = NULL;

    /* mark as dirty */
    vlelctx->is_dirty = true;

    /* if this is to be cached, cache it */
    if (use_cache == true)
    {
        cache_VLE_local_context(vlelctx);
    }

    /* switch back to the original context */
    MemoryContextSwitchTo(oldctx);

    /* return the new context */
    return vlelctx;
}

/*
 * Helper function to get the specified edge's state, using a precomputed hash
 * value. Callers can compute graphid_hash() once and reuse it for lookups in
 * both the dynahash edge_state_hashtable here and the agehash-backed
 * edge_table on the global-graph lookup path elsewhere.
 */
/*
 * Get (creating if necessary) the edge_state_entry for edge_id.
 *
 * Every caller -- the DFS peek at the top of dfs_find_a_path_between()/
 * from(), and the dynamic used_in_path re-check in
 * add_valid_vertex_edges() -- only ever asks about an edge already known
 * to be statically valid (matched): it came out of
 * vertex_edge_cache_entry->valid_edges[], or it is already sitting on
 * dfs_edge_stack. A lookup miss therefore never means "not yet
 * classified" (that only happens in get_or_build_vertex_edge_cache() /
 * rdist_expand_vertex(), which gate insertion on is_an_edge_match()); it
 * means the entry was reclaimed by evict_edge_state_entries_if_needed()
 * while unreferenced (pin_count == 0, not used_in_path). On a miss we
 * re-derive start/end from the underlying edge and recreate the entry in
 * the same neutral state (pin_count 0, flags 0) it was evicted in, which
 * makes eviction transparent to every caller.
 */
static edge_state_entry *get_edge_state_with_hash(VLE_local_context *vlelctx,
                                                  graphid edge_id,
                                                  uint32 hashvalue)
{
    edge_state_entry *ese = NULL;
    bool found = false;

    ese = (edge_state_entry *)hash_search_with_hash_value(
                                            vlelctx->edge_state_hashtable,
                                            (void *)&edge_id, hashvalue,
                                            HASH_ENTER, &found);
    if (!found)
    {
        edge_entry *ee = get_edge_entry_with_hash(vlelctx->ggctx, edge_id,
                                                  hashvalue);

        if (ee == NULL)
        {
            elog(ERROR, "get_edge_state_with_hash: no edge found");
        }

        /*
         * Callers only ever pass an edge_id already known to be a match
         * (see above), so it is not re-verified in production builds --
         * only re-confirmed here under assertions, to catch any future
         * caller that violates the precondition instead of silently
         * fabricating state for a non-matching edge.
         */
        Assert(is_an_edge_match(vlelctx, ee));

        ese->edge_id = edge_id;
        ese->state = 0;
        ese->start_vertex_id = get_edge_entry_start_vertex_id(ee);
        ese->end_vertex_id = get_edge_entry_end_vertex_id(ee);
    }

    /* mark as recently touched for the clock-eviction sweep */
    EDGE_STATE_ENTRY_SET_CLOCK_REF(ese);

    return ese;
}

/*
 * HASH_FIND-only counterpart of get_edge_state_with_hash(), used by the
 * classification call sites (get_or_build_vertex_edge_cache(),
 * rdist_expand_vertex()) to check whether edge_id has already been
 * classified as a match, without creating an entry as a side effect --
 * an edge that fails is_an_edge_match() must never get an entry, or
 * edge_state_hashtable would grow with every rejected edge instead of
 * just the matched ones.
 */
static edge_state_entry *find_edge_state_with_hash(VLE_local_context *vlelctx,
                                                    graphid edge_id,
                                                    uint32 hashvalue)
{
    bool found = false;
    edge_state_entry *ese;

    ese = (edge_state_entry *)hash_search_with_hash_value(
                                            vlelctx->edge_state_hashtable,
                                            (void *)&edge_id, hashvalue,
                                            HASH_FIND, &found);
    if (found)
    {
        EDGE_STATE_ENTRY_SET_CLOCK_REF(ese);
        return ese;
    }
    return NULL;
}

/*
 * Record edge_id as a freshly-matched edge in edge_state_hashtable. Only
 * called right after is_an_edge_match() returned true for it (see
 * get_or_build_vertex_edge_cache() and rdist_expand_vertex()), and only
 * when a prior find_edge_state_with_hash() call already established that
 * no entry exists yet -- so found is expected to come back false here.
 *
 * found is still checked rather than assumed: if it ever does come back
 * true (e.g. a future caller stops honoring that precondition), the
 * existing entry -- and in particular its pin_count and used_in_path,
 * which encode live DFS state -- must be left untouched. Overwriting them
 * unconditionally would silently corrupt in-progress path tracking.
 */
static edge_state_entry *insert_matched_edge_state(VLE_local_context *vlelctx,
                                                    graphid edge_id,
                                                    uint32 hashvalue,
                                                    edge_entry *ee)
{
    edge_state_entry *ese;
    bool found = false;

    ese = (edge_state_entry *) hash_search_with_hash_value(
                                            vlelctx->edge_state_hashtable,
                                            (void *) &edge_id, hashvalue,
                                            HASH_ENTER, &found);
    if (!found)
    {
        ese->edge_id = edge_id;
        ese->state = 0;
        ese->start_vertex_id = get_edge_entry_start_vertex_id(ee);
        ese->end_vertex_id = get_edge_entry_end_vertex_id(ee);
    }
    EDGE_STATE_ENTRY_SET_CLOCK_REF(ese);

    return ese;
}

/*
 * Clock-style eviction for edge_state_hashtable, triggered once the table
 * exceeds age.vle_edge_state_max_entries.
 *
 * Safety: an entry is only ever removed when BOTH pin_count == 0 (no live
 * occurrence anywhere on dfs_edge_stack -- see the struct comment on
 * edge_state_entry) AND it is not used_in_path. used_in_path can only be
 * set while at least one occurrence is pinned, so the pin_count check
 * alone is sufficient in a correctly-functioning traversal; the
 * used_in_path check is kept as a cheap, defensive belt-and-braces
 * condition. Every eviction is followed, on next access, by a transparent
 * re-derivation in get_edge_state_with_hash() -- see that function's
 * comment for why this is fully safe.
 *
 * A sweep evicts down to a low-water mark below the cap rather than
 * stopping the instant it dips under it, so the table has room to absorb
 * a batch of further insertions before the next full sweep is needed.
 * Without this, a workload that hovers right at the cap can end up
 * paying for a full table scan on every single insertion.
 *
 * At most two passes are made per call: a clock algorithm's first pass
 * may do no more than clear the "recently used" bit on every remaining
 * candidate (giving each one more reprieve), in which case a second pass
 * is what actually reclaims them. A third pass could never find anything
 * a second pass didn't already make eligible, so two is both necessary
 * and sufficient. If two passes still leave the table above the
 * low-water mark, everything left is pinned, and no amount of sweeping
 * will free more -- the table is simply that busy right now.
 *
 * hash_seq_search() explicitly supports removing the just-returned
 * element mid-scan (documented dynahash behavior), so HASH_REMOVE here is
 * safe to call from inside the scan loop.
 */
static void evict_edge_state_entries_if_needed(VLE_local_context *vlelctx)
{
    long low_watermark;
    int  pass;

    if (!vle_edge_state_eviction_enabled)
    {
        return;
    }

    if (hash_get_num_entries(vlelctx->edge_state_hashtable) <=
        vle_edge_state_max_entries)
    {
        return;
    }

    low_watermark = vle_edge_state_max_entries -
                        (vle_edge_state_max_entries / 8);

    for (pass = 0;
         pass < 2 &&
             hash_get_num_entries(vlelctx->edge_state_hashtable) >
                 low_watermark;
         pass++)
    {
        HASH_SEQ_STATUS status;
        edge_state_entry *ese;

        hash_seq_init(&status, vlelctx->edge_state_hashtable);
        while ((ese = (edge_state_entry *) hash_seq_search(&status)) != NULL)
        {
            if (EDGE_STATE_ENTRY_PIN_COUNT(ese) > 0 || EDGE_STATE_ENTRY_USE_IN_PATH(ese))
            {
                /* live occurrence(s) on dfs_edge_stack -- never evict */
                continue;
            }

            if (EDGE_STATE_ENTRY_CLOCK_REF(ese))
            {
                /* give it one more sweep before it becomes eligible */
                EDGE_STATE_ENTRY_UNSET_CLOCK_REF(ese);
                continue;
            }

            (void) hash_search_with_hash_value(vlelctx->edge_state_hashtable,
                                               (void *) &ese->edge_id,
                                               graphid_hash(&ese->edge_id,
                                                            sizeof(int64)),
                                               HASH_REMOVE, NULL);
        }
    }
}

/*
 * Helper function to get the id of the next vertex to move to. This is to
 * simplify finding the next vertex due to the VLE edge's direction.
 */
static graphid get_next_vertex(VLE_local_context *vlelctx, edge_entry *ee)
{
    graphid terminal_vertex_id;

    /* get the result based on the specified VLE edge direction */
    switch (vlelctx->edge_direction)
    {
        case CYPHER_REL_DIR_RIGHT:
            terminal_vertex_id = get_edge_entry_end_vertex_id(ee);
            break;

        case CYPHER_REL_DIR_LEFT:
            terminal_vertex_id = get_edge_entry_start_vertex_id(ee);
            break;

        case CYPHER_REL_DIR_NONE:
        {
            GraphIdStack *vertex_stack = NULL;
            graphid parent_vertex_id;

            vertex_stack = vlelctx->dfs_vertex_stack;
            /*
             * Get the parent vertex of this edge. When we are looking at edges
             * as un-directional, where we go to next depends on where we came
             * from. This is because we can go against an edge.
             */
            parent_vertex_id = gid_stack_peek(vertex_stack);
            /* find the terminal vertex */
            if (get_edge_entry_start_vertex_id(ee) == parent_vertex_id)
            {
                terminal_vertex_id = get_edge_entry_end_vertex_id(ee);
            }
            else if (get_edge_entry_end_vertex_id(ee) == parent_vertex_id)
            {
                terminal_vertex_id = get_edge_entry_start_vertex_id(ee);
            }
            else
            {
                elog(ERROR, "get_next_vertex: no parent match");
            }

            break;
        }

        default:
            elog(ERROR, "get_next_vertex: unknown edge direction");
    }

    return terminal_vertex_id;
}

/*
 * Cache-based counterpart to get_next_vertex(). Resolves the vertex the DFS
 * moves to when it takes edge `ese`, using only start_vertex_id/end_vertex_id
 * already cached on the edge_state_entry -- no edge_entry lookup required.
 *
 * Precondition: ese must represent a matched edge (guaranteed for any edge
 * that ever reaches the DFS hot loop, since only matched edges are ever
 * pushed onto dfs_edge_stack -- see get_or_build_vertex_edge_cache() and the
 * push site in add_valid_vertex_edges()).
 *
 * Mirrors get_next_vertex()'s branching exactly; see that function's
 * comments for why CYPHER_REL_DIR_NONE must consult the vertex stack (the
 * next vertex for an undirected edge depends on which endpoint the DFS is
 * currently standing on, not on the edge alone).
 */
static graphid get_next_vertex_from_state(VLE_local_context *vlelctx,
                                          edge_state_entry *ese)
{
    Assert(ese != NULL);

    switch (vlelctx->edge_direction)
    {
        case CYPHER_REL_DIR_RIGHT:
            return ese->end_vertex_id;

        case CYPHER_REL_DIR_LEFT:
            return ese->start_vertex_id;

        case CYPHER_REL_DIR_NONE:
        {
            graphid parent_vertex_id;

            /*
             * The whole vertex-stack scheme for CYPHER_REL_DIR_NONE relies
             * on dfs_vertex_stack and dfs_edge_stack staying in lockstep
             * (one vertex pushed/popped per edge pushed/popped -- see
             * add_valid_vertex_edges() and the backtracking branches in
             * dfs_find_a_path_between()/dfs_find_a_path_from()). Cheap
             * enough to check on every step in cassert builds, and it
             * would catch a future refactor breaking that invariant
             * immediately instead of manifesting as a confusing
             * "get_next_vertex_from_state: no parent match" error (or
             * worse, a wrong-but-plausible path) far away from the bug.
             */
            Assert(gid_stack_size(vlelctx->dfs_vertex_stack) ==
                   gid_stack_size(vlelctx->dfs_edge_stack));

            parent_vertex_id = gid_stack_peek(vlelctx->dfs_vertex_stack);

            if (ese->start_vertex_id == parent_vertex_id)
            {
                return ese->end_vertex_id;
            }
            else if (ese->end_vertex_id == parent_vertex_id)
            {
                return ese->start_vertex_id;
            }

            elog(ERROR, "get_next_vertex_from_state: no parent match");
        }

        default:
            elog(ERROR, "get_next_vertex_from_state: unknown edge direction");
    }

    return 0; /* unreachable; silences compiler control-reaches-end warning */
}

/*
 * Helper function to find one path BETWEEN two vertices.
 *
 * Note: On the very first entry into this function, the starting vertex's edges
 * should have already been loaded into the edge stack (this should have been
 * done by the SRF initialization phase).
 *
 * This function will always return on either a valid path found (true) or none
 * found (false). If one is found, the position (vertex & edge) will still be in
 * the stack. Each successive invocation within the SRF will then look for the
 * next available path until there aren't any left.
 */
static bool dfs_find_a_path_between(VLE_local_context *vlelctx)
{
    GraphIdStack *vertex_stack = NULL;
    GraphIdStack *edge_stack = NULL;
    GraphIdStack *path_stack = NULL;
    graphid end_vertex_id;

    Assert(vlelctx != NULL);

    /* for ease of reading */
    vertex_stack = vlelctx->dfs_vertex_stack;
    edge_stack = vlelctx->dfs_edge_stack;
    path_stack = vlelctx->dfs_path_stack;
    end_vertex_id = vlelctx->veid;

    /* while we have edges to process */
    while (!(gid_stack_is_empty(edge_stack)))
    {
        graphid edge_id;
        graphid next_vertex_id;
        edge_state_entry *ese = NULL;
        bool found = false;
        uint32 edge_hashvalue;

        /*
         * Allow this traversal to be cancelled (e.g. by a user Ctrl-C or a
         * statement_timeout). On a large or densely connected graph this DFS
         * can run for a long time, so we must yield to interrupt processing
         * on every iteration.
         */
        CHECK_FOR_INTERRUPTS();

        /* get an edge, but leave it on the stack for now */
        edge_id = gid_stack_peek(edge_stack);
        /*
         * Compute the hash for edge_id once and reuse it for the
         * edge_state_hashtable lookup.
         */
        edge_hashvalue = graphid_hash(&edge_id, sizeof(int64));
        /* get the edge's state */
        ese = get_edge_state_with_hash(vlelctx, edge_id, edge_hashvalue);
        /*
         * If the edge is already in use, it means that the edge is in the path.
         * So, we need to see if it is the last path entry (we are backing up -
         * we need to remove the edge from the path stack and reset its state
         * and from the edge stack as we are done with it) or an interior edge
         * in the path (loop - we need to remove the edge from the edge stack
         * and start with the next edge).
         */
        if (EDGE_STATE_ENTRY_USE_IN_PATH(ese))
        {
            graphid path_edge_id;

            /* get the edge id on the top of the path stack (last edge) */
            path_edge_id = gid_stack_peek(path_stack);
            /*
             * If the ids are the same, we're backing up. So, remove it from the
             * path stack and reset used_in_path.
             */
            if (edge_id == path_edge_id)
            {
                gid_stack_pop(path_stack);
                EDGE_STATE_ENTRY_UNSET_USE_IN_PATH(ese);
            }
            /* now remove it from the edge stack */
            gid_stack_pop(edge_stack);
            /*
             * This occurrence of edge_id is done -- see the
             * pin_count comment on edge_state_entry. Once this
             * reaches 0 and used_in_path is clear, the entry
             * becomes eligible for eviction.
             */
            Assert(EDGE_STATE_ENTRY_PIN_COUNT(ese) > 0);
            EDGE_STATE_ENTRY_PIN_COUNT_DEC(ese);
            /*
             * Remove its source vertex, if we are looking at edges as
             * un-directional. We only maintain the vertex stack when the
             * edge_direction is CYPHER_REL_DIR_NONE. This is to save space
             * and time.
             */
            if (vlelctx->edge_direction == CYPHER_REL_DIR_NONE)
            {
                gid_stack_pop(vertex_stack);
            }
            Assert(vlelctx->edge_direction != CYPHER_REL_DIR_NONE ||
                   gid_stack_size(vertex_stack) == gid_stack_size(edge_stack));
            /* move to the next edge */
            continue;
        }

        /*
         * Mark it and push it on the path stack. There is no need to push it on
         * the edge stack as it is already there.
         */
        EDGE_STATE_ENTRY_SET_USE_IN_PATH(ese);
        gid_stack_push(path_stack, edge_id);

        /*
         * Resolve the next vertex directly from the cache populated by
         * get_or_build_vertex_edge_cache() when this edge was first
         * classified. No edge_entry lookup needed here -- that lookup
         * already happened once, ever, per edge, not once per DFS step.
         */
        next_vertex_id = get_next_vertex_from_state(vlelctx, ese);

        /*
         * Is this the end of a path that meets our requirements? Is its length
         * within the bounds specified?
         */
        if (next_vertex_id == end_vertex_id &&
            gid_stack_size(path_stack) >= vlelctx->lidx &&
            (vlelctx->uidx_infinite ||
             gid_stack_size(path_stack) <= vlelctx->uidx))
        {
            /* we found one */
            found = true;
        }
        /*
         * If we have found the end vertex but, we are not within our upper
         * bounds, we need to back up. We still need to continue traversing
         * the graph if we aren't within our lower bounds, though.
         */
        if (next_vertex_id == end_vertex_id &&
            !vlelctx->uidx_infinite &&
            gid_stack_size(path_stack) > vlelctx->uidx)
        {
            continue;
        }

        /* add in the edges for the next vertex if we won't exceed the bounds */
        if (vlelctx->uidx_infinite ||
            gid_stack_size(path_stack) < vlelctx->uidx)
        {
            add_valid_vertex_edges(vlelctx, next_vertex_id);
        }

        if (found)
        {
            return true;
        }
    }

    return false;
}

/*
 * Helper function to find one path FROM a start vertex.
 *
 * Note: On the very first entry into this function, the starting vertex's edges
 * should have already been loaded into the edge stack (this should have been
 * done by the SRF initialization phase).
 *
 * This function will always return on either a valid path found (true) or none
 * found (false). If one is found, the position (vertex & edge) will still be in
 * the stack. Each successive invocation within the SRF will then look for the
 * next available path until there aren't any left.
 */
static bool dfs_find_a_path_from(VLE_local_context *vlelctx)
{
    GraphIdStack *vertex_stack = NULL;
    GraphIdStack *edge_stack = NULL;
    GraphIdStack *path_stack = NULL;

    Assert(vlelctx != NULL);

    /* for ease of reading */
    vertex_stack = vlelctx->dfs_vertex_stack;
    edge_stack = vlelctx->dfs_edge_stack;
    path_stack = vlelctx->dfs_path_stack;

    /* while we have edges to process */
    while (!(gid_stack_is_empty(edge_stack)))
    {
        graphid edge_id;
        graphid next_vertex_id;
        edge_state_entry *ese = NULL;
        bool found = false;
        uint32 edge_hashvalue;

        /*
         * Allow this traversal to be cancelled (e.g. by a user Ctrl-C or a
         * statement_timeout). On a large or densely connected graph this DFS
         * can run for a long time, so we must yield to interrupt processing
         * on every iteration.
         */
        CHECK_FOR_INTERRUPTS();

        /* get an edge, but leave it on the stack for now */
        edge_id = gid_stack_peek(edge_stack);
        /*
         * Compute the hash for edge_id once and reuse it for the
         * edge_state_hashtable lookup.
         */
        edge_hashvalue = graphid_hash(&edge_id, sizeof(int64));
        /* get the edge's state */
        ese = get_edge_state_with_hash(vlelctx, edge_id, edge_hashvalue);
        /*
         * If the edge is already in use, it means that the edge is in the path.
         * So, we need to see if it is the last path entry (we are backing up -
         * we need to remove the edge from the path stack and reset its state
         * and from the edge stack as we are done with it) or an interior edge
         * in the path (loop - we need to remove the edge from the edge stack
         * and start with the next edge).
         */
        if (EDGE_STATE_ENTRY_USE_IN_PATH(ese))
        {
            graphid path_edge_id;

            /* get the edge id on the top of the path stack (last edge) */
            path_edge_id = gid_stack_peek(path_stack);
            /*
             * If the ids are the same, we're backing up. So, remove it from the
             * path stack and reset used_in_path.
             */
            if (edge_id == path_edge_id)
            {
                gid_stack_pop(path_stack);
                EDGE_STATE_ENTRY_UNSET_USE_IN_PATH(ese);
            }
            /* now remove it from the edge stack */
            gid_stack_pop(edge_stack);
            /*
             * This occurrence of edge_id is done -- see the
             * pin_count comment on edge_state_entry. Once this
             * reaches 0 and used_in_path is clear, the entry
             * becomes eligible for eviction.
             */
            Assert(EDGE_STATE_ENTRY_PIN_COUNT(ese) > 0);
            EDGE_STATE_ENTRY_PIN_COUNT_DEC(ese);
            /*
             * Remove its source vertex, if we are looking at edges as
             * un-directional. We only maintain the vertex stack when the
             * edge_direction is CYPHER_REL_DIR_NONE. This is to save space
             * and time.
             */
            if (vlelctx->edge_direction == CYPHER_REL_DIR_NONE)
            {
                gid_stack_pop(vertex_stack);
            }
            Assert(vlelctx->edge_direction != CYPHER_REL_DIR_NONE ||
                   gid_stack_size(vertex_stack) == gid_stack_size(edge_stack));
            /* move to the next edge */
            continue;
        }

        /*
         * Mark it and push it on the path stack. There is no need to push it on
         * the edge stack as it is already there.
         */
        EDGE_STATE_ENTRY_SET_USE_IN_PATH(ese);
        gid_stack_push(path_stack, edge_id);

        /*
         * Resolve the next vertex directly from the cache populated by
         * get_or_build_vertex_edge_cache() when this edge was first
         * classified. No edge_entry lookup needed here.
         */
        next_vertex_id = get_next_vertex_from_state(vlelctx, ese);

        /*
         * Is this a path that meets our requirements? Is its length within the
         * bounds specified?
         */
        if (gid_stack_size(path_stack) >= vlelctx->lidx &&
            (vlelctx->uidx_infinite ||
             gid_stack_size(path_stack) <= vlelctx->uidx))
        {
            /* we found one */
            found = true;
        }

        /* add in the edges for the next vertex if we won't exceed the bounds */
        if (vlelctx->uidx_infinite ||
            gid_stack_size(path_stack) < vlelctx->uidx)
        {
            add_valid_vertex_edges(vlelctx, next_vertex_id);
        }

        if (found)
        {
            return true;
        }
    }

    return false;
}

/*
 * Helper routine to quickly check if an edge_id is in the path stack. It is
 * only meant as a quick check to avoid doing a much more costly hash search for
 * smaller sized lists. But, it is O(n) so it should only be used for small
 * path_stacks and where appropriate.
 */
static bool is_edge_in_path(VLE_local_context *vlelctx, graphid edge_id)
{
    GraphIdStack *stack = vlelctx->dfs_path_stack;
    int64 i;

    /* scan the array-based path stack */
    for (i = 0; i < gid_stack_size(stack); i++)
    {
        if (gid_stack_get(stack, i) == edge_id)
        {
            return true;
        }
    }
    /* we didn't find it if we get here */
    return false;
}

/*
 * Batched candidate buffer size for the adjacency lookup pipeline below.
 * 8 was chosen because it comfortably fits within the OoO window and the
 * per-core L1 MSHR count of modern Xeons (12+), so the K back-to-back
 * hashtable misses overlap in a single MLP wave.
 */
#define VLE_LOOKUP_BATCH 8

/*
 * Build (on first visit) or fetch (on every subsequent visit) the static
 * classification cache for `vertex_id`: the subset of its adjacent edges
 * that pass is_an_edge_match(), independent of anything path-dependent.
 *
 * This is the "vertex-level adjacency cache" optimization. It is deliberately
 * split apart from the DYNAMIC used_in_path/is_edge_in_path check, which
 * changes constantly as the DFS advances and backtracks and can therefore
 * never be cached at the vertex level -- only the *static* match result can.
 *
 * On the very first visit to a vertex, this runs the same 5-phase MLP-batched
 * pipeline that add_valid_vertex_edges used to run on *every* visit: gather,
 * hash, look up edge_entry (agehash), look up/create edge_state_entry
 * (dynahash), classify. On every subsequent visit, this is a single dynahash
 * lookup that returns the already-built array -- the adjacency arrays are
 * never re-walked, and get_edge_entry_with_hash() (the expensive, L3-miss
 * prone lookup) is never called again for any edge already classified,
 * whether it was classified from this vertex or from its other endpoint.
 *
 * Note that because this cache lives in vlelctx and, for a grammar-node
 * backed VLE call (the normal Cypher path -- see build_local_vle_context()'s
 * use_cache handling), vlelctx itself is reused across many separate
 * age_vle() SRF invocations (e.g. once per outer-loop row feeding into a
 * VLE join), this cache's benefit compounds far beyond a single path
 * enumeration: a vertex visited by row 1's traversal is already fully
 * classified, for free, if row 2's traversal ever reaches it too. This
 * also means the cache can grow large over the lifetime of a long-running
 * backend, which is exactly why vertex_edge_cache_mcxt's lifetime has to be
 * managed explicitly rather than left to an ambient context -- see
 * create_VLE_local_state_hashtable() and free_VLE_local_context().
 *
 * The returned array and its length are only ever read by the caller
 * (add_valid_vertex_edges) below; they are never mutated after this
 * function returns.
 */
static vertex_edge_cache_entry *get_or_build_vertex_edge_cache(
                                                    VLE_local_context *vlelctx,
                                                    graphid vertex_id)
{
    vertex_edge_cache_entry *vce = NULL;
    bool found = false;
    vertex_entry *ve = NULL;
    graphid *arr_out = NULL;
    int32    sz_out = 0;
    int32    idx_out = 0;
    graphid *arr_in = NULL;
    int32    sz_in = 0;
    int32    idx_in = 0;
    graphid *arr_self = NULL;
    int32    sz_self = 0;
    int32    idx_self = 0;
    VertexEdgeArray *vea = NULL;
    graphid  *scratch = NULL;
    int32     nscratch = 0;
    int32     scratch_cap;
    MemoryContext oldcontext;

    /*
     * Keep the cache within its cap BEFORE inserting a new key, not
     * after -- evicting after insertion would risk immediately evicting
     * the entry we are about to build. This is a pure performance cache
     * with no pinning constraints (see the clock_ref field comment on
     * vertex_edge_cache_entry), so eviction here is always safe.
     */
    evict_vertex_edge_cache_entries_if_needed(vlelctx);

    vce = (vertex_edge_cache_entry *) hash_search(vlelctx->vertex_edge_cache,
                                                  &vertex_id, HASH_ENTER,
                                                  &found);
    if (found)
    {
        vce->clock_ref = true;
        return vce;
    }

    /* get the vertex entry */
    ve = get_vertex_entry(vlelctx->ggctx, vertex_id);
    /* there better be a valid vertex */
    if (ve == NULL)
    {
        elog(ERROR, "get_or_build_vertex_edge_cache: no vertex found");
    }

    /* set up walked arrays for the requested direction(s) */
    if (vlelctx->edge_direction == CYPHER_REL_DIR_RIGHT ||
        vlelctx->edge_direction == CYPHER_REL_DIR_NONE)
    {
        vea = get_vertex_entry_edges_out_array(ve);
        arr_out = vea->array;
        sz_out  = vea->size;
    }
    if (vlelctx->edge_direction == CYPHER_REL_DIR_LEFT ||
        vlelctx->edge_direction == CYPHER_REL_DIR_NONE)
    {
        vea = get_vertex_entry_edges_in_array(ve);
        arr_in = vea->array;
        sz_in  = vea->size;
    }
    /* selfloops are always traversed */
    vea = get_vertex_entry_edges_self_array(ve);
    arr_self = vea->array;
    sz_self  = vea->size;

    scratch_cap = sz_out + sz_in + sz_self;

    /*
     * The result array must outlive this SRF call (it is read on every
     * future visit to this vertex, across many successive age_vle()
     * invocations), so it must be allocated in
     * vlelctx->vertex_edge_cache_mcxt, not whatever the ambient
     * CurrentMemoryContext happens to be here.
     */
    oldcontext = MemoryContextSwitchTo(vlelctx->vertex_edge_cache_mcxt);
    scratch = (scratch_cap > 0) ? palloc(sizeof(graphid) * scratch_cap) : NULL;
    MemoryContextSwitchTo(oldcontext);

    /*
     * Walks each adjacency array exactly once for the lifetime of this
     * VLE_local_context (not once per visit to vertex_id), in batches
     * pipelined across five phases -- gather, hash, look up any existing
     * classification, look up the underlying edge for anything not yet
     * classified, then classify and collect -- so that the latency of
     * independent hash/heap lookups within a batch can overlap. The
     * dynamic used_in_path / is_edge_in_path check is deliberately NOT
     * done here -- see add_valid_vertex_edges() below.
     */
    while (idx_out < sz_out || idx_in < sz_in || idx_self < sz_self)
    {
        graphid           batch_eids[VLE_LOOKUP_BATCH];
        uint32            batch_hashes[VLE_LOOKUP_BATCH];
        edge_entry       *batch_ee[VLE_LOOKUP_BATCH];
        edge_state_entry *batch_ese[VLE_LOOKUP_BATCH];
        int batch_n = 0;
        int i;

        /* Phase 1: gather (no is_edge_in_path check -- that's dynamic) */
        while (batch_n < VLE_LOOKUP_BATCH &&
               (idx_out < sz_out || idx_in < sz_in || idx_self < sz_self))
        {
            if (idx_out < sz_out)
            {
                batch_eids[batch_n++] = arr_out[idx_out++];
            }
            else if (idx_in < sz_in)
            {
                batch_eids[batch_n++] = arr_in[idx_in++];
            }
            else
            {
                batch_eids[batch_n++] = arr_self[idx_self++];
            }
        }

        /* Phase 2: compute hashes (pure compute, no misses) */
        for (i = 0; i < batch_n; i++)
        {
            batch_hashes[i] = graphid_hash(&batch_eids[i], sizeof(int64));
        }

        /*
         * Phase 3: K back-to-back edge_state_hashtable FIND-only lookups
         * (MLP wave 1), checked before touching edge_table at all. A hit
         * means this edge was already classified as a match earlier
         * (only possible for CYPHER_REL_DIR_NONE, or overlap with the
         * PATHS_BETWEEN reverse-BFS walk in rdist_expand_vertex() -- see
         * that function). edge_state_hashtable only ever holds matched
         * edges (see the struct comment on edge_state_entry), so a miss
         * here always means "not yet classified", never "classified and
         * rejected".
         */
        for (i = 0; i < batch_n; i++)
        {
            batch_ese[i] = find_edge_state_with_hash(vlelctx, batch_eids[i],
                                                      batch_hashes[i]);
        }

        /*
         * Phase 4: K back-to-back edge_table (agehash) lookups (MLP wave
         * 2), but ONLY for edges Phase 3 didn't already resolve as a
         * known match. This avoids the edge_entry lookup entirely (not
         * just the property heap_fetch inside is_an_edge_match()) on the
         * common repeat-visit path.
         */
        for (i = 0; i < batch_n; i++)
        {
            batch_ee[i] = (batch_ese[i] == NULL)
                            ? get_edge_entry_with_hash(vlelctx->ggctx,
                                                       batch_eids[i],
                                                       batch_hashes[i])
                            : NULL;
        }

        /* Phase 5: classify newly-seen edges and collect into scratch[] */
        for (i = 0; i < batch_n; i++)
        {
            edge_state_entry *ese = batch_ese[i];

            if (ese == NULL)
            {
                edge_entry *ee = batch_ee[i];

                if (ee == NULL)
                {
                    elog(ERROR, "get_or_build_vertex_edge_cache: no edge found");
                }

                if (!is_an_edge_match(vlelctx, ee))
                {
                    /*
                     * Not a match: no edge_state_entry is created for
                     * it. edge_state_hashtable's size is thus bounded by
                     * the number of matched edges ever seen, not by
                     * every edge ever scanned -- the dominant factor on
                     * a low-selectivity VLE predicate over a dense or
                     * hub-heavy graph. The cost is that a rejected edge
                     * encountered a second time (undirected traversal,
                     * or overlap with the PATHS_BETWEEN reverse-BFS
                     * walk) is re-classified rather than remembered.
                     */
                    continue;
                }

                ese = insert_matched_edge_state(vlelctx, batch_eids[i],
                                                batch_hashes[i], ee);
            }

            scratch[nscratch++] = batch_eids[i];
        }
    }

    evict_edge_state_entries_if_needed(vlelctx);

    /*
     * Shrink to the actual number of statically-valid edges. scratch_cap is
     * the raw (unfiltered) adjacency count; on a dense graph with a
     * selective edge predicate nscratch can be far smaller, and this array
     * lives for the remaining lifetime of vlelctx (potentially the lifetime
     * of a long-running, cached, reused-across-many-SRF-calls backend --
     * see the note at the top of this function), so the gap matters.
     */
    if (scratch != NULL)
    {
        oldcontext = MemoryContextSwitchTo(vlelctx->vertex_edge_cache_mcxt);
        if (nscratch > 0 && nscratch < scratch_cap)
        {
            scratch = repalloc(scratch, sizeof(graphid) * nscratch);
        }
        else if (nscratch == 0)
        {
            pfree(scratch);
            scratch = NULL;
        }
        MemoryContextSwitchTo(oldcontext);
    }

    vce->vertex_id = vertex_id;
    vce->nvalid = nscratch;
    vce->valid_edges = scratch;
    vce->clock_ref = true;

    return vce;
}

/*
 * Clock-style eviction for vertex_edge_cache, triggered once the table
 * exceeds age.vle_vertex_edge_cache_max_entries, or the memory actually
 * backing its cached adjacency arrays exceeds age.vle_vertex_edge_cache_max_kb
 * -- an entry-count cap alone under-protects against a small number of
 * high-degree (hub) vertices, each with a disproportionately large
 * valid_edges[] array.
 *
 * No pinning is needed here (unlike edge_state_hashtable): the
 * vertex_edge_cache_entry pointer returned by get_or_build_vertex_edge_cache()
 * is only ever used synchronously, within a single call to
 * add_valid_vertex_edges(), and never retained past it -- so any entry can
 * be evicted (and its valid_edges[] array freed) at any time between
 * calls. A revisited vertex simply gets its cache rebuilt from the
 * underlying adjacency arrays, which is deterministic and correct, just
 * not free -- exactly the ordinary LRU-cache-miss cost/memory trade-off.
 *
 * See evict_edge_state_entries_if_needed() for why eviction targets a
 * low-water mark below the cap, and why two passes are both necessary and
 * sufficient for a clock sweep.
 */
static void evict_vertex_edge_cache_entries_if_needed(VLE_local_context *vlelctx)
{
    long   low_watermark;
    Size   max_bytes = (Size) vle_vertex_edge_cache_max_kb * 1024;
    int    pass;

    if (hash_get_num_entries(vlelctx->vertex_edge_cache) <=
            vle_vertex_edge_cache_max_entries &&
        MemoryContextMemAllocated(vlelctx->vertex_edge_cache_mcxt, false) <=
            max_bytes)
    {
        return;
    }

    low_watermark = vle_vertex_edge_cache_max_entries -
                        (vle_vertex_edge_cache_max_entries / 8);

    for (pass = 0;
         pass < 2 &&
             (hash_get_num_entries(vlelctx->vertex_edge_cache) >
                  low_watermark ||
              MemoryContextMemAllocated(vlelctx->vertex_edge_cache_mcxt,
                                        false) > max_bytes - (max_bytes / 8));
         pass++)
    {
        HASH_SEQ_STATUS status;
        vertex_edge_cache_entry *vce;

        hash_seq_init(&status, vlelctx->vertex_edge_cache);
        while ((vce = (vertex_edge_cache_entry *) hash_seq_search(&status)) != NULL)
        {
            if (vce->clock_ref)
            {
                vce->clock_ref = false;
                continue;
            }

            if (vce->valid_edges != NULL)
            {
                pfree(vce->valid_edges);
            }

            (void) hash_search(vlelctx->vertex_edge_cache, &vce->vertex_id,
                               HASH_REMOVE, NULL);
        }
    }
}

/* ------------------------------------------------------------------------
 * Reverse-BFS-from-veid pruning, for VLE_FUNCTION_PATHS_BETWEEN only.
 *
 * Only PATHS_BETWEEN has a concrete target vertex, so it's the only mode
 * where "will this branch ever reach the target within the remaining hop
 * budget" is even a meaningful question. Nothing below is invoked from
 * dfs_find_a_path_from()'s path -- see the path_function guard in
 * add_valid_vertex_edges().
 *
 * Correctness rests on one fact: let d(v) be the shortest-path distance
 * from v to veid over the FULL matched-edge-filtered graph, ignoring which
 * edges the current DFS path has already used. Then d(v) is always <= the
 * true remaining distance once already-used edges are excluded (removing
 * edges can only lengthen or preserve a shortest path, never shorten it).
 * So "depth_so_far + 1 + d(next) > uidx" is a SAFE pruning condition -- it
 * never discards a real valid path -- even though it is not a complete
 * dead-end detector (a path can still turn out to be blocked later by an
 * edge already in use; the ordinary used_in_path check catches that case
 * the normal way, just possibly a little later). This is exactly why it
 * would be UNSOUND to instead memoize "v is a dead end" keyed on which
 * edges are currently used_in_path: that quantity is path-state-dependent
 * and cannot be safely reused across different branches of the same DFS.
 * d(v) as defined here has no such dependency, which is what makes
 * memoizing it permanently, and reusing it anywhere in the traversal, safe.
 *
 * IMPORTANT INVARIANT: none of these functions return a raw
 * reverse_dist_entry pointer to their caller.  set_reverse_dist() and
 * try_get_reverse_dist() copy the scalar distance in or out and keep
 * the entry pointer local.  This keeps the code safe if the hashtable
 * implementation ever moves entries on insert or growth.
 * ------------------------------------------------------------------------
 */

/*
 * "Reverse BFS from veid under direction D" == "forward BFS from veid under
 * flip(D)". CYPHER_REL_DIR_LEFT's forward step already walks a vertex's
 * in-array and lands on the edge's start -- i.e. it already walks backwards
 * along the edge -- which is exactly the predecessor-step reverse-BFS needs
 * for a RIGHT-oriented query, and symmetrically the other way around.
 * CYPHER_REL_DIR_NONE is self-symmetric (undirected).
 */
static cypher_rel_dir flip_edge_direction(cypher_rel_dir dir)
{
    switch (dir)
    {
        case CYPHER_REL_DIR_RIGHT:
            return CYPHER_REL_DIR_LEFT;
        case CYPHER_REL_DIR_LEFT:
            return CYPHER_REL_DIR_RIGHT;
        case CYPHER_REL_DIR_NONE:
        default:
            return CYPHER_REL_DIR_NONE;
    }
}

/*
 * Minimum size for the reverse-BFS queue.  The queue is not shrunk
 * below this value, to avoid reallocating for small targets.
 */
#define RDIST_QUEUE_MIN_CAP 256

/* Initial size for the reverse-distance hash table. */
#define RDIST_TABLE_MIN_SIZE 256

static void rdist_queue_grow(VLE_local_context *vlelctx, rdist_queue *q)
{
    MemoryContext oldcontext;
    int64 oldcap = q->cap;
    int64 newcap;
    graphid *newdata;

    Assert(q->cap == 0 || q->count == q->cap);

    newcap = (oldcap == 0) ? RDIST_QUEUE_MIN_CAP : oldcap * 2;

    oldcontext = MemoryContextSwitchTo(vlelctx->reverse_dist_mcxt);
    newdata = (graphid *) palloc(sizeof(graphid) * newcap);

    if (q->data != NULL && q->count > 0)
    {
        int64 i;

        /*
         * Copy active elements in FIFO order. This happens only on grow,
         * not on ordinary push/pop.
         */
        for (i = 0; i < q->count; i++)
        {
            newdata[i] = q->data[(q->head + i) & (oldcap - 1)];
        }
    }

    if (q->data != NULL)
    {
        pfree(q->data);
    }

    MemoryContextSwitchTo(oldcontext);

    q->data = newdata;
    q->cap = newcap;
    q->head = 0;
    q->tail = q->count;
}

static void rdist_queue_push(VLE_local_context *vlelctx, rdist_queue *q,
                             graphid v)
{
    if (q->cap == 0 || q->count == q->cap)
    {
        rdist_queue_grow(vlelctx, q);
    }

    q->data[q->tail] = v;
    q->tail = (q->tail + 1) & (q->cap - 1);
    q->count++;

    if (q->count > q->max_count)
    {
        q->max_count = q->count;
    }
}

static bool rdist_queue_is_empty(rdist_queue *q)
{
    return q->count == 0;
}

static graphid rdist_queue_pop(rdist_queue *q)
{
    graphid v;

    Assert(q->count > 0);

    v = q->data[q->head];
    q->head = (q->head + 1) & (q->cap - 1);
    q->count--;

    return v;
}

/*
 * Rewind to an empty queue for a new generation (new veid). If the
 * PREVIOUS generation used far less of the array than its current capacity
 * -- e.g. one huge query on a dense graph, followed on a cached vlelctx by
 * many small ones on unrelated later queries -- shrink the backing array
 * back down instead of holding the peak-ever size for the rest of this
 * vlelctx's lifetime. Hysteresis (4x threshold + a floor) avoids
 * reallocating on every single reset for queries of similar size run
 * back-to-back.
 */
static void rdist_queue_reset(VLE_local_context *vlelctx, rdist_queue *q)
{
    int64 peak = q->max_count;

    /*
     * Shrink if the previous generation's peak frontier was much smaller
     * than the current capacity. Keep a minimum size to avoid reallocating
     * for small/typical queries.
     */
    if (q->data != NULL && q->cap > RDIST_QUEUE_MIN_CAP &&
        peak < q->cap / 4)
    {
        MemoryContext oldcontext;
        int64 newcap = RDIST_QUEUE_MIN_CAP;
        int64 want = peak * 2;

        while (newcap < want)
        {
            newcap <<= 1;
        }

        oldcontext = MemoryContextSwitchTo(vlelctx->reverse_dist_mcxt);
        pfree(q->data);
        q->data = (graphid *) palloc(sizeof(graphid) * newcap);
        MemoryContextSwitchTo(oldcontext);

        q->cap = newcap;
    }

    q->head = 0;
    q->tail = 0;
    q->count = 0;
    q->max_count = 0;
}

/*
 * Store dist as vertex_id's reverse distance. Never returns the entry
 * pointer to the caller -- see the invariant in the block comment above.
 */
static void set_reverse_dist(VLE_local_context *vlelctx, graphid vertex_id,
                             int64 dist)
{
    reverse_dist_entry *rde;
    bool found;

    rde = (reverse_dist_entry *) hash_search(vlelctx->reverse_dist_table,
                                             &vertex_id, HASH_ENTER, &found);
    rde->vertex_id = vertex_id;
    rde->dist = dist;
}

/*
 * Read-only: true and *dist set iff vertex_id's reverse distance is
 * already known for the CURRENT target. A vertex the reverse BFS has never
 * reached simply isn't known, which is a very different thing from "known
 * to be unreachable" -- see reverse_dist_exhausted in
 * get_or_advance_reverse_dist(). Never returns the entry pointer to the
 * caller -- see the invariant in the block comment above.
 */
static bool try_get_reverse_dist(VLE_local_context *vlelctx, graphid vertex_id,
                                 int64 *dist)
{
    reverse_dist_entry *rde;
    bool found;

    rde = (reverse_dist_entry *) hash_search(vlelctx->reverse_dist_table,
                                             &vertex_id, HASH_FIND, &found);
    if (found)
    {
        *dist = rde->dist;
        return true;
    }
    return false;
}

/*
 * Rebuild reverse_dist_table and the BFS queue if the target vertex
 * changed.  Other path functions have no fixed target, so they do not
 * use this state.
 */
static void reset_reverse_dist_state_if_needed(VLE_local_context *vlelctx)
{
    if (vlelctx->path_function != VLE_FUNCTION_PATHS_BETWEEN)
    {
        return;
    }

    if (vlelctx->reverse_dist_initialized &&
        vlelctx->reverse_dist_target == vlelctx->veid)
    {
        return;
    }

    if (vlelctx->reverse_dist_table != NULL)
    {
        hash_destroy(vlelctx->reverse_dist_table);
    }
    {
        HASHCTL ctl;
        MemoryContext oldcontext;

        oldcontext = MemoryContextSwitchTo(vlelctx->reverse_dist_mcxt);
        MemSet(&ctl, 0, sizeof(ctl));
        ctl.keysize = sizeof(int64);
        ctl.entrysize = sizeof(reverse_dist_entry);
        ctl.hash = graphid_hash;
        vlelctx->reverse_dist_table = hash_create("VLE reverse distance",
                                                  RDIST_TABLE_MIN_SIZE, &ctl,
                                                  HASH_ELEM | HASH_FUNCTION);
        MemoryContextSwitchTo(oldcontext);
    }

    vlelctx->reverse_dist_initialized = true;
    vlelctx->reverse_dist_target = vlelctx->veid;
    vlelctx->reverse_dist_exhausted = false;
    vlelctx->reverse_dist_capped = false;
    rdist_queue_reset(vlelctx, &vlelctx->reverse_dist_queue);
    rdist_queue_push(vlelctx, &vlelctx->reverse_dist_queue, vlelctx->veid);
    set_reverse_dist(vlelctx, vlelctx->veid, 0);
}

/*
 * Expand frontier vertex u (currently known to be at reverse-distance du
 * from veid): walk u's flipped-direction adjacency, classify each edge
 * through the SAME edge_state_hashtable that get_or_build_vertex_edge_cache()
 * and add_valid_vertex_edges() use (an edge classified by either the
 * forward vertex_edge_cache build or this reverse walk is never
 * reclassified by the other -- is_an_edge_match()'s result depends only on
 * the edge itself, never on which side or direction discovered it first),
 * and push any newly-discovered predecessor at distance du+1.
 *
 * Deliberately does NOT go through get_or_build_vertex_edge_cache() -- that
 * would classify u's FORWARD-direction adjacency (wrong direction for this
 * walk) and would force the expensive classification work for vertices
 * that may turn out to be pruned and never actually visited by the forward
 * DFS. This function does its own adjacency walk, in the flipped direction.
 */
static void rdist_expand_vertex(VLE_local_context *vlelctx, graphid u,
                                int64 du, cypher_rel_dir flipped_dir)
{
    vertex_entry *ve = NULL;
    graphid *arr_out = NULL;
    int32    sz_out = 0;
    int32    idx_out = 0;
    graphid *arr_in = NULL;
    int32    sz_in = 0;
    int32    idx_in = 0;
    graphid *arr_self = NULL;
    int32    sz_self = 0;
    int32    idx_self = 0;
    VertexEdgeArray *vea = NULL;

    ve = get_vertex_entry(vlelctx->ggctx, u);
    if (ve == NULL)
    {
        /* defensive only -- u was reached via a real, already-classified edge */
        return;
    }

    if (flipped_dir == CYPHER_REL_DIR_RIGHT || flipped_dir == CYPHER_REL_DIR_NONE)
    {
        vea = get_vertex_entry_edges_out_array(ve);
        arr_out = vea->array;
        sz_out  = vea->size;
    }
    if (flipped_dir == CYPHER_REL_DIR_LEFT || flipped_dir == CYPHER_REL_DIR_NONE)
    {
        vea = get_vertex_entry_edges_in_array(ve);
        arr_in = vea->array;
        sz_in  = vea->size;
    }
    vea = get_vertex_entry_edges_self_array(ve);
    arr_self = vea->array;
    sz_self  = vea->size;

    while (idx_out < sz_out || idx_in < sz_in || idx_self < sz_self)
    {
        graphid           batch_eids[VLE_LOOKUP_BATCH];
        uint32            batch_hashes[VLE_LOOKUP_BATCH];
        edge_entry       *batch_ee[VLE_LOOKUP_BATCH];
        edge_state_entry *batch_ese[VLE_LOOKUP_BATCH];
        int batch_n = 0;
        int i;

        while (batch_n < VLE_LOOKUP_BATCH &&
               (idx_out < sz_out || idx_in < sz_in || idx_self < sz_self))
        {
            if (idx_out < sz_out)
            {
                batch_eids[batch_n++] = arr_out[idx_out++];
            }
            else if (idx_in < sz_in)
            {
                batch_eids[batch_n++] = arr_in[idx_in++];
            }
            else
            {
                batch_eids[batch_n++] = arr_self[idx_self++];
            }
        }

        for (i = 0; i < batch_n; i++)
        {
            batch_hashes[i] = graphid_hash(&batch_eids[i], sizeof(int64));
        }

        /* Phase: FIND-only edge_state lookup first -- see the identical
         * pattern (and rationale) in get_or_build_vertex_edge_cache(). */
        for (i = 0; i < batch_n; i++)
        {
            batch_ese[i] = find_edge_state_with_hash(vlelctx, batch_eids[i],
                                                      batch_hashes[i]);
        }

        for (i = 0; i < batch_n; i++)
        {
            batch_ee[i] = (batch_ese[i] == NULL)
                            ? get_edge_entry_with_hash(vlelctx->ggctx,
                                                       batch_eids[i],
                                                       batch_hashes[i])
                            : NULL;
        }

        for (i = 0; i < batch_n; i++)
        {
            edge_state_entry *ese = batch_ese[i];
            graphid p;
            int64 unused_dist;
            int64 new_dist;

            if (ese == NULL)
            {
                edge_entry *ee = batch_ee[i];

                if (ee == NULL)
                {
                    elog(ERROR, "rdist_expand_vertex: no edge found");
                }

                if (!is_an_edge_match(vlelctx, ee))
                {
                    continue;
                }

                ese = insert_matched_edge_state(vlelctx, batch_eids[i],
                                                batch_hashes[i], ee);
            }

            switch (flipped_dir)
            {
                case CYPHER_REL_DIR_RIGHT:
                    p = ese->end_vertex_id;
                    break;
                case CYPHER_REL_DIR_LEFT:
                    p = ese->start_vertex_id;
                    break;
                case CYPHER_REL_DIR_NONE:
                default:
                    p = (ese->start_vertex_id == u) ? ese->end_vertex_id
                                                    : ese->start_vertex_id;
                    break;
            }

            /* already known (discovered earlier, at this-or-smaller depth) */
            if (try_get_reverse_dist(vlelctx, p, &unused_dist))
            {
                continue;
            }

            new_dist = du + 1;

            /*
             * A reverse distance of exactly uidx or more can never help
             * add_valid_vertex_edges() prune a candidate: that check is
             *   depth_so_far + 1 + dnext > uidx
             * and for dnext == uidx this reduces to depth_so_far + 1 > 0,
             * which holds for every depth_so_far >= 0. Such a distance
             * therefore behaves exactly like the "unreachable"
             * PG_INT64_MAX sentinel for pruning purposes, and is not worth
             * a reverse_dist_table entry -- typically the single widest
             * layer of the reverse BFS. Only applies when uidx is finite.
             */
            if (!vlelctx->uidx_infinite && new_dist >= vlelctx->uidx)
            {
                continue;
            }

            /*
             * The reverse-BFS distance table is a pruning heuristic, not
             * a source of truth: an entry missing from it is read by
             * get_or_advance_reverse_dist() as "prune this candidate",
             * so it must never be capped by simply refusing to insert
             * without also recording that the search stopped early.
             * age.vle_reverse_dist_max_entries bounds it by degrading the
             * reverse BFS to a fail-open state instead: once reached, no
             * further frontier vertices are admitted, and unresolved
             * vertices are treated by get_or_advance_reverse_dist() as
             * "not provably prunable" rather than "unreachable" -- see
             * reverse_dist_capped's field comment.
             */
            if (hash_get_num_entries(vlelctx->reverse_dist_table) >=
                vle_reverse_dist_max_entries)
            {
                vlelctx->reverse_dist_capped = true;
                continue;
            }

            set_reverse_dist(vlelctx, p, new_dist);
            rdist_queue_push(vlelctx, &vlelctx->reverse_dist_queue, p);
        }
    }

    evict_edge_state_entries_if_needed(vlelctx);
}

/*
 * Resolve w's distance from veid along the flipped-direction adjacency,
 * advancing the lazy reverse BFS only as far as necessary to answer this
 * specific query.
 *
 * The reverse BFS is a pruning heuristic for VLE_FUNCTION_PATHS_BETWEEN:
 * add_valid_vertex_edges() discards a candidate edge once it can prove
 * the remaining budget can no longer reach veid. Proving unreachability
 * requires having actually exhausted the search (reverse_dist_exhausted);
 * merely stopping early to respect age.vle_reverse_dist_max_entries
 * (reverse_dist_capped) proves nothing, so the two must not be treated
 * alike. Returning PG_INT64_MAX in the capped-but-not-exhausted case
 * would make a memory limit silently discard valid paths, so this
 * function instead fails open: a distance of 0 for an unresolved vertex
 * never triggers pruning by itself (see add_valid_vertex_edges()), it
 * just forfeits the pruning benefit for that particular candidate.
 */
static int64 get_or_advance_reverse_dist(VLE_local_context *vlelctx, graphid w)
{
    int64 dist;

    if (try_get_reverse_dist(vlelctx, w, &dist))
    {
        return dist;
    }

    if (vlelctx->reverse_dist_exhausted)
    {
        return PG_INT64_MAX;
    }

    if (vlelctx->reverse_dist_capped)
    {
        return 0;
    }

    while (!rdist_queue_is_empty(&vlelctx->reverse_dist_queue))
    {
        graphid u = rdist_queue_pop(&vlelctx->reverse_dist_queue);
        int64   du;
        cypher_rel_dir flipped_dir;

        if (!try_get_reverse_dist(vlelctx, u, &du))
        {
            elog(ERROR, "get_or_advance_reverse_dist: frontier vertex has no distance");
        }

        /*
         * BFS pop order is non-decreasing in depth. Every neighbour of u
         * would land at du + 1, and rdist_expand_vertex() already refuses
         * to record any distance >= uidx as useless for pruning -- so
         * once du + 1 >= uidx, expanding u (and everything still queued
         * behind it) cannot add anything, and there is no point walking
         * its adjacency at all.
         */
        if (!vlelctx->uidx_infinite && du + 1 >= vlelctx->uidx)
        {
            break;
        }

        flipped_dir = flip_edge_direction(vlelctx->edge_direction);
        rdist_expand_vertex(vlelctx, u, du, flipped_dir);

        if (try_get_reverse_dist(vlelctx, w, &dist))
        {
            return dist;
        }

        if (vlelctx->reverse_dist_capped)
        {
            /*
             * The cap was already reached (permanently, for this target
             * -- reverse_dist_table never shrinks below it once hit, see
             * rdist_expand_vertex()), so continuing to pop and expand the
             * rest of the queue would just repeat the same no-op cap
             * check for every remaining candidate.
             */
            return 0;
        }
    }

    vlelctx->reverse_dist_exhausted = true;
    return PG_INT64_MAX;
}

/*
 * Helper function to add in valid vertex edges as part of the dfs path
 * algorithm. What constitutes a valid edge is the following -
 *
 *     1) Edge matches the correct direction specified.
 *     2) Edge is not currently in the path.
 *     3) Edge matches minimum edge properties specified.
 *
 * Note: The vertex must exist.
 *
 * Static classification (which of this vertex's edges match at all) is
 * handled by get_or_build_vertex_edge_cache() above and computed at most
 * once per vertex. This function only re-checks the DYNAMIC used_in_path
 * state, which must be re-checked on every visit, but now only for the
 * pre-filtered set of statically-valid edges -- not the full adjacency
 * list -- and with no edge_entry (agehash) lookups at all on repeat visits.
 *
 * The dynamic edge_state_hashtable lookups below are still run through the
 * same batched gather/hash/lookup/apply pipeline as
 * get_or_build_vertex_edge_cache() uses, for the same MLP reason: even
 * though only one hashtable is involved now (not two), a repeat visit to a
 * hot, highly-connected vertex can still re-check dozens of edges, and
 * batching the dynahash HASH_ENTER calls hides their miss latency the same
 * way it did before this vertex's edges were split out of the per-visit
 * pipeline.
 */
static void add_valid_vertex_edges(VLE_local_context *vlelctx,
                                   graphid vertex_id)
{
    GraphIdStack *vertex_stack = vlelctx->dfs_vertex_stack;
    GraphIdStack *edge_stack = vlelctx->dfs_edge_stack;
    vertex_edge_cache_entry *vce;
    int32 pos = 0;

    vce = get_or_build_vertex_edge_cache(vlelctx, vertex_id);

    while (pos < vce->nvalid)
    {
        graphid           batch_eids[VLE_LOOKUP_BATCH];
        uint32            batch_hashes[VLE_LOOKUP_BATCH];
        edge_state_entry *batch_ese[VLE_LOOKUP_BATCH];
        int batch_n = 0;
        int i;

        /* Phase 1: gather, applying the dynamic is_edge_in_path early-skip */
        while (batch_n < VLE_LOOKUP_BATCH && pos < vce->nvalid)
        {
            graphid edge_id = vce->valid_edges[pos++];

            /*
             * Fast early-skip when the path stack is small: avoids an
             * edge_state_hashtable lookup for edges already on the path.
             * (Kept exactly as before -- for the common, small-stack case
             * this bounded linear scan beats a hashtable lookup.)
             */
            if (gid_stack_size(vlelctx->dfs_path_stack) < 10 &&
                is_edge_in_path(vlelctx, edge_id))
            {
                continue;
            }

            batch_eids[batch_n++] = edge_id;
        }

        if (batch_n == 0)
        {
            continue;
        }

        /* Phase 2: compute hashes (pure compute, no misses) */
        for (i = 0; i < batch_n; i++)
        {
            batch_hashes[i] = graphid_hash(&batch_eids[i], sizeof(int64));
        }

        /* Phase 3: K back-to-back edge_state_hashtable lookups (MLP wave) */
        for (i = 0; i < batch_n; i++)
        {
            batch_ese[i] = get_edge_state_with_hash(vlelctx,
                                                    batch_eids[i],
                                                    batch_hashes[i]);
        }

        /* Phase 4: apply, in the same order the edges were discovered in */
        for (i = 0; i < batch_n; i++)
        {
            edge_state_entry *ese = batch_ese[i];
            graphid           edge_id = batch_eids[i];

            /*
             * Don't add any edges that we have already seen because they
             * will cause a loop to form.
             */
            if (EDGE_STATE_ENTRY_USE_IN_PATH(ese))
            {
                continue;
            }

            /*
             * PATHS_BETWEEN only: prune this candidate if it provably
             * cannot reach veid within the remaining hop budget. Checked
             * here, after the cheaper is_edge_in_path/used_in_path checks
             * have already had a chance to skip the edge for free, and
             * using ese->start_vertex_id/end_vertex_id (already resolved,
             * no extra lookup) rather than a fresh edge_entry fetch.
             */
            if (vlelctx->path_function == VLE_FUNCTION_PATHS_BETWEEN)
            {
                graphid next_vertex_id;
                int64   dnext;

                switch (vlelctx->edge_direction)
                {
                    case CYPHER_REL_DIR_RIGHT:
                        next_vertex_id = ese->end_vertex_id;
                        break;
                    case CYPHER_REL_DIR_LEFT:
                        next_vertex_id = ese->start_vertex_id;
                        break;
                    case CYPHER_REL_DIR_NONE:
                    default:
                        next_vertex_id = (ese->start_vertex_id == vertex_id)
                                            ? ese->end_vertex_id
                                            : ese->start_vertex_id;
                        break;
                }

                dnext = get_or_advance_reverse_dist(vlelctx, next_vertex_id);

                /*
                 * dnext == PG_INT64_MAX is checked before the budget
                 * arithmetic, and short-circuits it via ||, specifically
                 * to avoid overflowing depth_so_far + 1 + dnext.
                 */
                if (dnext == PG_INT64_MAX ||
                    (!vlelctx->uidx_infinite &&
                     gid_stack_size(vlelctx->dfs_path_stack) + 1 + dnext >
                         vlelctx->uidx))
                {
                    continue;
                }
            }

            /*
             * We need to maintain our source vertex for each edge added if
             * the edge_direction is CYPHER_REL_DIR_NONE. This is due to the
             * edges having a fixed direction and the dfs algorithm working
             * strictly through edges. With an un-directional VLE edge, you
             * don't know the vertex that you just came from. So, we need to
             * store it.
             */
            if (vlelctx->edge_direction == CYPHER_REL_DIR_NONE)
            {
                gid_stack_push(vertex_stack, vertex_id);
            }
            gid_stack_push(edge_stack, edge_id);
            /*
             * This edge_id now has a live, unpopped occurrence on
             * dfs_edge_stack. pin_count protects its edge_state_entry
             * from eviction until every such occurrence is popped again
             * (see the struct comment on edge_state_entry) -- vertices
             * can be revisited, so the SAME edge_id can end up pushed
             * more than once while an older copy is still buried deeper
             * in the stack.
             */
            EDGE_STATE_ENTRY_PIN_COUNT_INC(ese);
            Assert(vlelctx->edge_direction != CYPHER_REL_DIR_NONE ||
                   gid_stack_size(vertex_stack) == gid_stack_size(edge_stack));
        }
    }
}

/*
 * Helper function to create the VLE path container that holds the graphid array
 * containing the found path. The path_size is the total number of vertices and
 * edges in the path.
 */
static VLE_path_container *create_VLE_path_container(int64 path_size)
{
    VLE_path_container *vpc = NULL;
    int container_size_bytes = 0;

    /*
     * For the total container size (in graphids int64s) we need to add the
     * following space (in graphids) to hold each of the following fields -
     *
     *     One for the VARHDRSZ which is a int32 and a pad of 32.
     *     One for both the header and graph oid (they are both 32 bits).
     *     One for the size of the graphid_array_size.
     *     One for the container_size_bytes.
     *     One for start_vid (Stage 1: inline endpoint cache).
     *     One for end_vid   (Stage 1: inline endpoint cache).
     *
     */
    container_size_bytes = sizeof(graphid) * (path_size + 6);

    /* allocate the container */
    vpc = palloc0(container_size_bytes);

    /* initialize the PG headers */
    SET_VARSIZE(vpc, container_size_bytes);

    /* initialize the container */
    vpc->header = AGT_FBINARY | AGT_FBINARY_TYPE_VLE_PATH;
    vpc->graphid_array_size = path_size;
    vpc->container_size_bytes = container_size_bytes;

    /* the graphid array is already zeroed out */
    /* all of the other fields are set by the caller */

    return vpc;
}

/*
 * Helper function to build a VLE_path_container containing the graphid array
 * from the path_stack. The graphid array will be a complete path (vertices and
 * edges interleaved) -
 *
 *     start vertex, first edge,... nth edge, end vertex
 *
 * The VLE_path_container is allocated in such a way as to wrap the array and
 * include the following additional data -
 *
 *     The header is to allow the graphid array to be encoded as an agtype
 *     container of type BINARY. This way the array doesn't need to be
 *     transformed back and forth.
 *
 *     The graph oid to facilitate the retrieval of the correct vertex and edge
 *     entries.
 *
 *     The total number of elements in the array.
 *
 *     The total size of the container for copying.
 *
 * Note: Remember to pfree it when done. Even though it should be destroyed on
 *       exiting the SRF context.
 */

static VLE_path_container *build_VLE_path_container(VLE_local_context *vlelctx)
{
    GraphIdStack *stack = vlelctx->dfs_path_stack;
    VLE_path_container *vpc = NULL;
    graphid *graphid_array = NULL;
    graphid vid = 0;
    int index = 0;
    int ssize = 0;
    int j = 0;

    if (stack == NULL)
    {
        return NULL;
    }

    /* allocate the graphid array */
    ssize = gid_stack_size(stack);

    /*
     * Create the container. Note that the path size will always be 2 times the
     * number of edges plus 1 -> (u)-[e]-(v)
     */
    vpc = create_VLE_path_container((ssize * 2) + 1);

    /* set the graph_oid */
    vpc->graph_oid = vlelctx->graph_oid;

    /* get the graphid_array from the container */
    graphid_array = GET_GRAPHID_ARRAY_FROM_CONTAINER(vpc);

    /* get and store the start vertex */
    vid = vlelctx->vsid;
    graphid_array[0] = vid;

    /*
     * Fill in edge entries from the back to the front. The path stack
     * is array-based with index 0 = bottom (first pushed) and
     * index size-1 = top (last pushed). We iterate from top to bottom
     * to fill the graphid_array from back to front.
     */
    index = vpc->graphid_array_size - 2;

    for (j = ssize - 1; j >= 0; j--)
    {
        /* 0 is the vsid, we should never get here */
        Assert(index > 0);

        /* store the edge from stack position j */
        graphid_array[index] = gid_stack_get(stack, j);

        /* we need to skip over the interior vertices */
        index -= 2;
    }

    /* now add in the interior vertices, starting from the first edge */
    for (index = 1; index < vpc->graphid_array_size - 1; index += 2)
    {
        edge_entry *ee = NULL;

        ee = get_edge_entry(vlelctx->ggctx, graphid_array[index]);
        vid = (vid == get_edge_entry_start_vertex_id(ee)) ?
                   get_edge_entry_end_vertex_id(ee) :
                   get_edge_entry_start_vertex_id(ee);
        graphid_array[index+1] = vid;
    }

    /*
     * Stage 1: cache endpoints in the fixed header so the join qual can read
     * them without touching the (possibly toasted) graphid array.
     */
    vpc->start_vid = graphid_array[0];
    vpc->end_vid = graphid_array[vpc->graphid_array_size - 1];

    /* return the container */
    return vpc;
}

/* helper function to build a VPC for just the start vertex */
static VLE_path_container *build_VLE_zero_container(VLE_local_context *vlelctx)
{
    GraphIdStack *stack = vlelctx->dfs_path_stack;
    VLE_path_container *vpc = NULL;
    graphid *graphid_array = NULL;
    graphid vid = 0;

    /* we should have an empty stack */
    if (gid_stack_size(stack) != 0)
    {
        ereport(ERROR,
                (errcode(ERRCODE_DATA_EXCEPTION),
                 errmsg("build_VLE_zero_container: stack is not empty")));
    }

    /*
     * Create the container. Note that the path size will always be 1 as this is
     * just the starting vertex.
     */
    vpc = create_VLE_path_container(1);

    /* set the graph_oid */
    vpc->graph_oid = vlelctx->graph_oid;

    /* get the graphid_array from the container */
    graphid_array = GET_GRAPHID_ARRAY_FROM_CONTAINER(vpc);

    /* get and store the start vertex */
    vid = vlelctx->vsid;
    graphid_array[0] = vid;

    /*
     * Stage 1: zero-edge container; start and end are both the start vertex.
     */
    vpc->start_vid = vid;
    vpc->end_vid = vid;

    return vpc;
}

/*
 * Helper function to build an AGTV_ARRAY of edges from an array of graphids.
 *
 * Note: You should free the array when done. Although, it should be freed
 *       when the context is destroyed from the return of the SRF call.
 */
static agtype_value *build_edge_list(VLE_path_container *vpc)
{
    GRAPH_global_context *ggctx = NULL;
    agtype_in_state edges_result;
    Oid graph_oid = InvalidOid;
    graphid *graphid_array = NULL;
    int64 graphid_array_size = 0;
    int index = 0;

    /* get the graph_oid */
    graph_oid = vpc->graph_oid;

    /* get the GRAPH global context for this graph */
    ggctx = find_GRAPH_global_context(graph_oid);
    /* verify we got a global context */
    Assert(ggctx != NULL);

    /* get the graphid_array and size */
    graphid_array = GET_GRAPHID_ARRAY_FROM_CONTAINER(vpc);
    graphid_array_size = vpc->graphid_array_size;

    /* initialize our agtype array */
    MemSet(&edges_result, 0, sizeof(agtype_in_state));
    edges_result.res = push_agtype_value(&edges_result.parse_state,
                                         WAGT_BEGIN_ARRAY, NULL);

    for (index = 1; index < graphid_array_size - 1; index += 2)
    {
        char *label_name = NULL;
        edge_entry *ee = NULL;
        agtype_value *agtv_edge = NULL;

        /* get the edge entry from the hashtable */
        ee = get_edge_entry(ggctx, graphid_array[index]);
        /* get the label name from the oid */
        label_name = get_rel_name(get_edge_entry_label_table_oid(ee));
        /* reconstruct the edge */
        agtv_edge = agtype_value_build_edge(get_edge_entry_id(ee), label_name,
                                            get_edge_entry_end_vertex_id(ee),
                                            get_edge_entry_start_vertex_id(ee),
                                            get_edge_entry_properties(ee));
        /* push the edge*/
        edges_result.res = push_agtype_value(&edges_result.parse_state,
                                             WAGT_ELEM, agtv_edge);
    }

    /* close our agtype array */
    edges_result.res = push_agtype_value(&edges_result.parse_state,
                                         WAGT_END_ARRAY, NULL);

    /* make it an array */
    edges_result.res->type = AGTV_ARRAY;

    /* return it */
    return edges_result.res;
}

/*
 * Helper function to build an array of type AGTV_PATH from an array of
 * graphids.
 *
 * Note: You should free the array when done. Although, it should be freed
 *       when the context is destroyed from the return of the SRF call.
 */
static agtype_value *build_path(VLE_path_container *vpc)
{
    GRAPH_global_context *ggctx = NULL;
    agtype_in_state path_result;
    Oid graph_oid = InvalidOid;
    graphid *graphid_array = NULL;
    int64 graphid_array_size = 0;
    int index = 0;

    /* get the graph_oid */
    graph_oid = vpc->graph_oid;

    /* get the GRAPH global context for this graph */
    ggctx = find_GRAPH_global_context(graph_oid);
    /* verify we got a global context */
    Assert(ggctx != NULL);

    /* get the graphid_array and size */
    graphid_array = GET_GRAPHID_ARRAY_FROM_CONTAINER(vpc);
    graphid_array_size = vpc->graphid_array_size;

    /* initialize our agtype array */
    MemSet(&path_result, 0, sizeof(agtype_in_state));
    path_result.res = push_agtype_value(&path_result.parse_state,
                                        WAGT_BEGIN_ARRAY, NULL);

    for (index = 0; index < graphid_array_size; index += 2)
    {
        char *label_name = NULL;
        vertex_entry *ve = NULL;
        edge_entry *ee = NULL;
        agtype_value *agtv_vertex = NULL;
        agtype_value *agtv_edge = NULL;

        /* get the vertex entry from the hashtable */
        ve = get_vertex_entry(ggctx, graphid_array[index]);
        /* get the label name from the oid */
        label_name = get_rel_name(get_vertex_entry_label_table_oid(ve));
        /* reconstruct the vertex */
        agtv_vertex = agtype_value_build_vertex(get_vertex_entry_id(ve),
                                                label_name,
                                                get_vertex_entry_properties(ve));
        /* push the vertex */
        path_result.res = push_agtype_value(&path_result.parse_state, WAGT_ELEM,
                                            agtv_vertex);

        /*
         * Remember that we have more vertices than edges. So, we need to check
         * if the above vertex was the last vertex in the path.
         */
        if (index + 1 >= graphid_array_size)
        {
            break;
        }

        /* get the edge entry from the hashtable */
        ee = get_edge_entry(ggctx, graphid_array[index+1]);
        /* get the label name from the oid */
        label_name = get_rel_name(get_edge_entry_label_table_oid(ee));
        /* reconstruct the edge */
        agtv_edge = agtype_value_build_edge(get_edge_entry_id(ee), label_name,
                                            get_edge_entry_end_vertex_id(ee),
                                            get_edge_entry_start_vertex_id(ee),
                                            get_edge_entry_properties(ee));
        /* push the edge*/
        path_result.res = push_agtype_value(&path_result.parse_state, WAGT_ELEM,
                                            agtv_edge);
    }

    /* close our agtype array */
    path_result.res = push_agtype_value(&path_result.parse_state,
                                        WAGT_END_ARRAY, NULL);

    /* make it a path */
    path_result.res->type = AGTV_PATH;

    /* return the path */
    return path_result.res;
}

/*
 * All front facing PG and exposed functions below
 */

/*
 * PG VLE function that takes the following input and returns a row called edges
 * of type agtype BINARY VLE_path_container (this is an internal structure for
 * returning a graphid array of the path. You need to use internal routines to
 * properly use this data) -
 *
 *     0 - agtype REQUIRED (graph name as string)
 *                 Note: This is automatically added by transform_FuncCall.
 *
 *     1 - agtype OPTIONAL (start vertex as a vertex or the integer id)
 *                 Note: Leaving this NULL switches the path algorithm from
 *                       VLE_FUNCTION_PATHS_BETWEEN to VLE_FUNCTION_PATHS_TO
 *     2 - agtype OPTIONAL (end vertex as a vertex or the integer id)
 *                 Note: Leaving this NULL switches the path algorithm from
 *                       VLE_FUNCTION_PATHS_BETWEEN to VLE_FUNCTION_PATHS_FROM
 *                       or - if the starting vertex is NULL - from
 *                       VLE_FUNCTION_PATHS_TO to VLE_FUNCTION_PATHS_ALL
 *     3 - agtype REQUIRED (edge prototype to match as an edge)
 *                 Note: Only the label and properties are used. The
 *                       rest is ignored.
 *     4 - agtype OPTIONAL lidx (lower range index)
 *                 Note: 0 itself is currently not supported but here it is
 *                       equivalent to 1.
 *                       A NULL is appropriate here for a 0 lower bound.
 *     5 - agtype OPTIONAL uidx (upper range index)
 *                 Note: A NULL is appropriate here for an infinite upper bound.
 *     6 - agtype REQUIRED edge direction (enum) as an integer. REQUIRED
 *
 * This is a set returning function. This means that the first call sets
 * up the initial structures and then outputs the first row. After that each
 * subsequent call generates one row of output data. PG will continue to call
 * the function until the function tells PG that it doesn't have any more rows
 * to output. At that point, the function needs to clean up all of its data
 * structures that are not meant to last between SRFs.
 */
PG_FUNCTION_INFO_V1(age_vle);

Datum age_vle(PG_FUNCTION_ARGS)
{
    FuncCallContext *funcctx;
    VLE_local_context *vlelctx = NULL;
    bool found_a_path = false;
    bool done = false;
    bool is_zero_bound = false;
    MemoryContext oldctx;

    /* Initialization for the first call to the SRF */
    if (SRF_IS_FIRSTCALL())
    {
        /* all of these arguments need to be non NULL */
        if (PG_ARGISNULL(0) || /* graph name */
            PG_ARGISNULL(3) || /* edge prototype */
            PG_ARGISNULL(6))   /* direction */
        {
             ereport(ERROR,
                (errcode(ERRCODE_INVALID_PARAMETER_VALUE),
                 errmsg("age_vle: invalid NULL argument passed")));
        }

        /* create a function context for cross-call persistence */
        funcctx = SRF_FIRSTCALL_INIT();

        /*
         * S4: capture the result tuple descriptor.  age_vle now emits a
         * composite (edges, start_id, end_id) row, so we need a blessed
         * TupleDesc that survives across SRF calls.
         */
        {
            TupleDesc      tupdesc;
            MemoryContext  tdesc_oldctx;

            tdesc_oldctx = MemoryContextSwitchTo(funcctx->multi_call_memory_ctx);
            if (get_call_result_type(fcinfo, NULL, &tupdesc) != TYPEFUNC_COMPOSITE)
            {
                ereport(ERROR,
                        (errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
                         errmsg("age_vle: function returning record called in context that cannot accept type record")));
            }
            funcctx->tuple_desc = BlessTupleDesc(tupdesc);
            MemoryContextSwitchTo(tdesc_oldctx);
        }

        /* build the local vle context */
        vlelctx = build_local_vle_context(fcinfo, funcctx);

        /*
         * If the context is NULL, there are no paths to find.
         * This can happen when a cached VLE context has exhausted
         * its vertex list (e.g., from a NULL OPTIONAL MATCH variable).
         */
        if (vlelctx == NULL)
        {
            SRF_RETURN_DONE(funcctx);
        }

        /*
         * Point the function call context's user pointer to the local VLE
         * context just created
         */
        funcctx->user_fctx = vlelctx;

        /* if we are starting from zero [*0..x] flag it */
        if (vlelctx->lidx == 0)
        {
            is_zero_bound = true;
            done = true;
        }
    }

    /* stuff done on every call of the function */
    funcctx = SRF_PERCALL_SETUP();

    /* restore our VLE local context */
    vlelctx = (VLE_local_context *)funcctx->user_fctx;

    /*
     * All work done in dfs_find_a_path needs to be done in a context that
     * survives multiple SRF calls. So switch to the appropriate context.
     */
    oldctx = MemoryContextSwitchTo(funcctx->multi_call_memory_ctx);

    while (done == false)
    {
        /* find one path based on specific input */
        switch (vlelctx->path_function)
        {
            case VLE_FUNCTION_PATHS_TO:
            case VLE_FUNCTION_PATHS_BETWEEN:
                found_a_path = dfs_find_a_path_between(vlelctx);
                break;

            case VLE_FUNCTION_PATHS_ALL:
            case VLE_FUNCTION_PATHS_FROM:
                found_a_path = dfs_find_a_path_from(vlelctx);
                break;

            default:
                found_a_path = false;
                break;
        }

        /* if we found a path, or are done, flag it so we can output the data */
        if (found_a_path == true ||
            (found_a_path == false && vlelctx->next_vertex == NULL) ||
            (found_a_path == false &&
             (vlelctx->path_function == VLE_FUNCTION_PATHS_BETWEEN ||
              vlelctx->path_function == VLE_FUNCTION_PATHS_FROM)))
        {
            done = true;
        }
        /* if we need to fetch a new vertex and rerun the find */
        else if ((vlelctx->path_function == VLE_FUNCTION_PATHS_ALL) ||
                 (vlelctx->path_function == VLE_FUNCTION_PATHS_TO))
        {
            /* get the next start vertex id */
            vlelctx->vsid = get_graphid(vlelctx->next_vertex);

            /* increment to the next vertex */
            vlelctx->next_vertex = next_GraphIdNode(vlelctx->next_vertex);

            /* load in the starting edge(s) */
            load_initial_dfs_stacks(vlelctx);

            /* if we are starting from zero [*0..x] flag it */
            if (vlelctx->lidx == 0)
            {
                is_zero_bound = true;
                done = true;
            }
            /* otherwise we need to loop back around */
            else
            {
                done = false;
            }
        }
        /* we shouldn't get here */
        else
        {
            ereport(ERROR,
                    (errcode(ERRCODE_INVALID_PARAMETER_VALUE),
                     errmsg("age_vle() invalid path function")));
        }
    }

    /* switch back to a more volatile context */
    MemoryContextSwitchTo(oldctx);

    /*
     * If we find a path, we need to convert the path_stack into a list that
     * the outside world can use.
     */
    if (found_a_path || is_zero_bound)
    {
        VLE_path_container *vpc = NULL;

        /* if this isn't the zero boundary case generate a normal vpc */
        if (!is_zero_bound)
        {
            /* the path_stack should have something in it if we have a path */
            Assert(vlelctx->dfs_path_stack > 0);

            /*
             * Build the graphid array into a VLE_path_container from the
             * path_stack. This will also correct for the path_stack being last
             * in, first out.
             */
            vpc = build_VLE_path_container(vlelctx);
        }
        /* otherwise, this is the zero boundary case [*0..x] */
        else
        {
            vpc = build_VLE_zero_container(vlelctx);
        }

        /*
         * S4: emit a composite (edges, start_id, end_id) row.  The
         * scalar endpoint columns let the cypher transformer (S5)
         * rewrite terminal-edge quals as integer equalities, removing
         * the per-row age_match_vle_terminal_edge function call.
         */
        {
            Datum     values[3];
            bool      nulls[3] = {false, false, false};
            HeapTuple tuple;

            values[0] = PointerGetDatum(vpc);
            values[1] = GRAPHID_GET_DATUM(vpc->start_vid);
            values[2] = GRAPHID_GET_DATUM(vpc->end_vid);

            tuple = heap_form_tuple(funcctx->tuple_desc, values, nulls);
            SRF_RETURN_NEXT(funcctx, HeapTupleGetDatum(tuple));
        }
    }
    /* otherwise, we are done and we need to cleanup and signal done */
    else
    {
        /* mark local context as clean */
        vlelctx->is_dirty = false;

        /* free the local context, if we aren't caching it */
        if (vlelctx->use_cache == false)
        {
            free_VLE_local_context(vlelctx);
        }

        /* signal that we are done */
        SRF_RETURN_DONE(funcctx);
    }
}

/*
 * Exposed helper function to make an agtype AGTV_PATH from a
 * VLE_path_container.
 */
agtype *agt_materialize_vle_path(agtype *agt_arg_vpc)
{
    agtype *agt_path = NULL;
    agtype_value *agtv_path = NULL;

    /* get the path */
    agtv_path = agtv_materialize_vle_path(agt_arg_vpc);

    /* convert  agtype_value to agtype */
    agt_path = agtype_value_to_agtype(agtv_path);

    /* free in memory path */
    pfree_agtype_value(agtv_path);

    return agt_path;
}

/*
 * Exposed helper function to make an agtype_value AGTV_PATH from a
 * VLE_path_container.
 */
agtype_value *agtv_materialize_vle_path(agtype *agt_arg_vpc)
{
    VLE_path_container *vpc = NULL;
    agtype_value *agtv_path = NULL;

    /* the passed argument should not be NULL */
    Assert(agt_arg_vpc != NULL);

    /*
     * The path must be a binary container and the type of the object in the
     * container must be an AGT_FBINARY_TYPE_VLE_PATH.
     */
    Assert(AGT_ROOT_IS_BINARY(agt_arg_vpc));
    Assert(AGT_ROOT_BINARY_FLAGS(agt_arg_vpc) == AGT_FBINARY_TYPE_VLE_PATH);

    /* get the container */
    vpc = (VLE_path_container *)agt_arg_vpc;

    /* it should not be null */
    Assert(vpc != NULL);

    /* build the AGTV_PATH from the VLE_path_container */
    agtv_path = build_path(vpc);

    return agtv_path;
}

/*
 * age_match_two_vle_edges and age_match_vle_terminal_edge are retained as
 * stub C symbols only.  The cypher transformer no longer emits calls to
 * either function: terminal-edge match quals are now plain graphid
 * equalities on the age_vle SRF's start_id/end_id output columns
 * (Stages S4/S5/S6 of the VLE terminal-qual rewrite).
 *
 * The corresponding SQL declarations have been removed from fresh
 * installs (sql/agtype_typecast.sql) and are DROP'd by the upgrade
 * script (age--1.7.0--y.y.y.sql).  These C entry points exist solely so
 * the upgrade-test machinery, which loads an older "1.7.0_initial" SQL
 * snapshot against the current age.so, can resolve the symbols before
 * the immediate ALTER EXTENSION UPDATE drops them.  They should never
 * be reachable from any committed SQL path.
 */
PG_FUNCTION_INFO_V1(age_match_two_vle_edges);

Datum age_match_two_vle_edges(PG_FUNCTION_ARGS)
{
    ereport(ERROR,
        (errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
         errmsg("age_match_two_vle_edges() is removed; "
                "VLE endpoint matching is now handled by the planner via "
                "the age_vle SRF's start_id/end_id output columns")));
    PG_RETURN_BOOL(false);
}

/*
 * This function is used when we need to know if the passed in id is at the end
 * of a path. The first arg is the path, the second is the vertex id to check and
 * the last is a boolean that says whether to check the start or the end of the
 * vle path.
 */
PG_FUNCTION_INFO_V1(age_match_vle_edge_to_id_qual);

Datum age_match_vle_edge_to_id_qual(PG_FUNCTION_ARGS)
{
    agtype *agt_arg_vpc = NULL;
    agtype *edge_id = NULL;
    agtype *pos_agt = NULL;
    agtype_value *id, *position;
    VLE_path_container *vle_path = NULL;
    graphid *array = NULL;
    bool vle_is_on_left = false;
    graphid gid = 0;
    Oid type1;

    /* check argument count */
    if (PG_NARGS() != 3)
    {
        ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE),
                        errmsg("age_match_vle_edge_to_id_qual() invalid number of arguments")));
    }

    /*
     * If any argument is NULL, return FALSE. This can occur in
     * OPTIONAL MATCH (LEFT JOIN) contexts where a preceding clause
     * produced no results.
     */
    if (PG_ARGISNULL(0) || PG_ARGISNULL(1) || PG_ARGISNULL(2))
    {
        PG_RETURN_BOOL(false);
    }

    /* get the VLE_path_container argument */
    agt_arg_vpc = DATUM_GET_AGTYPE_P(PG_GETARG_DATUM(0));

    if (!AGT_ROOT_IS_BINARY(agt_arg_vpc) ||
        AGT_ROOT_BINARY_FLAGS(agt_arg_vpc) != AGT_FBINARY_TYPE_VLE_PATH)
    {
        ereport(ERROR,
            (errcode(ERRCODE_INVALID_PARAMETER_VALUE),
            errmsg("argument 1 of age_match_vle_edge_to_edge_qual must be a VLE_Path_Container")));
    }

    /* cast argument as a VLE_Path_Container and extract graphid array */
    vle_path = (VLE_path_container *)agt_arg_vpc;
    array = GET_GRAPHID_ARRAY_FROM_CONTAINER(vle_path);

    /*
     * Get arg type for argument 1 — cache in fn_extra to avoid
     * repeated expression type resolution.
     */
    if (fcinfo->flinfo->fn_extra == NULL)
    {
        Oid *cached_type = MemoryContextAlloc(fcinfo->flinfo->fn_mcxt,
                                               sizeof(Oid));
        *cached_type = get_fn_expr_argtype(fcinfo->flinfo, 1);
        fcinfo->flinfo->fn_extra = cached_type;
    }
    type1 = *(Oid *)fcinfo->flinfo->fn_extra;

    if (type1 == AGTYPEOID)
    {
        /* Get the edge id we are checking the end of the list too */
        edge_id = AG_GET_ARG_AGTYPE_P(1);
        if (!AGT_ROOT_IS_SCALAR(edge_id))
        {
            ereport(ERROR,
                    (errcode(ERRCODE_INVALID_PARAMETER_VALUE),
                     errmsg("argument 2 of age_match_vle_edge_to_edge_qual must be an integer")));
        }

        id = get_ith_agtype_value_from_container(&edge_id->root, 0);

        if (id->type != AGTV_INTEGER)
        {
            ereport(ERROR,
                    (errcode(ERRCODE_INVALID_PARAMETER_VALUE),
                     errmsg("argument 2 of age_match_vle_edge_to_edge_qual must be an integer")));
        }

        gid = id->val.int_value;
    }
    else if (type1 == GRAPHIDOID)
    {
        gid = DATUM_GET_GRAPHID(PG_GETARG_DATUM(1));
    }
    else
    {
        ereport(ERROR,
                (errcode(ERRCODE_INVALID_PARAMETER_VALUE),
                 errmsg("match_vle_terminal_edge() argument 1 must be an agtype integer or a graphid")));
    }

    pos_agt = AG_GET_ARG_AGTYPE_P(2);

    if (!AGT_ROOT_IS_SCALAR(pos_agt))
    {
        ereport(ERROR,
                (errcode(ERRCODE_INVALID_PARAMETER_VALUE),
                 errmsg("argument 3 of age_match_vle_edge_to_edge_qual must be an integer")));
    }

    position = get_ith_agtype_value_from_container(&pos_agt->root, 0);

    if (position->type != AGTV_BOOL)
    {
        ereport(ERROR,
                (errcode(ERRCODE_INVALID_PARAMETER_VALUE),
                 errmsg("argument 3 of age_match_vle_edge_to_edge_qual must be an integer")));
    }

    vle_is_on_left = position->val.boolean;

    if (vle_is_on_left)
    {
        int array_size = vle_path->graphid_array_size;

        /*
         * Path is like ...[vle_edge]-()-[regular_edge]... Get the graphid of
         * the vertex at the endof the path and check that it matches the id
         * that was passed in the second arg. The transform logic is responsible
         * for making that the start or end id, depending on its direction.
         */
        if (gid != array[array_size - 1])
        {
            PG_RETURN_BOOL(false);
        }

        PG_RETURN_BOOL(true);
    }
    else
    {
        /*
         * Path is like ...[edge]-()-[vle_edge]... Get the vertex at the start
         * of the vle edge and check against id.
         */
        if (gid != array[0])
        {
            PG_RETURN_BOOL(false);
        }

        PG_RETURN_BOOL(true);
    }
}

/*
 * Exposed helper function to make an agtype_value AGTV_ARRAY of edges from a
 * VLE_path_container.
 */
agtype_value *agtv_materialize_vle_edges(agtype *agt_arg_vpc)
{
    VLE_path_container *vpc = NULL;
    agtype_value *agtv_array = NULL;

    /* the passed argument should not be NULL */
    Assert(agt_arg_vpc != NULL);

    /*
     * The path must be a binary container and the type of the object in the
     * container must be an AGT_FBINARY_TYPE_VLE_PATH.
     */
    Assert(AGT_ROOT_IS_BINARY(agt_arg_vpc));
    Assert(AGT_ROOT_BINARY_FLAGS(agt_arg_vpc) == AGT_FBINARY_TYPE_VLE_PATH);

    /* get the container */
    vpc = (VLE_path_container *)agt_arg_vpc;

    /* it should not be null */
    Assert(vpc != NULL);

    /* build the AGTV_ARRAY of edges from the VLE_path_container */
    agtv_array = build_edge_list(vpc);

    return agtv_array;

}

/* PG wrapper function for agtv_materialize_vle_edges */
PG_FUNCTION_INFO_V1(age_materialize_vle_edges);

Datum age_materialize_vle_edges(PG_FUNCTION_ARGS)
{
    agtype *agt_arg_vpc = NULL;
    agtype_value *agtv_array = NULL;

    /* if we have a NULL VLE_path_container, return NULL */
    if (PG_ARGISNULL(0))
    {
        PG_RETURN_NULL();
    }

    /* get the VLE_path_container argument */
    agt_arg_vpc = AG_GET_ARG_AGTYPE_P(0);

    /* if NULL, return NULL */
    if (is_agtype_null(agt_arg_vpc))
    {
        PG_RETURN_NULL();
    }

    agtv_array = agtv_materialize_vle_edges(agt_arg_vpc);

    PG_RETURN_POINTER(agtype_value_to_agtype(agtv_array));
}

/* PG wrapper function for age_materialize_vle_path */
PG_FUNCTION_INFO_V1(age_materialize_vle_path);

Datum age_materialize_vle_path(PG_FUNCTION_ARGS)
{
    agtype *agt_arg_vpc = NULL;

    /* if we have a NULL VLE_path_container, return NULL */
    if (PG_ARGISNULL(0))
    {
        PG_RETURN_NULL();
    }

    /* get the VLE_path_container argument */
    agt_arg_vpc = AG_GET_ARG_AGTYPE_P(0);

    /* if NULL, return NULL */
    if (is_agtype_null(agt_arg_vpc))
    {
        PG_RETURN_NULL();
    }

    PG_RETURN_POINTER(agt_materialize_vle_path(agt_arg_vpc));
}

/* Stub: see comment on age_match_two_vle_edges above. */
PG_FUNCTION_INFO_V1(age_match_vle_terminal_edge);

Datum age_match_vle_terminal_edge(PG_FUNCTION_ARGS)
{
    ereport(ERROR,
        (errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
         errmsg("age_match_vle_terminal_edge() is removed; "
                "VLE endpoint matching is now handled by the planner via "
                "the age_vle SRF's start_id/end_id output columns")));
    PG_RETURN_BOOL(false);
}

/* PG helper function to build an agtype (Datum) edge for matching */
PG_FUNCTION_INFO_V1(age_build_vle_match_edge);

Datum age_build_vle_match_edge(PG_FUNCTION_ARGS)
{
    agtype_in_state result;
    agtype_value agtv_zero;
    agtype_value agtv_nstr;
    agtype_value *agtv_temp = NULL;

    /* create an agtype_value integer 0 */
    agtv_zero.type = AGTV_INTEGER;
    agtv_zero.val.int_value = 0;

    /* create an agtype_value null string */
    agtv_nstr.type = AGTV_STRING;
    agtv_nstr.val.string.len = 0;
    agtv_nstr.val.string.val = NULL;

    /* zero the state */
    memset(&result, 0, sizeof(agtype_in_state));

    /* start the object */
    result.res = push_agtype_value(&result.parse_state, WAGT_BEGIN_OBJECT,
                                   NULL);
    /* create dummy graph id */
    result.res = push_agtype_value(&result.parse_state, WAGT_KEY,
                                   string_to_agtype_value("id"));
    result.res = push_agtype_value(&result.parse_state, WAGT_VALUE, &agtv_zero);
    /* process the label */
    result.res = push_agtype_value(&result.parse_state, WAGT_KEY,
                                   string_to_agtype_value("label"));
    if (!PG_ARGISNULL(0))
    {
        agtv_temp = get_agtype_value("build_vle_match_edge",
                                     AG_GET_ARG_AGTYPE_P(0), AGTV_STRING, true);
        result.res = push_agtype_value(&result.parse_state, WAGT_VALUE,
                                       agtv_temp);
    }
    else
    {
        result.res = push_agtype_value(&result.parse_state, WAGT_VALUE,
                                       &agtv_nstr);
    }
    /* create dummy end_id */
    result.res = push_agtype_value(&result.parse_state, WAGT_KEY,
                                   string_to_agtype_value("end_id"));
    result.res = push_agtype_value(&result.parse_state, WAGT_VALUE, &agtv_zero);
    /* create dummy start_id */
    result.res = push_agtype_value(&result.parse_state, WAGT_KEY,
                                   string_to_agtype_value("start_id"));
    result.res = push_agtype_value(&result.parse_state, WAGT_VALUE, &agtv_zero);

    /* process the properties */
    result.res = push_agtype_value(&result.parse_state, WAGT_KEY,
                                   string_to_agtype_value("properties"));
    if (!PG_ARGISNULL(1))
    {
        agtype *properties = NULL;

        properties = AG_GET_ARG_AGTYPE_P(1);

        if (!AGT_ROOT_IS_OBJECT(properties))
        {
            ereport(ERROR,
                    (errcode(ERRCODE_INVALID_PARAMETER_VALUE),
                     errmsg("build_vle_match_edge(): properties argument must be an object")));
        }

        add_agtype((Datum)properties, false, &result, AGTYPEOID, false);

    }
    else
    {
        result.res = push_agtype_value(&result.parse_state, WAGT_BEGIN_OBJECT,
                                       NULL);
        result.res = push_agtype_value(&result.parse_state, WAGT_END_OBJECT,
                                       NULL);
    }

    result.res = push_agtype_value(&result.parse_state, WAGT_END_OBJECT, NULL);

    result.res->type = AGTV_EDGE;

    PG_RETURN_POINTER(agtype_value_to_agtype(result.res));
}

PG_FUNCTION_INFO_V1(_ag_enforce_edge_uniqueness2);

Datum _ag_enforce_edge_uniqueness2(PG_FUNCTION_ARGS)
{
    graphid gid1 = AG_GETARG_GRAPHID(0);
    graphid gid2 = AG_GETARG_GRAPHID(1);

    if (gid1 == gid2)
    {
        PG_RETURN_BOOL(false);
    }

    PG_RETURN_BOOL(true);
}

PG_FUNCTION_INFO_V1(_ag_enforce_edge_uniqueness3);

Datum _ag_enforce_edge_uniqueness3(PG_FUNCTION_ARGS)
{
    graphid gid1 = AG_GETARG_GRAPHID(0);
    graphid gid2 = AG_GETARG_GRAPHID(1);
    graphid gid3 = AG_GETARG_GRAPHID(2);

    if (gid1 == gid2 || gid1 == gid3 || gid2 == gid3)
    {
        PG_RETURN_BOOL(false);
    }

    PG_RETURN_BOOL(true);
}

PG_FUNCTION_INFO_V1(_ag_enforce_edge_uniqueness4);

Datum _ag_enforce_edge_uniqueness4(PG_FUNCTION_ARGS)
{
    graphid gid1 = AG_GETARG_GRAPHID(0);
    graphid gid2 = AG_GETARG_GRAPHID(1);
    graphid gid3 = AG_GETARG_GRAPHID(2);
    graphid gid4 = AG_GETARG_GRAPHID(3);

    if (gid1 == gid2 || gid1 == gid3 || gid1 == gid4 ||
        gid2 == gid3 || gid2 == gid4 || gid3 == gid4)
    {
        PG_RETURN_BOOL(false);
    }

    PG_RETURN_BOOL(true);
}

/*
 * This function checks the edges in a MATCH clause to see if they are unique or
 * not. Filters out all the paths where the edge uniques rules are not met.
 * Arguments can be a combination of agtype ints and VLE_path_containers.
 */
PG_FUNCTION_INFO_V1(_ag_enforce_edge_uniqueness);

Datum _ag_enforce_edge_uniqueness(PG_FUNCTION_ARGS)
{
    HTAB *exists_hash = NULL;
    HASHCTL exists_ctl;
    Datum *args = NULL;
    bool *nulls = NULL;
    Oid *types = NULL;
    int nargs = 0;
    int i = 0;

    /* extract our arguments */
    nargs = extract_variadic_args(fcinfo, 0, true, &args, &types, &nulls);

    /* verify the arguments */
    for (i = 0; i < nargs; i++)
    {
        if (nulls[i])
        {
             ereport(ERROR,
                     (errcode(ERRCODE_INVALID_PARAMETER_VALUE),
                      errmsg("_ag_enforce_edge_uniqueness argument %d must not be NULL",
                             i)));
        }
        if (types[i] != AGTYPEOID &&
            types[i] != INT8OID &&
            types[i] != GRAPHIDOID)
        {
             ereport(ERROR,
                     (errcode(ERRCODE_INVALID_PARAMETER_VALUE),
                      errmsg("_ag_enforce_edge_uniqueness argument %d must be AGTYPE, INT8, or GRAPHIDOID",
                             i)));
        }
    }

    /* configure the hash table */
    MemSet(&exists_ctl, 0, sizeof(exists_ctl));
    exists_ctl.keysize = sizeof(int64);
    exists_ctl.entrysize = sizeof(int64);
    exists_ctl.hash = graphid_hash;

    /* create exists_hash table */
    exists_hash = hash_create(EXISTS_HTAB_NAME, EXISTS_HTAB_NAME_INITIAL_SIZE,
                              &exists_ctl, HASH_ELEM | HASH_FUNCTION);

    /* insert arguments into hash table */
    for (i = 0; i < nargs; i++)
    {
        /* if it is an INT8OID or a GRAPHIDOID */
        if (types[i] == INT8OID || types[i] == GRAPHIDOID)
        {
            graphid edge_id = 0;
            bool found = false;
            int64 *value = NULL;

            edge_id = DatumGetInt64(args[i]);

            /* insert the edge_id */
            value = (int64 *)hash_search(exists_hash, (void *)&edge_id,
                                         HASH_ENTER, &found);

            /* if we found it, we're done, we have a duplicate */
            if (found)
            {
                hash_destroy(exists_hash);
                PG_RETURN_BOOL(false);
            }
            /* otherwise, add it to the returned bucket */
            else
            {
                *value = edge_id;
            }

            continue;
        }
        else if (types[i] == AGTYPEOID)
        {
            /* get the argument */
            agtype *agt_i = DATUM_GET_AGTYPE_P(args[i]);

            /* if the argument is an AGTYPE VLE_path_container */
            if (AGT_ROOT_IS_BINARY(agt_i) &&
                AGT_ROOT_BINARY_FLAGS(agt_i) == AGT_FBINARY_TYPE_VLE_PATH)
            {
                VLE_path_container *vpc = NULL;
                graphid *graphid_array = NULL;
                int64 graphid_array_size = 0;
                int64 j = 0;

                /* cast to VLE_path_container */
                vpc = (VLE_path_container *)agt_i;

                /* get the graphid array */
                graphid_array = GET_GRAPHID_ARRAY_FROM_CONTAINER(vpc);

                /* get the graphid array size */
                graphid_array_size = vpc->graphid_array_size;

                /* insert all the edges in the vpc, into the hash table */
                for (j = 1; j < graphid_array_size - 1; j+=2)
                {
                    int64 *value = NULL;
                    bool found = false;
                    graphid edge_id = 0;

                    /* get the edge id */
                    edge_id = graphid_array[j];

                    /* insert the edge id */
                    value = (int64 *)hash_search(exists_hash, (void *)&edge_id,
                                                 HASH_ENTER, &found);

                    /* if we found it, we're done, we have a duplicate */
                    if (found)
                    {
                        hash_destroy(exists_hash);
                        PG_RETURN_BOOL(false);
                    }
                    /* otherwise, add it to the returned bucket */
                    else
                    {
                        *value = edge_id;
                    }
                }
            }
            /* if it is a regular AGTYPE scalar */
            else if (AGT_ROOT_IS_SCALAR(agt_i))
            {
                agtype_value *agtv_id = NULL;
                int64 *value = NULL;
                bool found = false;
                graphid edge_id = 0;

                agtv_id = get_ith_agtype_value_from_container(&agt_i->root, 0);

                if (agtv_id->type != AGTV_INTEGER)
                {
                    ereport(ERROR,
                            (errcode(ERRCODE_INVALID_PARAMETER_VALUE),
                             errmsg("_ag_enforce_edge_uniqueness parameter %d must resolve to an agtype integer",
                                    i)));
                }

                edge_id = agtv_id->val.int_value;

                /* insert the edge_id */
                value = (int64 *)hash_search(exists_hash, (void *)&edge_id,
                                             HASH_ENTER, &found);

                /* if we found it, we're done, we have a duplicate */
                if (found)
                {
                    hash_destroy(exists_hash);
                    PG_RETURN_BOOL(false);
                }
                /* otherwise, add it to the returned bucket */
                else
                {
                    *value = edge_id;
                }
            }
            else
            {
                ereport(ERROR,
                        (errcode(ERRCODE_INVALID_PARAMETER_VALUE),
                         errmsg("_ag_enforce_edge_uniqueness invalid parameter type %d",
                                i)));
            }
        }
        /* it is neither a VLE_path_container, AGTYPE, INT8, or a GRAPHIDOID */
        else
        {
            ereport(ERROR,
                    (errcode(ERRCODE_INVALID_PARAMETER_VALUE),
                     errmsg("_ag_enforce_edge_uniqueness invalid parameter type %d",
                            i)));
        }
    }

    /* if all entries were successfully inserted, we have no duplicates */
    hash_destroy(exists_hash);
    PG_RETURN_BOOL(true);
}

/*
 * ---------------------------------------------------------------------------
 * Shortest path / all shortest paths
 * ---------------------------------------------------------------------------
 *
 * Plain (non-grammar) set-returning functions that compute the unweighted
 * (hop-count) shortest path between two vertices, built directly on top of the
 * cached global graph (GRAPH_global_context) and its flat-array adjacency
 * (VertexEdgeArray). These do NOT go through the VLE grammar/transform path;
 * they are user-callable helpers:
 *
 *     ag_catalog.age_shortest_path(graph, start, end
 *         [, edge_types [, direction [, min_hops [, max_hops]]]])
 *     ag_catalog.age_all_shortest_paths(graph, start, end
 *         [, edge_types [, direction [, min_hops [, max_hops]]]])
 *
 * Both perform a breadth-first search from the start vertex. age_shortest_path
 * returns a single path (0 or 1 rows); age_all_shortest_paths returns every
 * path whose length equals the minimum hop count (one row per path), by
 * recording a predecessor multiset during the BFS and enumerating the
 * resulting shortest-path DAG.
 *
 * Because BFS depth strictly increases, every emitted path is simple (no
 * repeated vertex and therefore no repeated edge), satisfying openCypher
 * edge-isomorphism for these fixed-length results.
 */

/* Simple FIFO queue of graphids for the BFS frontier. */
typedef struct sp_queue
{
    graphid *data;
    int64 head;
    int64 tail;
    int64 cap;
} sp_queue;

/* One predecessor edge on a shortest path (all-shortest-paths mode). */
typedef struct sp_pred
{
    graphid edge;
    graphid parent_vertex;
} sp_pred;

/* Per-vertex BFS bookkeeping, keyed by vertex_id in the visited hashtable. */
typedef struct sp_visit_entry
{
    graphid vertex_id;     /* hash key — must be first */
    int64 depth;           /* BFS depth from the source vertex */
    graphid parent_edge;   /* single-path reconstruction */
    graphid parent_vertex; /* single-path reconstruction */
    List *preds;           /* sp_pred * list for all-shortest-paths mode */
} sp_visit_entry;

/* Cross-call SRF state: the precomputed result paths streamed one per call. */
typedef struct sp_srf_state
{
    Datum *paths;
    int64 npaths;
    int64 next;
} sp_srf_state;

static void sp_queue_init(sp_queue *q)
{
    q->cap = 1024;
    q->head = 0;
    q->tail = 0;
    q->data = palloc(sizeof(graphid) * q->cap);
}

static void sp_queue_push(sp_queue *q, graphid v)
{
    if (q->tail == q->cap)
    {
        q->cap = q->cap * 2;
        q->data = repalloc(q->data, sizeof(graphid) * q->cap);
    }
    q->data[q->tail] = v;
    q->tail = q->tail + 1;
}

static bool sp_queue_is_empty(sp_queue *q)
{
    return q->head == q->tail;
}

static graphid sp_queue_pop(sp_queue *q)
{
    graphid v = q->data[q->head];

    q->head = q->head + 1;
    return v;
}

/* Resolve a vertex argument (a vertex agtype or an integer id) to a graphid. */
static graphid sp_agtype_to_graphid(agtype *agt, char *fname,
                                    const char *argname)
{
    agtype_value *agtv = NULL;

    agtv = get_agtype_value(fname, agt, AGTV_VERTEX, false);

    if (agtv != NULL && agtv->type == AGTV_VERTEX)
    {
        agtv = GET_AGTYPE_VALUE_OBJECT_VALUE(agtv, "id");
    }
    else if (agtv == NULL || agtv->type != AGTV_INTEGER)
    {
        ereport(ERROR,
                (errcode(ERRCODE_INVALID_PARAMETER_VALUE),
                 errmsg("%s argument must be a vertex or the integer id",
                        argname)));
    }

    return agtv->val.int_value;
}

/* Resolve the optional direction argument; NULL defaults to undirected. */
static cypher_rel_dir sp_agtype_to_direction(agtype *agt, char *fname)
{
    agtype_value *agtv = NULL;
    char *s = NULL;
    cypher_rel_dir dir = CYPHER_REL_DIR_NONE;

    if (agt == NULL)
    {
        return CYPHER_REL_DIR_NONE;
    }

    agtv = get_agtype_value(fname, agt, AGTV_STRING, true);
    s = pnstrdup(agtv->val.string.val, agtv->val.string.len);

    if (pg_strcasecmp(s, "out") == 0)
    {
        dir = CYPHER_REL_DIR_RIGHT;
    }
    else if (pg_strcasecmp(s, "in") == 0)
    {
        dir = CYPHER_REL_DIR_LEFT;
    }
    else if (pg_strcasecmp(s, "any") == 0)
    {
        dir = CYPHER_REL_DIR_NONE;
    }
    else
    {
        ereport(ERROR,
                (errcode(ERRCODE_INVALID_PARAMETER_VALUE),
                 errmsg("%s: direction argument must be one of 'out', 'in', or 'any'",
                        fname)));
    }

    pfree_if_not_null(s);
    return dir;
}

/*
 * Wrap an interleaved [vertex, edge, vertex, ... , vertex] graphid array in a
 * VLE_path_container and materialize it as an AGTV_PATH agtype Datum.
 */
static Datum sp_build_path_datum(Oid graph_oid, graphid *alt, int64 alt_len)
{
    VLE_path_container *vpc = NULL;
    graphid *arr = NULL;
    agtype_value *agtv_path = NULL;
    agtype *agt = NULL;

    vpc = create_VLE_path_container(alt_len);
    vpc->graph_oid = graph_oid;

    arr = GET_GRAPHID_ARRAY_FROM_CONTAINER(vpc);
    memcpy(arr, alt, sizeof(graphid) * alt_len);

    vpc->start_vid = alt[0];
    vpc->end_vid = alt[alt_len - 1];

    agtv_path = build_path(vpc);
    agt = agtype_value_to_agtype(agtv_path);

    return AGTYPE_P_GET_DATUM(agt);
}

/*
 * Breadth-first search from source toward target over the flat-array
 * adjacency. Returns the visited hashtable; sets *out_found and (if found)
 * *out_target_depth (the shortest hop count). In all-shortest-paths mode
 * (collect_all) every shortest-path predecessor is recorded per vertex.
 */
static HTAB *sp_run_bfs(GRAPH_global_context *ggctx, graphid source,
                        graphid target, Oid *label_oids, int n_label_oids,
                        cypher_rel_dir dir, int64 max_hops, bool collect_all,
                        int64 *out_target_depth, bool *out_found)
{
    HASHCTL ctl;
    HTAB *visited = NULL;
    sp_queue q;
    sp_visit_entry *se = NULL;
    bool found = false;
    int64 target_depth = -1;
    bool dir_out = (dir == CYPHER_REL_DIR_RIGHT || dir == CYPHER_REL_DIR_NONE);
    bool dir_in = (dir == CYPHER_REL_DIR_LEFT || dir == CYPHER_REL_DIR_NONE);

    /* visited hashtable: graphid -> sp_visit_entry */
    MemSet(&ctl, 0, sizeof(ctl));
    ctl.keysize = sizeof(int64);
    ctl.entrysize = sizeof(sp_visit_entry);
    ctl.hash = graphid_hash;
    visited = hash_create("age shortest path visited", 1024, &ctl,
                          HASH_ELEM | HASH_FUNCTION);

    /*
     * A path can only exist between vertices that actually exist in the graph.
     * If either endpoint is missing we are done: report "not found" and return
     * the (empty) visited table. This guard is critical: without it a source
     * that equals a non-existent target would be matched at depth 0 (see the
     * "u == target" check below), and path reconstruction would then try to
     * materialize a vertex that does not exist, dereferencing invalid memory
     * and crashing the backend.
     */
    if (get_vertex_entry(ggctx, source) == NULL ||
        get_vertex_entry(ggctx, target) == NULL)
    {
        *out_target_depth = -1;
        *out_found = false;
        return visited;
    }

    sp_queue_init(&q);

    /* seed the frontier with the source vertex at depth 0 */
    se = (sp_visit_entry *) hash_search(visited, &source, HASH_ENTER, NULL);
    se->vertex_id = source;
    se->depth = 0;
    se->parent_edge = 0;
    se->parent_vertex = source;
    se->preds = NIL;
    sp_queue_push(&q, source);

    while (!sp_queue_is_empty(&q))
    {
        graphid u = sp_queue_pop(&q);
        sp_visit_entry *ue = NULL;
        vertex_entry *ve = NULL;
        int64 du = 0;
        int pass = 0;

        /*
         * Allow this search to be cancelled (e.g. by a user Ctrl-C or a
         * statement_timeout). On a large graph the BFS frontier can grow very
         * large, so we must yield to interrupt processing on every iteration.
         */
        CHECK_FOR_INTERRUPTS();

        ue = (sp_visit_entry *) hash_search(visited, &u, HASH_FIND, NULL);
        du = ue->depth;

        /* target reached: record its (shortest) depth */
        if (u == target)
        {
            found = true;
            if (target_depth < 0)
            {
                target_depth = du;
            }
            /* single-path mode: the first discovery is sufficient */
            if (!collect_all)
            {
                break;
            }
        }

        /* never expand at or beyond the shortest target depth */
        if (target_depth >= 0 && du >= target_depth)
        {
            continue;
        }

        /* respect the optional upper hop bound */
        if (max_hops >= 0 && du >= max_hops)
        {
            continue;
        }

        ve = get_vertex_entry(ggctx, u);
        if (ve == NULL)
        {
            continue;
        }

        /* pass 0 = outgoing edges, pass 1 = incoming edges */
        for (pass = 0; pass < 2; pass++)
        {
            VertexEdgeArray *edges = NULL;
            int32 i = 0;

            if (pass == 0)
            {
                if (!dir_out)
                {
                    continue;
                }
                edges = get_vertex_entry_edges_out_array(ve);
            }
            else
            {
                if (!dir_in)
                {
                    continue;
                }
                edges = get_vertex_entry_edges_in_array(ve);
            }

            if (edges == NULL || edges->array == NULL)
            {
                continue;
            }

            for (i = 0; i < edges->size; i++)
            {
                graphid eid = edges->array[i];
                edge_entry *ee = NULL;
                graphid v = 0;
                sp_visit_entry *vse = NULL;
                bool was_present = false;

                ee = get_edge_entry(ggctx, eid);
                if (ee == NULL)
                {
                    continue;
                }

                /*
                 * Optional edge label filter. When a label filter is active
                 * (n_label_oids > 0) we keep only edges whose label table oid
                 * is one of the requested relationship types. A requested type
                 * that does not exist in this graph resolves to InvalidOid;
                 * since no real edge can have an InvalidOid label table, such a
                 * type contributes no matches and simply drops out of the set,
                 * while edges of any of the other (known) requested types still
                 * match. Only when every requested type is unknown does the
                 * filter match no edges, leaving just the zero-length
                 * (start == end) path -- matching the openCypher semantics that
                 * an unknown relationship type matches no relationships.
                 */
                if (n_label_oids > 0)
                {
                    Oid ee_label_oid = get_edge_entry_label_table_oid(ee);
                    bool label_match = false;
                    int li = 0;

                    for (li = 0; li < n_label_oids; li++)
                    {
                        if (label_oids[li] == ee_label_oid)
                        {
                            label_match = true;
                            break;
                        }
                    }
                    if (!label_match)
                    {
                        continue;
                    }
                }

                /* the neighbor depends on which side of the edge u is on */
                if (pass == 0)
                {
                    v = get_edge_entry_end_vertex_id(ee);
                }
                else
                {
                    v = get_edge_entry_start_vertex_id(ee);
                }

                /* self loops never shorten a path to a different vertex */
                if (v == u)
                {
                    continue;
                }

                vse = (sp_visit_entry *) hash_search(visited, &v, HASH_ENTER,
                                                     &was_present);
                if (!was_present)
                {
                    vse->vertex_id = v;
                    vse->depth = du + 1;
                    vse->parent_edge = eid;
                    vse->parent_vertex = u;
                    vse->preds = NIL;

                    if (collect_all)
                    {
                        sp_pred *p = palloc(sizeof(sp_pred));

                        p->edge = eid;
                        p->parent_vertex = u;
                        vse->preds = lappend(vse->preds, p);
                    }

                    sp_queue_push(&q, v);
                }
                else if (collect_all && vse->depth == du + 1)
                {
                    /* another equally-short predecessor of v */
                    sp_pred *p = palloc(sizeof(sp_pred));

                    p->edge = eid;
                    p->parent_vertex = u;
                    vse->preds = lappend(vse->preds, p);
                }
            }
        }
    }

    *out_target_depth = target_depth;
    *out_found = found;
    return visited;
}

/*
 * Maximum number of result paths age_all_shortest_paths will materialize
 * before raising an error. The shortest-path DAG can contain exponentially
 * many equal-length paths (grid-like or multi-edge graphs), and they are all
 * built up front in the SRF's memory context, so this is a backstop against
 * unbounded memory growth. CHECK_FOR_INTERRUPTS() in sp_enumerate still allows
 * cancellation, but a fast explosion can outrun a statement_timeout.
 */
#define SP_MAX_RESULT_PATHS 1000000

/*
 * Recursively enumerate every shortest path by walking the predecessor DAG
 * from target back to source. Each completed path is appended to *out as a
 * freshly allocated interleaved graphid array of length alt_len. The running
 * total is capped at SP_MAX_RESULT_PATHS to bound peak memory.
 */
static void sp_enumerate(HTAB *visited, graphid source, graphid cur,
                         graphid *alt, int64 alt_len, int64 pos,
                         char *fname, List **out)
{
    sp_visit_entry *e = NULL;
    ListCell *lc = NULL;

    /*
     * Enumerating every shortest path can be combinatorially expensive, so
     * allow the user to cancel (Ctrl-C / statement_timeout) at each step.
     */
    CHECK_FOR_INTERRUPTS();

    alt[pos] = cur;

    if (cur == source)
    {
        /* a complete path only when we have consumed the whole array */
        if (pos == 0)
        {
            graphid *copy = palloc(sizeof(graphid) * alt_len);

            memcpy(copy, alt, sizeof(graphid) * alt_len);
            *out = lappend(*out, copy);

            /*
             * Bound the number of materialized paths. Without a ceiling, a
             * combinatorial shortest-path DAG could exhaust memory before the
             * first row is returned.
             */
            if (list_length(*out) > SP_MAX_RESULT_PATHS)
            {
                ereport(ERROR,
                        (errcode(ERRCODE_PROGRAM_LIMIT_EXCEEDED),
                         errmsg("%s: shortest path count exceeded %d",
                                fname, SP_MAX_RESULT_PATHS),
                         errhint("Narrow the search with a relationship type or a maximum hop count, or use age_shortest_path for a single path.")));
            }
        }
        return;
    }

    e = (sp_visit_entry *) hash_search(visited, &cur, HASH_FIND, NULL);
    if (e == NULL)
    {
        return;
    }

    foreach(lc, e->preds)
    {
        sp_pred *p = (sp_pred *) lfirst(lc);

        alt[pos - 1] = p->edge;
        sp_enumerate(visited, source, p->parent_vertex, alt, alt_len, pos - 2,
                     fname, out);
    }
}

/*
 * Maximum number of distinct paths the minimum-hop fallback will enumerate
 * before giving up. The exhaustive DFS used for a minimum hop count greater
 * than the shortest distance can explode on dense graphs, so this acts as a
 * safety valve alongside CHECK_FOR_INTERRUPTS()/statement_timeout in the DFS.
 */
#define SP_MINHOPS_MAX_PATHS 1000000

/*
 * Fallback for the "minimum hop count greater than the shortest distance"
 * regime, which plain BFS cannot satisfy (it requires longer, vertex-revisiting
 * paths under relationship-uniqueness). This reuses the VLE depth-first engine
 * directly: it builds a VLE_local_context by hand (no fcinfo), enumerates every
 * path whose length is within [min_hops, max_hops], and keeps only those of the
 * smallest qualifying length. For shortest_path one such path is returned; for
 * all_shortest_paths every tie at that length is returned. Returns NULL with
 * *out_count == 0 when no qualifying path exists.
 *
 * The VLE engine matches a single edge label oid only, so a multi-type filter
 * is rejected by the caller before reaching here. A single label_oid of
 * InvalidOid means "any edge label".
 */
static Datum *sp_minhops_fallback(GRAPH_global_context *ggctx, Oid graph_oid,
                                  const char *graph_name, char *fname,
                                  graphid source, graphid target, Oid label_oid,
                                  cypher_rel_dir dir, int64 min_hops,
                                  int64 max_hops, bool collect_all,
                                  int64 *out_count)
{
    MemoryContext oldctx = CurrentMemoryContext;
    MemoryContext tmpctx = NULL;
    VLE_local_context *vlelctx = NULL;
    agtype_value av_empty;
    agtype *empty_constraint = NULL;
    List *best = NIL;
    ListCell *lc = NULL;
    int64 best_len = PG_INT64_MAX;
    int64 examined = 0;
    int64 result_len = 0;
    int64 n = 0;
    int64 idx = 0;
    Datum *paths = NULL;

    *out_count = 0;

    /* do the VLE enumeration in a private context we can throw away at the end */
    tmpctx = AllocSetContextCreate(oldctx, "age shortest path minhops",
                                   ALLOCSET_DEFAULT_SIZES);
    MemoryContextSwitchTo(tmpctx);

    /* an empty property constraint object: every edge satisfies it */
    av_empty.type = AGTV_OBJECT;
    av_empty.val.object.num_pairs = 0;
    av_empty.val.object.pairs = NULL;
    empty_constraint = agtype_value_to_agtype(&av_empty);

    /* build the VLE local context by hand (no fcinfo, no caching) */
    vlelctx = palloc0(sizeof(VLE_local_context));
    vlelctx->graph_name = pnstrdup(graph_name, strlen(graph_name));
    vlelctx->graph_oid = graph_oid;
    vlelctx->ggctx = ggctx;
    vlelctx->path_function = VLE_FUNCTION_PATHS_BETWEEN;
    vlelctx->next_vertex = NULL;
    vlelctx->vsid = source;
    vlelctx->veid = target;
    vlelctx->edge_property_constraint = empty_constraint;
    vlelctx->edge_property_constraint_datum =
        AGTYPE_P_GET_DATUM(empty_constraint);
    vlelctx->edge_property_constraint_hash =
        datum_image_hash(vlelctx->edge_property_constraint_datum, false, -1);
    vlelctx->edge_label_name = NULL;
    vlelctx->edge_label_name_oid = label_oid;
    vlelctx->lidx = (min_hops > 0) ? min_hops : 1;
    if (max_hops < 0)
    {
        vlelctx->uidx_infinite = true;
        vlelctx->uidx = 0;
    }
    else
    {
        vlelctx->uidx_infinite = false;
        vlelctx->uidx = max_hops;
    }
    vlelctx->edge_direction = dir;
    vlelctx->use_cache = false;
    vlelctx->vle_grammar_node_id = 0;
    vlelctx->next = NULL;
    vlelctx->is_dirty = true;

    create_VLE_local_state_hashtable(vlelctx);
    vlelctx->dfs_vertex_stack = new_gid_stack();
    vlelctx->dfs_edge_stack = new_gid_stack();
    vlelctx->dfs_path_stack = new_gid_stack();
    load_initial_dfs_stacks(vlelctx);

    /*
     * Enumerate qualifying paths, keeping only those of the smallest length
     * seen. The DFS yields paths in no particular length order, so a strictly
     * shorter path resets the kept set.
     */
    while (dfs_find_a_path_between(vlelctx))
    {
        int64 hops = gid_stack_size(vlelctx->dfs_path_stack);
        bool take = false;
        bool reset = false;

        examined = examined + 1;
        if (examined > SP_MINHOPS_MAX_PATHS)
        {
            ereport(ERROR,
                    (errcode(ERRCODE_PROGRAM_LIMIT_EXCEEDED),
                     errmsg("%s: minimum hop count search exceeded %d candidate paths",
                            fname, SP_MINHOPS_MAX_PATHS),
                     errhint("Provide a maximum hop count to bound the search.")));
        }

        if (hops < best_len)
        {
            take = true;
            reset = true;
        }
        else if (hops == best_len && collect_all)
        {
            take = true;
        }

        if (take)
        {
            VLE_path_container *vpc = NULL;
            graphid *garr = NULL;
            int64 arrlen = 0;

            vpc = build_VLE_path_container(vlelctx);
            garr = GET_GRAPHID_ARRAY_FROM_CONTAINER(vpc);
            arrlen = vpc->graphid_array_size;

            /* copy the path into the surviving context and record it */
            MemoryContextSwitchTo(oldctx);
            if (reset)
            {
                list_free_deep(best);
                best = NIL;
                best_len = hops;
            }
            {
                graphid *copy = palloc(sizeof(graphid) * arrlen);

                memcpy(copy, garr, sizeof(graphid) * arrlen);
                best = lappend(best, copy);
            }
            MemoryContextSwitchTo(tmpctx);

            pfree(vpc);
        }
    }

    /* tear down the VLE engine state, then drop the whole scratch context */
    free_VLE_local_context(vlelctx);
    MemoryContextSwitchTo(oldctx);
    MemoryContextDelete(tmpctx);

    n = list_length(best);
    if (n == 0)
    {
        return NULL;
    }

    /* every kept path has the same (minimum qualifying) length */
    result_len = (2 * best_len) + 1;
    paths = palloc(sizeof(Datum) * n);
    foreach(lc, best)
    {
        graphid *a = (graphid *) lfirst(lc);

        paths[idx] = sp_build_path_datum(graph_oid, a, result_len);
        idx = idx + 1;
    }

    list_free_deep(best);
    *out_count = n;
    return paths;
}

/*
 * Resolve arguments, run the BFS, and materialize the result path(s) as an
 * array of AGTV_PATH agtype Datums. Returns NULL with *out_count == 0 when no
 * path exists. Caller must run in a context that survives the SRF.
 */
static Datum *sp_compute_paths(agtype *graph_name_agt, agtype *start_agt,
                               agtype *end_agt, agtype *label_agt,
                               agtype *dir_agt, agtype *minhops_agt,
                               agtype *maxhops_agt, char *fname,
                               bool collect_all, int64 *out_count)
{
    agtype_value *agtv_temp = NULL;
    char *graph_name = NULL;
    Oid graph_oid = InvalidOid;
    GRAPH_global_context *ggctx = NULL;
    graphid source = 0;
    graphid target = 0;
    Oid *label_oids = NULL;
    int n_label_oids = 0;
    cypher_rel_dir dir = CYPHER_REL_DIR_NONE;
    int64 min_hops = 0;
    int64 max_hops = -1;
    HTAB *visited = NULL;
    int64 target_depth = -1;
    bool found = false;
    Datum *paths = NULL;
    MemoryContext oldctx = CurrentMemoryContext;
    MemoryContext scratch = NULL;

    *out_count = 0;

    /* the graph name is required */
    if (graph_name_agt == NULL)
    {
        ereport(ERROR,
                (errcode(ERRCODE_INVALID_PARAMETER_VALUE),
                 errmsg("%s: graph name cannot be NULL", fname)));
    }

    agtv_temp = get_agtype_value(fname, graph_name_agt,
                                 AGTV_STRING, true);
    graph_name = pnstrdup(agtv_temp->val.string.val,
                          agtv_temp->val.string.len);
    graph_oid = get_graph_oid(graph_name);

    /*
     * A NULL start or end vertex yields no rows, matching Cypher semantics
     * where a null endpoint simply produces no match (it is not an error).
     */
    if (start_agt == NULL || end_agt == NULL)
    {
        pfree_if_not_null(graph_name);
        return NULL;
    }

    source = sp_agtype_to_graphid(start_agt, fname, "start vertex");
    target = sp_agtype_to_graphid(end_agt, fname, "end vertex");

    /*
     * Optional edge type filter. A relationship type may be supplied as a
     * bare string, or one or more types may be supplied as an array of
     * strings. Each (non-empty) type name is resolved to its edge label table
     * oid; an edge is kept when its label oid is one of the requested set. An
     * empty string, an empty array, or NULL means no filter (every edge is
     * traversed). An unknown type resolves to InvalidOid and so matches no
     * edges.
     */
    if (label_agt != NULL)
    {
        char *label_name = NULL;

        if (AGT_ROOT_IS_ARRAY(label_agt) && !AGT_ROOT_IS_SCALAR(label_agt))
        {
            int nelems = AGT_ROOT_COUNT(label_agt);
            int i = 0;

            if (nelems > 0)
            {
                label_oids = palloc(sizeof(Oid) * nelems);
            }

            for (i = 0; i < nelems; i++)
            {
                agtv_temp = get_ith_agtype_value_from_container(
                    &label_agt->root, i);
                if (agtv_temp->type != AGTV_STRING)
                {
                    ereport(ERROR,
                            (errcode(ERRCODE_INVALID_PARAMETER_VALUE),
                             errmsg("%s: relationship type must be a string",
                                    fname)));
                }
                /* skip empty type names; they impose no constraint */
                if (agtv_temp->val.string.len != 0)
                {
                    label_name = pnstrdup(agtv_temp->val.string.val,
                                          agtv_temp->val.string.len);
                    label_oids[n_label_oids] =
                        get_label_relation(label_name, graph_oid);
                    n_label_oids = n_label_oids + 1;

                    /* the resolved oid is all we keep; free the type name */
                    pfree(label_name);
                    label_name = NULL;
                }
            }
        }
        else
        {
            agtv_temp = get_agtype_value(fname, label_agt,
                                         AGTV_STRING, true);
            if (agtv_temp->val.string.len != 0)
            {
                label_name = pnstrdup(agtv_temp->val.string.val,
                                      agtv_temp->val.string.len);
                label_oids = palloc(sizeof(Oid));
                label_oids[0] = get_label_relation(label_name, graph_oid);
                n_label_oids = 1;

                /* the resolved oid is all we keep; free the type name */
                pfree(label_name);
                label_name = NULL;
            }
        }
    }

    /* optional direction (defaults to undirected) */
    dir = sp_agtype_to_direction(dir_agt, fname);

    /*
     * Optional minimum hop count (NULL or negative means none). A minimum
     * that does not exceed the true shortest distance imposes no additional
     * constraint, so it is handled directly by the BFS result below. A
     * minimum greater than the shortest distance requires enumerating longer,
     * vertex-revisiting paths, which plain BFS cannot do; that case falls
     * back to the VLE depth-first engine after the search (see below).
     */
    if (minhops_agt != NULL)
    {
        agtv_temp = get_agtype_value(fname, minhops_agt,
                                     AGTV_INTEGER, true);
        min_hops = agtv_temp->val.int_value;
        if (min_hops < 0)
        {
            min_hops = 0;
        }
    }

    /* optional upper hop bound (NULL or negative means unbounded) */
    if (maxhops_agt != NULL)
    {
        agtv_temp = get_agtype_value(fname, maxhops_agt,
                                     AGTV_INTEGER, true);
        max_hops = agtv_temp->val.int_value;
        if (max_hops < 0)
        {
            max_hops = -1;
        }
    }

    /* build / fetch the global graph cache for this graph */
    ggctx = manage_GRAPH_global_contexts(graph_name, graph_oid);
    if (ggctx == NULL)
    {
        pfree_if_not_null(graph_name);
        pfree_if_not_null(label_oids);
        return NULL;
    }

    /*
     * Run the search and reconstruct the result path(s) in a private scratch
     * context. The BFS bookkeeping (visited table, frontier queue, predecessor
     * multiset) and the intermediate path arrays are only needed while we
     * compute; the surviving result Datums are built in the caller's
     * (SRF-lifetime) context and copied out before the scratch context is
     * deleted. This bounds peak memory to the result set plus one search,
     * rather than retaining the whole search state for the life of the SRF.
     */
    scratch = AllocSetContextCreate(oldctx, "age shortest path scratch",
                                    ALLOCSET_DEFAULT_SIZES);
    MemoryContextSwitchTo(scratch);

    /* run the breadth-first search */
    visited = sp_run_bfs(ggctx, source, target, label_oids, n_label_oids,
                         dir, max_hops, collect_all, &target_depth, &found);

    if (!found)
    {
        MemoryContextSwitchTo(oldctx);
        MemoryContextDelete(scratch);
        pfree_if_not_null(graph_name);
        pfree_if_not_null(label_oids);
        return NULL;
    }

    /*
     * A minimum hop count greater than the true shortest distance can only be
     * satisfied by longer, vertex-revisiting paths (Neo4j's exhaustive search
     * regime). Plain BFS cannot produce those, so fall back to the VLE
     * depth-first engine for that case. When min_hops <= target_depth the
     * bound imposes no additional constraint and the shortest path(s) already
     * found are returned unchanged.
     *
     * The VLE engine matches a single edge label only, so a multi-type filter
     * combined with this regime is still unsupported.
     */
    if (min_hops > 0 && target_depth < min_hops)
    {
        Oid fallback_label_oid = InvalidOid;

        /* the BFS scratch is no longer needed; the fallback uses its own */
        MemoryContextSwitchTo(oldctx);
        MemoryContextDelete(scratch);

        if (n_label_oids > 1)
        {
            ereport(ERROR,
                    (errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
                     errmsg("%s: a minimum hop count greater than the shortest path length is not supported with multiple relationship types",
                            fname)));
        }

        if (n_label_oids == 1)
        {
            fallback_label_oid = label_oids[0];
        }

        /*
         * The fallback duplicates graph_name internally and only needs the
         * resolved label oid, so the temporaries are freed here once its
         * result is captured rather than retained for the SRF's lifetime.
         */
        {
            Datum *fb_paths;

            fb_paths = sp_minhops_fallback(ggctx, graph_oid, graph_name, fname,
                                           source, target, fallback_label_oid,
                                           dir, min_hops, max_hops, collect_all,
                                           out_count);
            pfree_if_not_null(graph_name);
            pfree_if_not_null(label_oids);
            return fb_paths;
        }
    }

    if (!collect_all)
    {
        /* reconstruct the single shortest path from the parent pointers */
        int64 alt_len = (2 * target_depth) + 1;
        graphid *alt = palloc(sizeof(graphid) * alt_len);
        int64 pos = alt_len - 1;
        graphid cur = target;

        alt[pos] = cur;
        pos = pos - 1;
        while (cur != source)
        {
            sp_visit_entry *e = NULL;

            e = (sp_visit_entry *) hash_search(visited, &cur, HASH_FIND, NULL);
            alt[pos] = e->parent_edge;
            pos = pos - 1;
            alt[pos] = e->parent_vertex;
            pos = pos - 1;
            cur = e->parent_vertex;
        }

        /* build the surviving result Datum in the caller's context */
        MemoryContextSwitchTo(oldctx);
        paths = palloc(sizeof(Datum));
        paths[0] = sp_build_path_datum(graph_oid, alt, alt_len);
        *out_count = 1;
    }
    else
    {
        /* enumerate every equal-length shortest path */
        int64 alt_len = (2 * target_depth) + 1;
        graphid *alt = palloc(sizeof(graphid) * alt_len);
        List *arrays = NIL;
        ListCell *lc = NULL;
        int64 n = 0;
        int64 idx = 0;

        sp_enumerate(visited, source, target, alt, alt_len, alt_len - 1,
                     fname, &arrays);

        n = list_length(arrays);

        /* build the surviving result Datums in the caller's context */
        MemoryContextSwitchTo(oldctx);
        paths = palloc(sizeof(Datum) * (n > 0 ? n : 1));
        foreach(lc, arrays)
        {
            graphid *a = (graphid *) lfirst(lc);

            paths[idx] = sp_build_path_datum(graph_oid, a, alt_len);
            idx = idx + 1;
        }
        *out_count = n;
    }

    /* results are copied out; drop the BFS/enumeration scratch */
    MemoryContextSwitchTo(oldctx);
    MemoryContextDelete(scratch);
    pfree_if_not_null(graph_name);
    pfree_if_not_null(label_oids);
    return paths;
}

/*
 * Shared SRF driver for age_shortest_path / age_all_shortest_paths. The first
 * call computes every result path up front and stores them; subsequent calls
 * stream them one per row.
 */
static Datum sp_srf_impl(FunctionCallInfo fcinfo, bool collect_all)
{
    FuncCallContext *funcctx = NULL;
    sp_srf_state *state = NULL;

    if (SRF_IS_FIRSTCALL())
    {
        MemoryContext oldctx;
        agtype *a_graph = NULL;
        agtype *a_start = NULL;
        agtype *a_end = NULL;
        agtype *a_label = NULL;
        agtype *a_dir = NULL;
        agtype *a_min = NULL;
        agtype *a_max = NULL;

        funcctx = SRF_FIRSTCALL_INIT();
        oldctx = MemoryContextSwitchTo(funcctx->multi_call_memory_ctx);

        /*
         * Argument order mirrors the Cypher shortestPath() pattern
         * (a)-[:type*min_hops..max_hops]->(b):
         *   0 graph, 1 start, 2 end, 3 edge_types, 4 direction,
         *   5 min_hops, 6 max_hops
         */
        a_graph = PG_ARGISNULL(0) ? NULL : AG_GET_ARG_AGTYPE_P(0);
        a_start = PG_ARGISNULL(1) ? NULL : AG_GET_ARG_AGTYPE_P(1);
        a_end = PG_ARGISNULL(2) ? NULL : AG_GET_ARG_AGTYPE_P(2);
        a_label = PG_ARGISNULL(3) ? NULL : AG_GET_ARG_AGTYPE_P(3);
        a_dir = PG_ARGISNULL(4) ? NULL : AG_GET_ARG_AGTYPE_P(4);
        a_min = PG_ARGISNULL(5) ? NULL : AG_GET_ARG_AGTYPE_P(5);
        a_max = PG_ARGISNULL(6) ? NULL : AG_GET_ARG_AGTYPE_P(6);

        /* treat an explicit agtype null the same as a SQL NULL */
        if (a_start != NULL && is_agtype_null(a_start))
        {
            a_start = NULL;
        }
        if (a_end != NULL && is_agtype_null(a_end))
        {
            a_end = NULL;
        }
        if (a_label != NULL && is_agtype_null(a_label))
        {
            a_label = NULL;
        }
        if (a_dir != NULL && is_agtype_null(a_dir))
        {
            a_dir = NULL;
        }
        if (a_min != NULL && is_agtype_null(a_min))
        {
            a_min = NULL;
        }
        if (a_max != NULL && is_agtype_null(a_max))
        {
            a_max = NULL;
        }

        state = palloc0(sizeof(sp_srf_state));
        state->next = 0;
        state->paths = sp_compute_paths(a_graph, a_start, a_end, a_label,
                                        a_dir, a_min, a_max,
                                        collect_all ? "age_all_shortest_paths"
                                                    : "age_shortest_path",
                                        collect_all, &state->npaths);
        funcctx->user_fctx = state;

        MemoryContextSwitchTo(oldctx);
    }

    funcctx = SRF_PERCALL_SETUP();
    state = (sp_srf_state *) funcctx->user_fctx;

    if (state->next < state->npaths)
    {
        Datum d = state->paths[state->next];

        state->next = state->next + 1;
        SRF_RETURN_NEXT(funcctx, d);
    }

    SRF_RETURN_DONE(funcctx);
}

/*
 * age_shortest_path(graph_name, start, end [, edge_types [, direction
 * [, min_hops [, max_hops]]]]) -> SETOF agtype
 *
 * Returns the single unweighted shortest path (as an AGTV_PATH) between the
 * start and end vertices, or no rows if unreachable.
 */
PG_FUNCTION_INFO_V1(age_shortest_path);

Datum age_shortest_path(PG_FUNCTION_ARGS)
{
    return sp_srf_impl(fcinfo, false);
}

/*
 * age_all_shortest_paths(graph_name, start, end [, edge_types [, direction
 * [, min_hops [, max_hops]]]]) -> SETOF agtype
 *
 * Returns every unweighted shortest path (one AGTV_PATH per row) between the
 * start and end vertices, i.e. all paths whose length equals the minimum hop
 * count, or no rows if unreachable.
 */
PG_FUNCTION_INFO_V1(age_all_shortest_paths);

Datum age_all_shortest_paths(PG_FUNCTION_ARGS)
{
    return sp_srf_impl(fcinfo, true);
}
