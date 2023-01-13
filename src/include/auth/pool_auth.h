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
 */


#ifndef pool_auth_h
#define pool_auth_h

extern void connection_do_auth(POOL_CONNECTION_POOL_SLOT * cp, char *password);
extern int	pool_do_auth(POOL_CONNECTION * frontend, POOL_CONNECTION_POOL * backend);
extern int	pool_do_reauth(POOL_CONNECTION * frontend, POOL_CONNECTION_POOL * cp);
extern void authenticate_frontend(POOL_CONNECTION * frontend);

extern void pool_random_salt(char *md5Salt);
extern void pool_random(void *buf, size_t len);

#endif /* pool_auth_h */
