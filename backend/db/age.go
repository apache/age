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
 
package db

const INIT_EXTENSION = `
CREATE EXTENSION IF NOT EXISTS age;
LOAD 'age';
`
const SET_SEARCH_PATH = `SET search_path = ag_catalog, "$user", public;`

const META_DATA_11 = `SELECT label, cnt, kind, namespace, namespace_id, graph, relation::text FROM (
	SELECT c.relname AS label, n.oid as namespace_id, c.reltuples AS cnt
	FROM pg_catalog.pg_class c
	JOIN pg_catalog.pg_namespace n
	ON n.oid = c.relnamespace
	WHERE c.relkind = 'r'
	
) as q1
JOIN ag_graph as g ON q1.namespace_id = g.namespace
INNER JOIN ag_label as label

ON label.name = q1.label
AND label.graph = g.oid;`

const META_DATA_12 = `SELECT label, cnt, kind, namespace, namespace_id, graph, relation::text FROM (
	SELECT c.relname AS label, n.oid as namespace_id, c.reltuples AS cnt
	FROM pg_catalog.pg_class c
	JOIN pg_catalog.pg_namespace n
	ON n.oid = c.relnamespace
	WHERE c.relkind = 'r'
	
) as q1
JOIN ag_graph as g ON q1.namespace_id = g.namespace
INNER JOIN ag_label as label

ON label.name = q1.label
AND label.graph = g.graphid;`

const GET_ALL_GRAPHS = `SELECT DISTINCT(split_part(relation::text, '.', 1)) as graph_name FROM ag_catalog.ag_label;`

const ANALYZE = `ANALYZE;`

const PG_VERSION = `show server_version_num;`
