/*
 * $Header$
 *
 * Handles watchdog connection, and protocol communication with pgpool-II
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

#include "utils/socket_stream.h"
#include "pool_config.h"
#include "watchdog/wd_ipc_conn.h"
#include "watchdog/wd_ipc_defines.h"

static int	open_wd_command_sock(bool throw_error);
/* shared memory variables */
char	   *watchdog_ipc_address = NULL;

void wd_set_ipc_address(char *socket_dir, int port)
{
	if (watchdog_ipc_address == NULL)
	{
		char		wd_ipc_sock_addr[255];

		snprintf(wd_ipc_sock_addr, sizeof(wd_ipc_sock_addr), "%s/.s.PGPOOLWD_CMD.%d",
				 socket_dir,
				 port);

#ifndef POOL_PRIVATE
		watchdog_ipc_address = pool_shared_memory_segment_get_chunk(strlen(wd_ipc_sock_addr) + 1);
		strcpy(watchdog_ipc_address, wd_ipc_sock_addr);
#else
		watchdog_ipc_address = pstrdup(wd_ipc_sock_addr);
#endif
	}
}

void
wd_ipc_conn_initialize(void)
{
	if (watchdog_ipc_address == NULL)
	{
		wd_set_ipc_address(pool_config->wd_ipc_socket_dir, pool_config->wd_nodes.wd_node_info[pool_config->pgpool_node_id].wd_port);
	}
}

size_t estimate_ipc_socket_addr_len(void)
{
	return strlen(pool_config->wd_ipc_socket_dir) + 25; /* wd_ipc_socket_dir/.s.PGPOOLWD_CMD.port*/
}

char *
get_watchdog_ipc_address(void)
{
	return watchdog_ipc_address;
}

/*
 * function issues the command to watchdog process over the watchdog
 * IPC command socket.
 * type:            command type to send. valid command
 *                  types are defined in wd_ipc_defines.h
 * timeout_sec:     number of seconds to wait for the command response
 *                  from watchdog
 * data:            command data
 * data_len:        length of data
 * blocking:        send true if caller wants to wait for the results
 *                  when blocking is false the timeout_sec is ignored
 */
WDIPCCmdResult *
issue_command_to_watchdog(char type, int timeout_sec, char *data, int data_len, bool blocking)
{
	struct timeval start_time,
				tv;
	int			sock;
	WDIPCCmdResult *result = NULL;
	char		res_type = 'P';
	int			res_length,
				len;

	gettimeofday(&start_time, NULL);

	/* open the watchdog command socket for IPC */
	sock = open_wd_command_sock(false);
	if (sock < 0)
		return NULL;

	len = htonl(data_len);

	if (socket_write(sock, &type, sizeof(char)) <= 0)
	{
		close(sock);
		return NULL;
	}

	if (socket_write(sock, &len, sizeof(int)) <= 0)
	{
		close(sock);
		return NULL;
	}
	if (data && data_len > 0)
	{
		if (socket_write(sock, data, data_len) <= 0)
		{
			close(sock);
			return NULL;
		}
	}

	if (blocking)
	{
		/* if we are asked to wait for results */
		fd_set		fds;
		struct timeval *timeout_st = NULL;

		if (timeout_sec > 0)
		{
			tv.tv_sec = timeout_sec;
			tv.tv_usec = 0;
			timeout_st = &tv;
		}
		FD_ZERO(&fds);
		FD_SET(sock, &fds);
		for (;;)
		{
			int			select_res;

			select_res = select(sock + 1, &fds, NULL, NULL, timeout_st);
			if (select_res == 0)
			{
				close(sock);
				result = palloc(sizeof(WDIPCCmdResult));
				result->type = WD_IPC_CMD_TIMEOUT;
				result->length = 0;
				result->data = NULL;
				return result;
			}
			if (select_res < 0)
			{
				if (errno == EAGAIN || errno == EINTR)
					continue;
				ereport(WARNING,
						(errmsg("error reading from IPC command socket for ipc command %c", type),
						 errdetail("select system call failed with error \"%m\"")));
				close(sock);
				return NULL;
			}
			if (select_res > 0)
			{
				/* read the result type char */
				if (socket_read(sock, &res_type, 1, 0) <= 0)
				{
					ereport(WARNING,
							(errmsg("error reading from IPC command socket for ipc command %c", type),
							 errdetail("read from socket failed with error \"%m\"")));
					close(sock);
					return result;
				}
				/* read the result data length */
				if (socket_read(sock, &res_length, sizeof(int), 0) <= 0)
				{
					ereport(WARNING,
							(errmsg("error reading from IPC command socket for ipc command %c", type),
							 errdetail("read from socket failed with error \"%m\"")));
					close(sock);
					return result;
				}

				result = palloc(sizeof(WDIPCCmdResult));
				result->type = res_type;
				result->length = ntohl(res_length);
				result->data = NULL;

				if (result->length > 0)
				{
					result->data = palloc(result->length);
					if (socket_read(sock, result->data, result->length, 0) <= 0)
					{
						pfree(result->data);
						pfree(result);
						ereport(DEBUG1,
								(errmsg("error reading from IPC command socket for ipc command %c", type),
								 errdetail("read from socket failed with error \"%m\"")));
						close(sock);
						return NULL;
					}
				}
				break;
			}
		}
	}
	else
	{
		/*
		 * For non blocking mode if we are successful in sending the command
		 * that means the command is success
		 */
		result = palloc0(sizeof(WDIPCCmdResult));
		result->type = WD_IPC_CMD_RESULT_OK;
	}
	close(sock);
	return result;
}

static int
open_wd_command_sock(bool throw_error)
{
	size_t		len;
	struct sockaddr_un addr;
	int			sock = -1;

	/* We use unix domain stream sockets for the purpose */
	if ((sock = socket(AF_UNIX, SOCK_STREAM, 0)) < 0)
	{
		/* socket create failed */
		ereport(throw_error ? ERROR : LOG,
				(errmsg("failed to connect to watchdog command server socket"),
				 errdetail("connect on \"%s\" failed with reason: \"%m\"", addr.sun_path)));
		return -1;
	}

	memset(&addr, 0, sizeof(addr));
	addr.sun_family = AF_UNIX;
	snprintf(addr.sun_path, sizeof(addr.sun_path), "%s", watchdog_ipc_address);
	len = sizeof(struct sockaddr_un);

	if (connect(sock, (struct sockaddr *) &addr, len) == -1)
	{
		close(sock);
		ereport(throw_error ? ERROR : LOG,
				(errmsg("failed to connect to watchdog command server socket"),
				 errdetail("connect on \"%s\" failed with reason: \"%m\"", addr.sun_path)));
		return -1;
	}
	return sock;
}

void
FreeCmdResult(WDIPCCmdResult * res)
{
	if (res == NULL)
		return;
	
	if (res->data)
		pfree(res->data);
	pfree(res);
}
