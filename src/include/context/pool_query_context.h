/* -*-pgsql-c-*- */
/*
 *
 * $Header$
 *
 * pgpool: a language independent connection pool server for PostgreSQL
 * written by Tatsuo Ishii
 *
 * Copyright (c) 2003-2017	PgPool Global Development Group
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
 * pool_process_context.h.: process context information
 *
 */

#ifndef POOL_QUERY_CONTEXT_H
#define POOL_QUERY_CONTEXT_H

#include "pool.h"
#include "pool_process_context.h"
#include "parser/nodes.h"
#include "parser/parsenodes.h"
#include "utils/palloc.h"
#include "query_cache/pool_memqcache.h"

/*
 * Parse state transition.
 * transition order is:
 * UNPARSED < PARSE_COMPLETE < BIND_COMPLETE < EXECUTE_COMPLETE
 */
typedef enum
{
	POOL_UNPARSED,
	POOL_PARSE_COMPLETE,
	POOL_BIND_COMPLETE,
	POOL_EXECUTE_COMPLETE
}			POOL_QUERY_STATE;

/*
 * Query context:
 * Manages per query context
 */
typedef struct
{
	char	   *original_query; /* original query string */
	char	   *rewritten_query;	/* rewritten query string if any */
	int			original_length;	/* original query length which contains
									 * '\0' */
	int			rewritten_length;	/* rewritten query length which contains
									 * '\0' if any */
	Node	   *parse_tree;		/* raw parser output if any */
	Node	   *rewritten_parse_tree;	/* rewritten raw parser output if any */
	bool		where_to_send[MAX_NUM_BACKENDS];	/* DB node map to send
													 * query */
	int         load_balance_node_id;	/* load balance node id per statement */
	int			virtual_main_node_id; /* the 1st DB node to send query */
	POOL_QUERY_STATE query_state[MAX_NUM_BACKENDS]; /* for extended query
													 * protocol */
	bool		is_cache_safe;	/* true if SELECT is safe to cache */
	POOL_TEMP_QUERY_CACHE *temp_cache;	/* temporary cache */
	bool		is_multi_statement; /* true if multi statement query */
	int			dboid;			/* DB oid which is used at DROP DATABASE */
	char	   *query_w_hex;	/* original_query with bind message hex which
								 * used for committing cache of extended query */
	bool		is_parse_error; /* if true, we could not parse the original
								 * query and parsed node is actually a dummy
								 * query. */
	int			num_original_params;	/* number of parameters in original
										 * query */
	ConnectionInfo *pg_terminate_backend_conn;

	/*
	 * pointer to the shared memory connection info object referred by
	 * pg_terminate_backend() function. we need this to reset the flag after
	 * executing the pg_terminate_backend query, especially for the case when
	 * the query gets fail on the backend.
	 */

	bool		skip_cache_commit;	/* In streaming replication mode and
									 * extended query, do not commit cache if
									 * this flag is true. */

	MemoryContext memory_context;	/* memory context for query context */
}			POOL_QUERY_CONTEXT;

extern POOL_QUERY_CONTEXT * pool_init_query_context(void);
extern void pool_query_context_destroy(POOL_QUERY_CONTEXT * query_context);
extern POOL_QUERY_CONTEXT * pool_query_context_shallow_copy(POOL_QUERY_CONTEXT * query_context);
extern void pool_start_query(POOL_QUERY_CONTEXT * query_context, char *query, int len, Node *node);
extern void pool_set_node_to_be_sent(POOL_QUERY_CONTEXT * query_context, int node_id);
extern bool pool_is_node_to_be_sent(POOL_QUERY_CONTEXT * query_context, int node_id);
extern void pool_set_node_to_be_sent(POOL_QUERY_CONTEXT * query_context, int node_id);
extern void pool_unset_node_to_be_sent(POOL_QUERY_CONTEXT * query_context, int node_id);
extern void pool_clear_node_to_be_sent(POOL_QUERY_CONTEXT * query_context);
extern void pool_setall_node_to_be_sent(POOL_QUERY_CONTEXT * query_context);
extern bool pool_multi_node_to_be_sent(POOL_QUERY_CONTEXT * query_context);
extern void pool_where_to_send(POOL_QUERY_CONTEXT * query_context, char *query, Node *node);
extern POOL_STATUS pool_send_and_wait(POOL_QUERY_CONTEXT * query_context, int send_type, int node_id);
extern POOL_STATUS pool_extended_send_and_wait(POOL_QUERY_CONTEXT * query_context, char *kind, int len, char *contents, int send_type, int node_id, bool nowait);
extern Node *pool_get_parse_tree(void);
extern char *pool_get_query_string(void);
extern bool is_set_transaction_serializable(Node *node);
extern bool is_start_transaction_query(Node *node);
extern bool is_read_write(TransactionStmt *node);
extern bool is_serializable(TransactionStmt *node);
extern bool pool_need_to_treat_as_if_default_transaction(POOL_QUERY_CONTEXT * query_context);
extern bool is_savepoint_query(Node *node);
extern bool is_2pc_transaction_query(Node *node);
extern void pool_set_query_state(POOL_QUERY_CONTEXT * query_context, POOL_QUERY_STATE state);
extern int	statecmp(POOL_QUERY_STATE s1, POOL_QUERY_STATE s2);
extern bool pool_is_cache_safe(void);
extern void pool_set_cache_safe(void);
extern void pool_unset_cache_safe(void);
extern bool pool_is_cache_exceeded(void);
extern void pool_set_cache_exceeded(void);
extern void pool_unset_cache_exceeded(void);
extern bool pool_is_transaction_read_only(Node *node);
extern void pool_force_query_node_to_backend(POOL_QUERY_CONTEXT * query_context, int backend_id);
extern void check_object_relationship_list(char *name, bool is_func_name);
#endif							/* POOL_QUERY_CONTEXT_H */
