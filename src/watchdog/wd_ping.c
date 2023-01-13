/*
 * $Header$
 *
 * Handles watchdog connection, and protocol communication with pgpool-II
 *
 * pgpool: a language independent connection pool server for PostgreSQL
 * written by Tatsuo Ishii
 *
 * Copyright (c) 2003-2022	PgPool Global Development Group
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

#include <pthread.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <netdb.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include "pool.h"
#include "utils/elog.h"
#include "pool_config.h"
#include "watchdog/wd_utils.h"
#include "utils/pool_signal.h"

#define WD_MAX_PING_RESULT 256

static double get_result(char *ping_data);

/**
 * check if IP address can be pinged.
 */
bool
wd_is_ip_exists(char *ip)
{
	pid_t		pid;
	int			outputfd;
	int			status;

	if (ip == NULL)
		return false;

	POOL_SETMASK(&AuthBlockSig);

	pid = wd_issue_ping_command(ip, &outputfd);
	if (pid <= 0)
	{
		POOL_SETMASK(&UnBlockSig);
		return false;
	}

	for (;;)
	{
		pid_t		sts = waitpid(pid, &status, 0);

		if (sts == pid)
		{
			bool		ret = wd_get_ping_result(ip, status, outputfd);

			POOL_SETMASK(&UnBlockSig);
			close(outputfd);
			return ret;
		}
		if (errno == EINTR)
			continue;

		POOL_SETMASK(&UnBlockSig);
		close(outputfd);
		ereport(WARNING,
				(errmsg("watchdog failed to ping host\"%s\"", ip),
				 errdetail("waitpid() failed with reason \"%m\"")));
		return false;
	}

	POOL_SETMASK(&UnBlockSig);
	return false;
}

/*
 * function issues the ping command using the execv system call
 * and return the pid of the process.
 */
pid_t
wd_issue_ping_command(char *hostname, int *outfd)
{
	int			status;
	char	   *args[8];
	int			pid,
				i = 0;
	int			pfd[2];
	char		ping_path[WD_MAX_PATH_LEN];

	snprintf(ping_path, sizeof(ping_path), "%s/ping", pool_config->ping_path);

	ereport(DEBUG2,
			(errmsg("watchdog trying to ping host \"%s\"", hostname)));

	if (pipe(pfd) == -1)
	{
		ereport(WARNING,
				(errmsg("watchdog failed to ping host\"%s\"", hostname),
				 errdetail("pipe open failed. reason: %m")));
		return -1;
	}

	args[i++] = "ping";
	args[i++] = "-q";
	args[i++] = "-c3";
	args[i++] = hostname;
	args[i++] = NULL;

	pid = fork();
	if (pid == -1)
	{
		ereport(WARNING,
				(errmsg("watchdog failed to ping host\"%s\"", hostname),
				 errdetail("fork() failed. reason: %m")));
		return -1;
	}
	if (pid == 0)
	{
		/* CHILD */
		on_exit_reset();
		SetProcessGlobalVariables(PT_WATCHDOG_UTILITY);

		close(STDOUT_FILENO);
		dup2(pfd[1], STDOUT_FILENO);
		close(pfd[0]);
		status = execv(ping_path, args);

		if (status == -1)
		{
			ereport(FATAL,
					(errmsg("watchdog failed to ping host\"%s\"", hostname),
					 errdetail("execv(%s) failed. reason: %m", ping_path)));
		}
		exit(0);
	}
	close(pfd[1]);
	*outfd = pfd[0];
	return pid;
}

/*
 * execute specified command for trusted servers and return the
 * pid of the process.
 */
pid_t
wd_trusted_server_command(char *hostname)
{
	int			status;
	int			pid;
	StringInfoData		exec_cmd_data;
	StringInfo				exec_cmd = &exec_cmd_data;
	char *command_line = pstrdup(pool_config->trusted_server_command);

	initStringInfo(exec_cmd);

	while (*command_line)
	{
		if (*command_line == '%')
		{
			if (*(command_line + 1))
			{
				char		val = *(command_line + 1);

				switch (val)
				{
					case 'h':	/* trusted server host name */
						appendStringInfoString(exec_cmd, hostname);
						break;

					case '%':	/* escape */
						appendStringInfoString(exec_cmd, "%");
						break;

					default:	/* ignore */
						break;
				}
				command_line++;
			}
		}
		else
			appendStringInfoChar(exec_cmd, *command_line);

		command_line++;
	}

	pid = fork();
	if (pid == -1)
	{
		ereport(WARNING,
				(errmsg("watchdog failed to ping host\"%s\"", hostname),
				 errdetail("fork() failed. reason: %m")));
		return -1;
	}
	if (pid == 0)
	{
		/* CHILD */
		int	fd;

		on_exit_reset();
		SetProcessGlobalVariables(PT_WATCHDOG_UTILITY);
		if (strlen(exec_cmd->data) != 0)
			elog(DEBUG1, "trusted_server_command: %s", exec_cmd->data);

		fd = open("/dev/null", O_RDWR);
		if (fd < 0)
		{
			ereport(ERROR,
					(errmsg("failed to open \"/dev/null\""),
					 errdetail("%m")));
		}
		dup2(fd, 0);
		dup2(fd, 1);
		dup2(fd, 2);
		close(fd);

		if (strlen(exec_cmd->data) != 0)
		{
			status = system(exec_cmd->data);
		}
		pfree(exec_cmd->data);

		if (WIFEXITED(status) == 0 || WEXITSTATUS(status) != 0)
		{
			exit(EXIT_FAILURE);
		}
		exit(0);
	}

	return pid;
}

/*
 * The function is helper function and can be used with the
 * wd_issue_ping_command() function to identify if the ping command
 * was successful */
bool
wd_get_ping_result(char *hostname, int exit_status, int outfd)
{
	/* First check the exit status of ping process */
	if (WIFEXITED(exit_status) == 0)
	{
		ereport(WARNING,
				(errmsg("watchdog failed to ping host\"%s\"", hostname),
				 errdetail("ping process exited abnormally")));
	}
	else if (WEXITSTATUS(exit_status) != 0)
	{
		ereport(WARNING,
				(errmsg("watchdog failed to ping host\"%s\"", hostname),
				 errdetail("ping process exits with code: %d", WEXITSTATUS(exit_status))));
	}
	else
	{
		StringInfoData  result;
		char		buf[WD_MAX_PING_RESULT];
		int		r_size = 0;

		ereport(DEBUG1,
				(errmsg("watchdog ping process for host \"%s\" exited successfully", hostname)));

		initStringInfo(&result);
		while ((r_size = read(outfd, &buf, sizeof(buf) - 1)) > 0)
		{
			buf[r_size] = '\0';
			appendStringInfoString(&result, buf);
		}
		/* Check whether average RTT >= 0 */
		if (get_result(result.data) >= 0)
		{
			ereport(DEBUG1,
					(errmsg("watchdog succeeded to ping a host \"%s\"", hostname)));
			pfree(result.data);
			return true;
		}
		ereport(WARNING,
				(errmsg("ping host\"%s\" failed", hostname),
				 errdetail("average RTT value is not greater than zero")));
		pfree(result.data);
	}
	return false;
}

/**
 * Get average round-trip time of ping result.
 */
static double
get_result(char *ping_data)
{
	char	   *sp = NULL;
	char	   *ep = NULL;
	int			i;
	double		msec = 0;

	if (ping_data == NULL)
	{
		ereport(WARNING,
				(errmsg("get_result: no ping data")));
		return -1;
	}
	ereport(DEBUG1,
			(errmsg("watchdog ping"),
			 errdetail("ping data: %s", ping_data)));

	/*
	 * skip result until average data typical result of ping is as follows,
	 * "rtt min/avg/max/mdev = 0.045/0.045/0.046/0.006 ms" we can find the
	 * average data beyond the 4th '/'.
	 */
	sp = ping_data;
	for (i = 0; i < 4; i++)
	{
		sp = strchr(sp, '/');
		if (sp == NULL)
		{
			return -1;
		}
		sp++;
	}

	ep = strchr(sp, '/');
	if (ep == NULL)
	{
		return -1;
	}

	*ep = '\0';
	errno = 0;

	/* convert to numeric data from text */
	msec = strtod(sp, (char **) NULL);

	if (errno != 0)
	{
		ereport(WARNING,
				(errmsg("get_result: strtod() failed"),
				 errdetail("%m")));
		return -1;
	}

	return msec;
}
