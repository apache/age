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

--
-- pg_upgrade helper functions resolve built-ins from pg_catalog first.
--
-- Each helper must place pg_catalog ahead of ag_catalog in its search_path, so
-- that built-in functions and operators always resolve to pg_catalog and are
-- not overridden by same-named objects defined in ag_catalog.
--
SELECT p.proname,
       array_to_string(p.proconfig, ', ') AS proconfig
FROM pg_proc p
JOIN pg_namespace n ON n.oid = p.pronamespace
WHERE n.nspname = 'ag_catalog'
  AND p.proname IN ('age_prepare_pg_upgrade', 'age_finish_pg_upgrade',
                    'age_revert_pg_upgrade_changes', 'age_pg_upgrade_status')
ORDER BY p.proname;

--
-- The helper bodies must not contain unqualified format()/hashtext() calls;
-- those built-ins are explicitly schema-qualified to pg_catalog.
--
SELECT p.proname,
       (p.prosrc ~ '[^.]\mformat\s*\(')   AS has_unqualified_format,
       (p.prosrc ~ '[^.]\mhashtext\s*\(') AS has_unqualified_hashtext
FROM pg_proc p
JOIN pg_namespace n ON n.oid = p.pronamespace
WHERE n.nspname = 'ag_catalog'
  AND p.proname IN ('age_finish_pg_upgrade', 'age_revert_pg_upgrade_changes')
ORDER BY p.proname;

--
-- Install-time ownership check: CREATE EXTENSION age installs into ag_catalog
-- only when that schema does not already exist under a different owner. The
-- check compares schema ownership against the installing role. Verify the
-- underlying detection both ways with a probe schema, without disturbing the
-- already-installed extension.
--
CREATE ROLE age_probe_role NOLOGIN;
CREATE SCHEMA age_probe AUTHORIZATION age_probe_role;

-- A schema owned by a different role is detected as foreign-owned.
SELECT EXISTS (
    SELECT 1
    FROM pg_catalog.pg_namespace n
    WHERE n.nspname = 'age_probe'
      AND n.nspowner <> (SELECT r.oid FROM pg_catalog.pg_roles r
                         WHERE r.rolname = current_user)
) AS foreign_owner_detected;

-- ag_catalog, owned by the current (installing) role here, is not flagged
-- (the check does not false-positive on a normal install).
SELECT EXISTS (
    SELECT 1
    FROM pg_catalog.pg_namespace n
    WHERE n.nspname = 'ag_catalog'
      AND n.nspowner <> (SELECT r.oid FROM pg_catalog.pg_roles r
                         WHERE r.rolname = current_user)
) AS installer_owned_flagged;

DROP SCHEMA age_probe;
DROP ROLE age_probe_role;
