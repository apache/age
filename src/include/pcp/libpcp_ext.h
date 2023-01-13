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
 *
 * libpcp_ext.h -
 *	  This file contains definitions for structures and
 *	  externs for functions used by frontend libpcp applications.
 */

#ifndef LIBPCP_EXT_H
#define LIBPCP_EXT_H

#include <signal.h>
#include <stdio.h>

/*
 * startup packet definitions (v2) stolen from PostgreSQL
 */
#define SM_DATABASE		64
#define SM_USER			32
#define SM_OPTIONS		64
#define SM_UNUSED		64
#define SM_TTY			64

/*
 * Maximum hostname length including domain name and "." including NULL
 * terminate.
 * https://en.wikipedia.org/wiki/Hostname#cite_note-Raymond,_Microsoft_devblog,_2012-3
 */
#define MAX_FDQN_HOSTNAME_LEN	254

#define MAX_NUM_BACKENDS 128
#define MAX_CONNECTION_SLOTS MAX_NUM_BACKENDS
#define MAX_DB_HOST_NAMELEN	 MAX_FDQN_HOSTNAME_LEN
#define MAX_PATH_LENGTH 256
#define NAMEDATALEN 64

typedef enum
{
	CON_UNUSED,					/* unused slot */
	CON_CONNECT_WAIT,			/* waiting for connection starting */
	CON_UP,						/* up and running */
	CON_DOWN					/* down, disconnected */
}			BACKEND_STATUS;

/* backend status name strings */
#define BACKEND_STATUS_CON_UNUSED		"unused"
#define BACKEND_STATUS_CON_CONNECT_WAIT	"waiting"
#define BACKEND_STATUS_CON_UP			"up"
#define BACKEND_STATUS_CON_DOWN			"down"
#define BACKEND_STATUS_QUARANTINE		"quarantine"

/*
 * Backend status record file
 */
typedef struct
{
	BACKEND_STATUS status[MAX_NUM_BACKENDS];
}			BackendStatusRecord;

typedef enum
{
	ROLE_MAIN,
	ROLE_REPLICA,
	ROLE_PRIMARY,
	ROLE_STANDBY
}			SERVER_ROLE;

/*
 * PostgreSQL backend descriptor. Placed on shared memory area.
 */
typedef struct
{
	char		backend_hostname[MAX_DB_HOST_NAMELEN];	/* backend host name */
	int			backend_port;	/* backend port numbers */
	BACKEND_STATUS backend_status;	/* backend status */
	char		pg_backend_status[NAMEDATALEN];	/* backend status examined by show pool_nodes and pcp_node_info*/
	time_t		status_changed_time;	/* backend status changed time */
	double		backend_weight; /* normalized backend load balance ratio */
	double		unnormalized_weight;	/* described parameter */
	char		backend_data_directory[MAX_PATH_LENGTH];
	char		backend_application_name[NAMEDATALEN];	/* application_name for walreceiver */
	unsigned short flag;		/* various flags */
	bool		quarantine;		/* true if node is CON_DOWN because of
								 * quarantine */
	uint64		standby_delay;	/* The replication delay against the primary */
	bool		standby_delay_by_time;	/* true if standby_delay is measured in microseconds, not bytes */
	SERVER_ROLE role;			/* Role of server. used by pcp_node_info and
								 * failover() to keep track of quarantined
								 * primary node */
	char		pg_role[NAMEDATALEN];	/* backend role examined by show pool_nodes and pcp_node_info*/
	char		replication_state [NAMEDATALEN];	/* "state" from pg_stat_replication */
	char		replication_sync_state [NAMEDATALEN];	/* "sync_state" from pg_stat_replication */
}			BackendInfo;

typedef struct
{
	sig_atomic_t num_backends;	/* Number of used PostgreSQL backends. This
								 * needs to be a sig_atomic_t type since it is
								 * replaced by a local variable while
								 * reloading pgpool.conf. */

	BackendInfo backend_info[MAX_NUM_BACKENDS];
}			BackendDesc;

typedef enum
{
	WAIT_FOR_CONNECT,
	COMMAND_EXECUTE,
	IDLE,
	IDLE_IN_TRANS,
	CONNECTING
}			ProcessStatus;

/*
 * Connection pool information. Placed on shared memory area.
 */
typedef struct
{
	int			backend_id;		/* backend id */
	char		database[SM_DATABASE];	/* Database name */
	char		user[SM_USER];	/* User name */
	int			major;			/* protocol major version */
	int			minor;			/* protocol minor version */
	int			pid;			/* backend process id */
	int			key;			/* cancel key */
	int			counter;		/* used counter */
	time_t		create_time;	/* connection creation time */
	time_t		client_connection_time;	/* client connection time */
	time_t		client_disconnection_time;	/* client last disconnection time */
	int			client_idle_duration;	/* client idle duration time (s) */
	int			load_balancing_node;	/* load balancing node */
	char		connected;		/* True if frontend connected. Please note
								 * that we use "char" instead of "bool". Since
								 * 3.1, we need to export this structure so
								 * that PostgreSQL C functions can use.
								 * Problem is, PostgreSQL defines bool itself,
								 * and if we use bool, the size of the
								 * structure might be out of control of
								 * pgpool-II. So we use "char" here. */
	volatile char swallow_termination;

	/*
	 * Flag to mark that if the connection will be terminated by the backend.
	 * it should not be treated as a backend node failure. This flag is used
	 * to handle pg_terminate_backend()
	 */
}			ConnectionInfo;

/*
 * process information
 * This object put on shared memory.
 */
typedef struct
{
	pid_t		pid;			/* OS's process id */
	time_t		start_time;		/* fork() time */
	char		connected;		/* if not 0 this process is already used*/
	int			wait_for_connect;	/* waiting time for client connection (s) */
	ConnectionInfo *connection_info;	/* head of the connection info for
										 * this process */
	int			client_connection_count;	/* how many times clients used this process */
	ProcessStatus	status;
	bool		need_to_restart;	/* If non 0, exit this child process as
									 * soon as current session ends. Typical
									 * case this flag being set is failback a
									 * node in streaming replication mode. */
	bool		exit_if_idle;
	int		pooled_connections; /* Total number of pooled connections
									  * by this child */
}			ProcessInfo;

/*
 * reporting types
 */
/* some length definitions */
#define POOLCONFIG_MAXIDLEN 4
#define POOLCONFIG_MAXNAMELEN 64
#define POOLCONFIG_MAXVALLEN 512
#define POOLCONFIG_MAXDESCLEN 80
#define POOLCONFIG_MAXIDENTLEN 63
#define POOLCONFIG_MAXPORTLEN 6
#define POOLCONFIG_MAXSTATLEN 12
#define POOLCONFIG_MAXWEIGHTLEN 20
#define POOLCONFIG_MAXDATELEN 128
#define POOLCONFIG_MAXCOUNTLEN 16
#define POOLCONFIG_MAXLONGCOUNTLEN 20
#define POOLCONFIG_MAXPROCESSSTATUSLEN 20
/* config report struct*/
typedef struct
{
	char		name[POOLCONFIG_MAXNAMELEN + 1];
	char		value[POOLCONFIG_MAXVALLEN + 1];
	char		desc[POOLCONFIG_MAXDESCLEN + 1];
}			POOL_REPORT_CONFIG;

/* nodes report struct */
typedef struct
{
	char		node_id[POOLCONFIG_MAXIDLEN + 1];
	char		hostname[MAX_DB_HOST_NAMELEN + 1];
	char		port[POOLCONFIG_MAXPORTLEN + 1];
	char		status[POOLCONFIG_MAXSTATLEN + 1];
	char		pg_status[POOLCONFIG_MAXSTATLEN + 1];
	char		lb_weight[POOLCONFIG_MAXWEIGHTLEN + 1];
	char		role[POOLCONFIG_MAXWEIGHTLEN + 1];
	char		pg_role[POOLCONFIG_MAXWEIGHTLEN + 1];
	char		select[POOLCONFIG_MAXWEIGHTLEN + 1];
	char		load_balance_node[POOLCONFIG_MAXWEIGHTLEN + 1];
	char		delay[POOLCONFIG_MAXWEIGHTLEN + 1];
	char		rep_state[POOLCONFIG_MAXWEIGHTLEN + 1];
	char		rep_sync_state[POOLCONFIG_MAXWEIGHTLEN + 1];
	char		last_status_change[POOLCONFIG_MAXDATELEN];
}			POOL_REPORT_NODES;

/* processes report struct */
typedef struct
{
	char		pool_pid[POOLCONFIG_MAXCOUNTLEN + 1];
	char		process_start_time[POOLCONFIG_MAXDATELEN + 1];
	char		client_connection_count[POOLCONFIG_MAXCOUNTLEN + 1];
	char		database[POOLCONFIG_MAXIDENTLEN + 1];
	char		username[POOLCONFIG_MAXIDENTLEN + 1];
	char		backend_connection_time[POOLCONFIG_MAXDATELEN + 1];
	char		pool_counter[POOLCONFIG_MAXCOUNTLEN + 1];
	char		status[POOLCONFIG_MAXPROCESSSTATUSLEN + 1];
}			POOL_REPORT_PROCESSES;

/* pools reporting struct */
typedef struct
{
	char		pool_pid[POOLCONFIG_MAXCOUNTLEN + 1];
	char		process_start_time[POOLCONFIG_MAXDATELEN + 1];
	char		client_connection_count[POOLCONFIG_MAXCOUNTLEN + 1];
	char		pool_id[POOLCONFIG_MAXCOUNTLEN + 1];
	char		backend_id[POOLCONFIG_MAXCOUNTLEN + 1];
	char		database[POOLCONFIG_MAXIDENTLEN + 1];
	char		username[POOLCONFIG_MAXIDENTLEN + 1];
	char		backend_connection_time[POOLCONFIG_MAXDATELEN + 1];
	char		client_connection_time[POOLCONFIG_MAXDATELEN + 1];
	char		client_disconnection_time[POOLCONFIG_MAXDATELEN + 1];
	char		client_idle_duration[POOLCONFIG_MAXDATELEN + 1];
	char		pool_majorversion[POOLCONFIG_MAXCOUNTLEN + 1];
	char		pool_minorversion[POOLCONFIG_MAXCOUNTLEN + 1];
	char		pool_counter[POOLCONFIG_MAXCOUNTLEN + 1];
	char		pool_backendpid[POOLCONFIG_MAXCOUNTLEN + 1];
	char		pool_connected[POOLCONFIG_MAXCOUNTLEN + 1];
	char		status[POOLCONFIG_MAXPROCESSSTATUSLEN + 1];
}			POOL_REPORT_POOLS;

/* version struct */
typedef struct
{
	char		version[POOLCONFIG_MAXVALLEN + 1];
}			POOL_REPORT_VERSION;

/* health check statistics report struct */
typedef struct
{
	char		node_id[POOLCONFIG_MAXIDLEN + 1];
	char		hostname[MAX_DB_HOST_NAMELEN + 1];
	char		port[POOLCONFIG_MAXPORTLEN + 1];
	char		status[POOLCONFIG_MAXSTATLEN + 1];
	char		role[POOLCONFIG_MAXWEIGHTLEN + 1];
	char		last_status_change[POOLCONFIG_MAXDATELEN];
	char		total_count[POOLCONFIG_MAXLONGCOUNTLEN+1];
	char		success_count[POOLCONFIG_MAXLONGCOUNTLEN+1];
	char		fail_count[POOLCONFIG_MAXLONGCOUNTLEN+1];
	char		skip_count[POOLCONFIG_MAXLONGCOUNTLEN+1];
	char		retry_count[POOLCONFIG_MAXLONGCOUNTLEN+1];
	char		average_retry_count[POOLCONFIG_MAXLONGCOUNTLEN+1];
	char		max_retry_count[POOLCONFIG_MAXCOUNTLEN+1];
	char		max_health_check_duration[POOLCONFIG_MAXCOUNTLEN+1];
	char		min_health_check_duration[POOLCONFIG_MAXCOUNTLEN+1];
	char		average_health_check_duration[POOLCONFIG_MAXLONGCOUNTLEN+1];
	char		last_health_check[POOLCONFIG_MAXDATELEN];
	char		last_successful_health_check[POOLCONFIG_MAXDATELEN];
	char		last_skip_health_check[POOLCONFIG_MAXDATELEN];
	char		last_failed_health_check[POOLCONFIG_MAXDATELEN];
}			POOL_HEALTH_CHECK_STATS;

/* show backend statistics report struct */
typedef struct
{
	char		node_id[POOLCONFIG_MAXIDLEN + 1];
	char		hostname[MAX_DB_HOST_NAMELEN + 1];
	char		port[POOLCONFIG_MAXPORTLEN + 1];
	char		status[POOLCONFIG_MAXSTATLEN + 1];
	char		role[POOLCONFIG_MAXWEIGHTLEN + 1];
	char		select_cnt[POOLCONFIG_MAXWEIGHTLEN + 1];
	char		insert_cnt[POOLCONFIG_MAXWEIGHTLEN + 1];
	char		update_cnt[POOLCONFIG_MAXWEIGHTLEN + 1];
	char		delete_cnt[POOLCONFIG_MAXWEIGHTLEN + 1];
	char		ddl_cnt[POOLCONFIG_MAXWEIGHTLEN + 1];
	char		other_cnt[POOLCONFIG_MAXWEIGHTLEN + 1];	
	char		panic_cnt[POOLCONFIG_MAXWEIGHTLEN + 1];	
	char		fatal_cnt[POOLCONFIG_MAXWEIGHTLEN + 1];	
	char		error_cnt[POOLCONFIG_MAXWEIGHTLEN + 1];	
}			POOL_BACKEND_STATS;

typedef enum
{
	PCP_CONNECTION_OK,
	PCP_CONNECTION_CONNECTED,
	PCP_CONNECTION_NOT_CONNECTED,
	PCP_CONNECTION_BAD,
	PCP_CONNECTION_AUTH_ERROR
}			ConnStateType;

typedef enum
{
	PCP_RES_COMMAND_OK,
	PCP_RES_BAD_RESPONSE,
	PCP_RES_BACKEND_ERROR,
	PCP_RES_INCOMPLETE,
	PCP_RES_ERROR
}			ResultStateType;

struct PCPConnInfo;

typedef struct
{
	int			isint;			/* 1 if data in slot is integer, 0 otherwise */
	int			datalen;		/* Length of binary data */
	union
	{
		int		   *ptr;
		int			integer;
	}			data;
	void		(*free_func) (struct PCPConnInfo *, void *);	/* custom free function
																 * deep free of data */
}			PCPResultSlot;

typedef struct
{
	ResultStateType resultStatus;
	int			resultSlots;	/* Total number of slots contained in this
								 * result */
	int			nextFillSlot;	/* internal to keep track of last filled slot */
	PCPResultSlot resultSlot[1];	/* variable length slots */
}			PCPResultInfo;

typedef struct PCPConnInfo
{
	void	   *pcpConn;
	char	   *errMsg;			/* error message, or NULL if no error */
	ConnStateType connState;
	PCPResultInfo *pcpResInfo;
	FILE	   *Pfdebug;		/* File pointer to output debug info */
}			PCPConnInfo;

struct WdInfo;

extern PCPConnInfo * pcp_connect(char *hostname, int port, char *username, char *password, FILE *Pfdebug);
extern void pcp_disconnect(PCPConnInfo * pcpConn);

extern PCPResultInfo * pcp_terminate_pgpool(PCPConnInfo * pcpConn, char mode, char command_scope);
extern PCPResultInfo * pcp_node_count(PCPConnInfo * pcpCon);
extern PCPResultInfo * pcp_node_info(PCPConnInfo * pcpCon, int nid);
extern PCPResultInfo * pcp_health_check_stats(PCPConnInfo * pcpCon, int nid);
extern PCPResultInfo * pcp_process_count(PCPConnInfo * pcpConn);
extern PCPResultInfo * pcp_process_info(PCPConnInfo * pcpConn, int pid);
extern PCPResultInfo * pcp_reload_config(PCPConnInfo * pcpConn,char command_scope);

extern PCPResultInfo * pcp_detach_node(PCPConnInfo * pcpConn, int nid);
extern PCPResultInfo * pcp_detach_node_gracefully(PCPConnInfo * pcpConn, int nid);
extern PCPResultInfo * pcp_attach_node(PCPConnInfo * pcpConn, int nid);
extern PCPResultInfo * pcp_pool_status(PCPConnInfo * pcpConn);
extern PCPResultInfo * pcp_recovery_node(PCPConnInfo * pcpConn, int nid);
extern PCPResultInfo * pcp_promote_node(PCPConnInfo * pcpConn, int nid, bool promote);
extern PCPResultInfo * pcp_promote_node_gracefully(PCPConnInfo * pcpConn, int nid, bool promote);
extern PCPResultInfo * pcp_watchdog_info(PCPConnInfo * pcpConn, int nid);
extern PCPResultInfo * pcp_set_backend_parameter(PCPConnInfo * pcpConn, char *parameter_name, char *value);


extern ResultStateType PCPResultStatus(const PCPResultInfo * res);
extern ConnStateType PCPConnectionStatus(const PCPConnInfo * conn);
extern void *pcp_get_binary_data(const PCPResultInfo * res, unsigned int slotno);
extern int	pcp_get_int_data(const PCPResultInfo * res, unsigned int slotno);
extern int	pcp_get_data_length(const PCPResultInfo * res, unsigned int slotno);
extern void pcp_free_result(PCPConnInfo * pcpConn);
extern void pcp_free_connection(PCPConnInfo * pcpConn);
extern int	pcp_result_slot_count(PCPResultInfo * res);
extern char *pcp_get_last_error(PCPConnInfo * pcpConn);

extern int	pcp_result_is_empty(PCPResultInfo * res);

extern char *role_to_str(SERVER_ROLE role);

extern	int * pool_health_check_stats_offsets(int *n);
extern	int * pool_report_pools_offsets(int *n);

/* ------------------------------
 * pcp_error.c
 * ------------------------------
 */

#endif							/* LIBPCP_EXT_H */
