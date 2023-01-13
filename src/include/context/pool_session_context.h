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
 * pool_process_context.h.: process context information
 *
 */

#ifndef POOL_SESSION_CONTEXT_H
#define POOL_SESSION_CONTEXT_H
#define INIT_LIST_SIZE 8

#include "pool.h"
#include "pool_process_context.h"
#include "pool_session_context.h"
#include "pool_query_context.h"
#include "query_cache/pool_memqcache.h"

/*
 * Transaction isolation mode
 */
typedef enum
{
	POOL_UNKNOWN,				/* Unknown. Need to ask backend */
	POOL_READ_UNCOMMITTED,		/* Read uncommitted */
	POOL_READ_COMMITTED,		/* Read committed */
	POOL_REPEATABLE_READ,		/* Repeatable read */
	POOL_SERIALIZABLE			/* Serializable */
}			POOL_TRANSACTION_ISOLATION;

/*
 * Return values for pool_use_sync_map
 */
typedef enum
{
	POOL_IGNORE_SYNC_MAP,		/* Please ignore sync map */
	POOL_SYNC_MAP_IS_VALID,		/* Sync map is valid */
	POOL_SYNC_MAP_EMPTY,		/* Sync map is empty */
}			POOL_SYNC_MAP_STATE;

/*
 * Status of sent message
 */
typedef enum
{
	POOL_SENT_MESSAGE_CREATED,	/* initial state of sent message */
	POOL_SENT_MESSAGE_CLOSED	/* sent message closed but close complete
								 * message has not arrived yet */
}			POOL_SENT_MESSAGE_STATE;

/*
 * Message content of extended query
 */
typedef struct
{
	/*
	 * One of 'P':Parse, 'B':Bind or 'Q':Query (PREPARE).  If kind = 'B', it
	 * is assumed that the message is a portal.
	 */
	char		kind;

	int			len;			/* message length in host byte order */
	char	   *contents;
	POOL_SENT_MESSAGE_STATE state;	/* message state */
	int			num_tsparams;
	char	   *name;			/* object name of prepared statement or portal */
	POOL_QUERY_CONTEXT *query_context;

	/*
	 * Following members are only used when memcache is enabled.
	 */
	bool		is_cache_safe;	/* true if the query can be cached */
	int			param_offset;	/* Offset from contents where actual bind
								 * parameters are stored. This is meaningful
								 * only when is_cache_safe is true. */
}			POOL_SENT_MESSAGE;

/*
 * List of POOL_SENT_MESSAGE (XXX this should have been implemented using a
 * list, rather than an array)
 */
typedef struct
{
	int			capacity;		/* capacity of list */
	int			size;			/* number of elements */
	POOL_SENT_MESSAGE **sent_messages;
}			POOL_SENT_MESSAGE_LIST;

/*
 * Received message queue used in extended query/streaming replication mode.
 * The queue is an FIFO.  When Parse/Bind/Describe/Execute/Close message are
 * received, each message is en-queued.  The information is used to process
 * those response messages, when Parse complete/Bind completes, Parameter
 * description, row description, command complete and close compete message
 * are received because they don't have any information regarding
 * statement/portal.
 *
 * The memory used for the queue lives in the session context mememory.
 */

typedef enum
{
	POOL_PARSE = 0,
	POOL_BIND,
	POOL_EXECUTE,
	POOL_DESCRIBE,
	POOL_CLOSE,
	POOL_SYNC
}			POOL_MESSAGE_TYPE;

typedef struct
{
	POOL_MESSAGE_TYPE type;
	char	   *contents;		/* message packet contents excluding message
								 * kind */
	int			contents_len;	/* message packet length */
	char		query[QUERY_STRING_BUFFER_LEN]; /* copy of original query */
	char		statement[MAX_IDENTIFIER_LEN];	/* prepared statement name if
												 * any */
	char		portal[MAX_IDENTIFIER_LEN]; /* portal name if any */
	bool		is_rows_returned;	/* true if the message could produce row
									 * data */
	bool		not_forward_to_frontend;	/* Do not forward response from
											 * backend to frontend. This is
											 * used by parse_before_bind() */
	bool		node_ids[MAX_NUM_BACKENDS];	/* backend node map which this message was sent to */
	POOL_QUERY_CONTEXT *query_context;	/* query context */
	/*
	 * If "flush" message arrives, this flag is set to true until all buffered
	 * message for frontend are sent out.
	 */
	bool		flush_pending;
}			POOL_PENDING_MESSAGE;

typedef enum {
	TEMP_TABLE_CREATING = 1,		/* temp table creating, not committed yet. */
	TEMP_TABLE_DROPPING,			/* temp table dropping, not committed yet. */
	TEMP_TABLE_CREATE_COMMITTED,		/* temp table created and committed. */
	TEMP_TABLE_DROP_COMMITTED,		/* temp table dropped and committed. */
}		POOL_TEMP_TABLE_STATE;

typedef struct {
	char		tablename[MAX_IDENTIFIER_LEN];	/* temporary table name */
	POOL_TEMP_TABLE_STATE	state;	/* see above */
}			POOL_TEMP_TABLE;


typedef enum
{
	SI_NO_SNAPSHOT,
	SI_SNAPSHOT_PREPARED
} SI_STATE;

/*
 * Per session context:
 */
typedef struct
{
	POOL_PROCESS_CONTEXT *process_context;	/* belonging process */
	POOL_CONNECTION *frontend;	/* connection to frontend */
	POOL_CONNECTION_POOL *backend;	/* connection to backends */

	/*
	 * If true, we are waiting for backend response.  For SELECT this flags
	 * should be kept until all responses are returned from backend. i.e.
	 * until "Read for Query" packet.
	 */
	bool		in_progress;

	/* If true, we are doing extended query message */
	bool		doing_extended_query_message;

	/*
	 * If true, we have rewritten where_to_send map in the current query
	 * context. pool_unset_query_in_progress() should restore the data from
	 * where_to_send_save.  For now, this is only necessary while doing
	 * extended query protocol and in streaming replication mode.
	 */
	bool		need_to_restore_where_to_send;
	bool		where_to_send_save[MAX_NUM_BACKENDS];

	/* If true, the command in progress has finished successfully. */
	bool		command_success;

	/* If true, write query has been appeared in this transaction */
	bool		writing_transaction;

	/* If true, error occurred in this transaction */
	bool		failed_transaction;

	/* If true, we skip reading from backends */
	bool		skip_reading_from_backends;

	/* ignore any command until Sync message */
	bool		ignore_till_sync;

	/*
	 * Transaction isolation mode.
	 */
	POOL_TRANSACTION_ISOLATION transaction_isolation;

	/*
	 * Associated query context, only used for non-extended protocol. In
	 * extended protocol, the query context resides in "PreparedStatementList
	 * *pstmt_list" (see below).
	 */
	POOL_QUERY_CONTEXT *query_context;
#ifdef NOT_USED
	/* where to send map for PREPARE/EXECUTE/DEALLOCATE */
	POOL_PREPARED_SEND_MAP prep_where;
#endif							/* NOT_USED */
	MemoryContext memory_context;	/* memory context for session */

	/* message which doesn't receive complete message */
	POOL_SENT_MESSAGE *uncompleted_message;

	POOL_SENT_MESSAGE_LIST message_list;

	int			load_balance_node_id;	/* selected load balance node id */

	/*
	 * If true, UPDATE/DELETE caused difference in number of affected tuples
	 * in backends.
	 */
	bool		mismatch_ntuples;

	/*
	 * If mismatch_ntuples true, this array holds the number of affected
	 * tuples of each node. -1 for down nodes.
	 */
	int			ntuples[MAX_NUM_BACKENDS];

	/*
	 * If true, we are executing reset query list.
	 */
	bool		reset_context;

	/*
	 * Query cache management area
	 */
	POOL_QUERY_CACHE_ARRAY *query_cache_array;	/* pending SELECT results */
	long long int num_selects;	/* number of successful SELECTs in this
								 * transaction */

	/*
	 * Parse/Bind/Describe/Execute/Close message queue.
	 */
	List	   *pending_messages;

	/*
	 * The last pending message. Reset at Ready for query.  Note that this is
	 * a shallow copy of pending message.  Once the are is reset,
	 * previous_message_exists is set to false.
	 */
	bool		previous_message_exists;
	POOL_PENDING_MESSAGE previous_message;

	/* Protocol major version number */
	int			major;
	/* Protocol minor version number */
	int			minor;

	/*
	 * Do not read messages from frontend. Used in extended protocol +
	 * streaming replication.  If sync message is received from frontend, this
	 * flag prevent from reading any message from frontend until read for
	 * query message arrives from backend.
	 */
	bool		suspend_reading_from_frontend;

	/*
	 * Temp tables list
	 */
	List	   *temp_tables;

	bool		is_in_transaction;

	/*
	 * Current transaction temp write list
	 */
	List	   *transaction_temp_write_list;

#ifdef NOT_USED
	/* Preferred "main" node id. Only used for SimpleForwardToFrontend. */
	int			preferred_main_node_id;
#endif

	/* Whether snapshot is acquired in this transaction. Only used by Snapshot Isolation mode. */
	SI_STATE	si_state;
	/* Whether transaction is read only. Only used by Snapshot Isolation mode. */
	SI_STATE	transaction_read_only;

	/*
	 * If true, the current message from backend must be flushed to frontend.
	 * Set by read_kind_from_backend and reset by SimpleForwardToFrontend.
	 */
	bool		flush_pending;
}			POOL_SESSION_CONTEXT;

extern void pool_init_session_context(POOL_CONNECTION * frontend, POOL_CONNECTION_POOL * backend);
extern void pool_session_context_destroy(void);
extern POOL_SESSION_CONTEXT * pool_get_session_context(bool noerror);
extern int	pool_get_local_session_id(void);
extern bool pool_is_query_in_progress(void);
extern void pool_set_query_in_progress(void);
extern void pool_unset_query_in_progress(void);
extern bool pool_is_skip_reading_from_backends(void);
extern void pool_set_skip_reading_from_backends(void);
extern void pool_unset_skip_reading_from_backends(void);
extern bool pool_is_doing_extended_query_message(void);
extern void pool_set_doing_extended_query_message(void);
extern void pool_unset_doing_extended_query_message(void);
extern bool pool_is_ignore_till_sync(void);
extern void pool_set_ignore_till_sync(void);
extern void pool_unset_ignore_till_sync(void);
extern POOL_SENT_MESSAGE * pool_create_sent_message(char kind, int len, char *contents,
													int num_tsparams, const char *name,
													POOL_QUERY_CONTEXT * query_context);
extern void pool_add_sent_message(POOL_SENT_MESSAGE * message);
extern bool pool_remove_sent_message(char kind, const char *name);
extern void pool_remove_sent_messages(char kind);
extern void pool_clear_sent_message_list(void);
extern void pool_sent_message_destroy(POOL_SENT_MESSAGE * message);
extern POOL_SENT_MESSAGE * pool_get_sent_message(char kind, const char *name, POOL_SENT_MESSAGE_STATE state);
extern void pool_set_sent_message_state(POOL_SENT_MESSAGE * message);
extern void pool_zap_query_context_in_sent_messages(POOL_QUERY_CONTEXT *query_context);
extern POOL_SENT_MESSAGE * pool_get_sent_message_by_query_context(POOL_QUERY_CONTEXT * query_context);
extern void pool_unset_writing_transaction(void);
extern void pool_set_writing_transaction(void);
extern bool pool_is_writing_transaction(void);
extern void pool_unset_failed_transaction(void);
extern void pool_set_failed_transaction(void);
extern bool pool_is_failed_transaction(void);
extern void pool_unset_transaction_isolation(void);
extern void pool_set_transaction_isolation(POOL_TRANSACTION_ISOLATION isolation_level);
extern POOL_TRANSACTION_ISOLATION pool_get_transaction_isolation(void);
extern void pool_unset_command_success(void);
extern void pool_set_command_success(void);
extern bool pool_is_command_success(void);
extern void pool_copy_prep_where(bool *src, bool *dest);
extern bool can_query_context_destroy(POOL_QUERY_CONTEXT * qc);
extern void pool_pending_messages_init(void);
extern void pool_pending_messages_destroy(void);
extern POOL_PENDING_MESSAGE * pool_pending_message_create(char kind, int len, char *contents);
extern void pool_pending_message_free_pending_message(POOL_PENDING_MESSAGE * message);
extern void pool_pending_message_dest_set(POOL_PENDING_MESSAGE * message, POOL_QUERY_CONTEXT * query_context);
extern void pool_pending_message_query_context_dest_set(POOL_PENDING_MESSAGE * message, POOL_QUERY_CONTEXT * query_context);
extern void pool_pending_message_query_set(POOL_PENDING_MESSAGE * message, POOL_QUERY_CONTEXT * query_context);
extern void pool_pending_message_add(POOL_PENDING_MESSAGE * message);
extern POOL_PENDING_MESSAGE * pool_pending_message_head_message(void);
extern POOL_PENDING_MESSAGE * pool_pending_message_pull_out(void);
extern POOL_PENDING_MESSAGE * pool_pending_message_get(POOL_MESSAGE_TYPE type);
extern char pool_get_close_message_spec(POOL_PENDING_MESSAGE * msg);
extern char *pool_get_close_message_name(POOL_PENDING_MESSAGE * msg);
extern void pool_pending_message_reset_previous_message(void);
extern void pool_pending_message_set_previous_message(POOL_PENDING_MESSAGE * message);
extern POOL_PENDING_MESSAGE * pool_pending_message_get_previous_message(void);
extern bool pool_pending_message_exists(void);
extern const char *pool_pending_message_type_to_string(POOL_MESSAGE_TYPE type);
extern void pool_check_pending_message_and_reply(POOL_MESSAGE_TYPE type, char kind);
extern POOL_PENDING_MESSAGE * pool_pending_message_find_lastest_by_query_context(POOL_QUERY_CONTEXT * qc);
extern int	pool_pending_message_get_target_backend_id(POOL_PENDING_MESSAGE * msg);
extern int	pool_pending_message_get_message_num_by_backend_id(int backend_id);
extern void pool_pending_message_set_flush_request(void);
extern void dump_pending_message(void);
extern void pool_set_major_version(int major);
extern void pool_set_minor_version(int minor);
extern int	pool_get_minor_version(void);
extern bool pool_is_suspend_reading_from_frontend(void);
extern void pool_set_suspend_reading_from_frontend(void);
extern void pool_unset_suspend_reading_from_frontend(void);

extern void pool_temp_tables_init(void);
extern void pool_temp_tables_destroy(void);
extern void	pool_temp_tables_add(char * tablename, POOL_TEMP_TABLE_STATE state);
extern POOL_TEMP_TABLE * pool_temp_tables_find(char * tablename);
extern void pool_temp_tables_delete(char * tablename, POOL_TEMP_TABLE_STATE state);
extern void	pool_temp_tables_commit_pending(void);
extern void	pool_temp_tables_remove_pending(void);
extern void	pool_temp_tables_dump(void);

#ifdef NOT_USED
extern void pool_set_preferred_main_node_id(int node_id);
extern int	pool_get_preferred_main_node_id(void);
extern void pool_reset_preferred_main_node_id(void);
#endif

#ifdef NOT_USED
extern void pool_add_prep_where(char *name, bool *map);
extern bool *pool_get_prep_where(char *name);
extern void pool_delete_prep_where(char *name);
#endif							/* NOT_USED */
#endif							/* POOL_SESSION_CONTEXT_H */
