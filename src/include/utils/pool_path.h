/* -*-pgsql-c-*- */
/*
 *
 * $Header$
 *
 * pgpool: a language independent connection pool server for PostgreSQL
 * written by Tatsuo Ishii
 *
 * Portions Copyright (c) 2003-2015,	PgPool Global Development Group
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
 * pool_path.h.: interface to pool_path.c
 *
 */

#ifndef POOL_PATH_H
#define POOL_PATH_H
#include"pool_type.h"
/*
 * MAXPGPATH: standard size of a pathname buffer in PostgreSQL (hence,
 * maximum usable pathname length is one less).
 *
 * We'd use a standard system header symbol for this, if there weren't
 * so many to choose from: MAXPATHLEN, MAX_PATH, PATH_MAX are all
 * defined by different "standards", and often have different values
 * on the same platform!  So we just punt and use a reasonably
 * generous setting here.
 */
#define MAXPGPATH       1024

#define IS_DIR_SEP(ch)  ((ch) == '/')
#define is_absolute_path(filename) \
( \
    ((filename)[0] == '/') \
)

extern void get_parent_directory(char *path);
extern void join_path_components(char *ret_path, const char *head, const char *tail);
extern void canonicalize_path(char *path);
extern char *last_dir_separator(const char *filename);
extern bool get_home_directory(char *buf, int bufsize);
extern bool get_os_username(char *buf, int bufsize);
extern char *get_current_working_dir(void);
extern char *make_absolute_path(const char *path, const char *base_dir);

#endif							/* POOL_PATH_H */
