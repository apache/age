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
-- Pattern expressions in RETURN (boolean projection)
--
-- Each person gets a column showing whether they know someone
SELECT * FROM cypher('pattern_expr', $$
    MATCH (a:Person)
    RETURN a.name, (a)-[:KNOWS]->(:Person) AS knows_someone
    ORDER BY a.name
$$) AS (name agtype, knows_someone agtype);

-- Mix pattern expression with other projections
SELECT * FROM cypher('pattern_expr', $$
    MATCH (a:Person)
    RETURN a.name, (a)-[:KNOWS]->(:Person), (a)-[:WORKS_WITH]->(:Person)
    ORDER BY a.name
$$) AS (name agtype, knows agtype, works_with agtype);

--
-- Pattern expressions in CASE WHEN
--
SELECT * FROM cypher('pattern_expr', $$
    MATCH (a:Person)
    RETURN a.name,
           CASE WHEN (a)-[:KNOWS]->(:Person) THEN 'social'
                ELSE 'loner'
           END
    ORDER BY a.name
$$) AS (name agtype, kind agtype);

--
-- Pattern expressions combined with boolean operators in RETURN
--
SELECT * FROM cypher('pattern_expr', $$
    MATCH (a:Person)
    RETURN a.name,
           (a)-[:KNOWS]->(:Person) AND (a)-[:WORKS_WITH]->(:Person) AS has_both,
           (a)-[:KNOWS]->(:Person) OR (a)-[:WORKS_WITH]->(:Person) AS has_either
    ORDER BY a.name
$$) AS (name agtype, has_both agtype, has_either agtype);

--
-- Pattern expression in SET (store boolean as property)
--
SELECT * FROM cypher('pattern_expr', $$
    MATCH (a:Person)
    SET a.is_social = (a)-[:KNOWS]->(:Person)
    RETURN a.name, a.is_social
    ORDER BY a.name
$$) AS (name agtype, is_social agtype);

--
-- Pattern expression in WITH (carry boolean through pipeline)
--
SELECT * FROM cypher('pattern_expr', $$
    MATCH (a:Person)
    WITH a.name AS name, (a)-[:KNOWS]->(:Person) AS knows
    WHERE knows
    RETURN name
    ORDER BY name
$$) AS (result agtype);

--
-- Follow-up coverage (review #2360): pattern expressions in additional
-- expression contexts opened up by allowing anonymous_path as an expr_atom.
--

--
-- Single-node pattern on an already-bound variable: (a:Label)
--
-- NOTE: as of #2443 a single-node labeled pattern is a correlated label
-- predicate -- in WHERE / EXISTS it tests whether the bound vertex actually
-- has the label (see the WHERE (a:Person) / EXISTS((a:Company)) cases in the
-- #2443 section below). Here the variable is already bound to the SAME label,
-- so the predicate is trivially true (the label matches). A *different* label
-- on an already-bound variable is still rejected by AGE's pre-existing
-- "multiple labels for variable" restriction rather than evaluating to false;
-- that is an orthogonal limitation, captured here so any future change to
-- single-node-pattern semantics is caught by this test.
SELECT * FROM cypher('pattern_expr', $$
    MATCH (a:Person)
    RETURN a.name, (a:Person)
    ORDER BY a.name
$$) AS (name agtype, is_person agtype);

-- A non-matching label errors (pre-existing limitation, not a regression)
SELECT * FROM cypher('pattern_expr', $$
    MATCH (a:Person)
    RETURN a.name, (a:Animal)
    ORDER BY a.name
$$) AS (name agtype, is_animal agtype);

--
-- Pattern expressions inside a list literal
--
SELECT * FROM cypher('pattern_expr', $$
    MATCH (a:Person)
    RETURN a.name, [(a)-[:KNOWS]->(:Person), (a)-[:WORKS_WITH]->(:Person)]
    ORDER BY a.name
$$) AS (name agtype, flags agtype);

--
-- Pattern expressions inside a map literal
--
SELECT * FROM cypher('pattern_expr', $$
    MATCH (a:Person)
    RETURN a.name, {knows: (a)-[:KNOWS]->(:Person), works: (a)-[:WORKS_WITH]->(:Person)}
    ORDER BY a.name
$$) AS (name agtype, m agtype);

--
-- Pattern expressions as function arguments
--
-- collect() shows the per-row boolean values are correct (ORDER BY before
-- the aggregate so the collected order is deterministic across scan plans).
SELECT * FROM cypher('pattern_expr', $$
    MATCH (a:Person)
    WITH a ORDER BY a.name
    RETURN collect((a)-[:KNOWS]->(:Person))
$$) AS (vals agtype);

-- count() counts non-null values; a boolean (including false) is non-null,
-- so this counts every row rather than only the matching ones. This is the
-- expected SQL aggregate semantics, documented here so the value is not
-- mistaken for a bug.
SELECT * FROM cypher('pattern_expr', $$
    MATCH (a:Person)
    RETURN count((a)-[:KNOWS]->(:Person))
$$) AS (c agtype);

--
-- Pattern expression in OPTIONAL MATCH ... WHERE (null-preserving)
--
SELECT * FROM cypher('pattern_expr', $$
    MATCH (a:Person)
    OPTIONAL MATCH (b:Person) WHERE (a)-[:KNOWS]->(b)
    RETURN a.name, b.name
    ORDER BY a.name, b.name
$$) AS (a agtype, b agtype);

--
-- EXISTS() and a bare pattern combined in a single predicate
--
SELECT * FROM cypher('pattern_expr', $$
    MATCH (a:Person)
    WHERE EXISTS((a)-[:KNOWS]->(:Person)) AND (a)-[:WORKS_WITH]->(:Person)
    RETURN a.name
    ORDER BY a.name
$$) AS (name agtype);

--
-- Single-node labeled pattern as a boolean (#2443)
--
-- A bound vertex carrying a label, e.g. (a:Person), must test that vertex's
-- label rather than be trivially true. Add a non-Person vertex so the filter
-- is observable (every other vertex in this graph is a :Person).
SELECT * FROM cypher('pattern_expr', $$
    CREATE (:Company {name: 'Acme'})
$$) AS (result agtype);

-- bare single-node label predicate in WHERE: only the :Person vertices
SELECT * FROM cypher('pattern_expr', $$
    MATCH (a)
    WHERE (a:Person)
    RETURN a.name
    ORDER BY a.name
$$) AS (name agtype);

-- negated: only the non-Person vertex
SELECT * FROM cypher('pattern_expr', $$
    MATCH (a)
    WHERE NOT (a:Person)
    RETURN a.name
    ORDER BY a.name
$$) AS (name agtype);

-- EXISTS() form of a single-node label predicate
SELECT * FROM cypher('pattern_expr', $$
    MATCH (a)
    WHERE EXISTS((a:Company))
    RETURN a.name
    ORDER BY a.name
$$) AS (name agtype);

--
-- Cleanup
--
SELECT * FROM drop_graph('pattern_expr', true);
