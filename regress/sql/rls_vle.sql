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

LOAD 'age';
SET search_path TO ag_catalog;

--
-- Row-level security enforcement during VLE / global-graph traversal
-- (controlled by age.enforce_rls_in_traversal, default on).
--
-- Chain: n1 -[LINK visible:true]-> n2 -[LINK visible:false]-> n3 -[LINK visible:true]-> n4
--

SELECT create_graph('rls_vle');

SELECT * FROM cypher('rls_vle', $$ CREATE (:Node {name: 'n1'}) $$) AS (a agtype);
SELECT * FROM cypher('rls_vle', $$ CREATE (:Node {name: 'n2'}) $$) AS (a agtype);
SELECT * FROM cypher('rls_vle', $$ CREATE (:Node {name: 'n3'}) $$) AS (a agtype);
SELECT * FROM cypher('rls_vle', $$ CREATE (:Node {name: 'n4'}) $$) AS (a agtype);

SELECT * FROM cypher('rls_vle', $$
    MATCH (a:Node {name: 'n1'}), (b:Node {name: 'n2'})
    CREATE (a)-[:LINK {visible: true}]->(b)
$$) AS (a agtype);
SELECT * FROM cypher('rls_vle', $$
    MATCH (a:Node {name: 'n2'}), (b:Node {name: 'n3'})
    CREATE (a)-[:LINK {visible: false}]->(b)
$$) AS (a agtype);
SELECT * FROM cypher('rls_vle', $$
    MATCH (a:Node {name: 'n3'}), (b:Node {name: 'n4'})
    CREATE (a)-[:LINK {visible: true}]->(b)
$$) AS (a agtype);

-- restricted role: SELECT on the graph + ag_catalog, nothing more
CREATE ROLE rls_vle_user LOGIN;
GRANT USAGE ON SCHEMA rls_vle TO rls_vle_user;
GRANT SELECT ON ALL TABLES IN SCHEMA rls_vle TO rls_vle_user;
GRANT USAGE ON SCHEMA ag_catalog TO rls_vle_user;
GRANT SELECT ON ALL TABLES IN SCHEMA ag_catalog TO rls_vle_user;

-- ============================================================================
-- PART 1: Baseline (superuser, RLS bypassed) -- VLE from n1 reaches n2, n3, n4.
-- ============================================================================
SELECT * FROM cypher('rls_vle', $$
    MATCH (a:Node {name: 'n1'})-[:LINK*1..3]->(b) RETURN b.name
$$) AS (name agtype) ORDER BY name;

-- ============================================================================
-- PART 2: Edge-label RLS -- hide edges whose "visible" property is false.
-- ============================================================================
ALTER TABLE rls_vle."LINK" ENABLE ROW LEVEL SECURITY;
ALTER TABLE rls_vle."LINK" FORCE ROW LEVEL SECURITY;
CREATE POLICY link_visible ON rls_vle."LINK"
    FOR SELECT USING ((properties->>'"visible"')::boolean = true);

-- Enforced (default): the restricted role reaches ONLY n2 (n2->n3 hidden).
SET ROLE rls_vle_user;
SHOW age.enforce_rls_in_traversal;
SELECT * FROM cypher('rls_vle', $$
    MATCH (a:Node {name: 'n1'})-[:LINK*1..3]->(b) RETURN b.name
$$) AS (name agtype) ORDER BY name;
RESET ROLE;

-- Opt-out: with enforcement off, the legacy direct scan bypasses RLS, so the
-- restricted role again reaches n2, n3, n4.
SET age.enforce_rls_in_traversal = off;
SET ROLE rls_vle_user;
SELECT * FROM cypher('rls_vle', $$
    MATCH (a:Node {name: 'n1'})-[:LINK*1..3]->(b) RETURN b.name
$$) AS (name agtype) ORDER BY name;
RESET ROLE;
RESET age.enforce_rls_in_traversal;

-- clear the edge policy for the next part
DROP POLICY link_visible ON rls_vle."LINK";
ALTER TABLE rls_vle."LINK" NO FORCE ROW LEVEL SECURITY;
ALTER TABLE rls_vle."LINK" DISABLE ROW LEVEL SECURITY;

-- ============================================================================
-- PART 3: Vertex-label RLS -- exercises the vertex loader and the dropping of
-- edges whose endpoint vertex is hidden (matches MATCH's inner-join behavior).
-- All edges are visible here; only vertices are filtered.
-- ============================================================================
ALTER TABLE rls_vle."Node" ENABLE ROW LEVEL SECURITY;
ALTER TABLE rls_vle."Node" FORCE ROW LEVEL SECURITY;

-- 3.1: hide the INTERMEDIATE vertex n2. Its edges are dropped, so the traversal
-- cannot pass through it -- expect no rows (and no "no vertex found" error).
CREATE POLICY node_visible ON rls_vle."Node"
    FOR SELECT USING ((properties->>'"name"') <> 'n2');
SET ROLE rls_vle_user;
SELECT * FROM cypher('rls_vle', $$
    MATCH (a:Node {name: 'n1'})-[:LINK*1..3]->(b) RETURN b.name
$$) AS (name agtype) ORDER BY name;
RESET ROLE;

-- 3.2: hide only the LEAF vertex n4. The edge into n4 is dropped; the traversal
-- still reaches n2 and n3.
DROP POLICY node_visible ON rls_vle."Node";
CREATE POLICY node_visible ON rls_vle."Node"
    FOR SELECT USING ((properties->>'"name"') <> 'n4');
SET ROLE rls_vle_user;
SELECT * FROM cypher('rls_vle', $$
    MATCH (a:Node {name: 'n1'})-[:LINK*1..3]->(b) RETURN b.name
$$) AS (name agtype) ORDER BY name;
RESET ROLE;

DROP POLICY node_visible ON rls_vle."Node";
ALTER TABLE rls_vle."Node" NO FORCE ROW LEVEL SECURITY;
ALTER TABLE rls_vle."Node" DISABLE ROW LEVEL SECURITY;

-- ============================================================================
-- PART 4: Per-role cache key -- two roles with different edge policies in one
-- session must see different results (the cache is rebuilt when the role
-- changes). user1 sees all edges; user2 sees none.
-- ============================================================================
CREATE ROLE rls_vle_user2 LOGIN;
GRANT USAGE ON SCHEMA rls_vle TO rls_vle_user2;
GRANT SELECT ON ALL TABLES IN SCHEMA rls_vle TO rls_vle_user2;
GRANT USAGE ON SCHEMA ag_catalog TO rls_vle_user2;
GRANT SELECT ON ALL TABLES IN SCHEMA ag_catalog TO rls_vle_user2;

ALTER TABLE rls_vle."LINK" ENABLE ROW LEVEL SECURITY;
ALTER TABLE rls_vle."LINK" FORCE ROW LEVEL SECURITY;
CREATE POLICY link_user1 ON rls_vle."LINK" FOR SELECT TO rls_vle_user  USING (true);
CREATE POLICY link_user2 ON rls_vle."LINK" FOR SELECT TO rls_vle_user2 USING (false);

-- user1: all edges visible -> n2, n3, n4
SET ROLE rls_vle_user;
SELECT * FROM cypher('rls_vle', $$
    MATCH (a:Node {name: 'n1'})-[:LINK*1..3]->(b) RETURN b.name
$$) AS (name agtype) ORDER BY name;
RESET ROLE;

-- user2: no edges visible -> empty (cache must not be reused from user1)
SET ROLE rls_vle_user2;
SELECT * FROM cypher('rls_vle', $$
    MATCH (a:Node {name: 'n1'})-[:LINK*1..3]->(b) RETURN b.name
$$) AS (name agtype) ORDER BY name;
RESET ROLE;

-- tidy up Part 4's edge policies before the next parts
DROP POLICY link_user1 ON rls_vle."LINK";
DROP POLICY link_user2 ON rls_vle."LINK";
ALTER TABLE rls_vle."LINK" NO FORCE ROW LEVEL SECURITY;
ALTER TABLE rls_vle."LINK" DISABLE ROW LEVEL SECURITY;

-- ============================================================================
-- PART 5: SELECT-ACL enforcement -- the low-level loader must not bypass table
-- privileges, not only RLS policies. rls_vle_user3 can read every label table
-- EXCEPT the edge table "LINK".
-- ============================================================================
CREATE ROLE rls_vle_user3 LOGIN;
GRANT USAGE ON SCHEMA rls_vle TO rls_vle_user3;
GRANT SELECT ON ALL TABLES IN SCHEMA rls_vle TO rls_vle_user3;
GRANT USAGE ON SCHEMA ag_catalog TO rls_vle_user3;
GRANT SELECT ON ALL TABLES IN SCHEMA ag_catalog TO rls_vle_user3;
REVOKE SELECT ON rls_vle."LINK" FROM rls_vle_user3;

-- Enforced (default): traversal fails closed with permission denied on "LINK".
-- The start-node MATCH on "Node" is allowed; only the edge load is denied.
SET ROLE rls_vle_user3;
SELECT * FROM cypher('rls_vle', $$
    MATCH (a:Node {name: 'n1'})-[:LINK*1..3]->(b) RETURN b.name
$$) AS (name agtype) ORDER BY name;
RESET ROLE;

-- Opt-out: with enforcement off, the legacy direct scan bypasses the ACL, so
-- the same role again reaches n2, n3, n4 (the legacy behavior).
SET age.enforce_rls_in_traversal = off;
SET ROLE rls_vle_user3;
SELECT * FROM cypher('rls_vle', $$
    MATCH (a:Node {name: 'n1'})-[:LINK*1..3]->(b) RETURN b.name
$$) AS (name agtype) ORDER BY name;
RESET ROLE;
RESET age.enforce_rls_in_traversal;

-- ============================================================================
-- PART 6: Session-dependent RLS policy -- a policy whose result depends on a
-- session GUC (current_setting) must be re-evaluated every statement, so an
-- RLS-enforced cache is never reused across statements even when the role, the
-- enforcement GUC, and the snapshot are unchanged.
-- ============================================================================
ALTER TABLE rls_vle."LINK" ENABLE ROW LEVEL SECURITY;
ALTER TABLE rls_vle."LINK" FORCE ROW LEVEL SECURITY;
CREATE POLICY link_by_setting ON rls_vle."LINK"
    FOR SELECT USING (current_setting('rls_vle.mode', true) = 'all');

SET ROLE rls_vle_user;
-- mode=all: every edge visible -> n2, n3, n4
SET rls_vle.mode = 'all';
SELECT * FROM cypher('rls_vle', $$
    MATCH (a:Node {name: 'n1'})-[:LINK*1..3]->(b) RETURN b.name
$$) AS (name agtype) ORDER BY name;
-- mode changed in the same session (same role / GUC / snapshot): the cache must
-- be rebuilt, so now no edge is visible -> empty.
SET rls_vle.mode = 'none';
SELECT * FROM cypher('rls_vle', $$
    MATCH (a:Node {name: 'n1'})-[:LINK*1..3]->(b) RETURN b.name
$$) AS (name agtype) ORDER BY name;
RESET rls_vle.mode;
RESET ROLE;

DROP POLICY link_by_setting ON rls_vle."LINK";
ALTER TABLE rls_vle."LINK" NO FORCE ROW LEVEL SECURITY;
ALTER TABLE rls_vle."LINK" DISABLE ROW LEVEL SECURITY;

-- ============================================================================
-- PART 7: ACL-metadata cache invalidation -- an enforced (non-RLS) direct-scan
-- cache must not survive a REVOKE on a label table. In DSM/SHMEM mode the graph
-- version fast path only tracks data changes, so the cache is invalidated via
-- catalog-invalidation callbacks; without them the REVOKE'd rows stay readable.
-- Uses a fresh graph with no RLS so the direct-scan (ACL-only) path is taken.
-- ============================================================================
SET age.enforce_rls_in_traversal = on;
SELECT create_graph('rls_acl');
SELECT * FROM cypher('rls_acl', $$
    CREATE (:Node {name: 'a'})-[:LINK]->(:Node {name: 'b'})-[:LINK]->(:Node {name: 'c'})
$$) AS (x agtype);
GRANT USAGE ON SCHEMA rls_acl TO rls_vle_user;
GRANT SELECT ON ALL TABLES IN SCHEMA rls_acl TO rls_vle_user;

-- build the enforced cache: rls_vle_user has SELECT -> reaches b, c
SET ROLE rls_vle_user;
SELECT * FROM cypher('rls_acl', $$
    MATCH (a:Node {name: 'a'})-[:LINK*1..2]->(x) RETURN x.name
$$) AS (name agtype) ORDER BY name;
RESET ROLE;

-- REVOKE SELECT on the edge label -- no graph data change, version counter same
REVOKE SELECT ON rls_acl."LINK" FROM rls_vle_user;

-- the cached traversal must now be rebuilt and denied
SET ROLE rls_vle_user;
SELECT * FROM cypher('rls_acl', $$
    MATCH (a:Node {name: 'a'})-[:LINK*1..2]->(x) RETURN x.name
$$) AS (name agtype) ORDER BY name;
RESET ROLE;

SELECT drop_graph('rls_acl', true);
RESET age.enforce_rls_in_traversal;

-- ============================================================================
-- PART 8: Traversal lifetime -- a traversal's graph context stays alive until
-- its query ends, even when another traversal in the same query must load its
-- own RLS-filtered context. Within one query an RLS-filtered context is reused
-- under the rules PostgreSQL gives a statement's scans (see 8.3 to 8.7).
-- ============================================================================
SELECT create_graph('rls_vle_chain');
SELECT * FROM cypher('rls_vle_chain', $$
    CREATE (:Node {n: 1})-[:LINK]->(:Node {n: 2})-[:LINK]->(:Node {n: 3})-[:LINK]->(:Node {n: 4})
$$) AS (x agtype);
SELECT create_graph('rls_vle_chain2');
SELECT * FROM cypher('rls_vle_chain2', $$
    CREATE (:Node {n: 1})-[:LINK]->(:Node {n: 2})-[:LINK]->(:Node {n: 3})
$$) AS (x agtype);
SELECT create_graph('rls_vle_many');
SELECT * FROM cypher('rls_vle_many', $$
    UNWIND range(1, 20) AS i
    CREATE (:Source {i: i})-[:LINK]->(:Target {i: i})
$$) AS (x agtype);
GRANT USAGE ON SCHEMA rls_vle_chain, rls_vle_chain2, rls_vle_many TO rls_vle_user;
GRANT SELECT ON ALL TABLES IN SCHEMA rls_vle_chain, rls_vle_chain2, rls_vle_many
    TO rls_vle_user;
ALTER TABLE rls_vle_chain."LINK" ENABLE ROW LEVEL SECURITY;
CREATE POLICY link_all ON rls_vle_chain."LINK" FOR SELECT USING (true);
ALTER TABLE rls_vle_chain2."LINK" ENABLE ROW LEVEL SECURITY;
CREATE POLICY link_all ON rls_vle_chain2."LINK" FOR SELECT USING (true);
ALTER TABLE rls_vle_many."LINK" ENABLE ROW LEVEL SECURITY;
CREATE POLICY link_all ON rls_vle_many."LINK" FOR SELECT USING (true);

SET ROLE rls_vle_user;
-- 8.1: two traversals advanced row by row in one target list: 6 and 20 paths
SELECT count(a) AS chain_paths, count(b) AS independent_paths
FROM (
    SELECT
        (age_vle('"rls_vle_chain"', NULL, NULL,
                 age_build_vle_match_edge('"LINK"', NULL),
                 '1', '3', '1')).start_id AS a,
        (age_vle('"rls_vle_many"', NULL, NULL,
                 age_build_vle_match_edge('"LINK"', NULL),
                 '1', '2', '1')).end_id AS b
) AS interleaved;

-- 8.2: paths of the first graph are materialized after the second graph's
-- traversal loaded its own context: 6 x 3 pairs of distinct paths
SELECT count(*) AS pairs, count(DISTINCT p) AS chain_paths,
       count(DISTINCT q) AS chain2_paths
FROM cypher('rls_vle_chain', $$
         MATCH p = (a)-[:LINK*1..3]->(b) RETURN p
     $$) AS one(p agtype),
     cypher('rls_vle_chain2', $$
         MATCH q = (a)-[:LINK*1..2]->(b) RETURN q
     $$) AS two(q agtype);
RESET ROLE;

-- 8.3: policy evaluations per query. Each traversal loads the 20 visible
-- edges once and reuses its load when it is evaluated again for another row,
-- as PostgreSQL rescans a materialized scan. Another traversal in the query
-- loads the graph again, as a separate scan would evaluate the policy again,
-- whichever functions the policy calls.
SELECT age_delete_global_graphs(NULL) IS NOT NULL AS graph_caches_cleared;
CREATE SEQUENCE public.rls_vle_policy_calls;
GRANT USAGE, SELECT ON SEQUENCE public.rls_vle_policy_calls TO rls_vle_user;
CREATE FUNCTION public.rls_vle_count_stable() RETURNS boolean
    LANGUAGE plpgsql STABLE
AS $f$
BEGIN
    PERFORM nextval('public.rls_vle_policy_calls');
    RETURN true;
END
$f$;
ALTER POLICY link_all ON rls_vle_many."LINK"
    USING (public.rls_vle_count_stable());

SET ROLE rls_vle_user;
SELECT * FROM cypher('rls_vle_many', $$
    MATCH (a:Source)-[:LINK*1..1]->(b:Target) RETURN count(*)
$$) AS (n agtype);
RESET ROLE;
SELECT last_value AS stable_one_hop_evaluations FROM public.rls_vle_policy_calls;
ALTER SEQUENCE public.rls_vle_policy_calls RESTART;

SET ROLE rls_vle_user;
SELECT * FROM cypher('rls_vle_many', $$
    MATCH (a:Source)-[:LINK*1..2]->(b:Target) RETURN count(*)
$$) AS (n agtype);
RESET ROLE;
SELECT last_value AS stable_one_to_two_hop_evaluations
FROM public.rls_vle_policy_calls;
ALTER SEQUENCE public.rls_vle_policy_calls RESTART;

SET ROLE rls_vle_user;
SELECT * FROM cypher('rls_vle_many', $$
    MATCH p = (a:Source)-[:LINK*1..1]->(b:Target) RETURN count(p)
$$) AS (n agtype);
RESET ROLE;
SELECT last_value AS stable_named_path_evaluations
FROM public.rls_vle_policy_calls;
ALTER SEQUENCE public.rls_vle_policy_calls RESTART;

SET ROLE rls_vle_user;
SELECT * FROM cypher('rls_vle_many', $$
    MATCH (a:Source)-[:LINK*1..1]->(b:Target)
    WITH count(*) AS first
    MATCH (c:Source)-[:LINK*1..1]->(d:Target)
    RETURN first, count(*)
$$) AS (first agtype, second agtype);
RESET ROLE;
SELECT last_value AS stable_two_call_site_evaluations
FROM public.rls_vle_policy_calls;

-- each traversal loaded its own context: the first one's was freed when the
-- query ended, and the second one's stays in the backend until the next
-- graph load frees it
SELECT count(*) AS contexts_after_stable_query
FROM pg_backend_memory_contexts WHERE name = 'AGE edge_table';
SELECT age_delete_global_graphs(NULL) IS NOT NULL AS graph_caches_cleared;

-- a policy that calls only IMMUTABLE functions, alone and with current_user:
-- each traversal still loads the graph. The counter's argument keeps it from
-- being evaluated once at planning.
CREATE FUNCTION public.rls_vle_count_fixed(id graphid) RETURNS boolean
    LANGUAGE plpgsql IMMUTABLE
AS $f$
BEGIN
    PERFORM nextval('public.rls_vle_policy_calls');
    RETURN true;
END
$f$;
ALTER POLICY link_all ON rls_vle_many."LINK"
    USING (public.rls_vle_count_fixed(id));
ALTER SEQUENCE public.rls_vle_policy_calls RESTART;
SET ROLE rls_vle_user;
SELECT * FROM cypher('rls_vle_many', $$
    MATCH (a:Source)-[:LINK*1..1]->(b:Target)
    WITH count(*) AS first
    MATCH (c:Source)-[:LINK*1..1]->(d:Target)
    RETURN first, count(*)
$$) AS (first agtype, second agtype);
RESET ROLE;
SELECT last_value AS immutable_two_call_site_evaluations
FROM public.rls_vle_policy_calls;

ALTER POLICY link_all ON rls_vle_many."LINK"
    USING (public.rls_vle_count_fixed(id) AND current_user = 'rls_vle_user');
ALTER SEQUENCE public.rls_vle_policy_calls RESTART;
SET ROLE rls_vle_user;
SELECT * FROM cypher('rls_vle_many', $$
    MATCH (a:Source)-[:LINK*1..1]->(b:Target)
    WITH count(*) AS first
    MATCH (c:Source)-[:LINK*1..1]->(d:Target)
    RETURN first, count(*)
$$) AS (first agtype, second agtype);
RESET ROLE;
SELECT last_value AS current_user_two_call_site_evaluations
FROM public.rls_vle_policy_calls;

-- a policy that reads the search path, and one that calls a STABLE function
-- in a subquery: each traversal loads the graph
ALTER POLICY link_all ON rls_vle_many."LINK"
    USING (public.rls_vle_count_fixed(id) AND current_schema IS NOT NULL);
ALTER SEQUENCE public.rls_vle_policy_calls RESTART;
SET ROLE rls_vle_user;
SELECT * FROM cypher('rls_vle_many', $$
    MATCH (a:Source)-[:LINK*1..1]->(b:Target)
    WITH count(*) AS first
    MATCH (c:Source)-[:LINK*1..1]->(d:Target)
    RETURN first, count(*)
$$) AS (first agtype, second agtype);
RESET ROLE;
SELECT last_value AS current_schema_two_call_site_evaluations
FROM public.rls_vle_policy_calls;

ALTER POLICY link_all ON rls_vle_many."LINK"
    USING (EXISTS (SELECT 1 WHERE id IS NOT NULL
                              AND public.rls_vle_count_stable()));
ALTER SEQUENCE public.rls_vle_policy_calls RESTART;
SET ROLE rls_vle_user;
SELECT * FROM cypher('rls_vle_many', $$
    MATCH (a:Source)-[:LINK*1..1]->(b:Target)
    WITH count(*) AS first
    MATCH (c:Source)-[:LINK*1..1]->(d:Target)
    RETURN first, count(*)
$$) AS (first agtype, second agtype);
RESET ROLE;
SELECT last_value AS stable_subquery_two_call_site_evaluations
FROM public.rls_vle_policy_calls;
SELECT age_delete_global_graphs(NULL) IS NOT NULL AS graph_caches_cleared;

ALTER POLICY link_all ON rls_vle_many."LINK"
    USING (nextval('public.rls_vle_policy_calls') > 0);
ALTER SEQUENCE public.rls_vle_policy_calls RESTART;
SET ROLE rls_vle_user;
SELECT * FROM cypher('rls_vle_many', $$
    MATCH (a:Source)-[:LINK*1..1]->(b:Target) RETURN count(*)
$$) AS (n agtype);
RESET ROLE;
SELECT last_value AS volatile_one_hop_evaluations
FROM public.rls_vle_policy_calls;
ALTER SEQUENCE public.rls_vle_policy_calls RESTART;

SET ROLE rls_vle_user;
SELECT * FROM cypher('rls_vle_many', $$
    MATCH (a:Source)-[:LINK*1..1]->(b:Target)
    WITH count(*) AS first
    MATCH (c:Source)-[:LINK*1..1]->(d:Target)
    RETURN first, count(*)
$$) AS (first agtype, second agtype);
RESET ROLE;
SELECT last_value AS volatile_two_call_site_evaluations
FROM public.rls_vle_policy_calls;

-- the first call site's context was freed when the query ended; the second
-- one is kept, as for the stable policy
SELECT count(*) AS contexts_after_volatile_query
FROM pg_backend_memory_contexts WHERE name = 'AGE edge_table';
ALTER POLICY link_all ON rls_vle_many."LINK" USING (true);

-- a policy that reads a setting with current_setting(), which the statement
-- itself changes: each traversal sets the limit through its graph argument
-- and sees its own Targets, the first those up to 5 and the second those up
-- to 10, as two scans of the label in PostgreSQL would
ALTER TABLE rls_vle_many."Target" ENABLE ROW LEVEL SECURITY;
CREATE POLICY target_limit ON rls_vle_many."Target" FOR SELECT
    USING ((properties->>'"i"')::int <=
           coalesce(nullif(current_setting('rls_vle.max_target', true), ''),
                    '0')::int);
CREATE FUNCTION public.rls_vle_limit_targets(n int) RETURNS agtype
    LANGUAGE plpgsql VOLATILE
AS $f$
BEGIN
    PERFORM set_config('rls_vle.max_target', n::text, false);
    RETURN '"rls_vle_many"'::agtype;
END
$f$;
SET ROLE rls_vle_user;
SELECT count(DISTINCT first.end_id) AS first_targets,
       count(DISTINCT second.end_id) AS second_targets
FROM age_vle(public.rls_vle_limit_targets(5), NULL, NULL,
             age_build_vle_match_edge('"LINK"', NULL),
             '1', '1', '1') AS first,
     LATERAL age_vle(public.rls_vle_limit_targets(
                         CASE WHEN first.start_id IS NOT NULL THEN 10 END),
                     NULL, NULL,
                     age_build_vle_match_edge('"LINK"', NULL),
                     '1', '1', '1') AS second;
RESET ROLE;
RESET rls_vle.max_target;
DROP POLICY target_limit ON rls_vle_many."Target";
ALTER TABLE rls_vle_many."Target" DISABLE ROW LEVEL SECURITY;
DROP FUNCTION public.rls_vle_limit_targets(int);

-- 8.4: a statement's own write is visible to its later traversal. The write
-- advances the command id, so the later traversal does not reuse the graph
-- loaded before it.
BEGIN;
GRANT INSERT ON rls_vle_many."LINK" TO rls_vle_user;
GRANT USAGE ON SEQUENCE rls_vle_many."LINK_id_seq" TO rls_vle_user;
CREATE POLICY link_insert ON rls_vle_many."LINK" FOR INSERT WITH CHECK (true);
SET LOCAL ROLE rls_vle_user;
SELECT * FROM cypher('rls_vle_many', $$
    MATCH (a:Source {i: 1})-[:LINK*1..1]->(b:Target)
    WITH a, count(b) AS before
    MATCH (t:Target {i: 2})
    CREATE (a)-[:LINK]->(t)
    WITH a, before, count(*) AS created
    MATCH (a)-[:LINK*1..1]->(c:Target)
    RETURN before, created, count(c)
$$) AS (before agtype, created agtype, after agtype);
ROLLBACK;

-- 8.5: a policy change made while a statement runs does not change what that
-- statement's traversal sees, as for a PostgreSQL scan; the next statement
-- sees it. The start vertex argument hides every edge when the traversal is
-- evaluated for the second row, after the first row loaded the graph.
CREATE FUNCTION public.rls_vle_start_hiding_links(id graphid, rn bigint)
    RETURNS ag_catalog.agtype
    LANGUAGE plpgsql SECURITY DEFINER
AS $f$
BEGIN
    IF rn = 2 THEN
        ALTER POLICY link_all ON rls_vle_many."LINK" USING (false);
    END IF;
    RETURN (id::text)::ag_catalog.agtype;
END
$f$;

SET ROLE rls_vle_user;
SELECT count(*) AS paths_with_policy_changed_midway
FROM (SELECT id, row_number() OVER (ORDER BY id) AS rn
      FROM rls_vle_many."Source") AS s,
     LATERAL age_vle('"rls_vle_many"',
                     public.rls_vle_start_hiding_links(s.id, s.rn), NULL,
                     age_build_vle_match_edge('"LINK"', NULL),
                     '1', '1', '1') AS v;
SELECT count(*) AS paths_in_next_statement
FROM rls_vle_many."Source" AS s,
     LATERAL age_vle('"rls_vle_many"', (s.id::text)::agtype, NULL,
                     age_build_vle_match_edge('"LINK"', NULL),
                     '1', '1', '1') AS v;
RESET ROLE;

-- 8.6: a graph loaded for one role is not reused for another, even within a
-- statement. The start vertex argument switches to rls_vle_user2, from whom
-- every edge is hidden, when the traversal is evaluated for the second row.
ALTER POLICY link_all ON rls_vle_many."LINK" USING (true);
GRANT USAGE ON SCHEMA rls_vle_many TO rls_vle_user2;
GRANT SELECT ON ALL TABLES IN SCHEMA rls_vle_many TO rls_vle_user2;
CREATE POLICY link_hidden_from_user2 ON rls_vle_many."LINK" AS RESTRICTIVE
    FOR SELECT TO rls_vle_user2 USING (false);
CREATE FUNCTION public.rls_vle_start_as_user2(id graphid, rn bigint)
    RETURNS ag_catalog.agtype
    LANGUAGE plpgsql
AS $f$
BEGIN
    IF rn = 2 THEN
        PERFORM set_config('role', 'rls_vle_user2', true);
    END IF;
    RETURN (id::text)::ag_catalog.agtype;
END
$f$;

SET ROLE rls_vle_user;
SELECT count(*) AS paths_with_role_changed_midway
FROM (SELECT id, row_number() OVER (ORDER BY id) AS rn
      FROM rls_vle_many."Source") AS s,
     LATERAL age_vle('"rls_vle_many"',
                     public.rls_vle_start_as_user2(s.id, s.rn), NULL,
                     age_build_vle_match_edge('"LINK"', NULL),
                     '1', '1', '1') AS v;
RESET ROLE;

-- 8.7: a statement's own deletes do not make a traversal load its graph
-- again. The traversal keeps the graph it loaded, as it does without RLS, so
-- a policy that allows every row returns what no policy returns, and the
-- query holds one copy of the graph instead of one for every row.
SELECT create_graph('rls_vle_before_plain');
SELECT create_graph('rls_vle_before_rls');
SELECT create_graph('rls_vle_after_plain');
SELECT create_graph('rls_vle_after_rls');
SELECT * FROM cypher('rls_vle_before_plain', $$
    CREATE (:Source {i: 1})-[:LINK]->(m:Mid {i: 0})-[:LINK]->(:Target {i: 9}),
           (:Source {i: 2})-[:LINK]->(m)
$$) AS (x agtype);
SELECT * FROM cypher('rls_vle_before_rls', $$
    CREATE (:Source {i: 1})-[:LINK]->(m:Mid {i: 0})-[:LINK]->(:Target {i: 9}),
           (:Source {i: 2})-[:LINK]->(m)
$$) AS (x agtype);
SELECT * FROM cypher('rls_vle_after_plain', $$
    CREATE (:Node {i: 1, ord: 0})-[:LINK]->(:Node {i: 2, ord: 2})
           -[:LINK]->(:Node {i: 3, ord: 3}),
           (:Node {i: 9, ord: 1})
$$) AS (x agtype);
SELECT * FROM cypher('rls_vle_after_rls', $$
    CREATE (:Node {i: 1, ord: 0})-[:LINK]->(:Node {i: 2, ord: 2})
           -[:LINK]->(:Node {i: 3, ord: 3}),
           (:Node {i: 9, ord: 1})
$$) AS (x agtype);
GRANT USAGE ON SCHEMA rls_vle_before_plain, rls_vle_before_rls,
    rls_vle_after_plain, rls_vle_after_rls TO rls_vle_user;
GRANT SELECT, DELETE ON ALL TABLES IN SCHEMA rls_vle_before_plain,
    rls_vle_before_rls, rls_vle_after_plain, rls_vle_after_rls
    TO rls_vle_user;
ALTER TABLE rls_vle_before_rls."LINK" ENABLE ROW LEVEL SECURITY;
CREATE POLICY link_read ON rls_vle_before_rls."LINK" FOR SELECT USING (true);
CREATE POLICY link_delete ON rls_vle_before_rls."LINK" FOR DELETE USING (true);
ALTER TABLE rls_vle_after_rls."LINK" ENABLE ROW LEVEL SECURITY;
CREATE POLICY link_read ON rls_vle_after_rls."LINK" FOR SELECT USING (true);
CREATE POLICY link_delete ON rls_vle_after_rls."LINK" FOR DELETE USING (true);

SET ROLE rls_vle_user;
-- a traversal before the delete, for each Source: s1 -> m <- s2, m -> t
SELECT * FROM cypher('rls_vle_before_plain', $$
    MATCH (a:Source)-[:LINK*1..2]->(b)
    DETACH DELETE b
    RETURN a.i, b.i
$$) AS (source agtype, deleted agtype) ORDER BY source, deleted;
SELECT * FROM cypher('rls_vle_before_rls', $$
    MATCH (a:Source)-[:LINK*1..2]->(b)
    DETACH DELETE b
    RETURN a.i, b.i
$$) AS (source agtype, deleted agtype) ORDER BY source, deleted;

-- a traversal after the delete of each node but n1, in the order of ord:
-- n1 -> n2 -> n3 and an unconnected n9, which is deleted first
SELECT * FROM cypher('rls_vle_after_plain', $$
    MATCH (d:Node) WHERE d.i > 1
    WITH d ORDER BY d.ord
    DETACH DELETE d
    WITH d
    MATCH p = (n:Node {i: 1})-[:LINK*1..3]->()
    RETURN d.i, length(p)
$$) AS (deleted agtype, path_length agtype) ORDER BY deleted, path_length;
SELECT * FROM cypher('rls_vle_after_rls', $$
    MATCH (d:Node) WHERE d.i > 1
    WITH d ORDER BY d.ord
    DETACH DELETE d
    WITH d
    MATCH p = (n:Node {i: 1})-[:LINK*1..3]->()
    RETURN d.i, length(p)
$$) AS (deleted agtype, path_length agtype) ORDER BY deleted, path_length;
RESET ROLE;

-- the copies of the graph held while a traversal for each Source is
-- followed by a delete, counted after every row
CREATE FUNCTION public.rls_vle_graph_copies(anyelement) RETURNS bigint
    LANGUAGE plpgsql VOLATILE SECURITY DEFINER
AS $f$
BEGIN
    RETURN (SELECT count(*) FROM pg_backend_memory_contexts
            WHERE name = 'AGE edge_table');
END
$f$;
SELECT age_delete_global_graphs(NULL) IS NOT NULL AS graph_caches_cleared;
BEGIN;
GRANT DELETE ON ALL TABLES IN SCHEMA rls_vle_many TO rls_vle_user;
CREATE POLICY link_delete ON rls_vle_many."LINK" FOR DELETE USING (true);
SET LOCAL ROLE rls_vle_user;
SELECT count(*) AS deleted, max(copies) AS most_graph_copies_held
FROM (SELECT public.rls_vle_graph_copies(i) AS copies
      FROM cypher('rls_vle_many', $$
          MATCH (a:Source)-[:LINK*1..1]->(b:Target)
          DETACH DELETE b
          RETURN a.i
      $$) AS (i agtype)) AS per_row;
ROLLBACK;

-- 8.8: a path value kept after its query is built from a graph context that
-- holds all of its vertices and edges. When no context does, building it
-- raises an error: after the caches are deleted, and for a role whose
-- context lacks the path's edges.
CREATE TABLE public.rls_vle_stored_paths AS
SELECT edges FROM age_vle('"rls_vle_many"', NULL, NULL,
                          age_build_vle_match_edge('"LINK"', NULL),
                          '1', '1', '1');
SELECT count(age_materialize_vle_path(edges)) AS stored_paths_built
FROM public.rls_vle_stored_paths;
SELECT age_delete_global_graphs(NULL) IS NOT NULL AS graph_caches_cleared;
SELECT count(age_materialize_vle_path(edges)) AS stored_paths_built
FROM public.rls_vle_stored_paths;
SELECT count(age_materialize_vle_edges(edges)) AS stored_edge_lists_built
FROM public.rls_vle_stored_paths;
GRANT SELECT ON public.rls_vle_stored_paths TO rls_vle_user2;
SET ROLE rls_vle_user2;
SELECT count(*) AS paths_visible_to_user2
FROM age_vle('"rls_vle_many"', NULL, NULL,
             age_build_vle_match_edge('"LINK"', NULL), '1', '1', '1');
SELECT count(age_materialize_vle_path(edges)) AS stored_paths_built
FROM public.rls_vle_stored_paths;
SELECT count(age_materialize_vle_edges(edges)) AS stored_edge_lists_built
FROM public.rls_vle_stored_paths;
RESET ROLE;
DROP TABLE public.rls_vle_stored_paths;

-- 8.9: a query that fails after its traversal releases its graph context
-- during the error cleanup, at the top level and in PL/pgSQL, so failed
-- queries leave no contexts behind
SET ROLE rls_vle_user;
SELECT 1 / ((n::text)::int - 20) AS fails_after_traversal
FROM cypher('rls_vle_many', $$
    MATCH (a:Source)-[:LINK*1..1]->(b:Target) RETURN count(*)
$$) AS (n agtype);
SELECT 1 / ((n::text)::int - 20) AS fails_after_traversal
FROM cypher('rls_vle_many', $$
    MATCH (a:Source)-[:LINK*1..1]->(b:Target) RETURN count(*)
$$) AS (n agtype);
DO $d$
BEGIN
    PERFORM 1 / ((n::text)::int - 20)
    FROM cypher('rls_vle_many', $$
        MATCH (a:Source)-[:LINK*1..1]->(b:Target) RETURN count(*)
    $$) AS (n agtype);
EXCEPTION WHEN division_by_zero THEN
    NULL;
END
$d$;
SELECT * FROM cypher('rls_vle_many', $$
    MATCH (a:Source)-[:LINK*1..1]->(b:Target) RETURN count(*)
$$) AS (paths agtype);
RESET ROLE;
SELECT count(*) AS contexts_after_failed_queries
FROM pg_backend_memory_contexts WHERE name = 'AGE edge_table';

-- 8.10: deleting the graph caches while a query holds its graph retires the
-- context instead of freeing it. The query still builds its paths from it,
-- and it is freed when the query ends, also when the query then fails.
SET ROLE rls_vle_user;
SELECT count(age_materialize_vle_path(v.edges)) AS paths_built_after_delete
FROM (SELECT edges, row_number() OVER () AS rn
      FROM age_vle('"rls_vle_many"', NULL, NULL,
                   age_build_vle_match_edge('"LINK"', NULL),
                   '1', '1', '1')) AS v
WHERE CASE WHEN v.rn = 1 THEN age_delete_global_graphs(NULL) IS NOT NULL
           ELSE true END;
RESET ROLE;
SELECT count(*) AS contexts_after_query
FROM pg_backend_memory_contexts WHERE name = 'AGE edge_table';
SET ROLE rls_vle_user;
SELECT 1 / (count(age_materialize_vle_path(v.edges)) - 20) AS fails_after_paths
FROM (SELECT edges, row_number() OVER () AS rn
      FROM age_vle('"rls_vle_many"', NULL, NULL,
                   age_build_vle_match_edge('"LINK"', NULL),
                   '1', '1', '1')) AS v
WHERE CASE WHEN v.rn = 1
           THEN age_delete_global_graphs('"rls_vle_many"') IS NOT NULL
           ELSE true END;
RESET ROLE;
SELECT count(*) AS contexts_after_failed_query
FROM pg_backend_memory_contexts WHERE name = 'AGE edge_table';

DROP FUNCTION public.rls_vle_graph_copies(anyelement);
DROP FUNCTION public.rls_vle_start_as_user2(graphid, bigint);
DROP FUNCTION public.rls_vle_start_hiding_links(graphid, bigint);
DROP FUNCTION public.rls_vle_count_fixed(graphid);
DROP FUNCTION public.rls_vle_count_stable();
DROP SEQUENCE public.rls_vle_policy_calls;
SELECT drop_graph('rls_vle_chain', true);
SELECT drop_graph('rls_vle_chain2', true);
SELECT drop_graph('rls_vle_many', true);
SELECT drop_graph('rls_vle_before_plain', true);
SELECT drop_graph('rls_vle_before_rls', true);
SELECT drop_graph('rls_vle_after_plain', true);
SELECT drop_graph('rls_vle_after_rls', true);

-- ============================================================================
-- PART 9: vertex_stats on a vertex hidden from traversals. An untyped MATCH
-- scans the parent label table, which the child table's policy does not
-- filter, so it can return a vertex that the traversal graph does not hold.
-- vertex_stats returns null for it; visible vertices get statistics computed
-- over the graph the caller can traverse.
-- ============================================================================
ALTER TABLE rls_vle."Node" ENABLE ROW LEVEL SECURITY;
CREATE POLICY node_visible ON rls_vle."Node"
    FOR SELECT USING ((properties->>'"name"') <> 'n2');
SET ROLE rls_vle_user;
SELECT * FROM cypher('rls_vle', $$
    MATCH (n:Node) RETURN n.name
$$) AS (name agtype) ORDER BY name;
SELECT * FROM cypher('rls_vle', $$
    MATCH (n)
    WITH n, vertex_stats(n) AS s
    RETURN n.name, s IS NULL, s.in_degree, s.out_degree
$$) AS (name agtype, no_stats agtype, in_degree agtype, out_degree agtype)
ORDER BY name;
RESET ROLE;
DROP POLICY node_visible ON rls_vle."Node";
ALTER TABLE rls_vle."Node" DISABLE ROW LEVEL SECURITY;

-- ============================================================================
-- PART 10: Column privileges. A role granted SELECT on every user column of
-- an RLS-protected label, but not on the table, traverses it just as MATCH
-- reads it. It cannot read ctid, so its graph keeps the rows' properties.
-- ============================================================================
ALTER TABLE rls_vle."LINK" ENABLE ROW LEVEL SECURITY;
CREATE POLICY link_all ON rls_vle."LINK" FOR SELECT USING (true);
REVOKE SELECT ON rls_vle."LINK" FROM rls_vle_user;
GRANT SELECT (id, start_id, end_id, properties) ON rls_vle."LINK"
    TO rls_vle_user;
SET ROLE rls_vle_user;
SELECT * FROM cypher('rls_vle', $$
    MATCH (a:Node {name: 'n1'})-[:LINK]->(b) RETURN b.name
$$) AS (name agtype);
SELECT * FROM cypher('rls_vle', $$
    MATCH (a:Node {name: 'n1'})-[e:LINK*1..3]->(b)
    RETURN b.name, [r IN e | r.visible]
$$) AS (name agtype, visible agtype) ORDER BY name;
RESET ROLE;
REVOKE SELECT (id, start_id, end_id, properties) ON rls_vle."LINK"
    FROM rls_vle_user;
GRANT SELECT ON rls_vle."LINK" TO rls_vle_user;
DROP POLICY link_all ON rls_vle."LINK";
ALTER TABLE rls_vle."LINK" DISABLE ROW LEVEL SECURITY;

ALTER TABLE rls_vle."Node" ENABLE ROW LEVEL SECURITY;
CREATE POLICY node_all ON rls_vle."Node" FOR SELECT USING (true);
REVOKE SELECT ON rls_vle."Node" FROM rls_vle_user;
GRANT SELECT (id, properties) ON rls_vle."Node" TO rls_vle_user;
SET ROLE rls_vle_user;
SELECT * FROM cypher('rls_vle', $$
    MATCH p = (a:Node {name: 'n1'})-[:LINK*1..3]->(b)
    RETURN b.name, [v IN nodes(p) | v.name]
$$) AS (name agtype, names agtype) ORDER BY name;
RESET ROLE;
REVOKE SELECT (id, properties) ON rls_vle."Node" FROM rls_vle_user;
GRANT SELECT ON rls_vle."Node" TO rls_vle_user;
DROP POLICY node_all ON rls_vle."Node";
ALTER TABLE rls_vle."Node" DISABLE ROW LEVEL SECURITY;

-- ============================================================================
-- PART 11: No visible vertices. When a policy hides every vertex, a traversal
-- has nothing to traverse and returns no paths instead of an error. The
-- untyped MATCH still returns the hidden vertices from the parent table, so
-- each traversal starts from a vertex outside the visible graph.
-- ============================================================================
ALTER TABLE rls_vle."Node" ENABLE ROW LEVEL SECURITY;
CREATE POLICY node_none ON rls_vle."Node" FOR SELECT USING (false);
SET ROLE rls_vle_user;
SELECT * FROM cypher('rls_vle', $$
    MATCH (n:Node) RETURN count(n)
$$) AS (count agtype);
SELECT * FROM cypher('rls_vle', $$
    MATCH (a)-[:LINK*1..3]->(b) RETURN b.name
$$) AS (name agtype);
SELECT * FROM cypher('rls_vle', $$
    MATCH (a)-[:LINK*0..1]->(b) RETURN count(*)
$$) AS (count agtype);
SELECT count(*) AS paths
FROM age_vle('"rls_vle"', NULL, NULL,
             age_build_vle_match_edge('"LINK"', NULL), '1', '3', '1');
RESET ROLE;
DROP POLICY node_none ON rls_vle."Node";
ALTER TABLE rls_vle."Node" DISABLE ROW LEVEL SECURITY;

-- ============================================================================
-- PART 12: Parallel operations. The policies are evaluated by the process
-- that loads the graph, and a parallel worker lacks part of the caller's
-- session state, so loading an RLS-protected label during a parallel
-- operation is refused. Roles that bypass the policy, and serial plans, are
-- unaffected.
-- ============================================================================
ALTER TABLE rls_vle."LINK" ENABLE ROW LEVEL SECURITY;
CREATE POLICY link_all ON rls_vle."LINK" FOR SELECT USING (true);
SET debug_parallel_query = on;
\set VERBOSITY terse
SET ROLE rls_vle_user;
SELECT * FROM cypher('rls_vle', $$
    MATCH (n:Node) RETURN vertex_stats(n)
$$) AS (stats agtype);
SELECT age_graph_stats('"rls_vle"');
RESET ROLE;
\set VERBOSITY default
SELECT * FROM cypher('rls_vle', $$
    MATCH (n:Node {name: 'n1'}) WITH vertex_stats(n) AS s RETURN s.out_degree
$$) AS (out_degree agtype);
RESET debug_parallel_query;
SET ROLE rls_vle_user;
SELECT * FROM cypher('rls_vle', $$
    MATCH (n:Node {name: 'n1'}) WITH vertex_stats(n) AS s RETURN s.out_degree
$$) AS (out_degree agtype);
RESET ROLE;
DROP POLICY link_all ON rls_vle."LINK";
ALTER TABLE rls_vle."LINK" DISABLE ROW LEVEL SECURITY;

-- ============================================================================
-- cleanup
-- ============================================================================
SELECT drop_graph('rls_vle', true);
DROP OWNED BY rls_vle_user, rls_vle_user2, rls_vle_user3;
DROP ROLE rls_vle_user, rls_vle_user2, rls_vle_user3;
