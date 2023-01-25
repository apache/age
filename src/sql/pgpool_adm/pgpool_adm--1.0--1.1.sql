/* contrib/pgpool_adm/pgpool_adm--1.0--1.1.sql */

-- complain if script is sourced in psql, rather than via ALTER EXTENSION
\echo Use "ALTER EXTENSION pgpool_adm UPDATE TO '1.1'" to load this file. \quit

ALTER EXTENSION pgpool_adm DROP FUNCTION pcp_node_info(integer, text, integer, text, text, OUT host text, OUT port integer, OUT status text, OUT weight float4);
DROP FUNCTION pcp_node_info(integer, text, integer, text, text, OUT host text, OUT port integer, OUT status text, OUT weight float4);

CREATE FUNCTION pcp_node_info(integer, text, integer, text, text, OUT host text, OUT port integer, OUT status text, OUT weight float4, OUT role text, OUT replication_delay bigint, OUT last_status_change timestamp)
RETURNS record
AS 'MODULE_PATHNAME', '_pcp_node_info'
LANGUAGE C VOLATILE STRICT;
