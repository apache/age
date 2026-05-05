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
-- Extension upgrade template regression test
--
-- Validates the upgrade template (age--<VER>--y.y.y.sql) by comparing
-- the pg_catalog entries of a fresh install against an upgraded install.
-- If any object exists in the fresh install but is missing after upgrade,
-- the template is incomplete. This catches the case where a developer adds
-- a new SQL object to sql/ and sql_files but forgets to add it to the
-- upgrade template.
--
-- Compared catalogs:
--   pg_proc     — functions, aggregates, procedures (name, args, properties
--                 including probin/prosrc to catch C-symbol renames and
--                 SQL-body changes)
--   pg_class    — tables, views, sequences, indexes (name, kind)
--   pg_type     — types (name, type category)
--   pg_operator — operators (name, left/right types)
--   pg_cast     — casts involving AGE types (source, target, context)
--   pg_opclass  — operator classes (name, access method)
--   pg_constraint — constraints (name, type, table, referenced table)
--   pg_depend   — extension membership (every AGE-owned object must be
--                 linked back to the extension via deptype='e'; catches
--                 a missing ALTER EXTENSION age ADD ... in the template)
--
-- All comparison queries should return 0 rows.
--
-- Note on synthetic-initial install (step 10): the synthetic '*_initial'
-- snapshot is built from a fixed historical commit, so its CREATE FUNCTION
-- statements may reference C symbols that have since been removed from the
-- current age.so. Step 10 disables check_function_bodies so that dlsym is
-- deferred to call time; the immediately-following ALTER EXTENSION UPDATE
-- (step 11) DROPs any such retired functions before any plan can call them.
-- This lets developers cleanly remove deprecated C entry points without
-- needing to keep error-raising stubs in age.so.
--

LOAD 'age';
SET search_path TO ag_catalog;

-- Step 1: Clean up any graphs left by prior tests (deterministic, no output).
DO $$
DECLARE
    graph_name ag_graph.name%TYPE;
BEGIN
    PERFORM set_config('client_min_messages', 'warning', true);

    FOR graph_name IN
        SELECT name
        FROM ag_graph
        ORDER BY name
    LOOP
        PERFORM drop_graph(graph_name, true);
    END LOOP;
END
$$;

-- =====================================================================
-- FRESH INSTALL SNAPSHOTS (Steps 2-7b)
-- Capture the catalog state from the default CREATE EXTENSION install.
-- =====================================================================

-- Step 2: Snapshot functions (includes aggregates via prokind).
-- probin/prosrc capture the binding to the implementation:
--   * LANGUAGE c        : probin = '$libdir/age', prosrc = C symbol name
--                         (a renamed/retargeted symbol shows up here)
--   * LANGUAGE sql/plpgsql: probin = NULL, prosrc = function body text
--                         (a body change in the upgrade template shows up here)
--   * LANGUAGE internal : probin = NULL, prosrc = builtin name
CREATE TEMP TABLE _fresh_funcs AS
SELECT proname::text,
       pg_get_function_identity_arguments(oid) AS args,
       provolatile::text, proisstrict::text, prokind::text,
       prorettype::regtype::text AS rettype, proretset::text,
       COALESCE(probin, '')  AS probin,
       COALESCE(prosrc, '')  AS prosrc
FROM pg_proc
WHERE pronamespace = 'ag_catalog'::regnamespace
ORDER BY proname, args;

-- Step 3: Snapshot relations (tables, views, sequences, indexes).
CREATE TEMP TABLE _fresh_rels AS
SELECT relname::text, relkind::text
FROM pg_class
WHERE relnamespace = 'ag_catalog'::regnamespace
  AND relkind IN ('r', 'v', 'S', 'i')
ORDER BY relname;

-- Step 4: Snapshot types.
CREATE TEMP TABLE _fresh_types AS
SELECT typname::text, typtype::text
FROM pg_type
WHERE typnamespace = 'ag_catalog'::regnamespace
  AND typname NOT LIKE 'pg_toast%'
ORDER BY typname;

-- Step 5: Snapshot operators.
CREATE TEMP TABLE _fresh_ops AS
SELECT oprname::text,
       oprleft::regtype::text AS lefttype,
       oprright::regtype::text AS righttype
FROM pg_operator
WHERE oprnamespace = 'ag_catalog'::regnamespace
ORDER BY oprname, lefttype, righttype;

-- Step 6: Snapshot casts involving AGE types, and operator classes.
CREATE TEMP TABLE _fresh_casts AS
SELECT castsource::regtype::text AS source_type,
       casttarget::regtype::text AS target_type,
       castcontext::text
FROM pg_cast
WHERE castsource IN (SELECT oid FROM pg_type WHERE typnamespace = 'ag_catalog'::regnamespace)
   OR casttarget IN (SELECT oid FROM pg_type WHERE typnamespace = 'ag_catalog'::regnamespace)
ORDER BY source_type, target_type;

CREATE TEMP TABLE _fresh_opclass AS
SELECT opcname::text,
       (SELECT amname FROM pg_am WHERE oid = opcmethod)::text AS amname
FROM pg_opclass
WHERE opcnamespace = 'ag_catalog'::regnamespace
ORDER BY opcname;

-- Step 7: Snapshot constraints.
CREATE TEMP TABLE _fresh_constraints AS
SELECT conname::text, contype::text,
       conrelid::regclass::text AS table_name,
       confrelid::regclass::text AS ref_table
FROM pg_constraint
WHERE connamespace = 'ag_catalog'::regnamespace
ORDER BY conname;

-- Step 7b: Snapshot extension membership (pg_depend deptype='e').
-- Every object that CREATE EXTENSION owns has a row in pg_depend linking
-- it to the extension. The upgrade template must produce the same set:
-- if it CREATEs an object but forgets to ALTER EXTENSION ADD it, the
-- catalog row exists (so funcs_match/rels_match would pass) but the
-- pg_depend link is absent and pg_dump --extension would diverge.
CREATE TEMP TABLE _fresh_extmembers AS
SELECT
    CASE d.classid
        WHEN 'pg_proc'::regclass
            THEN 'function: '   || d.objid::regprocedure::text
        WHEN 'pg_type'::regclass
            THEN 'type: '       || d.objid::regtype::text
        WHEN 'pg_class'::regclass
            THEN 'relation: '   || d.objid::regclass::text
            || ' ('   || (SELECT relkind::text FROM pg_class WHERE oid = d.objid) || ')'
        WHEN 'pg_operator'::regclass
            THEN 'operator: '   || d.objid::regoperator::text
        WHEN 'pg_cast'::regclass
            THEN 'cast: '       || (SELECT castsource::regtype::text || ' -> ' || casttarget::regtype::text
                                    FROM pg_cast WHERE oid = d.objid)
        WHEN 'pg_opclass'::regclass
            THEN 'opclass: '    || (SELECT opcname || ' (' || (SELECT amname FROM pg_am WHERE oid = opcmethod) || ')'
                                    FROM pg_opclass WHERE oid = d.objid)
        WHEN 'pg_constraint'::regclass
            THEN 'constraint: ' || (SELECT conname || ' on ' || conrelid::regclass::text
                                    FROM pg_constraint WHERE oid = d.objid)
        WHEN 'pg_opfamily'::regclass
            THEN 'opfamily: '   || (SELECT opfname || ' (' || (SELECT amname FROM pg_am WHERE oid = opfmethod) || ')'
                                    FROM pg_opfamily WHERE oid = d.objid)
        WHEN 'pg_amop'::regclass
            THEN 'amop: '       || (SELECT (SELECT opfname FROM pg_opfamily WHERE oid = a.amopfamily)
                                       || ' [strategy ' || a.amopstrategy || '] '
                                       || a.amoplefttype::regtype::text  || ','
                                       || a.amoprighttype::regtype::text || ' op '
                                       || a.amopopr::regoperator::text
                                    FROM pg_amop a WHERE a.oid = d.objid)
        WHEN 'pg_amproc'::regclass
            THEN 'amproc: '     || (SELECT (SELECT opfname FROM pg_opfamily WHERE oid = p.amprocfamily)
                                       || ' [proc ' || p.amprocnum || '] '
                                       || p.amproclefttype::regtype::text  || ','
                                       || p.amprocrighttype::regtype::text || ' fn '
                                       || p.amproc::regprocedure::text
                                    FROM pg_amproc p WHERE p.oid = d.objid)
        ELSE 'unhandled[' || d.classid::regclass::text || ']'
    END AS member
FROM pg_depend d
WHERE d.deptype = 'e'
  AND d.refclassid = 'pg_extension'::regclass
  AND d.refobjid   = (SELECT oid FROM pg_extension WHERE extname = 'age')
ORDER BY 1;

-- Step 8: Drop AGE entirely.
DROP EXTENSION age;

-- Step 9: Verify we have an upgrade path available.
SELECT count(*) > 1 AS has_upgrade_path
FROM pg_available_extension_versions WHERE name = 'age';

-- Step 10: Install AGE at the synthetic initial version.
--
-- Disable check_function_bodies for this CREATE only. The synthetic
-- '*_initial' SQL is pulled from a fixed historical commit and may
-- declare C functions whose symbols have since been removed from the
-- current age.so. With check_function_bodies=on, PostgreSQL would dlsym
-- each such symbol at CREATE FUNCTION time and abort. Deferring the
-- symbol probe to call time is safe because step 11 (ALTER EXTENSION
-- UPDATE) immediately runs the upgrade template, which DROPs any
-- removed-in-HEAD functions before the test (or any user) can call them.
-- The fresh CREATE EXTENSION at step 35 keeps the GUC at its default,
-- so any inconsistency between HEAD's SQL and HEAD's age.so is still
-- caught at install time on the production code path.
SET check_function_bodies = off;
DO $$
DECLARE init_ver text;
BEGIN
    SELECT version INTO init_ver
    FROM pg_available_extension_versions
    WHERE name = 'age' AND version LIKE '%_initial'
    ORDER BY version DESC
    LIMIT 1;
    IF init_ver IS NULL THEN
        RAISE EXCEPTION 'No initial version available for upgrade test';
    END IF;
    EXECUTE format('CREATE EXTENSION age VERSION %L', init_ver);
END;
$$;
RESET check_function_bodies;

-- Step 11: Upgrade to the current (default) version via the stamped template.
DO $$
DECLARE curr_ver text;
BEGIN
    SELECT default_version INTO curr_ver
    FROM pg_available_extensions
    WHERE name = 'age';
    IF curr_ver IS NULL THEN
        RAISE EXCEPTION 'No default version found for upgrade test';
    END IF;
    EXECUTE format('ALTER EXTENSION age UPDATE TO %L', curr_ver);
END;
$$;

-- Step 12: Confirm the upgrade succeeded.
SELECT installed_version = default_version AS upgraded_to_current
FROM pg_available_extensions WHERE name = 'age';

-- =====================================================================
-- UPGRADED INSTALL SNAPSHOTS (Steps 13-18b)
-- Capture the catalog state after upgrade from initial to current.
-- =====================================================================

-- Step 13: Snapshot functions (probin/prosrc included; see step 2).
CREATE TEMP TABLE _upgraded_funcs AS
SELECT proname::text,
       pg_get_function_identity_arguments(oid) AS args,
       provolatile::text, proisstrict::text, prokind::text,
       prorettype::regtype::text AS rettype, proretset::text,
       COALESCE(probin, '')  AS probin,
       COALESCE(prosrc, '')  AS prosrc
FROM pg_proc
WHERE pronamespace = 'ag_catalog'::regnamespace
ORDER BY proname, args;

-- Step 14: Snapshot relations.
CREATE TEMP TABLE _upgraded_rels AS
SELECT relname::text, relkind::text
FROM pg_class
WHERE relnamespace = 'ag_catalog'::regnamespace
  AND relkind IN ('r', 'v', 'S', 'i')
ORDER BY relname;

-- Step 15: Snapshot types.
CREATE TEMP TABLE _upgraded_types AS
SELECT typname::text, typtype::text
FROM pg_type
WHERE typnamespace = 'ag_catalog'::regnamespace
  AND typname NOT LIKE 'pg_toast%'
ORDER BY typname;

-- Step 16: Snapshot operators.
CREATE TEMP TABLE _upgraded_ops AS
SELECT oprname::text,
       oprleft::regtype::text AS lefttype,
       oprright::regtype::text AS righttype
FROM pg_operator
WHERE oprnamespace = 'ag_catalog'::regnamespace
ORDER BY oprname, lefttype, righttype;

-- Step 17: Snapshot casts and operator classes.
CREATE TEMP TABLE _upgraded_casts AS
SELECT castsource::regtype::text AS source_type,
       casttarget::regtype::text AS target_type,
       castcontext::text
FROM pg_cast
WHERE castsource IN (SELECT oid FROM pg_type WHERE typnamespace = 'ag_catalog'::regnamespace)
   OR casttarget IN (SELECT oid FROM pg_type WHERE typnamespace = 'ag_catalog'::regnamespace)
ORDER BY source_type, target_type;

CREATE TEMP TABLE _upgraded_opclass AS
SELECT opcname::text,
       (SELECT amname FROM pg_am WHERE oid = opcmethod)::text AS amname
FROM pg_opclass
WHERE opcnamespace = 'ag_catalog'::regnamespace
ORDER BY opcname;


-- Step 18: Snapshot constraints.
CREATE TEMP TABLE _upgraded_constraints AS
SELECT conname::text, contype::text,
       conrelid::regclass::text AS table_name,
       confrelid::regclass::text AS ref_table
FROM pg_constraint
WHERE connamespace = 'ag_catalog'::regnamespace
ORDER BY conname;

-- Step 18b: Snapshot extension membership after upgrade (see step 7b).
CREATE TEMP TABLE _upgraded_extmembers AS
SELECT
    CASE d.classid
        WHEN 'pg_proc'::regclass
            THEN 'function: '   || d.objid::regprocedure::text
        WHEN 'pg_type'::regclass
            THEN 'type: '       || d.objid::regtype::text
        WHEN 'pg_class'::regclass
            THEN 'relation: '   || d.objid::regclass::text
            || ' ('   || (SELECT relkind::text FROM pg_class WHERE oid = d.objid) || ')'
        WHEN 'pg_operator'::regclass
            THEN 'operator: '   || d.objid::regoperator::text
        WHEN 'pg_cast'::regclass
            THEN 'cast: '       || (SELECT castsource::regtype::text || ' -> ' || casttarget::regtype::text
                                    FROM pg_cast WHERE oid = d.objid)
        WHEN 'pg_opclass'::regclass
            THEN 'opclass: '    || (SELECT opcname || ' (' || (SELECT amname FROM pg_am WHERE oid = opcmethod) || ')'
                                    FROM pg_opclass WHERE oid = d.objid)
        WHEN 'pg_constraint'::regclass
            THEN 'constraint: ' || (SELECT conname || ' on ' || conrelid::regclass::text
                                    FROM pg_constraint WHERE oid = d.objid)
        WHEN 'pg_opfamily'::regclass
            THEN 'opfamily: '   || (SELECT opfname || ' (' || (SELECT amname FROM pg_am WHERE oid = opfmethod) || ')'
                                    FROM pg_opfamily WHERE oid = d.objid)
        WHEN 'pg_amop'::regclass
            THEN 'amop: '       || (SELECT (SELECT opfname FROM pg_opfamily WHERE oid = a.amopfamily)
                                       || ' [strategy ' || a.amopstrategy || '] '
                                       || a.amoplefttype::regtype::text  || ','
                                       || a.amoprighttype::regtype::text || ' op '
                                       || a.amopopr::regoperator::text
                                    FROM pg_amop a WHERE a.oid = d.objid)
        WHEN 'pg_amproc'::regclass
            THEN 'amproc: '     || (SELECT (SELECT opfname FROM pg_opfamily WHERE oid = p.amprocfamily)
                                       || ' [proc ' || p.amprocnum || '] '
                                       || p.amproclefttype::regtype::text  || ','
                                       || p.amprocrighttype::regtype::text || ' fn '
                                       || p.amproc::regprocedure::text
                                    FROM pg_amproc p WHERE p.oid = d.objid)
        ELSE 'unhandled[' || d.classid::regclass::text || ']'
    END AS member
FROM pg_depend d
WHERE d.deptype = 'e'
  AND d.refclassid = 'pg_extension'::regclass
  AND d.refobjid   = (SELECT oid FROM pg_extension WHERE extname = 'age')
ORDER BY 1;

-- =====================================================================
-- COMPARISON: Missing or extra objects (Steps 19-33b)
-- Any rows returned indicate a template deficiency.
-- =====================================================================

-- Step 19: Functions MISSING after upgrade.
SELECT f.proname || '(' || f.args || ')' AS missing_function
FROM _fresh_funcs f
LEFT JOIN _upgraded_funcs u USING (proname, args)
WHERE u.proname IS NULL
ORDER BY 1;

-- Step 20: Functions EXTRA after upgrade.
SELECT u.proname || '(' || u.args || ')' AS extra_function
FROM _upgraded_funcs u
LEFT JOIN _fresh_funcs f USING (proname, args)
WHERE f.proname IS NULL
ORDER BY 1;

-- Step 21: Function PROPERTY changes
-- (kind, volatility, strictness, return type, return-set, binding,
--  body/symbol). The probin/prosrc check catches:
--   * a C function whose symbol was renamed in the upgrade template
--   * a SQL/plpgsql function whose body was changed in either path
--   * a language change between the fresh and upgrade installs.
SELECT f.proname || '(' || f.args || ')' AS function_name,
       CASE WHEN f.prokind     <> u.prokind     THEN 'prokind: '    || f.prokind     || '->' || u.prokind     END AS kind_change,
       CASE WHEN f.provolatile <> u.provolatile THEN 'volatile: '   || f.provolatile || '->' || u.provolatile END AS volatility_change,
       CASE WHEN f.proisstrict <> u.proisstrict THEN 'strict: '     || f.proisstrict || '->' || u.proisstrict END AS strict_change,
       CASE WHEN f.rettype     <> u.rettype     THEN 'rettype: '    || f.rettype     || '->' || u.rettype     END AS rettype_change,
       CASE WHEN f.proretset   <> u.proretset   THEN 'retset: '     || f.proretset   || '->' || u.proretset   END AS retset_change,
       CASE WHEN f.probin      <> u.probin      THEN 'probin: '     || f.probin      || '->' || u.probin      END AS probin_change,
       CASE WHEN f.prosrc      <> u.prosrc      THEN 'prosrc changed' END                                       AS prosrc_change
FROM _fresh_funcs f
JOIN _upgraded_funcs u USING (proname, args)
WHERE f.provolatile <> u.provolatile
   OR f.proisstrict <> u.proisstrict
   OR f.prokind     <> u.prokind
   OR f.rettype     <> u.rettype
   OR f.proretset   <> u.proretset
   OR f.probin      <> u.probin
   OR f.prosrc      <> u.prosrc
ORDER BY 1;

-- Step 22: Relations MISSING after upgrade.
SELECT f.relname || ' (' || f.relkind || ')' AS missing_relation
FROM _fresh_rels f
LEFT JOIN _upgraded_rels u USING (relname, relkind)
WHERE u.relname IS NULL
ORDER BY 1;

-- Step 23: Relations EXTRA after upgrade.
SELECT u.relname || ' (' || u.relkind || ')' AS extra_relation
FROM _upgraded_rels u
LEFT JOIN _fresh_rels f USING (relname, relkind)
WHERE f.relname IS NULL
ORDER BY 1;

-- Step 24: Types MISSING after upgrade.
SELECT f.typname || ' (' || f.typtype || ')' AS missing_type
FROM _fresh_types f
LEFT JOIN _upgraded_types u USING (typname, typtype)
WHERE u.typname IS NULL
ORDER BY 1;

-- Step 25: Types EXTRA after upgrade.
SELECT u.typname || ' (' || u.typtype || ')' AS extra_type
FROM _upgraded_types u
LEFT JOIN _fresh_types f USING (typname, typtype)
WHERE f.typname IS NULL
ORDER BY 1;

-- Step 26: Operators MISSING after upgrade.
SELECT f.oprname || ' (' || f.lefttype || ', ' || f.righttype || ')' AS missing_operator
FROM _fresh_ops f
LEFT JOIN _upgraded_ops u USING (oprname, lefttype, righttype)
WHERE u.oprname IS NULL
ORDER BY 1;

-- Step 27: Operators EXTRA after upgrade.
SELECT u.oprname || ' (' || u.lefttype || ', ' || u.righttype || ')' AS extra_operator
FROM _upgraded_ops u
LEFT JOIN _fresh_ops f USING (oprname, lefttype, righttype)
WHERE f.oprname IS NULL
ORDER BY 1;

-- Step 28: Casts MISSING after upgrade.
SELECT f.source_type || ' -> ' || f.target_type || ' (' || f.castcontext || ')' AS missing_cast
FROM _fresh_casts f
LEFT JOIN _upgraded_casts u USING (source_type, target_type, castcontext)
WHERE u.source_type IS NULL
ORDER BY 1;

-- Step 29: Casts EXTRA after upgrade.
SELECT u.source_type || ' -> ' || u.target_type || ' (' || u.castcontext || ')' AS extra_cast
FROM _upgraded_casts u
LEFT JOIN _fresh_casts f USING (source_type, target_type, castcontext)
WHERE f.source_type IS NULL
ORDER BY 1;

-- Step 30: Operator classes MISSING after upgrade.
SELECT f.opcname || ' (' || f.amname || ')' AS missing_opclass
FROM _fresh_opclass f
LEFT JOIN _upgraded_opclass u USING (opcname, amname)
WHERE u.opcname IS NULL
ORDER BY 1;

-- Step 31: Operator classes EXTRA after upgrade.
SELECT u.opcname || ' (' || u.amname || ')' AS extra_opclass
FROM _upgraded_opclass u
LEFT JOIN _fresh_opclass f USING (opcname, amname)
WHERE f.opcname IS NULL
ORDER BY 1;

-- Step 32: Constraints MISSING after upgrade.
SELECT f.conname || ' (' || f.contype || ' on ' || f.table_name || ')' AS missing_constraint
FROM _fresh_constraints f
LEFT JOIN _upgraded_constraints u USING (conname, contype, table_name)
WHERE u.conname IS NULL
ORDER BY 1;

-- Step 33: Constraints EXTRA after upgrade.
SELECT u.conname || ' (' || u.contype || ' on ' || u.table_name || ')' AS extra_constraint
FROM _upgraded_constraints u
LEFT JOIN _fresh_constraints f USING (conname, contype, table_name)
WHERE f.conname IS NULL
ORDER BY 1;

-- Step 33a: Extension members MISSING after upgrade
-- (object exists in pg_proc/pg_class/etc. but is not linked to the AGE
-- extension via pg_depend, i.e. ALTER EXTENSION age ADD ... was forgotten).
SELECT f.member AS missing_extension_member
FROM _fresh_extmembers f
LEFT JOIN _upgraded_extmembers u USING (member)
WHERE u.member IS NULL
ORDER BY 1;

-- Step 33b: Extension members EXTRA after upgrade.
SELECT u.member AS extra_extension_member
FROM _upgraded_extmembers u
LEFT JOIN _fresh_extmembers f USING (member)
WHERE f.member IS NULL
ORDER BY 1;

-- =====================================================================
-- SUMMARY (Step 34)
-- =====================================================================

-- Step 34: Verify all counts match (result: single row, all true).
SELECT
    (SELECT count(*) FROM _fresh_funcs)       = (SELECT count(*) FROM _upgraded_funcs)       AS funcs_match,
    (SELECT count(*) FROM _fresh_rels)        = (SELECT count(*) FROM _upgraded_rels)        AS rels_match,
    (SELECT count(*) FROM _fresh_types)       = (SELECT count(*) FROM _upgraded_types)       AS types_match,
    (SELECT count(*) FROM _fresh_ops)         = (SELECT count(*) FROM _upgraded_ops)         AS ops_match,
    (SELECT count(*) FROM _fresh_casts)       = (SELECT count(*) FROM _upgraded_casts)       AS casts_match,
    (SELECT count(*) FROM _fresh_opclass)     = (SELECT count(*) FROM _upgraded_opclass)     AS opclass_match,
    (SELECT count(*) FROM _fresh_constraints) = (SELECT count(*) FROM _upgraded_constraints) AS constraints_match,
    (SELECT count(*) FROM _fresh_extmembers)  = (SELECT count(*) FROM _upgraded_extmembers)  AS extmembers_match;

-- =====================================================================
-- CLEANUP (Steps 35-36)
-- =====================================================================

-- Step 35: Drop temp tables, restore AGE at default version.
DROP TABLE _fresh_funcs, _upgraded_funcs, _fresh_rels, _upgraded_rels,
           _fresh_types, _upgraded_types, _fresh_ops, _upgraded_ops,
           _fresh_casts, _upgraded_casts, _fresh_opclass, _upgraded_opclass,
           _fresh_constraints, _upgraded_constraints,
           _fresh_extmembers, _upgraded_extmembers;
DROP EXTENSION age;
CREATE EXTENSION age;

-- Step 36: Remove synthetic upgrade test files from the extension directory.
\! sh ./regress/age_upgrade_cleanup.sh
