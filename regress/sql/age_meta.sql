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

SELECT create_graph('meta_test_graph');

SELECT list_graphs();

SELECT list_vertex_labels('meta_test_graph');
SELECT count_vertex_labels('meta_test_graph');
SELECT create_vlabel('meta_test_graph','vertex_a');
SELECT list_vertex_labels('meta_test_graph');
SELECT count_vertex_labels('meta_test_graph');

SELECT list_edge_labels('meta_test_graph');
SELECT count_edge_labels('meta_test_graph');
SELECT create_elabel('meta_test_graph','edge_a');
SELECT list_edge_labels('meta_test_graph');
SELECT count_edge_labels('meta_test_graph');

SELECT drop_graph('meta_test_graph', true);
