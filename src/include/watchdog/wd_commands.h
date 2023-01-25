
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

#ifndef WD_COMMANDS_H
#define WD_COMMANDS_H

#include "watchdog/wd_ipc_defines.h"
#include "watchdog/wd_ipc_conn.h"

typedef struct WDNodeInfo
{
	int			state;
	int			membership_status;
	char		membership_status_string[WD_MAX_HOST_NAMELEN];
	char		nodeName[WD_MAX_HOST_NAMELEN];
	char		hostName[WD_MAX_HOST_NAMELEN];	/* host name */
	char		stateName[WD_MAX_HOST_NAMELEN]; /* watchdog state name */
	int			wd_port;		/* watchdog port */
	int			pgpool_port;	/* pgpool port */
	int			wd_priority;	/* node priority */
	char		delegate_ip[WD_MAX_HOST_NAMELEN];	/* delegate IP */
	int			id;
}			WDNodeInfo;

typedef struct WDGenericData
{
	WDValueDataType valueType;
	union data
	{
		char	   *stringVal;
		int			intVal;
		bool		boolVal;
		long		longVal;
	}			data;
}			WDGenericData;



extern WDGenericData * get_wd_runtime_variable_value(char *wd_authkey, char *varName);
extern WD_STATES get_watchdog_local_node_state(char *wd_authkey);
extern int	get_watchdog_quorum_state(char *wd_authkey);
extern char *wd_get_watchdog_nodes_json(char *wd_authkey, int nodeID);
extern void set_wd_command_timeout(int sec);
extern char* get_request_json(char *key, char *value, char *authKey);
extern WDNodeInfo *parse_watchdog_node_info_from_wd_node_json(json_value * source);
#endif							/* WD_COMMANDS_H */
