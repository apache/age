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
-- graphid - int8 cross-type comparison operators
--
-- These allow efficient comparison of graphid with integer literals,
-- avoiding the need to convert to agtype for comparisons like id(v) > 0.
--

-- graphid vs int8 comparison functions
CREATE FUNCTION ag_catalog.graphid_eq_int8(graphid, int8)
    RETURNS boolean
    LANGUAGE c
    IMMUTABLE
RETURNS NULL ON NULL INPUT
PARALLEL SAFE
AS 'MODULE_PATHNAME';

CREATE FUNCTION ag_catalog.graphid_ne_int8(graphid, int8)
    RETURNS boolean
    LANGUAGE c
    IMMUTABLE
RETURNS NULL ON NULL INPUT
PARALLEL SAFE
AS 'MODULE_PATHNAME';

CREATE FUNCTION ag_catalog.graphid_lt_int8(graphid, int8)
    RETURNS boolean
    LANGUAGE c
    IMMUTABLE
RETURNS NULL ON NULL INPUT
PARALLEL SAFE
AS 'MODULE_PATHNAME';

CREATE FUNCTION ag_catalog.graphid_gt_int8(graphid, int8)
    RETURNS boolean
    LANGUAGE c
    IMMUTABLE
RETURNS NULL ON NULL INPUT
PARALLEL SAFE
AS 'MODULE_PATHNAME';

CREATE FUNCTION ag_catalog.graphid_le_int8(graphid, int8)
    RETURNS boolean
    LANGUAGE c
    IMMUTABLE
RETURNS NULL ON NULL INPUT
PARALLEL SAFE
AS 'MODULE_PATHNAME';

CREATE FUNCTION ag_catalog.graphid_ge_int8(graphid, int8)
    RETURNS boolean
    LANGUAGE c
    IMMUTABLE
RETURNS NULL ON NULL INPUT
PARALLEL SAFE
AS 'MODULE_PATHNAME';

-- int8 vs graphid comparison functions
CREATE FUNCTION ag_catalog.int8_eq_graphid(int8, graphid)
    RETURNS boolean
    LANGUAGE c
    IMMUTABLE
RETURNS NULL ON NULL INPUT
PARALLEL SAFE
AS 'MODULE_PATHNAME';

CREATE FUNCTION ag_catalog.int8_ne_graphid(int8, graphid)
    RETURNS boolean
    LANGUAGE c
    IMMUTABLE
RETURNS NULL ON NULL INPUT
PARALLEL SAFE
AS 'MODULE_PATHNAME';

CREATE FUNCTION ag_catalog.int8_lt_graphid(int8, graphid)
    RETURNS boolean
    LANGUAGE c
    IMMUTABLE
RETURNS NULL ON NULL INPUT
PARALLEL SAFE
AS 'MODULE_PATHNAME';

CREATE FUNCTION ag_catalog.int8_gt_graphid(int8, graphid)
    RETURNS boolean
    LANGUAGE c
    IMMUTABLE
RETURNS NULL ON NULL INPUT
PARALLEL SAFE
AS 'MODULE_PATHNAME';

CREATE FUNCTION ag_catalog.int8_le_graphid(int8, graphid)
    RETURNS boolean
    LANGUAGE c
    IMMUTABLE
RETURNS NULL ON NULL INPUT
PARALLEL SAFE
AS 'MODULE_PATHNAME';

CREATE FUNCTION ag_catalog.int8_ge_graphid(int8, graphid)
    RETURNS boolean
    LANGUAGE c
    IMMUTABLE
RETURNS NULL ON NULL INPUT
PARALLEL SAFE
AS 'MODULE_PATHNAME';

-- Cross-type operators: graphid vs int8
CREATE OPERATOR = (
  FUNCTION = ag_catalog.graphid_eq_int8,
  LEFTARG = graphid,
  RIGHTARG = int8,
  COMMUTATOR = =,
  NEGATOR = <>,
  RESTRICT = eqsel,
  JOIN = eqjoinsel,
  HASHES,
  MERGES
);

CREATE OPERATOR <> (
  FUNCTION = ag_catalog.graphid_ne_int8,
  LEFTARG = graphid,
  RIGHTARG = int8,
  COMMUTATOR = <>,
  NEGATOR = =,
  RESTRICT = neqsel,
  JOIN = neqjoinsel
);

CREATE OPERATOR < (
  FUNCTION = ag_catalog.graphid_lt_int8,
  LEFTARG = graphid,
  RIGHTARG = int8,
  COMMUTATOR = >,
  NEGATOR = >=,
  RESTRICT = scalarltsel,
  JOIN = scalarltjoinsel
);

CREATE OPERATOR > (
  FUNCTION = ag_catalog.graphid_gt_int8,
  LEFTARG = graphid,
  RIGHTARG = int8,
  COMMUTATOR = <,
  NEGATOR = <=,
  RESTRICT = scalargtsel,
  JOIN = scalargtjoinsel
);

CREATE OPERATOR <= (
  FUNCTION = ag_catalog.graphid_le_int8,
  LEFTARG = graphid,
  RIGHTARG = int8,
  COMMUTATOR = >=,
  NEGATOR = >,
  RESTRICT = scalarlesel,
  JOIN = scalarlejoinsel
);

CREATE OPERATOR >= (
  FUNCTION = ag_catalog.graphid_ge_int8,
  LEFTARG = graphid,
  RIGHTARG = int8,
  COMMUTATOR = <=,
  NEGATOR = <,
  RESTRICT = scalargesel,
  JOIN = scalargejoinsel
);

-- Cross-type operators: int8 vs graphid
CREATE OPERATOR = (
  FUNCTION = ag_catalog.int8_eq_graphid,
  LEFTARG = int8,
  RIGHTARG = graphid,
  COMMUTATOR = =,
  NEGATOR = <>,
  RESTRICT = eqsel,
  JOIN = eqjoinsel,
  HASHES,
  MERGES
);

CREATE OPERATOR <> (
  FUNCTION = ag_catalog.int8_ne_graphid,
  LEFTARG = int8,
  RIGHTARG = graphid,
  COMMUTATOR = <>,
  NEGATOR = =,
  RESTRICT = neqsel,
  JOIN = neqjoinsel
);

CREATE OPERATOR < (
  FUNCTION = ag_catalog.int8_lt_graphid,
  LEFTARG = int8,
  RIGHTARG = graphid,
  COMMUTATOR = >,
  NEGATOR = >=,
  RESTRICT = scalarltsel,
  JOIN = scalarltjoinsel
);

CREATE OPERATOR > (
  FUNCTION = ag_catalog.int8_gt_graphid,
  LEFTARG = int8,
  RIGHTARG = graphid,
  COMMUTATOR = <,
  NEGATOR = <=,
  RESTRICT = scalargtsel,
  JOIN = scalargtjoinsel
);

CREATE OPERATOR <= (
  FUNCTION = ag_catalog.int8_le_graphid,
  LEFTARG = int8,
  RIGHTARG = graphid,
  COMMUTATOR = >=,
  NEGATOR = >,
  RESTRICT = scalarlesel,
  JOIN = scalarlejoinsel
);

CREATE OPERATOR >= (
  FUNCTION = ag_catalog.int8_ge_graphid,
  LEFTARG = int8,
  RIGHTARG = graphid,
  COMMUTATOR = <=,
  NEGATOR = <,
  RESTRICT = scalargesel,
  JOIN = scalargejoinsel
);

-- Cross-type btree comparison support functions
CREATE FUNCTION ag_catalog.graphid_btree_cmp_int8(graphid, int8)
    RETURNS int
    LANGUAGE c
    IMMUTABLE
RETURNS NULL ON NULL INPUT
PARALLEL SAFE
AS 'MODULE_PATHNAME';

CREATE FUNCTION ag_catalog.int8_btree_cmp_graphid(int8, graphid)
    RETURNS int
    LANGUAGE c
    IMMUTABLE
RETURNS NULL ON NULL INPUT
PARALLEL SAFE
AS 'MODULE_PATHNAME';

-- Update operator class to include cross-type operators for index scans
-- We need to drop and recreate the operator class
DROP OPERATOR CLASS IF EXISTS ag_catalog.graphid_ops USING btree CASCADE;

CREATE OPERATOR CLASS ag_catalog.graphid_ops DEFAULT FOR TYPE graphid USING btree AS
  -- same-type operators (graphid vs graphid)
  OPERATOR 1 < (graphid, graphid),
  OPERATOR 2 <= (graphid, graphid),
  OPERATOR 3 = (graphid, graphid),
  OPERATOR 4 >= (graphid, graphid),
  OPERATOR 5 > (graphid, graphid),
  -- cross-type operators (graphid vs int8)
  OPERATOR 1 < (graphid, int8),
  OPERATOR 2 <= (graphid, int8),
  OPERATOR 3 = (graphid, int8),
  OPERATOR 4 >= (graphid, int8),
  OPERATOR 5 > (graphid, int8),
  -- same-type support functions
  FUNCTION 1 ag_catalog.graphid_btree_cmp (graphid, graphid),
  FUNCTION 2 ag_catalog.graphid_btree_sort (internal),
  -- cross-type support function (graphid vs int8)
  FUNCTION 1 (graphid, int8) ag_catalog.graphid_btree_cmp_int8 (graphid, int8);
