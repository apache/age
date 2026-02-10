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
-- pg_upgrade support functions
--
-- These functions help users upgrade PostgreSQL major versions using pg_upgrade
-- while preserving Apache AGE graph data. The ag_graph.namespace column uses
-- the regnamespace type which is not supported by pg_upgrade. These functions
-- temporarily convert the schema to be pg_upgrade compatible, then restore it
-- to the original state after the upgrade completes.
--
-- Usage:
--   1. Before pg_upgrade: SELECT age_prepare_pg_upgrade();
--   2. Run pg_upgrade as normal
--   3. After pg_upgrade:  SELECT age_finish_pg_upgrade();
--
-- To cancel an upgrade after preparation (before running pg_upgrade):
--   SELECT age_revert_pg_upgrade_changes();
--

--
-- age_prepare_pg_upgrade()
--
-- Prepares an AGE database for pg_upgrade by converting the ag_graph.namespace
-- column from regnamespace to oid type. This is necessary because pg_upgrade
-- does not support the regnamespace type in user tables.
--
-- This function:
--   1. Creates a backup table with graph name to namespace name mappings
--   2. Drops the existing namespace index
--   3. Converts the namespace column from regnamespace to oid
--   4. Recreates the namespace index with oid type
--   5. Creates a view for backward-compatible namespace display
--
-- Returns: void
-- Side effects: Modifies ag_graph table structure
--
CREATE FUNCTION ag_catalog.age_prepare_pg_upgrade()
    RETURNS void
    LANGUAGE plpgsql
    SET search_path = ag_catalog, pg_catalog
    AS $function$
DECLARE
    graph_count integer;
BEGIN
    -- Check if namespace column is already oid type (already prepared)
    IF EXISTS (
        SELECT 1 FROM information_schema.columns
        WHERE table_schema = 'ag_catalog'
          AND table_name = 'ag_graph'
          AND column_name = 'namespace'
          AND data_type = 'oid'
    ) THEN
        RAISE NOTICE 'Database already prepared for pg_upgrade (namespace is oid type).';
        RETURN;
    END IF;

    -- Drop existing backup table if it exists (from a previous failed attempt)
    DROP TABLE IF EXISTS public._age_pg_upgrade_backup;

    -- Create backup table with graph names mapped to namespace names
    -- We store nspname directly (not regnamespace::text) to avoid quoting issues
    -- Names survive pg_upgrade while OIDs don't
    CREATE TABLE public._age_pg_upgrade_backup AS
    SELECT
        g.graphid AS old_graphid,
        g.name AS graph_name,
        n.nspname AS namespace_name
    FROM ag_catalog.ag_graph g
    JOIN pg_namespace n ON n.oid = g.namespace::oid;

    SELECT count(*) INTO graph_count FROM public._age_pg_upgrade_backup;

    RAISE NOTICE 'Created backup table public._age_pg_upgrade_backup with % graph(s)', graph_count;

    -- Even with zero graphs, we still need to convert the column type
    -- because the regnamespace type itself blocks pg_upgrade

    -- Drop the existing regnamespace-based index
    DROP INDEX IF EXISTS ag_catalog.ag_graph_namespace_index;

    -- Convert namespace column from regnamespace to oid
    ALTER TABLE ag_catalog.ag_graph
        ALTER COLUMN namespace TYPE oid USING namespace::oid;

    -- Recreate the index with oid type
    CREATE UNIQUE INDEX ag_graph_namespace_index
        ON ag_catalog.ag_graph USING btree (namespace);

    -- Create a view for backward-compatible display of namespace as schema name
    CREATE OR REPLACE VIEW ag_catalog.ag_graph_view AS
    SELECT graphid, name, namespace::regnamespace AS namespace
    FROM ag_catalog.ag_graph;

    RAISE NOTICE 'Successfully prepared database for pg_upgrade.';
    RAISE NOTICE 'The ag_graph.namespace column has been converted from regnamespace to oid.';
    RAISE NOTICE 'You can now run pg_upgrade.';
    RAISE NOTICE 'After pg_upgrade completes, run: SELECT age_finish_pg_upgrade();';
END;
$function$;

COMMENT ON FUNCTION ag_catalog.age_prepare_pg_upgrade() IS 
'Prepares an AGE database for pg_upgrade by converting ag_graph.namespace from regnamespace to oid type. Run this before pg_upgrade.';

--
-- age_finish_pg_upgrade()
--
-- Completes the pg_upgrade process by remapping stale OIDs in ag_graph and
-- ag_label tables to their new values, then restores the original schema.
-- After pg_upgrade, the namespace OIDs stored in these tables no longer match
-- the actual pg_namespace OIDs because PostgreSQL assigns new OIDs to schemas
-- during the upgrade.
--
-- This function:
--   1. Reads the backup table created by age_prepare_pg_upgrade()
--   2. Looks up new namespace OIDs by schema name
--   3. Updates ag_label.graph references
--   4. Updates ag_graph.graphid and ag_graph.namespace
--   5. Restores namespace column to regnamespace type
--   6. Cleans up the backup table and view
--   7. Invalidates AGE caches to ensure cypher queries work immediately
--
-- Returns: void
-- Side effects: Updates OIDs in ag_graph and ag_label tables, restores schema
--
CREATE FUNCTION ag_catalog.age_finish_pg_upgrade()
    RETURNS void
    LANGUAGE plpgsql
    SET search_path = ag_catalog, pg_catalog
    AS $function$
DECLARE
    mapping_count integer;
    updated_labels integer;
    updated_graphs integer;
BEGIN
    -- Check if backup table exists
    IF NOT EXISTS (
        SELECT 1 FROM information_schema.tables
        WHERE table_schema = 'public'
          AND table_name = '_age_pg_upgrade_backup'
    ) THEN
        RAISE EXCEPTION 'Backup table public._age_pg_upgrade_backup not found. '
            'Did you run age_prepare_pg_upgrade() before pg_upgrade?';
    END IF;

    -- Check if namespace column is oid type (was properly prepared)
    IF NOT EXISTS (
        SELECT 1 FROM information_schema.columns
        WHERE table_schema = 'ag_catalog'
          AND table_name = 'ag_graph'
          AND column_name = 'namespace'
          AND data_type = 'oid'
    ) THEN
        RAISE EXCEPTION 'ag_graph.namespace is not oid type. '
            'Did you run age_prepare_pg_upgrade() before pg_upgrade?';
    END IF;

    -- Create temporary mapping table with old and new OIDs
    CREATE TEMP TABLE _graphid_mapping AS
    SELECT
        b.old_graphid,
        b.graph_name,
        n.oid AS new_graphid
    FROM public._age_pg_upgrade_backup b
    JOIN pg_namespace n ON n.nspname = b.namespace_name;

    GET DIAGNOSTICS mapping_count = ROW_COUNT;

    -- Verify all backup rows were mapped (detect missing schemas)
    DECLARE
        backup_count integer;
    BEGIN
        SELECT count(*) INTO backup_count FROM public._age_pg_upgrade_backup;
        IF mapping_count < backup_count THEN
            RAISE EXCEPTION 'Only % of % graphs could be mapped. Some schema names may have changed or been dropped.',
                mapping_count, backup_count;
        END IF;
    END;

    -- Handle zero-graph case (still need to restore schema)
    IF mapping_count = 0 THEN
        RAISE NOTICE 'No graphs to remap (empty backup table).';
        DROP TABLE _graphid_mapping;
        -- Skip to schema restoration
    ELSE
        RAISE NOTICE 'Found % graph(s) to remap', mapping_count;

        -- Temporarily drop foreign key constraint
        ALTER TABLE ag_catalog.ag_label DROP CONSTRAINT IF EXISTS fk_graph_oid;

        -- Update ag_label.graph references to use new OIDs
        UPDATE ag_catalog.ag_label l
        SET graph = m.new_graphid
        FROM _graphid_mapping m
        WHERE l.graph = m.old_graphid;

        GET DIAGNOSTICS updated_labels = ROW_COUNT;
        RAISE NOTICE 'Updated % label record(s)', updated_labels;

        -- Update ag_graph.graphid and ag_graph.namespace to new OIDs
        UPDATE ag_catalog.ag_graph g
        SET graphid = m.new_graphid,
            namespace = m.new_graphid
        FROM _graphid_mapping m
        WHERE g.graphid = m.old_graphid;

        GET DIAGNOSTICS updated_graphs = ROW_COUNT;
        RAISE NOTICE 'Updated % graph record(s)', updated_graphs;

        -- Restore foreign key constraint
        ALTER TABLE ag_catalog.ag_label
        ADD CONSTRAINT fk_graph_oid
            FOREIGN KEY(graph) REFERENCES ag_catalog.ag_graph(graphid);

        -- Clean up temporary mapping table
        DROP TABLE _graphid_mapping;

        RAISE NOTICE 'Successfully completed pg_upgrade OID remapping.';
    END IF;

    --
    -- Restore original schema (revert namespace to regnamespace)
    --
    RAISE NOTICE 'Restoring original schema...';

    -- Drop the view (no longer needed with regnamespace)
    DROP VIEW IF EXISTS ag_catalog.ag_graph_view;

    -- Drop the existing oid-based index
    DROP INDEX IF EXISTS ag_catalog.ag_graph_namespace_index;

    -- Convert namespace column back to regnamespace
    ALTER TABLE ag_catalog.ag_graph
        ALTER COLUMN namespace TYPE regnamespace USING namespace::regnamespace;

    -- Recreate the index with regnamespace type
    CREATE UNIQUE INDEX ag_graph_namespace_index
        ON ag_catalog.ag_graph USING btree (namespace);

    RAISE NOTICE 'Successfully restored ag_graph.namespace to regnamespace type.';

    --
    -- Invalidate AGE's internal caches by touching each graph's namespace
    -- AGE registers a syscache callback on NAMESPACEOID, so altering a schema
    -- triggers cache invalidation. This ensures cypher queries work immediately
    -- without requiring a session reconnect.
    --
    -- We use xact-level advisory lock (auto-released at transaction end)
    -- and preserve original schema ownership.
    --
    RAISE NOTICE 'Invalidating AGE caches...';
    PERFORM pg_catalog.pg_advisory_xact_lock(hashtext('age_finish_pg_upgrade'));
    DECLARE
        graph_rec RECORD;
        cache_invalidated boolean := false;
    BEGIN
        FOR graph_rec IN
            SELECT n.nspname AS ns_name, r.rolname AS owner_name
            FROM ag_catalog.ag_graph g
            JOIN pg_namespace n ON n.oid = g.namespace
            JOIN pg_roles r ON r.oid = n.nspowner
        LOOP
            BEGIN
                -- Touch schema by changing owner to current_user then back to original
                -- This triggers cache invalidation without permanently changing ownership
                EXECUTE format('ALTER SCHEMA %I OWNER TO %I', graph_rec.ns_name, current_user);
                EXECUTE format('ALTER SCHEMA %I OWNER TO %I', graph_rec.ns_name, graph_rec.owner_name);
                cache_invalidated := true;
            EXCEPTION WHEN insufficient_privilege THEN
                -- If we can't change ownership, skip this schema
                -- The cache will be invalidated on first use anyway
                RAISE NOTICE 'Could not invalidate cache for schema % (insufficient privileges)', graph_rec.ns_name;
            END;
        END LOOP;
        IF NOT cache_invalidated AND (SELECT count(*) FROM ag_catalog.ag_graph) > 0 THEN
            RAISE NOTICE 'Cache invalidation skipped. You may need to reconnect for cypher queries to work.';
        END IF;
    END;

    -- Now that all steps succeeded, clean up the backup table
    DROP TABLE IF EXISTS public._age_pg_upgrade_backup;

    RAISE NOTICE '';
    RAISE NOTICE 'pg_upgrade complete. All graph data has been preserved.';
END;
$function$;

COMMENT ON FUNCTION ag_catalog.age_finish_pg_upgrade() IS
'Completes pg_upgrade by remapping stale OIDs and restoring the original schema. Run this after pg_upgrade.';

--
-- age_revert_pg_upgrade_changes()
--
-- Reverts the schema changes made by age_prepare_pg_upgrade() if you need to
-- cancel the upgrade process before running pg_upgrade. This restores the
-- namespace column to its original regnamespace type.
--
-- NOTE: This function is NOT needed after age_finish_pg_upgrade(), which
-- automatically restores the original schema. Use this only if you called
-- age_prepare_pg_upgrade() but decided not to proceed with pg_upgrade.
--
-- This function:
--   1. Drops the ag_graph_view (no longer needed)
--   2. Drops the oid-based namespace index
--   3. Converts namespace column back to regnamespace
--   4. Recreates the namespace index with regnamespace type
--   5. Invalidates AGE caches to ensure cypher queries work immediately
--   6. Does NOT clean up the backup table (manual cleanup may be needed)
--
-- Returns: void
-- Side effects: Modifies ag_graph table structure
--
CREATE FUNCTION ag_catalog.age_revert_pg_upgrade_changes()
    RETURNS void
    LANGUAGE plpgsql
    SET search_path = ag_catalog, pg_catalog
    AS $function$
BEGIN
    -- Check if namespace column is oid type (needs reverting)
    IF NOT EXISTS (
        SELECT 1 FROM information_schema.columns
        WHERE table_schema = 'ag_catalog'
          AND table_name = 'ag_graph'
          AND column_name = 'namespace'
          AND data_type = 'oid'
    ) THEN
        RAISE NOTICE 'ag_graph.namespace is already regnamespace type. Nothing to revert.';
        RETURN;
    END IF;

    -- Drop the view (no longer needed with regnamespace)
    DROP VIEW IF EXISTS ag_catalog.ag_graph_view;

    -- Drop the existing oid-based index
    DROP INDEX IF EXISTS ag_catalog.ag_graph_namespace_index;

    -- Convert namespace column back to regnamespace
    ALTER TABLE ag_catalog.ag_graph
        ALTER COLUMN namespace TYPE regnamespace USING namespace::regnamespace;

    -- Recreate the index with regnamespace type
    CREATE UNIQUE INDEX ag_graph_namespace_index
        ON ag_catalog.ag_graph USING btree (namespace);

    --
    -- Invalidate AGE's internal caches by touching each graph's namespace
    -- We use xact-level advisory lock and preserve original ownership
    --
    PERFORM pg_catalog.pg_advisory_xact_lock(hashtext('age_revert_pg_upgrade'));
    DECLARE
        graph_rec RECORD;
    BEGIN
        FOR graph_rec IN
            SELECT n.nspname AS ns_name, r.rolname AS owner_name
            FROM ag_catalog.ag_graph g
            JOIN pg_namespace n ON n.oid = g.namespace
            JOIN pg_roles r ON r.oid = n.nspowner
        LOOP
            BEGIN
                -- Touch schema by changing owner to current_user then back to original
                EXECUTE format('ALTER SCHEMA %I OWNER TO %I', graph_rec.ns_name, current_user);
                EXECUTE format('ALTER SCHEMA %I OWNER TO %I', graph_rec.ns_name, graph_rec.owner_name);
            EXCEPTION WHEN insufficient_privilege THEN
                RAISE NOTICE 'Could not invalidate cache for schema % (insufficient privileges)', graph_rec.ns_name;
            END;
        END LOOP;
    END;

    RAISE NOTICE 'Successfully reverted ag_graph.namespace to regnamespace type.';
    RAISE NOTICE '';
    RAISE NOTICE 'Upgrade preparation has been cancelled.';
    RAISE NOTICE 'You may want to drop the backup table: DROP TABLE IF EXISTS public._age_pg_upgrade_backup;';
END;
$function$;

COMMENT ON FUNCTION ag_catalog.age_revert_pg_upgrade_changes() IS
'Reverts schema changes if you need to cancel after age_prepare_pg_upgrade() but before pg_upgrade. Not needed after age_finish_pg_upgrade().';

--
-- age_pg_upgrade_status()
--
-- Returns the current pg_upgrade readiness status of the AGE installation.
-- Useful for checking whether the database needs preparation before pg_upgrade.
--
-- Returns: TABLE with status information
--
CREATE FUNCTION ag_catalog.age_pg_upgrade_status()
    RETURNS TABLE (
        status text,
        namespace_type text,
        graph_count bigint,
        backup_exists boolean,
        message text
    )
    LANGUAGE plpgsql
    SET search_path = ag_catalog, pg_catalog
    AS $function$
DECLARE
    ns_type text;
    g_count bigint;
    backup_exists boolean;
BEGIN
    -- Get namespace column type
    SELECT data_type INTO ns_type
    FROM information_schema.columns
    WHERE table_schema = 'ag_catalog'
      AND table_name = 'ag_graph'
      AND column_name = 'namespace';

    -- Get graph count
    SELECT count(*) INTO g_count FROM ag_catalog.ag_graph;

    -- Check for backup table
    SELECT EXISTS(
        SELECT 1 FROM information_schema.tables
        WHERE table_schema = 'public'
          AND table_name = '_age_pg_upgrade_backup'
    ) INTO backup_exists;

    -- Determine status and message
    IF ns_type = 'regnamespace' AND NOT backup_exists THEN
        -- Normal state - ready for use, needs prep before pg_upgrade
        RETURN QUERY SELECT
            'NORMAL'::text,
            ns_type,
            g_count,
            backup_exists,
            'Run SELECT age_prepare_pg_upgrade(); before pg_upgrade'::text;
    ELSIF ns_type = 'regnamespace' AND backup_exists THEN
        -- Unusual state - backup exists but schema wasn't converted
        RETURN QUERY SELECT
            'WARNING'::text,
            ns_type,
            g_count,
            backup_exists,
            'Backup table exists but schema not converted. Run age_prepare_pg_upgrade() again.'::text;
    ELSIF ns_type = 'oid' AND backup_exists THEN
        -- Prepared and ready for pg_upgrade, or awaiting finish after pg_upgrade
        RETURN QUERY SELECT
            'PREPARED - AWAITING FINISH'::text,
            ns_type,
            g_count,
            backup_exists,
            'After pg_upgrade, run SELECT age_finish_pg_upgrade();'::text;
    ELSE
        -- oid type without backup - manually converted or partial state
        RETURN QUERY SELECT
            'CONVERTED'::text,
            ns_type,
            g_count,
            backup_exists,
            'Namespace is oid type. If upgrading, ensure backup table exists.'::text;
    END IF;
END;
$function$;

COMMENT ON FUNCTION ag_catalog.age_pg_upgrade_status() IS
'Returns the current pg_upgrade readiness status of the AGE installation.';
