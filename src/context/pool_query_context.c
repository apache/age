/* -*-pgsql-c-*- */
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
#include "pool.h"
#include "pool_config.h"
#include "protocol/pool_proto_modules.h"
#include "protocol/pool_process_query.h"
#include "protocol/pool_pg_utils.h"
#include "utils/palloc.h"
#include "utils/memutils.h"
#include "utils/elog.h"
#include "utils/statistics.h"
#include "utils/pool_select_walker.h"
#include "utils/pool_stream.h"
#include "context/pool_session_context.h"
#include "context/pool_query_context.h"
#include "parser/nodes.h"

#include <string.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <time.h>

/*
 * Where to send query
 */
typedef enum
{
	POOL_PRIMARY,
	POOL_STANDBY,
	POOL_EITHER,
	POOL_BOTH
}			POOL_DEST;

#define CHECK_QUERY_CONTEXT_IS_VALID \
						do { \
							if (!query_context) \
								ereport(ERROR, \
									(errmsg("setting db node for query to be sent, no query context")));\
						} while (0)

static POOL_DEST send_to_where(Node *node, char *query);
static void where_to_send_deallocate(POOL_QUERY_CONTEXT * query_context, Node *node);
static char *remove_read_write(int len, const char *contents, int *rewritten_len);
static void set_virtual_main_node(POOL_QUERY_CONTEXT *query_context);
static void set_load_balance_info(POOL_QUERY_CONTEXT *query_context);

static bool is_in_list(char *name, List *list);
static bool is_select_object_in_temp_write_list(Node *node, void *context);
static bool add_object_into_temp_write_list(Node *node, void *context);
static void dml_adaptive(Node *node, char *query);
static char* get_associated_object_from_dml_adaptive_relations
							(char *left_token, DBObjectTypes object_type);

/*
 * Create and initialize per query session context
 */
POOL_QUERY_CONTEXT *
pool_init_query_context(void)
{
	MemoryContext memory_context = AllocSetContextCreate(QueryContext,
														 "QueryContextMemoryContext",
														 ALLOCSET_SMALL_MINSIZE,
														 ALLOCSET_SMALL_INITSIZE,
														 ALLOCSET_SMALL_MAXSIZE);

	MemoryContext oldcontext = MemoryContextSwitchTo(memory_context);
	POOL_QUERY_CONTEXT *qc;

	qc = palloc0(sizeof(*qc));
	qc->memory_context = memory_context;
	MemoryContextSwitchTo(oldcontext);
	return qc;
}

/*
 * Destroy query context
 */
void
pool_query_context_destroy(POOL_QUERY_CONTEXT * query_context)
{
	POOL_SESSION_CONTEXT *session_context;

	if (query_context)
	{
		MemoryContext memory_context = query_context->memory_context;

		ereport(DEBUG5,
				(errmsg("pool_query_context_destroy: query context:%p query: \"%s\"",
						query_context, query_context->original_query)));

		session_context = pool_get_session_context(false);
		pool_unset_query_in_progress();
		if (!pool_is_command_success() && query_context->pg_terminate_backend_conn)
		{
			ereport(DEBUG1,
					(errmsg("clearing the connection flag for pg_terminate_backend")));
			pool_unset_connection_will_be_terminated(query_context->pg_terminate_backend_conn);
		}
		query_context->pg_terminate_backend_conn = NULL;
		query_context->original_query = NULL;
		session_context->query_context = NULL;
		pfree(query_context);
		MemoryContextDelete(memory_context);
	}
}

/*
 * Perform shallow copy of given query context. Used in parse_before_bind.
 */
POOL_QUERY_CONTEXT *
pool_query_context_shallow_copy(POOL_QUERY_CONTEXT * query_context)
{
	POOL_QUERY_CONTEXT *qc;
	MemoryContext memory_context;

	qc = pool_init_query_context();
	memory_context = qc->memory_context;
	memcpy(qc, query_context, sizeof(POOL_QUERY_CONTEXT));
	qc->memory_context = memory_context;
	return qc;
}

/*
 * Start query
 */
void
pool_start_query(POOL_QUERY_CONTEXT * query_context, char *query, int len, Node *node)
{
	POOL_SESSION_CONTEXT *session_context;

	if (query_context)
	{
		MemoryContext old_context;

		session_context = pool_get_session_context(false);
		old_context = MemoryContextSwitchTo(query_context->memory_context);
		query_context->original_length = len;
		query_context->rewritten_length = -1;
		query_context->original_query = pstrdup(query);
		query_context->rewritten_query = NULL;
		query_context->parse_tree = node;
		query_context->virtual_main_node_id = my_main_node_id;
		query_context->load_balance_node_id = my_main_node_id;
		query_context->is_cache_safe = false;
		query_context->num_original_params = -1;
		if (pool_config->memory_cache_enabled)
			query_context->temp_cache = pool_create_temp_query_cache(query);
		pool_set_query_in_progress();
		query_context->skip_cache_commit = false;
		session_context->query_context = query_context;
		MemoryContextSwitchTo(old_context);
	}
}

/*
 * Specify DB node to send query
 */
void
pool_set_node_to_be_sent(POOL_QUERY_CONTEXT * query_context, int node_id)
{
	CHECK_QUERY_CONTEXT_IS_VALID;

	if (node_id < 0 || node_id >= MAX_NUM_BACKENDS)
		ereport(ERROR,
				(errmsg("setting db node for query to be sent, invalid node id:%d", node_id),
				 errdetail("backend node id: %d out of range, node id can be between 0 and %d", node_id, MAX_NUM_BACKENDS)));

	query_context->where_to_send[node_id] = true;

	return;
}

/*
 * Unspecified DB node to send query
 */
void
pool_unset_node_to_be_sent(POOL_QUERY_CONTEXT * query_context, int node_id)
{
	CHECK_QUERY_CONTEXT_IS_VALID;

	if (node_id < 0 || node_id >= MAX_NUM_BACKENDS)
		ereport(ERROR,
				(errmsg("un setting db node for query to be sent, invalid node id:%d", node_id),
				 errdetail("backend node id: %d out of range, node id can be between 0 and %d", node_id, MAX_NUM_BACKENDS)));

	query_context->where_to_send[node_id] = false;

	return;
}

/*
 * Clear DB node map
 */
void
pool_clear_node_to_be_sent(POOL_QUERY_CONTEXT * query_context)
{
	CHECK_QUERY_CONTEXT_IS_VALID;

	memset(query_context->where_to_send, false, sizeof(query_context->where_to_send));
	return;
}

/*
 * Set all DB node map entry
 */
void
pool_setall_node_to_be_sent(POOL_QUERY_CONTEXT * query_context)
{
	int			i;
	POOL_SESSION_CONTEXT *sc;

	sc = pool_get_session_context(false);

	CHECK_QUERY_CONTEXT_IS_VALID;

	for (i = 0; i < NUM_BACKENDS; i++)
	{
		if (private_backend_status[i] == CON_UP ||
			(private_backend_status[i] == CON_CONNECT_WAIT))
		{
			/*
			 * In streaming replication mode, if the node is not primary node
			 * nor load balance node, there's no point to send query.
			 */
			if (SL_MODE && !pool_config->statement_level_load_balance &&
				i != PRIMARY_NODE_ID && i != sc->load_balance_node_id)
			{
				continue;
			}
			query_context->where_to_send[i] = true;
		}
	}
	return;
}

/*
 * Return true if multiple nodes are targets
 */
bool
pool_multi_node_to_be_sent(POOL_QUERY_CONTEXT * query_context)
{
	int			i;
	int			cnt = 0;

	CHECK_QUERY_CONTEXT_IS_VALID;

	for (i = 0; i < NUM_BACKENDS; i++)
	{
		if (((BACKEND_INFO(i)).backend_status == CON_UP ||
			 BACKEND_INFO((i)).backend_status == CON_CONNECT_WAIT) &&
			query_context->where_to_send[i])
		{
			cnt++;
			if (cnt > 1)
			{
				return true;
			}
		}
	}
	return false;
}

/*
 * Return if the DB node is needed to send query
 */
bool
pool_is_node_to_be_sent(POOL_QUERY_CONTEXT * query_context, int node_id)
{
	CHECK_QUERY_CONTEXT_IS_VALID;

	if (node_id < 0 || node_id >= MAX_NUM_BACKENDS)
		ereport(ERROR,
				(errmsg("checking if db node is needed to be sent, invalid node id:%d", node_id),
				 errdetail("backend node id: %d out of range, node id can be between 0 and %d", node_id, MAX_NUM_BACKENDS)));

	return query_context->where_to_send[node_id];
}

/*
 * Returns true if the DB node is needed to send query.
 * Intended to be called from VALID_BACKEND
 */
bool
pool_is_node_to_be_sent_in_current_query(int node_id)
{
	POOL_SESSION_CONTEXT *sc;

	if (RAW_MODE)
		return node_id == REAL_MAIN_NODE_ID;

	sc = pool_get_session_context(true);
	if (!sc)
		return true;

	if (pool_is_query_in_progress() && sc->query_context)
	{
		return pool_is_node_to_be_sent(sc->query_context, node_id);
	}
	return true;
}

/*
 * Returns virtual main DB node id,
 */
int
pool_virtual_main_db_node_id(void)
{
	POOL_SESSION_CONTEXT *sc;

	/*
	 * Check whether failover is in progress. If so, just abort this session.
	 */
	if (Req_info->switching)
	{
#ifdef NOT_USED
		POOL_SETMASK(&BlockSig);
		ereport(WARNING,
				(errmsg("failover/failback is in progress"),
						errdetail("executing failover or failback on backend"),
				 errhint("In a moment you should be able to reconnect to the database")));
		POOL_SETMASK(&UnBlockSig);
#endif
		child_exit(POOL_EXIT_AND_RESTART);
	}

	sc = pool_get_session_context(true);
	if (!sc)
	{
		return REAL_MAIN_NODE_ID;
	}

	if (sc->in_progress && sc->query_context)
	{
		int			node_id = sc->query_context->virtual_main_node_id;

		if (SL_MODE)
		{
			/*
			 * Make sure that virtual_main_node_id is either primary node id
			 * or load balance node id.  If not, it is likely that
			 * virtual_main_node_id is not set up yet. Let's use the primary
			 * node id. except for the special case where we need to send the
			 * query to the node which is not primary nor the load balance
			 * node. Currently there is only one special such case that is
			 * handling of pg_terminate_backend() function, which may refer to
			 * the backend connection that is neither hosted by the primary or
			 * load balance node for current child process, but the query must
			 * be forwarded to that node. Since only that backend node can
			 * handle that pg_terminate_backend query
			 *
			 */

			ereport(DEBUG5,
					(errmsg("pool_virtual_main_db_node_id: virtual_main_node_id:%d load_balance_node_id:%d PRIMARY_NODE_ID:%d",
							node_id, sc->load_balance_node_id, PRIMARY_NODE_ID)));

			if (node_id != sc->query_context->load_balance_node_id && node_id != PRIMARY_NODE_ID)
			{
				/*
				 * Only return the primary node id if we are not processing
				 * the pg_terminate_backend query
				 */
				if (sc->query_context->pg_terminate_backend_conn == NULL)
					node_id = PRIMARY_NODE_ID;
			}
		}

		return node_id;
	}

	/*
	 * No query context exists.  If in native replication mode, returns primary node
	 * if exists.  Otherwise returns my_main_node_id, which represents the
	 * last REAL_MAIN_NODE_ID.
	 */
	if (MAIN_REPLICA)
	{
		return PRIMARY_NODE_ID;
	}
	return my_main_node_id;
}

/*
 * The function sets the destination for the current query to the specific backend node
 */
void
pool_force_query_node_to_backend(POOL_QUERY_CONTEXT * query_context, int backend_id)
{
	int			i;

	CHECK_QUERY_CONTEXT_IS_VALID;

	ereport(DEBUG1,
			(errmsg("forcing query destination node to backend node:%d", backend_id)));

	pool_set_node_to_be_sent(query_context, backend_id);
	for (i = 0; i < NUM_BACKENDS; i++)
	{
		if (query_context->where_to_send[i])
		{
			query_context->virtual_main_node_id = i;
			break;
		}
	}
}

/*
 * Decide where to send queries(thus expecting response)
 */
void
pool_where_to_send(POOL_QUERY_CONTEXT * query_context, char *query, Node *node)
{
	POOL_SESSION_CONTEXT *session_context;
	POOL_CONNECTION_POOL *backend;

	CHECK_QUERY_CONTEXT_IS_VALID;

	session_context = pool_get_session_context(false);
	backend = session_context->backend;

	/*
	 * Zap out DB node map
	 */
	pool_clear_node_to_be_sent(query_context);

	/*
	 * In raw mode, we send only to main node. Simple enough.
	 */
	if (RAW_MODE)
	{
		pool_set_node_to_be_sent(query_context, REAL_MAIN_NODE_ID);
	}
	else if (MAIN_REPLICA && query_context->is_multi_statement)
	{
		/*
		 * If we are in native replication mode and we have multi statement query,
		 * we should send it to primary server only. Otherwise it is possible
		 * to send a write query to standby servers because we only use the
		 * first element of the multi statement query and don't care about the
		 * rest.  Typical situation where we are bugged by this is,
		 * "BEGIN;DELETE FROM table;END". Note that from pgpool-II 3.1.0
		 * transactional statements such as "BEGIN" is unconditionally sent to
		 * all nodes(see send_to_where() for more details). Someday we might
		 * be able to understand all part of multi statement queries, but
		 * until that day we need this band aid.
		 */
		pool_set_node_to_be_sent(query_context, PRIMARY_NODE_ID);
	}
	else if (MAIN_REPLICA)
	{
		POOL_DEST	dest;

		dest = send_to_where(node, query);

		dml_adaptive(node, query);

		ereport(DEBUG1,
				(errmsg("decide where to send the query"),
				 errdetail("destination = %d for query= \"%s\"", dest, query)));

		/* Should be sent to primary only? */
		if (dest == POOL_PRIMARY)
		{
			pool_set_node_to_be_sent(query_context, PRIMARY_NODE_ID);
		}
		/* Should be sent to both primary and standby? */
		else if (dest == POOL_BOTH)
		{
			pool_setall_node_to_be_sent(query_context);
		}
		else if (pool_is_writing_transaction() &&
				 pool_config->disable_load_balance_on_write == DLBOW_ALWAYS)
		{
			pool_set_node_to_be_sent(query_context, PRIMARY_NODE_ID);
		}

		/*
		 * Ok, we might be able to load balance the SELECT query.
		 */
		else
		{
			if (pool_config->load_balance_mode &&
				is_select_query(node, query) &&
				MAJOR(backend) == PROTO_MAJOR_V3)
			{
				/*
				 * If (we are outside of an explicit transaction) OR (the
				 * transaction has not issued a write query yet, AND
				 * transaction isolation level is not SERIALIZABLE) we might
				 * be able to load balance.
				 */

				ereport(DEBUG1,
						(errmsg("checking load balance preconditions. TSTATE:%c writing_transaction:%d failed_transaction:%d isolation:%d",
								TSTATE(backend, PRIMARY_NODE_ID),
								pool_is_writing_transaction(),
								pool_is_failed_transaction(),
								pool_get_transaction_isolation()),
						 errdetail("destination = %d for query= \"%s\"", dest, query)));

				if (TSTATE(backend, PRIMARY_NODE_ID) == 'I' ||
					(!pool_is_writing_transaction() &&
					 !pool_is_failed_transaction() &&
					 pool_get_transaction_isolation() != POOL_SERIALIZABLE))
				{
					BackendInfo *bkinfo = pool_get_node_info(session_context->load_balance_node_id);

					/*
					 * Load balance if possible
					 */

					/*
					 * As streaming replication delay is too much, if
					 * prefer_lower_delay_standby is true then elect new
					 * load balance node which is lowest delayed,
					 * false then send to the primary.
					 */
					if (STREAM &&
						pool_config->delay_threshold &&
						bkinfo->standby_delay > pool_config->delay_threshold)
					{
						ereport(DEBUG1,
								(errmsg("could not load balance because of too much replication delay"),
								 errdetail("destination = %d for query= \"%s\"", dest, query)));

						if (pool_config->prefer_lower_delay_standby)
						{
							int new_load_balancing_node = select_load_balancing_node();

							session_context->load_balance_node_id = new_load_balancing_node;
							session_context->query_context->load_balance_node_id = session_context->load_balance_node_id;
							pool_set_node_to_be_sent(query_context, session_context->query_context->load_balance_node_id);
						}
						else
						{
							pool_set_node_to_be_sent(query_context, PRIMARY_NODE_ID);
						}
					}

					/*
					 * If system catalog is used in the SELECT, we prefer to
					 * send to the primary. Example: SELECT * FROM pg_class
					 * WHERE relname = 't1'; Because 't1' is a constant, it's
					 * hard to recognize as table name.  Most use case such
					 * query is against system catalog, and the table name can
					 * be a temporary table, it's best to query against
					 * primary system catalog. Please note that this test must
					 * be done *before* test using pool_has_temp_table.
					 */
					else if (pool_has_system_catalog(node))
					{
						ereport(DEBUG1,
								(errmsg("could not load balance because systems catalogs are used"),
								 errdetail("destination = %d for query= \"%s\"", dest, query)));

						pool_set_node_to_be_sent(query_context, PRIMARY_NODE_ID);
					}

					/*
					 * If temporary table is used in the SELECT, we prefer to
					 * send to the primary.
					 */
					else if (pool_config->check_temp_table && pool_has_temp_table(node))
					{
						ereport(DEBUG1,
								(errmsg("could not load balance because temporary tables are used"),
								 errdetail("destination = %d for query= \"%s\"", dest, query)));

						pool_set_node_to_be_sent(query_context, PRIMARY_NODE_ID);
					}

					/*
					 * If unlogged table is used in the SELECT, we prefer to
					 * send to the primary.
					 */
					else if (pool_config->check_unlogged_table && pool_has_unlogged_table(node))
					{
						ereport(DEBUG1,
								(errmsg("could not load balance because unlogged tables are used"),
								 errdetail("destination = %d for query= \"%s\"", dest, query)));

						pool_set_node_to_be_sent(query_context, PRIMARY_NODE_ID);
					}
					/*
					 * When query match the query patterns in primary_routing_query_pattern_list, we
					 * send only to main node.
					 */
					else if (pattern_compare(query, WRITELIST, "primary_routing_query_pattern_list") == 1)
					{
						pool_set_node_to_be_sent(query_context, PRIMARY_NODE_ID);
					}
					/*
					 * If a writing function call is used, we prefer to send
					 * to the primary.
					 */
					else if (pool_has_function_call(node))
					{
						ereport(DEBUG1,
								(errmsg("could not load balance because writing functions are used"),
								 errdetail("destination = %d for query= \"%s\"", dest, query)));

						pool_set_node_to_be_sent(query_context, PRIMARY_NODE_ID);
					}
					else if (is_select_object_in_temp_write_list(node, query))
					{
						pool_set_node_to_be_sent(query_context, PRIMARY_NODE_ID);
					}
					else
					{
						if (pool_config->statement_level_load_balance)
							session_context->load_balance_node_id = select_load_balancing_node();

						session_context->query_context->load_balance_node_id = session_context->load_balance_node_id;
						pool_set_node_to_be_sent(query_context,
												 session_context->query_context->load_balance_node_id);
					}
				}
				else
				{
					/* Send to the primary only */
					pool_set_node_to_be_sent(query_context, PRIMARY_NODE_ID);
				}
			}
			else
			{
				/* Send to the primary only */
				pool_set_node_to_be_sent(query_context, PRIMARY_NODE_ID);
			}
		}
	}
	else if (REPLICATION)
	{
		if (pool_config->load_balance_mode &&
			is_select_query(node, query) &&
			MAJOR(backend) == PROTO_MAJOR_V3)
		{
			/*
			 * In snapshot isolation mode, we always load balance if current
			 * transaction is read only unless load balance mode is off.
			 */
			if (pool_config->backend_clustering_mode == CM_SNAPSHOT_ISOLATION &&
				pool_config->load_balance_mode)
			{
				if (TSTATE(backend, MAIN_NODE_ID) == 'T')
				{
					/*
					 * We are in an explicit transaction. If the transaction is
					 * read only, we can load balance.
					 */
					if (session_context->transaction_read_only)
					{
						/* Ok, we can load balance. We are done! */
						set_load_balance_info(query_context);
						set_virtual_main_node(query_context);
						return;
					}
				}
				else if (TSTATE(backend, MAIN_NODE_ID) == 'I')
				{
					/*
					 * We are out side transaction. If default transaction is read only,
					 * we can load balance.
					 */
					static	char *si_query = "SELECT current_setting('transaction_read_only')";
					POOL_SELECT_RESULT *res;
					bool	load_balance = false;

					do_query(CONNECTION(backend, MAIN_NODE_ID), si_query, &res, MAJOR(backend));
					if (res)
					{
						if (res->data[0] && !strcmp(res->data[0], "on"))
						{
							load_balance = true;
						}
						free_select_result(res);
					}

					per_node_statement_log(backend, MAIN_NODE_ID, si_query);

					if (load_balance)
					{
						/* Ok, we can load balance. We are done! */
						set_load_balance_info(query_context);
						set_virtual_main_node(query_context);
						return;
					}
				}
			}
			
			/*
			 * If a writing function call is used or replicate_select is true,
			 * we prefer to send to all nodes.
			 */
			if (pool_has_function_call(node) || pool_config->replicate_select)
			{
				pool_setall_node_to_be_sent(query_context);
			}

			/*
			 * If (we are outside of an explicit transaction) OR (the
			 * transaction has not issued a write query yet, AND transaction
			 * isolation level is not SERIALIZABLE) we might be able to load
			 * balance.
			 */
			else if (TSTATE(backend, MAIN_NODE_ID) == 'I' ||
					 (!pool_is_writing_transaction() &&
					  !pool_is_failed_transaction() &&
					  pool_get_transaction_isolation() != POOL_SERIALIZABLE))
			{
				set_load_balance_info(query_context);
			}
			else
			{
				/* only send to main node */
				pool_set_node_to_be_sent(query_context, REAL_MAIN_NODE_ID);
			}
		}
		else
		{
			if (is_select_query(node, query) && !pool_config->replicate_select &&
				!pool_has_function_call(node))
			{
				/* only send to main node */
				pool_set_node_to_be_sent(query_context, REAL_MAIN_NODE_ID);
			}
			else
			{
				/* send to all nodes */
				pool_setall_node_to_be_sent(query_context);
			}
		}
	}
	else
	{
		ereport(WARNING,
				(errmsg("unknown pgpool-II mode while deciding for where to send query")));
		return;
	}

	/*
	 * EXECUTE?
	 */
	if (IsA(node, ExecuteStmt))
	{
		POOL_SENT_MESSAGE *msg;

		msg = pool_get_sent_message('Q', ((ExecuteStmt *) node)->name, POOL_SENT_MESSAGE_CREATED);
		if (!msg)
			msg = pool_get_sent_message('P', ((ExecuteStmt *) node)->name, POOL_SENT_MESSAGE_CREATED);
		if (msg)
			pool_copy_prep_where(msg->query_context->where_to_send,
								 query_context->where_to_send);
	}

	/*
	 * DEALLOCATE?
	 */
	else if (IsA(node, DeallocateStmt))
	{
		where_to_send_deallocate(query_context, node);
	}

	/* Set virtual main node according to the where_to_send map. */
	set_virtual_main_node(query_context);

	return;
}

/*
 * Send simple query and wait for response
 * send_type:
 *  -1: do not send this node_id
 *   0: send to all nodes
 *  >0: send to this node_id
 */
POOL_STATUS
pool_send_and_wait(POOL_QUERY_CONTEXT * query_context,
				   int send_type, int node_id)
{
	POOL_SESSION_CONTEXT *session_context;
	POOL_CONNECTION *frontend;
	POOL_CONNECTION_POOL *backend;
	bool		is_commit;
	bool		is_begin_read_write;
	int			i;
	int			len;
	char	   *string;

	session_context = pool_get_session_context(false);
	frontend = session_context->frontend;
	backend = session_context->backend;
	is_commit = is_commit_or_rollback_query(query_context->parse_tree);
	is_begin_read_write = false;
	len = 0;
	string = NULL;

	/*
	 * If the query is BEGIN READ WRITE or BEGIN ... SERIALIZABLE in
	 * native replication mode, we send BEGIN to standbys instead.
	 * original_query which is BEGIN READ WRITE is sent to primary.
	 * rewritten_query which is BEGIN is sent to standbys.
	 */
	if (pool_need_to_treat_as_if_default_transaction(query_context))
	{
		is_begin_read_write = true;
	}
	else
	{
		if (query_context->rewritten_query)
		{
			len = query_context->rewritten_length;
			string = query_context->rewritten_query;
		}
		else
		{
			len = query_context->original_length;
			string = query_context->original_query;
		}
	}

	/* Send query */
	for (i = 0; i < NUM_BACKENDS; i++)
	{
		if (!VALID_BACKEND(i))
			continue;
		else if (send_type < 0 && i == node_id)
			continue;
		else if (send_type > 0 && i != node_id)
			continue;

		/*
		 * If in native replication mode, we do not send COMMIT/ABORT to
		 * standbys if it's in I(idle) state.
		 */
		if (is_commit && MAIN_REPLICA && !IS_MAIN_NODE_ID(i) && TSTATE(backend, i) == 'I')
		{
			pool_unset_node_to_be_sent(query_context, i);
			continue;
		}

		/*
		 * If in reset context, we send COMMIT/ABORT to nodes those are not in
		 * I(idle) state.  This will ensure that transactions are closed.
		 */
		if (is_commit && session_context->reset_context && TSTATE(backend, i) == 'I')
		{
			pool_unset_node_to_be_sent(query_context, i);
			continue;
		}

		if (is_begin_read_write)
		{
			if (REAL_PRIMARY_NODE_ID == i)
			{
				len = query_context->original_length;
				string = query_context->original_query;
			}
			else
			{
				len = query_context->rewritten_length;
				string = query_context->rewritten_query;
			}
		}

		per_node_statement_log(backend, i, string);
		stat_count_up(i, query_context->parse_tree);
		send_simplequery_message(CONNECTION(backend, i), len, string, MAJOR(backend));
	}

	/* Wait for response */
	for (i = 0; i < NUM_BACKENDS; i++)
	{
		if (!VALID_BACKEND(i))
			continue;
		else if (send_type < 0 && i == node_id)
			continue;
		else if (send_type > 0 && i != node_id)
			continue;

#ifdef NOT_USED

		/*
		 * If in native replication mode, we do not send COMMIT/ABORT to
		 * standbys if it's in I(idle) state.
		 */
		if (is_commit && MAIN_REPLICA && !IS_MAIN_NODE_ID(i) && TSTATE(backend, i) == 'I')
		{
			continue;
		}
#endif

		if (is_begin_read_write)
		{
			if (REAL_PRIMARY_NODE_ID == i)
				string = query_context->original_query;
			else
				string = query_context->rewritten_query;
		}

		wait_for_query_response_with_trans_cleanup(frontend,
												   CONNECTION(backend, i),
												   MAJOR(backend),
												   MAIN_CONNECTION(backend)->pid,
												   MAIN_CONNECTION(backend)->key);

		/*
		 * Check if some error detected.  If so, emit log. This is useful when
		 * invalid encoding error occurs. In this case, PostgreSQL does not
		 * report what statement caused that error and make users confused.
		 */
		per_node_error_log(backend, i, string, "pool_send_and_wait: Error or notice message from backend: ", true);
	}

	return POOL_CONTINUE;
}

/*
 * Send extended query and wait for response
 * send_type:
 *  -1: do not send this node_id
 *   0: send to all nodes
 *  >0: send to this node_id
 */
POOL_STATUS
pool_extended_send_and_wait(POOL_QUERY_CONTEXT * query_context,
							char *kind, int len, char *contents,
							int send_type, int node_id, bool nowait)
{
	POOL_SESSION_CONTEXT *session_context;
	POOL_CONNECTION *frontend;
	POOL_CONNECTION_POOL *backend;
	bool		is_commit;
	bool		is_begin_read_write;
	int			i;
	int			str_len;
	int			rewritten_len;
	char	   *str;
	char	   *rewritten_begin;

	session_context = pool_get_session_context(false);
	frontend = session_context->frontend;
	backend = session_context->backend;
	is_commit = is_commit_or_rollback_query(query_context->parse_tree);
	is_begin_read_write = false;
	str_len = 0;
	rewritten_len = 0;
	str = NULL;
	rewritten_begin = NULL;

	/*
	 * If the query is BEGIN READ WRITE or BEGIN ... SERIALIZABLE in
	 * native replication mode, we send BEGIN to standbys instead.
	 * original_query which is BEGIN READ WRITE is sent to primary.
	 * rewritten_query which is BEGIN is sent to standbys.
	 */
	if (pool_need_to_treat_as_if_default_transaction(query_context))
	{
		is_begin_read_write = true;

		if (*kind == 'P')
			rewritten_begin = remove_read_write(len, contents, &rewritten_len);
	}

	if (!rewritten_begin)
	{
		str_len = len;
		str = contents;
	}

	/* Send query */
	for (i = 0; i < NUM_BACKENDS; i++)
	{
		if (!VALID_BACKEND(i))
			continue;
		else if (send_type < 0 && i == node_id)
			continue;
		else if (send_type > 0 && i != node_id)
			continue;

		/*
		 * If in reset context, we send COMMIT/ABORT to nodes those are not in
		 * I(idle) state.  This will ensure that transactions are closed.
		 */
		if (is_commit && session_context->reset_context && TSTATE(backend, i) == 'I')
		{
			pool_unset_node_to_be_sent(query_context, i);
			continue;
		}

		if (rewritten_begin)
		{
			if (REAL_PRIMARY_NODE_ID == i)
			{
				str = contents;
				str_len = len;
			}
			else
			{
				str = rewritten_begin;
				str_len = rewritten_len;
			}
		}

		if (pool_config->log_per_node_statement)
		{
			char		msgbuf[QUERY_STRING_BUFFER_LEN];
			char	   *stmt;

			if (*kind == 'P' || *kind == 'E' || *kind == 'B')
			{
				if (query_context->rewritten_query)
				{
					if (is_begin_read_write)
					{
						if (REAL_PRIMARY_NODE_ID == i)
							stmt = query_context->original_query;
						else
							stmt = query_context->rewritten_query;
					}
					else
					{
						stmt = query_context->rewritten_query;
					}
				}
				else
				{
					stmt = query_context->original_query;
				}

				if (*kind == 'P')
					snprintf(msgbuf, sizeof(msgbuf), "Parse: %s", stmt);
				else if (*kind == 'B')
					snprintf(msgbuf, sizeof(msgbuf), "Bind: %s", stmt);
				else
					snprintf(msgbuf, sizeof(msgbuf), "Execute: %s", stmt);
			}
			else
			{
				snprintf(msgbuf, sizeof(msgbuf), "%c message", *kind);
			}

			per_node_statement_log(backend, i, msgbuf);
		}

		/* if Execute message, count up stats count */
		if (*kind == 'E')
		{
			stat_count_up(i, query_context->parse_tree);
		}

		send_extended_protocol_message(backend, i, kind, str_len, str);

		if ((*kind == 'P' || *kind == 'E' || *kind == 'C') && STREAM)
		{
			/*
			 * Send flush message to backend to make sure that we get any
			 * response from backend in Streaming replication mode.
			 */

			POOL_CONNECTION *cp = CONNECTION(backend, i);
			int			len;

			pool_write(cp, "H", 1);
			len = htonl(sizeof(len));
			pool_write_and_flush(cp, &len, sizeof(len));

			ereport(DEBUG5,
					(errmsg("pool_send_and_wait: send flush message to %d", i)));
		}
	}

	if (!is_begin_read_write)
	{
		if (query_context->rewritten_query)
			str = query_context->rewritten_query;
		else
			str = query_context->original_query;
	}

	if (!nowait)
	{
		/* Wait for response */
		for (i = 0; i < NUM_BACKENDS; i++)
		{
			if (!VALID_BACKEND(i))
				continue;
			else if (send_type < 0 && i == node_id)
				continue;
			else if (send_type > 0 && i != node_id)
				continue;

			/*
			 * If in native replication mode, we do not send COMMIT/ABORT to
			 * standbys if it's in I(idle) state.
			 */
			if (is_commit && MAIN_REPLICA && !IS_MAIN_NODE_ID(i) && TSTATE(backend, i) == 'I')
			{
				continue;
			}

			if (is_begin_read_write)
			{
				if (REAL_PRIMARY_NODE_ID == i)
					str = query_context->original_query;
				else
					str = query_context->rewritten_query;
			}

			wait_for_query_response_with_trans_cleanup(frontend,
													   CONNECTION(backend, i),
													   MAJOR(backend),
													   MAIN_CONNECTION(backend)->pid,
													   MAIN_CONNECTION(backend)->key);

			/*
			 * Check if some error detected.  If so, emit log. This is useful
			 * when invalid encoding error occurs. In this case, PostgreSQL
			 * does not report what statement caused that error and make users
			 * confused.
			 */
			per_node_error_log(backend, i, str, "pool_send_and_wait: Error or notice message from backend: ", true);
		}
	}

	if (rewritten_begin)
		pfree(rewritten_begin);
	return POOL_CONTINUE;
}

/*
 * From syntactically analysis decide the statement to be sent to the
 * primary, the standby or either or both in native replication+HR/SR mode.
 */
static POOL_DEST send_to_where(Node *node, char *query)

{
/* From storage/lock.h */
#define NoLock					0
#define AccessShareLock			1	/* SELECT */
#define RowShareLock			2	/* SELECT FOR UPDATE/FOR SHARE */
#define RowExclusiveLock		3	/* INSERT, UPDATE, DELETE */
#define ShareUpdateExclusiveLock 4	/* VACUUM (non-FULL),ANALYZE, CREATE INDEX
									 * CONCURRENTLY */
#define ShareLock				5	/* CREATE INDEX (WITHOUT CONCURRENTLY) */
#define ShareRowExclusiveLock	6	/* like EXCLUSIVE MODE, but allows ROW
									 * SHARE */
#define ExclusiveLock			7	/* blocks ROW SHARE/SELECT...FOR UPDATE */
#define AccessExclusiveLock		8	/* ALTER TABLE, DROP TABLE, VACUUM FULL,
									 * and unqualified LOCK TABLE */

/* From 9.5 include/nodes/node.h ("TAGS FOR STATEMENT NODES" part) */
	static NodeTag nodemap[] = {
		T_RawStmt,
		T_Query,
		T_PlannedStmt,
		T_InsertStmt,
		T_DeleteStmt,
		T_UpdateStmt,
		T_SelectStmt,
		T_AlterTableStmt,
		T_AlterTableCmd,
		T_AlterDomainStmt,
		T_SetOperationStmt,
		T_GrantStmt,
		T_GrantRoleStmt,
		T_AlterDefaultPrivilegesStmt,
		T_ClosePortalStmt,
		T_ClusterStmt,
		T_CopyStmt,
		T_CreateStmt,			/* CREATE TABLE */
		T_DefineStmt,			/* CREATE AGGREGATE, OPERATOR, TYPE */
		T_DropStmt,				/* DROP TABLE etc. */
		T_TruncateStmt,
		T_CommentStmt,
		T_FetchStmt,
		T_IndexStmt,			/* CREATE INDEX */
		T_CreateFunctionStmt,
		T_AlterFunctionStmt,
		T_DoStmt,
		T_RenameStmt,			/* ALTER AGGREGATE etc. */
		T_RuleStmt,				/* CREATE RULE */
		T_NotifyStmt,
		T_ListenStmt,
		T_UnlistenStmt,
		T_TransactionStmt,
		T_ViewStmt,				/* CREATE VIEW */
		T_LoadStmt,
		T_CreateDomainStmt,
		T_CreatedbStmt,
		T_DropdbStmt,
		T_VacuumStmt,
		T_ExplainStmt,
		T_CreateTableAsStmt,
		T_CreateSeqStmt,
		T_AlterSeqStmt,
		T_VariableSetStmt,		/* SET */
		T_VariableShowStmt,
		T_DiscardStmt,
		T_CreateTrigStmt,
		T_CreatePLangStmt,
		T_CreateRoleStmt,
		T_AlterRoleStmt,
		T_DropRoleStmt,
		T_LockStmt,
		T_ConstraintsSetStmt,
		T_ReindexStmt,
		T_CheckPointStmt,
		T_CreateSchemaStmt,
		T_AlterDatabaseStmt,
		T_AlterDatabaseSetStmt,
		T_AlterRoleSetStmt,
		T_CreateConversionStmt,
		T_CreateCastStmt,
		T_CreateOpClassStmt,
		T_CreateOpFamilyStmt,
		T_AlterOpFamilyStmt,
		T_PrepareStmt,
		T_ExecuteStmt,
		T_DeallocateStmt,		/* DEALLOCATE */
		T_DeclareCursorStmt,	/* DECLARE */
		T_CreateTableSpaceStmt,
		T_DropTableSpaceStmt,
		T_AlterObjectSchemaStmt,
		T_AlterOwnerStmt,
		T_DropOwnedStmt,
		T_ReassignOwnedStmt,
		T_CompositeTypeStmt,	/* CREATE TYPE */
		T_CreateEnumStmt,
		T_CreateRangeStmt,
		T_AlterEnumStmt,
		T_AlterTSDictionaryStmt,
		T_AlterTSConfigurationStmt,
		T_CreateFdwStmt,
		T_AlterFdwStmt,
		T_CreateForeignServerStmt,
		T_AlterForeignServerStmt,
		T_CreateUserMappingStmt,
		T_AlterUserMappingStmt,
		T_DropUserMappingStmt,
		T_AlterTableSpaceOptionsStmt,
		T_AlterTableMoveAllStmt,
		T_SecLabelStmt,
		T_CreateForeignTableStmt,
		T_ImportForeignSchemaStmt,
		T_CreateExtensionStmt,
		T_AlterExtensionStmt,
		T_AlterExtensionContentsStmt,
		T_CreateEventTrigStmt,
		T_AlterEventTrigStmt,
		T_RefreshMatViewStmt,
		T_ReplicaIdentityStmt,
		T_AlterSystemStmt,
		T_CreatePolicyStmt,
		T_AlterPolicyStmt,
		T_CreateTransformStmt,
		T_CreateAmStmt,
		T_CreatePublicationStmt,
		T_AlterPublicationStmt,
		T_CreateSubscriptionStmt,
		T_DropSubscriptionStmt,
		T_CreateStatsStmt,
		T_AlterCollationStmt,
	};

	if (bsearch(&nodeTag(node), nodemap, sizeof(nodemap) / sizeof(nodemap[0]),
				sizeof(NodeTag), compare) != NULL)
	{
		/*
		 * SELECT INTO SELECT FOR SHARE or UPDATE
		 */
		if (IsA(node, SelectStmt))
		{
			/* SELECT INTO or SELECT FOR SHARE or UPDATE ? */
			if (pool_has_insertinto_or_locking_clause(node))
				return POOL_PRIMARY;

			/* non-SELECT query in WITH clause ? */
			if (((SelectStmt *) node)->withClause)
			{
				List	   *ctes = ((SelectStmt *) node)->withClause->ctes;
				ListCell   *cte_item;

				foreach(cte_item, ctes)
				{
					CommonTableExpr *cte = (CommonTableExpr *) lfirst(cte_item);

					if (!IsA(cte->ctequery, SelectStmt))
						return POOL_PRIMARY;
				}
			}

			return POOL_EITHER;
		}

		/*
		 * COPY
		 */
		else if (IsA(node, CopyStmt))
		{
			if (((CopyStmt *) node)->is_from)
				return POOL_PRIMARY;
			else
			{
				if (((CopyStmt *) node)->query == NULL)
					return POOL_EITHER;
				else
					return (IsA(((CopyStmt *) node)->query, SelectStmt)) ? POOL_EITHER : POOL_PRIMARY;
			}
		}

		/*
		 * LOCK
		 */
		else if (IsA(node, LockStmt))
		{
			return (((LockStmt *) node)->mode >= RowExclusiveLock) ? POOL_PRIMARY : POOL_BOTH;
		}

		/*
		 * Transaction commands
		 */
		else if (IsA(node, TransactionStmt))
		{
			/*
			 * Check "BEGIN READ WRITE" "START TRANSACTION READ WRITE"
			 */
			if (is_start_transaction_query(node))
			{
				/*
				 * But actually, we send BEGIN to standby if it's BEGIN READ
				 * WRITE or START TRANSACTION READ WRITE
				 */
				if (is_read_write((TransactionStmt *) node))
					return POOL_BOTH;

				/*
				 * Other TRANSACTION start commands are sent to both primary
				 * and standby
				 */
				else
					return POOL_BOTH;
			}
			/* SAVEPOINT related commands are sent to both primary and standby */
			else if (is_savepoint_query(node))
				return POOL_BOTH;

			/*
			 * 2PC commands
			 */
			else if (is_2pc_transaction_query(node))
				return POOL_PRIMARY;
			else
				/* COMMIT etc. */
				return POOL_BOTH;
		}

		/*
		 * SET
		 */
		else if (IsA(node, VariableSetStmt))
		{
			ListCell   *list_item;
			bool		ret = POOL_BOTH;

			/*
			 * SET transaction_read_only TO off
			 */
			if (((VariableSetStmt *) node)->kind == VAR_SET_VALUE &&
				!strcmp(((VariableSetStmt *) node)->name, "transaction_read_only"))
			{
				List	   *options = ((VariableSetStmt *) node)->args;

				foreach(list_item, options)
				{
					A_Const    *v = (A_Const *) lfirst(list_item);

					switch (nodeTag(&v->val))
					{
						case T_String:
							if (!strcasecmp(v->val.sval.sval, "off") ||
								!strcasecmp(v->val.sval.sval, "f") ||
								!strcasecmp(v->val.sval.sval, "false"))
								ret = POOL_PRIMARY;
							break;
						case T_Integer:
							if (v->val.ival.ival)
								ret = POOL_PRIMARY;
						default:
							break;
					}
				}
				return ret;
			}

			/*
			 * SET TRANSACTION ISOLATION LEVEL SERIALIZABLE or SET SESSION
			 * CHARACTERISTICS AS TRANSACTION ISOLATION LEVEL SERIALIZABLE or
			 * SET transaction_isolation TO 'serializable' SET
			 * default_transaction_isolation TO 'serializable'
			 */
			else if (is_set_transaction_serializable(node))
			{
				return POOL_PRIMARY;
			}

			/*
			 * Check "SET TRANSACTION READ WRITE" "SET SESSION CHARACTERISTICS
			 * AS TRANSACTION READ WRITE"
			 */
			else if (((VariableSetStmt *) node)->kind == VAR_SET_MULTI &&
					 (!strcmp(((VariableSetStmt *) node)->name, "TRANSACTION") ||
					  !strcmp(((VariableSetStmt *) node)->name, "SESSION CHARACTERISTICS")))
			{
				List	   *options = ((VariableSetStmt *) node)->args;

				foreach(list_item, options)
				{
					DefElem    *opt = (DefElem *) lfirst(list_item);

					if (!strcmp("transaction_read_only", opt->defname))
					{
						bool		read_only;

						read_only = ((A_Const *) opt->arg)->val.ival.ival;
						if (!read_only)
							return POOL_PRIMARY;
					}
				}
				return POOL_BOTH;
			}
			else
			{
				/*
				 * All other SET command sent to both primary and standby
				 */
				return POOL_BOTH;
			}
		}

		/*
		 * DISCARD
		 */
		else if (IsA(node, DiscardStmt))
		{
			return POOL_BOTH;
		}

		/*
		 * PREPARE
		 */
		else if (IsA(node, PrepareStmt))
		{
			PrepareStmt *prepare_statement = (PrepareStmt *) node;

			char	   *string = nodeToString(prepare_statement->query);

			/* Note that this is a recursive call */
			return send_to_where((Node *) (prepare_statement->query), string);
		}

		/*
		 * EXECUTE
		 */
		else if (IsA(node, ExecuteStmt))
		{
			/*
			 * This is temporary decision. where_to_send will inherit same
			 * destination AS PREPARE.
			 */
			return POOL_PRIMARY;
		}

		/*
		 * DEALLOCATE
		 */
		else if (IsA(node, DeallocateStmt))
		{
			/*
			 * This is temporary decision. where_to_send will inherit same
			 * destination AS PREPARE.
			 */
			return POOL_PRIMARY;
		}

		/*
		 * SHOW
		 */
		else if (IsA(node, VariableShowStmt))
		{
			return POOL_EITHER;
		}

		/*
		 * Other statements are sent to primary
		 */
		return POOL_PRIMARY;
	}

	/*
	 * All unknown statements are sent to primary
	 */
	return POOL_PRIMARY;
}

static
void
where_to_send_deallocate(POOL_QUERY_CONTEXT * query_context, Node *node)
{
	DeallocateStmt *d = (DeallocateStmt *) node;
	POOL_SENT_MESSAGE *msg;

	/* DEALLOCATE ALL? */
	if (d->name == NULL)
	{
		pool_setall_node_to_be_sent(query_context);
	}
	else
	{
		msg = pool_get_sent_message('Q', d->name, POOL_SENT_MESSAGE_CREATED);
		if (!msg)
			msg = pool_get_sent_message('P', d->name, POOL_SENT_MESSAGE_CREATED);
		if (msg)
		{
			/* Inherit same map from PREPARE or PARSE */
			pool_copy_prep_where(msg->query_context->where_to_send,
								 query_context->where_to_send);

			/* copy load balance node id as well */
			query_context->load_balance_node_id = msg->query_context->load_balance_node_id;
		}
		else
			/* prepared statement was not found */
			pool_setall_node_to_be_sent(query_context);
	}
}

/*
 * Returns parse tree for current query.
 * Precondition: the query is in progress state.
 */
Node *
pool_get_parse_tree(void)
{
	POOL_SESSION_CONTEXT *sc;

	sc = pool_get_session_context(true);
	if (!sc)
		return NULL;

	if (pool_is_query_in_progress() && sc->query_context)
	{
		return sc->query_context->parse_tree;
	}
	return NULL;
}

/*
 * Returns raw query string for current query.
 * Precondition: the query is in progress state.
 */
char *
pool_get_query_string(void)
{
	POOL_SESSION_CONTEXT *sc;

	sc = pool_get_session_context(true);
	if (!sc)
		return NULL;

	if (pool_is_query_in_progress() && sc->query_context)
	{
		return sc->query_context->original_query;
	}
	return NULL;
}

/*
 * Returns true if the query is one of:
 *
 * SET TRANSACTION ISOLATION LEVEL SERIALIZABLE or
 * SET SESSION CHARACTERISTICS AS TRANSACTION ISOLATION LEVEL SERIALIZABLE or
 * SET transaction_isolation TO 'serializable'
 * SET default_transaction_isolation TO 'serializable'
 */
bool
is_set_transaction_serializable(Node *node)
{
	ListCell   *list_item;

	if (!IsA(node, VariableSetStmt))
		return false;

	if (((VariableSetStmt *) node)->kind == VAR_SET_VALUE &&
		(!strcmp(((VariableSetStmt *) node)->name, "transaction_isolation") ||
		 !strcmp(((VariableSetStmt *) node)->name, "default_transaction_isolation")))
	{
		List	   *options = ((VariableSetStmt *) node)->args;

		foreach(list_item, options)
		{
			A_Const    *v = (A_Const *) lfirst(list_item);

			switch (nodeTag(&v->val))
			{
				case T_String:
					if (!strcasecmp(v->val.sval.sval, "serializable"))
						return true;
					break;
				default:
					break;
			}
		}
		return false;
	}

	else if (((VariableSetStmt *) node)->kind == VAR_SET_MULTI &&
			 (!strcmp(((VariableSetStmt *) node)->name, "TRANSACTION") ||
			  !strcmp(((VariableSetStmt *) node)->name, "SESSION CHARACTERISTICS")))
	{
		List	   *options = ((VariableSetStmt *) node)->args;

		foreach(list_item, options)
		{
			DefElem    *opt = (DefElem *) lfirst(list_item);

			if (!strcmp("transaction_isolation", opt->defname) ||
				!strcmp("default_transaction_isolation", opt->defname))
			{
				A_Const    *v = (A_Const *) opt->arg;

				if (!strcasecmp(v->val.sval.sval, "serializable"))
					return true;
			}
		}
	}
	return false;
}

/*
 * Returns true if SQL is transaction starting command (START
 * TRANSACTION or BEGIN)
 */
bool
is_start_transaction_query(Node *node)
{
	TransactionStmt *stmt;

	if (node == NULL || !IsA(node, TransactionStmt))
		return false;

	stmt = (TransactionStmt *) node;
	return stmt->kind == TRANS_STMT_START || stmt->kind == TRANS_STMT_BEGIN;
}

/*
 * Return true if start transaction query with "READ WRITE" option.
 */
bool
is_read_write(TransactionStmt *node)
{
	ListCell   *list_item;

	List	   *options = node->options;

	foreach(list_item, options)
	{
		DefElem    *opt = (DefElem *) lfirst(list_item);

		if (!strcmp("transaction_read_only", opt->defname))
		{
			bool		read_only;

			read_only = ((A_Const *) opt->arg)->val.ival.ival;
			if (read_only)
				return false;	/* TRANSACTION READ ONLY */
			else

				/*
				 * TRANSACTION READ WRITE specified. This sounds a little bit
				 * strange, but actually the parse code works in the way.
				 */
				return true;
		}
	}

	/*
	 * No TRANSACTION READ ONLY/READ WRITE clause specified.
	 */
	return false;
}

/*
 * Return true if start transaction query with "SERIALIZABLE" option.
 */
bool
is_serializable(TransactionStmt *node)
{
	ListCell   *list_item;

	List	   *options = node->options;

	foreach(list_item, options)
	{
		DefElem    *opt = (DefElem *) lfirst(list_item);

		if (!strcmp("transaction_isolation", opt->defname) &&
			IsA(opt->arg, A_Const) &&
			IsA(&((A_Const *) opt->arg)->val, String) &&
			!strcmp("serializable", ((A_Const *) opt->arg)->val.sval.sval))
			return true;
	}
	return false;
}

/*
 * If the query is BEGIN READ WRITE or
 * BEGIN ... SERIALIZABLE in native replication mode,
 * we send BEGIN to standbys instead.
 * original_query which is BEGIN READ WRITE is sent to primary.
 * rewritten_query which is BEGIN is sent to standbys.
 */
bool
pool_need_to_treat_as_if_default_transaction(POOL_QUERY_CONTEXT * query_context)
{
	return (MAIN_REPLICA &&
			is_start_transaction_query(query_context->parse_tree) &&
			(is_read_write((TransactionStmt *) query_context->parse_tree) ||
			 is_serializable((TransactionStmt *) query_context->parse_tree)));
}

/*
 * Return true if the query is SAVEPOINT related query.
 */
bool
is_savepoint_query(Node *node)
{
	if (((TransactionStmt *) node)->kind == TRANS_STMT_SAVEPOINT ||
		((TransactionStmt *) node)->kind == TRANS_STMT_ROLLBACK_TO ||
		((TransactionStmt *) node)->kind == TRANS_STMT_RELEASE)
		return true;

	return false;
}

/*
 * Return true if the query is 2PC transaction query.
 */
bool
is_2pc_transaction_query(Node *node)
{
	if (((TransactionStmt *) node)->kind == TRANS_STMT_PREPARE ||
		((TransactionStmt *) node)->kind == TRANS_STMT_COMMIT_PREPARED ||
		((TransactionStmt *) node)->kind == TRANS_STMT_ROLLBACK_PREPARED)
		return true;

	return false;
}

/*
 * Set query state, if a current state is before it than the specified state.
 */
void
pool_set_query_state(POOL_QUERY_CONTEXT * query_context, POOL_QUERY_STATE state)
{
	int			i;

	CHECK_QUERY_CONTEXT_IS_VALID;

	for (i = 0; i < NUM_BACKENDS; i++)
	{
		if (query_context->where_to_send[i] &&
			statecmp(query_context->query_state[i], state) < 0)
			query_context->query_state[i] = state;
	}
}

/*
 * Return -1, 0 or 1 according to s1 is "before, equal or after" s2 in terms of state
 * transition order.
 * The State transition order is defined as: UNPARSED < PARSE_COMPLETE < BIND_COMPLETE < EXECUTE_COMPLETE
 */
int
statecmp(POOL_QUERY_STATE s1, POOL_QUERY_STATE s2)
{
	int			ret;

	switch (s2)
	{
		case POOL_UNPARSED:
			ret = (s1 == s2) ? 0 : 1;
			break;
		case POOL_PARSE_COMPLETE:
			if (s1 == POOL_UNPARSED)
				ret = -1;
			else
				ret = (s1 == s2) ? 0 : 1;
			break;
		case POOL_BIND_COMPLETE:
			if (s1 == POOL_UNPARSED || s1 == POOL_PARSE_COMPLETE)
				ret = -1;
			else
				ret = (s1 == s2) ? 0 : 1;
			break;
		case POOL_EXECUTE_COMPLETE:
			ret = (s1 == s2) ? 0 : -1;
			break;
		default:
			ret = -2;
			break;
	}

	return ret;
}

/*
 * Remove READ WRITE option from the packet of START TRANSACTION command.
 * To free the return value is required.
 */
static
char *
remove_read_write(int len, const char *contents, int *rewritten_len)
{
	char	   *rewritten_query;
	char	   *rewritten_contents;
	const char *name;
	const char *stmt;

	rewritten_query = "BEGIN";
	name = contents;
	stmt = contents + strlen(name) + 1;

	*rewritten_len = len - strlen(stmt) + strlen(rewritten_query);
	if (len < *rewritten_len)
	{
		ereport(ERROR,
				(errmsg("invalid message length of transaction packet")));
	}

	rewritten_contents = palloc(*rewritten_len);

	strcpy(rewritten_contents, name);
	strcpy(rewritten_contents + strlen(name) + 1, rewritten_query);
	memcpy(rewritten_contents + strlen(name) + strlen(rewritten_query) + 2,
		   stmt + strlen(stmt) + 1,
		   len - (strlen(name) + strlen(stmt) + 2));

	return rewritten_contents;
}

/*
 * Return true if current query is safe to cache.
 */
bool
pool_is_cache_safe(void)
{
	POOL_SESSION_CONTEXT *sc;

	sc = pool_get_session_context(true);
	if (!sc)
		return false;

	if (pool_is_query_in_progress() && sc->query_context)
	{
		return sc->query_context->is_cache_safe;
	}
	return false;
}

/*
 * Set safe to cache.
 */
void
pool_set_cache_safe(void)
{
	POOL_SESSION_CONTEXT *sc;

	sc = pool_get_session_context(true);
	if (!sc)
		return;

	if (sc->query_context)
	{
		sc->query_context->is_cache_safe = true;
	}
}

/*
 * Unset safe to cache.
 */
void
pool_unset_cache_safe(void)
{
	POOL_SESSION_CONTEXT *sc;

	sc = pool_get_session_context(true);
	if (!sc)
		return;

	if (sc->query_context)
	{
		sc->query_context->is_cache_safe = false;
	}
}

/*
 * Return true if current temporary query cache is exceeded
 */
bool
pool_is_cache_exceeded(void)
{
	POOL_SESSION_CONTEXT *sc;

	sc = pool_get_session_context(true);
	if (!sc)
		return false;

	if (pool_is_query_in_progress() && sc->query_context)
	{
		if (sc->query_context->temp_cache)
			return sc->query_context->temp_cache->is_exceeded;
		return true;
	}
	return false;
}

/*
 * Set current temporary query cache is exceeded
 */
void
pool_set_cache_exceeded(void)
{
	POOL_SESSION_CONTEXT *sc;

	sc = pool_get_session_context(true);
	if (!sc)
		return;

	if (sc->query_context && sc->query_context->temp_cache)
	{
		sc->query_context->temp_cache->is_exceeded = true;
	}
}

/*
 * Unset current temporary query cache is exceeded
 */
void
pool_unset_cache_exceeded(void)
{
	POOL_SESSION_CONTEXT *sc;

	sc = pool_get_session_context(true);
	if (!sc)
		return;

	if (sc->query_context && sc->query_context->temp_cache)
	{
		sc->query_context->temp_cache->is_exceeded = false;
	}
}

/*
 * Return true if one of followings is true
 *
 * SET transaction_read_only TO on
 * SET TRANSACTION READ ONLY
 * SET TRANSACTION CHARACTERISTICS AS TRANSACTION READ ONLY
 *
 * Note that if the node is not a variable statement, returns false.
 */
bool
pool_is_transaction_read_only(Node *node)
{
	ListCell   *list_item;
	bool		ret = false;

	if (!IsA(node, VariableSetStmt))
		return ret;

	/*
	 * SET transaction_read_only TO on
	 */
	if (((VariableSetStmt *) node)->kind == VAR_SET_VALUE &&
		!strcmp(((VariableSetStmt *) node)->name, "transaction_read_only"))
	{
		List	   *options = ((VariableSetStmt *) node)->args;

		foreach(list_item, options)
		{
			A_Const    *v = (A_Const *) lfirst(list_item);

			switch (nodeTag(&v->val))
			{
				case T_String:
					if (!strcasecmp(v->val.sval.sval, "on") ||
						!strcasecmp(v->val.sval.sval, "t") ||
						!strcasecmp(v->val.sval.sval, "true"))
						ret = true;
					break;
				case T_Integer:
					if (v->val.ival.ival)
						ret = true;
				default:
					break;
			}
		}
	}

	/*
	 * SET SESSION CHARACTERISTICS AS TRANSACTION READ ONLY SET TRANSACTION
	 * READ ONLY
	 */
	else if (((VariableSetStmt *) node)->kind == VAR_SET_MULTI &&
			 (!strcmp(((VariableSetStmt *) node)->name, "TRANSACTION") ||
			  !strcmp(((VariableSetStmt *) node)->name, "SESSION CHARACTERISTICS")))
	{
		List	   *options = ((VariableSetStmt *) node)->args;

		foreach(list_item, options)
		{
			DefElem    *opt = (DefElem *) lfirst(list_item);

			if (!strcmp("transaction_read_only", opt->defname))
			{
				bool		read_only;

				read_only = ((A_Const *) opt->arg)->val.ival.ival;
				if (read_only)
				{
					ret = true;
					break;
				}
			}
		}
	}
	return ret;
}

/*
 * Set virtual main node according to the where_to_send map.
 */
static void
set_virtual_main_node(POOL_QUERY_CONTEXT *query_context)
{
	int	i;

	for (i = 0; i < NUM_BACKENDS; i++)
	{
		if (query_context->where_to_send[i])
		{
			query_context->virtual_main_node_id = i;
			break;
		}
	}
}

/*
 * Set load balance info.
 */
static void
set_load_balance_info(POOL_QUERY_CONTEXT *query_context)
{
	POOL_SESSION_CONTEXT *session_context;
	session_context = pool_get_session_context(false);

	if (pool_config->statement_level_load_balance)
		session_context->load_balance_node_id = select_load_balancing_node();

	session_context->query_context->load_balance_node_id = session_context->load_balance_node_id;

	pool_set_node_to_be_sent(query_context,
							 query_context->load_balance_node_id);
}

/*
 * Check if the name is in the list.
 */
static bool
is_in_list(char *name, List *list)
{
	if (name == NULL || list == NIL)
		return false;

	ListCell *cell;
	foreach (cell, list)
	{
		char *cell_name = (char *)lfirst(cell);
		if (strcasecmp(name, cell_name) == 0)
		{
			ereport(DEBUG1,
					(errmsg("[%s] is in list", name)));
			return true;
		}
	}

	return false;
}

/*
 * Check if the relname of SelectStmt is in the temp write list.
 */
static bool
is_select_object_in_temp_write_list(Node *node, void *context)
{
	if (node == NULL || pool_config->disable_load_balance_on_write != DLBOW_DML_ADAPTIVE)
		return false;

	if (IsA(node, RangeVar))
	{
		RangeVar   *rgv = (RangeVar *) node;
		POOL_SESSION_CONTEXT *session_context = pool_get_session_context(false);

		if (pool_config->disable_load_balance_on_write == DLBOW_DML_ADAPTIVE && session_context->is_in_transaction)
		{
			ereport(DEBUG1,
					(errmsg("is_select_object_in_temp_write_list: \"%s\", found relation \"%s\"", (char*)context, rgv->relname)));

			return is_in_list(rgv->relname, session_context->transaction_temp_write_list);
		}
	}

	return raw_expression_tree_walker(node, is_select_object_in_temp_write_list, context);
}

static char*
get_associated_object_from_dml_adaptive_relations
						(char *left_token, DBObjectTypes object_type)
{
	int i;
	char *right_token = NULL;
	if (!pool_config->parsed_dml_adaptive_object_relationship_list)
		return NULL;
	for (i=0 ;; i++)
	{
		if (pool_config->parsed_dml_adaptive_object_relationship_list[i].left_token.name == NULL)
			break;

		if (pool_config->parsed_dml_adaptive_object_relationship_list[i].left_token.object_type != object_type)
			continue;

		if (strcasecmp(pool_config->parsed_dml_adaptive_object_relationship_list[i].left_token.name, left_token) == 0)
		{
			right_token = pool_config->parsed_dml_adaptive_object_relationship_list[i].right_token.name;
			break;
		}
	}
	return right_token;
}

/*
 * Check the object relationship list.
 * If find the name in the list, will add related objects to the transaction temp write list.
 */
void
check_object_relationship_list(char *name, bool is_func_name)
{
	if (pool_config->disable_load_balance_on_write == DLBOW_DML_ADAPTIVE && pool_config->parsed_dml_adaptive_object_relationship_list)
	{
		POOL_SESSION_CONTEXT *session_context = pool_get_session_context(false);

		if (session_context->is_in_transaction)
		{
			char	*right_token =
						get_associated_object_from_dml_adaptive_relations
							(name, is_func_name? OBJECT_TYPE_FUNCTION : OBJECT_TYPE_RELATION);

			if (right_token)
			{
				MemoryContext old_context = MemoryContextSwitchTo(session_context->memory_context);
				session_context->transaction_temp_write_list =
					lappend(session_context->transaction_temp_write_list, pstrdup(right_token));
				MemoryContextSwitchTo(old_context);
			}
		}

	}
}

/*
 * Find the relname and add it to the transaction temp write list.
 */
static bool
add_object_into_temp_write_list(Node *node, void *context)
{
	if (node == NULL)
		return false;

	if (IsA(node, RangeVar))
	{
		RangeVar   *rgv = (RangeVar *) node;

		ereport(DEBUG5,
				(errmsg("add_object_into_temp_write_list: \"%s\", found relation \"%s\"", (char*)context, rgv->relname)));

		POOL_SESSION_CONTEXT *session_context = pool_get_session_context(false);
		MemoryContext old_context = MemoryContextSwitchTo(session_context->memory_context);

		if (!is_in_list(rgv->relname, session_context->transaction_temp_write_list))
		{
			ereport(DEBUG1,
					(errmsg("add \"%s\" into transaction_temp_write_list", rgv->relname)));

			session_context->transaction_temp_write_list = lappend(session_context->transaction_temp_write_list, pstrdup(rgv->relname));
		}

		MemoryContextSwitchTo(old_context);

		check_object_relationship_list(rgv->relname, false);
	}

	return raw_expression_tree_walker(node, add_object_into_temp_write_list, context);
}

/*
 * dml adaptive.
 */
static void
dml_adaptive(Node *node, char *query)
{
	if (pool_config->disable_load_balance_on_write == DLBOW_DML_ADAPTIVE)
	{
		/* Set/Unset transaction status flags */
		if (IsA(node, TransactionStmt))
		{
			POOL_SESSION_CONTEXT *session_context = pool_get_session_context(false);
			MemoryContext old_context = MemoryContextSwitchTo(session_context->memory_context);

			if (is_start_transaction_query(node))
			{
				session_context->is_in_transaction = true;

				if (session_context->transaction_temp_write_list != NIL)
					list_free_deep(session_context->transaction_temp_write_list);

				session_context->transaction_temp_write_list = NIL;
			}
			else if(is_commit_or_rollback_query(node))
			{
				session_context->is_in_transaction = false;

				if (session_context->transaction_temp_write_list != NIL)
					list_free_deep(session_context->transaction_temp_write_list);

				session_context->transaction_temp_write_list = NIL;
			}

			MemoryContextSwitchTo(old_context);
			return;
		}

		/* If non-selectStmt, find the relname and add it to the transaction temp write list. */
		if (!is_select_query(node, query))
			add_object_into_temp_write_list(node, query);

	}
}
