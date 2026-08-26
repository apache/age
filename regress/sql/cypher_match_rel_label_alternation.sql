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
 *
 * Tests for relationship-type alternation in MATCH patterns:
 * `[:A|B|C]` and the equivalent `[:A|:B|:C]` openCypher form.
 */
LOAD 'age';
SET search_path TO ag_catalog;

SELECT create_graph('rel_label_alt');

SELECT create_elabel('rel_label_alt', 'WORKS_AT');
SELECT create_elabel('rel_label_alt', 'REPORTS_TO');
SELECT create_elabel('rel_label_alt', 'OWNS');

SELECT * FROM cypher('rel_label_alt', $$
    CREATE (a:Person {n:'alice'})-[:WORKS_AT]->(:Company {n:'acme'}),
           (a)-[:REPORTS_TO]->(:Person {n:'bob'}),
           (a)-[:OWNS]->(:Asset {n:'car'})
$$) AS (v agtype);

-- Two-label alternation. Returns two rows ordered by destination name.
SELECT * FROM cypher('rel_label_alt', $$
    MATCH (p)-[:WORKS_AT|REPORTS_TO]->(c) RETURN c.n ORDER BY c.n
$$) AS (n agtype);

-- Three-label alternation. All three outgoing edges match.
SELECT * FROM cypher('rel_label_alt', $$
    MATCH (p)-[:WORKS_AT|REPORTS_TO|OWNS]->(c) RETURN c.n ORDER BY c.n
$$) AS (n agtype);

-- Redundant-colon openCypher form [:A|:B] is equivalent to [:A|B].
SELECT * FROM cypher('rel_label_alt', $$
    MATCH (p)-[:WORKS_AT|:REPORTS_TO]->(c) RETURN c.n ORDER BY c.n
$$) AS (n agtype);

-- Single-label edge is unchanged (regression check).
SELECT * FROM cypher('rel_label_alt', $$
    MATCH (p)-[:WORKS_AT]->(c) RETURN c.n
$$) AS (n agtype);

-- Edge variable + multi-label, projecting type(r).
SELECT * FROM cypher('rel_label_alt', $$
    MATCH (p)-[r:WORKS_AT|REPORTS_TO]->(c)
    RETURN type(r) AS t, c.n AS n ORDER BY n
$$) AS (t agtype, n agtype);

-- Unknown labels in the alternation are silently ignored — only the
-- valid label contributes rows.
SELECT * FROM cypher('rel_label_alt', $$
    MATCH (p)-[:WORKS_AT|DOES_NOT_EXIST]->(c) RETURN c.n
$$) AS (n agtype);

-- All labels in the alternation are unknown — yields zero rows, not an
-- error.
SELECT * FROM cypher('rel_label_alt', $$
    MATCH (p)-[:DOES_NOT_EXIST|ALSO_MISSING]->(c) RETURN c.n
$$) AS (n agtype);

-- Multi-label edge inside a two-edge path; checks the qual integrates
-- with the join-tree quals correctly.
SELECT * FROM cypher('rel_label_alt', $$
    MATCH (a:Person {n:'alice'})-[:WORKS_AT|OWNS]->(x)
    RETURN x.n ORDER BY x.n
$$) AS (n agtype);

-- Undirected alternation `-[:A|B]-` matches edges in either direction.
SELECT * FROM cypher('rel_label_alt', $$
    MATCH (p:Person {n:'alice'})-[:WORKS_AT|REPORTS_TO]-(c) RETURN c.n ORDER BY c.n
$$) AS (n agtype);

-- Reverse-directed alternation `<-[:A|B]-`.
SELECT * FROM cypher('rel_label_alt', $$
    MATCH (b:Person {n:'bob'})<-[:WORKS_AT|REPORTS_TO]-(p) RETURN p.n ORDER BY p.n
$$) AS (n agtype);

-- Variable-length relationship combined with alternation is not yet
-- supported and must be rejected with a clear error.
SELECT * FROM cypher('rel_label_alt', $$
    MATCH (p)-[:WORKS_AT|REPORTS_TO*1..2]->(c) RETURN c.n
$$) AS (n agtype);

SELECT drop_graph('rel_label_alt', true);
