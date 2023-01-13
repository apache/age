/* -*-pgsql-c-*- */
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
 *
 * Global variables. Should be eventually removed.
 */
#include <unistd.h> /*For getpid*/
#include "pool.h"
#include "utils/elog.h"


pid_t		mypid;				/* pgpool parent process id */
pid_t		myProcPid;		/* process pid */
ProcessType processType;
ProcessState processState;

/*
 * Application name
 */
static	char	*process_application_name = "main";

/*
 * Fixed application names. ordered by ProcessType.
 */
char	*application_names[] = {"main",
								"child",
								"sr_check_worker",
								"heart_beat_sender",
								"heart_beat_receiver",
								"watchdog",
								"life_check",
								"follow_child",
								"watchdog_utility",
								"pcp_main",
								"pcp_child",
								"health_check",
								"logger"
};

char *
get_application_name_for_process(ProcessType ptype)
{
	if (ptype < 0 || ptype >= PT_LAST_PTYPE)
	{
		ereport(ERROR,
				(errmsg("failed to set application name. process type: %d", ptype)));
	}
	return application_names[ptype];
}

/*
 * Set application name by ProcessType
 */
void
set_application_name(ProcessType ptype)
{
	process_application_name = get_application_name_for_process(ptype);
}

/*
 * Set application name with arbitrary string. The storage for the string must
 * be persistent at least in the session.
 */
void
set_application_name_with_string(char *string)
{
	process_application_name = string;
}

/*
 * Set application name with suffix
 */
void
set_application_name_with_suffix(ProcessType ptype, int suffix)
{
	static char	appname_buf[POOLCONFIG_MAXNAMELEN +1];
	snprintf(appname_buf, POOLCONFIG_MAXNAMELEN, "%s%d", get_application_name_for_process(ptype), suffix);
	set_application_name_with_string(appname_buf);
}

/*
 * Get current application name
 */
char *
get_application_name(void)
{
	return process_application_name;
}

void SetProcessGlobalVariables(ProcessType pType)
{
	processType = pType;
	myProcPid = getpid();
	set_application_name(pType);
}
