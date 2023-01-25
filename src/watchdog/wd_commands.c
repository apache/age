/*
 * $Header$
 *
 * Handles watchdog ipc commands that are also allowed from outside the pgpool-II
 *
 * pgpool: a language independent connection pool server for PostgreSQL
 * written by Tatsuo Ishii
 *
 * Copyright (c) 2003-2019	PgPool Global Development Group
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
#ifndef POOL_PRIVATE
#include "utils/elog.h"
#else
#include "utils/fe_ports.h"
#endif

#include "utils/json_writer.h"
#include "utils/json.h"
#include "watchdog/wd_commands.h"
#include "watchdog/wd_ipc_defines.h"

#define WD_DEFAULT_IPC_COMMAND_TIMEOUT	8	/* default number of seconds to
											 * wait for IPC command results */

int wd_command_timeout_sec = WD_DEFAULT_IPC_COMMAND_TIMEOUT;

WD_STATES
get_watchdog_local_node_state(char* wd_authkey)
{
	WD_STATES	ret = WD_DEAD;
	WDGenericData *state = get_wd_runtime_variable_value(wd_authkey, WD_RUNTIME_VAR_WD_STATE);

	if (state == NULL)
	{
		ereport(LOG,
				(errmsg("failed to get current state of local watchdog node"),
				 errdetail("get runtime variable value from watchdog returned no data")));
		return WD_DEAD;
	}
	if (state->valueType != VALUE_DATA_TYPE_INT)
	{
		ereport(LOG,
				(errmsg("failed to get current state of local watchdog node"),
				 errdetail("get runtime variable value from watchdog returned invalid value type")));
		pfree(state);
		return WD_DEAD;
	}
	ret = (WD_STATES) state->data.intVal;
	pfree(state);
	return ret;
}

int
get_watchdog_quorum_state(char* wd_authkey)
{
	WD_STATES	ret = WD_DEAD;
	WDGenericData *state = get_wd_runtime_variable_value(wd_authkey, WD_RUNTIME_VAR_QUORUM_STATE);

	if (state == NULL)
	{
		ereport(LOG,
				(errmsg("failed to get quorum state of watchdog cluster"),
				 errdetail("get runtime variable value from watchdog returned no data")));
		return WD_DEAD;
	}
	if (state->valueType != VALUE_DATA_TYPE_INT)
	{
		ereport(LOG,
				(errmsg("failed to get quorum state of watchdog cluster"),
				 errdetail("get runtime variable value from watchdog returned invalid value type")));
		pfree(state);
		return WD_DEAD;
	}
	ret = (WD_STATES) state->data.intVal;
	pfree(state);
	return ret;
}


/*
 * Function gets the runtime value of watchdog variable using the
 * watchdog IPC
 */
WDGenericData *
get_wd_runtime_variable_value(char* wd_authkey, char *varName)
{
	char	   *data = get_request_json(WD_JSON_KEY_VARIABLE_NAME, varName,
											   wd_authkey);

	WDIPCCmdResult *result = issue_command_to_watchdog(WD_GET_RUNTIME_VARIABLE_VALUE,
													   wd_command_timeout_sec,
													   data, strlen(data), true);

	pfree(data);

	if (result == NULL)
	{
		ereport(WARNING,
				(errmsg("get runtime variable value from watchdog failed"),
				 errdetail("issue command to watchdog returned NULL")));
		return NULL;
	}
	if (result->type == WD_IPC_CMD_CLUSTER_IN_TRAN)
	{
		ereport(WARNING,
				(errmsg("get runtime variable value from watchdog failed"),
				 errdetail("watchdog cluster is not in stable state"),
				 errhint("try again when the cluster is fully initialized")));
		FreeCmdResult(result);
		return NULL;
	}
	else if (result->type == WD_IPC_CMD_TIMEOUT)
	{
		ereport(WARNING,
				(errmsg("get runtime variable value from watchdog failed"),
				 errdetail("ipc command timeout")));
		FreeCmdResult(result);
		return NULL;
	}
	else if (result->type == WD_IPC_CMD_RESULT_OK)
	{
		json_value *root = NULL;
		WDGenericData *genData = NULL;
		WDValueDataType dayaType;

		root = json_parse(result->data, result->length);
		/* The root node must be object */
		if (root == NULL || root->type != json_object)
		{
			FreeCmdResult(result);
			return NULL;
		}

		if (json_get_int_value_for_key(root, WD_JSON_KEY_VALUE_DATA_TYPE, (int *) &dayaType))
		{
			FreeCmdResult(result);
			json_value_free(root);
			return NULL;
		}

		switch (dayaType)
		{
			case VALUE_DATA_TYPE_INT:
				{
					int			intVal;

					if (json_get_int_value_for_key(root, WD_JSON_KEY_VALUE_DATA, &intVal))
					{
						ereport(WARNING,
								(errmsg("get runtime variable value from watchdog failed"),
								 errdetail("unable to get INT value from JSON data returned by watchdog")));
					}
					else
					{
						genData = palloc(sizeof(WDGenericData));
						genData->valueType = dayaType;
						genData->data.intVal = intVal;
					}
				}
				break;

			case VALUE_DATA_TYPE_LONG:
				{
					long		longVal;

					if (json_get_long_value_for_key(root, WD_JSON_KEY_VALUE_DATA, &longVal))
					{
						ereport(WARNING,
								(errmsg("get runtime variable value from watchdog failed"),
								 errdetail("unable to get LONG value from JSON data returned by watchdog")));
					}
					else
					{
						genData = palloc(sizeof(WDGenericData));
						genData->valueType = dayaType;
						genData->data.longVal = longVal;
					}
				}
				break;

			case VALUE_DATA_TYPE_BOOL:
				{
					bool		boolVal;

					if (json_get_bool_value_for_key(root, WD_JSON_KEY_VALUE_DATA, &boolVal))
					{
						ereport(WARNING,
								(errmsg("get runtime variable value from watchdog failed"),
								 errdetail("unable to get BOOL value from JSON data returned by watchdog")));
					}
					else
					{
						genData = palloc(sizeof(WDGenericData));
						genData->valueType = dayaType;
						genData->data.boolVal = boolVal;
					}
				}
				break;

			case VALUE_DATA_TYPE_STRING:
				{
					char	   *ptr = json_get_string_value_for_key(root, WD_JSON_KEY_VALUE_DATA);

					if (ptr == NULL)
					{
						ereport(WARNING,
								(errmsg("get runtime variable value from watchdog failed"),
								 errdetail("unable to get STRING value from JSON data returned by watchdog")));
					}
					else
					{
						genData = palloc(sizeof(WDGenericData));
						genData->valueType = dayaType;
						genData->data.stringVal = pstrdup(ptr);
					}
				}
				break;

			default:
				ereport(WARNING,
						(errmsg("get runtime variable value from watchdog failed, unknown value data type")));
				break;
		}

		json_value_free(root);
		FreeCmdResult(result);
		return genData;
	}

	ereport(WARNING,
			(errmsg("get runtime variable value from watchdog failed")));
	FreeCmdResult(result);
	return NULL;

}

/*
 * Function returns the JSON of watchdog nodes
 * pass nodeID = -1 to get list of all nodes
 */
char *
wd_get_watchdog_nodes_json(char *wd_authkey, int nodeID)
{
	WDIPCCmdResult *result;
	char	   *json_str;

	JsonNode   *jNode = jw_create_with_object(true);

	jw_put_int(jNode, "NodeID", nodeID);

	if (wd_authkey != NULL && strlen(wd_authkey) > 0)
		jw_put_string(jNode, WD_IPC_AUTH_KEY, wd_authkey); /* put the auth key */

	jw_finish_document(jNode);

	json_str = jw_get_json_string(jNode);

	result = issue_command_to_watchdog(WD_GET_NODES_LIST_COMMAND
									   ,wd_command_timeout_sec,
									   json_str, strlen(json_str), true);

	jw_destroy(jNode);

	if (result == NULL)
	{
		ereport(WARNING,
				(errmsg("get watchdog nodes command failed"),
				 errdetail("issue command to watchdog returned NULL")));
		return NULL;
	}
	else if (result->type == WD_IPC_CMD_CLUSTER_IN_TRAN)
	{
		ereport(WARNING,
				(errmsg("get watchdog nodes command failed"),
				 errdetail("watchdog cluster is not in stable state"),
				 errhint("try again when the cluster is fully initialized")));
		FreeCmdResult(result);
		return NULL;
	}
	else if (result->type == WD_IPC_CMD_TIMEOUT)
	{
		ereport(WARNING,
				(errmsg("get watchdog nodes command failed"),
				 errdetail("ipc command timeout")));
		FreeCmdResult(result);
		return NULL;
	}
	else if (result->type == WD_IPC_CMD_RESULT_OK)
	{
		char	   *data = result->data;

		/* do not free the result->data, Save the data copy */
		pfree(result);
		return data;
	}
	FreeCmdResult(result);
	return NULL;
}

WDNodeInfo *
parse_watchdog_node_info_from_wd_node_json(json_value * source)
{
	char	   *ptr;
	WDNodeInfo *wdNodeInfo = palloc0(sizeof(WDNodeInfo));
	
	if (source->type != json_object)
		ereport(ERROR,
				(errmsg("invalid json data"),
				 errdetail("node is not of object type")));
	
	if (json_get_int_value_for_key(source, "ID", &wdNodeInfo->id))
	{
		ereport(ERROR,
				(errmsg("invalid json data"),
				 errdetail("unable to find Watchdog Node ID")));
	}
	if (json_get_int_value_for_key(source, "Membership", &wdNodeInfo->membership_status))
	{
		/* would be from the older version. No need to panic */
		wdNodeInfo->membership_status = WD_NODE_MEMBERSHIP_ACTIVE;
	}

	ptr = json_get_string_value_for_key(source, "MembershipString");
	if (ptr == NULL)
	{
		strncpy(wdNodeInfo->membership_status_string, "NOT-Available", sizeof(wdNodeInfo->membership_status_string) - 1);
	}
	else
		strncpy(wdNodeInfo->membership_status_string, ptr, sizeof(wdNodeInfo->membership_status_string) - 1);

	
	ptr = json_get_string_value_for_key(source, "NodeName");
	if (ptr == NULL)
	{
		ereport(ERROR,
				(errmsg("invalid json data"),
				 errdetail("unable to find Watchdog Node Name")));
	}
	strncpy(wdNodeInfo->nodeName, ptr, sizeof(wdNodeInfo->nodeName) - 1);
	
	ptr = json_get_string_value_for_key(source, "HostName");
	if (ptr == NULL)
	{
		ereport(ERROR,
				(errmsg("invalid json data"),
				 errdetail("unable to find Watchdog Host Name")));
	}
	strncpy(wdNodeInfo->hostName, ptr, sizeof(wdNodeInfo->hostName) - 1);
	
	ptr = json_get_string_value_for_key(source, "DelegateIP");
	if (ptr == NULL)
	{
		ereport(ERROR,
				(errmsg("invalid json data"),
				 errdetail("unable to find Watchdog delegate IP")));
	}
	strncpy(wdNodeInfo->delegate_ip, ptr, sizeof(wdNodeInfo->delegate_ip) - 1);
	
	if (json_get_int_value_for_key(source, "WdPort", &wdNodeInfo->wd_port))
	{
		ereport(ERROR,
				(errmsg("invalid json data"),
				 errdetail("unable to find WdPort")));
	}
	
	if (json_get_int_value_for_key(source, "PgpoolPort", &wdNodeInfo->pgpool_port))
	{
		ereport(ERROR,
				(errmsg("invalid json data"),
				 errdetail("unable to find PgpoolPort")));
	}
	
	if (json_get_int_value_for_key(source, "State", &wdNodeInfo->state))
	{
		ereport(ERROR,
				(errmsg("invalid json data"),
				 errdetail("unable to find state")));
	}
	
	ptr = json_get_string_value_for_key(source, "StateName");
	if (ptr == NULL)
	{
		ereport(ERROR,
				(errmsg("invalid json data"),
				 errdetail("unable to find Watchdog State Name")));
	}
	strncpy(wdNodeInfo->stateName, ptr, sizeof(wdNodeInfo->stateName) - 1);
	
	if (json_get_int_value_for_key(source, "Priority", &wdNodeInfo->wd_priority))
	{
		ereport(ERROR,
				(errmsg("invalid json data"),
				 errdetail("unable to find state")));
	}
	
	return wdNodeInfo;
	
}

extern void set_wd_command_timeout(int sec)
{
	wd_command_timeout_sec = sec;
}

/* The function returns the simple JSON string that contains
 * only one KEY,VALUE along with the authkey key value if provided
 */
char *
get_request_json(char *key, char *value, char *authKey)
{
	char	   *json_str;

	JsonNode   *jNode = jw_create_with_object(true);

	if (authKey != NULL && strlen(authKey) > 0)
		jw_put_string(jNode, WD_IPC_AUTH_KEY, authKey); /* put the auth key */

	jw_put_string(jNode, key, value);
	jw_finish_document(jNode);
	json_str = pstrdup(jw_get_json_string(jNode));
	jw_destroy(jNode);
	return json_str;
}

