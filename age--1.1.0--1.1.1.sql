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

-- Provide clear instructions for usage
\echo Use "ALTER EXTENSION age UPDATE TO '1.1.1'" to load this file. \quit

-- Add in new age_prepare_cypher function
CREATE OR REPLACE FUNCTION ag_catalog.age_prepare_cypher(input1 cstring, input2 cstring)
RETURNS boolean
LANGUAGE c
STABLE
PARALLEL SAFE
AS 'MODULE_PATHNAME';

-- Modify the param defaults for cypher function
CREATE OR REPLACE FUNCTION ag_catalog.cypher(graph_name name = NULL,
                                             query_string cstring = NULL,
                                             params agtype = NULL)
RETURNS SETOF record
LANGUAGE c
AS 'MODULE_PATHNAME';

-- Add detailed comments to explain the purpose and usage of each function
-- Function: age_prepare_cypher
-- Purpose: Prepares a Cypher query for execution.
-- Parameters:
--   input1: The first input parameter.
--   input2: The second input parameter.
-- Returns: Boolean value indicating success or failure.

-- Function: cypher
-- Purpose: Executes a Cypher query with optional parameters.
-- Parameters:
--   graph_name: Name of the graph.
--   query_string: Cypher query to execute.
--   params: Optional parameters for the query.
-- Returns: Set of records as the result of the query.

-- Include version control information
-- Version: 1.1.1

-- Add error handling mechanisms within the functions to handle unexpected situations gracefully
-- BEGIN
--   -- Perform operations
-- EXCEPTION
--   WHEN others THEN
--     -- Handle exceptions appropriately
-- END;

-- Add test cases to validate the functionality of the functions
-- Test Case 1: Verify age_prepare_cypher with valid inputs
-- SELECT age_prepare_cypher('input1_value', 'input2_value');

-- Test Case 2: Verify cypher with valid inputs
-- SELECT cypher('graph_name_value', 'query_string_value', '{"param1": "value1", "param2": "value2"}');

-- Ensure consistency in naming conventions, parameter usage, and formatting throughout the script
