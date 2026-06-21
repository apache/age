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
-- age_shortest_path / age_all_shortest_paths
--

SELECT * FROM create_graph('sp_graph');

-- Build a small deterministic graph:
--
--        A
--       / \
--      B   C        (A->B, A->C, B->D, C->D : two shortest A..D paths)
--       \ /
--        D
--        |
--        E          (D->E : unique 3-hop path A..E)
--
--        Z          (isolated, unreachable)
--
SELECT * FROM cypher('sp_graph', $$
    CREATE (a:Person {name: 'A'}),
           (b:Person {name: 'B'}),
           (c:Person {name: 'C'}),
           (d:Person {name: 'D'}),
           (e:Person {name: 'E'}),
           (z:Person {name: 'Z'}),
           (a)-[:KNOWS]->(b),
           (a)-[:KNOWS]->(c),
           (b)-[:KNOWS]->(d),
           (c)-[:KNOWS]->(d),
           (d)-[:KNOWS]->(e)
$$) AS (result agtype);

-- materialize the global graph context
SELECT * FROM cypher('sp_graph', $$ MATCH (u) RETURN vertex_stats(u) ORDER BY id(u) $$)
    AS (result agtype);

-- A -> D shortest path (length 2); expected: path_count = 1
SELECT count(*) AS path_count
FROM age_shortest_path(
    '"sp_graph"'::agtype,
    (SELECT id FROM cypher('sp_graph', $$ MATCH (n {name:'A'}) RETURN id(n) $$) AS (id agtype)),
    (SELECT id FROM cypher('sp_graph', $$ MATCH (n {name:'D'}) RETURN id(n) $$) AS (id agtype))
);

-- all shortest A -> D; expected: 2 paths (A-B-D and A-C-D), each length 2
SELECT path
FROM age_all_shortest_paths(
    '"sp_graph"'::agtype,
    (SELECT id FROM cypher('sp_graph', $$ MATCH (n {name:'A'}) RETURN id(n) $$) AS (id agtype)),
    (SELECT id FROM cypher('sp_graph', $$ MATCH (n {name:'D'}) RETURN id(n) $$) AS (id agtype))
) AS path
ORDER BY path;

-- A -> E unique 3-hop path; expected: path_count = 1
SELECT count(*) AS path_count
FROM age_shortest_path(
    '"sp_graph"'::agtype,
    (SELECT id FROM cypher('sp_graph', $$ MATCH (n {name:'A'}) RETURN id(n) $$) AS (id agtype)),
    (SELECT id FROM cypher('sp_graph', $$ MATCH (n {name:'E'}) RETURN id(n) $$) AS (id agtype))
);

-- A -> E with max_hops = 2; expected: path_count = 0 (E is 3 hops away)
SELECT count(*) AS path_count
FROM age_shortest_path(
    '"sp_graph"'::agtype,
    (SELECT id FROM cypher('sp_graph', $$ MATCH (n {name:'A'}) RETURN id(n) $$) AS (id agtype)),
    (SELECT id FROM cypher('sp_graph', $$ MATCH (n {name:'E'}) RETURN id(n) $$) AS (id agtype)),
    NULL, NULL, NULL, 2::agtype
);

-- zero-length path, start == end; expected: path_count = 1
SELECT count(*) AS path_count
FROM age_shortest_path(
    '"sp_graph"'::agtype,
    (SELECT id FROM cypher('sp_graph', $$ MATCH (n {name:'A'}) RETURN id(n) $$) AS (id agtype)),
    (SELECT id FROM cypher('sp_graph', $$ MATCH (n {name:'A'}) RETURN id(n) $$) AS (id agtype))
);

-- unreachable vertex Z; expected: path_count = 0
SELECT count(*) AS path_count
FROM age_shortest_path(
    '"sp_graph"'::agtype,
    (SELECT id FROM cypher('sp_graph', $$ MATCH (n {name:'A'}) RETURN id(n) $$) AS (id agtype)),
    (SELECT id FROM cypher('sp_graph', $$ MATCH (n {name:'Z'}) RETURN id(n) $$) AS (id agtype))
);

-- direction 'in': D -> A traversing edges backwards; expected: path_count = 2
SELECT count(*) AS path_count
FROM age_all_shortest_paths(
    '"sp_graph"'::agtype,
    (SELECT id FROM cypher('sp_graph', $$ MATCH (n {name:'D'}) RETURN id(n) $$) AS (id agtype)),
    (SELECT id FROM cypher('sp_graph', $$ MATCH (n {name:'A'}) RETURN id(n) $$) AS (id agtype)),
    NULL, '"in"'::agtype
);

-- direction 'out': D -> A not reachable forwards; expected: path_count = 0
SELECT count(*) AS path_count
FROM age_shortest_path(
    '"sp_graph"'::agtype,
    (SELECT id FROM cypher('sp_graph', $$ MATCH (n {name:'D'}) RETURN id(n) $$) AS (id agtype)),
    (SELECT id FROM cypher('sp_graph', $$ MATCH (n {name:'A'}) RETURN id(n) $$) AS (id agtype)),
    NULL, '"out"'::agtype
);

-- label filter 'KNOWS': A -> D still found; expected: path_count = 1
SELECT count(*) AS path_count
FROM age_shortest_path(
    '"sp_graph"'::agtype,
    (SELECT id FROM cypher('sp_graph', $$ MATCH (n {name:'A'}) RETURN id(n) $$) AS (id agtype)),
    (SELECT id FROM cypher('sp_graph', $$ MATCH (n {name:'D'}) RETURN id(n) $$) AS (id agtype)),
    '"KNOWS"'::agtype
);

-- error: invalid direction string; expected: ERROR (must be 'out', 'in', or 'any')
SELECT count(*) AS path_count
FROM age_shortest_path(
    '"sp_graph"'::agtype,
    (SELECT id FROM cypher('sp_graph', $$ MATCH (n {name:'A'}) RETURN id(n) $$) AS (id agtype)),
    (SELECT id FROM cypher('sp_graph', $$ MATCH (n {name:'D'}) RETURN id(n) $$) AS (id agtype)),
    NULL, '"sideways"'::agtype
);

-- error: start argument is neither a vertex nor an integer id; expected: ERROR
SELECT count(*) AS path_count
FROM age_shortest_path(
    '"sp_graph"'::agtype,
    '"not_a_vertex"'::agtype,
    (SELECT id FROM cypher('sp_graph', $$ MATCH (n {name:'D'}) RETURN id(n) $$) AS (id agtype))
);

--
-- Non-existent endpoint guards. These must NOT crash the backend and must
-- return no rows (a path can only exist between vertices in the graph).
-- Previously, start == end on a non-existent vertex id was matched at BFS
-- depth 0 and path reconstruction dereferenced a missing vertex, crashing
-- the server.
--

-- start == end on a non-existent integer id; expected: path_count = 0
SELECT count(*) AS path_count
FROM age_shortest_path('"sp_graph"'::agtype, 999999::agtype, 999999::agtype);

-- existing start -> non-existent end; expected: path_count = 0
SELECT count(*) AS path_count
FROM age_shortest_path(
    '"sp_graph"'::agtype,
    (SELECT id FROM cypher('sp_graph', $$ MATCH (n {name:'A'}) RETURN id(n) $$) AS (id agtype)),
    999999::agtype
);

-- non-existent start -> existing end; expected: path_count = 0
SELECT count(*) AS path_count
FROM age_shortest_path(
    '"sp_graph"'::agtype,
    999999::agtype,
    (SELECT id FROM cypher('sp_graph', $$ MATCH (n {name:'A'}) RETURN id(n) $$) AS (id agtype))
);

-- all-shortest-paths with start == end non-existent; expected: 0 rows
SELECT count(*) AS path_count
FROM age_all_shortest_paths('"sp_graph"'::agtype, 999999::agtype, 999999::agtype);

-- cleanup
SELECT * FROM drop_graph('sp_graph', true);


--
-- Empty graph: a graph that exists but has no vertices must return no rows
-- (and must not hang or crash) for any endpoint query.
--
SELECT * FROM create_graph('sp_empty');
SELECT count(*) AS path_count
FROM age_shortest_path('"sp_empty"'::agtype, 0::agtype, 1::agtype);
SELECT count(*) AS path_count
FROM age_all_shortest_paths('"sp_empty"'::agtype, 0::agtype, 0::agtype);
SELECT * FROM drop_graph('sp_empty', true);



--
-- A large, programmatically generated graph (120 nodes) exercising long
-- shortest paths (length up to 20), high-multiplicity all-shortest-paths,
-- label filtering, and directed vs. undirected reachability.
--
-- Nodes: (:N {id: 0..119}).  Structures built on top of them:
--
--   * Main chain      0 -> 1 -> ... -> 20            (unique 20-hop path)
--   * Alternate chain 0 -> 50 -> 51 -> ... -> 68 -> 20
--                       (a second, disjoint 20-hop path 0..20)
--       => all-shortest-paths 0..20 under KNOWS = 2 paths of length 20
--   * 3x3 lattice on ids 70..78, id = 70 + 3*row + col, edges go right
--     (id->id+1) and down (id->id+3).  Monotone 70..78 paths:
--       => all-shortest-paths 70..78 = C(4,2) = 6 paths of length 4
--   * LIKES shortcut  0 -[:LIKES]-> 20  (1 hop; only visible when the edge
--     label filter is NOT restricted to KNOWS)
--   * Back-edge triangle 0 -> 96 -> 95 -> 0
--       => directed 0->95 = 2 hops (0-96-95); undirected 0..95 = 1 hop
--   * Many unused ids (e.g. 119) remain isolated / unreachable.
--
SELECT * FROM create_graph('sp_big');

-- 120 vertices, ids 0..119
SELECT * FROM cypher('sp_big', $$
    UNWIND range(0, 119) AS i CREATE (:N {id: i})
$$) AS (result agtype);

-- main chain 0->1->...->20  (KNOWS)
SELECT * FROM cypher('sp_big', $$
    UNWIND range(0, 19) AS i
    MATCH (a:N {id: i}), (b:N {id: i + 1})
    CREATE (a)-[:KNOWS]->(b)
$$) AS (result agtype);

-- alternate, disjoint 20-hop path 0->50->51->...->68->20  (KNOWS)
SELECT * FROM cypher('sp_big', $$
    MATCH (a:N {id: 0}), (b:N {id: 50}) CREATE (a)-[:KNOWS]->(b)
$$) AS (result agtype);
SELECT * FROM cypher('sp_big', $$
    UNWIND range(50, 67) AS i
    MATCH (a:N {id: i}), (b:N {id: i + 1})
    CREATE (a)-[:KNOWS]->(b)
$$) AS (result agtype);
SELECT * FROM cypher('sp_big', $$
    MATCH (a:N {id: 68}), (b:N {id: 20}) CREATE (a)-[:KNOWS]->(b)
$$) AS (result agtype);

-- 3x3 lattice on ids 70..78: right edges (id -> id+1)
SELECT * FROM cypher('sp_big', $$
    UNWIND [0, 1, 2] AS r
    UNWIND [0, 1] AS c
    MATCH (a:N {id: 70 + 3 * r + c}), (b:N {id: 70 + 3 * r + c + 1})
    CREATE (a)-[:KNOWS]->(b)
$$) AS (result agtype);

-- 3x3 lattice: down edges (id -> id+3)
SELECT * FROM cypher('sp_big', $$
    UNWIND [0, 1] AS r
    UNWIND [0, 1, 2] AS c
    MATCH (a:N {id: 70 + 3 * r + c}), (b:N {id: 70 + 3 * (r + 1) + c})
    CREATE (a)-[:KNOWS]->(b)
$$) AS (result agtype);

-- back-edge triangle 0 -> 96 -> 95 -> 0  (KNOWS)
SELECT * FROM cypher('sp_big', $$
    MATCH (a:N {id: 0}),  (b:N {id: 96}) CREATE (a)-[:KNOWS]->(b)
$$) AS (result agtype);
SELECT * FROM cypher('sp_big', $$
    MATCH (a:N {id: 96}), (b:N {id: 95}) CREATE (a)-[:KNOWS]->(b)
$$) AS (result agtype);
SELECT * FROM cypher('sp_big', $$
    MATCH (a:N {id: 95}), (b:N {id: 0})  CREATE (a)-[:KNOWS]->(b)
$$) AS (result agtype);

-- labelled shortcut 0 -[:LIKES]-> 20
SELECT * FROM cypher('sp_big', $$
    MATCH (a:N {id: 0}), (b:N {id: 20}) CREATE (a)-[:LIKES]->(b)
$$) AS (result agtype);

-- sanity: vertex count (also materializes the global context); expected: count = 120
SELECT * FROM cypher('sp_big', $$ MATCH (n) RETURN count(n) $$) AS (n agtype);

-- all shortest 0 -> 20 under KNOWS (main chain + disjoint alternate);
-- expected: 2 paths, each exactly 20 hops
SELECT path
FROM age_all_shortest_paths(
    '"sp_big"'::agtype,
    (SELECT id FROM cypher('sp_big', $$ MATCH (n:N {id: 0})  RETURN id(n) $$) AS (id agtype)),
    (SELECT id FROM cypher('sp_big', $$ MATCH (n:N {id: 20}) RETURN id(n) $$) AS (id agtype)),
    '"KNOWS"'::agtype, '"out"'::agtype
) AS path
ORDER BY path;

-- any label: the LIKES shortcut collapses 0 -> 20; expected: path_count = 1
SELECT count(*) AS path_count
FROM age_all_shortest_paths(
    '"sp_big"'::agtype,
    (SELECT id FROM cypher('sp_big', $$ MATCH (n:N {id: 0})  RETURN id(n) $$) AS (id agtype)),
    (SELECT id FROM cypher('sp_big', $$ MATCH (n:N {id: 20}) RETURN id(n) $$) AS (id agtype)),
    NULL, '"out"'::agtype
);

-- all shortest 70 -> 78 across the 3x3 lattice; expected: path_count = 6 (C(4,2))
SELECT count(*) AS path_count
FROM age_all_shortest_paths(
    '"sp_big"'::agtype,
    (SELECT id FROM cypher('sp_big', $$ MATCH (n:N {id: 70}) RETURN id(n) $$) AS (id agtype)),
    (SELECT id FROM cypher('sp_big', $$ MATCH (n:N {id: 78}) RETURN id(n) $$) AS (id agtype)),
    '"KNOWS"'::agtype, '"out"'::agtype
);

-- the lattice paths listed; expected: 6 paths, each 4 hops
SELECT path
FROM age_all_shortest_paths(
    '"sp_big"'::agtype,
    (SELECT id FROM cypher('sp_big', $$ MATCH (n:N {id: 70}) RETURN id(n) $$) AS (id agtype)),
    (SELECT id FROM cypher('sp_big', $$ MATCH (n:N {id: 78}) RETURN id(n) $$) AS (id agtype)),
    '"KNOWS"'::agtype, '"out"'::agtype
) AS path
ORDER BY path;

-- max_hops = 19, one short of the 20-hop route; expected: path_count = 0
SELECT count(*) AS path_count
FROM age_shortest_path(
    '"sp_big"'::agtype,
    (SELECT id FROM cypher('sp_big', $$ MATCH (n:N {id: 0})  RETURN id(n) $$) AS (id agtype)),
    (SELECT id FROM cypher('sp_big', $$ MATCH (n:N {id: 20}) RETURN id(n) $$) AS (id agtype)),
    '"KNOWS"'::agtype, '"out"'::agtype, NULL, 19::agtype
);

-- max_hops = 20 admits the full route; expected: path_count = 2
SELECT count(*) AS path_count
FROM age_all_shortest_paths(
    '"sp_big"'::agtype,
    (SELECT id FROM cypher('sp_big', $$ MATCH (n:N {id: 0})  RETURN id(n) $$) AS (id agtype)),
    (SELECT id FROM cypher('sp_big', $$ MATCH (n:N {id: 20}) RETURN id(n) $$) AS (id agtype)),
    '"KNOWS"'::agtype, '"out"'::agtype, NULL, 20::agtype
);

-- DIRECTED out: 0 -> 95 must traverse 0->96->95; expected: 1 path (length 2)
SELECT path
FROM age_shortest_path(
    '"sp_big"'::agtype,
    (SELECT id FROM cypher('sp_big', $$ MATCH (n:N {id: 0})  RETURN id(n) $$) AS (id agtype)),
    (SELECT id FROM cypher('sp_big', $$ MATCH (n:N {id: 95}) RETURN id(n) $$) AS (id agtype)),
    NULL, '"out"'::agtype
) AS path
ORDER BY path;

-- UNDIRECTED: 0 .. 95 via the 95->0 back edge; expected: 1 path (length 1)
SELECT path
FROM age_shortest_path(
    '"sp_big"'::agtype,
    (SELECT id FROM cypher('sp_big', $$ MATCH (n:N {id: 0})  RETURN id(n) $$) AS (id agtype)),
    (SELECT id FROM cypher('sp_big', $$ MATCH (n:N {id: 95}) RETURN id(n) $$) AS (id agtype))
) AS path
ORDER BY path;

-- DIRECTED out: 78 -> 70 against lattice flow; expected: path_count = 0
SELECT count(*) AS path_count
FROM age_shortest_path(
    '"sp_big"'::agtype,
    (SELECT id FROM cypher('sp_big', $$ MATCH (n:N {id: 78}) RETURN id(n) $$) AS (id agtype)),
    (SELECT id FROM cypher('sp_big', $$ MATCH (n:N {id: 70}) RETURN id(n) $$) AS (id agtype)),
    NULL, '"out"'::agtype
);

-- UNDIRECTED: 78 .. 70 reverses the lattice; expected: path_count = 6
SELECT count(*) AS path_count
FROM age_all_shortest_paths(
    '"sp_big"'::agtype,
    (SELECT id FROM cypher('sp_big', $$ MATCH (n:N {id: 78}) RETURN id(n) $$) AS (id agtype)),
    (SELECT id FROM cypher('sp_big', $$ MATCH (n:N {id: 70}) RETURN id(n) $$) AS (id agtype))
);

-- isolated id 119 unreachable from 0; expected: path_count = 0
SELECT count(*) AS path_count
FROM age_shortest_path(
    '"sp_big"'::agtype,
    (SELECT id FROM cypher('sp_big', $$ MATCH (n:N {id: 0})   RETURN id(n) $$) AS (id agtype)),
    (SELECT id FROM cypher('sp_big', $$ MATCH (n:N {id: 119}) RETURN id(n) $$) AS (id agtype))
);

-- zero-length path, start == end; expected: path_count = 1
SELECT count(*) AS path_count
FROM age_shortest_path(
    '"sp_big"'::agtype,
    (SELECT id FROM cypher('sp_big', $$ MATCH (n:N {id: 0}) RETURN id(n) $$) AS (id agtype)),
    (SELECT id FROM cypher('sp_big', $$ MATCH (n:N {id: 0}) RETURN id(n) $$) AS (id agtype))
);

-- cleanup
SELECT * FROM drop_graph('sp_big', true);

--
-- Calling the age_* SRFs from inside cypher() (Tier 1).
--
-- Because the functions are prefixed with age_, the cypher() parser resolves
-- the unqualified names 'shortest_path' and 'all_shortest_paths' to
-- ag_catalog.age_shortest_path / ag_catalog.age_all_shortest_paths, and the
-- graph name is auto-injected as the first argument (like vle/vertex_stats),
-- so callers pass only the bound endpoints. A whole vertex implicitly casts to
-- agtype, so the argument types resolve. The SRFs are set-returning and now
-- work in a cypher RETURN projection (ProjectSet), returning one row per path.
--
SELECT * FROM create_graph('sp_cy');

SELECT * FROM cypher('sp_cy', $$
    CREATE (a:N {name: 'A'}),
           (b:N {name: 'B'}),
           (c:N {name: 'C'}),
           (a)-[:KNOWS]->(b),
           (b)-[:KNOWS]->(c)
$$) AS (result agtype);

-- materialize the global graph context
SELECT * FROM cypher('sp_cy', $$ MATCH (u) RETURN vertex_stats(u) ORDER BY id(u) $$)
    AS (result agtype);

-- shortest_path() inside a cypher RETURN; the graph name is auto-injected and
-- the bound vertices are passed; expected: 1 path A..C (length 2)
SELECT * FROM cypher('sp_cy', $$
    MATCH (a:N {name:'A'}), (c:N {name:'C'})
    RETURN shortest_path(a, c)
$$) AS (path agtype);

-- all_shortest_paths() inside a cypher RETURN; expected: 1 path A..C (length 2)
SELECT * FROM cypher('sp_cy', $$
    MATCH (a:N {name:'A'}), (c:N {name:'C'})
    RETURN all_shortest_paths(a, c)
$$) AS (path agtype);

-- in-cypher with an explicit edge-label filter; expected: 1 path A..C (length 2)
SELECT * FROM cypher('sp_cy', $$
    MATCH (a:N {name:'A'}), (c:N {name:'C'})
    RETURN shortest_path(a, c, 'KNOWS')
$$) AS (path agtype);

-- still supported: call the SRF at the top level; expected: 1 path A..C (length 2)
SELECT path
FROM age_shortest_path(
    '"sp_cy"'::agtype,
    (SELECT id FROM cypher('sp_cy', $$ MATCH (n {name:'A'}) RETURN id(n) $$) AS (id agtype)),
    (SELECT id FROM cypher('sp_cy', $$ MATCH (n {name:'C'}) RETURN id(n) $$) AS (id agtype))
) AS path
ORDER BY path;

-- cleanup
SELECT * FROM drop_graph('sp_cy', true);

--
-- Edge cases: parallel/multi-edges, self-loops, unknown edge labels,
-- max_hops boundaries (0 and negative), explicit 'any' direction, and
-- NULL / unknown-graph argument errors.
--
SELECT * FROM create_graph('sp_edge');

-- A and B are connected by TWO parallel KNOWS edges plus one LIKES edge.
-- B->C is a single KNOWS edge. S has a self-loop. These exercise the
-- multi-predecessor (parallel edge) logic and the label filter.
SELECT * FROM cypher('sp_edge', $$
    CREATE (a:N {name: 'A'}),
           (b:N {name: 'B'}),
           (c:N {name: 'C'}),
           (s:N {name: 'S'}),
           (a)-[:KNOWS]->(b),
           (a)-[:KNOWS]->(b),
           (a)-[:LIKES]->(b),
           (b)-[:KNOWS]->(c),
           (s)-[:KNOWS]->(s)
$$) AS (result agtype);

-- materialize the global graph context
SELECT * FROM cypher('sp_edge', $$ MATCH (u) RETURN vertex_stats(u) ORDER BY id(u) $$)
    AS (result agtype);

-- parallel edges: two distinct KNOWS edges A->B are two distinct shortest
-- paths; expected count 2
SELECT count(*) FROM age_all_shortest_paths(
    '"sp_edge"'::agtype,
    (SELECT id FROM cypher('sp_edge', $$ MATCH (n {name:'A'}) RETURN id(n) $$) AS (id agtype)),
    (SELECT id FROM cypher('sp_edge', $$ MATCH (n {name:'B'}) RETURN id(n) $$) AS (id agtype)),
    '"KNOWS"'::agtype, '"out"'::agtype);

-- no label filter: 2 KNOWS + 1 LIKES edge A->B are three distinct shortest
-- paths; expected count 3
SELECT count(*) FROM age_all_shortest_paths(
    '"sp_edge"'::agtype,
    (SELECT id FROM cypher('sp_edge', $$ MATCH (n {name:'A'}) RETURN id(n) $$) AS (id agtype)),
    (SELECT id FROM cypher('sp_edge', $$ MATCH (n {name:'B'}) RETURN id(n) $$) AS (id agtype)),
    NULL::agtype, '"out"'::agtype);

-- single shortest path A->B picks exactly one of the parallel edges; count 1
SELECT count(*) FROM age_shortest_path(
    '"sp_edge"'::agtype,
    (SELECT id FROM cypher('sp_edge', $$ MATCH (n {name:'A'}) RETURN id(n) $$) AS (id agtype)),
    (SELECT id FROM cypher('sp_edge', $$ MATCH (n {name:'B'}) RETURN id(n) $$) AS (id agtype)));

-- self-loop: a vertex with an edge to itself yields only the zero-length
-- path for start == end (the self-loop is never used); count 1
SELECT count(*) FROM age_shortest_path(
    '"sp_edge"'::agtype,
    (SELECT id FROM cypher('sp_edge', $$ MATCH (n {name:'S'}) RETURN id(n) $$) AS (id agtype)),
    (SELECT id FROM cypher('sp_edge', $$ MATCH (n {name:'S'}) RETURN id(n) $$) AS (id agtype)));

-- all_shortest_paths with start == end (existing vertex): one zero-length
-- path; count 1
SELECT count(*) FROM age_all_shortest_paths(
    '"sp_edge"'::agtype,
    (SELECT id FROM cypher('sp_edge', $$ MATCH (n {name:'S'}) RETURN id(n) $$) AS (id agtype)),
    (SELECT id FROM cypher('sp_edge', $$ MATCH (n {name:'S'}) RETURN id(n) $$) AS (id agtype)));

-- unknown relationship type matches no edges: A..C filtered by a label that
-- does not exist must return no path (NOT silently fall back to all edges);
-- count 0
SELECT count(*) FROM age_shortest_path(
    '"sp_edge"'::agtype,
    (SELECT id FROM cypher('sp_edge', $$ MATCH (n {name:'A'}) RETURN id(n) $$) AS (id agtype)),
    (SELECT id FROM cypher('sp_edge', $$ MATCH (n {name:'C'}) RETURN id(n) $$) AS (id agtype)),
    '"NOSUCHLABEL"'::agtype, '"out"'::agtype);

SELECT count(*) FROM age_all_shortest_paths(
    '"sp_edge"'::agtype,
    (SELECT id FROM cypher('sp_edge', $$ MATCH (n {name:'A'}) RETURN id(n) $$) AS (id agtype)),
    (SELECT id FROM cypher('sp_edge', $$ MATCH (n {name:'C'}) RETURN id(n) $$) AS (id agtype)),
    '"NOSUCHLABEL"'::agtype, '"out"'::agtype);

-- the zero-length (start == end) path has no edges, so an unknown label
-- still matches it; count 1
SELECT count(*) FROM age_shortest_path(
    '"sp_edge"'::agtype,
    (SELECT id FROM cypher('sp_edge', $$ MATCH (n {name:'S'}) RETURN id(n) $$) AS (id agtype)),
    (SELECT id FROM cypher('sp_edge', $$ MATCH (n {name:'S'}) RETURN id(n) $$) AS (id agtype)),
    '"NOSUCHLABEL"'::agtype, '"out"'::agtype);

-- existing label that does not connect the endpoints: LIKES only exists on
-- A->B, so A..C filtered by LIKES is unreachable; count 0
SELECT count(*) FROM age_shortest_path(
    '"sp_edge"'::agtype,
    (SELECT id FROM cypher('sp_edge', $$ MATCH (n {name:'A'}) RETURN id(n) $$) AS (id agtype)),
    (SELECT id FROM cypher('sp_edge', $$ MATCH (n {name:'C'}) RETURN id(n) $$) AS (id agtype)),
    '"LIKES"'::agtype, '"out"'::agtype);

-- max_hops = 0 with start == end: the zero-length path is still returned;
-- count 1
SELECT count(*) FROM age_shortest_path(
    '"sp_edge"'::agtype,
    (SELECT id FROM cypher('sp_edge', $$ MATCH (n {name:'A'}) RETURN id(n) $$) AS (id agtype)),
    (SELECT id FROM cypher('sp_edge', $$ MATCH (n {name:'A'}) RETURN id(n) $$) AS (id agtype)),
    NULL::agtype, NULL::agtype, NULL::agtype, '0'::agtype);

-- max_hops = 0 with adjacent distinct endpoints: no path within zero hops;
-- count 0
SELECT count(*) FROM age_shortest_path(
    '"sp_edge"'::agtype,
    (SELECT id FROM cypher('sp_edge', $$ MATCH (n {name:'A'}) RETURN id(n) $$) AS (id agtype)),
    (SELECT id FROM cypher('sp_edge', $$ MATCH (n {name:'B'}) RETURN id(n) $$) AS (id agtype)),
    NULL::agtype, NULL::agtype, NULL::agtype, '0'::agtype);

-- negative max_hops is treated as unbounded: A..C (length 2) is found;
-- count 1
SELECT count(*) FROM age_shortest_path(
    '"sp_edge"'::agtype,
    (SELECT id FROM cypher('sp_edge', $$ MATCH (n {name:'A'}) RETURN id(n) $$) AS (id agtype)),
    (SELECT id FROM cypher('sp_edge', $$ MATCH (n {name:'C'}) RETURN id(n) $$) AS (id agtype)),
    NULL::agtype, NULL::agtype, NULL::agtype, '-1'::agtype);

-- explicit 'any' direction string (vs the default NULL == undirected);
-- two parallel KNOWS edges A->B give two shortest paths; count 2
SELECT count(*) FROM age_all_shortest_paths(
    '"sp_edge"'::agtype,
    (SELECT id FROM cypher('sp_edge', $$ MATCH (n {name:'A'}) RETURN id(n) $$) AS (id agtype)),
    (SELECT id FROM cypher('sp_edge', $$ MATCH (n {name:'B'}) RETURN id(n) $$) AS (id agtype)),
    '"KNOWS"'::agtype, '"any"'::agtype);

-- NULL start (or end) vertex yields no rows (Cypher null semantics: a null
-- endpoint simply produces no match, it is not an error); count 0
SELECT count(*) FROM age_shortest_path(
    '"sp_edge"'::agtype,
    NULL::agtype,
    (SELECT id FROM cypher('sp_edge', $$ MATCH (n {name:'B'}) RETURN id(n) $$) AS (id agtype)));

-- NULL end vertex likewise yields no rows; count 0
SELECT count(*) FROM age_shortest_path(
    '"sp_edge"'::agtype,
    (SELECT id FROM cypher('sp_edge', $$ MATCH (n {name:'A'}) RETURN id(n) $$) AS (id agtype)),
    NULL::agtype);

-- all_shortest_paths with a NULL endpoint also yields no rows; count 0
SELECT count(*) FROM age_all_shortest_paths(
    '"sp_edge"'::agtype,
    (SELECT id FROM cypher('sp_edge', $$ MATCH (n {name:'A'}) RETURN id(n) $$) AS (id agtype)),
    NULL::agtype);

-- a single relationship type may be passed as a one-element array; expected:
-- same as the bare-string form, A..C under KNOWS (length 2); count 1
SELECT count(*) FROM age_shortest_path(
    '"sp_edge"'::agtype,
    (SELECT id FROM cypher('sp_edge', $$ MATCH (n {name:'A'}) RETURN id(n) $$) AS (id agtype)),
    (SELECT id FROM cypher('sp_edge', $$ MATCH (n {name:'C'}) RETURN id(n) $$) AS (id agtype)),
    '["KNOWS"]'::agtype, '"out"'::agtype);

-- multiple relationship types are not yet supported; expected: ERROR
SELECT count(*) FROM age_shortest_path(
    '"sp_edge"'::agtype,
    (SELECT id FROM cypher('sp_edge', $$ MATCH (n {name:'A'}) RETURN id(n) $$) AS (id agtype)),
    (SELECT id FROM cypher('sp_edge', $$ MATCH (n {name:'C'}) RETURN id(n) $$) AS (id agtype)),
    '["KNOWS", "LIKES"]'::agtype, '"out"'::agtype);

-- a non-zero minimum hop count is not yet supported; expected: ERROR
SELECT count(*) FROM age_shortest_path(
    '"sp_edge"'::agtype,
    (SELECT id FROM cypher('sp_edge', $$ MATCH (n {name:'A'}) RETURN id(n) $$) AS (id agtype)),
    (SELECT id FROM cypher('sp_edge', $$ MATCH (n {name:'C'}) RETURN id(n) $$) AS (id agtype)),
    '"KNOWS"'::agtype, '"out"'::agtype, 2::agtype);

-- a minimum hop count of 0 is the default and is accepted; A..C (length 2);
-- count 1
SELECT count(*) FROM age_shortest_path(
    '"sp_edge"'::agtype,
    (SELECT id FROM cypher('sp_edge', $$ MATCH (n {name:'A'}) RETURN id(n) $$) AS (id agtype)),
    (SELECT id FROM cypher('sp_edge', $$ MATCH (n {name:'C'}) RETURN id(n) $$) AS (id agtype)),
    '"KNOWS"'::agtype, '"out"'::agtype, 0::agtype);

-- a graph name that does not exist is an error
SELECT count(*) FROM age_shortest_path('"no_such_graph"'::agtype, '1'::agtype, '2'::agtype);

-- cleanup
SELECT * FROM drop_graph('sp_edge', true);
