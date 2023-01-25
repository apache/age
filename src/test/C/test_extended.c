/* -*-pgsql-c-*- */
/*
 * $Header$
 *
 * pgpool: a language independent connection pool server for PostgreSQL
 * written by Tatsuo Ishii
 *
 * Copyright (c) 2003-2010	PgPool Global Development Group
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
 * Extended protocol test program.
 * Features are:
 * libq trace on/off
 * explicit transaction on/off
 * multiple SQL statements
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include "libpq-fe.h"
#include "libpq/libpq-fs.h"

int
main(int argc, char **argv)
{
	char	   *connect_string = "user=t-ishii dbname=test port=5432";
	int			doTrace = 1;	/* set 1 to start libpq trace */
	int			doTransaction = 1;	/* set 1 to start explicit transaction */
	int			doSleep = 0;	/* set non 0 seconds to sleep after connection
								 * and before starting command */

	/* SQL commands to be executed */
	static char *commands[] = {
		"SAVEPOINT S1",
		"UPDATE t1 SET k = 1",
		"ROLLBACK TO S1",
		"SELECT 1",
		"RELEASE SAVEPOINT S1"
	};

	PGconn	   *conn;
	PGresult   *res;
	int			i;

	conn = PQconnectdb(connect_string);
	if (PQstatus(conn) == CONNECTION_BAD)
	{
		printf("Unable to connect to db\n");
		PQfinish(conn);
		return 1;
	}

	if (doSleep)
		sleep(doSleep);

	if (doTrace == 1)
		PQtrace(conn, stdout);

	if (doTransaction)
		PQexec(conn, "BEGIN;");

	for (i = 0; i < sizeof(commands) / sizeof(char *); i++)
	{
		char	   *command = commands[i];

		res = PQexecParams(conn, command, 0, NULL, NULL, NULL, NULL, 0);
		switch (PQresultStatus(res))
		{
			case PGRES_COMMAND_OK:
			case PGRES_TUPLES_OK:
				fprintf(stderr, "\"%s\" : succeeded\n", command);
				break;
			default:
				fprintf(stderr, "\"%s\" failed: %s\n", command, PQresultErrorMessage(res));
				break;
		}
		PQclear(res);

	}

	if (doTransaction == 1)
	{
		PQexec(conn, "COMMIT;");
	}

	PQfinish(conn);
	return 0;
}
