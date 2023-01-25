/*
 * Copyright (c) 2017-2018	Tatsuo Ishii
 * Copyright (c) 2018-2021	PgPool Global Development Group
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
 */

#include "../../include/config.h"
#include "pgproto/pgproto.h"
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <getopt.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include "pgproto/fe_memutils.h"
#include <libpq-fe.h>
#include "pgproto/read.h"
#include "pgproto/send.h"

static	void write_it(int fd, void *buf, int len);

/*
 * Send a character to the connection.
 */
void
send_char(char c, PGconn *conn)
{
	write_it(PQsocket(conn), &c, 1);
}

/*
 * Send a 4-byte integer to the connection.
 */
void
send_int(int intval, PGconn *conn)
{
	int			l = htonl(intval);

	write_it(PQsocket(conn), &l, sizeof(l));
}

/*
 * Send a 2-byte integer to the connection.
 */
void
send_int16(short shortval, PGconn *conn)
{
	short		s = htons(shortval);

	write_it(PQsocket(conn), &s, sizeof(s));
}

/*
 * Send a string to the connection. buf must be NULL terminated.
 */
void
send_string(char *buf, PGconn *conn)
{
	write_it(PQsocket(conn), buf, strlen(buf) + 1);
}

/*
 * Send byte to the connection.
 */
void
send_byte(char *buf, int len, PGconn *conn)
{
	write_it(PQsocket(conn), buf, len);
}

/*
 * Wrapper for write(2).
 */
static
void write_it(int fd, void *buf, int len)
{
	int errsave = errno;
	errno = 0;
	if (write(fd, buf, len) < 0)
	{
		fprintf(stderr, "write_it: warning write(2) failed: %s\n", strerror(errno));
	}
	errno = errsave;
}

