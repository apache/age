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


-- Tests for barbell graph generation
SELECT * FROM age_create_barbell_graph('gp1',5,0,'vertices',NULL,'edges',NULL);

SELECT COUNT(*) FROM gp1."edges";
SELECT COUNT(*) FROM gp1."vertices";

SELECT * FROM cypher('gp1', $$MATCH (a)-[e]->(b) RETURN e$$) as (n agtype);

SELECT * FROM age_create_barbell_graph('gp1',5,0,'vertices',NULL,'edges',NULL);

SELECT COUNT(*) FROM gp1."edges";
SELECT COUNT(*) FROM gp1."vertices";

SELECT * FROM age_create_barbell_graph('gp2',5,10,'vertices',NULL,'edges',NULL);

-- SHOULD FAIL
SELECT * FROM age_create_barbell_graph(NULL,NULL,NULL,NULL,NULL,NULL,NULL);
SELECT * FROM age_create_barbell_graph('gp2',NULL,0,'vertices',NULL,'edges',NULL); 
SELECT * FROM age_create_barbell_graph('gp3',5,NULL,'vertices',NULL,'edges',NULL);
SELECT * FROM age_create_barbell_graph('gp4',NULL,0,'vertices',NULL,'edges',NULL);
SELECT * FROM age_create_barbell_graph('gp5',5,0,'vertices',NULL,NULL,NULL);

-- Should error out because same labels are used for both vertices and edges
SELECT * FROM age_create_barbell_graph('gp6',5,10,'label',NULL,'label',NULL);

-- DROPPING GRAPHS
SELECT drop_graph('gp1', true);
SELECT drop_graph('gp2', true);


-- Tests for Erdos-Renyi graph generation.

-- Unidirectional.
SELECT * FROM age_create_erdos_renyi_graph('ErdosRenyi_1', 6, '0', NULL, NULL, false); -- No edges are created.
SELECT * FROM age_create_erdos_renyi_graph('ErdosRenyi_2', 6, '1', NULL, NULL, false); -- All edges are created.

SELECT count(*) FROM "ErdosRenyi_1"._ag_label_edge; -- No edges are created.
SELECT count(*) FROM "ErdosRenyi_2"._ag_label_edge; -- All edges are created (15).

-- Bidirectional.
SELECT * FROM age_create_erdos_renyi_graph('ErdosRenyi_3', 6, '1', NULL, NULL, true); -- All edges are created.

SELECT count(*) FROM "ErdosRenyi_3"._ag_label_edge; -- All edges are created (30).

-- Errors.
SELECT * FROM age_create_erdos_renyi_graph(NULL, NULL, NULL, NULL, NULL, NULL); -- Graph name cannot be NULL.
SELECT * FROM age_create_erdos_renyi_graph('ErdosRenyi_Error', NULL, NULL, NULL, NULL, NULL); -- Number of vertices cannot be NULL.
SELECT * FROM age_create_erdos_renyi_graph('ErdosRenyi_Error', 6, NULL, NULL, NULL, NULL); -- Probability cannot be NULL.

-- Drop Erdos-Renyi Graphs.
SELECT drop_graph('ErdosRenyi_1', true);
SELECT drop_graph('ErdosRenyi_2', true);
SELECT drop_graph('ErdosRenyi_3', true);
