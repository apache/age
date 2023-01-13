-- complain if script is sourced in psql, rather than via CREATE EXTENSION
\echo Use "CREATE EXTENSION pgpool_regclass" to load this file. \quit

CREATE FUNCTION pg_catalog.pgpool_regclass(IN expression cstring)
RETURNS oid
AS 'MODULE_PATHNAME', 'pgpool_regclass'
LANGUAGE C STRICT;
