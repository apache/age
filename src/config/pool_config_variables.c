/* -*-pgsql-c-*- */
/*
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
 * pool_config_variables.c.
 *
 */

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <ctype.h>
#include <unistd.h>
#include <limits.h>
#include <math.h>

#include "pool.h"
#include "pool_config.h"
#include "pool_config_variables.h"
#include "utils/regex_array.h"

#ifndef POOL_PRIVATE
#include "utils/elog.h"
#include "parser/stringinfo.h"
#include "utils/pool_process_reporting.h"
#include "utils/pool_stream.h"
#include "utils/palloc.h"
#include "utils/memutils.h"
#include "watchdog/wd_utils.h"

#else
#include "utils/fe_ports.h"
#endif

#define default_reset_query_list	"ABORT;DISCARD ALL"
#define default_listen_addresses_list	"localhost"
#define default_pcp_listen_addresses_list	"localhost"
#define default_unix_socket_directories_list	"/tmp"
#define default_read_only_function_list ""
#define default_write_function_list ""

#define EMPTY_CONFIG_GENERIC {NULL, 0, 0, NULL, 0, false, 0, 0, 0, 0, NULL, NULL}
#define EMPTY_CONFIG_BOOL {EMPTY_CONFIG_GENERIC, NULL, false, NULL, NULL, NULL, false}
#define EMPTY_CONFIG_INT {EMPTY_CONFIG_GENERIC, NULL, 0, 0, 0, NULL, NULL, NULL, 0}
#define EMPTY_CONFIG_DOUBLE {EMPTY_CONFIG_GENERIC, NULL, 0, 0, 0, NULL, NULL, NULL, 0}
#define EMPTY_CONFIG_LONG {EMPTY_CONFIG_GENERIC, NULL, 0, 0, 0, NULL, NULL, NULL, 0}
#define EMPTY_CONFIG_STRING {EMPTY_CONFIG_GENERIC, NULL, NULL, NULL, NULL, NULL, NULL, NULL}
#define EMPTY_CONFIG_ENUM {EMPTY_CONFIG_GENERIC, NULL, 0, NULL, NULL, NULL, NULL, NULL, 0}
#define EMPTY_CONFIG_INT_ARRAY {EMPTY_CONFIG_GENERIC, NULL, 0, 0, 0, EMPTY_CONFIG_INT, NULL, NULL, NULL, NULL, NULL}
#define EMPTY_CONFIG_DOUBLE_ARRAY {EMPTY_CONFIG_GENERIC, NULL, 0, 0, 0, EMPTY_CONFIG_DOUBLE, NULL, NULL, NULL, NULL, NULL}
#define EMPTY_CONFIG_STRING_ARRAY {EMPTY_CONFIG_GENERIC, NULL, NULL, EMPTY_CONFIG_STRING, NULL, NULL, NULL, NULL, NULL}
#define EMPTY_CONFIG_STRING_LIST {EMPTY_CONFIG_GENERIC, NULL, NULL, NULL, NULL, false, NULL, NULL, NULL, NULL, NULL}
#define EMPTY_CONFIG_GROUP_ARRAY {EMPTY_CONFIG_GENERIC, 0, NULL}

extern POOL_CONFIG g_pool_config;
struct config_generic **all_parameters = NULL;
static int	num_all_parameters = 0;

static void initialize_variables_with_default(struct config_generic *gconf);
static bool config_enum_lookup_by_name(struct config_enum *record, const char *value, int *retval);

static void build_variable_groups(void);
static void build_config_variables(void);
static void initialize_config_gen(struct config_generic *gen);

static struct config_generic *find_option(const char *name, int elevel);

static bool config_post_processor(ConfigContext context, int elevel);

static void sort_config_vars(void);
static bool setConfigOptionArrayVarWithConfigDefault(struct config_generic *record, const char *name,
										 const char *value, ConfigContext context, int elevel);

static bool setConfigOption(const char *name, const char *value,
				ConfigContext context, GucSource source, int elevel);
static bool setConfigOptionVar(struct config_generic *record, const char *name, int index_val,
				   const char *value, ConfigContext context, GucSource source, int elevel);

static bool get_index_in_var_name(struct config_generic *record,
					  const char *name, int *index, int elevel);


static bool MakeDBRedirectListRegex(char *newval, int elevel);
static bool MakeAppRedirectListRegex(char *newval, int elevel);
static bool MakeDMLAdaptiveObjectRelationList(char *newval, int elevel);
static char* getParsedToken(char *token, DBObjectTypes *object_type);

static bool check_redirect_node_spec(char *node_spec);
static char **get_list_from_string(const char *str, const char *delimi, int *n);
static char **get_list_from_string_regex_delim(const char *str, const char *delimi, int *n);


/*show functions */
static const char *IntValueShowFunc(int value);
static const char *FilePermissionShowFunc(int value);
static const char *UnixSockPermissionsShowFunc(void);
static const char *HBDestinationPortShowFunc(int index);
static const char *HBDeviceShowFunc(int index);
static const char *HBHostnameShowFunc(int index);
static const char *OtherWDPortShowFunc(int index);
static const char *OtherPPPortShowFunc(int index);
static const char *OtherPPHostShowFunc(int index);
static const char *BackendFlagsShowFunc(int index);
static const char *BackendDataDirShowFunc(int index);
static const char *BackendHostShowFunc(int index);
static const char *BackendPortShowFunc(int index);
static const char *BackendWeightShowFunc(int index);
static const char *BackendAppNameShowFunc(int index);

static const char *HealthCheckPeriodShowFunc(int index);
static const char *HealthCheckTimeOutShowFunc(int index);
static const char *HealthCheckMaxRetriesShowFunc(int index);
static const char *HealthCheckRetryDelayShowFunc(int index);
static const char *HealthCheckConnectTimeOutShowFunc(int index);
static const char *HealthCheckUserShowFunc(int index);
static const char *HealthCheckPasswordShowFunc(int index);
static const char *HealthCheckDatabaseShowFunc(int index);


/* check empty slot functions */
static bool WdIFSlotEmptyCheckFunc(int index);
static bool WdSlotEmptyCheckFunc(int index);
static bool BackendSlotEmptyCheckFunc(int index);

/*variable custom assign functions */
static bool FailOverOnBackendErrorAssignMessage(ConfigContext scontext, bool newval, int elevel);
static bool DelegateIPAssignMessage(ConfigContext scontext, char *newval, int elevel);
static bool BackendPortAssignFunc(ConfigContext context, int newval, int index, int elevel);
static bool BackendHostAssignFunc(ConfigContext context, char *newval, int index, int elevel);
static bool BackendDataDirAssignFunc(ConfigContext context, char *newval, int index, int elevel);
static bool BackendFlagsAssignFunc(ConfigContext context, char *newval, int index, int elevel);
static bool BackendWeightAssignFunc(ConfigContext context, double newval, int index, int elevel);
static bool BackendAppNameAssignFunc(ConfigContext context, char *newval, int index, int elevel);
static bool HBDestinationPortAssignFunc(ConfigContext context, int newval, int index, int elevel);
static bool HBDeviceAssignFunc(ConfigContext context, char *newval, int index, int elevel);
static bool HBHostnameAssignFunc(ConfigContext context, char *newval, int index, int elevel);
static bool OtherWDPortAssignFunc(ConfigContext context, int newval, int index, int elevel);
static bool OtherPPPortAssignFunc(ConfigContext context, int newval, int index, int elevel);
static bool OtherPPHostAssignFunc(ConfigContext context, char *newval, int index, int elevel);

static bool HealthCheckPeriodAssignFunc(ConfigContext context, int newval, int index, int elevel);
static bool HealthCheckTimeOutAssignFunc(ConfigContext context, int newval, int index, int elevel);
static bool HealthCheckMaxRetriesAssignFunc(ConfigContext context, int newval, int index, int elevel);
static bool HealthCheckRetryDelayAssignFunc(ConfigContext context, int newval, int index, int elevel);
static bool HealthCheckConnectTimeOutAssignFunc(ConfigContext context, int newval, int index, int elevel);

static bool HealthCheckUserAssignFunc(ConfigContext context, char *newval, int index, int elevel);
static bool HealthCheckPasswordAssignFunc(ConfigContext context, char *newval, int index, int elevel);
static bool HealthCheckDatabaseAssignFunc(ConfigContext context, char *newval, int index, int elevel);

static bool LogDestinationProcessFunc(char *newval, int elevel);
static bool SyslogIdentProcessFunc(char *newval, int elevel);
static bool SyslogFacilityProcessFunc(int newval, int elevel);
static bool SetHBDestIfFunc(int elevel);
static bool SetPgpoolNodeId(int elevel);

static struct config_generic *get_index_free_record_if_any(struct config_generic *record);

static bool parse_int(const char *value, int64 *result, int flags, const char **hintmsg, int64 MaxVal);
static bool convert_to_base_unit(double value, const char *unit,
								 int base_unit, double *base_value);

#ifndef POOL_PRIVATE

static void convert_int_from_base_unit(int64 base_value, int base_unit,
						   int64 *value, const char **unit);


/* These functions are used to provide Hints for enum type config parameters and
 * to output the values of the parameters.
 * These functions are not available for tools since they use the stringInfo that is
 * not present for tools.
 */

static const char *config_enum_lookup_by_value(struct config_enum *record, int val);

static char *ShowOption(struct config_generic *record, int index, int elevel);

static char *config_enum_get_options(struct config_enum *record, const char *prefix,
						const char *suffix, const char *separator);
static void send_row_description_for_detail_view(POOL_CONNECTION * frontend, POOL_CONNECTION_POOL * backend);
static int send_grouped_type_variable_to_frontend(struct config_grouped_array_var *grouped_record,
									   POOL_CONNECTION * frontend, POOL_CONNECTION_POOL * backend);
static int send_array_type_variable_to_frontend(struct config_generic *record,
									 POOL_CONNECTION * frontend, POOL_CONNECTION_POOL * backend);

#endif


static const struct config_enum_entry log_error_verbosity_options[] = {
	{"terse", PGERROR_TERSE, false},
	{"default", PGERROR_DEFAULT, false},
	{"verbose", PGERROR_VERBOSE, false},
	{NULL, 0, false}
};
static const struct config_enum_entry server_message_level_options[] = {
	{"debug", DEBUG2, true},
	{"debug5", DEBUG5, false},
	{"debug4", DEBUG4, false},
	{"debug3", DEBUG3, false},
	{"debug2", DEBUG2, false},
	{"debug1", DEBUG1, false},
	{"info", INFO, false},
	{"notice", NOTICE, false},
	{"warning", WARNING, false},
	{"error", ERROR, false},
	{"log", LOG, false},
	{"fatal", FATAL, false},
	{"panic", PANIC, false},
	{NULL, 0, false}
};

static const struct config_enum_entry backend_clustering_mode_options[] = {
	{"streaming_replication", CM_STREAMING_REPLICATION, false},
	{"native_replication", CM_NATIVE_REPLICATION, false},
	{"logical_replication", CM_LOGICAL_REPLICATION, false},
	{"slony", CM_SLONY, false},
	{"raw", CM_RAW, false},
	{"snapshot_isolation", CM_SNAPSHOT_ISOLATION, false},
	{NULL, 0, false}
};

static const struct config_enum_entry process_management_mode_options[] = {
	{"static", PM_STATIC, false},
	{"dynamic", PM_DYNAMIC, false},

	{NULL, 0, false}
};

static const struct config_enum_entry process_management_strategy_options[] = {
	{"aggressive", PM_STRATEGY_AGGRESSIVE, false},
	{"gentle", PM_STRATEGY_GENTLE, false},
	{"lazy", PM_STRATEGY_LAZY, false},

	{NULL, 0, false}
};

static const struct config_enum_entry log_standby_delay_options[] = {
	{"always", LSD_ALWAYS, false},
	{"if_over_threshold", LSD_OVER_THRESHOLD, false},
	{"none", LSD_NONE, false},
	{NULL, 0, false}
};

static const struct config_enum_entry memqcache_method_options[] = {
	{"shmem", SHMEM_CACHE, false},
	{"memcached", MEMCACHED_CACHE, false},
	{NULL, 0, false}
};

static const struct config_enum_entry wd_lifecheck_method_options[] = {
	{"query", LIFECHECK_BY_QUERY, false},
	{"heartbeat", LIFECHECK_BY_HB, false},
	{"external", LIFECHECK_BY_EXTERNAL, false},
	{NULL, 0, false}
};

static const struct config_enum_entry syslog_facility_options[] = {
	{"LOCAL0", LOG_LOCAL0, false},
	{"LOCAL1", LOG_LOCAL1, false},
	{"LOCAL2", LOG_LOCAL2, false},
	{"LOCAL3", LOG_LOCAL3, false},
	{"LOCAL4", LOG_LOCAL4, false},
	{"LOCAL5", LOG_LOCAL5, false},
	{"LOCAL6", LOG_LOCAL6, false},
	{"LOCAL7", LOG_LOCAL7, false},
	{NULL, 0, false}
};

static const struct config_enum_entry disable_load_balance_on_write_options[] = {
	{"off", DLBOW_OFF, false},
	{"transaction", DLBOW_TRANSACTION, false},
	{"trans_transaction", DLBOW_TRANS_TRANSACTION, false},
	{"always", DLBOW_ALWAYS, false},
	{"dml_adaptive", DLBOW_DML_ADAPTIVE, false},
	{NULL, 0, false}
};

static const struct config_enum_entry relcache_query_target_options[] = {
	{"primary", RELQTARGET_PRIMARY, false},
	{"load_balance_node", RELQTARGET_LOAD_BALANCE_NODE, false},
	{NULL, 0, false}
};

static const struct config_enum_entry check_temp_table_options[] = {
	{"catalog", CHECK_TEMP_CATALOG, false},	/* search system catalogs */
	{"trace", CHECK_TEMP_TRACE, false},		/* tracing temp tables */
	{"none", CHECK_TEMP_NONE, false},		/* do not check temp tables */
	{"on", CHECK_TEMP_ON, false},			/* same as CHECK_TEMP_CATALOG. Just for backward compatibility. */
	{"off", CHECK_TEMP_OFF, false},			/* same as CHECK_TEMP_NONE. Just for backward compatibility. */
	{NULL, 0, false}
};

/* From PostgreSQL's guc.c */
/*
 * Unit conversion tables.
 *
 * There are two tables, one for memory units, and another for time units.
 * For each supported conversion from one unit to another, we have an entry
 * in the table.
 *
 * To keep things simple, and to avoid possible roundoff error,
 * conversions are never chained.  There needs to be a direct conversion
 * between all units (of the same type).
 *
 * The conversions for each base unit must be kept in order from greatest to
 * smallest human-friendly unit; convert_xxx_from_base_unit() rely on that.
 * (The order of the base-unit groups does not matter.)
 */
#define MAX_UNIT_LEN		3	/* length of longest recognized unit string */

typedef struct
{
	char		unit[MAX_UNIT_LEN + 1]; /* unit, as a string, like "kB" or
										 * "min" */
	int			base_unit;		/* GUC_UNIT_XXX */
	double		multiplier;		/* Factor for converting unit -> base_unit */
} unit_conversion;

static const char *memory_units_hint = "Valid units for this parameter are \"B\", \"kB\", \"MB\", \"GB\", and \"TB\".";

static const unit_conversion memory_unit_conversion_table[] =
{
	{"TB", GUC_UNIT_BYTE, 1024.0 * 1024.0 * 1024.0 * 1024.0},
	{"GB", GUC_UNIT_BYTE, 1024.0 * 1024.0 * 1024.0},
	{"MB", GUC_UNIT_BYTE, 1024.0 * 1024.0},
	{"kB", GUC_UNIT_BYTE, 1024.0},
	{"B", GUC_UNIT_BYTE, 1.0},

	{"TB", GUC_UNIT_KB, 1024.0 * 1024.0 * 1024.0},
	{"GB", GUC_UNIT_KB, 1024.0 * 1024.0},
	{"MB", GUC_UNIT_KB, 1024.0},
	{"kB", GUC_UNIT_KB, 1.0},
	{"B", GUC_UNIT_KB, 1.0 / 1024.0},

	{"TB", GUC_UNIT_MB, 1024.0 * 1024.0},
	{"GB", GUC_UNIT_MB, 1024.0},
	{"MB", GUC_UNIT_MB, 1.0},
	{"kB", GUC_UNIT_MB, 1.0 / 1024.0},
	{"B", GUC_UNIT_MB, 1.0 / (1024.0 * 1024.0)},

	{""}						/* end of table marker */
};

static const char *time_units_hint = "Valid units for this parameter are \"us\", \"ms\", \"s\", \"min\", \"h\", and \"d\".";

static const unit_conversion time_unit_conversion_table[] =
{
	{"d", GUC_UNIT_MS, 1000 * 60 * 60 * 24},
	{"h", GUC_UNIT_MS, 1000 * 60 * 60},
	{"min", GUC_UNIT_MS, 1000 * 60},
	{"s", GUC_UNIT_MS, 1000},
	{"ms", GUC_UNIT_MS, 1},
	{"us", GUC_UNIT_MS, 1.0 / 1000},

	{"d", GUC_UNIT_S, 60 * 60 * 24},
	{"h", GUC_UNIT_S, 60 * 60},
	{"min", GUC_UNIT_S, 60},
	{"s", GUC_UNIT_S, 1},
	{"ms", GUC_UNIT_S, 1.0 / 1000},
	{"us", GUC_UNIT_S, 1.0 / (1000 * 1000)},

	{"d", GUC_UNIT_MIN, 60 * 24},
	{"h", GUC_UNIT_MIN, 60},
	{"min", GUC_UNIT_MIN, 1},
	{"s", GUC_UNIT_MIN, 1.0 / 60},
	{"ms", GUC_UNIT_MIN, 1.0 / (1000 * 60)},
	{"us", GUC_UNIT_MIN, 1.0 / (1000 * 1000 * 60)},

	{""}						/* end of table marker */
};
static struct config_bool ConfigureNamesBool[] =
{
	{
		{"serialize_accept", CFGCXT_INIT, CONNECTION_CONFIG,
			"whether to serialize accept() call to avoid thundering herd problem",
			CONFIG_VAR_TYPE_BOOL, false, 0
		},
		&g_pool_config.serialize_accept,	/* variable */
		false,					/* boot value */
		NULL,					/* assign func */
		NULL,					/* check func */
		NULL					/* show hook */
	},
	{
		{"failover_when_quorum_exists", CFGCXT_INIT, FAILOVER_CONFIG,
			"Do failover only when cluster has the quorum.",
			CONFIG_VAR_TYPE_BOOL, false, 0
		},
		&g_pool_config.failover_when_quorum_exists,
		true,
		NULL, NULL, NULL
	},
	{
		{"failover_require_consensus", CFGCXT_INIT, FAILOVER_CONFIG,
			"Only do failover when majority aggrees.",
			CONFIG_VAR_TYPE_BOOL, false, 0
		},
		&g_pool_config.failover_require_consensus,
		true,
		NULL, NULL, NULL
	},
	{
		{"allow_multiple_failover_requests_from_node", CFGCXT_INIT, FAILOVER_CONFIG,
			"A Pgpool-II node can send multiple failover requests to build consensus.",
			CONFIG_VAR_TYPE_BOOL, false, 0
		},
		&g_pool_config.allow_multiple_failover_requests_from_node,
		false,
		NULL, NULL, NULL
	},
	{
		{"enable_consensus_with_half_votes", CFGCXT_INIT, FAILOVER_CONFIG,
			"apply majority rule for consensus and quorum computation at 50% of votes in a cluster with an even number of nodes.",
			CONFIG_VAR_TYPE_BOOL, false, 0
		},
		&g_pool_config.enable_consensus_with_half_votes,
		false,
		NULL, NULL, NULL
	},
	{
		{"wd_remove_shutdown_nodes", CFGCXT_RELOAD, WATCHDOG_CONFIG,
			"Revoke the cluster membership of properly shutdown watchdog nodes.",
			CONFIG_VAR_TYPE_BOOL, false, 0
		},
		&g_pool_config.wd_remove_shutdown_nodes,
		false,
		NULL, NULL, NULL
	},
	{
		{"log_connections", CFGCXT_RELOAD, LOGGING_CONFIG,
			"Logs each successful connection.",
			CONFIG_VAR_TYPE_BOOL, false, 0
		},
		&g_pool_config.log_connections,
		false,
		NULL, NULL, NULL
	},

	{
		{"log_disconnections", CFGCXT_RELOAD, LOGGING_CONFIG,
			"Logs end of a session.",
			CONFIG_VAR_TYPE_BOOL, false, 0
		},
		&g_pool_config.log_disconnections,
		false,
		NULL, NULL, NULL
	},

	{
		{"log_hostname", CFGCXT_RELOAD, LOGGING_CONFIG,
			"Logs the host name in the connection logs.",
			CONFIG_VAR_TYPE_BOOL, false, 0
		},
		&g_pool_config.log_hostname,
		false,
		NULL, NULL, NULL
	},

	{
		{"enable_pool_hba", CFGCXT_RELOAD, CONNECTION_CONFIG,
			"Use pool_hba.conf for client authentication.",
			CONFIG_VAR_TYPE_BOOL, false, 0
		},
		&g_pool_config.enable_pool_hba,
		false,
		NULL, NULL, NULL
	},

	{
		{"replication_mode", CFGCXT_INIT, LOGGING_CONFIG,
			"Enables replication mode.",
			CONFIG_VAR_TYPE_BOOL, false, 0
		},
		&g_pool_config.replication_mode,
		false,
		NULL, NULL, NULL
	},

	{
		{"load_balance_mode", CFGCXT_INIT, LOAD_BALANCE_CONFIG,
			"Enables load balancing of queries.",
			CONFIG_VAR_TYPE_BOOL, false, 0
		},
		&g_pool_config.load_balance_mode,
		true,
		NULL, NULL, NULL
	},

	{
		{"replication_stop_on_mismatch", CFGCXT_RELOAD, REPLICATION_CONFIG,
			"Starts degeneration and stops replication, If there's a data mismatch between primary and secondary.",
			CONFIG_VAR_TYPE_BOOL, false, 0
		},
		&g_pool_config.replication_stop_on_mismatch,
		false,
		NULL, NULL, NULL
	},

	{
		{"failover_if_affected_tuples_mismatch", CFGCXT_RELOAD, REPLICATION_CONFIG,
			"Starts degeneration, If there's a data mismatch between primary and secondary.",
			CONFIG_VAR_TYPE_BOOL, false, 0
		},
		&g_pool_config.failover_if_affected_tuples_mismatch,
		false,
		NULL, NULL, NULL
	},

	{
		{"replicate_select", CFGCXT_RELOAD, REPLICATION_CONFIG,
			"Replicate SELECT statements when load balancing is disabled.",
			CONFIG_VAR_TYPE_BOOL, false, 0
		},
		&g_pool_config.replicate_select,
		false,
		NULL, NULL, NULL
	},

	{
		{"prefer_lower_delay_standby", CFGCXT_RELOAD, STREAMING_REPLICATION_CONFIG,
			"If the load balance node is delayed over delay_threshold on SR, pgpool find another standby node which is lower delayed.",
			CONFIG_VAR_TYPE_BOOL, false, 0
		},
		&g_pool_config.prefer_lower_delay_standby,
		false,
		NULL, NULL, NULL
	},

	{
		{"connection_cache", CFGCXT_INIT, CONNECTION_POOL_CONFIG,
			"Caches connections to backends.",
			CONFIG_VAR_TYPE_BOOL, false, 0
		},
		&g_pool_config.connection_cache,
		true,
		NULL, NULL, NULL
	},

	{
		{"fail_over_on_backend_error", CFGCXT_RELOAD, FAILOVER_CONFIG,
			"Old config parameter for failover_on_backend_error.",
			CONFIG_VAR_TYPE_BOOL, false, VAR_HIDDEN_IN_SHOW_ALL
		},
		NULL,
		true,
		FailOverOnBackendErrorAssignMessage, NULL, NULL
	},

	{
		{"failover_on_backend_error", CFGCXT_RELOAD, FAILOVER_CONFIG,
			"Triggers fail over when reading/writing to backend socket fails.",
			CONFIG_VAR_TYPE_BOOL, false, 0
		},
		&g_pool_config.failover_on_backend_error,
		true,
		NULL, NULL, NULL
	},

	{
		{"failover_on_backend_shutdown", CFGCXT_RELOAD, FAILOVER_CONFIG,
			"Triggers fail over when backend is shutdown.",
			CONFIG_VAR_TYPE_BOOL, false, 0
		},
		&g_pool_config.failover_on_backend_shutdown,
		true,
		NULL, NULL, NULL
	},

	{
		{"detach_false_primary", CFGCXT_RELOAD, FAILOVER_CONFIG,
			"Automatically detaches false primary node.",
			CONFIG_VAR_TYPE_BOOL, false, 0
		},
		&g_pool_config.detach_false_primary,
		false,
		NULL, NULL, NULL
	},

	{
		{"insert_lock", CFGCXT_RELOAD, REPLICATION_CONFIG,
			"Automatically locks table with INSERT to keep SERIAL data consistency",
			CONFIG_VAR_TYPE_BOOL, false, 0
		},
		&g_pool_config.insert_lock,
		true,
		NULL, NULL, NULL
	},

	{
		{"ignore_leading_white_space", CFGCXT_RELOAD, LOAD_BALANCE_CONFIG,
			"Ignores leading white spaces of each query string.",
			CONFIG_VAR_TYPE_BOOL, false, 0
		},
		&g_pool_config.ignore_leading_white_space,
		true,
		NULL, NULL, NULL
	},

	{
		{"log_statement", CFGCXT_SESSION, LOGGING_CONFIG,
			"Logs all statements in the pgpool logs.",
			CONFIG_VAR_TYPE_BOOL, false, 0
		},
		&g_pool_config.log_statement,
		false,
		NULL, NULL, NULL
	},

	{
		{"log_per_node_statement", CFGCXT_SESSION, LOGGING_CONFIG,
			"Logs per node detailed SQL statements.",
			CONFIG_VAR_TYPE_BOOL, false, 0
		},
		&g_pool_config.log_per_node_statement,
		false,
		NULL, NULL, NULL
	},

	{
		{"log_client_messages", CFGCXT_SESSION, LOGGING_CONFIG,
			"Logs any client messages in the pgpool logs.",
			CONFIG_VAR_TYPE_BOOL, false, 0
		},
		&g_pool_config.log_client_messages,
		false,
		NULL, NULL, NULL
	},

	{
		{"use_watchdog", CFGCXT_INIT, WATCHDOG_CONFIG,
			"Enables the pgpool-II watchdog.",
			CONFIG_VAR_TYPE_BOOL, false, 0
		},
		&g_pool_config.use_watchdog,
		false,
		NULL, NULL, NULL
	},

	{
		{"clear_memqcache_on_escalation", CFGCXT_RELOAD, WATCHDOG_CONFIG,
			"Clears the query cache in the shared memory when pgpool-II escalates to leader watchdog node.",
			CONFIG_VAR_TYPE_BOOL, false, 0
		},
		&g_pool_config.clear_memqcache_on_escalation,
		true,
		NULL, NULL, NULL
	},

	{
		{"ssl", CFGCXT_INIT, SSL_CONFIG,
			"Enables SSL support for frontend and backend connections",
			CONFIG_VAR_TYPE_BOOL, false, 0
		},
		&g_pool_config.ssl,
		false,
		NULL, NULL, NULL
	},

	{
		{"ssl_prefer_server_ciphers", CFGCXT_INIT, SSL_CONFIG,
			"Use server's SSL cipher preferences, rather than the client's",
			CONFIG_VAR_TYPE_BOOL, false, 0
		},
		&g_pool_config.ssl_prefer_server_ciphers,
		false,
		NULL, NULL, NULL
	},

	{
		{"check_unlogged_table", CFGCXT_SESSION, GENERAL_CONFIG,
			"Enables unlogged table check.",
			CONFIG_VAR_TYPE_BOOL, false, 0
		},
		&g_pool_config.check_unlogged_table,
		true,
		NULL, NULL, NULL
	},

	{
		{"memory_cache_enabled", CFGCXT_INIT, CACHE_CONFIG,
			"Enables the memory cache functionality.",
			CONFIG_VAR_TYPE_BOOL, false, 0
		},
		&g_pool_config.memory_cache_enabled,
		false,
		NULL, NULL, NULL
	},

	{
		{"enable_shared_relcache", CFGCXT_INIT, CACHE_CONFIG,
			"relation cache stored in memory cache.",
			CONFIG_VAR_TYPE_BOOL, false, 0
		},
		&g_pool_config.enable_shared_relcache,
		true,
		NULL, NULL, NULL
	},

	{
		{"memqcache_auto_cache_invalidation", CFGCXT_RELOAD, CACHE_CONFIG,
			"Automatically deletes the cache related to the updated tables.",
			CONFIG_VAR_TYPE_BOOL, false, 0
		},
		&g_pool_config.memqcache_auto_cache_invalidation,
		true,
		NULL, NULL, NULL
	},

	{
		{"allow_sql_comments", CFGCXT_SESSION, LOAD_BALANCE_CONFIG,
			"Ignore SQL comments, while judging if load balance or query cache is possible.",
			CONFIG_VAR_TYPE_BOOL, false, 0
		},
		&g_pool_config.allow_sql_comments,
		false,
		NULL, NULL, NULL
	},
	{
		{"allow_clear_text_frontend_auth", CFGCXT_RELOAD, GENERAL_CONFIG,
			"allow to use clear text password authentication with clients, when pool_passwd does not contain the user password.",
			CONFIG_VAR_TYPE_BOOL, false, 0
		},
		&g_pool_config.allow_clear_text_frontend_auth,
		false,
		NULL, NULL, NULL
	},

	{
		{"statement_level_load_balance", CFGCXT_RELOAD, LOAD_BALANCE_CONFIG,
			"Enables statement level load balancing",
			CONFIG_VAR_TYPE_BOOL, false, 0
		},
		&g_pool_config.statement_level_load_balance,
		false,
		NULL, NULL, NULL
	},

	{
		{"auto_failback", CFGCXT_RELOAD, FAILOVER_CONFIG,
			"Enables nodes automatically reattach, when detached node continue streaming replication.",
			CONFIG_VAR_TYPE_BOOL, false, 0
		},
		&g_pool_config.auto_failback,
		false,
		NULL, NULL, NULL
	},
	{
		{"logging_collector", CFGCXT_INIT, LOGGING_CONFIG,
			"Enable capturing of stderr into log files.",
			CONFIG_VAR_TYPE_BOOL, false, 0
		},
		&g_pool_config.logging_collector,
		false,
		NULL, NULL, NULL
	},
	{
		{"log_truncate_on_rotation", CFGCXT_INIT, LOGGING_CONFIG,
			"If on, an existing log file gets truncated on time based log rotation.",
			CONFIG_VAR_TYPE_BOOL, false, 0
		},
		&g_pool_config.log_truncate_on_rotation,
		false,
		NULL, NULL, NULL
	},

	{
		{"health_check_test", CFGCXT_INIT, HEALTH_CHECK_CONFIG,
		 "If on, enable health check testing.",
		 CONFIG_VAR_TYPE_BOOL, false, 0
		},
		&g_pool_config.health_check_test,
		false,
		NULL, NULL, NULL
	},

	/* End-of-list marker */
	EMPTY_CONFIG_BOOL

};

static struct config_string ConfigureNamesString[] =
{
	{
		{"database_redirect_preference_list", CFGCXT_RELOAD, STREAMING_REPLICATION_CONFIG,
			"redirect by database name.",
			CONFIG_VAR_TYPE_STRING, false, 0
		},
		&g_pool_config.database_redirect_preference_list,	/* variable */
		NULL,					/* boot value */
		NULL,					/* assign_func */
		NULL,					/* check_func */
		MakeDBRedirectListRegex,	/* process func */
		NULL					/* show hook */
	},

	{
		{"app_name_redirect_preference_list", CFGCXT_RELOAD, STREAMING_REPLICATION_CONFIG,
			"redirect by application name.",
			CONFIG_VAR_TYPE_STRING, false, 0
		},
		&g_pool_config.app_name_redirect_preference_list,
		NULL,
		NULL, NULL,
		MakeAppRedirectListRegex, NULL
	},

	{
		{"dml_adaptive_object_relationship_list", CFGCXT_RELOAD, STREAMING_REPLICATION_CONFIG,
			"list of relationships between objects.",
			CONFIG_VAR_TYPE_STRING, false, 0
		},
		&g_pool_config.dml_adaptive_object_relationship_list,
		NULL,
		NULL, NULL,
		MakeDMLAdaptiveObjectRelationList, NULL
	},

	{
		{"unix_socket_group", CFGCXT_INIT, CONNECTION_CONFIG,
			"The owning user of the sockets that always starts the server.",
			CONFIG_VAR_TYPE_STRING, false, 0
		},
		&g_pool_config.unix_socket_group,
		"",
		NULL, NULL, NULL, NULL
	},

	{
		{"pcp_socket_dir", CFGCXT_INIT, CONNECTION_CONFIG,
			"The directory to create the UNIX domain socket for accepting pgpool-II PCP connections.",
			CONFIG_VAR_TYPE_STRING, false, 0
		},
		&g_pool_config.pcp_socket_dir,
		DEFAULT_SOCKET_DIR,
		NULL, NULL, NULL, NULL
	},

	{
		{"wd_ipc_socket_dir", CFGCXT_INIT, CONNECTION_CONFIG,
			"The directory to create the UNIX domain socket for accepting pgpool-II watchdog IPC connections.",
			CONFIG_VAR_TYPE_STRING, false, 0
		},
		&g_pool_config.wd_ipc_socket_dir,
		DEFAULT_SOCKET_DIR,
		NULL, NULL, NULL, NULL
	},

	{
		{"log_destination", CFGCXT_RELOAD, LOGGING_CONFIG,
			"destination of pgpool-II log",
			CONFIG_VAR_TYPE_STRING, false, 0
		},
		&g_pool_config.log_destination_str,
		"stderr",
		NULL, NULL,
		LogDestinationProcessFunc,
		NULL
	},

	{
		{"syslog_ident", CFGCXT_RELOAD, LOGGING_CONFIG,
			"syslog program ident string.",
			CONFIG_VAR_TYPE_STRING, false, 0
		},
		&g_pool_config.syslog_ident,
		"pgpool",
		NULL, NULL,
		SyslogIdentProcessFunc,
		NULL
	},

	{
		{"pid_file_name", CFGCXT_INIT, FILE_LOCATION_CONFIG,
			"Path to store pgpool-II pid file.",
			CONFIG_VAR_TYPE_STRING, false, 0
		},
		&g_pool_config.pid_file_name,
		DEFAULT_PID_FILE_NAME,
		NULL, NULL, NULL, NULL
	},

	{
		{"pool_passwd", CFGCXT_INIT, FILE_LOCATION_CONFIG,
			"File name of pool_passwd for md5 authentication.",
			CONFIG_VAR_TYPE_STRING, false, VAR_HIDDEN_VALUE
		},
		&g_pool_config.pool_passwd,
		"pool_passwd",
		NULL, NULL, NULL, NULL
	},

	{
		{"log_line_prefix", CFGCXT_RELOAD, LOGGING_CONFIG,
			"printf-style string to output at beginning of each log line.",
			CONFIG_VAR_TYPE_STRING, false, 0
		},
		&g_pool_config.log_line_prefix,
		"%m: %a pid %p: ",
		NULL, NULL, NULL, NULL
	},

	{
		{"sr_check_user", CFGCXT_RELOAD, STREAMING_REPLICATION_CONFIG,
			"The User name to perform streaming replication delay check.",
			CONFIG_VAR_TYPE_STRING, false, 0
		},
		&g_pool_config.sr_check_user,
		"nobody",
		NULL, NULL, NULL, NULL
	},

	{
		{"sr_check_password", CFGCXT_RELOAD, STREAMING_REPLICATION_CONFIG,
			"The password for user to perform streaming replication delay check.",
			CONFIG_VAR_TYPE_STRING, false, VAR_HIDDEN_VALUE
		},
		&g_pool_config.sr_check_password,
		"",
		NULL, NULL, NULL, NULL
	},

	{
		{"sr_check_database", CFGCXT_RELOAD, STREAMING_REPLICATION_CONFIG,
			"The database name to perform streaming replication delay check.",
			CONFIG_VAR_TYPE_STRING, false, 0
		},
		&g_pool_config.sr_check_database,
		"postgres",
		NULL, NULL, NULL, NULL
	},

	{
		{"failback_command", CFGCXT_RELOAD, FAILOVER_CONFIG,
			"Command to execute when backend node is attached.",
			CONFIG_VAR_TYPE_STRING, false, 0
		},
		&g_pool_config.failback_command,
		"",
		NULL, NULL, NULL, NULL
	},

	{
		{"follow_primary_command", CFGCXT_RELOAD, FAILOVER_CONFIG,
			"Command to execute in streaming replication mode after a primary node failover.",
			CONFIG_VAR_TYPE_STRING, false, 0
		},
		&g_pool_config.follow_primary_command,
		"",
		NULL, NULL, NULL, NULL
	},

	{
		{"failover_command", CFGCXT_RELOAD, FAILOVER_CONFIG,
			"Command to execute when backend node is detached.",
			CONFIG_VAR_TYPE_STRING, false, 0
		},
		&g_pool_config.failover_command,
		"",
		NULL, NULL, NULL, NULL
	},

	{
		{"recovery_user", CFGCXT_RELOAD, RECOVERY_CONFIG,
			"User name for online recovery.",
			CONFIG_VAR_TYPE_STRING, false, 0
		},
		&g_pool_config.recovery_user,
		"",
		NULL, NULL, NULL, NULL
	},

	{
		{"recovery_password", CFGCXT_RELOAD, RECOVERY_CONFIG,
			"Password for online recovery.",
			CONFIG_VAR_TYPE_STRING, false, VAR_HIDDEN_VALUE
		},
		&g_pool_config.recovery_password,
		"",
		NULL, NULL, NULL, NULL
	},

	{
		{"recovery_1st_stage_command", CFGCXT_RELOAD, RECOVERY_CONFIG,
			"Command to execute in first stage recovery.",
			CONFIG_VAR_TYPE_STRING, false, 0
		},
		&g_pool_config.recovery_1st_stage_command,
		"",
		NULL, NULL, NULL, NULL
	},

	{
		{"recovery_2nd_stage_command", CFGCXT_RELOAD, RECOVERY_CONFIG,
			"Command to execute in second stage recovery.",
			CONFIG_VAR_TYPE_STRING, false, 0
		},
		&g_pool_config.recovery_2nd_stage_command,
		"",
		NULL, NULL, NULL, NULL
	},

	{
		{"lobj_lock_table", CFGCXT_RELOAD, REPLICATION_CONFIG,
			"Table name used for large object replication control.",
			CONFIG_VAR_TYPE_STRING, false, 0
		},
		&g_pool_config.lobj_lock_table,
		"",
		NULL, NULL, NULL, NULL
	},

	{
		{"wd_escalation_command", CFGCXT_RELOAD, WATCHDOG_CONFIG,
			"Command to execute when watchdog node becomes cluster leader node.",
			CONFIG_VAR_TYPE_STRING, false, 0
		},
		&g_pool_config.wd_escalation_command,
		"",
		NULL, NULL, NULL, NULL
	},

	{
		{"wd_de_escalation_command", CFGCXT_RELOAD, WATCHDOG_CONFIG,
			"Command to execute when watchdog node resigns from the cluster leader node.",
			CONFIG_VAR_TYPE_STRING, false, 0
		},
		&g_pool_config.wd_de_escalation_command,
		"",
		NULL, NULL, NULL, NULL
	},

	{
		{"trusted_servers", CFGCXT_INIT, WATCHDOG_CONFIG,
			"List of servers to verify connectivity.",
			CONFIG_VAR_TYPE_STRING, false, 0
		},
		&g_pool_config.trusted_servers,
		"",
		NULL, NULL, NULL, NULL
	},

	{
		{"trusted_server_command", CFGCXT_RELOAD, WATCHDOG_CONFIG,
			"Command to excute when communicate with trusted server.",
			CONFIG_VAR_TYPE_STRING, false, 0
		},
		&g_pool_config.trusted_server_command,
		"ping -q -c3 %h",
		NULL, NULL, NULL, NULL
	},

	{
		{"delegate_IP", CFGCXT_INIT, WATCHDOG_CONFIG,
			"Old config parameter for delegate_ip.",
			CONFIG_VAR_TYPE_STRING, false, VAR_HIDDEN_IN_SHOW_ALL
		},
		NULL,
		"",
		DelegateIPAssignMessage, NULL, NULL, NULL
	},

	{
		{"delegate_ip", CFGCXT_INIT, WATCHDOG_CONFIG,
			"Delegate IP address to be used when pgpool node become a watchdog cluster leader.",
			CONFIG_VAR_TYPE_STRING, false, 0
		},
		&g_pool_config.delegate_ip,
		"",
		NULL, NULL, NULL, NULL
	},

	{
		{"ping_path", CFGCXT_INIT, WATCHDOG_CONFIG,
			"path to ping command.",
			CONFIG_VAR_TYPE_STRING, false, 0
		},
		&g_pool_config.ping_path,
		"/bin",
		NULL, NULL, NULL, NULL
	},

	{
		{"if_cmd_path", CFGCXT_INIT, WATCHDOG_CONFIG,
			"Path to interface command.",
			CONFIG_VAR_TYPE_STRING, false, 0
		},
		&g_pool_config.if_cmd_path,
		"/sbin",
		NULL, NULL, NULL, NULL
	},

	{
		{"if_up_cmd", CFGCXT_INIT, WATCHDOG_CONFIG,
			"Complete command to bring UP virtual interface.",
			CONFIG_VAR_TYPE_STRING, false, 0
		},
		&g_pool_config.if_up_cmd,
		"/usr/bin/sudo /sbin/ip addr add $_IP_$/24 dev eth0 label eth0:0",
		NULL, NULL, NULL, NULL
	},

	{
		{"if_down_cmd", CFGCXT_INIT, WATCHDOG_CONFIG,
			"Complete command to bring down virtual interface.",
			CONFIG_VAR_TYPE_STRING, false, 0
		},
		&g_pool_config.if_down_cmd,
		"/usr/bin/sudo /sbin/ip addr del $_IP_$/24 dev eth0",
		NULL, NULL, NULL, NULL
	},

	{
		{"arping_path", CFGCXT_INIT, WATCHDOG_CONFIG,
			"path to arping command.",
			CONFIG_VAR_TYPE_STRING, false, 0
		},
		&g_pool_config.arping_path,
		"/usr/sbin",
		NULL, NULL, NULL, NULL
	},

	{
		{"arping_cmd", CFGCXT_INIT, WATCHDOG_CONFIG,
			"arping command.",
			CONFIG_VAR_TYPE_STRING, false, 0
		},
		&g_pool_config.arping_cmd,
		"/usr/bin/sudo /usr/sbin/arping -U $_IP_$ -w 1 -I eth0",
		NULL, NULL, NULL, NULL
	},

	{
		{"wd_lifecheck_query", CFGCXT_INIT, WATCHDOG_CONFIG,
			"SQL query to be used by watchdog lifecheck.",
			CONFIG_VAR_TYPE_STRING, false, 0
		},
		&g_pool_config.wd_lifecheck_query,
		"SELECT 1",
		NULL, NULL, NULL, NULL
	},

	{
		{"wd_lifecheck_dbname", CFGCXT_RELOAD, WATCHDOG_CONFIG,
			"Database name to be used for by watchdog lifecheck.",
			CONFIG_VAR_TYPE_STRING, false, 0
		},
		&g_pool_config.wd_lifecheck_dbname,
		"template1",
		NULL, NULL, NULL, NULL
	},

	{
		{"wd_lifecheck_user", CFGCXT_RELOAD, WATCHDOG_CONFIG,
			"User name to be used for by watchdog lifecheck.",
			CONFIG_VAR_TYPE_STRING, false, 0
		},
		&g_pool_config.wd_lifecheck_user,
		"nobody",
		NULL, NULL, NULL, NULL
	},

	{
		{"wd_lifecheck_password", CFGCXT_RELOAD, WATCHDOG_CONFIG,
			"Password for watchdog user in lifecheck.",
			CONFIG_VAR_TYPE_STRING, false, VAR_HIDDEN_VALUE
		},
		&g_pool_config.wd_lifecheck_password,
		"postgres",
		NULL, NULL, NULL, NULL
	},

	{
		{"wd_authkey", CFGCXT_RELOAD, WATCHDOG_CONFIG,
			"Authentication key to be used in watchdog communication.",
			CONFIG_VAR_TYPE_STRING, false, 0
		},
		&g_pool_config.wd_authkey,
		"",
		NULL, NULL, NULL, NULL
	},

	{
		{"ssl_cert", CFGCXT_INIT, SSL_CONFIG,
			"SSL public certificate file.",
			CONFIG_VAR_TYPE_STRING, false, 0
		},
		&g_pool_config.ssl_cert,
		"",
		NULL, NULL, NULL, NULL
	},

	{
		{"ssl_key", CFGCXT_INIT, SSL_CONFIG,
			"SSL private key file.",
			CONFIG_VAR_TYPE_STRING, false, 0
		},
		&g_pool_config.ssl_key,
		"",
		NULL, NULL, NULL, NULL
	},

	{
		{"ssl_ca_cert", CFGCXT_INIT, SSL_CONFIG,
			"Single PEM format file containing CA root certificate(s).",
			CONFIG_VAR_TYPE_STRING, false, 0
		},
		&g_pool_config.ssl_ca_cert,
		"",
		NULL, NULL, NULL, NULL
	},

	{
		{"ssl_ca_cert_dir", CFGCXT_INIT, SSL_CONFIG,
			"Directory containing CA root certificate(s).",
			CONFIG_VAR_TYPE_STRING, false, 0
		},
		&g_pool_config.ssl_ca_cert_dir,
		"",
		NULL, NULL, NULL, NULL
	},

	{
		{"ssl_crl_file", CFGCXT_INIT, SSL_CONFIG,
			"SSL certificate revocation list file",
			CONFIG_VAR_TYPE_STRING, false, 0
		},
		&g_pool_config.ssl_crl_file,
		"",
		NULL, NULL, NULL, NULL
	},

	{
		{"ssl_ciphers", CFGCXT_INIT, SSL_CONFIG,
			"Allowed SSL ciphers.",
			CONFIG_VAR_TYPE_STRING, false, 0
		},
		&g_pool_config.ssl_ciphers,
		"HIGH:MEDIUM:+3DES:!aNULL",
		NULL, NULL, NULL, NULL
	},

	{
		{"ssl_ecdh_curve", CFGCXT_INIT, SSL_CONFIG,
			"The curve to use in ECDH key exchange.",
			CONFIG_VAR_TYPE_STRING, false, 0
		},
		&g_pool_config.ssl_ecdh_curve,
		"prime256v1",
		NULL, NULL, NULL, NULL
	},

	{
		{"ssl_dh_params_file", CFGCXT_INIT, SSL_CONFIG,
			"Path to the Diffie-Hellman parameters contained file",
			CONFIG_VAR_TYPE_STRING, false, 0
		},
		&g_pool_config.ssl_dh_params_file,
		"",
		NULL, NULL, NULL, NULL
	},

	{
		{"ssl_passphrase_command", CFGCXT_INIT, SSL_CONFIG,
			"Path to the Diffie-Hellman parameters contained file",
			CONFIG_VAR_TYPE_STRING, false, 0
		},
		&g_pool_config.ssl_passphrase_command,
		"",
		NULL, NULL, NULL, NULL
	},


	{
		{"memqcache_oiddir", CFGCXT_INIT, CACHE_CONFIG,
			"Temporary directory to record table oids.",
			CONFIG_VAR_TYPE_STRING, false, 0
		},
		&g_pool_config.memqcache_oiddir,
		"/var/log/pgpool/oiddir",
		NULL, NULL, NULL, NULL
	},

	{
		{"memqcache_memcached_host", CFGCXT_INIT, CACHE_CONFIG,
			"Hostname or IP address of memcached.",
			CONFIG_VAR_TYPE_STRING, false, 0
		},
		&g_pool_config.memqcache_memcached_host,
		"localhost",
		NULL, NULL, NULL, NULL
	},

	{
		{"logdir", CFGCXT_INIT, LOGGING_CONFIG,
			"PgPool status file logging directory.",
			CONFIG_VAR_TYPE_STRING, false, 0
		},
		&g_pool_config.logdir,
		DEFAULT_LOGDIR,
		NULL, NULL, NULL, NULL
	},
	{
		{"log_directory", CFGCXT_INIT, LOGGING_CONFIG,
			"directory where log files are written.",
			CONFIG_VAR_TYPE_STRING, false, 0
		},
		&g_pool_config.log_directory,
		"/tmp/pgpool_logs",
		NULL, NULL, NULL, NULL
	},

	{
		{"log_filename", CFGCXT_INIT, LOGGING_CONFIG,
			"log file name pattern.",
			CONFIG_VAR_TYPE_STRING, false, 0
		},
		&g_pool_config.log_filename,
		"pgpool-%Y-%m-%d_%H%M%S.log",
		NULL, NULL, NULL, NULL
	},

	/* End-of-list marker */
	EMPTY_CONFIG_STRING
};

static struct config_string_list ConfigureNamesStringList[] =
{
	{
		{"reset_query_list", CFGCXT_RELOAD, CONNECTION_POOL_CONFIG,
			"list of commands sent to reset the backend connection when user session exits.",
			CONFIG_VAR_TYPE_STRING_LIST, false, 0
		},
		&g_pool_config.reset_query_list,	/* variable */
		&g_pool_config.num_reset_queries,	/* item count var  */
		(const char *) default_reset_query_list,	/* boot value */
		";",					/* token separator */
		false,					/* compute_regex ? */
		NULL, NULL, NULL		/* assign, check, show funcs */
	},

	{
		{"listen_addresses", CFGCXT_INIT, CONNECTION_CONFIG,
			"hostname(s) or IP address(es) on which pgpool will listen on.",
			CONFIG_VAR_TYPE_STRING_LIST, false, 0
		},
		&g_pool_config.listen_addresses,
		&g_pool_config.num_listen_addresses,
		(const char *) default_listen_addresses_list,
		",",
		false,
		NULL, NULL, NULL
	},

	{
		{"pcp_listen_addresses", CFGCXT_INIT, CONNECTION_CONFIG,
			"hostname(s) or IP address(es) on which pcp will listen on.",
			CONFIG_VAR_TYPE_STRING_LIST, false, 0
		},
		&g_pool_config.pcp_listen_addresses,
		&g_pool_config.num_pcp_listen_addresses,
		(const char *) default_pcp_listen_addresses_list,
		",",
		false,
		NULL, NULL, NULL
	},

	{
		{"unix_socket_directories", CFGCXT_INIT, CONNECTION_CONFIG,
			"The directories to create the UNIX domain sockets for accepting pgpool-II client connections.",
			CONFIG_VAR_TYPE_STRING_LIST, false, 0
		},
		&g_pool_config.unix_socket_directories,
		&g_pool_config.num_unix_socket_directories,
		(const char *) default_unix_socket_directories_list,
		",",
		false,
		NULL, NULL, NULL
	},

	{
		{"read_only_function_list", CFGCXT_RELOAD, CONNECTION_POOL_CONFIG,
			"list of functions that does not writes to database.",
			CONFIG_VAR_TYPE_STRING_LIST, false, 0
		},
		&g_pool_config.read_only_function_list,
		&g_pool_config.num_read_only_function_list,
		(const char *) default_read_only_function_list,
		",",
		true,
		NULL, NULL, NULL
	},

	{
		{"write_function_list", CFGCXT_RELOAD, CONNECTION_POOL_CONFIG,
			"list of functions that writes to database.",
			CONFIG_VAR_TYPE_STRING_LIST, false, 0
		},
		&g_pool_config.write_function_list,
		&g_pool_config.num_write_function_list,
		(const char *) default_write_function_list,
		",",
		true,
		NULL, NULL, NULL
	},
	{
		{"cache_safe_memqcache_table_list", CFGCXT_RELOAD, CACHE_CONFIG,
			"list of tables to be cached.",
			CONFIG_VAR_TYPE_STRING_LIST, false, 0
		},
		&g_pool_config.cache_safe_memqcache_table_list,
		&g_pool_config.num_cache_safe_memqcache_table_list,
		NULL,
		",",
		true,
		NULL, NULL, NULL
	},

	{
		{"cache_unsafe_memqcache_table_list", CFGCXT_RELOAD, CACHE_CONFIG,
			"list of tables should not be cached.",
			CONFIG_VAR_TYPE_STRING_LIST, false, 0
		},
		&g_pool_config.cache_unsafe_memqcache_table_list,
		&g_pool_config.num_cache_unsafe_memqcache_table_list,
		NULL,
		",",
		true,
		NULL, NULL, NULL
	},

	{
		{"primary_routing_query_pattern_list", CFGCXT_RELOAD, CONNECTION_POOL_CONFIG,
			"list of query patterns that should be sent to primary node.",
			CONFIG_VAR_TYPE_STRING_LIST, false, 0
		},
		&g_pool_config.primary_routing_query_pattern_list,
		&g_pool_config.num_primary_routing_query_pattern_list,
		NULL,
		";",
		true,
		NULL, NULL, NULL
	},

	{
		{"wd_monitoring_interfaces_list", CFGCXT_INIT, WATCHDOG_CONFIG,
			"List of network device names, to be monitored by the watchdog process for the network link state.",
			CONFIG_VAR_TYPE_STRING, false, 0
		},
		&g_pool_config.wd_monitoring_interfaces_list,
		&g_pool_config.num_wd_monitoring_interfaces_list,
		NULL,
		",",
		false,
		NULL, NULL, NULL
	},

	/* End-of-list marker */
	EMPTY_CONFIG_STRING_LIST
};

/* long configs*/
static struct config_long ConfigureNamesLong[] =
{
	{
		{"delay_threshold", CFGCXT_RELOAD, STREAMING_REPLICATION_CONFIG,
			"standby delay threshold in bytes.",
			CONFIG_VAR_TYPE_LONG, false, GUC_UNIT_BYTE
		},
		&g_pool_config.delay_threshold,
		0,
		0, LONG_MAX,
		NULL, NULL, NULL
	},

	{
		{"relcache_expire", CFGCXT_INIT, CACHE_CONFIG,
			"Relation cache expiration time in seconds.",
			CONFIG_VAR_TYPE_LONG, false, GUC_UNIT_S
		},
		&g_pool_config.relcache_expire,
		0,
		0, LONG_MAX,
		NULL, NULL, NULL
	},

	{
		{"memqcache_total_size", CFGCXT_INIT, CACHE_CONFIG,
			"Total memory size in bytes for storing memory cache.",
			CONFIG_VAR_TYPE_LONG, false, GUC_UNIT_BYTE
		},
		&g_pool_config.memqcache_total_size,
		(int64) 67108864,
		0, LONG_MAX,
		NULL, NULL, NULL
	},

	/* End-of-list marker */
	EMPTY_CONFIG_LONG

};


static struct config_int_array ConfigureNamesIntArray[] =
{
	{
		{"backend_port", CFGCXT_RELOAD, CONNECTION_CONFIG,
			"port number of PostgreSQL backend.",
			CONFIG_VAR_TYPE_INT_ARRAY, true, 0, MAX_NUM_BACKENDS
		},
		NULL,
		0,
		1024, 65535,
		EMPTY_CONFIG_INT,
		BackendPortAssignFunc, NULL, BackendPortShowFunc, BackendSlotEmptyCheckFunc
	},

	{
		{"heartbeat_port", CFGCXT_RELOAD, WATCHDOG_LIFECHECK,
			"Port for sending heartbeat.",
			CONFIG_VAR_TYPE_INT_ARRAY, true, 0, WD_MAX_IF_NUM
		},
		NULL,
		0,
		1024, 65535,
		EMPTY_CONFIG_INT,
		HBDestinationPortAssignFunc, NULL, HBDestinationPortShowFunc, WdIFSlotEmptyCheckFunc
	},

	{
		{"wd_port", CFGCXT_RELOAD, WATCHDOG_CONFIG,
			"tcp/ip watchdog port number of other pgpool node for watchdog connection..",
			CONFIG_VAR_TYPE_INT_ARRAY, true, 0, MAX_WATCHDOG_NUM
		},
		NULL,
		0,
		1024, 65535,
		EMPTY_CONFIG_INT,
		OtherWDPortAssignFunc, NULL, OtherWDPortShowFunc, WdSlotEmptyCheckFunc
	},

	{
		{"pgpool_port", CFGCXT_RELOAD, WATCHDOG_CONFIG,
			"tcp/ip pgpool port number of other pgpool node for watchdog connection.",
			CONFIG_VAR_TYPE_INT_ARRAY, true, 0, MAX_WATCHDOG_NUM
		},
		NULL,
		0,
		1024, 65535,
		EMPTY_CONFIG_INT,
		OtherPPPortAssignFunc, NULL, OtherPPPortShowFunc, WdSlotEmptyCheckFunc
	},

	{
		{"health_check_timeout", CFGCXT_RELOAD, HEALTH_CHECK_CONFIG,
			"Backend node health check timeout value in seconds.",
			CONFIG_VAR_TYPE_INT_ARRAY, true, GUC_UNIT_S, MAX_NUM_BACKENDS
		},
		NULL,
		20,
		0, INT_MAX,
		{
			{"health_check_timeout", CFGCXT_RELOAD, HEALTH_CHECK_CONFIG,
				"Default health check timeout value for node for which health_check_timeout[node-id] is not specified.",
				CONFIG_VAR_TYPE_INT, false, DEFAULT_FOR_NO_VALUE_ARRAY_VAR
			},
			&g_pool_config.health_check_timeout,
			20,
			0, INT_MAX,
			NULL, NULL, NULL
		},
		HealthCheckTimeOutAssignFunc, NULL, HealthCheckTimeOutShowFunc, BackendSlotEmptyCheckFunc
	},

	{
		{"health_check_period", CFGCXT_RELOAD, HEALTH_CHECK_CONFIG,
			"Time interval in seconds between the health checks.",
			CONFIG_VAR_TYPE_INT_ARRAY, true, GUC_UNIT_S, MAX_NUM_BACKENDS
		},
		NULL,
		0,
		0, INT_MAX,
		{
			{"health_check_period", CFGCXT_RELOAD, HEALTH_CHECK_CONFIG,
				"Default time interval between health checks for node for which health_check_period[node-id] is not specified.",
				CONFIG_VAR_TYPE_INT, false, DEFAULT_FOR_NO_VALUE_ARRAY_VAR
			},
			&g_pool_config.health_check_period,
			0,
			0, INT_MAX,
			NULL, NULL, NULL
		},
		HealthCheckPeriodAssignFunc, NULL, HealthCheckPeriodShowFunc, BackendSlotEmptyCheckFunc
	},

	{
		{"health_check_max_retries", CFGCXT_RELOAD, HEALTH_CHECK_CONFIG,
			"The maximum number of times to retry a failed health check before giving up and initiating failover.",
			CONFIG_VAR_TYPE_INT_ARRAY, true, 0, MAX_NUM_BACKENDS
		},
		NULL,
		0,
		0, INT_MAX,
		{
			{"health_check_max_retries", CFGCXT_RELOAD, HEALTH_CHECK_CONFIG,
				"Default maximum number of retries for node for which health_check_max_retries[node-id] is not specified.",
				CONFIG_VAR_TYPE_INT, false, DEFAULT_FOR_NO_VALUE_ARRAY_VAR
			},
			&g_pool_config.health_check_max_retries,
			0,
			0, INT_MAX,
			NULL, NULL, NULL
		},
		HealthCheckMaxRetriesAssignFunc, NULL, HealthCheckMaxRetriesShowFunc, BackendSlotEmptyCheckFunc
	},

	{
		{"health_check_retry_delay", CFGCXT_RELOAD, HEALTH_CHECK_CONFIG,
			"The amount of time in seconds to wait between failed health check retries.",
			CONFIG_VAR_TYPE_INT_ARRAY, true, GUC_UNIT_S, MAX_NUM_BACKENDS
		},
		NULL,
		1,
		0, INT_MAX,
		{
			{"health_check_retry_delay", CFGCXT_RELOAD, HEALTH_CHECK_CONFIG,
				"Default time between failed health check retries for node for which health_check_retry_delay[node-id] is not specified.",
				CONFIG_VAR_TYPE_INT, false, DEFAULT_FOR_NO_VALUE_ARRAY_VAR
			},
			&g_pool_config.health_check_retry_delay,
			1,
			0, INT_MAX,
			NULL, NULL, NULL
		},
		HealthCheckRetryDelayAssignFunc, NULL, HealthCheckRetryDelayShowFunc, BackendSlotEmptyCheckFunc
	},

	{
		{"connect_timeout", CFGCXT_RELOAD, HEALTH_CHECK_CONFIG,
			"Timeout in milliseconds before giving up connecting to backend.",
			CONFIG_VAR_TYPE_INT_ARRAY, true, GUC_UNIT_MS, MAX_NUM_BACKENDS
		},
		NULL,
		10000,
		0, INT_MAX,
		{
			{"connect_timeout", CFGCXT_RELOAD, HEALTH_CHECK_CONFIG,
				"Default connect_timeout value for node for which connect_timeout[node-id] is not specified.",
				CONFIG_VAR_TYPE_INT, false, DEFAULT_FOR_NO_VALUE_ARRAY_VAR
			},
			&g_pool_config.connect_timeout,
			10000,
			0, INT_MAX,
			NULL, NULL, NULL
		},
		HealthCheckConnectTimeOutAssignFunc, NULL, HealthCheckConnectTimeOutShowFunc, BackendSlotEmptyCheckFunc
	},
	/* End-of-list marker */
	EMPTY_CONFIG_INT_ARRAY

};


static struct config_double ConfigureNamesDouble[] =
{
	/* End-of-list marker */
	EMPTY_CONFIG_DOUBLE
};


static struct config_double_array ConfigureNamesDoubleArray[] =
{
	{
		{"backend_weight", CFGCXT_RELOAD, CONNECTION_CONFIG,
			"load balance weight of backend.",
			CONFIG_VAR_TYPE_DOUBLE_ARRAY, true, 0, MAX_NUM_BACKENDS
		},
		NULL,
		0,
		0.0, 100000000.0,
		EMPTY_CONFIG_DOUBLE,
		BackendWeightAssignFunc, NULL, BackendWeightShowFunc, BackendSlotEmptyCheckFunc
	},

	/* End-of-list marker */
	EMPTY_CONFIG_DOUBLE_ARRAY
};


static struct config_string_array ConfigureNamesStringArray[] =
{
	{
		{"backend_hostname", CFGCXT_RELOAD, CONNECTION_CONFIG,
			"hostname or IP address of PostgreSQL backend.",
			CONFIG_VAR_TYPE_STRING_ARRAY, true, 0, MAX_NUM_BACKENDS
		},
		NULL,
		"",
		EMPTY_CONFIG_STRING,
		BackendHostAssignFunc, NULL, BackendHostShowFunc, BackendSlotEmptyCheckFunc
	},

	{
		{"backend_data_directory", CFGCXT_RELOAD, CONNECTION_CONFIG,
			"data directory of the backend.",
			CONFIG_VAR_TYPE_STRING_ARRAY, true, 0, MAX_NUM_BACKENDS
		},
		NULL,
		"",
		EMPTY_CONFIG_STRING,
		BackendDataDirAssignFunc, NULL, BackendDataDirShowFunc, BackendSlotEmptyCheckFunc
	},

	{
		{"backend_application_name", CFGCXT_RELOAD, CONNECTION_CONFIG,
			"application_name of the backend.",
			CONFIG_VAR_TYPE_STRING_ARRAY, true, 0, MAX_NUM_BACKENDS
		},
		NULL,
		"",
		EMPTY_CONFIG_STRING,
		BackendAppNameAssignFunc, NULL, BackendAppNameShowFunc, BackendSlotEmptyCheckFunc
	},

	{
		{"backend_flag", CFGCXT_RELOAD, CONNECTION_CONFIG,
			"Controls various backend behavior.",
			CONFIG_VAR_TYPE_STRING_ARRAY, true, 0, MAX_NUM_BACKENDS
		},
		NULL,
		"ALLOW_TO_FAILOVER",
		EMPTY_CONFIG_STRING,
		BackendFlagsAssignFunc, NULL, BackendFlagsShowFunc, BackendSlotEmptyCheckFunc
	},

	{
		/*
		 * There are two entries of "backend_flag" for "ALLOW_TO_FAILOVER" and
		 * "ALWAYS_PRIMARY". This is mostly ok but "pgpool show all" command
		 * displayed both backend_flag entries, which looks redundant. The
		 * reason for this is, report_all_variables() shows grouped variables
		 * first then other variables except already shown as grouped
		 * variables.  Unfortunately build_variable groups() is not smart
		 * enough to build grouped variable data: it only registers the first
		 * backend_flag entry and leaves the second entry. since the second
		 * entry is not a grouped variable, backend_flag is shown firstly as a
		 * grouped variable and then is show as a non grouped variable in
		 * report_all_variables(). To fix this, mark that the second variable
		 * is also a grouped variable (the flag is set by
		 * build_config_variables()). See bug 728 for the report of the
		 * problem.
		 */
		{"backend_flag", CFGCXT_RELOAD, CONNECTION_CONFIG,
			"Controls various backend behavior.",
			CONFIG_VAR_TYPE_STRING_ARRAY, true, VAR_PART_OF_GROUP, MAX_NUM_BACKENDS
		},
		NULL,
		"",	/* for ALWAYS_PRIMARY */
		EMPTY_CONFIG_STRING,
		BackendFlagsAssignFunc, NULL, BackendFlagsShowFunc, BackendSlotEmptyCheckFunc
	},

	{
		{"heartbeat_hostname", CFGCXT_RELOAD, WATCHDOG_LIFECHECK,
			"Hostname for sending heartbeat signal.",
			CONFIG_VAR_TYPE_STRING_ARRAY, true, 0, WD_MAX_IF_NUM
		},
		NULL,
		"",
		EMPTY_CONFIG_STRING,
		HBHostnameAssignFunc, NULL, HBHostnameShowFunc, WdIFSlotEmptyCheckFunc
	},

	{
		{"heartbeat_device", CFGCXT_RELOAD, WATCHDOG_LIFECHECK,
			"Name of NIC device for sending heartbeat.",
			CONFIG_VAR_TYPE_STRING_ARRAY, true, 0, WD_MAX_IF_NUM
		},
		NULL,
		"",
		EMPTY_CONFIG_STRING,
		HBDeviceAssignFunc, NULL, HBDeviceShowFunc, WdIFSlotEmptyCheckFunc
	},

	{
		{"hostname", CFGCXT_RELOAD, WATCHDOG_CONFIG,
			"Hostname of pgpool node for watchdog connection.",
			CONFIG_VAR_TYPE_STRING_ARRAY, true, 0, MAX_WATCHDOG_NUM
		},
		NULL,
		"localhost",
		EMPTY_CONFIG_STRING,
		OtherPPHostAssignFunc, NULL, OtherPPHostShowFunc, WdSlotEmptyCheckFunc
	},

	{
		{"health_check_user", CFGCXT_RELOAD, HEALTH_CHECK_CONFIG,
			"User name for PostgreSQL backend health check.",
			CONFIG_VAR_TYPE_STRING_ARRAY, true, 0, MAX_NUM_BACKENDS
		},
		NULL,
		"nobody",
		{
			{"health_check_user", CFGCXT_RELOAD, HEALTH_CHECK_CONFIG,
				"Default PostgreSQL user name to perform health check on node for which health_check_user[node-id] is not specified.",
				CONFIG_VAR_TYPE_STRING, false, DEFAULT_FOR_NO_VALUE_ARRAY_VAR
			},
			&g_pool_config.health_check_user,
			"nobody",
			NULL, NULL, NULL, NULL
		},
		HealthCheckUserAssignFunc, NULL, HealthCheckUserShowFunc, BackendSlotEmptyCheckFunc
	},

	{
		{"health_check_password", CFGCXT_RELOAD, HEALTH_CHECK_CONFIG,
			"Password for PostgreSQL backend health check database user.",
			CONFIG_VAR_TYPE_STRING_ARRAY, true, VAR_HIDDEN_VALUE, MAX_NUM_BACKENDS
		},
		NULL,
		"",
		{
			{"health_check_password", CFGCXT_RELOAD, HEALTH_CHECK_CONFIG,
				"Default PostgreSQL user password to perform health check on node for which health_check_password[node-id] is not specified.",
				CONFIG_VAR_TYPE_STRING, false, VAR_HIDDEN_VALUE | DEFAULT_FOR_NO_VALUE_ARRAY_VAR
			},
			&g_pool_config.health_check_password,
			"",
			NULL, NULL, NULL, NULL
		},
		HealthCheckPasswordAssignFunc, NULL, HealthCheckPasswordShowFunc, BackendSlotEmptyCheckFunc
	},

	{
		{"health_check_database", CFGCXT_RELOAD, HEALTH_CHECK_CONFIG,
			"The database name to be used to perform PostgreSQL backend health check.",
			CONFIG_VAR_TYPE_STRING_ARRAY, true, 0, MAX_NUM_BACKENDS
		},
		NULL,
		"postgres",
		{
			{"health_check_database", CFGCXT_RELOAD, HEALTH_CHECK_CONFIG,
				"Default PostgreSQL database name to perform health check on node for which health_check_database[node-id] is not specified.",
				CONFIG_VAR_TYPE_STRING, false, DEFAULT_FOR_NO_VALUE_ARRAY_VAR
			},
			&g_pool_config.health_check_database,
			"postgres",
			NULL, NULL, NULL, NULL
		},
		HealthCheckDatabaseAssignFunc, NULL, HealthCheckDatabaseShowFunc, BackendSlotEmptyCheckFunc
	},

	/* End-of-list marker */
	EMPTY_CONFIG_STRING_ARRAY

};


/* int configs*/
static struct config_int ConfigureNamesInt[] =
{

	{
		{"port", CFGCXT_INIT, CONNECTION_CONFIG,
			"tcp/IP port number on which pgpool will listen on.",
			CONFIG_VAR_TYPE_INT, false, 0
		},
		&g_pool_config.port,
		9999,
		1024, 65535,
		NULL, NULL, NULL
	},

	{
		{"pcp_port", CFGCXT_INIT, CONNECTION_CONFIG,
			"tcp/IP port number on which pgpool PCP process will listen on.",
			CONFIG_VAR_TYPE_INT, false, 0
		},
		&g_pool_config.pcp_port,
		9898,
		1024, 65535,
		NULL, NULL, NULL
	},

	{
		{"unix_socket_permissions", CFGCXT_INIT, CONNECTION_CONFIG,
			"The access permissions of the Unix domain sockets.",
			CONFIG_VAR_TYPE_INT, false, 0
		},
		&g_pool_config.unix_socket_permissions,
		0777,
		0000, 0777,
		NULL, NULL, UnixSockPermissionsShowFunc
	},

	{
		{"num_init_children", CFGCXT_INIT, CONNECTION_POOL_CONFIG,
			"Maximim number of child processs to handle client connections.",
			CONFIG_VAR_TYPE_INT, false, 0
		},
		&g_pool_config.num_init_children,
		32,
		1, INT_MAX,
		NULL, NULL, NULL
	},

	{
		{"min_spare_children", CFGCXT_RELOAD, CONNECTION_POOL_CONFIG,
			"Minimum number of spare child processes.",
			CONFIG_VAR_TYPE_INT, false, 0
		},
		&g_pool_config.min_spare_children,
		5,
		1, INT_MAX,
		NULL, NULL, NULL
	},

	{
		{"max_spare_children", CFGCXT_RELOAD, CONNECTION_POOL_CONFIG,
			"Maximum number of spare child processes.",
			CONFIG_VAR_TYPE_INT, false, 0
		},
		&g_pool_config.max_spare_children,
		10,
		1, INT_MAX,
		NULL, NULL, NULL
	},

	{
		{"reserved_connections", CFGCXT_INIT, CONNECTION_POOL_CONFIG,
			"Number of reserved connections.",
			CONFIG_VAR_TYPE_INT, false, 0
		},
		&g_pool_config.reserved_connections,
		0,
		0, INT_MAX,
		NULL, NULL, NULL
	},

	{
		{"listen_backlog_multiplier", CFGCXT_INIT, CONNECTION_CONFIG,
			"length of connection queue from frontend to pgpool-II",
			CONFIG_VAR_TYPE_INT, false, 0
		},
		&g_pool_config.listen_backlog_multiplier,
		2,
		1, INT_MAX,
		NULL, NULL, NULL
	},

	{
		{"child_life_time", CFGCXT_INIT, CONNECTION_POOL_CONFIG,
			"pgpool-II child process life time in seconds.",
			CONFIG_VAR_TYPE_INT, false, GUC_UNIT_S
		},
		&g_pool_config.child_life_time,
		300,
		0, INT_MAX,
		NULL, NULL, NULL
	},

	{
		{"client_idle_limit", CFGCXT_SESSION, CONNECTION_POOL_CONFIG,
			"idle time in seconds to disconnects a client.",
			CONFIG_VAR_TYPE_INT, false, GUC_UNIT_S
		},
		&g_pool_config.client_idle_limit,
		0,
		0, INT_MAX,
		NULL, NULL, NULL
	},

	{
		{"connection_life_time", CFGCXT_INIT, CONNECTION_POOL_CONFIG,
			"Cached connections expiration time in seconds.",
			CONFIG_VAR_TYPE_INT, false, GUC_UNIT_S
		},
		&g_pool_config.connection_life_time,
		0,
		0, INT_MAX,
		NULL, NULL, NULL
	},

	{
		{"child_max_connections", CFGCXT_INIT, CONNECTION_POOL_CONFIG,
			"A pgpool-II child process will be terminated after this many connections from clients.",
			CONFIG_VAR_TYPE_INT, false, 0
		},
		&g_pool_config.child_max_connections,
		0,
		0, INT_MAX,
		NULL, NULL, NULL
	},

	{
		{"authentication_timeout", CFGCXT_INIT, CONNECTION_CONFIG,
			"Time out value in seconds for client authentication.",
			CONFIG_VAR_TYPE_INT, false, GUC_UNIT_S
		},
		&g_pool_config.authentication_timeout,
		60,
		0, INT_MAX,
		NULL, NULL, NULL
	},

	{
		{"max_pool", CFGCXT_INIT, CONNECTION_POOL_CONFIG,
			"Maximum number of connection pools per child process.",
			CONFIG_VAR_TYPE_INT, false, 0
		},
		&g_pool_config.max_pool,
		4,
		0, INT_MAX,
		NULL, NULL, NULL
	},

	{
		{"sr_check_period", CFGCXT_RELOAD, STREAMING_REPLICATION_CONFIG,
			"Time interval in seconds between the streaming replication delay checks.",
			CONFIG_VAR_TYPE_INT, false, GUC_UNIT_S
		},
		&g_pool_config.sr_check_period,
		10,
		0, INT_MAX,
		NULL, NULL, NULL
	},

	{
		{"recovery_timeout", CFGCXT_RELOAD, RECOVERY_CONFIG,
			"Maximum time in seconds to wait for the recovering PostgreSQL node.",
			CONFIG_VAR_TYPE_INT, false, GUC_UNIT_S
		},
		&g_pool_config.recovery_timeout,
		90,
		0, INT_MAX,
		NULL, NULL, NULL
	},

	{
		{"client_idle_limit_in_recovery", CFGCXT_SESSION, RECOVERY_CONFIG,
			"Time limit is seconds for the child connection, before it is terminated during the 2nd stage recovery.",
			CONFIG_VAR_TYPE_INT, false, GUC_UNIT_S
		},
		&g_pool_config.client_idle_limit_in_recovery,
		0,
		-1, INT_MAX,
		NULL, NULL, NULL
	},

	{
		{"search_primary_node_timeout", CFGCXT_RELOAD, FAILOVER_CONFIG,
			"Max time in seconds to search for primary node after failover.",
			CONFIG_VAR_TYPE_INT, false, GUC_UNIT_S
		},
		&g_pool_config.search_primary_node_timeout,
		300,
		0, INT_MAX,
		NULL, NULL, NULL
	},

	{
		{"wd_priority", CFGCXT_INIT, WATCHDOG_CONFIG,
			"Watchdog node priority for leader election.",
			CONFIG_VAR_TYPE_INT, false, 0
		},
		&g_pool_config.wd_priority,
		1,
		0, INT_MAX,
		NULL, NULL, NULL
	},

	{
		{"wd_interval", CFGCXT_INIT, WATCHDOG_CONFIG,
			"Time interval in seconds between life check.",
			CONFIG_VAR_TYPE_INT, false, GUC_UNIT_S
		},
		&g_pool_config.wd_interval,
		10,
		0, INT_MAX,
		NULL, NULL, NULL
	},
	{
		{"wd_lost_node_removal_timeout", CFGCXT_RELOAD, WATCHDOG_CONFIG,
			"Timeout in seconds to revoke the cluster membership of LOST watchdog nodes.",
			CONFIG_VAR_TYPE_INT, false, GUC_UNIT_S
		},
		&g_pool_config.wd_lost_node_removal_timeout,
		0,
		0, INT_MAX,
		NULL, NULL, NULL
	},
	{
		{"wd_no_show_node_removal_timeout", CFGCXT_RELOAD, WATCHDOG_CONFIG,
			"Timeout in seconds to revoke the cluster membership of NO-SHOW watchdog nodes.",
			CONFIG_VAR_TYPE_INT, false, GUC_UNIT_S
		},
		&g_pool_config.wd_no_show_node_removal_timeout,
		0,
		0, INT_MAX,
		NULL, NULL, NULL
	},

	{
		{"wd_life_point", CFGCXT_INIT, WATCHDOG_CONFIG,
			"Maximum number of retries before failing the life check.",
			CONFIG_VAR_TYPE_INT, false, 0
		},
		&g_pool_config.wd_life_point,
		3,
		0, INT_MAX,
		NULL, NULL, NULL
	},

	{
		{"wd_heartbeat_keepalive", CFGCXT_INIT, WATCHDOG_CONFIG,
			"Time interval in seconds between sending the heartbeat signal.",
			CONFIG_VAR_TYPE_INT, false, GUC_UNIT_S
		},
		&g_pool_config.wd_heartbeat_keepalive,
		2,
		1, INT_MAX,
		NULL, NULL, NULL
	},

	{
		{"wd_heartbeat_deadtime", CFGCXT_INIT, WATCHDOG_CONFIG,
			"Deadtime interval in seconds for heartbeat signal.",
			CONFIG_VAR_TYPE_INT, false, GUC_UNIT_S
		},
		&g_pool_config.wd_heartbeat_deadtime,
		30,
		1, INT_MAX,
		NULL, NULL, NULL
	},

	{
		{"relcache_size", CFGCXT_INIT, CACHE_CONFIG,
			"Number of relation cache entry.",
			CONFIG_VAR_TYPE_INT, false, 0
		},
		&g_pool_config.relcache_size,
		256,
		0, INT_MAX,
		NULL, NULL, NULL
	},

	{
		{"memqcache_memcached_port", CFGCXT_INIT, CACHE_CONFIG,
			"Port number of Memcached server.",
			CONFIG_VAR_TYPE_INT, false, 0
		},
		&g_pool_config.memqcache_memcached_port,
		11211,
		0, INT_MAX,
		NULL, NULL, NULL
	},

	{
		{"memqcache_max_num_cache", CFGCXT_INIT, CACHE_CONFIG,
			"Total number of cache entries.",
			CONFIG_VAR_TYPE_INT, false, 0
		},
		&g_pool_config.memqcache_max_num_cache,
		1000000,
		0, INT_MAX,
		NULL, NULL, NULL
	},

	{
		{"memqcache_expire", CFGCXT_INIT, CACHE_CONFIG,
			"Memory cache entry life time specified in seconds.",
			CONFIG_VAR_TYPE_INT, false, GUC_UNIT_S
		},
		&g_pool_config.memqcache_expire,
		0,
		0, INT_MAX,
		NULL, NULL, NULL
	},

	{
		{"memqcache_maxcache", CFGCXT_INIT, CACHE_CONFIG,
			"Maximum SELECT result size in bytes.",
			CONFIG_VAR_TYPE_INT, false, GUC_UNIT_BYTE
		},
		&g_pool_config.memqcache_maxcache,
		409600,
		0, INT_MAX,
		NULL, NULL, NULL
	},

	{
		{"memqcache_cache_block_size", CFGCXT_INIT, CACHE_CONFIG,
			"Cache block size in bytes.",
			CONFIG_VAR_TYPE_INT, false, GUC_UNIT_BYTE
		},
		&g_pool_config.memqcache_cache_block_size,
		1048576,
		512, INT_MAX,
		NULL, NULL, NULL
	},

	{
		{"auto_failback_interval", CFGCXT_RELOAD, FAILOVER_CONFIG,
			"min interval of executing auto_failback in seconds",
			CONFIG_VAR_TYPE_INT, false, GUC_UNIT_S
		},
		&g_pool_config.auto_failback_interval,
		60,
		0, INT_MAX,
		NULL, NULL, NULL
	},
	{
		{"log_rotation_age", CFGCXT_INIT, LOGGING_CONFIG,
			"Automatic rotation of logfiles will happen after that (minutes) time.",
			CONFIG_VAR_TYPE_INT, false, GUC_UNIT_MIN
		},
		&g_pool_config.log_rotation_age,
		1440,/*1 day*/
		0, INT_MAX,
		NULL, NULL, NULL
	},
	{
		{"log_rotation_size", CFGCXT_INIT, LOGGING_CONFIG,
			"Automatic rotation of logfiles will happen after that much (kilobytes) log output.",
			CONFIG_VAR_TYPE_INT, false, GUC_UNIT_KB
		},
		&g_pool_config.log_rotation_size,
		10 * 1024,
		0, INT_MAX/1024,
		NULL, NULL, NULL
	},
	{
		{"log_file_mode", CFGCXT_INIT, LOGGING_CONFIG,
			"creation mode for log files.",
			CONFIG_VAR_TYPE_INT, false, 0
		},
		&g_pool_config.log_file_mode,
		0600,
		0, INT_MAX,
		NULL, NULL, NULL
	},
	{
		{"delay_threshold_by_time", CFGCXT_RELOAD, STREAMING_REPLICATION_CONFIG,
			"standby delay threshold by time.",
			CONFIG_VAR_TYPE_INT, false, GUC_UNIT_S,
		},
		&g_pool_config.delay_threshold_by_time,
		0,
		0, INT_MAX,
		NULL, NULL, NULL
	},

	/* End-of-list marker */
	EMPTY_CONFIG_INT
};

static struct config_enum ConfigureNamesEnum[] =
{
	{
		{"backend_clustering_mode", CFGCXT_INIT, MAIN_REPLICA_CONFIG,
			"backend clustering mode.",
			CONFIG_VAR_TYPE_ENUM, false, 0
		},
		(int *) &g_pool_config.backend_clustering_mode,
		CM_STREAMING_REPLICATION,
		backend_clustering_mode_options,
		NULL, NULL, NULL, NULL
	},

	{
		{"process_management_mode", CFGCXT_RELOAD, CONNECTION_POOL_CONFIG,
			"child process management mode.",
			CONFIG_VAR_TYPE_ENUM, false, 0
		},
		(int *) &g_pool_config.process_management,
		PM_STATIC,
		process_management_mode_options,
		NULL, NULL, NULL, NULL
	},

	{
		{"process_management_strategy", CFGCXT_RELOAD, CONNECTION_POOL_CONFIG,
			"child process management strategy.",
			CONFIG_VAR_TYPE_ENUM, false, 0
		},
		(int *) &g_pool_config.process_management_strategy,
		PM_STRATEGY_GENTLE,
		process_management_strategy_options,
		NULL, NULL, NULL, NULL
	},

	{
		{"syslog_facility", CFGCXT_RELOAD, LOGGING_CONFIG,
			"syslog local facility.",
			CONFIG_VAR_TYPE_ENUM, false, 0
		},
		&g_pool_config.syslog_facility,
		LOG_LOCAL0,
		syslog_facility_options,
		NULL, NULL,
		SyslogFacilityProcessFunc,
		NULL
	},

	{
		{"log_error_verbosity", CFGCXT_SESSION, LOGGING_CONFIG,
			"How much details about error should be emitted.",
			CONFIG_VAR_TYPE_ENUM, false, 0
		},
		&g_pool_config.log_error_verbosity,
		PGERROR_DEFAULT,
		log_error_verbosity_options,
		NULL, NULL, NULL, NULL
	},

	{
		{"client_min_messages", CFGCXT_SESSION, LOGGING_CONFIG,
			"Which messages should be sent to client.",
			CONFIG_VAR_TYPE_ENUM, false, 0
		},
		&g_pool_config.client_min_messages,
		NOTICE,
		server_message_level_options,
		NULL, NULL, NULL, NULL
	},

	{
		{"log_min_messages", CFGCXT_SESSION, LOGGING_CONFIG,
			"Which messages should be emitted to server log.",
			CONFIG_VAR_TYPE_ENUM, false, 0
		},
		&g_pool_config.log_min_messages,
		WARNING,
		server_message_level_options,
		NULL, NULL, NULL, NULL
	},

	{
		{"log_standby_delay", CFGCXT_RELOAD, MAIN_REPLICA_CONFIG,
			"When to log standby delay.",
			CONFIG_VAR_TYPE_ENUM, false, 0
		},
		(int *) &g_pool_config.log_standby_delay,
		LSD_OVER_THRESHOLD,
		log_standby_delay_options,
		NULL, NULL, NULL, NULL
	},

	{
		{"wd_lifecheck_method", CFGCXT_INIT, WATCHDOG_CONFIG,
			"method for watchdog lifecheck.",
			CONFIG_VAR_TYPE_ENUM, false, 0
		},
		(int *) &g_pool_config.wd_lifecheck_method,
		LIFECHECK_BY_HB,
		wd_lifecheck_method_options,
		NULL, NULL, NULL, NULL
	},

	{
		{"memqcache_method", CFGCXT_INIT, CACHE_CONFIG,
			"Cache store method. either shmem(shared memory) or Memcached. shmem by default.",
			CONFIG_VAR_TYPE_ENUM, false, 0
		},
		(int *) &g_pool_config.memqcache_method,
		SHMEM_CACHE,
		memqcache_method_options,
		NULL, NULL, NULL, NULL
	},

	{
		{"disable_load_balance_on_write", CFGCXT_RELOAD, LOAD_BALANCE_CONFIG,
			"Load balance behavior when write query is received.",
			CONFIG_VAR_TYPE_ENUM, false, 0
		},
		(int *) &g_pool_config.disable_load_balance_on_write,
		DLBOW_TRANSACTION,
		disable_load_balance_on_write_options,
		NULL, NULL, NULL, NULL
	},

	{
		{"relcache_query_target", CFGCXT_RELOAD, LOAD_BALANCE_CONFIG,
			"Target node to send relache queries.",
			CONFIG_VAR_TYPE_ENUM, false, 0
		},
		(int *) &g_pool_config.relcache_query_target,
		RELQTARGET_PRIMARY,
		relcache_query_target_options,
		NULL, NULL, NULL, NULL
	},

	{
		{"check_temp_table", CFGCXT_RELOAD, GENERAL_CONFIG,
			"Enables temporary table check.",
			CONFIG_VAR_TYPE_BOOL, false, 0
		},
		(int *) &g_pool_config.check_temp_table,
		CHECK_TEMP_CATALOG,
		check_temp_table_options,
		NULL, NULL, NULL, NULL
	},

	/* End-of-list marker */
	EMPTY_CONFIG_ENUM
};

/* finally the groups */
static struct config_grouped_array_var ConfigureVarGroups[] =
{
	{
		{"backend", CFGCXT_BOOT, CONNECTION_CONFIG,
			"backend configuration group.",
			CONFIG_VAR_TYPE_GROUP, false, 0
		},
		-1,						/* until initialized */
		NULL
	},
	{
		{"watchdog", CFGCXT_BOOT, WATCHDOG_CONFIG,
			"watchdog nodes configuration group.",
			CONFIG_VAR_TYPE_GROUP, false, 0
		},
		-1,						/* until initialized */
		NULL
	},
	{
		{"heartbeat", CFGCXT_BOOT, WATCHDOG_LIFECHECK,
			"heartbeat configuration group.",
			CONFIG_VAR_TYPE_GROUP, false, 0
		},
		-1,						/* until initialized */
		NULL
	},
	{
		{"health_check", CFGCXT_BOOT, HEALTH_CHECK_CONFIG,
			"backend health check configuration group.",
			CONFIG_VAR_TYPE_GROUP, false, 0
		},
		-1,						/* until initialized */
		NULL
	},
	/* End-of-list marker */
	EMPTY_CONFIG_GROUP_ARRAY

};

static void
initialize_config_gen(struct config_generic *gen)
{
	if (gen->dynamic_array_var == false)
		gen->max_elements = 1;

	gen->sources = palloc(sizeof(GucSource) * gen->max_elements);
	gen->reset_sources = palloc(sizeof(GucSource) * gen->max_elements);
	gen->scontexts = palloc(sizeof(ConfigContext) * gen->max_elements);
}

static void
build_config_variables(void)
{
	struct config_generic **all_vars;
	int			i;
	int			num_vars = 0;

	for (i = 0; ConfigureNamesBool[i].gen.name; i++)
	{
		struct config_bool *conf = &ConfigureNamesBool[i];

		/* Rather than requiring vartype to be filled in by hand, do this: */
		initialize_config_gen(&conf->gen);
		conf->gen.vartype = CONFIG_VAR_TYPE_BOOL;
		num_vars++;
	}

	for (i = 0; ConfigureNamesInt[i].gen.name; i++)
	{
		struct config_int *conf = &ConfigureNamesInt[i];

		initialize_config_gen(&conf->gen);
		conf->gen.vartype = CONFIG_VAR_TYPE_INT;
		num_vars++;
	}

	for (i = 0; ConfigureNamesIntArray[i].gen.name; i++)
	{
		struct config_int_array *conf = &ConfigureNamesIntArray[i];

		conf->gen.vartype = CONFIG_VAR_TYPE_INT_ARRAY;
		conf->gen.dynamic_array_var = true;
		initialize_config_gen(&conf->gen);

		/* Assign the memory for reset vals */
		conf->reset_vals = palloc0(sizeof(int) * conf->gen.max_elements);

		if (conf->config_no_index.gen.name)
		{
			conf->config_no_index.gen.dynamic_array_var = false;
			initialize_config_gen(&conf->config_no_index.gen);
			conf->config_no_index.gen.context = conf->gen.context;
			conf->gen.flags |= ARRAY_VAR_ALLOW_NO_INDEX;
		}
		num_vars++;
	}

	for (i = 0; ConfigureNamesLong[i].gen.name; i++)
	{
		struct config_long *conf = &ConfigureNamesLong[i];

		initialize_config_gen(&conf->gen);
		conf->gen.vartype = CONFIG_VAR_TYPE_LONG;
		num_vars++;
	}

	for (i = 0; ConfigureNamesDouble[i].gen.name; i++)
	{
		struct config_double *conf = &ConfigureNamesDouble[i];

		initialize_config_gen(&conf->gen);
		conf->gen.vartype = CONFIG_VAR_TYPE_DOUBLE;
		num_vars++;
	}

	for (i = 0; ConfigureNamesString[i].gen.name; i++)
	{
		struct config_string *conf = &ConfigureNamesString[i];

		initialize_config_gen(&conf->gen);
		conf->gen.vartype = CONFIG_VAR_TYPE_STRING;
		conf->reset_val = NULL;
		num_vars++;
	}

	for (i = 0; ConfigureNamesEnum[i].gen.name; i++)
	{
		struct config_enum *conf = &ConfigureNamesEnum[i];

		initialize_config_gen(&conf->gen);
		conf->gen.vartype = CONFIG_VAR_TYPE_ENUM;
		num_vars++;
	}

	for (i = 0; ConfigureNamesStringList[i].gen.name; i++)
	{
		struct config_string_list *conf = &ConfigureNamesStringList[i];

		initialize_config_gen(&conf->gen);
		conf->gen.vartype = CONFIG_VAR_TYPE_STRING_LIST;
		conf->reset_val = NULL;
		num_vars++;
	}

	for (i = 0; ConfigureNamesStringArray[i].gen.name; i++)
	{
		struct config_string_array *conf = &ConfigureNamesStringArray[i];

		conf->gen.dynamic_array_var = true;
		initialize_config_gen(&conf->gen);

		conf->gen.vartype = CONFIG_VAR_TYPE_STRING_ARRAY;
		/* Assign the memory for reset vals */
		conf->reset_vals = palloc0(sizeof(char *) * conf->gen.max_elements);

		if (conf->config_no_index.gen.name)
		{
			conf->config_no_index.gen.dynamic_array_var = false;
			initialize_config_gen(&conf->config_no_index.gen);
			conf->config_no_index.gen.context = conf->gen.context;
			conf->gen.flags |= ARRAY_VAR_ALLOW_NO_INDEX;
		}

		num_vars++;
	}

	for (i = 0; ConfigureNamesDoubleArray[i].gen.name; i++)
	{
		struct config_double_array *conf = &ConfigureNamesDoubleArray[i];

		conf->gen.dynamic_array_var = true;
		initialize_config_gen(&conf->gen);

		conf->gen.vartype = CONFIG_VAR_TYPE_DOUBLE_ARRAY;
		/* Assign the memory for reset vals */
		conf->reset_vals = palloc0(sizeof(double) * conf->gen.max_elements);

		if (conf->config_no_index.gen.name)
		{
			conf->config_no_index.gen.dynamic_array_var = false;
			initialize_config_gen(&conf->config_no_index.gen);
			conf->config_no_index.gen.context = conf->gen.context;
			conf->gen.flags |= ARRAY_VAR_ALLOW_NO_INDEX;
		}

		num_vars++;
	}

	/* For end marker */
	num_vars++;

	all_vars = (struct config_generic **)
		palloc(num_vars * sizeof(struct config_generic *));

	num_vars = 0;

	for (i = 0; ConfigureNamesBool[i].gen.name; i++)
		all_vars[num_vars++] = &ConfigureNamesBool[i].gen;

	for (i = 0; ConfigureNamesInt[i].gen.name; i++)
		all_vars[num_vars++] = &ConfigureNamesInt[i].gen;

	for (i = 0; ConfigureNamesLong[i].gen.name; i++)
		all_vars[num_vars++] = &ConfigureNamesLong[i].gen;

	for (i = 0; ConfigureNamesDouble[i].gen.name; i++)
		all_vars[num_vars++] = &ConfigureNamesDouble[i].gen;

	for (i = 0; ConfigureNamesString[i].gen.name; i++)
		all_vars[num_vars++] = &ConfigureNamesString[i].gen;

	for (i = 0; ConfigureNamesEnum[i].gen.name; i++)
		all_vars[num_vars++] = &ConfigureNamesEnum[i].gen;

	for (i = 0; ConfigureNamesStringList[i].gen.name; i++)
		all_vars[num_vars++] = &ConfigureNamesStringList[i].gen;

	for (i = 0; ConfigureNamesStringArray[i].gen.name; i++)
		all_vars[num_vars++] = &ConfigureNamesStringArray[i].gen;

	for (i = 0; ConfigureNamesIntArray[i].gen.name; i++)
		all_vars[num_vars++] = &ConfigureNamesIntArray[i].gen;

	for (i = 0; ConfigureNamesDoubleArray[i].gen.name; i++)
		all_vars[num_vars++] = &ConfigureNamesDoubleArray[i].gen;

	if (all_parameters)
		pfree(all_parameters);
	all_parameters = all_vars;
	num_all_parameters = num_vars;
	sort_config_vars();
	build_variable_groups();
}

static void
build_variable_groups(void)
{
	/* we build these by hand */
	/* group 1. Backend config vars */
	ConfigureVarGroups[0].var_count = 6;
	ConfigureVarGroups[0].var_list = palloc0(sizeof(struct config_generic *) * ConfigureVarGroups[0].var_count);
	ConfigureVarGroups[0].var_list[0] = find_option("backend_hostname", FATAL);
	ConfigureVarGroups[0].var_list[0]->flags |= VAR_PART_OF_GROUP;
	ConfigureVarGroups[0].var_list[1] = find_option("backend_port", FATAL);
	ConfigureVarGroups[0].var_list[1]->flags |= VAR_PART_OF_GROUP;
	ConfigureVarGroups[0].var_list[2] = find_option("backend_weight", FATAL);
	ConfigureVarGroups[0].var_list[2]->flags |= VAR_PART_OF_GROUP;
	ConfigureVarGroups[0].var_list[3] = find_option("backend_data_directory", FATAL);
	ConfigureVarGroups[0].var_list[3]->flags |= VAR_PART_OF_GROUP;
	ConfigureVarGroups[0].var_list[4] = find_option("backend_application_name", FATAL);
	ConfigureVarGroups[0].var_list[4]->flags |= VAR_PART_OF_GROUP;
	ConfigureVarGroups[0].var_list[5] = find_option("backend_flag", FATAL);
	ConfigureVarGroups[0].var_list[5]->flags |= VAR_PART_OF_GROUP;
	ConfigureVarGroups[0].gen.max_elements = ConfigureVarGroups[0].var_list[0]->max_elements;

	/* group 2. other_pgpool config vars */
	ConfigureVarGroups[1].var_count = 3;
	ConfigureVarGroups[1].var_list = palloc0(sizeof(struct config_generic *) * ConfigureVarGroups[1].var_count);
	/* backend hostname */
	ConfigureVarGroups[1].var_list[0] = find_option("hostname", FATAL);
	ConfigureVarGroups[1].var_list[0]->flags |= VAR_PART_OF_GROUP;
	ConfigureVarGroups[1].var_list[1] = find_option("pgpool_port", FATAL);
	ConfigureVarGroups[1].var_list[1]->flags |= VAR_PART_OF_GROUP;
	ConfigureVarGroups[1].var_list[2] = find_option("wd_port", FATAL);
	ConfigureVarGroups[1].var_list[2]->flags |= VAR_PART_OF_GROUP;
	ConfigureVarGroups[1].gen.max_elements = ConfigureVarGroups[1].var_list[0]->max_elements;


	/* group 3. heartbeat config vars */
	ConfigureVarGroups[2].var_count = 3;
	ConfigureVarGroups[2].var_list = palloc0(sizeof(struct config_generic *) * ConfigureVarGroups[2].var_count);
	/* backend hostname */
	ConfigureVarGroups[2].var_list[0] = find_option("heartbeat_device", FATAL);
	ConfigureVarGroups[2].var_list[0]->flags |= VAR_PART_OF_GROUP;
	ConfigureVarGroups[2].var_list[1] = find_option("heartbeat_hostname", FATAL);
	ConfigureVarGroups[2].var_list[1]->flags |= VAR_PART_OF_GROUP;
	ConfigureVarGroups[2].var_list[2] = find_option("heartbeat_port", FATAL);
	ConfigureVarGroups[2].var_list[2]->flags |= VAR_PART_OF_GROUP;
	ConfigureVarGroups[2].gen.max_elements = ConfigureVarGroups[2].var_list[0]->max_elements;

	/* group 4. health_check config vars */
	ConfigureVarGroups[3].var_count = 8;
	ConfigureVarGroups[3].var_list = palloc0(sizeof(struct config_generic *) * ConfigureVarGroups[3].var_count);
	ConfigureVarGroups[3].var_list[0] = find_option("health_check_period", FATAL);
	ConfigureVarGroups[3].var_list[0]->flags |= VAR_PART_OF_GROUP;
	ConfigureVarGroups[3].var_list[1] = find_option("health_check_timeout", FATAL);
	ConfigureVarGroups[3].var_list[1]->flags |= VAR_PART_OF_GROUP;
	ConfigureVarGroups[3].var_list[2] = find_option("health_check_user", FATAL);
	ConfigureVarGroups[3].var_list[2]->flags |= VAR_PART_OF_GROUP;
	ConfigureVarGroups[3].var_list[3] = find_option("health_check_password", FATAL);
	ConfigureVarGroups[3].var_list[3]->flags |= VAR_PART_OF_GROUP;
	ConfigureVarGroups[3].var_list[4] = find_option("health_check_database", FATAL);
	ConfigureVarGroups[3].var_list[4]->flags |= VAR_PART_OF_GROUP;
	ConfigureVarGroups[3].var_list[5] = find_option("health_check_max_retries", FATAL);
	ConfigureVarGroups[3].var_list[5]->flags |= VAR_PART_OF_GROUP;
	ConfigureVarGroups[3].var_list[6] = find_option("health_check_retry_delay", FATAL);
	ConfigureVarGroups[3].var_list[6]->flags |= VAR_PART_OF_GROUP;
	ConfigureVarGroups[3].var_list[7] = find_option("connect_timeout", FATAL);
	ConfigureVarGroups[3].var_list[7]->flags |= VAR_PART_OF_GROUP;
	ConfigureVarGroups[3].gen.max_elements = ConfigureVarGroups[3].var_list[0]->max_elements;

}

/* Sort the config variables on based of string length of
 * variable names. Since we want to compare long variable names to be compared
 * before the smaller ones. This ensure heartbeat_destination should not match
 * with heartbeat_destination_port, when we use strncmp to cater for the index at
 * the end of the variable names.*/
static void
sort_config_vars(void)
{
	int			i,
				j;

	for (i = 0; i < num_all_parameters; ++i)
	{
		struct config_generic *gconfi = all_parameters[i];
		int			leni = strlen(gconfi->name);

		for (j = i + 1; j < num_all_parameters; ++j)
		{
			struct config_generic *gconfj = all_parameters[j];
			int			lenj = strlen(gconfj->name);

			if (leni < lenj)
			{
				all_parameters[i] = gconfj;
				all_parameters[j] = gconfi;
				gconfi = all_parameters[i];
				leni = lenj;
			}
		}
	}
}

/*
 * Initialize all variables to its compiled-in default.
 */
static void
initialize_variables_with_default(struct config_generic *gconf)
{
	int			i;

	for (i = 0; i < gconf->max_elements; i++)
	{
		gconf->sources[i] = PGC_S_DEFAULT;
		gconf->scontexts[i] = CFGCXT_BOOT;
		gconf->reset_sources[i] = gconf->sources[i];
	}
	gconf->sourceline = 0;

	/* Also set the default value for index free record if any */
	if (gconf->dynamic_array_var && gconf->flags & ARRAY_VAR_ALLOW_NO_INDEX)
	{
		struct config_generic *idx_free_record = get_index_free_record_if_any(gconf);

		if (idx_free_record)
		{
			ereport(DEBUG1,
					(errmsg("setting array element defaults for parameter \"%s\"",
							gconf->name)));
			initialize_variables_with_default(idx_free_record);
		}
	}

	switch (gconf->vartype)
	{
		case CONFIG_VAR_TYPE_GROUP: /* just to keep compiler quite */
			break;

		case CONFIG_VAR_TYPE_BOOL:
			{
				struct config_bool *conf = (struct config_bool *) gconf;
				bool		newval = conf->boot_val;

				if (conf->assign_func)
				{
					(*conf->assign_func) (CFGCXT_BOOT, newval, ERROR);
				}
				else
				{
					*conf->variable = conf->reset_val = newval;
				}
				break;
			}

		case CONFIG_VAR_TYPE_INT:
			{
				struct config_int *conf = (struct config_int *) gconf;
				int			newval = conf->boot_val;

				if (conf->assign_func)
				{
					(*conf->assign_func) (CFGCXT_BOOT, newval, ERROR);
				}
				else
				{
					*conf->variable = newval;
				}
				conf->reset_val = newval;

				break;
			}

		case CONFIG_VAR_TYPE_DOUBLE:
			{
				struct config_double *conf = (struct config_double *) gconf;
				double		newval = conf->boot_val;

				if (conf->assign_func)
				{
					(*conf->assign_func) (CFGCXT_BOOT, newval, ERROR);
				}
				else
				{
					*conf->variable = newval;
				}
				conf->reset_val = newval;

				break;
			}

		case CONFIG_VAR_TYPE_INT_ARRAY:
			{
				struct config_int_array *conf = (struct config_int_array *) gconf;
				int			newval = conf->boot_val;
				int			i;

				for (i = 0; i < gconf->max_elements; i++)
				{
					if (conf->assign_func)
					{
						(*conf->assign_func) (CFGCXT_BOOT, newval, i, ERROR);
					}
					else
					{
						*conf->variable[i] = newval;
					}
					conf->reset_vals[i] = newval;
				}
				break;
			}

		case CONFIG_VAR_TYPE_DOUBLE_ARRAY:
			{
				struct config_double_array *conf = (struct config_double_array *) gconf;
				double		newval = conf->boot_val;
				int			i;

				for (i = 0; i < gconf->max_elements; i++)
				{
					if (conf->assign_func)
					{
						(*conf->assign_func) (CFGCXT_BOOT, newval, i, ERROR);
					}
					else
					{
						*conf->variable[i] = newval;
					}
					conf->reset_vals[i] = newval;
				}
				break;
			}

		case CONFIG_VAR_TYPE_LONG:
			{
				struct config_long *conf = (struct config_long *) gconf;
				long		newval = conf->boot_val;

				if (conf->assign_func)
				{
					(*conf->assign_func) (CFGCXT_BOOT, newval, ERROR);
				}
				else
				{
					*conf->variable = conf->reset_val = newval;
				}
				break;
			}

		case CONFIG_VAR_TYPE_STRING:
			{
				struct config_string *conf = (struct config_string *) gconf;
				char	   *newval = (char *) conf->boot_val;

				if (conf->assign_func)
				{
					(*conf->assign_func) (CFGCXT_BOOT, newval, ERROR);
				}
				else
				{
					*conf->variable = newval ? pstrdup(newval) : NULL;
				}

				conf->reset_val = newval ? pstrdup(newval) : NULL;

				if (conf->process_func)
				{
					(*conf->process_func) (newval, ERROR);
				}
				break;
			}

		case CONFIG_VAR_TYPE_STRING_ARRAY:
			{
				struct config_string_array *conf = (struct config_string_array *) gconf;
				char	   *newval = (char *) conf->boot_val;
				int			i;

				for (i = 0; i < gconf->max_elements; i++)
				{

					if (conf->assign_func)
					{
						(*conf->assign_func) (CFGCXT_BOOT, newval, i, ERROR);
					}
					else
					{
						*conf->variable[i] = newval ? pstrdup(newval) : NULL;
					}

					if (newval)
						conf->reset_vals[i] = pstrdup(newval);
				}
				break;
			}

		case CONFIG_VAR_TYPE_ENUM:
			{
				struct config_enum *conf = (struct config_enum *) gconf;
				int			newval = conf->boot_val;

				if (conf->assign_func)
				{
					(*conf->assign_func) (CFGCXT_BOOT, newval, ERROR);
				}
				else
				{
					*conf->variable = newval;
				}
				conf->reset_val = newval;

				if (conf->process_func)
				{
					(*conf->process_func) (newval, ERROR);
				}

				break;
			}

		case CONFIG_VAR_TYPE_STRING_LIST:
			{
				struct config_string_list *conf = (struct config_string_list *) gconf;
				char	   *newval = (char *) conf->boot_val;

				if (conf->assign_func)
				{
					(*conf->assign_func) (CFGCXT_BOOT, newval, ERROR);
				}
				else
				{
					if (strcmp(gconf->name, "primary_routing_query_pattern_list") == 0)
					{
						*conf->variable = get_list_from_string_regex_delim(newval, conf->separator, conf->list_elements_count);
					}
					else
					{
						*conf->variable = get_list_from_string(newval, conf->separator, conf->list_elements_count);
					}

					if (conf->compute_regex)
					{
						int			i;

						for (i = 0; i < *conf->list_elements_count; i++)
						{
							add_regex_pattern((const char *) conf->gen.name, (*conf->variable)[i]);
						}
					}
				}
				/* save the string value */
				conf->reset_val = newval ? pstrdup(newval) : NULL;

				if (conf->current_val)
					pfree(conf->current_val);

				conf->current_val = newval ? pstrdup(newval) : NULL;
				break;
			}

	}
}

/*
 * Extract tokens separated by delimi from str. Return value is an
 * array of pointers in pallocd strings. number of elements are set to
 * n.
 */
static char **
get_list_from_string(const char *str, const char *delimi, int *n)
{
	char	   *token;
	char	  **tokens;
	char	   *temp_string;
	const int	MAXTOKENS = 256;

	*n = 0;

	if (str == NULL || *str == '\0')
		return NULL;

	temp_string = pstrdup(str);
	tokens = palloc(MAXTOKENS * sizeof(char *));

	ereport(DEBUG3,
			(errmsg("extracting string tokens from [%s] based on %s", temp_string, delimi)));

	for (token = strtok(temp_string, delimi); token != NULL; token = strtok(NULL, delimi))
	{
		tokens[*n] = pstrdup(token);
		ereport(DEBUG3,
				(errmsg("initializing pool configuration"),
				 errdetail("extracting string tokens [token[%d]: %s]", *n, tokens[*n])));

		(*n)++;

		if (((*n) % MAXTOKENS) == 0)
		{
			tokens = repalloc(tokens, (MAXTOKENS * sizeof(char *) * (((*n) / MAXTOKENS) + 1)));
		}
	}
	/* how about reclaiming the unused space */
	if (*n > 0)
		tokens = repalloc(tokens, (sizeof(char *) * (*n)));
	pfree(temp_string);

	return tokens;
}

/*
 * Extract tokens separated by delimiter from str. Allow to
 * use regex of delimiter. Return value is an array of
 * pointers in pallocd strings. number of elements are set
 * to n.
 */
static char **
get_list_from_string_regex_delim(const char *input, const char *delimi, int *n)
{
#ifndef POOL_PRIVATE
	int			j = 0;
	char	   *str;
	char	   *buf,
			   *str_temp;
	char	  **tokens;
	const int	MAXTOKENS = 256;

	*n = 0;

	if (input == NULL || *input == '\0')
		return NULL;

	tokens = palloc(MAXTOKENS * sizeof(char *));
	if (*(input + strlen(input) - 1) != *delimi || *(input + strlen(input) - 2) == '\\')
	{
		int			len = strlen(input) + 2;

		str = palloc(len);
		snprintf(str, len, "%s;", input);
	}
	else
	{
		str = pstrdup(input);
	}

	buf = str;
	str_temp = str;

	while (*str_temp != '\0')
	{
		if (*str_temp == '\\')
		{
			j += 2;
			str_temp++;
		}
		else if (*str_temp == *delimi)
		{
			char *output = (char *) palloc(j + 1);
			StrNCpy(output, buf, j + 1);

			/* replace escape character of "'" */
			tokens[*n] = string_replace(output, "\\'", "'");
			pfree(output);
			ereport(DEBUG3,
					(errmsg("initializing pool configuration"),
					 errdetail("extracting string tokens [token[%d]: %s]", *n, tokens[*n])));

			(*n)++;
			buf = str_temp + 1;
			j = 0;

			if (((*n) % MAXTOKENS) == 0)
				tokens = repalloc(tokens, (MAXTOKENS * sizeof(char *) * (((*n) / MAXTOKENS) + 1)));
		}
		else
		{
			j++;
		}
		str_temp++;
	}

	if (*n > 0)
		tokens = repalloc(tokens, (sizeof(char *) * (*n)));

	pfree(str);

	return tokens;
#else
	return NULL;
#endif
}

/*
 * Memory of the array type variables must be initialized before calling this function
 */
void
InitializeConfigOptions(void)
{
	int			i;

	/*
	 * Before we do anything set the log_min_messages to ERROR. Reason for
	 * doing that is before the log_min_messages gets initialized with the
	 * actual value the pgpool-II log should not get flooded by DEBUG messages
	 */
	g_pool_config.log_min_messages = ERROR;
	g_pool_config.syslog_facility = LOG_LOCAL0;
	build_config_variables();

	/*
	 * Load all variables with their compiled-in defaults, and initialize
	 * status fields as needed.
	 */
	for (i = 0; i < num_all_parameters; i++)
	{
		initialize_variables_with_default(all_parameters[i]);
	}
	/* do the post processing */
	config_post_processor(CFGCXT_BOOT, FATAL);
}

/*
 * returns the index value postfixed with the variable name
 * for example if the if name contains "backend_hostname11" and
 * the record name must be for the variable named "backend_hostname"
 * if the index is not present at end of the name the function
 * will return true and out parameter index will be assigned with -ve value
 */
static bool
get_index_in_var_name(struct config_generic *record, const char *name, int *index, int elevel)
{
	char	   *ptr;
	int			index_start_index = strlen(record->name);

	if (strlen(name) <= index_start_index)
	{
		/* no index is provided */
		*index = -1;
		return true;
	}
	ptr = (char *) (name + index_start_index);
	while (*ptr)
	{
		if (isdigit(*ptr) == 0)
		{
			ereport(elevel,
					(errmsg("invalid index value for parameter \"%s\" ", name),
					 (errdetail("index part contains the invalid non digit character"))));
			return false;
		}
		ptr++;
	}
	*index = atoi(name + index_start_index);
	return true;
}

/*
 * Look up option NAME.  If it exists, return a pointer to its record,
 * else return NULL.
 */
static struct config_generic *
find_option(const char *name, int elevel)
{
	int			i;

	for (i = 0; i < num_all_parameters; i++)
	{
		struct config_generic *gconf = all_parameters[i];

		if (gconf->dynamic_array_var)
		{
			int			index_start_index = strlen(gconf->name);

			/*
			 * For dynamic array type vars the key also have the index at the
			 * end e.g. backend_hostname0 so we only compare the key's name
			 * part
			 */
			if (!strncmp(gconf->name, name, index_start_index))
				return gconf;
		}
		else
		{
			if (!strcmp(gconf->name, name))
				return gconf;
		}
	}
	/* Unknown name */
	return NULL;
}

/*
 * Lookup the value for an enum option with the selected name
 * (case-insensitive).
 * If the enum option is found, sets the retval value and returns
 * true. If it's not found, return FALSE and retval is set to 0.
 */
static bool
config_enum_lookup_by_name(struct config_enum *record, const char *value,
						   int *retval)
{
	const struct config_enum_entry *entry;

	for (entry = record->options; entry && entry->name; entry++)
	{
		if (strcasecmp(value, entry->name) == 0)
		{
			*retval = entry->val;
			return TRUE;
		}
	}

	*retval = 0;
	return FALSE;
}


bool
set_config_options(ConfigVariable *head_p,
				   ConfigContext context, GucSource source, int elevel)
{
	ConfigVariable *item = head_p;

	while (item)
	{
		ConfigVariable *next = item->next;

		setConfigOption(item->name, item->value, context, source, elevel);
		item = next;
	}
	return config_post_processor(context, elevel);
}

bool
set_one_config_option(const char *name, const char *value,
					  ConfigContext context, GucSource source, int elevel)
{
	if (setConfigOption(name, value, context, source, elevel) == true)
		return config_post_processor(context, elevel);
	return false;
}

static bool
setConfigOption(const char *name, const char *value,
				ConfigContext context, GucSource source, int elevel)
{
	struct config_generic *record;
	int			index_val = -1;

	ereport(DEBUG2,
			(errmsg("set_config_option \"%s\" = \"%s\"", name, value)));

	record = find_option(name, elevel);

	if (record == NULL)
	{
		/*
		 * we emit only INFO message when setting the option from
		 * configuration file. As the conf file may still contain some
		 * configuration parameters only exist in older version and does not
		 * exist anymore
		 */
		ereport(source == PGC_S_FILE ? INFO : elevel,
				(errmsg("unrecognized configuration parameter \"%s\"", name)));
		return false;
	}
	if (record->dynamic_array_var)
	{
		if (get_index_in_var_name(record, name, &index_val, elevel) == false)
			return false;

		if (index_val < 0)
		{
			/* index is not provided */
			if (record->flags & ARRAY_VAR_ALLOW_NO_INDEX)
			{
				ereport(DEBUG2,
						(errmsg("parameter \"%s\" is an array type variable and allows index-free value as well",
								name)));
			}
			else
			{
				ereport(elevel,
						(errmsg("parameter \"%s\" expects the index value",
								name)));
				return false;
			}
		}
	}

	return setConfigOptionVar(record, name, index_val, value, context, source, elevel);
}

static bool
setConfigOptionArrayVarWithConfigDefault(struct config_generic *record, const char *name,
										 const char *value, ConfigContext context, int elevel)
{
	int			index;

	if (record->dynamic_array_var == false)
	{
		ereport(elevel,
				(errmsg("parameter \"%s\" is not the array type configuration parameter",
						name)));
		return false;
	}
	if (!(record->flags & ARRAY_VAR_ALLOW_NO_INDEX))
	{
		ereport(elevel,
				(errmsg("parameter \"%s\" does not allow value type default",
						name)));
		return false;
	}

	for (index = 0; index < record->max_elements; index++)
	{
		/*
		 * only elements having the values from the default source can be
		 * updated
		 */
		if (record->sources[index] != PGC_S_DEFAULT
			&& record->sources[index] != PGC_S_VALUE_DEFAULT)
			continue;
		setConfigOptionVar(record, record->name, index, value,
						   CFGCXT_INIT, PGC_S_VALUE_DEFAULT, elevel);
	}
	return true;
}

static bool
setConfigOptionVar(struct config_generic *record, const char *name, int index_val, const char *value,
				   ConfigContext context, GucSource source, int elevel)
{
	bool		reset = false;

	switch (record->context)
	{
		case CFGCXT_BOOT:
			if (context != CFGCXT_BOOT)
			{
				if (context == CFGCXT_RELOAD)
				{
					/*
					 * Do not treat it as an error. Since the RELOAD context
					 * is used by reload config mechanism of pgpool-II and the
					 * configuration file always contain all the values,
					 * including those that are not allowed to be changed in
					 * reload context. So silently ignoring this for the time
					 * being is the best way to go until we enhance the logic
					 * around this
					 */
					ereport(DEBUG2,
							(errmsg("invalid Context, value for parameter \"%s\" cannot be changed",
									name)));
					return true;
				}

				ereport(elevel,
						(errmsg("invalid Context, value for parameter \"%s\" cannot be changed",
								name)));
				return false;
			}
			break;
		case CFGCXT_INIT:
			if (context != CFGCXT_INIT && context != CFGCXT_BOOT)
			{
				if (context == CFGCXT_RELOAD)
				{
					ereport(DEBUG2,
							(errmsg("invalid Context, value for parameter \"%s\" cannot be changed",
									name)));
					return true;
				}

				ereport(elevel,
						(errmsg("invalid Context, value for parameter \"%s\" cannot be changed",
								name)));
				return false;
			}
			break;
		case CFGCXT_RELOAD:
			if (context > CFGCXT_RELOAD)
			{
				ereport(elevel,
						(errmsg("invalid Context, value for parameter \"%s\" cannot be changed",
								name)));
				return false;
			}
			break;
		case CFGCXT_PCP:
			if (context > CFGCXT_PCP)
			{
				ereport(elevel,
						(errmsg("invalid Context, value for parameter \"%s\" cannot be changed",
								name)));
				return false;
			}
			break;

		case CFGCXT_SESSION:
			break;

		default:
			{
				ereport(elevel,
						(errmsg("invalid record context, value for parameter \"%s\" cannot be changed",
								name)));
				return false;
			}
			break;
	}

	if (record->dynamic_array_var)
	{
		if (index_val < 0)
		{
			ereport(DEBUG1,
					(errmsg("setting value no index value for parameter \"%s\" source = %d",
							name, source)));

			if (record->flags & ARRAY_VAR_ALLOW_NO_INDEX)
			{
				struct config_generic *idx_free_record = get_index_free_record_if_any(record);

				if (idx_free_record)
				{
					bool		ret;

					ereport(DEBUG1,
							(errmsg("parameter \"%s\" is an array type variable and allows index-free value as well",
									name)));
					ret = setConfigOptionVar(idx_free_record, name, index_val, value,
											 context, source, elevel);
					if (idx_free_record->flags & DEFAULT_FOR_NO_VALUE_ARRAY_VAR)
					{
						const char *newVal;

						/* we need to update the default values */
						ereport(DEBUG2,
								(errmsg("modifying the array index values for parameter \"%s\" source = %d",
										name, source)));
#ifndef POOL_PRIVATE
						if (value == NULL)
						{
							newVal = ShowOption(idx_free_record, -1, elevel);
						}
						else
#endif
							newVal = value;

						setConfigOptionArrayVarWithConfigDefault(record, name, newVal,
																 context, elevel);
					}
					return ret;
				}
			}
			/* index is not provided */
			ereport(elevel,
					(errmsg("parameter \"%s\" expects the index postfix",
							name)));
			return false;
		}
	}

	/*
	 * Evaluate value and set variable.
	 */
	switch (record->vartype)
	{
		case CONFIG_VAR_TYPE_GROUP:
			ereport(ERROR, (errmsg("invalid config variable type. operation not allowed")));
			break;

		case CONFIG_VAR_TYPE_BOOL:
			{
				struct config_bool *conf = (struct config_bool *) record;
				bool		newval = conf->boot_val;

				if (value != NULL)
				{
					newval = eval_logical(value);

				}
				else if (source == PGC_S_DEFAULT)
				{
					newval = conf->boot_val;
				}
				else
				{
					/* Reset */
					newval = conf->reset_val;
					reset = true;
				}
				if (conf->assign_func)
				{
					if ((*conf->assign_func) (context, newval, elevel) == false)
						return false;
				}
				else
					*conf->variable = newval;

				if (context == CFGCXT_INIT)
					conf->reset_val = newval;
			}
			break;

		case CONFIG_VAR_TYPE_INT:
			{
				struct config_int *conf = (struct config_int *) record;
				int			newval;

				if (value != NULL)
				{
					int64 newval64;
					const char *hintmsg;

					if (!parse_int(value, &newval64,
								   conf->gen.flags, &hintmsg, INT_MAX))
					{
						ereport(elevel,
								(errmsg("invalid value for parameter \"%s\": \"%s\"",
										name, value),
								 hintmsg ? errhint("%s", _(hintmsg)) : 0));
						return false;
					}
					newval = (int)newval64;
				}
				else if (source == PGC_S_DEFAULT)
				{
					newval = conf->boot_val;
				}
				else
				{
					/* Reset */
					newval = conf->reset_val;
					reset = true;
				}

				if (newval < conf->min || newval > conf->max)
				{
					ereport(elevel,
							(errmsg("%d is outside the valid range for parameter \"%s\" (%d .. %d)",
									newval, name,
									conf->min, conf->max)));
					return false;
				}

				if (conf->assign_func)
				{
					if ((*conf->assign_func) (context, newval, elevel) == false)
						return false;
				}
				else
				{
					*conf->variable = newval;
				}

				if (context == CFGCXT_INIT)
					conf->reset_val = newval;
			}
			break;

		case CONFIG_VAR_TYPE_DOUBLE:
			{
				struct config_double *conf = (struct config_double *) record;
				double		newval;

				if (value != NULL)
				{
					newval = atof(value);
				}
				else if (source == PGC_S_DEFAULT)
				{
					newval = conf->boot_val;
				}
				else
				{
					/* Reset */
					newval = conf->reset_val;
					reset = true;
				}

				if (newval < conf->min || newval > conf->max)
				{
					ereport(elevel,
							(errmsg("%f is outside the valid range for parameter \"%s\" (%f .. %f)",
									newval, name,
									conf->min, conf->max)));
					return false;
				}

				if (conf->assign_func)
				{
					if ((*conf->assign_func) (context, newval, elevel) == false)
						return false;
				}
				else
				{
					*conf->variable = newval;
				}

				if (context == CFGCXT_INIT)
					conf->reset_val = newval;

			}
			break;

		case CONFIG_VAR_TYPE_INT_ARRAY:
			{
				struct config_int_array *conf = (struct config_int_array *) record;
				int			newval;

				if (index_val < 0 || index_val > record->max_elements)
				{
					ereport(elevel,
							(errmsg("%d index outside the valid range for parameter \"%s\" (%d .. %d)",
									index_val, name,
									0, record->max_elements)));
					return false;
				}

				if (value != NULL)
				{
					int64 newval64;
					const char *hintmsg;

					if (!parse_int(value, &newval64,
								   conf->gen.flags, &hintmsg, INT_MAX))
					{
						ereport(elevel,
								(errmsg("invalid value for parameter \"%s\": \"%s\"",
										name, value),
								 hintmsg ? errhint("%s", _(hintmsg)) : 0));
						return false;
					}
					newval = (int)newval64;
				}
				else if (source == PGC_S_DEFAULT)
				{
					newval = conf->boot_val;
				}
				else
				{
					/* Reset */
					newval = conf->reset_vals[index_val];
					reset = true;
				}

				if (newval < conf->min || newval > conf->max)
				{
					ereport(elevel,
							(errmsg("%d is outside the valid range for parameter \"%s\" (%d .. %d)",
									newval, name,
									conf->min, conf->max)));
					return false;
				}

				if (conf->assign_func)
				{
					if ((*conf->assign_func) (context, newval, index_val, elevel) == false)
						return false;
				}
				else
				{
					*conf->variable[index_val] = newval;
				}

				if (context == CFGCXT_INIT)
					conf->reset_vals[index_val] = newval;

			}
			break;

		case CONFIG_VAR_TYPE_DOUBLE_ARRAY:
			{
				struct config_double_array *conf = (struct config_double_array *) record;
				double		newval;

				if (index_val < 0 || index_val > record->max_elements)
				{
					ereport(elevel,
							(errmsg("%d index outside the valid range for parameter \"%s\" (%d .. %d)",
									index_val, name,
									0, record->max_elements)));
					return false;
				}

				if (value != NULL)
				{
					newval = atof(value);
				}
				else if (source == PGC_S_DEFAULT)
				{
					newval = conf->boot_val;
				}
				else
				{
					/* Reset */
					newval = conf->reset_vals[index_val];
					reset = true;
				}

				if (newval < conf->min || newval > conf->max)
				{
					ereport(elevel,
							(errmsg("%f is outside the valid range for parameter \"%s\" (%f .. %f)",
									newval, name,
									conf->min, conf->max)));
					return false;
				}

				if (conf->assign_func)
				{
					if ((*conf->assign_func) (context, newval, index_val, elevel) == false)
						return false;
				}
				else
				{
					*conf->variable[index_val] = newval;
				}

				if (context == CFGCXT_INIT)
					conf->reset_vals[index_val] = newval;

			}
			break;


		case CONFIG_VAR_TYPE_LONG:
			{
				struct config_long *conf = (struct config_long *) record;
				int64		newval;

				if (value != NULL)
				{
					const char *hintmsg;

					if (!parse_int(value, &newval,
								   conf->gen.flags, &hintmsg, conf->max))
					{
						ereport(elevel,
								(errmsg("invalid value for parameter \"%s\": \"%s\"",
										name, value),
								 hintmsg ? errhint("%s", _(hintmsg)) : 0));
						return false;
					}
				}
				else if (source == PGC_S_DEFAULT)
				{
					newval = conf->boot_val;
				}
				else
				{
					/* Reset */
					newval = conf->reset_val;
					reset = true;
				}

				if (newval < conf->min || newval > conf->max)
				{
					ereport(elevel,
							(errmsg("%ld is outside the valid range for parameter \"%s\" (%ld .. %ld)",
									newval, name,
									conf->min, conf->max)));
					return false;
				}

				if (conf->assign_func)
				{
					if ((*conf->assign_func) (context, newval, elevel) == false)
						return false;
				}
				else
				{
					*conf->variable = newval;
				}

				if (context == CFGCXT_INIT)
					conf->reset_val = newval;

			}
			break;

		case CONFIG_VAR_TYPE_STRING:
			{
				struct config_string *conf = (struct config_string *) record;
				char	   *newval = NULL;

				if (value != NULL)
				{
					newval = (char *) value;
				}
				else if (source == PGC_S_DEFAULT)
				{
					newval = (char *) conf->boot_val;
				}
				else
				{
					/* Reset */
					newval = conf->reset_val;
					reset = true;
				}

				if (conf->assign_func)
				{
					if ((*conf->assign_func) (context, newval, elevel) == false)
						return false;
				}
				else
				{
					if (*conf->variable)
						pfree(*conf->variable);

					*conf->variable = newval ? pstrdup(newval) : NULL;
				}
				if (context == CFGCXT_INIT)
				{
					if (conf->reset_val)
						pfree(conf->reset_val);

					conf->reset_val = newval ? pstrdup(newval) : NULL;
				}
				if (conf->process_func)
				{
					(*conf->process_func) (newval, elevel);
				}
			}
			break;

		case CONFIG_VAR_TYPE_STRING_ARRAY:
			{
				struct config_string_array *conf = (struct config_string_array *) record;
				char	   *newval = NULL;

				if (index_val < 0 || index_val > record->max_elements)
				{
					ereport(elevel,
							(errmsg("%d index outside the valid range for parameter \"%s\" (%d .. %d)",
									index_val, name,
									0, record->max_elements)));
					return false;
				}

				if (value != NULL)
				{
					newval = (char *) value;
				}
				else if (source == PGC_S_DEFAULT)
				{
					newval = (char *) conf->boot_val;
				}
				else
				{
					/* Reset */
					newval = conf->reset_vals[index_val];
					reset = true;
				}

				if (conf->assign_func)
				{
					if ((*conf->assign_func) (context, newval, index_val, elevel) == false)
						return false;
				}
				else
				{
					if (*conf->variable[index_val])
						pfree(*conf->variable[index_val]);

					*conf->variable[index_val] = newval ? pstrdup(newval) : NULL;
				}
				if (context == CFGCXT_INIT)
				{
					if (conf->reset_vals[index_val])
						pfree(conf->reset_vals[index_val]);

					conf->reset_vals[index_val] = newval ? pstrdup(newval) : NULL;
				}
			}
			break;

		case CONFIG_VAR_TYPE_STRING_LIST:
			{
				struct config_string_list *conf = (struct config_string_list *) record;
				char	   *newval = NULL;

				if (value != NULL)
				{
					newval = (char *) value;
				}
				else if (source == PGC_S_DEFAULT)
				{
					newval = (char *) conf->boot_val;
				}
				else
				{
					/* Reset */
					newval = conf->reset_val;
					reset = true;
				}

				if (conf->assign_func)
				{
					if ((*conf->assign_func) (context, newval, elevel) == false)
					{
						return false;
					}
				}
				else
				{
					if (*conf->variable)
					{
						int			i;

						for (i = 0; i < *conf->list_elements_count; i++)
						{
							if ((*conf->variable)[i])
								pfree((*conf->variable)[i]);
							(*conf->variable)[i] = NULL;
						}
						pfree(*conf->variable);
					}

					if (strcmp(name, "primary_routing_query_pattern_list") == 0)
					{
						*conf->variable = get_list_from_string_regex_delim(newval, conf->separator, conf->list_elements_count);
					}
					else
					{
						*conf->variable = get_list_from_string(newval, conf->separator, conf->list_elements_count);
					}

					if (conf->compute_regex)
					{
						/* TODO clear the old regex array please */
						int			i;

						for (i = 0; i < *conf->list_elements_count; i++)
						{
							add_regex_pattern(conf->gen.name, (*conf->variable)[i]);
						}
					}
				}

				if (context == CFGCXT_INIT)
				{
					if (conf->reset_val)
						pfree(conf->reset_val);

					conf->reset_val = newval ? pstrdup(newval) : NULL;
				}

				/* save the string value */
				if (conf->current_val)
					pfree(conf->current_val);

				conf->current_val = newval ? pstrdup(newval) : NULL;
			}
			break;

		case CONFIG_VAR_TYPE_ENUM:
			{
				struct config_enum *conf = (struct config_enum *) record;
				int			newval;

				if (value != NULL)
				{
					newval = atoi(value);
				}
				else if (source == PGC_S_DEFAULT)
				{
					newval = conf->boot_val;
				}
				else
				{
					/* Reset */
					newval = conf->reset_val;
					reset = true;
				}

				if (value && !config_enum_lookup_by_name(conf, value, &newval))
				{

					char	   *hintmsg = NULL;

#ifndef POOL_PRIVATE
					hintmsg = config_enum_get_options(conf,
													  "Available values: ",
													  ".", ", ");
#endif

					ereport(elevel,
							(errmsg("invalid value for parameter \"%s\": \"%s\"",
									name, value),
							 hintmsg ? errhint("%s", hintmsg) : 0));

					if (hintmsg)
						pfree(hintmsg);
					return false;
				}

				if (conf->assign_func)
				{
					if ((*conf->assign_func) (context, newval, elevel) == false)
						return false;
				}
				else
				{
					*conf->variable = newval;
				}

				if (context == CFGCXT_INIT)
					conf->reset_val = newval;

				if (conf->process_func)
				{
					(*conf->process_func) (newval, elevel);
				}

				break;
			}
	}
	if (record->dynamic_array_var)
	{
		if (index_val < 0 || index_val > record->max_elements)
		{
			ereport(elevel,
					(errmsg("%d index outside the valid range for parameter \"%s\" (%d .. %d)",
							index_val, name,
							0, record->max_elements)));
			return false;
		}
	}
	else
	{
		index_val = 0;
	}

	record->scontexts[index_val] = context;

	if (reset)
		record->sources[index_val] = record->reset_sources[index_val];
	else
		record->sources[index_val] = source;

	if (context == CFGCXT_INIT)
		record->reset_sources[index_val] = source;

	return true;
}

#ifndef POOL_PRIVATE

/*
 * Return a list of all available options for an enum, excluding
 * hidden ones, separated by the given separator.
 * If prefix is non-NULL, it is added before the first enum value.
 * If suffix is non-NULL, it is added to the end of the string.
 */
static char *
config_enum_get_options(struct config_enum *record, const char *prefix,
						const char *suffix, const char *separator)
{
	const struct config_enum_entry *entry;
	StringInfoData retstr;
	int			seplen;

	initStringInfo(&retstr);
	appendStringInfoString(&retstr, prefix);

	seplen = strlen(separator);
	for (entry = record->options; entry && entry->name; entry++)
	{
		if (!entry->hidden)
		{
			appendStringInfoString(&retstr, entry->name);
			appendBinaryStringInfo(&retstr, separator, seplen);
		}
	}

	/*
	 * All the entries may have been hidden, leaving the string empty if no
	 * prefix was given. This indicates a broken GUC setup, since there is no
	 * use for an enum without any values, so we just check to make sure we
	 * don't write to invalid memory instead of actually trying to do
	 * something smart with it.
	 */
	if (retstr.len >= seplen)
	{
		/* Replace final separator */
		retstr.data[retstr.len - seplen] = '\0';
		retstr.len -= seplen;
	}

	appendStringInfoString(&retstr, suffix);

	return retstr.data;
}
#endif

static bool
BackendWeightAssignFunc(ConfigContext context, double newval, int index, int elevel)
{
	double		old_v = g_pool_config.backend_desc->backend_info[index].unnormalized_weight;

	g_pool_config.backend_desc->backend_info[index].unnormalized_weight = newval;

	/*
	 * Log weight change event only when context is reloading of pgpool.conf
	 * and weight is actually changed
	 */
	if (context == CFGCXT_RELOAD && old_v != newval)
	{
		ereport(LOG,
				(errmsg("initializing pool configuration: backend weight for backend:%d changed from %f to %f", index, old_v, newval),
				 errdetail("This change will be effective from next client session")));
	}
	return true;
}


static bool
BackendPortAssignFunc(ConfigContext context, int newval, int index, int elevel)
{
	BACKEND_STATUS backend_status = g_pool_config.backend_desc->backend_info[index].backend_status;

	if (context <= CFGCXT_INIT)
	{
		g_pool_config.backend_desc->backend_info[index].backend_port = newval;
		g_pool_config.backend_desc->backend_info[index].backend_status = CON_CONNECT_WAIT;
	}
	else if (backend_status == CON_UNUSED)
	{
		g_pool_config.backend_desc->backend_info[index].backend_port = newval;
		g_pool_config.backend_desc->backend_info[index].backend_status = CON_DOWN;
	}
	else
	{
		if (context != CFGCXT_RELOAD)
			ereport(WARNING,
					(errmsg("backend_port%d cannot be changed in context %d and backend status = %d", index, context, backend_status)));
		return false;
	}
	return true;
}


static bool
BackendHostAssignFunc(ConfigContext context, char *newval, int index, int elevel)
{
	BACKEND_STATUS backend_status = g_pool_config.backend_desc->backend_info[index].backend_status;

	if (context <= CFGCXT_INIT || backend_status == CON_UNUSED)
	{
		if (newval == NULL || strlen(newval) == 0)
			g_pool_config.backend_desc->backend_info[index].backend_hostname[0] = '\0';
		else
			strlcpy(g_pool_config.backend_desc->backend_info[index].backend_hostname, newval, MAX_DB_HOST_NAMELEN - 1);
		return true;
	}
	/* silent the warning in reload contxt */
	if (context != CFGCXT_RELOAD)
		ereport(WARNING,
				(errmsg("backend_hostname%d cannot be changed in context %d and backend status = %d", index, context, backend_status)));
	return false;
}

static bool
BackendDataDirAssignFunc(ConfigContext context, char *newval, int index, int elevel)
{
	BACKEND_STATUS backend_status = g_pool_config.backend_desc->backend_info[index].backend_status;

	if (context <= CFGCXT_INIT || backend_status == CON_UNUSED || backend_status == CON_DOWN)
	{
		if (newval == NULL || strlen(newval) == 0)
			g_pool_config.backend_desc->backend_info[index].backend_data_directory[0] = '\0';
		else
			strlcpy(g_pool_config.backend_desc->backend_info[index].backend_data_directory, newval, MAX_PATH_LENGTH - 1);
		return true;
	}
	/* silent the warning in reload contxt */
	if (context != CFGCXT_RELOAD)
		ereport(WARNING,
				(errmsg("backend_data_directory%d cannot be changed in context %d and backend status = %d", index, context, backend_status)));
	return false;
}

static bool
BackendFlagsAssignFunc(ConfigContext context, char *newval, int index, int elevel)
{

	unsigned short flag;
	int			i,
				n;
	bool		allow_to_failover_is_specified = false;
	bool		disallow_to_failover_is_specified = false;
	char	  **flags;

	flag = g_pool_config.backend_desc->backend_info[index].flag;

	flags = get_list_from_string(newval, "|", &n);

	for (i = 0; i < n; i++)
	{
		int			k;

		if (!strcmp(flags[i], "ALLOW_TO_FAILOVER"))
		{
			if (disallow_to_failover_is_specified)
			{
				for (k = i; k < n; k++)
					pfree(flags[k]);
				pfree(flags);
				ereport(elevel,
						(errmsg("invalid configuration for key \"backend_flag%d\"", index),
						 errdetail("cannot set ALLOW_TO_FAILOVER and DISALLOW_TO_FAILOVER at the same time")));
				return false;
			}
			flag &= ~POOL_FAILOVER;
			allow_to_failover_is_specified = true;

		}

		else if (!strcmp(flags[i], "DISALLOW_TO_FAILOVER"))
		{
			if (allow_to_failover_is_specified)
			{
				for (k = i; k < n; k++)
					pfree(flags[k]);
				pfree(flags);

				ereport(elevel,
						(errmsg("invalid configuration for key \"backend_flag%d\"", index),
						 errdetail("cannot set ALLOW_TO_FAILOVER and DISALLOW_TO_FAILOVER at the same time")));
				return false;
			}
			flag |= POOL_FAILOVER;
			disallow_to_failover_is_specified = true;
		}

		else if ((!strcmp(flags[i], "ALWAYS_PRIMARY")))
		{
			flag |= POOL_ALWAYS_PRIMARY;
		}

		else
		{
			ereport(elevel,
					(errmsg("invalid configuration for key \"backend_flag%d\"", index),
					 errdetail("unknown backend flag:%s", flags[i])));
			for (k = i; k < n; k++)
				pfree(flags[k]);
			pfree(flags);
			return false;
		}
		pfree(flags[i]);
	}

	g_pool_config.backend_desc->backend_info[index].flag = flag;
	ereport(DEBUG1,
			(errmsg("setting \"backend_flag%d\" flag: %04x ", index, flag)));
	if (flags)
		pfree(flags);
	return true;
}

static bool
BackendAppNameAssignFunc(ConfigContext context, char *newval, int index, int elevel)
{
	if (newval == NULL || strlen(newval) == 0)
		g_pool_config.backend_desc->backend_info[index].backend_application_name[0] = '\0';
	else
		strlcpy(g_pool_config.backend_desc->backend_info[index].backend_application_name, newval, NAMEDATALEN - 1);
	return true;
}

static bool
LogDestinationProcessFunc(char *newval, int elevel)
{
#ifndef POOL_PRIVATE
	char	  **destinations;
	int			n,
				i;
	int			log_destination = 0;

	destinations = get_list_from_string(newval, ",", &n);
	if (!destinations || n < 0)
	{
		if (destinations)
			pfree(destinations);

		ereport(elevel,
				(errmsg("invalid value \"%s\" for log_destination", newval)));
		return false;
	}
	for (i = 0; i < n; i++)
	{
		if (!strcmp(destinations[i], "syslog"))
		{
			log_destination |= LOG_DESTINATION_SYSLOG;
		}
		else if (!strcmp(destinations[i], "stderr"))
		{
			log_destination |= LOG_DESTINATION_STDERR;
		}
		else
		{
			int			k;

			ereport(elevel,
					(errmsg("invalid configuration for \"log_destination\""),
					 errdetail("unknown destination :%s", destinations[i])));
			for (k = i; k < n; k++)
				pfree(destinations[k]);
			pfree(destinations);
			return false;
		}
		pfree(destinations[i]);
	}
	if (g_pool_config.log_destination & LOG_DESTINATION_SYSLOG)
	{
		if (!(log_destination & LOG_DESTINATION_SYSLOG))
			closelog();
	}
	g_pool_config.log_destination = log_destination;
	pfree(destinations);
#endif
	return true;
}

static bool
SyslogFacilityProcessFunc(int newval, int elevel)
{
#ifndef POOL_PRIVATE
#ifdef HAVE_SYSLOG
	/* set syslog parameters */
	set_syslog_parameters(g_pool_config.syslog_ident ? g_pool_config.syslog_ident : "pgpool",
						  g_pool_config.syslog_facility);
#endif
#endif
	return true;
}

static bool
SyslogIdentProcessFunc(char *newval, int elevel)
{
#ifndef POOL_PRIVATE
#ifdef HAVE_SYSLOG
	/* set syslog parameters */
	set_syslog_parameters(g_pool_config.syslog_ident ? g_pool_config.syslog_ident : "pgpool",
						  g_pool_config.syslog_facility);
#endif
#endif
	return true;
}

static const char *
IntValueShowFunc(int value)
{
	static char buffer[10];

	snprintf(buffer, sizeof(buffer), "%d", value);
	return buffer;
}

static const char *
FilePermissionShowFunc(int value)
{
	static char buffer[10];

	snprintf(buffer, sizeof(buffer), "%04o", value);
	return buffer;
}

static const char *
UnixSockPermissionsShowFunc(void)
{
	return FilePermissionShowFunc(g_pool_config.unix_socket_permissions);
}

static const char *
BackendWeightShowFunc(int index)
{
	return IntValueShowFunc(g_pool_config.backend_desc->backend_info[index].unnormalized_weight);
}

static const char *
BackendPortShowFunc(int index)
{
	return IntValueShowFunc(g_pool_config.backend_desc->backend_info[index].backend_port);
}

static const char *
BackendHostShowFunc(int index)
{
	return g_pool_config.backend_desc->backend_info[index].backend_hostname;
}

static const char *
BackendDataDirShowFunc(int index)
{
	return g_pool_config.backend_desc->backend_info[index].backend_data_directory;
}

static const char *
BackendFlagsShowFunc(int index)
{
	static char buffer[1024];

	unsigned short flag = g_pool_config.backend_desc->backend_info[index].flag;

	*buffer = '\0';

	if (POOL_ALLOW_TO_FAILOVER(flag))
		snprintf(buffer, sizeof(buffer), "ALLOW_TO_FAILOVER");
	else if (POOL_DISALLOW_TO_FAILOVER(flag))
		snprintf(buffer, sizeof(buffer), "DISALLOW_TO_FAILOVER");

	if (POOL_ALWAYS_PRIMARY & flag)
	{
		if (*buffer == '\0')
			snprintf(buffer, sizeof(buffer), "ALWAYS_PRIMARY");
		else
			snprintf(buffer+strlen(buffer), sizeof(buffer), "|ALWAYS_PRIMARY");
	}
	return buffer;
}

static const char *
BackendAppNameShowFunc(int index)
{
	return g_pool_config.backend_desc->backend_info[index].backend_application_name;
}

static bool
BackendSlotEmptyCheckFunc(int index)
{
	return (g_pool_config.backend_desc->backend_info[index].backend_port == 0);
}

static bool
WdSlotEmptyCheckFunc(int index)
{
	return (g_pool_config.wd_nodes.wd_node_info[index].pgpool_port == 0);
}

static bool
WdIFSlotEmptyCheckFunc(int index)
{
	return (g_pool_config.hb_ifs[index].dest_port  == 0);
}

static const char *
OtherPPHostShowFunc(int index)
{
	return g_pool_config.wd_nodes.wd_node_info[index].hostname;
}

static const char *
OtherPPPortShowFunc(int index)
{
	return IntValueShowFunc(g_pool_config.wd_nodes.wd_node_info[index].pgpool_port);
}

static const char *
OtherWDPortShowFunc(int index)
{
	return IntValueShowFunc(g_pool_config.wd_nodes.wd_node_info[index].wd_port);
}

static const char *
HBDeviceShowFunc(int index)
{
	return g_pool_config.hb_ifs[index].if_name;
}

static const char *
HBHostnameShowFunc(int index)
{
	return g_pool_config.hb_ifs[index].addr;
}

static const char *
HBDestinationPortShowFunc(int index)
{
	return IntValueShowFunc(g_pool_config.hb_ifs[index].dest_port);
}

static const char *
HealthCheckPeriodShowFunc(int index)
{
	return IntValueShowFunc(g_pool_config.health_check_params[index].health_check_period);
}
static const char *
HealthCheckTimeOutShowFunc(int index)
{
	return IntValueShowFunc(g_pool_config.health_check_params[index].health_check_timeout);
}
static const char *
HealthCheckMaxRetriesShowFunc(int index)
{
	return IntValueShowFunc(g_pool_config.health_check_params[index].health_check_max_retries);
}
static const char *
HealthCheckRetryDelayShowFunc(int index)
{
	return IntValueShowFunc(g_pool_config.health_check_params[index].health_check_retry_delay);
}
static const char *
HealthCheckConnectTimeOutShowFunc(int index)
{
	return IntValueShowFunc(g_pool_config.health_check_params[index].connect_timeout);
}
static const char *
HealthCheckUserShowFunc(int index)
{
	return g_pool_config.health_check_params[index].health_check_user;
}
static const char *
HealthCheckPasswordShowFunc(int index)
{
	return g_pool_config.health_check_params[index].health_check_password;
}
static const char *
HealthCheckDatabaseShowFunc(int index)
{
	return g_pool_config.health_check_params[index].health_check_database;
}


/* Health check configuration assign functions */

/*health_check_period*/
static bool
HealthCheckPeriodAssignFunc(ConfigContext context, int newval, int index, int elevel)
{
	g_pool_config.health_check_params[index].health_check_period = newval;
	return true;
}

/*health_check_timeout*/
static bool
HealthCheckTimeOutAssignFunc(ConfigContext context, int newval, int index, int elevel)
{
	g_pool_config.health_check_params[index].health_check_timeout = newval;
	return true;
}

/*health_check_max_retries*/
static bool
HealthCheckMaxRetriesAssignFunc(ConfigContext context, int newval, int index, int elevel)
{
	g_pool_config.health_check_params[index].health_check_max_retries = newval;
	return true;
}

/*health_check_retry_delay*/
static bool
HealthCheckRetryDelayAssignFunc(ConfigContext context, int newval, int index, int elevel)
{
	g_pool_config.health_check_params[index].health_check_retry_delay = newval;
	return true;
}

/*connect_timeout*/
static bool
HealthCheckConnectTimeOutAssignFunc(ConfigContext context, int newval, int index, int elevel)
{
	g_pool_config.health_check_params[index].connect_timeout = newval;
	return true;
}

static bool
HealthCheckUserAssignFunc(ConfigContext context, char *newval, int index, int elevel)
{
	if (g_pool_config.health_check_params[index].health_check_user)
		pfree(g_pool_config.health_check_params[index].health_check_user);
	if (newval)
		g_pool_config.health_check_params[index].health_check_user = pstrdup(newval);
	else
		g_pool_config.health_check_params[index].health_check_user = NULL;
	return true;
}

static bool
HealthCheckPasswordAssignFunc(ConfigContext context, char *newval, int index, int elevel)
{
	if (g_pool_config.health_check_params[index].health_check_password)
		pfree(g_pool_config.health_check_params[index].health_check_password);
	if (newval)
		g_pool_config.health_check_params[index].health_check_password = pstrdup(newval);
	else
		g_pool_config.health_check_params[index].health_check_password = newval;
	return true;
}

static bool
HealthCheckDatabaseAssignFunc(ConfigContext context, char *newval, int index, int elevel)
{
	if (g_pool_config.health_check_params[index].health_check_database)
		pfree(g_pool_config.health_check_params[index].health_check_database);

	if (newval)
		g_pool_config.health_check_params[index].health_check_database = pstrdup(newval);
	else
		g_pool_config.health_check_params[index].health_check_database = newval;
	return true;
}

/* Watchdog hostname and heartbeat hostname assign functions */
/* hostname */
static bool
OtherPPHostAssignFunc(ConfigContext context, char *newval, int index, int elevel)
{
	if (newval == NULL || strlen(newval) == 0)
		g_pool_config.wd_nodes.wd_node_info[index].hostname[0] = '\0';
	else
		strlcpy(g_pool_config.wd_nodes.wd_node_info[index].hostname, newval, MAX_DB_HOST_NAMELEN - 1);
	return true;
}

/* pgpool_port */
static bool
OtherPPPortAssignFunc(ConfigContext context, int newval, int index, int elevel)
{
	g_pool_config.wd_nodes.wd_node_info[index].pgpool_port = newval;
	return true;
}

/* wd_port */
static bool
OtherWDPortAssignFunc(ConfigContext context, int newval, int index, int elevel)
{
	g_pool_config.wd_nodes.wd_node_info[index].wd_port = newval;
	return true;
}

/*heartbeat_device*/
static bool
HBDeviceAssignFunc(ConfigContext context, char *newval, int index, int elevel)
{
	if (newval == NULL || strlen(newval) == 0)
		g_pool_config.hb_ifs[index].if_name[0] = '\0';
	else
		strlcpy(g_pool_config.hb_ifs[index].if_name, newval, WD_MAX_IF_NAME_LEN - 1);
	return true;
}

/*heartbeat_hostname*/
static bool
HBHostnameAssignFunc(ConfigContext context, char *newval, int index, int elevel)
{
	if (newval == NULL || strlen(newval) == 0)
		g_pool_config.hb_ifs[index].addr[0] = '\0';
	else
		strlcpy(g_pool_config.hb_ifs[index].addr, newval, WD_MAX_HOST_NAMELEN - 1);
	return true;
}

/*heartbeat_port*/
static bool
HBDestinationPortAssignFunc(ConfigContext context, int newval, int index, int elevel)
{
	g_pool_config.hb_ifs[index].dest_port = newval;
	return true;
}

/*
 * Throws warning for if someone uses the removed fail_over_on_backend
 * configuration parameter
 */
static bool
FailOverOnBackendErrorAssignMessage(ConfigContext scontext, bool newval, int elevel)
{
	if (scontext != CFGCXT_BOOT)
		ereport(WARNING,
				(errmsg("fail_over_on_backend_error is changed to failover_on_backend_error"),
				 errdetail("if fail_over_on_backend_error is specified, the value will be set to failover_on_backend_error")));
	g_pool_config.failover_on_backend_error = newval;
	return true;
}
/*
 * Throws warning for if someone uses the removed delegate_IP
 * configuration parameter and set the value to delegate_ip
 */
static bool
DelegateIPAssignMessage(ConfigContext scontext, char *newval, int elevel)
{
	if (scontext != CFGCXT_BOOT)
		ereport(WARNING,
				(errmsg("delegate_IP is changed to delegate_ip"),
				 errdetail("if delegate_IP is specified, the value will be set to delegate_ip")));
	g_pool_config.delegate_ip = newval;
	return true;
}
/*
 * Check DB node spec. node spec should be either "primary", "standby" or
 * numeric DB node id.
 */
static bool
check_redirect_node_spec(char *node_spec)
{
	int			len = strlen(node_spec);
	int			i;
	int64		val;

	if (len <= 0)
		return false;

	if (strcasecmp("primary", node_spec) == 0)
	{
		return true;
	}

	if (strcasecmp("standby", node_spec) == 0)
	{
		return true;
	}

	for (i = 0; i < len; i++)
	{
		if (!isdigit((int) node_spec[i]))
			return false;
	}

	val = pool_atoi64(node_spec);

	if (val >= 0 && val < MAX_NUM_BACKENDS)
		return true;

	return false;
}

static bool
config_post_processor(ConfigContext context, int elevel)
{
	double		 total_weight = 0.0;
	sig_atomic_t local_num_backends = 0;
	int			 i;

	/* read from pgpool_node_id */
	SetPgpoolNodeId(elevel);

	if (context == CFGCXT_BOOT)
	{
		char		localhostname[128];
		int			res = gethostname(localhostname, sizeof(localhostname));

		if (res != 0)
		{
			ereport(WARNING,
					(errmsg("initializing pool configuration"),
					 errdetail("failed to get the local hostname")));
			return false;
		}
		strcpy(g_pool_config.wd_nodes.wd_node_info[g_pool_config.pgpool_node_id].hostname, localhostname);
		return true;
	}
	for (i = 0; i < MAX_CONNECTION_SLOTS; i++)
	{
		BackendInfo *backend_info = &g_pool_config.backend_desc->backend_info[i];

		/* port number == 0 indicates that this server is out of use */
		if (backend_info->backend_port == 0)
		{
			*backend_info->backend_hostname = '\0';
			backend_info->backend_status = CON_UNUSED;
			backend_info->backend_weight = 0.0;
		}
		else
		{
			total_weight += backend_info->unnormalized_weight;
			local_num_backends = i + 1;

			/* initialize backend_hostname with a default socket path if empty */
			if (*(backend_info->backend_hostname) == '\0')
			{
				ereport(LOG,
						(errmsg("initializing pool configuration"),
						 errdetail("empty backend_hostname%d, use PostgreSQL's default unix socket path (%s)",
								   i, DEFAULT_SOCKET_DIR)));
				strlcpy(backend_info->backend_hostname, DEFAULT_SOCKET_DIR, MAX_DB_HOST_NAMELEN);
			}
		}
	}

	if (local_num_backends != pool_config->backend_desc->num_backends)
		pool_config->backend_desc->num_backends = local_num_backends;

	ereport(DEBUG1,
			(errmsg("initializing pool configuration"),
			 errdetail("num_backends: %d total_weight: %f",
					   pool_config->backend_desc->num_backends, total_weight)));

	/*
	 * Normalize load balancing weights. What we are doing here is, assign 0
	 * to RAND_MAX to each backend's weight according to the value weightN.
	 * For example, if two backends are assigned 1.0, then each backend will
	 * get RAND_MAX/2 normalized weight.
	 */
	for (i = 0; i < MAX_CONNECTION_SLOTS; i++)
	{
		BackendInfo *backend_info = &g_pool_config.backend_desc->backend_info[i];

		if (backend_info->backend_port != 0)
		{
			backend_info->backend_weight =
				(RAND_MAX) * backend_info->unnormalized_weight / total_weight;

			ereport(DEBUG1,
					(errmsg("initializing pool configuration"),
					 errdetail("backend %d weight: %f flag: %04x", i, backend_info->backend_weight, backend_info->flag)));
		}
	}

	/* Set the number of configured Watchdog nodes */
	g_pool_config.wd_nodes.num_wd = 0;

	if (g_pool_config.use_watchdog)
	{
		for (i = 0; i < MAX_WATCHDOG_NUM; i++)
		{
			WdNodeInfo *wdNode = &g_pool_config.wd_nodes.wd_node_info[i];

			if (i == g_pool_config.pgpool_node_id && wdNode->wd_port <= 0)
			{
				ereport(elevel,
						(errmsg("invalid watchdog configuration"),
						 errdetail("no watchdog configuration for local pgpool node, pgpool node id: %d ", g_pool_config.pgpool_node_id)));
				return false;
			}

			if (wdNode->wd_port > 0)
				g_pool_config.wd_nodes.num_wd = i + 1;
		}
	}

	/* Set configured heartbeat destination interfaces */
	SetHBDestIfFunc(elevel);

	if (strcmp(pool_config->recovery_1st_stage_command, "") ||
		strcmp(pool_config->recovery_2nd_stage_command, ""))
	{
		for (i = 0; i < MAX_CONNECTION_SLOTS; i++)
		{
			BackendInfo *backend_info = &g_pool_config.backend_desc->backend_info[i];

			if (backend_info->backend_port != 0 &&
				!strcmp(backend_info->backend_data_directory, ""))
			{
				ereport(elevel,
						(errmsg("invalid configuration, recovery_1st_stage_command and recovery_2nd_stage_command requires backend_data_directory to be set")));
				return false;
			}
		}
	}

	/*
	 * Quarantine state in native replication mode is dangerous and it can
	 * potentially cause data inconsistency.
	 * So as per the discussions, we agreed on disallowing setting
	 * failover_when_quorum_exists in native replication mode
	 */

	if (pool_config->failover_when_quorum_exists && pool_config->replication_mode)
	{
		pool_config->failover_when_quorum_exists = false;
		ereport(elevel,
				(errmsg("invalid configuration, failover_when_quorum_exists is not allowed in native replication mode")));
		return false;
	}

	if (pool_config->min_spare_children >= pool_config->max_spare_children)
	{
		ereport(elevel,
				(errmsg("invalid configuration, max_spare_children:%d must be greater than max_spare_children:%d",
				pool_config->max_spare_children,pool_config->min_spare_children)));
		return false;
	}
	return true;
}

static bool
MakeDMLAdaptiveObjectRelationList(char *newval, int elevel)
{
	int i;
	int elements_count = 0;
	char **rawList = get_list_from_string(newval, ",", &elements_count);

	if (rawList == NULL || elements_count == 0)
	{
		pool_config->parsed_dml_adaptive_object_relationship_list = NULL;
		return true;
	}
	pool_config->parsed_dml_adaptive_object_relationship_list = palloc(sizeof(DBObjectRelation) * (elements_count + 1));

	for (i = 0; i < elements_count; i++)
	{
		char *kvstr = rawList[i];
		char *left_token = strtok(kvstr, ":");
		char *right_token = strtok(NULL, ":");
		DBObjectTypes object_type;

		ereport(DEBUG5,
				(errmsg("dml_adaptive_init"),
					errdetail("%s -- left_token[%s] right_token[%s]", kvstr, left_token, right_token)));

		pool_config->parsed_dml_adaptive_object_relationship_list[i].left_token.name =
																	getParsedToken(left_token, &object_type);
		pool_config->parsed_dml_adaptive_object_relationship_list[i].left_token.object_type = object_type;

		pool_config->parsed_dml_adaptive_object_relationship_list[i].right_token.name =
																	getParsedToken(right_token,&object_type);
		pool_config->parsed_dml_adaptive_object_relationship_list[i].right_token.object_type = object_type;
		pfree(kvstr);
	}
	pool_config->parsed_dml_adaptive_object_relationship_list[i].left_token.name = NULL;
	pool_config->parsed_dml_adaptive_object_relationship_list[i].left_token.object_type = OBJECT_TYPE_UNKNOWN;
	pool_config->parsed_dml_adaptive_object_relationship_list[i].right_token.name = NULL;
	pool_config->parsed_dml_adaptive_object_relationship_list[i].right_token.object_type = OBJECT_TYPE_UNKNOWN;

	pfree(rawList);
	return true;
}

/*
 * Identify the object type for dml adaptive object
 * the function is very primitive and just looks for token
 * ending with ().
 * We also remove the trailing spaces from the function type token
 * and return the palloc'd copy of token in new_token
 */
static char*
getParsedToken(char *token, DBObjectTypes *object_type)
{
	int len;
	*object_type = OBJECT_TYPE_UNKNOWN;

	if (!token)
		return NULL;

	len = strlen(token);
	if (len > strlen("*()"))
	{
		int namelen = len - 2;
		/* check if token ends with () */
		if (strcmp(token + namelen,"()") == 0)
		{
			/*
			 * Remove the Parentheses from end of
			 * token name
			 */
			char *new_token;
			int new_len = strlen(token) - 2;
			new_token = palloc(new_len + 1);
			strncpy(new_token,token,new_len);
			new_token[new_len] = '\0';
			*object_type = OBJECT_TYPE_FUNCTION;
			return new_token;
		}
	}
	*object_type = OBJECT_TYPE_RELATION;
	return pstrdup(token);
}

static bool
MakeAppRedirectListRegex(char *newval, int elevel)
{
	/* TODO Deal with the memory */
	int			i;
	Left_right_tokens *lrtokens;

	if (newval == NULL)
	{
		pool_config->redirect_app_names = NULL;
		pool_config->app_name_redirect_tokens = NULL;
		return true;
	}

	lrtokens = create_lrtoken_array();
	extract_string_tokens2(newval, ",", ':', lrtokens);

	pool_config->redirect_app_names = create_regex_array();
	pool_config->app_name_redirect_tokens = lrtokens;

	for (i = 0; i < lrtokens->pos; i++)
	{
		if (!check_redirect_node_spec(lrtokens->token[i].right_token))
		{
			ereport(elevel,
					(errmsg("invalid configuration for key \"app_name_redirect_preference_list\""),
					 errdetail("wrong redirect db node spec: \"%s\"", lrtokens->token[i].right_token)));
			return false;
		}


		if (*(lrtokens->token[i].left_token) == '\0' ||
			add_regex_array(pool_config->redirect_app_names, lrtokens->token[i].left_token))
		{
			ereport(elevel,
					(errmsg("invalid configuration for key \"app_name_redirect_preference_list\""),
					 errdetail("wrong redirect app name regular expression: \"%s\"", lrtokens->token[i].left_token)));
			return false;
		}
	}

	return true;
}

static bool
MakeDBRedirectListRegex(char *newval, int elevel)
{
	/* TODO Deal with the memory */
	int			i;
	Left_right_tokens *lrtokens;

	if (newval == NULL)
	{
		pool_config->redirect_dbnames = NULL;
		pool_config->db_redirect_tokens = NULL;
		return true;
	}

	lrtokens = create_lrtoken_array();
	extract_string_tokens2(newval, ",", ':', lrtokens);

	pool_config->redirect_dbnames = create_regex_array();
	pool_config->db_redirect_tokens = lrtokens;

	for (i = 0; i < lrtokens->pos; i++)
	{
		if (!check_redirect_node_spec(lrtokens->token[i].right_token))
		{
			ereport(elevel,
					(errmsg("invalid configuration for key \"database_redirect_preference_list\""),
					 errdetail("wrong redirect db node spec: \"%s\"", lrtokens->token[i].right_token)));
			return false;
		}

		if (*(lrtokens->token[i].left_token) == '\0' ||
			add_regex_array(pool_config->redirect_dbnames, lrtokens->token[i].left_token))
		{
			ereport(elevel,
					(errmsg("invalid configuration for key \"database_redirect_preference_list\""),
					 errdetail("wrong redirect dbname regular expression: \"%s\"", lrtokens->token[i].left_token)));
			return false;
		}
	}
	return true;
}

/* Read the pgpool_node_id file */
static bool
SetPgpoolNodeId(int elevel)
{
	char		pgpool_node_id_file[POOLMAXPATHLEN + 1];
	FILE		*fd;
	int         length;
	int         i;

	if (g_pool_config.use_watchdog)
	{
		snprintf(pgpool_node_id_file, sizeof(pgpool_node_id_file), "%s/%s", config_file_dir, NODE_ID_FILE_NAME);

#define MAXLINE 10
		char        readbuf[MAXLINE];

		fd = fopen(pgpool_node_id_file, "r");
		if (!fd)
		{
			ereport(elevel,
					(errmsg("Pgpool node id file %s does not exist", pgpool_node_id_file),
					 errdetail("If watchdog is enable, pgpool_node_id file is required")));
			return false;
		}

		readbuf[MAXLINE - 1] = '\0';
		if (fgets(readbuf, MAXLINE - 1, fd) == 0)
		{
			ereport(elevel,
					(errmsg("pgpool_node_id file is empty"),
					 errdetail("If watchdog is enable, we need to specify pgpool node id in %s file", pgpool_node_id_file)));
			fclose(fd);
			return false;
		}

		length = strlen(readbuf);
		if (length > 0 && readbuf[length - 1] == '\n')
		{
			readbuf[length - 1] = '\0';

			length = strlen(readbuf);
			if (length <= 0)
			{
				ereport(elevel,
						(errmsg("pgpool_node_id file is empty"),
						 errdetail("If watchdog is enable, we need to specify pgpool node id in %s file", pgpool_node_id_file)));
				fclose(fd);
				return false;
			}
		}

		for (i = 0; i < length; i++)
		{
			if (!isdigit((int) readbuf[i]))
			{
				ereport(elevel,
						(errmsg("pgpool_node_id is not a numeric value"),
						 errdetail("Please specify a numeric value in %s file", pgpool_node_id_file)));
				fclose(fd);
				return false;
			}
		}

		g_pool_config.pgpool_node_id = atoi(readbuf);

		if (g_pool_config.pgpool_node_id < 0 || g_pool_config.pgpool_node_id > MAX_WATCHDOG_NUM)
		{
			ereport(elevel,
					(errmsg("Invalid pgpool node id \"%d\", must be between 0 and %d",
							g_pool_config.pgpool_node_id, MAX_WATCHDOG_NUM)));
			fclose(fd);
			return false;
		}
		else
		{
			ereport(DEBUG1,
					(errmsg("read pgpool node id file %s", pgpool_node_id_file),
					 errdetail("pgpool node id: %s", readbuf)));
		}

		fclose(fd);
	}

	return true;
}

/* Set configured heartbeat destination interfaces */
static bool
SetHBDestIfFunc(int elevel)
{
	int		idx = 0;
	char	**addrs;
	char	**if_names;
	int		i, j,
			n_addr,
			n_if_name;

	g_pool_config.num_hb_dest_if = 0;

	if (g_pool_config.wd_lifecheck_method != LIFECHECK_BY_HB)
	{
		return true;
	}

	/*
	 * g_pool_config.hb_ifs is the information for sending/receiving heartbeat
	 * for all nodes specified in pgpool.conf.
	 * If it is local pgpool node information, set dest_port to g_pool_config.wd_heartbeat_port
	 * and ignore addr and if_name.
	 * g_pool_config.hb_dest_if is the heartbeat destination information.
	 */
	for (i = 0; i < WD_MAX_IF_NUM; i++)
	{
		if (g_pool_config.hb_ifs[i].dest_port > 0)
		{
			/* Ignore local pgpool node */
			if (i == g_pool_config.pgpool_node_id)
			{
				g_pool_config.wd_heartbeat_port = g_pool_config.hb_ifs[i].dest_port;
				continue;
			}

			WdHbIf *hbNodeInfo = &g_pool_config.hb_ifs[i];

			addrs = get_list_from_string(hbNodeInfo->addr, ";", &n_addr);
			if_names = get_list_from_string(hbNodeInfo->if_name, ";", &n_if_name);

			if (!addrs || n_addr < 0)
			{
				g_pool_config.hb_dest_if[idx].addr[0] = '\0';

				if (addrs)
					pfree(addrs);
				if (if_names)
					pfree(if_names);

				ereport(elevel,
						(errmsg("invalid watchdog configuration"),
						 errdetail("heartbeat_hostname%d is not defined", i)));

				return false;
			}

			for (j = 0; j < n_addr; j++)
			{
				strlcpy(g_pool_config.hb_dest_if[idx].addr, addrs[j], WD_MAX_HOST_NAMELEN - 1);
				g_pool_config.hb_dest_if[idx].dest_port = hbNodeInfo->dest_port;
				if (n_if_name > j + 1)
				{
					strlcpy(g_pool_config.hb_dest_if[idx].if_name, if_names[j], WD_MAX_IF_NAME_LEN - 1);
					pfree(if_names[j]);
				}
				else
					g_pool_config.hb_dest_if[idx].if_name[0] = '\0';

				g_pool_config.num_hb_dest_if = idx + 1;
				idx++;
				pfree(addrs[j]);
			}

			if (addrs)
				pfree(addrs);
			if (if_names)
				pfree(if_names);
		}
	}
	return true;
}

static struct config_generic *
get_index_free_record_if_any(struct config_generic *record)
{
	struct config_generic *ret = NULL;

	switch (record->vartype)
	{
		case CONFIG_VAR_TYPE_INT_ARRAY:
			{
				struct config_int_array *conf = (struct config_int_array *) record;

				if (conf->config_no_index.gen.name)
					ret = (struct config_generic *) &conf->config_no_index;
			}
			break;

		case CONFIG_VAR_TYPE_DOUBLE_ARRAY:
			{
				struct config_double_array *conf = (struct config_double_array *) record;

				if (conf->config_no_index.gen.name)
					ret = (struct config_generic *) &conf->config_no_index;
			}
			break;

		case CONFIG_VAR_TYPE_STRING_ARRAY:
			{
				struct config_string_array *conf = (struct config_string_array *) record;

				if (conf->config_no_index.gen.name)
					ret = (struct config_generic *) &conf->config_no_index;
			}
			break;
		default:
			break;
	}
	return ret;
}

/*
 * Try to parse value as an integer.  The accepted formats are the
 * usual decimal, octal, or hexadecimal formats, as well as floating-point
 * formats (which will be rounded to integer after any units conversion).
 * Optionally, the value can be followed by a unit name if "flags" indicates
 * a unit is allowed.
 *
 * If the string parses okay, return true, else false.
 * If okay and result is not NULL, return the value in *result.
 * If not okay and hintmsg is not NULL, *hintmsg is set to a suitable
 * HINT message, or NULL if no hint provided.
 */
static bool
parse_int(const char *value, int64 *result, int flags, const char **hintmsg, int64 MaxVal)
{
	/*
	 * We assume here that double is wide enough to represent any integer
	 * value with adequate precision.
	 */
	double		val;
	char	   *endptr;

	/* To suppress compiler warnings, always set output params */
	if (result)
		*result = 0;
	if (hintmsg)
		*hintmsg = NULL;

	/*
	 * Try to parse as an integer (allowing octal or hex input).  If the
	 * conversion stops at a decimal point or 'e', or overflows, re-parse as
	 * float.  This should work fine as long as we have no unit names starting
	 * with 'e'.  If we ever do, the test could be extended to check for a
	 * sign or digit after 'e', but for now that's unnecessary.
	 */
	errno = 0;
	val = strtol(value, &endptr, 0);
	if (*endptr == '.' || *endptr == 'e' || *endptr == 'E' ||
		errno == ERANGE)
	{
		errno = 0;
		val = strtod(value, &endptr);
	}

	if (endptr == value || errno == ERANGE)
		return false;			/* no HINT for these cases */

	/* reject NaN (infinities will fail range check below) */
	if (isnan(val))
		return false;			/* treat same as syntax error; no HINT */


	/* allow whitespace between number and unit */
	while (isspace((unsigned char) *endptr))
		endptr++;

	/* Handle possible unit */
	if (*endptr != '\0')
	{
		if ((flags & GUC_UNIT) == 0)
		{
			return false;		/* this setting does not accept a unit */
		}
		if (!convert_to_base_unit(val,
								  endptr, (flags & GUC_UNIT),
								  &val))
		{
			/* invalid unit, or garbage after the unit; set hint and fail. */
			if (hintmsg)
			{
				if (flags & GUC_UNIT_MEMORY)
					*hintmsg = memory_units_hint;
				else
					*hintmsg = time_units_hint;
			}
			return false;
		}
	}

	/* Round to int, then check for overflow */
	val = rint(val);

	if (val > MaxVal || val < INT_MIN)
	{
		if (hintmsg)
			*hintmsg = "Value exceeds allowed range.";
		return false;
	}

	if (result)
		*result = (int64) val;
	return true;
}
/*
 * Convert a value from one of the human-friendly units ("kB", "min" etc.)
 * to the given base unit.  'value' and 'unit' are the input value and unit
 * to convert from (there can be trailing spaces in the unit string).
 * The converted value is stored in *base_value.
 * It's caller's responsibility to round off the converted value as necessary
 * and check for out-of-range.
 *
 * Returns true on success, false if the input unit is not recognized.
 */

static bool
convert_to_base_unit(double value, const char *unit,
					 int base_unit, double *base_value)
{
	char		unitstr[MAX_UNIT_LEN + 1];
	int			unitlen;
	const unit_conversion *table;
	int			i;

	/* extract unit string to compare to table entries */
	unitlen = 0;
	while (*unit != '\0' && !isspace((unsigned char) *unit) &&
		   unitlen < MAX_UNIT_LEN)
		unitstr[unitlen++] = *(unit++);
	unitstr[unitlen] = '\0';
	/* allow whitespace after unit */
	while (isspace((unsigned char) *unit))
		unit++;
	if (*unit != '\0')
		return false;			/* unit too long, or garbage after it */

	/* now search the appropriate table */
	if (base_unit & GUC_UNIT_MEMORY)
		table = memory_unit_conversion_table;
	else
		table = time_unit_conversion_table;

	for (i = 0; *table[i].unit; i++)
	{
		if (base_unit == table[i].base_unit &&
			strcmp(unitstr, table[i].unit) == 0)
		{
			double		cvalue = value * table[i].multiplier;

			/*
			 * If the user gave a fractional value such as "30.1GB", round it
			 * off to the nearest multiple of the next smaller unit, if there
			 * is one.
			 */
			if (*table[i + 1].unit &&
				base_unit == table[i + 1].base_unit)
				cvalue = rint(cvalue / table[i + 1].multiplier) *
					table[i + 1].multiplier;

			*base_value = cvalue;
			return true;
		}
	}
	return false;
}
#ifndef POOL_PRIVATE

/*
 * Convert an integer value in some base unit to a human-friendly unit.
 *
 * The output unit is chosen so that it's the greatest unit that can represent
 * the value without loss.  For example, if the base unit is GUC_UNIT_KB, 1024
 * is converted to 1 MB, but 1025 is represented as 1025 kB.
 */
static void
convert_int_from_base_unit(int64 base_value, int base_unit,
						   int64 *value, const char **unit)
{
	const unit_conversion *table;
	int			i;

	*unit = NULL;

	if (base_unit & GUC_UNIT_MEMORY)
		table = memory_unit_conversion_table;
	else
		table = time_unit_conversion_table;

	for (i = 0; *table[i].unit; i++)
	{
		if (base_unit == table[i].base_unit)
		{
			/*
			 * Accept the first conversion that divides the value evenly.  We
			 * assume that the conversions for each base unit are ordered from
			 * greatest unit to the smallest!
			 */
			if (table[i].multiplier <= 1.0 ||
				base_value % (int64) table[i].multiplier == 0)
			{
				*value = (int64) rint(base_value / table[i].multiplier);
				*unit = table[i].unit;
				break;
			}
		}
	}

	Assert(*unit != NULL);
}

/*
 * Lookup the name for an enum option with the selected value.
 * The returned string is a pointer to static data and not
 * allocated for modification.
 */
static const char *
config_enum_lookup_by_value(struct config_enum *record, int val)
{
	const struct config_enum_entry *entry;

	for (entry = record->options; entry && entry->name; entry++)
	{
		if (entry->val == val)
			return entry->name;
	}
	/* should never happen */
	return NULL;
}

static char *
ShowOption(struct config_generic *record, int index, int elevel)
{
	char		buffer[256];
	const char *val;

	/*
	 * if the value needs to be hidden no need to dig further
	 */
	if (record->flags & VAR_HIDDEN_VALUE)
		return pstrdup("*****");

	switch (record->vartype)
	{
		case CONFIG_VAR_TYPE_BOOL:
			{
				struct config_bool *conf = (struct config_bool *) record;

				if (conf->show_hook)
					val = (*conf->show_hook) ();
				else if (conf->variable)
					val = *conf->variable ? "on" : "off";
				else
					val = "";
			}
			break;

		case CONFIG_VAR_TYPE_INT:
			{
				struct config_int *conf = (struct config_int *) record;

				if (conf->show_hook)
					val = (*conf->show_hook) ();
				else
				{

					/*
					 * Use int64 arithmetic to avoid overflows in units
					 * conversion.
					 */
					int64		result = (int64) *conf->variable;
					const char *unit;

					if (result > 0 && (record->flags & GUC_UNIT))
						convert_int_from_base_unit(result,
												   record->flags & GUC_UNIT,
												   &result, &unit);
					else
						unit = "";

					snprintf(buffer, sizeof(buffer), INT64_FORMAT "%s",
							 result, unit);
					val = buffer;
				}
			}
			break;

		case CONFIG_VAR_TYPE_LONG:
			{
				struct config_long *conf = (struct config_long *) record;

				if (conf->show_hook)
					val = (*conf->show_hook) ();
				else
				{
					int64		result = (int64) *conf->variable;
					const char *unit;

					if (result > 0 && (record->flags & GUC_UNIT))
						convert_int_from_base_unit(result,
												   record->flags & GUC_UNIT,
												   &result, &unit);
					else
						unit = "";

					snprintf(buffer, sizeof(buffer), INT64_FORMAT "%s",
							 result, unit);
					val = buffer;
				}
			}
			break;

		case CONFIG_VAR_TYPE_DOUBLE:
			{
				struct config_double *conf = (struct config_double *) record;

				if (conf->show_hook)
					val = (*conf->show_hook) ();
				else
				{
					snprintf(buffer, sizeof(buffer), "%g",
							 *conf->variable);
					val = buffer;
				}
			}
			break;

		case CONFIG_VAR_TYPE_STRING:
			{
				struct config_string *conf = (struct config_string *) record;

				if (conf->show_hook)
					val = (*conf->show_hook) ();
				else if (conf->variable && *conf->variable && **conf->variable)
					val = *conf->variable;
				else
					val = "";
			}
			break;

		case CONFIG_VAR_TYPE_ENUM:
			{
				struct config_enum *conf = (struct config_enum *) record;

				if (conf->show_hook)
					val = (*conf->show_hook) ();
				else
					val = config_enum_lookup_by_value(conf, *conf->variable);
			}

			break;

		case CONFIG_VAR_TYPE_INT_ARRAY:
			{
				struct config_int_array *conf = (struct config_int_array *) record;

				if (index >= record->max_elements || index < 0)
				{
					ereport(elevel,
							(errmsg("invalid index %d for configuration parameter \"%s\"", index, conf->gen.name),
							 errdetail("allowed index is between 0 and %d", record->max_elements - 1)));

					val = NULL;
				}
				else
				{
					if (conf->show_hook)
						val = (*conf->show_hook) (index);
					else
					{
						int			result = *conf->variable[index];

						snprintf(buffer, sizeof(buffer), "%d",
								 result);
						val = buffer;
					}
				}
			}
			break;

		case CONFIG_VAR_TYPE_DOUBLE_ARRAY:
			{
				struct config_double_array *conf = (struct config_double_array *) record;

				if (index >= record->max_elements || index < 0)
				{
					ereport(elevel,
							(errmsg("invalid index %d for configuration parameter \"%s\"", index, conf->gen.name),
							 errdetail("allowed index is between 0 and %d", record->max_elements - 1)));

					val = NULL;
				}
				else
				{
					if (conf->show_hook)
						val = (*conf->show_hook) (index);
					else
					{
						double		result = *conf->variable[index];

						snprintf(buffer, sizeof(buffer), "%g",
								 result);
						val = buffer;
					}
				}
			}
			break;

		case CONFIG_VAR_TYPE_STRING_ARRAY:
			{
				struct config_string_array *conf = (struct config_string_array *) record;

				if (index >= record->max_elements || index < 0)
				{
					ereport(elevel,
							(errmsg("invalid index %d for configuration parameter \"%s\"", index, conf->gen.name),
							 errdetail("allowed index is between 0 and %d", record->max_elements - 1)));

					val = NULL;
				}
				else
				{
					if (conf->show_hook)
						val = (*conf->show_hook) (index);
					else if ((*conf->variable)[index] && ***conf->variable)
						val = (*conf->variable)[index];
					else
						val = "";
				}
			}
			break;
		case CONFIG_VAR_TYPE_STRING_LIST:
			{
				struct config_string_list *conf = (struct config_string_list *) record;

				if (conf->show_hook)
					val = (*conf->show_hook) ();
				else if (conf->current_val)
					val = conf->current_val;
				else
					val = "";
			}
			break;

		default:
			/* just to keep compiler quiet */
			val = "???";
			break;
	}
	if (val)
		return pstrdup(val);
	return NULL;
}


static bool
value_slot_for_config_record_is_empty(struct config_generic *record, int index)
{
	switch (record->vartype)
	{
		case CONFIG_VAR_TYPE_BOOL:
		case CONFIG_VAR_TYPE_INT:
		case CONFIG_VAR_TYPE_LONG:
		case CONFIG_VAR_TYPE_DOUBLE:
		case CONFIG_VAR_TYPE_STRING:
		case CONFIG_VAR_TYPE_ENUM:
			return false;
			break;

		case CONFIG_VAR_TYPE_INT_ARRAY:
			{
				struct config_int_array *conf = (struct config_int_array *) record;

				if (conf->empty_slot_check_func)
					return (*conf->empty_slot_check_func) (index);
			}
			break;

		case CONFIG_VAR_TYPE_DOUBLE_ARRAY:
			{
				struct config_double_array *conf = (struct config_double_array *) record;

				if (conf->empty_slot_check_func)
					return (*conf->empty_slot_check_func) (index);
			}
			break;

		case CONFIG_VAR_TYPE_STRING_ARRAY:
			{
				struct config_string_array *conf = (struct config_string_array *) record;

				if (conf->empty_slot_check_func)
					return (*conf->empty_slot_check_func) (index);
			}
			break;

		default:
			break;
	}
	return false;
}

bool
set_config_option_for_session(POOL_CONNECTION * frontend, POOL_CONNECTION_POOL * backend, const char *name, const char *value)
{
	bool		ret;
	MemoryContext oldCxt = MemoryContextSwitchTo(TopMemoryContext);

	ret = set_one_config_option(name, value, CFGCXT_SESSION, PGC_S_SESSION, FRONTEND_ONLY_ERROR);
	if (ret == true)
	{
		send_complete_and_ready(frontend, backend, "SET", -1);
	}
	MemoryContextSwitchTo(oldCxt);
	return ret;
}

bool
reset_all_variables(POOL_CONNECTION * frontend, POOL_CONNECTION_POOL * backend)
{
	int			i;
	int			elevel = (frontend == NULL) ? FATAL : FRONTEND_ONLY_ERROR;
	MemoryContext oldCxt = MemoryContextSwitchTo(TopMemoryContext);

	ereport(DEBUG2,
			(errmsg("RESET ALL CONFIG VARIABLES")));

	for (i = 0; i < num_all_parameters; ++i)
	{
		struct config_generic *record = all_parameters[i];

		/* Don't reset if special exclusion from RESET ALL */
		if (record->flags & VAR_NO_RESET_ALL)
			continue;

		if (record->dynamic_array_var)
		{
			int			index;

			for (index = 0; index < record->max_elements; index++)
			{
				if (record->scontexts[index] != CFGCXT_SESSION)
					continue;

				if (value_slot_for_config_record_is_empty(record, index))
					continue;

				setConfigOptionVar(record, record->name, index, NULL,
								   CFGCXT_INIT, PGC_S_FILE, elevel);
			}

			/* Also set the default value for index free record if any */
			if (record->flags & ARRAY_VAR_ALLOW_NO_INDEX)
			{
				struct config_generic *idx_free_record = get_index_free_record_if_any(record);

				if (idx_free_record && idx_free_record->scontexts[0] == CFGCXT_SESSION)
				{
					ereport(DEBUG1,
							(errmsg("reset index-free value of array type parameter \"%s\"",
									record->name)));
					setConfigOptionVar(idx_free_record, idx_free_record->name, -1, NULL,
									   CFGCXT_INIT, PGC_S_FILE, elevel);
				}
			}
		}
		else
		{
			/* do nothing if variable is not changed in session */
			if (record->scontexts[0] != CFGCXT_SESSION)
				continue;
			setConfigOptionVar(record, record->name, -1, NULL,
							   CFGCXT_INIT, PGC_S_FILE, elevel);
		}
	}
	if (frontend)
		send_complete_and_ready(frontend, backend, "RESET", -1);

	MemoryContextSwitchTo(oldCxt);

	return true;
}

/*
 * Handle "pgpool show all" command.
*/
bool
report_all_variables(POOL_CONNECTION * frontend, POOL_CONNECTION_POOL * backend)
{
	int			i;
	int			num_rows = 0;
	const char *value;

	send_row_description_for_detail_view(frontend, backend);

	/*
	 * grouped variables are not listed in all parameter array so start with
	 * group variables.
	 */
	for (i = 0; ConfigureVarGroups[i].gen.name; i++)
	{
		int			rows = send_grouped_type_variable_to_frontend(&ConfigureVarGroups[i], frontend, backend);

		if (rows < 0)
			return false;
		num_rows += rows;
	}

	for (i = 0; i < num_all_parameters; ++i)
	{
		struct config_generic *record = all_parameters[i];

		if (record->flags & VAR_PART_OF_GROUP)
			continue;

		if (record->flags & VAR_HIDDEN_IN_SHOW_ALL)
			continue;

		if (record->dynamic_array_var)
		{
			int			rows = send_array_type_variable_to_frontend(record, frontend, backend);

			if (rows < 0)
				return false;
			num_rows += rows;

		}
		else
		{
			value = ShowOption(record, 0, FRONTEND_ONLY_ERROR);
			if (value == NULL)
				return false;
			send_config_var_detail_row(frontend, backend, record->name, value, record->description);
			pfree((void *) value);
			num_rows++;
		}
	}
	send_complete_and_ready(frontend, backend, "SELECT", num_rows);
	return true;
}

/*
 * Handle "pgpool show" command.
*/
bool
report_config_variable(POOL_CONNECTION * frontend, POOL_CONNECTION_POOL * backend, const char *var_name)
{
	int			index = 0;
	char	   *value;
	int			num_rows = 0;
	struct config_generic *record;

	if (strcmp(var_name, "all") == 0)
		return report_all_variables(frontend, backend);

	/* search the variable */
	record = find_option(var_name, WARNING);
	if (record == NULL)
	{
		int			i;

		/* check if the var name match the grouped var */
		for (i = 0; ConfigureVarGroups[i].gen.name; i++)
		{
			if (strcmp(ConfigureVarGroups[i].gen.name, var_name) == 0)
			{
				send_row_description_for_detail_view(frontend, backend);
				num_rows = send_grouped_type_variable_to_frontend(&ConfigureVarGroups[i], frontend, backend);
				if (num_rows < 0)
					return false;
				send_complete_and_ready(frontend, backend, "SELECT", num_rows);
				return true;
			}
		}

		ereport(FRONTEND_ONLY_ERROR,
				(errmsg("config variable \"%s\" not found", var_name)));

		return false;
	}

	if (record->dynamic_array_var)
	{
		if (get_index_in_var_name(record, var_name, &index, FRONTEND_ONLY_ERROR) == false)
			return false;

		if (index < 0)
		{
			/*
			 * Index is not included in parameter name. this is the multi
			 * value config variable
			 */
			ereport(DEBUG3,
					(errmsg("show parameter \"%s\" without index", var_name)));
			send_row_description_for_detail_view(frontend, backend);
			num_rows = send_array_type_variable_to_frontend(record, frontend, backend);
			if (num_rows < 0)
				return false;
		}
	}

	if (index >= 0)
	{
		/* single value only */
		num_rows = 1;
		send_row_description(frontend, backend, 1, (char **) &var_name);
		value = ShowOption(record, index, FRONTEND_ONLY_ERROR);
		if (value == NULL)
			return false;
		send_config_var_value_only_row(frontend, backend, value);
		pfree((void *) value);
	}

	send_complete_and_ready(frontend, backend, "SELECT", num_rows);

	return true;
}

static int
send_array_type_variable_to_frontend(struct config_generic *record, POOL_CONNECTION * frontend, POOL_CONNECTION_POOL * backend)
{
	if (record->dynamic_array_var)
	{
		const int	MAX_NAME_LEN = 255;
		char		name[MAX_NAME_LEN + 1];
		int			index;
		int			num_rows = 0;
		const char *value;
		struct config_generic *idx_free_record = get_index_free_record_if_any(record);

		/* Before printing the array, print the index free value if any */
		if (idx_free_record)
		{
			value = ShowOption(idx_free_record, 0, FRONTEND_ONLY_ERROR);
			if (value)
			{
				send_config_var_detail_row(frontend, backend, (const char *) idx_free_record->name, value, record->description);
				pfree((void *) value);
				num_rows++;
			}
		}
		for (index = 0; index < record->max_elements; index++)
		{
			if (value_slot_for_config_record_is_empty(record, index))
				continue;
			/* construct the name with index */
			snprintf(name, MAX_NAME_LEN, "%s%d", record->name, index);
			value = ShowOption(record, index, FRONTEND_ONLY_ERROR);
			if (value == NULL)
				return -1;
			send_config_var_detail_row(frontend, backend, (const char *) name, value, record->description);
			pfree((void *) value);
			num_rows++;
		}
		return num_rows;
	}
	return -1;					/* error */
}

static int
send_grouped_type_variable_to_frontend(struct config_grouped_array_var *grouped_record, POOL_CONNECTION * frontend, POOL_CONNECTION_POOL * backend)
{
	int			k,
				index;
	int			max_elements = grouped_record->var_list[0]->max_elements;
	int			num_rows = 0;
	const char *value;

	for (k = 0; k < grouped_record->var_count; k++)
	{
		struct config_generic *record = grouped_record->var_list[k];
		struct config_generic *idx_free_record = get_index_free_record_if_any(record);

		if (idx_free_record)
		{
			value = ShowOption(idx_free_record, 0, FRONTEND_ONLY_ERROR);
			if (value)
			{
				send_config_var_detail_row(frontend, backend, (const char *) idx_free_record->name, value, record->description);
				pfree((void *) value);
				num_rows++;
			}
		}
	}

	for (index = 0; index < max_elements; index++)
	{
		for (k = 0; k < grouped_record->var_count; k++)
		{
			const int	MAX_NAME_LEN = 255;
			char		name[MAX_NAME_LEN + 1];

			struct config_generic *record = grouped_record->var_list[k];

			if (value_slot_for_config_record_is_empty(record, index))
				break;
			/* construct the name with index */
			snprintf(name, MAX_NAME_LEN, "%s%d", record->name, index);
			value = ShowOption(record, index, FRONTEND_ONLY_ERROR);
			if (value == NULL)
				return -1;
			send_config_var_detail_row(frontend, backend, (const char *) name, value, record->description);
			pfree((void *) value);
			num_rows++;
		}
	}

	return num_rows;
}

static void
send_row_description_for_detail_view(POOL_CONNECTION * frontend, POOL_CONNECTION_POOL * backend)
{
	static char *field_names[] = {"item", "value", "description"};

	send_row_description(frontend, backend, 3, field_names);
}
#endif							/* not defined POOL_FRONTEND */
