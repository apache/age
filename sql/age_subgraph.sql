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
-- create_subgraph(): materialized subgraph extraction.
--
-- Builds a new, persistent AGE graph that is the subgraph of an existing graph
-- selected by a node predicate and a relationship predicate. The semantics
-- follow the graph-theory "induced subgraph" definition as operationalized by
-- Neo4j GDS gds.graph.filter():
--
--   * a vertex is kept iff node_filter evaluates true ('*' keeps all);
--   * an edge is kept iff relationship_filter evaluates true AND BOTH of its
--     endpoints were kept (the induced rule -- no dangling edges).
--
-- Unlike the Neo4j in-memory projection, the result is a real, ACID,
-- fully-Cypher-queryable AGE graph; properties of any agtype are preserved, and
-- self-loops / parallel edges (multigraph structure) are kept.
--
-- node_filter / relationship_filter are Cypher predicates bound to a single
-- entity -- the node variable is `n`, the relationship variable is `r` -- or
-- the literal '*' to keep all. They are evaluated by AGE's own Cypher engine
-- against the source graph, so the full Cypher predicate language is available.
--
-- Internal entity ids (graphids) are reassigned in the new graph (a graphid
-- encodes the source graph's label id, which differs in the destination), and
-- edge endpoints are remapped accordingly. Properties are copied verbatim.
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
            '       ag_catalog._graphid(%s, nextval(%L)) AS new_id, '
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
            'SELECT ag_catalog._graphid(%s, nextval(%L)) AS new_id, '
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
