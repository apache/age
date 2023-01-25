/*
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
 * pgpool_logger.h.: master definition header file
 *
 */
/*-------------------------------------------------------------------------
 * From: PostgreSQL
 * src/include/postmaster/syslogger.h
 *	  Exports from postmaster/syslogger.c.
 *
 * Copyright (c) 2004-2020, PostgreSQL Global Development Group
 *
 *-------------------------------------------------------------------------
 */
#ifndef _SYSLOGGER_H
#define _SYSLOGGER_H

#include <limits.h>				/* for PIPE_BUF */


/*
 * Primitive protocol structure for writing to syslogger pipe(s).  The idea
 * here is to divide long messages into chunks that are not more than
 * PIPE_BUF bytes long, which according to POSIX spec must be written into
 * the pipe atomically.  The pipe reader then uses the protocol headers to
 * reassemble the parts of a message into a single string.  The reader can
 * also cope with non-protocol data coming down the pipe, though we cannot
 * guarantee long strings won't get split apart.
 *
 * We use non-nul bytes in is_last to make the protocol a tiny bit
 * more robust against finding a false double nul byte prologue. But
 * we still might find it in the len and/or pid bytes unless we're careful.
 */

#ifdef PIPE_BUF
/* Are there any systems with PIPE_BUF > 64K?  Unlikely, but ... */
#if PIPE_BUF > 65536
#define PIPE_CHUNK_SIZE  65536
#else
#define PIPE_CHUNK_SIZE  ((int) PIPE_BUF)
#endif
#else							/* not defined */
/* POSIX says the value of PIPE_BUF must be at least 512, so use that */
#define PIPE_CHUNK_SIZE  512
#endif

typedef struct
{
	char		nuls[2];		/* always \0\0 */
	uint16		len;			/* size of this chunk (counts data only) */
	int32		pid;			/* writer's pid */
	char		is_last;		/* last chunk of message? 't' or 'f' ('T' or
								 * 'F' for CSV case) */
	char		data[];	/* data payload starts here */
} PipeProtoHeader;

typedef union
{
	PipeProtoHeader proto;
	char		filler[PIPE_CHUNK_SIZE];
} PipeProtoChunk;

#define PIPE_HEADER_SIZE  offsetof(PipeProtoHeader, data)
#define PIPE_MAX_PAYLOAD  ((int) (PIPE_CHUNK_SIZE - PIPE_HEADER_SIZE))


extern int	syslogPipe[2];
extern bool redirection_done;


extern int	SysLogger_Start(void);

extern void write_syslogger_file(const char *buffer, int count, int dest);


extern bool CheckLogrotateSignal(void);
extern void RemoveLogrotateSignalFiles(void);


#endif							/* _SYSLOGGER_H */
