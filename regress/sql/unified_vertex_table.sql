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
-- Test 18: SET label operation - error when vertex already has a label
-- Multiple labels are not supported. SET only works on unlabeled vertices.
--
SELECT * FROM cypher('unified_test', $$
    CREATE (:OldLabel {id: 1, name: 'vertex1'})
$$) AS (v agtype);

SELECT * FROM cypher('unified_test', $$
    CREATE (:OldLabel {id: 2, name: 'vertex2'})
$$) AS (v agtype);

-- Verify initial label
SELECT * FROM cypher('unified_test', $$
    MATCH (n:OldLabel)
    RETURN n.id, n.name, label(n) ORDER BY n.id
$$) AS (id agtype, name agtype, lbl agtype);

-- Try to change label on vertex1 - should FAIL because it already has a label
SELECT * FROM cypher('unified_test', $$
    MATCH (n:OldLabel {id: 1})
    SET n:NewLabel
    RETURN n.id, n.name, label(n)
$$) AS (id agtype, name agtype, lbl agtype);

-- Verify vertex1 still has OldLabel (unchanged due to error)
SELECT * FROM cypher('unified_test', $$
    MATCH (n:OldLabel)
    RETURN n.id, n.name, label(n) ORDER BY n.id
$$) AS (id agtype, name agtype, lbl agtype);

--
-- Test 19: REMOVE label operation
-- This tests removing a vertex's label using REMOVE n:Label syntax
--
SELECT * FROM cypher('unified_test', $$
    CREATE (:RemoveTest {id: 1, data: 'test1'})
$$) AS (v agtype);

SELECT * FROM cypher('unified_test', $$
    CREATE (:RemoveTest {id: 2, data: 'test2'})
$$) AS (v agtype);

-- Verify initial label
SELECT * FROM cypher('unified_test', $$
    MATCH (n:RemoveTest)
    RETURN n.id, n.data, label(n) ORDER BY n.id
$$) AS (id agtype, data agtype, lbl agtype);

-- Remove label from vertex1
SELECT * FROM cypher('unified_test', $$
    MATCH (n:RemoveTest {id: 1})
    REMOVE n:RemoveTest
    RETURN n.id, n.data, label(n)
$$) AS (id agtype, data agtype, lbl agtype);

-- Verify vertex1 now has no label (empty string)
SELECT * FROM cypher('unified_test', $$
    MATCH (n {data: 'test1'})
    RETURN n.id, n.data, label(n)
$$) AS (id agtype, data agtype, lbl agtype);

-- Verify vertex2 still has RemoveTest label
SELECT * FROM cypher('unified_test', $$
    MATCH (n:RemoveTest)
    RETURN n.id, n.data, label(n)
$$) AS (id agtype, data agtype, lbl agtype);

-- Verify properties are preserved after label removal
SELECT * FROM cypher('unified_test', $$
    MATCH (n)
    WHERE n.data = 'test1'
    RETURN n.id, n.data, label(n)
$$) AS (id agtype, data agtype, lbl agtype);

--
-- Test 20: SET label with property updates - error when vertex has label
--
SELECT * FROM cypher('unified_test', $$
    CREATE (:CombinedTest {id: 1, val: 'original'})
$$) AS (v agtype);

-- Try to SET label and property - should FAIL because vertex has a label
SELECT * FROM cypher('unified_test', $$
    MATCH (n:CombinedTest {id: 1})
    SET n:CombinedNew, n.val = 'updated'
    RETURN n.id, n.val, label(n)
$$) AS (id agtype, val agtype, lbl agtype);

-- Verify vertex is unchanged
SELECT * FROM cypher('unified_test', $$
    MATCH (n:CombinedTest)
    RETURN n.id, n.val, label(n) ORDER BY n.id
$$) AS (id agtype, val agtype, lbl agtype);

--
-- Test 21: Proper workflow - REMOVE then SET label
-- To change a label, first REMOVE the old one, then SET the new one
--
SELECT * FROM cypher('unified_test', $$
    CREATE (:WorkflowTest {id: 50, val: 'workflow'})
$$) AS (v agtype);

-- First REMOVE the label
SELECT * FROM cypher('unified_test', $$
    MATCH (n:WorkflowTest {id: 50})
    REMOVE n:WorkflowTest
    RETURN n.id, n.val, label(n)
$$) AS (id agtype, val agtype, lbl agtype);

-- Now SET a new label (should work because vertex has no label)
SELECT * FROM cypher('unified_test', $$
    MATCH (n {id: 50})
    SET n:NewWorkflowLabel
    RETURN n.id, n.val, label(n)
$$) AS (id agtype, val agtype, lbl agtype);

-- Verify the new label
SELECT * FROM cypher('unified_test', $$
    MATCH (n:NewWorkflowLabel)
    RETURN n.id, n.val, label(n) ORDER BY n.id
$$) AS (id agtype, val agtype, lbl agtype);

--
-- Test 22: SET label auto-creates label when vertex has no label
--
-- First create and remove label to get unlabeled vertex
SELECT * FROM cypher('unified_test', $$
    CREATE (:TempForAuto {id: 60, name: 'auto_create_test'})
$$) AS (v agtype);

SELECT * FROM cypher('unified_test', $$
    MATCH (n:TempForAuto {id: 60})
    REMOVE n:TempForAuto
    RETURN n.id, n.name, label(n)
$$) AS (id agtype, name agtype, lbl agtype);

-- Now SET a new label that doesn't exist yet (should auto-create)
SELECT * FROM cypher('unified_test', $$
    MATCH (n {id: 60})
    SET n:AutoCreatedLabel
    RETURN n.id, n.name, label(n)
$$) AS (id agtype, name agtype, lbl agtype);

-- Verify the new label exists and the vertex is there
SELECT * FROM cypher('unified_test', $$
    MATCH (n:AutoCreatedLabel)
    RETURN n.id, n.name, label(n)
$$) AS (id agtype, name agtype, lbl agtype);

--
-- Test 23: SET label on vertex with NO label (blank -> labeled)
--
-- First create a vertex with a label, then remove it to get a blank label
SELECT * FROM cypher('unified_test', $$
    CREATE (:TempLabel {id: 100, data: 'unlabeled_test'})
$$) AS (v agtype);

-- Remove the label to make it blank
SELECT * FROM cypher('unified_test', $$
    MATCH (n:TempLabel {id: 100})
    REMOVE n:TempLabel
    RETURN n.id, n.data, label(n)
$$) AS (id agtype, data agtype, lbl agtype);

-- Verify it has no label (blank)
SELECT * FROM cypher('unified_test', $$
    MATCH (n {id: 100})
    RETURN n.id, n.data, label(n)
$$) AS (id agtype, data agtype, lbl agtype);

-- Now SET a label on the unlabeled vertex
SELECT * FROM cypher('unified_test', $$
    MATCH (n {id: 100})
    SET n:FromBlankLabel
    RETURN n.id, n.data, label(n)
$$) AS (id agtype, data agtype, lbl agtype);

-- Verify the label was set
SELECT * FROM cypher('unified_test', $$
    MATCH (n:FromBlankLabel)
    RETURN n.id, n.data, label(n)
$$) AS (id agtype, data agtype, lbl agtype);

--
-- Test 24: REMOVE label on vertex that already has NO label (no-op)
--
-- Create another unlabeled vertex
SELECT * FROM cypher('unified_test', $$
    CREATE (:TempLabel2 {id: 101, data: 'already_blank'})
$$) AS (v agtype);

SELECT * FROM cypher('unified_test', $$
    MATCH (n:TempLabel2 {id: 101})
    REMOVE n:TempLabel2
    RETURN n.id, n.data, label(n)
$$) AS (id agtype, data agtype, lbl agtype);

-- Now try to REMOVE a label from already-unlabeled vertex (should be no-op)
SELECT * FROM cypher('unified_test', $$
    MATCH (n {id: 101})
    REMOVE n:SomeLabel
    RETURN n.id, n.data, label(n)
$$) AS (id agtype, data agtype, lbl agtype);

-- Verify still has no label
SELECT * FROM cypher('unified_test', $$
    MATCH (n {id: 101})
    RETURN n.id, n.data, label(n)
$$) AS (id agtype, data agtype, lbl agtype);

--
-- Test 25: REMOVE with wrong label name (should be no-op)
-- REMOVE should only remove the label if it matches the specified name
--
SELECT * FROM cypher('unified_test', $$
    CREATE (:KeepThisLabel {id: 103, data: 'wrong_label_test'})
$$) AS (v agtype);

-- Try to REMOVE a different label than the vertex has - should be no-op
SELECT * FROM cypher('unified_test', $$
    MATCH (n:KeepThisLabel {id: 103})
    REMOVE n:WrongLabel
    RETURN n.id, n.data, label(n)
$$) AS (id agtype, data agtype, lbl agtype);

-- Verify label is still KeepThisLabel (unchanged)
SELECT * FROM cypher('unified_test', $$
    MATCH (n:KeepThisLabel {id: 103})
    RETURN n.id, n.data, label(n)
$$) AS (id agtype, data agtype, lbl agtype);

-- Now REMOVE with the correct label - should work
SELECT * FROM cypher('unified_test', $$
    MATCH (n:KeepThisLabel {id: 103})
    REMOVE n:KeepThisLabel
    RETURN n.id, n.data, label(n)
$$) AS (id agtype, data agtype, lbl agtype);

-- Verify label is now empty
SELECT * FROM cypher('unified_test', $$
    MATCH (n {id: 103})
    RETURN n.id, n.data, label(n)
$$) AS (id agtype, data agtype, lbl agtype);

--
-- Test 26: SET label to same label - error (vertex already has a label)
--
SELECT * FROM cypher('unified_test', $$
    CREATE (:SameLabel {id: 102, data: 'same_label_test'})
$$) AS (v agtype);

-- SET to the same label it already has - should FAIL
SELECT * FROM cypher('unified_test', $$
    MATCH (n:SameLabel {id: 102})
    SET n:SameLabel
    RETURN n.id, n.data, label(n)
$$) AS (id agtype, data agtype, lbl agtype);

-- Verify label is unchanged
SELECT * FROM cypher('unified_test', $$
    MATCH (n:SameLabel {id: 102})
    RETURN n.id, n.data, label(n)
$$) AS (id agtype, data agtype, lbl agtype);

--
-- Test 27: Error case - SET/REMOVE label on edge (should error)
--
SELECT * FROM cypher('unified_test', $$
    CREATE (:EdgeTest1 {id: 200})-[:CONNECTS]->(:EdgeTest2 {id: 201})
$$) AS (v agtype);

-- Try to SET label on an edge - should fail
SELECT * FROM cypher('unified_test', $$
    MATCH (:EdgeTest1)-[e:CONNECTS]->(:EdgeTest2)
    SET e:NewEdgeLabel
    RETURN e
$$) AS (e agtype);

-- Try to REMOVE label on an edge - should fail
SELECT * FROM cypher('unified_test', $$
    MATCH (:EdgeTest1)-[e:CONNECTS]->(:EdgeTest2)
    REMOVE e:CONNECTS
    RETURN e
$$) AS (e agtype);

--
-- Test 28: Verify id() and properties() optimization
--
-- The optimization avoids rebuilding the full vertex agtype when accessing
-- id() or properties() on a vertex. Instead of:
--   age_id(_agtype_build_vertex(id, _label_name_from_table_oid(labels), properties))
-- It generates:
--   graphid_to_agtype(id)
--
-- And for properties:
--   age_properties(_agtype_build_vertex(...))
-- It generates:
--   properties (direct column access)
--

-- Create test data
SELECT * FROM cypher('unified_test', $$
    CREATE (:OptimizeTest {val: 1}),
           (:OptimizeTest {val: 2}),
           (:OptimizeTest {val: 3})
$$) AS (v agtype);

-- Test that id() works correctly with optimization
SELECT * FROM cypher('unified_test', $$
    MATCH (n:OptimizeTest)
    RETURN id(n), n.val
    ORDER BY n.val
$$) AS (id agtype, val agtype);

-- Test that properties() works correctly with optimization
SELECT * FROM cypher('unified_test', $$
    MATCH (n:OptimizeTest)
    RETURN properties(n), n.val
    ORDER BY n.val
$$) AS (props agtype, val agtype);

-- Test id() in WHERE clause (common optimization target)
SELECT * FROM cypher('unified_test', $$
    MATCH (n:OptimizeTest)
    WHERE id(n) % 10 = 0
    RETURN n.val
$$) AS (val agtype);

-- Test properties() access in expressions
SELECT * FROM cypher('unified_test', $$
    MATCH (n:OptimizeTest)
    WHERE properties(n).val > 1
    RETURN n.val
    ORDER BY n.val
$$) AS (val agtype);

-- Test edge id/properties optimization
SELECT * FROM cypher('unified_test', $$
    CREATE (:OptStart {x: 1})-[:OPT_EDGE {weight: 10}]->(:OptEnd {y: 2})
$$) AS (v agtype);

SELECT * FROM cypher('unified_test', $$
    MATCH (a)-[e:OPT_EDGE]->(b)
    RETURN id(e), properties(e), start_id(e), end_id(e)
$$) AS (eid agtype, props agtype, sid agtype, eid2 agtype);

--
-- Cleanup
--
SELECT drop_graph('unified_test', true);
