/* -*-pgsql-c-*- */
/*
 * $Header$
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
 * pool_relcache.c: Per process relation cache modules
 */
#include "config.h"
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <limits.h>
#include <time.h>

#include "pool.h"
#include "utils/pool_relcache.h"
#include "context/pool_session_context.h"
#include "query_cache/pool_memqcache.h"
#include "protocol/pool_process_query.h"
#include "pool_config.h"
#include "utils/palloc.h"
#include "utils/memutils.h"
#include "utils/elog.h"
#include "parser/scansup.h"

static void SearchRelCacheErrorCb(void *arg);
static POOL_SELECT_RESULT *query_cache_to_relation_cache(char *data, size_t size);
static char *relation_cache_to_query_cache(POOL_SELECT_RESULT *res,size_t *size);


/*
 * Create relation cache
 */
POOL_RELCACHE *
pool_create_relcache(int cachesize, char *sql,
					 func_ptr register_func, func_ptr unregister_func,
					 bool issessionlocal)
{
	POOL_RELCACHE *p;
	PoolRelCache *ip;
	MemoryContext old_context;

	if (cachesize < 0)
	{
		ereport(WARNING,
				(errmsg("failed to create relcache: wrong cache size: %d", cachesize)));
		return NULL;
	}

	/*
	 * Create the relcache in session context if the cache is session local,
	 * otherwise make home in TopMemoryContext
	 */
	old_context = MemoryContextSwitchTo(TopMemoryContext);

	ip = (PoolRelCache *) palloc0(sizeof(PoolRelCache) * cachesize);
	p = (POOL_RELCACHE *) palloc(sizeof(POOL_RELCACHE));

	MemoryContextSwitchTo(old_context);

	p->num = cachesize;
	strlcpy(p->sql, sql, sizeof(p->sql));
	p->register_func = register_func;
	p->unregister_func = unregister_func;
	p->cache_is_session_local = issessionlocal;
	p->no_cache_if_zero = false;
	p->cache = ip;

	return p;
}

/*
 * Discard relation cache.
 */
void
pool_discard_relcache(POOL_RELCACHE * relcache)
{
	int			i;

	for (i = 0; i < relcache->num; i++)
	{
		(*relcache->unregister_func) (relcache->cache[i].data);
	}
	pfree(relcache->cache);
	pfree(relcache);
}

/*
 * Search relcache. If found, return user data. Otherwise return 0.
 * If not found in cache, do the query and store the result into cache and return it.
 */
void *
pool_search_relcache(POOL_RELCACHE * relcache, POOL_CONNECTION_POOL * backend, char *table)
{
	char	   *dbname;
	int			i;
	int			maxrefcnt = INT_MAX;
	char		query[1024];
	POOL_SELECT_RESULT *res = NULL;
	int			index = 0;
	int			local_session_id;
	time_t		now;
	void		*result;
	ErrorContextCallback callback;
    pool_sigset_t oldmask;
	bool locked;
	int			query_cache_not_found = 1;
	char		*query_cache_data = NULL;
	size_t		query_cache_len;
	POOL_SESSION_CONTEXT *session_context;
	int			node_id;

	session_context = pool_get_session_context(false);

	local_session_id = pool_get_local_session_id();
	if (local_session_id < 0)
		return NULL;

	/*
	 * Obtain database name and node id to be sent query.  If
	 * relcache_query_target is RELQTARGET_LOADL_BALANCE_NODE, we consider
	 * load balance node id to be used to send queries.
	 * 
	 * Note that we need to use VALID_BACKEND_RAW, rather than VALID_BACKEND
	 * since pool_is_node_to_be_sent_in_current_query(being called by
	 * VALID_BACKEND) assumes that if query context exists, where_to_send map
	 * is already setup but it's not always the case because
	 * pool_search_relcache is mostly called *before* the where_to_send map is
	 * established.
	 */
	if (pool_config->relcache_query_target == RELQTARGET_LOAD_BALANCE_NODE &&
		session_context && VALID_BACKEND_RAW(session_context->load_balance_node_id) &&
		backend->slots[session_context->load_balance_node_id])
	{
		dbname = backend->slots[session_context->load_balance_node_id]->sp->database;
		node_id = session_context->load_balance_node_id;
	}
	else
	{
		dbname = MAIN_CONNECTION(backend)->sp->database;

		/*
		 * If in streaming replication mode, prefer to send query to the
		 * primary node if it exists.
		 */
		if (STREAM && PRIMARY_NODE_ID >= 0)
			node_id = PRIMARY_NODE_ID;
		else
			node_id = MAIN_NODE_ID;
	}

	now = time(NULL);

	/* Look for cache first */
	for (i = 0; i < relcache->num; i++)
	{
		/*
		 * If cache is session local, we need to check session id
		 */
		if (relcache->cache_is_session_local)
		{
			if (relcache->cache[i].session_id != local_session_id)
				continue;
		}

		if (strcasecmp(relcache->cache[i].dbname, dbname) == 0 &&
			strcasecmp(relcache->cache[i].relname, table) == 0)
		{
			if (relcache->cache[i].expire > 0)
			{
				if (now > relcache->cache[i].expire)
				{
					ereport(DEBUG1,
							(errmsg("searching relcache"),
							 errdetail("relcache for database:%s table:%s expired. now:%ld expiration time:%ld", dbname, table, now, relcache->cache[i].expire)));

					relcache->cache[i].refcnt = 0;
					break;
				}
			}

			/* Found */
			if (relcache->cache[i].refcnt < INT_MAX)
				relcache->cache[i].refcnt++;

			ereport(DEBUG1,
					(errmsg("hit local relation cache"),
					errdetail("query:%s", relcache->sql)));

			return relcache->cache[i].data;
		}
	}

	/* Not in cache. Check the system catalog */
	snprintf(query, sizeof(query), relcache->sql, table);

	per_node_statement_log(backend, node_id, query);

	/*
	 * Register a error context callback to throw proper context message
	 */
	callback.callback = SearchRelCacheErrorCb;
	callback.arg = NULL;
	callback.previous = error_context_stack;
	error_context_stack = &callback;

	locked = pool_is_shmem_lock();
	/*
	 * if enable_shared_relcache is true, search query cache.
	 */
    if (pool_config->enable_shared_relcache)
	{
		/* if shmem is not locked by this process, get the lock */
		if (!locked)
		{
			POOL_SETMASK2(&BlockSig, &oldmask);
			pool_shmem_lock(POOL_MEMQ_SHARED_LOCK);
		}
	    PG_TRY();
		{
			/* search catalog cache in query cache */
			query_cache_not_found = pool_fetch_cache(backend, query, &query_cache_data, &query_cache_len);
		}
	    PG_CATCH();
		{
			pool_shmem_unlock();
			POOL_SETMASK(&oldmask);
	        PG_RE_THROW();
		}
		PG_END_TRY();
	}
	/* If not in query cache or not used, send query for backend. */
	if (query_cache_not_found)
	{
		ereport(DEBUG1,
				(errmsg("not hit local relation cache and query cache"),
				errdetail("query:%s", query)));

		do_query(CONNECTION(backend, node_id), query, &res, MAJOR(backend));
		/* Register cache */
		result = (*relcache->register_func) (res);
		/* save local catalog cache in query cache */
	    if (pool_config->enable_shared_relcache)
		{
			query_cache_data = relation_cache_to_query_cache(res, &query_cache_len);
			pool_catalog_commit_cache(backend, query, query_cache_data, query_cache_len);
		}
	}
	else
	{
		ereport(DEBUG1,
				(errmsg("hit query cache"),
				errdetail("query:%s", query)));

		/* catalog cache found in query_cache, copy local relation cache */
		res = query_cache_to_relation_cache(query_cache_data,query_cache_len);
		result = (*relcache->register_func) (res);
	}
	/* if shmem is locked by this function, unlock it */
	if (pool_config->enable_shared_relcache && !locked)
	{
		pool_shmem_unlock();
		POOL_SETMASK(&oldmask);
	}

	error_context_stack = callback.previous;

	/*
	 * Look for replacement in cache
	 */
	for (i = 0; i < relcache->num; i++)
	{
		/*
		 * If cache is session local, we can discard old cache immediately
		 */
		if (relcache->cache_is_session_local)
		{
			if (relcache->cache[i].session_id != local_session_id)
			{
				index = i;
				relcache->cache[i].refcnt = 0;
				break;
			}
		}

		if (relcache->cache[i].refcnt == 0)
		{
			/* Found empty slot */
			index = i;
			break;
		}
		else if (relcache->cache[i].refcnt < maxrefcnt)
		{
			maxrefcnt = relcache->cache[i].refcnt;
			index = i;
		}
	}

	if (relcache->cache[index].refcnt != 0)
	{
		ereport(LOG,
				(errmsg("searching relcache. cache replacement occurred")));

	}

	if (!pool_is_ignore_till_sync() && (!relcache->no_cache_if_zero || result))
	{
		strlcpy(relcache->cache[index].dbname, dbname, MAX_ITEM_LENGTH);
		strlcpy(relcache->cache[index].relname, table, MAX_ITEM_LENGTH);
		relcache->cache[index].refcnt = 1;
		relcache->cache[index].session_id = local_session_id;
		if (pool_config->relcache_expire > 0)
		{
			relcache->cache[index].expire = now + pool_config->relcache_expire;
		}
		else
		{
			relcache->cache[index].expire = 0;
		}

		/*
		 * Call user defined unregister/register function.
		 */
		(*relcache->unregister_func) (relcache->cache[index].data);
		relcache->cache[index].data = result;
	}
	free_select_result(res);
	if (query_cache_data)
		pfree(query_cache_data);
	return result;
}

static void
SearchRelCacheErrorCb(void *arg)
{
	errcontext("while searching system catalog, When relcache is missed");
}


/*
 * SplitIdentifierString --- parse a string containing identifiers
 *
 * This is the guts of textToQualifiedNameList, and is exported for use in
 * other situations such as parsing GUC variables.  In the GUC case, it's
 * important to avoid memory leaks, so the API is designed to minimize the
 * amount of stuff that needs to be allocated and freed.
 *
 * Inputs:
 *	rawstring: the input string; must be overwritable!	On return, it's
 *			   been modified to contain the separated identifiers.
 *	separator: the separator punctuation expected between identifiers
 *			   (typically '.' or ',').  Whitespace may also appear around
 *			   identifiers.
 * Outputs:
 *	namelist: filled with a palloc'd list of pointers to identifiers within
 *			  rawstring.  Caller should list_free() this even on error return.
 *
 * Returns true if okay, false if there is a syntax error in the string.
 *
 * Note that an empty string is considered okay here, though not in
 * textToQualifiedNameList.
 */
bool
SplitIdentifierString(char *rawstring, char separator,
					  Node **nlist)
{
	char	   *nextp = rawstring;
	bool		done = false;
	List	  **namelist = (List **) nlist;

	*namelist = NIL;

	while (scanner_isspace(*nextp))
		nextp++;				/* skip leading whitespace */

	if (*nextp == '\0')
		return true;			/* allow empty string */

	/* At the top of the loop, we are at start of a new identifier. */
	do
	{
		char	   *curname;
		char	   *endp;

		if (*nextp == '"')
		{
			/* Quoted name --- collapse quote-quote pairs, no downcasing */
			curname = nextp + 1;
			for (;;)
			{
				endp = strchr(nextp + 1, '"');
				if (endp == NULL)
					return false;	/* mismatched quotes */
				if (endp[1] != '"')
					break;		/* found end of quoted name */
				/* Collapse adjacent quotes into one quote, and look again */
				memmove(endp, endp + 1, strlen(endp));
				nextp = endp;
			}
			/* endp now points at the terminating quote */
			nextp = endp + 1;
		}
		else
		{
			/* Unquoted name --- extends to separator or whitespace */
			char	   *downname;
			int			len;

			curname = nextp;
			while (*nextp && *nextp != separator &&
				   !scanner_isspace(*nextp))
				nextp++;
			endp = nextp;
			if (curname == nextp)
				return false;	/* empty unquoted name not allowed */

			/*
			 * Downcase the identifier, using same code as main lexer does.
			 *
			 * XXX because we want to overwrite the input in-place, we cannot
			 * support a downcasing transformation that increases the string
			 * length.  This is not a problem given the current implementation
			 * of downcase_truncate_identifier, but we'll probably have to do
			 * something about this someday.
			 */
			len = endp - curname;
			downname = downcase_truncate_identifier(curname, len, false);
			Assert(strlen(downname) <= len);
			strncpy(curname, downname, len);	/* strncpy is required here */
			pfree(downname);
		}

		while (scanner_isspace(*nextp))
			nextp++;			/* skip trailing whitespace */

		if (*nextp == separator)
		{
			nextp++;
			while (scanner_isspace(*nextp))
				nextp++;		/* skip leading whitespace for next */
			/* we expect another name, so done remains false */
		}
		else if (*nextp == '\0')
			done = true;
		else
			return false;		/* invalid syntax */

		/* Now safe to overwrite separator with a null */
		*endp = '\0';

		/* Truncate name if it's overlength */
		truncate_identifier(curname, strlen(curname), false);

		/*
		 * Finished isolating current name --- add it to list
		 */
		*namelist = lappend(*namelist, curname);

		/* Loop back if we didn't reach end of string */
	} while (!done);

	return true;
}

char *
remove_quotes_and_schema_from_relname(char *table)
{
	static char rel[MAX_ITEM_LENGTH];
	char	   *rawstring;
	List	   *names;

	rawstring = pstrdup(table);
	if(SplitIdentifierString(rawstring, '.', (Node **) &names) && names != NIL)
	{
		/*
		 * Since table name is always the last one in the list,
		 * we use llast() to get table name.
		 */
		strlcpy(rel, llast(names), sizeof(rel));
	}
	else
	{
		rel[0] = '\0';
	}

	pfree(rawstring);
	list_free(names);

	return rel;
}

/*
 * Standard register/unregister function for "SELECT count(*)" type
 * query. Returns row count.
 */
void *
int_register_func(POOL_SELECT_RESULT * res)
{
	if (res->numrows >= 1)
		return (void *) atol(res->data[0]);
	return (void *) 0;
}

void *
int_unregister_func(void *data)
{
	/* Nothing to do since no memory was allocated */
	return NULL;
}

void *
string_register_func(POOL_SELECT_RESULT * res)
{
	return (res->numrows > 0) ? strdup(res->data[0]) : NULL;
}

void *
string_unregister_func(void *data)
{
	if (data)
		free(data);
	return (void *) 0;
}

static POOL_SELECT_RESULT *
query_cache_to_relation_cache(char *data, size_t size)
{
	POOL_SELECT_RESULT *res;
	char *p;
	int i;
	int len;

	p = data;

	res = palloc0(sizeof(*res));
	res->rowdesc = palloc0(sizeof(RowDesc));

	/* rowdesc */
	res->rowdesc->num_attrs = *((int *)p);
	p += sizeof(int);

	/* numrows */
	res->numrows = *((int *)p);
	p += sizeof(int);

	len = res->rowdesc->num_attrs * res->numrows;

	res->nullflags = palloc(len * sizeof(int));
	res->data = palloc0(len * sizeof(char *));

	/* nullflags */
	for (i = 0; i < len; i++)
	{
		res->nullflags[i] = *((int *)p);
		p += sizeof(int);
	}
	/* data */
	for (i = 0; i < len; i++)
	{
		if ( res->nullflags[i] > 0)
		{
			res->data[i] = palloc(res->nullflags[i] + 1);
			memcpy(res->data[i], p, res->nullflags[i]);
			*(res->data[i] + res->nullflags[i]) = '\0';
			p += res->nullflags[i];
		}
	}

	return res;
}

static char *
relation_cache_to_query_cache(POOL_SELECT_RESULT *res,size_t *size)
{
	char * data;
	char * p;

	int i;
	int array_size;
	int mysize;

	mysize = 0;

	/* RoeDesc *rowdesc */
	/* int res->rowdesc->num_attrs;*/
	mysize += sizeof(int); /* only rodesc->num_attrs */
	/* int numrows */
	mysize += sizeof(int);
	/* int *nullflags  */
	mysize += sizeof(int) * res->rowdesc->num_attrs * res->numrows;
	/*  char **data */
	/* res->rowdesc->num_attrs * res->numrows */
	for (i = 0; i < res->rowdesc->num_attrs * res->numrows; i++)
	{
		if(res->nullflags[i] > 0)
		{
			mysize += res->nullflags[i];
		}
	}

	/* init array */
	*size = mysize;
	data = palloc(mysize + 1);

	/* data copy */
	p = data;
	memcpy(p, &res->rowdesc->num_attrs, sizeof(int));
	p += sizeof(int);
	memcpy(p, &res->numrows, sizeof(int));
	p += sizeof(int);

	array_size = res->rowdesc->num_attrs * res->numrows;

	memcpy(p, res->nullflags, sizeof(int) * array_size);
	p += sizeof(int) * array_size;

	for (i = 0; i < array_size; i++)
	{
		if( res->nullflags[i] > 0) /* NOT NULL? */
		{
			memcpy(p, res->data[i], res->nullflags[i]);
			p += res->nullflags[i];
		}
	}

	return data;
}
