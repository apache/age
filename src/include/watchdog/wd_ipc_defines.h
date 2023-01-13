/* -*-pgsql-c-*- */
/*
 *
 * $Header$
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
 *
 */

#ifndef WD_IPC_DEFINES_H
#define WD_IPC_DEFINES_H

typedef enum WDFailoverCMDResults
{
	FAILOVER_RES_ERROR = 0,		/* processing of command is failed */
	FAILOVER_RES_TRANSITION,	/* cluster is transitioning and is currently
								 * not accepting any commands. retry is the
								 * best option when this result is returned by
								 * watchdog */
	FAILOVER_RES_SUCCESS,
	FAILOVER_RES_PROCEED,
	FAILOVER_RES_NO_QUORUM,
	FAILOVER_RES_WILL_BE_DONE,
	FAILOVER_RES_NOT_ALLOWED,
	FAILOVER_RES_INVALID_FUNCTION,
	FAILOVER_RES_LEADER_REJECTED,
	FAILOVER_RES_BUILDING_CONSENSUS,
	FAILOVER_RES_CONSENSUS_MAY_FAIL,
	FAILOVER_RES_TIMEOUT
}			WDFailoverCMDResults;

typedef enum WDValueDataType
{
	VALUE_DATA_TYPE_INT = 1,
	VALUE_DATA_TYPE_STRING,
	VALUE_DATA_TYPE_BOOL,
	VALUE_DATA_TYPE_LONG
}			WDValueDataType;

/* IPC MESSAGES TYPES */
#define WD_REGISTER_FOR_NOTIFICATION		'0'
#define WD_NODE_STATUS_CHANGE_COMMAND		'2'
#define WD_GET_NODES_LIST_COMMAND			'3'

#define WD_IPC_CMD_CLUSTER_IN_TRAN			'5'
#define WD_IPC_CMD_RESULT_BAD				'6'
#define WD_IPC_CMD_RESULT_OK				'7'
#define WD_IPC_CMD_TIMEOUT					'8'

#define WD_EXECUTE_CLUSTER_COMMAND			'c'
#define WD_IPC_FAILOVER_COMMAND				'f'
#define WD_IPC_ONLINE_RECOVERY_COMMAND	'r'
#define WD_FAILOVER_LOCKING_REQUEST		's'
#define WD_GET_LEADER_DATA_REQUEST			'd'
#define WD_GET_RUNTIME_VARIABLE_VALUE		'v'
#define WD_FAILOVER_INDICATION				'i'


#define WD_COMMAND_RESTART_CLUSTER		"RESTART_CLUSTER"
#define WD_COMMAND_REELECT_LEADER		"REELECT_LEADER"
#define WD_COMMAND_SHUTDOWN_CLUSTER	"SHUTDOWN_CLUSTER"
#define WD_COMMAND_RELOAD_CONFIG_CLUSTER		"RELOAD_CONFIG_CLUSTER"
#define WD_COMMAND_LOCK_ON_STANDBY		"APPLY_LOCK_ON_STANDBY"


#define WD_FUNCTION_START_RECOVERY		"START_RECOVERY"
#define WD_FUNCTION_END_RECOVERY		"END_RECOVERY"
#define WD_FUNCTION_FAILBACK_REQUEST	"FAILBACK_REQUEST"
#define WD_FUNCTION_DEGENERATE_REQUEST	"DEGENERATE_BACKEND_REQUEST"
#define WD_FUNCTION_PROMOTE_REQUEST		"PROMOTE_BACKEND_REQUEST"

#define WD_DATE_REQ_PG_BACKEND_DATA		"BackendStatus"
#define WD_JSON_KEY_DATA_REQ_TYPE		"DataRequestType"
#define WD_JSON_KEY_VARIABLE_NAME		"VarName"
#define WD_JSON_KEY_VALUE_DATA_TYPE		"ValueDataType"
#define WD_JSON_KEY_VALUE_DATA			"ValueData"

#define WD_REQ_FAILOVER_START			"FAILOVER_START"
#define WD_REQ_FAILOVER_END				"FAILOVER_FINISH"
#define WD_REQ_FAILOVER_RELEASE_LOCK	"RELEASE_LOCK"
#define WD_REQ_FAILOVER_LOCK_STATUS		"CHECK_LOCKED"

#define WD_FAILOVER_RESULT_KEY			"FAILOVER_COMMAND_RESULT"
#define WD_FAILOVER_ID_KEY				"FAILOVER_COMMAND_ID"


#define WD_IPC_AUTH_KEY			"IPCAuthKey"	/* JSON data key for
												 * authentication. watchdog
												 * IPC server use the value
												 * for this key to
												 * authenticate the external
												 * IPC clients The valid value
												 * for this key is wd_authkey
												 * configuration parameter */
#define WD_IPC_SHARED_KEY		"IPCSharedKey"	/* JSON data key for
												 * authentication. watchdog
												 * IPC server use the value of
												 * this key to authenticate
												 * the internal pgpool-II
												 * processes */

/* Watchdog runtime variable names */

#define WD_RUNTIME_VAR_WD_STATE			"WDState"
#define WD_RUNTIME_VAR_QUORUM_STATE		"QuorumState"
#define WD_RUNTIME_VAR_ESCALATION_STATE	"Escalated"

/* Use to inform node new node status by lifecheck */
#define WD_LIFECHECK_NODE_STATUS_DEAD 	1
#define WD_LIFECHECK_NODE_STATUS_ALIVE	2



#endif
