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
#include "pgproto/buffer.h"
#include "pgproto/extended_query.h"

#undef DEBUG

static void show_version(void);
static void usage(void);
static FILE *openfile(char *filename);
static PGconn *connect_db(char *host, char *port, char *user, char *database);
static void read_and_process(FILE *fd, PGconn *conn);
static int	process_a_line(char *buf, PGconn *con);
static int	process_message_type(int kind, char *buf, PGconn *conn);
static void process_function_call(char *buf, PGconn *conn);

int			read_nap = 0;

int
main(int argc, char **argv)
{
	int			opt;
	int			optindex;
	char	   *env;
	char	   *host = "";
	char	   *port = "5432";
	char	   *user = "";
	char	   *database = "";
	char	   *data_file = PGPROTODATA;
	int			debug = 0;
	FILE	   *fd;
	PGconn	   *con;
	int			var;

	static struct option long_options[] = {
		{"host", optional_argument, NULL, 'h'},
		{"port", optional_argument, NULL, 'p'},
		{"username", optional_argument, NULL, 'u'},
		{"database", optional_argument, NULL, 'd'},
		{"proto-data-file", optional_argument, NULL, 'f'},
		{"debug", no_argument, NULL, 'D'},
		{"help", no_argument, NULL, '?'},
		{"version", no_argument, NULL, 'v'},
		{"read-nap", optional_argument, NULL, 'r'},
		{NULL, 0, NULL, 0}
	};

	if ((env = getenv("PGHOST")) != NULL && *env != '\0')
		host = env;
	if ((env = getenv("PGPORT")) != NULL && *env != '\0')
		port = env;
	if ((env = getenv("PGDATABASE")) != NULL && *env != '\0')
		database = env;
	if ((env = getenv("PGUSER")) != NULL && *env != '\0')
		user = env;

	while ((opt = getopt_long(argc, argv, "v?Dh:p:u:d:f:r:", long_options, &optindex)) != -1)
	{
		switch (opt)
		{
			case 'v':
				show_version();
				exit(0);
				break;

			case '?':
				usage();
				exit(0);
				break;

			case 'D':
				debug++;
				break;

			case 'h':
				host = pg_strdup(optarg);
				break;

			case 'p':
				port = pg_strdup(optarg);
				break;

			case 'u':
				user = pg_strdup(optarg);
				break;

			case 'd':
				database = pg_strdup(optarg);
				break;

			case 'f':
				data_file = pg_strdup(optarg);
				break;

			case 'r':
				read_nap = atoi(optarg);
				break;

			default:
				usage();
				exit(1);
		}
	}

	fd = openfile(data_file);
	con = connect_db(host, port, user, database);
	var = fcntl(PQsocket(con), F_GETFL, 0);
	if (var == -1)
	{
		fprintf(stderr, "fcntl failed (%s)\n", strerror(errno));
		exit(1);
	}

	/*
	 * Set the socket to non block.
	 */
	if (fcntl(PQsocket(con), F_SETFL, var & ~O_NONBLOCK) == -1)
	{
		fprintf(stderr, "fcntl failed (%s)\n", strerror(errno));
		exit(1);
	}

	read_and_process(fd, con);

	return 0;
}

static void
show_version(void)
{
	fprintf(stderr, "pgproto (%s) %s\n", PACKAGE, PACKAGE_VERSION);
}

static void
usage(void)
{
	printf("Usage: %s\n"
		   "-h, --hostname=HOSTNAME (default: UNIX domain socket)\n"
		   "-p, --port=PORT (default: 5432)\n"
		   "-u, --user USERNAME (default: OS user)\n"
		   "-d, --database DATABASENAME (default: same as user)\n"
		   "-f, --proto-data-file FILENAME (default: pgproto.data)\n"
		   "-r, --read-nap NAPTIME (in micro seconds. default: 0)\n"
		   "-D, --debug\n"
		   "-?, --help\n"
		   "-v, --version\n",
		   PACKAGE);
}

/*
 * Open protocol data and return the file descriptor.  If failed to open the
 * file, do not return and exit within this function.
 */
static FILE *
openfile(char *filename)
{
	FILE	   *fd = fopen(filename, "r");

	if (fd == NULL)
	{
		fprintf(stderr, "Failed to open protocol data file: %s.\n", filename);
		exit(1);
	}
	return fd;
}

/*
 * Connect to the specified PostgreSQL. If failed, do not return and exit
 * within this function.
 */
static PGconn *
connect_db(char *host, char *port, char *user, char *database)
{
	char		conninfo[1024];
	PGconn	   *conn;
	size_t		n;
	char	*app_name_str = " application_name=pgproto";

	conninfo[0] = '\0';
	n = sizeof(conninfo);

	if (host && host[0] != '\0')
	{
		n -= sizeof("host=");
		strncat(conninfo, "host=", n);
		n -= strlen(host) + 1;
		strncat(conninfo, host, n);
	}

	if (port && port[0] != '\0')
	{
		n -= sizeof("port=");
		strncat(conninfo, " port=", n);
		n -= strlen(port) + 1;
		strncat(conninfo, port, n);
	}

	if (user && user[0] != '\0')
	{
		n -= sizeof("user=");
		strncat(conninfo, " user=", n);
		n -= strlen(user) + 1;
		strncat(conninfo, user, n);
	}

	if (database && database[0] != '\0')
	{
		n -= sizeof("dbname=");
		strncat(conninfo, " dbname=", n);
		n -= strlen(database) + 1;
		strncat(conninfo, database, n);
	}

	n -= strlen(app_name_str);
	strncat(conninfo, app_name_str, n);

	conn = PQconnectdb(conninfo);

	if (conn == NULL || PQstatus(conn) == CONNECTION_BAD)
	{
		fprintf(stderr, "Failed to connect to %s.\n", conninfo);
		exit(1);
	}

	return conn;
}

/*
 * Read the protocol data file and process it.
 */
static void
read_and_process(FILE *fd, PGconn *conn)
{
#define PGPROTO_READBUF_LENGTH 8192
	int			status;
	char	   *buf;
	int			buflen;
	char	   *p;
	int			len;
	int			readp;

	for (;;)
	{
		buflen = PGPROTO_READBUF_LENGTH;
		buf = pg_malloc(buflen);
		readp = 0;

		for (;;)
		{
			p = fgets(buf + readp, buflen - readp, fd);
			if (p == NULL)
			{
				/* EOF detected */
				exit(0);
			}

			/*
			 * if ends with backslash + new line, assume it's a continuous
			 * line
			 */
			len = strlen(p);
			if (p[len - 2] != '\\' || p[len - 1] != '\n')
			{
				break;
			}

			buflen += PGPROTO_READBUF_LENGTH;
			buf = pg_realloc(buf, buflen);
			readp += len;
		}

		status = process_a_line(buf, conn);
		pg_free(buf);

		if (status < 0)
		{
			exit(1);
		}
	}
	PQfinish(conn);
}

/*
 * Process a line of protocol data.
 */
static int
process_a_line(char *buf, PGconn *conn)
{
	char	   *p = buf;
	int			kind;

#ifdef DEBUG
	fprintf(stderr, "buf: %s", buf);
#endif

	if (p == NULL)
	{
		fprintf(stderr, "process_a_line: buf is NULL\n");
		return -1;
	}

	/* An empty line or a line starting with '#' is ignored. */
	if (*p == '\n' || *p == '#')
	{
		return 0;
	}
	else if (*p != '\'')
	{
		fprintf(stderr, "process_a_line: first character is not \' but %x\n", *p);
		return -1;
	}

	p++;

	kind = (unsigned char) (*p++);

	if (*p != '\'')
	{
		fprintf(stderr, "process_a_line: message kind is not followed by \' but %x\n", *p);
		return -1;
	}

	p++;

	process_message_type(kind, p, conn);

	return 0;
}

/*
 * Read a line of message from buf and process it.
 */
static int
process_message_type(int kind, char *buf, PGconn *conn)
{
#ifdef DEBUG
	fprintf(stderr, "message kind is %c\n", kind);
#endif

	char	   *query;
	char	   *err_msg;
	char	   *data;
	char	   *bufp;

	switch (kind)
	{
		case 'Y':
			read_until_ready_for_query(conn, 0, 1);
			break;

		case 'y':
			read_until_ready_for_query(conn, 1, 1);
			break;

		case 'z':
			read_until_ready_for_query(conn, 1, 0);
			break;

		case 'X':
			fprintf(stderr, "FE=> Terminate\n");
			send_char((char) kind, conn);
			send_int(sizeof(int), conn);
			break;

		case 'S':
			fprintf(stderr, "FE=> Sync\n");
			send_char((char) kind, conn);
			send_int(sizeof(int), conn);
			break;

		case 'H':
			fprintf(stderr, "FE=> Flush\n");
			send_char((char) kind, conn);
			send_int(sizeof(int), conn);
			break;

		case 'Q':
			buf++;
			query = buffer_read_string(buf, &bufp);
			fprintf(stderr, "FE=> Query (query=\"%s\")\n", query);
			send_char((char) kind, conn);
			send_int(sizeof(int) + strlen(query) + 1, conn);
			send_string(query, conn);
			pg_free(query);
			break;

		case 'd':
			buf++;
			data = buffer_read_string(buf, &bufp);
			fprintf(stderr, "FE=> CopyData (copy data=\"%s\")\n", data);
			send_char((char) kind, conn);
			send_int(sizeof(int) + strlen(data), conn);
			send_byte(data, strlen(data), conn);
			pg_free(data);
			break;

		case 'c':
			fprintf(stderr, "FE=> CopyDone\n");
			send_char((char) kind, conn);
			send_int(sizeof(int), conn);
			break;

		case 'f':
			buf++;
			err_msg = buffer_read_string(buf, &bufp);
			fprintf(stderr, "FE=> CopyFail (error message=\"%s\")\n", err_msg);
			send_char((char) kind, conn);
			send_int(sizeof(int) + strlen(err_msg) + 1, conn);
			send_string(err_msg, conn);
			pg_free(err_msg);
			break;

		case 'P':
			process_parse(buf, conn);
			break;

		case 'B':
			process_bind(buf, conn);
			break;

		case 'E':
			process_execute(buf, conn);
			break;

		case 'D':
			process_describe(buf, conn);
			break;

		case 'C':
			process_close(buf, conn);
			break;

		case 'F':
			process_function_call(buf, conn);
			break;

		default:
			fprintf(stderr, "Unknown kind: %c", kind);
			break;
	}
	return 0;
}

/*
 * Process function call message
 */
static void
process_function_call(char *buf, PGconn *conn)
{
	int			len;
	int			foid;
	short		nparams;
	short		ncodes;
	short		codes[MAXENTRIES];
	int			paramlens[MAXENTRIES];
	char	   *paramvals[MAXENTRIES];
	short		result_formatcode;
	int			i;
	char	   *bufp;

	SKIP_TABS(buf);

	len = sizeof(int);

	/* function oid */
	foid = buffer_read_int(buf, &bufp);
	buf = bufp;
	len += sizeof(int);

	SKIP_TABS(buf);

	/* number of argument format codes */
	ncodes = buffer_read_int(buf, &bufp);
	len += sizeof(short) + sizeof(short) * ncodes;
	buf = bufp;

	SKIP_TABS(buf);

	if (ncodes > MAXENTRIES)
	{
		fprintf(stderr, "Too many argument format codes for function call message (%d)\n", ncodes);
		exit(1);
	}

	/* read each format code */
	if (ncodes > 0)
	{
		for (i = 0; i < ncodes; i++)
		{
			codes[i] = buffer_read_int(buf, &bufp);
			buf = bufp;
			SKIP_TABS(buf);
		}
	}

	/* number of function arguments */
	nparams = buffer_read_int(buf, &bufp);
	len += sizeof(short);
	buf = bufp;
	SKIP_TABS(buf);

	if (nparams > MAXENTRIES)
	{
		fprintf(stderr, "Too many function arguments for function call message (%d)\n", nparams);
		exit(1);
	}

	/* read each function argument */
	for (i = 0; i < nparams; i++)
	{
		paramlens[i] = buffer_read_int(buf, &bufp);
		len += sizeof(int);
		buf = bufp;
		SKIP_TABS(buf);

		if (paramlens[i] > 0)
		{
			paramvals[i] = buffer_read_string(buf, &bufp);
			buf = bufp;
			SKIP_TABS(buf);
			len += paramlens[i];
		}
	}

	SKIP_TABS(buf);

	/* result format code */
	result_formatcode = buffer_read_int(buf, &bufp);

	if (result_formatcode != 0 && result_formatcode != 1)
	{
		fprintf(stderr, "Result format code is not either 0 or 1 (%d)\n", result_formatcode);
		exit(1);
	}

	buf = bufp;
	len += sizeof(short);
	SKIP_TABS(buf);

	fprintf(stderr, "\n");

	send_char('F', conn);
	send_int(len, conn);		/* message length */
	send_int(foid, conn);		/* function oid */
	send_int16(ncodes, conn);	/* number of argument format code */
	for (i = 0; i < ncodes; i++)	/* argument format codes */
	{
		send_int16(codes[i], conn);
	}

	send_int16(nparams, conn);	/* number of function arguments */
	for (i = 0; i < nparams; i++)	/* function arguments */
	{
		send_int(paramlens[i], conn);	/* argument length */

		/* NULL? */
		if (paramlens[i] != -1)
		{
			/*
			 * Actually we only support text format only for now.  To support
			 * binary format, we need a binary expression in the data file.
			 */
			send_byte(paramvals[i], paramlens[i], conn);
#ifdef NOT_USED
			if (ncodes == 0)	/* text format? */
			{
				send_byte(paramvals[i], paramlens[i], conn);
			}
			else if (ncodes == 1)
			{
				if (codes[0] == 0)
					send_byte(paramvals[i], paramlens[i], conn);
				else
					send_byte(paramvals[i], paramlens[i], conn);
			}
			else
			{
				if (codes[i] == 0)
					send_byte(paramvals[i], paramlens[i], conn);
				else
					send_byte(paramvals[i], paramlens[i], conn);
			}
#endif
		}
	}

	/* result format code */
	send_int16(result_formatcode, conn);
}
