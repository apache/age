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

\! rm -rf /tmp/age/age_load
\! mkdir -p /tmp/age
\! cp -r regress/age_load/data /tmp/age/age_load

LOAD 'age';

SET search_path TO ag_catalog;

-- Create a country using CREATE clause
SELECT create_graph('agload_test_graph');

SELECT * FROM cypher('agload_test_graph', $$CREATE (n:Country {__id__:1}) RETURN n$$) as (n agtype);

--
-- Load countries with id
--
SELECT load_labels_from_file('agload_test_graph', 'Country',
    'age_load/countries.csv', true);

-- Sequence should be equal to max entry id i.e. 248
SELECT currval('agload_test_graph."Country_id_seq"')=248;

-- Should error out on loading the same file again due to duplicate id
SELECT load_labels_from_file('agload_test_graph', 'Country',
    'age_load/countries.csv', true);

--
-- Load cities with id
--

-- Should create City label automatically and load cities
SELECT load_labels_from_file('agload_test_graph', 'City',
    'age_load/cities.csv', true);

-- Sequence should be equal to max entry id i.e. 146941
SELECT currval('agload_test_graph."City_id_seq"')=146941;

-- Should error out on loading the same file again due to duplicate id
SELECT load_labels_from_file('agload_test_graph', 'City',
    'age_load/cities.csv', true);

--
-- Load edges -- Connects cities to countries
--

-- Should error out for using vertex label
SELECT load_edges_from_file('agload_test_graph', 'Country',
    'age_load/edges.csv');

SELECT create_elabel('agload_test_graph','has_city');
SELECT load_edges_from_file('agload_test_graph', 'has_city',
     'age_load/edges.csv');

-- Sequence should be equal to number of edges loaded i.e. 72485
SELECT currval('agload_test_graph."has_city_id_seq"')=72485;

-- Should error out for using edge label
SELECT load_labels_from_file('agload_test_graph', 'has_city',
     'age_load/cities.csv');

SELECT table_catalog, table_schema, lower(table_name) as table_name, table_type
FROM information_schema.tables
WHERE table_schema = 'agload_test_graph' ORDER BY table_name ASC;

SELECT COUNT(*) FROM agload_test_graph."Country";
SELECT COUNT(*) FROM agload_test_graph."City";
SELECT COUNT(*) FROM agload_test_graph."has_city";

SELECT COUNT(*) FROM cypher('agload_test_graph', $$MATCH(n) RETURN n$$) as (n agtype);

SELECT COUNT(*) FROM cypher('agload_test_graph', $$MATCH (a)-[e]->(b) RETURN e$$) as (n agtype);

--
-- Load countries and cities without id
--

-- Should load countries in Country label without error since it should use sequence now
SELECT load_labels_from_file('agload_test_graph', 'Country',
    'age_load/countries.csv', false);

SELECT create_vlabel('agload_test_graph','Country2');
SELECT load_labels_from_file('agload_test_graph', 'Country2',
                             'age_load/countries.csv', false);

SELECT create_vlabel('agload_test_graph','City2');
SELECT load_labels_from_file('agload_test_graph', 'City2',
                             'age_load/cities.csv', false);

SELECT COUNT(*) FROM agload_test_graph."Country2";
SELECT COUNT(*) FROM agload_test_graph."City2";

SELECT id FROM agload_test_graph."Country" LIMIT 10;
SELECT id FROM agload_test_graph."Country2" LIMIT 10;

-- Should return 2 rows for Country with same properties, but different ids
SELECT * FROM cypher('agload_test_graph', $$MATCH(n:Country {iso2 : 'BE'})
    RETURN id(n), n.name, n.iso2 $$) as ("id(n)" agtype, "n.name" agtype, "n.iso2" agtype);
-- Should return 1 row
SELECT * FROM cypher('agload_test_graph', $$MATCH(n:Country2 {iso2 : 'BE'})
    RETURN id(n), n.name, n.iso2 $$) as ("id(n)" agtype, "n.name" agtype, "n.iso2" agtype);

-- Should return 2 rows for Country with same properties, but different ids
SELECT * FROM cypher('agload_test_graph', $$MATCH(n:Country {iso2 : 'AT'})
    RETURN id(n), n.name, n.iso2 $$) as ("id(n)" agtype, "n.name" agtype, "n.iso2" agtype);
-- Should return 1 row
SELECT * FROM cypher('agload_test_graph', $$MATCH(n:Country2 {iso2 : 'AT'})
    RETURN id(n), n.name, n.iso2 $$) as ("id(n)" agtype, "n.name" agtype, "n.iso2" agtype);

-- Should return 2 rows for Country with same properties, but different ids
SELECT * FROM cypher('agload_test_graph', $$
    MATCH (u:Country {region : "Europe"})
    WHERE u.name =~ 'Cro.*'
    RETURN id(u), u.name, u.region
$$) AS ("id(u)" agtype, result_1 agtype, result_2 agtype);

-- There shouldn't be any duplicates
SELECT * FROM cypher('agload_test_graph', $$return graph_stats('agload_test_graph')$$) as (a agtype);

SELECT drop_graph('agload_test_graph', true);

--
-- Test property type conversion
--

-- vertex: load as agtype

-- Should create graph and label automatically
SELECT load_labels_from_file('agload_conversion', 'Person1', 'age_load/conversion_vertices.csv', true, true);
SELECT * FROM cypher('agload_conversion', $$ MATCH (n:Person1) RETURN properties(n) $$) as (a agtype);

-- vertex: load as string
SELECT create_vlabel('agload_conversion','Person2');
SELECT load_labels_from_file('agload_conversion', 'Person2', 'age_load/conversion_vertices.csv', true, false);
SELECT * FROM cypher('agload_conversion', $$ MATCH (n:Person2) RETURN properties(n) $$) as (a agtype);

-- edge: load as agtype
SELECT create_elabel('agload_conversion','Edges1');
SELECT load_edges_from_file('agload_conversion', 'Edges1', 'age_load/conversion_edges.csv', true);
SELECT * FROM cypher('agload_conversion', $$ MATCH ()-[e:Edges1]->() RETURN properties(e) $$) as (a agtype);

-- edge: load as string
SELECT create_elabel('agload_conversion','Edges2');
SELECT load_edges_from_file('agload_conversion', 'Edges2', 'age_load/conversion_edges.csv', false);
SELECT * FROM cypher('agload_conversion', $$ MATCH ()-[e:Edges2]->() RETURN properties(e) $$) as (a agtype);

--
-- Check sandbox
--
-- check null file name
SELECT load_labels_from_file('agload_conversion', 'Person1', NULL, true, true);
SELECT load_edges_from_file('agload_conversion', 'Edges1', NULL, true);

-- check no file name
SELECT load_labels_from_file('agload_conversion', 'Person1', '', true, true);
SELECT load_edges_from_file('agload_conversion', 'Edges1', '', true);

-- check for file/path does not exist
SELECT load_labels_from_file('agload_conversion', 'Person1', 'age_load_xxx/conversion_vertices.csv', true, true);
SELECT load_edges_from_file('agload_conversion', 'Edges1', 'age_load_xxx/conversion_edges.csv', true);
SELECT load_labels_from_file('agload_conversion', 'Person1', 'age_load/conversion_vertices.txt', true, true);
SELECT load_edges_from_file('agload_conversion', 'Edges1', 'age_load/conversion_edges.txt', true);

-- check wrong extension
\! touch /tmp/age/age_load/conversion_vertices.txt
\! touch /tmp/age/age_load/conversion_edges.txt
SELECT load_labels_from_file('agload_conversion', 'Person1', 'age_load/conversion_vertices.txt', true, true);
SELECT load_edges_from_file('agload_conversion', 'Edges1', 'age_load/conversion_edges.txt', true);

-- check outside sandbox directory
SELECT load_labels_from_file('agload_conversion', 'Person1', '../../etc/passwd', true, true);
SELECT load_edges_from_file('agload_conversion', 'Edges1', '../../etc/passwd', true);

--
-- Cleanup
--
SELECT drop_graph('agload_conversion', true);

--
-- Test security and permissions
--

SELECT create_graph('agload_security');
SELECT create_vlabel('agload_security', 'Person1');
SELECT create_vlabel('agload_security', 'Person2');
SELECT create_elabel('agload_security', 'SecEdge');

--
-- Test 1: File read permission (pg_read_server_files role)
--
-- Create a user without pg_read_server_files role
CREATE USER load_test_user;
GRANT USAGE ON SCHEMA ag_catalog TO load_test_user;

-- This should fail because load_test_user doesn't have pg_read_server_files
SET ROLE load_test_user;
SELECT load_labels_from_file('agload_security', 'Person1', 'age_load/conversion_vertices.csv', true);
SELECT load_edges_from_file('agload_security', 'SecEdge', 'age_load/conversion_edges.csv');
RESET ROLE;

-- Grant pg_read_server_files and try again - should fail on table permission now
GRANT pg_read_server_files TO load_test_user;

--
-- Test 2: Table INSERT permission (ACL_INSERT)
--
-- User has file read permission but no INSERT on the label table
SET ROLE load_test_user;
SELECT load_labels_from_file('agload_security', 'Person1', 'age_load/conversion_vertices.csv', true);
SELECT load_edges_from_file('agload_security', 'SecEdge', 'age_load/conversion_edges.csv');
RESET ROLE;

-- Grant INSERT permission and try again - should succeed
GRANT USAGE ON SCHEMA agload_security TO load_test_user;
GRANT INSERT ON agload_security."Person1" TO load_test_user;
GRANT INSERT ON agload_security."SecEdge" TO load_test_user;
GRANT UPDATE ON SEQUENCE agload_security."Person1_id_seq" TO load_test_user;
GRANT UPDATE ON SEQUENCE agload_security."SecEdge_id_seq" TO load_test_user;
GRANT SELECT ON ag_catalog.ag_label TO load_test_user;
GRANT SELECT ON ag_catalog.ag_graph TO load_test_user;

SET ROLE load_test_user;
SELECT load_labels_from_file('agload_security', 'Person1', 'age_load/conversion_vertices.csv', true);
SELECT load_edges_from_file('agload_security', 'SecEdge', 'age_load/conversion_edges.csv');
RESET ROLE;

-- Verify data was loaded
SELECT COUNT(*) FROM agload_security."Person1";
SELECT COUNT(*) FROM agload_security."SecEdge";

-- cleanup
DELETE FROM agload_security."Person1";
DELETE FROM agload_security."SecEdge";

--
-- Test 3: Row-Level Security (RLS)
--

-- Enable RLS on the label tables
ALTER TABLE agload_security."Person1" ENABLE ROW LEVEL SECURITY;
ALTER TABLE agload_security."SecEdge" ENABLE ROW LEVEL SECURITY;

-- Switch to load_test_user
SET ROLE load_test_user;

-- Loading should fail when RLS is enabled
SELECT load_labels_from_file('agload_security', 'Person1', 'age_load/conversion_vertices.csv', true);
SELECT load_edges_from_file('agload_security', 'SecEdge', 'age_load/conversion_edges.csv');

RESET ROLE;

-- Disable RLS and try again - should succeed
ALTER TABLE agload_security."Person1" DISABLE ROW LEVEL SECURITY;
ALTER TABLE agload_security."SecEdge" DISABLE ROW LEVEL SECURITY;

SELECT load_labels_from_file('agload_security', 'Person1', 'age_load/conversion_vertices.csv', true);
SELECT load_edges_from_file('agload_security', 'SecEdge', 'age_load/conversion_edges.csv');

-- Verify data was loaded
SELECT COUNT(*) FROM agload_security."Person1";
SELECT COUNT(*) FROM agload_security."SecEdge";

-- cleanup
DELETE FROM agload_security."Person1";
DELETE FROM agload_security."SecEdge";

--
-- Test 4: Constraint checking (CHECK constraint)
--

-- Add constraint on vertex properties - fail if bool property is false
ALTER TABLE agload_security."Person1" ADD CONSTRAINT check_bool_true
    CHECK ((properties->>'"bool"')::boolean = true);

-- This should fail - constraint violation
SELECT load_labels_from_file('agload_security', 'Person1', 'age_load/conversion_vertices.csv', true);

-- Add constraint on edge properties - fail if bool property is false
ALTER TABLE agload_security."SecEdge" ADD CONSTRAINT check_bool_true
    CHECK ((properties->>'"bool"')::boolean = true);

-- This should fail - some edges have bool = false
SELECT load_edges_from_file('agload_security', 'SecEdge', 'age_load/conversion_edges.csv');

-- cleanup
ALTER TABLE agload_security."Person1" DROP CONSTRAINT check_bool_true;
ALTER TABLE agload_security."SecEdge" DROP CONSTRAINT check_bool_true;

--
-- Cleanup
--
REVOKE ALL ON agload_security."Person1" FROM load_test_user;
REVOKE ALL ON agload_security."SecEdge" FROM load_test_user;
REVOKE ALL ON SEQUENCE agload_security."Person1_id_seq" FROM load_test_user;
REVOKE ALL ON SEQUENCE agload_security."SecEdge_id_seq" FROM load_test_user;
REVOKE ALL ON ag_catalog.ag_label FROM load_test_user;
REVOKE ALL ON ag_catalog.ag_graph FROM load_test_user;
REVOKE ALL ON SCHEMA agload_security FROM load_test_user;
REVOKE ALL ON SCHEMA ag_catalog FROM load_test_user;
REVOKE pg_read_server_files FROM load_test_user;
DROP USER load_test_user;
SELECT drop_graph('agload_security', true);

--
-- End
--
