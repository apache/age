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

-- A single aggregate guard: there must be NO operator in ag_catalog whose
-- selectivity is still bound to matchingsel / matchingjoinsel. This is the
-- catch-all that keeps issue #2356 from silently regressing if a future
-- operator is added.
SELECT COUNT(*) AS leaked_matchingsel_bindings
FROM   pg_catalog.pg_operator o
JOIN   pg_catalog.pg_namespace n ON n.oid = o.oprnamespace
WHERE  n.nspname = 'ag_catalog'
  AND  (o.oprrest::text IN ('matchingsel')
        OR o.oprjoin::text IN ('matchingjoinsel'));

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
