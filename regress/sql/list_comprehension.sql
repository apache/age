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

SELECT create_graph('list_comprehension');

SELECT * from cypher('list_comprehension', $$ CREATE (u {list: [0, 2, 4, 6, 8, 10, 12]}) RETURN u $$) AS (result agtype);
SELECT * from cypher('list_comprehension', $$ CREATE (u {list: [1, 3, 5, 7, 9, 11, 13]}) RETURN u $$) AS (result agtype);
SELECT * from cypher('list_comprehension', $$ CREATE (u {list: []}) RETURN u $$) AS (result agtype);

SELECT * FROM cypher('list_comprehension', $$ RETURN [u IN range(1, 30, 2)] $$) AS (result agtype);
SELECT * FROM cypher('list_comprehension', $$ RETURN [u IN range(1, 30, 2)][2] $$) AS (result agtype);
SELECT * FROM cypher('list_comprehension', $$ RETURN [u IN range(1, 30, 2)][1..4] $$) AS (result agtype);

SELECT * FROM cypher('list_comprehension', $$ RETURN [u IN range(1, 30, 2) WHERE u % 3 = 0] $$) AS (result agtype);
SELECT * FROM cypher('list_comprehension', $$ RETURN [u IN range(1, 30, 2) WHERE u % 3 = 0][2] $$) AS (result agtype);
SELECT * FROM cypher('list_comprehension', $$ RETURN [u IN range(1, 30, 2) WHERE u % 3 = 0][0..4] $$) AS (result agtype);

SELECT * FROM cypher('list_comprehension', $$ RETURN [u IN range(1, 30, 2) WHERE u % 3 = 0 | u^2 ] $$) AS (result agtype);
SELECT * FROM cypher('list_comprehension', $$ RETURN [u IN range(1, 30, 2) WHERE u % 3 = 0 | u^2 ][3] $$) AS (result agtype);
SELECT * FROM cypher('list_comprehension', $$ RETURN [u IN range(1, 30, 2) WHERE u % 3 = 0 | u^2 ][1..5] $$) AS (result agtype);

SELECT * FROM cypher('list_comprehension', $$ RETURN [u IN range(1, 30, 2) | u^2 ] $$) AS (result agtype);
SELECT * FROM cypher('list_comprehension', $$ RETURN [u IN range(1, 30, 2) | u^2 ][0] $$) AS (result agtype);
SELECT * FROM cypher('list_comprehension', $$ RETURN [u IN range(1, 30, 2) | u^2 ][0..2] $$) AS (result agtype);

SELECT * FROM cypher('list_comprehension', $$ MATCH (u) RETURN (u) $$) AS (result agtype);
SELECT * FROM cypher('list_comprehension', $$ MATCH () RETURN [i IN range(0, 20, 2) WHERE i % 3 = 0 ] $$) AS (result agtype);
SELECT * FROM cypher('list_comprehension', $$ MATCH () RETURN [i IN range(0, 20, 2) WHERE i % 3 = 0 | i^2 ] $$) AS (result agtype);
SELECT * FROM cypher('list_comprehension', $$ MATCH (u) RETURN [i IN range(0, 20, 2) WHERE i % 3 = 0 | i^2 ] $$) AS (result agtype);
SELECT * FROM cypher('list_comprehension', $$ MATCH p=() RETURN [i IN range(0, 20, 2) WHERE i % 3 = 0 | i^2 ] $$) AS (result agtype);
SELECT * FROM cypher('list_comprehension', $$ MATCH p=(u) RETURN [i IN range(0, 20, 2) WHERE i % 3 = 0 | i^2 ] $$) AS (result agtype);
SELECT * FROM cypher('list_comprehension', $$ MATCH (u) RETURN [i IN u.list] $$) AS (result agtype);
SELECT * FROM cypher('list_comprehension', $$ MATCH (u) RETURN [i IN u.list WHERE i % 3 = 0] $$) AS (result agtype);
SELECT * FROM cypher('list_comprehension', $$ MATCH (u) RETURN [i IN u.list WHERE i % 3 = 0 | i/3] $$) AS (result agtype);
SELECT * FROM cypher('list_comprehension', $$ MATCH (u) RETURN [i IN u.list WHERE i % 3 = 0 | i/3][1] $$) AS (result agtype);
SELECT * FROM cypher('list_comprehension', $$ MATCH (u) RETURN [i IN u.list WHERE i % 3 = 0 | i/3][0..2] $$) AS (result agtype);
SELECT * FROM cypher('list_comprehension', $$ MATCH (u) RETURN [i IN u.list WHERE i % 3 = 0 | i/3][0..2][1] $$) AS (result agtype);

-- Nested cases
SELECT * FROM cypher('list_comprehension', $$ RETURN [i IN [i IN [1,2,3]]] $$) AS (result agtype);
SELECT * FROM cypher('list_comprehension', $$ RETURN [i IN [i IN [i IN [1,2,3]]]] $$) AS (result agtype);

SELECT * FROM cypher('list_comprehension', $$ RETURN [i IN [i IN [1,2,3] WHERE i>1]] $$) AS (result agtype);
SELECT * FROM cypher('list_comprehension', $$ RETURN [i IN [i IN [1,2,3]] WHERE i>1] $$) AS (result agtype);
SELECT * FROM cypher('list_comprehension', $$ RETURN [i IN [i IN [1,2,3] WHERE i>1] WHERE i>2] $$) AS (result agtype);

SELECT * FROM cypher('list_comprehension', $$ RETURN [i IN [i IN [1,2,3] WHERE i>1 | i^2]] $$) AS (result agtype);
SELECT * FROM cypher('list_comprehension', $$ RETURN [i IN [i IN [1,2,3]] WHERE i>1 | i^2] $$) AS (result agtype);
SELECT * FROM cypher('list_comprehension', $$ RETURN [i IN [i IN [1,2,3] WHERE i>1] WHERE i>2 | i^2] $$) AS (result agtype);
SELECT * FROM cypher('list_comprehension', $$ RETURN [i IN [i IN [1,2,3] WHERE i>1 | i^2] WHERE i>4] $$) AS (result agtype);
SELECT * FROM cypher('list_comprehension', $$ RETURN [i IN [i IN [1,2,3] WHERE i>1 | i^2] WHERE i>4 | i^2] $$) AS (result agtype);

-- List comprehension inside where and property constraints
SELECT * FROM cypher('list_comprehension', $$ MATCH (u) WHERE u.list=[i IN range(0,12,2)] RETURN u $$) AS (result agtype);
SELECT * FROM cypher('list_comprehension', $$ MATCH (u) WHERE u.list=[i IN range(1,13,2)] RETURN u $$) AS (result agtype);
SELECT * FROM cypher('list_comprehension', $$ MATCH (u) WHERE u.list=[i IN range(0,12,2) WHERE i>4] RETURN u $$) AS (result agtype);
SELECT * FROM cypher('list_comprehension', $$ MATCH (u) WHERE u.list=[i IN range(1,13,2) WHERE i>4 | i^1] RETURN u $$) AS (result agtype);
SELECT * FROM cypher('list_comprehension', $$ MATCH (u) WHERE u.list@>[i IN range(0,6,2)] RETURN u $$) AS (result agtype);
SELECT * FROM cypher('list_comprehension', $$ MATCH (u) MATCH (v) WHERE v.list=[i IN u.list] RETURN v $$) AS (result agtype);

SELECT * FROM cypher('list_comprehension', $$ MATCH (u {list:[i IN range(0,12,2)]}) RETURN u $$) AS (result agtype);
SELECT * FROM cypher('list_comprehension', $$ MATCH (u {list:[i IN range(1,13,2)]}) RETURN u $$) AS (result agtype);
SELECT * FROM cypher('list_comprehension', $$ MATCH (u {list:[i IN range(0,12,2) WHERE i>4]}) RETURN u $$) AS (result agtype);
SELECT * FROM cypher('list_comprehension', $$ MATCH (u {list:[i IN range(1,13,2) WHERE i>4 | i^1]}) RETURN u $$) AS (result agtype);
SELECT * FROM cypher('list_comprehension', $$ MATCH (u) MATCH (v {list:[i IN u.list]}) RETURN v $$) AS (result agtype);

SELECT * FROM cypher('list_comprehension', $$ CREATE (u {list:[i IN range(12,24,2)]}) RETURN u $$) AS (result agtype);
SELECT * FROM cypher('list_comprehension', $$ CREATE (u {list:[i IN range(0,12,2) WHERE i>4]}) RETURN u $$) AS (result agtype);
SELECT * FROM cypher('list_comprehension', $$ CREATE (u {list:[i IN range(1,13,2) WHERE i>4 | i^2]}) RETURN u $$) AS (result agtype);
SELECT * FROM cypher('list_comprehension', $$ WITH [u IN [1,2,3]] AS a CREATE (u {list:a}) RETURN u $$) AS (result agtype);

-- Known issue
SELECT * FROM cypher('list_comprehension', $$ MERGE (u {list:[i IN [1,2,3]]}) RETURN u $$) AS (result agtype);

-- List comprehension variable scoping
SELECT * FROM cypher('list_comprehension', $$ WITH 1 AS m, [m IN [1, 2, 3]] AS n RETURN [m IN [1, 2, 3]] $$) AS (result agtype);
SELECT * FROM cypher('list_comprehension', $$ WITH 1 AS m RETURN [m IN [1, 2, 3]], m $$) AS (result agtype, result2 agtype);
SELECT * FROM cypher('list_comprehension', $$ WITH [m IN [1,2,3]] AS m RETURN [m IN [1, 2, 3]], m $$) AS (result agtype, result2 agtype);

-- Multiple list comprehensions in RETURN and WITH clause
SELECT * FROM cypher('list_comprehension', $$ RETURN [u IN [1,2,3]], [u IN [1,2,3]] $$) AS (result agtype, result2 agtype);
SELECT * FROM cypher('list_comprehension', $$ RETURN [u IN [1,2,3] WHERE u>1], [u IN [1,2,3] WHERE u>2] $$) AS (result agtype, result2 agtype);
SELECT * FROM cypher('list_comprehension', $$ RETURN [u IN [1,2,3] WHERE u>1 | u^3], [u IN [1,2,3] WHERE u>2 | u^3] $$) AS (result agtype, result2 agtype);

SELECT * FROM cypher('list_comprehension', $$ RETURN [u IN [1,2,3]] AS u, [u IN [1,2,3]] AS i $$) AS (result agtype, result2 agtype);
SELECT * FROM cypher('list_comprehension', $$ RETURN [u IN [1,2,3] WHERE u>1] AS u, [u IN [1,2,3] WHERE u>2] AS i $$) AS (result agtype, result2 agtype);
SELECT * FROM cypher('list_comprehension', $$ RETURN [u IN [1,2,3] WHERE u>1 | u^3] AS u, [u IN [1,2,3] WHERE u>2 | u^3] AS i $$) AS (result agtype, result2 agtype);

SELECT * FROM cypher('list_comprehension', $$ RETURN [u IN [u IN [1,2,3]]], [u IN [u IN [1,2,3]]] $$) AS (result agtype, result2 agtype);
SELECT * FROM cypher('list_comprehension', $$ RETURN [u IN [u IN [1,2,3] WHERE u>1] WHERE u>2], [u IN [u IN [1,2,3] WHERE u>1] WHERE u>2] $$) AS (result agtype, result2 agtype);
SELECT * FROM cypher('list_comprehension', $$ RETURN [u IN [u IN [1,2,3] WHERE u>1] WHERE u>2 | u^3], [u IN [u IN [1,2,3] WHERE u>1] WHERE u>2 | u^3] $$) AS (result agtype, result2 agtype);

SELECT * FROM cypher('list_comprehension', $$ WITH [u IN [1,2,3]] AS u, [u IN [1,2,3]] AS i RETURN u, i $$) AS (result agtype, result2 agtype);

-- Should error out
SELECT * FROM cypher('list_comprehension', $$ RETURN [i IN range(0, 10, 2)],i $$) AS (result agtype, i agtype);
SELECT * FROM cypher('list_comprehension', $$ RETURN [i IN range(0, 10, 2) WHERE i>5 | i^2], i $$) AS (result agtype, i agtype);

SELECT * FROM drop_graph('list_comprehension', true);