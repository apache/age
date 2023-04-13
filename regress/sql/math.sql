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
SELECT create_graph('math_test');
-- Factorial function tests age_fact()
-- 0! = 1
SELECT * FROM age_fact(0);
-- 1! = 1
SELECT * FROM age_fact(1);
-- 5! = 120
SELECT * FROM age_fact(5);
-- 10! = 3628800
SELECT * FROM age_fact(100);
-- 20! = 2432902008176640000
SELECT * FROM age_fact(500);


-- Overflow test
SELECT * FROM age_fact(32178);


-- Inside the cypher query
-- At CREATE clause
SELECT * FROM cypher('math_test', $$
CREATE (n:Node {value: fact(5)})
RETURN n.value
$$) AS (value agtype);

-- At MATCH clause
SELECT * FROM cypher('math_test', $$
MATCH (n:Node {value: fact(5)})
RETURN n.value
$$) AS (value agtype);

-- At RETURN clause
SELECT * FROM cypher('math_test', $$
MATCH (n:Node {value: fact(5)})
RETURN fact(toInteger(n.value))
$$) AS (value agtype);

-- At WHERE clause
SELECT * FROM cypher('math_test', $$
MATCH (n:Node)
WHERE n.value = fact(5)
RETURN n.value
$$) AS (value agtype);

-- At SET clause
SELECT * FROM cypher('math_test', $$
MATCH (n:Node)
SET n.value = fact(3)
RETURN n.value
$$) AS (value agtype);

-- CLEANUP

SELECT * FROM drop_graph('math_test', true);

-- End of math.sql



