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

/*
 * Regression coverage for issue #2356:
 *   The containment (@>, <@, @>>, <<@) and key-existence (?, ?|, ?&)
 *   operators on agtype must be bound to the lightweight selectivity
 *   helpers contsel / contjoinsel during planning. Earlier PG14+
 *   branches used matchingsel / matchingjoinsel, which caused planning
 *   to invoke agtype_contains() against pg_statistic MCVs and produced
 *   a 30%+ planning-time regression on point queries (severe TPS drop
 *   reported on the PG18 branch).
 *
 *   This test pins the bindings by querying pg_operator directly. If
 *   someone re-introduces matchingsel here, the test diff is loud and
 *   precise.
 */

LOAD 'age';
SET search_path TO ag_catalog;

-- Selectivity helpers for the four containment operators.
SELECT o.oprname,
       pg_catalog.format_type(o.oprleft,  NULL) AS lhs,
       pg_catalog.format_type(o.oprright, NULL) AS rhs,
       o.oprrest::text  AS restrict_fn,
       o.oprjoin::text  AS join_fn
FROM   pg_catalog.pg_operator o
JOIN   pg_catalog.pg_namespace n ON n.oid = o.oprnamespace
WHERE  n.nspname = 'ag_catalog'
  AND  o.oprname IN ('@>', '<@', '@>>', '<<@')
ORDER  BY o.oprname, lhs, rhs;

-- Selectivity helpers for all key-existence operator overloads
-- (right-hand side may be text, text[], or agtype).
SELECT o.oprname,
       pg_catalog.format_type(o.oprleft,  NULL) AS lhs,
       pg_catalog.format_type(o.oprright, NULL) AS rhs,
       o.oprrest::text  AS restrict_fn,
       o.oprjoin::text  AS join_fn
FROM   pg_catalog.pg_operator o
JOIN   pg_catalog.pg_namespace n ON n.oid = o.oprnamespace
WHERE  n.nspname = 'ag_catalog'
  AND  o.oprname IN ('?', '?|', '?&')
ORDER  BY o.oprname, lhs, rhs;

-- Scoped guard for issue #2356: assert that none of the specific containment
-- and key-existence operators on agtype are bound to matchingsel /
-- matchingjoinsel. We deliberately limit the check to these operator names
-- (rather than every operator in ag_catalog) so unrelated operators that
-- legitimately use matchingsel for their own semantics are not affected by
-- this regression test.
SELECT COUNT(*) AS leaked_matchingsel_bindings
FROM   pg_catalog.pg_operator o
JOIN   pg_catalog.pg_namespace n ON n.oid = o.oprnamespace
WHERE  n.nspname = 'ag_catalog'
  AND  o.oprname IN ('@>', '<@', '@>>', '<<@', '?', '?|', '?&')
  AND  (o.oprrest::text  = 'matchingsel'
        OR o.oprjoin::text = 'matchingjoinsel');

-- Smoke test: each operator still works functionally. Selectivity binding
-- only affects the planner; this guards against an inadvertent operator
-- removal as part of any future cleanup.
SELECT '{"a":1,"b":2}'::agtype @>  '{"a":1}'::agtype             AS contains_yes;
SELECT '{"a":1}'::agtype       <@  '{"a":1,"b":2}'::agtype       AS contained_yes;
SELECT '{"a":{"b":1}}'::agtype @>> '{"a":{"b":1}}'::agtype       AS top_contains_yes;
SELECT '{"a":{"b":1}}'::agtype <<@ '{"a":{"b":1}}'::agtype       AS top_contained_yes;
SELECT '{"a":1}'::agtype       ?   'a'::text                     AS exists_text_yes;
SELECT '{"a":1}'::agtype       ?   '"a"'::agtype                 AS exists_agtype_yes;
SELECT '{"a":1,"b":2}'::agtype ?|  ARRAY['a','c']                AS exists_any_text_yes;
SELECT '{"a":1,"b":2}'::agtype ?|  '["a","c"]'::agtype           AS exists_any_agtype_yes;
SELECT '{"a":1,"b":2}'::agtype ?&  ARRAY['a','b']                AS exists_all_text_yes;
SELECT '{"a":1,"b":2}'::agtype ?&  '["a","b"]'::agtype           AS exists_all_agtype_yes;

-- Upgrade-path assertion for issue #2356.
--
-- The checks above cover a FRESH install: contsel / contjoinsel come straight
-- from agtype_operators.sql and agtype_exists.sql. Existing installs instead
-- pick up the fix from the ALTER OPERATOR ... SET (RESTRICT, JOIN) block that
-- age--1.7.0--y.y.y.sql ships and "ALTER EXTENSION age UPDATE" replays. Nothing
-- above exercises that block, so a silent regression in it would go unnoticed.
--
-- We replay the shipped ALTER OPERATOR statements directly rather than running
-- ALTER EXTENSION age UPDATE: the dev upgrade script targets the placeholder
-- version "y.y.y" and is not a stable version-chain target inside the
-- regression harness. The whole section runs in a transaction that is rolled
-- back, so it observes the flip without permanently mutating the operator
-- catalog (PostgreSQL DDL is transactional).
BEGIN;

-- Simulate a stale (pre-fix) install: force all ten overloads back onto
-- matchingsel / matchingjoinsel.
ALTER OPERATOR ag_catalog.@>(agtype, agtype)   SET (RESTRICT = matchingsel, JOIN = matchingjoinsel);
ALTER OPERATOR ag_catalog.<@(agtype, agtype)   SET (RESTRICT = matchingsel, JOIN = matchingjoinsel);
ALTER OPERATOR ag_catalog.@>>(agtype, agtype)  SET (RESTRICT = matchingsel, JOIN = matchingjoinsel);
ALTER OPERATOR ag_catalog.<<@(agtype, agtype)  SET (RESTRICT = matchingsel, JOIN = matchingjoinsel);
ALTER OPERATOR ag_catalog.?(agtype, text)      SET (RESTRICT = matchingsel, JOIN = matchingjoinsel);
ALTER OPERATOR ag_catalog.?(agtype, agtype)    SET (RESTRICT = matchingsel, JOIN = matchingjoinsel);
ALTER OPERATOR ag_catalog.?|(agtype, text[])   SET (RESTRICT = matchingsel, JOIN = matchingjoinsel);
ALTER OPERATOR ag_catalog.?|(agtype, agtype)   SET (RESTRICT = matchingsel, JOIN = matchingjoinsel);
ALTER OPERATOR ag_catalog.?&(agtype, text[])   SET (RESTRICT = matchingsel, JOIN = matchingjoinsel);
ALTER OPERATOR ag_catalog.?&(agtype, agtype)   SET (RESTRICT = matchingsel, JOIN = matchingjoinsel);

-- Stale state: every overload now reports matchingsel / matchingjoinsel.
SELECT o.oprname,
       pg_catalog.format_type(o.oprleft,  NULL) AS lhs,
       pg_catalog.format_type(o.oprright, NULL) AS rhs,
       o.oprrest::text  AS restrict_fn,
       o.oprjoin::text  AS join_fn
FROM   pg_catalog.pg_operator o
JOIN   pg_catalog.pg_namespace n ON n.oid = o.oprnamespace
WHERE  n.nspname = 'ag_catalog'
  AND  o.oprname IN ('@>', '<@', '@>>', '<<@', '?', '?|', '?&')
ORDER  BY o.oprname, lhs, rhs;

-- Replay the exact ALTER OPERATOR block shipped in age--1.7.0--y.y.y.sql.
ALTER OPERATOR ag_catalog.@>(agtype, agtype)   SET (RESTRICT = contsel, JOIN = contjoinsel);
ALTER OPERATOR ag_catalog.<@(agtype, agtype)   SET (RESTRICT = contsel, JOIN = contjoinsel);
ALTER OPERATOR ag_catalog.@>>(agtype, agtype)  SET (RESTRICT = contsel, JOIN = contjoinsel);
ALTER OPERATOR ag_catalog.<<@(agtype, agtype)  SET (RESTRICT = contsel, JOIN = contjoinsel);
ALTER OPERATOR ag_catalog.?(agtype, text)      SET (RESTRICT = contsel, JOIN = contjoinsel);
ALTER OPERATOR ag_catalog.?(agtype, agtype)    SET (RESTRICT = contsel, JOIN = contjoinsel);
ALTER OPERATOR ag_catalog.?|(agtype, text[])   SET (RESTRICT = contsel, JOIN = contjoinsel);
ALTER OPERATOR ag_catalog.?|(agtype, agtype)   SET (RESTRICT = contsel, JOIN = contjoinsel);
ALTER OPERATOR ag_catalog.?&(agtype, text[])   SET (RESTRICT = contsel, JOIN = contjoinsel);
ALTER OPERATOR ag_catalog.?&(agtype, agtype)   SET (RESTRICT = contsel, JOIN = contjoinsel);

-- After the upgrade replay every overload is back on contsel / contjoinsel.
SELECT o.oprname,
       pg_catalog.format_type(o.oprleft,  NULL) AS lhs,
       pg_catalog.format_type(o.oprright, NULL) AS rhs,
       o.oprrest::text  AS restrict_fn,
       o.oprjoin::text  AS join_fn
FROM   pg_catalog.pg_operator o
JOIN   pg_catalog.pg_namespace n ON n.oid = o.oprnamespace
WHERE  n.nspname = 'ag_catalog'
  AND  o.oprname IN ('@>', '<@', '@>>', '<<@', '?', '?|', '?&')
ORDER  BY o.oprname, lhs, rhs;

ROLLBACK;
