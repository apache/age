/* -*-pgsql-c-*- */
/*
 *
 * $Header$
 *
 * pgpool: a language independent connection pool server for PostgreSQL
 * written by Tatsuo Ishii
 *
 * Copyright (c) 2003-2022	PgPool Global Development Group
 *
 * Permission to use, copy, modify, and distribute this software and
 * its documentation for any purpose and without fee is hereby
 * granted, provided that the above copyright notice appear in all
 * copies and that both that copyright notice and this permission
 * notice appear in supporting documentation, and that the name of the
 * author not be used in advertising or publicity pertaining to
 * distribution of the software without specific, written prior
 * permission. The author makes no representations about the
 * suitability of this software for any purpose.  It is provided "as
 * is" without express or implied warranty.
 *
 * pool_config.h.: pool_config.l related header file
 *
 */

#ifndef POOL_CONFIG_H
#define POOL_CONFIG_H

#include <sys/utsname.h>

#include "pcp/libpcp_ext.h"

/*
 * watchdog
 */
#define WD_MAX_HOST_NAMELEN MAX_FDQN_HOSTNAME_LEN
#define WD_MAX_NODE_NAMELEN (WD_MAX_HOST_NAMELEN + POOLCONFIG_MAXPORTLEN + sizeof(((struct utsname *)NULL)->sysname) + sizeof(((struct utsname *)NULL)->nodename) + 3)
#define WD_MAX_PATH_LEN (128)
#define MAX_WATCHDOG_NUM (128)
#define WD_SEND_TIMEOUT (1)
#define WD_MAX_IF_NUM (256)
#define WD_MAX_IF_NAME_LEN (16)

#include "utils/regex_array.h"
/*
 *  Regex support in write and readonly list function
 */
#include <regex.h>
#define WRITELIST	0
#define READONLYLIST	1
#define PATTERN_ARR_SIZE 16		/* Default length of regex array: 16 patterns */
typedef struct
{
	char	   *pattern;
	int			type;
	int			flag;
	regex_t		regexv;
}			RegPattern;

typedef enum ProcessManagementModes
{
	PM_STATIC = 1,
	PM_DYNAMIC
}			ProcessManagementModes;

typedef enum ProcessManagementSstrategies
{
	PM_STRATEGY_AGGRESSIVE = 1,
	PM_STRATEGY_GENTLE,
	PM_STRATEGY_LAZY
}			ProcessManagementSstrategies;

typedef enum NativeReplicationSubModes
{
	SLONY_MODE = 1,
	STREAM_MODE,
	LOGICAL_MODE
}			NativeReplicationSubModes;

typedef enum ClusteringModes
{
	CM_STREAMING_REPLICATION = 1,
	CM_NATIVE_REPLICATION,
	CM_LOGICAL_REPLICATION,
	CM_SLONY,
	CM_RAW,
	CM_SNAPSHOT_ISOLATION
}			ClusteringModes;

typedef enum LogStandbyDelayModes
{
	LSD_ALWAYS = 1,
	LSD_OVER_THRESHOLD,
	LSD_NONE
}			LogStandbyDelayModes;


typedef enum MemCacheMethod
{
	SHMEM_CACHE = 1,
	MEMCACHED_CACHE
}			MemCacheMethod;

typedef enum WdLifeCheckMethod
{
	LIFECHECK_BY_QUERY = 1,
	LIFECHECK_BY_HB,
	LIFECHECK_BY_EXTERNAL
}			WdLifeCheckMethod;

typedef enum DLBOW_OPTION
{
	DLBOW_OFF = 1,
	DLBOW_TRANSACTION,
	DLBOW_TRANS_TRANSACTION,
	DLBOW_ALWAYS,
	DLBOW_DML_ADAPTIVE
}			DLBOW_OPTION;

typedef enum RELQTARGET_OPTION
{
	RELQTARGET_PRIMARY = 1,
	RELQTARGET_LOAD_BALANCE_NODE
}			RELQTARGET_OPTION;

typedef enum CHECK_TEMP_TABLE_OPTION
{
	CHECK_TEMP_CATALOG = 1,
	CHECK_TEMP_TRACE,
	CHECK_TEMP_NONE,
	CHECK_TEMP_ON,
	CHECK_TEMP_OFF,
}			CHECK_TEMP_TABLE_OPTION;

/*
 * Flags for backendN_flag
 */
#define POOL_FAILOVER	(1 << 0)	/* allow or disallow failover */
#define POOL_ALWAYS_PRIMARY	(1 << 1)	/* this backend is always primary */
#define POOL_DISALLOW_TO_FAILOVER(x) ((unsigned short)(x) & POOL_FAILOVER)
#define POOL_ALLOW_TO_FAILOVER(x) (!(POOL_DISALLOW_TO_FAILOVER(x)))

/*
 * watchdog list
 */
typedef struct WdNodeInfo
{
	char		hostname[WD_MAX_HOST_NAMELEN];	/* host name */
	int			pgpool_port;	/* pgpool port */
	int			wd_port;		/* watchdog port */
}			WdNodeInfo;

typedef struct WdNodesConfig
{
	int			num_wd;			/* number of watchdogs */
	WdNodeInfo wd_node_info[MAX_WATCHDOG_NUM];
}			WdNodesConfig;


typedef struct
{
	char		addr[WD_MAX_HOST_NAMELEN];
	char		if_name[WD_MAX_IF_NAME_LEN];
	int			dest_port;
}			WdHbIf;

#define WD_INFO(wd_id) (pool_config->wd_nodes.wd_node_info[(wd_id)])
#define WD_HB_IF(if_id) (pool_config->hb_dest_if[(if_id)])

/*
 * Per node health check parameters
*/
typedef struct
{
	int			health_check_timeout;	/* health check timeout */
	int			health_check_period;	/* health check period */
	char	   *health_check_user;	/* PostgreSQL user name for health check */
	char	   *health_check_password;	/* password for health check username */
	char	   *health_check_database;	/* database name for health check
										 * username */
	int			health_check_max_retries;	/* health check max retries */
	int			health_check_retry_delay;	/* amount of time to wait between
											 * retries */
	int			connect_timeout;	/* timeout value before giving up
									 * connecting to backend */
}			HealthCheckParams;

/*
 * For dml adaptive object relations
 * Currently we only require functions
 * and relations
 *
 */
typedef enum
{
	OBJECT_TYPE_FUNCTION,
	OBJECT_TYPE_RELATION,
	OBJECT_TYPE_UNKNOWN
}		DBObjectTypes;

typedef struct
{
	char	*name;
	DBObjectTypes object_type;
}		DBObject;

typedef struct
{
	DBObject	left_token;
	DBObject	right_token;
}		DBObjectRelation;

/*
 * configuration parameters
 */
typedef struct
{
	ClusteringModes	backend_clustering_mode;	/* Backend clustering mode */
	ProcessManagementModes	process_management;
	ProcessManagementSstrategies process_management_strategy;
	char	   **listen_addresses;	/* hostnames/IP addresses to listen on */
	int			port;			/* port # to bind */
	char	   **pcp_listen_addresses;	/* PCP listen address to listen on */
	int			pcp_port;		/* PCP port # to bind */
	char	   **unix_socket_directories;		/* pgpool socket directories */
	char		*unix_socket_group;			/* owner group of pgpool sockets */
	int			unix_socket_permissions;	/* pgpool sockets permissions */
	char	   *wd_ipc_socket_dir;	/* watchdog command IPC socket directory */
	char	   *pcp_socket_dir; /* PCP socket directory */
	int			num_init_children;	/* Maximum number of child to
										 * accept connections */
	int			min_spare_children;		/* Minimum number of idle children */
	int			max_spare_children;		/* Minimum number of idle children */
	int			listen_backlog_multiplier;	/* determines the size of the
											 * connection queue */
	int			reserved_connections;	/* # of reserved connections */
	bool		serialize_accept;	/* if non 0, serialize call to accept() to
									 * avoid thundering herd problem */
	int			child_life_time;	/* if idle for this seconds, child exits */
	int			connection_life_time;	/* if idle for this seconds,
										 * connection closes */
	int			child_max_connections;	/* if max_connections received, child
										 * exits */
	int			client_idle_limit;	/* If client_idle_limit is n (n > 0), the
									 * client is forced to be disconnected
									 * after n seconds idle */
	bool		allow_clear_text_frontend_auth;

	/*
	 * enable Pgpool-II to use clear text password authentication between
	 * Pgpool and client to get the password when password for user does not
	 * exist in pool_password file.
	 */
	int			authentication_timeout; /* maximum time in seconds to complete
										 * client authentication */
	int			max_pool;		/* max # of connection pool per child */
	char	   *logdir;			/* logging directory */
	char	   *log_destination_str;	/* log destination: stderr and/or
										 * syslog */
	int			log_destination;	/* log destination */
	int			syslog_facility;	/* syslog facility: LOCAL0, LOCAL1, ... */
	char	   *syslog_ident;	/* syslog ident string: pgpool */
	char	   *pid_file_name;	/* pid file name */
	bool		replication_mode;	/* replication mode */
	bool		log_connections;	/* logs incoming connections */
	bool		log_disconnections;	/* logs closing connections */
	bool		log_hostname;	/* resolve hostname */
	bool		enable_pool_hba;	/* enables pool_hba.conf file
									 * authentication */
	char	   *pool_passwd;	/* pool_passwd file name. "" disables
								 * pool_passwd */
	bool		load_balance_mode;	/* load balance mode */

	bool		replication_stop_on_mismatch;	/* if there's a data mismatch
												 * between primary and
												 * secondary start
												 * degeneration to stop
												 * replication mode */
	bool		failover_if_affected_tuples_mismatch;	/* If there's a
														 * disagreement with the
														 * number of affected
														 * tuples in
														 * UPDATE/DELETE, then
														 * degenerate the node
														 * which is most likely
														 * "minority".  # If
														 * false, just abort the
														 * transaction to keep
														 * the consistency. */
	bool		auto_failback;	/* If true, backend node reattach,
								 * when backend node detached and
								 * replication_status is 'stream' */
	int			auto_failback_interval;	/* min interval of executing auto_failback */
	bool		replicate_select;	/* replicate SELECT statement when load
									 * balancing is disabled. */
	char	  **reset_query_list;	/* comma separated list of queries to be
									 * issued at the end of session */
	char	  **read_only_function_list;	/* list of functions with no side
										 * effects */
	char	  **write_function_list;	/* list of functions with side effects */
	char	  **primary_routing_query_pattern_list;	/* list of query patterns that
											 * should be sent to primary node */
	char	   *log_line_prefix;	/* printf-style string to output at
									 * beginning of each log line */
	int			log_error_verbosity;	/* controls how much detail about
										 * error should be emitted */
	int			client_min_messages;	/* controls which message should be
										 * sent to client */
	int			log_min_messages;	/* controls which message should be
									 * emitted to server log */
	/* log collector settings */
	bool		logging_collector;
	int			log_rotation_age;
	int			log_rotation_size;
	char		*log_directory;
	char		*log_filename;
	bool		log_truncate_on_rotation;
	int			log_file_mode;

	int64		delay_threshold;	/* If the standby server delays more than
									 * delay_threshold, any query goes to the
									 * primary only. The unit is in bytes. 0
									 * disables the check. Default is 0. Note
									 * that health_check_period required to be
									 * greater than 0 to enable the
									 * functionality. */

	int		delay_threshold_by_time;	/* If the standby server delays more than
										* delay_threshold_in_time, any query goes to the
										* primary only. The unit is in seconds. 0
										* disables the check. Default is 0.
										* If delay_threshold_in_time is greater than 0,
										* delay_threshold will be ignored.
										* Note that health_check_period required to be
										* greater than 0 to enable the
										* functionality. */

	bool		prefer_lower_delay_standby;

	LogStandbyDelayModes log_standby_delay; /* how to log standby lag */
	bool		connection_cache;	/* cache connection pool? */
	int			health_check_timeout;	/* health check timeout */
	int			health_check_period;	/* health check period */
	char	   *health_check_user;	/* PostgreSQL user name for health check */
	char	   *health_check_password;	/* password for health check username */
	char	   *health_check_database;	/* database name for health check
										 * username */
	int			health_check_max_retries;	/* health check max retries */
	int			health_check_retry_delay;	/* amount of time to wait between
											 * retries */
	int			connect_timeout;	/* timeout value before giving up
									 * connecting to backend */
	HealthCheckParams *health_check_params; /* per node health check
											 * parameters */
	int			sr_check_period;	/* streaming replication check period */
	char	   *sr_check_user;	/* PostgreSQL user name for streaming
								 * replication check */
	char	   *sr_check_password;	/* password for sr_check_user */
	char	   *sr_check_database;	/* PostgreSQL database name for streaming
									 * replication check */
	char	   *failover_command;	/* execute command when failover happens */
	char	   *follow_primary_command;	/* execute command when failover is
										 * ended */
	char	   *failback_command;	/* execute command when failback happens */

	bool		failover_on_backend_error; /* If true, trigger fail over when
											 * writing to the backend
											 * communication socket fails.
											 * This is the same behavior of
											 * pgpool-II 2.2.x or earlier. If
											 * set to false, pgpool will
											 * report an error and disconnect
											 * the session. */
	bool		failover_on_backend_shutdown; /* If true, trigger fail over
												 when backend is going down */
	bool		detach_false_primary;	/* If true, detach false primary */
	char	   *recovery_user;	/* PostgreSQL user name for online recovery */
	char	   *recovery_password;	/* PostgreSQL user password for online
									 * recovery */
	char	   *recovery_1st_stage_command; /* Online recovery command in 1st
											 * stage */
	char	   *recovery_2nd_stage_command; /* Online recovery command in 2nd
											 * stage */
	int			recovery_timeout;	/* maximum time in seconds to wait for
									 * remote start-up */
	int			search_primary_node_timeout;	/* maximum time in seconds to
												 * search for new primary node
												 * after failover */
	int			client_idle_limit_in_recovery;	/* If > 0, the client is
												 * forced to be disconnected
												 * after n seconds idle This
												 * parameter is only valid
												 * while in recovery 2nd stage */
	bool		insert_lock;	/* automatically locking of table with INSERT
								 * to keep SERIAL data consistency? */
	bool		ignore_leading_white_space; /* ignore leading white spaces of
											 * each query */
	bool		log_statement;	/* logs all SQL statements */
	bool		log_per_node_statement; /* logs per node detailed SQL
										 * statements */
	bool		log_client_messages;	/* If true, logs any client messages */
	char	   *lobj_lock_table;	/* table name to lock for rewriting
									 * lo_creat */

	BackendDesc *backend_desc;	/* PostgreSQL Server description. Placed on
								 * shared memory */

	LOAD_BALANCE_STATUS load_balance_status[MAX_NUM_BACKENDS];	/* to remember which DB
																 * node is selected for
																 * load balancing */

	/* followings till syslog, does not exist in the configuration file */
	int			num_reset_queries;	/* number of queries in reset_query_list */
	int			num_listen_addresses;	/* number of entries in listen_addresses */
	int			num_pcp_listen_addresses;	/* number of entries in pcp_listen_addresses */
	int			num_unix_socket_directories;	/* number of entries in unix_socket_directories */
	int			num_read_only_function_list;	/* number of functions in
											 * read_only_function_list */
	int			num_write_function_list;	/* number of functions in
											 * write_function_list */
	int			num_cache_safe_memqcache_table_list; /* number of functions in
												 * cache_safe_memqcache_table_list */
	int			num_cache_unsafe_memqcache_table_list; /* number of functions in
												 * cache_unsafe_memqcache_table_list */
	int			num_primary_routing_query_pattern_list;	/* number of query patterns in
												 * primary_routing_query_pattern_list */
	int			num_wd_monitoring_interfaces_list;	/* number of items in
													 * wd_monitoring_interfaces_list */
	/* ssl configuration */
	bool		ssl;			/* if non 0, activate ssl support
								 * (frontend+backend) */
	char	   *ssl_cert;		/* path to ssl certificate (frontend only) */
	char	   *ssl_key;		/* path to ssl key (frontend only) */
	char	   *ssl_ca_cert;	/* path to root (CA) certificate */
	char	   *ssl_ca_cert_dir;	/* path to directory containing CA
									 * certificates */
	char	   *ssl_crl_file;	/* path to the SSL certificate revocation list file */
	char	   *ssl_ciphers;	/* allowed ssl ciphers */
	bool		ssl_prefer_server_ciphers; /*Use SSL cipher preferences, rather than the client's*/
	char	   *ssl_ecdh_curve; /* the curve to use in ECDH key exchange */
	char	   *ssl_dh_params_file; /* path to the Diffie-Hellman parameters contained file */
	char	   *ssl_passphrase_command; /* path to the Diffie-Hellman parameters contained file */
	int64		relcache_expire;	/* relation cache life time in seconds */
	int			relcache_size;	/* number of relation cache life entry */
	CHECK_TEMP_TABLE_OPTION		check_temp_table;	/* how to check temporary table */
	bool		check_unlogged_table;	/* enable unlogged table check */
	bool		enable_shared_relcache;	/* If true, relation cache stored in memory cache */
	RELQTARGET_OPTION	relcache_query_target;	/* target node to send relcache queries */

	/*
	 * followings are for regex support and do not exist in the configuration
	 * file
	 */
	RegPattern *lists_patterns; /* Precompiled regex patterns for write/readonly
								 * lists */
	int			pattc;			/* number of regexp pattern */
	int			current_pattern_size;	/* size of the regex pattern array */

	RegPattern *lists_query_patterns;	/* Precompiled regex patterns for
										 * primary routing query pattern lists */
	int			query_pattc;	/* number of regexp pattern */
	int			current_query_pattern_size; /* size of the regex pattern array */

	bool		memory_cache_enabled;	/* if true, use the memory cache
										 * functionality, false by default */
	MemCacheMethod memqcache_method;	/* Cache store method. Either
										 * 'shmem'(shared memory) or
										 * 'memcached'. 'shmem' by default */
	char	   *memqcache_memcached_host;	/* Memcached host name. Mandatory
											 * if memqcache_method=memcached. */
	int			memqcache_memcached_port;	/* Memcached port number.
											 * Mandatory if
											 * memqcache_method=memcached. */
	int64		memqcache_total_size;	/* Total memory size in bytes for
										 * storing memory cache. Mandatory if
										 * memqcache_method=shmem. */
	int			memqcache_max_num_cache;	/* Total number of cache entries.
											 * Mandatory if
											 * memqcache_method=shmem. */
	int			memqcache_expire;	/* Memory cache entry life time specified
									 * in seconds. 60 by default. */
	bool		memqcache_auto_cache_invalidation;	/* If true, invalidation
													 * of query cache is
													 * triggered by
													 * corresponding */
	/* DDL/DML/DCL(and memqcache_expire).  If false, it is only triggered */
	/* by memqcache_expire.  True by default. */
	int			memqcache_maxcache; /* Maximum SELECT result size in bytes. */
	int			memqcache_cache_block_size; /* Cache block size in bytes. 8192
											 * by default */
	char	   *memqcache_oiddir;	/* Temporary work directory to record
									 * table oids */
	char	  **cache_safe_memqcache_table_list; /* list of tables to memqcache */
	char	  **cache_unsafe_memqcache_table_list; /* list of tables not to memqcache */

	RegPattern *lists_memqcache_table_patterns; /* Precompiled regex patterns
												 * for cache safe/unsafe lists */
	int			memqcache_table_pattc;	/* number of regexp pattern */
	int			current_memqcache_table_pattern_size;	/* size of the regex
														 * pattern array */

	/*
	 * database_redirect_preference_list =
	 * 'postgres:primary,mydb[0-4]:1,mydb[5-9]:2'
	 */
	char	   *database_redirect_preference_list;	/* raw string in
													 * pgpool.conf */
	RegArray   *redirect_dbnames;	/* Precompiled regex patterns for db
									 * preference list */
	Left_right_tokens *db_redirect_tokens;	/* db redirect for dbname and node
											 * string */

	/*
	 * app_name_redirect_preference_list =
	 * 'psql:primary,myapp[0-4]:1,myapp[5-9]:standby'
	 */
	char	   *app_name_redirect_preference_list;	/* raw string in
													 * pgpool.conf */
	RegArray   *redirect_app_names; /* Precompiled regex patterns for app name
									 * preference list */
	Left_right_tokens *app_name_redirect_tokens;	/* app name redirect for
													 * app_name and node
													 * string */

	bool		allow_sql_comments; /* if on, ignore SQL comments when judging
									 * if load balance or query cache is
									 * possible. If off, SQL comments
									 * effectively prevent the judgment (pre
									 * 3.4 behavior). For backward
									 * compatibility sake, default is off. */

	DLBOW_OPTION disable_load_balance_on_write; /* Load balance behavior when
												 * write query is issued in an
												 * explicit transaction. Note
												 * that any query not in an
												 * explicit transaction is not
												 * affected by the parameter.
												 * 'transaction' (the
												 * default): if a write query
												 * is issued, subsequent read
												 * queries will not be load
												 * balanced until the
												 * transaction ends.
												 * 'trans_transaction': if a
												 * write query is issued,
												 * subsequent read queries in
												 * an explicit transaction
												 * will not be load balanced
												 * until the session ends. */

	char	   *dml_adaptive_object_relationship_list;	/* objects relationship list*/
	DBObjectRelation *parsed_dml_adaptive_object_relationship_list;

	bool		statement_level_load_balance; /* if on, select load balancing node per statement */

	/*
	 * add for watchdog
	 */
	bool		use_watchdog;	/* Enables watchdog */
	bool		failover_when_quorum_exists;	/* Do failover only when wd
												 * cluster holds the quorum */
	bool		failover_require_consensus; /* Only do failover when majority
											 * aggrees */
	bool		allow_multiple_failover_requests_from_node; /* One Pgpool-II node
															 * can send multiple
															 * failover requests to
															 * build consensus */
	bool		enable_consensus_with_half_votes;
											/* apply majority rule for consensus
											 * and quorum computation at 50% of
											 * votes in a cluster with an even
											 * number of nodes.
											 */
	bool		wd_remove_shutdown_nodes;
											/* revoke membership of properly shutdown watchdog
											 * nodes.
											 */
	int 		wd_lost_node_removal_timeout;
											/* timeout in seconds to revoke membership of
											 * LOST watchdog nodes
											 */
	int 		wd_no_show_node_removal_timeout;
											/* time in seconds to revoke membership of
											 * NO-SHOW watchdog node
											 */

	WdLifeCheckMethod wd_lifecheck_method;	/* method of lifecheck.
											 * 'heartbeat' or 'query' */
	bool		clear_memqcache_on_escalation;	/* Clear query cache on shmem
												 * when escalating ? */
	char	   *wd_escalation_command;	/* Executes this command at escalation
										 * on new active pgpool. */
	char	   *wd_de_escalation_command;	/* Executes this command when
											 * leader pgpool goes down. */
	int			wd_priority;	/* watchdog node priority, during leader
								 * election */
	int			pgpool_node_id;	/* pgpool (watchdog) node id */
	WdNodesConfig wd_nodes;		/* watchdog lists */
	char	   *trusted_servers;	/* icmp reachable server list (A,B,C) */
	char	   *trusted_server_command;	/* Executes this command when upper servers are observed */
	char	   *delegate_ip;	/* delegate IP address */
	int			wd_interval;	/* lifecheck interval (sec) */
	char	   *wd_authkey;		/* Authentication key for watchdog
								 * communication */
	char	   *ping_path;		/* path to ping command */
	char	   *if_cmd_path;	/* path to interface up/down command */
	char	   *if_up_cmd;		/* ifup command */
	char	   *if_down_cmd;	/* ifdown command */
	char	   *arping_path;	/* path to arping command */
	char	   *arping_cmd;		/* arping command */
	int			wd_life_point;	/* life point (retry times at lifecheck) */
	char	   *wd_lifecheck_query; /* lifecheck query */
	char	   *wd_lifecheck_dbname;	/* Database name connected for
										 * lifecheck */
	char	   *wd_lifecheck_user;	/* PostgreSQL user name for watchdog */
	char	   *wd_lifecheck_password;	/* password for watchdog user */
	int			wd_heartbeat_port;	/* Port number for heartbeat lifecheck */
	int			wd_heartbeat_keepalive; /* Interval time of sending heartbeat
										 * signal (sec) */
	int			wd_heartbeat_deadtime;	/* Deadtime interval for heartbeat
										 * signal (sec) */
	WdHbIf		hb_ifs[WD_MAX_IF_NUM];		/* heartbeat interfaces of all watchdog nodes */
	WdHbIf		hb_dest_if[WD_MAX_IF_NUM];	/* heartbeat destination interfaces */
	int			num_hb_dest_if;				/* number of interface devices */
	char	  **wd_monitoring_interfaces_list;	/* network interface name list
												 * to be monitored by watchdog */
	bool		health_check_test;			/* if on, enable health check testing */

}			POOL_CONFIG;

extern POOL_CONFIG * pool_config;
extern char *config_file_dir; /* directory path of config file pgpool.conf */

typedef enum
{
	CFGCXT_BOOT,
	CFGCXT_INIT,
	CFGCXT_RELOAD,
	CFGCXT_PCP,
	CFGCXT_SESSION
}			ConfigContext;

typedef struct ConfigVariable
{
	char	   *name;
	char	   *value;
	int			sourceline;
	struct ConfigVariable *next;
} ConfigVariable;


extern int	pool_init_config(void);
extern bool pool_get_config(const char *config_file, ConfigContext context);
extern int	eval_logical(const char *str);
extern char *pool_flag_to_str(unsigned short flag);
extern char *backend_status_to_str(BackendInfo * bi);

/* methods used for regexp support */
extern int	add_regex_pattern(const char *type, char *s);
extern int	growFunctionPatternArray(RegPattern item);
extern int	growMemqcacheTablePatternArray(RegPattern item);
extern int	growQueryPatternArray(RegPattern item);

#endif							/* POOL_CONFIG_H */
