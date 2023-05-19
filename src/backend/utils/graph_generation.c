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
#include "utils/age_graphid_ds.h"


int64 get_nextval_internal(graph_cache_data* graph_cache, 
                           label_cache_data* label_cache);
/*
 * Auxiliary function to get the next internal value in the graph,
 * so a new object (node or edge) graph id can be composed.
 */

int64 get_nextval_internal(graph_cache_data* graph_cache, 
                           label_cache_data* label_cache) 
{
    Oid obj_seq_id;
    char* label_seq_name_str;

    label_seq_name_str = NameStr(label_cache->seq_name);
    obj_seq_id = get_relname_relid(label_seq_name_str, 
                                   graph_cache->namespace);
    
    return nextval_internal(obj_seq_id, true);
}


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
    graphid end_vertex_graph_id;

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

    props = create_empty_agtype();  
  
    /* Creating vertices*/
    for (i=(int64)1;i<=no_vertices;i++)
    {   
        vid = nextval_internal(vtx_seq_id, true);
        object_graph_id = make_graphid(vtx_label_id, vid);
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
          
            insert_edge_simple(graph_id, edge_name_str,
                            object_graph_id, start_vertex_graph_id,
                            end_vertex_graph_id, props);
        }
    }
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

Datum age_create_barbell_graph(PG_FUNCTION_ARGS) 
{
    FunctionCallInfo arguments;
    Oid graph_oid;
    Name graph_name;
    char* graph_name_str;

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
    label_cache_data* edge_cache;

    agtype* properties = NULL;

    arguments = fcinfo;

    // Checking for possible NULL arguments 
    // Name graph_name
    if (PG_ARGISNULL(0))
    {
        ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE),
                        errmsg("Graph name cannot be NULL")));
    }
    graph_name = PG_GETARG_NAME(0);
    graph_name_str = NameStr(*graph_name);

    // int graph size (number of nodes in each complete graph)
    if (PG_ARGISNULL(1) && PG_GETARG_INT32(1) < 3)
    {
        ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE),
                errmsg("Graph size cannot be NULL or lower than 3")));
    }
    
    /*
     * int64 bridge_size: currently only stays at zero.
     * to do: implement bridge with variable number of nodes.
    */ 
    if (PG_ARGISNULL(2) || PG_GETARG_INT32(2) < 0 )
    {
        ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE),
                errmsg("Bridge size cannot be NULL or lower than 0")));
    }

    // node label: if null, gets default label, which is "_ag_label_vertex"
    if (PG_ARGISNULL(3)) 
    {
        namestrcpy(node_label_name, AG_DEFAULT_LABEL_VERTEX);
    }
    else 
    {
        node_label_name = PG_GETARG_NAME(3);
    }
    node_label_str = NameStr(*node_label_name);

    /* 
    * Name edge_label 
    */
    if (PG_ARGISNULL(5))
    {
        ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE),
                errmsg("edge label can not be NULL")));
    }
    edge_label_name = PG_GETARG_NAME(5);
    edge_label_str = NameStr(*edge_label_name);


    // create two separate complete graphs
    DirectFunctionCall4(create_complete_graph, arguments->arg[0], 
                                               arguments->arg[1],
                                               arguments->arg[5], 
                                               arguments->arg[3]);
    DirectFunctionCall4(create_complete_graph, arguments->arg[0], 
                                               arguments->arg[1],
                                               arguments->arg[5], 
                                               arguments->arg[3]);

    graph_oid = get_graph_oid(graph_name_str);
    node_label_id = get_label_id(node_label_str, graph_oid);
    edge_label_id = get_label_id(edge_label_str, graph_oid);

    /*
     * Fetching caches to get next values for graph id's, and access nodes
     * to be connected with edges.
     */ 
    graph_cache = search_graph_name_cache(graph_name_str);
    edge_cache = search_label_name_graph_cache(edge_label_str,graph_oid);

    // connect a node from each graph
    start_node_index = 1; // first created node, from the first complete graph
    end_node_index = arguments->arg[1]*2; // last created node, second graph

    // next index to be assigned to a node or edge
    nextval = get_nextval_internal(graph_cache, edge_cache);

    // build the graph id's of the edge to be created
    object_graph_id = make_graphid(edge_label_id, nextval);
    start_node_graph_id = make_graphid(node_label_id, start_node_index);
    end_node_graph_id = make_graphid(node_label_id, end_node_index);
    properties = create_empty_agtype();

    // connect two nodes
    insert_edge_simple(graph_oid, edge_label_str,
                       object_graph_id, start_node_graph_id,
                       end_node_graph_id, properties);
    
    PG_RETURN_VOID();
}


PG_FUNCTION_INFO_V1(age_create_erdos_renyi_graph);
/*
    ag_catalog.age_create_erdos_renyi_graph(graph_name Name, n int, p float
                                    vertex_label_name Name DEFAULT = NULL,
                                    edge_label_name Name DEAULT = NULL,
                                    bidirectional bool DEFAULT = true)

    The Erdős–Rényi model is a random graph generation model that produces 
    graphs where each edge has a fixed probability of being present or absent, 
    independently of the other edges.
*/
Datum age_create_erdos_renyi_graph(PG_FUNCTION_ARGS)
{
    Oid graph_id;
    Oid vertex_seq_id;
    Oid edge_seq_id;
    Oid nsp_id;

    graphid object_graph_id;
    graphid current_graphid; 
    graphid* vertex_array;

    agtype *props = NULL;

	Name graph_name;
    Name vertex_label_name;
    Name vertex_seq_name;
    Name edge_label_name;
    Name edge_seq_name;

	char* graph_name_str;
    char* vertex_label_str;
    char* vertex_seq_name_str;
    char* edge_label_str;
    char* edge_seq_name_str;

    graph_cache_data *graph_cache;
    label_cache_data *vertex_cache;
    label_cache_data *edge_cache;

    int64 no_vertices, eid;
    int64 vid = 1;
    int32 vertex_label_id;
    int32 edge_label_id;

    float random_prob;
    float set_prob;
    srand(time(NULL));

    bool bidirectional;


    /* Retrieve the graph name (cannot be null). */
    if (PG_ARGISNULL(0))
    {
        ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE),
                        errmsg("Graph name cannot be NULL")));
    }
    graph_name = PG_GETARG_NAME(0);
    graph_name_str = NameStr(*graph_name);


    /* Check if graph exists. If it doesn't, create the graph. */
    if (!graph_exists(graph_name_str))
    {
        DirectFunctionCall1(create_graph, CStringGetDatum(graph_name));
    }
    graph_id = get_graph_oid(graph_name_str);


    /* Get the number of vertices. */
	if (PG_ARGISNULL(1))
	{
		ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE),
                                errmsg("Number of vertices cannot be NULL.")));
	}
    no_vertices = PG_GETARG_INT64(1);


    /* Get the probability for each edge to exist. */
    if (PG_ARGISNULL(2))
	{
		ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE),
                                errmsg("Probability cannot be NULL.")));
	}
    Name prob_name = PG_GETARG_NAME(2);
    char* prob_name_str = NameStr(*prob_name);
    set_prob = strtof(prob_name_str, NULL);


    /* Get the vertex label. */
    if (PG_ARGISNULL(3)) 
    {
        namestrcpy(vertex_label_name, AG_DEFAULT_LABEL_VERTEX);
        vertex_label_str = AG_DEFAULT_LABEL_VERTEX;
    }
    else 
    {
        vertex_label_name = PG_GETARG_NAME(3);
        vertex_label_str = NameStr(*vertex_label_name);
    }


    /* Get the edge label. */
    if (PG_ARGISNULL(4))
    {
        namestrcpy(edge_label_name, AG_DEFAULT_LABEL_EDGE);
        edge_label_str = AG_DEFAULT_LABEL_EDGE;
    }
    else
    {
        edge_label_name = PG_GETARG_NAME(4);
        edge_label_str = NameStr(*edge_label_name);
    }


    /* Compare both edge and vertex labels (they cannot be the same).*/
    if (strcmp(vertex_label_str, edge_label_str) == 0)
    {
        ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE),
                errmsg("vertex and edge label can not be same")));
    }


    /* If the vertex label does not exist, create the label. */
    if (!label_exists(vertex_label_str, graph_id))
    {
        DirectFunctionCall2(create_vlabel, CStringGetDatum(graph_name), CStringGetDatum(vertex_label_name));
    }


    /* If the edge label does not exist, create the label. */
    if (!label_exists(edge_label_str, graph_id))
    {
        DirectFunctionCall2(create_elabel, CStringGetDatum(graph_name), CStringGetDatum(edge_label_name));
    }


    /* Get the direction of the graph. */
    bidirectional = (PG_GETARG_BOOL(5) == true) ? true : false;


    vertex_label_id = get_label_id(vertex_label_str, graph_id);
    edge_label_id = get_label_id(edge_label_str, graph_id);

    graph_cache = search_graph_name_cache(graph_name_str);
    vertex_cache = search_label_name_graph_cache(vertex_label_str,graph_id);
    edge_cache = search_label_name_graph_cache(edge_label_str,graph_id);

    nsp_id = graph_cache->namespace;
    vertex_seq_name = &(vertex_cache->seq_name);
    vertex_seq_name_str = NameStr(*vertex_seq_name);

    edge_seq_name = &(edge_cache->seq_name);
    edge_seq_name_str = NameStr(*edge_seq_name);

    vertex_seq_id = get_relname_relid(vertex_seq_name_str, nsp_id);
    edge_seq_id = get_relname_relid(edge_seq_name_str, nsp_id);

    vertex_array = (graphid*) malloc(sizeof(graphid) * no_vertices);


    /* Create the vertices and add them to vertex_array */
    for (int i = 0; i < no_vertices; i++)
    {
        vid = nextval_internal(vertex_seq_id, true);
        object_graph_id = make_graphid(vertex_label_id, vid);
        props = create_empty_agtype();

        /* Insert the vertex in the graph and in the list. */
        insert_vertex_simple(graph_id, vertex_label_str, object_graph_id, props);
        vertex_array[i] = object_graph_id;
    }


    /* For each unique pair (i, j), generate a random number. If it's probability is P or grater, create
       an edge between i and j. 
     */
    for (int i = 0; i < no_vertices; i++)
    {
        /* It starts with (int j = i) because it's a combination of C(n, 2) total edges. */
        for (int j = i; j < no_vertices; j++)
        {
            /* Generate a random float number between 0 and 1. */
            random_prob = (float) ((float)rand() / (float)RAND_MAX);

            if (random_prob <= set_prob && i != j)
            {
                eid = nextval_internal(edge_seq_id, true);
                object_graph_id = make_graphid(edge_label_id, eid);

                props = create_empty_agtype();

                insert_edge_simple(graph_id, edge_label_str,
                                object_graph_id, vertex_array[i],
                                vertex_array[j], props);
                
                if (bidirectional == true)
                {
                    eid = nextval_internal(edge_seq_id, true);
                    object_graph_id = make_graphid(edge_label_id, eid);

                    props = create_empty_agtype();

                    insert_edge_simple(graph_id, edge_label_str,
                                    object_graph_id, vertex_array[j],
                                    vertex_array[i], props);
                }
            }
        }
    }


    free(vertex_array);
    PG_RETURN_VOID();
}