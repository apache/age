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

SET enable_mergejoin = ON;
SET enable_hashjoin = ON;
SET enable_nestloop = ON;
SET enable_seqscan = false;

SELECT create_graph('cypher_index');

/*
 * Section 1: Unique Index on Properties
 */
--Section 1 Setup
SELECT create_vlabel('cypher_index', 'idx');
CREATE UNIQUE INDEX cypher_index_idx_props_uq ON cypher_index.idx(properties);

--Test 1
SELECT * FROM cypher('cypher_index', $$ CREATE (:idx {i: 1}) $$) AS (a agtype);
SELECT * FROM cypher('cypher_index', $$ CREATE (:idx {i: 1}) $$) AS (a agtype);

--Clean Up
SELECT * FROM cypher('cypher_index', $$ MATCH(n) DETACH DELETE n $$) AS (a agtype);

--Test 2
SELECT * FROM cypher('cypher_index', $$ CREATE (:idx {i: 1}), (:idx {i: 1}) $$) AS (a agtype);

--Clean Up
SELECT * FROM cypher('cypher_index', $$ MATCH(n) DETACH DELETE n $$) AS (a agtype);

--Test 3
--Data Setup
SELECT * FROM cypher('cypher_index', $$ CREATE (:idx {i: 1}) $$) AS (a agtype);
SELECT * FROM cypher('cypher_index', $$ CREATE (:idx) $$) AS (a agtype);

--Query
SELECT * FROM cypher('cypher_index', $$ MATCH(n) SET n.i = 1$$) AS (a agtype);

--Clean Up
SELECT * FROM cypher('cypher_index', $$ MATCH(n) DETACH DELETE n $$) AS (a agtype);

--Test 4
--create a vertex with i = 1
SELECT * FROM cypher('cypher_index', $$ CREATE (:idx {i: 1}) $$) AS (a agtype);

--delete the vertex
SELECT * FROM cypher('cypher_index', $$ MATCH(n) DETACH DELETE n $$) AS (a agtype);

--we should be able to create a new vertex with the same value
SELECT * FROM cypher('cypher_index', $$ CREATE (:idx {i: 1}) $$) AS (a agtype);

--data cleanup
SELECT * FROM cypher('cypher_index', $$ MATCH(n) DETACH DELETE n $$) AS (a agtype);

/*
 * Test 5
 *
 * Same queries as Test 4, only in 1 transaction
 */
BEGIN TRANSACTION;
--create a vertex with i = 1
SELECT * FROM cypher('cypher_index', $$ CREATE (:idx {i: 1}) $$) AS (a agtype);

--delete the vertex
SELECT * FROM cypher('cypher_index', $$ MATCH(n) DETACH DELETE n $$) AS (a agtype);

--we should be able to create a new vertex with the same value
SELECT * FROM cypher('cypher_index', $$ CREATE (:idx {i: 1}) $$) AS (a agtype);

COMMIT;

--data cleanup
SELECT * FROM cypher('cypher_index', $$ MATCH(n) DETACH DELETE n $$) AS (a agtype);


--Test 6
--create a vertex with i = 1
SELECT * FROM cypher('cypher_index', $$ CREATE (:idx {i: 1}) $$) AS (a agtype);

-- change the value
SELECT * FROM cypher('cypher_index', $$ MATCH(n) SET n.i = 2 $$) AS (a agtype);

--we should be able to create a new vertex with the same value
SELECT * FROM cypher('cypher_index', $$ CREATE (:idx {i: 1}) $$) AS (a agtype);

--validate the data
SELECT * FROM cypher('cypher_index', $$ MATCH(n) RETURN n $$) AS (a agtype);

--data cleanup
SELECT * FROM cypher('cypher_index', $$ MATCH(n) DETACH DELETE n $$) AS (a agtype);

/*
 * Test 7
 *
 * Same queries as Test 6, only in 1 transaction
 */
BEGIN TRANSACTION;
--create a vertex with i = 1
SELECT * FROM cypher('cypher_index', $$ CREATE (:idx {i: 1}) $$) AS (a agtype);

-- change the value
SELECT * FROM cypher('cypher_index', $$ MATCH(n) SET n.i = 2 $$) AS (a agtype);

--we should be able to create a new vertex with the same value
SELECT * FROM cypher('cypher_index', $$ CREATE (:idx {i: 1}) $$) AS (a agtype);

--validate the data
SELECT * FROM cypher('cypher_index', $$ MATCH(n) RETURN n $$) AS (a agtype);

COMMIT;

--validate the data again out of the transaction, just in case
SELECT * FROM cypher('cypher_index', $$ MATCH(n) RETURN n $$) AS (a agtype);

--data cleanup
SELECT * FROM cypher('cypher_index', $$ MATCH(n) DETACH DELETE n $$) AS (a agtype);


--Test 8
--create a vertex with i = 1
SELECT * FROM cypher('cypher_index', $$ CREATE (:idx {i: 1}) $$) AS (a agtype);

-- Use Merge and force an index error
SELECT * FROM cypher('cypher_index', $$ MATCH(n) MERGE (n)-[:e]->(:idx {i: n.i}) $$) AS (a agtype);

--data cleanup
SELECT * FROM cypher('cypher_index', $$ MATCH(n) DETACH DELETE n $$) AS (a agtype);

/*
 * Section 2: Graphid Indices to Improve Join Performance
 */
SELECT * FROM cypher('cypher_index', $$
    CREATE (us:Country {name: "United States", country_code: "US", life_expectancy: 78.79, gdp: 20.94::numeric}),
        (ca:Country {name: "Canada", country_code: "CA", life_expectancy: 82.05, gdp: 1.643::numeric}),
        (mx:Country {name: "Mexico", country_code: "MX", life_expectancy: 75.05, gdp: 1.076::numeric}),
        (us)<-[:has_city]-(:City {city_id: 1, name:"New York", west_coast: false, country_code:"US"}),
        (us)<-[:has_city]-(:City {city_id: 2, name:"San Fransisco", west_coast: true, country_code:"US"}),
        (us)<-[:has_city]-(:City {city_id: 3, name:"Los Angeles", west_coast: true, country_code:"US"}),
        (us)<-[:has_city]-(:City {city_id: 4, name:"Seattle", west_coast: true, country_code:"US"}),
        (ca)<-[:has_city]-(:City {city_id: 5, name:"Vancouver", west_coast: true, country_code:"CA"}),
        (ca)<-[:has_city]-(:City {city_id: 6, name:"Toronto", west_coast: false, country_code:"CA"}),
        (ca)<-[:has_city]-(:City {city_id: 7, name:"Montreal", west_coast: false, country_code:"CA"}),
        (mx)<-[:has_city]-(:City {city_id: 8, name:"Mexico City", west_coast: false, country_code:"MX"}),
        (mx)<-[:has_city]-(:City {city_id: 9, name:"Monterrey", west_coast: false, country_code:"MX"}),
        (mx)<-[:has_city]-(:City {city_id: 10, name:"Tijuana", west_coast: false, country_code:"MX"})
$$) as (n agtype);

-- Verify that the incices are created on id columns
SELECT indexname, indexdef FROM pg_indexes WHERE schemaname= 'cypher_index' ORDER BY 1;

SET enable_mergejoin = ON;
SET enable_hashjoin = OFF;
SET enable_nestloop = OFF;

SELECT COUNT(*) FROM cypher('cypher_index', $$
    MATCH (a:Country)<-[e:has_city]-()
    RETURN e
$$) as (n agtype);

SELECT COUNT(*) FROM cypher('cypher_index', $$
    EXPLAIN (costs off) MATCH (a:Country)<-[e:has_city]-()
    RETURN e
$$) as (n agtype);

SET enable_mergejoin = OFF;
SET enable_hashjoin = ON;
SET enable_nestloop = OFF;

SELECT COUNT(*) FROM cypher('cypher_index', $$
    MATCH (a:Country)<-[e:has_city]-()
    RETURN e
$$) as (n agtype);

SELECT COUNT(*) FROM cypher('cypher_index', $$
    EXPLAIN (costs off) MATCH (a:Country)<-[e:has_city]-()
    RETURN e
$$) as (n agtype);

SET enable_mergejoin = OFF;
SET enable_hashjoin = OFF;
SET enable_nestloop = ON;

SELECT COUNT(*) FROM cypher('cypher_index', $$
    EXPLAIN (costs off) MATCH (a:Country)<-[e:has_city]-()
    RETURN e
$$) as (n agtype);

SET enable_mergejoin = ON;
SET enable_hashjoin = ON;
SET enable_nestloop = ON;

--
-- Section 3: Agtype GIN Indices to Improve WHERE clause Performance
--
CREATE INDEX load_city_gin_idx
ON cypher_index."City" USING gin (properties);

CREATE INDEX load_country_gin_idx
ON cypher_index."Country" USING gin (properties);

-- Verify GIN index is used for City property match
SELECT * FROM cypher('cypher_index', $$
    EXPLAIN (costs off) MATCH (c:City {city_id: 1})
    RETURN c
$$) as (plan agtype);

SELECT * FROM cypher('cypher_index', $$
    MATCH (c:City {city_id: 1})
    RETURN c
$$) as (n agtype);

SELECT * FROM cypher('cypher_index', $$
    MATCH (:Country {country_code: "US"})<-[]-(city:City)
    RETURN city
$$) as (n agtype);

SELECT * FROM cypher('cypher_index', $$
    MATCH (c:City {west_coast: true})
    RETURN c
$$) as (n agtype);

-- Verify GIN index is used for Country property match
SELECT * FROM cypher('cypher_index', $$
    EXPLAIN (costs off) MATCH (c:Country {life_expectancy: 82.05})
    RETURN c
$$) as (plan agtype);

SELECT * FROM cypher('cypher_index', $$
    MATCH (c:Country {life_expectancy: 82.05})
    RETURN c
$$) as (n agtype);

SELECT * FROM cypher('cypher_index', $$
    MATCH (c:Country {gdp: 20.94::numeric})
    RETURN c
$$) as (n agtype);

DROP INDEX cypher_index.load_city_gin_idx;
DROP INDEX cypher_index.load_country_gin_idx;
--
-- Section 4: Index use with WHERE clause
--
-- Create expression index on country_code property
CREATE INDEX city_country_code_idx ON cypher_index."City"
(ag_catalog.agtype_access_operator(properties, '"country_code"'::agtype));

-- Verify index is used with EXPLAIN (should show Index Scan on city_country_code_idx)
SELECT * FROM cypher('cypher_index', $$
    EXPLAIN (costs off) MATCH (a:City)
    WHERE a.country_code = 'US'
    RETURN a
$$) as (plan agtype);

-- Test WHERE with indexed string property
SELECT * FROM cypher('cypher_index', $$
    MATCH (a:City)
    WHERE a.country_code = 'US'
    RETURN a.name
    ORDER BY a.city_id
$$) as (name agtype);

SELECT * FROM cypher('cypher_index', $$
    MATCH (a:City)
    WHERE a.country_code = 'CA'
    RETURN a.name
    ORDER BY a.city_id
$$) as (name agtype);

-- Test WHERE with no matching results
SELECT * FROM cypher('cypher_index', $$
    MATCH (a:City)
    WHERE a.country_code = 'XX'
    RETURN a.name
$$) as (name agtype);

-- Create expression index on city_id property
CREATE INDEX city_id_idx ON cypher_index."City"
(ag_catalog.agtype_access_operator(properties, '"city_id"'::agtype));

-- Verify index is used with EXPLAIN for integer property
SELECT * FROM cypher('cypher_index', $$
    EXPLAIN (costs off) MATCH (a:City)
    WHERE a.city_id = 1
    RETURN a
$$) as (plan agtype);

-- Test WHERE with indexed integer property
SELECT * FROM cypher('cypher_index', $$
    MATCH (a:City)
    WHERE a.city_id = 1
    RETURN a.name
$$) as (name agtype);

SELECT * FROM cypher('cypher_index', $$
    MATCH (a:City)
    WHERE a.city_id = 5
    RETURN a.name
$$) as (name agtype);

-- Test WHERE with comparison operators on indexed property
SELECT * FROM cypher('cypher_index', $$
    MATCH (a:City)
    WHERE a.city_id < 3
    RETURN a.name
    ORDER BY a.city_id
$$) as (name agtype);

SELECT * FROM cypher('cypher_index', $$
    MATCH (a:City)
    WHERE a.city_id >= 8
    RETURN a.name
    ORDER BY a.city_id
$$) as (name agtype);

-- Create expression index on west_coast boolean property
CREATE INDEX city_west_coast_idx ON cypher_index."City"
(ag_catalog.agtype_access_operator(properties, '"west_coast"'::agtype));

-- Verify index is used with EXPLAIN for boolean property
SELECT * FROM cypher('cypher_index', $$
    EXPLAIN (costs off) MATCH (a:City)
    WHERE a.west_coast = true
    RETURN a
$$) as (plan agtype);

-- Test WHERE with indexed boolean property
SELECT * FROM cypher('cypher_index', $$
    MATCH (a:City)
    WHERE a.west_coast = true
    RETURN a.name
    ORDER BY a.city_id
$$) as (name agtype);

SELECT * FROM cypher('cypher_index', $$
    MATCH (a:City)
    WHERE a.west_coast = false
    RETURN a.name
    ORDER BY a.city_id
$$) as (name agtype);

-- EXPLAIN for pattern with WHERE clause
SELECT * FROM cypher('cypher_index', $$
    EXPLAIN (costs off) MATCH (a:City)
    WHERE a.country_code = 'US' AND a.west_coast = true
    RETURN a
$$) as (plan agtype);

-- Test WHERE with multiple conditions (AND)
SELECT * FROM cypher('cypher_index', $$
    MATCH (a:City)
    WHERE a.country_code = 'US' AND a.west_coast = true
    RETURN a.name
    ORDER BY a.city_id
$$) as (name agtype);

-- Test WHERE with OR conditions
SELECT * FROM cypher('cypher_index', $$
    MATCH (a:City)
    WHERE a.city_id = 1 OR a.city_id = 5
    RETURN a.name
    ORDER BY a.city_id
$$) as (name agtype);

-- Test WHERE with NOT
SELECT * FROM cypher('cypher_index', $$
    MATCH (a:City)
    WHERE NOT a.west_coast = true AND a.country_code = 'US'
    RETURN a.name
$$) as (name agtype);

-- Create expression index on life_expectancy for Country
CREATE INDEX country_life_exp_idx ON cypher_index."Country"
(ag_catalog.agtype_access_operator(properties, '"life_expectancy"'::agtype));

-- Verify index is used with EXPLAIN for float property
SELECT * FROM cypher('cypher_index', $$
    EXPLAIN (costs off) MATCH (c:Country)
    WHERE c.life_expectancy > 80.0
    RETURN c
$$) as (plan agtype);

-- Test WHERE with float property
SELECT * FROM cypher('cypher_index', $$
    MATCH (c:Country)
    WHERE c.life_expectancy > 80.0
    RETURN c.name
$$) as (name agtype);

SELECT * FROM cypher('cypher_index', $$
    MATCH (c:Country)
    WHERE c.life_expectancy < 76.0
    RETURN c.name
$$) as (name agtype);

-- EXPLAIN for pattern with filters on both country and city
SELECT * FROM cypher('cypher_index', $$
    EXPLAIN (costs off) MATCH (country:Country)<-[:has_city]-(city:City)
    WHERE country.country_code = 'CA' AND city.west_coast = true
    RETURN city.name
$$) as (plan agtype);

-- Test WHERE in combination with pattern matching
SELECT * FROM cypher('cypher_index', $$
    MATCH (country:Country)<-[:has_city]-(city:City)
    WHERE country.country_code = 'CA'
    RETURN city.name
    ORDER BY city.city_id
$$) as (name agtype);

-- Clean up indices
DROP INDEX cypher_index.city_country_code_idx;
DROP INDEX cypher_index.city_id_idx;
DROP INDEX cypher_index.city_west_coast_idx;
DROP INDEX cypher_index.country_life_exp_idx;

--
-- General Cleanup
--
SELECT drop_graph('cypher_index', true);
