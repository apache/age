/* -*-pgsql-c-*- */
/*
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
 */
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "pool.h"
#include "utils/elog.h"
#include "pool_config.h"
#include "utils/pool_select_walker.h"
#include "utils/pool_relcache.h"
#include "parser/parsenodes.h"
#include "context/pool_session_context.h"
#include "rewrite/pool_timestamp.h"
#include "protocol/pool_pg_utils.h"

/*
 * Possible argument (property) values for function_volatile_property
 */
typedef enum {
	FUNC_VOLATILE,
	FUNC_STABLE,
	FUNC_IMMUTABLE
} FUNC_VOLATILE_PROPERTY;

static bool function_call_walker(Node *node, void *context);
static bool system_catalog_walker(Node *node, void *context);
static bool is_system_catalog(char *table_name);
static bool temp_table_walker(Node *node, void *context);
static bool unlogged_table_walker(Node *node, void *context);
static bool view_walker(Node *node, void *context);
static bool is_temp_table(char *table_name);
static bool insertinto_or_locking_clause_walker(Node *node, void *context);
static bool is_immutable_function(char *fname);
static bool select_table_walker(Node *node, void *context);
static bool non_immutable_function_call_walker(Node *node, void *context);
static char *strip_quote(char *str);
static bool function_volatile_property(char *fname, FUNC_VOLATILE_PROPERTY property);
static bool function_has_return_type(char *fname, char *typename);

/*
 * Return true if this SELECT has function calls *and* supposed to
 * modify database.  We check write/read_only function list to determine
 * whether the function modifies database.
 */
bool
pool_has_function_call(Node *node)
{
	SelectContext ctx;

	if (!IsA(node, SelectStmt))
		return false;

	ctx.has_function_call = false;
	ctx.pg_terminate_backend_pid = -1;

	raw_expression_tree_walker(node, function_call_walker, &ctx);

	return ctx.has_function_call;
}

/*
 * Search the pg_terminate_backend() call in the query
 */
int
pool_get_terminate_backend_pid(Node *node)
{
	SelectContext ctx;

	if (!IsA(node, SelectStmt))
		return false;

	ctx.has_function_call = false;
	ctx.pg_terminate_backend_pid = 0;

	raw_expression_tree_walker(node, function_call_walker, &ctx);

	return ctx.pg_terminate_backend_pid;

}

/*
 * Return true if this SELECT has system catalog table.
 */
bool
pool_has_system_catalog(Node *node)
{

	SelectContext ctx;

	if (!IsA(node, SelectStmt))
		return false;

	ctx.has_system_catalog = false;

	raw_expression_tree_walker(node, system_catalog_walker, &ctx);

	return ctx.has_system_catalog;
}

/*
 * Return true if this SELECT has temporary table.
 */
bool
pool_has_temp_table(Node *node)
{

	SelectContext ctx;

	if (!IsA(node, SelectStmt))
		return false;

	ctx.has_temp_table = false;

	raw_expression_tree_walker(node, temp_table_walker, &ctx);

	return ctx.has_temp_table;
}

/*
 * Return true if this SELECT has unlogged table.
 */
bool
pool_has_unlogged_table(Node *node)
{

	SelectContext ctx;

	if (!IsA(node, SelectStmt))
		return false;

	ctx.has_unlogged_table = false;

	raw_expression_tree_walker(node, unlogged_table_walker, &ctx);

	return ctx.has_unlogged_table;
}

/*
 * Return true if this SELECT has a view.
 */
bool
pool_has_view(Node *node)
{

	SelectContext ctx;

	if (!IsA(node, SelectStmt))
		return false;

	ctx.has_view = false;

	raw_expression_tree_walker(node, view_walker, &ctx);

	return ctx.has_view;
}

/*
 * Return true if this SELECT has INSERT INTO or FOR SHARE or FOR UPDATE.
 */
bool
pool_has_insertinto_or_locking_clause(Node *node)
{
	SelectContext ctx;

	if (!IsA(node, SelectStmt))
		return false;

	ctx.has_insertinto_or_locking_clause = false;

	raw_expression_tree_walker(node, insertinto_or_locking_clause_walker, &ctx);

	ereport(DEBUG1,
			(errmsg("checking if query has INSERT INTO, FOR SHARE or FOR UPDATE"),
			 errdetail("result = %d", ctx.has_insertinto_or_locking_clause)));

	return ctx.has_insertinto_or_locking_clause;
}

/*
 * Search function name in readonlylist or writelist regex array
 * Return 1 on success (found in list)
 * Return 0 when not found in list
 * Return -1 if the given search type doesn't exist.
 * Search type supported are: READONLYLIST and WRITELIST
 */
int
pattern_compare(char *str, const int type, const char *param_name)
{
	int			i = 0;
	char	   *s;
	int			result = 0;

	RegPattern *lists_patterns;
	int		   *pattc;

	if (strcmp(param_name, "read_only_function_list") == 0 ||
		strcmp(param_name, "write_function_list") == 0)
	{
		lists_patterns = pool_config->lists_patterns;
		pattc = &pool_config->pattc;

	}
	else if (strcmp(param_name, "cache_safe_memqcache_table_list") == 0 ||
			 strcmp(param_name, "cache_unsafe_memqcache_table_list") == 0)
	{
		lists_patterns = pool_config->lists_memqcache_table_patterns;
		pattc = &pool_config->memqcache_table_pattc;

	}
	else if (strcmp(param_name, "primary_routing_query_pattern_list") == 0)
	{
		lists_patterns = pool_config->lists_query_patterns;
		pattc = &pool_config->query_pattc;

	}
	else
	{
		ereport(WARNING,
				(errmsg("pattern_compare: unknown paramname %s", param_name)));
		return -1;
	}

	s = strip_quote(str);
	if (!s)
	{
		elog(WARNING, "pattern_compare: strip_quote() returns error");
		return -1;
	}

	for (i = 0; i < *pattc; i++)
	{
		if (lists_patterns[i].type != type)
			continue;

		if (regexec(&lists_patterns[i].regexv, s, 0, 0, 0) == 0)
		{
			switch (type)
			{
					/* return 1 if string matches readonly list pattern */
				case READONLYLIST:
					ereport(DEBUG2,
							(errmsg("comparing function name in readonly list regex array"),
							 errdetail("pattern_compare: %s (%s) matched: %s",
									   param_name, lists_patterns[i].pattern, s)));
					result = 1;
					break;
					/* return 1 if string matches writelist pattern */
				case WRITELIST:
					ereport(DEBUG2,
							(errmsg("comparing function name in writelist regex array"),
							 errdetail("pattern_compare: %s (%s) matched: %s",
									   param_name, lists_patterns[i].pattern, s)));
					result = 1;
					break;
				default:
					ereport(WARNING,
							(errmsg("pattern_compare: \"%s\" unknown pattern match type: \"%s\"", param_name, s)));
					result = -1;
					break;
			}
			/* return the result */
			break;
		}
		ereport(DEBUG2,
				(errmsg("comparing function name in write/readonly list regex array"),
				 errdetail("pattern_compare: %s (%s) not matched: %s",
						   param_name, lists_patterns[i].pattern, s)));
	}

	free(s);
	return result;
}

/*
 * Returns double quotes stripped version of malloced string.
 * Callers must free() after using it.
 * Returns NULL on error.
 */
static char *
strip_quote(char *str)
{
	char	   *after;
	int		   len;
	int			i = 0;

	len = strlen(str);
	after = malloc(sizeof(char) * len + 1);
	if (!after)
	{
		return NULL;
	}

	if (len == 0)
	{
		/* empty string case */
		*after = '\0';
		return after;
	}

	do
	{
		if (*str != '"')
		{
			after[i] = *str;
			i++;
		}
		str++;
	} while (*str != '\0');

	after[i] = '\0';

	return after;
}

/*
 * Walker function to find a function call which is supposed to write
 * database.
 */
static bool
function_call_walker(Node *node, void *context)
{
	SelectContext *ctx = (SelectContext *) context;

	if (node == NULL)
		return false;

	if (IsA(node, FuncCall))
	{
		FuncCall   *fcall = (FuncCall *) node;
		char	   *fname;
		int			length = list_length(fcall->funcname);

		if (length > 0)
		{
			fname = make_function_name_from_funccall(fcall);

			ereport(DEBUG1,
					(errmsg("function call walker, function name: \"%s\"", fname)));

			check_object_relationship_list(strVal(llast(fcall->funcname)), true);

			if (ctx->pg_terminate_backend_pid == 0 && strcmp("pg_terminate_backend", strVal(llast(fcall->funcname))) == 0)
			{
				/* PostgreSQL 14 or after accept two arguments. */
				if (list_length(fcall->args) == 1 ||
					list_length(fcall->args) == 2)
				{
					Node	   *arg = linitial(fcall->args);

					if (IsA(arg, A_Const) &&
						IsA(&((A_Const *) arg)->val, Integer))
					{
						ctx->pg_terminate_backend_pid = ((A_Const *) arg)->val.ival.ival;
						ereport(DEBUG1,
								(errmsg("pg_terminate_backend pid = %d", ctx->pg_terminate_backend_pid)));
					}
				}
			}

			/*
			 * If both read_only_function_list and write_function_list is empty,
			 * check volatile property of the function in the system catalog.
			 */
			if (pool_config->num_read_only_function_list == 0 &&
				pool_config->num_write_function_list == 0)
			{
				if (function_volatile_property(fname, FUNC_VOLATILE))
				{
					ctx->has_function_call = true;
					return false;
				}
				return raw_expression_tree_walker(node, function_call_walker, context);
			}

			/*
			 * Check read_only list if any.
			 */
			if (pool_config->num_read_only_function_list > 0)
			{
				/* Search function in the read_only list regex patterns */
				if (pattern_compare(fname, READONLYLIST, "read_only_function_list") == 1)
				{
					/*
					 * If the function is found in the read_only list, we can
					 * ignore it
					 */
					return raw_expression_tree_walker(node, function_call_walker, context);
				}

				/*
				 * Since the function was not found in read_only list, we have
				 * found a writing function.
				 */
				ctx->has_function_call = true;
				return false;
			}

			/*
			 * Check write list if any.
			 */
			if (pool_config->num_write_function_list > 0)
			{
				/* Search function in the write list regex patterns */
				if (pattern_compare(fname, WRITELIST, "write_function_list") == 1)
				{
					/* Found. */
					ctx->has_function_call = true;
					return false;
				}
			}
		}
	}
	return raw_expression_tree_walker(node, function_call_walker, context);
}

/*
 * Walker function to find a system catalog
 */
static bool
system_catalog_walker(Node *node, void *context)
{
	SelectContext *ctx = (SelectContext *) context;

	if (node == NULL)
		return false;

	if (IsA(node, RangeVar))
	{
		RangeVar   *rgv = (RangeVar *) node;

		ereport(DEBUG1,
				(errmsg("system catalog walker, checking relation \"%s\"", rgv->relname)));

		if (is_system_catalog(rgv->relname))
		{
			ctx->has_system_catalog = true;
			return false;
		}
	}
	return raw_expression_tree_walker(node, system_catalog_walker, context);
}

/*
 * Walker function to find a temp table
 */
static bool
temp_table_walker(Node *node, void *context)
{
	SelectContext *ctx = (SelectContext *) context;

	if (node == NULL)
		return false;

	if (IsA(node, RangeVar))
	{
		RangeVar   *rgv = (RangeVar *) node;

		ereport(DEBUG1,
				(errmsg("temporary table walker. checking relation \"%s\"", rgv->relname)));

		if (is_temp_table(rgv->relname))
		{
			ctx->has_temp_table = true;
			return false;
		}
	}
	return raw_expression_tree_walker(node, temp_table_walker, context);
}

/*
 * Walker function to find a unlogged table
 */
static bool
unlogged_table_walker(Node *node, void *context)
{
	SelectContext *ctx = (SelectContext *) context;
	char	   *relname;

	if (node == NULL)
		return false;

	if (IsA(node, RangeVar))
	{
		RangeVar   *rgv = (RangeVar *) node;

		relname = make_table_name_from_rangevar(rgv);

		ereport(DEBUG1,
				(errmsg("unlogged table walker. checking relation \"%s\"", relname)));

		if (is_unlogged_table(relname))
		{
			ctx->has_unlogged_table = true;
			return false;
		}
	}
	return raw_expression_tree_walker(node, unlogged_table_walker, context);
}

/*
 * Walker function to find a view
 */
static bool
view_walker(Node *node, void *context)
{
	SelectContext *ctx = (SelectContext *) context;
	char	   *relname;

	if (node == NULL)
		return false;

	if (IsA(node, RangeVar))
	{
		RangeVar   *rgv = (RangeVar *) node;

		relname = make_table_name_from_rangevar(rgv);

		ereport(DEBUG1,
				(errmsg("view walker. checking relation \"%s\"", relname)));

		if (is_view(relname))
		{
			ctx->has_view = true;
			return false;
		}
	}
	return raw_expression_tree_walker(node, view_walker, context);
}

/*
 * Determine whether table_name is a system catalog or not.
 */
static bool
is_system_catalog(char *table_name)
{
/*
 * Query to know if pg_namespace exists. PostgreSQL 7.2 or before doesn't have.
 */
#define HASPGNAMESPACEQUERY "SELECT count(*) FROM pg_catalog.pg_class AS c WHERE c.relname = '%s'"

/*
 * Query to know if the target table belongs pg_catalog schema.
 */
#define ISBELONGTOPGCATALOGQUERY "SELECT count(*) FROM pg_class AS c, pg_namespace AS n WHERE c.relname = '%s' AND c.relnamespace = n.oid AND n.nspname = 'pg_catalog'"

#define ISBELONGTOPGCATALOGQUERY2 "SELECT count(*) FROM pg_class AS c, pg_namespace AS n WHERE c.oid = pgpool_regclass('\"%s\"') AND c.relnamespace = n.oid AND n.nspname = 'pg_catalog'"

#define ISBELONGTOPGCATALOGQUERY3 "SELECT count(*) FROM pg_class AS c, pg_namespace AS n WHERE c.oid = pg_catalog.to_regclass('\"%s\"') AND c.relnamespace = n.oid AND n.nspname = 'pg_catalog'"

	bool		result;
	static POOL_RELCACHE * relcache;
	POOL_CONNECTION_POOL *backend;

	if (table_name == NULL)
	{
		return false;
	}

	if (!pool_has_to_regclass() && !pool_has_pgpool_regclass())
		table_name = remove_quotes_and_schema_from_relname(table_name);

	backend = pool_get_session_context(false)->backend;

	/*
	 * Check if pg_namespace exists. PostgreSQL 7.3 or later has it.
	 */
	if (Pgversion(backend)->major >= 73)
	{
		/*
		 * If relcache does not exist, create it.
		 */
		if (!relcache)
		{
			char	   *query;

			/* PostgreSQL 9.4 or later has to_regclass() */
			if (pool_has_to_regclass())
			{
				query = ISBELONGTOPGCATALOGQUERY3;
			}
			/* pgpool_regclass has been installed */
			else if (pool_has_pgpool_regclass())
			{
				query = ISBELONGTOPGCATALOGQUERY2;
			}
			else
			{
				query = ISBELONGTOPGCATALOGQUERY;
			}

			relcache = pool_create_relcache(pool_config->relcache_size, query,
											int_register_func, int_unregister_func,
											false);
			if (relcache == NULL)
			{
				ereport(WARNING,
						(errmsg("unable to create relcache, while checking for system catalog")));
				return false;
			}
		}

		/*
		 * Search relcache.
		 */
		result = pool_search_relcache(relcache, backend, table_name) == 0 ? false : true;
		return result;
	}

	/*
	 * Pre 7.3. Just check whether the table starts with "pg_".
	 */
	return (strcasecmp(table_name, "pg_") == 0);
}

/*
 * Returns true if table_name is a temporary table.
 */
static POOL_RELCACHE * is_temp_table_relcache;

static bool
is_temp_table(char *table_name)
{
/*
 * Query to know if pg_class has relistemp column or not.
 * PostgreSQL 8.4 and 9.0 have this.
 */
#define HASRELITEMPPQUERY "SELECT count(*) FROM pg_catalog.pg_class AS c, pg_attribute AS a WHERE c.relname = 'pg_class' AND a.attrelid = c.oid AND a.attname = 'relistemp'"

/*
 * Query to know if the target table is a temporary one.  This query
 * is valid in PostgreSQL 7.3 to 8.3 and 9.1 or later.  We do not use
 * regclass (or its variant) here, because temporary tables never have
 * schema qualified name.
 */
#define ISTEMPQUERY83 "SELECT count(*) FROM pg_catalog.pg_class AS c, pg_namespace AS n WHERE c.relname = '%s' AND c.relnamespace = n.oid AND n.nspname ~ '^pg_temp_'"

/*
 * Query to know if the target table is a temporary one.  This query
 * is valid in PostgreSQL 8.4 and 9.0. We do not use regclass (or its
 * variant) here, because temporary tables never have schema qualified
 * name.
 */
#define ISTEMPQUERY84 "SELECT count(*) FROM pg_catalog.pg_class AS c WHERE c.relname = '%s' AND c.relistemp"
	bool		result;
	char	   *query;
	POOL_CONNECTION_POOL *backend;
	int			major;

	if (table_name == NULL || pool_config->check_temp_table == CHECK_TEMP_NONE ||
		pool_config->check_temp_table == CHECK_TEMP_OFF)
	{
		return false;
	}

	backend = pool_get_session_context(false)->backend;

	if (pool_config->check_temp_table == CHECK_TEMP_TRACE)
	{
		POOL_TEMP_TABLE	*temp_table;

		temp_table = pool_temp_tables_find(table_name);
		if (temp_table && (temp_table->state == TEMP_TABLE_CREATE_COMMITTED ||
						   temp_table->state == TEMP_TABLE_CREATING))
		{
			ereport(DEBUG1,
					(errmsg("is_temp_table: %s is a temp table", table_name)));
			return true;
		}
		else
		{
			ereport(DEBUG1,
					(errmsg("is_temp_table: %s is not a temp table", table_name)));
			return false;
		}
	}

	/*
	 * Below is check_temp_table == CHECK_TEMP_CATALOG or CHECK_TEMP_ON case.
	 */

	/*
	 * Check backend version.
	 */
	major = Pgversion(backend)->major;
	if (major == 73 || major == 83 || major >= 91)
		query = ISTEMPQUERY83;
	else if (major == 84 || major == 90)
		query = ISTEMPQUERY84;
	else
	{
		ereport(WARNING,
				(errmsg("is_temp_table: unexpected PostgreSQL version: %s", Pgversion(backend)->version_string)));
		return false;	/* fall back to assume that the table is not a temporary table. */
	}

	/*
	 * If relcache does not exist, create it.
	 */
	if (!is_temp_table_relcache)
	{
		is_temp_table_relcache = pool_create_relcache(pool_config->relcache_size, query,
													  int_register_func, int_unregister_func,
													  true);
		if (is_temp_table_relcache == NULL)
		{
			ereport(WARNING,
					(errmsg("unable to create relcache, while checking for temporary table")));
			return false;
		}
	}

	/*
	 * Search relcache.
	 */
	result = pool_search_relcache(is_temp_table_relcache, backend, table_name) == 0 ? false : true;
	return result;
}

/*
 * Discard relcache used by is_temp_table_relcache().
 */
void
discard_temp_table_relcache(void)
{
	if (is_temp_table_relcache)
	{
		pool_discard_relcache(is_temp_table_relcache);
		is_temp_table_relcache = NULL;
	}
}

/*
 * Returns true if table_name is an unlogged table.
 */
bool
is_unlogged_table(char *table_name)
{
/*
 * Query to know if pg_class has relpersistence column or not.
 * PostgreSQL 9.1 or later has this.
 */
#define HASRELPERSISTENCEQUERY "SELECT count(*) FROM pg_catalog.pg_class AS c, pg_catalog.pg_attribute AS a WHERE c.relname = 'pg_class' AND a.attrelid = c.oid AND a.attname = 'relpersistence'"

/*
 * Query to know if the target table is a unlogged one.  This query
 * is valid in PostgreSQL 9.1 or later.
 */
#define ISUNLOGGEDQUERY "SELECT count(*) FROM pg_catalog.pg_class AS c WHERE c.relname = '%s' AND c.relpersistence = 'u'"

#define ISUNLOGGEDQUERY2 "SELECT count(*) FROM pg_catalog.pg_class AS c WHERE c.oid = pgpool_regclass('%s') AND c.relpersistence = 'u'"

#define ISUNLOGGEDQUERY3 "SELECT count(*) FROM pg_catalog.pg_class AS c WHERE c.oid = pg_catalog.to_regclass('%s') AND c.relpersistence = 'u'"

	static POOL_RELCACHE * relcache;
	POOL_CONNECTION_POOL *backend;
	int		major;

	if (table_name == NULL)
	{
		return false;
	}

	if (!pool_has_to_regclass() && !pool_has_pgpool_regclass())
		table_name = remove_quotes_and_schema_from_relname(table_name);

	backend = pool_get_session_context(false)->backend;

	/*
	 * Check backend version.
	 */
	major = Pgversion(backend)->major;
	if (major >= 91)
	{
		bool		result;
		char	   *query;

		/* PostgreSQL 9.4 or later has to_regclass() */
		if (pool_has_to_regclass())
		{
			query = ISUNLOGGEDQUERY3;
		}
		/* pgpool_regclass has been installed */
		else if (pool_has_pgpool_regclass())
		{
			query = ISUNLOGGEDQUERY2;
		}
		else
		{
			query = ISUNLOGGEDQUERY;
		}

		/*
		 * If relcache does not exist, create it.
		 */
		if (!relcache)
		{
			relcache = pool_create_relcache(pool_config->relcache_size, query,
											int_register_func, int_unregister_func,
											false);
			if (relcache == NULL)
			{
				ereport(WARNING,
						(errmsg("unable to create relcache, while checking for unlogged table")));
				return false;
			}
		}

		/*
		 * Search relcache.
		 */
		result = pool_search_relcache(relcache, backend, table_name) == 0 ? false : true;
		return result;
	}
	else
	{
		return false;
	}
}

/*
 * Returns true if table_name is a view.
 * This function is called by query cache module.
 */
bool
is_view(char *table_name)
{
/*
 * Query to know if the target table is a view (including a materialized view).
 */
#define ISVIEWQUERY "SELECT count(*) FROM pg_catalog.pg_class AS c WHERE c.relname = '%s' AND (c.relkind = 'v' OR c.relkind = 'm')"

#define ISVIEWQUERY2 "SELECT count(*) FROM pg_catalog.pg_class AS c WHERE c.oid = pgpool_regclass('%s') AND (c.relkind = 'v' OR c.relkind = 'm')"

#define ISVIEWQUERY3 "SELECT count(*) FROM pg_catalog.pg_class AS c WHERE c.oid = pg_catalog.to_regclass('%s') AND (c.relkind = 'v' OR c.relkind = 'm')"

	static POOL_RELCACHE * relcache;
	POOL_CONNECTION_POOL *backend;
	bool		result;
	char	   *query;

	if (table_name == NULL)
	{
		return false;
	}

	if (!pool_has_to_regclass() && !pool_has_pgpool_regclass())
		table_name = remove_quotes_and_schema_from_relname(table_name);

	backend = pool_get_session_context(false)->backend;

	/* PostgreSQL 9.4 or later has to_regclass() */
	if (pool_has_to_regclass())
	{
		query = ISVIEWQUERY3;
	}
	/* pgpool_regclass has been installed */
	else if (pool_has_pgpool_regclass())
	{
		query = ISVIEWQUERY2;
	}
	else
	{
		query = ISVIEWQUERY;
	}

	if (!relcache)
	{
		relcache = pool_create_relcache(pool_config->relcache_size, query,
										int_register_func, int_unregister_func,
										false);
		if (relcache == NULL)
		{
			ereport(WARNING,
					(errmsg("unable to create relcache, while checking for view")));
			return false;
		}

	}

	/*
	 * Search relcache.
	 */
	result = pool_search_relcache(relcache, backend, table_name) == 0 ? false : true;
	return result;
}

/*
 * Judge if we have pgpool_regclass or not.
 */
bool
pool_has_pgpool_regclass(void)
{
/*
 * Query to know if pgpool_regclass exists.
 */
#define HASPGPOOL_REGCLASSQUERY "SELECT count(*) from (SELECT has_function_privilege('%s', 'pgpool_regclass(cstring)', 'execute') WHERE EXISTS(SELECT * FROM pg_catalog.pg_proc AS p WHERE p.proname = 'pgpool_regclass')) AS s"

	bool		result;
	static POOL_RELCACHE * relcache;
	POOL_CONNECTION_POOL *backend;
	char	   *user;

	backend = pool_get_session_context(false)->backend;
	user = MAIN_CONNECTION(backend)->sp->user;

	if (!relcache)
	{
		relcache = pool_create_relcache(pool_config->relcache_size, HASPGPOOL_REGCLASSQUERY,
										int_register_func, int_unregister_func,
										false);
		if (relcache == NULL)
		{
			ereport(WARNING,
					(errmsg("unable to create relcache, while checking pgpool regclass presence")));
			return false;
		}
	}

	result = pool_search_relcache(relcache, backend, user) == 0 ? 0 : 1;
	return result;
}

/*
 * Returns true if we have to_regclass().
 */
bool
pool_has_to_regclass(void)
{
	POOL_CONNECTION_POOL *backend;
	PGVersion	*pgversion;

	backend = pool_get_session_context(false)->backend;
	pgversion = Pgversion(backend);

	/* PostgreSQL 9.4 or above has to_regclass() */
	if (pgversion->major >= 94)
		return true;

	return false;
}

/*
 * Walker function to find intoClause or lockingClause.
 */
static bool
insertinto_or_locking_clause_walker(Node *node, void *context)
{
	SelectContext *ctx = (SelectContext *) context;

	if (node == NULL)
		return false;

	if (IsA(node, IntoClause) ||IsA(node, LockingClause))
	{
		ctx->has_insertinto_or_locking_clause = true;
		return false;
	}
	return raw_expression_tree_walker(node, insertinto_or_locking_clause_walker, ctx);
}

/*
 * Return true if this SELECT has non immutable function calls.
 */
bool
pool_has_non_immutable_function_call(Node *node)
{
	SelectContext ctx;

	if (!IsA(node, SelectStmt))
		return false;

	ctx.has_non_immutable_function_call = false;

	raw_expression_tree_walker(node, non_immutable_function_call_walker, &ctx);

	ereport(DEBUG1,
			(errmsg("checking if SELECT statement contains the IMMUTABLE function call"),
			 errdetail("result = %d", ctx.has_non_immutable_function_call)));

	return ctx.has_non_immutable_function_call;
}

/*
 * Walker function to find non immutable function call.
 */
static bool
non_immutable_function_call_walker(Node *node, void *context)
{
	SelectContext *ctx = (SelectContext *) context;

	if (node == NULL)
		return false;

	if (IsA(node, FuncCall))
	{
		FuncCall   *fcall = (FuncCall *) node;
		char	   *fname;
		int			length = list_length(fcall->funcname);

		if (length > 0)
		{
			fname = make_function_name_from_funccall(fcall);

			ereport(DEBUG1,
					(errmsg("non immutable function walker. checking function \"%s\"", fname)));

			/* Check system catalog if the function is immutable */
			if (is_immutable_function(fname) == false)
			{
				/* Non immutable function call found */
				ctx->has_non_immutable_function_call = true;
				return false;
			}

			/* check return type is timestamptz */
			if (function_has_return_type(fname, "timestamptz"))
			{
				/* timestamptz should not be cached */
				ctx->has_non_immutable_function_call = true;
				return false;
			}

			/* return type is timetz */
			if (function_has_return_type(fname, "timetz"))
			{
				/* timetz should not be cached */
				ctx->has_non_immutable_function_call = true;
				return false;
			}
		}
	}

	/* Check type cast */
	else if (IsA(node, TypeCast))
	{
		/* TIMESTAMP WITH TIME ZONE and TIME WITH TIME ZONE should not be cached. */
		TypeCast   *tc = (TypeCast *) node;

		if (isSystemType((Node *) tc->typeName, "timestamptz") ||
			isSystemType((Node *) tc->typeName, "timetz"))
		{
			ctx->has_non_immutable_function_call = true;
			return false;
		}
	}
	else if (IsA(node, SQLValueFunction))
	{
		/*
		 * SQLValueFunctions (CURRENT_TIME, CURRENT_USER etc.) are regarded as
		 * non immutable functions.
		 */
		ctx->has_non_immutable_function_call = true;
		return false;
	}

	return raw_expression_tree_walker(node, non_immutable_function_call_walker, context);
}

/*
 * Check if the function is stable.
 */
static bool
is_immutable_function(char *fname)
{
	return function_volatile_property(fname, FUNC_IMMUTABLE);
}

/*
 * Check volatile property of function specified by the name.
 * If the function property is match with "property" argument, returns true.
 * Note that "fname" can be schema qualified.
 */
static
bool function_volatile_property(char *fname, FUNC_VOLATILE_PROPERTY property)
{
/*
 * Query to know if function's volatile property.
 */
#define VOLATILE_FUNCTION_QUERY "SELECT count(*) FROM pg_catalog.pg_proc AS p, pg_catalog.pg_namespace AS n WHERE p.proname = '%s' AND n.oid = p.pronamespace AND n.nspname %s '%s' AND p.provolatile = '%c'"
	bool		result;
	char		query[1024];
	char	   *rawstring = NULL;
	List	   *names = NIL;
	POOL_CONNECTION_POOL   *backend;
	static POOL_RELCACHE   *relcache;
	char	prop_volatile;

	/* We need a modifiable copy of the input string. */
	rawstring = pstrdup(fname);

	/* split "schemaname.funcname" */
	if(!SplitIdentifierString(rawstring, '.', (Node **) &names) ||
		names == NIL)
	{
		pfree(rawstring);
		list_free(names);

		ereport(WARNING,
				(errmsg("invalid function name %s", fname)));

		return false;
	}

	/*
	 * Get volatile property character.
	 */
	switch (property)
	{
		case FUNC_STABLE:
			prop_volatile = 's';
			break;

		case FUNC_IMMUTABLE:
			prop_volatile = 'i';
			break;

		default:
			prop_volatile = 'v';
			break;
	}

	/* with schema qualification */
	if(list_length(names) == 2)
	{
		snprintf(query, sizeof(query), VOLATILE_FUNCTION_QUERY, (char *) llast(names),
				 "=", (char *) linitial(names), prop_volatile);
	}
	else
	{
		snprintf(query, sizeof(query), VOLATILE_FUNCTION_QUERY, (char *) llast(names),
				 "~", ".*", prop_volatile);
	}

	backend = pool_get_session_context(false)->backend;

	if (!relcache)
	{
		/*
		 * We pass "%s" as a template query so that pool_search_relcache
		 * passes whole query.
		 */
		relcache = pool_create_relcache(pool_config->relcache_size, "%s",
										int_register_func, int_unregister_func,
										false);
		if (relcache == NULL)
		{
			pfree(rawstring);
			list_free(names);

			ereport(WARNING,
					(errmsg("unable to create relcache, while checking the function volatile property")));
			return false;
		}
		ereport(DEBUG1,
				(errmsg("checking the function volatile property"),
				 errdetail("relcache created")));
	}

	/*
	 * We pass whole query as "table" parameter of pool_search_relcache so
	 * that each relcache entry is distinguished by actual query string.
	 */
	result = (pool_search_relcache(relcache, backend, query) == 0) ? 0 : 1;

	pfree(rawstring);
	list_free(names);

	ereport(DEBUG1,
			(errmsg("checking the function volatile property"),
			 errdetail("search result = %d (%c)", result, prop_volatile)));
	return result;
}

/*
 * Convert table_name(possibly including schema name) to oid
 */
int
pool_table_name_to_oid(char *table_name)
{
/*
 * Query to convert table name to oid
 */
#define TABLE_TO_OID_QUERY "SELECT pgpool_regclass('%s')"
#define TABLE_TO_OID_QUERY2 "SELECT oid FROM pg_class WHERE relname = '%s'"
#define TABLE_TO_OID_QUERY3 "SELECT COALESCE(pg_catalog.to_regclass('%s')::oid, 0)"

	int			oid = 0;
	static POOL_RELCACHE * relcache;
	POOL_CONNECTION_POOL *backend;
	char	   *query;

	if (table_name == NULL)
	{
		return oid;
	}

	if (!pool_has_to_regclass() && !pool_has_pgpool_regclass())
		table_name = remove_quotes_and_schema_from_relname(table_name);

	backend = pool_get_session_context(false)->backend;

	if (pool_has_to_regclass())
	{
		query = TABLE_TO_OID_QUERY3;
	}
	else if (pool_has_pgpool_regclass())
	{
		query = TABLE_TO_OID_QUERY;
	}
	else
	{
		query = TABLE_TO_OID_QUERY2;
	}

	/*
	 * If relcache does not exist, create it.
	 */
	if (!relcache)
	{
		relcache = pool_create_relcache(pool_config->relcache_size, query,
										int_register_func, int_unregister_func,
										true);
		if (relcache == NULL)
		{
			ereport(WARNING,
					(errmsg("unable to create relcache, getting OID from table name")));
			return oid;
		}

		/*
		 * Se do not cache if pgpool_regclass() returns 0, which indicates
		 * there's no such a table. In this case we do not want to cache the
		 * state because the table might be created later in this session.
		 */
		relcache->no_cache_if_zero = true;
	}

	/*
	 * Search relcache.
	 */
	oid = (int) (intptr_t) pool_search_relcache(relcache, backend, table_name);
	return oid;
}

/*
 * Extract table oids from SELECT statement. Returns number of oids.
 * Oids are returned as an int array. The contents of oid array are
 * discarded by next call to this function.
 */
int
pool_extract_table_oids_from_select_stmt(Node *node, SelectContext * ctx)
{
	if (!node)
		return 0;
	if (!IsA(node, SelectStmt))
		return 0;

	ctx->num_oids = 0;
	raw_expression_tree_walker(node, select_table_walker, ctx);

	return ctx->num_oids;
}

/*
 * Walker function to extract table oids from SELECT statement.
 */
static bool
select_table_walker(Node *node, void *context)
{
	SelectContext *ctx = (SelectContext *) context;
	int			num_oids;

	if (node == NULL)
		return false;

	if (IsA(node, RangeVar))
	{
		RangeVar   *rgv = (RangeVar *) node;
		char	   *table;
		int			oid;

		table = make_table_name_from_rangevar(rgv);
		oid = pool_table_name_to_oid(table);

		if (oid)
		{
			if (POOL_MAX_SELECT_OIDS <= ctx->num_oids)
			{
				ereport(DEBUG1,
						(errmsg("extracting table oids from SELECT statement"),
						 errdetail("number of oids = %d exceeds the maximum limit = %d",
								   ctx->num_oids, POOL_MAX_SELECT_OIDS)));
				return false;
			}

			num_oids = ctx->num_oids++;

			ctx->table_oids[num_oids] = oid;
			strlcpy(ctx->table_names[num_oids], table, POOL_NAMEDATALEN);

			ereport(DEBUG1,
					(errmsg("extracting table oids from SELECT statement"),
					 errdetail("ctx->table_names[%d] = \"%s\"",
							   num_oids, ctx->table_names[num_oids])));
		}
	}

	/* Skip Data-Modifying Statements in SELECT. */
	else if (IsA(node, InsertStmt) || IsA(node, DeleteStmt) || IsA(node, UpdateStmt))
	{
		return false;
	}

	return raw_expression_tree_walker(node, select_table_walker, context);
}


/*
 * makeRangeVarFromNameList
 *		Utility routine to convert a qualified-name list into RangeVar form.
 *
 * Copied from backend/catalog/namespace.c
 */
RangeVar *
makeRangeVarFromNameList(List *names)
{
	RangeVar   *rel = makeRangeVar(NULL, NULL, -1);

	switch (list_length(names))
	{
		case 1:
			rel->relname = strVal(linitial(names));
			break;
		case 2:
			rel->schemaname = strVal(linitial(names));
			rel->relname = strVal(lsecond(names));
			break;
		case 3:
			rel->catalogname = strVal(linitial(names));
			rel->schemaname = strVal(lsecond(names));
			rel->relname = strVal(lthird(names));
			break;
		default:
			ereport(WARNING,
					(errmsg("invalid relation name, too many indirections, while converting from table name to RangeVar")));
			break;
	}

	return rel;
}

/*
 * Extract function name from FuncCall.  Make schema qualification name if
 * necessary.  The returned function name is in static area. So next
 * call to this function will break previous result.
 */
char *
make_function_name_from_funccall(FuncCall *fcall)
{
	/*
	 * Function name. Max size is calculated as follows: schema
	 * name(POOL_NAMEDATALEN byte) + quotation marks for schema name(2 byte) +
	 * period(1 byte) + table name (POOL_NAMEDATALEN byte) + quotation marks
	 * for table name(2 byte) + NULL(1 byte)
	 */
	static char funcname[POOL_NAMEDATALEN * 2 + 1 + 2 * 2 + 1];
	List 	   *names;

	if(fcall == NULL)
	{
		ereport(WARNING,
				(errmsg("FuncCall argument is NULL, while getting function name from FuncCall")));
		return "";
	}

	*funcname = '\0';
	names = fcall->funcname;

	switch (list_length(names))
	{
		case 1:
			strcat(funcname, "\"");
			strncat(funcname, strVal(linitial(names)), POOL_NAMEDATALEN);
			strcat(funcname, "\"");
			break;
		case 2:
			strcat(funcname, "\"");
			strncat(funcname, strVal(linitial(names)), POOL_NAMEDATALEN);
			strcat(funcname, "\"");

			strcat(funcname, ".");

			strcat(funcname, "\"");
			strncat(funcname, strVal(lsecond(names)), POOL_NAMEDATALEN);
			strcat(funcname, "\"");
			break;
		case 3:
			strcat(funcname, "\"");
			strncat(funcname, strVal(lsecond(names)), POOL_NAMEDATALEN);
			strcat(funcname, "\"");

			strcat(funcname, ".");

			strcat(funcname, "\"");
			strncat(funcname, strVal(lthird(names)), POOL_NAMEDATALEN);
			strcat(funcname, "\"");

			break;
		default:
			ereport(WARNING,
					(errmsg("invalid function name, too many indirections, while getting function name from FuncCall")));
			break;
	}

	ereport(DEBUG1,
			(errmsg("make function name from funccall: funcname:\"%s\"", funcname)));

	return funcname;
}

/*
 * Extract table name from RageVar.  Make schema qualification name if
 * necessary.  The returned table name is in static area. So next
 * call to this function will break previous result.
 */
char *
make_table_name_from_rangevar(RangeVar *rangevar)
{
	/*
	 * Table name. Max size is calculated as follows: schema
	 * name(POOL_NAMEDATALEN byte) + quotation marks for schema name(2 byte) +
	 * period(1 byte) + table name (POOL_NAMEDATALEN byte) + quotation marks
	 * for table name(2 byte) + NULL(1 byte)
	 */
	static char tablename[POOL_NAMEDATALEN * 2 + 1 + 2 * 2 + 1];

	if (rangevar == NULL)
	{
		ereport(WARNING,
				(errmsg("RangeVar argument is NULL, while getting table name from RangeVar")));
		return "";
	}

	if (!IsA(rangevar, RangeVar))
	{
		ereport(WARNING,
				(errmsg("invalid argument, while getting table name from RangeVar")));

		return "";
	}

	*tablename = '\0';

	if (rangevar->schemaname)
	{
		strcat(tablename, "\"");
		strncat(tablename, rangevar->schemaname, POOL_NAMEDATALEN);
		strcat(tablename, "\"");
		strcat(tablename, ".");
	}

	if (!rangevar->relname)
	{
		ereport(WARNING,
				(errmsg("relname is NULL in RangeVar, while getting table name from RangeVar")));

		return "";
	}

	strcat(tablename, "\"");
	strncat(tablename, rangevar->relname, POOL_NAMEDATALEN);
	strcat(tablename, "\"");

	ereport(DEBUG1,
			(errmsg("make table name from rangevar: tablename:\"%s\"", tablename)));

	return tablename;
}

/*
 * Return whether given function has the given type name.  If one or more
 * functions match, return true.
 */
static
bool function_has_return_type(char *fname, char *typename)
{
/*
 * Query to count the number of records matching given function name and type name.
 */
#define FUNCTION_RETURN_TYPE_MATCHEING_QUERY "SELECT count(*) FROM pg_type AS t, pg_catalog.pg_proc AS p, pg_catalog.pg_namespace AS n WHERE p.proname = '%s' AND n.oid = p.pronamespace AND n.nspname %s '%s' AND p.prorettype = t.oid AND t.typname = '%s';"
	bool		result;
	char		query[1024];
	char	   *rawstring = NULL;
	List	   *names = NIL;
	POOL_CONNECTION_POOL   *backend;
	static POOL_RELCACHE   *relcache;

	/* We need a modifiable copy of the input string. */
	rawstring = pstrdup(fname);

	/* split "schemaname.funcname" */
	if(!SplitIdentifierString(rawstring, '.', (Node **) &names) ||
		names == NIL)
	{
		pfree(rawstring);
		list_free(names);

		ereport(WARNING,
				(errmsg("invalid function name %s", fname)));

		return false;
	}

	if (list_length(names) == 2)
	{
		/* with schema qualification */
		snprintf(query, sizeof(query), FUNCTION_RETURN_TYPE_MATCHEING_QUERY, (char *) llast(names),
				 "=", (char *) linitial(names), typename);
	}
	else
	{
		snprintf(query, sizeof(query), FUNCTION_RETURN_TYPE_MATCHEING_QUERY, (char *) llast(names),
				 "~", ".*", typename);
	}

	backend = pool_get_session_context(false)->backend;

	if (!relcache)
	{
		/*
		 * We pass "%s" as a template query so that pool_search_relcache
		 * passes whole query.
		 */
		relcache = pool_create_relcache(pool_config->relcache_size, "%s",
										int_register_func, int_unregister_func,
										false);
		if (relcache == NULL)
		{
			pfree(rawstring);
			list_free(names);

			ereport(WARNING,
					(errmsg("unable to create relcache, while checking the function volatile property")));
			return false;
		}
		ereport(DEBUG1,
				(errmsg("checking the function matches the given type name"),
				 errdetail("relcache created")));
	}

	/*
	 * We pass whole query as "table" parameter of pool_search_relcache so
	 * that each relcache entry is distinguished by actual query string.
	 */
	result = (pool_search_relcache(relcache, backend, query) == 0) ? 0 : 1;

	pfree(rawstring);
	list_free(names);

	ereport(DEBUG1,
			(errmsg("checking the function matches the given type name"),
			 errdetail("search result = %d (function name: %s type name: %s)", result, fname, typename)));

	return result;
}
