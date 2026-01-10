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
-- graph id conversion function
--
CREATE FUNCTION ag_catalog.graphid_to_agtype(graphid)
    RETURNS agtype
    LANGUAGE c
    IMMUTABLE
RETURNS NULL ON NULL INPUT
PARALLEL SAFE
AS 'MODULE_PATHNAME';

CREATE CAST (graphid AS agtype)
    WITH FUNCTION ag_catalog.graphid_to_agtype(graphid);

CREATE FUNCTION ag_catalog.agtype_to_graphid(agtype)
    RETURNS graphid
    LANGUAGE c
    IMMUTABLE
RETURNS NULL ON NULL INPUT
PARALLEL SAFE
AS 'MODULE_PATHNAME';

CREATE CAST (agtype AS graphid)
    WITH FUNCTION ag_catalog.agtype_to_graphid(agtype)
AS IMPLICIT;

--
-- Composite types for vertex and edge
--
CREATE TYPE ag_catalog.vertex AS (
    id graphid,
    label agtype,
    properties agtype
);

CREATE TYPE ag_catalog.edge AS (
    id graphid,
    label agtype,
    start_id graphid,
    end_id graphid,
    properties agtype
);

--
-- Label name function
--
CREATE FUNCTION ag_catalog._label_name(graph_oid oid, graphid)
    RETURNS agtype
    LANGUAGE c
    IMMUTABLE
PARALLEL SAFE
AS 'MODULE_PATHNAME';

--
-- agtype - path
--
CREATE FUNCTION ag_catalog._agtype_build_path(VARIADIC "any")
    RETURNS agtype
    LANGUAGE c
    IMMUTABLE
CALLED ON NULL INPUT
PARALLEL SAFE
AS 'MODULE_PATHNAME';

--
-- agtype - vertex
--
CREATE FUNCTION ag_catalog._agtype_build_vertex(graphid, agtype, agtype)
    RETURNS agtype
    LANGUAGE c
    IMMUTABLE
CALLED ON NULL INPUT
PARALLEL SAFE
AS 'MODULE_PATHNAME';

--
-- agtype - edge
--
CREATE FUNCTION ag_catalog._agtype_build_edge(graphid, graphid, graphid,
                                              agtype, agtype)
    RETURNS agtype
    LANGUAGE c
    IMMUTABLE
CALLED ON NULL INPUT
PARALLEL SAFE
AS 'MODULE_PATHNAME';

--
-- vertex/edge to agtype cast functions
--
CREATE FUNCTION ag_catalog.vertex_to_agtype(vertex)
    RETURNS agtype
    LANGUAGE c
    IMMUTABLE
RETURNS NULL ON NULL INPUT
PARALLEL SAFE
AS 'MODULE_PATHNAME';

CREATE FUNCTION ag_catalog.edge_to_agtype(edge)
    RETURNS agtype
    LANGUAGE c
    IMMUTABLE
RETURNS NULL ON NULL INPUT
PARALLEL SAFE
AS 'MODULE_PATHNAME';

--
-- Implicit casts from vertex/edge to agtype
--
CREATE CAST (vertex AS agtype)
    WITH FUNCTION ag_catalog.vertex_to_agtype(vertex)
AS IMPLICIT;

CREATE CAST (edge AS agtype)
    WITH FUNCTION ag_catalog.edge_to_agtype(edge)
AS IMPLICIT;

CREATE FUNCTION ag_catalog.vertex_to_json(vertex)
    RETURNS json
    LANGUAGE sql
    IMMUTABLE
RETURNS NULL ON NULL INPUT
PARALLEL SAFE
AS $$ SELECT ag_catalog.agtype_to_json(ag_catalog.vertex_to_agtype($1)) $$;

CREATE FUNCTION ag_catalog.edge_to_json(edge)
    RETURNS json
    LANGUAGE sql
    IMMUTABLE
RETURNS NULL ON NULL INPUT
PARALLEL SAFE
AS $$ SELECT ag_catalog.agtype_to_json(ag_catalog.edge_to_agtype($1)) $$;

CREATE CAST (vertex AS json)
    WITH FUNCTION ag_catalog.vertex_to_json(vertex);

CREATE CAST (edge AS json)
    WITH FUNCTION ag_catalog.edge_to_json(edge);

CREATE FUNCTION ag_catalog._ag_enforce_edge_uniqueness2(graphid, graphid)
    RETURNS bool
    LANGUAGE c
    STABLE
PARALLEL SAFE
as 'MODULE_PATHNAME';

CREATE FUNCTION ag_catalog._ag_enforce_edge_uniqueness3(graphid, graphid, graphid)
    RETURNS bool
    LANGUAGE c
    STABLE
PARALLEL SAFE
as 'MODULE_PATHNAME';

CREATE FUNCTION ag_catalog._ag_enforce_edge_uniqueness4(graphid, graphid, graphid, graphid)
    RETURNS bool
    LANGUAGE c
    STABLE
PARALLEL SAFE
as 'MODULE_PATHNAME';

CREATE FUNCTION ag_catalog._ag_enforce_edge_uniqueness(VARIADIC "any")
    RETURNS bool
    LANGUAGE c
    STABLE
PARALLEL SAFE
as 'MODULE_PATHNAME';

--
-- agtype - map literal (`{key: expr, ...}`)
--

CREATE FUNCTION ag_catalog.agtype_build_map(VARIADIC "any")
    RETURNS agtype
    LANGUAGE c
    IMMUTABLE
CALLED ON NULL INPUT
PARALLEL SAFE
AS 'MODULE_PATHNAME';

CREATE FUNCTION ag_catalog.agtype_build_map()
    RETURNS agtype
    LANGUAGE c
    IMMUTABLE
CALLED ON NULL INPUT
PARALLEL SAFE
AS 'MODULE_PATHNAME', 'agtype_build_map_noargs';

CREATE FUNCTION ag_catalog.agtype_build_map_nonull(VARIADIC "any")
    RETURNS agtype
    LANGUAGE c
    IMMUTABLE
CALLED ON NULL INPUT
PARALLEL SAFE
AS 'MODULE_PATHNAME';

--
-- There are times when the optimizer might eliminate
-- functions we need. Wrap the function with this to
-- prevent that from happening
--

CREATE FUNCTION ag_catalog.agtype_volatile_wrapper("any")
    RETURNS agtype
    LANGUAGE c
    VOLATILE
CALLED ON NULL INPUT
PARALLEL SAFE
AS 'MODULE_PATHNAME';

--
-- agtype - list literal (`[expr, ...]`)
--

CREATE FUNCTION ag_catalog.agtype_build_list(VARIADIC "any")
    RETURNS agtype
    LANGUAGE c
    IMMUTABLE
CALLED ON NULL INPUT
PARALLEL SAFE
AS 'MODULE_PATHNAME';

CREATE FUNCTION ag_catalog.agtype_build_list()
    RETURNS agtype
    LANGUAGE c
    IMMUTABLE
CALLED ON NULL INPUT
PARALLEL SAFE
AS 'MODULE_PATHNAME', 'agtype_build_list_noargs';

