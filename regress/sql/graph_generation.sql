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

SET search_path = ag_catalog;

-- TESTS FOR COMPLETE GRAPH GENERATION

SELECT * FROM create_complete_graph('gp1',5,'edges','vertices');

SELECT COUNT(*) FROM gp1."edges";
SELECT COUNT(*) FROM gp1."vertices";

SELECT * FROM cypher('gp1', $$MATCH (a)-[e]->(b) RETURN e$$) as (n agtype);

SELECT * FROM create_complete_graph('gp1',5,'edges','vertices');

SELECT COUNT(*) FROM gp1."edges";
SELECT COUNT(*) FROM gp1."vertices";

SELECT * FROM create_complete_graph('gp2',5,'edges');

-- SHOULD FAIL
SELECT * FROM create_complete_graph('gp3',5, NULL);

SELECT * FROM create_complete_graph('gp4',NULL,NULL);

SELECT * FROM create_complete_graph(NULL,NULL,NULL);

-- Should error out because same labels are used for both vertices and edges
SELECT * FROM create_complete_graph('gp5',5,'label','label');

-- DROPPING GRAPHS
SELECT drop_graph('gp1', true);
SELECT drop_graph('gp2', true);


-- TESTS FOR BARBELL GRAPH GENERATION

SELECT * FROM age_create_barbell_graph('gp1',6,0,'vertices1',NULL,'edges1',NULL);
SELECT COUNT(*) FROM gp1."vertices1";
SELECT COUNT(*) FROM gp1."edges1";

SELECT * FROM age_create_barbell_graph('gp1',5,3,'vertices2','{}','edges2','{}');
SELECT COUNT(*) FROM gp1."vertices2";
SELECT COUNT(*) FROM gp1."edges2";

SELECT * FROM age_create_barbell_graph('gp1',4,1,'vertices3',NULL,'edges3',NULL);
SELECT COUNT(*) FROM gp1."vertices3";
SELECT COUNT(*) FROM gp1."edges3";

SELECT * FROM age_create_barbell_graph('gp1',3,2,'vertices4',NULL,'edges4',NULL);
SELECT COUNT(*) FROM gp1."vertices4";
SELECT COUNT(*) FROM gp1."edges4";

-- Counting total vertexes and edges
SELECT * FROM cypher('gp1', $$MATCH (v) RETURN COUNT(v)$$) as (n agtype);
SELECT * FROM cypher('gp1', $$MATCH (a)-[e]->(b) RETURN COUNT(e)$$) as (n agtype);

-- List all connections
SELECT * FROM cypher('gp1', $$MATCH (a)-[e]->(b) RETURN e$$) as (n agtype);

-- creating another graph
SELECT * FROM age_create_barbell_graph('gp2',10,10,'vertices',NULL,'edges',NULL);

-- testing vertex NULL labels
SELECT * FROM age_create_barbell_graph('gp3',6,6,NULL,NULL,'edges');

-- SHOULD FAIL
-- invalid graph name
SELECT * FROM age_create_barbell_graph(NULL,NULL,NULL,NULL,NULL,NULL,NULL);
SELECT * FROM age_create_barbell_graph(NULL,NULL,NULL);

-- invalid number of vertexes in complete graphs
SELECT * FROM age_create_barbell_graph('gp2',NULL,0,'vertices',NULL,'edges',NULL); 
SELECT * FROM age_create_barbell_graph('gp2',0,0,'vertices',NULL,'edges',NULL);
SELECT * FROM age_create_barbell_graph('gp2',1,1,'vertices',NULL,'edges',NULL);
SELECT * FROM age_create_barbell_graph('gp2',2,2,'vertices',NULL,'edges',NULL);
SELECT * FROM age_create_barbell_graph('gp2',-1,0,'vertices',NULL,'edges',NULL);

-- invalid number of vertexes in bridge
SELECT * FROM age_create_barbell_graph('gp2',3,-1,'vertices',NULL,'edges',NULL);
SELECT * FROM age_create_barbell_graph('gp3',5,NULL,'vertices',NULL,'edges',NULL);

-- invalid edge label
SELECT * FROM age_create_barbell_graph('gp5',5,3,'vertices',NULL,NULL,NULL);
SELECT * FROM age_create_barbell_graph('gp5',5,4,'vertices');

-- Should error out because same labels are used for both vertices and edges
SELECT * FROM age_create_barbell_graph('gp6',5,10,'label',NULL,'label',NULL);

-- DROPPING GRAPHS
SELECT drop_graph('gp1', true);
SELECT drop_graph('gp2', true);
SELECT drop_graph('gp3', true);

