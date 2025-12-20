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
-- Cleanup
--
SELECT drop_graph('unified_test', true);
