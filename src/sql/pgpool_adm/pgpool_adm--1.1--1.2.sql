/* contrib/pgpool_adm/pgpool_adm--1.1--1.2.sql */

-- complain if script is sourced in psql, rather than via ALTER EXTENSION
\echo Use "ALTER EXTENSION pgpool_adm UPDATE TO '1.1'" to load this file. \quit

CREATE FUNCTION pcp_health_check_stats(integer, text, integer, text, text, OUT host text, OUT port integer, OUT status text, OUT role text, OUT last_status_change timestamp, OUT total_count bigint, OUT fail_count bigint, OUT skip_count bigint, OUT retry_count bigint, OUT average_retry_count float4, OUT max_retry_count bigint, OUT max_health_check_duration bigint, OUT min_health_check_duration bigint, OUT average_health_check_duration float4, OUT last_health_check timestamp, OUT last_successful_health_check timestamp, OUT last_skip_health_check timestamp, OUT last_failed_health_check timestamp);
RETURNS record
AS 'MODULE_PATHNAME', '_pcp_health_check_stats'
LANGUAGE C VOLATILE STRICT;
