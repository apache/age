LOAD 'age';
SET search_path TO ag_catalog;

--
-- delete_specific_GRAPH_global_contexts
--

--
SELECT * FROM create_graph('ag_graph_1');
SELECT * FROM cypher('ag_graph_1', $$ CREATE (v:vertex1) RETURN v  $$) AS (v agtype);

SELECT * FROM create_graph('ag_graph_2');
SELECT * FROM cypher('ag_graph_2', $$ CREATE (v:vertex2) RETURN v  $$) AS (v agtype);

SELECT * FROM create_graph('ag_graph_3');
SELECT * FROM cypher('ag_graph_3', $$ CREATE (v:vertex3) RETURN v  $$) AS (v agtype);

-- load contexts using the vertex_stats command
SELECT * FROM cypher('ag_graph_3', $$ MATCH (u) RETURN vertex_stats(u) ORDER BY id(u) $$) AS (result agtype);
SELECT * FROM cypher('ag_graph_2', $$ MATCH (u) RETURN vertex_stats(u) ORDER BY id(u) $$) AS (result agtype);
SELECT * FROM cypher('ag_graph_1', $$ MATCH (u) RETURN vertex_stats(u) ORDER BY id(u) $$) AS (result agtype);

--- loading undefined contexts
--- should throw exception - graph "ag_graph_4" does not exist
SELECT * FROM cypher('ag_graph_4', $$ MATCH (u) RETURN vertex_stats(u) ORDER BY id(u) $$) AS (result agtype);

--- delete with invalid parameter
---should return false
SELECT * FROM cypher('ag_graph_2', $$ RETURN delete_global_graphs('E1337') $$) AS (result agtype);

-- delete ag_graph_2's context
-- should return true
SELECT * FROM cypher('ag_graph_2', $$ RETURN delete_global_graphs('ag_graph_2') $$) AS (result agtype);

-- delete ag_graph_1's context
-- should return true(succeed) because the previous command should not delete the 1st graph's context
SELECT * FROM cypher('ag_graph_1', $$ RETURN delete_global_graphs('ag_graph_1') $$) AS (result agtype);

-- delete ag_graph_3's context
-- should return true(succeed) because the previous commands should not delete the 3rd graph's context
SELECT * FROM cypher('ag_graph_3', $$ RETURN delete_global_graphs('ag_graph_3') $$) AS (result agtype);

-- delete all graphs' context again
-- should return false (did not succeed) for all of them because they are already removed
SELECT * FROM cypher('ag_graph_2', $$ RETURN delete_global_graphs('ag_graph_2') $$) AS (result agtype);
SELECT * FROM cypher('ag_graph_1', $$ RETURN delete_global_graphs('ag_graph_1') $$) AS (result agtype);
SELECT * FROM cypher('ag_graph_3', $$ RETURN delete_global_graphs('ag_graph_3') $$) AS (result agtype);

--- delete uninitialized graph context
--- should throw exception graph "ag_graph_4" does not exist
SELECT * FROM cypher('ag_graph_4', $$ RETURN delete_global_graphs('ag_graph_4') $$) AS (result agtype);

--
-- delete_GRAPH_global_contexts
--

-- load contexts again
SELECT * FROM cypher('ag_graph_3', $$ MATCH (u) RETURN vertex_stats(u) ORDER BY id(u) $$) AS (result agtype);
SELECT * FROM cypher('ag_graph_2', $$ MATCH (u) RETURN vertex_stats(u) ORDER BY id(u) $$) AS (result agtype);
SELECT * FROM cypher('ag_graph_1', $$ MATCH (u) RETURN vertex_stats(u) ORDER BY id(u) $$) AS (result agtype);

-- delete all graph contexts
-- should return true
SELECT * FROM cypher('ag_graph_1', $$ RETURN delete_global_graphs(NULL) $$) AS (result agtype);

-- delete all graphs' context individually
-- should return false for all of them because already removed
SELECT * FROM cypher('ag_graph_1', $$ RETURN delete_global_graphs('ag_graph_1') $$) AS (result agtype);
SELECT * FROM cypher('ag_graph_2', $$ RETURN delete_global_graphs('ag_graph_2') $$) AS (result agtype);
SELECT * FROM cypher('ag_graph_3', $$ RETURN delete_global_graphs('ag_graph_3') $$) AS (result agtype);

--
-- age_vertex_stats
--

--checking validity of vertex_stats
--adding unlabelled vertices to ag_graph_1
SELECT * FROM cypher('ag_graph_1', $$ CREATE (n), (m) $$) as (v agtype);

--adding labelled vertice to graph 2
SELECT * FROM cypher('ag_graph_2', $$ CREATE (:Person) $$) as (v agtype);

---adding edges between nodes
SELECT * FROM cypher('ag_graph_2', $$ MATCH (a:Person), (b:Person) WHERE a.name = 'A' AND b.name = 'B' CREATE (a)-[e:RELTYPE]->(b) RETURN e ORDER BY id(e) $$) as (e agtype);

--checking if vertex stats have been updated along with the new label
--should return 3 vertices
SELECT * FROM cypher('ag_graph_1', $$ MATCH (n) RETURN vertex_stats(n) ORDER BY id(n) $$) AS (result agtype);

--should return 1 vertice and 1 label
SELECT * FROM cypher('ag_graph_2', $$ MATCH (a) RETURN vertex_stats(a) ORDER BY id(a) $$) AS (result agtype);

--
-- graph_stats command
--
-- what's in the current graphs?
SELECT * FROM cypher('ag_graph_1', $$ RETURN graph_stats('ag_graph_1') $$) AS (result agtype);
SELECT * FROM cypher('ag_graph_1', $$ RETURN graph_stats('ag_graph_2') $$) AS (result agtype);
SELECT * FROM cypher('ag_graph_1', $$ RETURN graph_stats('ag_graph_3') $$) AS (result agtype);
-- add some edges
SELECT * FROM cypher('ag_graph_1', $$ CREATE ()-[:knows]->() $$) AS (results agtype);
SELECT * FROM cypher('ag_graph_1', $$ CREATE ()-[:knows]->() $$) AS (results agtype);
SELECT * FROM cypher('ag_graph_1', $$ CREATE ()-[:knows]->() $$) AS (results agtype);
SELECT * FROM cypher('ag_graph_1', $$ CREATE ()-[:knows]->() $$) AS (results agtype);
-- what is there now?
SELECT * FROM cypher('ag_graph_1', $$ RETURN graph_stats('ag_graph_1') $$) AS (result agtype);
-- add some more
SELECT * FROM cypher('ag_graph_1', $$ MATCH (u)-[]->(v) SET u.id = id(u)
                                                        SET v.id = id(v)
                                                        SET u.name = 'u'
                                                        SET v.name = 'v'
                                      RETURN u,v ORDER BY id(u), id(v) $$) AS (u agtype, v agtype);
SELECT * FROM cypher('ag_graph_1', $$ MATCH (u)-[]->(v) MERGE (v)-[:stalks]->(u) $$) AS (result agtype);
SELECT * FROM cypher('ag_graph_1', $$ MATCH (u)-[e]->(v) RETURN u, e, v ORDER BY id(e) $$) AS (u agtype, e agtype, v agtype);
-- what is there now?
SELECT * FROM cypher('ag_graph_1', $$ RETURN graph_stats('ag_graph_1') $$) AS (result agtype);
-- remove some vertices
SELECT * FROM ag_graph_1._ag_label_vertex;
DELETE FROM ag_graph_1._ag_label_vertex WHERE id::text = '281474976710661';
DELETE FROM ag_graph_1._ag_label_vertex WHERE id::text = '281474976710662';
DELETE FROM ag_graph_1._ag_label_vertex WHERE id::text = '281474976710664';
SELECT * FROM ag_graph_1._ag_label_vertex;
SELECT * FROM ag_graph_1._ag_label_edge;
-- The graph_stats query below will produce warnings for the dangling edges
-- created by the DELETE commands above. The warnings appear in nondeterministic
-- order because they come from iterating edge label tables (knows, stalks),
-- so we suppress them with client_min_messages. Without suppression, the
-- output would include these warnings (in some order):
--
-- WARNING:  edge: [id: 1125899906842626, start: 281474976710661, end: 281474976710662, label: knows] start and end vertices not found
-- WARNING:  ignored malformed or dangling edge
-- WARNING:  edge: [id: 1125899906842627, start: 281474976710663, end: 281474976710664, label: knows] end vertex not found
-- WARNING:  ignored malformed or dangling edge
-- WARNING:  edge: [id: 1407374883553282, start: 281474976710662, end: 281474976710661, label: stalks] start and end vertices not found
-- WARNING:  ignored malformed or dangling edge
-- WARNING:  edge: [id: 1407374883553283, start: 281474976710664, end: 281474976710663, label: stalks] start vertex not found
-- WARNING:  ignored malformed or dangling edge
--
-- The result row validates that graph_stats handled the dangling edges correctly.
SET client_min_messages = error;
SELECT * FROM cypher('ag_graph_1', $$ RETURN graph_stats('ag_graph_1') $$) AS (result agtype);
RESET client_min_messages;

--drop graphs

SELECT * FROM drop_graph('ag_graph_1', true);
SELECT * FROM drop_graph('ag_graph_2', true);
SELECT * FROM drop_graph('ag_graph_3', true);

-----------------------------------------------------------------------------------------------------------------------------
--
-- VLE cache invalidation tests
--
-- These tests verify that the graph version counter properly invalidates
-- the VLE hash table cache when the graph is mutated, and that thin
-- entry lazy property fetch returns correct data.
--

-- Setup: create a graph with a chain a->b->c->d
SELECT * FROM create_graph('vle_cache_test');

SELECT * FROM cypher('vle_cache_test', $$
  CREATE (a:Node {name: 'a'})-[:Edge]->(b:Node {name: 'b'})-[:Edge]->(c:Node {name: 'c'})-[:Edge]->(d:Node {name: 'd'})
$$) AS (v agtype);

-- VLE query: find all paths from a's neighbors (should find b, b->c, b->c->d)
SELECT * FROM cypher('vle_cache_test', $$
  MATCH (a:Node {name: 'a'})-[:Edge*1..3]->(n:Node)
  RETURN n.name
  ORDER BY n.name
$$) AS (name agtype);

-- Now add a new node e connected to d. This should invalidate the cache.
SELECT * FROM cypher('vle_cache_test', $$
  MATCH (d:Node {name: 'd'})
  CREATE (d)-[:Edge]->(:Node {name: 'e'})
$$) AS (v agtype);

-- VLE query again: should now also find e via a->b->c->d->e (4 hops won't reach,
-- but d->e is 1 hop from d, and a->b->c->d->e would be 4 hops from a).
-- Increase range to *1..4 to include e
SELECT * FROM cypher('vle_cache_test', $$
  MATCH (a:Node {name: 'a'})-[:Edge*1..4]->(n:Node)
  RETURN n.name
  ORDER BY n.name
$$) AS (name agtype);

-- Test cache invalidation on DELETE: remove node c and its edges
SELECT * FROM cypher('vle_cache_test', $$
  MATCH (c:Node {name: 'c'})
  DETACH DELETE c
$$) AS (v agtype);

-- VLE query: should only find b now (c is gone, so b->c path is broken)
SELECT * FROM cypher('vle_cache_test', $$
  MATCH (a:Node {name: 'a'})-[:Edge*1..4]->(n:Node)
  RETURN n.name
  ORDER BY n.name
$$) AS (name agtype);

-- Test cache invalidation on SET: change b's name property
SELECT * FROM cypher('vle_cache_test', $$
  MATCH (b:Node {name: 'b'})
  SET b.name = 'b_modified'
  RETURN b.name
$$) AS (name agtype);

-- VLE query: verify the updated property is returned via lazy fetch
SELECT * FROM cypher('vle_cache_test', $$
  MATCH (a:Node {name: 'a'})-[:Edge*1..4]->(n:Node)
  RETURN n.name
  ORDER BY n.name
$$) AS (name agtype);

-- Test VLE with edge properties (exercises thin entry edge property fetch)
SELECT * FROM drop_graph('vle_cache_test', true);
SELECT * FROM create_graph('vle_cache_test2');

SELECT * FROM cypher('vle_cache_test2', $$
  CREATE (a:N {name: 'a'})-[:E {weight: 1}]->(b:N {name: 'b'})-[:E {weight: 2}]->(c:N {name: 'c'})
$$) AS (v agtype);

-- VLE path output to verify edge properties are fetched correctly via
-- thin entry lazy fetch. Returning the full path forces build_path()
-- to call get_edge_entry_properties() for each edge in the result.
-- The output must contain the correct weight values (1 and 2).
SELECT * FROM cypher('vle_cache_test2', $$
  MATCH p=(a:N {name: 'a'})-[:E *1..2]->(n:N)
  RETURN p
  ORDER BY n.name
$$) AS (p agtype);

-- VLE edge properties via UNWIND + relationships() to individually verify
-- each edge's properties are correctly fetched from the heap via TID.
SELECT * FROM cypher('vle_cache_test2', $$
  MATCH p=(a:N {name: 'a'})-[:E *1..2]->(n:N)
  WITH p, n
  UNWIND relationships(p) AS e
  RETURN n.name, e.weight
  ORDER BY n.name, e.weight
$$) AS (name agtype, weight agtype);

-- Cleanup
SELECT * FROM drop_graph('vle_cache_test2', true);

-----------------------------------------------------------------------------------------------------------------------------
--
-- VLE cache invalidation via direct SQL (trigger tests)
--
-- These tests verify that the SQL trigger (age_invalidate_graph_cache)
-- properly invalidates the VLE cache when label tables are mutated
-- via direct SQL INSERT, UPDATE, DELETE, and TRUNCATE — not via Cypher.
--

-- Setup: create graph with a chain a->b->c using Cypher
SELECT * FROM create_graph('vle_trigger_test');

SELECT * FROM cypher('vle_trigger_test', $$
  CREATE (a:Node {name: 'a'})-[:Edge]->(b:Node {name: 'b'})-[:Edge]->(c:Node {name: 'c'})
$$) AS (v agtype);

-- Prime the VLE cache: find all nodes reachable from a
SELECT * FROM cypher('vle_trigger_test', $$
  MATCH (a:Node {name: 'a'})-[:Edge*1..3]->(n:Node)
  RETURN n.name
  ORDER BY n.name
$$) AS (name agtype);

-- Direct SQL INSERT on vertex: add a new vertex via SQL.
-- This should fire the trigger and invalidate the VLE cache.
-- Use _graphid() with the label's id and nextval for the entry id.
INSERT INTO vle_trigger_test."Node" (id, properties)
SELECT ag_catalog._graphid(l.id,
           nextval('vle_trigger_test."Node_id_seq"'::regclass)),
       '{"name": "d"}'::agtype
FROM ag_catalog.ag_label l
JOIN ag_catalog.ag_graph g ON g.graphid = l.graph
WHERE g.name = 'vle_trigger_test' AND l.name = 'Node';

-- VLE query: results should be unchanged (d has no edges) but cache was rebuilt
SELECT * FROM cypher('vle_trigger_test', $$
  MATCH (a:Node {name: 'a'})-[:Edge*1..3]->(n:Node)
  RETURN n.name
  ORDER BY n.name
$$) AS (name agtype);

-- Direct SQL UPDATE on vertex: change b's name to 'b_updated'
-- This should fire the trigger and invalidate the VLE cache.
UPDATE vle_trigger_test."Node"
SET properties = '{"name": "b_updated"}'::agtype
WHERE properties @> '{"name": "b"}'::agtype;

-- VLE query: verify updated property is visible (cache invalidated by trigger)
SELECT * FROM cypher('vle_trigger_test', $$
  MATCH (a:Node {name: 'a'})-[:Edge*1..3]->(n:Node)
  RETURN n.name
  ORDER BY n.name
$$) AS (name agtype);

-- Direct SQL DELETE on edge: remove the edge from b_updated to c
DELETE FROM vle_trigger_test."Edge"
WHERE end_id = (SELECT id FROM vle_trigger_test."Node"
                WHERE properties @> '{"name": "c"}'::agtype);

-- VLE query: only b_updated reachable now (edge to c is gone)
SELECT * FROM cypher('vle_trigger_test', $$
  MATCH (a:Node {name: 'a'})-[:Edge*1..3]->(n:Node)
  RETURN n.name
  ORDER BY n.name
$$) AS (name agtype);

-- Direct SQL TRUNCATE on edge table: remove all edges
TRUNCATE vle_trigger_test."Edge";

-- VLE query: no edges exist, should return no rows
SELECT * FROM cypher('vle_trigger_test', $$
  MATCH (a:Node {name: 'a'})-[:Edge*1..3]->(n:Node)
  RETURN n.name
  ORDER BY n.name
$$) AS (name agtype);

-- Cleanup
SELECT * FROM drop_graph('vle_trigger_test', true);

-----------------------------------------------------------------------------------------------------------------------------
--
-- End of tests
--

