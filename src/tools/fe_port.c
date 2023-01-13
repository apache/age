/*
 * pgpool: a language independent connection pool server for PostgreSQL
 * written by Tatsuo Ishii
 *
 * Copyright (c) 2003-2014	PgPool Global Development Group
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
 * fe_port.c
 *
 */
#ifndef POOL_PRIVATE
#error "This file is not expected to be compiled for pgpool utilities only"
#endif

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <sys/types.h>
#include <unistd.h>
#include <time.h>
#include "utils/fe_ports.h"


int			_fe_error_level = 0;
const char *_filename;
const char *_funcname;
int			_lineno;
int			_print_timestamp = 0;

static const char *error_severity(int elevel);
static char *nowsec(void);

#define MAXSTRFTIME 128

/*
 * ignore errdetail() in frontend
 */
int
errdetail(const char *fmt,...)
{
	return 0;
}

/*
 * ignore errhint() in frontend
 */
int
errhint(const char *fmt,...)
{
	return 0;
}

void
errfinish(int dummy,...)
{
}

void
errmsg(const char *fmt,...)
{
	va_list		ap;
#ifdef HAVE_ASPRINTF
	char	   *fmt2;
	int			len;
#endif

#ifdef HAVE_ASPRINTF
	if (_print_timestamp)
		len = asprintf(&fmt2, "%s %s: pid %d: %s\n", nowsec(), error_severity(_fe_error_level), (int) getpid(), fmt);
	else
		len = asprintf(&fmt2, "%s: pid %d: %s\n", error_severity(_fe_error_level), (int) getpid(), fmt);

	if (len >= 0 && fmt2)
	{
		va_start(ap, fmt);
		vfprintf(stderr, fmt2, ap);
		va_end(ap);
		fflush(stderr);
		free(fmt2);
	}
#else
	if (_print_timestamp)
		fprintf(stderr, "%s %s: pid %d: ", nowsec(), error_severity(_fe_error_level), (int) getpid());
	else
		fprintf(stderr, "%s: pid %d: ", error_severity(_fe_error_level), (int) getpid());

	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
	fprintf(stderr, "\n");
#endif

}

/*
 * error_severity --- get localized string representing elevel
 */
static const char *
error_severity(int elevel)
{
	const char *prefix;

	switch (elevel)
	{
		case DEBUG1:
		case DEBUG2:
		case DEBUG3:
		case DEBUG4:
		case DEBUG5:
			prefix = "DEBUG";
			break;
		case LOG:
		case COMMERROR:
			prefix = "LOG";
			break;
		case INFO:
			prefix = "INFO";
			break;
		case NOTICE:
			prefix = "NOTICE";
			break;
		case WARNING:
			prefix = "WARNING";
			break;
		case ERROR:
			prefix = "ERROR";
			break;
		case FATAL:
			prefix = "FATAL";
			break;
		case PANIC:
			prefix = "PANIC";
			break;
		default:
			prefix = "???";
			break;
	}

	return prefix;
}

static char *
nowsec(void)
{
	static char strbuf[MAXSTRFTIME];
	time_t		now = time(NULL);

	strftime(strbuf, MAXSTRFTIME, "%Y-%m-%d %H:%M:%S", localtime(&now));
	return strbuf;
}

int
errstart(int elevel, const char *filename, int lineno,
		 const char *funcname)
{
	_fe_error_level = elevel;

	/*
	 * This is a basic version and for now we just suppress all messages below
	 * WARNING for frontend
	 */
	if (_fe_error_level < WARNING)
		return 0;
	_filename = filename;
	_lineno = lineno;
	_funcname = funcname;
	return 1;
}
