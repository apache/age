/* -*-pgsql-c-*- */
/*
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
 * pool.h.: pool definition header file
 *
 */

#ifndef POOL_H
#define POOL_H

#include "config.h"
#include "pool_type.h"
#include "pcp/libpcp_ext.h"
#include "auth/pool_passwd.h"
#include "utils/pool_params.h"
#include "parser/nodes.h"

#ifdef USE_SSL
#include <openssl/crypto.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
#endif

#include <syslog.h>

/* undef this if you have problems with non blocking accept() */
#define NONE_BLOCK

#define POOLMAXPATHLEN 8192

#define POOLKEYFILE 	".pgpoolkey"
#define POOLKEYFILEENV "PGPOOLKEYFILE"

/*
 * Brought from PostgreSQL's pg_config_manual.h.
 *
 * Maximum length for identifiers (e.g. table names, column names,
 * function names).  Names actually are limited to one less byte than this,
 * because the length must include a trailing zero byte.
 *
 * Please note that in version 2 protocol, maximum user name length is
 * SM_USER, which is 32.
 */
#define NAMEDATALEN 64

/* configuration file name */
#define POOL_CONF_FILE_NAME "pgpool.conf"

/* PCP user/password file name */
#define PCP_PASSWD_FILE_NAME "pcp.conf"

/* HBA configuration file name */
#define HBA_CONF_FILE_NAME "pool_hba.conf"

/* pid file directory */
#define DEFAULT_LOGDIR "/tmp"

/* Unix domain socket directory */
#define DEFAULT_SOCKET_DIR "/tmp"

/* Unix domain socket directory for watchdog IPC */
#define DEFAULT_WD_IPC_SOCKET_DIR "/tmp"

/* pid file name */
#define DEFAULT_PID_FILE_NAME "/var/run/pgpool/pgpool.pid"

/* status file name */
#define STATUS_FILE_NAME "pgpool_status"

/* query cache lock file name */
#define QUERY_CACHE_LOCK_FILE "memq_lock_file"

/* default string used to identify pgpool on syslog output */
#define DEFAULT_SYSLOG_IDENT "pgpool"

/* Pgpool node id file name */
#define NODE_ID_FILE_NAME "pgpool_node_id"

/* function return codes */
#define GENERAL_ERROR		(-1)
#define RETRY				(-2)
#define OPERATION_TIMEOUT	(-3)


typedef enum
{
	POOL_CONTINUE = 0,
	POOL_IDLE,
	POOL_END,
	POOL_ERROR,
	POOL_FATAL,
	POOL_DEADLOCK
}			POOL_STATUS;

typedef enum
{
	POOL_SOCKET_CLOSED = 0,
	POOL_SOCKET_VALID,
	POOL_SOCKET_ERROR,
	POOL_SOCKET_EOF
}			POOL_SOCKET_STATE;

/* protocol major version numbers */
#define PROTO_MAJOR_V2	2
#define PROTO_MAJOR_V3	3

/* Cancel packet proto major */
#define PROTO_CANCEL	80877102

/*
 * In protocol 3.0 and later, the startup packet length is not fixed, but
 * we set an arbitrary limit on it anyway.	This is just to prevent simple
 * denial-of-service attacks via sending enough data to run the server
 * out of memory.
 */
#define MAX_STARTUP_PACKET_LENGTH 10000


typedef struct StartupPacket_v2
{
	int			protoVersion;	/* Protocol version */
	char		database[SM_DATABASE];	/* Database name */
	char		user[SM_USER];	/* User name */
	char		options[SM_OPTIONS];	/* Optional additional args */
	char		unused[SM_UNUSED];	/* Unused */
	char		tty[SM_TTY];	/* Tty for debug output */
}			StartupPacket_v2;

/* startup packet info */
typedef struct
{
	char	   *startup_packet; /* raw startup packet without packet length
								 * (malloced area) */
	int			len;			/* raw startup packet length */
	int			major;			/* protocol major version */
	int			minor;			/* protocol minor version */
	char	   *database;		/* database name in startup_packet (malloced
								 * area) */
	char	   *user;			/* user name in startup_packet (malloced area) */
	char	   *application_name;	/* not malloced. pointing to in
									 * startup_packet */
} StartupPacket;

typedef struct CancelPacket
{
	int			protoVersion;	/* Protocol version */
	int			pid;			/* backend process id */
	int			key;			/* cancel key */
}			CancelPacket;

#define MAX_PASSWORD_SIZE		1024


/*
 * HbaLines is declared in pool_hba.h
 * we use forward declaration here
 */
typedef struct HbaLine HbaLine;


/*
 * stream connection structure
 */
typedef struct
{
	int			fd;				/* fd for connection */

	char	   *wbuf;			/* write buffer for the connection */
	int			wbufsz;			/* write buffer size */
	int			wbufpo;			/* buffer offset */

#ifdef USE_SSL
	SSL_CTX    *ssl_ctx;		/* SSL connection context */
	SSL		   *ssl;			/* SSL connection */
	X509	   *peer;
	char	   *cert_cn;		/* common in the ssl certificate presented by
								 * frontend connection Used for cert
								 * authentication */
	bool		client_cert_loaded;

#endif
	int			ssl_active;		/* SSL is failed if < 0, off if 0, on if > 0 */

	char	   *hp;				/* pending data buffer head address */
	int			po;				/* pending data offset */
	int			bufsz;			/* pending data buffer size */
	int			len;			/* pending data length */

	char	   *sbuf;			/* buffer for pool_read_string */
	int			sbufsz;			/* its size in bytes */

	char	   *buf2;			/* buffer for pool_read2 */
	int			bufsz2;			/* its size in bytes */

	char	   *buf3;			/* buffer for pool_push/pop */
	int			bufsz3;			/* its size in bytes */

	int			isbackend;		/* this connection is for backend if non 0 */
	int			db_node_id;		/* DB node id for this connection */

	char		tstate;			/* Transaction state (V3 only) 'I' if idle
								 * (not in a transaction block); 'T' if in a
								 * transaction block; or 'E' if in a failed
								 * transaction block */

	/* True if an internal transaction has already started */
	bool		is_internal_transaction_started;

	/*
	 * following are used to remember when re-use the authenticated connection
	 */
	int			auth_kind;		/* 3: clear text password, 4: crypt password,
								 * 5: md5 password */
	int			pwd_size;		/* password (sent back from frontend) size in
								 * host order */
	char		password[MAX_PASSWORD_SIZE + 1];	/* password (sent back
													 * from frontend) */
	char		salt[4];		/* password salt */
	PasswordType passwordType;

	/*
	 * following are used to remember current session parameter status.
	 * re-used connection will need them (V3 only)
	 */
	ParamStatus params;

	int			no_forward;		/* if non 0, do not write to frontend */

	char		kind;			/* kind cache */

	/* true if remote end closed the connection */
	POOL_SOCKET_STATE socket_state;

	/*
	 * frontend info needed for hba
	 */
	int			protoVersion;
	SockAddr	raddr;
	HbaLine    *pool_hba;
	char	   *database;
	char	   *username;
	char	   *remote_hostname;
	int			remote_hostname_resolv;
	bool		frontend_authenticated;
	PasswordMapping *passwordMapping;
	ConnectionInfo *con_info;	/* shared memory coninfo used for handling the
								 * query containing pg_terminate_backend */
}			POOL_CONNECTION;

/*
 * connection pool structure
 */
typedef struct
{
	StartupPacket *sp;			/* startup packet info */
	int			pid;			/* backend pid */
	int			key;			/* cancel key */
	POOL_CONNECTION *con;
	time_t		closetime;		/* absolute time in second when the connection
								 * closed if 0, that means the connection is
								 * under use. */
}			POOL_CONNECTION_POOL_SLOT;

typedef struct
{
	ConnectionInfo *info;		/* connection info on shmem */
	POOL_CONNECTION_POOL_SLOT *slots[MAX_NUM_BACKENDS];
}			POOL_CONNECTION_POOL;


/* Defined in pool_session_context.h */
extern int	pool_get_major_version(void);

/* NUM_BACKENDS now always returns actual number of backends */
#define NUM_BACKENDS (pool_config->backend_desc->num_backends)
#define BACKEND_INFO(backend_id) (pool_config->backend_desc->backend_info[(backend_id)])
#define LOAD_BALANCE_STATUS(backend_id) (pool_config->load_balance_status[(backend_id)])

/*
 * This macro returns true if:
 *   current query is in progress and the DB node is healthy OR
 *   no query is in progress and the DB node is healthy
 */
extern bool pool_is_node_to_be_sent_in_current_query(int node_id);
extern int	pool_virtual_main_db_node_id(void);

extern BACKEND_STATUS * my_backend_status[];
extern int	my_main_node_id;

#define VALID_BACKEND(backend_id) \
	((RAW_MODE && (backend_id) == REAL_MAIN_NODE_ID) ||		\
	(pool_is_node_to_be_sent_in_current_query((backend_id)) &&	\
	 ((*(my_backend_status[(backend_id)]) == CON_UP) ||			\
	  (*(my_backend_status[(backend_id)]) == CON_CONNECT_WAIT))))

/*
 * For raw mode failover control
 */
#define VALID_BACKEND_RAW(backend_id) \
	((*(my_backend_status[(backend_id)]) == CON_UP) ||			\
	 (*(my_backend_status[(backend_id)]) == CON_CONNECT_WAIT))

#define CONNECTION_SLOT(p, slot) ((p)->slots[(slot)])
#define CONNECTION(p, slot) (CONNECTION_SLOT(p, slot)->con)

/*
 * The first DB node id appears in pgpool.conf or the first "live" DB
 * node otherwise.
 */
#define REAL_MAIN_NODE_ID (Req_info->main_node_id)

/*
 * The primary node id in streaming replication mode. If not in the
 * mode or there's no primary node, this macro returns
 * REAL_MAIN_NODE_ID.
 */
#define PRIMARY_NODE_ID (Req_info->primary_node_id >=0 && VALID_BACKEND_RAW(Req_info->primary_node_id) ? \
						 Req_info->primary_node_id:REAL_MAIN_NODE_ID)
#define IS_PRIMARY_NODE_ID(node_id)	(node_id == PRIMARY_NODE_ID)

/*
 * Real primary node id. If not in the mode or there's no primary
 * node, this macro returns -1.
 */
#define REAL_PRIMARY_NODE_ID (Req_info->primary_node_id)

/*
 * "Virtual" main node id. It's same as REAL_MAIN_NODE_ID if not
 * in load balance mode. If in load balance, it's the first load
 * balance node.
 */
#define MAIN_NODE_ID (pool_virtual_main_db_node_id())
#define IS_MAIN_NODE_ID(node_id) (MAIN_NODE_ID == (node_id))
#define MAIN_CONNECTION(p) ((p)->slots[MAIN_NODE_ID])
#define MAIN(p) MAIN_CONNECTION(p)->con

/*
 * Backend node status in streaming replication mode.
 */
typedef enum
{
	POOL_NODE_STATUS_UNUSED,	/* unused */
	POOL_NODE_STATUS_PRIMARY,	/* primary ndoe */
	POOL_NODE_STATUS_STANDBY,	/* standby node */
	POOL_NODE_STATUS_INVALID	/* invalid node (split branin, stand alone) */
}			POOL_NODE_STATUS;

/* Clustering mode macros */
#define REPLICATION (pool_config->backend_clustering_mode == CM_NATIVE_REPLICATION || \
					 pool_config->backend_clustering_mode == CM_SNAPSHOT_ISOLATION)
#define MAIN_REPLICA (pool_config->backend_clustering_mode == CM_STREAMING_REPLICATION || \
					  pool_config->backend_clustering_mode == CM_LOGICAL_REPLICATION || \
					  pool_config->backend_clustering_mode == CM_SLONY)
#define STREAM (pool_config->backend_clustering_mode == CM_STREAMING_REPLICATION)
#define LOGICAL (pool_config->backend_clustering_mode == CM_LOGICAL_REPLICATION)
#define SLONY (pool_config->backend_clustering_mode == CM_SLONY)
#define DUAL_MODE (REPLICATION || NATIVE_REPLICATION)
#define RAW_MODE (pool_config->backend_clustering_mode == CM_RAW)
#define SL_MODE (STREAM || LOGICAL) /* streaming or logical replication mode */

#define MAJOR(p) (pool_get_major_version())
#define TSTATE(p, i) (CONNECTION(p, i)->tstate)
#define INTERNAL_TRANSACTION_STARTED(p, i) (CONNECTION(p, i)->is_internal_transaction_started)

#define Max(x, y)		((x) > (y) ? (x) : (y))
#define Min(x, y)		((x) < (y) ? (x) : (y))


#define MAX_NUM_SEMAPHORES		8
#define CONN_COUNTER_SEM		0
#define REQUEST_INFO_SEM		1
#define QUERY_CACHE_STATS_SEM	2
#define PCP_REQUEST_SEM			3
#define ACCEPT_FD_SEM			4
#define SI_CRITICAL_REGION_SEM	5
#define FOLLOW_PRIMARY_SEM		6
#define MAIN_EXIT_HANDLER_SEM	7	/* used in exit_hander in pgpool main process */
#define MAX_REQUEST_QUEUE_SIZE	10

#define MAX_SEC_WAIT_FOR_CLUSTER_TRANSACTION 10	/* time in seconds to keep
												 * retrying for a watchdog
												 * command if the cluster is
												 * not in stable state */
#define MAX_IDENTIFIER_LEN		128

#define SERIALIZE_ACCEPT (pool_config->serialize_accept == true && \
						  pool_config->child_life_time == 0)

/*
 * number specified when semaphore is locked/unlocked
 */
typedef enum SemNum
{
	SEMNUM_CONFIG,
	SEMNUM_NODES,
	SEMNUM_PROCESSES
}			SemNum;

/*
 * up/down request info area in shared memory
 */
typedef enum
{
	NODE_UP_REQUEST = 0,
	NODE_DOWN_REQUEST,
	NODE_RECOVERY_REQUEST,
	CLOSE_IDLE_REQUEST,
	PROMOTE_NODE_REQUEST,
	NODE_QUARANTINE_REQUEST
}			POOL_REQUEST_KIND;

#define REQ_DETAIL_SWITCHOVER	0x00000001	/* failover due to switch over */
#define REQ_DETAIL_WATCHDOG		0x00000002	/* failover req from watchdog */
#define REQ_DETAIL_CONFIRMED	0x00000004	/* failover req that does not
											 * require majority vote */
#define REQ_DETAIL_UPDATE		0x00000008	/* failover req is just an update
											 * node status request */
#define REQ_DETAIL_PROMOTE		0x00000010	/* failover req is actually promoting the specified standby node.
											 * current primary will be detached */

typedef struct
{
	POOL_REQUEST_KIND kind;		/* request kind */
	unsigned char request_details;	/* option flags kind */
	int			node_id[MAX_NUM_BACKENDS];	/* request node id */
	int			count;			/* request node ids count */
}			POOL_REQUEST_NODE;

typedef struct
{
	POOL_REQUEST_NODE request[MAX_REQUEST_QUEUE_SIZE];
	int			request_queue_head;
	int			request_queue_tail;
	int			main_node_id; /* the youngest node id which is not in down
								 * status */
	int			primary_node_id;	/* the primary node id in streaming
									 * replication mode */
	int			conn_counter;	/* number of connections from clients to pgpool */
	bool		switching;		/* it true, failover or failback is in
								 * progress */
	/* greater than 0 if follow primary command or detach_false_primary in
	 * execution */
	bool		follow_primary_count;
	bool		follow_primary_lock_pending; /* watchdog process can't wait
											  * for follow_primary lock acquisition
											  * in case it is held at the time of
											  * request.
											  * This flag indicates that lock was requested
											  * by watchdog coordinator and next contender should
											  * wait for the coordinator to release the lock
											  */
	bool		follow_primary_lock_held_remotely; /* true when lock is held by
													watchdog coordinator*/
	bool		follow_primary_ongoing;	/* true if follow primary command is ongoing */
}			POOL_REQUEST_INFO;

/* description of row. corresponding to RowDescription message */
typedef struct
{
	char	   *attrname;		/* attribute name */
	int			oid;			/* 0 or non 0 if it's a table object */
	int			attrnumber;		/* attribute number starting with 1. 0 if it's
								 * not a table */
	int			typeoid;		/* data type oid */
	int			size;			/* data length minus means variable data type */
	int			mod;			/* data type modifier */
}			AttrInfo;

typedef struct
{
	int			num_attrs;		/* number of attributes */
	AttrInfo   *attrinfo;
}			RowDesc;

typedef struct
{
	RowDesc    *rowdesc;		/* attribute info */
	int			numrows;		/* number of rows */
	int		   *nullflags;		/* if NULL, -1 or length of the string
								 * excluding termination null */
	char	  **data;			/* actual row character data terminated with
								 * null */
}			POOL_SELECT_RESULT;

/*
 * recovery mode
 */
typedef enum
{
	RECOVERY_INIT = 0,
	RECOVERY_ONLINE,
	RECOVERY_DETACH,
	RECOVERY_PROMOTE
}			POOL_RECOVERY_MODE;

/*
 * Process types.  DO NOT change the order of each enum meber!  If you do
 * that, you must change application_name array in src/main/pgpool_main.c
 * accordingly.
 */
typedef enum
{
	PT_MAIN = 0,
	PT_CHILD,
	PT_WORKER,
	PT_HB_SENDER,
	PT_HB_RECEIVER,
	PT_WATCHDOG,
	PT_LIFECHECK,
	PT_FOLLOWCHILD,
	PT_WATCHDOG_UTILITY,
	PT_PCP,
	PT_PCP_WORKER,
	PT_HEALTH_CHECK,
	PT_LOGGER,
	PT_LAST_PTYPE	/* last ptype marker. any ptype must be above this. */
}			ProcessType;


typedef enum
{
	INITIALIZING,
	PERFORMING_HEALTH_CHECK,
	SLEEPING,
	WAITING_FOR_CONNECTION,
	BACKEND_CONNECTING,
	PROCESSING,
	EXITING
}			ProcessState;

/*
 * Snapshot isolation manage area in shared memory
 */
typedef struct
{
	uint32		commit_counter;		/* number of committing children */
	uint32		snapshot_counter;	/* number of snapshot acquiring children */
	pid_t		*snapshot_waiting_children;		/* array size is num_init_children */
	pid_t		*commit_waiting_children;		/* array size is num_init_children */
} SI_ManageInfo;

/*
 * global variables
 * pool_global.c
 */
extern pid_t mypid;				/* parent pid */
extern pid_t myProcPid;			/* process pid */
extern ProcessType processType;
extern ProcessState processState;
extern void set_application_name(ProcessType ptype);
extern void set_application_name_with_string(char *string);
extern void set_application_name_with_suffix(ProcessType ptype, int suffix);
extern char *get_application_name(void);
extern char *get_application_name_for_process(ProcessType ptype);

void SetProcessGlobalVariables(ProcessType pType);

extern volatile SI_ManageInfo *si_manage_info;
extern volatile sig_atomic_t sigusr2_received;

extern volatile sig_atomic_t backend_timer_expired; /* flag for connection
													 * closed timer is expired */
extern volatile sig_atomic_t health_check_timer_expired;	/* non 0 if health check
															 * timer expired */
extern int	my_proc_id;			/* process table id (!= UNIX's PID) */
extern ProcessInfo * process_info;	/* shmem process information table */
extern ConnectionInfo * con_info;	/* shmem connection info table */
extern POOL_REQUEST_INFO * Req_info;
extern volatile sig_atomic_t *InRecovery;
extern volatile sig_atomic_t got_sighup;
extern volatile sig_atomic_t exit_request;
extern volatile sig_atomic_t ignore_sigusr1;

#define QUERY_STRING_BUFFER_LEN 1024
extern char query_string_buffer[];	/* last query string sent to simpleQuery() */

extern BACKEND_STATUS private_backend_status[MAX_NUM_BACKENDS];

extern char remote_host[];		/* client host */
extern char remote_port[];		/* client port */


/*
 * public functions
 */

/*main.c*/
extern char *get_config_file_name(void);
extern char *get_hba_file_name(void);
extern char *get_pool_key(void);


/*pcp_child.c*/
extern void pcp_main(int *fds);



/*child.c*/

extern void do_child(int *fds);
extern void child_exit(int code);

extern void cancel_request(CancelPacket * sp);
extern void check_stop_request(void);
extern void pool_initialize_private_backend_status(void);
extern int	send_to_pg_frontend(char *data, int len, bool flush);
extern int	pg_frontend_exists(void);
extern int	set_pg_frontend_blocking(bool blocking);
extern int	get_frontend_protocol_version(void);
extern void set_process_status(ProcessStatus status);

/*pool_shmem.c*/
extern void *pool_shared_memory_create(size_t size);
extern void pool_shmem_exit(int code);
extern void initialize_shared_memory_main_segment(size_t size);
extern void * pool_shared_memory_segment_get_chunk(size_t size);


/* pool_main.c*/
extern BackendInfo * pool_get_node_info(int node_number);
extern int	pool_get_node_count(void);
extern int *pool_get_process_list(int *array_size);
extern ProcessInfo * pool_get_process_info(pid_t pid);
extern void pool_sleep(unsigned int second);
extern int	PgpoolMain(bool discard_status, bool clear_memcache_oidmaps);
extern int	pool_send_to_frontend(char *data, int len, bool flush);
extern int	pool_frontend_exists(void);
extern pid_t pool_waitpid(int *status);
extern int	write_status_file(void);
extern POOL_NODE_STATUS * verify_backend_node_status(POOL_CONNECTION_POOL_SLOT * *slots);
extern POOL_NODE_STATUS * pool_get_node_status(void);
extern void pool_set_backend_status_changed_time(int backend_id);
extern int	get_next_main_node(void);
extern bool pool_acquire_follow_primary_lock(bool block, bool remote_reques);
extern void pool_release_follow_primary_lock(bool remote_reques);

/* strlcpy.c */
#ifndef HAVE_STRLCPY
extern size_t strlcpy(char *dst, const char *src, size_t siz);
#endif

/* pool_worker_child.c */
extern void do_worker_child(void);
extern int	get_query_result(POOL_CONNECTION_POOL_SLOT * *slots, int backend_id, char *query, POOL_SELECT_RESULT * *res);

#endif							/* POOL_H */
