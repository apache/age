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
\echo Use "ALTER EXTENSION age UPDATE TO '1.7.0'" to load this file. \quit

CREATE FUNCTION ag_catalog._ag_enforce_edge_uniqueness2(graphid, graphid)
    RETURNS bool
    LANGUAGE c
    STABLE
PARALLEL SAFE
as 'MODULE_PATHNAME';

CREATE FUNCTION ag_catalog._ag_enforce_edge_uniqueness3(graphid, graphid, graphid)
    RETURNS bool
    LANGUAGE c
    STABLE
PARALLEL SAFE
as 'MODULE_PATHNAME';

CREATE FUNCTION ag_catalog._ag_enforce_edge_uniqueness4(graphid, graphid, graphid, graphid)
    RETURNS bool
    LANGUAGE c
    STABLE
PARALLEL SAFE
as 'MODULE_PATHNAME';

-- Create indexes on id columns for existing labels
-- Vertex labels get PRIMARY KEY on id, Edge labels get indexes on start_id/end_id
DO $$
DECLARE
    label_rec record;
    schema_name text;
    table_name text;
    idx_exists boolean;
    pk_exists boolean;
    idx_name text;
BEGIN
    FOR label_rec IN
        SELECT l.relation, l.kind
        FROM ag_catalog.ag_label l
    LOOP
        SELECT n.nspname, c.relname INTO schema_name, table_name
        FROM pg_class c
        JOIN pg_namespace n ON c.relnamespace = n.oid
        WHERE c.oid = label_rec.relation;

        IF label_rec.kind = 'e' THEN
            -- Edge: check/create index on start_id
            SELECT EXISTS (
                SELECT 1 FROM pg_index i
                JOIN pg_class c ON c.oid = i.indexrelid
                JOIN pg_am am ON am.oid = c.relam
                JOIN pg_attribute a ON a.attrelid = i.indrelid AND a.attnum = i.indkey[0]
                WHERE i.indrelid = label_rec.relation
                  AND a.attname = 'start_id'
                  AND i.indpred IS NULL              -- not a partial index
                  AND i.indexprs IS NULL             -- not an expression index
                  AND am.amname = 'btree'            -- btree access method
            ) INTO idx_exists;

            IF NOT idx_exists THEN
                EXECUTE format('CREATE INDEX %I ON %I.%I USING btree (start_id)',
                    table_name || '_start_id_idx', schema_name, table_name);
            END IF;

            -- Edge: check/create index on end_id
            SELECT EXISTS (
                SELECT 1 FROM pg_index i
                JOIN pg_class c ON c.oid = i.indexrelid
                JOIN pg_am am ON am.oid = c.relam
                JOIN pg_attribute a ON a.attrelid = i.indrelid AND a.attnum = i.indkey[0]
                WHERE i.indrelid = label_rec.relation
                  AND a.attname = 'end_id'
                  AND i.indpred IS NULL              -- not a partial index
                  AND i.indexprs IS NULL             -- not an expression index
                  AND am.amname = 'btree'            -- btree access method
            ) INTO idx_exists;

            IF NOT idx_exists THEN
                EXECUTE format('CREATE INDEX %I ON %I.%I USING btree (end_id)',
                    table_name || '_end_id_idx', schema_name, table_name);
            END IF;
        ELSE
            -- Vertex: check/create PRIMARY KEY on id
            SELECT EXISTS (
                SELECT 1 FROM pg_constraint
                WHERE conrelid = label_rec.relation AND contype = 'p'
            ) INTO pk_exists;

            IF NOT pk_exists THEN
                -- Check if a usable unique index on id already exists
                SELECT c.relname INTO idx_name
                FROM pg_index i
                JOIN pg_class c ON c.oid = i.indexrelid
                JOIN pg_am am ON am.oid = c.relam
                WHERE i.indrelid = label_rec.relation
                  AND i.indisunique = true
                  AND i.indpred IS NULL              -- not a partial index
                  AND i.indexprs IS NULL             -- not an expression index
                  AND am.amname = 'btree'            -- btree access method
                  AND i.indnkeyatts = 1              -- single column index
                  AND EXISTS (
                      SELECT 1 FROM pg_attribute a
                      WHERE a.attrelid = i.indrelid
                        AND a.attnum = i.indkey[0]
                        AND a.attname = 'id'
                  )
                LIMIT 1;

                IF idx_name IS NOT NULL THEN
                    -- Reuse existing unique index for primary key
                    EXECUTE format('ALTER TABLE %I.%I ADD CONSTRAINT %I PRIMARY KEY USING INDEX %I',
                        schema_name, table_name, table_name || '_pkey', idx_name);
                ELSE
                    -- Create new primary key
                    EXECUTE format('ALTER TABLE %I.%I ADD PRIMARY KEY (id)',
                        schema_name, table_name);
                END IF;
            END IF;
        END IF;
    END LOOP;
END $$;
