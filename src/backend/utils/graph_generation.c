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


static void validate_barbell_function_args(PG_FUNCTION_ARGS);
static void initialize_graph(PG_FUNCTION_ARGS, graph_components* graph);
static void fetch_label_ids(graph_components* graph);
static void fetch_seq_ids(graph_components* graph);
static graphid create_vertex(graph_components* graph);
static graphid connect_vertexes_by_graphid(graph_components* graph, 
                                           graphid start, 
                                           graphid end);
static void insert_bridge(graph_components* graph, graphid start, 
                          graphid end, int32 bridge_size);


PG_FUNCTION_INFO_V1(create_complete_graph);

/*
* SELECT * FROM ag_catalog.create_complete_graph('graph_name',no_of_nodes, 'edge_label', 'node_label'=NULL);
*/

Datum create_complete_graph(PG_FUNCTION_ARGS)
{   
    Oid graph_id;
    Name graph_name;

    int64 no_vertices;
    int64 i,j,vid = 1, eid, start_vid, end_vid;

    Name vtx_label_name = NULL;
    Name edge_label_name;
    int32 vtx_label_id;
    int32 edge_label_id;

    agtype *props = NULL;
    graphid object_graph_id;
    graphid start_vertex_graph_id;
    graphid end_vertex_graph_id = 0;

    Oid vtx_seq_id;
    Oid edge_seq_id;

    char* graph_name_str;
    char* vtx_name_str;
    char* edge_name_str;

    graph_cache_data *graph_cache;
    label_cache_data *vertex_cache;
    label_cache_data *edge_cache;

    Oid nsp_id;
    Name vtx_seq_name;
    char *vtx_seq_name_str;

    Name edge_seq_name;
    char *edge_seq_name_str;

    int64 lid; 

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
    namestrcpy(vtx_label_name, AG_DEFAULT_LABEL_VERTEX);

    graph_name_str = NameStr(*graph_name);
    vtx_name_str = AG_DEFAULT_LABEL_VERTEX;
    edge_name_str = NameStr(*edge_label_name);

    if (!PG_ARGISNULL(3))
    {
        vtx_label_name = PG_GETARG_NAME(3);
        vtx_name_str = NameStr(*vtx_label_name);
        
        // Check if vertex and edge label are same
        if (strcmp(vtx_name_str, edge_name_str) == 0)
        {
            ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE),
                    errmsg("vertex and edge label can not be same")));
        }
    }

    if (!graph_exists(graph_name_str))
    {
        DirectFunctionCall1(create_graph, CStringGetDatum(graph_name));
    }

    graph_id = get_graph_oid(graph_name_str);

    
    
    if (!PG_ARGISNULL(3))
    {
        // Check if label with the input name already exists
        if (!label_exists(vtx_name_str, graph_id))
        {
            DirectFunctionCall2(create_vlabel, CStringGetDatum(graph_name), CStringGetDatum(vtx_label_name));
        }
    }

    if (!label_exists(edge_name_str, graph_id))
    {
        DirectFunctionCall2(create_elabel, CStringGetDatum(graph_name), CStringGetDatum(edge_label_name));   
    }

    vtx_label_id = get_label_id(vtx_name_str, graph_id);
    edge_label_id = get_label_id(edge_name_str, graph_id);

    graph_cache = search_graph_name_cache(graph_name_str);
    vertex_cache = search_label_name_graph_cache(vtx_name_str,graph_id);
    edge_cache = search_label_name_graph_cache(edge_name_str,graph_id);

    nsp_id = graph_cache->namespace;
    vtx_seq_name = &(vertex_cache->seq_name);
    vtx_seq_name_str = NameStr(*vtx_seq_name);

    edge_seq_name = &(edge_cache->seq_name);
    edge_seq_name_str = NameStr(*edge_seq_name);

    vtx_seq_id = get_relname_relid(vtx_seq_name_str, nsp_id);
    edge_seq_id = get_relname_relid(edge_seq_name_str, nsp_id);

    /* Creating vertices*/
    for (i=(int64)1;i<=no_vertices;i++)
    {   
        vid = nextval_internal(vtx_seq_id, true);
        object_graph_id = make_graphid(vtx_label_id, vid);
        props = create_empty_agtype();
        insert_vertex_simple(graph_id,vtx_name_str,object_graph_id,props);
    }

    lid = vid;
    
    /* Creating edges*/
    for (i = 1;i<=no_vertices-1;i++)
    {   
        start_vid = lid-no_vertices+i;
        for(j=i+1;j<=no_vertices;j++)
        {  
            end_vid = lid-no_vertices+j;
            eid = nextval_internal(edge_seq_id, true);
            object_graph_id = make_graphid(edge_label_id, eid);

            start_vertex_graph_id = make_graphid(vtx_label_id, start_vid);
            end_vertex_graph_id = make_graphid(vtx_label_id, end_vid);

            props = create_empty_agtype();

            insert_edge_simple(graph_id, edge_name_str,
                            object_graph_id, start_vertex_graph_id,
                            end_vertex_graph_id, props);
        }
    }
    PG_RETURN_DATUM(GRAPHID_GET_DATUM(end_vertex_graph_id));
}


/* 
 * The barbell graph is two complete graphs connected by a bridge path
 * Syntax:
 * ag_catalog.age_create_barbell_graph(graph_name Name,
 *                                     m int,
 *                                     n int, 
 *                                     vertex_label_name Name DEFAULT = NULL,
 *                                     vertex_properties agtype DEFAULT = NULL,
 *                                     edge_label_name Name DEAULT = NULL,
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

PG_FUNCTION_INFO_V1(age_create_barbell_graph);

Datum age_create_barbell_graph(PG_FUNCTION_ARGS) 
{
    struct graph_components graph;
    Datum root1, root2;
    int32 bridge_size;

    validate_barbell_function_args(fcinfo);
    initialize_graph(fcinfo, &graph);

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
    bridge_size = fcinfo->arg[2];
    insert_bridge(&graph, DATUM_GET_GRAPHID(root1), 
                  DATUM_GET_GRAPHID(root2), bridge_size);
    
    PG_RETURN_DATUM(root1);
}


static void validate_barbell_function_args(PG_FUNCTION_ARGS)
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


static void initialize_graph(PG_FUNCTION_ARGS, graph_components* graph)
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


static void fetch_seq_ids(graph_components* graph)
{
    graph_cache_data* graph_cache;
    label_cache_data* vtx_cache;
    label_cache_data* edge_cache;

    graph_cache = search_graph_name_cache(graph->graph_name);
    vtx_cache = search_label_name_graph_cache(graph->vertex_label,
                                              graph->graph_oid);
    edge_cache = search_label_name_graph_cache(graph->edge_label,
                                               graph->graph_oid);

    graph->vtx_seq_id = 
        get_relname_relid(NameStr(vtx_cache->seq_name),
                          graph_cache->namespace);
    graph->edge_seq_id = 
        get_relname_relid(NameStr(edge_cache->seq_name),
                          graph_cache->namespace);
}


static graphid create_vertex(graph_components* graph)
{
    int next_index;
    graphid new_graph_id; 
    
    next_index = nextval_internal(graph->vtx_seq_id, true);
    new_graph_id = make_graphid(graph->vertex_label_id, 
                                next_index);
    insert_vertex_simple(graph->graph_oid,
                         graph->vertex_label,
                         new_graph_id,
                         create_empty_agtype());
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
                       create_empty_agtype());
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

