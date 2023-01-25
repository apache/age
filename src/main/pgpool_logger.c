/* -*-pgpool_logger-c-*- */
/*
 * $Header$
 *
 * pgpool: a language independent connection pool server for PostgreSQL
 * written by Tatsuo Ishii
 *
 * Copyright (c) 2003-2021	PgPool Global Development Group
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
/*-------------------------------------------------------------------------
 *
 * From: PostgreSQL
 *	  src/backend/postmaster/syslogger.c
 * Copyright (c) 2004-2020, PostgreSQL Global Development Group
 *-------------------------------------------------------------------------
 */

#include <fcntl.h>
#include <limits.h>
#include <signal.h>
#include <time.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/time.h>

#include "pool.h"
#include "pool_config.h"
#include "utils/palloc.h"
#include "utils/memutils.h"
#include "utils/elog.h"
#include "parser/pg_list.h"
#include "parser/stringinfo.h"
#include "utils/ps_status.h"
#include "utils/timestamp.h"
#include "utils/pool_signal.h"
#include "main/pgpool_logger.h"

#define DEVNULL "/dev/null"
typedef int64 pg_time_t;
/*
 * We read() into a temp buffer twice as big as a chunk, so that any fragment
 * left after processing can be moved down to the front and we'll still have
 * room to read a full chunk.
 */
#define READ_BUF_SIZE (2 * PIPE_CHUNK_SIZE)

/* Log rotation signal file path, relative to $PGDATA */
#define LOGROTATE_SIGNAL_FILE	"logrotate"


/*
 * GUC parameters.  Logging_collector cannot be changed after postmaster
 * start, but the rest can change at SIGHUP.
 */


bool redirection_done = false;

/*
 * Private state
 */
static pg_time_t next_rotation_time;
static bool pipe_eof_seen = false;
static bool rotation_disabled = false;
static FILE *syslogFile = NULL;
static FILE *csvlogFile = NULL;
static pg_time_t first_syslogger_file_time = 0;
static char *last_file_name = NULL;
static char *last_csv_file_name = NULL;

/*
 * Buffers for saving partial messages from different backends.
 *
 * Keep NBUFFER_LISTS lists of these, with the entry for a given source pid
 * being in the list numbered (pid % NBUFFER_LISTS), so as to cut down on
 * the number of entries we have to examine for any one incoming message.
 * There must never be more than one entry for the same source pid.
 *
 * An inactive buffer is not removed from its list, just held for re-use.
 * An inactive buffer has pid == 0 and undefined contents of data.
 */
typedef struct
{
	int32		pid;			/* PID of source process */
	StringInfoData data;		/* accumulated data, as a StringInfo */
} save_buffer;

#define NBUFFER_LISTS 256
static List *buffer_lists[NBUFFER_LISTS];

int			syslogPipe[2] = {-1, -1};


/*
 * Flags set by interrupt handlers for later service in the main loop.
 */
static volatile sig_atomic_t got_SIGHUP = false;
static volatile sig_atomic_t rotation_requested = false;


static void SysLoggerMain(int argc, char *argv[]) pg_attribute_noreturn();
static void process_pipe_input(char *logbuffer, int *bytes_in_logbuffer);
static void flush_pipe_input(char *logbuffer, int *bytes_in_logbuffer);
static FILE *logfile_open(const char *filename, const char *mode,
						  bool allow_errors);

static void logfile_rotate(bool time_based_rotation, int size_rotation_for);
static char *logfile_getname(pg_time_t timestamp, const char *suffix);
static void set_next_rotation_time(void);
static void sigHupHandler(int sig);
static void sigUsr1Handler(int sig);


/*
 * Main entry point for syslogger process
 * argc/argv parameters are valid only in EXEC_BACKEND case.
 */
static void
SysLoggerMain(int argc, char *argv[])
{
	char		logbuffer[READ_BUF_SIZE];
	int			bytes_in_logbuffer = 0;
	char	   *currentLogDir;
	char	   *currentLogFilename;
	int			currentLogRotationAge;
	pg_time_t	now;

	now = time(NULL);


	init_ps_display("", "", "", "");
	set_ps_display("PgpoolLogger", false);

	/*
	 * If we restarted, our stderr is already redirected into our own input
	 * pipe.  This is of course pretty useless, not to mention that it
	 * interferes with detecting pipe EOF.  Point stderr to /dev/null. This
	 * assumes that all interesting messages generated in the syslogger will
	 * come through elog.c and will be sent to write_syslogger_file.
	 */
	if (redirection_done)
	{
		int			fd = open(DEVNULL, O_WRONLY, 0);

		/*
		 * The closes might look redundant, but they are not: we want to be
		 * darn sure the pipe gets closed even if the open failed.  We can
		 * survive running with stderr pointing nowhere, but we can't afford
		 * to have extra pipe input descriptors hanging around.
		 *
		 * As we're just trying to reset these to go to DEVNULL, there's not
		 * much point in checking for failure from the close/dup2 calls here,
		 * if they fail then presumably the file descriptors are closed and
		 * any writes will go into the bitbucket anyway.
		 */
		close(fileno(stdout));
		close(fileno(stderr));
		if (fd != -1)
		{
			(void) dup2(fd, fileno(stdout));
			(void) dup2(fd, fileno(stderr));
			close(fd);
		}
	}


	/*
	 * Also close our copy of the write end of the pipe.  This is needed to
	 * ensure we can detect pipe EOF correctly.  (But note that in the restart
	 * case, the postmaster already did this.)
	 */
	if (syslogPipe[1] >= 0)
		close(syslogPipe[1]);
	syslogPipe[1] = -1;

	/*
	 * Properly accept or ignore signals the postmaster might send us
	 *
	 * Note: we ignore all termination signals, and instead exit only when all
	 * upstream processes are gone, to ensure we don't miss any dying gasps of
	 * broken backends...
	 */

	pool_signal(SIGHUP, sigHupHandler);

	pool_signal(SIGINT, SIG_IGN);
	pool_signal(SIGTERM, SIG_IGN);
	pool_signal(SIGQUIT, SIG_IGN);
	pool_signal(SIGALRM, SIG_IGN);
	pool_signal(SIGPIPE, SIG_IGN);
	pool_signal(SIGUSR1, sigUsr1Handler);	/* request log rotation */
	pool_signal(SIGUSR2, SIG_IGN);

	/*
	 * Reset some signals that are accepted by postmaster but not here
	 */
	pool_signal(SIGCHLD, SIG_DFL);

	POOL_SETMASK(&UnBlockSig);


	/*
	 * Remember active logfiles' name(s).  We recompute 'em from the reference
	 * time because passing down just the pg_time_t is a lot cheaper than
	 * passing a whole file path in the EXEC_BACKEND case.
	 */
	last_file_name = logfile_getname(first_syslogger_file_time, NULL);
	if (csvlogFile != NULL)
		last_csv_file_name = logfile_getname(first_syslogger_file_time, ".csv");

	/* remember active logfile parameters */
	currentLogDir = pstrdup(pool_config->log_directory);
	currentLogFilename = pstrdup(pool_config->log_filename);
	currentLogRotationAge = pool_config->log_rotation_age;
	/* set next planned rotation time */
	set_next_rotation_time();

	/*
	 * Set up a reusable WaitEventSet object we'll use to wait for our latch,
	 * and (except on Windows) our socket.
	 *
	 * Unlike all other postmaster child processes, we'll ignore postmaster
	 * death because we want to collect final log output from all backends and
	 * then exit last.  We'll do that by running until we see EOF on the
	 * syslog pipe, which implies that all other backends have exited
	 * (including the postmaster).
	 */

	/* main worker loop */
	for (;;)
	{
		bool		time_based_rotation = false;
		int			size_rotation_for = 0;
		struct timeval timeout;
		fd_set		rfds;
		int			rc;

		/*
		 * Process any requests or signals received recently.
		 */
		if (got_SIGHUP)
		{
			got_SIGHUP = false;
			MemoryContext oldContext = MemoryContextSwitchTo(TopMemoryContext);

			pool_get_config(get_config_file_name(), CFGCXT_RELOAD);
			MemoryContextSwitchTo(oldContext);

			/*
			 * Check if the log directory or filename pattern changed in
			 * pgpool.conf. If so, force rotation to make sure we're
			 * writing the logfiles in the right place.
			 */
			if (strcmp(pool_config->log_directory, currentLogDir) != 0)
			{
				pfree(currentLogDir);
				currentLogDir = pstrdup(pool_config->log_directory);
				rotation_requested = true;

				/*
				 * Also, create new directory if not present; ignore errors
				 */
				if (mkdir(pool_config->log_directory, S_IREAD | S_IWRITE | S_IEXEC) == -1)
				{
					ereport(LOG,
							(errmsg("pgpool logger, failed to create directory:\"%s\". error:\"%m\"", pool_config->log_directory)));
				}
			}
			if (strcmp(pool_config->log_filename, currentLogFilename) != 0)
			{
				pfree(currentLogFilename);
				currentLogFilename = pstrdup(pool_config->log_filename);
				rotation_requested = true;
			}

			/*
			 * Force a rotation if CSVLOG output was just turned on or off and
			 * we need to open or close csvlogFile accordingly.
			 */
			if (((pool_config->log_destination & LOG_DESTINATION_CSVLOG) != 0) !=
				(csvlogFile != NULL))
				rotation_requested = true;

			/*
			 * If rotation time parameter changed, reset next rotation time,
			 * but don't immediately force a rotation.
			 */
			if (currentLogRotationAge != pool_config->log_rotation_age)
			{
				currentLogRotationAge = pool_config->log_rotation_age;
				set_next_rotation_time();
			}

			/*
			 * If we had a rotation-disabling failure, re-enable rotation
			 * attempts after SIGHUP, and force one immediately.
			 */
			if (rotation_disabled)
			{
				rotation_disabled = false;
				rotation_requested = true;
			}

		}

		if (pool_config->log_rotation_age > 0 && !rotation_disabled)
		{
			/* Do a logfile rotation if it's time */
			now = (pg_time_t) time(NULL);
			if (now >= next_rotation_time)
				rotation_requested = time_based_rotation = true;
		}

		if (!rotation_requested && pool_config->log_rotation_size > 0 && !rotation_disabled)
		{
			/* Do a rotation if file is too big */
			if (ftell(syslogFile) >= pool_config->log_rotation_size * 1024L)
			{
				rotation_requested = true;
				size_rotation_for |= LOG_DESTINATION_STDERR;
			}
			if (csvlogFile != NULL &&
				ftell(csvlogFile) >= pool_config->log_rotation_size * 1024L)
			{
				rotation_requested = true;
				size_rotation_for |= LOG_DESTINATION_CSVLOG;
			}
		}

		if (rotation_requested)
		{
			/*
			 * Force rotation when both values are zero. It means the request
			 * was sent by pg_rotate_logfile() or "pg_ctl logrotate".
			 */
			if (!time_based_rotation && size_rotation_for == 0)
				size_rotation_for = LOG_DESTINATION_STDERR | LOG_DESTINATION_CSVLOG;
			logfile_rotate(time_based_rotation, size_rotation_for);
		}

		/*
		 * Calculate time till next time-based rotation, so that we don't
		 * sleep longer than that.  We assume the value of "now" obtained
		 * above is still close enough.  Note we can't make this calculation
		 * until after calling logfile_rotate(), since it will advance
		 * next_rotation_time.
		 *
		 * Also note that we need to beware of overflow in calculation of the
		 * timeout: with large settings of pool_config->log_rotation_age, next_rotation_time
		 * could be more than INT_MAX msec in the future.  In that case we'll
		 * wait no more than INT_MAX msec, and try again.
		 */
		timeout.tv_sec = 0;
		/* Reset usec everytime before calling sellect */
		timeout.tv_usec = 0;

		if (pool_config->log_rotation_age > 0 && !rotation_disabled)
		{
			pg_time_t	delay;

			delay = next_rotation_time - now;
			if (delay > 0)
			{
				if (delay > INT_MAX / 1000)
					delay = INT_MAX / 1000;
				timeout.tv_sec = delay;
			}
		}

		/*
		 * Sleep until there's something to do
		 */
		
		FD_ZERO(&rfds);
		FD_SET(syslogPipe[0], &rfds);
		rc = select(syslogPipe[0] + 1, &rfds, NULL, NULL, timeout.tv_sec?&timeout:NULL);
		if (rc == 1)
		{
			int			bytesRead;

			bytesRead = read(syslogPipe[0],
							 logbuffer + bytes_in_logbuffer,
							 sizeof(logbuffer) - bytes_in_logbuffer);
			if (bytesRead < 0)
			{
				if (errno != EINTR)
					ereport(LOG,
							(errmsg("could not read from logger pipe: %m")));
			}
			else if (bytesRead > 0)
			{
				bytes_in_logbuffer += bytesRead;
				process_pipe_input(logbuffer, &bytes_in_logbuffer);
				continue;
			}
			else
			{
				/*
				 * Zero bytes read when select() is saying read-ready means
				 * EOF on the pipe: that is, there are no longer any processes
				 * with the pipe write end open.  Therefore, the postmaster
				 * and all backends are shut down, and we are done.
				 */
				pipe_eof_seen = true;

				/* if there's any data left then force it out now */
				flush_pipe_input(logbuffer, &bytes_in_logbuffer);
			}
		}

		if (pipe_eof_seen)
		{
			/*
			 * seeing this message on the real stderr is annoying - so we make
			 * it DEBUG1 to suppress in normal use.
			 */
			ereport(DEBUG1,
					(errmsg("logger shutting down")));

			/*
			 * Normal exit from the syslogger is here.  Note that we
			 * deliberately do not close syslogFile before exiting; this is to
			 * allow for the possibility of elog messages being generated
			 * inside proc_exit.  Regular exit() will take care of flushing
			 * and closing stdio channels.
			 */
			proc_exit(0);
		}
	}
}

/*
 * Postmaster subroutine to start a syslogger subprocess.
 */
int
SysLogger_Start(void)
{
	pid_t		sysloggerPid;
	char	   *filename;

	if (!pool_config->logging_collector)
		return 0;

	/*
	 * If first time through, create the pipe which will receive stderr
	 * output.
	 *
	 * If the syslogger crashes and needs to be restarted, we continue to use
	 * the same pipe (indeed must do so, since extant backends will be writing
	 * into that pipe).
	 *
	 * This means the postmaster must continue to hold the read end of the
	 * pipe open, so we can pass it down to the reincarnated syslogger. This
	 * is a bit klugy but we have little choice.
	 *
	 * Also note that we don't bother counting the pipe FDs by calling
	 * Reserve/ReleaseExternalFD.  There's no real need to account for them
	 * accurately in the postmaster or syslogger process, and both ends of the
	 * pipe will wind up closed in all other postmaster children.
	 */
	if (syslogPipe[0] < 0)
	{
		if (pipe(syslogPipe) < 0)
			ereport(FATAL,
					(errmsg("could not create pipe for syslog: %m")));
	}

	/*
	 * Create log directory if not present; ignore errors
	 */
	mkdir(pool_config->log_directory, S_IREAD | S_IWRITE | S_IEXEC);


	/*
	 * The initial logfile is created right in the postmaster, to verify that
	 * the pool_config->log_directory is writable.  We save the reference time so that the
	 * syslogger child process can recompute this file name.
	 *
	 * It might look a bit strange to re-do this during a syslogger restart,
	 * but we must do so since the postmaster closed syslogFile after the
	 * previous fork (and remembering that old file wouldn't be right anyway).
	 * Note we always append here, we won't overwrite any existing file.  This
	 * is consistent with the normal rules, because by definition this is not
	 * a time-based rotation.
	 */
	first_syslogger_file_time = time(NULL);

	filename = logfile_getname(first_syslogger_file_time, NULL);

	syslogFile = logfile_open(filename, "a", false);

	pfree(filename);

	/*
	 * Likewise for the initial CSV log file, if that's enabled.  (Note that
	 * we open syslogFile even when only CSV output is nominally enabled,
	 * since some code paths will write to syslogFile anyway.)
	 */
	if (pool_config->log_destination & LOG_DESTINATION_CSVLOG)
	{
		filename = logfile_getname(first_syslogger_file_time, ".csv");

		csvlogFile = logfile_open(filename, "a", false);

		pfree(filename);
	}

	switch ((sysloggerPid = fork()))
	{
		case -1:
			ereport(LOG,
					(errmsg("could not fork system logger: %m")));
			return 0;

		case 0:
			on_exit_reset();
			SetProcessGlobalVariables(PT_LOGGER);
			/* do the work */
			SysLoggerMain(0, NULL);
			break;

		default:
			/* success, in postmaster */

			/* now we redirect stderr, if not done already */
			if (!redirection_done)
			{

				/*
				 * Leave a breadcrumb trail when redirecting, in case the user
				 * forgets that redirection is active and looks only at the
				 * original stderr target file.
				 */
				ereport(LOG,
						(errmsg("redirecting log output to logging collector process"),
						 errhint("Future log output will appear in directory \"%s\".",
								 pool_config->log_directory)));

#ifndef WIN32
				fflush(stdout);
				if (dup2(syslogPipe[1], fileno(stdout)) < 0)
					ereport(FATAL,
							(errmsg("could not redirect stdout: %m")));
				fflush(stderr);
				if (dup2(syslogPipe[1], fileno(stderr)) < 0)
					ereport(FATAL,
							(errmsg("could not redirect stderr: %m")));
				/* Now we are done with the write end of the pipe. */
				close(syslogPipe[1]);
				syslogPipe[1] = -1;
#else

				/*
				 * open the pipe in binary mode and make sure stderr is binary
				 * after it's been dup'ed into, to avoid disturbing the pipe
				 * chunking protocol.
				 */
				fflush(stderr);
				fd = _open_osfhandle((intptr_t) syslogPipe[1],
									 _O_APPEND | _O_BINARY);
				if (dup2(fd, _fileno(stderr)) < 0)
					ereport(FATAL,
							(errmsg("could not redirect stderr: %m")));
				close(fd);
				_setmode(_fileno(stderr), _O_BINARY);

				/*
				 * Now we are done with the write end of the pipe.
				 * CloseHandle() must not be called because the preceding
				 * close() closes the underlying handle.
				 */
				syslogPipe[1] = 0;
#endif
				redirection_done = true;
			}

			/* postmaster will never write the file(s); close 'em */
			fclose(syslogFile);
			syslogFile = NULL;
			if (csvlogFile != NULL)
			{
				fclose(csvlogFile);
				csvlogFile = NULL;
			}
			return (int) sysloggerPid;
	}

	/* we should never reach here */
	return 0;
}


/* --------------------------------
 *		pipe protocol handling
 * --------------------------------
 */

/*
 * Process data received through the syslogger pipe.
 *
 * This routine interprets the log pipe protocol which sends log messages as
 * (hopefully atomic) chunks - such chunks are detected and reassembled here.
 *
 * The protocol has a header that starts with two nul bytes, then has a 16 bit
 * length, the pid of the sending process, and a flag to indicate if it is
 * the last chunk in a message. Incomplete chunks are saved until we read some
 * more, and non-final chunks are accumulated until we get the final chunk.
 *
 * All of this is to avoid 2 problems:
 * . partial messages being written to logfiles (messes rotation), and
 * . messages from different backends being interleaved (messages garbled).
 *
 * Any non-protocol messages are written out directly. These should only come
 * from non-Pgpool sources, however (e.g. third party libraries writing to
 * stderr).
 *
 * logbuffer is the data input buffer, and *bytes_in_logbuffer is the number
 * of bytes present.  On exit, any not-yet-eaten data is left-justified in
 * logbuffer, and *bytes_in_logbuffer is updated.
 */
static void
process_pipe_input(char *logbuffer, int *bytes_in_logbuffer)
{
	char	   *cursor = logbuffer;
	int			count = *bytes_in_logbuffer;
	int			dest = LOG_DESTINATION_STDERR;

	/* While we have enough for a header, process data... */
	while (count >= (int) (offsetof(PipeProtoHeader, data) + 1))
	{
		PipeProtoHeader p;
		int			chunklen;

		/* Do we have a valid header? */
		memcpy(&p, cursor, offsetof(PipeProtoHeader, data));
		if (p.nuls[0] == '\0' && p.nuls[1] == '\0' &&
			p.len > 0 && p.len <= PIPE_MAX_PAYLOAD &&
			p.pid != 0 &&
			(p.is_last == 't' || p.is_last == 'f' ||
			 p.is_last == 'T' || p.is_last == 'F'))
		{
			List	   *buffer_list;
			ListCell   *cell;
			save_buffer *existing_slot = NULL,
					   *free_slot = NULL;
			StringInfo	str;

			chunklen = PIPE_HEADER_SIZE + p.len;

			/* Fall out of loop if we don't have the whole chunk yet */
			if (count < chunklen)
				break;

			dest = (p.is_last == 'T' || p.is_last == 'F') ?
				LOG_DESTINATION_CSVLOG : LOG_DESTINATION_STDERR;

			/* Locate any existing buffer for this source pid */
			buffer_list = buffer_lists[p.pid % NBUFFER_LISTS];
			foreach(cell, buffer_list)
			{
				save_buffer *buf = (save_buffer *) lfirst(cell);

				if (buf->pid == p.pid)
				{
					existing_slot = buf;
					break;
				}
				if (buf->pid == 0 && free_slot == NULL)
					free_slot = buf;
			}

			if (p.is_last == 'f' || p.is_last == 'F')
			{
				/*
				 * Save a complete non-final chunk in a per-pid buffer
				 */
				if (existing_slot != NULL)
				{
					/* Add chunk to data from preceding chunks */
					str = &(existing_slot->data);
					appendBinaryStringInfo(str,
										   cursor + PIPE_HEADER_SIZE,
										   p.len);
				}
				else
				{
					/* First chunk of message, save in a new buffer */
					if (free_slot == NULL)
					{
						/*
						 * Need a free slot, but there isn't one in the list,
						 * so create a new one and extend the list with it.
						 */
						free_slot = palloc(sizeof(save_buffer));
						buffer_list = lappend(buffer_list, free_slot);
						buffer_lists[p.pid % NBUFFER_LISTS] = buffer_list;
					}
					free_slot->pid = p.pid;
					str = &(free_slot->data);
					initStringInfo(str);
					appendBinaryStringInfo(str,
										   cursor + PIPE_HEADER_SIZE,
										   p.len);
				}
			}
			else
			{
				/*
				 * Final chunk --- add it to anything saved for that pid, and
				 * either way write the whole thing out.
				 */
				if (existing_slot != NULL)
				{
					str = &(existing_slot->data);
					appendBinaryStringInfo(str,
										   cursor + PIPE_HEADER_SIZE,
										   p.len);
					write_syslogger_file(str->data, str->len, dest);
					/* Mark the buffer unused, and reclaim string storage */
					existing_slot->pid = 0;
					pfree(str->data);
				}
				else
				{
					/* The whole message was one chunk, evidently. */
					write_syslogger_file(cursor + PIPE_HEADER_SIZE, p.len,
										 dest);
				}
			}

			/* Finished processing this chunk */
			cursor += chunklen;
			count -= chunklen;
		}
		else
		{
			/* Process non-protocol data */

			/*
			 * Look for the start of a protocol header.  If found, dump data
			 * up to there and repeat the loop.  Otherwise, dump it all and
			 * fall out of the loop.  (Note: we want to dump it all if at all
			 * possible, so as to avoid dividing non-protocol messages across
			 * logfiles.  We expect that in many scenarios, a non-protocol
			 * message will arrive all in one read(), and we want to respect
			 * the read() boundary if possible.)
			 */
			for (chunklen = 1; chunklen < count; chunklen++)
			{
				if (cursor[chunklen] == '\0')
					break;
			}
			/* fall back on the stderr log as the destination */
			write_syslogger_file(cursor, chunklen, LOG_DESTINATION_STDERR);
			cursor += chunklen;
			count -= chunklen;
		}
	}

	/* We don't have a full chunk, so left-align what remains in the buffer */
	if (count > 0 && cursor != logbuffer)
		memmove(logbuffer, cursor, count);
	*bytes_in_logbuffer = count;
}

/*
 * Force out any buffered data
 *
 * This is currently used only at syslogger shutdown, but could perhaps be
 * useful at other times, so it is careful to leave things in a clean state.
 */
static void
flush_pipe_input(char *logbuffer, int *bytes_in_logbuffer)
{
	int			i;

	/* Dump any incomplete protocol messages */
	for (i = 0; i < NBUFFER_LISTS; i++)
	{
		List	   *list = buffer_lists[i];
		ListCell   *cell;

		foreach(cell, list)
		{
			save_buffer *buf = (save_buffer *) lfirst(cell);

			if (buf->pid != 0)
			{
				StringInfo	str = &(buf->data);

				write_syslogger_file(str->data, str->len,
									 LOG_DESTINATION_STDERR);
				/* Mark the buffer unused, and reclaim string storage */
				buf->pid = 0;
				pfree(str->data);
			}
		}
	}

	/*
	 * Force out any remaining pipe data as-is; we don't bother trying to
	 * remove any protocol headers that may exist in it.
	 */
	if (*bytes_in_logbuffer > 0)
		write_syslogger_file(logbuffer, *bytes_in_logbuffer,
							 LOG_DESTINATION_STDERR);
	*bytes_in_logbuffer = 0;
}


/* --------------------------------
 *		logfile routines
 * --------------------------------
 */

/*
 * Write text to the currently open logfile
 *
 * This is exported so that elog.c can call it when processType is PT_LOGGER;
.
 * This allows the syslogger process to record elog messages of its own,
 * even though its stderr does not point at the syslog pipe.
 */
void
write_syslogger_file(const char *buffer, int count, int destination)
{
	int			rc;
	FILE	   *logfile;

	/*
	 * If we're told to write to csvlogFile, but it's not open, dump the data
	 * to syslogFile (which is always open) instead.  This can happen if CSV
	 * output is enabled after postmaster start and we've been unable to open
	 * csvlogFile.  There are also race conditions during a parameter change
	 * whereby backends might send us CSV output before we open csvlogFile or
	 * after we close it.  Writing CSV-formatted output to the regular log
	 * file isn't great, but it beats dropping log output on the floor.
	 *
	 * Think not to improve this by trying to open csvlogFile on-the-fly.  Any
	 * failure in that would lead to recursion.
	 */
	logfile = (destination == LOG_DESTINATION_CSVLOG &&
			   csvlogFile != NULL) ? csvlogFile : syslogFile;

	rc = fwrite(buffer, 1, count, logfile);

	/*
	 * Try to report any failure.  We mustn't use ereport because it would
	 * just recurse right back here, but write_stderr is OK: it will write
	 * either to the postmaster's original stderr, or to /dev/null, but never
	 * to our input pipe which would result in a different sort of looping.
	 */
	if (rc != count)
		write_stderr("could not write to log file: %s\n", strerror(errno));
}


/*
 * Open a new logfile with proper permissions and buffering options.
 *
 * If allow_errors is true, we just log any open failure and return NULL
 * (with errno still correct for the fopen failure).
 * Otherwise, errors are treated as fatal.
 */
static FILE *
logfile_open(const char *filename, const char *mode, bool allow_errors)
{
	FILE	   *fh;
	mode_t		oumask;

	/*
	 * Note we do not let pool_config->log_file_mode disable IWUSR, since we certainly want
	 * to be able to write the files ourselves.
	 */
	oumask = umask((mode_t) ((~(pool_config->log_file_mode | S_IWUSR)) & (S_IRWXU | S_IRWXG | S_IRWXO)));
	fh = fopen(filename, mode);
	umask(oumask);

	if (fh)
	{
		setvbuf(fh, NULL, _IOLBF, 0);

#ifdef WIN32
		/* use CRLF line endings on Windows */
		_setmode(_fileno(fh), _O_TEXT);
#endif
	}
	else
	{
		int			save_errno = errno;

		ereport(allow_errors ? LOG : FATAL,
				(errmsg("could not open log file \"%s\": %m",
						filename)));
		errno = save_errno;
	}

	return fh;
}

/*
 * perform logfile rotation
 */
static void
logfile_rotate(bool time_based_rotation, int size_rotation_for)
{
	char	   *filename;
	char	   *csvfilename = NULL;
	pg_time_t	fntime;
	FILE	   *fh;

	rotation_requested = false;

	/*
	 * When doing a time-based rotation, invent the new logfile name based on
	 * the planned rotation time, not current time, to avoid "slippage" in the
	 * file name when we don't do the rotation immediately.
	 */
	if (time_based_rotation)
		fntime = next_rotation_time;
	else
		fntime = time(NULL);
	filename = logfile_getname(fntime, NULL);
	if (pool_config->log_destination & LOG_DESTINATION_CSVLOG)
		csvfilename = logfile_getname(fntime, ".csv");

	/*
	 * Decide whether to overwrite or append.  We can overwrite if (a)
	 * pool_config->log_truncate_on_rotation is set, (b) the rotation was triggered by
	 * elapsed time and not something else, and (c) the computed file name is
	 * different from what we were previously logging into.
	 *
	 * Note: last_file_name should never be NULL here, but if it is, append.
	 */
	if (time_based_rotation || (size_rotation_for & LOG_DESTINATION_STDERR))
	{
		if (pool_config->log_truncate_on_rotation && time_based_rotation &&
			last_file_name != NULL &&
			strcmp(filename, last_file_name) != 0)
			fh = logfile_open(filename, "w", true);
		else
			fh = logfile_open(filename, "a", true);

		if (!fh)
		{
			/*
			 * ENFILE/EMFILE are not too surprising on a busy system; just
			 * keep using the old file till we manage to get a new one.
			 * Otherwise, assume something's wrong with pool_config->log_directory and stop
			 * trying to create files.
			 */
			if (errno != ENFILE && errno != EMFILE)
			{
				ereport(LOG,
						(errmsg("disabling automatic rotation (use SIGHUP to re-enable)")));
				rotation_disabled = true;
			}

			if (filename)
				pfree(filename);
			if (csvfilename)
				pfree(csvfilename);
			return;
		}

		fclose(syslogFile);
		syslogFile = fh;

		/* instead of pfree'ing filename, remember it for next time */
		if (last_file_name != NULL)
			pfree(last_file_name);
		last_file_name = filename;
		filename = NULL;
	}

	/*
	 * Same as above, but for csv file.  Note that if LOG_DESTINATION_CSVLOG
	 * was just turned on, we might have to open csvlogFile here though it was
	 * not open before.  In such a case we'll append not overwrite (since
	 * last_csv_file_name will be NULL); that is consistent with the normal
	 * rules since it's not a time-based rotation.
	 */
	if ((pool_config->log_destination & LOG_DESTINATION_CSVLOG) &&
		(csvlogFile == NULL ||
		 time_based_rotation || (size_rotation_for & LOG_DESTINATION_CSVLOG)))
	{
		if (pool_config->log_truncate_on_rotation && time_based_rotation &&
			last_csv_file_name != NULL &&
			strcmp(csvfilename, last_csv_file_name) != 0)
			fh = logfile_open(csvfilename, "w", true);
		else
			fh = logfile_open(csvfilename, "a", true);

		if (!fh)
		{
			/*
			 * ENFILE/EMFILE are not too surprising on a busy system; just
			 * keep using the old file till we manage to get a new one.
			 * Otherwise, assume something's wrong with pool_config->log_directory and stop
			 * trying to create files.
			 */
			if (errno != ENFILE && errno != EMFILE)
			{
				ereport(LOG,
						(errmsg("disabling automatic rotation (use SIGHUP to re-enable)")));
				rotation_disabled = true;
			}

			if (filename)
				pfree(filename);
			if (csvfilename)
				pfree(csvfilename);
			return;
		}

		if (csvlogFile != NULL)
			fclose(csvlogFile);
		csvlogFile = fh;

		/* instead of pfree'ing filename, remember it for next time */
		if (last_csv_file_name != NULL)
			pfree(last_csv_file_name);
		last_csv_file_name = csvfilename;
		csvfilename = NULL;
	}
	else if (!(pool_config->log_destination & LOG_DESTINATION_CSVLOG) &&
			 csvlogFile != NULL)
	{
		/* CSVLOG was just turned off, so close the old file */
		fclose(csvlogFile);
		csvlogFile = NULL;
		if (last_csv_file_name != NULL)
			pfree(last_csv_file_name);
		last_csv_file_name = NULL;
	}

	if (filename)
		pfree(filename);
	if (csvfilename)
		pfree(csvfilename);

	set_next_rotation_time();
}


/*
 * construct logfile name using timestamp information
 *
 * If suffix isn't NULL, append it to the name, replacing any ".log"
 * that may be in the pattern.
 *
 * Result is palloc'd.
 */
static char *
logfile_getname(pg_time_t timestamp, const char *suffix)
{
	char	   *filename;
	int			len;

	filename = palloc(MAXPGPATH);

	snprintf(filename, MAXPGPATH, "%s/", pool_config->log_directory);

	len = strlen(filename);

	/* treat pool_config->log_filename as a strftime pattern */
//	strftime(strbuf, 128, "%Y-%m-%d %H:%M:%S", localtime(&now));
	strftime(filename + len, MAXPGPATH - len, pool_config->log_filename,
				localtime(&timestamp));

	if (suffix != NULL)
	{
		len = strlen(filename);
		if (len > 4 && (strcmp(filename + (len - 4), ".log") == 0))
			len -= 4;
		strlcpy(filename + len, suffix, MAXPGPATH - len);
	}

	return filename;
}

/*
 * Determine the next planned rotation time, and store in next_rotation_time.
 */
static void
set_next_rotation_time(void)
{
	pg_time_t	now;
	struct tm *tm;
	int			rotinterval;

	/* nothing to do if time-based rotation is disabled */
	if (pool_config->log_rotation_age <= 0)
		return;

	/*
	 * The requirements here are to choose the next time > now that is a
	 * "multiple" of the log rotation interval.  "Multiple" can be interpreted
	 * fairly loosely.  In this version we align to log_timezone rather than
	 * GMT.
	 */
	rotinterval = pool_config->log_rotation_age * SECS_PER_MINUTE;	/* convert to seconds */
	now = (pg_time_t) time(NULL);
	tm = localtime(&now);
	now += tm->tm_gmtoff;
	now -= now % rotinterval;
	now += rotinterval;
	now -= tm->tm_gmtoff;
	next_rotation_time = now;
}


/* --------------------------------
 *		signal handler routines
 * --------------------------------
 */

/*
 * Check to see if a log rotation request has arrived.  Should be
 * called by postmaster after receiving SIGUSR1.
 */
bool
CheckLogrotateSignal(void)
{
	struct stat stat_buf;

	if (stat(LOGROTATE_SIGNAL_FILE, &stat_buf) == 0)
		return true;

	return false;
}

/*
 * Remove the file signaling a log rotation request.
 */
void
RemoveLogrotateSignalFiles(void)
{
	unlink(LOGROTATE_SIGNAL_FILE);
}

/* SIGHUP: set flag to reload config file */
static void
sigHupHandler(int sig)
{
	int			save_errno = errno;

	got_SIGHUP = true;
//	SetLatch(MyLatch);

	errno = save_errno;
}

/* SIGUSR1: set flag to rotate logfile */
static void
sigUsr1Handler(int sig)
{
	int			save_errno = errno;

	rotation_requested = true;
//	SetLatch(MyLatch);

	errno = save_errno;
}
