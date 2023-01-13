/*
 * $Header$
 *
 * Handles watchdog connection, and protocol communication with pgpool-II
 *
 * pgpool: a language independent connection pool server for PostgreSQL
 * written by Tatsuo Ishii
 *
 * Copyright (c) 2003-2018	PgPool Global Development Group
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
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <sys/un.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/wait.h>
#include <errno.h>

#include "pool.h"
#include "auth/md5.h"
#include "utils/elog.h"
#include "utils/palloc.h"
#include "utils/memutils.h"
#include "pool_config.h"
#include "watchdog/wd_utils.h"
#include "utils/ssl_utils.h"

static int	has_setuid_bit(char *path);
static void *exec_func(void *arg);

/*
 * thread information for pool_thread
 */
typedef struct
{
	void	   *(*start_routine) (void *);
	void	   *arg;
}			WdThreadInfo;


void
wd_check_network_command_configurations(void)
{
	char		path[128];
	char	   *command;

	if (pool_config->use_watchdog == 0)
		return;

	/*
	 * If delegate IP is not assigned to the node the configuration is not
	 * used
	 */
	if (strlen(pool_config->delegate_ip) == 0)
		return;

	/* check setuid bit of ifup command */
	command = wd_get_cmd(pool_config->if_up_cmd);

	if (command)
	{
		if (command[0] == '/')
		{
			pfree(command);
			return;
		}

		snprintf(path, sizeof(path), "%s/%s", pool_config->if_cmd_path, command);
		pfree(command);
		if (!has_setuid_bit(path))
		{
			ereport(WARNING,
					(errmsg("checking setuid bit of if_up_cmd"),
					 errdetail("ifup[%s] doesn't have setuid bit", path)));
		}
	}
	else
	{
		ereport(FATAL,
				(errmsg("invalid configuration for if_up_cmd parameter"),
				 errdetail("unable to get command from \"%s\"", pool_config->if_up_cmd)));
	}
	/* check setuid bit of ifdown command */

	command = wd_get_cmd(pool_config->if_down_cmd);
	if (command)
	{
		snprintf(path, sizeof(path), "%s/%s", pool_config->if_cmd_path, command);
		pfree(command);
		if (!has_setuid_bit(path))
		{
			ereport(WARNING,
					(errmsg("checking setuid bit of if_down_cmd"),
					 errdetail("ifdown[%s] doesn't have setuid bit", path)));
		}
	}
	else
	{
		ereport(FATAL,
				(errmsg("invalid configuration for if_down_cmd parameter"),
				 errdetail("unable to get command from \"%s\"", pool_config->if_down_cmd)));
	}

	/* check setuid bit of arping command */
	command = wd_get_cmd(pool_config->arping_cmd);
	if (command)
	{
		snprintf(path, sizeof(path), "%s/%s", pool_config->arping_path, command);
		pfree(command);
		if (!has_setuid_bit(path))
		{
			ereport(WARNING,
					(errmsg("checking setuid bit of arping command"),
					 errdetail("arping[%s] doesn't have setuid bit", path)));
		}
	}
	else
	{
		ereport(FATAL,
				(errmsg("invalid configuration for arping_cmd parameter"),
				 errdetail("unable to get command from \"%s\"", pool_config->arping_cmd)));
	}

}

/*
 * if the file has setuid bit and the owner is root, it returns 1, otherwise returns 0
 */
static int
has_setuid_bit(char *path)
{
	struct stat buf;

	if (stat(path, &buf) < 0)
	{
		ereport(FATAL,
				(return_code(1),
				 errmsg("has_setuid_bit: command '%s' not found", path)));
	}
	return ((buf.st_uid == 0) && (S_ISREG(buf.st_mode)) && (buf.st_mode & S_ISUID)) ? 1 : 0;
}


#ifdef USE_SSL

void
wd_calc_hash(const char *str, int len, char *buf)
{
	calculate_hmac_sha256(str, len, buf);
}
#else

/* calculate hash for authentication using packet contents */
void
wd_calc_hash(const char *str, int len, char *buf)
{
	char		pass[(MAX_PASSWORD_SIZE + 1) / 2];
	char		username[(MAX_PASSWORD_SIZE + 1) / 2];
	size_t		pass_len;
	size_t		username_len;
	size_t		authkey_len;
	char		tmp_buf[(MD5_PASSWD_LEN + 1) * 2];

	/* use first half of authkey as username, last half as password */
	authkey_len = strlen(pool_config->wd_authkey);

	if (len <= 0 || authkey_len <= 0)
		goto wd_calc_hash_error;

	username_len = authkey_len / 2;
	pass_len = authkey_len - username_len;
	if (snprintf(username, username_len + 1, "%s", pool_config->wd_authkey) < 0
		|| snprintf(pass, pass_len + 1, "%s", pool_config->wd_authkey + username_len) < 0)
		goto wd_calc_hash_error;

	/* calculate hash using md5 encrypt */
	if (!pool_md5_encrypt(pass, username, strlen(username), tmp_buf + MD5_PASSWD_LEN + 1))
		goto wd_calc_hash_error;

	tmp_buf[sizeof(tmp_buf) - 1] = '\0';

	if (!pool_md5_encrypt(tmp_buf + MD5_PASSWD_LEN + 1, str, len, tmp_buf))
		goto wd_calc_hash_error;

	memcpy(buf, tmp_buf, MD5_PASSWD_LEN);
	buf[MD5_PASSWD_LEN] = '\0';

	return;

wd_calc_hash_error:
	buf[0] = '\0';
	return;
}
#endif

/*
 * string_replace:
 * returns the new palloced string after replacing all
 * occurrences of pattern in string with replacement string
 */
char *
string_replace(const char *string, const char *pattern, const char *replacement)
{
	char	   *tok = NULL;
	char	   *newstr = NULL;
	char	   *oldstr = NULL;
	char	   *head = NULL;
	size_t		pat_len,
				rep_len;

	newstr = pstrdup(string);
	/* bail out if no pattern or replacement is given */
	if (pattern == NULL || replacement == NULL)
		return newstr;

	pat_len = strlen(pattern);
	rep_len = strlen(replacement);

	head = newstr;
	while ((tok = strstr(head, pattern)))
	{
		oldstr = newstr;
		newstr = palloc(strlen(oldstr) - pat_len + rep_len + 1);

		memcpy(newstr, oldstr, tok - oldstr);
		memcpy(newstr + (tok - oldstr), replacement, rep_len);
		memcpy(newstr + (tok - oldstr) + rep_len, tok + pat_len, strlen(oldstr) - pat_len - (tok - oldstr));
		/* put the string terminator */
		memset(newstr + strlen(oldstr) - pat_len + rep_len, 0, 1);
		/* move back head right after the last replacement */
		head = newstr + (tok - oldstr) + rep_len;
		pfree(oldstr);
	}
	return newstr;
}

/*
 * The function is wrapper over pthread_create.
 */
int			watchdog_thread_create(pthread_t *thread, const pthread_attr_t *attr, void *(*start_routine) (void *), void *arg)
{
	WdThreadInfo *thread_arg = palloc(sizeof(WdThreadInfo));

	thread_arg->arg = arg;
	thread_arg->start_routine = start_routine;
	return pthread_create(thread, attr, exec_func, thread_arg);
}

static void *
exec_func(void *arg)
{
	WdThreadInfo *thread_arg = (WdThreadInfo *) arg;

	Assert(thread_arg != NULL);
	return thread_arg->start_routine(thread_arg->arg);
}
