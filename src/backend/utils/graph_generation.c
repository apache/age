/*
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
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
#include "access/table.h"
#include "commands/graph_commands.h"
#include "nodes/makefuncs.h"
#include "utils/load/age_load.h"


int64 get_nextval_internal(graph_cache_data* graph_cache,
                           label_cache_data* label_cache);
static void queue_vertex_insert(batch_insert_state *batch_state,
                                graphid vertex_id,
                                agtype *vertex_properties);
static void queue_edge_insert(batch_insert_state *batch_state,
                              graphid edge_id, graphid start_id,
                              graphid end_id, agtype *edge_properties);
static void create_complete_graph_internal(Name graph_name, int64 no_vertices,
                                           Name edge_label_name,
                                           Name vtx_label_name);
/*
 * Auxiliary function to get the next internal value in the graph,
 * so a new object (node or edge) graph id can be composed.
 */

int64 get_nextval_internal(graph_cache_data* graph_cache,
                           label_cache_data* label_cache)
{
    Oid obj_seq_id;

    obj_seq_id = get_label_seq_relation_cached(label_cache,
                                               graph_cache->namespace);

    return nextval_internal(obj_seq_id, true);
}

static void queue_vertex_insert(batch_insert_state *batch_state,
                                graphid vertex_id,
                                agtype *vertex_properties)
{
    TupleTableSlot *slot;

    slot = batch_state->slots[batch_state->num_tuples];
    ExecClearTuple(slot);

    slot->tts_values[0] = GRAPHID_GET_DATUM(vertex_id);
    slot->tts_values[1] = AGTYPE_P_GET_DATUM(vertex_properties);
    slot->tts_isnull[0] = false;
    slot->tts_isnull[1] = false;

    ExecStoreVirtualTuple(slot);

    batch_state->buffered_bytes += VARSIZE(vertex_properties);
    batch_state->num_tuples++;

    if (batch_state->num_tuples >= BATCH_SIZE ||
        batch_state->buffered_bytes >= MAX_BUFFERED_BYTES)
    {
        insert_batch(batch_state);
        batch_state->num_tuples = 0;
        batch_state->buffered_bytes = 0;
    }
}

static void queue_edge_insert(batch_insert_state *batch_state,
                              graphid edge_id, graphid start_id,
                              graphid end_id, agtype *edge_properties)
{
    TupleTableSlot *slot;

    slot = batch_state->slots[batch_state->num_tuples];
    ExecClearTuple(slot);

    slot->tts_values[0] = GRAPHID_GET_DATUM(edge_id);
    slot->tts_values[1] = GRAPHID_GET_DATUM(start_id);
    slot->tts_values[2] = GRAPHID_GET_DATUM(end_id);
    slot->tts_values[3] = AGTYPE_P_GET_DATUM(edge_properties);
    slot->tts_isnull[0] = false;
    slot->tts_isnull[1] = false;
    slot->tts_isnull[2] = false;
    slot->tts_isnull[3] = false;

    ExecStoreVirtualTuple(slot);

    batch_state->buffered_bytes += VARSIZE(edge_properties);
    batch_state->num_tuples++;

    if (batch_state->num_tuples >= BATCH_SIZE ||
        batch_state->buffered_bytes >= MAX_BUFFERED_BYTES)
    {
        insert_batch(batch_state);
        batch_state->num_tuples = 0;
        batch_state->buffered_bytes = 0;
    }
}

PG_FUNCTION_INFO_V1(create_complete_graph);

/*
* SELECT * FROM ag_catalog.create_complete_graph('graph_name',no_of_nodes, 'edge_label', 'node_label'=NULL);
*/

static void create_complete_graph_internal(Name graph_name, int64 no_vertices,
                                           Name edge_label_name,
                                           Name vtx_label_name)
{
    Oid graph_oid;

    int64 i,j,vid = 1, eid, start_vid, end_vid;

    int32 vtx_label_id;
    int32 edge_label_id;

    agtype *props = NULL;
    graphid object_graph_id;
    graphid start_vertex_graph_id;
    graphid end_vertex_graph_id;

    Oid vtx_seq_id;
    Oid edge_seq_id;

    char* graph_name_str;
    char* vtx_name_str;
    char* edge_name_str;

    graph_cache_data *graph_cache;
    label_cache_data *vertex_cache;
    label_cache_data *edge_cache;
    batch_insert_state *vtx_batch_state = NULL;
    batch_insert_state *edge_batch_state = NULL;
    Relation vtx_rel;
    Relation edge_rel;

    Oid nsp_id;
    int64 lid;

    graph_name_str = NameStr(*graph_name);
    vtx_name_str = AG_DEFAULT_LABEL_VERTEX;
    edge_name_str = NameStr(*edge_label_name);

    if (vtx_label_name != NULL)
    {
        vtx_name_str = NameStr(*vtx_label_name);

        /* Check if vertex and edge label are same */
        if (strcmp(vtx_name_str, edge_name_str) == 0)
        {
            ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE),
                    errmsg("vertex and edge label can not be same")));
        }
    }

    graph_cache = search_graph_name_cache_cached(graph_name_str);
    if (graph_cache == NULL)
    {
        Oid created_graph_oid = create_graph_internal(graph_name);

        graph_cache = search_graph_namespace_cache_cached(created_graph_oid);
        if (graph_cache == NULL)
        {
            ereport(ERROR,
                    (errcode(ERRCODE_UNDEFINED_SCHEMA),
                     errmsg("graph \"%s\" was not found after creation",
                            graph_name_str)));
        }
        ereport(NOTICE,
                (errmsg("graph \"%s\" has been created", graph_name_str)));
    }

    graph_oid = graph_cache->oid;
    vertex_cache = search_label_name_graph_cache_cached(vtx_name_str,
                                                        graph_oid);
    if (vertex_cache == NULL)
    {
        List *parent = list_make1(makeRangeVar(
            graph_name_str, pstrdup(AG_DEFAULT_LABEL_VERTEX), -1));
        Oid label_relid = create_label_with_graph_oid(
            graph_name_str, graph_oid, vtx_name_str, LABEL_TYPE_VERTEX,
            parent);

        vertex_cache = search_label_relation_cache_cached(label_relid);
        if (vertex_cache == NULL)
        {
            ereport(ERROR,
                    (errcode(ERRCODE_UNDEFINED_TABLE),
                     errmsg("vertex label \"%s\" was not found after creation",
                            vtx_name_str)));
        }
        ereport(NOTICE,
                (errmsg("VLabel \"%s\" has been created", vtx_name_str)));
    }

    edge_cache = search_label_name_graph_cache_cached(edge_name_str, graph_oid);
    if (edge_cache == NULL)
    {
        List *parent = list_make1(makeRangeVar(
            graph_name_str, pstrdup(AG_DEFAULT_LABEL_EDGE), -1));
        Oid label_relid = create_label_with_graph_oid(
            graph_name_str, graph_oid, edge_name_str, LABEL_TYPE_EDGE,
            parent);

        edge_cache = search_label_relation_cache_cached(label_relid);
        if (edge_cache == NULL)
        {
            ereport(ERROR,
                    (errcode(ERRCODE_UNDEFINED_TABLE),
                     errmsg("edge label \"%s\" was not found after creation",
                            edge_name_str)));
        }
        ereport(NOTICE,
                (errmsg("ELabel \"%s\" has been created", edge_name_str)));
    }

    vtx_label_id = vertex_cache->id;
    edge_label_id = edge_cache->id;

    nsp_id = graph_cache->namespace;
    vtx_seq_id = get_label_seq_relation_cached(vertex_cache, nsp_id);
    edge_seq_id = get_label_seq_relation_cached(edge_cache, nsp_id);

    props = create_empty_agtype();

    vtx_rel = table_open(vertex_cache->relation, RowExclusiveLock);
    init_batch_insert(&vtx_batch_state, vertex_cache->relation);

    /* Creating vertices*/
    for (i=(int64)1; i<=no_vertices; i++)
    {
        vid = nextval_internal(vtx_seq_id, true);
        object_graph_id = make_graphid(vtx_label_id, vid);
        queue_vertex_insert(vtx_batch_state, object_graph_id, props);
    }
    finish_batch_insert(&vtx_batch_state);
    table_close(vtx_rel, RowExclusiveLock);

    lid = vid;

    edge_rel = table_open(edge_cache->relation, RowExclusiveLock);
    init_batch_insert(&edge_batch_state, edge_cache->relation);

    /* Creating edges*/
    for (i = 1; i<=no_vertices-1; i++)
    {
        start_vid = lid-no_vertices+i;
        for(j=i+1; j<=no_vertices; j++)
        {
            end_vid = lid-no_vertices+j;
            eid = nextval_internal(edge_seq_id, true);
            object_graph_id = make_graphid(edge_label_id, eid);

            start_vertex_graph_id = make_graphid(vtx_label_id, start_vid);
            end_vertex_graph_id = make_graphid(vtx_label_id, end_vid);

            queue_edge_insert(edge_batch_state, object_graph_id,
                              start_vertex_graph_id, end_vertex_graph_id,
                              props);
        }
    }
    finish_batch_insert(&edge_batch_state);
    table_close(edge_rel, RowExclusiveLock);
}

Datum create_complete_graph(PG_FUNCTION_ARGS)
{
    Name graph_name;
    Name edge_label_name;
    Name vtx_label_name = NULL;
    int64 no_vertices;

    if (PG_ARGISNULL(0))
    {
        ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE),
                        errmsg("graph name can not be NULL")));
    }

    if (PG_ARGISNULL(1))
    {
        ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE),
                        errmsg("number of nodes can not be NULL")));
    }

    if (PG_ARGISNULL(2))
    {
        ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE),
                        errmsg("edge label can not be NULL")));
    }

    graph_name = PG_GETARG_NAME(0);
    no_vertices = (int64) PG_GETARG_INT64(1);
    edge_label_name = PG_GETARG_NAME(2);

    if (!PG_ARGISNULL(3))
    {
        vtx_label_name = PG_GETARG_NAME(3);
    }

    create_complete_graph_internal(graph_name, no_vertices, edge_label_name,
                                   vtx_label_name);
    PG_RETURN_VOID();
}


PG_FUNCTION_INFO_V1(age_create_barbell_graph);

/*
 * The barbell graph is two complete graphs connected by a bridge path
 * Syntax:
 * ag_catalog.age_create_barbell_graph(graph_name Name,
 *                                     m int,
 *                                     n int,
 *                                     vertex_label_name Name DEFAULT = NULL,
 *                                     vertex_properties agtype DEFAULT = NULL,
 *                                     edge_label_name Name DEFAULT = NULL,
 *                                     edge_properties agtype DEFAULT = NULL)
 * Input:
 *
 * graph_name - Name of the graph to be created.
 * m - number of vertices in one complete graph.
 * n - number of vertices in the bridge path.
 * vertex_label_name - Name of the label to assign each vertex to.
 * vertex_properties - Property values to assign each vertex. Default is NULL
 * edge_label_name - Name of the label to assign each edge to.
 * edge_properties - Property values to assign each edge. Default is NULL
 *
 * https://en.wikipedia.org/wiki/Barbell_graph
 */

Datum age_create_barbell_graph(PG_FUNCTION_ARGS)
{
    Oid graph_oid;
    Name graph_name;
    char* graph_name_str;

    int64 graph_size;
    int64 start_node_index, end_node_index, nextval;

    Name node_label_name = NULL;
    int32 node_label_id;
    char* node_label_str;

    Name edge_label_name;
    int32 edge_label_id;
    char* edge_label_str;

    graphid object_graph_id;
    graphid start_node_graph_id;
    graphid end_node_graph_id;

    graph_cache_data* graph_cache;
    label_cache_data* node_cache;
    label_cache_data* edge_cache;

    agtype* properties = NULL;

    /* Checking for possible NULL arguments */
    /* Name graph_name */
    if (PG_ARGISNULL(0))
    {
        ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE),
                        errmsg("Graph name cannot be NULL")));
    }

    graph_name = PG_GETARG_NAME(0);
    graph_name_str = NameStr(*graph_name);

    /* int graph size (number of nodes in each complete graph) */
    if (PG_ARGISNULL(1) || PG_GETARG_INT32(1) < 3)
    {
        ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE),
                errmsg("Graph size cannot be NULL or lower than 3")));
    }
    graph_size = (int64) PG_GETARG_INT32(1);

    /*
     * int64 bridge_size: currently only stays at zero.
     * to do: implement bridge with variable number of nodes.
     */
    if (PG_ARGISNULL(2) || PG_GETARG_INT32(2) < 0 )
    {
        ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE),
                errmsg("Bridge size cannot be NULL or lower than 0")));
    }

    /* node label: if null, gets default label, which is "_ag_label_vertex" */
    if (PG_ARGISNULL(3))
    {
        node_label_str = AG_DEFAULT_LABEL_VERTEX;
    }
    else
    {
        node_label_name = PG_GETARG_NAME(3);
        node_label_str = NameStr(*node_label_name);
    }

    /* Name edge_label */
    if (PG_ARGISNULL(5))
    {
        ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE),
                errmsg("edge label can not be NULL")));
    }

    edge_label_name = PG_GETARG_NAME(5);
    edge_label_str = NameStr(*edge_label_name);


    /* create two separate complete graphs */
    create_complete_graph_internal(graph_name, graph_size, edge_label_name,
                                   node_label_name);
    create_complete_graph_internal(graph_name, graph_size, edge_label_name,
                                   node_label_name);

    /*
     * Fetching caches to get next values for graph id's, and access nodes
     * to be connected with edges.
     */
    graph_cache = search_graph_name_cache_cached(graph_name_str);
    if (graph_cache == NULL)
    {
        ereport(ERROR, (errcode(ERRCODE_UNDEFINED_SCHEMA),
                        errmsg("graph \"%s\" does not exist",
                               graph_name_str)));
    }

    graph_oid = graph_cache->oid;
    node_cache = search_label_name_graph_cache_cached(node_label_str,
                                                      graph_oid);
    if (node_cache == NULL)
    {
        ereport(ERROR,
                (errcode(ERRCODE_UNDEFINED_TABLE),
                 errmsg("vertex label \"%s\" does not exist",
                        node_label_str)));
    }

    edge_cache = search_label_name_graph_cache_cached(edge_label_str,
                                                      graph_oid);
    if (edge_cache == NULL)
    {
        ereport(ERROR,
                (errcode(ERRCODE_UNDEFINED_TABLE),
                 errmsg("edge label \"%s\" does not exist", edge_label_str)));
    }

    node_label_id = node_cache->id;
    edge_label_id = edge_cache->id;

    /* connect a node from each graph */
    /* first created node, from the first complete graph */
    start_node_index = 1;
    /* last created node, second graph */
    end_node_index = graph_size * 2;

    /* next index to be assigned to a node or edge */
    nextval = get_nextval_internal(graph_cache, edge_cache);

    /* build the graph id's of the edge to be created */
    object_graph_id = make_graphid(edge_label_id, nextval);
    start_node_graph_id = make_graphid(node_label_id, start_node_index);
    end_node_graph_id = make_graphid(node_label_id, end_node_index);
    properties = create_empty_agtype();

    /* connect two nodes */
    insert_edge_simple(graph_oid, edge_label_str,
                       object_graph_id, start_node_graph_id,
                       end_node_graph_id, properties);

    PG_RETURN_VOID();
}
