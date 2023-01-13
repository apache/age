
/* -*-pgsql-c-*- */
/*
 *
 * $Header$
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
 *
 */

#ifndef WD_IPC_CONN_H
#define WD_IPC_CONN_H

#include "watchdog/wd_ipc_defines.h"
#include "watchdog/wd_json_data.h"

typedef enum WdCommandResult
{
	CLUSTER_IN_TRANSACTIONING,
	COMMAND_OK,
	COMMAND_FAILED,
	COMMAND_TIMEOUT
}			WdCommandResult;


typedef struct WDIPCCmdResult
{
	char		type;
	int			length;
	char	   *data;
}			WDIPCCmdResult;


extern void wd_ipc_conn_initialize(void);
extern void wd_set_ipc_address(char *socket_dir, int port);
extern size_t estimate_ipc_socket_addr_len(void);
extern char *get_watchdog_ipc_address(void);

extern WDIPCCmdResult * issue_command_to_watchdog(char type, int timeout_sec, char *data, int data_len, bool blocking);

extern void FreeCmdResult(WDIPCCmdResult * res);

#endif							/* WD_IPC_CONN_H */
