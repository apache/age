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
-- Regression tests for the unified vertex table architecture
--
-- Tests verify:
--   1. Labels column correctly stores label table OID
--   2. Label retrieval from labels column works correctly
--   3. Multiple labels coexist in single table
--   4. startNode/endNode correctly retrieve vertex labels
--   5. VLE correctly handles label retrieval
--

LOAD 'age';
SET search_path TO ag_catalog;

--
-- Test 1: Basic unified table structure verification
--
SELECT create_graph('unified_test');

-- Create vertices with different labels
SELECT * FROM cypher('unified_test', $$
    CREATE (:Person {name: 'Alice'})
$$) AS (v agtype);

SELECT * FROM cypher('unified_test', $$
    CREATE (:Company {name: 'Acme Corp'})
$$) AS (v agtype);

SELECT * FROM cypher('unified_test', $$
    CREATE (:Location {name: 'Seattle'})
$$) AS (v agtype);

-- Verify all vertices are in the unified table
SELECT COUNT(*) AS vertex_count FROM unified_test._ag_label_vertex;

-- Verify labels column contains different OIDs for different labels
SELECT COUNT(DISTINCT labels) AS distinct_label_count FROM unified_test._ag_label_vertex;

-- Verify we can filter by label using the labels column
SELECT id, properties FROM unified_test._ag_label_vertex
WHERE labels = 'unified_test."Person"'::regclass::oid;

SELECT id, properties FROM unified_test._ag_label_vertex
WHERE labels = 'unified_test."Company"'::regclass::oid;

SELECT id, properties FROM unified_test._ag_label_vertex
WHERE labels = 'unified_test."Location"'::regclass::oid;

--
-- Test 2: Label retrieval via Cypher MATCH
--
SELECT * FROM cypher('unified_test', $$
    MATCH (p:Person) RETURN p.name ORDER BY p.name
$$) AS (name agtype);

SELECT * FROM cypher('unified_test', $$
    MATCH (c:Company) RETURN c.name ORDER BY c.name
$$) AS (name agtype);

SELECT * FROM cypher('unified_test', $$
    MATCH (n) RETURN labels(n), n.name ORDER BY n.name
$$) AS (label agtype, name agtype);

--
-- Test 3: startNode/endNode label retrieval
--
SELECT * FROM cypher('unified_test', $$
    CREATE (:Person {name: 'Bob'})-[:WORKS_AT]->(:Company {name: 'Tech Inc'})
$$) AS (v agtype);

SELECT * FROM cypher('unified_test', $$
    CREATE (:Person {name: 'Carol'})-[:LIVES_IN]->(:Location {name: 'Portland'})
$$) AS (v agtype);

-- Verify startNode returns correct label
SELECT * FROM cypher('unified_test', $$
    MATCH ()-[e:WORKS_AT]->()
    RETURN startNode(e).name AS employee, label(startNode(e)) AS emp_label,
           endNode(e).name AS company, label(endNode(e)) AS comp_label
$$) AS (employee agtype, emp_label agtype, company agtype, comp_label agtype);

SELECT * FROM cypher('unified_test', $$
    MATCH ()-[e:LIVES_IN]->()
    RETURN startNode(e).name AS person, label(startNode(e)) AS person_label,
           endNode(e).name AS location, label(endNode(e)) AS loc_label
$$) AS (person agtype, person_label agtype, location agtype, loc_label agtype);

--
-- Test 4: Unlabeled vertices (default vertex label)
--
SELECT * FROM cypher('unified_test', $$
    CREATE ({type: 'unlabeled_1'})
$$) AS (v agtype);

SELECT * FROM cypher('unified_test', $$
    CREATE ({type: 'unlabeled_2'})
$$) AS (v agtype);

-- Verify unlabeled vertices have empty string label
SELECT * FROM cypher('unified_test', $$
    MATCH (n) WHERE n.type IS NOT NULL
    RETURN n.type, label(n) ORDER BY n.type
$$) AS (type agtype, label agtype);

-- Verify labels column for default vertices points to unified table
SELECT COUNT(*) AS unlabeled_count FROM unified_test._ag_label_vertex
WHERE labels = 'unified_test._ag_label_vertex'::regclass::oid;

--
-- Test 5: Mixed label queries (MATCH without label)
--
SELECT * FROM cypher('unified_test', $$
    MATCH (n)
    RETURN label(n), count(*) AS cnt
    ORDER BY label(n)
$$) AS (label agtype, cnt agtype);

--
-- Test 6: Label with SET operations
--
SELECT * FROM cypher('unified_test', $$
    MATCH (p:Person {name: 'Alice'})
    SET p.age = 30
    RETURN p.name, p.age, label(p)
$$) AS (name agtype, age agtype, label agtype);

-- Verify label is preserved after SET
SELECT * FROM cypher('unified_test', $$
    MATCH (p:Person {name: 'Alice'})
    RETURN label(p)
$$) AS (label agtype);

--
-- Test 7: MERGE with labels
--
SELECT * FROM cypher('unified_test', $$
    MERGE (d:Department {name: 'Engineering'})
    RETURN d.name, label(d)
$$) AS (name agtype, label agtype);

SELECT * FROM cypher('unified_test', $$
    MERGE (d:Department {name: 'Engineering'})
    RETURN d.name, label(d)
$$) AS (name agtype, label agtype);

-- Verify only one Department vertex exists
SELECT * FROM cypher('unified_test', $$
    MATCH (d:Department)
    RETURN count(d)
$$) AS (cnt agtype);

--
-- Test 8: DELETE preserves labels column integrity
--
SELECT * FROM cypher('unified_test', $$
    CREATE (:Temporary {id: 1})
$$) AS (v agtype);

SELECT * FROM cypher('unified_test', $$
    CREATE (:Temporary {id: 2})
$$) AS (v agtype);

-- Verify vertices exist
SELECT * FROM cypher('unified_test', $$
    MATCH (t:Temporary)
    RETURN t.id ORDER BY t.id
$$) AS (id agtype);

-- Delete one
SELECT * FROM cypher('unified_test', $$
    MATCH (t:Temporary {id: 1})
    DELETE t
$$) AS (v agtype);

-- Verify remaining vertex still has correct label
SELECT * FROM cypher('unified_test', $$
    MATCH (t:Temporary)
    RETURN t.id, label(t)
$$) AS (id agtype, label agtype);

--
-- Test 9: VLE with label retrieval
--
SELECT * FROM cypher('unified_test', $$
    CREATE (:Node {name: 'a'})-[:CONNECTED]->(:Node {name: 'b'})-[:CONNECTED]->(:Node {name: 'c'})
$$) AS (v agtype);

-- Test that VLE path returns vertices with correct labels
SELECT * FROM cypher('unified_test', $$
    MATCH p = (n:Node {name: 'a'})-[:CONNECTED*1..2]->(m)
    RETURN label(n), label(m)
    ORDER BY label(m)
$$) AS (start_label agtype, end_label agtype);

--
-- Test 10: _label_name_from_table_oid function
--
-- Get a label table OID and verify function works
SELECT ag_catalog._label_name_from_table_oid('unified_test."Person"'::regclass::oid);
SELECT ag_catalog._label_name_from_table_oid('unified_test."Company"'::regclass::oid);
SELECT ag_catalog._label_name_from_table_oid('unified_test."Location"'::regclass::oid);

--
-- Test 11: Verify all labels coexist in unified table with distinct OIDs
--
SELECT * FROM cypher('unified_test', $$
    CREATE (:LabelA {val: 1})
$$) AS (v agtype);

SELECT * FROM cypher('unified_test', $$
    CREATE (:LabelB {val: 2})
$$) AS (v agtype);

SELECT * FROM cypher('unified_test', $$
    CREATE (:LabelC {val: 3})
$$) AS (v agtype);

-- Verify all have distinct label OIDs (count should equal 3)
SELECT COUNT(DISTINCT labels) AS distinct_labels FROM unified_test._ag_label_vertex
WHERE properties::text LIKE '%val%';

--
-- Test 12: Index scan optimization for vertex_exists()
-- This exercises the systable_beginscan path in vertex_exists()
--
SELECT * FROM cypher('unified_test', $$
    CREATE (:IndexTest {id: 100})
$$) AS (v agtype);

SELECT * FROM cypher('unified_test', $$
    CREATE (:IndexTest {id: 101})
$$) AS (v agtype);

SELECT * FROM cypher('unified_test', $$
    CREATE (:IndexTest {id: 102})
$$) AS (v agtype);

-- DETACH DELETE exercises vertex_exists() to check vertex validity
SELECT * FROM cypher('unified_test', $$
    MATCH (n:IndexTest {id: 100})
    DETACH DELETE n
$$) AS (v agtype);

-- Verify vertex was deleted and others remain
SELECT * FROM cypher('unified_test', $$
    MATCH (n:IndexTest)
    RETURN n.id ORDER BY n.id
$$) AS (id agtype);

-- Multiple deletes to exercise index scan repeatedly
SELECT * FROM cypher('unified_test', $$
    MATCH (n:IndexTest)
    DELETE n
$$) AS (v agtype);

-- Verify all IndexTest vertices are gone
SELECT * FROM cypher('unified_test', $$
    MATCH (n:IndexTest)
    RETURN count(n)
$$) AS (cnt agtype);

--
-- Test 13: Index scan optimization for get_vertex() via startNode/endNode
-- This exercises the systable_beginscan path in get_vertex()
--
SELECT * FROM cypher('unified_test', $$
    CREATE (a:GetVertexTest {name: 'source1'})-[:LINK]->(b:GetVertexTest {name: 'target1'})
$$) AS (v agtype);

SELECT * FROM cypher('unified_test', $$
    CREATE (a:GetVertexTest {name: 'source2'})-[:LINK]->(b:GetVertexTest {name: 'target2'})
$$) AS (v agtype);

SELECT * FROM cypher('unified_test', $$
    CREATE (a:GetVertexTest {name: 'source3'})-[:LINK]->(b:GetVertexTest {name: 'target3'})
$$) AS (v agtype);

-- Multiple startNode/endNode calls exercise get_vertex() with index scans
SELECT * FROM cypher('unified_test', $$
    MATCH ()-[e:LINK]->()
    RETURN startNode(e).name AS src, endNode(e).name AS tgt,
           label(startNode(e)) AS src_label, label(endNode(e)) AS tgt_label
    ORDER BY src
$$) AS (src agtype, tgt agtype, src_label agtype, tgt_label agtype);

-- Chain of edges to test repeated get_vertex calls
SELECT * FROM cypher('unified_test', $$
    MATCH (a:GetVertexTest {name: 'target1'})
    CREATE (a)-[:CHAIN]->(:GetVertexTest {name: 'chain1'})-[:CHAIN]->(:GetVertexTest {name: 'chain2'})
$$) AS (v agtype);

SELECT * FROM cypher('unified_test', $$
    MATCH ()-[e:CHAIN]->()
    RETURN startNode(e).name, endNode(e).name
    ORDER BY startNode(e).name
$$) AS (src agtype, tgt agtype);

--
-- Test 14: Index scan optimization for process_delete_list()
-- This exercises the F_INT8EQ fix and systable_beginscan in DELETE
--
SELECT * FROM cypher('unified_test', $$
    CREATE (:DeleteTest {seq: 1}), (:DeleteTest {seq: 2}), (:DeleteTest {seq: 3}),
           (:DeleteTest {seq: 4}), (:DeleteTest {seq: 5})
$$) AS (v agtype);

-- Verify vertices exist
SELECT * FROM cypher('unified_test', $$
    MATCH (n:DeleteTest)
    RETURN n.seq ORDER BY n.seq
$$) AS (seq agtype);

-- Delete specific vertex by property (exercises index lookup)
SELECT * FROM cypher('unified_test', $$
    MATCH (n:DeleteTest {seq: 3})
    DELETE n
$$) AS (v agtype);

-- Verify correct vertex was deleted
SELECT * FROM cypher('unified_test', $$
    MATCH (n:DeleteTest)
    RETURN n.seq ORDER BY n.seq
$$) AS (seq agtype);

-- Delete with edges (exercises process_delete_list with edge cleanup)
SELECT * FROM cypher('unified_test', $$
    MATCH (a:DeleteTest {seq: 1})
    CREATE (a)-[:DEL_EDGE]->(:DeleteTest {seq: 10})
$$) AS (v agtype);

SELECT * FROM cypher('unified_test', $$
    MATCH (n:DeleteTest {seq: 1})
    DETACH DELETE n
$$) AS (v agtype);

-- Verify vertex and edge were deleted
SELECT * FROM cypher('unified_test', $$
    MATCH (n:DeleteTest)
    RETURN n.seq ORDER BY n.seq
$$) AS (seq agtype);

--
-- Test 15: Index scan optimization for process_update_list()
-- This exercises the systable_beginscan in SET/REMOVE operations
--
SELECT * FROM cypher('unified_test', $$
    CREATE (:UpdateTest {id: 1, val: 'original1'}),
           (:UpdateTest {id: 2, val: 'original2'}),
           (:UpdateTest {id: 3, val: 'original3'})
$$) AS (v agtype);

-- Single SET operation
SELECT * FROM cypher('unified_test', $$
    MATCH (n:UpdateTest {id: 1})
    SET n.val = 'updated1'
    RETURN n.id, n.val
$$) AS (id agtype, val agtype);

-- Multiple SET operations in one query (exercises repeated index lookups)
SELECT * FROM cypher('unified_test', $$
    MATCH (n:UpdateTest)
    SET n.modified = true
    RETURN n.id, n.val, n.modified ORDER BY n.id
$$) AS (id agtype, val agtype, modified agtype);

-- SET with property addition
SELECT * FROM cypher('unified_test', $$
    MATCH (n:UpdateTest {id: 2})
    SET n.extra = 'new_property', n.val = 'updated2'
    RETURN n.id, n.val, n.extra
$$) AS (id agtype, val agtype, extra agtype);

-- REMOVE property operation
SELECT * FROM cypher('unified_test', $$
    MATCH (n:UpdateTest {id: 3})
    REMOVE n.val
    RETURN n.id, n.val, n.modified
$$) AS (id agtype, val agtype, modified agtype);

-- Verify final state of all UpdateTest vertices
SELECT * FROM cypher('unified_test', $$
    MATCH (n:UpdateTest)
    RETURN n ORDER BY n.id
$$) AS (n agtype);

--
-- Test 16: OID caching in _label_name_from_table_oid()
-- Repeated calls should use cache after first lookup
--
-- Call multiple times to exercise cache hit path
SELECT ag_catalog._label_name_from_table_oid('unified_test."Person"'::regclass::oid);
SELECT ag_catalog._label_name_from_table_oid('unified_test."Person"'::regclass::oid);
SELECT ag_catalog._label_name_from_table_oid('unified_test."Company"'::regclass::oid);
SELECT ag_catalog._label_name_from_table_oid('unified_test."Company"'::regclass::oid);
SELECT ag_catalog._label_name_from_table_oid('unified_test."Location"'::regclass::oid);
SELECT ag_catalog._label_name_from_table_oid('unified_test."Location"'::regclass::oid);

-- Call with unified table OID (default vertex label case)
SELECT ag_catalog._label_name_from_table_oid('unified_test._ag_label_vertex'::regclass::oid);

-- Verify label function also benefits from caching (exercises full path)
SELECT * FROM cypher('unified_test', $$
    MATCH (p:Person)
    RETURN label(p), label(p), label(p)
$$) AS (l1 agtype, l2 agtype, l3 agtype);

--
-- Test 17: Combined operations stress test
-- Multiple operations in sequence to verify optimizations work together
--
SELECT * FROM cypher('unified_test', $$
    CREATE (a:StressTest {id: 1})-[:ST_EDGE]->(b:StressTest {id: 2})
$$) AS (v agtype);

-- startNode/endNode (get_vertex index scan)
SELECT * FROM cypher('unified_test', $$
    MATCH ()-[e:ST_EDGE]->()
    RETURN startNode(e).id, endNode(e).id
$$) AS (start_id agtype, end_id agtype);

-- SET (process_update_list index scan)
SELECT * FROM cypher('unified_test', $$
    MATCH (n:StressTest)
    SET n.updated = true
    RETURN n.id, n.updated ORDER BY n.id
$$) AS (id agtype, updated agtype);

-- label() calls (OID cache)
SELECT * FROM cypher('unified_test', $$
    MATCH (n:StressTest)
    RETURN n.id, label(n) ORDER BY n.id
$$) AS (id agtype, lbl agtype);

-- DETACH DELETE (vertex_exists + process_delete_list index scans)
SELECT * FROM cypher('unified_test', $$
    MATCH (n:StressTest)
    DETACH DELETE n
$$) AS (v agtype);

-- Verify cleanup
SELECT * FROM cypher('unified_test', $$
    MATCH (n:StressTest)
    RETURN count(n)
$$) AS (cnt agtype);

--
-- Cleanup
--
SELECT drop_graph('unified_test', true);
