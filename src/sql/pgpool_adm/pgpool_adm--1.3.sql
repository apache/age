/* contrib/pgpool_adm/pgpool_adm--1.3.sql */

/* ***********************************************
 * Administrative functions for pgPool
 * *********************************************** */

/**
 * input parameters: node_id, host, port, username, password
 */
CREATE FUNCTION pcp_node_info(IN node_id integer, IN host text, IN port integer, IN username text, IN password text, OUT host text, OUT port integer, OUT status text, OUT weight float4, OUT role text, OUT replication_delay bigint, OUT replication_state text, OUT replication_sync_state text, OUT last_status_change timestamp)
RETURNS record
AS 'MODULE_PATHNAME', '_pcp_node_info'
LANGUAGE C VOLATILE STRICT;

/**
 * input parameters: node_id, pcp_server
 */
CREATE FUNCTION pcp_node_info(IN node_id integer, IN pcp_server text, OUT host text, OUT port integer, OUT status text, OUT weight float4)
RETURNS record
AS 'MODULE_PATHNAME', '_pcp_node_info'
LANGUAGE C VOLATILE STRICT;

/**
 * input parameters: node_id, host, port, username, password
 */
CREATE FUNCTION pcp_health_check_stats(IN node_id integer, IN host text, IN port integer, IN username text, IN password text, OUT node_id integer, OUT host text, OUT port integer, OUT status text, OUT role text, OUT last_status_change timestamp, OUT total_count bigint, OUT success_count bigint, OUT fail_count bigint, OUT skip_count bigint, OUT retry_count bigint, OUT average_retry_count float4, OUT max_retry_count bigint, OUT max_health_check_duration bigint, OUT min_health_check_duration bigint, OUT average_health_check_duration float4, OUT last_health_check timestamp, OUT last_successful_health_check timestamp, OUT last_skip_health_check timestamp, OUT last_failed_health_check timestamp)
RETURNS record
AS 'MODULE_PATHNAME', '_pcp_health_check_stats'
LANGUAGE C VOLATILE STRICT;

/**
 * input parameters: node_id, pcp_server
 */
CREATE FUNCTION pcp_health_check_stats(IN node_id integer, IN pcp_server text, OUT host text, OUT port integer, OUT status text, OUT weight float4)
RETURNS record
AS 'MODULE_PATHNAME', '_pcp_health_check_stats'
LANGUAGE C VOLATILE STRICT;

/**
 * input parameters: host, port, username, password
 */
CREATE FUNCTION pcp_pool_status(IN host text, IN port integer, IN username text, IN password text, OUT item text, OUT value text, OUT description text)
RETURNS record
AS 'MODULE_PATHNAME', '_pcp_pool_status'
LANGUAGE C VOLATILE STRICT;

/**
 * input parameters: pcp_server
 */
CREATE FUNCTION pcp_pool_status(IN pcp_server text, OUT item text, OUT value text, OUT description text)
RETURNS record
AS 'MODULE_PATHNAME', '_pcp_pool_status'
LANGUAGE C VOLATILE STRICT;

/**
 * input parameters: host, port, username, password
 */
CREATE FUNCTION pcp_node_count(IN host text, IN port integer, IN username text, IN password text, OUT node_count integer)
RETURNS integer
AS 'MODULE_PATHNAME', '_pcp_node_count'
LANGUAGE C VOLATILE STRICT;

/**
 * input parameters: pcp_server
 */
CREATE FUNCTION pcp_node_count(IN pcp_server text, OUT node_count integer)
RETURNS integer
AS 'MODULE_PATHNAME', '_pcp_node_count'
LANGUAGE C VOLATILE STRICT;

/**
 * input parameters: node_id, host, port, username, password
 */
CREATE FUNCTION pcp_attach_node(IN node_id integer, IN host text, IN port integer, IN username text, IN password text, OUT node_attached boolean)
RETURNS boolean
AS 'MODULE_PATHNAME', '_pcp_attach_node'
LANGUAGE C VOLATILE STRICT;

/**
 * input parameters: node_id, pcp_server
 */
CREATE FUNCTION pcp_attach_node(IN node_id integer, IN pcp_server text, OUT node_attached boolean)
RETURNS boolean
AS 'MODULE_PATHNAME', '_pcp_attach_node'
LANGUAGE C VOLATILE STRICT;

/**
 * input parameters: node_id, gracefully, host, port, username, password
 */
CREATE FUNCTION pcp_detach_node(IN node_id integer, IN gracefully boolean, IN host text, IN port integer, IN username text, IN password text, OUT node_detached boolean)
RETURNS boolean
AS 'MODULE_PATHNAME', '_pcp_detach_node'
LANGUAGE C VOLATILE STRICT;

/**
 * input parameters: node_id, gracefully, pcp_server
 */
CREATE FUNCTION pcp_detach_node(IN node_id integer, IN gracefully boolean, IN pcp_server text, OUT node_detached boolean)
RETURNS boolean
AS 'MODULE_PATHNAME', '_pcp_detach_node'
LANGUAGE C VOLATILE STRICT;
