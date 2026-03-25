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

SELECT create_graph('predicate_functions');

--
-- all() predicate function
--
-- all elements satisfy predicate -> true
SELECT * FROM cypher('predicate_functions', $$
    RETURN all(x IN [1, 2, 3] WHERE x > 0)
$$) AS (result agtype);

-- not all elements satisfy predicate -> false
SELECT * FROM cypher('predicate_functions', $$
    RETURN all(x IN [1, 2, 3] WHERE x > 1)
$$) AS (result agtype);

-- empty list -> true (vacuous truth)
SELECT * FROM cypher('predicate_functions', $$
    RETURN all(x IN [] WHERE x > 0)
$$) AS (result agtype);

--
-- any() predicate function
--
-- at least one element satisfies -> true
SELECT * FROM cypher('predicate_functions', $$
    RETURN any(x IN [1, 2, 3] WHERE x > 2)
$$) AS (result agtype);

-- no element satisfies -> false
SELECT * FROM cypher('predicate_functions', $$
    RETURN any(x IN [1, 2, 3] WHERE x > 5)
$$) AS (result agtype);

-- empty list -> false
SELECT * FROM cypher('predicate_functions', $$
    RETURN any(x IN [] WHERE x > 0)
$$) AS (result agtype);

--
-- none() predicate function
--
-- no element satisfies predicate -> true
SELECT * FROM cypher('predicate_functions', $$
    RETURN none(x IN [1, 2, 3] WHERE x > 5)
$$) AS (result agtype);

-- at least one satisfies -> false
SELECT * FROM cypher('predicate_functions', $$
    RETURN none(x IN [1, 2, 3] WHERE x > 2)
$$) AS (result agtype);

-- empty list -> true
SELECT * FROM cypher('predicate_functions', $$
    RETURN none(x IN [] WHERE x > 0)
$$) AS (result agtype);

--
-- single() predicate function
--
-- exactly one element satisfies -> true
SELECT * FROM cypher('predicate_functions', $$
    RETURN single(x IN [1, 2, 3] WHERE x > 2)
$$) AS (result agtype);

-- more than one satisfies -> false
SELECT * FROM cypher('predicate_functions', $$
    RETURN single(x IN [1, 2, 3] WHERE x > 1)
$$) AS (result agtype);

-- none satisfies -> false
SELECT * FROM cypher('predicate_functions', $$
    RETURN single(x IN [1, 2, 3] WHERE x > 5)
$$) AS (result agtype);

-- empty list -> false
SELECT * FROM cypher('predicate_functions', $$
    RETURN single(x IN [] WHERE x > 0)
$$) AS (result agtype);

--
-- Integration with graph data
--
SELECT * FROM cypher('predicate_functions', $$
    CREATE ({name: 'even', vals: [2, 4, 6, 8]})
$$) AS (result agtype);

SELECT * FROM cypher('predicate_functions', $$
    CREATE ({name: 'mixed', vals: [1, 2, 3, 4]})
$$) AS (result agtype);

SELECT * FROM cypher('predicate_functions', $$
    CREATE ({name: 'odd', vals: [1, 3, 5, 7]})
$$) AS (result agtype);

-- all() with graph properties
SELECT * FROM cypher('predicate_functions', $$
    MATCH (u) WHERE all(x IN u.vals WHERE x % 2 = 0)
    RETURN u.name
    ORDER BY u.name
$$) AS (result agtype);

-- any() with graph properties
SELECT * FROM cypher('predicate_functions', $$
    MATCH (u) WHERE any(x IN u.vals WHERE x > 6)
    RETURN u.name
    ORDER BY u.name
$$) AS (result agtype);

-- none() with graph properties
SELECT * FROM cypher('predicate_functions', $$
    MATCH (u) WHERE none(x IN u.vals WHERE x < 0)
    RETURN u.name
    ORDER BY u.name
$$) AS (result agtype);

-- single() with graph properties
SELECT * FROM cypher('predicate_functions', $$
    MATCH (u) WHERE single(x IN u.vals WHERE x = 8)
    RETURN u.name
    ORDER BY u.name
$$) AS (result agtype);

--
-- Predicate functions in boolean expressions
--
SELECT * FROM cypher('predicate_functions', $$
    RETURN any(x IN [1, 2, 3] WHERE x > 2)
           AND all(y IN [4, 5, 6] WHERE y > 0)
$$) AS (result agtype);

SELECT * FROM cypher('predicate_functions', $$
    RETURN none(x IN [1, 2, 3] WHERE x > 5)
           OR single(y IN [1, 2, 3] WHERE y = 2)
$$) AS (result agtype);

--
-- Nested predicate functions
--
SELECT * FROM cypher('predicate_functions', $$
    RETURN any(x IN [1, 2, 3] WHERE all(y IN [1, 2] WHERE y < x))
$$) AS (result agtype);

--
-- Keywords as property key names (safe_keywords backward compatibility)
--
SELECT * FROM cypher('predicate_functions', $$
    RETURN {any: 1, none: 2, single: 3}
$$) AS (result agtype);

--
-- Cleanup
--
SELECT * FROM drop_graph('predicate_functions', true);
