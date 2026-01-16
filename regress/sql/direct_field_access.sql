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

/*
 * Direct Field Access Optimizations Test
 *
 * Tests for optimizations that directly access agtype fields without
 * using the full iterator machinery or binary search:
 *
 * 1. fill_agtype_value_no_copy() - Read-only access without memory allocation
 * 2. compare_agtype_scalar_containers() - Fast path for scalar comparisons
 * 3. Direct pairs[0] access for vertex/edge id comparison
 * 4. Fast path in get_one_agtype_from_variadic_args()
 */

LOAD 'age';
SET search_path TO ag_catalog;

SELECT create_graph('direct_access');

--
-- Section 1: Scalar Comparison Fast Path Tests
--
-- These tests exercise the compare_agtype_scalar_containers() fast path
-- which uses fill_agtype_value_no_copy() for read-only comparisons.
--

-- Integer comparisons
SELECT * FROM cypher('direct_access', $$
    RETURN 1 < 2, 2 > 1, 1 = 1, 1 <> 2
$$) AS (lt agtype, gt agtype, eq agtype, ne agtype);

SELECT * FROM cypher('direct_access', $$
    RETURN 100 < 50, 100 > 50, 100 = 100, 100 <> 100
$$) AS (lt agtype, gt agtype, eq agtype, ne agtype);

-- Float comparisons
SELECT * FROM cypher('direct_access', $$
    RETURN 1.5 < 2.5, 2.5 > 1.5, 1.5 = 1.5, 1.5 <> 2.5
$$) AS (lt agtype, gt agtype, eq agtype, ne agtype);

-- String comparisons (tests no-copy string pointer)
SELECT * FROM cypher('direct_access', $$
    RETURN 'abc' < 'abd', 'abd' > 'abc', 'abc' = 'abc', 'abc' <> 'abd'
$$) AS (lt agtype, gt agtype, eq agtype, ne agtype);

SELECT * FROM cypher('direct_access', $$
    RETURN 'hello world' < 'hello worlds', 'test' > 'TEST'
$$) AS (lt agtype, gt agtype);

-- Boolean comparisons
SELECT * FROM cypher('direct_access', $$
    RETURN false < true, true > false, true = true, false <> true
$$) AS (lt agtype, gt agtype, eq agtype, ne agtype);

-- Null comparisons
SELECT * FROM cypher('direct_access', $$
    RETURN null = null, null <> null
$$) AS (eq agtype, ne agtype);

-- Mixed numeric type comparisons (integer vs float)
SELECT * FROM cypher('direct_access', $$
    RETURN 1 < 1.5, 2.0 > 1, 1.0 = 1
$$) AS (lt agtype, gt agtype, eq agtype);

-- Numeric type comparisons
SELECT * FROM cypher('direct_access', $$
    RETURN 1.234::numeric < 1.235::numeric,
           1.235::numeric > 1.234::numeric,
           1.234::numeric = 1.234::numeric
$$) AS (lt agtype, gt agtype, eq agtype);

--
-- Section 2: ORDER BY Tests (exercises comparison fast path)
--
-- ORDER BY uses compare_agtype_containers_orderability which now has
-- a fast path for scalar comparisons.
--

-- Integer ORDER BY
SELECT * FROM cypher('direct_access', $$
    UNWIND [5, 3, 8, 1, 9, 2, 7, 4, 6] AS n
    RETURN n ORDER BY n
$$) AS (n agtype);

SELECT * FROM cypher('direct_access', $$
    UNWIND [5, 3, 8, 1, 9, 2, 7, 4, 6] AS n
    RETURN n ORDER BY n DESC
$$) AS (n agtype);

-- String ORDER BY
SELECT * FROM cypher('direct_access', $$
    UNWIND ['banana', 'apple', 'cherry', 'date'] AS s
    RETURN s ORDER BY s
$$) AS (s agtype);

-- Float ORDER BY
SELECT * FROM cypher('direct_access', $$
    UNWIND [3.14, 2.71, 1.41, 1.73] AS f
    RETURN f ORDER BY f
$$) AS (f agtype);

-- Boolean ORDER BY
SELECT * FROM cypher('direct_access', $$
    UNWIND [true, false, true, false] AS b
    RETURN b ORDER BY b
$$) AS (b agtype);

--
-- Section 3: Vertex/Edge Direct ID Access Tests
--
-- These tests exercise the direct pairs[0] access optimization for
-- extracting graphid from vertices and edges during comparison.
--

-- Create test data
SELECT * FROM cypher('direct_access', $$
    CREATE (a:Person {name: 'Alice', age: 30}),
           (b:Person {name: 'Bob', age: 25}),
           (c:Person {name: 'Charlie', age: 35}),
           (d:Person {name: 'Diana', age: 28}),
           (e:Person {name: 'Eve', age: 32}),
           (a)-[:KNOWS {since: 2020}]->(b),
           (b)-[:KNOWS {since: 2019}]->(c),
           (c)-[:KNOWS {since: 2021}]->(d),
           (d)-[:KNOWS {since: 2018}]->(e),
           (e)-[:KNOWS {since: 2022}]->(a)
$$) AS (result agtype);

-- Test max() on vertices (uses compare_agtype_scalar_values with AGTV_VERTEX)
SELECT * FROM cypher('direct_access', $$
    MATCH (p:Person)
    RETURN max(p)
$$) AS (max_vertex agtype);

-- Test min() on vertices
SELECT * FROM cypher('direct_access', $$
    MATCH (p:Person)
    RETURN min(p)
$$) AS (min_vertex agtype);

-- Test max() on edges (uses compare_agtype_scalar_values with AGTV_EDGE)
SELECT * FROM cypher('direct_access', $$
    MATCH ()-[r:KNOWS]->()
    RETURN max(r)
$$) AS (max_edge agtype);

-- Test min() on edges
SELECT * FROM cypher('direct_access', $$
    MATCH ()-[r:KNOWS]->()
    RETURN min(r)
$$) AS (min_edge agtype);

-- ORDER BY on vertices (uses direct id comparison)
SELECT * FROM cypher('direct_access', $$
    MATCH (p:Person)
    RETURN p.name ORDER BY p
$$) AS (name agtype);

SELECT * FROM cypher('direct_access', $$
    MATCH (p:Person)
    RETURN p.name ORDER BY p DESC
$$) AS (name agtype);

-- ORDER BY on edges
SELECT * FROM cypher('direct_access', $$
    MATCH ()-[r:KNOWS]->()
    RETURN r.since ORDER BY r
$$) AS (since agtype);

-- Vertex comparison in WHERE
SELECT * FROM cypher('direct_access', $$
    MATCH (a:Person), (b:Person)
    WHERE a < b
    RETURN a.name, b.name
$$) AS (a_name agtype, b_name agtype);

--
-- Section 4: Fast Path for get_one_agtype_from_variadic_args
--
-- These tests exercise the fast path that bypasses extract_variadic_args
-- when the argument is already agtype.
--

-- Direct agtype comparison operators (use the fast path)
SELECT * FROM cypher('direct_access', $$
    RETURN 42 = 42, 42 <> 43, 42 < 100, 42 > 10
$$) AS (eq agtype, ne agtype, lt agtype, gt agtype);

-- Arithmetic operators (also use the fast path)
SELECT * FROM cypher('direct_access', $$
    RETURN 10 + 5, 10 - 5, 10 * 5, 10 / 5
$$) AS (add agtype, sub agtype, mul agtype, div agtype);

-- String functions that take agtype args
SELECT * FROM cypher('direct_access', $$
    RETURN toUpper('hello'), toLower('WORLD'), size('test')
$$) AS (upper agtype, lower agtype, sz agtype);

-- Type checking functions
SELECT * FROM cypher('direct_access', $$
    RETURN toInteger('42'), toFloat('3.14'), toString(42)
$$) AS (int_val agtype, float_val agtype, str_val agtype);

--
-- Section 5: Direct Field Access for Accessor Functions
--
-- These tests exercise the direct field access macros in id(), start_id(),
-- end_id(), label(), and properties() functions.
--

-- Test id() on vertices (uses AGTYPE_VERTEX_GET_ID macro - index 0)
SELECT * FROM cypher('direct_access', $$
    MATCH (p:Person {name: 'Alice'})
    RETURN id(p)
$$) AS (vertex_id agtype);

-- Test id() on edges (uses AGTYPE_EDGE_GET_ID macro - index 0)
SELECT * FROM cypher('direct_access', $$
    MATCH (a:Person {name: 'Alice'})-[r:KNOWS]->(b:Person {name: 'Bob'})
    RETURN id(r)
$$) AS (edge_id agtype);

-- Test start_id() on edges (uses AGTYPE_EDGE_GET_START_ID macro - index 3)
SELECT * FROM cypher('direct_access', $$
    MATCH (a:Person {name: 'Alice'})-[r:KNOWS]->(b:Person {name: 'Bob'})
    RETURN start_id(r), id(a)
$$) AS (start_id agtype, alice_id agtype);

-- Test end_id() on edges (uses AGTYPE_EDGE_GET_END_ID macro - index 2)
SELECT * FROM cypher('direct_access', $$
    MATCH (a:Person {name: 'Alice'})-[r:KNOWS]->(b:Person {name: 'Bob'})
    RETURN end_id(r), id(b)
$$) AS (end_id agtype, bob_id agtype);

-- Test label() on vertices (uses AGTYPE_VERTEX_GET_LABEL macro - index 1)
SELECT * FROM cypher('direct_access', $$
    MATCH (p:Person {name: 'Alice'})
    RETURN label(p)
$$) AS (vertex_label agtype);

-- Test label() on edges (uses AGTYPE_EDGE_GET_LABEL macro - index 1)
SELECT * FROM cypher('direct_access', $$
    MATCH ()-[r:KNOWS]->()
    RETURN DISTINCT label(r)
$$) AS (edge_label agtype);

-- Test properties() on vertices (uses AGTYPE_VERTEX_GET_PROPERTIES macro - index 2)
SELECT * FROM cypher('direct_access', $$
    MATCH (p:Person {name: 'Alice'})
    RETURN properties(p)
$$) AS (vertex_props agtype);

-- Test properties() on edges (uses AGTYPE_EDGE_GET_PROPERTIES macro - index 4)
SELECT * FROM cypher('direct_access', $$
    MATCH (a:Person {name: 'Alice'})-[r:KNOWS]->(b:Person {name: 'Bob'})
    RETURN properties(r)
$$) AS (edge_props agtype);

-- Combined accessor test - verify all fields are accessible
SELECT * FROM cypher('direct_access', $$
    MATCH (a:Person {name: 'Alice'})-[r:KNOWS]->(b:Person)
    RETURN id(a), label(a), properties(a).name,
           id(r), start_id(r), end_id(r), label(r), properties(r).since,
           id(b), label(b), properties(b).name
$$) AS (a_id agtype, a_label agtype, a_name agtype,
        r_id agtype, r_start agtype, r_end agtype, r_label agtype, r_since agtype,
        b_id agtype, b_label agtype, b_name agtype);

--
-- Section 6: Mixed Comparisons and Edge Cases
--

-- Array comparisons (should NOT use scalar fast path)
SELECT * FROM cypher('direct_access', $$
    RETURN [1,2,3] = [1,2,3], [1,2,3] < [1,2,4]
$$) AS (eq agtype, lt agtype);

-- Object comparisons (should NOT use scalar fast path)
SELECT * FROM cypher('direct_access', $$
    RETURN {a:1, b:2} = {a:1, b:2}
$$) AS (eq agtype);

-- Large integer comparisons
SELECT * FROM cypher('direct_access', $$
    RETURN 9223372036854775807 > 9223372036854775806,
           -9223372036854775808 < -9223372036854775807
$$) AS (big_gt agtype, neg_lt agtype);

-- Empty string comparison
SELECT * FROM cypher('direct_access', $$
    RETURN '' < 'a', '' = ''
$$) AS (lt agtype, eq agtype);

-- Special float values
SELECT * FROM cypher('direct_access', $$
    RETURN 0.0 = -0.0
$$) AS (zero_eq agtype);

--
-- Cleanup
--
SELECT drop_graph('direct_access', true);
