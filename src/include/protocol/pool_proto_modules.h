/* -*-pgsql-c-*- */
/*
 *
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
 * pool_proto_modules.h.: header file for pool_proto_modules.c, pool_proto2.c
 * and pool_process_query.c
 *
 */

#ifndef POOL_PROTO_MODULES_H
#define POOL_PROTO_MODULES_H

#include "parser/parser.h"
#include "parser/pg_list.h"
#include "parser/parsenodes.h"
#include "context/pool_session_context.h"
#include "utils/pool_process_reporting.h"

#define SPECIFIED_ERROR 1
#define POOL_ERROR_QUERY "send invalid query from pgpool to abort transaction"

extern char *copy_table;		/* copy table name */
extern char *copy_schema;		/* copy table name */
extern char copy_delimiter;		/* copy delimiter char */
extern char *copy_null;			/* copy null string */

extern int	is_select_pgcatalog;
extern int	is_select_for_update;	/* also for SELECT ... INTO */
extern char *parsed_query;

/*
 * modules defined in pool_proto_modules.c
 */
extern POOL_STATUS SimpleQuery(POOL_CONNECTION * frontend,
							   POOL_CONNECTION_POOL * backend,
							   int len, char *contents);

extern POOL_STATUS Execute(POOL_CONNECTION * frontend,
						   POOL_CONNECTION_POOL * backend,
						   int len, char *contents);

extern POOL_STATUS Parse(POOL_CONNECTION * frontend,
						 POOL_CONNECTION_POOL * backend,
						 int len, char *contents);

extern POOL_STATUS Bind(POOL_CONNECTION * frontend,
						POOL_CONNECTION_POOL * backend,
						int len, char *contents);

extern POOL_STATUS Describe(POOL_CONNECTION * frontend,
							POOL_CONNECTION_POOL * backend,
							int len, char *contents);

extern POOL_STATUS Close(POOL_CONNECTION * frontend,
						 POOL_CONNECTION_POOL * backend,
						 int len, char *contents);

extern POOL_STATUS FunctionCall3(POOL_CONNECTION * frontend,
								 POOL_CONNECTION_POOL * backend,
								 int len, char *contents);

extern POOL_STATUS ReadyForQuery(POOL_CONNECTION * frontend,
								 POOL_CONNECTION_POOL * backend, bool send_ready, bool cache_commit);

extern POOL_STATUS ParseComplete(POOL_CONNECTION * frontend,
								 POOL_CONNECTION_POOL * backend);

extern POOL_STATUS BindComplete(POOL_CONNECTION * frontend,
								POOL_CONNECTION_POOL * backend);

extern POOL_STATUS CloseComplete(POOL_CONNECTION * frontend,
								 POOL_CONNECTION_POOL * backend);

extern POOL_STATUS ParameterDescription(POOL_CONNECTION * frontend,
										POOL_CONNECTION_POOL * backend);

extern POOL_STATUS ErrorResponse3(POOL_CONNECTION * frontend,
								  POOL_CONNECTION_POOL * backend);

extern POOL_STATUS CopyInResponse(POOL_CONNECTION * frontend,
								  POOL_CONNECTION_POOL * backend);

extern POOL_STATUS CopyOutResponse(POOL_CONNECTION * frontend,
								   POOL_CONNECTION_POOL * backend);

extern POOL_STATUS CopyDataRows(POOL_CONNECTION * frontend,
								POOL_CONNECTION_POOL * backend, int copyin);

extern POOL_STATUS FunctionCall(POOL_CONNECTION * frontend,
								POOL_CONNECTION_POOL * backend);

extern POOL_STATUS ProcessFrontendResponse(POOL_CONNECTION * frontend,
										   POOL_CONNECTION_POOL * backend);

extern POOL_STATUS ProcessBackendResponse(POOL_CONNECTION * frontend,
										  POOL_CONNECTION_POOL * backend,
										  int *state, short *num_fields);

extern void handle_query_context(POOL_CONNECTION_POOL * backend);;

extern void pool_emit_log_for_message_length_diff(int *length_array, char *name);

/*
 * modules defined in pool_proto2.c
 */
extern POOL_STATUS AsciiRow(POOL_CONNECTION * frontend,
							POOL_CONNECTION_POOL * backend,
							short num_fields);

extern POOL_STATUS BinaryRow(POOL_CONNECTION * frontend,
							 POOL_CONNECTION_POOL * backend,
							 short num_fields);

extern POOL_STATUS CompletedResponse(POOL_CONNECTION * frontend,
									 POOL_CONNECTION_POOL * backend);

extern POOL_STATUS CursorResponse(POOL_CONNECTION * frontend,
								  POOL_CONNECTION_POOL * backend);

extern void EmptyQueryResponse(POOL_CONNECTION * frontend,
				   POOL_CONNECTION_POOL * backend);

extern POOL_STATUS FunctionResultResponse(POOL_CONNECTION * frontend,
										  POOL_CONNECTION_POOL * backend);

extern POOL_STATUS NotificationResponse(POOL_CONNECTION * frontend,
										POOL_CONNECTION_POOL * backend);

extern int RowDescription(POOL_CONNECTION * frontend,
			   POOL_CONNECTION_POOL * backend,
			   short *result);

extern void wait_for_query_response_with_trans_cleanup(POOL_CONNECTION * frontend, POOL_CONNECTION * backend, int protoVersion, int pid, int key);
extern POOL_STATUS wait_for_query_response(POOL_CONNECTION * frontend, POOL_CONNECTION * backend, int protoVersion);
extern bool is_select_query(Node *node, char *sql);
extern bool is_commit_query(Node *node);
extern bool is_rollback_query(Node *node);
extern bool is_commit_or_rollback_query(Node *node);
extern bool is_rollback_to_query(Node *node);
extern bool is_strict_query(Node *node);	/* returns non 0 if this is strict
											 * query */
extern int	need_insert_lock(POOL_CONNECTION_POOL * backend, char *query, Node *node);
extern POOL_STATUS insert_lock(POOL_CONNECTION * frontend, POOL_CONNECTION_POOL * backend, char *query, InsertStmt *node, int lock_kind);
extern char *parse_copy_data(char *buf, int len, char delimiter, int col_id);
extern int	check_copy_from_stdin(Node *node);	/* returns non 0 if this is a
												 * COPY FROM STDIN */
extern void query_ps_status(char *query, POOL_CONNECTION_POOL * backend);	/* show ps status */
extern POOL_STATUS start_internal_transaction(POOL_CONNECTION * frontend, POOL_CONNECTION_POOL * backend, Node *node);
extern POOL_STATUS end_internal_transaction(POOL_CONNECTION * frontend, POOL_CONNECTION_POOL * backend);
extern int	detect_deadlock_error(POOL_CONNECTION * backend, int major);
extern int	detect_serialization_error(POOL_CONNECTION * backend, int major, bool unread);
extern int	detect_active_sql_transaction_error(POOL_CONNECTION * backend, int major);
extern int	detect_query_cancel_error(POOL_CONNECTION * backend, int major);
extern int	detect_idle_in_transaction_session_timeout_error(POOL_CONNECTION * backend, int major);
extern int	detect_idle_session_timeout_error(POOL_CONNECTION * backend, int major);
extern bool is_partition_table(POOL_CONNECTION_POOL * backend, Node *node);
extern POOL_STATUS pool_discard_packet(POOL_CONNECTION_POOL * cp);
extern void query_cache_register(char kind, POOL_CONNECTION * frontend, char *database, char *data, int data_len);
extern int	is_drop_database(Node *node);	/* returns non 0 if this is a DROP
											 * DATABASE command */

extern void send_simplequery_message(POOL_CONNECTION * backend, int len, char *string, int major);
extern POOL_STATUS send_extended_protocol_message(POOL_CONNECTION_POOL * backend,
												  int node_id, char *kind,
												  int len, char *string);

extern int	synchronize(POOL_CONNECTION * cp);
extern void read_kind_from_backend(POOL_CONNECTION * frontend, POOL_CONNECTION_POOL * backend, char *decided_kind);
extern void read_kind_from_one_backend(POOL_CONNECTION * frontend, POOL_CONNECTION_POOL * backend, char *kind, int node);
extern void do_error_command(POOL_CONNECTION * backend, int major);
extern void raise_intentional_error_if_need(POOL_CONNECTION_POOL * backend);

extern void pool_at_command_success(POOL_CONNECTION * frontend, POOL_CONNECTION_POOL * backend);

/*
 * modules defined in CommandComplete.c
 */
extern POOL_STATUS CommandComplete(POOL_CONNECTION * frontend, POOL_CONNECTION_POOL * backend, bool command_complete);

extern int	pool_read_message_length(POOL_CONNECTION_POOL * cp);
extern int *pool_read_message_length2(POOL_CONNECTION_POOL * cp);
extern signed char pool_read_kind(POOL_CONNECTION_POOL * cp);
extern int	pool_read_int(POOL_CONNECTION_POOL * cp);

/* pool_proto2.c */
extern POOL_STATUS ErrorResponse(POOL_CONNECTION * frontend,
								 POOL_CONNECTION_POOL * backend);

extern void NoticeResponse(POOL_CONNECTION * frontend,
			   POOL_CONNECTION_POOL * backend);

extern void per_node_error_log(POOL_CONNECTION_POOL * backend, int node_id,
char *query, char *prefix, bool unread);

#endif
