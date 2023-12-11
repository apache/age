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

DROP FUNCTION IF EXISTS ag_catalog.list_graphs;

CREATE OR REPLACE FUNCTION ag_catalog.list_graphs()
       RETURNS TABLE (graph_name name) 
LANGUAGE plpgsql
IMMUTABLE
RETURNS NULL ON NULL INPUT
AS $$
BEGIN
RETURN QUERY EXECUTE 'SELECT name FROM ag_catalog.ag_graph';
END $$;

DROP FUNCTION IF EXISTS ag_catalog.list_vertex_labels;

CREATE OR REPLACE FUNCTION ag_catalog.list_vertex_labels(graph_name name)
RETURNS TABLE (label_name name) 
LANGUAGE plpgsql
IMMUTABLE
RETURNS NULL ON NULL INPUT
AS $$
DECLARE
stmt TEXT;
BEGIN
stmt = FORMAT('SELECT ag_catalog.ag_label.name FROM ag_catalog.ag_label ' ||
     'INNER JOIN ag_catalog.ag_graph ' ||
     'ON ag_catalog.ag_label.graph = ag_catalog.ag_graph.graphid ' ||
     'WHERE kind = ''v'' AND ag_catalog.ag_graph.name = %L', graph_name);
RETURN QUERY EXECUTE stmt;
     
END $$;

DROP FUNCTION IF EXISTS ag_catalog.count_vertex_labels;

CREATE OR REPLACE FUNCTION ag_catalog.count_vertex_labels(graph_name name)
RETURNS TABLE (total_vertex_labels bigint) 
LANGUAGE plpgsql
IMMUTABLE
RETURNS NULL ON NULL INPUT
AS $$
DECLARE
stmt TEXT;
BEGIN
stmt = FORMAT('SELECT COUNT(ag_catalog.ag_label.name) AS total_vertex_labels ' ||
     'FROM ag_catalog.ag_label INNER JOIN ag_catalog.ag_graph ' ||
     'ON ag_catalog.ag_label.graph = ag_catalog.ag_graph.graphid ' ||
     'WHERE kind = ''v'' AND ag_catalog.ag_graph.name = %L', graph_name);
RETURN QUERY EXECUTE stmt;

END $$;


DROP FUNCTION IF EXISTS ag_catalog.list_edge_labels;

CREATE OR REPLACE FUNCTION ag_catalog.list_edge_labels(graph_name name)
RETURNS TABLE (label_name name) 
LANGUAGE plpgsql
IMMUTABLE
RETURNS NULL ON NULL INPUT
AS $$
DECLARE
stmt TEXT;
BEGIN

stmt = FORMAT('SELECT ag_catalog.ag_label.name FROM ag_catalog.ag_label ' ||
     'INNER JOIN ag_catalog.ag_graph ' ||
     'ON ag_catalog.ag_label.graph = ag_catalog.ag_graph.graphid ' ||
     'WHERE kind = ''e'' AND ag_catalog.ag_graph.name = %L', graph_name);
RETURN QUERY EXECUTE stmt;

END $$;

DROP FUNCTION IF EXISTS ag_catalog.count_edge_labels;

CREATE OR REPLACE FUNCTION ag_catalog.count_edge_labels(graph_name name)
RETURNS TABLE (total_edges_labels bigint) 
LANGUAGE plpgsql
IMMUTABLE
RETURNS NULL ON NULL INPUT
AS $$
DECLARE
stmt TEXT;
BEGIN

stmt = FORMAT('SELECT COUNT(ag_catalog.ag_label.name) AS total_edges_labels ' ||
     'FROM ag_catalog.ag_label INNER JOIN ag_catalog.ag_graph ' ||
     'ON ag_catalog.ag_label.graph = ag_catalog.ag_graph.graphid ' ||
     'WHERE kind = ''e'' AND ag_catalog.ag_graph.name = %L', graph_name);
RETURN QUERY EXECUTE stmt;

END $$;
 
CREATE FUNCTION mul(numeric, numeric) RETURNS numeric
AS 'select $1*$2;'
LANGUAGE SQL
IMMUTABLE
RETURNS NULL ON NULL INPUT;
