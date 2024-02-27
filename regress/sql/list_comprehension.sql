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

SELECT * FROM cypher('list_comprehension', $$ RETURN [u FROM range(1, 30, 2)] $$) AS (result agtype);
SELECT * FROM cypher('list_comprehension', $$ RETURN [u FROM range(1, 30, 2)][2] $$) AS (result agtype);
SELECT * FROM cypher('list_comprehension', $$ RETURN [u FROM range(1, 30, 2)][1..4] $$) AS (result agtype);

SELECT * FROM cypher('list_comprehension', $$ RETURN [u FROM range(1, 30, 2) WHERE u % 3 = 0] $$) AS (result agtype);
SELECT * FROM cypher('list_comprehension', $$ RETURN [u FROM range(1, 30, 2) WHERE u % 3 = 0][2] $$) AS (result agtype);
SELECT * FROM cypher('list_comprehension', $$ RETURN [u FROM range(1, 30, 2) WHERE u % 3 = 0][0..4] $$) AS (result agtype);

SELECT * FROM cypher('list_comprehension', $$ RETURN [u FROM range(1, 30, 2) WHERE u % 3 = 0 | u^2 ] $$) AS (result agtype);
SELECT * FROM cypher('list_comprehension', $$ RETURN [u FROM range(1, 30, 2) WHERE u % 3 = 0 | u^2 ][3] $$) AS (result agtype);
SELECT * FROM cypher('list_comprehension', $$ RETURN [u FROM range(1, 30, 2) WHERE u % 3 = 0 | u^2 ][1..5] $$) AS (result agtype);

SELECT * FROM cypher('list_comprehension', $$ RETURN [u FROM range(1, 30, 2) | u^2 ] $$) AS (result agtype);
SELECT * FROM cypher('list_comprehension', $$ RETURN [u FROM range(1, 30, 2) | u^2 ][0] $$) AS (result agtype);
SELECT * FROM cypher('list_comprehension', $$ RETURN [u FROM range(1, 30, 2) | u^2 ][0..2] $$) AS (result agtype);

SELECT * FROM cypher('list_comprehension', $$ MATCH (u) RETURN (u) $$) AS (result agtype);
SELECT * FROM cypher('list_comprehension', $$ MATCH () RETURN [i FROM range(0, 20, 2) WHERE i % 3 = 0 ] $$) AS (result agtype);
SELECT * FROM cypher('list_comprehension', $$ MATCH () RETURN [i FROM range(0, 20, 2) WHERE i % 3 = 0 | i^2 ] $$) AS (result agtype);
SELECT * FROM cypher('list_comprehension', $$ MATCH (u) RETURN [i FROM range(0, 20, 2) WHERE i % 3 = 0 | i^2 ] $$) AS (result agtype);
SELECT * FROM cypher('list_comprehension', $$ MATCH p=() RETURN [i FROM range(0, 20, 2) WHERE i % 3 = 0 | i^2 ] $$) AS (result agtype);
SELECT * FROM cypher('list_comprehension', $$ MATCH p=(u) RETURN [i FROM range(0, 20, 2) WHERE i % 3 = 0 | i^2 ] $$) AS (result agtype);
SELECT * FROM cypher('list_comprehension', $$ MATCH (u) RETURN [i FROM u.list] $$) AS (result agtype);
SELECT * FROM cypher('list_comprehension', $$ MATCH (u) RETURN [i FROM u.list WHERE i % 3 = 0] $$) AS (result agtype);
SELECT * FROM cypher('list_comprehension', $$ MATCH (u) RETURN [i FROM u.list WHERE i % 3 = 0 | i/3] $$) AS (result agtype);
SELECT * FROM cypher('list_comprehension', $$ MATCH (u) RETURN [i FROM u.list WHERE i % 3 = 0 | i/3][1] $$) AS (result agtype);
SELECT * FROM cypher('list_comprehension', $$ MATCH (u) RETURN [i FROM u.list WHERE i % 3 = 0 | i/3][0..2] $$) AS (result agtype);
SELECT * FROM cypher('list_comprehension', $$ MATCH (u) RETURN [i FROM u.list WHERE i % 3 = 0 | i/3][0..2][1] $$) AS (result agtype);

-- Nested cases
SELECT * FROM cypher('list_comprehension', $$ RETURN [i FROM [i FROM [1,2,3]]] $$) AS (result agtype);
SELECT * FROM cypher('list_comprehension', $$ RETURN [i FROM [i FROM [i FROM [1,2,3]]]] $$) AS (result agtype);

SELECT * FROM cypher('list_comprehension', $$ RETURN [i FROM [i FROM [1,2,3] WHERE i>1]] $$) AS (result agtype);
SELECT * FROM cypher('list_comprehension', $$ RETURN [i FROM [i FROM [1,2,3]] WHERE i>1] $$) AS (result agtype);
SELECT * FROM cypher('list_comprehension', $$ RETURN [i FROM [i FROM [1,2,3] WHERE i>1] WHERE i>2] $$) AS (result agtype);

SELECT * FROM cypher('list_comprehension', $$ RETURN [i FROM [i FROM [1,2,3] WHERE i>1 | i^2]] $$) AS (result agtype);
SELECT * FROM cypher('list_comprehension', $$ RETURN [i FROM [i FROM [1,2,3]] WHERE i>1 | i^2] $$) AS (result agtype);
SELECT * FROM cypher('list_comprehension', $$ RETURN [i FROM [i FROM [1,2,3] WHERE i>1] WHERE i>2 | i^2] $$) AS (result agtype);
SELECT * FROM cypher('list_comprehension', $$ RETURN [i FROM [i FROM [1,2,3] WHERE i>1 | i^2] WHERE i>4] $$) AS (result agtype);
SELECT * FROM cypher('list_comprehension', $$ RETURN [i FROM [i FROM [1,2,3] WHERE i>1 | i^2] WHERE i>4 | i^2] $$) AS (result agtype);

-- List comprehension inside where and property constraints
SELECT * FROM cypher('list_comprehension', $$ MATCH (u) WHERE u.list=[i FROM range(0,12,2)] RETURN u $$) AS (result agtype);
SELECT * FROM cypher('list_comprehension', $$ MATCH (u) WHERE u.list=[i FROM range(1,13,2)] RETURN u $$) AS (result agtype);
SELECT * FROM cypher('list_comprehension', $$ MATCH (u) WHERE u.list=[i FROM range(0,12,2) WHERE i>4] RETURN u $$) AS (result agtype);
SELECT * FROM cypher('list_comprehension', $$ MATCH (u) WHERE u.list=[i FROM range(1,13,2) WHERE i>4 | i^1] RETURN u $$) AS (result agtype);
SELECT * FROM cypher('list_comprehension', $$ MATCH (u) WHERE u.list@>[i FROM range(0,6,2)] RETURN u $$) AS (result agtype);
SELECT * FROM cypher('list_comprehension', $$ MATCH (u) MATCH (v) WHERE v.list=[i FROM u.list] RETURN v $$) AS (result agtype);

SELECT * FROM cypher('list_comprehension', $$ MATCH (u {list:[i FROM range(0,12,2)]}) RETURN u $$) AS (result agtype);
SELECT * FROM cypher('list_comprehension', $$ MATCH (u {list:[i FROM range(1,13,2)]}) RETURN u $$) AS (result agtype);
SELECT * FROM cypher('list_comprehension', $$ MATCH (u {list:[i FROM range(0,12,2) WHERE i>4]}) RETURN u $$) AS (result agtype);
SELECT * FROM cypher('list_comprehension', $$ MATCH (u {list:[i FROM range(1,13,2) WHERE i>4 | i^1]}) RETURN u $$) AS (result agtype);
SELECT * FROM cypher('list_comprehension', $$ MATCH (u) MATCH (v {list:[i FROM u.list]}) RETURN v $$) AS (result agtype);

SELECT * FROM cypher('list_comprehension', $$ CREATE (u {list:[i FROM range(12,24,2)]}) RETURN u $$) AS (result agtype);
SELECT * FROM cypher('list_comprehension', $$ CREATE (u {list:[i FROM range(0,12,2) WHERE i>4]}) RETURN u $$) AS (result agtype);
SELECT * FROM cypher('list_comprehension', $$ CREATE (u {list:[i FROM range(1,13,2) WHERE i>4 | i^2]}) RETURN u $$) AS (result agtype);
SELECT * FROM cypher('list_comprehension', $$ WITH [u FROM [1,2,3]] AS a CREATE (u {list:a}) RETURN u $$) AS (result agtype);

-- List comprehension in the WITH WHERE clause
SELECT * FROM cypher('list_comprehension', $$ MATCH(u) WITH u WHERE u.list = [u FROM [0, 2, 4, 6, 8, 10, 12]] RETURN u $$) AS (u agtype);
SELECT * FROM cypher('list_comprehension', $$ MATCH(u) WITH u WHERE u.list = [u FROM [0, 2, 4, 6, 8, 10, 12]] OR size(u.list) = 0 RETURN u $$) AS (u agtype);
SELECT * FROM cypher('list_comprehension', $$ MATCH(u) WITH u WHERE u.list = [u FROM [0, 2, 4, 6, 8, 10, 12]] OR size(u.list)::bool RETURN u $$) AS (u agtype);
SELECT * FROM cypher('list_comprehension', $$ MATCH(u) WITH u WHERE u.list = [u FROM [0, 2, 4, 6, 8, 10, 12]] OR NOT size(u.list)::bool RETURN u $$) AS (u agtype);

SELECT * FROM cypher('list_comprehension', $$ CREATE(u:csm_match {list: ['abc', 'def', 'ghi']}) $$) AS (u agtype);
SELECT * FROM cypher('list_comprehension', $$ MATCH(u: csm_match) WITH u WHERE [u FROM u.list][0] STARTS WITH "ab" RETURN u $$) AS (u agtype);

SELECT * FROM cypher('list_comprehension', $$ WITH [1, 2, 3] AS u WHERE u = [u FROM [1, 2, 3]] RETURN u $$) AS (u agtype);
SELECT * FROM cypher('list_comprehension', $$ WITH 1 AS u WHERE u = [u FROM [1, 2, 3]][0] RETURN u $$) AS (u agtype);
SELECT * FROM cypher('list_comprehension', $$ WITH ['abc', 'defgh'] AS u WHERE [v FROM u][1] STARTS WITH 'de' RETURN u $$) AS (u agtype);

-- List comprehension in UNWIND
SELECT * FROM cypher('list_comprehension', $$ UNWIND [u FROM [1, 2, 3]] AS u RETURN u $$) AS (u agtype);
SELECT * FROM cypher('list_comprehension', $$ UNWIND [u FROM [1, 2, 3] WHERE u > 1 | u*2] AS u RETURN u $$) AS (u agtype);

-- invalid use of aggregation in UNWIND
SELECT * FROM cypher('list_comprehension', $$ WITH [1, 2, 3] AS u UNWIND collect(u) AS v RETURN v $$) AS (u agtype);

-- List comprehension in SET
SELECT * FROM cypher('list_comprehension', $$ MATCH(u {list: [0, 2, 4, 6, 8, 10, 12]}) SET u.a = [u FROM range(0, 5)] RETURN u $$) AS (u agtype);
SELECT * FROM cypher('list_comprehension', $$ MATCH(u {list: [0, 2, 4, 6, 8, 10, 12]}) SET u.a = [u FROM []] RETURN u $$) AS (u agtype);
SELECT * FROM cypher('list_comprehension', $$ MATCH(u {list: [0, 2, 4, 6, 8, 10, 12]}) SET u += {b: [u FROM range(0, 5)]} RETURN u $$) AS (u agtype);
SELECT * FROM cypher('list_comprehension', $$ MATCH(u {list: [0, 2, 4, 6, 8, 10, 12]}) WITh u, collect(u.list) AS v SET u += {b: [u FROM range(0, 5)]} SET u.c = [u FROM v[0]] RETURN u $$) AS (u agtype);

-- invalid use of aggregation in SET
SELECT * FROM cypher('list_comprehension', $$ MATCH(u {list: [0, 2, 4, 6, 8, 10, 12]}) SET u.c = collect(u.list) RETURN u $$) AS (u agtype);

-- Known issue
SELECT * FROM cypher('list_comprehension', $$ MERGE (u {list:[i FROM [1,2,3]]}) RETURN u $$) AS (result agtype);

-- List comprehension variable scoping
SELECT * FROM cypher('list_comprehension', $$ WITH 1 AS m, [m FROM [1, 2, 3]] AS n RETURN [m FROM [1, 2, 3]] $$) AS (result agtype);
SELECT * FROM cypher('list_comprehension', $$ WITH 1 AS m RETURN [m FROM [1, 2, 3]], m $$) AS (result agtype, result2 agtype);
SELECT * FROM cypher('list_comprehension', $$ WITH [m FROM [1,2,3]] AS m RETURN [m FROM [1, 2, 3]], m $$) AS (result agtype, result2 agtype);
SELECT * FROM cypher('list_comprehension', $$ CREATE n=()-[:edge]->() RETURN [n FROM nodes(n)] $$) AS (u agtype);

-- Multiple list comprehensions in RETURN and WITH clause
SELECT * FROM cypher('list_comprehension', $$ RETURN [u FROM [1,2,3]], [u FROM [1,2,3]] $$) AS (result agtype, result2 agtype);
SELECT * FROM cypher('list_comprehension', $$ RETURN [u FROM [1,2,3] WHERE u>1], [u FROM [1,2,3] WHERE u>2] $$) AS (result agtype, result2 agtype);
SELECT * FROM cypher('list_comprehension', $$ RETURN [u FROM [1,2,3] WHERE u>1 | u^3], [u FROM [1,2,3] WHERE u>2 | u^3] $$) AS (result agtype, result2 agtype);

SELECT * FROM cypher('list_comprehension', $$ RETURN [u FROM [1,2,3]] AS u, [u FROM [1,2,3]] AS i $$) AS (result agtype, result2 agtype);
SELECT * FROM cypher('list_comprehension', $$ RETURN [u FROM [1,2,3] WHERE u>1] AS u, [u FROM [1,2,3] WHERE u>2] AS i $$) AS (result agtype, result2 agtype);
SELECT * FROM cypher('list_comprehension', $$ RETURN [u FROM [1,2,3] WHERE u>1 | u^3] AS u, [u FROM [1,2,3] WHERE u>2 | u^3] AS i $$) AS (result agtype, result2 agtype);

SELECT * FROM cypher('list_comprehension', $$ RETURN [u FROM [u FROM [1,2,3]]], [u FROM [u FROM [1,2,3]]] $$) AS (result agtype, result2 agtype);
SELECT * FROM cypher('list_comprehension', $$ RETURN [u FROM [u FROM [1,2,3] WHERE u>1] WHERE u>2], [u FROM [u FROM [1,2,3] WHERE u>1] WHERE u>2] $$) AS (result agtype, result2 agtype);
SELECT * FROM cypher('list_comprehension', $$ RETURN [u FROM [u FROM [1,2,3] WHERE u>1] WHERE u>2 | u^3], [u FROM [u FROM [1,2,3] WHERE u>1] WHERE u>2 | u^3] $$) AS (result agtype, result2 agtype);

SELECT * FROM cypher('list_comprehension', $$ WITH [u FROM [1,2,3]] AS u, [u FROM [1,2,3]] AS i RETURN u, i $$) AS (result agtype, result2 agtype);

-- Should error out
SELECT * FROM cypher('list_comprehension', $$ RETURN [i FROM range(0, 10, 2)],i $$) AS (result agtype, i agtype);
SELECT * FROM cypher('list_comprehension', $$ RETURN [i FROM range(0, 10, 2) WHERE i>5 | i^2], i $$) AS (result agtype, i agtype);

SELECT * FROM drop_graph('list_comprehension', true);
