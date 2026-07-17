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

-- This will only work within a major version of PostgreSQL, not across
-- major versions.

--* This is a TEMPLATE for upgrading from the previous version of Apache AGE
--* Please adjust the below ALTER EXTENSION to reflect the -- correct version it
-- is upgrading to.

-- This will only work within a major version of PostgreSQL, not across
-- major versions.

-- complain if script is sourced in psql, rather than via CREATE EXTENSION
\echo Use "ALTER EXTENSION age UPDATE TO '1.X.0'" to load this file. \quit

--* Please add all additions, deletions, and modifications to the end of this
--* file. We need to keep the order of these changes.
--* REMOVE ALL LINES ABOVE, and this one, that start with --*

--
-- Add delimiter parameter to load_labels_from_file and load_edges_from_file
--
-- Issue #2449: Both load_labels_from_file and load_edges_from_file now accept
-- an optional delimiter parameter (default ',') to support non-CSV delimiters
-- such as pipe-delimited files.

-- Drop and recreate load_labels_from_file with new delimiter parameter
DROP FUNCTION IF EXISTS ag_catalog.load_labels_from_file(name, name, text, bool, bool);

CREATE FUNCTION ag_catalog.load_labels_from_file(graph_name name,
                                                 label_name name,
                                                 file_path text,
                                                 id_field_exists bool default true,
                                                 load_as_agtype bool default false,
                                                 delimiter text default ',')
    RETURNS void
    LANGUAGE c
    AS 'MODULE_PATHNAME';

-- Drop and recreate load_edges_from_file with new delimiter parameter
DROP FUNCTION IF EXISTS ag_catalog.load_edges_from_file(name, name, text, bool);

CREATE FUNCTION ag_catalog.load_edges_from_file(graph_name name,
                                                label_name name,
                                                file_path text,
                                                load_as_agtype bool default false,
                                                delimiter text default ',')
    RETURNS void
    LANGUAGE c
    AS 'MODULE_PATHNAME';
