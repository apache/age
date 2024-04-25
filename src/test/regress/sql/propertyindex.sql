--
-- Cypher Query Language - Property Index
--
DROP ROLE IF EXISTS regressrole;
CREATE ROLE regressrole SUPERUSER;
SET ROLE regressrole;

--
-- CREATE GRAPH
--

SHOW graph_path;
CREATE GRAPH propidx;
SHOW graph_path;


CREATE VLABEL piv1;

CREATE PROPERTY INDEX ON piv1 (name);
CREATE PROPERTY INDEX ON piv1 (name.first, name.last);
CREATE PROPERTY INDEX ON piv1 ((name.first + name.last));
CREATE PROPERTY INDEX ON piv1 (age);
CREATE PROPERTY INDEX ON piv1 ((body.weight / body.height));

\d propidx.piv1
\dGi piv1*

-- Check property name & access method type
CREATE VLABEL piv2;

CREATE PROPERTY INDEX ON piv2 (name);
CREATE PROPERTY INDEX ON piv2 USING btree (name.first);
CREATE PROPERTY INDEX ON piv2 USING hash (name.first);
CREATE PROPERTY INDEX ON piv2 USING brin (name.first);
CREATE PROPERTY INDEX ON piv2 USING gin (name);
CREATE PROPERTY INDEX ON piv2 USING gist (name);

--CREATE PROPERTY INDEX ON piv2 USING gin ((self_intro::tsvector));
--CREATE PROPERTY INDEX ON piv2 USING gist ((hobby::tsvector));

\d propidx.piv2
\dGv+ piv2
\dGi piv2*

-- Concurrently build & if not exist
CREATE VLABEL piv3;

CREATE PROPERTY INDEX CONCURRENTLY ON piv3 (name.first);
CREATE PROPERTY INDEX IF NOT EXISTS piv3_first_idx ON piv3 (name.first);

-- Collation & Sort & NULL order
--CREATE PROPERTY INDEX ON piv3 (name.first COLLATE "C" ASC NULLS FIRST);

-- Tablespace
CREATE PROPERTY INDEX ON piv3 (name) TABLESPACE pg_default;

-- Storage parameter & partial index
CREATE PROPERTY INDEX ON piv3 (name.first) WITH (fillfactor = 80);
CREATE PROPERTY INDEX ON piv3 (name.first) WHERE (name IS NOT NULL);

\d propidx.piv3
\dGv+ piv3
\dGi piv3*

-- Unique property index
CREATE VLABEL piv4;

CREATE UNIQUE PROPERTY INDEX ON piv4 (id);
CREATE (:piv4 {id: 100});
CREATE (:piv4 {id: 100});

\d propidx.piv4
\dGv+ piv4
\dGi piv4*

-- Multi-column unique property index
CREATE VLABEL piv5;

CREATE UNIQUE PROPERTY INDEX ON piv5 (name.first, name.last);
CREATE (:piv5 {name: {first: 'pg'}});
CREATE (:piv5 {name: {first: 'pg'}});
CREATE (:piv5 {name: {first: 'pg', last: 'graph'}});
CREATE (:piv5 {name: {first: 'pg', last: 'graph'}});

\d propidx.piv5
\dGv+ piv5
\dGi piv5*

-- DROP PROPERTY INDEX
CREATE VLABEL piv6;

CREATE PROPERTY INDEX piv6_idx ON piv6 (name);

DROP PROPERTY INDEX piv6_idx;
DROP PROPERTY INDEX IF EXISTS piv6_idx;
DROP PROPERTY INDEX piv6_pkey;

DROP VLABEL piv6;

CREATE ELABEL pie1;
CREATE PROPERTY INDEX pie1_idx ON pie1 (reltype);

DROP PROPERTY INDEX pie1_idx;
DROP PROPERTY INDEX IF EXISTS pie1_idx;
DROP PROPERTY INDEX pie1_id_idx;
DROP PROPERTY INDEX pie1_start_idx;
DROP PROPERTY INDEX pie1_end_idx;

DROP ELABEL pie1;

CREATE VLABEL piv7;

CREATE PROPERTY INDEX piv7_multi_col ON piv7 (name.first, name.middle, name.last);
\dGv+ piv7
\dGi piv7*
DROP PROPERTY INDEX piv7_multi_col;

CREATE PROPERTY INDEX piv7_multi_expr ON piv7 ((name.first + name.last), age);
\dGv+ piv7
\dGi piv7*
DROP PROPERTY INDEX piv7_multi_expr;

DROP VLABEL piv7;

-- wrong case
CREATE VLABEL piv8;

CREATE PROPERTY INDEX piv8_index_key1 ON piv8 (key1);
CREATE PROPERTY INDEX piv8_index_key1 ON piv8 (key1);

CREATE PROPERTY INDEX ON nonexsist_name (key1);

DROP VLABEL piv8;

CREATE VLABEL piv9;

CREATE PROPERTY INDEX piv9_property_index_key1 ON piv9 (key1);
DROP INDEX propidx.piv9_property_index_key1;

CREATE INDEX piv9_index_key1 ON propidx.piv9 (properties);
DROP PROPERTY INDEX piv9_index_key1;

DROP VLABEL piv9;

-- teardown

RESET ROLE;
