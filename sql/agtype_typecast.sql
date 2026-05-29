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

--
-- function for typecasting an agtype value to another agtype value
--
CREATE FUNCTION ag_catalog.agtype_typecast_int(variadic "any")
    RETURNS agtype
    LANGUAGE c
    IMMUTABLE
PARALLEL SAFE
AS 'MODULE_PATHNAME';

CREATE FUNCTION ag_catalog.agtype_typecast_numeric(variadic "any")
    RETURNS agtype
    LANGUAGE c
    IMMUTABLE
PARALLEL SAFE
AS 'MODULE_PATHNAME';

CREATE FUNCTION ag_catalog.agtype_typecast_float(variadic "any")
    RETURNS agtype
    LANGUAGE c
    IMMUTABLE
PARALLEL SAFE
AS 'MODULE_PATHNAME';

CREATE FUNCTION ag_catalog.agtype_typecast_bool(variadic "any")
    RETURNS agtype
    LANGUAGE c
    IMMUTABLE
PARALLEL SAFE
AS 'MODULE_PATHNAME';

CREATE FUNCTION ag_catalog.agtype_typecast_vertex(variadic "any")
    RETURNS agtype
    LANGUAGE c
    IMMUTABLE
PARALLEL SAFE
AS 'MODULE_PATHNAME';

CREATE FUNCTION ag_catalog.agtype_typecast_edge(variadic "any")
    RETURNS agtype
    LANGUAGE c
    IMMUTABLE
PARALLEL SAFE
AS 'MODULE_PATHNAME';

CREATE FUNCTION ag_catalog.agtype_typecast_path(variadic "any")
    RETURNS agtype
    LANGUAGE c
    IMMUTABLE
PARALLEL SAFE
AS 'MODULE_PATHNAME';

-- original VLE function definition
CREATE FUNCTION ag_catalog.age_vle(IN agtype, IN agtype, IN agtype, IN agtype,
                                   IN agtype, IN agtype, IN agtype,
                                   OUT edges agtype)
    RETURNS SETOF agtype
LANGUAGE C
STABLE
CALLED ON NULL INPUT
PARALLEL UNSAFE -- might be safe
AS 'MODULE_PATHNAME';

-- This is an overloaded function definition to allow for the VLE local context
-- caching mechanism to coexist with the previous VLE version.
CREATE FUNCTION ag_catalog.age_vle(IN agtype, IN agtype, IN agtype, IN agtype,
                                   IN agtype, IN agtype, IN agtype, IN agtype,
                                   OUT edges agtype)
    RETURNS SETOF agtype
LANGUAGE C
STABLE
CALLED ON NULL INPUT
PARALLEL UNSAFE -- might be safe
AS 'MODULE_PATHNAME';

-- function to build an edge for a VLE match
CREATE FUNCTION ag_catalog.age_build_vle_match_edge(agtype, agtype)
    RETURNS agtype
    LANGUAGE C
    IMMUTABLE
PARALLEL SAFE
AS 'MODULE_PATHNAME';

-- function to match a terminal vle edge
CREATE FUNCTION ag_catalog.age_match_vle_terminal_edge(variadic "any")
    RETURNS boolean
    LANGUAGE C
    STABLE
CALLED ON NULL INPUT
PARALLEL SAFE
AS 'MODULE_PATHNAME';

-- function to create an AGTV_PATH from a VLE_path_container
CREATE FUNCTION ag_catalog.age_materialize_vle_path(agtype)
    RETURNS agtype
    LANGUAGE C
    STABLE
RETURNS NULL ON NULL INPUT
PARALLEL SAFE
AS 'MODULE_PATHNAME';

-- function to read the length of a VLE_path_container without materializing it
CREATE FUNCTION ag_catalog.age_vle_path_length(agtype)
    RETURNS agtype
    LANGUAGE C
    STABLE
RETURNS NULL ON NULL INPUT
PARALLEL SAFE
AS 'MODULE_PATHNAME';

-- function to read the number of nodes in a VLE_path_container
CREATE FUNCTION ag_catalog.age_vle_path_node_count(agtype)
    RETURNS agtype
    LANGUAGE C
    STABLE
RETURNS NULL ON NULL INPUT
PARALLEL SAFE
AS 'MODULE_PATHNAME';

-- function to read the number of edges in tail(relationships) for a VLE path
CREATE FUNCTION ag_catalog.age_vle_edge_tail_count(agtype)
    RETURNS agtype
    LANGUAGE C
    STABLE
RETURNS NULL ON NULL INPUT
PARALLEL SAFE
AS 'MODULE_PATHNAME';

-- function to test VLE node/edge list emptiness without materializing lists
CREATE FUNCTION ag_catalog.age_vle_list_is_empty(agtype, agtype)
    RETURNS boolean
    LANGUAGE C
    STABLE
RETURNS NULL ON NULL INPUT
PARALLEL SAFE
AS 'MODULE_PATHNAME';

-- function to create an AGTV_ARRAY of edges from a VLE_path_container
CREATE FUNCTION ag_catalog.age_materialize_vle_edges(agtype)
    RETURNS agtype
    LANGUAGE C
    STABLE
RETURNS NULL ON NULL INPUT
PARALLEL SAFE
AS 'MODULE_PATHNAME';

-- function to create an AGTV_ARRAY of nodes from a VLE_path_container
CREATE FUNCTION ag_catalog.age_materialize_vle_nodes(agtype)
    RETURNS agtype
    LANGUAGE C
    STABLE
RETURNS NULL ON NULL INPUT
PARALLEL SAFE
AS 'MODULE_PATHNAME';

-- function to create one vertex from a VLE_path_container node index
CREATE FUNCTION ag_catalog.age_materialize_vle_node_at(agtype, agtype)
    RETURNS agtype
    LANGUAGE C
    STABLE
RETURNS NULL ON NULL INPUT
PARALLEL SAFE
AS 'MODULE_PATHNAME';

-- function to create one vertex from a reversed VLE_path_container node index
CREATE FUNCTION ag_catalog.age_materialize_vle_node_reversed_at(agtype, agtype)
    RETURNS agtype
    LANGUAGE C
    STABLE
RETURNS NULL ON NULL INPUT
PARALLEL SAFE
AS 'MODULE_PATHNAME';

-- function to create the last vertex from a VLE_path_container node-list tail
CREATE FUNCTION ag_catalog.age_materialize_vle_node_tail_last(agtype)
    RETURNS agtype
    LANGUAGE C
    STABLE
RETURNS NULL ON NULL INPUT
PARALLEL SAFE
AS 'MODULE_PATHNAME';

-- function to read the last vertex id from a VLE_path_container node-list tail
CREATE FUNCTION ag_catalog.age_vle_node_tail_last_id(agtype)
    RETURNS agtype
    LANGUAGE C
    STABLE
RETURNS NULL ON NULL INPUT
PARALLEL SAFE
AS 'MODULE_PATHNAME';

-- function to read one vertex id from a VLE_path_container node index
CREATE FUNCTION ag_catalog.age_vle_node_id_at(agtype, agtype)
    RETURNS agtype
    LANGUAGE C
    STABLE
RETURNS NULL ON NULL INPUT
PARALLEL SAFE
AS 'MODULE_PATHNAME';

-- function to read one vertex label from a VLE_path_container node index
CREATE FUNCTION ag_catalog.age_vle_node_label_at(agtype, agtype)
    RETURNS agtype
    LANGUAGE C
    STABLE
RETURNS NULL ON NULL INPUT
PARALLEL SAFE
AS 'MODULE_PATHNAME';

-- function to read one vertex labels array from a VLE_path_container node index
CREATE FUNCTION ag_catalog.age_vle_node_labels_at(agtype, agtype)
    RETURNS agtype
    LANGUAGE C
    STABLE
RETURNS NULL ON NULL INPUT
PARALLEL SAFE
AS 'MODULE_PATHNAME';

-- function to read one vertex properties object from a VLE_path_container node index
CREATE FUNCTION ag_catalog.age_vle_node_properties_at(agtype, agtype)
    RETURNS agtype
    LANGUAGE C
    STABLE
RETURNS NULL ON NULL INPUT
PARALLEL SAFE
AS 'MODULE_PATHNAME';

-- function to create a node slice from a VLE_path_container
CREATE FUNCTION ag_catalog.age_materialize_vle_nodes_slice(agtype, agtype, agtype)
    RETURNS agtype
    LANGUAGE C
    STABLE
PARALLEL SAFE
AS 'MODULE_PATHNAME';

-- function to create a VLE list slice without materializing transformed lists
CREATE FUNCTION ag_catalog.age_materialize_vle_list_slice(agtype, agtype, agtype, agtype)
    RETURNS agtype
    LANGUAGE C
    STABLE
PARALLEL SAFE
AS 'MODULE_PATHNAME';

-- function to count a VLE list slice without materializing it
CREATE FUNCTION ag_catalog.age_vle_list_slice_count(agtype, agtype, agtype, agtype)
    RETURNS agtype
    LANGUAGE C
    STABLE
PARALLEL SAFE
AS 'MODULE_PATHNAME';

-- function to check whether a VLE list slice is empty without materializing it
CREATE FUNCTION ag_catalog.age_vle_list_slice_is_empty(agtype, agtype, agtype, agtype)
    RETURNS boolean
    LANGUAGE C
    STABLE
PARALLEL SAFE
AS 'MODULE_PATHNAME';

-- function to create the head/last element of a VLE list slice
CREATE FUNCTION ag_catalog.age_materialize_vle_slice_boundary(agtype, agtype, agtype, agtype)
    RETURNS agtype
    LANGUAGE C
    STABLE
PARALLEL SAFE
AS 'MODULE_PATHNAME';

-- function to create the tail of a VLE_path_container node list
CREATE FUNCTION ag_catalog.age_materialize_vle_nodes_tail(agtype)
    RETURNS agtype
    LANGUAGE C
    STABLE
RETURNS NULL ON NULL INPUT
PARALLEL SAFE
AS 'MODULE_PATHNAME';

-- function to create a reversed VLE_path_container node list
CREATE FUNCTION ag_catalog.age_materialize_vle_nodes_reversed(agtype)
    RETURNS agtype
    LANGUAGE C
    STABLE
RETURNS NULL ON NULL INPUT
PARALLEL SAFE
AS 'MODULE_PATHNAME';

-- function to create one edge from a VLE_path_container relationship index
CREATE FUNCTION ag_catalog.age_materialize_vle_edge_at(agtype, agtype)
    RETURNS agtype
    LANGUAGE C
    STABLE
RETURNS NULL ON NULL INPUT
PARALLEL SAFE
AS 'MODULE_PATHNAME';

-- function to create one edge from a reversed VLE_path_container relationship index
CREATE FUNCTION ag_catalog.age_materialize_vle_edge_reversed_at(agtype, agtype)
    RETURNS agtype
    LANGUAGE C
    STABLE
RETURNS NULL ON NULL INPUT
PARALLEL SAFE
AS 'MODULE_PATHNAME';

-- function to create the last edge from a VLE_path_container edge-list tail
CREATE FUNCTION ag_catalog.age_materialize_vle_edge_tail_last(agtype)
    RETURNS agtype
    LANGUAGE C
    STABLE
RETURNS NULL ON NULL INPUT
PARALLEL SAFE
AS 'MODULE_PATHNAME';

-- function to read the last edge id from a VLE_path_container edge-list tail
CREATE FUNCTION ag_catalog.age_vle_edge_tail_last_id(agtype)
    RETURNS agtype
    LANGUAGE C
    STABLE
RETURNS NULL ON NULL INPUT
PARALLEL SAFE
AS 'MODULE_PATHNAME';

-- function to read one field from the last element of a VLE tail list
CREATE FUNCTION ag_catalog.age_vle_tail_last_field(agtype, agtype)
    RETURNS agtype
    LANGUAGE C
    STABLE
RETURNS NULL ON NULL INPUT
PARALLEL SAFE
AS 'MODULE_PATHNAME';

-- function to read an endpoint from the last edge of a VLE tail list
CREATE FUNCTION ag_catalog.age_vle_tail_last_edge_endpoint(agtype, agtype)
    RETURNS agtype
    LANGUAGE C
    STABLE
RETURNS NULL ON NULL INPUT
PARALLEL SAFE
AS 'MODULE_PATHNAME';

-- function to read one endpoint field from the last edge of a VLE tail list
CREATE FUNCTION ag_catalog.age_vle_tail_last_endpoint_field(agtype, agtype)
    RETURNS agtype
    LANGUAGE C
    STABLE
RETURNS NULL ON NULL INPUT
PARALLEL SAFE
AS 'MODULE_PATHNAME';

-- function to read one edge id from a VLE_path_container relationship index
CREATE FUNCTION ag_catalog.age_vle_edge_id_at(agtype, agtype)
    RETURNS agtype
    LANGUAGE C
    STABLE
RETURNS NULL ON NULL INPUT
PARALLEL SAFE
AS 'MODULE_PATHNAME';

-- function to check whether a VLE edge index resolves to an edge
CREATE FUNCTION ag_catalog.age_vle_edge_index_exists(agtype, agtype)
    RETURNS agtype
    LANGUAGE C
    STABLE
RETURNS NULL ON NULL INPUT
PARALLEL SAFE
AS 'MODULE_PATHNAME';

-- function to compare two indexed VLE edges by id
CREATE FUNCTION ag_catalog.age_vle_edge_indices_equal(agtype, agtype, agtype)
    RETURNS agtype
    LANGUAGE C
    STABLE
RETURNS NULL ON NULL INPUT
PARALLEL SAFE
AS 'MODULE_PATHNAME';

-- function to compare a reversed-indexed VLE edge with a normal indexed edge
CREATE FUNCTION ag_catalog.age_vle_edge_reversed_index_equal(agtype, agtype, agtype)
    RETURNS agtype
    LANGUAGE C
    STABLE
RETURNS NULL ON NULL INPUT
PARALLEL SAFE
AS 'MODULE_PATHNAME';

-- function to read an indexed edge's label/type
CREATE FUNCTION ag_catalog.age_vle_edge_label_at(agtype, agtype)
    RETURNS agtype
    LANGUAGE C
    STABLE
RETURNS NULL ON NULL INPUT
PARALLEL SAFE
AS 'MODULE_PATHNAME';

-- function to read an indexed edge's properties
CREATE FUNCTION ag_catalog.age_vle_edge_properties_at(agtype, agtype)
    RETURNS agtype
    LANGUAGE C
    STABLE
RETURNS NULL ON NULL INPUT
PARALLEL SAFE
AS 'MODULE_PATHNAME';

-- function to create an indexed edge's stored start vertex
CREATE FUNCTION ag_catalog.age_vle_edge_start_node_at(agtype, agtype)
    RETURNS agtype
    LANGUAGE C
    STABLE
RETURNS NULL ON NULL INPUT
PARALLEL SAFE
AS 'MODULE_PATHNAME';

-- function to create an indexed edge's stored end vertex
CREATE FUNCTION ag_catalog.age_vle_edge_end_node_at(agtype, agtype)
    RETURNS agtype
    LANGUAGE C
    STABLE
RETURNS NULL ON NULL INPUT
PARALLEL SAFE
AS 'MODULE_PATHNAME';

-- function to read one indexed edge endpoint field
CREATE FUNCTION ag_catalog.age_vle_edge_endpoint_field_at(agtype, agtype, agtype)
    RETURNS agtype
    LANGUAGE C
    STABLE
RETURNS NULL ON NULL INPUT
PARALLEL SAFE
AS 'MODULE_PATHNAME';

-- function to read an indexed edge's stored start vertex id
CREATE FUNCTION ag_catalog.age_vle_edge_start_id_at(agtype, agtype)
    RETURNS agtype
    LANGUAGE C
    STABLE
RETURNS NULL ON NULL INPUT
PARALLEL SAFE
AS 'MODULE_PATHNAME';

-- function to read an indexed edge's stored end vertex id
CREATE FUNCTION ag_catalog.age_vle_edge_end_id_at(agtype, agtype)
    RETURNS agtype
    LANGUAGE C
    STABLE
RETURNS NULL ON NULL INPUT
PARALLEL SAFE
AS 'MODULE_PATHNAME';

-- function to create the tail of a VLE_path_container edge list
CREATE FUNCTION ag_catalog.age_materialize_vle_edges_tail(agtype)
    RETURNS agtype
    LANGUAGE C
    STABLE
RETURNS NULL ON NULL INPUT
PARALLEL SAFE
AS 'MODULE_PATHNAME';

-- function to create a reversed VLE_path_container edge list
CREATE FUNCTION ag_catalog.age_materialize_vle_edges_reversed(agtype)
    RETURNS agtype
    LANGUAGE C
    STABLE
RETURNS NULL ON NULL INPUT
PARALLEL SAFE
AS 'MODULE_PATHNAME';

CREATE FUNCTION ag_catalog.age_match_vle_edge_to_id_qual(variadic "any")
    RETURNS boolean
    LANGUAGE C
    STABLE
RETURNS NULL ON NULL INPUT
PARALLEL SAFE
AS 'MODULE_PATHNAME';

CREATE FUNCTION ag_catalog.age_match_two_vle_edges(agtype, agtype)
    RETURNS boolean
    LANGUAGE C
    STABLE
RETURNS NULL ON NULL INPUT
PARALLEL SAFE
AS 'MODULE_PATHNAME';

-- list functions
CREATE FUNCTION ag_catalog.age_keys(agtype)
    RETURNS agtype
    LANGUAGE c
    STABLE
PARALLEL SAFE
AS 'MODULE_PATHNAME';

CREATE FUNCTION ag_catalog.age_labels(agtype)
    RETURNS agtype
    LANGUAGE c
    STABLE
RETURNS NULL ON NULL INPUT
PARALLEL SAFE
AS 'MODULE_PATHNAME';

CREATE FUNCTION ag_catalog.age_nodes(agtype)
    RETURNS agtype
    LANGUAGE c
    STABLE
RETURNS NULL ON NULL INPUT
PARALLEL SAFE
AS 'MODULE_PATHNAME';

CREATE FUNCTION ag_catalog.age_relationships(agtype)
    RETURNS agtype
    LANGUAGE c
    STABLE
PARALLEL SAFE
AS 'MODULE_PATHNAME';

CREATE FUNCTION ag_catalog.age_range(variadic "any")
    RETURNS agtype
    LANGUAGE c
    IMMUTABLE
PARALLEL SAFE
AS 'MODULE_PATHNAME';

CREATE FUNCTION ag_catalog.age_unnest(agtype)
    RETURNS SETOF agtype
LANGUAGE c
IMMUTABLE
PARALLEL SAFE
AS 'MODULE_PATHNAME';

CREATE FUNCTION ag_catalog.age_vertex_stats(agtype, agtype)
    RETURNS agtype
    LANGUAGE c
    STABLE
PARALLEL SAFE
AS 'MODULE_PATHNAME';

CREATE FUNCTION ag_catalog.age_graph_stats(agtype)
    RETURNS agtype
    LANGUAGE c
    STABLE
PARALLEL SAFE
AS 'MODULE_PATHNAME';

CREATE FUNCTION ag_catalog.age_delete_global_graphs(agtype)
    RETURNS boolean
    LANGUAGE c
    STABLE
PARALLEL SAFE
AS 'MODULE_PATHNAME';

CREATE FUNCTION ag_catalog.create_complete_graph(graph_name name, nodes int,
                                                 edge_label name,
                                                 node_label name = NULL)
    RETURNS void
    LANGUAGE c
    CALLED ON NULL INPUT
    PARALLEL SAFE
AS 'MODULE_PATHNAME';

CREATE FUNCTION ag_catalog.age_create_barbell_graph(graph_name name,
                                                    graph_size int,
                                                    bridge_size int,
                                                    node_label name = NULL,
                                                    node_properties agtype = NULL,
                                                    edge_label name = NULL,
                                                    edge_properties agtype = NULL)
    RETURNS void
    LANGUAGE c
    CALLED ON NULL INPUT
    PARALLEL SAFE
AS 'MODULE_PATHNAME';

CREATE FUNCTION ag_catalog.age_prepare_cypher(cstring, cstring)
    RETURNS boolean
    LANGUAGE c
    STABLE
PARALLEL SAFE
AS 'MODULE_PATHNAME';

--
-- End
--
