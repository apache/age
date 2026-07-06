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
-- agtype -> jsonb casts
--

-- String scalar
SELECT '"hello"'::agtype::jsonb;

-- Null
SELECT 'null'::agtype::jsonb;

-- Array
SELECT '[1, "two", null]'::agtype::jsonb;

-- Nested array
SELECT '[[1, 2], [3, 4]]'::agtype::jsonb;

-- Empty array
SELECT '[]'::agtype::jsonb;

-- Object
SELECT '{"name": "Alice", "age": 30}'::agtype::jsonb;

-- Nested object
SELECT '{"a": {"b": {"c": 1}}}'::agtype::jsonb;

-- Object with array values
SELECT '{"tags": ["a", "b"], "count": 2}'::agtype::jsonb;

-- Empty object
SELECT '{}'::agtype::jsonb;

--
-- jsonb -> agtype casts
--

-- String scalar
SELECT '"hello"'::jsonb::agtype;

-- Numeric scalar
SELECT '42'::jsonb::agtype;

-- Float scalar
SELECT '3.14'::jsonb::agtype;

-- Boolean
SELECT 'true'::jsonb::agtype;

-- Null
SELECT 'null'::jsonb::agtype;

-- Array
SELECT '[1, "two", null]'::jsonb::agtype;

-- Nested array
SELECT '[[1, 2], [3, 4]]'::jsonb::agtype;

-- Empty array
SELECT '[]'::jsonb::agtype;

-- Object
SELECT '{"name": "Alice", "age": 30}'::jsonb::agtype;

-- Nested object
SELECT '{"a": {"b": {"c": 1}}}'::jsonb::agtype;

-- Empty object
SELECT '{}'::jsonb::agtype;

--
-- Roundtrip: jsonb -> agtype -> jsonb
--

SELECT ('{"key": "value"}'::jsonb::agtype)::jsonb;
SELECT ('[1, 2, 3]'::jsonb::agtype)::jsonb;
SELECT ('null'::jsonb::agtype)::jsonb;

--
-- Graph data -> jsonb (vertex and edge)
--

SELECT create_graph('agtype_jsonb_test');

SELECT * FROM cypher('agtype_jsonb_test', $$
    CREATE (a:Person {name: 'Alice', age: 30})-[:KNOWS {since: 2020}]->(b:Person {name: 'Bob', age: 25})
    RETURN a, b
$$) AS (a agtype, b agtype);

-- Vertex to jsonb: check structure
SELECT v::jsonb ? 'label' AS has_label,
       v::jsonb ? 'properties' AS has_properties,
       (v::jsonb -> 'properties' ->> 'name') AS name
FROM cypher('agtype_jsonb_test', $$
    MATCH (n:Person) RETURN n ORDER BY n.name
$$) AS (v agtype);

-- Edge to jsonb: check structure
SELECT e::jsonb ? 'label' AS has_label,
       (e::jsonb ->> 'label') AS label,
       (e::jsonb -> 'properties' ->> 'since') AS since
FROM cypher('agtype_jsonb_test', $$
    MATCH ()-[r:KNOWS]->() RETURN r
$$) AS (e agtype);

--
-- NULL handling
--

SELECT NULL::agtype::jsonb;
SELECT NULL::jsonb::agtype;

--
-- Cleanup
--

SELECT drop_graph('agtype_jsonb_test', true);
