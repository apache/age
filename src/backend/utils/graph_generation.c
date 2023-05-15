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


/* Acessing struct members from GRAPH_global_context does not work, neither the functions located
*  at age_global_graph.h. These functions and the use of the struct's fields would have helped
*  when creating the Barabasi-Albert graph due to the way which the graph is created (searching 
*  each vertex on the graph to see which one of them has the largest amount of edges related to
*  it). Therefore, some rather simplistic structures were created to fulfill this purpose.
**/

/* Auxiliary struct to store the content of the edges. */
typedef struct edge_content {
    graphid oid;
    graphid from_vertex;
    graphid to_vertex;
} edge_content;


/* Auxiliary struct to store the content of the vertices. */
typedef struct vertex_content {
    graphid oid;
    int num_edges_in;
    int num_edges_out;
    edge_content* edges_in;
    edge_content* edges_out;
} vertex_content;


/* Auxiliary struct to store content of the graph. */
typedef struct graph_content {
    int total_vertices;
    vertex_content* vertices;
} graph_content;


graph_content* new_graph_content()
{
    graph_content* new_graph = malloc(sizeof(graph_content));
    new_graph->total_vertices = 0;
    new_graph->vertices = NULL;
    return new_graph;
}


vertex_content* new_vertex_content(graphid oid)
{
    vertex_content* new_vertex = malloc(sizeof(vertex_content));
    new_vertex->oid = oid;
    new_vertex->edges_in = NULL;
    new_vertex->edges_out = NULL;
    new_vertex->num_edges_in = 0;
    new_vertex->num_edges_out = 0;
    return new_vertex;
}


edge_content* new_edge_content(graphid oid, graphid from_vertex, graphid to_vertex)
{
    edge_content* new_edge = malloc(sizeof(edge_content));
    new_edge->oid = oid;
    new_edge->from_vertex = from_vertex;
    new_edge->to_vertex = to_vertex;
    return new_edge;
}


static void content_insert_edge_in_vertex(vertex_content* vertex, edge_content* edge)
{
    if (vertex->num_edges_in == 0)
    {
        vertex->edges_in = (edge_content*) malloc(sizeof(edge_content));
        vertex->edges_in[vertex->num_edges_in] = *edge;
        vertex->num_edges_in++;
    }
    else
    {
        int new_size = vertex->num_edges_in + 1;
        int old_size = new_size - 1;
        edge_content* new_edges = (edge_content*) malloc(sizeof(edge_content) * new_size);
        memcpy(new_edges, vertex->edges_in, old_size * sizeof(edge_content));
        free(vertex->edges_in);

        vertex->edges_in = new_edges;

        vertex->edges_in[old_size] = *edge;

        vertex->num_edges_in++;
    }
}


static void content_insert_edge_out_vertex(vertex_content* vertex, edge_content* edge)
{
    if (vertex->num_edges_out == 0)
    {
        vertex->edges_out = (edge_content*) malloc(sizeof(edge_content));
        vertex->edges_out[vertex->num_edges_out] = *edge;
        vertex->num_edges_out++;
    }
    else
    {
        int new_size = vertex->num_edges_out + 1;
        int old_size = new_size - 1;
        edge_content* new_edges = (edge_content*) malloc(sizeof(edge_content) * new_size);
        memcpy(new_edges, vertex->edges_out, old_size * sizeof(edge_content));
        free(vertex->edges_out);

        vertex->edges_out = new_edges;

        vertex->edges_out[old_size] = *edge;

        vertex->num_edges_out++;
    }
}


static void content_insert_vertex(graph_content* graph, vertex_content* vertex)
{
    int new_size = graph->total_vertices + 1;
    int old_size = new_size - 1;
    vertex_content* new_vertices = (vertex_content*) malloc(sizeof(vertex_content) * new_size);
    memcpy(new_vertices, graph->vertices, old_size * sizeof(vertex_content));
    free(graph->vertices);

    graph->vertices = new_vertices;

    graph->vertices[old_size] = *vertex;

    graph->total_vertices++;
}


static void free_graph_content(graph_content* graph)
{
    vertex_content vc;
    for (int i = 0; i < graph->total_vertices; i++)
    {
        vc = graph->vertices[i];
        free(vc.edges_in);
        free(vc.edges_out);
    }
    free(graph->vertices);
    free(graph);
}


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

PG_FUNCTION_INFO_V1(age_create_barabasi_albert_graph);
/*
* Returns a random graph using Barabási–Albert preferential attachment
*
* A graph of nodes is grown by attaching new nodes each with edges that are preferentially attached to existing nodes with high degree.
* 
* The network begins with an initial connected network of m_0 nodes.  New nodes are added to the network one at a time. Each new node is 
* connected to m <= m_0 existing nodes with a probability that is proportional to the number of links that the existing nodes already have. 
* Formally, the probability p_i that the new node is connected to node i is: (p_i = k_i / Σ k_j). Where k_i is the degree of node i and the sum
* is made over all pre-existing nodes j. 
*
* Input:
* 
* 1. graph_name - Name of the graph to be created
* 2. n - The number of vertices
* 3. m - Number of edges to attach from a new node to existing nodes
* 4. vertex_label_name - Name of the label to assign each vertex to.
* 5. edge_label_name - Name of the label to assign each edge to.
* 6. bidirectional - Bidirectional True or False. Default True.
*
*/
Datum age_create_barabasi_albert_graph(PG_FUNCTION_ARGS) 
{
    Oid graph_id;
    Oid vertex_seq_id;
    Oid edge_seq_id;
    Oid nsp_id;

    graphid object_graph_id;
    graphid current_graphid; 

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

    graph_content* graph_c;
    vertex_content* vertex_c;
    vertex_content* current_vertex_c;
    vertex_content* end_vertex = NULL;
    edge_content* edge_c;

    ListGraphId* vertex_content_analysis_list = NULL;
    GraphIdNode* list_head;

    int64 no_vertices, no_edges, eid, list_size;
    int64 vid = 1;
    int32 vertex_label_id;
    int32 edge_label_id;
    int vc_num_edges;

    float random_prob;
    srand(time(NULL));

    bool bidirectional;

    /* Get the name of the graph. */
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
    graph_c = new_graph_content();


    /* Get the number of vertices. */
	if (PG_ARGISNULL(1))
	{
		ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE),
                                errmsg("Number of vertices cannot be NULL.")));
	}
    no_vertices = PG_GETARG_INT64(1);


    /* Get the number of edges to attach from a new node to existing nodes. */
    if (PG_ARGISNULL(2))
	{
		ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE),
                                errmsg("Number of edges cannot be NULL.")));
	}
    no_edges = PG_GETARG_INT64(2);


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
    
    /* For each vertex we create. */
    for (int i = 0; i < no_vertices; i++)
    {   
        vertex_content_analysis_list = NULL;
        vid = nextval_internal(vertex_seq_id, true);
        object_graph_id = make_graphid(vertex_label_id, vid);
        props = create_empty_agtype();

        /* Insert the vertex in the auxiliary graph. */
        vertex_c = new_vertex_content(object_graph_id);
        content_insert_vertex(graph_c, vertex_c);

        /* Insert the vertex in the real graph. */
        insert_vertex_simple(graph_id, vertex_label_str, object_graph_id, props);
        
        /* Not the first vertex we create. */
        if(i > 0)
        {
            /* Iterate through the created vertices to see which are the ones with most edges. 
            *  Add it N times where N equals to the amount of edges it has. */
            for(int j = 0; j < graph_c->total_vertices; j++)
            {
                current_vertex_c = new_vertex_content(NULL);
                *current_vertex_c = graph_c->vertices[j];
                vc_num_edges = current_vertex_c->num_edges_in + current_vertex_c->num_edges_out;

                if(vc_num_edges == 0)
                {
                    vertex_content_analysis_list = append_graphid(vertex_content_analysis_list, current_vertex_c->oid);
                }
                else
                {
                    for (int num = 0; num < vc_num_edges; num++)
                    {
                        vertex_content_analysis_list = append_graphid(vertex_content_analysis_list, current_vertex_c->oid);
                    }
                } 
            }


            /* For each vertex we create to the new node, calculate the probability */
            for (int w = 0; w < no_edges; w++)
            {
                list_size = get_list_size(vertex_content_analysis_list);
                if (list_size != 0)
                {
                    random_prob = rand() % list_size;
                }
                else
                {
                    random_prob = list_size;
                }
                

                /* Iterate through the list of vertex_content until we reach the random position */
                list_head = get_list_head(vertex_content_analysis_list);
                current_graphid = get_graphid(list_head);
                for (int k = 0; k < random_prob; k++)
                {
                    current_graphid = get_graphid(list_head);
                    list_head = next_GraphIdNode(list_head);
                }

                /* Fetch the vertex_contet according to the random graphid. */
                for (int a = 0; a <= graph_c->total_vertices; a++)
                {
                    if (graph_c->vertices[a].oid == current_graphid)
                    {
                        end_vertex = &graph_c->vertices[a];
                    }
                }

                /* Create the edge id and store it in the graph and in the auxiliary vertices. */
                eid = nextval_internal(edge_seq_id, true);
                object_graph_id = make_graphid(edge_label_id, eid);

                props = create_empty_agtype();

                insert_edge_simple(graph_id, edge_label_str,
                                object_graph_id, vertex_c->oid,
                                current_graphid, props);

                edge_c = new_edge_content(object_graph_id, vertex_c->oid, current_graphid);
                content_insert_edge_in_vertex(vertex_c, edge_c);
                content_insert_edge_out_vertex(end_vertex, edge_c);
                
                if (bidirectional == true)
                {
                    eid = nextval_internal(edge_seq_id, true);
                    object_graph_id = make_graphid(edge_label_id, eid);

                    props = create_empty_agtype();

                    insert_edge_simple(graph_id, edge_label_str,
                                object_graph_id, current_graphid,
                                vertex_c->oid, props);

                    content_insert_edge_in_vertex(end_vertex, edge_c);
                    content_insert_edge_out_vertex(vertex_c, edge_c);
                }
            }
            free_graphid_stack(vertex_content_analysis_list);
        }
    }
    free_graph_content(graph_c);
    free_ListGraphId(vertex_content_analysis_list);
    PG_RETURN_VOID();
}