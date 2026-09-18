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

-- Suppress the create_graph / create_vlabel NOTICE chatter so the assertions
-- below are the deterministic output. (The feature is exercised regardless.)
SET client_min_messages = warning;

--
-- Build a "somewhat large" source graph with NO MATCH (fast bulk CREATE):
--   * 2000 isolated components, each  (:Person{pid,age})-[:KNOWS{w}]->(:Friend{pid})
--     => 2000 Person + 2000 Friend vertices, 2000 KNOWS edges
--   * 500 isolated :Company vertices (no edges)
-- Totals: 4500 vertices, 2000 edges, label set {Person,Friend,Company,KNOWS}.
--
SELECT create_graph('sg_src');

SELECT count(*) FROM cypher('sg_src', $$
    UNWIND range(1, 2000) AS i
    CREATE (:Person {pid: i, age: i % 100})-[:KNOWS {w: i}]->(:Friend {pid: i})
$$) AS (a agtype);

SELECT count(*) FROM cypher('sg_src', $$
    UNWIND range(1, 500) AS i CREATE (:Company {cid: i})
$$) AS (a agtype);

-- Source baseline (printed for reference; deterministic).
SELECT
    (SELECT count(*) FROM cypher('sg_src', $$ MATCH (n) RETURN n $$) AS (n agtype)) AS src_vertices,
    (SELECT count(*) FROM cypher('sg_src', $$ MATCH ()-[r]->() RETURN r $$) AS (r agtype)) AS src_edges;

--
-- 1. Full copy ('*','*'): counts equal the source, and the new graph round-trips.
--
SELECT node_count, relationship_count
FROM create_subgraph('sg_all', 'sg_src', '*', '*');

SELECT
    (SELECT count(*) FROM cypher('sg_all', $$ MATCH (n) RETURN n $$) AS (n agtype))
      = (SELECT count(*) FROM cypher('sg_src', $$ MATCH (n) RETURN n $$) AS (n agtype)) AS nodes_match,
    (SELECT count(*) FROM cypher('sg_all', $$ MATCH ()-[r]->() RETURN r $$) AS (r agtype))
      = (SELECT count(*) FROM cypher('sg_src', $$ MATCH ()-[r]->() RETURN r $$) AS (r agtype)) AS edges_match;

--
-- 2. Vertex-induced (node filter only): keep pid <= 1000. An edge survives iff
--    BOTH endpoints survive (induced rule), with no relationship filter.
--    node_count is asserted against the function return; correctness is verified
--    by recomputing the induced set from the source (robust booleans).
--
SELECT node_count, relationship_count
FROM create_subgraph('sg_v', 'sg_src', 'n.pid <= 1000', '*');

SELECT
    (SELECT count(*) FROM cypher('sg_v', $$ MATCH (n) RETURN n $$) AS (n agtype))
      = (SELECT count(*) FROM cypher('sg_src',
            $$ MATCH (n) WHERE n.pid <= 1000 RETURN n $$) AS (n agtype)) AS nodes_ok,
    (SELECT count(*) FROM cypher('sg_v', $$ MATCH ()-[r]->() RETURN r $$) AS (r agtype))
      = (SELECT count(*) FROM cypher('sg_src',
            $$ MATCH (a)-[r]->(b) WHERE a.pid <= 1000 AND b.pid <= 1000 RETURN r $$)
            AS (r agtype)) AS edges_ok;

--
-- 3. Node + relationship predicate: keep pid <= 1000 vertices and w <= 300 edges.
--    Edge survives iff w<=300 AND both endpoints pid<=1000.
--
SELECT node_count, relationship_count
FROM create_subgraph('sg_nr', 'sg_src', 'n.pid <= 1000', 'r.w <= 300');

SELECT
    (SELECT count(*) FROM cypher('sg_nr', $$ MATCH ()-[r]->() RETURN r $$) AS (r agtype))
      = (SELECT count(*) FROM cypher('sg_src',
            $$ MATCH (a)-[r]->(b) WHERE r.w <= 300 AND a.pid <= 1000 AND b.pid <= 1000
               RETURN r $$) AS (r agtype)) AS edges_ok;

--
-- 4. Label filter excludes one endpoint type: keep only :Person. Every KNOWS
--    edge points Person->Friend, so all edges must be dropped (induced rule).
--    (AGE evaluates label predicates with label(n); GDS uses n:Person -- same
--    containment semantics, different predicate syntax.)
--
SELECT node_count, relationship_count
FROM create_subgraph('sg_person', 'sg_src', $f$label(n) = 'Person'$f$, '*');

--
-- 5. Bipartite (type filter): keep Person+Friend and KNOWS edges => all 2000.
--
SELECT node_count, relationship_count
FROM create_subgraph('sg_bip', 'sg_src',
                     $f$label(n) = 'Person' OR label(n) = 'Friend'$f$,
                     $f$label(r) = 'KNOWS'$f$);

--
-- 6. Empty result: a predicate matching nothing yields an empty subgraph
--    (not an error), with the default labels only.
--
SELECT node_count, relationship_count
FROM create_subgraph('sg_empty', 'sg_src', 'n.pid < 0', '*');

SELECT count(*) AS empty_vertices
FROM cypher('sg_empty', $$ MATCH (n) RETURN n $$) AS (n agtype);

--
-- 7. Composability: extract a subgraph from an already-extracted subgraph.
--    From sg_v (pid<=1000) keep pid<=500; verify against recomputation on sg_v.
--
SELECT node_count, relationship_count
FROM create_subgraph('sg_v2', 'sg_v', 'n.pid <= 500', '*');

SELECT
    (SELECT count(*) FROM cypher('sg_v2', $$ MATCH (n) RETURN n $$) AS (n agtype))
      = (SELECT count(*) FROM cypher('sg_v',
            $$ MATCH (n) WHERE n.pid <= 500 RETURN n $$) AS (n agtype)) AS nodes_ok,
    (SELECT count(*) FROM cypher('sg_v2', $$ MATCH ()-[r]->() RETURN r $$) AS (r agtype))
      = (SELECT count(*) FROM cypher('sg_v',
            $$ MATCH (a)-[r]->(b) WHERE a.pid <= 500 AND b.pid <= 500 RETURN r $$)
            AS (r agtype)) AS edges_ok;

--
-- 8. Self-loops and parallel edges (multigraph structure) are preserved.
--
SELECT create_graph('sg_multi');
SELECT * FROM cypher('sg_multi', $$
    CREATE (a:N {k: 1}) CREATE (a)-[:E {t: 1}]->(a)
$$) AS (a agtype);
SELECT * FROM cypher('sg_multi', $$
    CREATE (a:N {k: 2}), (b:N {k: 3}),
           (a)-[:E {t: 2}]->(b), (a)-[:E {t: 3}]->(b)
$$) AS (a agtype);

SELECT node_count, relationship_count
FROM create_subgraph('sg_multi_sub', 'sg_multi', '*', '*');

-- self-loop preserved (exactly one edge from a node to itself)
SELECT count(*) AS self_loops
FROM cypher('sg_multi_sub', $$ MATCH (a)-[r]->(a) RETURN r $$) AS (r agtype);

-- parallel edges preserved (two edges between k=2 and k=3)
SELECT count(*) AS parallel_edges
FROM cypher('sg_multi_sub', $$ MATCH (a {k: 2})-[r]->(b {k: 3}) RETURN r $$) AS (r agtype);

--
-- 9. Property fidelity: a copied vertex keeps its properties verbatim.
--
SELECT count(*) AS person_500_age_ok
FROM cypher('sg_v', $$ MATCH (n:Person {pid: 500}) WHERE n.age = 0 RETURN n $$) AS (n agtype);

--
-- 10. Error handling / edge cases.
--
-- NULL graph name
SELECT create_subgraph(NULL, 'sg_src', '*', '*');
-- source does not exist
SELECT create_subgraph('sg_x', 'no_such_graph', '*', '*');
-- extracting into the source itself
SELECT create_subgraph('sg_src', 'sg_src', '*', '*');
-- destination already exists
SELECT create_subgraph('sg_all', 'sg_src', '*', '*');
-- invalid Cypher predicate is reported (propagated from the engine)
SELECT create_subgraph('sg_bad', 'sg_src', 'n.pid <<>> 1', '*');

-- cleanup
SELECT drop_graph('sg_v2', true);
SELECT drop_graph('sg_multi_sub', true);
SELECT drop_graph('sg_multi', true);
SELECT drop_graph('sg_empty', true);
SELECT drop_graph('sg_bip', true);
SELECT drop_graph('sg_person', true);
SELECT drop_graph('sg_nr', true);
SELECT drop_graph('sg_v', true);
SELECT drop_graph('sg_all', true);
SELECT drop_graph('sg_src', true);
