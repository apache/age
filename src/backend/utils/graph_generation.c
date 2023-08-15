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

#include "access/xact.h"
#include "access/genam.h"
#include "access/heapam.h"
#include "catalog/dependency.h"
#include "catalog/objectaddress.h"
#include "commands/defrem.h"
#include "commands/schemacmds.h"
#include "commands/tablecmds.h"
#include "fmgr.h"
#include "miscadmin.h"
#include "nodes/makefuncs.h"
#include "nodes/nodes.h"
#include "nodes/parsenodes.h"
#include "nodes/pg_list.h"
#include "nodes/value.h"
#include "parser/parser.h"
#include "utils/fmgroids.h"
#include "utils/relcache.h"
#include "utils/rel.h"

#include "catalog/ag_graph.h"
#include "catalog/ag_label.h"
#include "commands/graph_commands.h"
#include "commands/label_commands.h"
#include "utils/age_graphid_ds.h"
#include "utils/graphid.h"
#include "utils/load/age_load.h"
#include "utils/load/ag_load_edges.h"
#include "utils/load/ag_load_labels.h"


typedef struct graph_components 
{
    Oid graph_oid;
    char* graph_name;
    int32 graph_size;

    char* vertex_label;
    int32 vertex_label_id;
    agtype* vertex_properties;
    Oid vtx_seq_id;
    
    char* edge_label;
    int32 edge_label_id;
    agtype* edge_properties;
    Oid edge_seq_id;

} graph_components;


static void assert_complete_graph_null_arguments(PG_FUNCTION_ARGS);
static void process_complete_graph_arguments(PG_FUNCTION_ARGS, 
                                             graph_components* graph);
static void check_same_vertex_and_edge_label(graph_components* graph);
static void create_graph_if_not_exists(const char* graphNameStr);
static void process_vertex_label(PG_FUNCTION_ARGS, graph_components* graph);
static void create_vertex_label_if_not_exists(const char* graphNameStr, 
                                              const char* vertexLabelName);
static void create_edge_label_if_not_exists(const char* graphNameStr, 
                                       const char* edgeLabelName);
static void processLabels(const char* graphNameStr, const char* vertexNameStr, 
                          const char* edgeNameStr);

static void assert_barbell_function_args(PG_FUNCTION_ARGS);
static void process_barbell_args(PG_FUNCTION_ARGS, graph_components* graph);
static void fetch_label_ids(graph_components* graph);
static void fetch_seq_ids(graph_components* graph);
static graphid create_vertex(graph_components* graph);
static graphid connect_vertexes_by_graphid(graph_components* graph, 
                                           graphid start, 
                                           graphid end);
static void insert_bridge(graph_components* graph, graphid start, 
                          graphid end, int32 bridge_size);

/*
 * SELECT * FROM ag_catalog.create_complete_graph('graph_name',
 *                                                no_of_nodes, 
 *                                                'edge_label', 
 *                                                'node_label'=NULL);
 */
PG_FUNCTION_INFO_V1(create_complete_graph);
Datum create_complete_graph(PG_FUNCTION_ARGS)
{   
    graph_components graph;
    ListGraphId* listgraphid = NULL;

    assert_complete_graph_null_arguments(fcinfo);
    process_complete_graph_arguments(fcinfo, &graph);
    create_graph_if_not_exists(graph.graph_name);
    processLabels(graph.graph_name, graph.vertex_label, graph.edge_label);
    fetch_label_ids(&graph);
    fetch_seq_ids(&graph);

    /* Creating vertices*/
    for (int64 i=(int64)1;i<=graph.graph_size;i++)
    {   
        GraphIdNode* next_vertex = NULL;
        GraphIdNode* curr_vertex = NULL;
        graphid vertex_to_connect;
        graphid new_graphid;
        
        new_graphid = create_vertex(&graph);
        
        for(curr_vertex = peek_stack_head(listgraphid); 
            curr_vertex != NULL; 
            curr_vertex = next_vertex)
        {
            vertex_to_connect = get_graphid(curr_vertex);

            connect_vertexes_by_graphid(&graph, 
                                        new_graphid, 
                                        vertex_to_connect);
            next_vertex = next_GraphIdNode(curr_vertex);
        }
        listgraphid = append_graphid(listgraphid, new_graphid);
    }
    PG_RETURN_DATUM(GRAPHID_GET_DATUM(PEEK_GRAPHID_STACK(listgraphid)));
}


/* 
 * The barbell graph is two complete graphs connected by a bridge path
 * https://en.wikipedia.org/wiki/Barbell_graph
 * 
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
 * returns the graphid as Datum of the first created vertex
 */

PG_FUNCTION_INFO_V1(age_create_barbell_graph);

Datum age_create_barbell_graph(PG_FUNCTION_ARGS) 
{
    struct graph_components graph;
    Datum root1, root2;
    int32 bridge_size;

    assert_barbell_function_args(fcinfo);
    process_barbell_args(fcinfo, &graph);

    // create two separate complete graphs
    root1 = DirectFunctionCall4(create_complete_graph, 
                                CStringGetDatum(graph.graph_name), 
                                Int32GetDatum(graph.graph_size),
                                CStringGetDatum(graph.edge_label), 
                                CStringGetDatum(graph.vertex_label));
    root2 = DirectFunctionCall4(create_complete_graph, 
                                CStringGetDatum(graph.graph_name), 
                                Int32GetDatum(graph.graph_size),
                                CStringGetDatum(graph.edge_label), 
                                CStringGetDatum(graph.vertex_label));

    fetch_label_ids(&graph);
    fetch_seq_ids(&graph);

    // connect two vertexes with a path of n vertexes
    bridge_size = PG_GETARG_INT32(2);
    insert_bridge(&graph, DATUM_GET_GRAPHID(root1), 
                  DATUM_GET_GRAPHID(root2), bridge_size);
    
    PG_RETURN_DATUM(root1);
}


static void assert_complete_graph_null_arguments(PG_FUNCTION_ARGS)
{
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
}


static void process_complete_graph_arguments(PG_FUNCTION_ARGS, 
                                             graph_components* graph)
{
    graph->graph_name = NameStr(*(PG_GETARG_NAME(0)));
    graph->graph_size = (int64) PG_GETARG_INT64(1);
    graph->edge_label = NameStr(*(PG_GETARG_NAME(2)));
    graph->vertex_label = AG_DEFAULT_LABEL_VERTEX;
    process_vertex_label(fcinfo, graph);    
    check_same_vertex_and_edge_label(graph);
    graph->vertex_properties = create_empty_agtype();
    graph->edge_properties = create_empty_agtype();
}


static void check_same_vertex_and_edge_label(graph_components* graph) 
{
    if (strcmp(graph->vertex_label, graph->edge_label) == 0) 
    {
        ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE),
                errmsg("Vertex and edge label cannot be the same")));
    }
}


static void create_graph_if_not_exists(const char* graphNameStr) 
{
    if (!graph_exists(graphNameStr)) 
    {
        DirectFunctionCall1(create_graph, CStringGetDatum(graphNameStr));
    }
}


static void process_vertex_label(PG_FUNCTION_ARGS, graph_components* graph) 
{
    if (!PG_ARGISNULL(3)) 
    {
        graph->vertex_label = NameStr(*(PG_GETARG_NAME(3)));
    } else 
    {
        graph->vertex_label = AG_DEFAULT_LABEL_VERTEX;
    }
}


static void create_vertex_label_if_not_exists(const char* graphNameStr, 
                                              const char* vertexLabelName) 
{
    const Oid graphId = get_graph_oid(graphNameStr);
    
    if (!label_exists(vertexLabelName, graphId)) 
    {
        DirectFunctionCall2(create_vlabel, 
                            CStringGetDatum(graphNameStr), 
                            CStringGetDatum(vertexLabelName));
    }
}


static void create_edge_label_if_not_exists(const char* graphNameStr, 
                                            const char* edgeLabelName) 
{
    const Oid graphId = get_graph_oid(graphNameStr);
    
    if (!label_exists(edgeLabelName, graphId)) 
    {
        DirectFunctionCall2(create_elabel, 
                            CStringGetDatum(graphNameStr), 
                            CStringGetDatum(edgeLabelName));
    }
}


static void processLabels(const char* graphNameStr, const char* vertexNameStr,
                          const char* edgeNameStr) 
{
    create_vertex_label_if_not_exists(graphNameStr, vertexNameStr);
    create_edge_label_if_not_exists(graphNameStr, edgeNameStr);
}


static void assert_barbell_function_args(PG_FUNCTION_ARGS)
{
    if (PG_ARGISNULL(0))
    {
        ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE),
                        errmsg("Graph name cannot be NULL")));
    }
    if (PG_ARGISNULL(1) || PG_GETARG_INT32(1) < 3)
    {
        ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE),
                errmsg("Graph size cannot be NULL or lower than 3")));
    }
    if (PG_ARGISNULL(2) || PG_GETARG_INT32(2) < 0 )
    {
        ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE),
                errmsg("Bridge size cannot be NULL or lower than 0")));
    }
    if (PG_ARGISNULL(5))
    {
        ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE),
                errmsg("edge label cannot be NULL")));
    }
    if (!PG_ARGISNULL(3) && !PG_ARGISNULL(5) &&
        strcmp(NameStr(*(PG_GETARG_NAME(3))), 
               NameStr(*(PG_GETARG_NAME(5)))) == 0)
    {
        ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE),
                errmsg("vertex and edge labels cannot be the same")));
    }
}


static void process_barbell_args(PG_FUNCTION_ARGS, graph_components* graph)
{
    graph->graph_name = NameStr(*(PG_GETARG_NAME(0)));
    graph->graph_size = PG_GETARG_INT32(1);

    if (PG_ARGISNULL(3))
    {
        graph->vertex_label = AG_DEFAULT_LABEL_VERTEX;
    }
    else
    {
        graph->vertex_label = NameStr(*(PG_GETARG_NAME(3)));
    }

    if (PG_ARGISNULL(4))
    {
        graph->vertex_properties = create_empty_agtype();
    }
    else
    {
        graph->vertex_properties = (agtype*)(PG_GETARG_DATUM(4));
    }
    
    if (PG_ARGISNULL(5))
    {
        graph->edge_label = AG_DEFAULT_LABEL_EDGE;
    }
    else
    {
        graph->edge_label = NameStr(*(PG_GETARG_NAME(5)));
    }

    if (PG_ARGISNULL(6))
    {
        graph->edge_properties = create_empty_agtype();
    }
    else
    {
        graph->edge_properties = (agtype*)(PG_GETARG_DATUM(6));
    }

}


static void fetch_label_ids(graph_components* graph) 
{
    graph->graph_oid = get_graph_oid(graph->graph_name);
    graph->vertex_label_id = 
        get_label_id(graph->vertex_label, 
                     graph->graph_oid);
    graph->edge_label_id = 
        get_label_id(graph->edge_label, 
                     graph->graph_oid);
}


/*
 * This function fetches the sequence IDs for the vertex and edge labels of a 
 * graph
*/ 
static void fetch_seq_ids(graph_components *graph)
{
    // Declare pointers to cache data structures
    graph_cache_data *graph_cache;
    label_cache_data *vtx_cache;
    label_cache_data *edge_cache;

    graph_cache = search_graph_name_cache(graph->graph_name);
    vtx_cache = search_label_name_graph_cache(graph->vertex_label,
                                                graph->graph_oid);
    edge_cache = search_label_name_graph_cache(graph->edge_label,
                                                   graph->graph_oid);
    /**
     * Get the relation ID for the vertex sequence name and assign it to 
     * graph->vtx_seq_id
     */ 
    graph->vtx_seq_id =
        get_relname_relid(NameStr(vtx_cache->seq_name),
                          graph_cache->namespace);
    /**
     * Get the relation ID for the edge sequence name and assign it to 
     * graph->edge_seq_id
     */ 
    graph->edge_seq_id =
        get_relname_relid(NameStr(edge_cache->seq_name),
                          graph_cache->namespace);
}


// This function creates a new vertex in the graph with a given label and an 
// empty agtype
// It returns the graphid of the created vertex
static graphid create_vertex(graph_components *graph)
{
    int next_index;
    graphid new_graph_id;

    // Get the next value of the vertex sequence
    next_index = nextval_internal(graph->vtx_seq_id, true);
    // Make a graphid from the label id and the sequence value
    new_graph_id = make_graphid(graph->vertex_label_id,
                                next_index);
    // Insert the vertex into the graph table with an empty agtype
    insert_vertex_simple(graph->graph_oid,
                         graph->vertex_label,
                         new_graph_id,
                         graph->vertex_properties);
    // Return the graphid of the new vertex
    return new_graph_id;
}


static graphid connect_vertexes_by_graphid(graph_components* graph, 
                                           graphid out_vtx,
                                           graphid in_vtx)
{
    int nextval;
    graphid new_graphid; 

    nextval = nextval_internal(graph->edge_seq_id, true);
    new_graphid = make_graphid(graph->edge_label_id, nextval);
    
    insert_edge_simple(graph->graph_oid,
                       graph->edge_label,
                       new_graphid, out_vtx, in_vtx,
                       graph->edge_properties);
    return new_graphid;
}


static void insert_bridge(graph_components* graph, graphid beginning, 
                          graphid end, int32 bridge_size) 
{
    graphid current_graphid;
    graphid prior_graphid;

    prior_graphid = end;
    
    for (int i = 0; i<bridge_size; i++)
    {
        current_graphid = create_vertex(graph);
        connect_vertexes_by_graphid(graph, prior_graphid, current_graphid);
        prior_graphid = current_graphid;
    }
    
    // connect prior vertex to last index
    connect_vertexes_by_graphid(graph, prior_graphid, beginning);
}

