/* -*-pgsql-c-*- */
/*
 *
 * $Header$
 *
 * pgpool: a language independent connection pool server for PostgreSQL
 * written by Tatsuo Ishii
 *
 * Copyright (c) 2003-2012	PgPool Global Development Group
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
 * pool_select_walker.h.: Walker functions for SELECT
 *
 */

#ifndef POOL_SELECT_WALKER_H
#define POOL_SELECT_WALKER_H

#include "pool.h"
#include "parser/nodes.h"
#include "parser/primnodes.h"
#include "parser/makefuncs.h"

#define POOL_MAX_SELECT_OIDS 128
#define POOL_NAMEDATALEN 64		/* from NAMEDATALEN of PostgreSQL */

typedef struct
{
	bool		has_system_catalog; /* True if system catalog table is used */
	bool		has_temp_table; /* True if temporary table is used */
	bool		has_unlogged_table; /* True if unlogged table is used */
	bool		has_view;		/* True if view is used */
	bool		has_function_call;	/* True if write function call is used */
	int			pg_terminate_backend_pid;	/* pid argument of
											 * pg_terminate_backend_call(if
											 * used) */
	bool		has_non_immutable_function_call;	/* True if non immutable
													 * functions are used */
	bool		has_insertinto_or_locking_clause;	/* True if it has SELECT
													 * INTO or FOR
													 * SHARE/UPDATE */
	int			num_oids;		/* number of oids */
	int			table_oids[POOL_MAX_SELECT_OIDS];	/* table oids */
	char		table_names[POOL_MAX_SELECT_OIDS][POOL_NAMEDATALEN];	/* table names */
}			SelectContext;

extern int	pool_get_terminate_backend_pid(Node *node);
extern bool pool_has_function_call(Node *node);
extern bool pool_has_non_immutable_function_call(Node *node);
extern bool pool_has_system_catalog(Node *node);
extern bool pool_has_temp_table(Node *node);
extern void discard_temp_table_relcache(void);
extern bool pool_has_unlogged_table(Node *node);
extern bool pool_has_view(Node *node);
extern bool pool_has_insertinto_or_locking_clause(Node *node);
extern bool pool_has_pgpool_regclass(void);
extern bool pool_has_to_regclass(void);
extern bool raw_expression_tree_walker(Node *node, bool (*walker) (), void *context);
extern int	pool_table_name_to_oid(char *table_name);
extern int	pool_extract_table_oids_from_select_stmt(Node *node, SelectContext * ctx);
extern RangeVar *makeRangeVarFromNameList(List *names);
extern char *make_table_name_from_rangevar(RangeVar *rangevar);
extern char *make_function_name_from_funccall(FuncCall *fcall);
extern int	pattern_compare(char *str, const int type, const char *param_name);
extern bool is_unlogged_table(char *table_name);
extern bool is_view(char *table_name);

#endif							/* POOL_SELECT_WALKER_H */
