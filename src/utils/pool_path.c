/* -*-pgsql-c-*- */
/*
 *
 * $Header$
 *
 * pgpool: a language independent connection pool server for PostgreSQL
 * written by Tatsuo Ishii
 *
 * Portions Copyright (c) 2003-2017,	PgPool Global Development Group
 * Portions Copyright (c) 2004, PostgreSQL Global Development Group
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
 * pool_path.c.: small functions to manipulate paths
 *
 */

#include "pool.h"
#include "utils/pool_path.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <pwd.h>
#include <errno.h>
#include "utils/elog.h"
#include "utils/palloc.h"
#include "utils/memutils.h"


static void trim_directory(char *path);
static void trim_trailing_separator(char *path);
static int pqGetpwuid(uid_t uid, struct passwd *resultbuf, char *buffer,
		   size_t buflen, struct passwd **result);

/*
 * get_parent_directory
 *
 * Modify the given string in-place to name the parent directory of the
 * named file.
 */
void
get_parent_directory(char *path)
{
	trim_directory(path);
}

/*
 * Obtain user's home directory, return in given buffer
 */
bool
get_home_directory(char *buf, int bufsize)
{
	char		pwdbuf[BUFSIZ];
	struct passwd pwdstr;
	struct passwd *pwd = NULL;

	if (pqGetpwuid(geteuid(), &pwdstr, pwdbuf, sizeof(pwdbuf), &pwd) != 0)
		return false;
	strlcpy(buf, pwd->pw_dir, bufsize);
	return true;
}

/*
 * Obtain user name, return in given buffer
 */
bool
get_os_username(char *buf, int bufsize)
{
	char		pwdbuf[BUFSIZ];
	struct passwd pwdstr;
	struct passwd *pwd = NULL;

	if (pqGetpwuid(geteuid(), &pwdstr, pwdbuf, sizeof(pwdbuf), &pwd) != 0)
		return false;
	strlcpy(buf, pwd->pw_name, bufsize);
	return true;
}

/* stolen from PostgreSQL src/port/thread.c
 * XXX: This is from 9.4 but the latest pgsql have changed this.
 */
/*
 * Wrapper around getpwuid() or getpwuid_r() to mimic POSIX getpwuid_r()
 * behaviour, if it is not available or required.
 */
static int
pqGetpwuid(uid_t uid, struct passwd *resultbuf, char *buffer,
		   size_t buflen, struct passwd **result)
{
#if defined(HAVE_GETPWUID_R)

#ifdef GETPWUID_R_5ARG
	/* POSIX version */
	getpwuid_r(uid, resultbuf, buffer, buflen, result);
#else

	/*
	 * Early POSIX draft of getpwuid_r() returns 'struct passwd *'.
	 * getpwuid_r(uid, resultbuf, buffer, buflen)
	 */
	*result = getpwuid_r(uid, resultbuf, buffer, buflen);
#endif
#else

	/* no getpwuid_r() available, just use getpwuid() */
	*result = getpwuid(uid);
#endif

	return (*result == NULL) ? -1 : 0;
}

/*
 *  trim_directory
 *
 *  Trim trailing directory from path, that is, remove any trailing slashes,
 *  the last pathname component, and the slash just ahead of it --- but never
 *  remove a leading slash.
 */
static void
trim_directory(char *path)
{
	char	   *p;

	if (path[0] == '\0')
		return;

	/* back up over trailing slash(es) */
	for (p = path + strlen(path) - 1; IS_DIR_SEP(*p) && p > path; p--);

	/* back up over directory name */
	for (; !IS_DIR_SEP(*p) && p > path; p--);

	/* if multiple slashes before directory name, remove 'em all */
	for (; p > path && IS_DIR_SEP(*(p - 1)); p--);

	/* don't erase a leading slash */
	if (p == path && IS_DIR_SEP(*p))
		p++;

	*p = '\0';
}


/*
 * join_path_components - join two path components, inserting a slash
 *
 * ret_path is the output area (must be of size MAXPGPATH)
 *
 * ret_path can be the same as head, but not the same as tail.
 */
void
join_path_components(char *ret_path, const char *head, const char *tail)
{
	if (ret_path != head)
		StrNCpy(ret_path, head, MAXPGPATH);

	/*
	 * Remove any leading "." and ".." in the tail component, adjusting head
	 * as needed.
	 */
	for (;;)
	{
		if (tail[0] == '.' && IS_DIR_SEP(tail[1]))
		{
			tail += 2;
		}
		else if (tail[0] == '.' && tail[1] == '\0')
		{
			tail += 1;
			break;
		}
		else if (tail[0] == '.' && tail[1] == '.' && IS_DIR_SEP(tail[2]))
		{
			trim_directory(ret_path);
			tail += 3;
		}
		else if (tail[0] == '.' && tail[1] == '.' && tail[2] == '\0')
		{
			trim_directory(ret_path);
			tail += 2;
			break;
		}
		else
			break;
	}

	if (*tail)
		snprintf(ret_path + strlen(ret_path), MAXPGPATH - strlen(ret_path), "/%s", tail);
}


/*
 *  Clean up path by:
 *      o  remove trailing slash
 *      o  remove duplicate adjacent separators
 *      o  remove trailing '.'
 *      o  process trailing '..' ourselves
 */
void
canonicalize_path(char *path)
{
	char	   *p,
			   *to_p;
	bool		was_sep = false;

	/*
	 * Removing the trailing slash on a path means we never get ugly double
	 * trailing slashes.
	 */
	trim_trailing_separator(path);

	/*
	 * Remove duplicate adjacent separators
	 */
	p = path;
	to_p = p;
	for (; *p; p++, to_p++)
	{
		/* Handle many adjacent slashes, like "/a///b" */
		while (*p == '/' && was_sep)
			p++;
		if (to_p != p)
			*to_p = *p;
		was_sep = (*p == '/');
	}
	*to_p = '\0';

	/*
	 * Remove any trailing uses of "." and process ".." ourselves
	 */
	for (;;)
	{
		int			len = strlen(path);

		if (len > 2 && strcmp(path + len - 2, "/.") == 0)
			trim_directory(path);
		else if (len > 3 && strcmp(path + len - 3, "/..") == 0)
		{
			trim_directory(path);
			trim_directory(path);
			/* remove directory above */
		}
		else
			break;
	}
}

/*
 *      last_dir_separator
 *
 * Find the location of the last directory separator, return
 * NULL if not found.
 */
char *
last_dir_separator(const char *filename)
{
	const char *p,
			   *ret = NULL;

	for (p = filename; *p; p++)
		if (IS_DIR_SEP(*p))
			ret = p;
	return (char *) ret;
}

/*
 *  trim_trailing_separator
 *
 * trim off trailing slashes, but not a leading slash
 */
static void
trim_trailing_separator(char *path)
{
	char	   *p;

	p = path + strlen(path);
	if (p > path)
		for (p--; p > path && IS_DIR_SEP(*p); p--)
			*p = '\0';
}

char *
get_current_working_dir(void)
{
	char	   *buf = NULL;
	size_t		buflen = MAXPGPATH;

	for (;;)
	{
		buf = palloc(buflen);

		if (getcwd(buf, buflen))
			break;

		else if (errno == ERANGE)
		{
			pfree(buf);
			buflen *= 2;
			continue;
		}
		else
		{
			int			save_errno = errno;

			pfree(buf);
			errno = save_errno;
			ereport(ERROR,
					(errmsg("could not get current working directory: %m")));
			return NULL;
		}
	}
	return buf;
}

/*
 * make_absolute_path
 *
 * If the given pathname isn't already absolute, make it so, interpreting
 * it relative to the current working directory.
 *
 * if arg base_dir is NULL, The function gets the current cwd
 * Also canonicalizes the path.  The result is always a palloc'd copy.
 *
 * Logic borrowed from PostgreSQL source
 */
char *
make_absolute_path(const char *path, const char *base_dir)
{
	char	   *new;

	/* Returning null for null input is convenient for some callers */
	if (path == NULL)
		return NULL;

	if (!is_absolute_path(path))
	{
		const char *cwd = NULL;

		if (base_dir == NULL)
		{
			cwd = get_current_working_dir();
			if (!cwd)
				return NULL;
		}
		else
			cwd = base_dir;

		new = palloc(strlen(cwd) + strlen(path) + 2);
		sprintf(new, "%s/%s", cwd, path);

		if (!base_dir)
			pfree((void *) cwd);
	}
	else
	{
		new = pstrdup(path);
	}

	/* Make sure punctuation is canonical, too */
	canonicalize_path(new);

	return new;
}
