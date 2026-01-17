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

--* This is a TEMPLATE for upgrading from the previous version of Apache AGE
--* Please adjust the below ALTER EXTENSION to reflect the -- correct version it
--* is upgrading to.

-- This will only work within a major version of PostgreSQL, not across
-- major versions.

-- complain if script is sourced in psql, rather than via CREATE EXTENSION
\echo Use "ALTER EXTENSION age UPDATE TO '1.X.0'" to load this file. \quit

--* Please add all additions, deletions, and modifications to the end of this
--* file. We need to keep the order of these changes.
--* REMOVE ALL LINES ABOVE, and this one, that start with --*

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
    end_id graphid,
    start_id graphid,
    properties agtype
);

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
    LANGUAGE c
    IMMUTABLE
RETURNS NULL ON NULL INPUT
PARALLEL SAFE
AS 'MODULE_PATHNAME';

CREATE FUNCTION ag_catalog.edge_to_json(edge)
    RETURNS json
    LANGUAGE c
    IMMUTABLE
RETURNS NULL ON NULL INPUT
PARALLEL SAFE
AS 'MODULE_PATHNAME';

CREATE CAST (vertex AS json)
    WITH FUNCTION ag_catalog.vertex_to_json(vertex);

CREATE CAST (edge AS json)
    WITH FUNCTION ag_catalog.edge_to_json(edge);

CREATE FUNCTION ag_catalog.vertex_to_jsonb(vertex)
    RETURNS jsonb
    LANGUAGE c
    IMMUTABLE
RETURNS NULL ON NULL INPUT
PARALLEL SAFE
AS 'MODULE_PATHNAME';

CREATE FUNCTION ag_catalog.edge_to_jsonb(edge)
    RETURNS jsonb
    LANGUAGE c
    IMMUTABLE
RETURNS NULL ON NULL INPUT
PARALLEL SAFE
AS 'MODULE_PATHNAME';

CREATE CAST (vertex AS jsonb)
    WITH FUNCTION ag_catalog.vertex_to_jsonb(vertex);

CREATE CAST (edge AS jsonb)
    WITH FUNCTION ag_catalog.edge_to_jsonb(edge);

--
-- Equality operators for vertex and edge (compare by id)
--
CREATE FUNCTION ag_catalog.vertex_eq(vertex, vertex)
    RETURNS boolean
    LANGUAGE sql
    IMMUTABLE
RETURNS NULL ON NULL INPUT
PARALLEL SAFE
AS $$ SELECT $1.id = $2.id $$;

CREATE OPERATOR = (
    FUNCTION = ag_catalog.vertex_eq,
    LEFTARG = vertex,
    RIGHTARG = vertex,
    COMMUTATOR = =,
    NEGATOR = <>,
    RESTRICT = eqsel,
    JOIN = eqjoinsel,
    HASHES,
    MERGES
);

CREATE FUNCTION ag_catalog.vertex_ne(vertex, vertex)
    RETURNS boolean
    LANGUAGE sql
    IMMUTABLE
RETURNS NULL ON NULL INPUT
PARALLEL SAFE
AS $$ SELECT $1.id <> $2.id $$;

CREATE OPERATOR <> (
    FUNCTION = ag_catalog.vertex_ne,
    LEFTARG = vertex,
    RIGHTARG = vertex,
    COMMUTATOR = <>,
    NEGATOR = =,
    RESTRICT = neqsel,
    JOIN = neqjoinsel
);

CREATE FUNCTION ag_catalog.edge_eq(edge, edge)
    RETURNS boolean
    LANGUAGE sql
    IMMUTABLE
RETURNS NULL ON NULL INPUT
PARALLEL SAFE
AS $$ SELECT $1.id = $2.id $$;

CREATE OPERATOR = (
    FUNCTION = ag_catalog.edge_eq,
    LEFTARG = edge,
    RIGHTARG = edge,
    COMMUTATOR = =,
    NEGATOR = <>,
    RESTRICT = eqsel,
    JOIN = eqjoinsel,
    HASHES,
    MERGES
);

CREATE FUNCTION ag_catalog.edge_ne(edge, edge)
    RETURNS boolean
    LANGUAGE sql
    IMMUTABLE
RETURNS NULL ON NULL INPUT
PARALLEL SAFE
AS $$ SELECT $1.id <> $2.id $$;

CREATE OPERATOR <> (
    FUNCTION = ag_catalog.edge_ne,
    LEFTARG = edge,
    RIGHTARG = edge,
    COMMUTATOR = <>,
    NEGATOR = =,
    RESTRICT = neqsel,
    JOIN = neqjoinsel
);

--
-- Drop and recreate _label_name with new return type (cstring -> agtype)
--
DROP FUNCTION IF EXISTS ag_catalog._label_name(oid, graphid);

CREATE FUNCTION ag_catalog._label_name(graph_oid oid, graphid)
    RETURNS agtype
    LANGUAGE c
    IMMUTABLE
PARALLEL SAFE
AS 'MODULE_PATHNAME';

--
-- Drop and recreate _agtype_build_vertex with new signature (cstring -> agtype for label)
--
DROP FUNCTION IF EXISTS ag_catalog._agtype_build_vertex(graphid, cstring, agtype);

CREATE FUNCTION ag_catalog._agtype_build_vertex(graphid, agtype, agtype)
    RETURNS agtype
    LANGUAGE c
    IMMUTABLE
CALLED ON NULL INPUT
PARALLEL SAFE
AS 'MODULE_PATHNAME';

--
-- Drop and recreate _agtype_build_edge with new signature (cstring -> agtype for label)
--
DROP FUNCTION IF EXISTS ag_catalog._agtype_build_edge(graphid, graphid, graphid, cstring, agtype);

CREATE FUNCTION ag_catalog._agtype_build_edge(graphid, graphid, graphid,
                                              agtype, agtype)
    RETURNS agtype
    LANGUAGE c
    IMMUTABLE
CALLED ON NULL INPUT
PARALLEL SAFE
AS 'MODULE_PATHNAME';

--
-- Helper function for optimized startNode/endNode
--
CREATE FUNCTION ag_catalog._get_vertex_by_graphid(text, graphid)
    RETURNS agtype
    LANGUAGE c
    STABLE
RETURNS NULL ON NULL INPUT
PARALLEL SAFE
AS 'MODULE_PATHNAME';
