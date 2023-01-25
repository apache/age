/*
 * Copyright (c) 2017-2018	Tatsuo Ishii
 * Copyright (c) 2018-2022	PgPool Global Development Group
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
 * Functions to read packets from connection to PostgreSQL.
 */

#include "../../include/config.h"
#include "pgproto/pgproto.h"
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <arpa/inet.h>
#include "pgproto/fe_memutils.h"
#include <libpq-fe.h>
#include "pgproto/read.h"

static char read_char(PGconn *conn);
static int	read_int32(PGconn *conn);
static char *read_bytes(int len, PGconn *conn);
static void read_and_discard(PGconn *conn);
static void read_it(PGconn *conn, char *buf, int len);
static char *read_string(PGconn *conn);

/*
 * Read message from connection until ready for query message is received.  If
 * a positive timeout is given, wait for timeout seconds then return if no
 * data is available from the connection.
 */
void
read_until_ready_for_query(PGconn *conn, int timeout, int wait_for_ready_for_query)
{
	int			kind;
	int			len;
	char	   *buf;
	char		c;
	char	   *p;
	int			fd;
	int			cont;
	struct timeval timeoutval;
	fd_set		readmask;
	int			fds;

	cont = 1;

	while (cont)
	{
		if (timeout > 0)
		{
			fd = PQsocket(conn);

			for (;;)
			{
				FD_ZERO(&readmask);
				FD_SET(fd, &readmask);
				timeoutval.tv_sec = timeout;
				timeoutval.tv_usec = 0;
				fds = select(fd + 1, &readmask, NULL, NULL, &timeoutval);
				if (fds == -1)
				{
					if (errno == EAGAIN || errno == EINTR)
					{
						continue;
					}
					else
					{
						fprintf(stderr, "reading from Pgpool-II failed. reason: %s\n",
								strerror(errno));
						exit(1);
					}
				}
				else if (fds == 0)
				{
					/* socket is not ready for reading */
					return;
				}
				else
					/* socket is ready for reading */
					break;
			}
		}

		kind = read_char(conn);
		switch (kind)
		{
			char	*channel, *payload;
			int		pid;

			case '1':			/* Parse complete */
				fprintf(stderr, "<= BE ParseComplete\n");
				read_and_discard(conn);
				break;

			case '2':			/* Bind complete */
				fprintf(stderr, "<= BE BindComplete\n");
				read_and_discard(conn);
				break;

			case '3':			/* Close complete */
				fprintf(stderr, "<= BE CloseComplete\n");
				read_and_discard(conn);
				break;

			case 'C':			/* Command complete */
				len = read_int32(conn);
				buf = read_bytes(len - sizeof(int), conn);
				fprintf(stderr, "<= BE CommandComplete(%s)\n", buf);
				pg_free(buf);
				break;

			case 'D':			/* Data row */
				fprintf(stderr, "<= BE DataRow\n");
				read_and_discard(conn);
				break;

			case 'E':			/* Error response */
			case 'N':			/* Notice response */
				if (kind == 'E')
					fprintf(stderr, "<= BE ErrorResponse(");
				else
					fprintf(stderr, "<= BE NoticeResponse(");
				len = read_int32(conn);
				p = buf = read_bytes(len - sizeof(int), conn);
				while (*p)
				{
					fprintf(stderr, "%c ", *p);
					p++;
					fprintf(stderr, "%s ", p);
					p += strlen(p) + 1;
				}

				fprintf(stderr, ")\n");

				pg_free(buf);
				break;

			case 'G':			/* Copy in response */
				fprintf(stderr, "<= BE CopyInResponse\n");
				read_and_discard(conn);
				break;

			case 'H':			/* Copy out response */
				fprintf(stderr, "<= BE CopyOutResponse\n");
				read_and_discard(conn);
				break;

			case 'I':			/* Empty query response */
				fprintf(stderr, "<= BE EmptyQueryResponse\n");
				read_and_discard(conn);
				break;

			case 'S':			/* Parameter status */
				fprintf(stderr, "<= BE ParameterStatus(");
				len = read_int32(conn);
				p = read_string(conn);
				fprintf(stderr, "name: \"%s\"", p);
				p = read_string(conn);
				fprintf(stderr, " value: \"%s\")\n", p);
				break;

			case 'T':			/* Row Description */
				fprintf(stderr, "<= BE RowDescription\n");
				read_and_discard(conn);
				break;

			case 'V':			/* Function call response */
				fprintf(stderr, "<= BE FunctionCallResponse\n");
				read_and_discard(conn);
				break;

			case 'W':			/* Copy both response */
				fprintf(stderr, "<= BE CopyBothResponse\n");
				read_and_discard(conn);
				break;

			case 'Z':			/* Ready for Query */
				if (read_int32(conn) < 0)
				{
					fprintf(stderr, "read_int32() failed\n");
					exit(1);
				}
				c = read_char(conn);
				fprintf(stderr, "<= BE ReadyForQuery(%c)\n", c);
				cont = 0;
				break;

			case 'c':			/* Copy Done */
				fprintf(stderr, "<= BE CopyDone\n");
				read_and_discard(conn);
				break;

			case 'd':			/* Copy Data */
				fprintf(stderr, "<= BE CopyData\n");
				read_and_discard(conn);
				break;

			case 'n':			/* No data */
				fprintf(stderr, "<= BE NoData\n");
				read_and_discard(conn);
				break;

			case 's':			/* Portal suspended */
				fprintf(stderr, "<= BE PortalSuspended\n");
				read_and_discard(conn);
				break;

			case 't':			/* Parameter description */
				fprintf(stderr, "<= BE ParameterDescription\n");
				read_and_discard(conn);
				break;

			case 'A':			/* Notification response */
				len = read_int32(conn);
				(void)len;
				pid = read_int32(conn);

				channel = read_string(conn);
				if (channel)
				{
					payload = read_string(conn);
					if (payload)
					{
						fprintf(stderr, "<= BE Notification response. pid: %d channel: %s payload: \"%s\"\n",
								pid, channel, payload);
						free(payload);
					}
					else
					{
						fprintf(stderr, "<= BE Notification response. pid: %d channel: %s\n",
								pid, channel);
					}
					free(channel);
				}
				else
				{
					fprintf(stderr, "<= BE Notification response. pid: %d\n",
						pid);
				}
				break;

			default:
				fprintf(stderr, "<= BE (%c)\n", kind);
				read_and_discard(conn);
				break;
		}

		/*
		 * wait_for_ready_for_query is false, immediately return.
		 */
		if (wait_for_ready_for_query == 0)
			cont = 0;

		/* If nap-between-line is requested, nap for some time */
		if (read_nap > 0)
		{
			(void) usleep(read_nap);
		}
	}
}

/*
 * Read a character from connection.
 */
static char
read_char(PGconn *conn)
{
	char		c;

	read_it(conn, (char *) &c, sizeof(c));

	return c;
}

/*
 * Read an integer from connection.
 */
static int
read_int32(PGconn *conn)
{
	int			len;

	read_it(conn, (char *) &len, sizeof(len));

	return ntohl(len);
}

/*
 * Read specified length of bytes from connection.
 * pg_malloc'ed buffer is returned.
 */
static char *
read_bytes(int len, PGconn *conn)
{
	char	   *buf;

	buf = pg_malloc(len);

	read_it(conn, buf, len);

	return buf;
}

/*
 * Read and discard a packet.
 */
static void
read_and_discard(PGconn *conn)
{
	int			len;
	char	   *buf;

	len = read_int32(conn);

	if (len > sizeof(int))
	{
		buf = read_bytes(len - sizeof(int), conn);
		pg_free(buf);
	}
}

/*
 * Read requested bytes from conn. exit in case of error or EOF.
 */
static void
read_it(PGconn *conn, char *buf, int len)
{
	int			sts;

	if (len <= 0)
		return;

	for (;;)
	{
		sts = read(PQsocket(conn), buf, len);

		if (sts == 0)
		{
			fprintf(stderr, "read_it: EOF detected");
			exit(1);
		}
		else if (sts < 0)
		{
			fprintf(stderr, "read_it: read(2) returns error %s\n", strerror(errno));
			exit(1);
		}

		len -= sts;
		buf += sts;

		if (len <= 0)
			break;
	}
}

/*
 * Read a string from conn and returns malloced buffer pointer.
 */
static char *
read_string(PGconn *conn)
{
#define	PROTO_ALLOC_SIZE	512

	int			sts;
	char		*buf;
	char		*p;
	int			alloc_factor = 1;
	int			len;

	buf = pg_malloc(PROTO_ALLOC_SIZE);
	len = PROTO_ALLOC_SIZE;
	p = buf;

	for (;;)
	{
		sts = read(PQsocket(conn), p, 1);

		if (sts == 0)
		{
			fprintf(stderr, "read_string: EOF detected");
			exit(1);
		}
		else if (sts < 0)
		{
			fprintf(stderr, "read_string: read(2) returns error %s\n", strerror(errno));
			exit(1);
		}


		if (*p == '\0')
		{
			return buf;
		}

		len--;
		p++;

		if (len <= 0)
		{
			alloc_factor++;
			buf = pg_realloc(buf, PROTO_ALLOC_SIZE * alloc_factor);
			len = PROTO_ALLOC_SIZE;
		}
	}

}
