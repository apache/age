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
 *
 * Process Parse, Bind, Execute message.
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
#include "pgproto/fe_memutils.h"
#include <libpq-fe.h>
#include "pgproto/read.h"
#include "pgproto/send.h"
#include "pgproto/extended_query.h"
#include "pgproto/buffer.h"

/*
 * Send parse message. "conn" should at the point right after the message kind
 * was read.
 */
void
process_parse(char *buf, PGconn *conn)
{
	char	   *query;
	int			len;
	char	   *stmt;
	short		noids;
	int			oids[MAXENTRIES];
	int			i;
	char	   *bufp;

	SKIP_TABS(buf);

	len = sizeof(int);

	stmt = buffer_read_string(buf, &bufp);
	buf = bufp;
	len += strlen(stmt) + 1;

	SKIP_TABS(buf);

	query = buffer_read_string(buf, &bufp);
	buf = bufp;
	len += strlen(query) + 1;

	SKIP_TABS(buf);

	fprintf(stderr, "FE=> Parse(stmt=\"%s\", query=\"%s\")", stmt, query);

	noids = buffer_read_int(buf, &bufp);
	buf = bufp;

	if (noids > MAXENTRIES)
	{
		fprintf(stderr, "Too many oid params for parse message (%d)\n", noids);
		exit(1);
	}

	len += sizeof(short) + noids * sizeof(int);

	if (noids > 0)
	{
		fprintf(stderr, ", oids={");

		for (i = 0; i < noids; i++)
		{
			oids[i] = buffer_read_int(buf, &bufp);
			fprintf(stderr, "%d", oids[i]);
			if ((i + 1) != noids)
				fprintf(stderr, ",");
			buf = bufp;
		}
	}
	fprintf(stderr, "\n");

	send_char('P', conn);
	send_int(len, conn);
	send_string(stmt, conn);
	free(stmt);
	send_string(query, conn);
	free(query);
	send_int16(noids, conn);
	if (noids > 0)
	{
		for (i = 0; i < noids; i++)
		{
			send_int(oids[i], conn);
		}
	}
}

/*
 * Send bind message. "conn" should be at the point right after the message kind
 * was read.
 */
void
process_bind(char *buf, PGconn *conn)
{
	int			len;
	char	   *stmt;
	char	   *portal;
	short		nparams;
	short		ncodes;
	short		codes[MAXENTRIES];
	int			paramlens[MAXENTRIES];
	char	   *paramvals[MAXENTRIES];
	short		nresult_formatcodes;
	short		result_formatcodes[MAXENTRIES];
	int			i;
	char	   *bufp;

	SKIP_TABS(buf);

	len = sizeof(int);

	portal = buffer_read_string(buf, &bufp);
	buf = bufp;
	len += strlen(portal) + 1;

	SKIP_TABS(buf);

	stmt = buffer_read_string(buf, &bufp);
	buf = bufp;
	len += strlen(stmt) + 1;

	fprintf(stderr, "FE=> Bind(stmt=\"%s\", portal=\"%s\")", stmt, portal);

	SKIP_TABS(buf);

	ncodes = buffer_read_int(buf, &bufp);
	len += sizeof(short) + sizeof(short) * ncodes;
	buf = bufp;

	SKIP_TABS(buf);

	if (ncodes > MAXENTRIES)
	{
		fprintf(stderr, "Too many codes for bind message (%d)\n", ncodes);
		exit(1);
	}

	if (ncodes > 0)
	{
		for (i = 0; i < ncodes; i++)
		{
			codes[i] = buffer_read_int(buf, &bufp);
			buf = bufp;
			SKIP_TABS(buf);
		}
	}

	nparams = buffer_read_int(buf, &bufp);
	len += sizeof(short) + sizeof(short) * nparams;
	buf = bufp;
	SKIP_TABS(buf);

	if (nparams > MAXENTRIES)
	{
		fprintf(stderr, "Too many params for bind message (%d)\n", nparams);
		exit(1);
	}

	for (i = 0; i < nparams; i++)
	{
		paramlens[i] = buffer_read_int(buf, &bufp);
		len += sizeof(int);

		if (paramlens[i] > 0)
		{
			buf = bufp;
			paramvals[i] = buffer_read_string(buf, &bufp);
			buf = bufp;
			SKIP_TABS(buf);
			len += paramlens[i];
		}
	}

	SKIP_TABS(buf);

	nresult_formatcodes = buffer_read_int(buf, &bufp);
	buf = bufp;
	len += sizeof(short) + sizeof(short) * nresult_formatcodes;
	SKIP_TABS(buf);

	if (nresult_formatcodes >= 2)
	{
		for (i = 0; i < nresult_formatcodes; i++)
		{
			result_formatcodes[i] = buffer_read_int(buf, &bufp);
			buf = bufp;
			SKIP_TABS(buf);
		}
	}
	fprintf(stderr, "\n");

	send_char('B', conn);
	send_int(len, conn);
	send_string(portal, conn);
	free(portal);
	send_string(stmt, conn);
	free(stmt);
	send_int16(ncodes, conn);
	for (i = 0; i < ncodes; i++)
	{
		send_int16(codes[i], conn);
	}

	send_int16(nparams, conn);
	for (i = 0; i < nparams; i++)
	{
		if (paramlens[i] != -1)
		{
			if (ncodes == 0 || codes[i] == 0)
			{
				send_string(paramvals[i], conn);
			}
			else
			{
				send_int(atoi(paramvals[i]), conn);
			}
		}
	}

	send_int16(nresult_formatcodes, conn);
	for (i = 0; i < nresult_formatcodes; i++)
	{
		send_int16(result_formatcodes[i], conn);
	}
}

/*
 * Send execute message. "conn" should at the point right after the message
 * kind was read.
 */
void
process_execute(char *buf, PGconn *conn)
{
	int			len;
	char	   *portal;
	int			maxrows;
	char	   *bufp;

	SKIP_TABS(buf);

	len = sizeof(int);

	portal = buffer_read_string(buf, &bufp);
	buf = bufp;
	len += strlen(portal) + 1;

	SKIP_TABS(buf);

	fprintf(stderr, "FE=> Execute(portal=\"%s\")\n", portal);

	SKIP_TABS(buf);

	maxrows = buffer_read_int(buf, &bufp);

	len += sizeof(int);

	send_char('E', conn);
	send_int(len, conn);
	send_string(portal, conn);
	send_int(maxrows, conn);
	free(portal);
}

/*
 * Send describe message. "conn" should at the point right after the message kind
 * was read.
 */
void
process_describe(char *buf, PGconn *conn)
{
	char		kind;
	int			len;
	char	   *stmt;
	char	   *bufp;

	SKIP_TABS(buf);

	len = sizeof(int);

	kind = buffer_read_char(buf, &bufp);
	buf = bufp;
	len += 1;

	SKIP_TABS(buf);

	stmt = buffer_read_string(buf, &bufp);
	buf = bufp;
	len += strlen(stmt) + 1;

	SKIP_TABS(buf);

	if (kind == 'S')
	{
		fprintf(stderr, "FE=> Describe(stmt=\"%s\")\n", stmt);
	}
	else if (kind == 'P')
	{
		fprintf(stderr, "FE=> Describe(portal=\"%s\")\n", stmt);
	}
	else
	{
		fprintf(stderr, "Close: unknown kind:%c\n", kind);
		exit(1);
	}

	send_char('D', conn);
	send_int(len, conn);
	send_char(kind, conn);
	send_string(stmt, conn);
	free(stmt);
}

/*
 * Send close message. "conn" should at the point right after the message kind
 * was read.
 */
void
process_close(char *buf, PGconn *conn)
{
	char		kind;
	int			len;
	char	   *stmt;
	char	   *bufp;

	SKIP_TABS(buf);

	len = sizeof(int);

	kind = buffer_read_char(buf, &bufp);
	buf = bufp;
	len += 1;

	SKIP_TABS(buf);

	stmt = buffer_read_string(buf, &bufp);
	buf = bufp;
	len += strlen(stmt) + 1;

	SKIP_TABS(buf);

	if (kind == 'S')
	{
		fprintf(stderr, "FE=> Close(stmt=\"%s\")\n", stmt);
	}
	else if (kind == 'P')
	{
		fprintf(stderr, "FE=> Close(portal=\"%s\")\n", stmt);
	}
	else
	{
		fprintf(stderr, "Close: unknown kind:%c\n", kind);
		exit(1);
	}

	send_char('C', conn);
	send_int(len, conn);
	send_char(kind, conn);
	send_string(stmt, conn);
	free(stmt);
}
