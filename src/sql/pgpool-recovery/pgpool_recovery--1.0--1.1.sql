/* contrib/pgpool_recovery/pgpool_recovery--1.0--1.1.sql */

-- complain if script is sourced in psql, rather than via ALTER EXTENSION
\echo Use "ALTER EXTENSION pgpool_recovery UPDATE TO '1.1'" to load this file. \quit

create FUNCTION pgpool_recovery(script_name text, remote_host text, remote_data_directory text, remote_port text)
RETURNS bool
AS 'MODULE_PATHNAME', 'pgpool_recovery'
LANGUAGE C STRICT;
