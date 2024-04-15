--
-- Cypher Query Language - DDL
--

-- setup

DROP ROLE IF EXISTS graph_role;
CREATE ROLE graph_role SUPERUSER;
SET ROLE graph_role;

--
-- CREATE GRAPH
--

SHOW graph_path;
CREATE GRAPH ddl;
SHOW graph_path;

CREATE GRAPH ddl;
CREATE GRAPH IF NOT EXISTS ddl;

-- check default graph objects
\dGl

--
-- ALTER GRAPH
--

CREATE ROLE temp;
ALTER GRAPH ddl RENAME TO p;
\dG
ALTER GRAPH p RENAME TO ddl;

ALTER GRAPH ddl OWNER TO temp;
\dG
ALTER GRAPH ddl OWNER TO graph_role;

-- ALTER GRAPH IF EXISTS is not supported

--
-- SET graph_path
--

SET graph_path = n;
SET graph_path = n, m;

--
-- CREATE label
--

CREATE VLABEL v0;

CREATE VLABEL v00 INHERITS (v0);
CREATE VLABEL v01 INHERITS (v0);
CREATE VLABEL v1 INHERITS (v00, v01);

CREATE ELABEL e0;
CREATE ELABEL e01 INHERITS (e0);
CREATE ELABEL e1;

SELECT labname, labkind FROM pg_catalog.ag_label;

SELECT child.labname AS child, parent.labname AS parent
FROM pg_catalog.ag_label AS parent,
     pg_catalog.ag_label AS child,
     pg_catalog.pg_inherits AS inh
WHERE child.relid = inh.inhrelid AND parent.relid = inh.inhparent
ORDER BY 1, 2;

-- IF NOT EXISTS

CREATE VLABEL v0;
CREATE VLABEL IF NOT EXISTS v0;

CREATE ELABEL e0;
CREATE ELABEL IF NOT EXISTS e0;

-- wrong cases

CREATE VLABEL wrong_parent INHERITS (e1);
CREATE ELABEL wrong_parent INHERITS (v1);

-- CREATE UNLOGGED
CREATE UNLOGGED VLABEL unlog;
SELECT l.labname AS name, c.relpersistence AS persistence
FROM pg_catalog.ag_label l
     LEFT JOIN pg_catalog.pg_class c ON c.oid = l.relid
ORDER BY 1;

-- WITH
CREATE VLABEL stor
WITH (fillfactor=90, autovacuum_enabled, autovacuum_vacuum_threshold=100);
SELECT l.labname AS name, c.reloptions AS options
FROM pg_catalog.ag_label l
     LEFT JOIN pg_catalog.pg_class c ON c.oid = l.relid
ORDER BY 1;

-- TABLESPACE
CREATE VLABEL tblspc TABLESPACE pg_default;

-- DISABLE INDEX
CREATE VLABEL vdi DISABLE INDEX;
\d ddl.vdi

-- REINDEX
REINDEX VLABEL vdi;
\d ddl.vdi

-- REINDEX wrong case
REINDEX ELABEL vdi;
REINDEX VLABEL ddl.vdi;

-- check default attstattarget of edge label
SELECT attname, attstattarget FROM pg_attribute
WHERE attrelid = 'ddl.e1'::regclass;

--
-- COMMENT and \dG commands
--

COMMENT ON GRAPH ddl IS 'a graph for regression tests';
COMMENT ON VLABEL v1 IS 'multiple inheritance test';

\dG+
\dGv+
\dGe+

--
-- ALTER LABEL
--

-- skip alter tablespace test, tablespace location must be an absolute path

ALTER VLABEL v0 SET STORAGE external;
\d+ ddl.v0

ALTER VLABEL v0 RENAME TO vv;
\dGv
ALTER VLABEL vv RENAME TO v0;

SELECT relname, rolname FROM pg_class c, pg_roles r
WHERE relname = 'v0' AND c.relowner = r.oid;
ALTER VLABEL v0 OWNER TO temp;

SELECT relname, rolname FROM pg_class c, pg_roles r
WHERE relname = 'v0' AND c.relowner = r.oid;
ALTER VLABEL v0 OWNER TO graph_role;
DROP ROLE temp;

SELECT indisclustered FROM pg_index WHERE indrelid = 'ddl.v0'::regclass;
ALTER VLABEL v0 CLUSTER ON v0_pkey;
SELECT indisclustered FROM pg_index WHERE indrelid = 'ddl.v0'::regclass ORDER BY indexrelid;
ALTER VLABEL v0 SET WITHOUT CLUSTER;
SELECT indisclustered FROM pg_index WHERE indrelid = 'ddl.v0'::regclass;

SELECT relpersistence FROM pg_class WHERE relname = 'v0';
ALTER VLABEL v0 SET UNLOGGED;
SELECT relpersistence FROM pg_class WHERE relname = 'v0';
ALTER VLABEL v0 SET LOGGED;
SELECT relpersistence FROM pg_class WHERE relname = 'v0';

\d ddl.v1
ALTER VLABEL v1 NO INHERIT v00;
\d ddl.v1
ALTER VLABEL v1 INHERIT v00;
\d ddl.v1
ALTER VLABEL v1 INHERIT ag_vertex;		--should fail
ALTER VLABEL v1 NO INHERIT ag_vertex;	--should fail

SELECT relreplident FROM pg_class WHERE relname = 'v0';
ALTER VLABEL v0 REPLICA IDENTITY full;
SELECT relreplident FROM pg_class WHERE relname = 'v0';
ALTER VLABEL v0 REPLICA IDENTITY default;

ALTER VLABEL vdi DISABLE INDEX;
\d ddl.vdi

-- IF EXISTS

ALTER VLABEL IF EXISTS v0 SET LOGGED;
ALTER VLABEL IF EXISTS unknown SET LOGGED;

--
-- DROP LABEL
--

-- wrong cases

DROP TABLE ddl.v1;
DROP TABLE ddl.e1;

DROP VLABEL unknown;
DROP ELABEL unknown;

DROP VLABEL IF EXISTS unknown;
DROP ELABEL IF EXISTS unknown;

DROP VLABEL e1;
DROP ELABEL v1;

DROP VLABEL IF EXISTS e1;
DROP ELABEL IF EXISTS v1;

DROP VLABEL v0;
DROP VLABEL v00;
DROP ELABEL e0;

DROP VLABEL ag_vertex CASCADE;
DROP ELABEL ag_edge CASCADE;

-- drop all

DROP VLABEL v01 CASCADE;
SELECT labname, labkind FROM pg_catalog.ag_label ORDER BY 2, 1;
DROP VLABEL v0 CASCADE;
DROP ELABEL e0 CASCADE;
DROP ELABEL e1;
SELECT labname, labkind FROM pg_catalog.ag_label;

-- drop label non-empty
CREATE (a:test_v1)-[:test_e1]->(b:test_v2), (a)<-[:test_e2]-(b);

DROP ELABEL test_e1;
DROP ELABEL test_e2 CASCADE;
DROP VLABEL test_v1;
DROP VLABEL test_v2 CASCADE;

MATCH (a:test_v1) DELETE (a);
DROP VLABEL test_v1;
DROP ELABEL test_e1;

--
-- CONSTRAINT
--
\set VERBOSITY terse

-- simple unique constraint
CREATE VLABEL regv1;

CREATE CONSTRAINT ON regv1 ASSERT a.b IS UNIQUE;
\dGv+ regv1

CREATE (:regv1 {a: {b: 'pg', c: 'graph'}});
CREATE (:regv1 {a: {b: 'pg', c: 'graph'}});
CREATE (:regv1 {a: {b: 'pg'}});
CREATE (:regv1 {a: {b: 'c'}});
CREATE (:regv1 {a: 'b'});
CREATE (:regv1 {a: 'pg-graph'});

-- expr unique constraint
CREATE ELABEL rege1;

CREATE CONSTRAINT ON rege1 ASSERT c + d IS UNIQUE;
\dGe+ rege1

CREATE ()-[:rege1 {c: 'pg', d: 'graph'}]->();
CREATE ()-[:rege1 {c: 'pg', d: 'graph'}]->();
CREATE ()-[:rege1 {c: 'pg', d: 'rdb'}]->();
CREATE ()-[:rege1 {c: 'p', d: 'ggraph'}]->();

-- simple not null constraint
CREATE VLABEL regv2;

CREATE CONSTRAINT ON regv2 ASSERT name IS NOT NULL;
\dGv+ regv2

CREATE (:regv2 {name: 'pg'});
CREATE (:regv2 {age: 0});
CREATE (:regv2 {age: 0, name: 'graph'});
CREATE (:regv2 {name: NULL});

-- multi not null constraint
CREATE VLABEL regv3;

CREATE CONSTRAINT ON regv3 ASSERT name.first IS NOT NULL AND name.last IS NOT NULL;
\dGv+ regv3

CREATE (:regv3 {name: 'pg'});
CREATE (:regv3 {name: {first: 'pg', last: 'graph'}});
CREATE (:regv3 {name: {first: 'pg'}});
CREATE (:regv3 {name: {last: 'graph'}});
CREATE (:regv3 {name: {first: NULL, last: NULL}});

-- simple check constraint
CREATE ELABEL rege2;

CREATE CONSTRAINT ON rege2 ASSERT a != b;
\dGe+ rege2

CREATE ()-[:rege2 {a: 'pg', b: 'graph'}]->();
CREATE ()-[:rege2 {a: 'pg', b: 'pg'}]->();
CREATE ()-[:rege2 {a: 'pg', b: 'PG'}]->();
CREATE ()-[:rege2 {a: 'pg', d: 'graph'}]->();

-- expression check constraint
CREATE VLABEL regv4;

CREATE CONSTRAINT ON regv4 ASSERT (length(password) > 8 AND length(password) < 16);
\dGv+ regv4

CREATE (:regv4 {password: '12345678'});
CREATE (:regv4 {password: '123456789'});
CREATE (:regv4 {password: '123456789012345'});
CREATE (:regv4 {password: '1234567890123456'});

-- IN check constraint
CREATE ELABEL rege3;

CREATE CONSTRAINT ON rege3 ASSERT type IN ['friend', 'lover', 'parent'];
\dGe+ rege3

CREATE ()-[:rege3 {type: 'friend', name: 'pg'}]->();
CREATE ()-[:rege3 {type: 'love', name: 'graph'}]->();
CREATE ()-[:rege3 {type: 'parents', name: 'PG'}]->();
CREATE ()-[:rege3 {type: 'lover', name: 'GRAPH'}]->();

-- case check constraint
CREATE VLABEL regv5;

CREATE CONSTRAINT ON regv5 ASSERT toLower(trim(id)) IS UNIQUE;
\dGv+ regv5

CREATE (:regv5 {id: 'pg'});
CREATE (:regv5 {id: ' pg'});
CREATE (:regv5 {id: 'pg '});
CREATE (:regv5 {id: 'PG'});
CREATE (:regv5 {id: ' PG '});
CREATE (:regv5 {id: 'GRAPH'});
CREATE (:regv5 {id: ' graph '});

-- IS NULL constraint
CREATE ELABEL rege4;

CREATE CONSTRAINT rege4_name_isnull_constraint ON rege4 ASSERT id IS NULL;
\dGe+ rege4

CREATE ()-[:rege4 {id: NULL, name: 'pg'}]->();
CREATE ()-[:rege4 {id: 10, name: 'pg'}]->();
CREATE ()-[:rege4 {name: 'graph'}]->();

DROP CONSTRAINT rege4_name_isnull_constraint ON ag_edge;
DROP CONSTRAINT ON rege4;
DROP CONSTRAINT rege4_name_isnull_constraint ON rege4;

-- Indirection constraint

CREATE VLABEL regv7;

CREATE CONSTRAINT ON regv7 ASSERT a.b[0].c IS NOT NULL;
\dGv+ regv7

CREATE (:regv7 {a: {b: [{c: 'd'}, {c: 'e'}]}});
CREATE (:regv7 {a: {b: [{c: 'd'}, {e: 'e'}]}});
CREATE (:regv7 {a: {b: [{d: 'd'}, {e: 'e'}]}});

-- wrong case

CREATE VLABEL regv8;

CREATE CONSTRAINT ON regv8 ASSERT (SELECT * FROM graph.regv8).c IS NOT NULL;
CREATE CONSTRAINT ON regv8 ASSERT (1).c IS NOT NULL;
CREATE CONSTRAINT ON regv8 ASSERT ($1).c IS NOT NULL;

--
-- DROP GRAPH
--
DROP GRAPH ddl;
DROP GRAPH unknown;
DROP GRAPH IF EXISTS unknown;

-- teardown

RESET ROLE;
