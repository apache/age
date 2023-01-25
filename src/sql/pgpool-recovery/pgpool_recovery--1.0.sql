-- complain if script is sourced in psql, rather than via CREATE EXTENSION
\echo Use "CREATE EXTENSION pgpool_recovery" to load this file. \quit

CREATE FUNCTION pgpool_recovery(IN script_name text,
	   IN remote_host text,
	   IN remote_data_directory text)
RETURNS bool
AS 'MODULE_PATHNAME', 'pgpool_recovery'
LANGUAGE C STRICT;

CREATE FUNCTION pgpool_remote_start(IN remote_host text, IN remote_data_directory text)
RETURNS bool
AS 'MODULE_PATHNAME', 'pgpool_remote_start'
LANGUAGE C STRICT;

CREATE FUNCTION pgpool_pgctl(IN action text, IN stop_mode text)
RETURNS bool
AS '$libdir/pgpool-recovery', 'pgpool_pgctl'
LANGUAGE C STRICT;

CREATE FUNCTION pgpool_switch_xlog(IN archive_dir text)
RETURNS text
AS 'MODULE_PATHNAME', 'pgpool_switch_xlog'
LANGUAGE C STRICT;
