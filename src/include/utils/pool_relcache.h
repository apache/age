/* -*-pgsql-c-*- */
/*
 *
 * $Header$
 *
 * pgpool: a language independent connection pool server for PostgreSQL
 * written by Tatsuo Ishii
 *
 * Copyright (c) 2003-2010	PgPool Global Development Group
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
 * pool_relcache.h.: pool_relcache.c related header file
 *
 */

#ifndef POOL_RELCACHE_H
#define POOL_RELCACHE_H

/* ------------------------
 * Relation cache structure
 *-------------------------
*/
#define MAX_ITEM_LENGTH	1024

/* Relation lookup cache structure */

typedef void *(*func_ptr) ();

typedef struct
{
	char		dbname[MAX_ITEM_LENGTH];	/* database name */
	char		relname[MAX_ITEM_LENGTH];	/* table name */
	void	   *data;			/* user data */
	int			refcnt;			/* reference count */
	int			session_id;		/* LocalSessionId */
	time_t		expire;			/* cache expiration absolute time in seconds */
}			PoolRelCache;

typedef struct
{
	int			num;			/* number of cache items */
	char		sql[MAX_ITEM_LENGTH];	/* Query to relation */

	/*
	 * User defined function to be called at data register. Argument is
	 * POOL_SELECT_RESULT *. This function must return a pointer to be saved
	 * in cache->data.
	 */
	func_ptr	register_func;

	/*
	 * User defined function to be called at data unregister. Argument
	 * cache->data.
	 */
	func_ptr	unregister_func;
	bool		cache_is_session_local; /* True if cache life time is session
										 * local */
	bool		no_cache_if_zero;	/* if register func returns 0, do not
									 * cache the data */
	PoolRelCache *cache;		/* cache data */
}			POOL_RELCACHE;

extern POOL_RELCACHE * pool_create_relcache(int cachesize, char *sql,
											func_ptr register_func, func_ptr unregister_func,
											bool issessionlocal);
extern void pool_discard_relcache(POOL_RELCACHE * relcache);
extern void *pool_search_relcache(POOL_RELCACHE * relcache, POOL_CONNECTION_POOL * backend, char *table);
extern char *remove_quotes_and_schema_from_relname(char *table);
extern void *int_register_func(POOL_SELECT_RESULT * res);
extern void *int_unregister_func(void *data);
extern void *string_register_func(POOL_SELECT_RESULT * res);
extern void *string_unregister_func(void *data);
extern bool SplitIdentifierString(char *rawstring, char separator, Node **namelist);

#endif							/* POOL_RELCACHE_H */
