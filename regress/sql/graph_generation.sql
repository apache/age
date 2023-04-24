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


-- PATH GRAPH TESTS

-- Should throw an error because number of vertices cannot be less than 2.
SELECT age_create_path('path_graph_1', 1, NULL, NULL, false);

-- Should create 5 vertices with 4 edges. All standard ag_labels.
SELECT age_create_path('path_graph_5_false', 5, NULL, NULL, false);
SELECT * FROM path_graph_5_false._ag_label_vertex;
SELECT * FROM path_graph_5_false._ag_label_edge;

-- Should create 5 vertices with 8 edges. All standard ag_labels.
SELECT age_create_path('path_graph_5_true', 5, NULL, NULL, true);
SELECT * FROM path_graph_5_true._ag_label_vertex;
SELECT * FROM path_graph_5_true._ag_label_edge;

-- Should create 10 vertices with 9 edges. New ag_labels.
SELECT age_create_path('path_graph_10_false', 10, 'PathVertex', 'PATH_EDGE', false);
SELECT * FROM path_graph_10_false."PathVertex";
SELECT * FROM path_graph_10_false."PATH_EDGE";

-- Should create 10 vertices with 18 edges. New ag_labels.
SELECT age_create_path('path_graph_10_true', 10, 'PathVertex', 'PATH_EDGE', true);
SELECT * FROM path_graph_10_true."PathVertex";
SELECT * FROM path_graph_10_true."PATH_EDGE";


-- DROPPING GRAPHS
SELECT drop_graph('gp1', true);
SELECT drop_graph('gp2', true);
SELECT drop_graph('path_graph_5_false', true);
SELECT drop_graph('path_graph_5_true', true);
SELECT drop_graph('path_graph_10_false', true);
SELECT drop_graph('path_graph_10_true', true);

