package db

const INIT_EXTENSION = `
CREATE EXTENSION IF NOT EXISTS age;
LOAD 'age';
`
const SET_SEARCH_PATH = `SET search_path = ag_catalog, "$user", public;`
const META_DATA_11 = `SELECT label, cnt, kind FROM (
	SELECT c.relname AS label, n.oid as namespace_id, c.reltuples AS cnt
	FROM pg_catalog.pg_class c
	JOIN pg_catalog.pg_namespace n
	ON n.oid = c.relnamespace
	WHERE c.relkind = 'r'
	AND n.nspname = $1
) as q1
JOIN ag_graph as g ON q1.namespace_id = g.namespace
INNER JOIN ag_label as label

ON label.name = q1.label
AND label.graph = g.oid;`

const META_DATA_12 = `SELECT label, cnt, kind FROM (
	SELECT c.relname AS label, n.oid as namespace_id, c.reltuples AS cnt
	FROM pg_catalog.pg_class c
	JOIN pg_catalog.pg_namespace n
	ON n.oid = c.relnamespace
	WHERE c.relkind = 'r'
	AND n.nspname = $1
) as q1
JOIN ag_graph as g ON q1.namespace_id = g.namespace
INNER JOIN ag_label as label

ON label.name = q1.label
AND label.graph = g.graphid;`

const ANALYZE = `ANALYZE;`

const PG_VERSION = `show server_version_num;`
