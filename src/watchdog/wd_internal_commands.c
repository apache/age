/*
 * $Header$
 *
 * Handles watchdog connection, and protocol communication with pgpool-II
 *
 * pgpool: a language independent connection pool server for PostgreSQL
 * written by Tatsuo Ishii
 *
 * Copyright (c) 2003-2020	PgPool Global Development Group
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
 */
#include <stdio.h>
#include <errno.h>
#include <ctype.h>
#include <time.h>
#include <string.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>

#include "pool.h"
#include "pool_config.h"
#include "watchdog/wd_json_data.h"
#include "watchdog/wd_internal_commands.h"
#include "utils/elog.h"
#include "utils/json_writer.h"
#include "utils/pool_signal.h"
#include "utils/json.h"
#include "auth/pool_auth.h"

#define WD_DEFAULT_IPC_COMMAND_TIMEOUT	8	/* default number of seconds to
											 * wait for IPC command results */
#define WD_INTERLOCK_WAIT_MSEC		500
#define WD_INTERLOCK_TIMEOUT_SEC	10
#define WD_INTERLOCK_WAIT_COUNT ((int) ((WD_INTERLOCK_TIMEOUT_SEC * 1000)/WD_INTERLOCK_WAIT_MSEC))

/* shared memory variables */
bool	   		*watchdog_require_cleanup = NULL;	/* shared memory variable set
													 * to true when watchdog
													 * process terminates
													 * abnormally */
bool	   		*watchdog_node_escalated = NULL; /* shared memory variable set to
												  * true when watchdog process has
												  * performed escalation */
unsigned int 	*ipc_shared_key = NULL;		/* key lives in shared memory used to
											 * identify the ipc internal clients */

static char *get_wd_failover_state_json(bool start);
static WDFailoverCMDResults wd_get_failover_result_from_data(WDIPCCmdResult * result,
															 unsigned int *wd_failover_id);
static WDFailoverCMDResults wd_issue_failover_command(char *func_name, int *node_id_set,
													  		int count, unsigned char flags);

static WdCommandResult wd_send_locking_command(WD_LOCK_STANDBY_TYPE lock_type,
															bool acquire);

void
wd_ipc_initialize_data(void)
{
	wd_ipc_conn_initialize();

	if (ipc_shared_key == NULL)
	{
		ipc_shared_key = pool_shared_memory_segment_get_chunk(sizeof(unsigned int));
		*ipc_shared_key = 0;
		while (*ipc_shared_key == 0)
		{
			pool_random_salt((char *) ipc_shared_key);
		}
	}

	if (watchdog_require_cleanup == NULL)
	{
		watchdog_require_cleanup = pool_shared_memory_segment_get_chunk(sizeof(bool));
		*watchdog_require_cleanup = false;
	}

	if (watchdog_node_escalated == NULL)
	{
		watchdog_node_escalated = pool_shared_memory_segment_get_chunk(sizeof(bool));
		*watchdog_node_escalated = false;
	}
}

size_t wd_ipc_get_shared_mem_size(void)
{
	size_t size = 0;
	size += MAXALIGN(sizeof(unsigned int)); /* ipc_shared_key */
	size += MAXALIGN(sizeof(bool)); /* watchdog_require_cleanup */
	size += MAXALIGN(sizeof(bool)); /* watchdog_node_escalated */
	size += estimate_ipc_socket_addr_len();
	return size;
}

/*
 * function gets the PG backend status of all attached nodes from
 * the leader watchdog node.
 */
WDPGBackendStatus *
get_pg_backend_status_from_leader_wd_node(void)
{
	unsigned int *shared_key = get_ipc_shared_key();
	char	   *data = get_data_request_json(WD_DATE_REQ_PG_BACKEND_DATA,
											 shared_key ? *shared_key : 0, pool_config->wd_authkey);

	WDIPCCmdResult *result = issue_command_to_watchdog(WD_GET_LEADER_DATA_REQUEST,
													   WD_DEFAULT_IPC_COMMAND_TIMEOUT,
													   data, strlen(data), true);

	pfree(data);

	if (result == NULL)
	{
		ereport(WARNING,
				(errmsg("get backend node status from leader watchdog failed"),
				 errdetail("issue command to watchdog returned NULL")));
		return NULL;
	}
	if (result->type == WD_IPC_CMD_CLUSTER_IN_TRAN)
	{
		ereport(WARNING,
				(errmsg("get backend node status from leader watchdog failed"),
				 errdetail("watchdog cluster is not in stable state"),
				 errhint("try again when the cluster is fully initialized")));
		FreeCmdResult(result);
		return NULL;
	}
	else if (result->type == WD_IPC_CMD_TIMEOUT)
	{
		ereport(WARNING,
				(errmsg("get backend node status from leader watchdog failed"),
				 errdetail("ipc command timeout")));
		FreeCmdResult(result);
		return NULL;
	}
	else if (result->type == WD_IPC_CMD_RESULT_OK)
	{
		WDPGBackendStatus *backendStatus = get_pg_backend_node_status_from_json(result->data, result->length);

		/*
		 * Watchdog returns the zero length data when the node itself is a
		 * leader watchdog node
		 */
		if (result->length <= 0)
		{
			backendStatus = palloc0(sizeof(WDPGBackendStatus));
			backendStatus->node_count = -1;
		}
		else
		{
			backendStatus = get_pg_backend_node_status_from_json(result->data, result->length);
		}
		FreeCmdResult(result);
		return backendStatus;
	}

	ereport(WARNING,
			(errmsg("get backend node status from leader watchdog failed")));
	FreeCmdResult(result);
	return NULL;
}

WdCommandResult
wd_start_recovery(void)
{
	char		type;
	unsigned int *shared_key = get_ipc_shared_key();

	char	   *func = get_wd_node_function_json(WD_FUNCTION_START_RECOVERY, NULL, 0, 0,
												 shared_key ? *shared_key : 0, pool_config->wd_authkey);

	WDIPCCmdResult *result = issue_command_to_watchdog(WD_IPC_ONLINE_RECOVERY_COMMAND,
													   pool_config->recovery_timeout + WD_DEFAULT_IPC_COMMAND_TIMEOUT,
													   func, strlen(func), true);

	pfree(func);

	if (result == NULL)
	{
		ereport(WARNING,
				(errmsg("start recovery command lock failed"),
				 errdetail("issue command to watchdog returned NULL")));
		return COMMAND_FAILED;
	}

	type = result->type;
	FreeCmdResult(result);
	if (type == WD_IPC_CMD_CLUSTER_IN_TRAN)
	{
		ereport(WARNING,
				(errmsg("start recovery command lock failed"),
				 errdetail("watchdog cluster is not in stable state"),
				 errhint("try again when the cluster is fully initialized")));
		return CLUSTER_IN_TRANSACTIONING;
	}
	else if (type == WD_IPC_CMD_TIMEOUT)
	{
		ereport(WARNING,
				(errmsg("start recovery command lock failed"),
				 errdetail("ipc command timeout")));
		return COMMAND_TIMEOUT;
	}
	else if (type == WD_IPC_CMD_RESULT_OK)
	{
		return COMMAND_OK;
	}
	return COMMAND_FAILED;
}

WdCommandResult
wd_end_recovery(void)
{
	char		type;
	unsigned int *shared_key = get_ipc_shared_key();

	char	   *func = get_wd_node_function_json(WD_FUNCTION_END_RECOVERY, NULL, 0, 0,
												 shared_key ? *shared_key : 0, pool_config->wd_authkey);


	WDIPCCmdResult *result = issue_command_to_watchdog(WD_IPC_ONLINE_RECOVERY_COMMAND,
													   WD_DEFAULT_IPC_COMMAND_TIMEOUT,
													   func, strlen(func), true);

	pfree(func);

	if (result == NULL)
	{
		ereport(WARNING,
				(errmsg("end recovery command lock failed"),
				 errdetail("issue command to watchdog returned NULL")));
		return COMMAND_FAILED;
	}

	type = result->type;
	FreeCmdResult(result);

	if (type == WD_IPC_CMD_CLUSTER_IN_TRAN)
	{
		ereport(WARNING,
				(errmsg("end recovery command lock failed"),
				 errdetail("watchdog cluster is not in stable state"),
				 errhint("try again when the cluster is fully initialized")));
		return CLUSTER_IN_TRANSACTIONING;
	}
	else if (type == WD_IPC_CMD_TIMEOUT)
	{
		ereport(WARNING,
				(errmsg("end recovery command lock failed"),
				 errdetail("ipc command timeout")));
		return COMMAND_TIMEOUT;
	}
	else if (type == WD_IPC_CMD_RESULT_OK)
	{
		return COMMAND_OK;
	}
	return COMMAND_FAILED;
}

WdCommandResult
wd_execute_cluster_command(char* clusterCommand, List *argsList)
{
	char		type;
	unsigned int *shared_key = get_ipc_shared_key();

	char	   *func = get_wd_exec_cluster_command_json(clusterCommand, argsList,
												 shared_key ? *shared_key : 0, pool_config->wd_authkey);

	WDIPCCmdResult *result = issue_command_to_watchdog(WD_EXECUTE_CLUSTER_COMMAND,
													   WD_DEFAULT_IPC_COMMAND_TIMEOUT,
													   func, strlen(func), true);

	pfree(func);

	if (result == NULL)
	{
		ereport(WARNING,
				(errmsg("execute cluster command failed"),
				 errdetail("issue command to watchdog returned NULL")));
		return COMMAND_FAILED;
	}

	type = result->type;
	FreeCmdResult(result);

	if (type == WD_IPC_CMD_CLUSTER_IN_TRAN)
	{
		ereport(WARNING,
				(errmsg("execute cluster command failed"),
				 errdetail("watchdog cluster is not in stable state"),
				 errhint("try again when the cluster is fully initialized")));
		return CLUSTER_IN_TRANSACTIONING;
	}
	else if (type == WD_IPC_CMD_TIMEOUT)
	{
		ereport(WARNING,
				(errmsg("execute cluster command failed"),
				 errdetail("ipc command timeout")));
		return COMMAND_TIMEOUT;
	}
	else if (type == WD_IPC_CMD_RESULT_OK)
	{
		return COMMAND_OK;
	}
	return COMMAND_FAILED;
}


static char *
get_wd_failover_state_json(bool start)
{
	char	   *json_str;
	JsonNode   *jNode = jw_create_with_object(true);
	unsigned int *shared_key = get_ipc_shared_key();

	jw_put_int(jNode, WD_IPC_SHARED_KEY, shared_key ? *shared_key : 0); /* put the shared key */
	if (pool_config->wd_authkey != NULL && strlen(pool_config->wd_authkey) > 0)
		jw_put_string(jNode, WD_IPC_AUTH_KEY, pool_config->wd_authkey); /* put the auth key */

	jw_put_int(jNode, "FailoverFuncState", start ? 0 : 1);
	jw_finish_document(jNode);
	json_str = pstrdup(jw_get_json_string(jNode));
	jw_destroy(jNode);
	return json_str;
}

static WDFailoverCMDResults
wd_send_failover_func_status_command(bool start)
{
	WDFailoverCMDResults res;
	unsigned int failover_id;

	char	   *json_data = get_wd_failover_state_json(start);

	WDIPCCmdResult *result = issue_command_to_watchdog(WD_FAILOVER_INDICATION
													   ,WD_DEFAULT_IPC_COMMAND_TIMEOUT,
													   json_data, strlen(json_data), true);

	pfree(json_data);

	res = wd_get_failover_result_from_data(result, &failover_id);

	FreeCmdResult(result);
	return res;
}

static WDFailoverCMDResults wd_get_failover_result_from_data(WDIPCCmdResult * result, unsigned int *wd_failover_id)
{
	if (result == NULL)
	{
		ereport(WARNING,
				(errmsg("failover command on watchdog failed"),
				 errdetail("issue command to watchdog returned NULL")));
		return FAILOVER_RES_ERROR;
	}
	if (result->type == WD_IPC_CMD_CLUSTER_IN_TRAN)
	{
		ereport(WARNING,
				(errmsg("failover command on watchdog failed"),
				 errdetail("watchdog cluster is not in stable state"),
				 errhint("try again when the cluster is fully initialized")));
		return FAILOVER_RES_TRANSITION;
	}
	else if (result->type == WD_IPC_CMD_TIMEOUT)
	{
		ereport(WARNING,
				(errmsg("failover command on watchdog failed"),
				 errdetail("ipc command timeout")));
		return FAILOVER_RES_TIMEOUT;
	}
	else if (result->type == WD_IPC_CMD_RESULT_OK)
	{
		WDFailoverCMDResults res = FAILOVER_RES_ERROR;
		json_value *root;

		root = json_parse(result->data, result->length);
		/* The root node must be object */
		if (root == NULL || root->type != json_object)
		{
			ereport(NOTICE,
					(errmsg("unable to parse json data from failover command result")));
			return res;
		}
		if (root && json_get_int_value_for_key(root, WD_FAILOVER_RESULT_KEY, (int *) &res))
		{
			json_value_free(root);
			return FAILOVER_RES_ERROR;
		}
		if (root && json_get_int_value_for_key(root, WD_FAILOVER_ID_KEY, (int *) wd_failover_id))
		{
			json_value_free(root);
			return FAILOVER_RES_ERROR;
		}
		return res;
	}
	return FAILOVER_RES_ERROR;
}

static WDFailoverCMDResults
wd_issue_failover_command(char *func_name, int *node_id_set, int count, unsigned char flags)
{
	WDFailoverCMDResults res;
	char	   *func;
	unsigned int *shared_key = get_ipc_shared_key();
	unsigned int wd_failover_id;

	func = get_wd_node_function_json(func_name, node_id_set, count, flags,
									 shared_key ? *shared_key : 0, pool_config->wd_authkey);

	WDIPCCmdResult *result = issue_command_to_watchdog(WD_IPC_FAILOVER_COMMAND,
													   WD_DEFAULT_IPC_COMMAND_TIMEOUT,
													   func, strlen(func), true);

	pfree(func);
	res = wd_get_failover_result_from_data(result, &wd_failover_id);
	FreeCmdResult(result);
	return res;
}

/*
 * send the degenerate backend request to watchdog.
 * now watchdog can respond to the request in following ways.
 *
 * 1 - It can tell the caller to process with failover. This
 * happens when the current node is the leader watchdog node.
 *
 * 2 - It can tell the caller to failover not allowed
 * this happens when either cluster does not have the quorum
 *
 */
WDFailoverCMDResults
wd_degenerate_backend_set(int *node_id_set, int count, unsigned char flags)
{
	if (pool_config->use_watchdog)
		return wd_issue_failover_command(WD_FUNCTION_DEGENERATE_REQUEST, node_id_set, count, flags);
	return FAILOVER_RES_PROCEED;
}

WDFailoverCMDResults
wd_promote_backend(int node_id, unsigned char flags)
{
	if (pool_config->use_watchdog)
		return wd_issue_failover_command(WD_FUNCTION_PROMOTE_REQUEST, &node_id, 1, flags);
	return FAILOVER_RES_PROCEED;
}

WDFailoverCMDResults
wd_send_failback_request(int node_id, unsigned char flags)
{
	if (pool_config->use_watchdog)
		return wd_issue_failover_command(WD_FUNCTION_FAILBACK_REQUEST, &node_id, 1, flags);
	return FAILOVER_RES_PROCEED;
}

/*
 * Function returns the JSON of watchdog nodes
 * pass nodeID = -1 to get list of all nodes
 */
char *
wd_internal_get_watchdog_nodes_json(int nodeID)
{
	return wd_get_watchdog_nodes_json(pool_config->wd_authkey, nodeID);
}

WDFailoverCMDResults
wd_failover_start(void)
{
	if (pool_config->use_watchdog)
		return wd_send_failover_func_status_command(true);
	return FAILOVER_RES_PROCEED;
}

WDFailoverCMDResults
wd_failover_end(void)
{
	if (pool_config->use_watchdog)
		return wd_send_failover_func_status_command(false);
	return FAILOVER_RES_PROCEED;
}

/* These functions are not available for frontend utilities */
unsigned int *
get_ipc_shared_key(void)
{
	return ipc_shared_key;
}

void
set_watchdog_process_needs_cleanup(void)
{
	*watchdog_require_cleanup = true;
}

void
reset_watchdog_process_needs_cleanup(void)
{
	*watchdog_require_cleanup = false;
}

bool
get_watchdog_process_needs_cleanup(void)
{
	return *watchdog_require_cleanup;
}


void
set_watchdog_node_escalated(void)
{
	*watchdog_node_escalated = true;
}

void
reset_watchdog_node_escalated(void)
{
	*watchdog_node_escalated = false;
}

bool
get_watchdog_node_escalation_state(void)
{
	return *watchdog_node_escalated;
}

int
wd_internal_get_watchdog_quorum_state(void)
{
	return get_watchdog_quorum_state(pool_config->wd_authkey);
}

WD_STATES
wd_internal_get_watchdog_local_node_state(void)
{
	return get_watchdog_local_node_state(pool_config->wd_authkey);
}

static WdCommandResult
wd_send_locking_command(WD_LOCK_STANDBY_TYPE lock_type, bool acquire)
{
	WdCommandResult res;
	List *args_list = NULL;
	WDExecCommandArg wdExecCommandArg[2];

	strncpy(wdExecCommandArg[0].arg_name, "StandbyLockType", sizeof(wdExecCommandArg[0].arg_name) - 1);
	snprintf(wdExecCommandArg[0].arg_value, sizeof(wdExecCommandArg[0].arg_value) - 1, "%d",lock_type);

	strncpy(wdExecCommandArg[1].arg_name, "LockingOperation", sizeof(wdExecCommandArg[1].arg_name) - 1);
	snprintf(wdExecCommandArg[1].arg_value, sizeof(wdExecCommandArg[1].arg_value) - 1,
			 "%s",acquire?"acquire":"release");

	args_list = lappend(args_list,&wdExecCommandArg[0]);
	args_list = lappend(args_list,&wdExecCommandArg[1]);
	ereport(DEBUG1,
			(errmsg("sending standby locking request to watchdog")));

	res = wd_execute_cluster_command(WD_COMMAND_LOCK_ON_STANDBY, args_list);
	list_free(args_list);
	return res;
}

WdCommandResult
wd_lock_standby(WD_LOCK_STANDBY_TYPE lock_type)
{
	if (pool_config->use_watchdog)
		return wd_send_locking_command(lock_type, true);
	return COMMAND_OK;
}

WdCommandResult
wd_unlock_standby(WD_LOCK_STANDBY_TYPE lock_type)
{
	if (pool_config->use_watchdog)
		return wd_send_locking_command(lock_type, false);
	return COMMAND_OK;
}
