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
-- Test Privileges
--

--
-- Setup: Create test graph and data as superuser
--
SELECT create_graph('security_test');

-- Create test vertices
SELECT * FROM cypher('security_test', $$
    CREATE (:Person {name: 'Alice', age: 30})
$$) AS (a agtype);

SELECT * FROM cypher('security_test', $$
    CREATE (:Person {name: 'Bob', age: 25})
$$) AS (a agtype);

SELECT * FROM cypher('security_test', $$
    CREATE (:Document {title: 'Secret', content: 'classified'})
$$) AS (a agtype);

-- Create test edges
SELECT * FROM cypher('security_test', $$
    MATCH (a:Person {name: 'Alice'}), (b:Person {name: 'Bob'})
    CREATE (a)-[:KNOWS {since: 2020}]->(b)
$$) AS (a agtype);

SELECT * FROM cypher('security_test', $$
    MATCH (a:Person {name: 'Alice'}), (d:Document)
    CREATE (a)-[:OWNS]->(d)
$$) AS (a agtype);

--
-- Create test roles with different permission levels
--

-- Role with only SELECT (read-only)
CREATE ROLE security_test_readonly LOGIN;
GRANT USAGE ON SCHEMA security_test TO security_test_readonly;
GRANT SELECT ON ALL TABLES IN SCHEMA security_test TO security_test_readonly;
GRANT USAGE ON SCHEMA ag_catalog TO security_test_readonly;
GRANT SELECT ON ALL TABLES IN SCHEMA ag_catalog TO security_test_readonly;

-- Role with SELECT and INSERT
CREATE ROLE security_test_insert LOGIN;
GRANT USAGE ON SCHEMA security_test TO security_test_insert;
GRANT SELECT, INSERT ON ALL TABLES IN SCHEMA security_test TO security_test_insert;
GRANT USAGE ON SCHEMA ag_catalog TO security_test_insert;
GRANT SELECT ON ALL TABLES IN SCHEMA ag_catalog TO security_test_insert;
-- Grant sequence usage for ID generation
GRANT USAGE ON ALL SEQUENCES IN SCHEMA security_test TO security_test_insert;

-- Role with SELECT and UPDATE
CREATE ROLE security_test_update LOGIN;
GRANT USAGE ON SCHEMA security_test TO security_test_update;
GRANT SELECT, UPDATE ON ALL TABLES IN SCHEMA security_test TO security_test_update;
GRANT USAGE ON SCHEMA ag_catalog TO security_test_update;
GRANT SELECT ON ALL TABLES IN SCHEMA ag_catalog TO security_test_update;

-- Role with SELECT and DELETE
CREATE ROLE security_test_delete LOGIN;
GRANT USAGE ON SCHEMA security_test TO security_test_delete;
GRANT SELECT, DELETE ON ALL TABLES IN SCHEMA security_test TO security_test_delete;
GRANT USAGE ON SCHEMA ag_catalog TO security_test_delete;
GRANT SELECT ON ALL TABLES IN SCHEMA ag_catalog TO security_test_delete;

CREATE ROLE security_test_detach_delete LOGIN;
GRANT USAGE ON SCHEMA security_test TO security_test_detach_delete;
GRANT SELECT ON ALL TABLES IN SCHEMA security_test TO security_test_detach_delete;
GRANT DELETE ON security_test."Person" TO security_test_detach_delete;
GRANT USAGE ON SCHEMA ag_catalog TO security_test_detach_delete;
GRANT SELECT ON ALL TABLES IN SCHEMA ag_catalog TO security_test_detach_delete;

-- Role with all permissions
CREATE ROLE security_test_full LOGIN;
GRANT USAGE ON SCHEMA security_test TO security_test_full;
GRANT ALL ON ALL TABLES IN SCHEMA security_test TO security_test_full;
GRANT USAGE ON SCHEMA ag_catalog TO security_test_full;
GRANT SELECT ON ALL TABLES IN SCHEMA ag_catalog TO security_test_full;
GRANT USAGE ON ALL SEQUENCES IN SCHEMA security_test TO security_test_full;

-- Role with NO SELECT on graph tables (to test read failures)
CREATE ROLE security_test_noread LOGIN;
GRANT USAGE ON SCHEMA security_test TO security_test_noread;
GRANT USAGE ON SCHEMA ag_catalog TO security_test_noread;
GRANT SELECT ON ALL TABLES IN SCHEMA ag_catalog TO security_test_noread;
-- No SELECT on security_test tables

-- ============================================================================
-- PART 1: SELECT Permission Tests - Failure Cases (No Read Permission)
-- ============================================================================

SET ROLE security_test_noread;

-- Test: MATCH on vertices should fail without SELECT permission
SELECT * FROM cypher('security_test', $$
    MATCH (p:Person) RETURN p.name
$$) AS (name agtype);

-- Test: MATCH on edges should fail without SELECT permission
SELECT * FROM cypher('security_test', $$
    MATCH ()-[k:KNOWS]->() RETURN k
$$) AS (k agtype);

-- Test: MATCH with path should fail
SELECT * FROM cypher('security_test', $$
    MATCH (a)-[e]->(b) RETURN a, e, b
$$) AS (a agtype, e agtype, b agtype);

RESET ROLE;

-- Create role with SELECT only on base label tables, not child labels
-- NOTE: PostgreSQL inheritance allows access to child table rows when querying
-- through a parent table. This is expected behavior - SELECT on _ag_label_vertex
-- allows reading all vertices (including Person, Document) via inheritance.
CREATE ROLE security_test_base_only LOGIN;
GRANT USAGE ON SCHEMA security_test TO security_test_base_only;
GRANT USAGE ON SCHEMA ag_catalog TO security_test_base_only;
GRANT SELECT ON ALL TABLES IN SCHEMA ag_catalog TO security_test_base_only;
-- Only grant SELECT on base tables, NOT on Person, Document, KNOWS, OWNS
GRANT SELECT ON security_test._ag_label_vertex TO security_test_base_only;
GRANT SELECT ON security_test._ag_label_edge TO security_test_base_only;

SET ROLE security_test_base_only;

-- Test: MATCH (n) succeeds because PostgreSQL inheritance allows access to child rows
-- when querying through parent table. Permission on _ag_label_vertex grants read
-- access to all vertices via inheritance hierarchy.
SELECT * FROM cypher('security_test', $$
    MATCH (n) RETURN n
$$) AS (n agtype);

-- Test: MATCH ()-[e]->() succeeds via inheritance (same reason as above)
SELECT * FROM cypher('security_test', $$
    MATCH ()-[e]->() RETURN e
$$) AS (e agtype);

-- ============================================================================
-- PART 2: SELECT Permission Tests - Success Cases (Read-Only Role)
-- ============================================================================

RESET ROLE;
SET ROLE security_test_readonly;

-- Test: MATCH should succeed with SELECT permission
SELECT * FROM cypher('security_test', $$
    MATCH (p:Person) RETURN p.name ORDER BY p.name
$$) AS (name agtype);

-- Test: MATCH with edges should succeed
SELECT * FROM cypher('security_test', $$
    MATCH (a:Person)-[k:KNOWS]->(b:Person)
    RETURN a.name, b.name
$$) AS (a agtype, b agtype);

-- Test: MATCH across multiple labels should succeed
SELECT * FROM cypher('security_test', $$
    MATCH (p:Person)-[:OWNS]->(d:Document)
    RETURN p.name, d.title
$$) AS (person agtype, doc agtype);

-- ============================================================================
-- PART 3: INSERT Permission Tests (CREATE clause)
-- ============================================================================

-- Test: CREATE should fail with only SELECT permission
SELECT * FROM cypher('security_test', $$
    CREATE (:Person {name: 'Charlie'})
$$) AS (a agtype);

-- Test: CREATE edge should fail
SELECT * FROM cypher('security_test', $$
    MATCH (a:Person {name: 'Alice'}), (b:Person {name: 'Bob'})
    CREATE (a)-[:FRIENDS]->(b)
$$) AS (a agtype);

RESET ROLE;
SET ROLE security_test_insert;

-- Test: CREATE vertex should succeed with INSERT permission
SELECT * FROM cypher('security_test', $$
    CREATE (:Person {name: 'Charlie', age: 35})
$$) AS (a agtype);

-- Test: CREATE edge should succeed with INSERT permission
SELECT * FROM cypher('security_test', $$
    MATCH (a:Person {name: 'Charlie'}), (b:Person {name: 'Alice'})
    CREATE (a)-[:KNOWS {since: 2023}]->(b)
$$) AS (a agtype);

-- Verify the inserts worked
SELECT * FROM cypher('security_test', $$
    MATCH (p:Person {name: 'Charlie'}) RETURN p.name, p.age
$$) AS (name agtype, age agtype);

-- ============================================================================
-- PART 4: UPDATE Permission Tests (SET clause)
-- ============================================================================

RESET ROLE;
SET ROLE security_test_readonly;

-- Test: SET should fail with only SELECT permission
SELECT * FROM cypher('security_test', $$
    MATCH (p:Person {name: 'Alice'})
    SET p.age = 31
    RETURN p
$$) AS (p agtype);

-- Test: SET on edge should fail
SELECT * FROM cypher('security_test', $$
    MATCH ()-[k:KNOWS]->()
    SET k.since = 2021
    RETURN k
$$) AS (k agtype);

RESET ROLE;
SET ROLE security_test_update;

-- Test: SET should succeed with UPDATE permission
SELECT * FROM cypher('security_test', $$
    MATCH (p:Person {name: 'Alice'})
    SET p.age = 31
    RETURN p.name, p.age
$$) AS (name agtype, age agtype);

-- Test: SET on edge should succeed
SELECT * FROM cypher('security_test', $$
    MATCH (a:Person {name: 'Alice'})-[k:KNOWS]->(b:Person {name: 'Bob'})
    SET k.since = 2019
    RETURN k.since
$$) AS (since agtype);

-- Test: SET with map update should succeed
SELECT * FROM cypher('security_test', $$
    MATCH (p:Person {name: 'Bob'})
    SET p += {hobby: 'reading'}
    RETURN p.name, p.hobby
$$) AS (name agtype, hobby agtype);

-- ============================================================================
-- PART 5: UPDATE Permission Tests (REMOVE clause)
-- ============================================================================

RESET ROLE;
SET ROLE security_test_readonly;

-- Test: REMOVE should fail with only SELECT permission
SELECT * FROM cypher('security_test', $$
    MATCH (p:Person {name: 'Bob'})
    REMOVE p.hobby
    RETURN p
$$) AS (p agtype);

RESET ROLE;
SET ROLE security_test_update;

-- Test: REMOVE should succeed with UPDATE permission
SELECT * FROM cypher('security_test', $$
    MATCH (p:Person {name: 'Bob'})
    REMOVE p.hobby
    RETURN p.name, p.hobby
$$) AS (name agtype, hobby agtype);

-- ============================================================================
-- PART 6: DELETE Permission Tests
-- ============================================================================

RESET ROLE;
SET ROLE security_test_readonly;

-- Test: DELETE should fail with only SELECT permission
SELECT * FROM cypher('security_test', $$
    MATCH (p:Person {name: 'Charlie'})
    DELETE p
$$) AS (a agtype);

RESET ROLE;
SET ROLE security_test_update;

-- Test: DELETE should fail with only UPDATE permission (need DELETE)
SELECT * FROM cypher('security_test', $$
    MATCH (p:Person {name: 'Charlie'})
    DELETE p
$$) AS (a agtype);

RESET ROLE;
SET ROLE security_test_delete;

-- Test: DELETE vertex should succeed with DELETE permission
-- First delete the edge connected to Charlie
SELECT * FROM cypher('security_test', $$
    MATCH (p:Person {name: 'Charlie'})-[k:KNOWS]->()
    DELETE k
$$) AS (a agtype);

-- Now delete the vertex
SELECT * FROM cypher('security_test', $$
    MATCH (p:Person {name: 'Charlie'})
    DELETE p
$$) AS (a agtype);

-- Verify deletion
SELECT * FROM cypher('security_test', $$
    MATCH (p:Person {name: 'Charlie'}) RETURN p
$$) AS (p agtype);

-- ============================================================================
-- PART 7: DETACH DELETE Tests
-- ============================================================================

RESET ROLE;

-- Create a new vertex with edge for DETACH DELETE test
SELECT * FROM cypher('security_test', $$
    CREATE (:Person {name: 'Dave', age: 40})
$$) AS (a agtype);

SELECT * FROM cypher('security_test', $$
    MATCH (a:Person {name: 'Alice'}), (d:Person {name: 'Dave'})
    CREATE (a)-[:KNOWS {since: 2022}]->(d)
$$) AS (a agtype);

SET ROLE security_test_detach_delete;

-- Test: DETACH DELETE should fail without DELETE on edge table
SELECT * FROM cypher('security_test', $$
    MATCH (p:Person {name: 'Dave'})
    DETACH DELETE p
$$) AS (a agtype);

RESET ROLE;
GRANT DELETE ON security_test."KNOWS" TO security_test_detach_delete;
SET ROLE security_test_detach_delete;

-- Test: DETACH DELETE should succeed now when user has DELETE on both vertex and edge tables
SELECT * FROM cypher('security_test', $$
    MATCH (p:Person {name: 'Dave'})
    DETACH DELETE p
$$) AS (a agtype);

-- Verify deletion
SELECT * FROM cypher('security_test', $$
    MATCH (p:Person {name: 'Dave'}) RETURN p
$$) AS (p agtype);

-- ============================================================================
-- PART 8: MERGE Permission Tests
-- ============================================================================

RESET ROLE;
SET ROLE security_test_readonly;

-- Test: MERGE that would create should fail without INSERT
SELECT * FROM cypher('security_test', $$
    MERGE (p:Person {name: 'Eve'})
    RETURN p
$$) AS (p agtype);

RESET ROLE;
SET ROLE security_test_insert;

-- Test: MERGE that creates should succeed with INSERT permission
SELECT * FROM cypher('security_test', $$
    MERGE (p:Person {name: 'Eve', age: 28})
    RETURN p.name, p.age
$$) AS (name agtype, age agtype);

-- Test: MERGE that matches existing should succeed (only needs SELECT)
SELECT * FROM cypher('security_test', $$
    MERGE (p:Person {name: 'Eve'})
    RETURN p.name
$$) AS (name agtype);

-- ============================================================================
-- PART 9: Full Permission Role Tests
-- ============================================================================

RESET ROLE;
SET ROLE security_test_full;

-- Full permission role should be able to do everything
SELECT * FROM cypher('security_test', $$
    CREATE (:Person {name: 'Frank', age: 50})
$$) AS (a agtype);

SELECT * FROM cypher('security_test', $$
    MATCH (p:Person {name: 'Frank'})
    SET p.age = 51
    RETURN p.name, p.age
$$) AS (name agtype, age agtype);

SELECT * FROM cypher('security_test', $$
    MATCH (p:Person {name: 'Frank'})
    DELETE p
$$) AS (a agtype);

-- ============================================================================
-- PART 10: Permission on Specific Labels
-- ============================================================================

RESET ROLE;

-- Create a role with permission only on Person label, not Document
CREATE ROLE security_test_person_only LOGIN;
GRANT USAGE ON SCHEMA security_test TO security_test_person_only;
GRANT USAGE ON SCHEMA ag_catalog TO security_test_person_only;
GRANT EXECUTE ON ALL FUNCTIONS IN SCHEMA ag_catalog TO security_test_person_only;
GRANT SELECT ON ALL TABLES IN SCHEMA ag_catalog TO security_test_person_only;
-- Only grant permissions on Person table
GRANT SELECT, INSERT, UPDATE, DELETE ON security_test."Person" TO security_test_person_only;
GRANT SELECT ON security_test."KNOWS" TO security_test_person_only;
GRANT SELECT ON security_test._ag_label_vertex TO security_test_person_only;
GRANT SELECT ON security_test._ag_label_edge TO security_test_person_only;
GRANT USAGE ON ALL SEQUENCES IN SCHEMA security_test TO security_test_person_only;

SET ROLE security_test_person_only;

-- Test: Operations on Person should succeed
SELECT * FROM cypher('security_test', $$
    MATCH (p:Person {name: 'Alice'}) RETURN p.name
$$) AS (name agtype);

-- Test: SELECT on Document should fail (no permission)
SELECT * FROM cypher('security_test', $$
    MATCH (d:Document) RETURN d.title
$$) AS (title agtype);

-- Test: CREATE Document should fail (no permission on Document table)
SELECT * FROM cypher('security_test', $$
    CREATE (:Document {title: 'New Doc'})
$$) AS (a agtype);

-- ============================================================================
-- PART 11: Function EXECUTE Permission Tests
-- ============================================================================

RESET ROLE;

-- Create role with no function execute permissions
CREATE ROLE security_test_noexec LOGIN;
GRANT USAGE ON SCHEMA security_test TO security_test_noexec;
GRANT USAGE ON SCHEMA ag_catalog TO security_test_noexec;

-- Revoke execute from PUBLIC on functions we want to test
REVOKE EXECUTE ON FUNCTION ag_catalog.create_graph(name) FROM PUBLIC;
REVOKE EXECUTE ON FUNCTION ag_catalog.drop_graph(name, boolean) FROM PUBLIC;
REVOKE EXECUTE ON FUNCTION ag_catalog.create_vlabel(cstring, cstring) FROM PUBLIC;
REVOKE EXECUTE ON FUNCTION ag_catalog.create_elabel(cstring, cstring) FROM PUBLIC;

SET ROLE security_test_noexec;

-- Test: create_graph should fail without EXECUTE permission
SELECT create_graph('unauthorized_graph');

-- Test: drop_graph should fail without EXECUTE permission
SELECT drop_graph('security_test', true);

-- Test: create_vlabel should fail without EXECUTE permission
SELECT create_vlabel('security_test', 'NewLabel');

-- Test: create_elabel should fail without EXECUTE permission
SELECT create_elabel('security_test', 'NewEdge');

RESET ROLE;

-- Grant execute on specific function and test
GRANT EXECUTE ON FUNCTION ag_catalog.create_vlabel(cstring, cstring) TO security_test_noexec;

SET ROLE security_test_noexec;

-- Test: create_vlabel should now get past execute check (will fail on schema permission instead)
SELECT create_vlabel('security_test', 'TestLabel');

-- Test: create_graph should still fail with execute permission denied
SELECT create_graph('unauthorized_graph');

RESET ROLE;

-- Restore execute permissions to PUBLIC
GRANT EXECUTE ON FUNCTION ag_catalog.create_graph(name) TO PUBLIC;
GRANT EXECUTE ON FUNCTION ag_catalog.drop_graph(name, boolean) TO PUBLIC;
GRANT EXECUTE ON FUNCTION ag_catalog.create_vlabel(cstring, cstring) TO PUBLIC;
GRANT EXECUTE ON FUNCTION ag_catalog.create_elabel(cstring, cstring) TO PUBLIC;

-- ============================================================================
-- PART 12: startNode/endNode Permission Tests
-- ============================================================================

-- Create role with SELECT on base tables but NOT on Person label
CREATE ROLE security_test_edge_only LOGIN;
GRANT USAGE ON SCHEMA security_test TO security_test_edge_only;
GRANT USAGE ON SCHEMA ag_catalog TO security_test_edge_only;
GRANT SELECT ON ALL TABLES IN SCHEMA ag_catalog TO security_test_edge_only;
GRANT SELECT ON security_test."KNOWS" TO security_test_edge_only;
GRANT SELECT ON security_test._ag_label_edge TO security_test_edge_only;
GRANT SELECT ON security_test._ag_label_vertex TO security_test_edge_only;
-- Note: NOT granting SELECT on security_test."Person"

SET ROLE security_test_edge_only;

-- Test: endNode fails without SELECT on Person table
SELECT * FROM cypher('security_test', $$
    MATCH ()-[e:KNOWS]->()
    RETURN endNode(e)
$$) AS (end_vertex agtype);

-- Test: startNode fails without SELECT on Person table
SELECT * FROM cypher('security_test', $$
    MATCH ()-[e:KNOWS]->()
    RETURN startNode(e)
$$) AS (start_vertex agtype);

RESET ROLE;

-- Grant SELECT on Person and verify success
GRANT SELECT ON security_test."Person" TO security_test_edge_only;

SET ROLE security_test_edge_only;

-- Test: Should now succeed with SELECT permission
SELECT * FROM cypher('security_test', $$
    MATCH ()-[e:KNOWS]->()
    RETURN startNode(e).name, endNode(e).name
$$) AS (start_name agtype, end_name agtype);

RESET ROLE;

-- ============================================================================
-- Cleanup
-- ============================================================================

RESET ROLE;

-- Drop all owned objects and privileges for each role, then drop the role
DROP OWNED BY security_test_noread CASCADE;
DROP ROLE security_test_noread;

DROP OWNED BY security_test_base_only CASCADE;
DROP ROLE security_test_base_only;

DROP OWNED BY security_test_readonly CASCADE;
DROP ROLE security_test_readonly;

DROP OWNED BY security_test_insert CASCADE;
DROP ROLE security_test_insert;

DROP OWNED BY security_test_update CASCADE;
DROP ROLE security_test_update;

DROP OWNED BY security_test_delete CASCADE;
DROP ROLE security_test_delete;

DROP OWNED BY security_test_detach_delete CASCADE;
DROP ROLE security_test_detach_delete;

DROP OWNED BY security_test_full CASCADE;
DROP ROLE security_test_full;

DROP OWNED BY security_test_person_only CASCADE;
DROP ROLE security_test_person_only;

DROP OWNED BY security_test_noexec CASCADE;
DROP ROLE security_test_noexec;

DROP OWNED BY security_test_edge_only CASCADE;
DROP ROLE security_test_edge_only;

-- Drop test graph
SELECT drop_graph('security_test', true);

--
-- Row-Level Security (RLS) Tests
--

--
-- Setup: Create test graph, data and roles for RLS tests
--
SELECT create_graph('rls_graph');

-- Create test roles
CREATE ROLE rls_user1 LOGIN;
CREATE ROLE rls_user2 LOGIN;
CREATE ROLE rls_admin LOGIN BYPASSRLS;  -- Role that bypasses RLS

-- Create base test data FIRST (as superuser) - this creates the label tables
SELECT * FROM cypher('rls_graph', $$
    CREATE (:Person {name: 'Alice', owner: 'rls_user1', department: 'Engineering', level: 1})
$$) AS (a agtype);

SELECT * FROM cypher('rls_graph', $$
    CREATE (:Person {name: 'Bob', owner: 'rls_user2', department: 'Engineering', level: 2})
$$) AS (a agtype);

SELECT * FROM cypher('rls_graph', $$
    CREATE (:Person {name: 'Charlie', owner: 'rls_user1', department: 'Sales', level: 1})
$$) AS (a agtype);

SELECT * FROM cypher('rls_graph', $$
    CREATE (:Person {name: 'Diana', owner: 'rls_user2', department: 'Sales', level: 3})
$$) AS (a agtype);

-- Create a second vertex label for multi-label tests
SELECT * FROM cypher('rls_graph', $$
    CREATE (:Document {title: 'Public Doc', classification: 'public', owner: 'rls_user1'})
$$) AS (a agtype);

SELECT * FROM cypher('rls_graph', $$
    CREATE (:Document {title: 'Secret Doc', classification: 'secret', owner: 'rls_user2'})
$$) AS (a agtype);

-- Create edges
SELECT * FROM cypher('rls_graph', $$
    MATCH (a:Person {name: 'Alice'}), (b:Person {name: 'Bob'})
    CREATE (a)-[:KNOWS {since: 2020, strength: 'weak'}]->(b)
$$) AS (a agtype);

SELECT * FROM cypher('rls_graph', $$
    MATCH (a:Person {name: 'Charlie'}), (b:Person {name: 'Diana'})
    CREATE (a)-[:KNOWS {since: 2021, strength: 'strong'}]->(b)
$$) AS (a agtype);

SELECT * FROM cypher('rls_graph', $$
    MATCH (a:Person {name: 'Alice'}), (b:Person {name: 'Charlie'})
    CREATE (a)-[:KNOWS {since: 2022, strength: 'strong'}]->(b)
$$) AS (a agtype);

SELECT * FROM cypher('rls_graph', $$
    MATCH (a:Person {name: 'Alice'}), (d:Document {title: 'Public Doc'})
    CREATE (a)-[:AUTHORED]->(d)
$$) AS (a agtype);

-- Grant permissions AFTER creating tables (so Person, Document, KNOWS, AUTHORED exist)
GRANT USAGE ON SCHEMA rls_graph TO rls_user1, rls_user2, rls_admin;
GRANT ALL ON ALL TABLES IN SCHEMA rls_graph TO rls_user1, rls_user2, rls_admin;
GRANT USAGE ON SCHEMA ag_catalog TO rls_user1, rls_user2, rls_admin;
GRANT USAGE ON ALL SEQUENCES IN SCHEMA rls_graph TO rls_user1, rls_user2, rls_admin;

-- ============================================================================
-- PART 1: Vertex SELECT Policies (USING clause)
-- ============================================================================

-- Enable RLS on Person label
ALTER TABLE rls_graph."Person" ENABLE ROW LEVEL SECURITY;
ALTER TABLE rls_graph."Person" FORCE ROW LEVEL SECURITY;

-- 1.1: Basic ownership filtering
CREATE POLICY person_select_own ON rls_graph."Person"
    FOR SELECT
    USING (properties->>'"owner"' = current_user);

-- Test as rls_user1 - should only see Alice and Charlie (owned by rls_user1)
SET ROLE rls_user1;
SELECT * FROM cypher('rls_graph', $$
    MATCH (p:Person) RETURN p.name ORDER BY p.name
$$) AS (name agtype);

-- Test as rls_user2 - should only see Bob and Diana (owned by rls_user2)
SET ROLE rls_user2;
SELECT * FROM cypher('rls_graph', $$
    MATCH (p:Person) RETURN p.name ORDER BY p.name
$$) AS (name agtype);

RESET ROLE;

-- 1.2: Default deny - no permissive policies means no access
DROP POLICY person_select_own ON rls_graph."Person";

-- With no policies, RLS blocks all access
SET ROLE rls_user1;
SELECT * FROM cypher('rls_graph', $$
    MATCH (p:Person) RETURN p.name ORDER BY p.name
$$) AS (name agtype);

RESET ROLE;

-- ============================================================================
-- PART 2: Vertex INSERT Policies (WITH CHECK) - CREATE
-- ============================================================================

-- Allow SELECT for all (so we can verify results)
CREATE POLICY person_select_all ON rls_graph."Person"
    FOR SELECT USING (true);

-- 2.1: Basic WITH CHECK - users can only insert rows they own
CREATE POLICY person_insert_own ON rls_graph."Person"
    FOR INSERT
    WITH CHECK (properties->>'"owner"' = current_user);

-- Test as rls_user1 - should succeed (owner matches current_user)
SET ROLE rls_user1;
SELECT * FROM cypher('rls_graph', $$
    CREATE (:Person {name: 'User1Created', owner: 'rls_user1', department: 'Test', level: 1})
$$) AS (a agtype);

-- Test as rls_user1 - should FAIL (owner doesn't match current_user)
SELECT * FROM cypher('rls_graph', $$
    CREATE (:Person {name: 'User1Fake', owner: 'rls_user2', department: 'Test', level: 1})
$$) AS (a agtype);

RESET ROLE;

-- Verify only User1Created was created
SELECT * FROM cypher('rls_graph', $$
    MATCH (p:Person) WHERE p.department = 'Test' RETURN p.name ORDER BY p.name
$$) AS (name agtype);

-- 2.2: Default deny for INSERT - no INSERT policy blocks all inserts
DROP POLICY person_insert_own ON rls_graph."Person";

SET ROLE rls_user1;
SELECT * FROM cypher('rls_graph', $$
    CREATE (:Person {name: 'ShouldFail', owner: 'rls_user1', department: 'Blocked', level: 1})
$$) AS (a agtype);
RESET ROLE;

-- Verify nothing was created in Blocked department
SELECT * FROM cypher('rls_graph', $$
    MATCH (p:Person) WHERE p.department = 'Blocked' RETURN p.name
$$) AS (name agtype);

-- cleanup
DROP POLICY person_select_all ON rls_graph."Person";
SELECT * FROM cypher('rls_graph', $$
    MATCH (p:Person) WHERE p.department = 'Test' DELETE p
$$) AS (a agtype);

-- ============================================================================
-- PART 3: Vertex UPDATE Policies - SET
-- ============================================================================

CREATE POLICY person_select_all ON rls_graph."Person"
    FOR SELECT USING (true);

-- 3.1: USING clause only - filter which rows can be updated
CREATE POLICY person_update_using ON rls_graph."Person"
    FOR UPDATE
    USING (properties->>'"owner"' = current_user);

SET ROLE rls_user1;

-- Should succeed - rls_user1 owns Alice
SELECT * FROM cypher('rls_graph', $$
    MATCH (p:Person {name: 'Alice'}) SET p.updated = true RETURN p.name, p.updated
$$) AS (name agtype, updated agtype);

-- Should silently skip - rls_user1 doesn't own Bob (USING filters it out)
SELECT * FROM cypher('rls_graph', $$
    MATCH (p:Person {name: 'Bob'}) SET p.updated = true RETURN p.name, p.updated
$$) AS (name agtype, updated agtype);

RESET ROLE;

-- Verify Alice was updated, Bob was not
SELECT * FROM cypher('rls_graph', $$
    MATCH (p:Person) WHERE p.name IN ['Alice', 'Bob'] RETURN p.name, p.updated ORDER BY p.name
$$) AS (name agtype, updated agtype);

-- 3.2: WITH CHECK clause - validate new values
DROP POLICY person_update_using ON rls_graph."Person";

CREATE POLICY person_update_check ON rls_graph."Person"
    FOR UPDATE
    USING (true)  -- Can update any row
    WITH CHECK (properties->>'"owner"' = current_user);  -- But new value must keep owner

SET ROLE rls_user1;

-- Should succeed - modifying property but keeping owner
SELECT * FROM cypher('rls_graph', $$
    MATCH (p:Person {name: 'Alice'}) SET p.verified = true RETURN p.name, p.verified
$$) AS (name agtype, verified agtype);

-- Should FAIL - trying to change owner to someone else
SELECT * FROM cypher('rls_graph', $$
    MATCH (p:Person {name: 'Alice'}) SET p.owner = 'rls_user2' RETURN p.owner
$$) AS (owner agtype);

RESET ROLE;

-- Verify owner wasn't changed
SELECT * FROM cypher('rls_graph', $$
    MATCH (p:Person {name: 'Alice'}) RETURN p.owner
$$) AS (owner agtype);

-- 3.3: Both USING and WITH CHECK together
DROP POLICY person_update_check ON rls_graph."Person";

CREATE POLICY person_update_both ON rls_graph."Person"
    FOR UPDATE
    USING (properties->>'"owner"' = current_user)
    WITH CHECK (properties->>'"owner"' = current_user);

SET ROLE rls_user1;

-- Should succeed - owns Alice, keeping owner
SELECT * FROM cypher('rls_graph', $$
    MATCH (p:Person {name: 'Alice'}) SET p.status = 'active' RETURN p.name, p.status
$$) AS (name agtype, status agtype);

-- Should silently skip - doesn't own Bob (USING filters)
SELECT * FROM cypher('rls_graph', $$
    MATCH (p:Person {name: 'Bob'}) SET p.status = 'active' RETURN p.name, p.status
$$) AS (name agtype, status agtype);

RESET ROLE;

-- ============================================================================
-- PART 4: Vertex UPDATE Policies - REMOVE
-- ============================================================================

-- Keep existing update policy, test REMOVE operation

SET ROLE rls_user1;

-- Should succeed - owns Alice
SELECT * FROM cypher('rls_graph', $$
    MATCH (p:Person {name: 'Alice'}) REMOVE p.status RETURN p.name, p.status
$$) AS (name agtype, status agtype);

-- Should silently skip - doesn't own Bob
SELECT * FROM cypher('rls_graph', $$
    MATCH (p:Person {name: 'Bob'}) REMOVE p.department RETURN p.name, p.department
$$) AS (name agtype, dept agtype);

RESET ROLE;

-- Verify Bob still has department
SELECT * FROM cypher('rls_graph', $$
    MATCH (p:Person {name: 'Bob'}) RETURN p.department
$$) AS (dept agtype);

-- cleanup
DROP POLICY person_select_all ON rls_graph."Person";
DROP POLICY person_update_both ON rls_graph."Person";

-- ============================================================================
-- PART 5: Vertex DELETE Policies
-- ============================================================================

CREATE POLICY person_select_all ON rls_graph."Person"
    FOR SELECT USING (true);

-- Create test data for delete tests
CREATE POLICY person_insert_all ON rls_graph."Person"
    FOR INSERT WITH CHECK (true);

SELECT * FROM cypher('rls_graph', $$
    CREATE (:Person {name: 'DeleteTest1', owner: 'rls_user1', department: 'DeleteTest', level: 1})
$$) AS (a agtype);

SELECT * FROM cypher('rls_graph', $$
    CREATE (:Person {name: 'DeleteTest2', owner: 'rls_user2', department: 'DeleteTest', level: 1})
$$) AS (a agtype);

SELECT * FROM cypher('rls_graph', $$
    CREATE (:Person {name: 'DeleteTest3', owner: 'rls_user1', department: 'DeleteTest', level: 1})
$$) AS (a agtype);

DROP POLICY person_insert_all ON rls_graph."Person";

-- 5.1: Basic USING filtering for DELETE
CREATE POLICY person_delete_own ON rls_graph."Person"
    FOR DELETE
    USING (properties->>'"owner"' = current_user);

SET ROLE rls_user1;

-- Should succeed - owns DeleteTest1
SELECT * FROM cypher('rls_graph', $$
    MATCH (p:Person {name: 'DeleteTest1'}) DELETE p
$$) AS (a agtype);

-- Should silently skip - doesn't own DeleteTest2
SELECT * FROM cypher('rls_graph', $$
    MATCH (p:Person {name: 'DeleteTest2'}) DELETE p
$$) AS (a agtype);

RESET ROLE;

-- Verify DeleteTest1 deleted, DeleteTest2 still exists
SELECT * FROM cypher('rls_graph', $$
    MATCH (p:Person) WHERE p.department = 'DeleteTest' RETURN p.name ORDER BY p.name
$$) AS (name agtype);

-- 5.2: Default deny for DELETE - no policy blocks all deletes
DROP POLICY person_delete_own ON rls_graph."Person";

SET ROLE rls_user1;

-- Should silently skip - no DELETE policy means default deny
SELECT * FROM cypher('rls_graph', $$
    MATCH (p:Person {name: 'DeleteTest3'}) DELETE p
$$) AS (a agtype);

RESET ROLE;

-- Verify DeleteTest3 still exists
SELECT * FROM cypher('rls_graph', $$
    MATCH (p:Person {name: 'DeleteTest3'}) RETURN p.name
$$) AS (name agtype);

-- cleanup
DROP POLICY person_select_all ON rls_graph."Person";
SELECT * FROM cypher('rls_graph', $$
    MATCH (p:Person) WHERE p.department = 'DeleteTest' DELETE p
$$) AS (a agtype);

-- ============================================================================
-- PART 6: MERGE Policies
-- ============================================================================

CREATE POLICY person_select_all ON rls_graph."Person"
    FOR SELECT USING (true);

CREATE POLICY person_insert_own ON rls_graph."Person"
    FOR INSERT
    WITH CHECK (properties->>'"owner"' = current_user);

-- 6.1: MERGE creating new vertex - INSERT policy applies
SET ROLE rls_user1;

-- Should succeed - creating with correct owner
SELECT * FROM cypher('rls_graph', $$
    MERGE (p:Person {name: 'MergeNew1', owner: 'rls_user1', department: 'Merge', level: 1})
    RETURN p.name
$$) AS (name agtype);

-- Should FAIL - creating with wrong owner
SELECT * FROM cypher('rls_graph', $$
    MERGE (p:Person {name: 'MergeNew2', owner: 'rls_user2', department: 'Merge', level: 1})
    RETURN p.name
$$) AS (name agtype);

RESET ROLE;

-- 6.2: MERGE matching existing - only SELECT needed
SET ROLE rls_user1;

-- Should succeed - Alice exists and SELECT allowed
SELECT * FROM cypher('rls_graph', $$
    MERGE (p:Person {name: 'Alice'})
    RETURN p.name, p.owner
$$) AS (name agtype, owner agtype);

RESET ROLE;

-- Verify only MergeNew1 was created
SELECT * FROM cypher('rls_graph', $$
    MATCH (p:Person) WHERE p.department = 'Merge' RETURN p.name ORDER BY p.name
$$) AS (name agtype);

-- cleanup
DROP POLICY person_select_all ON rls_graph."Person";
DROP POLICY person_insert_own ON rls_graph."Person";
SELECT * FROM cypher('rls_graph', $$
    MATCH (p:Person) WHERE p.department = 'Merge' DELETE p
$$) AS (a agtype);

-- ============================================================================
-- PART 7: Edge SELECT Policies
-- ============================================================================

-- Disable vertex RLS, enable edge RLS
ALTER TABLE rls_graph."Person" DISABLE ROW LEVEL SECURITY;
ALTER TABLE rls_graph."KNOWS" ENABLE ROW LEVEL SECURITY;
ALTER TABLE rls_graph."KNOWS" FORCE ROW LEVEL SECURITY;

-- Policy: Only see edges from 2021 or later
CREATE POLICY knows_select_recent ON rls_graph."KNOWS"
    FOR SELECT
    USING ((properties->>'"since"')::int >= 2021);

SET ROLE rls_user1;

-- Should only see 2021 and 2022 edges (not 2020)
SELECT * FROM cypher('rls_graph', $$
    MATCH ()-[k:KNOWS]->() RETURN k.since ORDER BY k.since
$$) AS (since agtype);

RESET ROLE;

-- ============================================================================
-- PART 8: Edge INSERT Policies (CREATE edge)
-- ============================================================================

DROP POLICY knows_select_recent ON rls_graph."KNOWS";

CREATE POLICY knows_select_all ON rls_graph."KNOWS"
    FOR SELECT USING (true);

-- Policy: Can only create edges with strength = 'strong'
CREATE POLICY knows_insert_strong ON rls_graph."KNOWS"
    FOR INSERT
    WITH CHECK (properties->>'"strength"' = 'strong');

SET ROLE rls_user1;

-- Should succeed - strength is 'strong'
SELECT * FROM cypher('rls_graph', $$
    MATCH (a:Person {name: 'Bob'}), (b:Person {name: 'Diana'})
    CREATE (a)-[:KNOWS {since: 2023, strength: 'strong'}]->(b)
$$) AS (a agtype);

-- Should FAIL - strength is 'weak'
SELECT * FROM cypher('rls_graph', $$
    MATCH (a:Person {name: 'Diana'}), (b:Person {name: 'Alice'})
    CREATE (a)-[:KNOWS {since: 2023, strength: 'weak'}]->(b)
$$) AS (a agtype);

RESET ROLE;

-- Verify only strong edge was created
SELECT * FROM cypher('rls_graph', $$
    MATCH ()-[k:KNOWS]->() WHERE k.since = 2023 RETURN k.strength ORDER BY k.strength
$$) AS (strength agtype);

-- cleanup
DROP POLICY knows_insert_strong ON rls_graph."KNOWS";

-- ============================================================================
-- PART 9: Edge UPDATE Policies (SET on edge)
-- ============================================================================

-- Policy: Can only update edges with strength = 'strong'
CREATE POLICY knows_update_strong ON rls_graph."KNOWS"
    FOR UPDATE
    USING (properties->>'"strength"' = 'strong')
    WITH CHECK (properties->>'"strength"' = 'strong');

SET ROLE rls_user1;

-- Should succeed - edge has strength 'strong'
SELECT * FROM cypher('rls_graph', $$
    MATCH ()-[k:KNOWS {since: 2021}]->() SET k.notes = 'updated' RETURN k.since, k.notes
$$) AS (since agtype, notes agtype);

-- Should silently skip - edge has strength 'weak'
SELECT * FROM cypher('rls_graph', $$
    MATCH ()-[k:KNOWS {since: 2020}]->() SET k.notes = 'updated' RETURN k.since, k.notes
$$) AS (since agtype, notes agtype);

RESET ROLE;

-- Verify only 2021 edge was updated
SELECT * FROM cypher('rls_graph', $$
    MATCH ()-[k:KNOWS]->() WHERE k.since IN [2020, 2021] RETURN k.since, k.notes ORDER BY k.since
$$) AS (since agtype, notes agtype);

-- cleanup
DROP POLICY knows_select_all ON rls_graph."KNOWS";
DROP POLICY knows_update_strong ON rls_graph."KNOWS";

-- ============================================================================
-- PART 10: Edge DELETE Policies
-- ============================================================================

CREATE POLICY knows_select_all ON rls_graph."KNOWS"
    FOR SELECT USING (true);

-- Create test edges for delete
CREATE POLICY knows_insert_all ON rls_graph."KNOWS"
    FOR INSERT WITH CHECK (true);

SELECT * FROM cypher('rls_graph', $$
    MATCH (a:Person {name: 'Bob'}), (b:Person {name: 'Charlie'})
    CREATE (a)-[:KNOWS {since: 2018, strength: 'weak'}]->(b)
$$) AS (a agtype);

SELECT * FROM cypher('rls_graph', $$
    MATCH (a:Person {name: 'Diana'}), (b:Person {name: 'Charlie'})
    CREATE (a)-[:KNOWS {since: 2019, strength: 'strong'}]->(b)
$$) AS (a agtype);

DROP POLICY knows_insert_all ON rls_graph."KNOWS";

-- Policy: Can only delete edges with strength = 'weak'
CREATE POLICY knows_delete_weak ON rls_graph."KNOWS"
    FOR DELETE
    USING (properties->>'"strength"' = 'weak');

SET ROLE rls_user1;

-- Should succeed - edge has strength 'weak'
SELECT * FROM cypher('rls_graph', $$
    MATCH ()-[k:KNOWS {since: 2018}]->() DELETE k
$$) AS (a agtype);

-- Should silently skip - edge has strength 'strong'
SELECT * FROM cypher('rls_graph', $$
    MATCH ()-[k:KNOWS {since: 2019}]->() DELETE k
$$) AS (a agtype);

RESET ROLE;

-- Verify 2018 edge deleted, 2019 edge still exists
SELECT * FROM cypher('rls_graph', $$
    MATCH ()-[k:KNOWS]->() WHERE k.since IN [2018, 2019] RETURN k.since ORDER BY k.since
$$) AS (since agtype);

-- cleanup
DROP POLICY knows_delete_weak ON rls_graph."KNOWS";

-- ============================================================================
-- PART 11: DETACH DELETE
-- ============================================================================

-- Re-enable Person RLS
ALTER TABLE rls_graph."Person" ENABLE ROW LEVEL SECURITY;
CREATE POLICY person_all ON rls_graph."Person"
    FOR ALL USING (true) WITH CHECK (true);

-- Create test data with a protected edge
CREATE POLICY knows_insert_all ON rls_graph."KNOWS"
    FOR INSERT WITH CHECK (true);

SELECT * FROM cypher('rls_graph', $$
    CREATE (:Person {name: 'DetachTest1', owner: 'test', department: 'Detach', level: 1})
$$) AS (a agtype);

SELECT * FROM cypher('rls_graph', $$
    CREATE (:Person {name: 'DetachTest2', owner: 'test', department: 'Detach', level: 1})
$$) AS (a agtype);

SELECT * FROM cypher('rls_graph', $$
    MATCH (a:Person {name: 'DetachTest1'}), (b:Person {name: 'DetachTest2'})
    CREATE (a)-[:KNOWS {since: 2010, strength: 'protected'}]->(b)
$$) AS (a agtype);

DROP POLICY knows_insert_all ON rls_graph."KNOWS";

-- Policy: Cannot delete edges with strength = 'protected'
CREATE POLICY knows_delete_not_protected ON rls_graph."KNOWS"
    FOR DELETE
    USING (properties->>'"strength"' != 'protected');

SET ROLE rls_user1;

-- Should ERROR - DETACH DELETE cannot silently skip (would leave dangling edge)
SELECT * FROM cypher('rls_graph', $$
    MATCH (p:Person {name: 'DetachTest1'}) DETACH DELETE p
$$) AS (a agtype);

RESET ROLE;

-- Verify vertex still exists (delete was blocked)
SELECT * FROM cypher('rls_graph', $$
    MATCH (p:Person {name: 'DetachTest1'}) RETURN p.name
$$) AS (name agtype);

-- cleanup
DROP POLICY person_all ON rls_graph."Person";
DROP POLICY knows_select_all ON rls_graph."KNOWS";
DROP POLICY knows_delete_not_protected ON rls_graph."KNOWS";
SELECT * FROM cypher('rls_graph', $$
    MATCH (p:Person) WHERE p.department = 'Detach' DETACH DELETE p
$$) AS (a agtype);

-- ============================================================================
-- PART 12: Multiple Labels in Single Query
-- ============================================================================

-- Enable RLS on Document too
ALTER TABLE rls_graph."Document" ENABLE ROW LEVEL SECURITY;
ALTER TABLE rls_graph."Document" FORCE ROW LEVEL SECURITY;

-- Policy: Users see their own Person records
CREATE POLICY person_own ON rls_graph."Person"
    FOR SELECT
    USING (properties->>'"owner"' = current_user);

-- Policy: Users see only public documents
CREATE POLICY doc_public ON rls_graph."Document"
    FOR SELECT
    USING (properties->>'"classification"' = 'public');

SET ROLE rls_user1;

-- Should only see Alice and Charlie (Person) with Public Doc (Document)
SELECT * FROM cypher('rls_graph', $$
    MATCH (p:Person) RETURN p.name ORDER BY p.name
$$) AS (name agtype);

SELECT * FROM cypher('rls_graph', $$
    MATCH (d:Document) RETURN d.title ORDER BY d.title
$$) AS (title agtype);

-- Combined query - should respect both policies
SELECT * FROM cypher('rls_graph', $$
    MATCH (p:Person)-[:AUTHORED]->(d:Document)
    RETURN p.name, d.title
$$) AS (person agtype, doc agtype);

RESET ROLE;

-- ============================================================================
-- PART 13: Permissive vs Restrictive Policies
-- ============================================================================

DROP POLICY person_own ON rls_graph."Person";
DROP POLICY doc_public ON rls_graph."Document";

ALTER TABLE rls_graph."Document" DISABLE ROW LEVEL SECURITY;
ALTER TABLE rls_graph."KNOWS" DISABLE ROW LEVEL SECURITY;

-- 13.1: Multiple permissive policies (OR logic)
CREATE POLICY person_permissive_own ON rls_graph."Person"
    AS PERMISSIVE FOR SELECT
    USING (properties->>'"owner"' = current_user);

CREATE POLICY person_permissive_eng ON rls_graph."Person"
    AS PERMISSIVE FOR SELECT
    USING (properties->>'"department"' = 'Engineering');

SET ROLE rls_user1;

-- Should see: Alice (own), Charlie (own), Bob (Engineering)
SELECT * FROM cypher('rls_graph', $$
    MATCH (p:Person) WHERE p.department IN ['Engineering', 'Sales']
    RETURN p.name ORDER BY p.name
$$) AS (name agtype);

RESET ROLE;

-- 13.2: Add restrictive policy (AND with permissive)
CREATE POLICY person_restrictive_level ON rls_graph."Person"
    AS RESTRICTIVE FOR SELECT
    USING ((properties->>'"level"')::int <= 2);

SET ROLE rls_user1;

-- Should see: Alice (own, level 1), Bob (Engineering, level 2), Charlie (own, level 1)
-- Diana (level 3) blocked by restrictive
SELECT * FROM cypher('rls_graph', $$
    MATCH (p:Person) RETURN p.name, p.level ORDER BY p.name
$$) AS (name agtype, level agtype);

RESET ROLE;

-- 13.3: Multiple restrictive policies (all must pass)
CREATE POLICY person_restrictive_sales ON rls_graph."Person"
    AS RESTRICTIVE FOR SELECT
    USING (properties->>'"department"' != 'Sales');

SET ROLE rls_user1;

-- Should see: Alice (own, level 1, not Sales), Bob (Engineering, level 2, not Sales)
-- Charlie blocked by Sales restriction
SELECT * FROM cypher('rls_graph', $$
    MATCH (p:Person) RETURN p.name ORDER BY p.name
$$) AS (name agtype);

RESET ROLE;

-- ============================================================================
-- PART 14: BYPASSRLS Role and Superuser Behavior
-- ============================================================================

DROP POLICY person_permissive_own ON rls_graph."Person";
DROP POLICY person_permissive_eng ON rls_graph."Person";
DROP POLICY person_restrictive_level ON rls_graph."Person";
DROP POLICY person_restrictive_sales ON rls_graph."Person";

-- Restrictive policy that blocks most access
CREATE POLICY person_very_restrictive ON rls_graph."Person"
    FOR SELECT
    USING (properties->>'"name"' = 'Nobody');

-- 14.1: Regular user sees nothing
SET ROLE rls_user1;

SELECT * FROM cypher('rls_graph', $$
    MATCH (p:Person) RETURN p.name ORDER BY p.name
$$) AS (name agtype);

RESET ROLE;

-- 14.2: BYPASSRLS role sees everything
SET ROLE rls_admin;

SELECT * FROM cypher('rls_graph', $$
    MATCH (p:Person) RETURN p.name ORDER BY p.name
$$) AS (name agtype);

RESET ROLE;

-- 14.3: Superuser sees everything (implicit bypass)
SELECT * FROM cypher('rls_graph', $$
    MATCH (p:Person) RETURN p.name ORDER BY p.name
$$) AS (name agtype);

-- ============================================================================
-- PART 15: Complex Multi-Operation Queries
-- ============================================================================

DROP POLICY person_very_restrictive ON rls_graph."Person";

CREATE POLICY person_select_all ON rls_graph."Person"
    FOR SELECT USING (true);

CREATE POLICY person_insert_own ON rls_graph."Person"
    FOR INSERT
    WITH CHECK (properties->>'"owner"' = current_user);

CREATE POLICY person_update_own ON rls_graph."Person"
    FOR UPDATE
    USING (properties->>'"owner"' = current_user)
    WITH CHECK (properties->>'"owner"' = current_user);

-- 15.1: MATCH + CREATE in one query
SET ROLE rls_user1;

-- Should succeed - creating with correct owner
SELECT * FROM cypher('rls_graph', $$
    MATCH (a:Person {name: 'Alice'})
    CREATE (a)-[:KNOWS]->(:Person {name: 'NewFromMatch', owner: 'rls_user1', department: 'Complex', level: 1})
$$) AS (a agtype);

RESET ROLE;

-- Verify creation
SELECT * FROM cypher('rls_graph', $$
    MATCH (p:Person {name: 'NewFromMatch'}) RETURN p.name, p.owner
$$) AS (name agtype, owner agtype);

-- 15.2: MATCH + SET in one query
SET ROLE rls_user1;

-- Should succeed on Alice (own), skip Bob (not own)
SELECT * FROM cypher('rls_graph', $$
    MATCH (p:Person) WHERE p.name IN ['Alice', 'Bob']
    SET p.complexTest = true
    RETURN p.name, p.complexTest
$$) AS (name agtype, test agtype);

RESET ROLE;

-- Verify only Alice was updated
SELECT * FROM cypher('rls_graph', $$
    MATCH (p:Person) WHERE p.name IN ['Alice', 'Bob']
    RETURN p.name, p.complexTest ORDER BY p.name
$$) AS (name agtype, test agtype);

-- cleanup
DROP POLICY IF EXISTS person_select_all ON rls_graph."Person";
DROP POLICY IF EXISTS person_insert_own ON rls_graph."Person";
DROP POLICY IF EXISTS person_update_own ON rls_graph."Person";

-- ============================================================================
-- PART 16: startNode/endNode RLS Enforcement
-- ============================================================================

ALTER TABLE rls_graph."Person" DISABLE ROW LEVEL SECURITY;

-- Enable RLS on Person with restrictive policy
ALTER TABLE rls_graph."Person" ENABLE ROW LEVEL SECURITY;
ALTER TABLE rls_graph."Person" FORCE ROW LEVEL SECURITY;

-- Policy: users can only see their own Person records
CREATE POLICY person_own ON rls_graph."Person"
    FOR SELECT
    USING (properties->>'"owner"' = current_user);

-- Enable edge access for testing
ALTER TABLE rls_graph."KNOWS" ENABLE ROW LEVEL SECURITY;
CREATE POLICY knows_all ON rls_graph."KNOWS"
    FOR SELECT USING (true);

-- 16.1: startNode blocked by RLS - should error
SET ROLE rls_user1;

-- rls_user1 can see the edge (Alice->Bob) but cannot see Bob (owned by rls_user2)
-- endNode should error because Bob is blocked by RLS
SELECT * FROM cypher('rls_graph', $$
    MATCH (a:Person {name: 'Alice'})-[e:KNOWS]->(b)
    RETURN endNode(e)
$$) AS (end_vertex agtype);

-- 16.2: endNode blocked by RLS - should error
-- rls_user1 cannot see Bob, so startNode on an edge starting from Bob should error
SET ROLE rls_user2;

-- rls_user2 can see Bob but not Alice (owned by rls_user1)
-- startNode should error because Alice is blocked by RLS
SELECT * FROM cypher('rls_graph', $$
    MATCH (a)-[e:KNOWS]->(b:Person {name: 'Bob'})
    RETURN startNode(e)
$$) AS (start_vertex agtype);

-- 16.3: startNode/endNode succeed when RLS allows access
SET ROLE rls_user1;

-- Alice->Charlie edge: rls_user1 owns both, should succeed
SELECT * FROM cypher('rls_graph', $$
    MATCH (a:Person {name: 'Alice'})-[e:KNOWS]->(c:Person {name: 'Charlie'})
    RETURN startNode(e).name, endNode(e).name
$$) AS (start_name agtype, end_name agtype);

RESET ROLE;

-- cleanup
DROP POLICY person_own ON rls_graph."Person";
DROP POLICY knows_all ON rls_graph."KNOWS";
ALTER TABLE rls_graph."KNOWS" DISABLE ROW LEVEL SECURITY;

-- ============================================================================
-- RLS CLEANUP
-- ============================================================================

RESET ROLE;

-- Disable RLS on all tables
ALTER TABLE rls_graph."Person" DISABLE ROW LEVEL SECURITY;
ALTER TABLE rls_graph."Document" DISABLE ROW LEVEL SECURITY;
ALTER TABLE rls_graph."KNOWS" DISABLE ROW LEVEL SECURITY;

-- Drop roles
DROP OWNED BY rls_user1 CASCADE;
DROP ROLE rls_user1;

DROP OWNED BY rls_user2 CASCADE;
DROP ROLE rls_user2;

DROP OWNED BY rls_admin CASCADE;
DROP ROLE rls_admin;

-- Drop test graph
SELECT drop_graph('rls_graph', true);
