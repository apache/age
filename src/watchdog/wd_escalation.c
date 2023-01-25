/*
 *
 * wd_escalation
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
 */


#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>

#ifdef __FreeBSD__
#include <sys/wait.h>
#endif

#include "utils/pool_signal.h"
#include "utils/elog.h"
#include "utils/palloc.h"
#include "utils/memutils.h"
#include "utils/ps_status.h"
#include "pool_config.h"
#include "watchdog/wd_utils.h"

#include "query_cache/pool_memqcache.h"

static void
wd_exit(int exit_signo)
{
	sigset_t	mask;

	sigemptyset(&mask);
	sigaddset(&mask, SIGTERM);
	sigaddset(&mask, SIGINT);
	sigaddset(&mask, SIGQUIT);
	sigaddset(&mask, SIGCHLD);
	sigprocmask(SIG_BLOCK, &mask, NULL);

	exit(0);
}

/*
 * fork escalation process
 */
pid_t
fork_escalation_process(void)
{
	pid_t		pid;

	pid = fork();
	if (pid != 0)
	{
		if (pid == -1)
			ereport(NOTICE,
					(errmsg("failed to fork a escalation process")));
		return pid;
	}
	on_exit_reset();
	SetProcessGlobalVariables(PT_WATCHDOG_UTILITY);

	POOL_SETMASK(&UnBlockSig);

	init_ps_display("", "", "", "");

	pool_signal(SIGTERM, wd_exit);
	pool_signal(SIGINT, wd_exit);
	pool_signal(SIGQUIT, wd_exit);
	pool_signal(SIGCHLD, SIG_DFL);
	pool_signal(SIGHUP, SIG_IGN);
	pool_signal(SIGPIPE, SIG_IGN);

	MemoryContextSwitchTo(TopMemoryContext);

	set_ps_display("watchdog escalation", false);

	ereport(LOG,
			(errmsg("watchdog: escalation started")));

	/*
	 * STEP 1 clear shared memory cache
	 */
	if (pool_config->memory_cache_enabled && pool_is_shmem_cache() &&
		pool_config->clear_memqcache_on_escalation)
	{
		ereport(LOG,
				(errmsg("watchdog escalation"),
				 errdetail("clearing all the query cache on shared memory")));

		pool_clear_memory_cache();
	}

	/*
	 * STEP 2 execute escalation command provided by user in pgpool conf file
	 */
	if (strlen(pool_config->wd_escalation_command))
	{
		int			r = system(pool_config->wd_escalation_command);

		if (WIFEXITED(r))
		{
			if (WEXITSTATUS(r) == EXIT_SUCCESS)
				ereport(LOG,
						(errmsg("watchdog escalation successful")));
			else
			{
				ereport(WARNING,
						(errmsg("watchdog escalation command failed with exit status: %d", WEXITSTATUS(r))));
			}
		}
		else
		{
			ereport(WARNING,
					(errmsg("watchdog escalation command exit abnormally")));
		}
	}

	/*
	 * STEP 3 bring up the delegate IP
	 */
	if (strlen(pool_config->delegate_ip) != 0)
	{
		if (wd_IP_up() != WD_OK)
			ereport(WARNING,
					(errmsg("watchdog escalation failed to acquire delegate IP")));

	}
	exit(0);
}

/*
 * fork de-escalation process
 */
pid_t
fork_plunging_process(void)
{
	pid_t		pid;

	pid = fork();
	if (pid != 0)
	{
		if (pid == -1)
			ereport(NOTICE,
					(errmsg("failed to fork a de-escalation process")));
		return pid;
	}
	on_exit_reset();
	SetProcessGlobalVariables(PT_WATCHDOG_UTILITY);

	POOL_SETMASK(&UnBlockSig);

	init_ps_display("", "", "", "");

	pool_signal(SIGTERM, wd_exit);
	pool_signal(SIGINT, wd_exit);
	pool_signal(SIGQUIT, wd_exit);
	pool_signal(SIGCHLD, SIG_DFL);
	pool_signal(SIGHUP, SIG_IGN);
	pool_signal(SIGPIPE, SIG_IGN);

	MemoryContextSwitchTo(TopMemoryContext);

	set_ps_display("watchdog de-escalation", false);

	ereport(LOG,
			(errmsg("watchdog: de-escalation started")));

	/*
	 * STEP 1 execute de-escalation command provided by user in pgpool conf
	 * file
	 */
	if (strlen(pool_config->wd_de_escalation_command))
	{
		int			r = system(pool_config->wd_de_escalation_command);

		if (WIFEXITED(r))
		{
			if (WEXITSTATUS(r) == EXIT_SUCCESS)
				ereport(LOG,
						(errmsg("watchdog de-escalation successful")));
			else
			{
				ereport(WARNING,
						(errmsg("watchdog de-escalation command failed with exit status: %d", WEXITSTATUS(r))));
			}
		}
		else
		{
			ereport(WARNING,
					(errmsg("watchdog de-escalation command exit abnormally")));
		}
	}

	/*
	 * STEP 2 bring down the delegate IP
	 */

	if (strlen(pool_config->delegate_ip) != 0)
	{
		if (wd_IP_down() != WD_OK)
			ereport(WARNING,
					(errmsg("watchdog de-escalation failed to bring down delegate IP")));
	}
	exit(0);
}
