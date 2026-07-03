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

/*
 * Regression test for issue #2450.
 *
 * A GENERATED ALWAYS ... STORED column (or any user-added column) on a label
 * table adds an attribute AGE's CREATE/MERGE/SET paths do not populate. The
 * slot's tuple descriptor is the full relation descriptor, so heap_form_tuple()
 * used to read the uninitialized slot entry and segfault. AGE now null-inits the
 * entity slot and computes stored generated columns before materializing the
 * tuple. Generated columns must be computed on insert and recomputed on update;
 * plain user columns must default to NULL rather than crash.
 */

LOAD 'age';
SET search_path TO ag_catalog;

SELECT create_graph('generated_columns');

--
-- GENERATED ALWAYS ... STORED column on a vertex label
--
SELECT create_vlabel('generated_columns', 'Product');
ALTER TABLE generated_columns."Product"
    ADD COLUMN category varchar(25)
    GENERATED ALWAYS AS (agtype_access_operator(properties, '"category"')) STORED;

-- CREATE: the generated column is computed from the new properties
SELECT * FROM cypher('generated_columns', $$
    CREATE (p:Product {category: 'disk', type: 'M1234'}) RETURN p
$$) AS (p agtype);

-- CREATE where the generation expression yields NULL (no such key)
SELECT * FROM cypher('generated_columns', $$
    CREATE (p:Product {type: 'no-cat'}) RETURN p
$$) AS (p agtype);

-- MERGE also computes the generated column
SELECT * FROM cypher('generated_columns', $$
    MERGE (p:Product {category: 'merged'}) RETURN p
$$) AS (p agtype);

-- MERGE again on the same pattern must MATCH (not re-insert); this exercises the
-- matched path where the slot is intentionally left uncleared (issue #2450)
SELECT * FROM cypher('generated_columns', $$
    MERGE (p:Product {category: 'merged'}) RETURN p
$$) AS (p agtype);

-- SET recomputes the generated column from the updated properties
SELECT * FROM cypher('generated_columns', $$
    MATCH (p:Product {category: 'disk'}) SET p.category = 'ssd', p.note = 'x' RETURN p
$$) AS (p agtype);

-- The stored generated column always mirrors properties.category
SELECT category, properties FROM generated_columns."Product" ORDER BY category NULLS LAST;

--
-- GENERATED ALWAYS ... STORED column on an edge label
--
SELECT create_elabel('generated_columns', 'REL');
ALTER TABLE generated_columns."REL"
    ADD COLUMN kind varchar(25)
    GENERATED ALWAYS AS (agtype_access_operator(properties, '"kind"')) STORED;

SELECT * FROM cypher('generated_columns', $$
    CREATE (:Product {category: 'a'})-[r:REL {kind: 'link'}]->(:Product {category: 'b'})
    RETURN r
$$) AS (r agtype);

SELECT kind FROM generated_columns."REL";

--
-- Plain (non-generated) user column: must not crash; defaults to NULL
--
SELECT create_vlabel('generated_columns', 'Plain');
ALTER TABLE generated_columns."Plain" ADD COLUMN tag text;

SELECT * FROM cypher('generated_columns', $$
    CREATE (p:Plain {n: 1}) RETURN p
$$) AS (p agtype);

SELECT * FROM cypher('generated_columns', $$
    MATCH (p:Plain) SET p.n = 2 RETURN p
$$) AS (p agtype);

SELECT properties, tag FROM generated_columns."Plain";

--
-- Generated column referencing the tableoid system column: the entity slot must
-- carry tts_tableOid before the generated columns are computed, otherwise the
-- expression sees InvalidOid instead of the label table's OID (issue #2450).
--
SELECT create_vlabel('generated_columns', 'T');
ALTER TABLE generated_columns."T"
    ADD COLUMN tbl oid GENERATED ALWAYS AS (tableoid) STORED;

SELECT * FROM cypher('generated_columns', $$
    CREATE (n:T {k: 1}) RETURN n
$$) AS (n agtype);

SELECT * FROM cypher('generated_columns', $$
    MATCH (n:T) SET n.k = 2 RETURN n
$$) AS (n agtype);

-- tbl must equal the real label-table OID on both the CREATE and SET paths
SELECT tbl = 'generated_columns."T"'::regclass::oid AS tableoid_ok
FROM generated_columns."T";

--
-- Cleanup
--
SELECT drop_graph('generated_columns', true);
