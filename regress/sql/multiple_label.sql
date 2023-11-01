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


/*
 * MATCH OR Expression (Vertex)
 *
 * TODO: All labels are mutually exclusive. When CREATE for multiple label is
 * implemented write tests for mutually-inclusive MATCHes.
 */

 -- create
SELECT create_graph('multiple-label-1');
SELECT * FROM cypher('multiple-label-1', $$ CREATE (v:A{name:'A1'}) RETURN v $$) as (a agtype);
SELECT * FROM cypher('multiple-label-1', $$ CREATE (v:A{name:'A2'}) RETURN v $$) as (a agtype);
SELECT * FROM cypher('multiple-label-1', $$ CREATE (v:B{name:'B1'}) RETURN v $$) as (a agtype);
SELECT * FROM cypher('multiple-label-1', $$ CREATE (v:B{name:'B2'}) RETURN v $$) as (a agtype);
SELECT * FROM cypher('multiple-label-1', $$ CREATE (v:C{name:'C1'}) RETURN v $$) as (a agtype);
SELECT * FROM cypher('multiple-label-1', $$ CREATE (v:C{name:'C2'}) RETURN v $$) as (a agtype);
SELECT * FROM cypher('multiple-label-1', $$ CREATE (v:D{name:'D1'}) RETURN v $$) as (a agtype);
SELECT * FROM cypher('multiple-label-1', $$ CREATE (v:D{name:'D2'}) RETURN v $$) as (a agtype);

-- general match
SELECT * FROM cypher('multiple-label-1', $$ MATCH (v) RETURN v $$) as (a agtype);
SELECT * FROM cypher('multiple-label-1', $$ MATCH (v:A) RETURN v.name $$) as (a agtype);
SELECT * FROM cypher('multiple-label-1', $$ MATCH (v:A|B) RETURN v.name $$) as (a agtype);
SELECT * FROM cypher('multiple-label-1', $$ MATCH (v:A|B|C) RETURN v.name $$) as (a agtype);
SELECT * FROM cypher('multiple-label-1', $$ MATCH (v:A|B|C|D) RETURN v.name $$) as (a agtype);

-- duplicate (should output same as MATCH (v:A|B))
SELECT * FROM cypher('multiple-label-1', $$ MATCH (v:A|B|A) RETURN v.name $$) as (a agtype);

-- different order (should output same as MATCH (v:A|B))
SELECT * FROM cypher('multiple-label-1', $$ MATCH (v:B|A) RETURN v.name $$) as (a agtype);

-- label M does not exists (should output same as MATCH (v:A|B))
SELECT * FROM cypher('multiple-label-1', $$ MATCH (v:A|B|M) RETURN v.name $$) as (a agtype);

-- no label exists (should output empty)
SELECT * FROM cypher('multiple-label-1', $$ MATCH (v:M|N|O) RETURN v.name $$) as (a agtype);

-- cleanup
SELECT drop_graph('multiple-label-1', true);


/*
 * MATCH OR Expression (Edge)
 */

 -- create
SELECT create_graph('multiple-label-2');
SELECT * FROM cypher('multiple-label-2', $$ CREATE ({name:'a1'})-[e:A]->({name:'a2'}) RETURN e $$) as (a agtype);
SELECT * FROM cypher('multiple-label-2', $$ CREATE ({name:'b1'})-[e:B]->({name:'b2'}) RETURN e $$) as (a agtype);
SELECT * FROM cypher('multiple-label-2', $$ CREATE ({name:'c1'})-[e:C]->({name:'c2'}) RETURN e $$) as (a agtype);

-- general match
SELECT * FROM cypher('multiple-label-2', $$ MATCH (x)-[e]->(y) RETURN x.name, y.name, e $$) as (x agtype, y agtype, e agtype);
SELECT * FROM cypher('multiple-label-2', $$ MATCH (x)-[:A]->(y) RETURN x.name, y.name $$) as (x agtype, y agtype);
SELECT * FROM cypher('multiple-label-2', $$ MATCH (x)-[:A|B]->(y) RETURN x.name, y.name $$) as (x agtype, y agtype);
SELECT * FROM cypher('multiple-label-2', $$ MATCH (x)-[:A|B|C]->(y) RETURN x.name, y.name $$) as (x agtype, y agtype);

-- duplicate (should output same as MATCH (x)-[:A|B]->(y))
SELECT * FROM cypher('multiple-label-2', $$ MATCH (x)-[:A|B|A]->(y) RETURN x.name, y.name $$) as (x agtype, y agtype);

-- different order (should output same as MATCH (x)-[:A|B]->(y))
SELECT * FROM cypher('multiple-label-2', $$ MATCH (x)-[:B|A]->(y) RETURN x.name, y.name $$) as (x agtype, y agtype);

-- label M does not exists (should output same as MATCH (x)-[:A|B]->(y))
SELECT * FROM cypher('multiple-label-2', $$ MATCH (x)-[:A|B|M]->(y) RETURN x.name, y.name $$) as (x agtype, y agtype);

-- no label exists (should output empty)
SELECT * FROM cypher('multiple-label-2', $$ MATCH (x)-[:M|N|O]->(y) RETURN x.name, y.name $$) as (x agtype, y agtype);

-- cleanup
SELECT drop_graph('multiple-label-2', true);

