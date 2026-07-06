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

-- complain if script is sourced in psql, rather than via CREATE EXTENSION
\echo Use "ALTER EXTENSION age UPDATE TO '1.8.0'" to load this file. \quit

--
-- pg_upgrade support functions
--
-- These functions help users upgrade PostgreSQL major versions using pg_upgrade
-- while preserving Apache AGE graph data.
--

CREATE FUNCTION ag_catalog.age_prepare_pg_upgrade()
    RETURNS void
    LANGUAGE plpgsql
    -- Resolve built-in functions and operators from pg_catalog first so they
    -- are not overridden by same-named objects defined in ag_catalog. The
    -- ag_catalog objects referenced here are schema-qualified.
    SET search_path = pg_catalog, ag_catalog
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

CREATE FUNCTION ag_catalog.age_finish_pg_upgrade()
    RETURNS void
    LANGUAGE plpgsql
    -- Resolve built-in functions and operators from pg_catalog first so they
    -- are not overridden by same-named objects defined in ag_catalog. The
    -- ag_catalog objects referenced here are schema-qualified.
    SET search_path = pg_catalog, ag_catalog
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
    PERFORM pg_catalog.pg_advisory_xact_lock(pg_catalog.hashtext('age_finish_pg_upgrade'));
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
                EXECUTE pg_catalog.format('ALTER SCHEMA %I OWNER TO %I', graph_rec.ns_name, current_user);
                EXECUTE pg_catalog.format('ALTER SCHEMA %I OWNER TO %I', graph_rec.ns_name, graph_rec.owner_name);
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

CREATE FUNCTION ag_catalog.age_revert_pg_upgrade_changes()
    RETURNS void
    LANGUAGE plpgsql
    -- Resolve built-in functions and operators from pg_catalog first so they
    -- are not overridden by same-named objects defined in ag_catalog. The
    -- ag_catalog objects referenced here are schema-qualified.
    SET search_path = pg_catalog, ag_catalog
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
    PERFORM pg_catalog.pg_advisory_xact_lock(pg_catalog.hashtext('age_revert_pg_upgrade'));
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
                EXECUTE pg_catalog.format('ALTER SCHEMA %I OWNER TO %I', graph_rec.ns_name, current_user);
                EXECUTE pg_catalog.format('ALTER SCHEMA %I OWNER TO %I', graph_rec.ns_name, graph_rec.owner_name);
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

CREATE FUNCTION ag_catalog.age_pg_upgrade_status()
    RETURNS TABLE (
        status text,
        namespace_type text,
        graph_count bigint,
        backup_exists boolean,
        message text
    )
    LANGUAGE plpgsql
    -- Resolve built-in functions and operators from pg_catalog first so they
    -- are not overridden by same-named objects defined in ag_catalog. The
    -- ag_catalog objects referenced here are schema-qualified.
    SET search_path = pg_catalog, ag_catalog
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

--
-- VLE cache invalidation trigger function
-- Installed on graph label tables to catch SQL-level mutations
-- and increment the per-graph version counter for VLE cache invalidation.
--
CREATE FUNCTION ag_catalog.age_invalidate_graph_cache()
    RETURNS trigger
    LANGUAGE c
AS 'MODULE_PATHNAME';

--
-- Install the cache invalidation trigger on all pre-existing label tables.
-- New label tables created after this upgrade will get the trigger
-- automatically via label_commands.c. This DO block handles tables
-- that were created before the upgrade.
--
DO $$
DECLARE
    r RECORD;
BEGIN
    FOR r IN
        SELECT n.nspname AS schema_name, c.relname AS table_name
        FROM ag_catalog.ag_label l
        JOIN pg_catalog.pg_class c ON c.oid = l.relation
        JOIN pg_catalog.pg_namespace n ON n.oid = c.relnamespace
        WHERE l.name != '_ag_label_vertex'
          AND l.name != '_ag_label_edge'
    LOOP
        -- Skip if trigger already exists on this table
        IF NOT EXISTS (
            SELECT 1 FROM pg_catalog.pg_trigger t
            JOIN pg_catalog.pg_class c ON c.oid = t.tgrelid
            JOIN pg_catalog.pg_namespace n ON n.oid = c.relnamespace
            WHERE n.nspname = r.schema_name
              AND c.relname = r.table_name
              AND t.tgname = '_age_cache_invalidate'
        )
        THEN
            EXECUTE pg_catalog.format(
                'CREATE TRIGGER _age_cache_invalidate '
                'AFTER INSERT OR UPDATE OR DELETE OR TRUNCATE '
                'ON %I.%I '
                'FOR EACH STATEMENT '
                'EXECUTE FUNCTION ag_catalog.age_invalidate_graph_cache()',
                r.schema_name, r.table_name
            );
        END IF;
    END LOOP;
END;
$$;

--
-- agtype <-> jsonb bidirectional casts
--

-- agtype -> jsonb (explicit)
-- Uses json intermediate (agtype_to_json -> json::jsonb) because agtype
-- extends jsonb's binary format with types (AGTV_INTEGER, AGTV_FLOAT,
-- AGTV_VERTEX, AGTV_EDGE, AGTV_PATH) that jsonb does not recognize.
CREATE FUNCTION ag_catalog.agtype_to_jsonb(agtype)
    RETURNS jsonb
    LANGUAGE sql
    IMMUTABLE
RETURNS NULL ON NULL INPUT
PARALLEL SAFE
AS 'SELECT ag_catalog.agtype_to_json($1)::jsonb';

CREATE CAST (agtype AS jsonb)
    WITH FUNCTION ag_catalog.agtype_to_jsonb(agtype);

-- jsonb -> agtype (explicit)
CREATE FUNCTION ag_catalog.jsonb_to_agtype(jsonb)
    RETURNS agtype
    LANGUAGE sql
    IMMUTABLE
RETURNS NULL ON NULL INPUT
PARALLEL SAFE
AS 'SELECT $1::text::agtype';

CREATE CAST (jsonb AS agtype)
    WITH FUNCTION ag_catalog.jsonb_to_agtype(jsonb);

--
-- S4: VLE SRF signature change
--
-- The age_vle SRF now emits start_id and end_id as scalar graphid columns
-- alongside the existing `edges` column. This allows the cypher transformer
-- to rewrite terminal-edge match quals as plain integer equalities,
-- removing the per-row age_match_vle_terminal_edge and age_match_two_vle_edges
-- function calls from VLE query plans.  Both qual functions are dropped.
--
-- BREAKING CHANGE for any external SQL that called age_vle(...) directly
-- and relied on `RETURNS SETOF agtype`, or called age_match_vle_terminal_edge
-- / age_match_two_vle_edges directly.  Internal AGE callers (the cypher
-- transformer) are not affected.
--
DROP FUNCTION IF EXISTS ag_catalog.age_match_vle_terminal_edge(variadic "any");
DROP FUNCTION IF EXISTS ag_catalog.age_match_two_vle_edges(agtype, agtype);

DROP FUNCTION IF EXISTS ag_catalog.age_vle(agtype, agtype, agtype, agtype,
                                           agtype, agtype, agtype);
DROP FUNCTION IF EXISTS ag_catalog.age_vle(agtype, agtype, agtype, agtype,
                                           agtype, agtype, agtype, agtype);

CREATE FUNCTION ag_catalog.age_vle(IN agtype, IN agtype, IN agtype, IN agtype,
                                   IN agtype, IN agtype, IN agtype,
                                   OUT edges    agtype,
                                   OUT start_id graphid,
                                   OUT end_id   graphid)
    RETURNS SETOF record
LANGUAGE C
STABLE
CALLED ON NULL INPUT
PARALLEL UNSAFE
AS 'MODULE_PATHNAME';

CREATE FUNCTION ag_catalog.age_vle(IN agtype, IN agtype, IN agtype, IN agtype,
                                   IN agtype, IN agtype, IN agtype, IN agtype,
                                   OUT edges    agtype,
                                   OUT start_id graphid,
                                   OUT end_id   graphid)
    RETURNS SETOF record
LANGUAGE C
STABLE
CALLED ON NULL INPUT
PARALLEL UNSAFE
AS 'MODULE_PATHNAME';

-- Unweighted (hop-count) shortest path between two vertices, computed over the
-- cached global graph adjacency via BFS. Returns a single path (0 or 1 rows).
-- Argument order mirrors the Cypher shortestPath() pattern
-- (a)-[:type*min_hops..max_hops]->(b):
--   (graph_name, start, end, edge_types, direction, min_hops, max_hops)
CREATE FUNCTION ag_catalog.age_shortest_path(IN agtype, IN agtype, IN agtype,
                                             IN agtype DEFAULT NULL,
                                             IN agtype DEFAULT NULL,
                                             IN agtype DEFAULT NULL,
                                             IN agtype DEFAULT NULL)
    RETURNS SETOF agtype
LANGUAGE C
STABLE
CALLED ON NULL INPUT
PARALLEL UNSAFE
AS 'MODULE_PATHNAME';

-- All unweighted shortest paths between two vertices (one path per row).
-- Same argument order as age_shortest_path.
CREATE FUNCTION ag_catalog.age_all_shortest_paths(IN agtype, IN agtype, IN agtype,
                                                  IN agtype DEFAULT NULL,
                                                  IN agtype DEFAULT NULL,
                                                  IN agtype DEFAULT NULL,
                                                  IN agtype DEFAULT NULL)
    RETURNS SETOF agtype
LANGUAGE C
STABLE
CALLED ON NULL INPUT
PARALLEL UNSAFE
AS 'MODULE_PATHNAME';

--
-- Composite types for vertex and edge
--
CREATE TYPE ag_catalog.vertex AS (
    id graphid,
    label agtype,
    properties agtype
);

CREATE TYPE ag_catalog.edge AS (
    id graphid,
    label agtype,
    end_id graphid,
    start_id graphid,
    properties agtype
);

--
-- vertex/edge to agtype cast functions
--
CREATE FUNCTION ag_catalog.vertex_to_agtype(vertex)
    RETURNS agtype
    LANGUAGE c
    IMMUTABLE
RETURNS NULL ON NULL INPUT
PARALLEL SAFE
AS 'MODULE_PATHNAME';

CREATE FUNCTION ag_catalog.edge_to_agtype(edge)
    RETURNS agtype
    LANGUAGE c
    IMMUTABLE
RETURNS NULL ON NULL INPUT
PARALLEL SAFE
AS 'MODULE_PATHNAME';

--
-- Implicit casts from vertex/edge to agtype
--
CREATE CAST (vertex AS agtype)
    WITH FUNCTION ag_catalog.vertex_to_agtype(vertex)
AS IMPLICIT;

CREATE CAST (edge AS agtype)
    WITH FUNCTION ag_catalog.edge_to_agtype(edge)
AS IMPLICIT;

CREATE FUNCTION ag_catalog.vertex_to_json(vertex)
    RETURNS json
    LANGUAGE c
    IMMUTABLE
RETURNS NULL ON NULL INPUT
PARALLEL SAFE
AS 'MODULE_PATHNAME';

CREATE FUNCTION ag_catalog.edge_to_json(edge)
    RETURNS json
    LANGUAGE c
    IMMUTABLE
RETURNS NULL ON NULL INPUT
PARALLEL SAFE
AS 'MODULE_PATHNAME';

CREATE CAST (vertex AS json)
    WITH FUNCTION ag_catalog.vertex_to_json(vertex);

CREATE CAST (edge AS json)
    WITH FUNCTION ag_catalog.edge_to_json(edge);

CREATE FUNCTION ag_catalog.vertex_to_jsonb(vertex)
    RETURNS jsonb
    LANGUAGE c
    IMMUTABLE
RETURNS NULL ON NULL INPUT
PARALLEL SAFE
AS 'MODULE_PATHNAME';

CREATE FUNCTION ag_catalog.edge_to_jsonb(edge)
    RETURNS jsonb
    LANGUAGE c
    IMMUTABLE
RETURNS NULL ON NULL INPUT
PARALLEL SAFE
AS 'MODULE_PATHNAME';

CREATE CAST (vertex AS jsonb)
    WITH FUNCTION ag_catalog.vertex_to_jsonb(vertex);

CREATE CAST (edge AS jsonb)
    WITH FUNCTION ag_catalog.edge_to_jsonb(edge);

--
-- Equality operators for vertex and edge (compare by id)
--
CREATE FUNCTION ag_catalog.vertex_eq(vertex, vertex)
    RETURNS boolean
    LANGUAGE sql
    IMMUTABLE
RETURNS NULL ON NULL INPUT
PARALLEL SAFE
AS $$ SELECT $1.id = $2.id $$;

CREATE OPERATOR = (
    FUNCTION = ag_catalog.vertex_eq,
    LEFTARG = vertex,
    RIGHTARG = vertex,
    COMMUTATOR = =,
    NEGATOR = <>,
    RESTRICT = eqsel,
    JOIN = eqjoinsel
);

CREATE FUNCTION ag_catalog.vertex_ne(vertex, vertex)
    RETURNS boolean
    LANGUAGE sql
    IMMUTABLE
RETURNS NULL ON NULL INPUT
PARALLEL SAFE
AS $$ SELECT $1.id <> $2.id $$;

CREATE OPERATOR <> (
    FUNCTION = ag_catalog.vertex_ne,
    LEFTARG = vertex,
    RIGHTARG = vertex,
    COMMUTATOR = <>,
    NEGATOR = =,
    RESTRICT = neqsel,
    JOIN = neqjoinsel
);

CREATE FUNCTION ag_catalog.edge_eq(edge, edge)
    RETURNS boolean
    LANGUAGE sql
    IMMUTABLE
RETURNS NULL ON NULL INPUT
PARALLEL SAFE
AS $$ SELECT $1.id = $2.id $$;

CREATE OPERATOR = (
    FUNCTION = ag_catalog.edge_eq,
    LEFTARG = edge,
    RIGHTARG = edge,
    COMMUTATOR = =,
    NEGATOR = <>,
    RESTRICT = eqsel,
    JOIN = eqjoinsel
);

CREATE FUNCTION ag_catalog.edge_ne(edge, edge)
    RETURNS boolean
    LANGUAGE sql
    IMMUTABLE
RETURNS NULL ON NULL INPUT
PARALLEL SAFE
AS $$ SELECT $1.id <> $2.id $$;

CREATE OPERATOR <> (
    FUNCTION = ag_catalog.edge_ne,
    LEFTARG = edge,
    RIGHTARG = edge,
    COMMUTATOR = <>,
    NEGATOR = =,
    RESTRICT = neqsel,
    JOIN = neqjoinsel
);

--
-- Drop and recreate _label_name with new return type (cstring -> agtype)
--
DROP FUNCTION IF EXISTS ag_catalog._label_name(oid, graphid);

CREATE FUNCTION ag_catalog._label_name(graph_oid oid, graphid)
    RETURNS agtype
    LANGUAGE c
    IMMUTABLE
PARALLEL SAFE
AS 'MODULE_PATHNAME';

--
-- Drop and recreate _agtype_build_vertex with new signature (cstring -> agtype for label)
--
DROP FUNCTION IF EXISTS ag_catalog._agtype_build_vertex(graphid, cstring, agtype);

CREATE FUNCTION ag_catalog._agtype_build_vertex(graphid, agtype, agtype)
    RETURNS agtype
    LANGUAGE c
    IMMUTABLE
CALLED ON NULL INPUT
PARALLEL SAFE
AS 'MODULE_PATHNAME';

--
-- Drop and recreate _agtype_build_edge with new signature (cstring -> agtype for label)
--
DROP FUNCTION IF EXISTS ag_catalog._agtype_build_edge(graphid, graphid, graphid, cstring, agtype);

CREATE FUNCTION ag_catalog._agtype_build_edge(graphid, graphid, graphid,
                                              agtype, agtype)
    RETURNS agtype
    LANGUAGE c
    IMMUTABLE
CALLED ON NULL INPUT
PARALLEL SAFE
AS 'MODULE_PATHNAME';

--
-- Helper function for optimized startNode/endNode
--
CREATE FUNCTION ag_catalog._get_vertex_by_graphid(text, graphid)
    RETURNS agtype
    LANGUAGE c
    STABLE
RETURNS NULL ON NULL INPUT
PARALLEL SAFE
AS 'MODULE_PATHNAME';

-- Internal selftest for the agehash open-addressing hashtable. Returns "OK"
-- on success or "FAIL: ..." with a diagnostic message. Intended for the
-- agehash regression test only.
CREATE FUNCTION ag_catalog._agehash_self_test()
    RETURNS text
    LANGUAGE c
    VOLATILE
    PARALLEL UNSAFE
AS 'MODULE_PATHNAME';

--
-- Issue #2356: rebind containment and key-existence operators to the
-- lightweight contsel / contjoinsel selectivity estimators.
--
-- @>, <@, @>>, <<@, ?, ?|, ?& on agtype were bound to matchingsel /
-- matchingjoinsel. During planning matchingsel invokes the operator's
-- underlying function (agtype_contains) once per pg_statistic MCV entry and
-- histogram bin; with a realistic default_statistics_target that planning
-- cost dominates simple point queries (the regression reported in #2356).
--
-- contsel / contjoinsel return fixed selectivity constants without calling the
-- operator function, so planning is constant-time. This is a deliberate
-- planning-speed vs. estimate-accuracy trade-off. Note it DIVERGES from
-- PostgreSQL core, which keeps jsonb's @>, <@, ?, ?|, ?& on matchingsel /
-- matchingjoinsel; it is an AGE-specific choice favoring workloads where these
-- operators appear in selective point lookups.
--
ALTER OPERATOR ag_catalog.@>(agtype, agtype)
    SET (RESTRICT = contsel, JOIN = contjoinsel);
ALTER OPERATOR ag_catalog.<@(agtype, agtype)
    SET (RESTRICT = contsel, JOIN = contjoinsel);
ALTER OPERATOR ag_catalog.@>>(agtype, agtype)
    SET (RESTRICT = contsel, JOIN = contjoinsel);
ALTER OPERATOR ag_catalog.<<@(agtype, agtype)
    SET (RESTRICT = contsel, JOIN = contjoinsel);
ALTER OPERATOR ag_catalog.?(agtype, text)
    SET (RESTRICT = contsel, JOIN = contjoinsel);
ALTER OPERATOR ag_catalog.?(agtype, agtype)
    SET (RESTRICT = contsel, JOIN = contjoinsel);
ALTER OPERATOR ag_catalog.?|(agtype, text[])
    SET (RESTRICT = contsel, JOIN = contjoinsel);
ALTER OPERATOR ag_catalog.?|(agtype, agtype)
    SET (RESTRICT = contsel, JOIN = contjoinsel);
ALTER OPERATOR ag_catalog.?&(agtype, text[])
    SET (RESTRICT = contsel, JOIN = contjoinsel);
ALTER OPERATOR ag_catalog.?&(agtype, agtype)
    SET (RESTRICT = contsel, JOIN = contjoinsel);

--
-- create_subgraph(): materialized subgraph extraction (see sql/age_subgraph.sql).
-- Induced-subgraph semantics matching Neo4j GDS gds.graph.filter(): a vertex is
-- kept iff node_filter holds ('*' = all); an edge is kept iff relationship_filter
-- holds AND both endpoints are kept. Produces a persistent, Cypher-queryable graph.
--
CREATE FUNCTION ag_catalog.create_subgraph(new_graph name,
                                           from_graph name,
                                           node_filter text DEFAULT '*',
                                           relationship_filter text DEFAULT '*')
    RETURNS TABLE(node_count bigint, relationship_count bigint)
    LANGUAGE plpgsql
    VOLATILE
    SET search_path = ag_catalog, pg_catalog
    AS $function$
DECLARE
    from_oid oid;
    new_oid  oid;
    v_node_count bigint := 0;
    v_rel_count  bigint := 0;
    rec RECORD;
    cypher_q text;
    where_clause text;
    dst_label_id int;
    dst_seq_fqn text;
    dst_relation text;
    inserted bigint;
    has_rows boolean;
BEGIN
    -- Argument validation.
    IF new_graph IS NULL THEN
        RAISE EXCEPTION 'new graph name must not be NULL';
    END IF;
    IF from_graph IS NULL THEN
        RAISE EXCEPTION 'source graph name must not be NULL';
    END IF;
    IF new_graph = from_graph THEN
        RAISE EXCEPTION 'cannot extract a subgraph of "%" into itself', from_graph;
    END IF;

    -- NULL predicate is treated as the '*' wildcard (keep all).
    IF node_filter IS NULL THEN
        node_filter := '*';
    END IF;
    IF relationship_filter IS NULL THEN
        relationship_filter := '*';
    END IF;

    -- The predicates are embedded into a dollar-quoted cypher() query using the
    -- $age_subgraph$ tag; reject predicates that contain the tag to keep the
    -- quoting unambiguous.
    IF position('$age_subgraph$' IN node_filter) > 0
       OR position('$age_subgraph$' IN relationship_filter) > 0 THEN
        RAISE EXCEPTION 'filter predicate must not contain the reserved token $age_subgraph$';
    END IF;

    -- Validate source graph exists.
    SELECT graphid INTO from_oid
    FROM ag_catalog.ag_graph WHERE name = from_graph;
    IF from_oid IS NULL THEN
        RAISE EXCEPTION 'graph "%" does not exist', from_graph;
    END IF;

    -- Validate destination graph does not exist (create_graph also enforces
    -- naming rules and uniqueness, but we give a clear early error).
    IF EXISTS (SELECT 1 FROM ag_catalog.ag_graph WHERE name = new_graph) THEN
        RAISE EXCEPTION 'graph "%" already exists', new_graph;
    END IF;

    -- Create the destination graph (default labels are created automatically).
    PERFORM ag_catalog.create_graph(new_graph);

    SELECT graphid INTO new_oid
    FROM ag_catalog.ag_graph WHERE name = new_graph;

    -- Working sets / mapping (uniquely named to avoid colliding with user temps).
    DROP TABLE IF EXISTS _ag_sg_kept_v;
    DROP TABLE IF EXISTS _ag_sg_kept_e;
    DROP TABLE IF EXISTS _ag_sg_vmap;
    DROP TABLE IF EXISTS _ag_sg_vstage;
    DROP TABLE IF EXISTS _ag_sg_estage;

    --
    -- Kept vertices: evaluate node_filter with AGE's Cypher engine. The node
    -- variable `n` is bound exactly as in the spec; '*' selects all vertices.
    --
    IF node_filter IS NULL OR btrim(node_filter) = '*' THEN
        where_clause := '';
    ELSE
        where_clause := ' WHERE ' || node_filter;
    END IF;
    cypher_q := 'MATCH (n)' || where_clause || ' RETURN id(n)';

    EXECUTE format(
        'CREATE TEMP TABLE _ag_sg_kept_v ON COMMIT DROP AS '
        'SELECT DISTINCT ag_catalog.agtype_to_graphid(vid) AS gid '
        'FROM ag_catalog.cypher(%L, $age_subgraph$%s$age_subgraph$) AS (vid agtype)',
        from_graph, cypher_q);
    CREATE INDEX ON _ag_sg_kept_v (gid);

    --
    -- Kept edges: evaluate relationship_filter with AGE's Cypher engine. The
    -- relationship variable `r` is bound exactly as in the spec.
    --
    IF relationship_filter IS NULL OR btrim(relationship_filter) = '*' THEN
        where_clause := '';
    ELSE
        where_clause := ' WHERE ' || relationship_filter;
    END IF;
    cypher_q := 'MATCH ()-[r]->()' || where_clause || ' RETURN id(r)';

    EXECUTE format(
        'CREATE TEMP TABLE _ag_sg_kept_e ON COMMIT DROP AS '
        'SELECT DISTINCT ag_catalog.agtype_to_graphid(eid) AS gid '
        'FROM ag_catalog.cypher(%L, $age_subgraph$%s$age_subgraph$) AS (eid agtype)',
        from_graph, cypher_q);
    CREATE INDEX ON _ag_sg_kept_e (gid);

    -- old -> new vertex id mapping (graphid is unique within a graph).
    CREATE TEMP TABLE _ag_sg_vmap (old_id graphid PRIMARY KEY,
                                   new_id graphid NOT NULL) ON COMMIT DROP;

    --
    -- PASS 1: copy kept vertices, label by label, assigning new graphids and
    -- recording the old->new mapping for edge remapping.
    --
    FOR rec IN
        SELECT name, id, relation, seq_name
        FROM ag_catalog.ag_label
        WHERE graph = from_oid AND kind = 'v'
        ORDER BY id
    LOOP
        -- Skip labels with no surviving vertices. Read ONLY this label's own
        -- rows: AGE label tables use table inheritance (custom labels inherit
        -- from _ag_label_vertex), so a plain scan of a parent would also return
        -- its children and copy them twice.
        EXECUTE format(
            'SELECT EXISTS (SELECT 1 FROM ONLY %s t '
            'WHERE EXISTS (SELECT 1 FROM _ag_sg_kept_v k WHERE k.gid = t.id))',
            rec.relation::regclass::text)
        INTO has_rows;
        IF NOT has_rows THEN
            CONTINUE;
        END IF;

        -- Ensure the label exists in the destination graph.
        IF rec.name <> '_ag_label_vertex' THEN
            PERFORM 1 FROM ag_catalog.ag_label
            WHERE graph = new_oid AND name = rec.name;
            IF NOT FOUND THEN
                EXECUTE format('SELECT ag_catalog.create_vlabel(%L, %L)',
                               new_graph, rec.name);
            END IF;
        END IF;

        SELECT id, seq_name, relation::regclass::text
        INTO dst_label_id, dst_seq_fqn, dst_relation
        FROM ag_catalog.ag_label
        WHERE graph = new_oid AND name = rec.name;
        dst_seq_fqn := format('%I.%I', new_graph, dst_seq_fqn);

        -- Stage surviving vertices with freshly generated ids in a real temp
        -- table (single evaluation), then copy to the label table and record
        -- the old->new mapping. A materialized stage avoids any ambiguity from
        -- referencing a nextval-bearing CTE more than once.
        DROP TABLE IF EXISTS _ag_sg_vstage;
        EXECUTE format(
            'CREATE TEMP TABLE _ag_sg_vstage ON COMMIT DROP AS '
            'SELECT t.id AS old_id, '
            '       ag_catalog._graphid(%s, nextval(%L::regclass)) AS new_id, '
            '       t.properties AS props '
            'FROM ONLY %s t '
            'WHERE EXISTS (SELECT 1 FROM _ag_sg_kept_v k WHERE k.gid = t.id)',
            dst_label_id, dst_seq_fqn, rec.relation::regclass::text);

        EXECUTE format('INSERT INTO %s (id, properties) '
                       'SELECT new_id, props FROM _ag_sg_vstage', dst_relation);

        INSERT INTO _ag_sg_vmap (old_id, new_id)
            SELECT old_id, new_id FROM _ag_sg_vstage;

        DROP TABLE _ag_sg_vstage;
    END LOOP;

    SELECT count(*) INTO v_node_count FROM _ag_sg_vmap;

    --
    -- PASS 2: copy kept edges, remapping endpoints. The joins to _ag_sg_vmap
    -- enforce the induced rule (an edge survives only if BOTH endpoints were
    -- kept); membership in _ag_sg_kept_e applies relationship_filter.
    --
    FOR rec IN
        SELECT name, id, relation, seq_name
        FROM ag_catalog.ag_label
        WHERE graph = from_oid AND kind = 'e'
        ORDER BY id
    LOOP
        -- Skip labels with no surviving edges. Read ONLY this label's own rows
        -- (see the vertex pass for why inheritance requires ONLY).
        EXECUTE format(
            'SELECT EXISTS ('
            '  SELECT 1 FROM ONLY %s x '
            '  JOIN _ag_sg_vmap vs ON vs.old_id = x.start_id '
            '  JOIN _ag_sg_vmap ve ON ve.old_id = x.end_id '
            '  WHERE EXISTS (SELECT 1 FROM _ag_sg_kept_e k WHERE k.gid = x.id))',
            rec.relation::regclass::text)
        INTO has_rows;
        IF NOT has_rows THEN
            CONTINUE;
        END IF;

        IF rec.name <> '_ag_label_edge' THEN
            PERFORM 1 FROM ag_catalog.ag_label
            WHERE graph = new_oid AND name = rec.name;
            IF NOT FOUND THEN
                EXECUTE format('SELECT ag_catalog.create_elabel(%L, %L)',
                               new_graph, rec.name);
            END IF;
        END IF;

        SELECT id, seq_name, relation::regclass::text
        INTO dst_label_id, dst_seq_fqn, dst_relation
        FROM ag_catalog.ag_label
        WHERE graph = new_oid AND name = rec.name;
        dst_seq_fqn := format('%I.%I', new_graph, dst_seq_fqn);

        -- Stage surviving edges, remapping endpoints through _ag_sg_vmap. The
        -- joins enforce the induced rule (both endpoints kept); membership in
        -- _ag_sg_kept_e applies relationship_filter.
        DROP TABLE IF EXISTS _ag_sg_estage;
        EXECUTE format(
            'CREATE TEMP TABLE _ag_sg_estage ON COMMIT DROP AS '
            'SELECT ag_catalog._graphid(%s, nextval(%L::regclass)) AS new_id, '
            '       vs.new_id AS new_start, ve.new_id AS new_end, '
            '       x.properties AS props '
            'FROM ONLY %s x '
            'JOIN _ag_sg_vmap vs ON vs.old_id = x.start_id '
            'JOIN _ag_sg_vmap ve ON ve.old_id = x.end_id '
            'WHERE EXISTS (SELECT 1 FROM _ag_sg_kept_e k WHERE k.gid = x.id)',
            dst_label_id, dst_seq_fqn, rec.relation::regclass::text);

        EXECUTE format('INSERT INTO %s (id, start_id, end_id, properties) '
                       'SELECT new_id, new_start, new_end, props '
                       'FROM _ag_sg_estage', dst_relation);
        GET DIAGNOSTICS inserted = ROW_COUNT;
        v_rel_count := v_rel_count + inserted;

        DROP TABLE _ag_sg_estage;
    END LOOP;

    RETURN QUERY SELECT v_node_count, v_rel_count;
END;
$function$;

COMMENT ON FUNCTION ag_catalog.create_subgraph(name, name, text, text) IS
'Materializes a new persistent graph as the induced subgraph of from_graph selected by a Cypher node predicate (on n) and relationship predicate (on r); ''*'' keeps all. An edge is kept only if its predicate holds and both endpoints are kept. Returns (node_count, relationship_count).';

--
-- reduce(acc = init, var IN list | body) fold support
--
-- Transition function for the age_reduce aggregate. The fold body is compiled
-- by transform_cypher_reduce() with the accumulator and element rewritten to
-- PARAM_EXEC params 0 and 1 and serialized into the text argument; the
-- transition evaluates it for each element in list order. The trailing
-- agtype[] argument carries the loop-invariant outer values (outer-query
-- variables and cypher() parameters) referenced by the body, bound to
-- PARAM_EXEC params 2, 3, ... It must be callable with a NULL transition state
-- (no initcond), so it is intentionally not STRICT.
CREATE FUNCTION ag_catalog.age_reduce_transfn(agtype, agtype, text, agtype, agtype[])
    RETURNS agtype
    LANGUAGE c
PARALLEL UNSAFE
AS 'MODULE_PATHNAME';

-- aggregate definition for reduce(); direct arguments are
-- (init, serialized-body, element, captured-outer-values), with the element
-- fed ORDER BY ordinality.
CREATE AGGREGATE ag_catalog.age_reduce(agtype, text, agtype, agtype[])
(
    stype = agtype,
    sfunc = ag_catalog.age_reduce_transfn
);
