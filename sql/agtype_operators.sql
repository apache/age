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
-- Contains operators @> <@
--
CREATE FUNCTION ag_catalog.agtype_contains(agtype, agtype)
    RETURNS boolean
    LANGUAGE c
    IMMUTABLE
RETURNS NULL ON NULL INPUT
PARALLEL SAFE
AS 'MODULE_PATHNAME';

CREATE OPERATOR @> (
  LEFTARG = agtype,
  RIGHTARG = agtype,
  FUNCTION = ag_catalog.agtype_contains,
  COMMUTATOR = '<@',
  RESTRICT = matchingsel,
  JOIN = matchingjoinsel
);

CREATE FUNCTION ag_catalog.agtype_contained_by(agtype, agtype)
    RETURNS boolean
    LANGUAGE c
    IMMUTABLE
RETURNS NULL ON NULL INPUT
PARALLEL SAFE
AS 'MODULE_PATHNAME';

CREATE OPERATOR <@ (
  LEFTARG = agtype,
  RIGHTARG = agtype,
  FUNCTION = ag_catalog.agtype_contained_by,
  COMMUTATOR = '@>',
  RESTRICT = matchingsel,
  JOIN = matchingjoinsel
);

CREATE FUNCTION ag_catalog.agtype_contains_top_level(agtype, agtype)
    RETURNS boolean
    LANGUAGE c
    IMMUTABLE
RETURNS NULL ON NULL INPUT
PARALLEL SAFE
AS 'MODULE_PATHNAME';

CREATE OPERATOR @>> (
  LEFTARG = agtype,
  RIGHTARG = agtype,
  FUNCTION = ag_catalog.agtype_contains_top_level,
  COMMUTATOR = '<<@',
  RESTRICT = matchingsel,
  JOIN = matchingjoinsel
);

CREATE FUNCTION ag_catalog.agtype_contained_by_top_level(agtype, agtype)
    RETURNS boolean
    LANGUAGE c
    IMMUTABLE
RETURNS NULL ON NULL INPUT
PARALLEL SAFE
AS 'MODULE_PATHNAME';

CREATE OPERATOR <<@ (
  LEFTARG = agtype,
  RIGHTARG = agtype,
  FUNCTION = ag_catalog.agtype_contained_by_top_level,
  COMMUTATOR = '@>>',
  RESTRICT = matchingsel,
  JOIN = matchingjoinsel
);