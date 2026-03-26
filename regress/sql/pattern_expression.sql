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

SELECT create_graph('pattern_expr');

--
-- Setup test data
--
SELECT * FROM cypher('pattern_expr', $$
    CREATE (alice:Person {name: 'Alice'})-[:KNOWS]->(bob:Person {name: 'Bob'}),
           (alice)-[:WORKS_WITH]->(charlie:Person {name: 'Charlie'}),
           (dave:Person {name: 'Dave'})
$$) AS (result agtype);

--
-- Basic pattern expression in WHERE
--
-- Bare pattern: (a)-[:REL]->(b)
SELECT * FROM cypher('pattern_expr', $$
    MATCH (a:Person), (b:Person)
    WHERE (a)-[:KNOWS]->(b)
    RETURN a.name, b.name
    ORDER BY a.name, b.name
$$) AS (a agtype, b agtype);

--
-- NOT pattern expression
--
-- Find people who don't KNOW anyone
SELECT * FROM cypher('pattern_expr', $$
    MATCH (a:Person)
    WHERE NOT (a)-[:KNOWS]->(:Person)
    RETURN a.name
    ORDER BY a.name
$$) AS (result agtype);

--
-- Pattern with labeled first node
--
SELECT * FROM cypher('pattern_expr', $$
    MATCH (a:Person), (b:Person)
    WHERE (a:Person)-[:KNOWS]->(b)
    RETURN a.name, b.name
    ORDER BY a.name
$$) AS (a agtype, b agtype);

--
-- Pattern combined with AND
--
SELECT * FROM cypher('pattern_expr', $$
    MATCH (a:Person), (b:Person)
    WHERE (a)-[:KNOWS]->(b) AND a.name = 'Alice'
    RETURN a.name, b.name
$$) AS (a agtype, b agtype);

--
-- Pattern combined with OR
--
SELECT * FROM cypher('pattern_expr', $$
    MATCH (a:Person), (b:Person)
    WHERE (a)-[:KNOWS]->(b) OR (a)-[:WORKS_WITH]->(b)
    RETURN a.name, b.name
    ORDER BY a.name, b.name
$$) AS (a agtype, b agtype);

--
-- Left-directed pattern
--
SELECT * FROM cypher('pattern_expr', $$
    MATCH (a:Person), (b:Person)
    WHERE (a)<-[:KNOWS]-(b)
    RETURN a.name, b.name
    ORDER BY a.name
$$) AS (a agtype, b agtype);

--
-- Pattern with anonymous nodes
--
-- Find anyone who has any outgoing KNOWS relationship
SELECT * FROM cypher('pattern_expr', $$
    MATCH (a:Person)
    WHERE (a)-[:KNOWS]->()
    RETURN a.name
    ORDER BY a.name
$$) AS (result agtype);

--
-- Multiple relationship pattern
--
SELECT * FROM cypher('pattern_expr', $$
    MATCH (a:Person), (c:Person)
    WHERE (a)-[:KNOWS]->()-[:WORKS_WITH]->(c)
    RETURN a.name, c.name
    ORDER BY a.name
$$) AS (a agtype, c agtype);

--
-- Existing EXISTS() syntax still works (backward compatibility)
--
SELECT * FROM cypher('pattern_expr', $$
    MATCH (a:Person), (b:Person)
    WHERE EXISTS((a)-[:KNOWS]->(b))
    RETURN a.name, b.name
    ORDER BY a.name
$$) AS (a agtype, b agtype);

--
-- Pattern expression produces same results as EXISTS()
--
SELECT * FROM cypher('pattern_expr', $$
    MATCH (a:Person)
    WHERE (a)-[:KNOWS]->(:Person)
    RETURN a.name
    ORDER BY a.name
$$) AS (result agtype);

SELECT * FROM cypher('pattern_expr', $$
    MATCH (a:Person)
    WHERE EXISTS((a)-[:KNOWS]->(:Person))
    RETURN a.name
    ORDER BY a.name
$$) AS (result agtype);

--
-- Regular (non-pattern) expressions still work (no regression)
--
SELECT * FROM cypher('pattern_expr', $$
    RETURN (1 + 2)
$$) AS (result agtype);

SELECT * FROM cypher('pattern_expr', $$
    MATCH (n:Person)
    WHERE n.name = 'Alice'
    RETURN (n.name)
$$) AS (result agtype);

--
-- Cleanup
--
SELECT * FROM drop_graph('pattern_expr', true);
