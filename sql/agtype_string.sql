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

--
-- agtype - string matching (`STARTS WITH`, `ENDS WITH`, `CONTAINS`, & =~)
--

CREATE FUNCTION ag_catalog.agtype_string_match_starts_with(agtype, agtype)
    RETURNS agtype
    LANGUAGE c
    STABLE
RETURNS NULL ON NULL INPUT
PARALLEL SAFE
AS 'MODULE_PATHNAME';

CREATE FUNCTION ag_catalog.agtype_string_match_ends_with(agtype, agtype)
    RETURNS agtype
    LANGUAGE c
    STABLE
RETURNS NULL ON NULL INPUT
PARALLEL SAFE
AS 'MODULE_PATHNAME';

CREATE FUNCTION ag_catalog.agtype_string_match_contains(agtype, agtype)
    RETURNS agtype
    LANGUAGE c
    IMMUTABLE
RETURNS NULL ON NULL INPUT
PARALLEL SAFE
AS 'MODULE_PATHNAME';

CREATE FUNCTION ag_catalog.age_eq_tilde(agtype, agtype)
    RETURNS agtype
    LANGUAGE c
    STABLE
PARALLEL SAFE
AS 'MODULE_PATHNAME';

CREATE OPERATOR =~ (
  LEFTARG = agtype,
  RIGHTARG = agtype,
  FUNCTION = ag_catalog.age_eq_tilde
);

CREATE FUNCTION ag_catalog.age_is_valid_label_name(agtype)
    RETURNS boolean
    LANGUAGE c
    IMMUTABLE
PARALLEL SAFE
AS 'MODULE_PATHNAME';
