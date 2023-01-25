/* -*-pgsql-c-*- */
/*
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
* pool_stream.c: stream I/O modules
*
*/

#include "config.h"

#ifdef HAVE_SYS_SELECT_H
#include <sys/select.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>

#ifdef HAVE_FCNTL_H
#include <fcntl.h>
#endif

#include "pool.h"
#include "utils/socket_stream.h"
#ifndef POOL_PRIVATE
#include "utils/elog.h"
#else
#include "utils/fe_ports.h"
#endif


/*
 * set non-block flag
 */
void
socket_set_nonblock(int fd)
{
	int			var;

	/* set fd to none blocking */
	var = fcntl(fd, F_GETFL, 0);
	if (var == -1)
	{
		ereport(FATAL,
				(errmsg("unable to set options on socket"),
				 errdetail("fcntl system call failed with error \"%m\"")));

	}
	if (fcntl(fd, F_SETFL, var | O_NONBLOCK) == -1)
	{
		ereport(FATAL,
				(errmsg("unable to set options on socket"),
				 errdetail("fcntl system call failed with error \"%m\"")));
	}
}

/*
 * unset non-block flag
 */
void
socket_unset_nonblock(int fd)
{
	int			var;

	/* set fd to none blocking */
	var = fcntl(fd, F_GETFL, 0);
	if (var == -1)
	{
		ereport(FATAL,
				(errmsg("unable to set options on socket"),
				 errdetail("fcntl system call failed with error \"%m\"")));
	}
	if (fcntl(fd, F_SETFL, var & ~O_NONBLOCK) == -1)
	{
		ereport(FATAL,
				(errmsg("unable to set options on socket"),
				 errdetail("fcntl system call failed with error \"%m\"")));
	}
}

#ifdef DEBUG
/*
 * Debug aid
 */
static void
dump_buffer(char *buf, int len)
{
	if (!message_level_is_interesting(DEBUG5))
		return;

	while (--len)
	{
		ereport(DEBUG5,
				(errmsg("%02x", *buf++)));
	}
}
#endif
int
socket_write(int fd, void *buf, size_t len)
{
	int			bytes_send = 0;

	do
	{
		int			ret;

		ret = write(fd, buf + bytes_send, (len - bytes_send));
		if (ret <= 0)
		{
			if (errno == EINTR || errno == EAGAIN)
			{
				ereport(DEBUG5,
						(errmsg("write on socket failed with error :\"%m\""),
						 errdetail("retrying...")));
				continue;
			}
			ereport(LOG,
					(errmsg("write on socket failed"),
					 errdetail("%m")));
			return -1;
		}
		bytes_send += ret;
	} while (bytes_send < len);
	return bytes_send;
}

int
socket_read(int fd, void *buf, size_t len, int timeout)
{
	int			ret,
				read_len;

	read_len = 0;
	struct timeval timeoutval;
	fd_set		readmask;
	int			fds;

	while (read_len < len)
	{
		FD_ZERO(&readmask);
		FD_SET(fd, &readmask);

		timeoutval.tv_sec = timeout;
		timeoutval.tv_usec = 0;

		fds = select(fd + 1, &readmask, NULL, NULL, timeout ? &timeoutval : NULL);
		if (fds == -1)
		{
			if (errno == EAGAIN || errno == EINTR)
				continue;

			ereport(WARNING,
					(errmsg("system call select() failed"),
					 errdetail("%m")));
			return -1;
		}
		else if (fds == 0)
		{
			return -2;
		}
		ret = read(fd, buf + read_len, (len - read_len));
		if (ret < 0)
		{
			if (errno == EINTR || errno == EAGAIN)
			{
				ereport(DEBUG5,
						(errmsg("read from socket failed with error :\"%m\""),
						 errdetail("retrying...")));
				continue;
			}
			ereport(LOG,
					(errmsg("read from socket failed with error :\"%m\"")));
			return -1;
		}
		if (ret == 0)
		{
			ereport(LOG,
					(errmsg("read from socket failed, remote end closed the connection")));
			return 0;
		}
		read_len += ret;
	}
	return read_len;
}
