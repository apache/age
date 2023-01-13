/* -*-pgsql-c-*- */
/*
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
 *---------------------------------------------------------------------
 * pool_proto_modules.c: modules corresponding to message protocols.
 * used by pool_process_query()
 *---------------------------------------------------------------------
 */
#include "config.h"
#include <errno.h>

#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#ifdef HAVE_SYS_TIME_H
#include <sys/time.h>
#endif
#ifdef HAVE_SYS_SELECT_H
#include <sys/select.h>
#endif


#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <netinet/in.h>
#include <ctype.h>

#include "pool.h"
#include "rewrite/pool_timestamp.h"
#include "rewrite/pool_lobj.h"
#include "protocol/pool_proto_modules.h"
#include "protocol/pool_process_query.h"
#include "protocol/pool_pg_utils.h"
#include "pool_config.h"
#include "context/pool_session_context.h"
#include "context/pool_query_context.h"
#include "utils/elog.h"
#include "utils/pool_select_walker.h"
#include "utils/pool_relcache.h"
#include "utils/pool_stream.h"
#include "utils/ps_status.h"
#include "utils/pool_signal.h"
#include "utils/pool_ssl.h"
#include "utils/palloc.h"
#include "utils/memutils.h"
#include "query_cache/pool_memqcache.h"
#include "main/pool_internal_comms.h"
#include "pool_config_variables.h"

char	   *copy_table = NULL;	/* copy table name */
char	   *copy_schema = NULL; /* copy table name */
char		copy_delimiter;		/* copy delimiter char */
char	   *copy_null = NULL;	/* copy null string */

/*
 * Non 0 if allow to close internal transaction.  This variable was
 * introduced on 2008/4/3 not to close an internal transaction when
 * Sync message is received after receiving Parse message. This hack
 * is for PHP-PDO.
 */
static int	allow_close_transaction = 1;

int			is_select_pgcatalog = 0;
int			is_select_for_update = 0;	/* 1 if SELECT INTO or SELECT FOR
										 * UPDATE */

/*
 * last query string sent to simpleQuery()
 */
char		query_string_buffer[QUERY_STRING_BUFFER_LEN];

static int	check_errors(POOL_CONNECTION_POOL * backend, int backend_id);
static void generate_error_message(char *prefix, int specific_error, char *query);
static POOL_STATUS parse_before_bind(POOL_CONNECTION * frontend,
									 POOL_CONNECTION_POOL * backend,
									 POOL_SENT_MESSAGE * message,
									 POOL_SENT_MESSAGE * bind_message);
static int *find_victim_nodes(int *ntuples, int nmembers, int main_node, int *number_of_nodes);
static POOL_STATUS close_standby_transactions(POOL_CONNECTION * frontend,
											  POOL_CONNECTION_POOL * backend);

static char *flatten_set_variable_args(const char *name, List *args);
static bool
			process_pg_terminate_backend_func(POOL_QUERY_CONTEXT * query_context);
static void pool_discard_except_sync_and_ready_for_query(POOL_CONNECTION * frontend,
											 POOL_CONNECTION_POOL * backend);
static void si_get_snapshot(POOL_CONNECTION * frontend, POOL_CONNECTION_POOL * backend, Node *node, bool tstate_check);

static bool check_transaction_state_and_abort(char *query, Node *node, POOL_CONNECTION * frontend, POOL_CONNECTION_POOL * backend);

/*
 * This is the workhorse of processing the pg_terminate_backend function to
 * make sure that the use of function should not trigger the backend node failover.
 *
 * The function searches for the pg_terminate_backend() function call in the
 * query parse tree and if the search comes out to be successful,
 * the next step is to locate the pgpool-II child process and a backend node
 * of that connection whose PID is specified in pg_terminate_backend argument.
 *
 * Once the connection is identified, we set the swallow_termination flag of
 * that connection (in shared memory) and also sets the query destination to
 * the same backend that hosts the connection.
 *
 * The function returns true on success, i.e. when the query contains the
 * pg_terminate_backend call and that call refers to the backend
 * connection that belongs to pgpool-II.
 *
 * Note:  Since upon successful return this function has already
 * set the destination backend node for the current query,
 * so for that case pool_where_to_send() should not be called.
 *
 */
static bool
process_pg_terminate_backend_func(POOL_QUERY_CONTEXT * query_context)
{
	/*
	 * locate pg_terminate_backend and get the pid argument, if
	 * pg_terminate_backend is present in the query
	 */
	int			backend_pid = pool_get_terminate_backend_pid(query_context->parse_tree);

	if (backend_pid > 0)
	{
		int			backend_node = 0;
		ConnectionInfo *conn = pool_coninfo_backend_pid(backend_pid, &backend_node);

		if (conn == NULL)
		{
			ereport(LOG,
					(errmsg("found the pg_terminate_backend request for backend pid:%d, but the backend connection does not belong to pgpool-II", backend_pid)));

			/*
			 * we are not able to find the backend connection with the pid so
			 * there is not much left for us to do here
			 */
			return false;
		}
		ereport(LOG,
				(errmsg("found the pg_terminate_backend request for backend pid:%d on backend node:%d", backend_pid, backend_node),
				 errdetail("setting the connection flag")));

		pool_set_connection_will_be_terminated(conn);

		/*
		 * It was the pg_terminate_backend call so send the query to
		 * appropriate backend
		 */
		query_context->pg_terminate_backend_conn = conn;
		pool_force_query_node_to_backend(query_context, backend_node);
		return true;
	}
	return false;
}

/*
 * Process Query('Q') message
 * Query messages include an SQL string.
 */
POOL_STATUS
SimpleQuery(POOL_CONNECTION * frontend,
			POOL_CONNECTION_POOL * backend, int len, char *contents)
{
	static char *sq_config = "pool_status";
	static char *sq_pools = "pool_pools";
	static char *sq_processes = "pool_processes";
	static char *sq_nodes = "pool_nodes";
	static char *sq_version = "pool_version";
	static char *sq_cache = "pool_cache";
	static char *sq_health_check_stats = "pool_health_check_stats";
	static char *sq_backend_stats = "pool_backend_stats";
	int			commit;
	List	   *parse_tree_list;
	Node	   *node = NULL;
	POOL_STATUS status;
	int			lock_kind;
	bool		is_likely_select = false;
	int			specific_error = 0;

	POOL_SESSION_CONTEXT *session_context;
	POOL_QUERY_CONTEXT *query_context;

	bool		error;

	/* Get session context */
	session_context = pool_get_session_context(false);

	/* save last query string for logging purpose */
	strlcpy(query_string_buffer, contents, sizeof(query_string_buffer));

	/* show ps status */
	query_ps_status(contents, backend);

	/* log query to log file if necessary */
	if (pool_config->log_statement)
		ereport(LOG, (errmsg("statement: %s", contents)));

	/*
	 * Fetch memory cache if possible
	 */
	if (pool_config->memory_cache_enabled)
	    is_likely_select = pool_is_likely_select(contents);

	/*
	 * If memory query cache enabled and the query seems to be a SELECT use
	 * query cache if possible. However if we are in an explicit transaction
	 * and we had writing query before, we should not use query cache. This
	 * means that even the writing query is not anything related to the table
	 * which is used the SELECT, we do not use cache. Of course we could
	 * analyze the SELECT to see if it uses the table modified in the
	 * transaction, but it will need parsing query and accessing to system
	 * catalog, which will add significant overhead. Moreover if we are in
	 * aborted transaction, commands should be ignored, so we should not use
	 * query cache.
	 */
	if (pool_config->memory_cache_enabled && is_likely_select &&
		!pool_is_writing_transaction() &&
		TSTATE(backend, MAIN_REPLICA ? PRIMARY_NODE_ID : REAL_MAIN_NODE_ID) != 'E')
	{
		bool		foundp;

		/*
		 * If the query is SELECT from table to cache, try to fetch cached
		 * result.
		 */
		status = pool_fetch_from_memory_cache(frontend, backend, contents, &foundp);

		if (status != POOL_CONTINUE)
			return status;

		if (foundp)
		{
			pool_ps_idle_display(backend);
			pool_set_skip_reading_from_backends();
			pool_stats_count_up_num_cache_hits();
			return POOL_CONTINUE;
		}
	}

	/* Create query context */
	query_context = pool_init_query_context();
	MemoryContext old_context = MemoryContextSwitchTo(query_context->memory_context);

	/* parse SQL string */
	parse_tree_list = raw_parser(contents, RAW_PARSE_DEFAULT, len, &error, !REPLICATION);

	if (parse_tree_list == NIL)
	{
		/* is the query empty? */
		if (*contents == '\0' || *contents == ';' || error == false)
		{
			/*
			 * JBoss sends empty queries for checking connections. We replace
			 * the empty query with SELECT command not to affect load balance.
			 * [Pgpool-general] Confused about JDBC and load balancing
			 */
			parse_tree_list = get_dummy_read_query_tree();
		}
		else
		{
			/*
			 * Unable to parse the query. Probably syntax error or the query
			 * is too new and our parser cannot understand. Treat as if it
			 * were an DELETE command. Note that the DELETE command does not
			 * execute, instead the original query will be sent to backends,
			 * which may or may not cause an actual syntax errors. The command
			 * will be sent to all backends in replication mode or
			 * primary in native replication mode.
			 */
			if (!strcmp(remote_host, "[local]"))
			{
				ereport(LOG,
						(errmsg("Unable to parse the query: \"%s\" from local client", contents)));
			}
			else
			{
				ereport(LOG,
						(errmsg("Unable to parse the query: \"%s\" from client %s(%s)", contents, remote_host, remote_port)));
			}
			parse_tree_list = get_dummy_write_query_tree();
			query_context->is_parse_error = true;
		}
	}
	MemoryContextSwitchTo(old_context);

	if (parse_tree_list != NIL)
	{
		node = raw_parser2(parse_tree_list);

		/*
		 * Start query context
		 */
		pool_start_query(query_context, contents, len, node);

		/*
		 * Check if the transaction is in abort status. If so, we do nothing
		 * and just return an error message to frontend, execpt for
		 * transaction COMMIT or ROLLBACK (TO) command.
		 */
		if (check_transaction_state_and_abort(contents, node, frontend, backend) == false)
		{
			pool_ps_idle_display(backend);
			pool_query_context_destroy(query_context);
			pool_set_skip_reading_from_backends();
			return POOL_CONTINUE;
		}

		/*
		 * Create PostgreSQL version cache.  Since the provided query might
		 * cause a syntax error, we want to issue "SELECT version()" which is
		 * called inside Pgversion() here.
		 */
		Pgversion(backend);

		/*
		 * If the query is DROP DATABASE, after executing it, cache files
		 * directory must be discarded. So we have to get the DB's oid before
		 * it will be DROPped.
		 */
		if (pool_config->memory_cache_enabled && is_drop_database(node))
		{
			DropdbStmt *stmt = (DropdbStmt *) node;

			query_context->dboid = pool_get_database_oid_from_dbname(stmt->dbname);
			if (query_context->dboid != 0)
			{
				ereport(DEBUG1,
						(errmsg("DB's oid to discard its cache directory: dboid = %d", query_context->dboid)));
			}
		}

		/*
		 * Check if multi statement query
		 */
		if (parse_tree_list && list_length(parse_tree_list) > 1)
		{
			query_context->is_multi_statement = true;
		}
		else
		{
			query_context->is_multi_statement = false;
		}

		/*
		 * check COPY FROM STDIN if true, set copy_* variable
		 */
		check_copy_from_stdin(node);

		if (IsA(node, PgpoolVariableShowStmt))
		{
			VariableShowStmt *vnode = (VariableShowStmt *) node;

			report_config_variable(frontend, backend, vnode->name);

			pool_ps_idle_display(backend);
			pool_query_context_destroy(query_context);
			pool_set_skip_reading_from_backends();
			return POOL_CONTINUE;
		}

		if (IsA(node, PgpoolVariableSetStmt))
		{
			VariableSetStmt *vnode = (VariableSetStmt *) node;
			const char *value = NULL;

			if (vnode->kind == VAR_SET_VALUE)
			{
				value = flatten_set_variable_args("name", vnode->args);
			}
			if (vnode->kind == VAR_RESET_ALL)
			{
				reset_all_variables(frontend, backend);
			}
			else
			{
				set_config_option_for_session(frontend, backend, vnode->name, value);
			}

			pool_ps_idle_display(backend);
			pool_query_context_destroy(query_context);
			pool_set_skip_reading_from_backends();
			return POOL_CONTINUE;
		}

		/* status reporting? */
		if (IsA(node, VariableShowStmt))
		{
			bool		is_valid_show_command = false;
			VariableShowStmt *vnode = (VariableShowStmt *) node;

			if (!strcmp(sq_config, vnode->name))
			{
				is_valid_show_command = true;
				ereport(DEBUG1,
						(errmsg("SimpleQuery"),
						 errdetail("config reporting")));
				config_reporting(frontend, backend);
			}
			else if (!strcmp(sq_pools, vnode->name))
			{
				is_valid_show_command = true;
				ereport(DEBUG1,
						(errmsg("SimpleQuery"),
						 errdetail("pools reporting")));

				pools_reporting(frontend, backend);
			}
			else if (!strcmp(sq_processes, vnode->name))
			{
				is_valid_show_command = true;
				ereport(DEBUG1,
						(errmsg("SimpleQuery"),
						 errdetail("processes reporting")));
				processes_reporting(frontend, backend);
			}
			else if (!strcmp(sq_nodes, vnode->name))
			{
				is_valid_show_command = true;
				ereport(DEBUG1,
						(errmsg("SimpleQuery"),
						 errdetail("nodes reporting")));
				nodes_reporting(frontend, backend);
			}
			else if (!strcmp(sq_version, vnode->name))
			{
				is_valid_show_command = true;
				ereport(DEBUG1,
						(errmsg("SimpleQuery"),
						 errdetail("version reporting")));
				version_reporting(frontend, backend);
			}
			else if (!strcmp(sq_cache, vnode->name))
			{
				is_valid_show_command = true;
				ereport(DEBUG1,
						(errmsg("SimpleQuery"),
						 errdetail("cache reporting")));
				cache_reporting(frontend, backend);
			}
			else if (!strcmp(sq_health_check_stats, vnode->name))
			{
				is_valid_show_command = true;
				ereport(DEBUG1,
						(errmsg("SimpleQuery"),
						 errdetail("health check stats")));
				show_health_check_stats(frontend, backend);
			}

			else if (!strcmp(sq_backend_stats, vnode->name))
			{
				is_valid_show_command = true;
				ereport(DEBUG1,
						(errmsg("SimpleQuery"),
						 errdetail("backend stats")));
				show_backend_stats(frontend, backend);
			}

			if (is_valid_show_command)
			{
				pool_ps_idle_display(backend);
				pool_query_context_destroy(query_context);
				pool_set_skip_reading_from_backends();
				return POOL_CONTINUE;
			}
		}

		/*
		 * If the table is to be cached, set is_cache_safe TRUE and register
		 * table oids.
		 */
		if (pool_config->memory_cache_enabled && query_context->is_multi_statement == false)
		{
			bool		is_select_query = false;
			int			num_oids;
			int		   *oids;
			int			i;

			/* Check if the query is actually SELECT */
			if (is_likely_select && IsA(node, SelectStmt))
			{
				is_select_query = true;
			}

			/* Switch the flag of is_cache_safe in session_context */
			if (is_select_query && !query_context->is_parse_error &&
				pool_is_allow_to_cache(query_context->parse_tree,
									   query_context->original_query))
			{
				pool_set_cache_safe();
			}
			else
			{
				pool_unset_cache_safe();
			}

			/*
			 * If table is to be cached and the query is DML, save the table
			 * oid
			 */
			if (!query_context->is_parse_error)
			{
				num_oids = pool_extract_table_oids(node, &oids);

				if (num_oids > 0)
				{
					/* Save to oid buffer */
					for (i = 0; i < num_oids; i++)
					{
						pool_add_dml_table_oid(oids[i]);
					}
				}
			}
		}

		/*
		 * If we are in SI mode, start an internal transaction if needed.
		 */
		if (pool_config->backend_clustering_mode == CM_SNAPSHOT_ISOLATION)
		{
			start_internal_transaction(frontend, backend, node);
		}

		/*
		 * From now on it is possible that query is actually sent to backend.
		 * So we need to acquire snapshot while there's no committing backend
		 * in snapshot isolation mode except while processing reset queries.
		 * For this purpose, we send a query to know whether the transaction
		 * is READ ONLY or not.  Sending actual user's query is not possible
		 * because it might cause rw-conflict, which in turn causes a
		 * deadlock.
		 */
		si_get_snapshot(frontend, backend, node,
						!INTERNAL_TRANSACTION_STARTED(backend, MAIN_NODE_ID));

		/*
		 * pg_terminate function needs special handling, process it if the
		 * query contains one, otherwise use pool_where_to_send() to decide
		 * destination backend node for the query
		 */
		if (process_pg_terminate_backend_func(query_context) == false)
		{
			/*
			 * Decide where to send query
			 */
			pool_where_to_send(query_context, query_context->original_query,
							   query_context->parse_tree);
		}

		/*
		 * if this is DROP DATABASE command, send USR1 signal to parent and
		 * ask it to close all idle connections. XXX This is overkill. It
		 * would be better to close the idle connection for the database which
		 * DROP DATABASE command tries to drop. This is impossible at this
		 * point, since we have no way to pass such info to other processes.
		 */
		if (is_drop_database(node))
		{
			struct timeval stime;

			stime.tv_usec = 0;
			stime.tv_sec = 5;	/* XXX give arbitrary time to allow
									 * closing idle connections */

			ereport(DEBUG1,
					(errmsg("Query: sending SIGUSR1 signal to parent")));

			ignore_sigusr1 = 1;	/* disable SIGUSR1 handler */
			close_idle_connections();

			/*
			 * We need to loop over here since we might get some signals while
			 * sleeping
			 */
			for (;;)
			{
				int sts;

				errno = 0;
				sts = select(0, NULL, NULL, NULL, &stime);
				if (stime.tv_usec == 0 && stime.tv_sec == 0)
					break;
				if (sts != 0 && errno != EINTR)
				{
					elog(DEBUG1, "select(2) returns error: %s", strerror(errno));
					break;
				}
			}

			ignore_sigusr1 = 0;
		}

		/*---------------------------------------------------------------------------
		 * determine if we need to lock the table
		 * to keep SERIAL data consistency among servers
		 * conditions:
		 * - replication is enabled
		 * - protocol is V3
		 * - statement is INSERT
		 * - either "INSERT LOCK" comment exists or insert_lock directive specified
		 *---------------------------------------------------------------------------
		 */
		if (!RAW_MODE && !SL_MODE)
		{
			/*
			 * If there's only one node to send the command, there's no point
			 * to start a transaction.
			 */
			if (pool_multi_node_to_be_sent(query_context))
			{
				/* start a transaction if needed */
				start_internal_transaction(frontend, backend, (Node *) node);

				/* check if need lock */
				lock_kind = need_insert_lock(backend, contents, node);
				if (lock_kind)
				{
					/* if so, issue lock command */
					status = insert_lock(frontend, backend, contents, (InsertStmt *) node, lock_kind);
					if (status != POOL_CONTINUE)
					{
						pool_query_context_destroy(query_context);
						return status;
					}
				}
			}
		}
	}

	if (MAJOR(backend) == PROTO_MAJOR_V2 && is_start_transaction_query(node))
	{
		int			i;

		for (i = 0; i < NUM_BACKENDS; i++)
		{
			if (VALID_BACKEND(i))
				TSTATE(backend, i) = 'T';
		}
	}

	if (node)
	{
		POOL_SENT_MESSAGE *msg = NULL;

		if (IsA(node, PrepareStmt))
		{
			msg = pool_create_sent_message('Q', len, contents, 0,
										   ((PrepareStmt *) node)->name,
										   query_context);
			session_context->uncompleted_message = msg;
		}
		else
			session_context->uncompleted_message = NULL;
	}


	if (!RAW_MODE)
	{
		/* check if query is "COMMIT" or "ROLLBACK" */
		commit = is_commit_or_rollback_query(node);

		/*
		 * Query is not commit/rollback
		 */
		if (!commit)
		{
			char	   *rewrite_query = NULL;

			if (node)
			{
				POOL_SENT_MESSAGE *msg = NULL;

				if (IsA(node, PrepareStmt))
				{
					msg = session_context->uncompleted_message;
				}
				else if (IsA(node, ExecuteStmt))
				{
					msg = pool_get_sent_message('Q', ((ExecuteStmt *) node)->name, POOL_SENT_MESSAGE_CREATED);
					if (!msg)
						msg = pool_get_sent_message('P', ((ExecuteStmt *) node)->name, POOL_SENT_MESSAGE_CREATED);
				}

				/* rewrite `now()' to timestamp literal */
				if (!is_select_query(node, query_context->original_query) ||
					pool_has_function_call(node) || pool_config->replicate_select)
					rewrite_query = rewrite_timestamp(backend, query_context->parse_tree, false, msg);

				/*
				 * If the query is BEGIN READ WRITE or BEGIN ... SERIALIZABLE
				 * in native replication mode, we send BEGIN to standbys
				 * instead. original_query which is BEGIN READ WRITE is sent
				 * to primary. rewritten_query which is BEGIN is sent to
				 * standbys.
				 */
				if (pool_need_to_treat_as_if_default_transaction(query_context))
				{
					rewrite_query = pstrdup("BEGIN");
				}

				if (rewrite_query != NULL)
				{
					query_context->rewritten_query = rewrite_query;
					query_context->rewritten_length = strlen(rewrite_query) + 1;
				}
			}

			/*
			 * Optimization effort: If there's only one session, we do not
			 * need to wait for the main node's response, and could execute
			 * the query concurrently.  In snapshot isolation mode we cannot
			 * do this optimization because we need to wait for main node's
			 * response first.
			 */
			if (pool_config->num_init_children == 1 &&
				pool_config->backend_clustering_mode != CM_SNAPSHOT_ISOLATION)
			{
				/* Send query to all DB nodes at once */
				status = pool_send_and_wait(query_context, 0, 0);
				/* free_parser(); */
				return status;
			}

			/* Send the query to main node */
			pool_send_and_wait(query_context, 1, MAIN_NODE_ID);

			/* Check specific errors */
			specific_error = check_errors(backend, MAIN_NODE_ID);
			if (specific_error)
			{
				/* log error message */
				generate_error_message("SimpleQuery: ", specific_error, contents);
			}
		}

		if (specific_error)
		{
			char		msg[1024] = POOL_ERROR_QUERY;	/* large enough */
			int			len = strlen(msg) + 1;

			memset(msg + len, 0, sizeof(int));

			/* send query to other nodes */
			query_context->rewritten_query = msg;
			query_context->rewritten_length = len;
			pool_send_and_wait(query_context, -1, MAIN_NODE_ID);
		}
		else
		{
			/*
			 * If commit command and Snapshot Isolation mode, wait for until
			 * snapshot prepared.
			 */
			if (commit && pool_config->backend_clustering_mode == CM_SNAPSHOT_ISOLATION)
			{
				si_commit_request();
			}

			/*
			 * Send the query to other than main node.
			 */
			pool_send_and_wait(query_context, -1, MAIN_NODE_ID);

		}

		/*
		 * Send "COMMIT" or "ROLLBACK" to only main node if query is
		 * "COMMIT" or "ROLLBACK"
		 */
		if (commit)
		{
			pool_send_and_wait(query_context, 1, MAIN_NODE_ID);

			/*
			 * If we are in the snapshot isolation mode, we need to declare
			 * that commit has been done. This would wake up other children
			 * waiting for acquiring snapshot.
			 */
			if (pool_config->backend_clustering_mode == CM_SNAPSHOT_ISOLATION)
			{
				si_commit_done();
				session_context->transaction_read_only = false;
			}
		}

	}
	else
	{
		pool_send_and_wait(query_context, 1, MAIN_NODE_ID);
	}

	return POOL_CONTINUE;
}

/*
 * process EXECUTE (V3 only)
 */
POOL_STATUS
Execute(POOL_CONNECTION * frontend, POOL_CONNECTION_POOL * backend,
		int len, char *contents)
{
	int			commit = 0;
	char	   *query = NULL;
	Node	   *node;
	int			specific_error = 0;
	POOL_SESSION_CONTEXT *session_context;
	POOL_QUERY_CONTEXT *query_context;
	POOL_SENT_MESSAGE *bind_msg;
	bool		foundp = false;

	/* Get session context */
	session_context = pool_get_session_context(false);

	if (pool_config->log_client_messages)
		ereport(LOG,
				(errmsg("Execute message from frontend."),
				 errdetail("portal: \"%s\"", contents)));
	ereport(DEBUG2,
			(errmsg("Execute: portal name <%s>", contents)));

	bind_msg = pool_get_sent_message('B', contents, POOL_SENT_MESSAGE_CREATED);
	if (!bind_msg)
		ereport(FATAL,
				(return_code(2),
				 errmsg("unable to Execute"),
				 errdetail("unable to get bind message")));

	if (!bind_msg->query_context)
		ereport(FATAL,
				(return_code(2),
				 errmsg("unable to Execute"),
				 errdetail("unable to get query context")));

	if (!bind_msg->query_context->original_query)
		ereport(FATAL,
				(return_code(2),
				 errmsg("unable to Execute"),
				 errdetail("unable to get original query")));

	if (!bind_msg->query_context->parse_tree)
		ereport(FATAL,
				(return_code(2),
				 errmsg("unable to Execute"),
				 errdetail("unable to get parse tree")));

	query_context = bind_msg->query_context;
	node = bind_msg->query_context->parse_tree;
	query = bind_msg->query_context->original_query;

	strlcpy(query_string_buffer, query, sizeof(query_string_buffer));

	ereport(DEBUG2, (errmsg("Execute: query string = <%s>", query)));

	ereport(DEBUG1, (errmsg("Execute: pool_is_writing_transaction: %d TSTATE: %c",
							pool_is_writing_transaction(),
							TSTATE(backend, MAIN_REPLICA ? PRIMARY_NODE_ID : REAL_MAIN_NODE_ID))));

	/* log query to log file if necessary */
	if (pool_config->log_statement)
		ereport(LOG, (errmsg("statement: %s", query)));

	/*
	 * Fetch memory cache if possible
	 */
	if (pool_config->memory_cache_enabled && !pool_is_writing_transaction() &&
		(TSTATE(backend, MAIN_REPLICA ? PRIMARY_NODE_ID : REAL_MAIN_NODE_ID) != 'E')
		&& pool_is_likely_select(query))
	{
		POOL_STATUS status;
		char	   *search_query = NULL;
		int			len;

#define STR_ALLOC_SIZE 1024
		ereport(DEBUG1, (errmsg("Execute: pool_is_likely_select: true pool_is_writing_transaction: %d TSTATE: %c",
								pool_is_writing_transaction(),
								TSTATE(backend, MAIN_REPLICA ? PRIMARY_NODE_ID : REAL_MAIN_NODE_ID))));

		len = strlen(query) + 1;
		search_query = MemoryContextStrdup(query_context->memory_context, query);

		ereport(DEBUG1, (errmsg("Execute: checking cache fetch condition")));

		/*
		 * Add bind message's info to query to search.
		 */
		if (query_context->is_cache_safe && bind_msg->param_offset && bind_msg->contents)
		{
			/* Extract binary contents from bind message */
			char	   *query_in_bind_msg = bind_msg->contents + bind_msg->param_offset;
			char		hex_str[4]; /* 02X chars + white space + null end */
			int			i;
			int			alloc_len;

			alloc_len = (len / STR_ALLOC_SIZE + 1) * STR_ALLOC_SIZE;
			search_query = repalloc(search_query, alloc_len);

			for (i = 0; i < bind_msg->len - bind_msg->param_offset; i++)
			{
				int			hexlen;

				snprintf(hex_str, sizeof(hex_str), (i == 0) ? " %02X" : "%02X", 0xff & query_in_bind_msg[i]);
				hexlen = strlen(hex_str);

				if ((len + hexlen) >= alloc_len)
				{
					alloc_len += STR_ALLOC_SIZE;
					search_query = repalloc(search_query, alloc_len);
				}
				strcat(search_query, hex_str);
				len += hexlen;
			}

			/*
			 * If bind message is sent again to an existing prepared statement,
			 * it is possible that query_w_hex remains. Before setting newly
			 * allocated query_w_hex's pointer to the query context, free the
			 * previously allocated memory.
			 */
			if (query_context->query_w_hex)
			{
				pfree(query_context->query_w_hex);
			}
			query_context->query_w_hex = search_query;

			/*
			 * When a transaction is committed,
			 * query_context->temp_cache->query is used to create md5 hash to
			 * search for query cache. So overwrite the query text in temp
			 * cache to the one with the hex of bind message. If not, md5 hash
			 * will be created by the query text without bind message, and it
			 * will happen to find cache never or to get a wrong result.
			 *
			 * However, It is possible that temp_cache does not exist.
			 * Consider following scenario: - In the previous execute cache is
			 * overflowed, and temp_cache discarded. - In the subsequent
			 * bind/execute uses the same portal
			 */
			if (query_context->temp_cache)
			{
				pfree(query_context->temp_cache->query);
				query_context->temp_cache->query = MemoryContextStrdup(session_context->memory_context, search_query);
			}
		}

		/*
		 * If the query is SELECT from table to cache, try to fetch cached
		 * result.
		 */
		status = pool_fetch_from_memory_cache(frontend, backend, search_query, &foundp);

		if (status != POOL_CONTINUE)
			return status;

		if (foundp)
		{
#ifdef DEBUG
			extern bool stop_now;
#endif
			pool_stats_count_up_num_cache_hits();
			query_context->skip_cache_commit = true;
#ifdef DEBUG
			stop_now = true;
#endif

			if (!SL_MODE || !pool_is_doing_extended_query_message())
			{
				pool_set_skip_reading_from_backends();
				pool_stats_count_up_num_cache_hits();
				pool_unset_query_in_progress();
				return POOL_CONTINUE;
			}
		}
		else
			query_context->skip_cache_commit = false;
	}

	/* show ps status */
	query_ps_status(query, backend);

	session_context->query_context = query_context;

	/*
	 * Calling pool_where_to_send here is dangerous because the node
	 * parse/bind has been sent could be change by pool_where_to_send() and it
	 * leads to "portal not found" etc. errors.
	 */

	/* check if query is "COMMIT" or "ROLLBACK" */
	commit = is_commit_or_rollback_query(node);

	if (!SL_MODE)
	{
		/*
		 * Query is not commit/rollback
		 */
		if (!commit)
		{
			/* Send the query to main node */
			pool_extended_send_and_wait(query_context, "E", len, contents, 1, MAIN_NODE_ID, false);

			/* Check specific errors */
			specific_error = check_errors(backend, MAIN_NODE_ID);
			if (specific_error)
			{
				/* log error message */
				generate_error_message("Execute: ", specific_error, contents);
			}
		}

		if (specific_error)
		{
			char		msg[1024] = "pgpool_error_portal";	/* large enough */
			int			len = strlen(msg);

			memset(msg + len, 0, sizeof(int));

			/* send query to other nodes */
			pool_extended_send_and_wait(query_context, "E", len, msg, -1, MAIN_NODE_ID, false);
		}
		else
		{
			/*
			 * If commit command and Snapshot Isolation mode, wait for until
			 * snapshot prepared.
			 */
			if (commit && pool_config->backend_clustering_mode == CM_SNAPSHOT_ISOLATION)
			{
				si_commit_request();
			}

			pool_extended_send_and_wait(query_context, "E", len, contents, -1, MAIN_NODE_ID, false);
		}

		/*
		 * send "COMMIT" or "ROLLBACK" to only main node if query is
		 * "COMMIT" or "ROLLBACK"
		 */
		if (commit)
		{
			pool_extended_send_and_wait(query_context, "E", len, contents, 1, MAIN_NODE_ID, false);

			/*
			 * If we are in the snapshot isolation mode, we need to declare
			 * that commit has been done. This would wake up other children
			 * waiting for acquiring snapshot.
			 */
			if (pool_config->backend_clustering_mode == CM_SNAPSHOT_ISOLATION)
			{
				si_commit_done();
				session_context->transaction_read_only = false;
			}
		}
	}
	else						/* streaming replication mode */
	{
		POOL_PENDING_MESSAGE *pmsg;

		if (!foundp)
		{
			pool_extended_send_and_wait(query_context, "E", len, contents, 1, MAIN_NODE_ID, true);
			pool_extended_send_and_wait(query_context, "E", len, contents, -1, MAIN_NODE_ID, true);
		}

		/* Add pending message */
		pmsg = pool_pending_message_create('E', len, contents);
		pool_pending_message_dest_set(pmsg, query_context);
		pool_pending_message_query_set(pmsg, query_context);
		pool_pending_message_add(pmsg);
		pool_pending_message_free_pending_message(pmsg);

		/* Various take care at the transaction start */
		handle_query_context(backend);

		/*
		 * Take care of "writing transaction" flag.
		 */
		if ((!is_select_query(node, query) || pool_has_function_call(node)) &&
			 !is_start_transaction_query(node) &&
			 !is_commit_or_rollback_query(node))
		{
			ereport(DEBUG1,
					(errmsg("Execute: TSTATE:%c",
							TSTATE(backend, MAIN_REPLICA ? PRIMARY_NODE_ID : REAL_MAIN_NODE_ID))));

			/*
			 * If the query was not READ SELECT, and we are in an explicit
			 * transaction, remember that we had a write query in this
			 * transaction.
			 */
			if (TSTATE(backend, MAIN_REPLICA ? PRIMARY_NODE_ID : REAL_MAIN_NODE_ID) == 'T' ||
				pool_config->disable_load_balance_on_write == DLBOW_ALWAYS)
			{
				/*
				 * However, if the query is "SET TRANSACTION READ ONLY" or its
				 * variant, don't set it.
				 */
				if (!pool_is_transaction_read_only(node))
				{
					pool_set_writing_transaction();
				}
			}
		}
		pool_unset_query_in_progress();
	}

	return POOL_CONTINUE;
}

/*
 * process Parse (V3 only)
 */
POOL_STATUS
Parse(POOL_CONNECTION * frontend, POOL_CONNECTION_POOL * backend,
	  int len, char *contents)
{
	int			deadlock_detected = 0;
	int			insert_stmt_with_lock = 0;
	char	   *name;
	char	   *stmt;
	List	   *parse_tree_list;
	Node	   *node = NULL;
	POOL_SENT_MESSAGE *msg;
	POOL_STATUS status;
	POOL_SESSION_CONTEXT *session_context;
	POOL_QUERY_CONTEXT *query_context;

	bool		error;

	/* Get session context */
	session_context = pool_get_session_context(false);

	/* Create query context */
	query_context = pool_init_query_context();

	ereport(DEBUG1,
			(errmsg("Parse: statement name <%s>", contents)));

	name = contents;
	stmt = contents + strlen(contents) + 1;

	if (pool_config->log_client_messages)
		ereport(LOG,
				(errmsg("Parse message from frontend."),
				 errdetail("statement: \"%s\", query: \"%s\"", name, stmt)));

	/* parse SQL string */
	MemoryContext old_context = MemoryContextSwitchTo(query_context->memory_context);

	parse_tree_list = raw_parser(stmt, RAW_PARSE_DEFAULT, strlen(stmt),&error,!REPLICATION);

	if (parse_tree_list == NIL)
	{
		/* is the query empty? */
		if (*stmt == '\0' || *stmt == ';' || error == false)
		{
			/*
			 * JBoss sends empty queries for checking connections. We replace
			 * the empty query with SELECT command not to affect load balance.
			 * [Pgpool-general] Confused about JDBC and load balancing
			 */
			parse_tree_list = get_dummy_read_query_tree();
		}
		else
		{
			/*
			 * Unable to parse the query. Probably syntax error or the query
			 * is too new and our parser cannot understand. Treat as if it
			 * were an DELETE command. Note that the DELETE command does not
			 * execute, instead the original query will be sent to backends,
			 * which may or may not cause an actual syntax errors. The command
			 * will be sent to all backends in replication mode or
			 * primary in native replication mode.
			 */
			if (!strcmp(remote_host, "[local]"))
			{
				ereport(LOG,
						(errmsg("Unable to parse the query: \"%s\" from local client", stmt)));
			}
			else
			{
				ereport(LOG,
						(errmsg("Unable to parse the query: \"%s\" from client %s(%s)", stmt, remote_host, remote_port)));
			}
			parse_tree_list = get_dummy_write_query_tree();
			query_context->is_parse_error = true;
		}
	}
	MemoryContextSwitchTo(old_context);

	if (parse_tree_list != NIL)
	{
		/* Save last query string for logging purpose */
		snprintf(query_string_buffer, sizeof(query_string_buffer), "Parse: %s", stmt);

		node = raw_parser2(parse_tree_list);

		/*
		 * If replication mode, check to see what kind of insert lock is
		 * necessary.
		 */
		if (REPLICATION)
			insert_stmt_with_lock = need_insert_lock(backend, stmt, node);

		/*
		 * Start query context
		 */
		pool_start_query(query_context, stmt, strlen(stmt) + 1, node);

		/*
		 * Create PostgreSQL version cache.  Since the provided query might
		 * cause a syntax error, we want to issue "SELECT version()" which is
		 * called inside Pgversion() here.
		 */
		Pgversion(backend);

		msg = pool_create_sent_message('P', len, contents, 0, name, query_context);

		session_context->uncompleted_message = msg;

		/*
		 * If the table is to be cached, set is_cache_safe TRUE and register
		 * table oids.
		 */
		if (pool_config->memory_cache_enabled)
		{
			bool		is_likely_select = false;
			bool		is_select_query = false;
			int			num_oids;
			int		   *oids;
			int			i;

			/* Check if the query is actually SELECT */
			is_likely_select = pool_is_likely_select(query_context->original_query);
			if (is_likely_select && IsA(node, SelectStmt))
			{
				is_select_query = true;
			}

			/* Switch the flag of is_cache_safe in session_context */
			if (is_select_query && !query_context->is_parse_error &&
				pool_is_allow_to_cache(query_context->parse_tree,
									   query_context->original_query))
			{
				pool_set_cache_safe();
			}
			else
			{
				pool_unset_cache_safe();
			}

			/*
			 * If table is to be cached and the query is DML, save the table
			 * oid
			 */
			if (!is_select_query && !query_context->is_parse_error)
			{
				num_oids = pool_extract_table_oids(node, &oids);

				if (num_oids > 0)
				{
					/* Save to oid buffer */
					for (i = 0; i < num_oids; i++)
					{
						pool_add_dml_table_oid(oids[i]);
					}
				}
			}
		}

		/*
		 * If we are in SI mode, start an internal transaction if needed.
		 */
		if (pool_config->backend_clustering_mode == CM_SNAPSHOT_ISOLATION)
		{
			start_internal_transaction(frontend, backend, node);
		}

		/*
		 * Get snapshot if needed.
		 */
		si_get_snapshot(frontend, backend, node,
						!INTERNAL_TRANSACTION_STARTED(backend, MAIN_NODE_ID));

		/*
		 * Decide where to send query
		 */
		pool_where_to_send(query_context, query_context->original_query,
						   query_context->parse_tree);

		if (pool_config->disable_load_balance_on_write == DLBOW_DML_ADAPTIVE && strlen(name) != 0)
			pool_setall_node_to_be_sent(query_context);

		if (REPLICATION)
		{
			char	   *rewrite_query;
			bool		rewrite_to_params = true;

			/*
			 * rewrite `now()'. if stmt is unnamed, we rewrite `now()' to
			 * timestamp constant. else we rewrite `now()' to params and
			 * expand that at Bind message.
			 */
			if (*name == '\0')
				rewrite_to_params = false;
			msg->num_tsparams = 0;
			rewrite_query = rewrite_timestamp(backend, node, rewrite_to_params, msg);
			if (rewrite_query != NULL)
			{
				int			alloc_len = len - strlen(stmt) + strlen(rewrite_query);

				/* switch memory context */
				MemoryContext oldcxt = MemoryContextSwitchTo(session_context->memory_context);

				contents = repalloc(msg->contents, alloc_len);
				MemoryContextSwitchTo(oldcxt);

				strcpy(contents, name);
				strcpy(contents + strlen(name) + 1, rewrite_query);
				memcpy(contents + strlen(name) + strlen(rewrite_query) + 2,
					   stmt + strlen(stmt) + 1,
					   len - (strlen(name) + strlen(stmt) + 2));

				len = alloc_len;
				name = contents;
				stmt = contents + strlen(name) + 1;
				ereport(DEBUG1,
						(errmsg("Parse: rewrite query name:\"%s\" statement:\"%s\" len=%d", name, stmt, len)));

				msg->len = len;
				msg->contents = contents;

				query_context->rewritten_query = rewrite_query;
			}
		}

		/*
		 * If the query is BEGIN READ WRITE in main replica mode, we send
		 * BEGIN instead of it to standbys. original_query which is
		 * BEGIN READ WRITE is sent to primary. rewritten_query which is BEGIN
		 * is sent to standbys.
		 */
		if (is_start_transaction_query(query_context->parse_tree) &&
			is_read_write((TransactionStmt *) query_context->parse_tree) &&
			MAIN_REPLICA)
		{
			query_context->rewritten_query = pstrdup("BEGIN");
		}
	}

	/*
	 * If not in streaming or logical replication mode, send "SYNC" message if not in a transaction.
	 */
	if (!SL_MODE)
	{
		char		kind;

		if (TSTATE(backend, MAIN_NODE_ID) != 'T')
		{
			int			i;

			/* synchronize transaction state */
			for (i = 0; i < NUM_BACKENDS; i++)
			{
				if (!VALID_BACKEND(i))
					continue;
				/* send sync message */
				send_extended_protocol_message(backend, i, "S", 0, "");
			}

			kind = pool_read_kind(backend);
			if (kind != 'Z')
				ereport(ERROR,
						(errmsg("unable to parse the query"),
						 errdetail("invalid read kind \"%c\" returned from backend %d after Sync message sent", kind, i)));

			/*
			 * SYNC message returns "Ready for Query" message.
			 */
			if (ReadyForQuery(frontend, backend, 0, false) != POOL_CONTINUE)
			{
				pool_query_context_destroy(query_context);
				return POOL_END;
			}

			/*
			 * set in_progress flag, because ReadyForQuery unset it.
			 * in_progress flag influences VALID_BACKEND.
			 */
			if (!pool_is_query_in_progress())
				pool_set_query_in_progress();
		}

		if (is_strict_query(query_context->parse_tree))
		{
			start_internal_transaction(frontend, backend, query_context->parse_tree);
			allow_close_transaction = 1;
		}

		if (insert_stmt_with_lock)
		{
			/* start a transaction if needed and lock the table */
			status = insert_lock(frontend, backend, stmt, (InsertStmt *) query_context->parse_tree, insert_stmt_with_lock);
			if (status != POOL_CONTINUE)
				ereport(ERROR,
						(errmsg("unable to parse the query"),
						 errdetail("unable to get insert lock")));

		}
	}

	if (REPLICATION || SLONY)
	{
		/*
		 * We must synchronize because Parse message acquires table locks.
		 */
		ereport(DEBUG1,
				(errmsg("Parse: waiting for main node completing the query")));
		pool_extended_send_and_wait(query_context, "P", len, contents, 1, MAIN_NODE_ID, false);


		/*
		 * We must check deadlock error because a aborted transaction by
		 * detecting deadlock isn't same on all nodes. If a transaction is
		 * aborted on main node, pgpool send a error query to another nodes.
		 */
		deadlock_detected = detect_deadlock_error(MAIN(backend), MAJOR(backend));

		/*
		 * Check if other than deadlock error detected.  If so, emit log. This
		 * is useful when invalid encoding error occurs. In this case,
		 * PostgreSQL does not report what statement caused that error and
		 * make users confused.
		 */
		per_node_error_log(backend, MAIN_NODE_ID, stmt, "Parse: Error or notice message from backend: ", true);

		if (deadlock_detected)
		{
			POOL_QUERY_CONTEXT *error_qc;

			error_qc = pool_init_query_context();


			pool_start_query(error_qc, POOL_ERROR_QUERY, strlen(POOL_ERROR_QUERY) + 1, node);
			pool_copy_prep_where(query_context->where_to_send, error_qc->where_to_send);

			ereport(LOG,
					(errmsg("Parse: received deadlock error message from main node")));

			pool_send_and_wait(error_qc, -1, MAIN_NODE_ID);

			pool_query_context_destroy(error_qc);


			pool_set_query_in_progress();
			session_context->query_context = query_context;
		}
		else
		{
			pool_extended_send_and_wait(query_context, "P", len, contents, -1, MAIN_NODE_ID, false);
		}
	}
	else if (SL_MODE)
	{
		POOL_PENDING_MESSAGE *pmsg;

		/*
		 * XXX fix me:even with streaming replication mode, couldn't we have a
		 * deadlock
		 */
		pool_set_query_in_progress();
#ifdef NOT_USED
		pool_clear_sync_map();
#endif
		pool_extended_send_and_wait(query_context, "P", len, contents, 1, MAIN_NODE_ID, true);
		pool_extended_send_and_wait(query_context, "P", len, contents, -1, MAIN_NODE_ID, true);
		pool_add_sent_message(session_context->uncompleted_message);

		/* Add pending message */
		pmsg = pool_pending_message_create('P', len, contents);
		pool_pending_message_dest_set(pmsg, query_context);
		pool_pending_message_add(pmsg);
		pool_pending_message_free_pending_message(pmsg);

		pool_unset_query_in_progress();
	}
	else
	{
		pool_extended_send_and_wait(query_context, "P", len, contents, 1, MAIN_NODE_ID, false);
	}

	return POOL_CONTINUE;

}

POOL_STATUS
Bind(POOL_CONNECTION * frontend, POOL_CONNECTION_POOL * backend,
	 int len, char *contents)
{
	char	   *pstmt_name;
	char	   *portal_name;
	char	   *rewrite_msg = NULL;
	POOL_SENT_MESSAGE *parse_msg;
	POOL_SENT_MESSAGE *bind_msg;
	POOL_SESSION_CONTEXT *session_context;
	POOL_QUERY_CONTEXT *query_context;
	int			insert_stmt_with_lock = 0;
	bool		nowait;

	/* Get session context */
	session_context = pool_get_session_context(false);

	/*
	 * Rewrite message
	 */
	portal_name = contents;
	pstmt_name = contents + strlen(portal_name) + 1;

	if (pool_config->log_client_messages)
		ereport(LOG,
				(errmsg("Bind message from frontend."),
				 errdetail("portal: \"%s\", statement: \"%s\"", portal_name, pstmt_name)));
	parse_msg = pool_get_sent_message('Q', pstmt_name, POOL_SENT_MESSAGE_CREATED);
	if (!parse_msg)
		parse_msg = pool_get_sent_message('P', pstmt_name, POOL_SENT_MESSAGE_CREATED);
	if (!parse_msg)
	{
		ereport(FATAL,
				(errmsg("unable to bind"),
				 errdetail("cannot get parse message \"%s\"", pstmt_name)));
	}

	bind_msg = pool_create_sent_message('B', len, contents,
										parse_msg->num_tsparams, portal_name,
										parse_msg->query_context);

	query_context = parse_msg->query_context;
	if (!query_context)
	{
		ereport(FATAL,
				(errmsg("unable to bind"),
				 errdetail("cannot get the query context")));
	}

	/*
	 * If the query can be cached, save its offset of query text in bind
	 * message's content.
	 */
	if (query_context->is_cache_safe)
	{
		bind_msg->param_offset = sizeof(char) * (strlen(portal_name) + strlen(pstmt_name) + 2);
	}

	session_context->uncompleted_message = bind_msg;

	/* rewrite bind message */
	if (REPLICATION && bind_msg->num_tsparams > 0)
	{
		rewrite_msg = bind_rewrite_timestamp(backend, bind_msg, contents, &len);
		if (rewrite_msg != NULL)
			contents = rewrite_msg;
	}

	session_context->query_context = query_context;

	/*
	 * Take care the case when the previous parse message has been sent to
	 * other than primary node. In this case, we send a parse message to the
	 * primary node.
	 */
	if (pool_config->load_balance_mode && pool_is_writing_transaction() &&
		TSTATE(backend, MAIN_REPLICA ? PRIMARY_NODE_ID : REAL_MAIN_NODE_ID) == 'T' &&
		pool_config->disable_load_balance_on_write != DLBOW_OFF)
	{
		if (!SL_MODE)
		{
			pool_where_to_send(query_context, query_context->original_query,
							   query_context->parse_tree);
		}

		if (parse_before_bind(frontend, backend, parse_msg, bind_msg) != POOL_CONTINUE)
			return POOL_END;
	}

	if (pool_config->disable_load_balance_on_write == DLBOW_DML_ADAPTIVE &&
		TSTATE(backend, MAIN_REPLICA ? PRIMARY_NODE_ID : REAL_MAIN_NODE_ID) == 'T')
	{
		pool_where_to_send(query_context, query_context->original_query,
							query_context->parse_tree);
	}

	/*
	 * Start a transaction if necessary in replication mode
	 */
	if (REPLICATION)
	{
		ereport(DEBUG1,
				(errmsg("Bind: checking strict query")));
		if (is_strict_query(query_context->parse_tree))
		{
			ereport(DEBUG1,
					(errmsg("Bind: strict query")));
			start_internal_transaction(frontend, backend, query_context->parse_tree);
			allow_close_transaction = 1;
		}

		ereport(DEBUG1,
				(errmsg("Bind: checking insert lock")));
		insert_stmt_with_lock = need_insert_lock(backend, query_context->original_query, query_context->parse_tree);
		if (insert_stmt_with_lock)
		{
			ereport(DEBUG1,
					(errmsg("Bind: issuing insert lock")));
			/* issue a LOCK command to keep consistency */
			if (insert_lock(frontend, backend, query_context->original_query, (InsertStmt *) query_context->parse_tree, insert_stmt_with_lock) != POOL_CONTINUE)
			{
				pool_query_context_destroy(query_context);
				return POOL_END;
			}
		}
	}

	ereport(DEBUG1,
			(errmsg("Bind: waiting for main node completing the query")));

	pool_set_query_in_progress();

	if (SL_MODE)
	{
		nowait = true;
		session_context->query_context = query_context = bind_msg->query_context;
	}
	else
		nowait = false;

	pool_extended_send_and_wait(query_context, "B", len, contents, 1, MAIN_NODE_ID, nowait);
	pool_extended_send_and_wait(query_context, "B", len, contents, -1, MAIN_NODE_ID, nowait);

	if (SL_MODE)
	{
		POOL_PENDING_MESSAGE *pmsg;

		pool_unset_query_in_progress();
		pool_add_sent_message(session_context->uncompleted_message);

		/* Add pending message */
		pmsg = pool_pending_message_create('B', len, contents);
		pool_pending_message_dest_set(pmsg, query_context);
		pool_pending_message_query_set(pmsg, query_context);
		pool_pending_message_add(pmsg);
		pool_pending_message_free_pending_message(pmsg);
	}

	if (rewrite_msg)
		pfree(rewrite_msg);
	return POOL_CONTINUE;
}

POOL_STATUS
Describe(POOL_CONNECTION * frontend, POOL_CONNECTION_POOL * backend,
		 int len, char *contents)
{
	POOL_SENT_MESSAGE *msg;
	POOL_SESSION_CONTEXT *session_context;
	POOL_QUERY_CONTEXT *query_context;

	bool		nowait;

	/* Get session context */
	session_context = pool_get_session_context(false);

	/* Prepared Statement */
	if (*contents == 'S')
	{
		if (pool_config->log_client_messages)
			ereport(LOG,
					(errmsg("Describe message from frontend."),
					 errdetail("statement: \"%s\"", contents + 1)));
		msg = pool_get_sent_message('Q', contents + 1, POOL_SENT_MESSAGE_CREATED);
		if (!msg)
			msg = pool_get_sent_message('P', contents + 1, POOL_SENT_MESSAGE_CREATED);
		if (!msg)
			ereport(FATAL,
					(return_code(2),
					 errmsg("unable to execute Describe"),
					 errdetail("unable to get the parse message")));
	}
	/* Portal */
	else
	{
		if (pool_config->log_client_messages)
			ereport(LOG,
					(errmsg("Describe message from frontend."),
					 errdetail("portal: \"%s\"", contents + 1)));
		msg = pool_get_sent_message('B', contents + 1, POOL_SENT_MESSAGE_CREATED);
		if (!msg)
			ereport(FATAL,
					(return_code(2),
					 errmsg("unable to execute Describe"),
					 errdetail("unable to get the bind message")));
	}

	query_context = msg->query_context;

	if (query_context == NULL)
		ereport(FATAL,
				(return_code(2),
				 errmsg("unable to execute Describe"),
				 errdetail("unable to get the query context")));

	session_context->query_context = query_context;

	/*
	 * Calling pool_where_to_send here is dangerous because the node
	 * parse/bind has been sent could be change by pool_where_to_send() and it
	 * leads to "portal not found" etc. errors.
	 */
	ereport(DEBUG1,
			(errmsg("Describe: waiting for main node completing the query")));

	nowait = SL_MODE;

	pool_set_query_in_progress();
	pool_extended_send_and_wait(query_context, "D", len, contents, 1, MAIN_NODE_ID, nowait);
	pool_extended_send_and_wait(query_context, "D", len, contents, -1, MAIN_NODE_ID, nowait);

	if (SL_MODE)
	{
		POOL_PENDING_MESSAGE *pmsg;

		/* Add pending message */
		pmsg = pool_pending_message_create('D', len, contents);
		pool_pending_message_dest_set(pmsg, query_context);
		pool_pending_message_query_set(pmsg, query_context);
		pool_pending_message_add(pmsg);
		pool_pending_message_free_pending_message(pmsg);

		pool_unset_query_in_progress();
	}

	return POOL_CONTINUE;
}


POOL_STATUS
Close(POOL_CONNECTION * frontend, POOL_CONNECTION_POOL * backend,
	  int len, char *contents)
{
	POOL_SENT_MESSAGE *msg;
	POOL_SESSION_CONTEXT *session_context;
	POOL_QUERY_CONTEXT *query_context;

	/* Get session context */
	session_context = pool_get_session_context(false);

	/* Prepared Statement */
	if (*contents == 'S')
	{
		msg = pool_get_sent_message('Q', contents + 1, POOL_SENT_MESSAGE_CREATED);
		if (pool_config->log_client_messages)
			ereport(LOG,
					(errmsg("Close message from frontend."),
					 errdetail("statement: \"%s\"", contents + 1)));
		if (!msg)
			msg = pool_get_sent_message('P', contents + 1, POOL_SENT_MESSAGE_CREATED);
	}
	/* Portal */
	else if (*contents == 'P')
	{
		if (pool_config->log_client_messages)
			ereport(LOG,
					(errmsg("Close message from frontend."),
					 errdetail("portal: \"%s\"", contents + 1)));
		msg = pool_get_sent_message('B', contents + 1, POOL_SENT_MESSAGE_CREATED);
	}
	else
		ereport(FATAL,
				(return_code(2),
				 errmsg("unable to execute close, invalid message")));

	/*
	 * For PostgreSQL, calling close on non existing portals or
	 * statements is not an error. So on the same footings we will ignore all
	 * such calls and return the close complete message to clients with out
	 * going to backend
	 */
	if (!msg)
	{
		int			len = htonl(4);

		pool_set_command_success();
		pool_unset_query_in_progress();

		pool_write(frontend, "3", 1);
		pool_write_and_flush(frontend, &len, sizeof(len));

		return POOL_CONTINUE;
	}

	session_context->uncompleted_message = msg;
	query_context = msg->query_context;

	if (!query_context)
		ereport(FATAL,
				(return_code(2),
				 errmsg("unable to execute close"),
				 errdetail("unable to get the query context")));

	session_context->query_context = query_context;

	/*
	 * pool_where_to_send(query_context, query_context->original_query,
	 * query_context->parse_tree);
	 */

	ereport(DEBUG1,
			(errmsg("Close: waiting for main node completing the query")));

	pool_set_query_in_progress();

	if (!SL_MODE)
	{
		pool_extended_send_and_wait(query_context, "C", len, contents, 1, MAIN_NODE_ID, false);
		pool_extended_send_and_wait(query_context, "C", len, contents, -1, MAIN_NODE_ID, false);
	}
	else
	{
		POOL_PENDING_MESSAGE *pmsg;
		bool		where_to_send_save[MAX_NUM_BACKENDS];

		/*
		 * Parse_before_bind() may have sent a bind message to the primary
		 * node id. So send the close message to the primary node as well.
		 * Even if we do not send the bind message, sending a close message
		 * for non existing statement/portal is harmless. No error will
		 * happen.
		 */
		if (session_context->load_balance_node_id != PRIMARY_NODE_ID)
		{
			/* save where_to_send map */
			memcpy(where_to_send_save, query_context->where_to_send, sizeof(where_to_send_save));

			query_context->where_to_send[PRIMARY_NODE_ID] = true;
			query_context->where_to_send[session_context->load_balance_node_id] = true;
		}

		pool_extended_send_and_wait(query_context, "C", len, contents, 1, MAIN_NODE_ID, true);
		pool_extended_send_and_wait(query_context, "C", len, contents, -1, MAIN_NODE_ID, true);

		/* Add pending message */
		pmsg = pool_pending_message_create('C', len, contents);
		pool_pending_message_dest_set(pmsg, query_context);
		pool_pending_message_query_set(pmsg, query_context);
		pool_pending_message_add(pmsg);
		pool_pending_message_free_pending_message(pmsg);

		if (session_context->load_balance_node_id != PRIMARY_NODE_ID)
		{
			/* Restore where_to_send map */
			memcpy(query_context->where_to_send, where_to_send_save, sizeof(where_to_send_save));
		}

#ifdef NOT_USED
		dump_pending_message();
#endif
		pool_unset_query_in_progress();

		/*
		 * Remove sent message
		 */
		ereport(DEBUG1,
				(errmsg("Close: removing sent message %c %s", *contents, contents + 1)));
		pool_set_sent_message_state(msg);
	}

	return POOL_CONTINUE;
}


POOL_STATUS
FunctionCall3(POOL_CONNECTION * frontend, POOL_CONNECTION_POOL * backend,
			  int len, char *contents)
{
	/*
	 * If Function call message for lo_creat, rewrite it
	 */
	char	   *rewrite_lo;
	int			rewrite_len;

	rewrite_lo = pool_rewrite_lo_creat('F', contents, len, frontend,
									   backend, &rewrite_len);

	if (rewrite_lo != NULL)
	{
		contents = rewrite_lo;
		len = rewrite_len;
	}
	return SimpleForwardToBackend('F', frontend, backend, len, contents);
}

/*
 * Process ReadyForQuery('Z') message.
 * If send_ready is true, send 'Z' message to frontend.
 * If cache_commit is true, commit or discard query cache according to
 * transaction state.
 *
 * - if the error status "mismatch_ntuples" is set, send an error query
 *	 to all DB nodes to abort transaction or do failover.
 * - internal transaction is closed
 */
POOL_STATUS
ReadyForQuery(POOL_CONNECTION * frontend,
			  POOL_CONNECTION_POOL * backend, bool send_ready, bool cache_commit)
{
	int			i;
	int			len;
	signed char kind;
	signed char state = 0;
	POOL_SESSION_CONTEXT *session_context;
	Node	   *node = NULL;
	char	   *query = NULL;
	bool		got_estate = false;

	/*
	 * It is possible that the "ignore until sync is received" flag was set if
	 * we send sync to backend and the backend returns error. Let's reset the
	 * flag unconditionally because we apparently have received a "ready for
	 * query" message from backend.
	 */
	pool_unset_ignore_till_sync();

	/* Reset previous message */
	pool_pending_message_reset_previous_message();

	/* Get session context */
	session_context = pool_get_session_context(false);

	/*
	 * If the numbers of update tuples are differ and
	 * failover_if_affected_tuples_mismatch is false, we abort transactions by
	 * using do_error_command.  If failover_if_affected_tuples_mismatch is
	 * true, trigger failover. This only works with PROTO_MAJOR_V3.
	 */
	if (session_context->mismatch_ntuples && MAJOR(backend) == PROTO_MAJOR_V3)
	{
		int			i;
		char		kind;

		/*
		 * If failover_if_affected_tuples_mismatch, is true, then decide
		 * victim nodes by using find_victim_nodes and degenerate them.
		 */
		if (pool_config->failover_if_affected_tuples_mismatch)
		{
			int		   *victim_nodes;
			int			number_of_nodes;

			victim_nodes = find_victim_nodes(session_context->ntuples, NUM_BACKENDS,
											 MAIN_NODE_ID, &number_of_nodes);
			if (victim_nodes)
			{
				int			i;
				StringInfoData	   msg;

				initStringInfo(&msg);
				appendStringInfoString(&msg, "ReadyForQuery: Degenerate backends:");

				for (i = 0; i < number_of_nodes; i++)
					appendStringInfo(&msg, " %d", victim_nodes[i]);

				ereport(LOG,
						(errmsg("processing ready for query message"),
						 errdetail("%s", msg.data)));

				pfree(msg.data);

				initStringInfo(&msg);
				appendStringInfoString(&msg, "ReadyForQuery: Number of affected tuples are:");

				for (i = 0; i < NUM_BACKENDS; i++)
					appendStringInfo(&msg, " %d", session_context->ntuples[i]);

				ereport(LOG,
						(errmsg("processing ready for query message"),
						 errdetail("%s", msg.data)));

				pfree(msg.data);

				degenerate_backend_set(victim_nodes, number_of_nodes, REQ_DETAIL_CONFIRMED | REQ_DETAIL_SWITCHOVER);
				child_exit(POOL_EXIT_AND_RESTART);
			}
			else
			{
				ereport(LOG,
						(errmsg("processing ready for query message"),
						 errdetail("find_victim_nodes returned no victim node")));
			}
		}

		/*
		 * XXX: discard rest of ReadyForQuery packet
		 */

		if (pool_read_message_length(backend) < 0)
			return POOL_END;

		state = pool_read_kind(backend);
		if (state < 0)
			return POOL_END;

		for (i = 0; i < NUM_BACKENDS; i++)
		{
			if (VALID_BACKEND(i))
			{
				/* abort transaction on all nodes. */
				do_error_command(CONNECTION(backend, i), PROTO_MAJOR_V3);
			}
		}

		/* loop through until we get ReadyForQuery */
		for (;;)
		{
			kind = pool_read_kind(backend);
			if (kind < 0)
				return POOL_END;

			if (kind == 'Z')
				break;


			/* put the message back to read buffer */
			for (i = 0; i < NUM_BACKENDS; i++)
			{
				if (VALID_BACKEND(i))
				{
					pool_unread(CONNECTION(backend, i), &kind, 1);
				}
			}

			/* discard rest of the packet */
			if (pool_discard_packet(backend) != POOL_CONTINUE)
				return POOL_END;
		}
		session_context->mismatch_ntuples = false;
	}

	/*
	 * if a transaction is started for insert lock, we need to close the
	 * transaction.
	 */
	/* if (pool_is_query_in_progress() && allow_close_transaction) */
	if (REPLICATION && allow_close_transaction)
	{
		bool internal_transaction_started = INTERNAL_TRANSACTION_STARTED(backend, MAIN_NODE_ID);

		/*
		 * If we are running in snapshot isolation mode and started an
		 * internal transaction, wait until snapshot is prepared.
		 */
		if (pool_config->backend_clustering_mode == CM_SNAPSHOT_ISOLATION &&
			internal_transaction_started)
		{
			si_commit_request();
		}

		/* close an internal transaction */
		if (internal_transaction_started)
			if (end_internal_transaction(frontend, backend) != POOL_CONTINUE)
				return POOL_END;

		/*
		 * If we are running in snapshot isolation mode and started an
		 * internal transaction, notice that commit is done.
		 */
		if (pool_config->backend_clustering_mode == CM_SNAPSHOT_ISOLATION &&
			internal_transaction_started)
		{
			si_commit_done();
			/* reset transaction readonly flag */
			session_context->transaction_read_only = false;
		}
	}

	if (MAJOR(backend) == PROTO_MAJOR_V3)
	{
		if ((len = pool_read_message_length(backend)) < 0)
			return POOL_END;

		/*
		 * Set transaction state for each node
		 */
		state = TSTATE(backend,
					   MAIN_REPLICA ? PRIMARY_NODE_ID : REAL_MAIN_NODE_ID);

		for (i = 0; i < NUM_BACKENDS; i++)
		{
			if (!VALID_BACKEND(i))
				continue;

			if (pool_read(CONNECTION(backend, i), &kind, sizeof(kind)))
				return POOL_END;

			TSTATE(backend, i) = kind;
			ereport(DEBUG5,
					(errmsg("processing ReadyForQuery"),
					 errdetail("transaction state '%c'(%02x)", state, state)));

			/*
			 * The transaction state to be returned to frontend is main node's.
			 */
			if (i == (MAIN_REPLICA ? PRIMARY_NODE_ID : REAL_MAIN_NODE_ID))
			{
				state = kind;
			}

			/*
			 * However, if the state is 'E', then frontend should had been
			 * already reported an ERROR. So, to match with that, let be the
			 * state to be returned to frontend.
			 */
			if (kind == 'E')
				got_estate = true;
		}
	}

	/*
	 * Make sure that no message remains in the backend buffer.  If something
	 * remains, it could be an "out of band" ERROR or FATAL error, or a NOTICE
	 * message, which was generated by backend itself for some reasons like
	 * recovery conflict or SIGTERM received. If so, let's consume it and emit
	 * a log message so that next read_kind_from_backend() will not hang in
	 * trying to read from backend which may have not produced such a message.
	 */
	if (pool_is_query_in_progress())
	{
		for (i = 0; i < NUM_BACKENDS; i++)
		{
			if (!VALID_BACKEND(i))
				continue;
			if (!pool_read_buffer_is_empty(CONNECTION(backend, i)))
				per_node_error_log(backend, i,
								   "(out of band message)",
								   "ReadyForQuery: Error or notice message from backend: ", false);
		}
	}

	if (send_ready)
	{
		pool_write(frontend, "Z", 1);

		if (MAJOR(backend) == PROTO_MAJOR_V3)
		{
			len = htonl(len);
			pool_write(frontend, &len, sizeof(len));
			if (got_estate)
				state = 'E';
			pool_write(frontend, &state, 1);
		}
		pool_flush(frontend);
	}

	if (pool_is_query_in_progress())
	{
		node = pool_get_parse_tree();
		query = pool_get_query_string();

		if (pool_is_command_success())
		{
			if (node)
				pool_at_command_success(frontend, backend);

			/* Memory cache enabled? */
			if (cache_commit && pool_config->memory_cache_enabled)
			{

				/*
				 * If we are doing extended query and the state is after
				 * EXECUTE, then we can commit cache. We check latter
				 * condition by looking at query_context->query_w_hex. This
				 * check is necessary for certain frame work such as PHP PDO.
				 * It sends Sync message right after PARSE and it produces
				 * "Ready for query" message from backend.
				 */
				if (pool_is_doing_extended_query_message())
				{
					if (session_context->query_context &&
						session_context->query_context->query_state[MAIN_NODE_ID] == POOL_EXECUTE_COMPLETE)
					{
						pool_handle_query_cache(backend, session_context->query_context->query_w_hex, node, state);
						if (session_context->query_context->query_w_hex)
							pfree(session_context->query_context->query_w_hex);
						session_context->query_context->query_w_hex = NULL;
					}
				}
				else
				{
					if (MAJOR(backend) != PROTO_MAJOR_V3)
					{
						state = 'I';	/* XXX I don't think query cache works
										 * with PROTO2 protocol */
					}
					pool_handle_query_cache(backend, query, node, state);
				}
			}
		}

		/*
		 * If PREPARE or extended query protocol commands caused error, remove
		 * the temporary saved message. (except when ReadyForQuery() is called
		 * during Parse() of extended queries)
		 */
		else
		{
			if ((pool_is_doing_extended_query_message() &&
				 session_context->query_context &&
				 session_context->query_context->query_state[MAIN_NODE_ID] != POOL_UNPARSED &&
				 session_context->uncompleted_message) ||
				(!pool_is_doing_extended_query_message() && session_context->uncompleted_message &&
				 session_context->uncompleted_message->kind != 0))
			{
				pool_add_sent_message(session_context->uncompleted_message);
				pool_remove_sent_message(session_context->uncompleted_message->kind,
										 session_context->uncompleted_message->name);
				session_context->uncompleted_message = NULL;
			}
		}

		pool_unset_query_in_progress();
	}
	if (!pool_is_doing_extended_query_message())
	{
		if (!(node && IsA(node, PrepareStmt)))
		{
			pool_query_context_destroy(pool_get_session_context(false)->query_context);
		}
	}

	/*
	 * Show ps idle status
	 */
	pool_ps_idle_display(backend);

	return POOL_CONTINUE;
}

/*
 * Close running transactions on standbys.
 */
static POOL_STATUS close_standby_transactions(POOL_CONNECTION * frontend,
											  POOL_CONNECTION_POOL * backend)
{
	int			i;

	for (i = 0; i < NUM_BACKENDS; i++)
	{
		if (CONNECTION_SLOT(backend, i) &&
			TSTATE(backend, i) == 'T' &&
			BACKEND_INFO(i).backend_status == CON_UP &&
			(MAIN_REPLICA ? PRIMARY_NODE_ID : REAL_MAIN_NODE_ID) != i)
		{
			per_node_statement_log(backend, i, "COMMIT");
			if (do_command(frontend, CONNECTION(backend, i), "COMMIT", MAJOR(backend),
						   MAIN_CONNECTION(backend)->pid,
						   MAIN_CONNECTION(backend)->key, 0) != POOL_CONTINUE)
				ereport(ERROR,
						(errmsg("unable to close standby transactions"),
						 errdetail("do_command returned DEADLOCK status")));
		}
	}
	return POOL_CONTINUE;
}

POOL_STATUS
ParseComplete(POOL_CONNECTION * frontend, POOL_CONNECTION_POOL * backend)
{
	POOL_SESSION_CONTEXT *session_context;

	/* Get session context */
	session_context = pool_get_session_context(false);

	if (!SL_MODE && session_context->uncompleted_message)
	{
		POOL_QUERY_CONTEXT *qc;

		pool_add_sent_message(session_context->uncompleted_message);

		qc = session_context->uncompleted_message->query_context;
		if (qc)
			pool_set_query_state(qc, POOL_PARSE_COMPLETE);

		session_context->uncompleted_message = NULL;
	}

	return SimpleForwardToFrontend('1', frontend, backend);
}

POOL_STATUS
BindComplete(POOL_CONNECTION * frontend, POOL_CONNECTION_POOL * backend)
{
	POOL_SESSION_CONTEXT *session_context;

	/* Get session context */
	session_context = pool_get_session_context(false);

	if (!SL_MODE && session_context->uncompleted_message)
	{
		POOL_QUERY_CONTEXT *qc;

		pool_add_sent_message(session_context->uncompleted_message);

		qc = session_context->uncompleted_message->query_context;
		if (qc)
			pool_set_query_state(qc, POOL_BIND_COMPLETE);

		session_context->uncompleted_message = NULL;
	}

	return SimpleForwardToFrontend('2', frontend, backend);
}

POOL_STATUS
CloseComplete(POOL_CONNECTION * frontend, POOL_CONNECTION_POOL * backend)
{
	POOL_SESSION_CONTEXT *session_context;
	POOL_STATUS status;
	char		kind = ' ';
	char	   *name = "";

	/* Get session context */
	session_context = pool_get_session_context(false);

	/* Send CloseComplete(3) to frontend before removing the target message */
	status = SimpleForwardToFrontend('3', frontend, backend);

	/* Remove the target message */
	if (SL_MODE)
	{
		POOL_PENDING_MESSAGE *pmsg;

		pmsg = pool_pending_message_pull_out();

		if (pmsg)
		{
			/* Sanity check */
			if (pmsg->type != POOL_CLOSE)
			{
				ereport(LOG,
						(errmsg("CloseComplete: pending message was not Close request: %s", pool_pending_message_type_to_string(pmsg->type))));
			}
			else
			{
				kind = pool_get_close_message_spec(pmsg);
				name = pool_get_close_message_name(pmsg);
				kind = kind == 'S' ? 'P' : 'B';
			}
			pool_pending_message_free_pending_message(pmsg);
		}
	}
	else
	{
		if (session_context->uncompleted_message)
		{
			kind = session_context->uncompleted_message->kind;
			name = session_context->uncompleted_message->name;
			session_context->uncompleted_message = NULL;
		}
		else
		{
			ereport(ERROR,
					(errmsg("processing CloseComplete, uncompleted message not found")));
		}
	}

	if (kind != ' ')
	{
		pool_remove_sent_message(kind, name);
		ereport(DEBUG1,
				(errmsg("CloseComplete: remove sent message. kind:%c, name:%s",
						kind, name)));
		if (pool_config->memory_cache_enabled)
		{
			/*
			 * Discard current temp query cache.  This is necessary for the
			 * case of clustering mode is not either streaming or logical
			 * replication. Because in the cases the cache has been already
			 * discarded upon receiving CommandComplete.
			 */
			pool_discard_current_temp_query_cache();
		}
	}

	return status;
}

POOL_STATUS
ParameterDescription(POOL_CONNECTION * frontend,
					 POOL_CONNECTION_POOL * backend)
{
	int			len,
				len1 = 0;
	char	   *p = NULL;
	char	   *p1 = NULL;
	int			sendlen;
	int			i;

	POOL_SESSION_CONTEXT *session_context;
	int			num_params,
				send_num_params,
				num_dmy;
	char		kind = 't';

	session_context = pool_get_session_context(false);

	/* only in replication mode and rewritten query */
	if (!REPLICATION || !session_context->query_context->rewritten_query)
		return SimpleForwardToFrontend('t', frontend, backend);

	/* get number of parameters in original query */
	num_params = session_context->query_context->num_original_params;

	pool_read(MAIN(backend), &len, sizeof(len));

	len = ntohl(len);
	len -= sizeof(int32);
	len1 = len;

	/* number of parameters in rewritten query is just discarded */
	pool_read(MAIN(backend), &num_dmy, sizeof(int16));
	len -= sizeof(int16);

	p = pool_read2(MAIN(backend), len);
	if (p == NULL)
		ereport(ERROR,
				(errmsg("ParameterDescription. connection error"),
				 errdetail("read from backend failed")));


	p1 = palloc(len);
	memcpy(p1, p, len);

	for (i = 0; i < NUM_BACKENDS; i++)
	{
		if (VALID_BACKEND(i) && !IS_MAIN_NODE_ID(i))
		{
			pool_read(CONNECTION(backend, i), &len, sizeof(len));

			len = ntohl(len);
			len -= sizeof(int32);

			p = pool_read2(CONNECTION(backend, i), len);
			if (p == NULL)
				ereport(ERROR,
						(errmsg("ParameterDescription. connection error"),
						 errdetail("read from backend no %d failed", i)));

			if (len != len1)
				ereport(DEBUG1,
						(errmsg("ParameterDescription. backends does not match"),
						 errdetail("length does not match between backends main(%d) %d th backend(%d) kind:(%c)", len, i, len1, kind)));
		}
	}

	pool_write(frontend, &kind, 1);

	/* send back OIDs of parameters in original query and left are discarded */
	len = sizeof(int16) + num_params * sizeof(int32);
	sendlen = htonl(len + sizeof(int32));
	pool_write(frontend, &sendlen, sizeof(int32));

	send_num_params = htons(num_params);
	pool_write(frontend, &send_num_params, sizeof(int16));

	pool_write_and_flush(frontend, p1, num_params * sizeof(int32));

	pfree(p1);
	return POOL_CONTINUE;
}

POOL_STATUS
ErrorResponse3(POOL_CONNECTION * frontend,
			   POOL_CONNECTION_POOL * backend)
{
	POOL_STATUS ret;

	ret = SimpleForwardToFrontend('E', frontend, backend);
	if (ret != POOL_CONTINUE)
		return ret;

	if (!SL_MODE)
		raise_intentional_error_if_need(backend);

	return POOL_CONTINUE;
}

POOL_STATUS
FunctionCall(POOL_CONNECTION * frontend,
			 POOL_CONNECTION_POOL * backend)
{
	char		dummy[2];
	int			oid;
	int			argn;
	int			i;

	for (i = 0; i < NUM_BACKENDS; i++)
	{
		if (VALID_BACKEND(i))
		{
			pool_write(CONNECTION(backend, i), "F", 1);
		}
	}

	/* dummy */
	pool_read(frontend, dummy, sizeof(dummy));

	for (i = 0; i < NUM_BACKENDS; i++)
	{
		if (VALID_BACKEND(i))
		{
			pool_write(CONNECTION(backend, i), dummy, sizeof(dummy));
		}
	}

	/* function object id */
	pool_read(frontend, &oid, sizeof(oid));

	for (i = 0; i < NUM_BACKENDS; i++)
	{
		if (VALID_BACKEND(i))
		{
			pool_write(CONNECTION(backend, i), &oid, sizeof(oid));
		}
	}

	/* number of arguments */
	pool_read(frontend, &argn, sizeof(argn));

	for (i = 0; i < NUM_BACKENDS; i++)
	{
		if (VALID_BACKEND(i))
		{
			pool_write(CONNECTION(backend, i), &argn, sizeof(argn));
		}
	}

	argn = ntohl(argn);

	for (i = 0; i < argn; i++)
	{
		int			len;
		char	   *arg;

		/* length of each argument in bytes */
		pool_read(frontend, &len, sizeof(len));

		for (i = 0; i < NUM_BACKENDS; i++)
		{
			if (VALID_BACKEND(i))
			{
				pool_write(CONNECTION(backend, i), &len, sizeof(len));
			}
		}

		len = ntohl(len);

		/* argument value itself */
		if ((arg = pool_read2(frontend, len)) == NULL)
			ereport(FATAL,
					(return_code(2),
					 errmsg("failed to process function call"),
					 errdetail("read from frontend failed")));

		for (i = 0; i < NUM_BACKENDS; i++)
		{
			if (VALID_BACKEND(i))
			{
				pool_write(CONNECTION(backend, i), arg, len);
			}
		}
	}

	for (i = 0; i < NUM_BACKENDS; i++)
	{
		if (VALID_BACKEND(i))
		{
			pool_flush(CONNECTION(backend, i));
		}
	}
	return POOL_CONTINUE;
}

POOL_STATUS
ProcessFrontendResponse(POOL_CONNECTION * frontend,
						POOL_CONNECTION_POOL * backend)
{
	char		fkind;
	char	   *bufp = NULL;
	char	   *contents;
	POOL_STATUS status;
	int			len = 0;

	/* Get session context */
	pool_get_session_context(false);

	if (pool_read_buffer_is_empty(frontend) && frontend->no_forward != 0)
		return POOL_CONTINUE;

	/* Are we suspending reading from frontend? */
	if (pool_is_suspend_reading_from_frontend())
		return POOL_CONTINUE;

	pool_read(frontend, &fkind, 1);

	ereport(DEBUG5,
			(errmsg("processing frontend response"),
			 errdetail("received kind '%c'(%02x) from frontend", fkind, fkind)));


	if (MAJOR(backend) == PROTO_MAJOR_V3)
	{
		if (pool_read(frontend, &len, sizeof(len)) < 0)
			ereport(ERROR,
					(errmsg("unable to process frontend response"),
					 errdetail("failed to read message length from frontend. frontend abnormally exited")));

		len = ntohl(len) - 4;
		if (len > 0)
			bufp = pool_read2(frontend, len);
		else if (len < 0)
			ereport(ERROR,
					(errmsg("frontend message length is less than 4 (kind: %c)", fkind)));
	}
	else
	{
		if (fkind != 'F')
			bufp = pool_read_string(frontend, &len, 0);
	}

	if (len > 0 && bufp == NULL)
		ereport(ERROR,
				(errmsg("unable to process frontend response"),
				 errdetail("failed to read message from frontend. frontend abnormally exited")));

	if (fkind != 'S' && pool_is_ignore_till_sync())
	{
		/*
		 * Flag setting for calling ProcessBackendResponse() in
		 * pool_process_query().
		 */
		if (!pool_is_query_in_progress())
			pool_set_query_in_progress();

		return POOL_CONTINUE;
	}

	pool_unset_doing_extended_query_message();

	/*
	 * Allocate buffer and copy the packet contents.  Because inside these
	 * protocol modules, pool_read2 maybe called and modify its buffer
	 * contents.
	 */
	if (len > 0)
	{
		contents = palloc(len);
		memcpy(contents, bufp, len);
	}
	else
	{
		/*
		 * Set dummy content if len <= 0. this happens only when protocol
		 * version is 2.
		 */
		contents = palloc(1);
		memcpy(contents, "", 1);
	}

	switch (fkind)
	{
			POOL_QUERY_CONTEXT *query_context;
			char	   *query;
			Node	   *node;

		case 'X':				/* Terminate */
			if (contents)
				pfree(contents);
			if (pool_config->log_client_messages)
				ereport(LOG,
						(errmsg("Terminate message from frontend.")));
			ereport(DEBUG5,
					(errmsg("Frontend terminated"),
					 errdetail("received message kind 'X' from frontend")));
			return POOL_END;

		case 'Q':				/* Query */
			allow_close_transaction = 1;
			if (pool_config->log_client_messages)
				ereport(LOG,
						(errmsg("Query message from frontend."),
						 errdetail("query: \"%s\"", contents)));
			status = SimpleQuery(frontend, backend, len, contents);
			break;

		case 'E':				/* Execute */
			allow_close_transaction = 1;
			pool_set_doing_extended_query_message();
			if (!pool_is_query_in_progress() && !pool_is_ignore_till_sync())
				pool_set_query_in_progress();
			status = Execute(frontend, backend, len, contents);
			break;

		case 'P':				/* Parse */
			allow_close_transaction = 0;
			pool_set_doing_extended_query_message();
			status = Parse(frontend, backend, len, contents);
			break;

		case 'B':				/* Bind */
			pool_set_doing_extended_query_message();
			status = Bind(frontend, backend, len, contents);
			break;

		case 'C':				/* Close */
			pool_set_doing_extended_query_message();
			if (!pool_is_query_in_progress() && !pool_is_ignore_till_sync())
				pool_set_query_in_progress();
			status = Close(frontend, backend, len, contents);
			break;

		case 'D':				/* Describe */
			pool_set_doing_extended_query_message();
			status = Describe(frontend, backend, len, contents);
			break;

		case 'S':				/* Sync */
			if (pool_config->log_client_messages)
				ereport(LOG,
						(errmsg("Sync message from frontend.")));
			pool_set_doing_extended_query_message();
			if (pool_is_ignore_till_sync())
				pool_unset_ignore_till_sync();

			if (SL_MODE)
			{
				POOL_PENDING_MESSAGE *msg;

				pool_unset_query_in_progress();
				msg = pool_pending_message_create('S', 0, NULL);
				pool_pending_message_add(msg);
				pool_pending_message_free_pending_message(msg);
			}
			else if (!pool_is_query_in_progress())
				pool_set_query_in_progress();
			status = SimpleForwardToBackend(fkind, frontend, backend, len, contents);

			if (SL_MODE)
			{
				/*
				 * From now on suspend to read from frontend until we receive
				 * ready for query message from backend.
				 */
				pool_set_suspend_reading_from_frontend();
			}
			break;

		case 'F':				/* FunctionCall */
			if (pool_config->log_client_messages)
			{
				int			oid;

				memcpy(&oid, contents, sizeof(int));
				ereport(LOG,
						(errmsg("FunctionCall message from frontend."),
						 errdetail("oid: \"%d\"", ntohl(oid))));
			}

			/*
			 * Create dummy query context as if it were an INSERT.
			 */
			query_context = pool_init_query_context();
			query = "INSERT INTO foo VALUES(1)";
			MemoryContext old_context = MemoryContextSwitchTo(query_context->memory_context);

			node = get_dummy_insert_query_node();
			pool_start_query(query_context, query, strlen(query) + 1, node);

			MemoryContextSwitchTo(old_context);

			pool_where_to_send(query_context, query_context->original_query,
							   query_context->parse_tree);

			if (MAJOR(backend) == PROTO_MAJOR_V3)
				status = FunctionCall3(frontend, backend, len, contents);
			else
				status = FunctionCall(frontend, backend);

			break;

		case 'c':				/* CopyDone */
		case 'd':				/* CopyData */
		case 'f':				/* CopyFail */
		case 'H':				/* Flush */
			if (fkind == 'H' && pool_config->log_client_messages)
				ereport(LOG,
						(errmsg("Flush message from frontend.")));
			if (MAJOR(backend) == PROTO_MAJOR_V3)
			{
				if (fkind == 'H')
				{
					pool_set_doing_extended_query_message();
					pool_pending_message_set_flush_request();
				}
				status = SimpleForwardToBackend(fkind, frontend, backend, len, contents);

				/*
				 * After flush message received, extended query mode should be
				 * continued.
				 */
				if (fkind != 'H' && pool_is_doing_extended_query_message())
				{
					pool_unset_doing_extended_query_message();
				}

				break;
			}

		default:
			ereport(FATAL,
					(return_code(2),
					 errmsg("unable to process frontend response"),
					 errdetail("unknown message type %c(%02x)", fkind, fkind)));

	}
	if (contents)
		pfree(contents);

	if (status != POOL_CONTINUE)
		ereport(FATAL,
				(return_code(2),
				 errmsg("unable to process frontend response")));

	return status;
}

POOL_STATUS
ProcessBackendResponse(POOL_CONNECTION * frontend,
					   POOL_CONNECTION_POOL * backend,
					   int *state, short *num_fields)
{
	int			status = POOL_CONTINUE;
	char		kind;

	/* Get session context */
	pool_get_session_context(false);

	if (pool_is_ignore_till_sync())
	{
		if (pool_is_query_in_progress())
			pool_unset_query_in_progress();

		/*
		 * Check if we have pending data in backend connection cache. If we
		 * do, it is likely that a sync message has been sent to backend and
		 * the backend replied back to us. So we need to process it.
		 */
		if (is_backend_cache_empty(backend))
		{
			return POOL_CONTINUE;
		}
	}

	if (pool_is_skip_reading_from_backends())
	{
		pool_unset_skip_reading_from_backends();
		return POOL_CONTINUE;
	}

	read_kind_from_backend(frontend, backend, &kind);

	/*
	 * Sanity check
	 */
	if (kind == 0)
	{
		ereport(FATAL,
				(return_code(2),
				 errmsg("unable to process backend response"),
				 errdetail("invalid message kind sent by backend connection")));
	}
	ereport(DEBUG5,
			(errmsg("processing backend response"),
			 errdetail("received kind '%c'(%02x) from backend", kind, kind)));


	if (MAJOR(backend) == PROTO_MAJOR_V3)
	{
		switch (kind)
		{
			case 'G':			/* CopyInResponse */
				status = CopyInResponse(frontend, backend);
				break;

			case 'S':			/* ParameterStatus */
				status = ParameterStatus(frontend, backend);
				break;

			case 'Z':			/* ReadyForQuery */
				ereport(DEBUG5,
						(errmsg("processing backend response"),
						 errdetail("Ready For Query received")));
				pool_unset_suspend_reading_from_frontend();
				status = ReadyForQuery(frontend, backend, true, true);
#ifdef DEBUG
				extern bool stop_now;

				if (stop_now)
					exit(0);
#endif
				break;

			case '1':			/* ParseComplete */
				if (SL_MODE)
				{
					POOL_PENDING_MESSAGE *pmsg;

					pmsg = pool_pending_message_get_previous_message();
					if (pmsg && pmsg->not_forward_to_frontend)
					{
						/*
						 * parse_before_bind() was called. Do not forward the
						 * parse complete message to frontend.
						 */
						ereport(DEBUG5,
								(errmsg("processing backend response"),
								 errdetail("do not forward parse complete message to frontend")));
						pool_discard_packet_contents(backend);
						pool_unset_query_in_progress();
						pool_set_command_success();
						status = POOL_CONTINUE;
						break;
					}
				}
				status = ParseComplete(frontend, backend);
				pool_set_command_success();
				pool_unset_query_in_progress();
				break;

			case '2':			/* BindComplete */
				status = BindComplete(frontend, backend);
				pool_set_command_success();
				pool_unset_query_in_progress();
				break;

			case '3':			/* CloseComplete */
				if (SL_MODE)
				{
					POOL_PENDING_MESSAGE *pmsg;

					pmsg = pool_pending_message_get_previous_message();
					if (pmsg && pmsg->not_forward_to_frontend)
					{
						/*
						 * parse_before_bind() was called. Do not forward the
						 * close complete message to frontend.
						 */
						ereport(DEBUG5,
								(errmsg("processing backend response"),
								 errdetail("do not forward close complete message to frontend")));
						pool_discard_packet_contents(backend);

						/*
						 * Remove pending message here because
						 * read_kind_from_backend() did not do it.
						 */
						pmsg = pool_pending_message_pull_out();
						pool_pending_message_free_pending_message(pmsg);

						if (pool_is_doing_extended_query_message())
							pool_unset_query_in_progress();
						pool_set_command_success();
						status = POOL_CONTINUE;
						break;
					}
				}

				status = CloseComplete(frontend, backend);
				pool_set_command_success();
				if (pool_is_doing_extended_query_message())
					pool_unset_query_in_progress();
				break;

			case 'E':			/* ErrorResponse */
				if (pool_is_doing_extended_query_message())
				{
					char	*message;

					/* Log the error message which was possibly missed till
					 * a sync message was sent */
					if (pool_extract_error_message(false, MAIN(backend), PROTO_MAJOR_V3,
												   true, &message) == 1)
					{
						ereport(LOG,
								(errmsg("Error message from backend: DB node id: %d message: \"%s\"",
										MAIN_NODE_ID, message)));
						pfree(message);
					}
				}

				/* Forward the error message to frontend */
				status = ErrorResponse3(frontend, backend);
				pool_unset_command_success();
				if (TSTATE(backend, MAIN_REPLICA ? PRIMARY_NODE_ID :
						   REAL_MAIN_NODE_ID) != 'I')
				{
					pool_set_failed_transaction();

					/* Remove ongoing CREATE/DROP temp tables */
					pool_temp_tables_remove_pending();
				}
				if (pool_is_doing_extended_query_message())
				{
					pool_set_ignore_till_sync();
					pool_unset_query_in_progress();
					pool_unset_suspend_reading_from_frontend();
					if (SL_MODE)
						pool_discard_except_sync_and_ready_for_query(frontend, backend);
				}
				break;

			case 'C':			/* CommandComplete */
				status = CommandComplete(frontend, backend, true);
				pool_set_command_success();
				if (pool_is_doing_extended_query_message())
					pool_unset_query_in_progress();
				break;

			case 't':			/* ParameterDescription */
				status = ParameterDescription(frontend, backend);
				break;

			case 'I':			/* EmptyQueryResponse */
				status = CommandComplete(frontend, backend, false);

				/*
				 * Empty query response message should be treated same as
				 * Command complete message. When we receive the Command
				 * complete message, we unset the query in progress flag if
				 * operated in streaming replication mode. So we unset the
				 * flag as well. See bug 190 for more details.
				 */
				if (pool_is_doing_extended_query_message())
					pool_unset_query_in_progress();
				break;

			case 'T':			/* RowDescription */
				status = SimpleForwardToFrontend(kind, frontend, backend);
				if (pool_is_doing_extended_query_message())
					pool_unset_query_in_progress();
				break;

			case 'n':			/* NoData */
				status = SimpleForwardToFrontend(kind, frontend, backend);
				if (pool_is_doing_extended_query_message())
					pool_unset_query_in_progress();
				break;

			case 's':			/* PortalSuspended */
				status = SimpleForwardToFrontend(kind, frontend, backend);
				if (pool_is_doing_extended_query_message())
					pool_unset_query_in_progress();
				break;

			default:
				status = SimpleForwardToFrontend(kind, frontend, backend);
				break;
		}

		pool_get_session_context(false)->flush_pending = false;

	}
	else
	{
		switch (kind)
		{
			case 'A':			/* NotificationResponse */
				status = NotificationResponse(frontend, backend);
				break;

			case 'B':			/* BinaryRow */
				status = BinaryRow(frontend, backend, *num_fields);
				break;

			case 'C':			/* CompletedResponse */
				status = CompletedResponse(frontend, backend);
				break;

			case 'D':			/* AsciiRow */
				status = AsciiRow(frontend, backend, *num_fields);
				break;

			case 'E':			/* ErrorResponse */
				status = ErrorResponse(frontend, backend);
				if (TSTATE(backend, MAIN_REPLICA ? PRIMARY_NODE_ID :
						   REAL_MAIN_NODE_ID) != 'I')
					pool_set_failed_transaction();
				break;

			case 'G':			/* CopyInResponse */
				status = CopyInResponse(frontend, backend);
				break;

			case 'H':			/* CopyOutResponse */
				status = CopyOutResponse(frontend, backend);
				break;

			case 'I':			/* EmptyQueryResponse */
				EmptyQueryResponse(frontend, backend);
				break;

			case 'N':			/* NoticeResponse */
				NoticeResponse(frontend, backend);
				break;

			case 'P':			/* CursorResponse */
				status = CursorResponse(frontend, backend);
				break;

			case 'T':			/* RowDescription */
				status = RowDescription(frontend, backend, num_fields);
				break;

			case 'V':			/* FunctionResultResponse and
								 * FunctionVoidResponse */
				status = FunctionResultResponse(frontend, backend);
				break;

			case 'Z':			/* ReadyForQuery */
				status = ReadyForQuery(frontend, backend, true, true);
				break;

			default:
				ereport(FATAL,
						(return_code(1),
						 errmsg("Unknown message type %c(%02x)", kind, kind)));
		}
	}

	/*
	 * Do we receive ready for query while processing reset request?
	 */
	if (kind == 'Z' && frontend->no_forward && *state == 1)
	{
		*state = 0;
	}

	if (status != POOL_CONTINUE)
		ereport(FATAL,
				(return_code(2),
				 errmsg("unable to process backend response for message kind '%c'", kind)));

	return status;
}

POOL_STATUS
CopyInResponse(POOL_CONNECTION * frontend,
			   POOL_CONNECTION_POOL * backend)
{
	POOL_STATUS status;

	/* forward to the frontend */
	if (MAJOR(backend) == PROTO_MAJOR_V3)
	{
		SimpleForwardToFrontend('G', frontend, backend);
		pool_flush(frontend);
	}
	else
		pool_write_and_flush(frontend, "G", 1);

	status = CopyDataRows(frontend, backend, 1);
	return status;
}

POOL_STATUS
CopyOutResponse(POOL_CONNECTION * frontend,
				POOL_CONNECTION_POOL * backend)
{
	POOL_STATUS status;

	/* forward to the frontend */
	if (MAJOR(backend) == PROTO_MAJOR_V3)
	{
		SimpleForwardToFrontend('H', frontend, backend);
		pool_flush(frontend);
	}
	else
		pool_write_and_flush(frontend, "H", 1);

	status = CopyDataRows(frontend, backend, 0);
	return status;
}

POOL_STATUS
CopyDataRows(POOL_CONNECTION * frontend,
			 POOL_CONNECTION_POOL * backend, int copyin)
{
	char	   *string = NULL;
	int			len;
	int			i;
	int			copy_count;

#ifdef DEBUG
	int			j = 0;
	char		buf[1024];
#endif

	copy_count = 0;
	for (;;)
	{
		if (copyin)
		{
			if (MAJOR(backend) == PROTO_MAJOR_V3)
			{
				char		kind;
				char	   *contents = NULL;

				pool_read(frontend, &kind, 1);

				ereport(DEBUG5,
						(errmsg("copy data rows"),
						 errdetail("read kind from frontend %c(%02x)", kind, kind)));

				pool_read(frontend, &len, sizeof(len));
				len = ntohl(len) - 4;
				if (len > 0)
					contents = pool_read2(frontend, len);

				SimpleForwardToBackend(kind, frontend, backend, len, contents);

				/* CopyData? */
				if (kind == 'd')
				{
					copy_count++;
					continue;
				}
				else
				{
					if (pool_config->log_client_messages && copy_count != 0)
						ereport(LOG,
								(errmsg("CopyData message from frontend."),
								 errdetail("count: %d", copy_count)));
					if (kind == 'c' && pool_config->log_client_messages)
						ereport(LOG,
								(errmsg("CopyDone message from frontend.")));
					if (kind == 'f' && pool_config->log_client_messages)
						ereport(LOG,
								(errmsg("CopyFail message from frontend.")));
					ereport(DEBUG1,
							(errmsg("copy data rows"),
							 errdetail("invalid copyin kind. expected 'd' got '%c'", kind)));
					break;
				}
			}
			else
				string = pool_read_string(frontend, &len, 1);
		}
		else
		{
			/* CopyOut */
			if (MAJOR(backend) == PROTO_MAJOR_V3)
			{
				signed char kind;

				kind = pool_read_kind(backend);

				SimpleForwardToFrontend(kind, frontend, backend);

				/* CopyData? */
				if (kind == 'd')
					continue;
				else
					break;
			}
			else
			{
				for (i = 0; i < NUM_BACKENDS; i++)
				{
					if (VALID_BACKEND(i))
					{
						string = pool_read_string(CONNECTION(backend, i), &len, 1);
					}
				}
			}
		}

		if (string == NULL)
			ereport(FATAL,
					(errmsg("unable to copy data rows"),
					 errdetail("cannot read string message from backend")));

#ifdef DEBUG
		strlcpy(buf, string, sizeof(buf));
		ereport(DEBUG1,
				(errmsg("copy data rows"),
				 errdetail("copy line %d %d bytes :%s:", j++, len, buf)));
#endif

		if (copyin)
		{
			for (i = 0; i < NUM_BACKENDS; i++)
			{
				if (VALID_BACKEND(i))
				{
					pool_write(CONNECTION(backend, i), string, len);
				}
			}
		}
		else
			pool_write(frontend, string, len);

		if (len == PROTO_MAJOR_V3)
		{
			/* end of copy? */
			if (string[0] == '\\' &&
				string[1] == '.' &&
				string[2] == '\n')
			{
				break;
			}
		}
	}

	/*
	 * Wait till backend responds
	 */
	if (copyin)
	{
		for (i = 0; i < NUM_BACKENDS; i++)
		{
			if (VALID_BACKEND(i))
			{
				pool_flush(CONNECTION(backend, i));

				/*
				 * Check response from the backend.  First check SSL and read
				 * buffer of the backend. It is possible that there's an error
				 * message in the buffer if the COPY command went wrong.
				 * Otherwise wait for data arrival to the backend socket.
				 */
				if (!pool_ssl_pending(CONNECTION(backend, i)) &&
					pool_read_buffer_is_empty(CONNECTION(backend, i)) &&
					synchronize(CONNECTION(backend, i)))
					ereport(FATAL,
							(return_code(2),
							 errmsg("unable to copy data rows"),
							 errdetail("failed to synchronize")));
			}
		}
	}
	else
		pool_flush(frontend);

	return POOL_CONTINUE;
}

/*
 * This function raises intentional error to make backends the same
 * transaction state.
 */
void
raise_intentional_error_if_need(POOL_CONNECTION_POOL * backend)
{
	int			i;
	POOL_SESSION_CONTEXT *session_context;
	POOL_QUERY_CONTEXT *query_context;

	/* Get session context */
	session_context = pool_get_session_context(false);

	query_context = session_context->query_context;

	if (MAIN_REPLICA &&
		TSTATE(backend, PRIMARY_NODE_ID) == 'T' &&
		PRIMARY_NODE_ID != MAIN_NODE_ID &&
		query_context &&
		is_select_query(query_context->parse_tree, query_context->original_query))
	{
		pool_set_node_to_be_sent(query_context, PRIMARY_NODE_ID);
		if (pool_is_doing_extended_query_message())
		{
			do_error_execute_command(backend, PRIMARY_NODE_ID, PROTO_MAJOR_V3);
		}
		else
		{
			do_error_command(CONNECTION(backend, PRIMARY_NODE_ID), MAJOR(backend));
		}
		ereport(DEBUG1,
				(errmsg("raising intentional error"),
				 errdetail("generating intentional error to sync backends transaction states")));
	}

	if (REPLICATION &&
		TSTATE(backend, REAL_MAIN_NODE_ID) == 'T' &&
		!pool_config->replicate_select &&
		query_context &&
		is_select_query(query_context->parse_tree, query_context->original_query))
	{
		for (i = 0; i < NUM_BACKENDS; i++)
		{
			/*
			 * Send a syntax error query to all backends except the node which
			 * the original query was sent.
			 */
			if (pool_is_node_to_be_sent(query_context, i))
				continue;
			else
				pool_set_node_to_be_sent(query_context, i);

			if (VALID_BACKEND(i))
			{
				/*
				 * We must abort transaction to sync transaction state. If the
				 * error was caused by an Execute message, we must send
				 * invalid Execute message to abort transaction.
				 *
				 * Because extended query protocol ignores all messages before
				 * receiving Sync message inside error state.
				 */
				if (pool_is_doing_extended_query_message())
				{
					do_error_execute_command(backend, i, PROTO_MAJOR_V3);
				}
				else
				{
					do_error_command(CONNECTION(backend, i), MAJOR(backend));
				}
			}
		}
	}
}

/*---------------------------------------------------
 * Check various errors from backend.  return values:
 *  0: no error
 *  1: deadlock detected
 *  2: serialization error detected
 *  3: query cancel
 * detected: 4
 *---------------------------------------------------
 */
static int
check_errors(POOL_CONNECTION_POOL * backend, int backend_id)
{

	/*
	 * Check dead lock error on the main node and abort transactions on all
	 * nodes if so.
	 */
	if (detect_deadlock_error(CONNECTION(backend, backend_id), MAJOR(backend)) == SPECIFIED_ERROR)
		return 1;

	/*-----------------------------------------------------------------------------------
	 * Check serialization failure error and abort
	 * transactions on all nodes if so. Otherwise we allow
	 * data inconsistency among DB nodes. See following
	 * scenario: (M:main, S:replica)
	 *
	 * M:S1:BEGIN;
	 * M:S2:BEGIN;
	 * S:S1:BEGIN;
	 * S:S2:BEGIN;
	 * M:S1:SET TRANSACTION ISOLATION LEVEL SERIALIZABLE;
	 * M:S2:SET TRANSACTION ISOLATION LEVEL SERIALIZABLE;
	 * S:S1:SET TRANSACTION ISOLATION LEVEL SERIALIZABLE;
	 * S:S2:SET TRANSACTION ISOLATION LEVEL SERIALIZABLE;
	 * M:S1:UPDATE t1 SET i = i + 1;
	 * S:S1:UPDATE t1 SET i = i + 1;
	 * M:S2:UPDATE t1 SET i = i + 1; <-- blocked
	 * S:S1:COMMIT;
	 * M:S1:COMMIT;
	 * M:S2:ERROR:  could not serialize access due to concurrent update
	 * S:S2:UPDATE t1 SET i = i + 1; <-- success in UPDATE and data becomes inconsistent!
	 *-----------------------------------------------------------------------------------
	 */
	if (detect_serialization_error(CONNECTION(backend, backend_id), MAJOR(backend), true) == SPECIFIED_ERROR)
		return 2;

	/*-------------------------------------------------------------------------------
	 * check "SET TRANSACTION ISOLATION LEVEL must be called before any query" error.
	 * This happens in following scenario:
	 *
	 * M:S1:BEGIN;
	 * S:S1:BEGIN;
	 * M:S1:SELECT 1; <-- only sent to MAIN
	 * M:S1:SET TRANSACTION ISOLATION LEVEL SERIALIZABLE;
	 * S:S1:SET TRANSACTION ISOLATION LEVEL SERIALIZABLE;
	 * M: <-- error
	 * S: <-- ok since no previous SELECT is sent. kind mismatch error occurs!
	 *-------------------------------------------------------------------------------
	 */

	if (detect_active_sql_transaction_error(CONNECTION(backend, backend_id), MAJOR(backend)) == SPECIFIED_ERROR)
		return 3;

	/* check query cancel error */
	if (detect_query_cancel_error(CONNECTION(backend, backend_id), MAJOR(backend)) == SPECIFIED_ERROR)
		return 4;

	return 0;
}

static void
generate_error_message(char *prefix, int specific_error, char *query)
{
	POOL_SESSION_CONTEXT *session_context;

	static char *error_messages[] = {
		"received deadlock error message from main node. query: %s",
		"received serialization failure error message from main node. query: %s",
		"received SET TRANSACTION ISOLATION LEVEL must be called before any query error. query: %s",
		"received query cancel error message from main node. query: %s"
	};

	StringInfoData	   msg;

	session_context = pool_get_session_context(true);
	if (!session_context)
		return;

	if (specific_error < 1 || specific_error > sizeof(error_messages) / sizeof(char *))
	{
		ereport(LOG,
				(errmsg("generate_error_message: invalid specific_error: %d", specific_error)));
		return;
	}

	specific_error--;

	initStringInfo(&msg);

	appendStringInfoString(&msg, error_messages[specific_error]);
	ereport(LOG,
			(errmsg(msg.data, query)));

	pfree(msg.data);
}

/*
 * Make per DB node statement log
 */
void
per_node_statement_log(POOL_CONNECTION_POOL * backend, int node_id, char *query)
{
	POOL_CONNECTION_POOL_SLOT *slot = backend->slots[node_id];

	if (pool_config->log_per_node_statement)
		ereport(LOG,
				(errmsg("DB node id: %d backend pid: %d statement: %s", node_id, ntohl(slot->pid), query)));
}

/*
 * Check kind and produce error message
 * All data read in this function is returned to stream.
 */
void
per_node_error_log(POOL_CONNECTION_POOL * backend, int node_id, char *query, char *prefix, bool unread)
{
	POOL_CONNECTION_POOL_SLOT *slot = backend->slots[node_id];
	char	   *message;
	char	   kind;

	pool_read(CONNECTION(backend, node_id), &kind, sizeof(kind));
	pool_unread(CONNECTION(backend, node_id), &kind, sizeof(kind));

	if (kind != 'E' && kind != 'N')
	{
		return;
	}

	if (pool_extract_error_message(true, CONNECTION(backend, node_id), MAJOR(backend), unread, &message) == 1)
	{
		ereport(LOG,
				(errmsg("%s: DB node id: %d backend pid: %d statement: \"%s\" message: \"%s\"",
						prefix, node_id, ntohl(slot->pid), query, message)));
		pfree(message);
	}
}

/*
 * Send parse message to primary/main node and wait for reply if particular
 * message is not yet parsed on the primary/main node but parsed on other
 * node. Caller must provide the parse message data as "message".
 */
static POOL_STATUS parse_before_bind(POOL_CONNECTION * frontend,
									 POOL_CONNECTION_POOL * backend,
									 POOL_SENT_MESSAGE * message,
									 POOL_SENT_MESSAGE * bind_message)
{
	int			i;
	int			len = message->len;
	char		kind = '\0';
	char	   *contents = message->contents;
	bool		parse_was_sent = false;
	bool		backup[MAX_NUM_BACKENDS];
	POOL_QUERY_CONTEXT *qc = message->query_context;

	memcpy(backup, qc->where_to_send, sizeof(qc->where_to_send));

	if (SL_MODE)
	{
		if (message->kind == 'P' && qc->where_to_send[PRIMARY_NODE_ID] == 0)
		{
			POOL_PENDING_MESSAGE *pmsg;
			POOL_QUERY_CONTEXT *new_qc;
			char		message_body[1024];
			int			offset;
			int			message_len;

			/*
			 * we are in streaming replication mode and the parse message has
			 * not been sent to primary yet
			 */

			/* Prepare modified query context */
			new_qc = pool_query_context_shallow_copy(qc);
			memset(new_qc->where_to_send, 0, sizeof(new_qc->where_to_send));
			new_qc->where_to_send[PRIMARY_NODE_ID] = 1;
			new_qc->virtual_main_node_id = PRIMARY_NODE_ID;
			new_qc->load_balance_node_id = PRIMARY_NODE_ID;

			/*
			 * Before sending the parse message to the primary, we need to
			 * close the named statement. Otherwise we will get an error from
			 * backend if the named statement already exists. This could
			 * happen if parse_before_bind is called with a bind message
			 * using the same named statement. If the named statement does not
			 * exist, it's fine. PostgreSQL just ignores a request trying to
			 * close a non-existing statement. If the statement is unnamed
			 * one, we do not need it because unnamed statement can be
			 * overwritten anytime.
			 */
			message_body[0] = 'S';
			offset = strlen(bind_message->contents) + 1;

			ereport(DEBUG1,
					(errmsg("parse before bind"),
					 errdetail("close statement: %s", bind_message->contents + offset)));

			/* named statement? */
			if (bind_message->contents[offset] != '\0')
			{
				message_len = 1 + strlen(bind_message->contents + offset) + 1;
				StrNCpy(message_body + 1, bind_message->contents + offset, sizeof(message_body) - 1);
				pool_extended_send_and_wait(qc, "C", message_len, message_body, 1, PRIMARY_NODE_ID, false);
				/* Add pending message */
				pmsg = pool_pending_message_create('C', message_len, message_body);
				pmsg->not_forward_to_frontend = true;
				pool_pending_message_dest_set(pmsg, new_qc);
				pool_pending_message_add(pmsg);
				pool_pending_message_free_pending_message(pmsg);
			}

			/* Send parse message to primary node */
			ereport(DEBUG1,
					(errmsg("parse before bind"),
					 errdetail("waiting for primary completing parse")));

			pool_extended_send_and_wait(qc, "P", len, contents, 1, PRIMARY_NODE_ID, false);

			/* Add pending message */
			pmsg = pool_pending_message_create('P', len, contents);
			pmsg->not_forward_to_frontend = true;
			pool_pending_message_dest_set(pmsg, new_qc);
			pool_pending_message_add(pmsg);
			pool_pending_message_free_pending_message(pmsg);

			/* Replace the query context of bind message */
			bind_message->query_context = new_qc;

#ifdef NOT_USED
			/*
			 * XXX 	pool_remove_sent_message() will pfree memory allocated by "contents".
			 */

			/* Remove old sent message */
			pool_remove_sent_message('P', contents);
			/* Create and add sent message of this parse message */
			msg = pool_create_sent_message('P', len, contents, 0, contents, new_qc);
			pool_add_sent_message(msg);
#endif
			/* Replace the query context of parse message */
			message->query_context = new_qc;

			return POOL_CONTINUE;
		}
		else
		{
			ereport(DEBUG1,
					(errmsg("parse before bind"),
					 errdetail("no need to re-send parse")));
			return POOL_CONTINUE;
		}
	}
	else
	{
		/* expect to send to main node only */
		for (i = 0; i < NUM_BACKENDS; i++)
		{
			if (qc->where_to_send[i] && statecmp(qc->query_state[i], POOL_PARSE_COMPLETE) < 0)
			{
				ereport(DEBUG1,
						(errmsg("parse before bind"),
						 errdetail("waiting for backend %d completing parse", i)));

				pool_extended_send_and_wait(qc, "P", len, contents, 1, i, false);
			}
			else
			{
				qc->where_to_send[i] = 0;
			}
		}
	}

	for (i = 0; i < NUM_BACKENDS; i++)
	{
		if (qc->where_to_send[i])
		{
			parse_was_sent = true;
			break;
		}
	}

	if (!SL_MODE && parse_was_sent)
	{
		pool_set_query_in_progress();

		while (kind != '1')
		{
			PG_TRY();
			{
				read_kind_from_backend(frontend, backend, &kind);
				pool_discard_packet_contents(backend);
			}
			PG_CATCH();
			{
				memcpy(qc->where_to_send, backup, sizeof(backup));
				PG_RE_THROW();
			}
			PG_END_TRY();
		}
	}

	memcpy(qc->where_to_send, backup, sizeof(backup));
	return POOL_CONTINUE;
}

/*
 * Find victim nodes by "decide by majority" rule and returns array
 * of victim node ids. If no victim is found, return NULL.
 *
 * Arguments:
 * ntuples: Array of number of affected tuples. -1 represents down node.
 * nmembers: Number of elements in ntuples.
 * main_node: The main node id. Less than 0 means ignore this parameter.
 * number_of_nodes: Number of elements in victim nodes array.
 *
 * Note: If no one wins and main_node >= 0, winner would be the
 * main and other nodes who has same number of tuples as the main.
 *
 * Caution: Returned victim node array is allocated in static memory
 * of this function. Subsequent calls to this function will overwrite
 * the memory.
 */
static int *
find_victim_nodes(int *ntuples, int nmembers, int main_node, int *number_of_nodes)
{
	static int	victim_nodes[MAX_NUM_BACKENDS];
	static int	votes[MAX_NUM_BACKENDS];
	int			maxvotes;
	int			majority_ntuples;
	int			me;
	int			cnt;
	int			healthy_nodes;
	int			i,
				j;

	healthy_nodes = 0;
	*number_of_nodes = 0;
	maxvotes = 0;
	majority_ntuples = 0;

	for (i = 0; i < nmembers; i++)
	{
		me = ntuples[i];

		/* Health node? */
		if (me < 0)
		{
			votes[i] = -1;
			continue;
		}

		healthy_nodes++;
		votes[i] = 1;

		for (j = 0; j < nmembers; j++)
		{
			if (i != j && me == ntuples[j])
			{
				votes[i]++;

				if (votes[i] > maxvotes)
				{
					maxvotes = votes[i];
					majority_ntuples = me;
				}
			}
		}
	}

	/* Everyone is different */
	if (maxvotes == 1)
	{
		/* Main node is specified? */
		if (main_node < 0)
			return NULL;

		/*
		 * If main node is specified, let it and others who has same ntuples
		 * win.
		 */
		majority_ntuples = ntuples[main_node];
	}
	else
	{
		/* Find number of majority */
		cnt = 0;
		for (i = 0; i < nmembers; i++)
		{
			if (votes[i] == maxvotes)
			{
				cnt++;
			}
		}

		if (cnt <= healthy_nodes / 2.0)
		{
			/* No one wins */

			/* Main node is specified? */
			if (main_node < 0)
				return NULL;

			/*
			 * If main node is specified, let it and others who has same
			 * ntuples win.
			 */
			majority_ntuples = ntuples[main_node];
		}
	}

	/* Make victim nodes list */
	for (i = 0; i < nmembers; i++)
	{
		if (ntuples[i] >= 0 && ntuples[i] != majority_ntuples)
		{
			victim_nodes[(*number_of_nodes)++] = i;
		}
	}

	return victim_nodes;
}


/*
 * flatten_set_variable_args
 *		Given a parsenode List as emitted by the grammar for SET,
 *		convert to the flat string representation used by GUC.
 *
 * We need to be told the name of the variable the args are for, because
 * the flattening rules vary (ugh).
 *
 * The result is NULL if args is NIL (ie, SET ... TO DEFAULT), otherwise
 * a palloc'd string.
 */
static char *
flatten_set_variable_args(const char *name, List *args)
{
	StringInfoData buf;
	ListCell   *l;

	/* Fast path if just DEFAULT */
	if (args == NIL)
		return NULL;

	initStringInfo(&buf);

	/*
	 * Each list member may be a plain A_Const node, or an A_Const within a
	 * TypeCast; the latter case is supported only for ConstInterval arguments
	 * (for SET TIME ZONE).
	 */
	foreach(l, args)
	{
		Node	   *arg = (Node *) lfirst(l);
		char	   *val;
		A_Const    *con;

		if (l != list_head(args))
			appendStringInfoString(&buf, ", ");

		if (IsA(arg, TypeCast))
		{
			TypeCast   *tc = (TypeCast *) arg;

			arg = tc->arg;
		}

		if (!IsA(arg, A_Const))
			elog(ERROR, "unrecognized node type: %d", (int) nodeTag(arg));
		con = (A_Const *) arg;

		switch (nodeTag(&con->val))
		{
			case T_Integer:
				appendStringInfo(&buf, "%d", intVal(&con->val));
				break;
			case T_Float:
				/* represented as a string, so just copy it */
				appendStringInfoString(&buf, strVal(&con->val));
				break;
			case T_String:
				val = strVal(&con->val);
				appendStringInfoString(&buf, val);
				break;
			default:
				ereport(ERROR, (errmsg("unrecognized node type: %d",
									   (int) nodeTag(&con->val))));
				break;
		}
	}

	return buf.data;
}

#ifdef NOT_USED
/* Called when sync message is received.
 * Wait till ready for query received.
 */
static void
pool_wait_till_ready_for_query(POOL_CONNECTION_POOL * backend)
{
	char		kind;
	int			len;
	int			poplen;
	char	   *buf;
	int			i;

	for (i = 0; i < NUM_BACKENDS; i++)
	{
		if (VALID_BACKEND(i))
		{
			for (;;)
			{
				pool_read(CONNECTION(backend, i), &kind, sizeof(kind));
				ereport(DEBUG5,
						(errmsg("pool_wait_till_ready_for_query: kind: %c", kind)));
				pool_push(CONNECTION(backend, i), &kind, sizeof(kind));
				pool_read(CONNECTION(backend, i), &len, sizeof(len));
				pool_push(CONNECTION(backend, i), &len, sizeof(len));
				if ((ntohl(len) - sizeof(len)) > 0)
				{
					buf = pool_read2(CONNECTION(backend, i), ntohl(len) - sizeof(len));
					pool_push(CONNECTION(backend, i), buf, ntohl(len) - sizeof(len));
				}

				if (kind == 'Z')	/* Ready for query? */
				{
					pool_pop(CONNECTION(backend, i), &poplen);
					ereport(DEBUG5,
							(errmsg("pool_wait_till_ready_for_query: backend:%d ready for query found. buffer length:%d",
									i, CONNECTION(backend, i)->len)));
					break;
				}
			}
		}
	}
}
#endif

/*
 * Called when error response received in streaming replication mode and doing
 * extended query. Remove all pending messages and backend message buffer data
 * except POOL_SYNC pending message and ready for query.  If sync message is
 * not received yet, continue to read data from frontend until a sync message
 * is read.
 */
static void
pool_discard_except_sync_and_ready_for_query(POOL_CONNECTION * frontend,
											 POOL_CONNECTION_POOL * backend)
{
	POOL_PENDING_MESSAGE *pmsg;
	int			i;

	if (!pool_is_doing_extended_query_message() || !SL_MODE)
		return;

	/*
	 * Check to see if we already received a sync message. If not, call
	 * ProcessFrontendResponse() to get the sync message from client.
	 */
	pmsg = pool_pending_message_get(POOL_SYNC);
	if (pmsg == NULL)
	{
		char		kind;
		int			len;
		POOL_PENDING_MESSAGE *msg;
		char	   *contents = NULL;

		for (;;)
		{
			pool_read(frontend, &kind, sizeof(kind));
			pool_read(frontend, &len, sizeof(len));
			len = ntohl(len) - sizeof(len);
			if (len > 0)
				contents = pool_read2(frontend, len);
			if (kind == 'S')
			{
				msg = pool_pending_message_create('S', 0, NULL);
				pool_pending_message_add(msg);
				pool_pending_message_free_pending_message(msg);
				SimpleForwardToBackend(kind, frontend, backend, len, contents);
				break;
			}
		}
	}
	else
		pool_pending_message_free_pending_message(pmsg);

	/* Remove all pending messages except sync message */
	do
	{
		pmsg = pool_pending_message_head_message();
		if (pmsg && pmsg->type == POOL_SYNC)
		{
			ereport(DEBUG1,
					(errmsg("Process backend response: sync pending message found after receiving error response")));
			pool_unset_ignore_till_sync();
			pool_pending_message_free_pending_message(pmsg);
			break;
		}
		pool_pending_message_free_pending_message(pmsg);
		pmsg = pool_pending_message_pull_out();
		pool_pending_message_free_pending_message(pmsg);
	}
	while (pmsg);

	pool_pending_message_reset_previous_message();

	/* Discard read buffer except "Ready for query" */
	for (i = 0; i < NUM_BACKENDS; i++)
	{
		if (VALID_BACKEND(i))
		{
			char		kind;
			int			len;
			int			sts;

			while (!pool_read_buffer_is_empty(CONNECTION(backend, i)))
			{
				sts = pool_read(CONNECTION(backend, i), &kind, sizeof(kind));
				if (sts < 0 || kind == '\0')
				{
					ereport(DEBUG1,
							(errmsg("pool_discard_except_sync_and_ready_for_query: EOF detected while reading from backend: %d buffer length: %d sts: %d",
									i, CONNECTION(backend, i)->len, sts)));
					pool_unread(CONNECTION(backend, i), &kind, sizeof(kind));
					break;
				}

				if (kind == 'Z')	/* Ready for query? */
				{
					pool_unread(CONNECTION(backend, i), &kind, sizeof(kind));
					ereport(DEBUG1,
							(errmsg("pool_discard_except_sync_and_ready_for_query: Ready for query found. backend:%d",
									i)));
					break;
				}
				else
				{
					/* Read and discard packet */
					pool_read(CONNECTION(backend, i), &len, sizeof(len));
					if ((ntohl(len) - sizeof(len)) > 0)
					{
						pool_read2(CONNECTION(backend, i), ntohl(len) - sizeof(len));
					}
					ereport(DEBUG1,
							(errmsg("pool_discard_except_sync_and_ready_for_query: discarding packet %c (len:%lu) of backend:%d", kind, ntohl(len) - sizeof(len), i)));
				}
			}
		}
	}
}

/*
 * Handle misc treatment when a command successfully completed.
 * Preconditions: query is in progress. The command is succeeded.
 */
void
pool_at_command_success(POOL_CONNECTION * frontend, POOL_CONNECTION_POOL * backend)
{
	Node	   *node;
	char	   *query;

	/* Sanity checks */
	if (!pool_is_query_in_progress())
	{
		ereport(ERROR,
				(errmsg("pool_at_command_success: query is not in progress")));
	}

	if (!pool_is_command_success())
	{
		ereport(ERROR,
				(errmsg("pool_at_command_success: command did not succeed")));
	}

	node = pool_get_parse_tree();

	if (!node)
	{
		ereport(ERROR,
				(errmsg("pool_at_command_success: no parse tree found")));
	}

	query = pool_get_query_string();

	if (query == NULL)
	{
		ereport(ERROR,
				(errmsg("pool_at_command_success: no query found")));
	}

	/*
	 * If the query was BEGIN/START TRANSACTION, clear the history that we had
	 * a writing command in the transaction and forget the transaction
	 * isolation level.
	 *
	 * XXX If BEGIN is received while we are already in an explicit
	 * transaction, the command *successes* (just with a NOTICE message). In
	 * this case we lose "writing_transaction" etc. info.
	 */
	if (is_start_transaction_query(node))
	{
		if (pool_config->disable_load_balance_on_write != DLBOW_TRANS_TRANSACTION)
			pool_unset_writing_transaction();

		pool_unset_failed_transaction();
		pool_unset_transaction_isolation();
	}

	/*
	 * If the query was COMMIT/ABORT, clear the history that we had a writing
	 * command in the transaction and forget the transaction isolation level.
	 * This is necessary if succeeding transaction is not an explicit one.
	 */
	else if (is_commit_or_rollback_query(node))
	{
		if (pool_config->disable_load_balance_on_write != DLBOW_TRANS_TRANSACTION)
			pool_unset_writing_transaction();

		pool_unset_failed_transaction();
		pool_unset_transaction_isolation();
	}

	/*
	 * SET TRANSACTION ISOLATION LEVEL SERIALIZABLE or SET SESSION
	 * CHARACTERISTICS AS TRANSACTION ISOLATION LEVEL SERIALIZABLE, remember
	 * it.
	 */
	else if (is_set_transaction_serializable(node))
	{
		pool_set_transaction_isolation(POOL_SERIALIZABLE);
	}

	/*
	 * If 2PC commands has been executed, automatically close transactions on
	 * standbys if there is any open transaction since 2PC commands close
	 * transaction on primary.
	 */
	else if (is_2pc_transaction_query(node))
	{
		close_standby_transactions(frontend, backend);
	}

	else if (!is_select_query(node, query) || pool_has_function_call(node))
	{
		/*
		 * If the query was not READ SELECT, and we are in an explicit
		 * transaction or disable_load_balance_on_write is 'ALWAYS', remember
		 * that we had a write query in this transaction.
		 */
		if (TSTATE(backend, MAIN_REPLICA ? PRIMARY_NODE_ID : REAL_MAIN_NODE_ID) == 'T' ||
			pool_config->disable_load_balance_on_write == DLBOW_ALWAYS)
		{
			/*
			 * However, if the query is "SET TRANSACTION READ ONLY" or its
			 * variant, don't set it.
			 */
			if (!pool_is_transaction_read_only(node))
			{
				ereport(DEBUG1,
						(errmsg("not SET TRANSACTION READ ONLY")));

				pool_set_writing_transaction();
			}
		}

		/*
		 * If the query was CREATE TEMP TABLE, discard temp table relcache
		 * because we might have had persistent table relation cache which has
		 * table name as the temp table.
		 */
		if (IsA(node, CreateStmt))
		{
			CreateStmt *create_table_stmt = (CreateStmt *) node;

			if (create_table_stmt->relation->relpersistence == 't')
				discard_temp_table_relcache();
		}
	}
}

/*
 * read message length (V3 only)
 */
int
pool_read_message_length(POOL_CONNECTION_POOL * cp)
{
	int			length,
				length0;
	int			i;

	/* read message from main node */
	pool_read(CONNECTION(cp, MAIN_NODE_ID), &length0, sizeof(length0));
	length0 = ntohl(length0);

	ereport(DEBUG5,
			(errmsg("reading message length"),
			 errdetail("slot: %d length: %d", MAIN_NODE_ID, length0)));

	for (i = 0; i < NUM_BACKENDS; i++)
	{
		if (!VALID_BACKEND(i) || IS_MAIN_NODE_ID(i))
		{
			continue;
		}

		pool_read(CONNECTION(cp, i), &length, sizeof(length));

		length = ntohl(length);
		ereport(DEBUG5,
				(errmsg("reading message length"),
				 errdetail("slot: %d length: %d", i, length)));

		if (length != length0)
		{
			ereport(LOG,
					(errmsg("unable to read message length"),
					 errdetail("message length (%d) in slot %d does not match with slot 0(%d)", length, i, length0)));
			return -1;
		}

	}

	if (length0 < 0)
		ereport(ERROR,
				(errmsg("unable to read message length"),
				 errdetail("invalid message length (%d)", length)));

	return length0;
}

/*
 * read message length2 (V3 only)
 * unlike pool_read_message_length, this returns an array of message length.
 * The array is in the static storage, thus it will be destroyed by subsequent calls.
 */
int *
pool_read_message_length2(POOL_CONNECTION_POOL * cp)
{
	int			length,
				length0;
	int			i;
	static int	length_array[MAX_CONNECTION_SLOTS];

	/* read message from main node */
	pool_read(CONNECTION(cp, MAIN_NODE_ID), &length0, sizeof(length0));

	length0 = ntohl(length0);
	length_array[MAIN_NODE_ID] = length0;
	ereport(DEBUG5,
			(errmsg("reading message length"),
			 errdetail("main slot: %d length: %d", MAIN_NODE_ID, length0)));

	for (i = 0; i < NUM_BACKENDS; i++)
	{
		if (VALID_BACKEND(i) && !IS_MAIN_NODE_ID(i))
		{
			pool_read(CONNECTION(cp, i), &length, sizeof(length));

			length = ntohl(length);
			ereport(DEBUG5,
					(errmsg("reading message length"),
					 errdetail("main slot: %d length: %d", i, length)));

			if (length != length0)
			{
				ereport(DEBUG1,
						(errmsg("reading message length"),
						 errdetail("message length (%d) in slot %d does not match with slot 0(%d)", length, i, length0)));
			}

			if (length < 0)
			{
				ereport(ERROR,
						(errmsg("unable to read message length"),
						 errdetail("invalid message length (%d)", length)));
			}

			length_array[i] = length;
		}

	}
	return &length_array[0];
}

/*
 * By given message length array, emit log message to complain the difference.
 * If no difference, no log is emitted.
 * If "name" is not NULL, it is added to the log message.
 */
void
pool_emit_log_for_message_length_diff(int *length_array, char *name)
{
	int			length0,	/* message length of main node id */
				length;
	int			i;

	length0 = length_array[MAIN_NODE_ID];

	for (i = 0; i < NUM_BACKENDS; i++)
	{
		if (VALID_BACKEND(i))
		{
			length = length_array[i];

			if (length != length0)
			{
				if (name != NULL)
					ereport(LOG,
							(errmsg("ParameterStatus \"%s\": node %d message length %d is different from main node message length %d",
									name, i, length_array[i], length0)));
				else
					ereport(LOG,
							(errmsg("node %d message length %d is different from main node message length %d",
									i, length_array[i], length0)));
			}
		}
	}
}

signed char
pool_read_kind(POOL_CONNECTION_POOL * cp)
{
	char		kind0,
				kind;
	int			i;

	kind = -1;
	kind0 = 0;

	for (i = 0; i < NUM_BACKENDS; i++)
	{
		if (!VALID_BACKEND(i))
		{
			continue;
		}

		pool_read(CONNECTION(cp, i), &kind, sizeof(kind));

		if (IS_MAIN_NODE_ID(i))
		{
			kind0 = kind;
		}
		else
		{
			if (kind != kind0)
			{
				char	   *message;

				if (kind0 == 'E')
				{
					if (pool_extract_error_message(false, MAIN(cp), MAJOR(cp), true, &message) == 1)
					{
						ereport(LOG,
								(errmsg("pool_read_kind: error message from main backend:%s", message)));
						pfree(message);
					}
				}
				else if (kind == 'E')
				{
					if (pool_extract_error_message(false, CONNECTION(cp, i), MAJOR(cp), true, &message) == 1)
					{
						ereport(LOG,
								(errmsg("pool_read_kind: error message from %d th backend:%s", i, message)));
						pfree(message);
					}
				}
				ereport(ERROR,
						(errmsg("unable to read message kind"),
						 errdetail("kind does not match between main(%x) slot[%d] (%x)", kind0, i, kind)));
			}
		}
	}

	return kind;
}

int
pool_read_int(POOL_CONNECTION_POOL * cp)
{
	int			data0,
				data;
	int			i;

	data = -1;
	data0 = 0;

	for (i = 0; i < NUM_BACKENDS; i++)
	{
		if (!VALID_BACKEND(i))
		{
			continue;
		}
		pool_read(CONNECTION(cp, i), &data, sizeof(data));
		if (IS_MAIN_NODE_ID(i))
		{
			data0 = data;
		}
		else
		{
			if (data != data0)
			{
				ereport(ERROR,
						(errmsg("unable to read int value"),
						 errdetail("data does not match between between main(%x) slot[%d] (%x)", data0, i, data)));

			}
		}
	}
	return data;
}

/*
 * Acquire snapshot in snapshot isolation mode.
 * If tstate_check is true, check the transaction state is 'T' (idle in transaction).
 * In case of starting an internal transaction, this should be false.
 */
static void
si_get_snapshot(POOL_CONNECTION * frontend, POOL_CONNECTION_POOL * backend, Node * node, bool tstate_check)
{
	POOL_SESSION_CONTEXT *session_context;

	session_context = pool_get_session_context(true);
	if (!session_context)
		return;

	/*
	 * From now on it is possible that query is actually sent to backend.
	 * So we need to acquire snapshot while there's no committing backend
	 * in snapshot isolation mode except while processing reset queries.
	 * For this purpose, we send a query to know whether the transaction
	 * is READ ONLY or not.  Sending actual user's query is not possible
	 * because it might cause rw-conflict, which in turn causes a
	 * deadlock.
	 */
	if (pool_config->backend_clustering_mode == CM_SNAPSHOT_ISOLATION &&
		(!tstate_check || (tstate_check && TSTATE(backend, MAIN_NODE_ID) == 'T')) &&
		si_snapshot_acquire_command(node) &&
		!si_snapshot_prepared() &&
		frontend && frontend->no_forward == 0)
	{
		int	i;

		si_acquire_snapshot();

		for (i = 0; i < NUM_BACKENDS; i++)
		{
			static	char *si_query = "SELECT current_setting('transaction_read_only')";
			POOL_SELECT_RESULT *res;

			/* We cannot use VALID_BACKEND macro here because load balance
			 * node has not been decided yet.
			 */
			if (!VALID_BACKEND_RAW(i))
				continue;

			do_query(CONNECTION(backend, i), si_query, &res, MAJOR(backend));
			if (res)
			{
				if (res->data[0] && !strcmp(res->data[0], "on"))
					session_context->transaction_read_only = true;
				else
					session_context->transaction_read_only = false;
				free_select_result(res);
			}
			per_node_statement_log(backend, i, si_query);
		}

		si_snapshot_acquired();
	}
}

/*
 * Check if the transaction is in abort status. If so, we do nothing and just
 * return error message and ready for query message to frontend, then return
 * false to caller.
 */
static bool
check_transaction_state_and_abort(char *query, Node *node, POOL_CONNECTION * frontend,
								  POOL_CONNECTION_POOL * backend)
{
	int		len;

	/*
	 * Are we in failed transaction and the command is not a transaction close
	 * command?
	 */
	if (pool_is_failed_transaction() && !is_commit_or_rollback_query(node))
	{
		StringInfoData buf;

		initStringInfo(&buf);
		appendStringInfo(&buf, "statement: %s", query);

		/* send an error message to frontend */
		pool_send_error_message(
			frontend,
			MAJOR(backend),
			"25P02",
			"current transaction is aborted, commands ignored until end of transaction block",
			buf.data,
			"",
			__FILE__,
			__LINE__);

		pfree(buf.data);

		/* send ready for query to frontend */
		pool_write(frontend, "Z", 1);
		len = 5;
		len = htonl(len);
		pool_write(frontend, &len, sizeof(len));
		pool_write(frontend, "E", 1);
		pool_flush(frontend);

		return false;
	}
	return true;
}
