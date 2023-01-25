/* -*-pgsql-c-*- */
/*
 *
 * $Header$
 *
 * pgpool: a language independent connection pool server for PostgreSQL
 * written by Tatsuo Ishii
 *
 * Copyright (c) 2003-2019	PgPool Global Development Group
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
 * watchdog.h.: watchdog definition header file
 *
 */

#ifndef WATCHDOG_H
#define WATCHDOG_H

#include <sys/time.h>
#include "pool_config.h"

#define WD_TIME_INIT(tv)      ((tv).tv_sec = (tv).tv_usec = 0)
#define WD_TIME_ISSET(tv)     ((tv).tv_sec || (tv).tv_usec)
#define WD_TIME_BEFORE(a,b)   (((a).tv_sec == (b).tv_sec) ? \
                               ((a).tv_usec < (b).tv_usec) : \
                               ((a).tv_sec < (b).tv_sec))
#define WD_TIME_DIFF_SEC(a,b) (int)(((a).tv_sec - (b).tv_sec) + \
                                    ((a).tv_usec - (b).tv_usec) / 1000000.0)

/*
 * Data version number of watchdog messages
 * The version number is in major.minor format
 * The major versions are always kept compatible.
 *
 * Increment the minor version whenever a minor change is
 * made to message data, where the older versions can still
 * work even when that change is not present in it.
 *
 * while incrementing the major version would mean that
 * the watchdog node with older major version will not be
 * allowed to join the cluster
 *
 * Since the message data version was not present from the
 * beginning, so the default version is considered to be 1.0
 * meaning if the data version number is not present in the
 * watchdog node info then it will be considered as version 1.0
 */

#define WD_MESSAGE_DATA_VERSION_MAJOR	"1"
#define WD_MESSAGE_DATA_VERSION_MINOR	"2"
#define WD_MESSAGE_DATA_VERSION	WD_MESSAGE_DATA_VERSION_MAJOR "." WD_MESSAGE_DATA_VERSION_MINOR
#define MAX_VERSION_STR_LEN		10

/*
 * watchdog state
 * If you add a new state. Remember to add entry in
 * wd_state_names (watchdog.c)
 */
typedef enum
{
	WD_DEAD = 0,
	WD_LOADING,
	WD_JOINING,
	WD_INITIALIZING,
	WD_COORDINATOR,
	WD_PARTICIPATE_IN_ELECTION,
	WD_STAND_FOR_COORDINATOR,
	WD_STANDBY,
	WD_LOST,
	/* the following states are only valid on local node */
	WD_IN_NW_TROUBLE,
	/* the following states are only valid on remote nodes */
	WD_SHUTDOWN,
	WD_ADD_MESSAGE_SENT,
	WD_NETWORK_ISOLATION
}			WD_STATES;

typedef enum
{
	WD_SOCK_UNINITIALIZED = 0,
	WD_SOCK_CREATED,
	WD_SOCK_WAITING_FOR_CONNECT,
	WD_SOCK_CONNECTED,
	WD_SOCK_ERROR,
	WD_SOCK_CLOSED
}			WD_SOCK_STATE;

typedef enum
{
	WD_EVENT_WD_STATE_CHANGED = 0,
	WD_EVENT_TIMEOUT,
	WD_EVENT_PACKET_RCV,
	WD_EVENT_COMMAND_FINISHED,
	WD_EVENT_NEW_OUTBOUND_CONNECTION,

	WD_EVENT_NW_IP_IS_REMOVED,
	WD_EVENT_NW_IP_IS_ASSIGNED,

	WD_EVENT_NW_LINK_IS_INACTIVE,
	WD_EVENT_NW_LINK_IS_ACTIVE,

	WD_EVENT_LOCAL_NODE_LOST,
	WD_EVENT_REMOTE_NODE_LOST,
	WD_EVENT_REMOTE_NODE_FOUND,
	WD_EVENT_LOCAL_NODE_FOUND,

	WD_EVENT_NODE_CON_LOST,
	WD_EVENT_NODE_CON_FOUND,
	WD_EVENT_CLUSTER_QUORUM_CHANGED,
	WD_EVENT_WD_STATE_REQUIRE_RELOAD,
	WD_EVENT_I_AM_APPEARING_LOST,
	WD_EVENT_I_AM_APPEARING_FOUND
}			WD_EVENTS;

/*
 * If you add a new lost reason. Remember to add entry in
 * wd_node_lost_reasons (watchdog.c)
 */
typedef enum {
	NODE_LOST_UNKNOWN_REASON = 0,
	NODE_LOST_BY_LIFECHECK,
	NODE_LOST_BY_SEND_FAILURE,
	NODE_LOST_BY_MISSING_BEACON,
	NODE_LOST_BY_RECEIVE_TIMEOUT,
	NODE_LOST_BY_NOT_REACHABLE,
	NODE_LOST_SHUTDOWN
} WD_NODE_LOST_REASONS;

/*
 * If you add a new membership status. Remember to add entry in
 * wd_cluster_membership_status (watchdog.c)
 */

typedef enum {
	WD_NODE_MEMBERSHIP_ACTIVE,
	WD_NODE_REVOKED_SHUTDOWN,
	WD_NODE_REVOKED_NO_SHOW,
	WD_NODE_REVOKED_LOST
}WD_NODE_MEMBERSHIP_STATUS;

typedef struct SocketConnection
{
	int			sock;			/* socket descriptor */
	struct timeval tv;			/* connect time of socket */
	char		addr[48];		/* ip address of socket connection */
	WD_SOCK_STATE sock_state;	/* current state of socket */
}			SocketConnection;

typedef struct WatchdogNode
{
	WD_STATES	state;
	struct timeval current_state_time;	/* time value when the node state last
										 * changed */
	struct timeval startup_time;	/* startup time value of node */
	struct timeval last_rcv_time;	/* timestamp when last packet was received
									 * from the node */
	struct timeval last_sent_time;	/* timestamp when last packet was sent on
									 * the node */
	struct timeval lost_time;	/* timestamp when the remote node was lost on coordinator
								 */
	WD_NODE_MEMBERSHIP_STATUS	membership_status; /* status of node membership
											*in watchdog cluster
											Only valid for remote nodes */
	bool   has_lost_us;             /*
									 * True when this remote node thinks
									 * we are lost
									 */
	int    sending_failures_count;  /* number of times we have failed
									 * to send message to the node.
									 * Gets reset after successful sent
									 */
	int    missed_beacon_count;     /* number of times the node has
									 * failed to reply for beacon.
									 * message
									 */
	WD_NODE_LOST_REASONS node_lost_reason;

	char		pgp_version[MAX_VERSION_STR_LEN];		/* Pgpool-II version */
	int			wd_data_major_version;	/* watchdog messaging version major*/
	int			wd_data_minor_version;  /* watchdog messaging version minor*/

	char		nodeName[WD_MAX_NODE_NAMELEN];	/* name of this node */
	char		hostname[WD_MAX_HOST_NAMELEN];	/* host name */
	int			wd_port;		/* watchdog port */
	int			pgpool_port;	/* pgpool port */
	int			wd_priority;	/* watchdog priority */
	char		delegate_ip[WD_MAX_HOST_NAMELEN];	/* delegate IP */
	int			pgpool_node_id;		/* pgpool node id specified in pgpool_node_id file */
	int			standby_nodes_count;	/* number of standby nodes joined the
										 * cluster only applicable when this
										 * WatchdogNode is the
										 * leader/coordinator node */
	int			quorum_status;	/* quorum status on the node */
	bool		escalated;		/* true if the Watchdog node has performed
								 * escalation */
	SocketConnection server_socket; /* socket connections for this node
									 * initiated by remote */
	SocketConnection client_socket; /* socket connections for this node
									 * initiated by local */
}			WatchdogNode;

/*
 * Argument for WD Exec cluster command
 */
#define WD_MAX_ARG_NAME_LEN		64
#define WD_MAX_ARG_VALUE_LEN	64

typedef struct WDExecCommandArg
{
	char arg_name[WD_MAX_ARG_NAME_LEN];
	char arg_value[WD_MAX_ARG_VALUE_LEN];
}			WDExecCommandArg;

extern pid_t initialize_watchdog(void);

#endif							/* WATCHDOG_H */
