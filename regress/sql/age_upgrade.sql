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
-- Extension upgrade regression test
--
-- This test validates the upgrade template (age--<VER>--y.y.y.sql) by:
--   1. Dropping AGE and reinstalling at the synthetic "initial" version
--      (built from the version-bump commit's SQL — the "day-one" state)
--   2. Creating three graphs with multiple labels, edges, GIN indexes,
--      and numeric properties that serve as integrity checksums
--   3. Upgrading to the current (default) version via the stamped template
--   4. Verifying all data, structure, and checksums survived the upgrade
--
-- The Makefile builds:
--   age--<CURR>.sql            from current HEAD's sql/sql_files (default)
--   age--<CURR>_initial.sql    from the version-bump commit (synthetic)
--   age--<CURR>_initial--<CURR>.sql  stamped from the upgrade template
--
-- All version discovery is dynamic — no hardcoded versions anywhere.
-- This test is version-agnostic and works on any branch for any version.
--

LOAD 'age';
SET search_path TO ag_catalog;

-- Step 1: Clean up any state left by prior tests, then drop AGE entirely.
-- The --load-extension=age flag installed AGE at the current (default) version.
-- We need to remove it so we can reinstall at the synthetic initial version.
SELECT drop_graph(name, true) FROM ag_graph ORDER BY name;
DROP EXTENSION age;

-- Step 2: Verify we have multiple installable versions.
SELECT count(*) > 1 AS has_upgrade_path
FROM pg_available_extension_versions WHERE name = 'age';

-- Step 3: Install AGE at the synthetic initial version (pre-upgrade state).
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
SELECT extversion IS NOT NULL AS version_installed FROM pg_extension WHERE extname = 'age';

-- Step 4: Create three test graphs with diverse labels, edges, and data.
LOAD 'age';
SET search_path TO ag_catalog, "$user", public;

--
-- Graph 1: "company" — organization hierarchy with numeric checksums.
-- Labels: Employee, Department, Project
-- Edges: WORKS_IN, MANAGES, ASSIGNED_TO
-- Each vertex has a "val" property (float) for checksum validation.
--
SELECT create_graph('company');

SELECT * FROM cypher('company', $$
    CREATE (e1:Employee {name: 'Alice',   role: 'VP',        val: 3.14159})
    CREATE (e2:Employee {name: 'Bob',     role: 'Manager',   val: 2.71828})
    CREATE (e3:Employee {name: 'Charlie', role: 'Engineer',  val: 1.41421})
    CREATE (e4:Employee {name: 'Diana',   role: 'Engineer',  val: 1.73205})
    CREATE (d1:Department {name: 'Engineering', budget: 500000, val: 42.0})
    CREATE (d2:Department {name: 'Research',    budget: 300000, val: 17.5})
    CREATE (p1:Project {name: 'Atlas',   priority: 1, val: 99.99})
    CREATE (p2:Project {name: 'Beacon',  priority: 2, val: 88.88})
    CREATE (p3:Project {name: 'Cipher',  priority: 3, val: 77.77})
    CREATE (e1)-[:WORKS_IN {since: 2019}]->(d1)
    CREATE (e2)-[:WORKS_IN {since: 2020}]->(d1)
    CREATE (e3)-[:WORKS_IN {since: 2021}]->(d1)
    CREATE (e4)-[:WORKS_IN {since: 2022}]->(d2)
    CREATE (e1)-[:MANAGES {level: 1}]->(e2)
    CREATE (e2)-[:MANAGES {level: 2}]->(e3)
    CREATE (e3)-[:ASSIGNED_TO {hours: 40}]->(p1)
    CREATE (e3)-[:ASSIGNED_TO {hours: 20}]->(p2)
    CREATE (e4)-[:ASSIGNED_TO {hours: 30}]->(p2)
    CREATE (e4)-[:ASSIGNED_TO {hours: 10}]->(p3)
    RETURN 'company graph created'
$$) AS (result agtype);

-- GIN index on Employee properties in company graph
CREATE INDEX company_employee_gin ON company."Employee" USING GIN (properties);

--
-- Graph 2: "network" — social network with weighted edges.
-- Labels: User, Post
-- Edges: FOLLOWS, AUTHORED, LIKES
--
SELECT create_graph('network');

SELECT * FROM cypher('network', $$
    CREATE (u1:User {handle: '@alpha',   score: 1000.01})
    CREATE (u2:User {handle: '@beta',    score: 2000.02})
    CREATE (u3:User {handle: '@gamma',   score: 3000.03})
    CREATE (u4:User {handle: '@delta',   score: 4000.04})
    CREATE (u5:User {handle: '@epsilon', score: 5000.05})
    CREATE (p1:Post {title: 'Hello World',        views: 150})
    CREATE (p2:Post {title: 'Graph Databases 101', views: 890})
    CREATE (p3:Post {title: 'AGE is awesome',      views: 2200})
    CREATE (u1)-[:FOLLOWS {weight: 0.9}]->(u2)
    CREATE (u2)-[:FOLLOWS {weight: 0.8}]->(u3)
    CREATE (u3)-[:FOLLOWS {weight: 0.7}]->(u4)
    CREATE (u4)-[:FOLLOWS {weight: 0.6}]->(u5)
    CREATE (u5)-[:FOLLOWS {weight: 0.5}]->(u1)
    CREATE (u1)-[:AUTHORED]->(p1)
    CREATE (u2)-[:AUTHORED]->(p2)
    CREATE (u3)-[:AUTHORED]->(p3)
    CREATE (u4)-[:LIKES]->(p1)
    CREATE (u5)-[:LIKES]->(p2)
    CREATE (u1)-[:LIKES]->(p3)
    CREATE (u2)-[:LIKES]->(p3)
    RETURN 'network graph created'
$$) AS (result agtype);

-- GIN indexes on network graph
CREATE INDEX network_user_gin ON network."User" USING GIN (properties);
CREATE INDEX network_post_gin ON network."Post" USING GIN (properties);

--
-- Graph 3: "routes" — geographic routing with precise coordinates.
-- Labels: City, Airport
-- Edges: ROAD, FLIGHT
-- Coordinates use precise decimals that are easy to checksum.
--
SELECT create_graph('routes');

SELECT * FROM cypher('routes', $$
    CREATE (c1:City    {name: 'Portland',   lat: 45.5152, lon: -122.6784, pop: 652503})
    CREATE (c2:City    {name: 'Seattle',    lat: 47.6062, lon: -122.3321, pop: 749256})
    CREATE (c3:City    {name: 'Vancouver',  lat: 49.2827, lon: -123.1207, pop: 631486})
    CREATE (a1:Airport {code: 'PDX', elev: 30.5})
    CREATE (a2:Airport {code: 'SEA', elev: 131.7})
    CREATE (a3:Airport {code: 'YVR', elev: 4.3})
    CREATE (c1)-[:ROAD    {distance_km: 279.5,  toll: 0.0}]->(c2)
    CREATE (c2)-[:ROAD    {distance_km: 225.3,  toll: 5.0}]->(c3)
    CREATE (c1)-[:ROAD    {distance_km: 502.1,  toll: 5.0}]->(c3)
    CREATE (a1)-[:FLIGHT  {distance_km: 229.0, duration_min: 55}]->(a2)
    CREATE (a2)-[:FLIGHT  {distance_km: 198.0, duration_min: 50}]->(a3)
    CREATE (a1)-[:FLIGHT  {distance_km: 426.0, duration_min: 75}]->(a3)
    RETURN 'routes graph created'
$$) AS (result agtype);

-- GIN index on routes graph
CREATE INDEX routes_city_gin ON routes."City" USING GIN (properties);

-- Step 5: Record pre-upgrade integrity checksums.
-- These sums use the "val" / "score" / coordinate properties as fingerprints.

-- company: sum of all val properties (should be a precise known value)
SELECT * FROM cypher('company', $$
    MATCH (n) WHERE n.val IS NOT NULL RETURN sum(n.val)
$$) AS (company_val_sum_before agtype);

-- network: sum of all score properties
SELECT * FROM cypher('network', $$
    MATCH (n:User) RETURN sum(n.score)
$$) AS (network_score_sum_before agtype);

-- routes: sum of all latitude values
SELECT * FROM cypher('routes', $$
    MATCH (c:City) RETURN sum(c.lat)
$$) AS (routes_lat_sum_before agtype);

-- Total vertex and edge counts across all three graphs
SELECT sum(cnt)::int AS total_vertices_before FROM (
    SELECT count(*) AS cnt FROM cypher('company', $$ MATCH (n) RETURN n $$) AS (n agtype)
    UNION ALL
    SELECT count(*) FROM cypher('network', $$ MATCH (n) RETURN n $$) AS (n agtype)
    UNION ALL
    SELECT count(*) FROM cypher('routes', $$ MATCH (n) RETURN n $$) AS (n agtype)
) sub;

SELECT sum(cnt)::int AS total_edges_before FROM (
    SELECT count(*) AS cnt FROM cypher('company', $$ MATCH ()-[e]->() RETURN e $$) AS (e agtype)
    UNION ALL
    SELECT count(*) FROM cypher('network', $$ MATCH ()-[e]->() RETURN e $$) AS (e agtype)
    UNION ALL
    SELECT count(*) FROM cypher('routes', $$ MATCH ()-[e]->() RETURN e $$) AS (e agtype)
) sub;

-- Count of distinct labels (ag_label entries) across all graphs
SELECT count(*)::int AS total_labels_before
FROM ag_label al JOIN ag_graph ag ON al.graph = ag.graphid
WHERE ag.name <> '_ag_catalog';

-- Step 6: Upgrade AGE from the initial version to the current (default) version
-- via the stamped upgrade template.
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

-- Step 7: Confirm version is now the default (current HEAD) version.
SELECT installed_version = default_version AS upgraded_to_current
FROM pg_available_extensions WHERE name = 'age';

-- Step 8: Verify all data survived — reload and recheck.
LOAD 'age';
SET search_path TO ag_catalog, "$user", public;

-- Repeat integrity checksums — must match pre-upgrade values exactly.
SELECT * FROM cypher('company', $$
    MATCH (n) WHERE n.val IS NOT NULL RETURN sum(n.val)
$$) AS (company_val_sum_after agtype);

SELECT * FROM cypher('network', $$
    MATCH (n:User) RETURN sum(n.score)
$$) AS (network_score_sum_after agtype);

SELECT * FROM cypher('routes', $$
    MATCH (c:City) RETURN sum(c.lat)
$$) AS (routes_lat_sum_after agtype);

SELECT sum(cnt)::int AS total_vertices_after FROM (
    SELECT count(*) AS cnt FROM cypher('company', $$ MATCH (n) RETURN n $$) AS (n agtype)
    UNION ALL
    SELECT count(*) FROM cypher('network', $$ MATCH (n) RETURN n $$) AS (n agtype)
    UNION ALL
    SELECT count(*) FROM cypher('routes', $$ MATCH (n) RETURN n $$) AS (n agtype)
) sub;

SELECT sum(cnt)::int AS total_edges_after FROM (
    SELECT count(*) AS cnt FROM cypher('company', $$ MATCH ()-[e]->() RETURN e $$) AS (e agtype)
    UNION ALL
    SELECT count(*) FROM cypher('network', $$ MATCH ()-[e]->() RETURN e $$) AS (e agtype)
    UNION ALL
    SELECT count(*) FROM cypher('routes', $$ MATCH ()-[e]->() RETURN e $$) AS (e agtype)
) sub;

SELECT count(*)::int AS total_labels_after
FROM ag_label al JOIN ag_graph ag ON al.graph = ag.graphid
WHERE ag.name <> '_ag_catalog';

-- Step 9: Verify specific structural queries across all three graphs.

-- company: management chain
SELECT * FROM cypher('company', $$
    MATCH (boss:Employee)-[:MANAGES*]->(report:Employee)
    RETURN boss.name, report.name
    ORDER BY boss.name, report.name
$$) AS (boss agtype, report agtype);

-- network: circular follow chain (proves full cycle survived)
SELECT * FROM cypher('network', $$
    MATCH (a:User)-[:FOLLOWS]->(b:User)
    RETURN a.handle, b.handle
    ORDER BY a.handle
$$) AS (follower agtype, followed agtype);

-- routes: all flights with distances (proves edge properties intact)
SELECT * FROM cypher('routes', $$
    MATCH (a:Airport)-[f:FLIGHT]->(b:Airport)
    RETURN a.code, b.code, f.distance_km
    ORDER BY a.code, b.code
$$) AS (origin agtype, dest agtype, dist agtype);

-- Step 10: Verify GIN indexes still exist after upgrade.
SELECT indexname FROM pg_indexes
WHERE schemaname IN ('company', 'network', 'routes')
  AND tablename IN ('Employee', 'User', 'Post', 'City')
  AND indexdef LIKE '%gin%'
ORDER BY indexname;

-- Step 11: Cleanup and restore AGE at the default version for subsequent tests.
SELECT drop_graph('routes', true);
SELECT drop_graph('network', true);
SELECT drop_graph('company', true);
DROP EXTENSION age;
CREATE EXTENSION age;

-- Step 12: Remove synthetic upgrade test files from the extension directory.
\! sh ./regress/age_upgrade_cleanup.sh
