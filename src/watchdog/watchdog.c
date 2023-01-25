/* -*-pgsql-c-*- */
/*
 * $Header$
 *
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
 * watchdog.c: child process main
 *
 */



#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/time.h>
#include <sys/utsname.h>
#include <sys/un.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <net/if.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <fcntl.h>
#include <ctype.h>

#include "pool.h"
#include "pool_config.h"
#include "auth/md5.h"
#include "utils/palloc.h"
#include "utils/memutils.h"
#include "utils/elog.h"
#include "utils/json_writer.h"
#include "utils/json.h"
#include "utils/socket_stream.h"
#include "utils/pool_signal.h"
#include "utils/ps_status.h"
#include "main/pool_internal_comms.h"
#include "pcp/recovery.h"

#include "watchdog/wd_utils.h"
#include "watchdog/watchdog.h"
#include "watchdog/wd_json_data.h"
#include "watchdog/wd_ipc_defines.h"
#include "watchdog/wd_internal_commands.h"
#include "parser/stringinfo.h"

/* These defines enables the consensus building feature
 * in watchdog for node failover operations
 * We can also take these to the configure script
 */
#define NODE_UP_REQUIRE_CONSENSUS
#define NODE_DOWN_REQUIRE_CONSENSUS
#define NODE_PROMOTE_REQUIRE_CONSENSUS

typedef enum IPC_CMD_PROCESS_RES
{
	IPC_CMD_COMPLETE,
	IPC_CMD_PROCESSING,
	IPC_CMD_ERROR,
	IPC_CMD_OK,
	IPC_CMD_TRY_AGAIN
}			IPC_CMD_PROCESS_RES;


#define MIN_SECS_CONNECTION_RETRY	10	/* Time in seconds to retry connection
										 * with node once it was failed */

#define MAX_SECS_ESC_PROC_EXIT_WAIT 5	/* maximum amount of seconds to wait
										 * for escalation/de-escalation process
										 * to exit normally before moving on */

#define BEACON_MESSAGE_INTERVAL_SECONDS		10	/* interval between beacon
												 * messages */

#define	MAX_SECS_WAIT_FOR_REPLY_FROM_NODE	5	/* time in seconds to wait for
												 * the reply from remote
												 * watchdog node */

#define MAX_ALLOWED_SEND_FAILURES           3	/* number of times sending message failure
                                                  * can be tolerated
                                                  */
#define MAX_ALLOWED_BEACON_REPLY_MISS       3	/* number of times missing beacon message reply
                                                  * can be tolerated
                                                  */


#define	FAILOVER_COMMAND_FINISH_TIMEOUT		15	/* timeout in seconds to wait
												 * for Pgpool-II to build
												 * consensus for failover */

#define MIN_SECS_BETWEEN_BROADCAST_SRV_MSG 5	/* minimum amount of seconds to wait
												 * before broadcasting the same cluster
												 * service message */

/*
 * Packet types. Used in WDPacketData->type.
 */
#define WD_NO_MESSAGE						0
#define WD_ADD_NODE_MESSAGE					'A'
#define WD_REQ_INFO_MESSAGE					'B'
#define WD_DECLARE_COORDINATOR_MESSAGE		'C'
#define WD_DATA_MESSAGE						'D'
#define WD_ERROR_MESSAGE					'E'
#define WD_ACCEPT_MESSAGE					'G'
#define WD_INFO_MESSAGE						'I'
#define WD_JOIN_COORDINATOR_MESSAGE			'J'
#define WD_IAM_COORDINATOR_MESSAGE			'M'
#define WD_IAM_IN_NW_TROUBLE_MESSAGE		'N'
#define WD_QUORUM_IS_LOST					'Q'
#define WD_REJECT_MESSAGE					'R'
#define WD_STAND_FOR_COORDINATOR_MESSAGE	'S'
#define WD_REMOTE_FAILOVER_REQUEST			'V'
#define WD_INFORM_I_AM_GOING_DOWN			'X'
#define WD_ASK_FOR_POOL_CONFIG				'Y'
#define WD_POOL_CONFIG_DATA					'Z'
#define WD_CMD_REPLY_IN_DATA				'-'
#define WD_CLUSTER_SERVICE_MESSAGE			'#'

#define WD_EXECUTE_COMMAND_REQUEST			'!'

#define WD_FAILOVER_START					'F'
#define WD_FAILOVER_END						'H'
#define WD_FAILOVER_WAITING_FOR_CONSENSUS	'K'

/*Cluster Service Message Types */
#define CLUSTER_QUORUM_LOST					'L'
#define CLUSTER_QUORUM_FOUND				'F'
#define CLUSTER_IN_SPLIT_BRAIN				'B'
#define CLUSTER_NEEDS_ELECTION				'E'
#define CLUSTER_IAM_TRUE_LEADER				'M'
#define CLUSTER_IAM_NOT_TRUE_LEADER			'X'
#define CLUSTER_IAM_RESIGNING_FROM_LEADER	'R'
#define CLUSTER_NODE_INVALID_VERSION		'V'
#define CLUSTER_NODE_REQUIRE_TO_RELOAD		'I'
#define CLUSTER_NODE_APPEARING_LOST 		'Y'
#define CLUSTER_NODE_APPEARING_FOUND 		'Z'


#define WD_LEADER_NODE getLeaderWatchdogNode()

typedef struct packet_types
{
	char		type;
	char		name[100];
}			packet_types;

packet_types all_packet_types[] = {
	{WD_ADD_NODE_MESSAGE, "ADD NODE"},
	{WD_REQ_INFO_MESSAGE, "REQUEST INFO"},
	{WD_DECLARE_COORDINATOR_MESSAGE, "DECLARE COORDINATOR"},
	{WD_DATA_MESSAGE, "DATA"},
	{WD_ERROR_MESSAGE, "ERROR"},
	{WD_ACCEPT_MESSAGE, "ACCEPT"},
	{WD_INFO_MESSAGE, "NODE INFO"},
	{WD_JOIN_COORDINATOR_MESSAGE, "JOIN COORDINATOR"},
	{WD_IAM_COORDINATOR_MESSAGE, "IAM COORDINATOR"},
	{WD_IAM_IN_NW_TROUBLE_MESSAGE, "I AM IN NETWORK TROUBLE"},
	{WD_QUORUM_IS_LOST, "QUORUM IS LOST"},
	{WD_REJECT_MESSAGE, "REJECT"},
	{WD_STAND_FOR_COORDINATOR_MESSAGE, "STAND FOR COORDINATOR"},
	{WD_REMOTE_FAILOVER_REQUEST, "REPLICATE FAILOVER REQUEST"},
	{WD_IPC_ONLINE_RECOVERY_COMMAND, "ONLINE RECOVERY REQUEST"},
	{WD_EXECUTE_CLUSTER_COMMAND, "EXECUTE CLUSTER COMMAND"},
	{WD_IPC_FAILOVER_COMMAND, "FAILOVER FUNCTION COMMAND"},
	{WD_INFORM_I_AM_GOING_DOWN, "INFORM I AM GOING DOWN"},
	{WD_ASK_FOR_POOL_CONFIG, "ASK FOR POOL CONFIG"},
	{WD_POOL_CONFIG_DATA, "CONFIG DATA"},
	{WD_GET_LEADER_DATA_REQUEST, "DATA REQUEST FOR LEADER"},
	{WD_GET_RUNTIME_VARIABLE_VALUE, "GET WD RUNTIME VARIABLE VALUE"},
	{WD_CMD_REPLY_IN_DATA, "COMMAND REPLY IN DATA"},
	{WD_FAILOVER_LOCKING_REQUEST, "FAILOVER LOCKING REQUEST"},
	{WD_FAILOVER_INDICATION, "FAILOVER INDICATION"},
	{WD_CLUSTER_SERVICE_MESSAGE, "CLUSTER SERVICE MESSAGE"},
	{WD_REGISTER_FOR_NOTIFICATION, "REGISTER FOR NOTIFICATION"},
	{WD_NODE_STATUS_CHANGE_COMMAND, "NODE STATUS CHANGE"},
	{WD_GET_NODES_LIST_COMMAND, "GET NODES LIST"},
	{WD_IPC_CMD_CLUSTER_IN_TRAN, "CLUSTER STATE NOT STABLE"},
	{WD_IPC_CMD_RESULT_BAD, "IPC RESPONSE BAD"},
	{WD_IPC_CMD_RESULT_OK, "IPC RESPONSE GOOD"},
	{WD_IPC_CMD_TIMEOUT, "IPC TIMEOUT"},
	{WD_EXECUTE_COMMAND_REQUEST, "WD EXECUTE COMMAND"},
	{WD_NO_MESSAGE, ""}
};


char	   *wd_event_name[] = {
	"STATE CHANGED",
	"TIMEOUT",
	"PACKET RECEIVED",
	"COMMAND FINISHED",
	"NEW OUTBOUND_CONNECTION",
	"NETWORK IP IS REMOVED",
	"NETWORK IP IS ASSIGNED",
	"NETWORK LINK IS INACTIVE",
	"NETWORK LINK IS ACTIVE",
	"THIS NODE LOST",
	"REMOTE NODE LOST",
	"REMOTE NODE FOUND",
	"THIS NODE FOUND",
	"NODE CONNECTION LOST",
	"NODE CONNECTION FOUND",
	"CLUSTER QUORUM STATUS CHANGED",
	"NODE REQUIRE TO RELOAD STATE",
	"REMOTE NODE LOST US",
	"REMOTE NODE FOUND US"
};

char	   *wd_state_names[] = {
	"DEAD",
	"LOADING",
	"JOINING",
	"INITIALIZING",
	"LEADER",
	"PARTICIPATING IN ELECTION",
	"STANDING FOR LEADER",
	"STANDBY",
	"LOST",
	"IN NETWORK TROUBLE",
	"SHUTDOWN",
	"ADD MESSAGE SENT",
	"NETWORK ISOLATION"
};

char *wd_node_lost_reasons[] = {
	"UNKNOWN REASON",
	"REPORTED BY LIFECHECK",
	"SEND MESSAGE FAILURES",
	"MISSING BEACON REPLIES",
	"RECEIVE TIMEOUT",
	"NOT REACHABLE",
	"SHUTDOWN"
};

char *wd_cluster_membership_status[] = {
	"MEMBER",
	"REVOKED-SHUTDOWN",
	"REVOKED-NO-SHOW",
	"REVOKED-LOST"
};
/*
 * Command packet definition.
 */
typedef struct WDPacketData
{
	char		type;	/* packet type. e.g. WD_ADD_NODE_MESSAGE. See #define above. */
	int			command_id;	/* command sequence number starting from 1 */
	int			len;
	char	   *data;
}			WDPacketData;


typedef enum WDNodeCommandState
{
	COMMAND_STATE_INIT,
	COMMAND_STATE_SENT,
	COMMAND_STATE_REPLIED,
	COMMAND_STATE_SEND_ERROR,
	COMMAND_STATE_DO_NOT_SEND
}			WDNodeCommandState;

typedef struct WDCommandNodeResult
{
	WatchdogNode *wdNode;
	WDNodeCommandState cmdState;
	char		result_type;
	int			result_data_len;
	char	   *result_data;
}			WDCommandNodeResult;

typedef enum WDCommandSource
{
	COMMAND_SOURCE_IPC,
	COMMAND_SOURCE_LOCAL,
	COMMAND_SOURCE_REMOTE,
	COMMAND_SOURCE_INTERNAL
}			WDCommandSource;

/*
 * Watchdog "function" descriptor.  "function" is not a C-function, it's one
 * of: START_RECOVERY, END_RECOVERY, FAILBACK_REQUEST, DEGENERATE_REQUEST and
 * PROMOTE_REQUEST. See #define function names (they are prefixed by
 * "WD_FUNCTION" in src/include/watchdog/wd_ipc_defines.h for more details.
 */
typedef struct WDFunctionCommandData
{
	char		commandType;
	unsigned int commandID;
	char	   *funcName;	/* function name */
	WatchdogNode *wdNode;
}			WDFunctionCommandData;

typedef struct WDCommandTimerData
{
	struct timeval startTime;
	unsigned int expire_sec;
	bool		need_tics;
	WDFunctionCommandData *wd_func_command;
}			WDCommandTimerData;


typedef enum WDCommandStatus
{
	COMMAND_EMPTY,
	COMMAND_IN_PROGRESS,
	COMMAND_FINISHED_TIMEOUT,
	COMMAND_FINISHED_ALL_REPLIED,
	COMMAND_FINISHED_NODE_REJECTED,
	COMMAND_FINISHED_SEND_FAILED
}			WDCommandStatus;

typedef struct WDCommandData
{
	WDPacketData sourcePacket;
	WDPacketData commandPacket;
	WDCommandNodeResult *nodeResults;
	WatchdogNode *sendToNode;	/* NULL means send to all */
	WDCommandStatus commandStatus;
	unsigned int commandTimeoutSecs;
	struct timeval commandTime;
	unsigned int commandSendToCount;
	unsigned int commandSendToErrorCount;
	unsigned int commandReplyFromCount;
	WDCommandSource commandSource;
	int			sourceIPCSocket;	/* Only valid for COMMAND_SOURCE_IPC */
	WatchdogNode *sourceWdNode; /* Only valid for COMMAND_SOURCE_REMOTE */
	char	   *errorMessage;
	MemoryContext memoryContext;
	void		(*commandCompleteFunc) (struct WDCommandData *command);
}			WDCommandData;

typedef struct WDInterfaceStatus
{
	char	   *if_name;
	unsigned int if_index;
	bool		if_up;
}			WDInterfaceStatus;

typedef struct WDClusterLeader
{
	WatchdogNode *leaderNode;
	WatchdogNode **standbyNodes;
	int			standby_nodes_count;
	bool		holding_vip;
}			WDClusterLeaderInfo;

typedef struct wd_cluster
{
	WatchdogNode *localNode;
	WatchdogNode *remoteNodes;
	WDClusterLeaderInfo clusterLeaderInfo;
	int			remoteNodeCount;
	int			memberRemoteNodeCount; /* no of nodes that count towards quorum and consensus */
	int			quorum_status;
	unsigned int nextCommandID;
	pid_t		escalation_pid;
	pid_t		de_escalation_pid;
	int			command_server_sock;
	int			network_monitor_sock;
	bool		clusterInitialized;
	bool		ipc_auth_needed;
	int 		current_failover_id;
	int 		failover_command_timeout;
	struct timeval last_bcast_srv_msg_time;	/* timestamp when last packet was
											 *  broadcasted by the local node */
	char		last_bcast_srv_msg;

	List	   *unidentified_socks;
	List	   *notify_clients;
	List	   *ipc_command_socks;
	List	   *ipc_commands;
	List	   *clusterCommands;
	List	   *wd_timer_commands;
	List	   *wdInterfaceToMonitor;
	List	   *wdCurrentFailovers;
}			wd_cluster;

typedef struct WDFailoverObject
{
	int			id;
	POOL_REQUEST_KIND reqKind;
	unsigned char reqFlags;
	int			nodesCount;
	unsigned int failoverID;
	int		   *nodeList;
	List	   *requestingNodes;
	int			request_count;
	struct timeval startTime;
	int			state;
}			WDFailoverObject;

#ifdef WATCHDOG_DEBUG_OPTS
#if WATCHDOG_DEBUG_OPTS > 0
#define WATCHDOG_DEBUG
#endif
#endif

static bool check_debug_request_do_not_send_beacon(void);
static bool check_debug_request_do_not_reply_beacon(void);
static bool check_debug_request_kill_all_communication(void);
static bool check_debug_request_kill_all_receivers(void);
static bool check_debug_request_kill_all_senders(void);


#ifdef WATCHDOG_DEBUG
static void load_watchdog_debug_test_option(void);
#endif

static void process_remote_failover_command_on_coordinator(WatchdogNode * wdNode, WDPacketData * pkt);
static WDFailoverObject * get_failover_object(POOL_REQUEST_KIND reqKind, int nodesCount, int *nodeList);
static bool does_int_array_contains_value(int *intArray, int count, int value);
static void clear_all_failovers(void);
static void remove_failover_object(WDFailoverObject * failoverObj);
static void service_expired_failovers(void);
static WDFailoverObject * add_failover(POOL_REQUEST_KIND reqKind, int *node_id_list, int node_count, WatchdogNode * wdNode,
									   unsigned char flags, bool *duplicate);
static WDFailoverCMDResults compute_failover_consensus(POOL_REQUEST_KIND reqKind, int *node_id_list,
													   int node_count, unsigned char *flags, WatchdogNode * wdNode);

static int	send_command_packet_to_remote_nodes(WDCommandData * ipcCommand, bool source_included);
static void wd_command_is_complete(WDCommandData * ipcCommand);
static IPC_CMD_PROCESS_RES wd_command_processor_for_node_lost_event(WDCommandData * ipcCommand, WatchdogNode * wdLostNode);

volatile sig_atomic_t reload_config_signal = 0;
volatile sig_atomic_t sigchld_request = 0;

static void check_signals(void);
static void wd_child_signal_handler(void);
static RETSIGTYPE watchdog_signal_handler(int sig);
static void FileUnlink(int code, Datum path);
static void wd_child_exit(int exit_signo);

static void wd_cluster_initialize(void);
static void wd_initialize_monitoring_interfaces(void);
static int	wd_create_client_socket(char *hostname, int port, bool *connected);
static int	connect_with_all_configured_nodes(void);
static void try_connecting_with_all_unreachable_nodes(void);
static bool connect_to_node(WatchdogNode * wdNode);
static bool is_socket_connection_connected(SocketConnection * conn);

static void service_unreachable_nodes(void);

static void allocate_resultNodes_in_command(WDCommandData * ipcCommand);
static bool is_node_active_and_reachable(WatchdogNode * wdNode);
static bool is_node_active(WatchdogNode * wdNode);
static bool is_node_reachable(WatchdogNode * wdNode);

static int	update_successful_outgoing_cons(fd_set *wmask, int pending_fds_count);
static int	prepare_fds(fd_set *rmask, fd_set *wmask, fd_set *emask);

static void set_next_commandID_in_message(WDPacketData * pkt);
static void set_message_commandID(WDPacketData * pkt, unsigned int commandID);
static void set_message_data(WDPacketData * pkt, const char *data, int len);
static void set_message_type(WDPacketData * pkt, char type);
static void free_packet(WDPacketData * pkt);

static WDPacketData * get_empty_packet(void);
static WDPacketData * read_packet_of_type(SocketConnection * conn, char ensure_type);
static WDPacketData * read_packet(SocketConnection * conn);
static WDPacketData * get_message_of_type(char type, WDPacketData * replyFor);
static WDPacketData * get_addnode_message(void);
static WDPacketData * get_beacon_message(char type, WDPacketData * replyFor);
static WDPacketData * get_mynode_info_message(WDPacketData * replyFor);
static WDPacketData * get_minimum_message(char type, WDPacketData * replyFor);


static int	issue_watchdog_internal_command(WatchdogNode * wdNode, WDPacketData * pkt, int timeout_sec);
static void check_for_current_command_timeout(void);
static bool watchdog_internal_command_packet_processor(WatchdogNode * wdNode, WDPacketData * pkt);
static bool service_lost_connections(void);
static void service_ipc_commands(void);
static void service_internal_command(void);

static unsigned int get_next_commandID(void);
static WatchdogNode * parse_node_info_message(WDPacketData * pkt, char **authkey);
static void update_quorum_status(void);
static int	get_minimum_remote_nodes_required_for_quorum(void);
static int	get_minimum_votes_to_resolve_consensus(void);

static bool write_packet_to_socket(int sock, WDPacketData * pkt, bool ipcPacket);
static int	read_sockets(fd_set *rmask, int pending_fds_count);
static void set_timeout(unsigned int sec);
static int	wd_create_command_server_socket(void);
static void close_socket_connection(SocketConnection * conn);
static bool send_message_to_connection(SocketConnection * conn, WDPacketData * pkt);

static int	send_message(WatchdogNode * wdNode, WDPacketData * pkt);
static bool send_message_to_node(WatchdogNode * wdNode, WDPacketData * pkt);
static bool reply_with_minimal_message(WatchdogNode * wdNode, char type, WDPacketData * replyFor);
static bool reply_with_message(WatchdogNode * wdNode, char type, char *data, int data_len, WDPacketData * replyFor);
static int	send_cluster_command(WatchdogNode * wdNode, char type, int timeout_sec);
static int	send_message_of_type(WatchdogNode * wdNode, char type, WDPacketData * replyFor);

static bool send_cluster_service_message(WatchdogNode * wdNode, WDPacketData * replyFor, char message);


static int	accept_incoming_connections(fd_set *rmask, int pending_fds_count);

static int	standard_packet_processor(WatchdogNode * wdNode, WDPacketData * pkt);
static void cluster_service_message_processor(WatchdogNode * wdNode, WDPacketData * pkt);
static int	get_cluster_node_count(void);
static void clear_command_node_result(WDCommandNodeResult * nodeResult);

static inline bool is_local_node_true_leader(void);
static inline WD_STATES get_local_node_state(void);
static int	set_state(WD_STATES newState);

static int	watchdog_state_machine_standby(WD_EVENTS event, WatchdogNode * wdNode, WDPacketData * pkt, WDCommandData * clusterCommand);
static int	watchdog_state_machine_voting(WD_EVENTS event, WatchdogNode * wdNode, WDPacketData * pkt, WDCommandData * clusterCommand);
static int	watchdog_state_machine_coordinator(WD_EVENTS event, WatchdogNode * wdNode, WDPacketData * pkt, WDCommandData * clusterCommand);
static int	watchdog_state_machine_standForCord(WD_EVENTS event, WatchdogNode * wdNode, WDPacketData * pkt, WDCommandData * clusterCommand);
static int	watchdog_state_machine_initializing(WD_EVENTS event, WatchdogNode * wdNode, WDPacketData * pkt, WDCommandData * clusterCommand);
static int	watchdog_state_machine_joining(WD_EVENTS event, WatchdogNode * wdNode, WDPacketData * pkt, WDCommandData * clusterCommand);
static int	watchdog_state_machine_loading(WD_EVENTS event, WatchdogNode * wdNode, WDPacketData * pkt, WDCommandData * clusterCommand);
static int	watchdog_state_machine(WD_EVENTS event, WatchdogNode * wdNode, WDPacketData * pkt, WDCommandData * clusterCommand);
static int	watchdog_state_machine_nw_error(WD_EVENTS event, WatchdogNode * wdNode, WDPacketData * pkt, WDCommandData * clusterCommand);
static int watchdog_state_machine_nw_isolation(WD_EVENTS event, WatchdogNode * wdNode, WDPacketData * pkt, WDCommandData * clusterCommand);

static int	I_am_leader_and_cluster_in_split_brain(WatchdogNode * otherLeaderNode);
static void handle_split_brain(WatchdogNode * otherLeaderNode, WDPacketData * pkt);
static bool beacon_message_received_from_node(WatchdogNode * wdNode, WDPacketData * pkt);

static void cleanUpIPCCommand(WDCommandData * ipcCommand);
static bool read_ipc_socket_and_process(int socket, bool *remove_socket);

static JsonNode * get_node_list_json(int id);
static bool add_nodeinfo_to_json(JsonNode * jNode, WatchdogNode * node);
static bool fire_node_status_event(int nodeID, int nodeStatus);
static void resign_from_escalated_node(void);
static void start_escalated_node(void);
static void init_wd_packet(WDPacketData * pkt);
static void wd_packet_shallow_copy(WDPacketData * srcPkt, WDPacketData * dstPkt);
static bool wd_commands_packet_processor(WD_EVENTS event, WatchdogNode * wdNode, WDPacketData * pkt);

static WDCommandData * get_wd_command_from_reply(List *commands, WDPacketData * pkt);
static WDCommandData * get_wd_cluster_command_from_reply(WDPacketData * pkt);
static WDCommandData * get_wd_IPC_command_from_reply(WDPacketData * pkt);
static WDCommandData * get_wd_IPC_command_from_socket(int sock);

static IPC_CMD_PROCESS_RES process_IPC_command(WDCommandData * ipcCommand);
static IPC_CMD_PROCESS_RES process_IPC_nodeStatusChange_command(WDCommandData * ipcCommand);
static IPC_CMD_PROCESS_RES process_IPC_nodeList_command(WDCommandData * ipcCommand);
static IPC_CMD_PROCESS_RES process_IPC_get_runtime_variable_value_request(WDCommandData * ipcCommand);
static IPC_CMD_PROCESS_RES process_IPC_online_recovery(WDCommandData * ipcCommand);
static IPC_CMD_PROCESS_RES process_IPC_failover_indication(WDCommandData * ipcCommand);
static IPC_CMD_PROCESS_RES process_IPC_data_request_from_leader(WDCommandData * ipcCommand);
static IPC_CMD_PROCESS_RES process_IPC_failover_command(WDCommandData * ipcCommand);
static IPC_CMD_PROCESS_RES process_failover_command_on_coordinator(WDCommandData * ipcCommand);
static IPC_CMD_PROCESS_RES process_IPC_execute_cluster_command(WDCommandData * ipcCommand);

static bool write_ipc_command_with_result_data(WDCommandData * ipcCommand, char type, char *data, int len);

static void process_wd_func_commands_for_timer_events(void);
static void add_wd_command_for_timer_events(unsigned int expire_secs, bool need_tics, WDFunctionCommandData * wd_func_command);
static bool reply_is_received_for_pgpool_replicate_command(WatchdogNode * wdNode, WDPacketData * pkt, WDCommandData * ipcCommand);

static void process_remote_online_recovery_command(WatchdogNode * wdNode, WDPacketData * pkt);

static WDFailoverCMDResults failover_end_indication(WDCommandData * ipcCommand);
static WDFailoverCMDResults failover_start_indication(WDCommandData * ipcCommand);

static void wd_system_will_go_down(int code, Datum arg);
static void verify_pool_configurations(WatchdogNode * wdNode, POOL_CONFIG * config);

static bool get_authhash_for_node(WatchdogNode * wdNode, char *authhash);
static bool verify_authhash_for_node(WatchdogNode * wdNode, char *authhash);

static void print_watchdog_node_info(WatchdogNode * wdNode);
static int	wd_create_recv_socket(int port);
static void wd_check_config(void);
static pid_t watchdog_main(void);
static pid_t fork_watchdog_child(void);
static bool check_IPC_client_authentication(json_value * rootObj, bool internal_client_only);
static bool check_and_report_IPC_authentication(WDCommandData * ipcCommand);

static void print_packet_node_info(WDPacketData * pkt, WatchdogNode * wdNode, bool sending);
static void print_packet_info(WDPacketData * pkt, bool sending);
static void update_interface_status(void);
static bool any_interface_available(void);
static WDPacketData * process_data_request(WatchdogNode * wdNode, WDPacketData * pkt);

static WatchdogNode * getLeaderWatchdogNode(void);
static void set_cluster_leader_node(WatchdogNode * wdNode);
static void clear_standby_nodes_list(void);
static int	standby_node_left_cluster(WatchdogNode * wdNode);
static int	standby_node_join_cluster(WatchdogNode * wdNode);
static void reset_lost_timers(void);
static int update_cluster_memberships(void);
static int revoke_cluster_membership_of_node(WatchdogNode* wdNode, WD_NODE_MEMBERSHIP_STATUS revoke_status);
static int restore_cluster_membership_of_node(WatchdogNode* wdNode);
static void update_missed_beacon_count(WDCommandData* ipcCommand, bool clear);
static void wd_execute_cluster_command_processor(WatchdogNode * wdNode, WDPacketData * pkt);
static void	update_failover_timeout(WatchdogNode * wdNode, POOL_CONFIG *pool_config);
/* global variables */
wd_cluster	g_cluster;
struct timeval g_tm_set_time;
int			g_timeout_sec = 0;

static unsigned int
get_next_commandID(void)
{
	return ++g_cluster.nextCommandID;
}

static void
set_timeout(unsigned int sec)
{
	g_timeout_sec = sec;
	gettimeofday(&g_tm_set_time, NULL);
}

pid_t
initialize_watchdog(void)
{
	if (!pool_config->use_watchdog)
		return -1;
	/* check pool_config data related to watchdog */
	wd_check_config();
	return fork_watchdog_child();
}

static void
wd_check_config(void)
{
	if (pool_config->wd_nodes.num_wd == 0)
		ereport(ERROR,
				(errmsg("invalid watchdog configuration. no watchdog nodes configured")));

	if (strlen(pool_config->wd_authkey) > MAX_PASSWORD_SIZE)
		ereport(ERROR,
				(errmsg("invalid watchdog configuration. wd_authkey length can't be larger than %d",
						MAX_PASSWORD_SIZE)));
	if (pool_config->wd_lifecheck_method == LIFECHECK_BY_HB)
	{
		if (pool_config->num_hb_dest_if <= 0)
			ereport(ERROR,
					(errmsg("invalid life-check configuration. no heartbeat interfaces defined")));
	}
}

static void
wd_initialize_monitoring_interfaces(void)
{
	g_cluster.wdInterfaceToMonitor = NULL;

	if (pool_config->num_wd_monitoring_interfaces_list <= 0)
	{
		ereport(LOG,
				(errmsg("interface monitoring is disabled in watchdog")));
		return;
	}

	if (strcasecmp("any", pool_config->wd_monitoring_interfaces_list[0]) == 0)
	{
		struct if_nameindex *if_ni,
				   *idx;

		ereport(LOG,
				(errmsg("ensure availability on any interface")));

		if_ni = if_nameindex();
		if (if_ni == NULL)
		{
			ereport(ERROR,
					(errmsg("initializing watchdog failed. unable to get network interface information")));
		}

		for (idx = if_ni; !(idx->if_index == 0 && idx->if_name == NULL); idx++)
		{
			WDInterfaceStatus *if_status;

			ereport(DEBUG1,
					(errmsg("interface name %s at index %d", idx->if_name, idx->if_index)));
			if (strncasecmp("lo", idx->if_name, 2) == 0)
			{
				/* ignoring local interface */
				continue;
			}
			if_status = palloc(sizeof(WDInterfaceStatus));
			if_status->if_name = pstrdup(idx->if_name);
			if_status->if_index = idx->if_index;
			if_status->if_up = true;	/* start with optimism */
			g_cluster.wdInterfaceToMonitor = lappend(g_cluster.wdInterfaceToMonitor, if_status);
		}
		if_freenameindex(if_ni);
	}
	else
	{
		WDInterfaceStatus *if_status;
		char	   *if_name;
		int			i;
		unsigned int if_idx;

		for (i = 0; i < pool_config->num_wd_monitoring_interfaces_list; i++)
		{
			if_name = pool_config->wd_monitoring_interfaces_list[i];
			/* ignore leading spaces */
			while (*if_name && isspace(*if_name))
				if_name++;

			if_idx = if_nametoindex(if_name);
			if (if_idx == 0)
				ereport(ERROR,
						(errmsg("initializing watchdog failed. invalid interface name \"%s\"", pool_config->wd_monitoring_interfaces_list[0])));

			ereport(DEBUG1,
					(errmsg("adding monitoring interface [%d] name %s index %d", i, if_name, if_idx)));

			if_status = palloc(sizeof(WDInterfaceStatus));
			if_status->if_name = pstrdup(if_name);
			if_status->if_index = if_idx;
			if_status->if_up = true;	/* start with optimism */
			g_cluster.wdInterfaceToMonitor = lappend(g_cluster.wdInterfaceToMonitor, if_status);
		}
	}
}

static void
wd_cluster_initialize(void)
{
	int			i = 0;
	int			pgpool_node_id = pool_config->pgpool_node_id;

	if (pool_config->wd_nodes.num_wd <= 0)
	{
		/* should also have upper limit??? */
		ereport(ERROR,
				(errmsg("initializing watchdog failed. no watchdog nodes configured")));
	}

	/* initialize local node settings */
	g_cluster.localNode = palloc0(sizeof(WatchdogNode));
	g_cluster.localNode->wd_port = pool_config->wd_nodes.wd_node_info[pgpool_node_id].wd_port;
	g_cluster.localNode->pgpool_port = pool_config->wd_nodes.wd_node_info[pgpool_node_id].pgpool_port;
	g_cluster.localNode->wd_priority = pool_config->wd_priority;
	g_cluster.localNode->pgpool_node_id = pool_config->pgpool_node_id;
	gettimeofday(&g_cluster.localNode->startup_time, NULL);

	strncpy(g_cluster.localNode->hostname, pool_config->wd_nodes.wd_node_info[pgpool_node_id].hostname, sizeof(g_cluster.localNode->hostname) - 1);
	strncpy(g_cluster.localNode->delegate_ip, pool_config->delegate_ip, sizeof(g_cluster.localNode->delegate_ip) - 1);
	/* Assign the node name */
	{
		struct utsname unameData;

		uname(&unameData);
		snprintf(g_cluster.localNode->nodeName, sizeof(g_cluster.localNode->nodeName), "%s:%d %s %s",
				 pool_config->wd_nodes.wd_node_info[pgpool_node_id].hostname,
				 pool_config->wd_nodes.wd_node_info[pgpool_node_id].pgpool_port,
				 unameData.sysname,
				 unameData.nodename);
		/* should also have upper limit??? */
		ereport(LOG,
				(errmsg("setting the local watchdog node name to \"%s\"", g_cluster.localNode->nodeName)));
	}

	/* initialize remote nodes */
	g_cluster.remoteNodeCount = pool_config->wd_nodes.num_wd - 1;
	g_cluster.memberRemoteNodeCount = g_cluster.remoteNodeCount;
	if (g_cluster.remoteNodeCount == 0)
		ereport(ERROR,
                (errmsg("invalid watchdog configuration. other pgpools setting is not defined")));
	ereport(LOG,
			(errmsg("watchdog cluster is configured with %d remote nodes", g_cluster.remoteNodeCount)));
	g_cluster.remoteNodes = palloc0((sizeof(WatchdogNode) * g_cluster.remoteNodeCount));
	int idx = 0;
	for (i = 0; i < pool_config->wd_nodes.num_wd; i++)
	{
		if (i == pool_config->pgpool_node_id)
			continue;

		g_cluster.remoteNodes[idx].wd_port = pool_config->wd_nodes.wd_node_info[i].wd_port;
		g_cluster.remoteNodes[idx].pgpool_node_id = i;
		g_cluster.remoteNodes[idx].pgpool_port = pool_config->wd_nodes.wd_node_info[i].pgpool_port;
		strcpy(g_cluster.remoteNodes[idx].hostname, pool_config->wd_nodes.wd_node_info[i].hostname);
		g_cluster.remoteNodes[idx].delegate_ip[0] = '\0'; /* this will be
														 * populated by remote
														 * node */

		ereport(LOG,
				(errmsg("watchdog remote node:%d on %s:%d", idx, g_cluster.remoteNodes[idx].hostname, g_cluster.remoteNodes[idx].wd_port)));

		idx++;
	}

	g_cluster.clusterLeaderInfo.leaderNode = NULL;
	g_cluster.clusterLeaderInfo.standbyNodes = palloc0(sizeof(WatchdogNode *) * g_cluster.remoteNodeCount);
	g_cluster.clusterLeaderInfo.standby_nodes_count = 0;
	g_cluster.clusterLeaderInfo.holding_vip = false;
	g_cluster.quorum_status = -1;
	g_cluster.nextCommandID = 1;
	g_cluster.clusterInitialized = false;
	g_cluster.escalation_pid = 0;
	g_cluster.de_escalation_pid = 0;
	g_cluster.unidentified_socks = NULL;
	g_cluster.command_server_sock = 0;
	g_cluster.notify_clients = NULL;
	g_cluster.ipc_command_socks = NULL;
	g_cluster.wd_timer_commands = NULL;
	g_cluster.wdCurrentFailovers = NULL;
	g_cluster.ipc_commands = NULL;
	g_cluster.localNode->state = WD_DEAD;
	g_cluster.clusterCommands = NULL;
	g_cluster.ipc_auth_needed = strlen(pool_config->wd_authkey) ? true : false;

	g_cluster.localNode->escalated = get_watchdog_node_escalation_state();

	wd_initialize_monitoring_interfaces();
	if (g_cluster.ipc_auth_needed)
	{
#ifndef USE_SSL
		ereport(LOG,
				(errmsg("watchdog is configured to use authentication, but pgpool-II is built without SSL support"),
				 errdetail("The authentication method used by pgpool-II without the SSL support is known to be weak")));
#endif
	}
	if (get_watchdog_process_needs_cleanup())
	{
		ereport(LOG,
				(errmsg("watchdog is recovering from the crash of watchdog process")));

		/*
		 * If we are recovering from crash or abnormal termination de-escalate
		 * the node if it was coordinator when it crashed
		 */
		resign_from_escalated_node();
	}
}

static void
clear_command_node_result(WDCommandNodeResult * nodeResult)
{
	nodeResult->result_type = WD_NO_MESSAGE;
	nodeResult->result_data = NULL;
	nodeResult->result_data_len = 0;
	nodeResult->cmdState = COMMAND_STATE_INIT;
}

static int
wd_create_recv_socket(int port)
{
	size_t		len = 0;
	struct sockaddr_in addr;
	int			one = 1;
	int			sock = -1;
	int			saved_errno;

	if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0)
	{
		/* socket create failed */
		ereport(ERROR,
				(errmsg("failed to create watchdog receive socket"),
				 errdetail("create socket failed with reason: \"%m\"")));
	}

	socket_set_nonblock(sock);

	if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (char *) &one, sizeof(one)) == -1)
	{
		/* setsockopt(SO_REUSEADDR) failed */
		saved_errno = errno;
		close(sock);
		ereport(ERROR,
				(errmsg("failed to create watchdog receive socket"),
				 errdetail("setsockopt(SO_REUSEADDR) failed with reason: \"%s\"", strerror(saved_errno))));
	}
	if (setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, (char *) &one, sizeof(one)) == -1)
	{
		/* setsockopt(TCP_NODELAY) failed */
		saved_errno = errno;
		close(sock);
		ereport(ERROR,
				(errmsg("failed to create watchdog receive socket"),
				 errdetail("setsockopt(TCP_NODELAY) failed with reason: \"%s\"", strerror(saved_errno))));
	}
	if (setsockopt(sock, SOL_SOCKET, SO_KEEPALIVE, (char *) &one, sizeof(one)) == -1)
	{
		/* setsockopt(SO_KEEPALIVE) failed */
		saved_errno = errno;
		close(sock);
		ereport(ERROR,
				(errmsg("failed to create watchdog receive socket"),
				 errdetail("setsockopt(SO_KEEPALIVE) failed with reason: \"%s\"", strerror(saved_errno))));
	}

	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = htonl(INADDR_ANY);
	addr.sin_port = htons(port);
	len = sizeof(struct sockaddr_in);

	if (bind(sock, (struct sockaddr *) &addr, len) < 0)
	{
		/* bind failed */
		saved_errno = errno;
		close(sock);
		ereport(ERROR,
				(errmsg("failed to create watchdog receive socket"),
				 errdetail("bind on \"TCP:%d\" failed with reason: \"%s\"", port, strerror(saved_errno))));
	}

	if (listen(sock, MAX_WATCHDOG_NUM * 2) < 0)
	{
		/* listen failed */
		saved_errno = errno;
		close(sock);
		ereport(ERROR,
				(errmsg("failed to create watchdog receive socket"),
				 errdetail("listen failed with reason: \"%s\"", strerror(saved_errno))));
	}

	return sock;
}



/*
 * creates a socket in non blocking mode and connects it to the hostname and port
 * the out parameter connected is set to true if the connection is successful
 */
static int
wd_create_client_socket(char *hostname, int port, bool *connected)
{
	int			sock;
	int			one = 1;
	size_t		len = 0;
	struct sockaddr_in addr;
	struct hostent *hp;

	*connected = false;
	/* create socket */
	if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0)
	{
		/* socket create failed */
		ereport(LOG,
				(errmsg("create socket failed with reason: \"%m\"")));
		return -1;
	}

	/* set socket option */
	if (setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, (char *) &one, sizeof(one)) == -1)
	{
		close(sock);
		ereport(LOG,
				(errmsg("failed to set socket options"),
				 errdetail("setsockopt(TCP_NODELAY) failed with error: \"%m\"")));
		return -1;
	}
	if (setsockopt(sock, SOL_SOCKET, SO_KEEPALIVE, (char *) &one, sizeof(one)) == -1)
	{
		ereport(LOG,
				(errmsg("failed to set socket options"),
				 errdetail("setsockopt(SO_KEEPALIVE) failed with error: \"%m\"")));
		close(sock);
		return -1;
	}
	/* set sockaddr_in */
	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	hp = gethostbyname(hostname);
	if ((hp == NULL) || (hp->h_addrtype != AF_INET))
	{
		hp = gethostbyaddr(hostname, strlen(hostname), AF_INET);
		if ((hp == NULL) || (hp->h_addrtype != AF_INET))
		{
			ereport(LOG,
					(errmsg("failed to get host address for \"%s\"", hostname),
					 errdetail("gethostbyaddr failed with error: \"%s\"", hstrerror(h_errno))));
			close(sock);
			return -1;
		}
	}
	memmove((char *) &(addr.sin_addr), (char *) hp->h_addr, hp->h_length);
	addr.sin_port = htons(port);
	len = sizeof(struct sockaddr_in);

	/* set socket to non blocking */
	socket_set_nonblock(sock);

	if (connect(sock, (struct sockaddr *) &addr, len) < 0)
	{
		if (errno == EINPROGRESS)
		{
			return sock;
		}
		if (errno == EISCONN)
		{
			socket_unset_nonblock(sock);
			*connected = true;
			return sock;
		}
		ereport(LOG,
				(errmsg("connect on socket failed"),
				 errdetail("connect failed with error: \"%m\"")));
		close(sock);
		return -1;
	}
	/* set socket to blocking again */
	socket_unset_nonblock(sock);
	*connected = true;
	return sock;
}

/* returns the number of successful connections */
static int
connect_with_all_configured_nodes(void)
{
	int			connect_count = 0;
	int			i;

	for (i = 0; i < g_cluster.remoteNodeCount; i++)
	{
		WatchdogNode *wdNode = &(g_cluster.remoteNodes[i]);

		if (connect_to_node(wdNode))
			connect_count++;
	}
	return connect_count;
}

/*
 * Function tries to connect with nodes which have both sockets
 * disconnected
 */
static void
try_connecting_with_all_unreachable_nodes(void)
{
	int			i;

	for (i = 0; i < g_cluster.remoteNodeCount; i++)
	{
		WatchdogNode *wdNode = &(g_cluster.remoteNodes[i]);

		if (wdNode->client_socket.sock_state != WD_SOCK_WAITING_FOR_CONNECT && wdNode->client_socket.sock_state != WD_SOCK_CONNECTED &&
			wdNode->server_socket.sock_state != WD_SOCK_WAITING_FOR_CONNECT && wdNode->server_socket.sock_state != WD_SOCK_CONNECTED)
		{
			if (wdNode->state == WD_SHUTDOWN)
				continue;
			connect_to_node(wdNode);
			if (wdNode->client_socket.sock_state == WD_SOCK_CONNECTED)
			{
				ereport(LOG,
						(errmsg("connection to the remote node \"%s\" is restored", wdNode->nodeName)));
				watchdog_state_machine(WD_EVENT_NEW_OUTBOUND_CONNECTION, wdNode, NULL, NULL);
			}
		}
	}
}

/*
 * returns true if the connection is in progress or connected successfully
 * false is returned in case of failure
 */
static bool
connect_to_node(WatchdogNode * wdNode)
{
	bool		connected = false;

	wdNode->client_socket.sock = wd_create_client_socket(wdNode->hostname, wdNode->wd_port, &connected);
	gettimeofday(&wdNode->client_socket.tv, NULL);
	if (wdNode->client_socket.sock <= 0)
	{
		wdNode->client_socket.sock_state = WD_SOCK_ERROR;
		ereport(DEBUG1,
				(errmsg("outbound connection to \"%s:%d\" failed", wdNode->hostname, wdNode->wd_port)));
	}
	else
	{
		if (connected)
			wdNode->client_socket.sock_state = WD_SOCK_CONNECTED;
		else
			wdNode->client_socket.sock_state = WD_SOCK_WAITING_FOR_CONNECT;
	}
	return (wdNode->client_socket.sock_state != WD_SOCK_ERROR);
}

/* signal handler for SIGHUP and SIGCHLD handler */
static RETSIGTYPE watchdog_signal_handler(int sig)
{
	if (sig == SIGHUP)
		reload_config_signal = 1;
	else if (sig == SIGCHLD)
		sigchld_request = 1;
}

static void
check_signals(void)
{
	/* reload config file signal? */
	if (reload_config_signal)
	{
		MemoryContext oldContext = MemoryContextSwitchTo(TopMemoryContext);

		pool_get_config(get_config_file_name(), CFGCXT_RELOAD);
		MemoryContextSwitchTo(oldContext);
		reload_config_signal = 0;
	}
	else if (sigchld_request)
	{
		wd_child_signal_handler();
	}
}


/*
 * fork a child for watchdog
 */
static pid_t
fork_watchdog_child(void)
{
	pid_t		pid;

	pid = fork();

	if (pid == 0)
	{
		on_exit_reset();

		SetProcessGlobalVariables(PT_WATCHDOG);

		/* call watchdog child main */
		POOL_SETMASK(&UnBlockSig);
		watchdog_main();
	}
	else if (pid == -1)
	{
		ereport(FATAL,
				(return_code(POOL_EXIT_FATAL),
				 errmsg("fork() failed"),
				 errdetail("%m")));
	}

	return pid;
}

/* Never returns */
static int
watchdog_main(void)
{
	fd_set		rmask;
	fd_set		wmask;
	fd_set		emask;
	const int	select_timeout = 1;
	struct timeval tv,
				ref_time;

	volatile int fd;
	sigjmp_buf	local_sigjmp_buf;

	pool_signal(SIGTERM, wd_child_exit);
	pool_signal(SIGINT, wd_child_exit);
	pool_signal(SIGQUIT, wd_child_exit);
	pool_signal(SIGHUP, watchdog_signal_handler);
	pool_signal(SIGCHLD, watchdog_signal_handler);
	pool_signal(SIGUSR1, SIG_IGN);
	pool_signal(SIGUSR2, SIG_IGN);
	pool_signal(SIGPIPE, SIG_IGN);
	pool_signal(SIGALRM, SIG_IGN);

	init_ps_display("", "", "", "");

	/* Create per loop iteration memory context */
	ProcessLoopContext = AllocSetContextCreate(TopMemoryContext,
											   "wd_child_main_loop",
											   ALLOCSET_DEFAULT_MINSIZE,
											   ALLOCSET_DEFAULT_INITSIZE,
											   ALLOCSET_DEFAULT_MAXSIZE);

	MemoryContextSwitchTo(TopMemoryContext);

	set_ps_display("watchdog", false);

	/* initialize all the local structures for watchdog */
	wd_cluster_initialize();
	/* create a server socket for incoming watchdog connections */
	g_cluster.localNode->server_socket.sock = wd_create_recv_socket(g_cluster.localNode->wd_port);
	g_cluster.localNode->server_socket.sock_state = WD_SOCK_CONNECTED;
	/* open the command server */
	g_cluster.command_server_sock = wd_create_command_server_socket();

	/* try connecting to all watchdog nodes */
	g_cluster.network_monitor_sock = create_monitoring_socket();

	if (any_interface_available() == false)
	{
		ereport(FATAL,
				(return_code(POOL_EXIT_FATAL),
				 errmsg("no valid network interface is active"),
				 errdetail("watchdog requires at least one valid network interface to continue"),
				 errhint("you can disable interface checking by setting wd_monitoring_interfaces_list = '' in pgpool config")));
	}

	connect_with_all_configured_nodes();

	/* set the initial state of local node */
	set_state(WD_LOADING);

	/*
	 * install the callback for the preparation of system exit
	 */
	on_system_exit(wd_system_will_go_down, (Datum) NULL);

	if (sigsetjmp(local_sigjmp_buf, 1) != 0)
	{
		/* Since not using PG_TRY, must reset error stack by hand */
		if (fd > 0)
			close(fd);

		error_context_stack = NULL;

		EmitErrorReport();
		MemoryContextSwitchTo(TopMemoryContext);
		FlushErrorState();
	}

	/* We can now handle ereport(ERROR) */
	PG_exception_stack = &local_sigjmp_buf;
	reset_watchdog_process_needs_cleanup();
	/* watchdog child loop */
	for (;;)
	{
		int			fd_max,
					select_ret;
		bool		timeout_event = false;

		MemoryContextSwitchTo(ProcessLoopContext);
		MemoryContextResetAndDeleteChildren(ProcessLoopContext);

		/* take care config reload request and SIGCHLD */
		check_signals();

		/*
		 * Establish all accepting socket descriptors and wait for
		 * incoming/outcoming events for up to 1 second.
		 */
		fd_max = prepare_fds(&rmask, &wmask, &emask);
		tv.tv_sec = select_timeout;
		tv.tv_usec = 0;
		select_ret = select(fd_max + 1, &rmask, &wmask, &emask, &tv);

		gettimeofday(&ref_time, NULL);

		if (g_timeout_sec > 0)
		{
			if (WD_TIME_DIFF_SEC(ref_time, g_tm_set_time) >= g_timeout_sec)
			{
				timeout_event = true;
				g_timeout_sec = 0;
			}
		}
#ifdef WATCHDOG_DEBUG
		load_watchdog_debug_test_option();
#endif
		/* process events */
		if (select_ret > 0)
		{
			int			processed_fds = 0;

			processed_fds += accept_incoming_connections(&rmask, (select_ret - processed_fds));
			processed_fds += update_successful_outgoing_cons(&wmask, (select_ret - processed_fds));
			processed_fds += read_sockets(&rmask, (select_ret - processed_fds));
		}

		/*
		 * Take care online recovery
		 */
		if (WD_TIME_DIFF_SEC(ref_time, g_tm_set_time) >= 1)
		{
			process_wd_func_commands_for_timer_events();
		}

		if (timeout_event)
		{
			g_timeout_sec = 0;
			watchdog_state_machine(WD_EVENT_TIMEOUT, NULL, NULL, NULL);
		}

		check_for_current_command_timeout();

		/*
		 * If any of connections to remote nodes are established, send
		 * commands to the remote nodes.
		 */
		if (service_lost_connections() == true)
		{
			service_internal_command();
			service_ipc_commands();
		}

		/*
		 * Remove the unreachable nodes from cluster
		 */
		service_unreachable_nodes();

		/*
		 * If I am the leader, update the quorum status.
		 */
		if (get_local_node_state() == WD_COORDINATOR)
		{
			update_quorum_status();
		}

		/*
		 * Remove any expired failover command (had spent over 15 seconds
		 * (FAILOVER_COMMAND_FINISH_TIMEOUT)
		 */
		service_expired_failovers();
	}
	return 0;
}

static int
wd_create_command_server_socket(void)
{
	size_t		len = 0;
	struct sockaddr_un addr;
	int			sock = -1;

	/* We use unix domain stream sockets for the purpose */
	if ((sock = socket(AF_UNIX, SOCK_STREAM, 0)) < 0)
	{
		/* socket create failed */
		ereport(FATAL,
				(return_code(POOL_EXIT_FATAL),
				 errmsg("failed to create watchdog command server socket"),
				 errdetail("create socket failed with reason: \"%m\"")));
	}
	memset((char *) &addr, 0, sizeof(addr));
	addr.sun_family = AF_UNIX;
	snprintf(addr.sun_path, sizeof(addr.sun_path), "%s", get_watchdog_ipc_address());
	len = sizeof(struct sockaddr_un);

	ereport(INFO,
			(errmsg("IPC socket path: \"%s\"", get_watchdog_ipc_address())));

	/* Delete any pre-existing socket file to avoid failure at bind() time */
	unlink(addr.sun_path);

	if (bind(sock, (struct sockaddr *) &addr, len) == -1)
	{
		int			saved_errno = errno;

		close(sock);
		unlink(addr.sun_path);
		ereport(FATAL,
				(return_code(POOL_EXIT_FATAL),
				 errmsg("failed to create watchdog command server socket"),
				 errdetail("bind on \"%s\" failed with reason: \"%s\"", addr.sun_path, strerror(saved_errno))));
	}

	if (listen(sock, 5) < 0)
	{
		/* listen failed */
		int			saved_errno = errno;

		close(sock);
		unlink(addr.sun_path);
		ereport(FATAL,
				(return_code(POOL_EXIT_FATAL),
				 errmsg("failed to create watchdog command server socket"),
				 errdetail("listen failed with reason: \"%s\"", strerror(saved_errno))));
	}
	on_proc_exit(FileUnlink, (Datum) pstrdup(addr.sun_path));
	return sock;
}

static void
FileUnlink(int code, Datum path)
{
	char	   *filePath = (char *) path;

	unlink(filePath);
}


/*
 * sets all the valid watchdog cluster descriptors to the fd_set.
 returns the fd_max */
static int
prepare_fds(fd_set *rmask, fd_set *wmask, fd_set *emask)
{
	int			i;
	ListCell   *lc;
	int			fd_max = g_cluster.localNode->server_socket.sock;

	FD_ZERO(rmask);
	FD_ZERO(wmask);
	FD_ZERO(emask);

	/* local node server socket will set the read and exception fds */
	FD_SET(g_cluster.localNode->server_socket.sock, rmask);
	FD_SET(g_cluster.localNode->server_socket.sock, emask);

	/* command server socket will set the read and exception fds */
	FD_SET(g_cluster.command_server_sock, rmask);
	FD_SET(g_cluster.command_server_sock, emask);
	if (fd_max < g_cluster.command_server_sock)
		fd_max = g_cluster.command_server_sock;

	FD_SET(g_cluster.network_monitor_sock, rmask);
	if (fd_max < g_cluster.network_monitor_sock)
		fd_max = g_cluster.network_monitor_sock;

	/*
	 * set write fdset for all waiting for connection sockets, while already
	 * connected will be only be waiting for read
	 */
	for (i = 0; i < g_cluster.remoteNodeCount; i++)
	{
		WatchdogNode *wdNode = &(g_cluster.remoteNodes[i]);

		if (wdNode->client_socket.sock > 0)
		{
			if (fd_max < wdNode->client_socket.sock)
				fd_max = wdNode->client_socket.sock;

			FD_SET(wdNode->client_socket.sock, emask);

			if (wdNode->client_socket.sock_state == WD_SOCK_WAITING_FOR_CONNECT)
				FD_SET(wdNode->client_socket.sock, wmask);
			else
				FD_SET(wdNode->client_socket.sock, rmask);
		}
		if (wdNode->server_socket.sock > 0)
		{
			if (fd_max < wdNode->server_socket.sock)
				fd_max = wdNode->server_socket.sock;

			FD_SET(wdNode->server_socket.sock, emask);
			FD_SET(wdNode->server_socket.sock, rmask);
		}
	}

	/*
	 * I know this is getting complex but we need to add all incoming
	 * unassigned connection sockets these one will go for reading
	 */
	foreach(lc, g_cluster.unidentified_socks)
	{
		SocketConnection *conn = lfirst(lc);
		int			ui_sock = conn->sock;

		if (ui_sock > 0)
		{
			FD_SET(ui_sock, rmask);
			FD_SET(ui_sock, emask);
			if (fd_max < ui_sock)
				fd_max = ui_sock;
		}
	}

	/* Add the notification connected clients */
	foreach(lc, g_cluster.notify_clients)
	{
		int			ui_sock = lfirst_int(lc);

		if (ui_sock > 0)
		{
			FD_SET(ui_sock, rmask);
			FD_SET(ui_sock, emask);
			if (fd_max < ui_sock)
				fd_max = ui_sock;
		}
	}

	/* Finally Add the command IPC sockets */
	foreach(lc, g_cluster.ipc_command_socks)
	{
		int			ui_sock = lfirst_int(lc);

		if (ui_sock > 0)
		{
			FD_SET(ui_sock, rmask);
			FD_SET(ui_sock, emask);
			if (fd_max < ui_sock)
				fd_max = ui_sock;
		}
	}

	return fd_max;
}

static int
read_sockets(fd_set *rmask, int pending_fds_count)
{
	int			i,
				count = 0;
	List	   *socks_to_del = NIL;
	ListCell   *lc;

	for (i = 0; i < g_cluster.remoteNodeCount; i++)
	{
		WatchdogNode *wdNode = &(g_cluster.remoteNodes[i]);

		if (is_socket_connection_connected(&wdNode->client_socket))
		{
			if (FD_ISSET(wdNode->client_socket.sock, rmask))
			{
				ereport(DEBUG2,
						(errmsg("client socket of %s is ready for reading", wdNode->nodeName)));

				WDPacketData *pkt = read_packet(&wdNode->client_socket);

				if (pkt)
				{
					if (check_debug_request_kill_all_communication() == false &&
						check_debug_request_kill_all_receivers() == false)
					{
						watchdog_state_machine(WD_EVENT_PACKET_RCV, wdNode, pkt, NULL);
						/* since a packet is received reset last sent time */
						wdNode->last_sent_time.tv_sec = 0;
						wdNode->last_sent_time.tv_usec = 0;
					}
					free_packet(pkt);
				}
				else
				{
					ereport(LOG,
							(errmsg("client socket of %s is closed", wdNode->nodeName)));
				}

				count++;
				if (count >= pending_fds_count)
					return count;
			}
		}
		if (is_socket_connection_connected(&wdNode->server_socket))
		{
			if (FD_ISSET(wdNode->server_socket.sock, rmask))
			{
				ereport(DEBUG2,
						(errmsg("server socket of %s is ready for reading", wdNode->nodeName)));
				WDPacketData *pkt = read_packet(&wdNode->server_socket);

				if (pkt)
				{
					if (check_debug_request_kill_all_communication() == false &&
						check_debug_request_kill_all_receivers() == false)
					{
						watchdog_state_machine(WD_EVENT_PACKET_RCV, wdNode, pkt, NULL);
						/* since a packet is received reset last sent time */
						wdNode->last_sent_time.tv_sec = 0;
						wdNode->last_sent_time.tv_usec = 0;
					}
					free_packet(pkt);
				}
				else
				{
					ereport(LOG,
							(errmsg("outbound socket of %s is closed", wdNode->nodeName)));
				}

				count++;
				if (count >= pending_fds_count)
					return count;
			}
		}
	}

	foreach(lc, g_cluster.unidentified_socks)
	{
		SocketConnection *conn = lfirst(lc);

		if (conn->sock > 0 && FD_ISSET(conn->sock, rmask))
		{
			WDPacketData *pkt;

			ereport(DEBUG2,
					(errmsg("un-identified socket %d is ready for reading", conn->sock)));
			/* we only entertain ADD NODE messages from unidentified sockets */
			pkt = read_packet_of_type(conn, WD_ADD_NODE_MESSAGE);
			if (pkt)
			{
				struct timeval 	previous_startup_time;
				char	   *authkey = NULL;
				WatchdogNode *tempNode = parse_node_info_message(pkt, &authkey);

				if (tempNode)
				{
					WatchdogNode *wdNode;
					bool		found = false;
					bool		authenticated = false;

					if (tempNode->pgpool_node_id == pool_config->pgpool_node_id)
					{
						ereport(ERROR,
								(errmsg("the pgpool node id configured on node \"%s\" cannot be same as local node", tempNode->nodeName),
								 errdetail("this node id is \"%d\" while local node is \"%d\"",
										   tempNode->pgpool_node_id,
										   pool_config->pgpool_node_id)));
					}

					print_watchdog_node_info(tempNode);
					authenticated = verify_authhash_for_node(tempNode, authkey);
					ereport(DEBUG1,
							(errmsg("ADD NODE MESSAGE from hostname:\"%s\" port:%d pgpool_port:%d", tempNode->hostname, tempNode->wd_port, tempNode->pgpool_port)));
					/* verify this node */
					if (authenticated)
					{
						WD_STATES oldNodeState = WD_DEAD;
						for (i = 0; i < g_cluster.remoteNodeCount; i++)
						{
							wdNode = &(g_cluster.remoteNodes[i]);

							if ((wdNode->wd_port == tempNode->wd_port && wdNode->pgpool_port == tempNode->pgpool_port &&
								wdNode->pgpool_node_id == tempNode->pgpool_node_id) &&
								((strcmp(wdNode->hostname, conn->addr) == 0) || (strcmp(wdNode->hostname, tempNode->hostname) == 0)))
							{
								/* We have found the match */
								found = true;
								previous_startup_time.tv_sec = wdNode->startup_time.tv_sec;
								oldNodeState = wdNode->state;

								close_socket_connection(&wdNode->server_socket);
								strlcpy(wdNode->delegate_ip, tempNode->delegate_ip, WD_MAX_HOST_NAMELEN);
								strlcpy(wdNode->nodeName, tempNode->nodeName, WD_MAX_HOST_NAMELEN);
								strlcpy(wdNode->pgp_version, tempNode->pgp_version, MAX_VERSION_STR_LEN);
								wdNode->state = tempNode->state;
								wdNode->wd_data_major_version = tempNode->wd_data_major_version;
								wdNode->wd_data_minor_version = tempNode->wd_data_minor_version;
								wdNode->startup_time.tv_sec = tempNode->startup_time.tv_sec;
								wdNode->wd_priority = tempNode->wd_priority;
								wdNode->server_socket = *conn;
								wdNode->server_socket.sock_state = WD_SOCK_CONNECTED;
								if (tempNode->current_state_time.tv_sec)
								{
									wdNode->current_state_time.tv_sec = tempNode->current_state_time.tv_sec;
									wdNode->escalated = tempNode->escalated;
									wdNode->standby_nodes_count = tempNode->standby_nodes_count;
									wdNode->quorum_status = tempNode->quorum_status;
								}
								break;
							}
						}
						if (found)
						{
							restore_cluster_membership_of_node(wdNode);
							/* reply with node info message */
							ereport(LOG,
									(errmsg("new node joined the cluster hostname:\"%s\" port:%d pgpool_port:%d", wdNode->hostname,
											wdNode->wd_port,
											wdNode->pgpool_port),
									 errdetail("Pgpool-II version:\"%s\" watchdog messaging version: %d.%d",
											   wdNode->pgp_version,
											   wdNode->wd_data_major_version,
											   wdNode->wd_data_minor_version)));

							if (oldNodeState == WD_SHUTDOWN)
							{
								ereport(LOG,
										(errmsg("The newly joined node:\"%s\" had left the cluster because it was shutdown",wdNode->nodeName)));
								watchdog_state_machine(WD_EVENT_PACKET_RCV, wdNode, pkt, NULL);

							}
							else if (oldNodeState == WD_LOST)
							{
								ereport(LOG,
										(errmsg("The newly joined node:\"%s\" had left the cluster because it was lost",wdNode->nodeName),
										 errdetail("lost reason was \"%s\" and startup time diff = %d",
												   wd_node_lost_reasons[wdNode->node_lost_reason],
												   abs((int)(previous_startup_time.tv_sec - wdNode->startup_time.tv_sec)))));

								watchdog_state_machine(WD_EVENT_PACKET_RCV, wdNode, pkt, NULL);
								/* Since the node was lost. Fire node found event as well */
								watchdog_state_machine(WD_EVENT_REMOTE_NODE_FOUND, wdNode, NULL, NULL);
							}

						}
						else
							ereport(NOTICE,
									(errmsg("add node from hostname:\"%s\" port:%d pgpool_port:%d rejected.", tempNode->hostname, tempNode->wd_port, tempNode->pgpool_port),
									 errdetail("verify the other watchdog node configurations")));
					}
					else
					{
						ereport(NOTICE,
								(errmsg("authentication failed for add node from hostname:\"%s\" port:%d pgpool_port:%d", tempNode->hostname, tempNode->wd_port, tempNode->pgpool_port),
								 errdetail("make sure wd_authkey configuration is same on all nodes")));
					}

					if (found == false || authenticated == false)
					{
						/*
						 * reply with reject message, We do not need to go to
						 * state processor
						 */
						/* For now, create a empty temp node. */
						WatchdogNode tmpNode;

						tmpNode.client_socket = *conn;
						tmpNode.client_socket.sock_state = WD_SOCK_CONNECTED;
						tmpNode.server_socket.sock = -1;
						tmpNode.server_socket.sock_state = WD_SOCK_UNINITIALIZED;
						reply_with_minimal_message(&tmpNode, WD_REJECT_MESSAGE, pkt);
						close_socket_connection(conn);
					}
					pfree(tempNode);
				}
				else
				{
					/*
					 * Probably some invalid data in the add message
					 */
					WatchdogNode tmpNode;

					ereport(LOG,
							(errmsg("unable to parse the add node message")));
					tmpNode.client_socket = *conn;
					tmpNode.client_socket.sock_state = WD_SOCK_CONNECTED;
					tmpNode.server_socket.sock = -1;
					tmpNode.server_socket.sock_state = WD_SOCK_UNINITIALIZED;
					reply_with_minimal_message(&tmpNode, WD_REJECT_MESSAGE, pkt);
					close_socket_connection(conn);
				}
				if (authkey)
					pfree(authkey);
				free_packet(pkt);
				count++;
			}
			socks_to_del = lappend(socks_to_del, conn);
			count++;
			if (count >= pending_fds_count)
				break;
		}
	}

	/* delete all the sockets from unidentified list which are now identified */
	foreach(lc, socks_to_del)
	{
		g_cluster.unidentified_socks = list_delete_ptr(g_cluster.unidentified_socks, lfirst(lc));
	}

	list_free_deep(socks_to_del);
	socks_to_del = NULL;

	if (count >= pending_fds_count)
		return count;

	foreach(lc, g_cluster.ipc_command_socks)
	{
		int			command_sock = lfirst_int(lc);

		if (command_sock > 0 && FD_ISSET(command_sock, rmask))
		{
			bool		remove_sock = false;

			read_ipc_socket_and_process(command_sock, &remove_sock);
			if (remove_sock)
			{
				/* Also locate the command if it has this socket */
				WDCommandData *ipcCommand = get_wd_IPC_command_from_socket(command_sock);

				if (ipcCommand)
				{
					/*
					 * special case we want to remove the socket from
					 * ipc_command_sock list manually, so mark the issuing
					 * socket of ipcCommand to invalid value
					 */
					ipcCommand->sourceIPCSocket = -1;
				}
				close(command_sock);
				socks_to_del = lappend_int(socks_to_del, command_sock);
			}
			count++;
			if (count >= pending_fds_count)
				break;
		}
	}
	/* delete all the sockets from unidentified list which are now identified */
	foreach(lc, socks_to_del)
	{
		g_cluster.ipc_command_socks = list_delete_int(g_cluster.ipc_command_socks, lfirst_int(lc));
	}

	list_free(socks_to_del);
	socks_to_del = NULL;

	if (count >= pending_fds_count)
		return count;

	foreach(lc, g_cluster.notify_clients)
	{
		int			notify_sock = lfirst_int(lc);

		if (notify_sock > 0 && FD_ISSET(notify_sock, rmask))
		{
			bool		remove_sock = false;

			read_ipc_socket_and_process(notify_sock, &remove_sock);
			if (remove_sock)
			{
				close(notify_sock);
				socks_to_del = lappend_int(socks_to_del, notify_sock);
			}
			count++;
			if (count >= pending_fds_count)
				break;
		}
	}
	/* delete all the sockets from unidentified list which are now identified */
	foreach(lc, socks_to_del)
	{
		g_cluster.notify_clients = list_delete_int(g_cluster.notify_clients, lfirst_int(lc));
	}

	list_free(socks_to_del);
	socks_to_del = NULL;


	/* Finally check if something waits us on interface monitoring socket */
	if (g_cluster.network_monitor_sock > 0 && FD_ISSET(g_cluster.network_monitor_sock, rmask))
	{
		bool		deleted;
		bool		link_event;

		if (read_interface_change_event(g_cluster.network_monitor_sock, &link_event, &deleted))
		{
			ereport(DEBUG1,
					(errmsg("network event received"),
					 errdetail("deleted = %s Link change event = %s",
							   deleted ? "YES" : "NO",
							   link_event ? "YES" : "NO")));
			if (link_event)
			{
				if (deleted)
					watchdog_state_machine(WD_EVENT_NW_LINK_IS_INACTIVE, NULL, NULL, NULL);
				else
					watchdog_state_machine(WD_EVENT_NW_LINK_IS_ACTIVE, NULL, NULL, NULL);
			}
			else
			{
				if (deleted)
					watchdog_state_machine(WD_EVENT_NW_IP_IS_REMOVED, NULL, NULL, NULL);
				else
					watchdog_state_machine(WD_EVENT_NW_IP_IS_ASSIGNED, NULL, NULL, NULL);
			}
		}
		count++;
	}
	return count;
}

static bool
write_ipc_command_with_result_data(WDCommandData * ipcCommand, char type, char *data, int len)
{
	WDPacketData pkt;

	pkt.data = data;
	pkt.len = len;
	pkt.type = type;
	pkt.command_id = 0;			/* command Id is not used in IPC packets */

	if (ipcCommand == NULL || ipcCommand->commandSource != COMMAND_SOURCE_IPC || ipcCommand->sourceIPCSocket <= 0)
	{
		ereport(DEBUG1,
				(errmsg("not replying to IPC, Invalid IPC command.")));
		return false;
	}
	/* DEBUG AID */
	if (ipcCommand->commandSource == COMMAND_SOURCE_REMOTE &&
		(check_debug_request_kill_all_senders() ||
		check_debug_request_kill_all_communication()))
		return false;

	return write_packet_to_socket(ipcCommand->sourceIPCSocket, &pkt, true);
}

static WDCommandData * create_command_object(int packet_data_length)
{
	MemoryContext mCxt,
				oldCxt;
	WDCommandData *wdCommand;

	/* wd command lives in its own memory context */
	mCxt = AllocSetContextCreate(TopMemoryContext,
								 "WDCommand",
								 ALLOCSET_SMALL_MINSIZE,
								 ALLOCSET_SMALL_INITSIZE,
								 ALLOCSET_SMALL_MAXSIZE);
	oldCxt = MemoryContextSwitchTo(mCxt);

	wdCommand = palloc0(sizeof(WDCommandData));
	wdCommand->memoryContext = mCxt;
	if (packet_data_length > 0)
		wdCommand->sourcePacket.data = palloc(packet_data_length);
	wdCommand->commandPacket.type = WD_NO_MESSAGE;
	wdCommand->sourcePacket.type = WD_NO_MESSAGE;
	MemoryContextSwitchTo(oldCxt);
	return wdCommand;
}

static bool
read_ipc_socket_and_process(int sock, bool *remove_socket)
{
	char		type;
	int			data_len,
				ret;
	WDCommandData *ipcCommand;
	IPC_CMD_PROCESS_RES res;

	*remove_socket = true;

	/* 1st byte is command type */
	ret = socket_read(sock, &type, sizeof(char), 0);
	if (ret == 0)				/* remote end has closed the connection */
		return false;

	if (ret != sizeof(char))
	{
		ereport(WARNING,
				(errmsg("error reading from IPC socket"),
				 errdetail("read from socket failed with error \"%m\"")));
		return false;
	}

	/* We should have data length */
	ret = socket_read(sock, &data_len, sizeof(int), 0);
	if (ret != sizeof(int))
	{
		ereport(WARNING,
				(errmsg("error reading from IPC socket"),
				 errdetail("read from socket failed with error \"%m\"")));
		return false;
	}

	data_len = ntohl(data_len);
	/* see if we have enough information to process this command */
	ipcCommand = create_command_object(data_len);
	ipcCommand->sourceIPCSocket = sock;
	ipcCommand->commandSource = COMMAND_SOURCE_IPC;
	ipcCommand->sourceWdNode = g_cluster.localNode;
	ipcCommand->sourcePacket.type = type;
	ipcCommand->sourcePacket.len = data_len;
	gettimeofday(&ipcCommand->commandTime, NULL);

	if (data_len > 0)
	{
		if (socket_read(sock, ipcCommand->sourcePacket.data, data_len, 0) <= 0)
		{
			ereport(LOG,
					(errmsg("error reading IPC from socket"),
					 errdetail("read from socket failed with error \"%m\"")));
			return false;
		}
	}

	res = process_IPC_command(ipcCommand);
	if (res == IPC_CMD_PROCESSING)
	{
		/*
		 * The command still needs further processing store it in the list
		 */
		MemoryContext oldCxt;

		*remove_socket = false;
		oldCxt = MemoryContextSwitchTo(TopMemoryContext);
		g_cluster.ipc_commands = lappend(g_cluster.ipc_commands, ipcCommand);
		MemoryContextSwitchTo(oldCxt);
		return true;
	}
	else if (res != IPC_CMD_COMPLETE)
	{
		char		res_type;
		char	   *data = NULL;
		int			data_len = 0;

		switch (res)
		{
			case IPC_CMD_TRY_AGAIN:
				res_type = WD_IPC_CMD_CLUSTER_IN_TRAN;
				break;
			case IPC_CMD_ERROR:
				ereport(NOTICE,
						(errmsg("IPC command returned error")));
				res_type = WD_IPC_CMD_RESULT_BAD;
				break;
			case IPC_CMD_OK:
				res_type = WD_IPC_CMD_RESULT_OK;
				break;
			default:
				res_type = WD_IPC_CMD_RESULT_BAD;
				ereport(NOTICE,
						(errmsg("unexpected IPC processing result")));
				break;
		}
		if (ipcCommand->errorMessage)
		{
			data = get_wd_simple_message_json(ipcCommand->errorMessage);
			data_len = strlen(data) + 1;
		}

		if (write_ipc_command_with_result_data(ipcCommand, res_type, data, data_len))
		{
			ereport(NOTICE,
					(errmsg("error writing to IPC socket")));
		}
		if (data)
			pfree(data);
	}

	/*
	 * Delete the Command structure, it is as simple as to delete the memory
	 * context
	 */
	MemoryContextDelete(ipcCommand->memoryContext);
	return (res != IPC_CMD_ERROR);
}

static IPC_CMD_PROCESS_RES process_IPC_command(WDCommandData * ipcCommand)
{
	/* authenticate the client first */
	if (check_and_report_IPC_authentication(ipcCommand) == false)
	{
		/* authentication error is already reported to the caller */
		return IPC_CMD_ERROR;
	}

	switch (ipcCommand->sourcePacket.type)
	{

		case WD_NODE_STATUS_CHANGE_COMMAND:
			return process_IPC_nodeStatusChange_command(ipcCommand);
			break;

		case WD_REGISTER_FOR_NOTIFICATION:
			/* Add this socket to the notify socket list */
			g_cluster.notify_clients = lappend_int(g_cluster.notify_clients, ipcCommand->sourceIPCSocket);
			/* The command is completed successfully */
			return IPC_CMD_COMPLETE;
			break;

		case WD_GET_NODES_LIST_COMMAND:
			return process_IPC_nodeList_command(ipcCommand);
			break;

		case WD_IPC_FAILOVER_COMMAND:
			return process_IPC_failover_command(ipcCommand);

		case WD_IPC_ONLINE_RECOVERY_COMMAND:
			return process_IPC_online_recovery(ipcCommand);
			break;

		case WD_FAILOVER_INDICATION:
			return process_IPC_failover_indication(ipcCommand);
			break;

		case WD_GET_LEADER_DATA_REQUEST:
			return process_IPC_data_request_from_leader(ipcCommand);
			break;

		case WD_GET_RUNTIME_VARIABLE_VALUE:
			return process_IPC_get_runtime_variable_value_request(ipcCommand);
			break;

		case WD_EXECUTE_CLUSTER_COMMAND:
			return process_IPC_execute_cluster_command(ipcCommand);
			break;

		default:
			ipcCommand->errorMessage = MemoryContextStrdup(ipcCommand->memoryContext, "unknown IPC command type");
			break;
	}
	return IPC_CMD_ERROR;
}

static IPC_CMD_PROCESS_RES
process_IPC_execute_cluster_command(WDCommandData * ipcCommand)
{
	/* get the json for node list */
	char 	*clusterCommand = NULL;
	List 	*args_list = NULL;

	if (ipcCommand->sourcePacket.len <= 0 || ipcCommand->sourcePacket.data == NULL)
		return IPC_CMD_ERROR;

	if (!parse_wd_exec_cluster_command_json(ipcCommand->sourcePacket.data, ipcCommand->sourcePacket.len,
									   &clusterCommand, &args_list))
	{
		goto ERROR_EXIT;
	}
	if (strcasecmp(WD_COMMAND_SHUTDOWN_CLUSTER, clusterCommand) == 0)
	{
		ereport(LOG,
				(errmsg("Watchdog has received shutdown cluster command from IPC channel")));
	}
	else if (strcasecmp(WD_COMMAND_RELOAD_CONFIG_CLUSTER, clusterCommand) == 0)
	{
		ereport(LOG,
				(errmsg("Watchdog has received reload config cluster command from IPC channel")));
	}
	else if (strcasecmp(WD_COMMAND_LOCK_ON_STANDBY, clusterCommand) == 0)
	{
		ereport(LOG,
				(errmsg("Watchdog has received 'LOCK ON STANDBY' command from IPC channel")));
		if (get_local_node_state() != WD_COORDINATOR)
		{
			ereport(LOG,
					(errmsg("'LOCK ON STANDBY' command can only be processed on coordinator node")));
			goto ERROR_EXIT;
		}
	}
	else
	{
		ipcCommand->errorMessage = MemoryContextStrdup(ipcCommand->memoryContext,
													   "unknown cluster command requested");
		goto ERROR_EXIT;
	}

	/*
	 * Just broadcast the execute command request to destination node
	 * Processing the command on the local node is the responsibility of caller
	 * process
	 */
	reply_with_message(NULL, WD_EXECUTE_COMMAND_REQUEST,
					   ipcCommand->sourcePacket.data, ipcCommand->sourcePacket.len,
					   NULL);

	if (args_list)
		list_free_deep(args_list);

	pfree(clusterCommand);
	return IPC_CMD_OK;

ERROR_EXIT:
	if (args_list)
		list_free_deep(args_list);
	if (clusterCommand)
		pfree(clusterCommand);
	return IPC_CMD_ERROR;
}

static IPC_CMD_PROCESS_RES process_IPC_get_runtime_variable_value_request(WDCommandData * ipcCommand)
{
	/* get the json for node list */
	JsonNode   *jNode = NULL;
	char	   *requestVarName = NULL;

	if (ipcCommand->sourcePacket.len <= 0 || ipcCommand->sourcePacket.data == NULL)
		return IPC_CMD_ERROR;

	json_value *root = json_parse(ipcCommand->sourcePacket.data, ipcCommand->sourcePacket.len);

	/* The root node must be object */
	if (root == NULL || root->type != json_object)
	{
		json_value_free(root);
		ereport(NOTICE,
				(errmsg("failed to process get local variable IPC command"),
				 errdetail("unable to parse JSON data")));
		return IPC_CMD_ERROR;
	}

	requestVarName = json_get_string_value_for_key(root, WD_JSON_KEY_VARIABLE_NAME);

	if (requestVarName == NULL)
	{
		json_value_free(root);
		ipcCommand->errorMessage = MemoryContextStrdup(ipcCommand->memoryContext,
													   "requested variable name is null");
		return IPC_CMD_ERROR;
	}

	jNode = jw_create_with_object(true);

	if (strcasecmp(WD_RUNTIME_VAR_WD_STATE, requestVarName) == 0)
	{
		jw_put_int(jNode, WD_JSON_KEY_VALUE_DATA_TYPE, VALUE_DATA_TYPE_INT);
		jw_put_int(jNode, WD_JSON_KEY_VALUE_DATA, g_cluster.localNode->state);
	}
	else if (strcasecmp(WD_RUNTIME_VAR_QUORUM_STATE, requestVarName) == 0)
	{
		jw_put_int(jNode, WD_JSON_KEY_VALUE_DATA_TYPE, VALUE_DATA_TYPE_INT);
		jw_put_int(jNode, WD_JSON_KEY_VALUE_DATA, WD_LEADER_NODE ? WD_LEADER_NODE->quorum_status : -2);
	}
	else if (strcasecmp(WD_RUNTIME_VAR_ESCALATION_STATE, requestVarName) == 0)
	{
		jw_put_int(jNode, WD_JSON_KEY_VALUE_DATA_TYPE, VALUE_DATA_TYPE_BOOL);
		jw_put_int(jNode, WD_JSON_KEY_VALUE_DATA, g_cluster.localNode->escalated);
	}
	else
	{
		json_value_free(root);
		jw_destroy(jNode);
		ipcCommand->errorMessage = MemoryContextStrdup(ipcCommand->memoryContext,
													   "unknown variable requested");
		return IPC_CMD_ERROR;
	}

	jw_finish_document(jNode);
	json_value_free(root);
	write_ipc_command_with_result_data(ipcCommand, WD_IPC_CMD_RESULT_OK,
									   jw_get_json_string(jNode), jw_get_json_length(jNode) + 1);
	jw_destroy(jNode);
	return IPC_CMD_COMPLETE;
}

static IPC_CMD_PROCESS_RES process_IPC_nodeList_command(WDCommandData * ipcCommand)
{
	/* get the json for node list */
	JsonNode   *jNode = NULL;
	int			NodeID = -1;

	if (ipcCommand->sourcePacket.len <= 0 || ipcCommand->sourcePacket.data == NULL)
		return IPC_CMD_ERROR;

	json_value *root = json_parse(ipcCommand->sourcePacket.data, ipcCommand->sourcePacket.len);

	/* The root node must be object */
	if (root == NULL || root->type != json_object)
	{
		json_value_free(root);
		ereport(NOTICE,
				(errmsg("failed to process GET NODE LIST IPC command"),
				 errdetail("unable to parse json data")));
		return IPC_CMD_ERROR;
	}

	if (json_get_int_value_for_key(root, "NodeID", &NodeID))
	{
		json_value_free(root);
		return IPC_CMD_ERROR;
	}

	json_value_free(root);
	jNode = get_node_list_json(NodeID);
	write_ipc_command_with_result_data(ipcCommand, WD_IPC_CMD_RESULT_OK,
									   jw_get_json_string(jNode), jw_get_json_length(jNode) + 1);
	jw_destroy(jNode);
	return IPC_CMD_COMPLETE;
}

static IPC_CMD_PROCESS_RES process_IPC_nodeStatusChange_command(WDCommandData * ipcCommand)
{
	int			nodeStatus;
	int			nodeID;
	char	   *message = NULL;
	bool		ret;

	if (ipcCommand->sourcePacket.len <= 0 || ipcCommand->sourcePacket.data == NULL)
		return IPC_CMD_ERROR;

	ret = parse_node_status_json(ipcCommand->sourcePacket.data, ipcCommand->sourcePacket.len, &nodeID, &nodeStatus, &message);

	if (ret == false)
	{
		ereport(NOTICE,
				(errmsg("failed to process NODE STATE CHANGE IPC command"),
				 errdetail("unable to parse JSON data")));
		return IPC_CMD_ERROR;
	}

	if (message)
	{
		ereport(LOG,
				(errmsg("received node status change ipc message"),
				 errdetail("%s", message)));
		pfree(message);
	}
	if (fire_node_status_event(nodeID, nodeStatus) == false)
		return IPC_CMD_ERROR;

	return IPC_CMD_COMPLETE;
}

static bool
fire_node_status_event(int nodeID, int nodeStatus)
{
	WatchdogNode *wdNode = NULL;

	if (g_cluster.localNode->pgpool_node_id == nodeID)
	{
		wdNode = g_cluster.localNode;
	}
	else
	{
		int			i;

		for (i = 0; i < g_cluster.remoteNodeCount; i++)
		{
			if (nodeID == g_cluster.remoteNodes[i].pgpool_node_id)
			{
				wdNode = &g_cluster.remoteNodes[i];
				break;
			}
		}
	}
	if (wdNode == NULL)
	{
		ereport(LOG,
				(errmsg("failed to process node status change event"),
				 errdetail("invalid Node ID in the event")));
		return false;
	}

	if (nodeStatus == WD_LIFECHECK_NODE_STATUS_DEAD)
	{
		ereport(DEBUG1,
				(errmsg("processing node status changed to DEAD event for node ID:%d", nodeID)));

		if (wdNode == g_cluster.localNode)
			watchdog_state_machine(WD_EVENT_LOCAL_NODE_LOST, wdNode, NULL, NULL);
		else
		{
			wdNode->node_lost_reason = NODE_LOST_BY_LIFECHECK;
			watchdog_state_machine(WD_EVENT_REMOTE_NODE_LOST, wdNode, NULL, NULL);
		}
	}
	else if (nodeStatus == WD_LIFECHECK_NODE_STATUS_ALIVE)
	{
		ereport(DEBUG1,
				(errmsg("processing node status changed to ALIVE event for node ID:%d", nodeID)));

		if (wdNode == g_cluster.localNode)
			watchdog_state_machine(WD_EVENT_LOCAL_NODE_FOUND, wdNode, NULL, NULL);
		else
			watchdog_state_machine(WD_EVENT_REMOTE_NODE_FOUND, wdNode, NULL, NULL);
	}
	else
		ereport(LOG,
				(errmsg("failed to process node status change event"),
				 errdetail("invalid event type")));
	return true;
}

/*
 * Free the failover object
 */
static void
remove_failover_object(WDFailoverObject * failoverObj)
{
	ereport(DEBUG1,
			(errmsg("removing failover request from %d nodes with ID:%d", failoverObj->request_count, failoverObj->failoverID)));
	g_cluster.wdCurrentFailovers = list_delete_ptr(g_cluster.wdCurrentFailovers, failoverObj);
	list_free(failoverObj->requestingNodes);
	pfree(failoverObj->nodeList);
	pfree(failoverObj);
}


/* if the wdNode is NULL. The function removes all failover objects */
static void
clear_all_failovers(void)
{
	ListCell   *lc;
	List	   *failovers_to_del = list_copy(g_cluster.wdCurrentFailovers);

	ereport(DEBUG1,
			(errmsg("Removing all failover objects")));

	foreach(lc, failovers_to_del)
	{
		WDFailoverObject *failoverObj = lfirst(lc);

		remove_failover_object(failoverObj);
	}
	list_free(failovers_to_del);
}

/* Remove the over stayed failover objects */
static void
service_expired_failovers(void)
{
	ListCell   *lc;
	List	   *failovers_to_del = NULL;
	bool		need_to_resign = false;
	struct timeval currTime;

	if (get_local_node_state() != WD_COORDINATOR)
		return;

	gettimeofday(&currTime, NULL);

	foreach(lc, g_cluster.wdCurrentFailovers)
	{
		WDFailoverObject *failoverObj = lfirst(lc);

		if (failoverObj)
		{
			if (WD_TIME_DIFF_SEC(currTime, failoverObj->startTime) >= g_cluster.failover_command_timeout)
			{
				failovers_to_del = lappend(failovers_to_del, failoverObj);
				ereport(DEBUG1,
					(errmsg("failover request from %d nodes with ID:%d is expired", failoverObj->request_count, failoverObj->failoverID),
						 errdetail("marking the failover object for removal")));
				if (!need_to_resign && failoverObj->reqKind == NODE_DOWN_REQUEST)
				{
					ListCell   *lc;
					/* search the in the requesting node list if we are also the ones
					 * who think the failover must have been done
					 */
					foreach(lc, failoverObj->requestingNodes)
					{
						WatchdogNode *reqWdNode = lfirst(lc);
						if (g_cluster.localNode == reqWdNode)
						{
							/* verify if that node requested by us is now quarantined */
							int	 i;
							for (i = 0; i < failoverObj->nodesCount; i++)
							{
								int node_id = failoverObj->nodeList[i];
								if (node_id != -1)
								{
									if (Req_info->primary_node_id == -1 &&
										BACKEND_INFO(node_id).quarantine == true &&
										BACKEND_INFO(node_id).role == ROLE_PRIMARY)
									{
										ereport(LOG,
												(errmsg("We are not able to build consensus for our primary node failover request, got %d votes only for failover request ID:%d", failoverObj->request_count, failoverObj->failoverID),
												 errdetail("resigning from the coordinator")));
										need_to_resign = true;
									}
								}
							}
						}
					}
				}
			}
		}
	}

	/* delete the failover objects */
	foreach(lc, failovers_to_del)
	{
		WDFailoverObject *failoverObj = lfirst(lc);

		remove_failover_object(failoverObj);
	}
	list_free(failovers_to_del);
	if (need_to_resign)
	{
		/* lower my wd_priority for moment */
		g_cluster.localNode->wd_priority = -1;
		send_cluster_service_message(NULL, NULL, CLUSTER_IAM_RESIGNING_FROM_LEADER);
		set_state(WD_JOINING);
	}
}

static bool
does_int_array_contains_value(int *intArray, int count, int value)
{
	int			i;

	for (i = 0; i < count; i++)
	{
		if (intArray[i] == value)
			return true;
	}
	return false;
}

static WDFailoverObject * get_failover_object(POOL_REQUEST_KIND reqKind, int nodesCount, int *nodeList)
{
	ListCell   *lc;

	foreach(lc, g_cluster.wdCurrentFailovers)
	{
		WDFailoverObject *failoverObj = lfirst(lc);

		if (failoverObj)
		{
			if (failoverObj->reqKind == reqKind && failoverObj->nodesCount == nodesCount)
			{
				bool		equal = true;
				int			i;

				for (i = 0; i < nodesCount; i++)
				{
					if (does_int_array_contains_value(nodeList, nodesCount, failoverObj->nodeList[i]) == false)
					{
						equal = false;
						break;
					}
				}
				if (equal)
					return failoverObj;
			}
		}
	}
	return NULL;
}

static void
process_remote_failover_command_on_coordinator(WatchdogNode * wdNode, WDPacketData * pkt)
{
	if (get_local_node_state() != WD_COORDINATOR)
	{
		/* only lock holder can resign itself */
		reply_with_minimal_message(wdNode, WD_ERROR_MESSAGE, pkt);
	}
	else
	{
		IPC_CMD_PROCESS_RES res;
		WDCommandData *ipcCommand = create_command_object(pkt->len);

		ipcCommand->sourcePacket.type = pkt->type;
		ipcCommand->sourcePacket.len = pkt->len;
		ipcCommand->sourcePacket.command_id = pkt->command_id;

		if (pkt->len > 0)
			memcpy(ipcCommand->sourcePacket.data, pkt->data, pkt->len);

		ipcCommand->commandSource = COMMAND_SOURCE_REMOTE;
		ipcCommand->sourceWdNode = wdNode;
		gettimeofday(&ipcCommand->commandTime, NULL);

		ereport(LOG,
				(errmsg("watchdog received the failover command from remote pgpool-II node \"%s\"", wdNode->nodeName)));

		res = process_failover_command_on_coordinator(ipcCommand);
		if (res == IPC_CMD_PROCESSING)
		{
			MemoryContext oldCxt = MemoryContextSwitchTo(TopMemoryContext);

			g_cluster.ipc_commands = lappend(g_cluster.ipc_commands, ipcCommand);
			MemoryContextSwitchTo(oldCxt);
			ereport(LOG,
					(errmsg("failover command from remote pgpool-II node \"%s\" is still processing", wdNode->nodeName),
					 errdetail("waiting for results...")));
		}
		else
		{
			cleanUpIPCCommand(ipcCommand);
		}
	}
}

static bool
reply_to_failover_command(WDCommandData * ipcCommand, WDFailoverCMDResults cmdResult, unsigned int failoverID)
{
	bool		ret = false;
	JsonNode   *jNode = jw_create_with_object(true);

	jw_put_int(jNode, WD_FAILOVER_RESULT_KEY, cmdResult);
	jw_put_int(jNode, WD_FAILOVER_ID_KEY, failoverID);
	/* create the packet */
	jw_end_element(jNode);
	jw_finish_document(jNode);

	ereport(DEBUG2,
			(errmsg("replying to failover command with failover ID: %d", failoverID),
			 errdetail("%.*s", jw_get_json_length(jNode), jw_get_json_string(jNode))));

	if (ipcCommand->commandSource == COMMAND_SOURCE_IPC)
	{
		ret = write_ipc_command_with_result_data(ipcCommand, WD_IPC_CMD_RESULT_OK,
												 jw_get_json_string(jNode), jw_get_json_length(jNode) + 1);
	}
	else if (ipcCommand->commandSource == COMMAND_SOURCE_REMOTE)
	{
		reply_with_message(ipcCommand->sourceWdNode, WD_CMD_REPLY_IN_DATA,
						   jw_get_json_string(jNode), jw_get_json_length(jNode) + 1,
						   &ipcCommand->sourcePacket);
	}
	jw_destroy(jNode);
	return ret;
}

/*
 * This function process the failover command and decides
 * about the execution of failover command.
 */

static WDFailoverCMDResults compute_failover_consensus(POOL_REQUEST_KIND reqKind, int *node_id_list, int node_count, unsigned char *flags, WatchdogNode * wdNode)
{
#ifndef NODE_UP_REQUIRE_CONSENSUS
	if (reqKind == NODE_UP_REQUEST)
		return FAILOVER_RES_PROCEED;
#endif
#ifndef NODE_DOWN_REQUIRE_CONSENSUS
	if (reqKind == NODE_DOWN_REQUEST)
		return FAILOVER_RES_PROCEED;
#endif
#ifndef NODE_PROMOTE_REQUIRE_CONSENSUS
	if (reqKind == PROMOTE_NODE_REQUEST)
		return FAILOVER_RES_PROCEED;
#endif

	if (pool_config->failover_when_quorum_exists == false)
	{
		/* No need for any calculation, We do not need a quorum for failover */
		ereport(LOG, (
					  errmsg("we do not need quorum to hold to proceed with failover"),
					  errdetail("proceeding with the failover"),
					  errhint("failover_when_quorum_exists is set to false")));

		return FAILOVER_RES_PROCEED;
	}
	if (*flags & REQ_DETAIL_CONFIRMED)
	{
		/* Check the request flags, If it asks to bypass the quorum status */
		ereport(LOG, (
					  errmsg("The failover request does not need quorum to hold"),
					  errdetail("proceeding with the failover"),
					  errhint("REQ_DETAIL_CONFIRMED")));
		return FAILOVER_RES_PROCEED;
	}
	update_quorum_status();
	if (g_cluster.quorum_status < 0)
	{
		/* quorum is must and it is not present at the moment */
		ereport(LOG, (
					  errmsg("failover requires the quorum to hold, which is not present at the moment"),
					  errdetail("Rejecting the failover request")));
		return FAILOVER_RES_NO_QUORUM;
	}

	/*
	 * So we reached here means quorum is present Now come to difficult part of
	 * ensuring the consensus
	 */
	if (pool_config->failover_require_consensus == true)
	{
		/* Record the failover. */
		bool		duplicate = false;
		WDFailoverObject *failoverObj = add_failover(reqKind, node_id_list, node_count, wdNode, *flags, &duplicate);

		if (failoverObj->request_count < get_minimum_votes_to_resolve_consensus())
		{
			ereport(LOG, (
						  errmsg("failover requires the majority vote, waiting for consensus"),
						  errdetail("failover request noted")));
			if (duplicate && !pool_config->allow_multiple_failover_requests_from_node)
				return FAILOVER_RES_CONSENSUS_MAY_FAIL;
			else
				return FAILOVER_RES_BUILDING_CONSENSUS;
		}
		else
		{
			/* We have received enough votes for this failover */
			ereport(LOG, (
						  errmsg("we have got the consensus to perform the failover"),
						  errdetail("%d node(s) voted in the favor", failoverObj->request_count)));
			/* restore the flag value to the one from the first call */
			*flags = failoverObj->reqFlags;
			/* remove this object, It is no longer needed */
			remove_failover_object(failoverObj);
			return FAILOVER_RES_PROCEED;
		}
	}
	else
	{
		ereport(LOG, (
					  errmsg("we do not require majority votes to proceed with failover"),
					  errdetail("proceeding with the failover"),
					  errhint("failover_require_consensus is set to false")));
	}
	return FAILOVER_RES_PROCEED;
}

static WDFailoverObject * add_failover(POOL_REQUEST_KIND reqKind, int *node_id_list, int node_count, WatchdogNode * wdNode,
									   unsigned char flags, bool *duplicate)
{
	MemoryContext oldCxt;

	/* Find the failover */
	WDFailoverObject *failoverObj = get_failover_object(reqKind, node_count, node_id_list);

	*duplicate = false;
	if (failoverObj)
	{
		ListCell   *lc;

		/* search the node if it is a duplicate request */
		foreach(lc, failoverObj->requestingNodes)
		{
			WatchdogNode *reqWdNode = lfirst(lc);

			if (wdNode == reqWdNode)
			{
				*duplicate = true;
				/* The failover request is duplicate */
				if (pool_config->allow_multiple_failover_requests_from_node)
				{
					failoverObj->request_count++;
					ereport(LOG, (
								  errmsg("duplicate failover request from \"%s\" node", wdNode->nodeName),
								  errdetail("Pgpool-II can send multiple failover requests for same node"),
								  errhint("allow_multiple_failover_requests_from_node is enabled")));
				}
				else
				{
					ereport(LOG, (
								  errmsg("Duplicate failover request from \"%s\" node", wdNode->nodeName),
								  errdetail("request ignored")));
				}
				return failoverObj;
			}
		}
	}
	else
	{
		oldCxt = MemoryContextSwitchTo(TopMemoryContext);
		failoverObj = palloc0(sizeof(WDFailoverObject));
		failoverObj->reqKind = reqKind;
		failoverObj->requestingNodes = NULL;
		failoverObj->nodesCount = node_count;
		failoverObj->reqFlags = flags;
		failoverObj->request_count = 0;
		if (node_count > 0)
		{
			failoverObj->nodeList = palloc(sizeof(int) * node_count);
			memcpy(failoverObj->nodeList, node_id_list, sizeof(int) * node_count);
		}
		failoverObj->failoverID = get_next_commandID();
		gettimeofday(&failoverObj->startTime, NULL);
		g_cluster.wdCurrentFailovers = lappend(g_cluster.wdCurrentFailovers, failoverObj);
		MemoryContextSwitchTo(oldCxt);
	}

	failoverObj->request_count++;
	oldCxt = MemoryContextSwitchTo(TopMemoryContext);
	failoverObj->requestingNodes = lappend(failoverObj->requestingNodes, wdNode);
	MemoryContextSwitchTo(oldCxt);
	return failoverObj;
}

/*
 * The function processes all failover commands on leader node
 */
static IPC_CMD_PROCESS_RES process_failover_command_on_coordinator(WDCommandData * ipcCommand)
{
	char	   *func_name;
	int			node_count = 0;
	int		   *node_id_list = NULL;
	bool		ret = false;
	unsigned char flags;
	POOL_REQUEST_KIND reqKind;
	WDFailoverCMDResults res;

	if (get_local_node_state() != WD_COORDINATOR)
		return IPC_CMD_ERROR;	/* should never happen */

	ret = parse_wd_node_function_json(ipcCommand->sourcePacket.data, ipcCommand->sourcePacket.len,
									  &func_name, &node_id_list, &node_count, &flags);
	if (ret == false)
	{
		ereport(LOG, (
					  errmsg("failed to process failover command"),
					  errdetail("unable to parse the command data")));
		reply_to_failover_command(ipcCommand, FAILOVER_RES_INVALID_FUNCTION, 0);
		return IPC_CMD_COMPLETE;
	}

	if (strcasecmp(WD_FUNCTION_FAILBACK_REQUEST, func_name) == 0)
		reqKind = NODE_UP_REQUEST;
	else if (strcasecmp(WD_FUNCTION_DEGENERATE_REQUEST, func_name) == 0)
		reqKind = NODE_DOWN_REQUEST;
	else if (strcasecmp(WD_FUNCTION_PROMOTE_REQUEST, func_name) == 0)
		reqKind = PROMOTE_NODE_REQUEST;
	else
	{
		reply_to_failover_command(ipcCommand, FAILOVER_RES_INVALID_FUNCTION, 0);
		return IPC_CMD_COMPLETE;
	}

	ereport(LOG,
			(errmsg("watchdog is processing the failover command [%s] received from %s",
					func_name,
					ipcCommand->commandSource == COMMAND_SOURCE_IPC ?
					"local pgpool-II on IPC interface" : ipcCommand->sourceWdNode->nodeName)));

	res = compute_failover_consensus(reqKind, node_id_list, node_count, &flags, ipcCommand->sourceWdNode);

	if (res == FAILOVER_RES_PROCEED)
	{
		/*
		 * We are allowed to proceed with the failover, now if the command was
		 * originated by the remote node, Kick the failover function on the
		 * Pgpool-II main process and inform the remote caller to wait for
		 * sync
		 */
		if (ipcCommand->commandSource == COMMAND_SOURCE_REMOTE)
		{
			/*
			 * Set the flag indicating the failover request is originated by
			 * watchdog
			 */
			flags |= REQ_DETAIL_WATCHDOG;

			if (reqKind == NODE_DOWN_REQUEST)
				ret = degenerate_backend_set(node_id_list, node_count, flags);
			else if (reqKind == NODE_UP_REQUEST)
				ret = send_failback_request(node_id_list[0], false, flags);
			else if (reqKind == PROMOTE_NODE_REQUEST)
				ret = promote_backend(node_id_list[0], flags);

			if (ret == true)
				reply_to_failover_command(ipcCommand, FAILOVER_RES_WILL_BE_DONE, 0);
			else
				reply_to_failover_command(ipcCommand, FAILOVER_RES_ERROR, 0);
		}
		else
		{
			/*
			 * It was the request from the local node, Just reply the caller
			 * to get on with the failover
			 */
			reply_to_failover_command(ipcCommand, FAILOVER_RES_PROCEED, 0);
		}
		return IPC_CMD_COMPLETE;
	}
	else if (res == FAILOVER_RES_NO_QUORUM)
	{
		ereport(LOG,
				(errmsg("failover command [%s] request from pgpool-II node \"%s\" is rejected because the watchdog cluster does not hold the quorum",
						func_name,
						ipcCommand->sourceWdNode->nodeName)));
	}
	else if (res == FAILOVER_RES_BUILDING_CONSENSUS)
	{
		ereport(LOG,
				(errmsg("failover command [%s] request from pgpool-II node \"%s\" is queued, waiting for the confirmation from other nodes",
						func_name,
						ipcCommand->sourceWdNode->nodeName)));

		/*
		 * Ask all the nodes to re-send the failover request for the
		 * quarantined nodes.
		 */
		send_message_of_type(NULL, WD_FAILOVER_WAITING_FOR_CONSENSUS, NULL);

		/*
		 * Also if the command was originated by remote node, check local
		 * quarantine space as-well
		 */
		if (ipcCommand->commandSource == COMMAND_SOURCE_REMOTE)
			register_inform_quarantine_nodes_req();
	}

	reply_to_failover_command(ipcCommand, res, 0);
	return IPC_CMD_COMPLETE;
}

static IPC_CMD_PROCESS_RES process_IPC_failover_command(WDCommandData * ipcCommand)
{
	if (is_local_node_true_leader())
	{
		ereport(LOG,
				(errmsg("watchdog received the failover command from local pgpool-II on IPC interface")));
		return process_failover_command_on_coordinator(ipcCommand);
	}
	else if (get_local_node_state() == WD_STANDBY)
	{
		/* I am a standby node, Just forward the request to coordinator */

		wd_packet_shallow_copy(&ipcCommand->sourcePacket, &ipcCommand->commandPacket);
		set_next_commandID_in_message(&ipcCommand->commandPacket);

		ipcCommand->sendToNode = WD_LEADER_NODE;	/* send the command to
													 * leader node */
		if (send_command_packet_to_remote_nodes(ipcCommand, true) <= 0)
		{
			ereport(LOG,
					(errmsg("unable to process the failover command request received on IPC interface"),
					 errdetail("failed to forward the request to the leader watchdog node \"%s\"", WD_LEADER_NODE->nodeName)));
			return IPC_CMD_ERROR;
		}
		else
		{
			/*
			 * we need to wait for the result
			 */
			ereport(LOG,
					(errmsg("failover request from local pgpool-II node received on IPC interface is forwarded to leader watchdog node \"%s\"",
							WD_LEADER_NODE->nodeName),
					 errdetail("waiting for the reply...")));
			return IPC_CMD_PROCESSING;
		}
	}
	else
	{
		/* we are not in stable state at the moment */
		ereport(LOG,
				(errmsg("unable to process the failover request received on IPC interface"),
				 errdetail("this watchdog node has not joined the cluster yet"),
				 errhint("try again in few seconds")));
	}
	return IPC_CMD_ERROR;
}

static IPC_CMD_PROCESS_RES process_IPC_online_recovery(WDCommandData * ipcCommand)
{
	if (get_local_node_state() == WD_STANDBY ||
		get_local_node_state() == WD_COORDINATOR)
	{
		/* save the hassel if I am the only alive node */
		if (get_cluster_node_count() == 0)
			return IPC_CMD_OK;

		wd_packet_shallow_copy(&ipcCommand->sourcePacket, &ipcCommand->commandPacket);
		set_next_commandID_in_message(&ipcCommand->commandPacket);

		ipcCommand->sendToNode = NULL;	/* command needs to be sent to all
										 * nodes */
		if (send_command_packet_to_remote_nodes(ipcCommand, true) <= 0)
		{
			ereport(LOG,
					(errmsg("unable to process the online recovery request received on IPC interface"),
					 errdetail("failed to forward the request to the leader watchdog node \"%s\"", WD_LEADER_NODE->nodeName)));
			return IPC_CMD_ERROR;
		}
		ereport(LOG,
				(errmsg("online recovery request from local pgpool-II node received on IPC interface is forwarded to leader watchdog node \"%s\"",
						WD_LEADER_NODE->nodeName),
				 errdetail("waiting for the reply...")));

		return IPC_CMD_PROCESSING;
	}
	/* we are not in any stable state at the moment */

	ereport(LOG,
			(errmsg("unable to process the online recovery request received on IPC interface"),
			 errdetail("this watchdog node has not joined the cluster yet"),
			 errhint("try again in few seconds")));

	return IPC_CMD_TRY_AGAIN;
}

static IPC_CMD_PROCESS_RES process_IPC_data_request_from_leader(WDCommandData * ipcCommand)
{
	/*
	 * if cluster or myself is not in stable state just return cluster in
	 * transaction
	 */
	ereport(LOG,
			(errmsg("received the get data request from local pgpool-II on IPC interface")));

	if (get_local_node_state() == WD_STANDBY)
	{
		/*
		 * set the command id in the IPC packet before forwarding it on the
		 * watchdog socket
		 */
		wd_packet_shallow_copy(&ipcCommand->sourcePacket, &ipcCommand->commandPacket);
		set_next_commandID_in_message(&ipcCommand->commandPacket);

		ipcCommand->sendToNode = WD_LEADER_NODE;
		if (send_command_packet_to_remote_nodes(ipcCommand, true) <= 0)
		{
			ereport(LOG,
					(errmsg("unable to process the get data request received on IPC interface"),
					 errdetail("failed to forward the request to the leader watchdog node \"%s\"", WD_LEADER_NODE->nodeName)));
			return IPC_CMD_ERROR;
		}
		else
		{
			/*
			 * we need to wait for the result
			 */
			ereport(LOG,
					(errmsg("get data request from local pgpool-II node received on IPC interface is forwarded to leader watchdog node \"%s\"",
							WD_LEADER_NODE->nodeName),
					 errdetail("waiting for the reply...")));

			return IPC_CMD_PROCESSING;
		}
	}
	else if (is_local_node_true_leader())
	{
		/*
		 * This node is itself a leader node, So send the empty result with OK
		 * tag
		 */
		return IPC_CMD_OK;
	}

	/* we are not in any stable state at the moment */
	ereport(LOG,
			(errmsg("unable to process the get data request received on IPC interface"),
			 errdetail("this watchdog node has not joined the cluster yet"),
			 errhint("try again in few seconds")));

	return IPC_CMD_TRY_AGAIN;
}

static IPC_CMD_PROCESS_RES process_IPC_failover_indication(WDCommandData * ipcCommand)
{
	WDFailoverCMDResults res = FAILOVER_RES_NOT_ALLOWED;

	/*
	 * if cluster or myself is not in stable state just return cluster in
	 * transaction
	 */
	ereport(LOG,
			(errmsg("received the failover indication from Pgpool-II on IPC interface")));

	if (get_local_node_state() == WD_COORDINATOR)
	{
		int			failoverState = -1;

		if (ipcCommand->sourcePacket.data == NULL || ipcCommand->sourcePacket.len <= 0)
		{
			ereport(LOG,
					(errmsg("watchdog unable to process failover indication"),
					 errdetail("invalid command packet")));
			res = FAILOVER_RES_INVALID_FUNCTION;
		}
		else
		{
			json_value *root = json_parse(ipcCommand->sourcePacket.data, ipcCommand->sourcePacket.len);

			if (root && root->type == json_object)
			{
				if (json_get_int_value_for_key(root, "FailoverFuncState", &failoverState))
				{
					ereport(LOG,
							(errmsg("unable to process failover indication"),
							 errdetail("failed to get failover state from json data in command packet")));
					res = FAILOVER_RES_INVALID_FUNCTION;
				}
			}
			else
			{
				ereport(LOG,
						(errmsg("unable to process failover indication"),
						 errdetail("invalid JSON data in command packet")));
				res = FAILOVER_RES_INVALID_FUNCTION;
			}
			if (root)
				json_value_free(root);
		}

		if (failoverState < 0)
		{
			ereport(LOG,
					(errmsg("unable to process failover indication"),
					 errdetail("invalid JSON data in command packet")));
			res = FAILOVER_RES_INVALID_FUNCTION;
		}
		else if (failoverState == 0)	/* start */
		{
			res = failover_start_indication(ipcCommand);
		}
		else					/* end */
		{
			res = failover_end_indication(ipcCommand);
		}
	}
	else
	{
		ereport(LOG,
				(errmsg("received the failover indication from Pgpool-II on IPC interface, but only leader can do failover")));
	}
	reply_to_failover_command(ipcCommand, res, 0);

	return IPC_CMD_COMPLETE;
}


/* Failover start basically does nothing fancy, It just sets the failover_in_progress
 * flag and inform all nodes that the failover is in progress.
 *
 * only the local node that is a leader can start the failover.
 */
static WDFailoverCMDResults
failover_start_indication(WDCommandData * ipcCommand)
{
	ereport(LOG,
			(errmsg("watchdog is informed of failover start by the main process")));

	/* only coordinator(leader) node is allowed to process failover */
	if (get_local_node_state() == WD_COORDINATOR)
	{
		/* inform to all nodes about failover start */
		send_message_of_type(NULL, WD_FAILOVER_START, NULL);
		return FAILOVER_RES_PROCEED;
	}
	else if (get_local_node_state() == WD_STANDBY)
	{
		/* The node might be performing the local quarantine operation */
		ereport(DEBUG1,
				(errmsg("main process is starting the local quarantine operation")));
		return FAILOVER_RES_PROCEED;
	}
	else
	{
		ereport(LOG,
				(errmsg("failed to process failover start request, I am not in stable state")));
	}
	return FAILOVER_RES_TRANSITION;
}

static WDFailoverCMDResults
failover_end_indication(WDCommandData * ipcCommand)
{
	ereport(LOG,
			(errmsg("watchdog is informed of failover end by the main process")));

	/* only coordinator(leader) node is allowed to process failover */
	if (get_local_node_state() == WD_COORDINATOR)
	{
		send_message_of_type(NULL, WD_FAILOVER_END, NULL);
		return FAILOVER_RES_PROCEED;
	}
	else if (get_local_node_state() == WD_STANDBY)
	{
		/* The node might be performing the local quarantine operation */
		ereport(DEBUG1,
				(errmsg("main process is ending the local quarantine operation")));
		return FAILOVER_RES_PROCEED;
	}
	else
	{
		ereport(LOG,
				(errmsg("failed to process failover start request, I am not in stable state")));
	}
	return FAILOVER_RES_TRANSITION;
}

static WatchdogNode * parse_node_info_message(WDPacketData * pkt, char **authkey)
{
	if (pkt == NULL || (pkt->type != WD_ADD_NODE_MESSAGE && pkt->type != WD_INFO_MESSAGE))
		return NULL;
	if (pkt->data == NULL || pkt->len <= 0)
		return NULL;
	return get_watchdog_node_from_json(pkt->data, pkt->len, authkey);
}

static WDPacketData * read_packet(SocketConnection * conn)
{
	return read_packet_of_type(conn, WD_NO_MESSAGE);
}

static WDPacketData * read_packet_of_type(SocketConnection * conn, char ensure_type)
{
	char		type;
	int			len;
	unsigned int cmd_id;
	char	   *buf;
	WDPacketData *pkt = NULL;
	int			ret;

	if (is_socket_connection_connected(conn) == false)
	{
		ereport(LOG,
				(errmsg("error reading from socket connection,socket is not connected")));
		return NULL;
	}

	ret = socket_read(conn->sock, &type, sizeof(char), 1);
	if (ret != sizeof(char))
	{
		close_socket_connection(conn);
		return NULL;
	}

	ereport(DEBUG1,
			(errmsg("received watchdog packet type:%c", type)));

	if (ensure_type != WD_NO_MESSAGE && ensure_type != type)
	{
		/* The packet type is not what we want. */
		ereport(DEBUG1,
				(errmsg("invalid packet type. expecting %c while received %c", ensure_type, type)));
		close_socket_connection(conn);
		return NULL;
	}

	ret = socket_read(conn->sock, &cmd_id, sizeof(int), 1);
	if (ret != sizeof(int))
	{
		close_socket_connection(conn);
		return NULL;
	}
	cmd_id = ntohl(cmd_id);

	ereport(DEBUG2,
			(errmsg("received packet with command id %d from watchdog node ", cmd_id)));

	ret = socket_read(conn->sock, &len, sizeof(int), 1);
	if (ret != sizeof(int))
	{
		close_socket_connection(conn);
		return NULL;
	}

	len = ntohl(len);

	ereport(DEBUG1,
			(errmsg("reading packet type %c of length %d", type, len)));

	pkt = get_empty_packet();
	set_message_type(pkt, type);
	set_message_commandID(pkt, cmd_id);

	buf = palloc(len);

	ret = socket_read(conn->sock, buf, len, 1);
	if (ret != len)
	{
		close_socket_connection(conn);
		free_packet(pkt);
		pfree(buf);
		return NULL;
	}
	set_message_data(pkt, buf, len);
	return pkt;
}



static void
wd_child_exit(int exit_signo)
{
	sigset_t	mask;

	sigemptyset(&mask);
	sigaddset(&mask, SIGTERM);
	sigaddset(&mask, SIGINT);
	sigaddset(&mask, SIGQUIT);
	sigprocmask(SIG_BLOCK, &mask, NULL);
	exit(0);
}

static void
wd_child_signal_handler(void)
{
	pid_t		pid;
	int			status;

	ereport(DEBUG1,
			(errmsg("watchdog process signal handler")));

	/* clear SIGCHLD request */
	sigchld_request = 0;

	while ((pid = pool_waitpid(&status)) > 0)
	{
		char	   *exiting_process_name;

		if (g_cluster.de_escalation_pid == pid)
		{
			exiting_process_name = "de-escalation";
			g_cluster.de_escalation_pid = 0;
		}
		else if (g_cluster.escalation_pid == pid)
		{
			exiting_process_name = "escalation";
			g_cluster.escalation_pid = 0;
		}
		else
			exiting_process_name = "unknown";

		if (WIFEXITED(status))
		{
			if (WEXITSTATUS(status) == POOL_EXIT_FATAL)
				ereport(LOG,
						(errmsg("watchdog %s process with pid: %d exit with FATAL ERROR.", exiting_process_name, pid)));
			else if (WEXITSTATUS(status) == POOL_EXIT_NO_RESTART)
				ereport(LOG,
						(errmsg("watchdog %s process with pid: %d exit with SUCCESS.", exiting_process_name, pid)));
		}
		else if (WIFSIGNALED(status))
		{
			/* Child terminated by segmentation fault. Report it */
			if (WTERMSIG(status) == SIGSEGV)
				ereport(WARNING,
						(errmsg("watchdog %s process with pid: %d was terminated by segmentation fault", exiting_process_name, pid)));
			else
				ereport(LOG,
						(errmsg("watchdog %s process with pid: %d exits with status %d by signal %d", exiting_process_name, pid, status, WTERMSIG(status))));
		}
		else
			ereport(LOG,
					(errmsg("watchdog %s process with pid: %d exits with status %d", exiting_process_name, pid, status)));
	}
}

/* Function invoked when watchdog process is about to exit */
static void
wd_system_will_go_down(int code, Datum arg)
{
	int			i;

	ereport(LOG,
			(errmsg("Watchdog is shutting down")));

	send_cluster_command(NULL, WD_INFORM_I_AM_GOING_DOWN, 0);

	if (get_local_node_state() == WD_COORDINATOR)
		resign_from_escalated_node();
	/* close server socket */
	close_socket_connection(&g_cluster.localNode->server_socket);
	/* close all node sockets */
	for (i = 0; i < g_cluster.remoteNodeCount; i++)
	{
		WatchdogNode *wdNode = &(g_cluster.remoteNodes[i]);

		close_socket_connection(&wdNode->client_socket);
		close_socket_connection(&wdNode->server_socket);
	}
	/* close network monitoring socket */
	if (g_cluster.network_monitor_sock > 0)
		close(g_cluster.network_monitor_sock);
	/* wait for sub-processes to exit */
	if (g_cluster.de_escalation_pid > 0 || g_cluster.escalation_pid > 0)
	{
		pid_t		wpid;

		do
		{
			wpid = wait(NULL);
		} while (wpid > 0 || (wpid == -1 && errno == EINTR));
	}
}

static void
close_socket_connection(SocketConnection * conn)
{
	if ((conn->sock > 0 && conn->sock_state == WD_SOCK_CONNECTED)
		|| conn->sock_state == WD_SOCK_WAITING_FOR_CONNECT)
	{
		close(conn->sock);
		conn->sock = -1;
		conn->sock_state = WD_SOCK_CLOSED;
	}
}

static bool
is_socket_connection_connected(SocketConnection * conn)
{
	return (conn->sock > 0 && conn->sock_state == WD_SOCK_CONNECTED);
}


static bool
is_node_reachable(WatchdogNode * wdNode)
{
	if (is_socket_connection_connected(&wdNode->client_socket))
		return true;
	if (is_socket_connection_connected(&wdNode->server_socket))
		return true;
	return false;
}

static bool
is_node_active(WatchdogNode * wdNode)
{
	if (wdNode->state == WD_DEAD || wdNode->state == WD_LOST || wdNode->state == WD_SHUTDOWN)
		return false;
	return true;
}

static bool
is_node_active_and_reachable(WatchdogNode * wdNode)
{
	if (is_node_active(wdNode))
		return is_node_reachable(wdNode);
	return false;
}

static int
accept_incoming_connections(fd_set *rmask, int pending_fds_count)
{
	int			processed_fds = 0;
	int			fd;

	if (FD_ISSET(g_cluster.localNode->server_socket.sock, rmask))
	{
		struct sockaddr_in addr;
		socklen_t	addrlen = sizeof(struct sockaddr_in);

		processed_fds++;
		fd = accept(g_cluster.localNode->server_socket.sock, (struct sockaddr *) &addr, &addrlen);
		if (fd < 0)
		{
			if (errno == EINTR || errno == 0 || errno == EAGAIN || errno == EWOULDBLOCK)
			{
				/* nothing to accept now */
				ereport(DEBUG2,
						(errmsg("Failed to accept incoming watchdog connection, Nothing to accept")));
			}
			/* accept failed */
			ereport(DEBUG1,
					(errmsg("Failed to accept incoming watchdog connection")));
		}
		else
		{
			MemoryContext oldCxt = MemoryContextSwitchTo(TopMemoryContext);
			SocketConnection *conn = palloc(sizeof(SocketConnection));

			conn->sock = fd;
			conn->sock_state = WD_SOCK_CONNECTED;
			gettimeofday(&conn->tv, NULL);
			strncpy(conn->addr, inet_ntoa(addr.sin_addr), sizeof(conn->addr) - 1);
			ereport(LOG,
					(errmsg("new watchdog node connection is received from \"%s:%d\"", inet_ntoa(addr.sin_addr), addr.sin_port)));
			g_cluster.unidentified_socks = lappend(g_cluster.unidentified_socks, conn);
			MemoryContextSwitchTo(oldCxt);
		}
	}

	if (processed_fds >= pending_fds_count)
		return processed_fds;

	if (FD_ISSET(g_cluster.command_server_sock, rmask))
	{
		struct sockaddr addr;
		socklen_t	addrlen = sizeof(struct sockaddr);

		processed_fds++;

		int			fd = accept(g_cluster.command_server_sock, &addr, &addrlen);

		if (fd < 0)
		{
			if (errno == EINTR || errno == 0 || errno == EAGAIN || errno == EWOULDBLOCK)
			{
				/* nothing to accept now */
				ereport(WARNING,
						(errmsg("failed to accept incoming watchdog IPC connection, Nothing to accept")));
			}
			/* accept failed */
			ereport(WARNING,
					(errmsg("failed to accept incoming watchdog IPC connection")));
		}
		else
		{
			MemoryContext oldCxt = MemoryContextSwitchTo(TopMemoryContext);

			ereport(DEBUG1,
					(errmsg("new IPC connection received")));
			g_cluster.ipc_command_socks = lappend_int(g_cluster.ipc_command_socks, fd);
			MemoryContextSwitchTo(oldCxt);
		}
	}

	return processed_fds;
}

static int
update_successful_outgoing_cons(fd_set *wmask, int pending_fds_count)
{
	int			i;
	int			count = 0;

	for (i = 0; i < g_cluster.remoteNodeCount; i++)
	{
		WatchdogNode *wdNode = &(g_cluster.remoteNodes[i]);

		if (wdNode->client_socket.sock > 0 && wdNode->client_socket.sock_state == WD_SOCK_WAITING_FOR_CONNECT)
		{
			if (FD_ISSET(wdNode->client_socket.sock, wmask))
			{
				socklen_t	lon;
				int			valopt;

				lon = sizeof(int);

				gettimeofday(&wdNode->client_socket.tv, NULL);

				if (getsockopt(wdNode->client_socket.sock, SOL_SOCKET, SO_ERROR, (void *) (&valopt), &lon) == 0)
				{
					if (valopt)
					{
						ereport(DEBUG1,
								(errmsg("error in outbound connection to %s:%d", wdNode->hostname, wdNode->wd_port),
								 errdetail("%s", strerror(valopt))));
						close_socket_connection(&wdNode->client_socket);
						wdNode->client_socket.sock_state = WD_SOCK_ERROR;
					}
					else
					{
						wdNode->client_socket.sock_state = WD_SOCK_CONNECTED;
						ereport(LOG,
								(errmsg("new outbound connection to %s:%d ", wdNode->hostname, wdNode->wd_port)));
						/* set socket to blocking again */
						socket_unset_nonblock(wdNode->client_socket.sock);
						watchdog_state_machine(WD_EVENT_NEW_OUTBOUND_CONNECTION, wdNode, NULL, NULL);
					}
				}
				else
				{
					ereport(DEBUG1,
							(errmsg("error in outbound connection to %s:%d ", wdNode->hostname, wdNode->wd_port),
							 errdetail("getsockopt failed with error \"%m\"")));
					close_socket_connection(&wdNode->client_socket);
					wdNode->client_socket.sock_state = WD_SOCK_ERROR;

				}
				count++;
				if (count >= pending_fds_count)
					break;
			}
		}
	}
	return count;
}

static bool
write_packet_to_socket(int sock, WDPacketData * pkt, bool ipcPacket)
{
	int			ret = 0;
	int			command_id,
				len;

	ereport(DEBUG1,
			(errmsg("sending watchdog packet to socket:%d, type:[%c], command ID:%d, data Length:%d", sock, pkt->type,
					pkt->command_id, pkt->len)));

	print_packet_info(pkt, true);

	/* TYPE */
	if (write(sock, &pkt->type, 1) < 1)
	{
		ereport(LOG,
				(errmsg("failed to write watchdog packet to socket"),
				 errdetail("%m")));
		return false;
	}
	if (ipcPacket == false)
	{
		/* IPC packets does not have command ID field */
		command_id = htonl(pkt->command_id);
		if (write(sock, &command_id, 4) < 4)
		{
			ereport(LOG,
					(errmsg("failed to write watchdog packet to socket"),
					 errdetail("%m")));
			return false;
		}
	}
	/* data length */
	len = htonl(pkt->len);
	if (write(sock, &len, 4) < 4)
	{
		ereport(LOG,
				(errmsg("failed to write watchdog packet to socket"),
				 errdetail("%m")));
		return false;
	}
	/* DATA */
	if (pkt->len > 0 && pkt->data)
	{
		int			bytes_send = 0;

		do
		{
			ret = write(sock, pkt->data + bytes_send, (pkt->len - bytes_send));
			if (ret <= 0)
			{
				ereport(LOG,
						(errmsg("failed to write watchdog packet to socket"),
						 errdetail("%m")));
				return false;
			}
			bytes_send += ret;
		} while (bytes_send < pkt->len);
	}
	return true;
}

static void
wd_packet_shallow_copy(WDPacketData * srcPkt, WDPacketData * dstPkt)
{
	dstPkt->command_id = srcPkt->command_id;
	dstPkt->data = srcPkt->data;
	dstPkt->len = srcPkt->len;
	dstPkt->type = srcPkt->type;
}

static void
init_wd_packet(WDPacketData * pkt)
{
	pkt->len = 0;
	pkt->data = NULL;
}

static WDPacketData * get_empty_packet(void)
{
	WDPacketData *pkt = palloc0(sizeof(WDPacketData));

	return pkt;
}

static void
free_packet(WDPacketData * pkt)
{
	if (pkt)
	{
		if (pkt->data)
			pfree(pkt->data);
		pfree(pkt);
	}
}

static void
set_message_type(WDPacketData * pkt, char type)
{
	pkt->type = type;
}

static void
set_message_commandID(WDPacketData * pkt, unsigned int commandID)
{
	pkt->command_id = commandID;
}

static void
set_next_commandID_in_message(WDPacketData * pkt)
{
	set_message_commandID(pkt, get_next_commandID());
}

static void
set_message_data(WDPacketData * pkt, const char *data, int len)
{
	pkt->data = (char *) data;
	pkt->len = len;
}

#define nodeIfNull_str(m,v) node&&strlen(node->m)?node->m:v
#define nodeIfNull_int(m,v) node?node->m:v
#define NotSet "Not_Set"

static bool
add_nodeinfo_to_json(JsonNode * jNode, WatchdogNode * node)
{
	jw_start_object(jNode, "WatchdogNode");

	jw_put_int(jNode, "ID", nodeIfNull_int(pgpool_node_id, -1));
	jw_put_int(jNode, "State", nodeIfNull_int(state, -1));
	jw_put_int(jNode, "Membership", nodeIfNull_int(membership_status, -1));
	jw_put_string(jNode, "MembershipString", node ? wd_cluster_membership_status[node->membership_status] : NotSet);
	jw_put_string(jNode, "NodeName", nodeIfNull_str(nodeName, NotSet));
	jw_put_string(jNode, "HostName", nodeIfNull_str(hostname, NotSet));
	jw_put_string(jNode, "StateName", node ? wd_state_names[node->state] : NotSet);
	jw_put_string(jNode, "DelegateIP", nodeIfNull_str(delegate_ip, NotSet));
	jw_put_int(jNode, "WdPort", nodeIfNull_int(wd_port, 0));
	jw_put_int(jNode, "PgpoolPort", nodeIfNull_int(pgpool_port, 0));
	jw_put_int(jNode, "Priority", nodeIfNull_int(wd_priority, 0));

	jw_end_element(jNode);

	return true;
}

static JsonNode * get_node_list_json(int id)
{
	int			i;
	JsonNode   *jNode = jw_create_with_object(true);

	jw_put_int(jNode, "RemoteNodeCount", g_cluster.remoteNodeCount);
	jw_put_int(jNode, "MemberRemoteNodeCount", g_cluster.memberRemoteNodeCount);
	jw_put_int(jNode, "NodesRequireForQuorum", get_minimum_votes_to_resolve_consensus());
	jw_put_int(jNode, "QuorumStatus", WD_LEADER_NODE ? WD_LEADER_NODE->quorum_status : -2);
	jw_put_int(jNode, "AliveNodeCount", WD_LEADER_NODE ? WD_LEADER_NODE->standby_nodes_count : 0);
	jw_put_int(jNode, "Escalated", g_cluster.localNode->escalated);
	jw_put_string(jNode, "LeaderNodeName", WD_LEADER_NODE ? WD_LEADER_NODE->nodeName : "Not Set");
	jw_put_string(jNode, "LeaderHostName", WD_LEADER_NODE ? WD_LEADER_NODE->hostname : "Not Set");
	if (id < 0)
	{
		jw_put_int(jNode, "NodeCount", g_cluster.remoteNodeCount + 1);

		/* add the array */
		jw_start_array(jNode, "WatchdogNodes");
		/* add the local node info */
		add_nodeinfo_to_json(jNode, g_cluster.localNode);
		/* add all remote nodes */
		for (i = 0; i < g_cluster.remoteNodeCount; i++)
		{
			WatchdogNode *wdNode = &(g_cluster.remoteNodes[i]);

			add_nodeinfo_to_json(jNode, wdNode);
		}
	}
	else
	{
		jw_put_int(jNode, "NodeCount", 1);
		/* add the array */
		jw_start_array(jNode, "WatchdogNodes");

		if (id == g_cluster.localNode->pgpool_node_id)
		{
			/* add the local node info */
			add_nodeinfo_to_json(jNode, g_cluster.localNode);
		}
		else
		{
			/* find from remote nodes */
			WatchdogNode *wdNodeToAdd = NULL;

			for (i = 0; i < g_cluster.remoteNodeCount; i++)
			{
				WatchdogNode *wdNode = &(g_cluster.remoteNodes[i]);

				if (wdNode->pgpool_node_id == id)
				{
					wdNodeToAdd = wdNode;
					break;
				}
			}
			add_nodeinfo_to_json(jNode, wdNodeToAdd);
		}
	}
	jw_finish_document(jNode);
	return jNode;
}

static WDPacketData * get_beacon_message(char type, WDPacketData * replyFor)
{
	WDPacketData *message = get_empty_packet();
	char	   *json_data;

	json_data = get_beacon_message_json(g_cluster.localNode);

	set_message_type(message, type);

	if (replyFor == NULL)
		set_next_commandID_in_message(message);
	else
		set_message_commandID(message, replyFor->command_id);

	set_message_data(message, json_data, strlen(json_data));
	return message;
}

static WDPacketData * get_addnode_message(void)
{
	char		authhash[WD_AUTH_HASH_LEN + 1];
	WDPacketData *message = get_empty_packet();
	bool		include_hash = get_authhash_for_node(g_cluster.localNode, authhash);
	char	   *json_data = get_watchdog_node_info_json(g_cluster.localNode, include_hash ? authhash : NULL);

	set_message_type(message, WD_ADD_NODE_MESSAGE);
	set_next_commandID_in_message(message);
	set_message_data(message, json_data, strlen(json_data));
	return message;
}

static WDPacketData * get_mynode_info_message(WDPacketData * replyFor)
{
	char		authhash[WD_AUTH_HASH_LEN + 1];
	WDPacketData *message = get_empty_packet();
	bool		include_hash = get_authhash_for_node(g_cluster.localNode, authhash);
	char	   *json_data = get_watchdog_node_info_json(g_cluster.localNode, include_hash ? authhash : NULL);

	set_message_type(message, WD_INFO_MESSAGE);
	if (replyFor == NULL)
		set_next_commandID_in_message(message);
	else
		set_message_commandID(message, replyFor->command_id);

	set_message_data(message, json_data, strlen(json_data));
	return message;
}

static WDPacketData * get_minimum_message(char type, WDPacketData * replyFor)
{
	/* TODO it is a waste of space */
	WDPacketData *message = get_empty_packet();

	set_message_type(message, type);
	if (replyFor == NULL)
		set_next_commandID_in_message(message);
	else
		set_message_commandID(message, replyFor->command_id);
	return message;
}

static WDCommandData * get_wd_IPC_command_from_reply(WDPacketData * pkt)
{
	return get_wd_command_from_reply(g_cluster.ipc_commands, pkt);
}
static WDCommandData * get_wd_cluster_command_from_reply(WDPacketData * pkt)
{
	return get_wd_command_from_reply(g_cluster.clusterCommands, pkt);
}

static WDCommandData * get_wd_command_from_reply(List *commands, WDPacketData * pkt)
{
	ListCell   *lc;

	if (commands == NULL)
		return NULL;

	foreach(lc, commands)
	{
		WDCommandData *ipcCommand = lfirst(lc);

		if (ipcCommand)
		{
			if (ipcCommand->commandPacket.command_id == pkt->command_id)
			{
				ereport(DEBUG1,
						(errmsg("packet %c with command ID %d is reply to the command %c", pkt->type, pkt->command_id,
								ipcCommand->commandPacket.type)));
				return ipcCommand;
			}
		}
	}
	return NULL;
}

static WDCommandData * get_wd_IPC_command_from_socket(int sock)
{
	ListCell   *lc;

	foreach(lc, g_cluster.ipc_commands)
	{
		WDCommandData *ipcCommand = lfirst(lc);

		if (ipcCommand)
		{
			if (ipcCommand->commandSource != COMMAND_SOURCE_IPC)
				continue;

			if (ipcCommand->sourceIPCSocket == sock)
				return ipcCommand;
		}
	}
	return NULL;
}


static void
cleanUpIPCCommand(WDCommandData * ipcCommand)
{
	/*
	 * close the socket associated with ipcCommand and remove it from
	 * ipcSocket list
	 */
	if (ipcCommand->commandSource == COMMAND_SOURCE_IPC &&
		ipcCommand->sourceIPCSocket > 0)
	{
		close(ipcCommand->sourceIPCSocket);
		g_cluster.ipc_command_socks = list_delete_int(g_cluster.ipc_command_socks, ipcCommand->sourceIPCSocket);
		ipcCommand->sourceIPCSocket = -1;
	}
	/* Now remove the ipcCommand instance from the command list */
	g_cluster.ipc_commands = list_delete_ptr(g_cluster.ipc_commands, ipcCommand);

	/*
	 * Finally the memory part As everything of IPCCommand live inside its own
	 * memory context. Delete the MemoryContext and we are good
	 */
	MemoryContextDelete(ipcCommand->memoryContext);
}

static WDPacketData * process_data_request(WatchdogNode * wdNode, WDPacketData * pkt)
{
	char	   *request_type;
	char	   *data = NULL;
	WDPacketData *replyPkt = NULL;

	if (pkt->data == NULL || pkt->len <= 0)
	{
		ereport(WARNING,
				(errmsg("invalid data request packet from watchdog node \"%s\"", wdNode->nodeName),
				 errdetail("no data found in the packet")));

		replyPkt = get_minimum_message(WD_ERROR_MESSAGE, pkt);
		return replyPkt;
	}

	if (!parse_data_request_json(pkt->data, pkt->len, &request_type))
	{
		ereport(WARNING,
				(errmsg("invalid data request packet from watchdog node \"%s\"", wdNode->nodeName),
				 errdetail("no data found in the packet")));

		replyPkt = get_minimum_message(WD_ERROR_MESSAGE, pkt);
		return replyPkt;
	}

	if (strcasecmp(request_type, WD_DATE_REQ_PG_BACKEND_DATA) == 0)
	{
		data = get_backend_node_status_json(g_cluster.localNode);
	}

	if (data)
	{
		replyPkt = get_empty_packet();
		set_message_type(replyPkt, WD_DATA_MESSAGE);
		set_message_commandID(replyPkt, pkt->command_id);
		set_message_data(replyPkt, data, strlen(data));
	}
	else
	{
		replyPkt = get_minimum_message(WD_ERROR_MESSAGE, pkt);
	}

	return replyPkt;
}

static void
cluster_service_message_processor(WatchdogNode * wdNode, WDPacketData * pkt)
{
	if (pkt->type != WD_CLUSTER_SERVICE_MESSAGE)
		return;

	if (pkt->len != 1 || pkt->data == NULL)
	{
		ereport(LOG,
				(errmsg("node \"%s\" sent an invalid cluster service message", wdNode->nodeName)));
		return;
	}

	switch (pkt->data[0])
	{
		case CLUSTER_IAM_TRUE_LEADER:
			{
				/*
				 * The cluster was in split-brain and remote node thinks it is
				 * the worthy leader
				 */
				if (get_local_node_state() == WD_COORDINATOR)
				{
					ereport(LOG,
							(errmsg("remote node \"%s\" decided it is the true leader", wdNode->nodeName),
							 errdetail("re-initializing the local watchdog cluster state because of split-brain")));

					send_cluster_service_message(NULL, pkt, CLUSTER_IAM_RESIGNING_FROM_LEADER);
					set_state(WD_JOINING);
				}
				else if (WD_LEADER_NODE != NULL && WD_LEADER_NODE != wdNode)
				{
					ereport(LOG,
							(errmsg("remote node \"%s\" thinks it is a leader/coordinator and I am causing the split-brain," \
									" but as per our record \"%s\" is the cluster leader/coordinator",
									wdNode->nodeName,
									WD_LEADER_NODE->nodeName),
							 errdetail("restarting the cluster")));
					send_cluster_service_message(NULL, pkt, CLUSTER_NEEDS_ELECTION);
					set_state(WD_JOINING);
				}
			}
			break;

		case CLUSTER_IAM_RESIGNING_FROM_LEADER:
			{
				if (WD_LEADER_NODE == wdNode)
				{
					ereport(LOG,
							(errmsg("leader/coordinator node \"%s\" decided to resigning from leader, probably because of split-brain",
									wdNode->nodeName),
							 errdetail("re-initializing the local watchdog cluster state")));

					set_state(WD_JOINING);
				}
				else
				{
					ereport(LOG,
							(errmsg("leader/coordinator node \"%s\" decided to resign from leader, probably because of split-brain",
									wdNode->nodeName),
							 errdetail("It was not our coordinator/leader anyway. ignoring the message")));
				}
			}
			break;

		case CLUSTER_IN_SPLIT_BRAIN:
			{
				try_connecting_with_all_unreachable_nodes();
				if (get_local_node_state() == WD_COORDINATOR)
				{
					ereport(LOG,
							(errmsg("remote node \"%s\" detected the cluster is in split-brain", wdNode->nodeName),
							 errdetail("broadcasting the beacon message")));
					send_message_of_type(NULL, WD_IAM_COORDINATOR_MESSAGE, NULL);
				}
			}
			break;

		case CLUSTER_NEEDS_ELECTION:
			{
				ereport(LOG,
						(errmsg("remote node \"%s\" detected the problem and asking us to rejoin the cluster", wdNode->nodeName)));

				set_state(WD_JOINING);
			}
			break;

		case CLUSTER_IAM_NOT_TRUE_LEADER:
			{
				if (WD_LEADER_NODE == wdNode)
				{
					ereport(LOG,
							(errmsg("leader/coordinator node \"%s\" decided it was not true leader, probably because of split-brain", wdNode->nodeName),
							 errdetail("re-initializing the local watchdog cluster state")));

					set_state(WD_JOINING);
				}
				else if (get_local_node_state() == WD_COORDINATOR)
				{
					ereport(LOG,
							(errmsg("node \"%s\" was also thinking it was a leader/coordinator and decided to resign", wdNode->nodeName),
							 errdetail("cluster is recovering from split-brain")));
				}
				else
				{
					ereport(LOG,
							(errmsg("leader/coordinator node \"%s\" decided to resign from leader, probably because of split-brain",
									wdNode->nodeName),
							 errdetail("but it was not our coordinator/leader anyway. ignoring the message")));
				}
			}
			break;

		case CLUSTER_NODE_REQUIRE_TO_RELOAD:
		{
			watchdog_state_machine(WD_EVENT_WD_STATE_REQUIRE_RELOAD, NULL, NULL, NULL);
		}
			break;

		case CLUSTER_NODE_APPEARING_LOST:
		{
			ereport(LOG,
				(errmsg("remote node \"%s\" is reporting that it has lost us",
							wdNode->nodeName)));
			wdNode->has_lost_us = true;
			watchdog_state_machine(WD_EVENT_I_AM_APPEARING_LOST, wdNode, NULL, NULL);
		}
			break;

		case CLUSTER_NODE_APPEARING_FOUND:
		{
			ereport(LOG,
				(errmsg("remote node \"%s\" is reporting that it has found us again",
							wdNode->nodeName)));
			wdNode->has_lost_us = false;
			watchdog_state_machine(WD_EVENT_I_AM_APPEARING_FOUND, wdNode, NULL, NULL);
		}
			break;

		case CLUSTER_NODE_INVALID_VERSION:
			{
				/*
				 * this should never happen means something is seriously wrong
				 */
				ereport(FATAL,
						(return_code(POOL_EXIT_FATAL),
						 errmsg("\"%s\" node has found serious issues in our watchdog messages",
								wdNode->nodeName),
						 errdetail("shutting down")));
			}
			break;
		default:
			break;
	}
}

static void
wd_execute_cluster_command_processor(WatchdogNode * wdNode, WDPacketData * pkt)
{
	/* get the json for node list */
	char 	*clusterCommand = NULL;
	List 	*args_list = NULL;

	if (pkt->type != WD_EXECUTE_COMMAND_REQUEST)
		return;

	if (pkt->len <= 0 || pkt->data == NULL)
	{
		ereport(LOG,
				(errmsg("node \"%s\" sent an empty execute cluster command message", wdNode->nodeName)));
		return;
	}

	if (!parse_wd_exec_cluster_command_json(pkt->data, pkt->len,
									   &clusterCommand, &args_list))
	{
		ereport(LOG,
				(errmsg("node \"%s\" sent an invalid JSON data in cluster command message", wdNode->nodeName)));
		return;
	}

	ereport(DEBUG1,
			(errmsg("received \"%s\" command from node \"%s\"",clusterCommand, wdNode->nodeName)));
	if (strcasecmp(WD_COMMAND_SHUTDOWN_CLUSTER, clusterCommand) == 0)
	{
		char mode = 's';
		ListCell   *lc;
		foreach(lc, args_list)
		{
			WDExecCommandArg *wdExecCommandArg = lfirst(lc);
			if (strcmp(wdExecCommandArg->arg_name, "mode") == 0)
			{
				mode = wdExecCommandArg->arg_value[0];
			}
			else
				ereport(LOG,
						(errmsg("unsupported argument \"%s\" in shutdown command from remote node \"%s\"", wdExecCommandArg->arg_name, wdNode->nodeName)));
		}

		ereport(LOG,
				(errmsg("processing shutdown command from remote node \"%s\"", wdNode->nodeName)));
		terminate_pgpool(mode, false);
	}
	else if (strcasecmp(WD_COMMAND_RELOAD_CONFIG_CLUSTER, clusterCommand) == 0)
	{
		ereport(LOG,
				(errmsg("processing reload config command from remote node \"%s\"", wdNode->nodeName)));
		pool_signal_parent(SIGHUP);
	}
	else if (strcasecmp(WD_COMMAND_LOCK_ON_STANDBY, clusterCommand) == 0)
	{
		int lock_type = -1;
		char *operation = NULL;
		if (get_local_node_state() == WD_STANDBY && wdNode->state == WD_COORDINATOR)
		{
			if (list_length(args_list) == 2)
			{
				ListCell   *lc;
				foreach(lc, args_list)
				{
					WDExecCommandArg *wdExecCommandArg = lfirst(lc);
					if (strcmp(wdExecCommandArg->arg_name, "StandbyLockType") == 0)
					{
						lock_type = atoi(wdExecCommandArg->arg_value);
					}
					else if (strcmp(wdExecCommandArg->arg_name, "LockingOperation") == 0)
					{
						operation = wdExecCommandArg->arg_value;
					}
					else
						ereport(LOG,
								(errmsg("unsupported argument \"%s\" in 'LOCK ON STANDBY' from remote node \"%s\"", wdExecCommandArg->arg_name, wdNode->nodeName)));
				}
				if (lock_type < 0 || operation == NULL)
				{
					ereport(LOG,
							(errmsg("missing argument in 'LOCK ON STANDBY' from remote node \"%s\"", wdNode->nodeName),
							 errdetail("command ignored")));
				}
				else if (lock_type == WD_FOLLOW_PRIMARY_LOCK)
				{
					ereport(LOG,
							(errmsg("processing follow primary looking[%s] request from remote node \"%s\"", operation,wdNode->nodeName)));

					if (strcasecmp("acquire", operation) == 0)
						pool_acquire_follow_primary_lock(false, true);
					else if (strcasecmp("release", operation) == 0)
						pool_release_follow_primary_lock(true);
					else
						ereport(LOG,
								(errmsg("invalid looking operaition[%s] in 'LOCK ON STANDBY' from remote node \"%s\"", operation, wdNode->nodeName),
								 errdetail("command ignored")));
				}
				else
					ereport(LOG,
							(errmsg("unsupported lock-type:%d in 'LOCK ON STANDBY' from remote node \"%s\"", lock_type, wdNode->nodeName)));

			}
			else
			{
				ereport(LOG,
						(errmsg("invalid arguments in 'LOCK ON STANDBY' command from remote node \"%s\"",  wdNode->nodeName)));
			}
		}
		else if (get_local_node_state() != WD_STANDBY)
		{
			ereport(LOG,
					(errmsg("invalid node state to execute 'LOCK ON STANDBY' command")));

		}
		else
		{
			ereport(LOG,
					(errmsg("'LOCK ON STANDBY' command can only be accepted from the coordinator watchdog node"),
					 errdetail("ignoring...")));
		}
	}
	else
	{
		ereport(WARNING,
				(errmsg("received \"%s\" command from node \"%s\" is not supported",clusterCommand, wdNode->nodeName)));
	}

	if (args_list)
		list_free_deep(args_list);
	pfree(clusterCommand);
	return;
}

static int
standard_packet_processor(WatchdogNode * wdNode, WDPacketData * pkt)
{
	WDPacketData *replyPkt = NULL;

	switch (pkt->type)
	{
		case WD_FAILOVER_WAITING_FOR_CONSENSUS:
			ereport(LOG,
					(errmsg("remote node \"%s\" is asking to inform about quarantined backend nodes", wdNode->nodeName)));
			register_inform_quarantine_nodes_req();
			break;

		case WD_EXECUTE_COMMAND_REQUEST:
			wd_execute_cluster_command_processor(wdNode, pkt);
			break;

		case WD_CLUSTER_SERVICE_MESSAGE:
			cluster_service_message_processor(wdNode, pkt);
			break;

		case WD_GET_LEADER_DATA_REQUEST:
			replyPkt = process_data_request(wdNode, pkt);
			break;

		case WD_ASK_FOR_POOL_CONFIG:
			{
				char	   *config_data = get_pool_config_json();

				if (config_data)
				{
					replyPkt = get_empty_packet();
					set_message_type(replyPkt, WD_POOL_CONFIG_DATA);
					set_message_commandID(replyPkt, pkt->command_id);
					set_message_data(replyPkt, config_data, strlen(config_data));
				}
				else
				{
					replyPkt = get_minimum_message(WD_ERROR_MESSAGE, pkt);

				}
			}
			break;

		case WD_POOL_CONFIG_DATA:
			{
				/* only accept config data if I am the coordinator node */
				if (get_local_node_state() == WD_COORDINATOR && pkt->data)
				{
					POOL_CONFIG *standby_config = get_pool_config_from_json(pkt->data, pkt->len);

					if (standby_config)
					{
						verify_pool_configurations(wdNode, standby_config);
						update_failover_timeout(wdNode, standby_config);
					}
				}
			}
			break;

		case WD_ADD_NODE_MESSAGE:
		case WD_REQ_INFO_MESSAGE:
			replyPkt = get_mynode_info_message(pkt);
			break;

		case WD_INFO_MESSAGE:
			{
				char	   *authkey = NULL;
				int			oldQuorumStatus;
				WD_STATES	oldNodeState;
				WatchdogNode *tempNode = parse_node_info_message(pkt, &authkey);

				if (tempNode == NULL)
				{
					ereport(WARNING,
							(errmsg("node \"%s\" sent an invalid node info message", wdNode->nodeName)));
					send_cluster_service_message(wdNode, pkt, CLUSTER_NODE_INVALID_VERSION);
					break;
				}
				oldQuorumStatus = wdNode->quorum_status;
				oldNodeState = wdNode->state;
				wdNode->state = tempNode->state;
				wdNode->startup_time.tv_sec = tempNode->startup_time.tv_sec;
				wdNode->wd_priority = tempNode->wd_priority;
				strlcpy(wdNode->nodeName, tempNode->nodeName, WD_MAX_HOST_NAMELEN);

				wdNode->current_state_time.tv_sec = tempNode->current_state_time.tv_sec;
				wdNode->escalated = tempNode->escalated;
				wdNode->standby_nodes_count = tempNode->standby_nodes_count;
				wdNode->quorum_status = tempNode->quorum_status;

				print_watchdog_node_info(wdNode);

				if (authkey)
					pfree(authkey);

				if (wdNode->state == WD_COORDINATOR)
				{
					if (WD_LEADER_NODE == NULL)
					{
						set_cluster_leader_node(wdNode);
					}
					else if (WD_LEADER_NODE != wdNode)
					{
						ereport(LOG,
								(errmsg("\"%s\" is the coordinator as per our record but \"%s\" is also announcing as a coordinator",
										WD_LEADER_NODE->nodeName, wdNode->nodeName),
								 errdetail("cluster is in the split-brain")));

						if (get_local_node_state() != WD_COORDINATOR)
						{
							/*
							 * This fight doesn't belong to me broadcast the
							 * message about cluster in split-brain
							 */

							send_cluster_service_message(NULL, pkt, CLUSTER_IN_SPLIT_BRAIN);
						}
						else
						{
							/*
							 * okay the contention is between me and the other
							 * node try to figure out which node is the worthy
							 * leader
							 */
							ereport(LOG,
									(errmsg("I am the coordinator but \"%s\" is also announcing as a coordinator", wdNode->nodeName),
									 errdetail("trying to figure out the best contender for the leader/coordinator node")));

							handle_split_brain(wdNode, pkt);
						}
					}
					else if (WD_LEADER_NODE == wdNode && oldQuorumStatus != wdNode->quorum_status)
					{
						/* inform Pgpool main about quorum status changes */
						register_watchdog_quorum_change_interrupt();
					}
				}

				/*
				 * if the info message is from leader node. Make sure we are
				 * in sync with the leader node state
				 */
				else if (WD_LEADER_NODE == wdNode)
				{
					if (wdNode->state != WD_COORDINATOR)
					{
						ereport(WARNING,
								(errmsg("the coordinator as per our record is not coordinator anymore"),
								 errdetail("re-initializing the cluster")));
						set_state(WD_JOINING);
					}
				}
				pfree(tempNode);

				if (oldNodeState == WD_STANDBY && wdNode->state != oldNodeState)
				{
					standby_node_left_cluster(wdNode);
				}
				if (oldNodeState == WD_LOST)
				{
					/*
					 * We have received the message from lost node
					 * add it back to cluster if it was not marked by
					 * life-check
					 * Node lost by life-check processes can only be
					 * added back when we get alive notification for the
					 * node from life-check
					 */
					ereport(LOG,
						(errmsg("we have received the NODE INFO message from the node:\"%s\" that was lost",wdNode->nodeName),
						 errdetail("we had lost this node because of \"%s\"",wd_node_lost_reasons[wdNode->node_lost_reason])));

					if (wdNode->node_lost_reason == NODE_LOST_BY_LIFECHECK)
					{
						ereport(LOG,
							(errmsg("node:\"%s\" was reported lost by the life-check process",wdNode->nodeName),
								 errdetail("node will be added to cluster once life-check mark it as reachable again")));
						/* restore the node's lost state */
						wdNode->state = oldNodeState;
					}
					else
					{
						watchdog_state_machine(WD_EVENT_REMOTE_NODE_FOUND, wdNode, NULL, NULL);
					}
				}
			}
			break;

		case WD_JOIN_COORDINATOR_MESSAGE:
			{
				/*
				 * if I am coordinator reply with accept, otherwise reject
				 */
				if (g_cluster.localNode == WD_LEADER_NODE)
				{
					replyPkt = get_minimum_message(WD_ACCEPT_MESSAGE, pkt);
				}
				else
				{
					replyPkt = get_minimum_message(WD_REJECT_MESSAGE, pkt);
				}
			}
			break;

		case WD_IAM_COORDINATOR_MESSAGE:
			{
				/*
				 * if the message is received from coordinator reply with
				 * info, otherwise reject
				 */
				if (WD_LEADER_NODE != NULL && wdNode != WD_LEADER_NODE)
				{
					ereport(LOG,
							(errmsg("\"%s\" is our coordinator node, but \"%s\" is also announcing as a coordinator",
									WD_LEADER_NODE->nodeName, wdNode->nodeName),
							 errdetail("broadcasting the cluster in split-brain message")));

					send_cluster_service_message(NULL, pkt, CLUSTER_IN_SPLIT_BRAIN);
				}
				else if (WD_LEADER_NODE != NULL)
				{
					replyPkt = get_mynode_info_message(pkt);
					beacon_message_received_from_node(wdNode, pkt);
				}
				/*
				 * if (WD_LEADER_NODE == NULL)
				 * do not reply to beacon if we are not connected to
				 * any leader node
				 */
			}
			break;

		default:
			break;
	}
	if (replyPkt)
	{
		if (send_message_to_node(wdNode, replyPkt) == false)
			ereport(LOG,
					(errmsg("sending packet to node \"%s\" failed", wdNode->nodeName)));
		free_packet(replyPkt);
	}
	return 1;
}


static bool
send_message_to_connection(SocketConnection * conn, WDPacketData * pkt)
{
	if (check_debug_request_kill_all_communication() == true ||
		check_debug_request_kill_all_senders() == true)
		return false;

	if (conn->sock > 0 && conn->sock_state == WD_SOCK_CONNECTED)
	{
		if (write_packet_to_socket(conn->sock, pkt, false) == true)
			return true;
		ereport(DEBUG1,
				(errmsg("sending packet failed, closing connection")));
		close_socket_connection(conn);
	}

	return false;
}

static bool
send_message_to_node(WatchdogNode * wdNode, WDPacketData * pkt)
{
	bool		ret;

	print_packet_node_info(pkt, wdNode, true);

	ret = send_message_to_connection(&wdNode->client_socket, pkt);
	if (ret == false)
	{
		ret = send_message_to_connection(&wdNode->server_socket, pkt);
	}
	if (ret)
	{
		/* reset the sending error counter */
		wdNode->sending_failures_count = 0;
		/* we only update the last sent time if reply for packet is expected */
		switch (pkt->type)
		{
			case WD_REMOTE_FAILOVER_REQUEST:
			case WD_IPC_FAILOVER_COMMAND:
				if (wdNode->last_sent_time.tv_sec <= 0)
					gettimeofday(&wdNode->last_sent_time, NULL);
				break;
			default:
				break;
		}
	}
	else
	{
		wdNode->sending_failures_count++;
		ereport(DEBUG1,
				(errmsg("sending packet %c to node \"%s\" failed", pkt->type, wdNode->nodeName)));
	}
	return ret;
}

/*
 * If wdNode is NULL message is sent to all nodes
 * Returns the number of nodes the message is sent to
 */
static int
send_message(WatchdogNode * wdNode, WDPacketData * pkt)
{
	int			i,
				count = 0;

	if (wdNode)
	{
		if (wdNode == g_cluster.localNode)	/* Always return 1 if I myself is
											 * intended receiver */
			return 1;
		if (send_message_to_node(wdNode, pkt))
			return 1;
		return 0;
	}
	/* NULL means send to all reachable nodes */
	for (i = 0; i < g_cluster.remoteNodeCount; i++)
	{
		wdNode = &(g_cluster.remoteNodes[i]);
		if (is_node_reachable(wdNode) && send_message_to_node(wdNode, pkt))
			count++;
	}
	return count;
}

static IPC_CMD_PROCESS_RES wd_command_processor_for_node_lost_event(WDCommandData * ipcCommand, WatchdogNode * wdLostNode)
{
	if (ipcCommand->sendToNode)
	{
		/* The command was sent to one node only */
		if (ipcCommand->sendToNode == wdLostNode)
		{
			/*
			 * Fail this command, Since the only node it was sent to is lost
			 */
			ipcCommand->commandStatus = COMMAND_FINISHED_SEND_FAILED;
			wd_command_is_complete(ipcCommand);
			return IPC_CMD_ERROR;
		}
		else
		{
			/* Dont worry this command is fine for now */
			return IPC_CMD_PROCESSING;
		}
	}
	else
	{
		/* search the node that is lost */
		int			i;

		for (i = 0; i < g_cluster.remoteNodeCount; i++)
		{
			WDCommandNodeResult *nodeResult = &ipcCommand->nodeResults[i];

			if (nodeResult->wdNode == wdLostNode)
			{
				if (nodeResult->cmdState == COMMAND_STATE_SENT)
				{
					ereport(LOG,
							(errmsg("remote node \"%s\" lost while IPC command was in progress ", wdLostNode->nodeName)));

					/*
					 * since the node is lost and will be removed from the
					 * cluster So remove decrement the sent count of command
					 * and see what is the situation after that
					 */
					nodeResult->cmdState = COMMAND_STATE_DO_NOT_SEND;
					ipcCommand->commandSendToCount--;
					if (ipcCommand->commandSendToCount <= ipcCommand->commandReplyFromCount)
					{
						/*
						 * If we have already received the results from all
						 * alive nodes finish the command
						 */
						ipcCommand->commandStatus = COMMAND_FINISHED_ALL_REPLIED;
						wd_command_is_complete(ipcCommand);
						return IPC_CMD_COMPLETE;
					}
				}
				break;
			}
		}
	}
	return IPC_CMD_PROCESSING;
}

static void
wd_command_is_complete(WDCommandData * ipcCommand)
{
	if (ipcCommand->commandCompleteFunc)
	{
		ipcCommand->commandCompleteFunc(ipcCommand);
		return;
	}

	/*
	 * There is not special function for this command use the standard reply
	 */
	if (ipcCommand->commandSource == COMMAND_SOURCE_IPC)
	{
		char		res_type;

		switch (ipcCommand->commandStatus)
		{
			case COMMAND_FINISHED_ALL_REPLIED:
				res_type = WD_IPC_CMD_RESULT_OK;
				break;
			case COMMAND_FINISHED_TIMEOUT:
				res_type = WD_IPC_CMD_TIMEOUT;
				break;
			case COMMAND_FINISHED_NODE_REJECTED:
			case COMMAND_FINISHED_SEND_FAILED:
				res_type = WD_IPC_CMD_RESULT_BAD;
				break;
			default:
				res_type = WD_IPC_CMD_RESULT_OK;
				break;
		}
		write_ipc_command_with_result_data(ipcCommand, res_type, NULL, 0);
	}
	else if (ipcCommand->commandSource == COMMAND_SOURCE_REMOTE)
	{
		char		res_type;

		if (ipcCommand->commandStatus == COMMAND_FINISHED_ALL_REPLIED)
			res_type = WD_ACCEPT_MESSAGE;
		else
			res_type = WD_REJECT_MESSAGE;

		reply_with_minimal_message(ipcCommand->sourceWdNode, res_type, &ipcCommand->commandPacket);
	}
}


static void
node_lost_while_ipc_command(WatchdogNode * wdNode)
{
	List	   *ipcCommands_to_del = NIL;
	ListCell   *lc;

	foreach(lc, g_cluster.ipc_commands)
	{
		WDCommandData *ipcCommand = lfirst(lc);
		IPC_CMD_PROCESS_RES res = wd_command_processor_for_node_lost_event(ipcCommand, wdNode);

		if (res != IPC_CMD_PROCESSING)
		{
			ipcCommands_to_del = lappend(ipcCommands_to_del, ipcCommand);
		}
	}
	/* delete completed commands */
	foreach(lc, ipcCommands_to_del)
	{
		WDCommandData *ipcCommand = lfirst(lc);

		cleanUpIPCCommand(ipcCommand);
	}

	list_free(ipcCommands_to_del);
}


/*
 * The function walks through all command and resends
 * the failed message again if it can.
 */
static void
service_ipc_commands(void)
{
	ListCell   *lc;

	foreach(lc, g_cluster.ipc_commands)
	{
		WDCommandData *ipcCommand = lfirst(lc);

		if (ipcCommand && ipcCommand->commandSendToErrorCount)
		{
			int			i;

			for (i = 0; i < g_cluster.remoteNodeCount; i++)
			{
				WDCommandNodeResult *nodeResult = &ipcCommand->nodeResults[i];

				if (nodeResult->cmdState == COMMAND_STATE_SEND_ERROR)
				{
					if (is_node_active_and_reachable(nodeResult->wdNode))
					{
						ereport(LOG,
								(errmsg("remote node \"%s\" is reachable again, resending the command packet ", nodeResult->wdNode->nodeName)));

						if (send_message_to_node(nodeResult->wdNode, &ipcCommand->commandPacket) == true)
						{
							nodeResult->cmdState = COMMAND_STATE_SENT;
							ipcCommand->commandSendToErrorCount--;
							ipcCommand->commandSendToCount++;
							if (ipcCommand->commandSendToErrorCount == 0)
								break;
						}
					}
				}
			}
		}
	}
}

static void
service_internal_command(void)
{
	int			i;
	ListCell   *lc;
	List	   *finishedCommands = NULL;

	if (g_cluster.clusterCommands == NULL)
		return;

	foreach(lc, g_cluster.clusterCommands)
	{
		WDCommandData *clusterCommand = lfirst(lc);

		if (clusterCommand->commandStatus != COMMAND_IN_PROGRESS)
		{
			/* command needs to be cleaned up */
			finishedCommands = lappend(finishedCommands, clusterCommand);
			continue;
		}

		for (i = 0; i < g_cluster.remoteNodeCount; i++)
		{
			WDCommandNodeResult *nodeResult = &clusterCommand->nodeResults[i];

			if (nodeResult->cmdState == COMMAND_STATE_SEND_ERROR)
			{
				if (is_node_active_and_reachable(nodeResult->wdNode))
				{
					if (send_message_to_node(nodeResult->wdNode, &clusterCommand->commandPacket) == true)
					{
						nodeResult->cmdState = COMMAND_STATE_SENT;
						clusterCommand->commandSendToCount++;
					}
				}
			}
		}
	}
	/* delete the finished commands */
	foreach(lc, finishedCommands)
	{
		WDCommandData *clusterCommand = lfirst(lc);

		g_cluster.clusterCommands = list_delete_ptr(g_cluster.clusterCommands, clusterCommand);
		MemoryContextDelete(clusterCommand->memoryContext);
	}

	list_free(finishedCommands);
}

/* remove the unreachable nodes from cluster */
static void
service_unreachable_nodes(void)
{
	int			i;
	struct timeval currTime;

	gettimeofday(&currTime, NULL);

	for (i = 0; i < g_cluster.remoteNodeCount; i++)
	{
		WatchdogNode *wdNode = &(g_cluster.remoteNodes[i]);

		if (wdNode->state == WD_LOST && wdNode->membership_status == WD_NODE_MEMBERSHIP_ACTIVE
			&& pool_config->wd_lost_node_removal_timeout)
		{
			int lost_seconds = WD_TIME_DIFF_SEC(currTime, wdNode->lost_time);
			if (lost_seconds >= pool_config->wd_lost_node_removal_timeout)
			{
				ereport(LOG,
						(errmsg("remote node \"%s\" is lost for %d seconds", wdNode->nodeName,lost_seconds),
						 errdetail("revoking the node's membership")));
				revoke_cluster_membership_of_node(wdNode,WD_NODE_REVOKED_LOST);
			}
			continue;
		}

		if (wdNode->state == WD_DEAD && wdNode->membership_status == WD_NODE_MEMBERSHIP_ACTIVE
			&& pool_config->wd_no_show_node_removal_timeout)
		{
			int no_show_seconds = WD_TIME_DIFF_SEC(currTime, g_cluster.localNode->startup_time);
			if (no_show_seconds >= pool_config->wd_no_show_node_removal_timeout)
			{
				ereport(LOG,
						(errmsg("remote node \"%s\" didn't showed-up in %d seconds", wdNode->nodeName,no_show_seconds),
						 errdetail("revoking the node's membership")));
				revoke_cluster_membership_of_node(wdNode,WD_NODE_REVOKED_NO_SHOW);
			}
			continue;
		}

		if (is_node_active(wdNode) == false)
			continue;

		if (is_node_reachable(wdNode) || wdNode->client_socket.sock_state == WD_SOCK_WAITING_FOR_CONNECT)
		{
			/* check if we are waiting for reply from this node */
			if (wdNode->last_sent_time.tv_sec > 0)
			{
				if (WD_TIME_DIFF_SEC(currTime, wdNode->last_sent_time) >= MAX_SECS_WAIT_FOR_REPLY_FROM_NODE)
				{
					ereport(LOG,
							(errmsg("remote node \"%s\" is not replying..", wdNode->nodeName),
							 errdetail("marking the node as lost")));
					/* mark the node as lost */
					wdNode->node_lost_reason = NODE_LOST_BY_RECEIVE_TIMEOUT;
					watchdog_state_machine(WD_EVENT_REMOTE_NODE_LOST, wdNode, NULL, NULL);
				}
			}
			else if (wdNode->sending_failures_count > MAX_ALLOWED_SEND_FAILURES)
			{
				ereport(LOG,
						(errmsg("not able to send messages to remote node \"%s\"",wdNode->nodeName),
						 errdetail("marking the node as lost")));
				/* mark the node as lost */
				wdNode->node_lost_reason = NODE_LOST_BY_SEND_FAILURE;
				watchdog_state_machine(WD_EVENT_REMOTE_NODE_LOST, wdNode, NULL, NULL);
			}
			else if (wdNode->missed_beacon_count > MAX_ALLOWED_BEACON_REPLY_MISS)
			{
				ereport(LOG,
						(errmsg("remote node \"%s\" is not responding to our beacon messages",wdNode->nodeName),
						 errdetail("marking the node as lost")));
				/* mark the node as lost */
				wdNode->node_lost_reason = NODE_LOST_BY_MISSING_BEACON;
				wdNode->missed_beacon_count = 0; /* Reset the counter */
				watchdog_state_machine(WD_EVENT_REMOTE_NODE_LOST, wdNode, NULL, NULL);
			}
		}
		else
		{
			ereport(LOG,
					(errmsg("remote node \"%s\" is not reachable", wdNode->nodeName),
					 errdetail("marking the node as lost")));
			wdNode->node_lost_reason = NODE_LOST_BY_NOT_REACHABLE;
			watchdog_state_machine(WD_EVENT_REMOTE_NODE_LOST, wdNode, NULL, NULL);
		}
	}
}

static bool
watchdog_internal_command_packet_processor(WatchdogNode * wdNode, WDPacketData * pkt)
{
	int			i;
	WDCommandNodeResult *nodeResult = NULL;
	WDCommandData *clusterCommand = get_wd_cluster_command_from_reply(pkt);

	if (clusterCommand == NULL || clusterCommand->commandStatus != COMMAND_IN_PROGRESS)
		return false;

	if (pkt->type != WD_ERROR_MESSAGE &&
		pkt->type != WD_ACCEPT_MESSAGE &&
		pkt->type != WD_REJECT_MESSAGE &&
		pkt->type != WD_INFO_MESSAGE)
		return false;

	if (pkt->type == WD_INFO_MESSAGE)
		standard_packet_processor(wdNode, pkt);

	/* get the result node for */
	for (i = 0; i < g_cluster.remoteNodeCount; i++)
	{
		WDCommandNodeResult *nodeRes = &clusterCommand->nodeResults[i];
		if (nodeRes->wdNode == wdNode)
		{
			nodeResult = nodeRes;
			break;
		}
	}
	if (nodeResult == NULL)
	{
		ereport(NOTICE, (errmsg("unable to find node result")));
		return true;
	}

	ereport(DEBUG1,
			(errmsg("Watchdog node \"%s\" has replied for command id %d", nodeResult->wdNode->nodeName, pkt->command_id)));

	nodeResult->result_type = pkt->type;
	nodeResult->cmdState = COMMAND_STATE_REPLIED;
	clusterCommand->commandReplyFromCount++;

	if (clusterCommand->commandReplyFromCount >= clusterCommand->commandSendToCount)
	{
		if (pkt->type == WD_REJECT_MESSAGE || pkt->type == WD_ERROR_MESSAGE)
		{
			ereport(DEBUG1,
					(errmsg("command %c with command id %d is finished with COMMAND_FINISHED_NODE_REJECTED", pkt->type, pkt->command_id)));
			clusterCommand->commandStatus = COMMAND_FINISHED_NODE_REJECTED;
		}
		else
		{
			ereport(DEBUG1,
					(errmsg("command %c with command id %d is finished with COMMAND_FINISHED_ALL_REPLIED", pkt->type, pkt->command_id)));
			clusterCommand->commandStatus = COMMAND_FINISHED_ALL_REPLIED;
		}
		watchdog_state_machine(WD_EVENT_COMMAND_FINISHED, wdNode, pkt, clusterCommand);
		g_cluster.clusterCommands = list_delete_ptr(g_cluster.clusterCommands, clusterCommand);
		MemoryContextDelete(clusterCommand->memoryContext);
	}
	else if (pkt->type == WD_REJECT_MESSAGE || pkt->type == WD_ERROR_MESSAGE)
	{
		/* Error or reject message by any node immediately finishes the command */
		ereport(DEBUG1,
				(errmsg("command %c with command id %d is finished with COMMAND_FINISHED_NODE_REJECTED", pkt->type, pkt->command_id)));
		clusterCommand->commandStatus = COMMAND_FINISHED_NODE_REJECTED;
		watchdog_state_machine(WD_EVENT_COMMAND_FINISHED, wdNode, pkt, clusterCommand);
		g_cluster.clusterCommands = list_delete_ptr(g_cluster.clusterCommands, clusterCommand);
		MemoryContextDelete(clusterCommand->memoryContext);
	}
	return true;				/* do not process this packet further */
}


static void
check_for_current_command_timeout(void)
{
	struct timeval currTime;

	ListCell   *lc;
	List	   *finishedCommands = NULL;

	if (g_cluster.clusterCommands == NULL)
		return;

	gettimeofday(&currTime, NULL);

	foreach(lc, g_cluster.clusterCommands)
	{
		WDCommandData *clusterCommand = lfirst(lc);

		if (clusterCommand->commandStatus != COMMAND_IN_PROGRESS)
		{
			/* command needs to be cleaned up */
			finishedCommands = lappend(finishedCommands, clusterCommand);
			continue;
		}
		if (WD_TIME_DIFF_SEC(currTime, clusterCommand->commandTime) >= clusterCommand->commandTimeoutSecs)
		{
			clusterCommand->commandStatus = COMMAND_FINISHED_TIMEOUT;
			watchdog_state_machine(WD_EVENT_COMMAND_FINISHED, NULL, NULL, clusterCommand);
			finishedCommands = lappend(finishedCommands, clusterCommand);
		}
	}
	/* delete the finished commands */
	foreach(lc, finishedCommands)
	{
		WDCommandData *clusterCommand = lfirst(lc);

		g_cluster.clusterCommands = list_delete_ptr(g_cluster.clusterCommands, clusterCommand);
		MemoryContextDelete(clusterCommand->memoryContext);
	}

	list_free(finishedCommands);
}


/*
 * If wdNode is NULL message is sent to all nodes
 * Returns the number of nodes the message is sent to
 */
static int
issue_watchdog_internal_command(WatchdogNode * wdNode, WDPacketData * pkt, int timeout_sec)
{
	int			i;
	bool		save_message = false;
	WDCommandData *clusterCommand;
	MemoryContext oldCxt;

	clusterCommand = create_command_object(0);

	clusterCommand->commandSource = COMMAND_SOURCE_LOCAL;
	clusterCommand->sourceWdNode = g_cluster.localNode;
	gettimeofday(&clusterCommand->commandTime, NULL);

	clusterCommand->commandTimeoutSecs = timeout_sec;
	clusterCommand->commandPacket.type = pkt->type;
	clusterCommand->commandPacket.command_id = pkt->command_id;
	clusterCommand->commandPacket.len = 0;
	clusterCommand->commandPacket.data = NULL;

	clusterCommand->sendToNode = wdNode;
	clusterCommand->commandSendToCount = 0;
	clusterCommand->commandReplyFromCount = 0;
	clusterCommand->commandStatus = COMMAND_IN_PROGRESS;

	allocate_resultNodes_in_command(clusterCommand);

	if (wdNode == NULL)			/* This is send to all */
	{
		for (i = 0; i < g_cluster.remoteNodeCount; i++)
		{
			WDCommandNodeResult *nodeResult = &clusterCommand->nodeResults[i];

			clear_command_node_result(nodeResult);
			if (is_node_active(nodeResult->wdNode) == false)
			{
				ereport(DEBUG2,
						(errmsg("not sending watchdog internal command packet to DEAD %s", nodeResult->wdNode->nodeName)));
				/* Do not send to dead nodes */
				nodeResult->cmdState = COMMAND_STATE_DO_NOT_SEND;
			}
			else
			{
				if (send_message_to_node(nodeResult->wdNode, pkt) == false)
				{
					ereport(DEBUG1,
							(errmsg("failed to send watchdog internal command packet %s", nodeResult->wdNode->nodeName),
							 errdetail("saving the packet. will try to resend it if connection recovers")));

					/* failed to send. May be try again later */
					save_message = true;
					nodeResult->cmdState = COMMAND_STATE_SEND_ERROR;
				}
				else
				{
					nodeResult->cmdState = COMMAND_STATE_SENT;
					clusterCommand->commandSendToCount++;
				}
			}
		}
	}
	if (wdNode)
	{
		WDCommandNodeResult *nodeResult = NULL;

		for (i = 0; i < g_cluster.remoteNodeCount; i++)
		{
			WDCommandNodeResult *nodeRes = &clusterCommand->nodeResults[i];

			clear_command_node_result(nodeRes);
			if (nodeRes->wdNode == wdNode)
				nodeResult = nodeRes;
		}
		if (nodeResult == NULL)
		{
			/* should never happen */
			ereport(WARNING,
					(errmsg("Internal error. Not able to locate node result slot")));
			MemoryContextDelete(clusterCommand->memoryContext);
			return -1;
		}
		if (send_message_to_node(nodeResult->wdNode, pkt) == false)
		{
			/* failed to send. May be try again later */
			save_message = true;
			nodeResult->cmdState = COMMAND_STATE_SEND_ERROR;
		}
		else
		{
			nodeResult->cmdState = COMMAND_STATE_SENT;
			clusterCommand->commandSendToCount++;
		}
	}
	if (save_message && pkt->len > 0)
	{
		clusterCommand->commandPacket.data = MemoryContextAlloc(clusterCommand->memoryContext, pkt->len);
		memcpy(clusterCommand->commandPacket.data, pkt->data, pkt->len);
		clusterCommand->commandPacket.len = pkt->len;
	}
	ereport(DEBUG2,
			(errmsg("new cluster command %c issued with command id %d", pkt->type, pkt->command_id)));

	oldCxt = MemoryContextSwitchTo(TopMemoryContext);
	g_cluster.clusterCommands = lappend(g_cluster.clusterCommands, clusterCommand);
	MemoryContextSwitchTo(oldCxt);

	return clusterCommand->commandSendToCount;
}

/*
 * Check remote connections except their state are either WD_SHUTDOWN or
 * WD_DEAD. If suncceeded in connecting to any of the remote nodes, returns
 * true, otherwise false.
 */
static bool
service_lost_connections(void)
{
	int			i;
	struct timeval currTime;
	bool		ret = false;

	gettimeofday(&currTime, NULL);
	for (i = 0; i < g_cluster.remoteNodeCount; i++)
	{
		WatchdogNode *wdNode = &(g_cluster.remoteNodes[i]);

		if (wdNode->state == WD_SHUTDOWN || wdNode->state == WD_DEAD)
			continue;

		if (is_socket_connection_connected(&wdNode->client_socket) == false)
		{
			if (WD_TIME_DIFF_SEC(currTime, wdNode->client_socket.tv) <= MIN_SECS_CONNECTION_RETRY)
				continue;

			if (wdNode->client_socket.sock_state != WD_SOCK_WAITING_FOR_CONNECT)
			{
				connect_to_node(wdNode);
				if (wdNode->client_socket.sock_state == WD_SOCK_CONNECTED)
				{
					ereport(LOG,
							(errmsg("connection to the remote node \"%s\" is restored", wdNode->nodeName)));
					watchdog_state_machine(WD_EVENT_NEW_OUTBOUND_CONNECTION, wdNode, NULL, NULL);
					ret = true;
				}
			}
		}
	}
	return ret;
}

/*
 * The function only considers the node state.
 * All node states count towards the cluster participating nodes
 * except the dead and lost nodes.
 */
static int
get_cluster_node_count(void)
{
	int			i;
	int			count = 0;

	for (i = 0; i < g_cluster.remoteNodeCount; i++)
	{
		WatchdogNode *wdNode = &(g_cluster.remoteNodes[i]);

		if (wdNode->state == WD_DEAD || wdNode->state == WD_LOST || wdNode->state == WD_SHUTDOWN)
			continue;
		count++;
	}
	return count;
}

static WDPacketData * get_message_of_type(char type, WDPacketData * replyFor)
{
	WDPacketData *pkt = NULL;

	switch (type)
	{
		case WD_INFO_MESSAGE:
			pkt = get_mynode_info_message(replyFor);
			break;
		case WD_ADD_NODE_MESSAGE:
			pkt = get_addnode_message();
			break;
		case WD_IAM_COORDINATOR_MESSAGE:
			pkt = get_beacon_message(WD_IAM_COORDINATOR_MESSAGE, replyFor);
			break;

		case WD_FAILOVER_START:
		case WD_FAILOVER_END:
		case WD_REQ_INFO_MESSAGE:
		case WD_STAND_FOR_COORDINATOR_MESSAGE:
		case WD_DECLARE_COORDINATOR_MESSAGE:
		case WD_JOIN_COORDINATOR_MESSAGE:
		case WD_QUORUM_IS_LOST:
		case WD_INFORM_I_AM_GOING_DOWN:
		case WD_ASK_FOR_POOL_CONFIG:
		case WD_FAILOVER_WAITING_FOR_CONSENSUS:
			pkt = get_minimum_message(type, replyFor);
			break;
		default:
			ereport(LOG, (errmsg("invalid message type %c", type)));
			break;
	}
	return pkt;
}

static int
send_message_of_type(WatchdogNode * wdNode, char type, WDPacketData * replyFor)
{
	int			ret = -1;
	WDPacketData *pkt = get_message_of_type(type, replyFor);

	if (pkt)
	{
		ret = send_message(wdNode, pkt);
		free_packet(pkt);
	}
	return ret;
}

static int
send_cluster_command(WatchdogNode * wdNode, char type, int timeout_sec)
{
	int			ret = -1;
	WDPacketData *pkt = get_message_of_type(type, NULL);

	if (pkt)
	{
		ret = issue_watchdog_internal_command(wdNode, pkt, timeout_sec);
		free_packet(pkt);
	}
	return ret;
}

static bool
reply_with_minimal_message(WatchdogNode * wdNode, char type, WDPacketData * replyFor)
{
	WDPacketData *pkt = get_minimum_message(type, replyFor);
	int			ret = send_message(wdNode, pkt);

	free_packet(pkt);
	return ret;
}

static bool
send_cluster_service_message(WatchdogNode * wdNode, WDPacketData * replyFor, char message)
{
	/* Check if its a broadcast message */
	if (wdNode == NULL)
	{
		/* see if we have already broadcasted the similar message recently */
		if (message == g_cluster.last_bcast_srv_msg)
		{
			struct timeval currTime;
			gettimeofday(&currTime, NULL);
			int	 last_bcast_sec = WD_TIME_DIFF_SEC(currTime, g_cluster.last_bcast_srv_msg_time);
			if (last_bcast_sec < MIN_SECS_BETWEEN_BROADCAST_SRV_MSG)
			{
				/*
				 * do not broadcast this message
				 * to prevent flooding
				 */
				ereport(DEBUG4,
						(errmsg("not broadcasting cluster service message %c to prevent flooding ",message),
						 errdetail("last time same message was sent %d seconds ago",last_bcast_sec)));
				return true;
			}
		}
		g_cluster.last_bcast_srv_msg = message;
		gettimeofday(&g_cluster.last_bcast_srv_msg_time, NULL);
	}
	return reply_with_message(wdNode, WD_CLUSTER_SERVICE_MESSAGE, &message, 1, replyFor);
}


static bool
reply_with_message(WatchdogNode * wdNode, char type, char *data, int data_len, WDPacketData * replyFor)
{
	WDPacketData wdPacket;
	int			ret;

	init_wd_packet(&wdPacket);
	set_message_type(&wdPacket, type);

	if (replyFor == NULL)
		set_next_commandID_in_message(&wdPacket);
	else
		set_message_commandID(&wdPacket, replyFor->command_id);

	set_message_data(&wdPacket, data, data_len);
	ret = send_message(wdNode, &wdPacket);
	return ret;
}

static inline WD_STATES get_local_node_state(void)
{
	return g_cluster.localNode->state;
}

static inline bool
is_local_node_true_leader(void)
{
	return (get_local_node_state() == WD_COORDINATOR && WD_LEADER_NODE == g_cluster.localNode);
}

/*
 * returns true if no message is swallowed by the
 * processor and no further action is required
 */
static bool
wd_commands_packet_processor(WD_EVENTS event, WatchdogNode * wdNode, WDPacketData * pkt)
{
	WDCommandData *ipcCommand;

	if (event != WD_EVENT_PACKET_RCV)
		return false;
	if (pkt == NULL)
		return false;

	if (pkt->type == WD_FAILOVER_LOCKING_REQUEST ||
		pkt->type == WD_REMOTE_FAILOVER_REQUEST)
	{
		/* Node is using the older version of Pgpool-II */
		ereport(WARNING,
				(errmsg("node \"%s\" is using the older version of Pgpool-II", wdNode->nodeName)));
		send_cluster_service_message(wdNode, pkt, CLUSTER_NODE_INVALID_VERSION);
		return true;
	}

	if (pkt->type == WD_IPC_FAILOVER_COMMAND)
	{
		process_remote_failover_command_on_coordinator(wdNode, pkt);
		return true;
	}

	if (pkt->type == WD_IPC_ONLINE_RECOVERY_COMMAND)
	{
		process_remote_online_recovery_command(wdNode, pkt);
		return true;
	}

	if (pkt->type == WD_DATA_MESSAGE)
	{
		ipcCommand = get_wd_IPC_command_from_reply(pkt);
		if (ipcCommand)
		{
			if (write_ipc_command_with_result_data(ipcCommand, WD_IPC_CMD_RESULT_OK, pkt->data, pkt->len) == false)
				ereport(LOG,
						(errmsg("failed to forward data message to IPC command socket")));

			cleanUpIPCCommand(ipcCommand);
			return true;		/* do not process this packet further */
		}
		return false;
	}

	if (pkt->type == WD_CMD_REPLY_IN_DATA)
	{
		ipcCommand = get_wd_IPC_command_from_reply(pkt);
		if (ipcCommand == NULL)
			return false;

		/* Just forward the data to IPC socket and finish the command */
		if (write_ipc_command_with_result_data(ipcCommand, WD_IPC_CMD_RESULT_OK, pkt->data, pkt->len) == false)
			ereport(LOG,
					(errmsg("failed to forward data message to IPC command socket")));

		/*
		 * ok we are done, delete this command
		 */
		cleanUpIPCCommand(ipcCommand);
		return true;			/* do not process this packet further */
	}

	else if (pkt->type == WD_ACCEPT_MESSAGE ||
			 pkt->type == WD_REJECT_MESSAGE ||
			 pkt->type == WD_ERROR_MESSAGE)
	{
		ipcCommand = get_wd_IPC_command_from_reply(pkt);

		if (ipcCommand == NULL)
			return false;

		if (ipcCommand->commandPacket.type == WD_IPC_FAILOVER_COMMAND)
		{
			if (pkt->type == WD_ACCEPT_MESSAGE)
				reply_to_failover_command(ipcCommand, FAILOVER_RES_PROCEED, 0);
			else
				reply_to_failover_command(ipcCommand, FAILOVER_RES_LEADER_REJECTED, 0);
			return true;
		}

		else if (ipcCommand->commandPacket.type == WD_IPC_ONLINE_RECOVERY_COMMAND)
		{
			return reply_is_received_for_pgpool_replicate_command(wdNode, pkt, ipcCommand);
		}
	}

	return false;
}


static void
update_interface_status(void)
{
	struct ifaddrs *ifAddrStruct = NULL;
	struct ifaddrs *ifa = NULL;
	ListCell   *lc;

	if (g_cluster.wdInterfaceToMonitor == NULL)
		return;

	getifaddrs(&ifAddrStruct);
	for (ifa = ifAddrStruct; ifa != NULL; ifa = ifa->ifa_next)
	{
		ereport(DEBUG1,
				(errmsg("network interface %s having flags %d", ifa->ifa_name, ifa->ifa_flags)));

		if (!strncasecmp("lo", ifa->ifa_name, 2))
			continue;			/* We do not need loop back addresses */

		foreach(lc, g_cluster.wdInterfaceToMonitor)
		{
			WDInterfaceStatus *if_status = lfirst(lc);

			if (!strcasecmp(if_status->if_name, ifa->ifa_name))
			{
				if_status->if_up = is_interface_up(ifa);
				break;
			}
		}
	}

	if (ifAddrStruct != NULL)
		freeifaddrs(ifAddrStruct);

}

static bool
any_interface_available(void)
{
	ListCell   *lc;

	update_interface_status();
	/* if interface monitoring is disabled we are good */
	if (g_cluster.wdInterfaceToMonitor == NULL)
		return true;

	foreach(lc, g_cluster.wdInterfaceToMonitor)
	{
		WDInterfaceStatus *if_status = lfirst(lc);

		if (if_status->if_up)
		{
			ereport(DEBUG1,
					(errmsg("network interface \"%s\" is up and we can continue", if_status->if_name)));
			return true;
		}
	}
	return false;
}

static int
watchdog_state_machine(WD_EVENTS event, WatchdogNode * wdNode, WDPacketData * pkt, WDCommandData * clusterCommand)
{
	ereport(DEBUG1,
			(errmsg("STATE MACHINE INVOKED WITH EVENT = %s Current State = %s",
					wd_event_name[event], wd_state_names[get_local_node_state()])));

	if (event == WD_EVENT_REMOTE_NODE_LOST)
	{

		if (wdNode->state == WD_SHUTDOWN)
		{
			ereport(LOG,
					(errmsg("remote node \"%s\" is shutting down", wdNode->nodeName)));
			if (pool_config->wd_remove_shutdown_nodes)
				revoke_cluster_membership_of_node(wdNode,WD_NODE_REVOKED_SHUTDOWN);
		}
		else
		{
			wdNode->state = WD_LOST;
			ereport(LOG,
					(errmsg("remote node \"%s\" is lost", wdNode->nodeName)));
			/* Inform the node, that it is lost for us */
			 send_cluster_service_message(wdNode, pkt, CLUSTER_NODE_APPEARING_LOST);
		}
		if (wdNode == WD_LEADER_NODE)
		{
			ereport(LOG,
					(errmsg("watchdog cluster has lost the coordinator node")));
			set_cluster_leader_node(NULL);
		}

		/* close all socket connections to the node */
		close_socket_connection(&wdNode->client_socket);
		close_socket_connection(&wdNode->server_socket);

		/* clear the wait timer on the node */
		wdNode->last_sent_time.tv_sec = 0;
		wdNode->last_sent_time.tv_usec = 0;
		wdNode->sending_failures_count = 0;
		node_lost_while_ipc_command(wdNode);
	}
	else if (event == WD_EVENT_REMOTE_NODE_FOUND)
	{
		ereport(LOG,
				(errmsg("remote node \"%s\" became reachable again", wdNode->nodeName),
				 errdetail("requesting the node info")));
		/*
		 * remove the lost state from the node
		 * and change it to joining for now
		 */
		wdNode->node_lost_reason = NODE_LOST_UNKNOWN_REASON;
		wdNode->state = WD_LOADING;
		send_cluster_service_message(wdNode, pkt, CLUSTER_NODE_APPEARING_FOUND);
		/* if this node was kicked out of quorum calculation. add it back */
		restore_cluster_membership_of_node(wdNode);
	}
	else if (event == WD_EVENT_PACKET_RCV)
	{
		print_packet_node_info(pkt, wdNode, false);
		/* update the last receive time */
		gettimeofday(&wdNode->last_rcv_time, NULL);

		if (pkt->type == WD_INFO_MESSAGE)
		{
			standard_packet_processor(wdNode, pkt);
		}

		if (pkt->type == WD_INFORM_I_AM_GOING_DOWN)
		{
			wdNode->state = WD_SHUTDOWN;
			wdNode->node_lost_reason = NODE_LOST_SHUTDOWN;
			return watchdog_state_machine(WD_EVENT_REMOTE_NODE_LOST, wdNode, NULL, NULL);
		}

		if (watchdog_internal_command_packet_processor(wdNode, pkt) == true)
		{
			return 0;
		}
	}
	else if (event == WD_EVENT_NEW_OUTBOUND_CONNECTION)
	{
		WDPacketData *addPkt = get_addnode_message();

		send_message(wdNode, addPkt);
		free_packet(addPkt);
	}

	else if (event == WD_EVENT_NW_IP_IS_REMOVED || event == WD_EVENT_NW_LINK_IS_INACTIVE)
	{
		List	   *local_addresses;

		/* check if we have an active link */
		if (any_interface_available() == false)
		{
			ereport(WARNING,
					(errmsg("network event has occurred and all monitored interfaces are down"),
					 errdetail("changing the state to in network trouble")));

			set_state(WD_IN_NW_TROUBLE);

		}
		/* check if all IP addresses are lost */
		local_addresses = get_all_local_ips();
		if (local_addresses == NULL)
		{
			/*
			 * We have lost all IP addresses we are in network trouble. Just
			 * move to in network trouble state
			 */
			ereport(WARNING,
					(errmsg("network IP is removed and system has no IP is assigned"),
					 errdetail("changing the state to in network trouble")));

			set_state(WD_IN_NW_TROUBLE);
		}
		else
		{
			ListCell   *lc;

			ereport(DEBUG1,
					(errmsg("network IP is removed but system still has a valid IP is assigned")));
			foreach(lc, local_addresses)
			{
				char	   *ip = lfirst(lc);

				ereport(DEBUG1,
						(errmsg("IP = %s", ip ? ip : "NULL")));
			}

			list_free_deep(local_addresses);
			local_addresses = NULL;
		}
	}

	else if (event == WD_EVENT_LOCAL_NODE_LOST)
	{
		ereport(WARNING,
				(errmsg("watchdog life-check reported, we are disconnected from the network"),
				 errdetail("changing the state to LOST")));
		set_state(WD_LOST);
	}

	if (wd_commands_packet_processor(event, wdNode, pkt) == true)
		return 0;

	switch (get_local_node_state())
	{
		case WD_LOADING:
			watchdog_state_machine_loading(event, wdNode, pkt, clusterCommand);
			break;
		case WD_JOINING:
			watchdog_state_machine_joining(event, wdNode, pkt, clusterCommand);
			break;
		case WD_INITIALIZING:
			watchdog_state_machine_initializing(event, wdNode, pkt, clusterCommand);
			break;
		case WD_COORDINATOR:
			watchdog_state_machine_coordinator(event, wdNode, pkt, clusterCommand);
			break;
		case WD_PARTICIPATE_IN_ELECTION:
			watchdog_state_machine_voting(event, wdNode, pkt, clusterCommand);
			break;
		case WD_STAND_FOR_COORDINATOR:
			watchdog_state_machine_standForCord(event, wdNode, pkt, clusterCommand);
			break;
		case WD_STANDBY:
			watchdog_state_machine_standby(event, wdNode, pkt, clusterCommand);
			break;
		case WD_LOST:
		case WD_IN_NW_TROUBLE:
			watchdog_state_machine_nw_error(event, wdNode, pkt, clusterCommand);
			break;
		case WD_NETWORK_ISOLATION:
			watchdog_state_machine_nw_isolation(event, wdNode, pkt, clusterCommand);
			break;
		default:
			/* Should never ever happen */
			ereport(WARNING,
					(errmsg("invalid watchdog state")));
			set_state(WD_LOADING);
			break;
	}

	return 0;
}

/*
 * This is the state where the watchdog enters when starting up.
 * upon entering this state we sends ADD node message to all reachable
 * nodes.
 * Wait for 4 seconds if some node rejects us.
 */
static int
watchdog_state_machine_loading(WD_EVENTS event, WatchdogNode * wdNode, WDPacketData * pkt, WDCommandData * clusterCommand)
{
	switch (event)
	{
		case WD_EVENT_WD_STATE_CHANGED:
			{
				int			i;
				WDPacketData *addPkt = get_addnode_message();

				/* set the status to ADD_MESSAGE_SEND by hand */
				for (i = 0; i < g_cluster.remoteNodeCount; i++)
				{
					WatchdogNode *wdTmpNode;

					wdTmpNode = &(g_cluster.remoteNodes[i]);
					if (wdTmpNode->client_socket.sock_state == WD_SOCK_CONNECTED && wdTmpNode->state == WD_DEAD)
					{
						if (send_message(wdTmpNode, addPkt))
							wdTmpNode->state = WD_ADD_MESSAGE_SENT;
					}
				}
				free_packet(addPkt);
				set_timeout(MAX_SECS_WAIT_FOR_REPLY_FROM_NODE);
			}
			break;

		case WD_EVENT_TIMEOUT:
			set_state(WD_JOINING);
			break;

		case WD_EVENT_PACKET_RCV:
			{
				switch (pkt->type)
				{
					case WD_STAND_FOR_COORDINATOR_MESSAGE:
						{
							/*
							 * We are loading but a node is already contesting
							 * for coordinator node well we can ignore it but
							 * then this could eventually mean a lower
							 * priority node can became a coordinator node. So
							 * check the priority of the node in stand for
							 * coordinator state
							 */
							if (g_cluster.localNode->wd_priority > wdNode->wd_priority)
							{
								reply_with_minimal_message(wdNode, WD_REJECT_MESSAGE, pkt);
								set_state(WD_STAND_FOR_COORDINATOR);
							}
							else
							{
								reply_with_minimal_message(wdNode, WD_ACCEPT_MESSAGE, pkt);
								set_state(WD_PARTICIPATE_IN_ELECTION);
							}
						}
						break;

					case WD_INFO_MESSAGE:
						{
							int			i;
							bool		all_replied = true;

							for (i = 0; i < g_cluster.remoteNodeCount; i++)
							{
								wdNode = &(g_cluster.remoteNodes[i]);
								if (wdNode->state == WD_ADD_MESSAGE_SENT)
								{
									all_replied = false;
									break;
								}
							}
							if (all_replied)
							{
								/*
								 * we are already connected to all configured
								 * nodes Just move to initializing state
								 */
								set_state(WD_INITIALIZING);
							}
						}
						break;

					case WD_REJECT_MESSAGE:
						if (wdNode->state == WD_ADD_MESSAGE_SENT || wdNode->state == WD_DEAD)
							ereport(FATAL,
									(return_code(POOL_EXIT_FATAL),
									 errmsg("Add to watchdog cluster request is rejected by node \"%s:%d\"", wdNode->hostname, wdNode->wd_port),
									 errhint("check the watchdog configurations.")));
						break;
					default:
						standard_packet_processor(wdNode, pkt);
						break;
				}
			}
			break;
		default:
			break;
	}
	return 0;
}

/*
 * This is the intermediate state before going to cluster initialization
 * here we update the information of all connected nodes and move to the
 * initialization state. moving to this state from loading does not make
 * much sence as at loading time we already have updated node informations
 */
static int
watchdog_state_machine_joining(WD_EVENTS event, WatchdogNode * wdNode, WDPacketData * pkt, WDCommandData * clusterCommand)
{
	switch (event)
	{
		case WD_EVENT_WD_STATE_CHANGED:
			set_cluster_leader_node(NULL);
			try_connecting_with_all_unreachable_nodes();
			send_cluster_command(NULL, WD_REQ_INFO_MESSAGE, 4);
			set_timeout(MAX_SECS_WAIT_FOR_REPLY_FROM_NODE);
			break;

		case WD_EVENT_TIMEOUT:
			set_state(WD_INITIALIZING);
			break;

		case WD_EVENT_COMMAND_FINISHED:
			{
				if (clusterCommand->commandPacket.type == WD_REQ_INFO_MESSAGE)
					set_state(WD_INITIALIZING);
			}
			break;

		case WD_EVENT_PACKET_RCV:
			{
				switch (pkt->type)
				{
					case WD_REJECT_MESSAGE:
						if (wdNode->state == WD_ADD_MESSAGE_SENT)
							ereport(FATAL,
									(return_code(POOL_EXIT_FATAL),
									 errmsg("add to watchdog cluster request is rejected by node \"%s:%d\"", wdNode->hostname, wdNode->wd_port),
									 errhint("check the watchdog configurations.")));
						break;

					case WD_STAND_FOR_COORDINATOR_MESSAGE:
						{
							/*
							 * We are loading but a node is already contesting
							 * for coordinator node well we can ignore it but
							 * then this could eventually mean a lower
							 * priority node can became a coordinator node. So
							 * check the priority of the node in stand for
							 * coordinator state
							 */
							if (g_cluster.localNode->wd_priority > wdNode->wd_priority)
							{
								reply_with_minimal_message(wdNode, WD_REJECT_MESSAGE, pkt);
								set_state(WD_STAND_FOR_COORDINATOR);
							}
							else
							{
								reply_with_minimal_message(wdNode, WD_ACCEPT_MESSAGE, pkt);
								set_state(WD_PARTICIPATE_IN_ELECTION);
							}
						}
						break;

					default:
						standard_packet_processor(wdNode, pkt);
						break;
				}
			}
			break;

		default:
			break;
	}

	return 0;
}

/*
 * This state only works on the local data and does not
 * sends any cluster command.
 */

static int
watchdog_state_machine_initializing(WD_EVENTS event, WatchdogNode * wdNode, WDPacketData * pkt, WDCommandData * clusterCommand)
{
	switch (event)
	{
		case WD_EVENT_WD_STATE_CHANGED:
			/* set 1 sec timeout, save ourself from recursion */
			set_timeout(1);
			break;

		case WD_EVENT_TIMEOUT:
			{
				/*
				 * If leader node exists in cluster, Join it otherwise try
				 * becoming a leader
				 */
				if (WD_LEADER_NODE)
				{
					/*
					 * we found the coordinator node in network. Just join the
					 * network
					 */
					set_state(WD_STANDBY);
				}
				else if (get_cluster_node_count() == 0)
				{
					ereport(LOG,
							(errmsg("I am the only alive node in the watchdog cluster"),
							 errhint("skipping stand for coordinator state")));

					/*
					 * I am the alone node in the cluster at the moment skip
					 * the intermediate steps and jump to the coordinator
					 * state
					 */
					set_state(WD_COORDINATOR);
				}
				else
				{
					int			i;

					for (i = 0; i < g_cluster.remoteNodeCount; i++)
					{
						WatchdogNode *wdNode = &(g_cluster.remoteNodes[i]);

						if (wdNode->state == WD_STAND_FOR_COORDINATOR)
						{
							set_state(WD_PARTICIPATE_IN_ELECTION);
							return 0;
						}
					}
					/* stand for coordinator */
					set_state(WD_STAND_FOR_COORDINATOR);
				}
			}
			break;

		case WD_EVENT_PACKET_RCV:
			{
				switch (pkt->type)
				{
					case WD_REJECT_MESSAGE:
						if (wdNode->state == WD_ADD_MESSAGE_SENT)
							ereport(FATAL,
									(return_code(POOL_EXIT_FATAL),
									 errmsg("Add to watchdog cluster request is rejected by node \"%s:%d\"", wdNode->hostname, wdNode->wd_port),
									 errhint("check the watchdog configurations.")));
						break;
					default:
						standard_packet_processor(wdNode, pkt);
						break;
				}
			}

			break;

		default:
			break;
	}
	return 0;
}

static int
watchdog_state_machine_standForCord(WD_EVENTS event, WatchdogNode * wdNode, WDPacketData * pkt, WDCommandData * clusterCommand)
{
	switch (event)
	{
		case WD_EVENT_WD_STATE_CHANGED:
			send_cluster_command(NULL, WD_STAND_FOR_COORDINATOR_MESSAGE, 4);
			/* wait for 5 seconds if someone rejects us */
			set_timeout(MAX_SECS_WAIT_FOR_REPLY_FROM_NODE);
			break;

		case WD_EVENT_COMMAND_FINISHED:
			{
				if (clusterCommand->commandPacket.type == WD_STAND_FOR_COORDINATOR_MESSAGE)
				{
					if (clusterCommand->commandStatus == COMMAND_FINISHED_ALL_REPLIED ||
						clusterCommand->commandStatus == COMMAND_FINISHED_TIMEOUT)
					{
						set_state(WD_COORDINATOR);
					}
					else
					{
						/* command finished with an error */
						if (pkt)
						{
							if (pkt->type == WD_ERROR_MESSAGE)
							{
								ereport(LOG,
										(errmsg("our stand for coordinator request is rejected by node \"%s\"",wdNode->nodeName),
										 errdetail("we might be in partial network isolation and cluster already have a valid leader"),
										 errhint("please verify the watchdog life-check and network is working properly")));
								set_state(WD_NETWORK_ISOLATION);
							}
							else if (pkt->type == WD_REJECT_MESSAGE)
							{
								ereport(LOG,
										(errmsg("our stand for coordinator request is rejected by node \"%s\"", wdNode->nodeName)));
								set_state(WD_PARTICIPATE_IN_ELECTION);
							}
						}
						else
						{
							ereport(LOG,
									(errmsg("our stand for coordinator request is rejected by node \"%s\"", wdNode->nodeName)));
							set_state(WD_JOINING);
						}
					}
				}
			}
			break;

		case WD_EVENT_TIMEOUT:
			set_state(WD_COORDINATOR);
			break;

		case WD_EVENT_PACKET_RCV:
			{
				switch (pkt->type)
				{
					case WD_STAND_FOR_COORDINATOR_MESSAGE:
						/* decide on base of priority */
						if (g_cluster.localNode->wd_priority > wdNode->wd_priority)
						{
							reply_with_minimal_message(wdNode, WD_REJECT_MESSAGE, pkt);
						}
						else if (g_cluster.localNode->wd_priority == wdNode->wd_priority)
						{
							/* decide on base of starting time */
							if (g_cluster.localNode->startup_time.tv_sec <= wdNode->startup_time.tv_sec)	/* I am older */
							{
								reply_with_minimal_message(wdNode, WD_REJECT_MESSAGE, pkt);
							}
							else
							{
								reply_with_minimal_message(wdNode, WD_ACCEPT_MESSAGE, pkt);
								set_state(WD_PARTICIPATE_IN_ELECTION);
							}
						}
						else
						{
							reply_with_minimal_message(wdNode, WD_ACCEPT_MESSAGE, pkt);
							set_state(WD_PARTICIPATE_IN_ELECTION);
						}
						break;

					case WD_DECLARE_COORDINATOR_MESSAGE:
						{
							/*
							 * meanwhile someone has declared itself
							 * coordinator
							 */
							if (g_cluster.localNode->wd_priority > wdNode->wd_priority)
							{
								ereport(LOG,
										(errmsg("rejecting the declare coordinator request from node \"%s\"", wdNode->nodeName),
										 errdetail("my wd_priority [%d] is higher than the requesting node's priority [%d]", g_cluster.localNode->wd_priority, wdNode->wd_priority)));
								reply_with_minimal_message(wdNode, WD_REJECT_MESSAGE, pkt);
							}
							else
							{
								ereport(LOG,
										(errmsg("node \"%s\" has declared itself as a coordinator", wdNode->nodeName)));
								reply_with_minimal_message(wdNode, WD_ACCEPT_MESSAGE, pkt);
								set_state(WD_JOINING);
							}
						}
						break;
					default:
						standard_packet_processor(wdNode, pkt);
						break;
				}
			}
			break;

		default:
			break;
	}
	return 0;
}

/*
 * Event handler for the coordinator/leader state.
 * The function handels all the event received when the local
 * node is the leader/coordinator node.
 */
static int
watchdog_state_machine_coordinator(WD_EVENTS event, WatchdogNode * wdNode, WDPacketData * pkt, WDCommandData * clusterCommand)
{
	switch (event)
	{
		case WD_EVENT_WD_STATE_CHANGED:
			{
				send_cluster_command(NULL, WD_DECLARE_COORDINATOR_MESSAGE, 4);
				set_timeout(MAX_SECS_WAIT_FOR_REPLY_FROM_NODE);
				update_missed_beacon_count(NULL,true);
				update_failover_timeout(g_cluster.localNode, pool_config);

				ereport(LOG,
						(errmsg("I am announcing my self as leader/coordinator watchdog node")));
				if (message_level_is_interesting(DEBUG2))
				{
					int			i;
					ereport(DEBUG2,
							(errmsg("printing all remote node information")));
					for (i = 0; i < g_cluster.remoteNodeCount; i++)
					{
						WatchdogNode *wdNode = &(g_cluster.remoteNodes[i]);

						print_watchdog_node_info(wdNode);
					}
				}
				/* Also reset my priority as per the original configuration */
				g_cluster.localNode->wd_priority = pool_config->wd_priority;
			}
			break;

		case WD_EVENT_COMMAND_FINISHED:
			{
				if (clusterCommand->commandPacket.type == WD_DECLARE_COORDINATOR_MESSAGE)
				{
					if (clusterCommand->commandStatus == COMMAND_FINISHED_ALL_REPLIED ||
						clusterCommand->commandStatus == COMMAND_FINISHED_TIMEOUT)
					{
						update_cluster_memberships();
						update_quorum_status();
						reset_lost_timers();
						ereport(DEBUG1,
								(errmsg("declare coordinator command finished with status:[%s]",
										clusterCommand->commandStatus == COMMAND_FINISHED_ALL_REPLIED ?
										"ALL NODES REPLIED" :
										"COMMAND TIMED OUT"),
								 errdetail("The command was sent to %d nodes and %d nodes replied to it",
										   clusterCommand->commandSendToCount,
										   clusterCommand->commandReplyFromCount
										   )));

						ereport(LOG,
								(errmsg("I am the cluster leader node"),
								 errdetail("our declare coordinator message is accepted by all nodes")));

						set_cluster_leader_node(g_cluster.localNode);
						register_watchdog_state_change_interrupt();

						/*
						 * Check if the quorum is present then start the
						 * escalation process otherwise keep in the
						 * coordinator state and wait for the quorum
						 */
						if (g_cluster.quorum_status == -1)
						{
							ereport(LOG,
									(errmsg("I am the cluster leader node but we do not have enough nodes in cluster"),
									 errdetail("waiting for the quorum to start escalation process")));
						}
						else
						{
							ereport(LOG,
									(errmsg("I am the cluster leader node. Starting escalation process")));
							start_escalated_node();
						}
					}
					else
					{
						/* command is finished but because of error */
						ereport(NOTICE,
								(errmsg("possible split brain scenario detected by \"%s\" node", wdNode->nodeName),
								 (errdetail("re-initializing cluster"))));
						set_state(WD_JOINING);
					}
				}

				else if (clusterCommand->commandPacket.type == WD_IAM_COORDINATOR_MESSAGE)
				{
					update_missed_beacon_count(clusterCommand,false);

					if (clusterCommand->commandStatus == COMMAND_FINISHED_ALL_REPLIED)
					{
						ereport(DEBUG1,
								(errmsg("I am the cluster leader node command finished with status:[ALL NODES REPLIED]"),
								 errdetail("The command was sent to %d nodes and %d nodes replied to it",
										   clusterCommand->commandSendToCount,
										   clusterCommand->commandReplyFromCount
										   )));
					}
					else if (clusterCommand->commandStatus == COMMAND_FINISHED_TIMEOUT)
					{
						ereport(DEBUG1,
								(errmsg("I am the cluster leader node command finished with status:[COMMAND TIMED OUT] which is success"),
								 errdetail("The command was sent to %d nodes and %d nodes replied to it",
										   clusterCommand->commandSendToCount,
										   clusterCommand->commandReplyFromCount
										   )));
					}
					else if (clusterCommand->commandStatus == COMMAND_FINISHED_NODE_REJECTED)
					{
						/*
						 * one of the node rejected out I am coordinator
						 * message
						 */
						ereport(LOG,
								(errmsg("possible split brain, \"%s\" node has rejected our coordinator beacon", wdNode->nodeName),
								 (errdetail("removing the node from out standby list"))));

						standby_node_left_cluster(wdNode);
					}
				}
			}
			break;

		case WD_EVENT_CLUSTER_QUORUM_CHANGED:
			{
				/* make sure we are accepted as leader */
				if (WD_LEADER_NODE == g_cluster.localNode)
				{
					if (g_cluster.quorum_status == -1)
					{
						ereport(LOG,
								(errmsg("We have lost the quorum")));

						/*
						 * We have lost the quorum, stay as a leader node but
						 * perform de-escalation. As keeping the VIP may
						 * result in split-brain
						 */
						resign_from_escalated_node();
					}
					else if (g_cluster.quorum_status >= 0)
					{
						if (g_cluster.localNode->escalated == false)
						{
							ereport(LOG,
									(errmsg("quorum found"),
									 errdetail("starting escalation process")));
							start_escalated_node();
						}
					}
					/* inform to the cluster about the new quorum status */
					send_message_of_type(NULL, WD_INFO_MESSAGE, NULL);
					register_watchdog_quorum_change_interrupt();
				}
			}
			break;

		case WD_EVENT_NW_IP_IS_REMOVED:
			{
				/* check if we were holding the virtual IP and it is now lost */
				List	   *local_addresses = get_all_local_ips();

				if (local_addresses == NULL)
				{
					/*
					 * We have lost all IP addresses we are in network
					 * trouble. Just move to in network trouble state
					 */
					set_state(WD_IN_NW_TROUBLE);
				}
				else
				{
					/*
					 * We do have some IP addresses assigned so its not a
					 * total black-out check if we still have the VIP assigned
					 */
					if (g_cluster.clusterLeaderInfo.holding_vip == true)
					{
						ListCell   *lc;
						bool		vip_exists = false;

						foreach(lc, local_addresses)
						{
							char	   *ip = lfirst(lc);

							if (!strcmp(ip, g_cluster.localNode->delegate_ip))
							{
								vip_exists = true;
								break;
							}
						}
						if (vip_exists == false)
						{
							/*
							 * Okay this is the case when only our VIP is lost
							 * but network interface seems to be working fine
							 * try to re-acquire the VIP
							 */
							wd_IP_up();
						}
					}
					list_free_deep(local_addresses);
					local_addresses = NULL;
				}
			}
			break;

		case WD_EVENT_NW_IP_IS_ASSIGNED:
			break;

		case WD_EVENT_TIMEOUT:
			{
				if (check_debug_request_do_not_send_beacon() == false)
					send_cluster_command(NULL, WD_IAM_COORDINATOR_MESSAGE, 5);
				set_timeout(BEACON_MESSAGE_INTERVAL_SECONDS);
			}
			break;

		case WD_EVENT_I_AM_APPEARING_LOST:
		{
			/* The remote node has lost us, It would have already marked
			 * us as lost, So remove it from standby*/
			standby_node_left_cluster(wdNode);
		}
			break;

		case WD_EVENT_I_AM_APPEARING_FOUND:
		{
			/* The remote node has found us again */
			if (wdNode->wd_data_major_version >= 1 && wdNode->wd_data_minor_version >= 1)
			{
				/*
				 * Since data version 1.1 we support CLUSTER_NODE_REQUIRE_TO_RELOAD
				 * which makes the standby nodes to re-send the join leader node
				 */
				ereport(DEBUG1,
					(errmsg("asking remote node \"%s\" to rejoin leader", wdNode->nodeName),
						errdetail("watchdog data version %s",WD_MESSAGE_DATA_VERSION)));

				send_cluster_service_message(wdNode, pkt, CLUSTER_NODE_REQUIRE_TO_RELOAD);
			}
			else
			{
				/*
				 * The node is on older version
				 * So ask it to re-join the cluster
				 */
				ereport(DEBUG1,
					(errmsg("asking remote node \"%s\" to rejoin cluster", wdNode->nodeName),
						errdetail("watchdog data version %s",WD_MESSAGE_DATA_VERSION)));
				send_cluster_service_message(wdNode, pkt, CLUSTER_NEEDS_ELECTION);
			}
		}
			break;

		case WD_EVENT_REMOTE_NODE_LOST:
			{
				standby_node_left_cluster(wdNode);
			}
			break;

		case WD_EVENT_REMOTE_NODE_FOUND:
		{
			ereport(LOG,
				(errmsg("remote node \"%s\" is reachable again", wdNode->nodeName),
					errdetail("trying to add it back as a standby")));
			wdNode->node_lost_reason = NODE_LOST_UNKNOWN_REASON;
			/* If I am the cluster leader. Ask for the node info and to re-send the join message */
			send_message_of_type(wdNode, WD_REQ_INFO_MESSAGE, NULL);
			if (wdNode->wd_data_major_version >= 1 && wdNode->wd_data_minor_version >= 1)
			{
				/*
				 * Since data version 1.1 we support CLUSTER_NODE_REQUIRE_TO_RELOAD
				 * which makes the standby nodes to re-send the join leader node
				 */
				ereport(DEBUG1,
					(errmsg("asking remote node \"%s\" to rejoin leader", wdNode->nodeName),
						errdetail("watchdog data version %s",WD_MESSAGE_DATA_VERSION)));

				send_cluster_service_message(wdNode, pkt, CLUSTER_NODE_REQUIRE_TO_RELOAD);
			}
			else
			{
				/*
				 * The node is on older version
				 * So ask it to re-join the cluster
				 */
				ereport(DEBUG1,
					(errmsg("asking remote node \"%s\" to rejoin cluster", wdNode->nodeName),
						errdetail("watchdog data version %s",WD_MESSAGE_DATA_VERSION)));
				send_cluster_service_message(wdNode, pkt, CLUSTER_NEEDS_ELECTION);
			}
			break;
		}

		case WD_EVENT_PACKET_RCV:
			{
				switch (pkt->type)
				{
					case WD_ADD_NODE_MESSAGE:
						/* In case we received the ADD node message from
						 * one of our standby, Remove that standby from
						 * the list
						 */
						standby_node_left_cluster(wdNode);
						standard_packet_processor(wdNode, pkt);
						break;

					case WD_STAND_FOR_COORDINATOR_MESSAGE:
						reply_with_minimal_message(wdNode, WD_REJECT_MESSAGE, pkt);
						break;
					case WD_DECLARE_COORDINATOR_MESSAGE:
						ereport(NOTICE,
								(errmsg("We are coordinator and another node tried a coup")));
						reply_with_minimal_message(wdNode, WD_ERROR_MESSAGE, pkt);
						break;

					case WD_IAM_COORDINATOR_MESSAGE:
						{
							ereport(NOTICE,
									(errmsg("We are in split brain, I AM COORDINATOR MESSAGE received from \"%s\" node", wdNode->nodeName)));

							if (beacon_message_received_from_node(wdNode, pkt) == true)
							{
								handle_split_brain(wdNode, pkt);
							}
							else
							{
								/*
								 * we are not able to decide which should be
								 * the best candidate to stay as
								 * leader/coordinator node This could also
								 * happen if the remote node is using the
								 * older version of Pgpool-II which send the
								 * empty beacon messages.
								 */
								ereport(LOG,
										(errmsg("We are in split brain, and not able to decide the best candidate for leader/coordinator"),
										 errdetail("re-initializing the local watchdog cluster state")));

								send_cluster_service_message(wdNode, pkt, CLUSTER_NEEDS_ELECTION);
								set_state(WD_JOINING);
							}
						}
						break;

					case WD_JOIN_COORDINATOR_MESSAGE:
						{
							/*
							 * If the node is marked as lost because of
							 * life-check, Do not let it join the cluster
							 */
							if (wdNode->state == WD_LOST && wdNode->node_lost_reason == NODE_LOST_BY_LIFECHECK)
							{
								ereport(LOG,
										(errmsg("lost remote node \"%s\" is requesting to join the cluster",wdNode->nodeName),
										 errdetail("rejecting the request until life-check inform us that it is reachable again")));
								reply_with_minimal_message(wdNode, WD_REJECT_MESSAGE, pkt);
							}
							else
							{
								reply_with_minimal_message(wdNode, WD_ACCEPT_MESSAGE, pkt);
								/* Also get the configurations from the standby node */
								send_message_of_type(wdNode,WD_ASK_FOR_POOL_CONFIG,NULL);
								standby_node_join_cluster(wdNode);
							}
						}
						break;

					default:
						standard_packet_processor(wdNode, pkt);
						break;
				}
			}
			break;

		default:
			break;
	}
	return 0;
}

/*
 * We can get into this state if we detect the total
 * network blackout, Here we just keep waiting for the
 * network to come back, and when it does we re-initialize
 * the cluster state.
 *
 * Note:
 *
 * All this is very good to detect the network black out or cable unplugged
 * scenarios, and moving to the WD_IN_NW_TROUBLE state. Although this state machine
 * function can gracefully handle the network black out situation and recovers the
 * watchdog node when the network becomes reachable, but there is a problem.
 *
 * Once the cable on the system is unplugged or when the node gets isolated from the
 * cluster there is every likelihood that the backend health-check of the isolated node
 * start reporting the backend node failure and the pgpool-II proceeds to perform
 * the failover for all attached backend nodes. Since the pgpool-II is yet not
 * smart enough to figure out it is because of the network failure of its own
 * system and the backend nodes are not actually at fault but, are working properly.
 *
 * So now when the network gets back the backend status of the node will be different
 * and incorrect from the other pgpool-II nodes in the cluster. So the ideal solution
 * for the situation is to make the pgpool-II main process aware of the network black out
 * and when the network recovers the pgpool-II asks the watchdog to sync again the state of
 * all configured backend nodes from the leader pgpool-II node. But to implement this lot
 * of time is required, So until that time we are just opting for the easiest solution here
 * which is to commit a suicide as soon an the network becomes unreachable
 */
static int
watchdog_state_machine_nw_error(WD_EVENTS event, WatchdogNode * wdNode, WDPacketData * pkt, WDCommandData * clusterCommand)
{
	switch (event)
	{
		case WD_EVENT_WD_STATE_CHANGED:
			/* commit suicide, see above note */
			ereport(FATAL,
					(return_code(POOL_EXIT_FATAL),
					 errmsg("system has lost the network")));

			set_timeout(2);
			break;

		case WD_EVENT_PACKET_RCV:

			/*
			 * Okay this is funny because according to us we are in network
			 * black out but yet we are able to receive the packet. Just check
			 * may be network is back and we are unable to detect it
			 */
			/* fall through */
		case WD_EVENT_TIMEOUT:
		case WD_EVENT_NW_IP_IS_ASSIGNED:
			{
				List	   *local_addresses = get_all_local_ips();

				if (local_addresses == NULL)
				{
					/*
					 * How come this is possible ?? but if somehow this
					 * happens keep in the state and ignore the packet
					 */
				}
				else
				{
					/*
					 * Seems like the network is back just go on initialize
					 * the cluster
					 */
					/*
					 * we might have broken sockets when the network gets
					 * back. Send the request info message to all nodes to
					 * confirm socket state
					 */
					WDPacketData *pkt = get_minimum_message(WD_IAM_IN_NW_TROUBLE_MESSAGE, NULL);

					send_message(NULL, pkt);
					try_connecting_with_all_unreachable_nodes();
					pfree(pkt);
					list_free_deep(local_addresses);
					local_addresses = NULL;
					set_state(WD_LOADING);
				}
			}
			break;

		default:
			break;
	}
	return 0;
}

/*
 * we could end up in tis state if we were connected to the
 * leader node as standby and got lost on the leader.
 * Here we just wait for BEACON_MESSAGE_INTERVAL_SECONDS
 * and retry to join the cluster.
 */
static int
watchdog_state_machine_nw_isolation(WD_EVENTS event, WatchdogNode * wdNode, WDPacketData * pkt, WDCommandData * clusterCommand)
{
	switch (event)
	{
		case WD_EVENT_WD_STATE_CHANGED:
			set_timeout(BEACON_MESSAGE_INTERVAL_SECONDS);
			break;

		case WD_EVENT_PACKET_RCV:
			standard_packet_processor(wdNode, pkt);
			break;

		case WD_EVENT_REMOTE_NODE_FOUND:
		case WD_EVENT_WD_STATE_REQUIRE_RELOAD:
		case WD_EVENT_I_AM_APPEARING_FOUND:
		case WD_EVENT_TIMEOUT:
			/* fall through */
		case WD_EVENT_NW_IP_IS_ASSIGNED:
			ereport(LOG,
				(errmsg("trying again to join the cluster")));
			set_state(WD_JOINING);
			break;

		default:
			break;
	}
	return 0;
}

static bool
beacon_message_received_from_node(WatchdogNode * wdNode, WDPacketData * pkt)
{
	long		seconds_since_node_startup;
	long		seconds_since_current_state;
	int			quorum_status;
	int			standby_nodes_count;
	bool		escalated;
	int			state;
	struct timeval current_time;

	gettimeofday(&current_time, NULL);

	if (pkt->data == NULL || pkt->len <= 0)
		return false;

	if (parse_beacon_message_json(pkt->data, pkt->len,
								  &state,
								  &seconds_since_node_startup,
								  &seconds_since_current_state,
								  &quorum_status,
								  &standby_nodes_count,
								  &escalated) == false)
	{
		return false;
	}

	wdNode->current_state_time.tv_sec = current_time.tv_sec - seconds_since_current_state;
	wdNode->startup_time.tv_sec = current_time.tv_sec - seconds_since_node_startup;
	wdNode->current_state_time.tv_usec = wdNode->startup_time.tv_usec = 0;
	wdNode->quorum_status = quorum_status;
	wdNode->standby_nodes_count = standby_nodes_count;
	wdNode->state = state;
	wdNode->escalated = escalated;
	return true;
}

/*
 * This function decides the best contender for a coordinator/leader node
 * when the remote node info states it is a coordinator while
 * the local node is also in the leader/coordinator state.
 *
 * return:
 * -1 : remote node is the best candidate to remain as leader
 *  0 : both local and remote nodes are not worthy leader or error
 *  1 : local node should remain as the leader/coordinator
 */
static int
I_am_leader_and_cluster_in_split_brain(WatchdogNode * otherLeaderNode)
{
	if (get_local_node_state() != WD_COORDINATOR)
		return 0;
	if (otherLeaderNode->state != WD_COORDINATOR)
		return 0;

	if (otherLeaderNode->current_state_time.tv_sec == 0)
	{
		ereport(LOG,
				(errmsg("not enough data to decide the leader node"),
				 errdetail("the watchdog node:\"%s\" is using the older version of Pgpool-II", otherLeaderNode->nodeName)));
		return 0;
	}

	/* Decide which node should stay as leader */
	if (otherLeaderNode->escalated != g_cluster.localNode->escalated)
	{
		if (otherLeaderNode->escalated == true && g_cluster.localNode->escalated == false)
		{
			/* remote node stays as the leader */
			ereport(LOG,
					(errmsg("remote node:\"%s\" is best suitable to stay as leader because it is escalated and I am not",
							otherLeaderNode->nodeName)));
			return -1;
		}
		else
		{
			/* local node stays as leader */
			ereport(LOG,
					(errmsg("remote node:\"%s\" should step down from leader because it is not escalated",
							otherLeaderNode->nodeName)));
			return 1;
		}
	}
	else if (otherLeaderNode->quorum_status != g_cluster.quorum_status)
	{
		if (otherLeaderNode->quorum_status > g_cluster.quorum_status)
		{
			/* quorum of remote node is in better state */
			ereport(LOG,
					(errmsg("remote node:\"%s\" is best suitable to stay as leader because it holds the quorum"
							,otherLeaderNode->nodeName)));

			return -1;
		}
		else
		{
			/* local node stays as leader */
			ereport(LOG,
					(errmsg("remote node:\"%s\" should step down from leader because it does not hold the quorum"
							,otherLeaderNode->nodeName)));
			return 1;
		}
	}
	else if (otherLeaderNode->standby_nodes_count != g_cluster.clusterLeaderInfo.standby_nodes_count)
	{
		if (otherLeaderNode->standby_nodes_count > g_cluster.clusterLeaderInfo.standby_nodes_count)
		{
			/* remote node has more alive nodes */
			ereport(LOG,
					(errmsg("remote node:\"%s\" is best suitable to stay as leader because it has more connected standby nodes"
							,otherLeaderNode->nodeName)));
			return -1;
		}
		else
		{
			/* local node stays as leader */
			ereport(LOG,
					(errmsg("remote node:\"%s\" should step down from leader because we have more connected standby nodes"
							,otherLeaderNode->nodeName)));
			return 1;
		}
	}
	else						/* decide on which node is the older master */
	{
		if (otherLeaderNode->current_state_time.tv_sec < g_cluster.localNode->current_state_time.tv_sec)
		{
			/* remote node has more alive nodes */
			ereport(LOG,
					(errmsg("remote node:\"%s\" is best suitable to stay as leader because it is the older leader"
							,otherLeaderNode->nodeName)));

			return -1;
		}
		else
		{
			/* local node should keep the leader status */
			ereport(LOG,
					(errmsg("remote node:\"%s\" should step down from leader because we are the older leader"
							,otherLeaderNode->nodeName)));

			return 1;
		}
	}
	return 0;					/* keep the compiler quite */
}

static void
handle_split_brain(WatchdogNode * otherLeaderNode, WDPacketData * pkt)
{
	int			decide_leader = I_am_leader_and_cluster_in_split_brain(otherLeaderNode);

	if (decide_leader == 0)
	{
		/*
		 * we are not able to decide which should be the best candidate to
		 * stay as leader/coordinator node This could also happen if the
		 * remote node is using the older version of Pgpool-II which send the
		 * empty beacon messages.
		 */
		ereport(LOG,
				(errmsg("We are in split brain, and not able to decide the best candidate for leader/coordinator"),
				 errdetail("re-initializing the local watchdog cluster state")));
		send_cluster_service_message(otherLeaderNode, pkt, CLUSTER_NEEDS_ELECTION);
		set_state(WD_JOINING);
	}
	else if (decide_leader == -1)
	{
		/* Remote node is the best candidate for the leader node */
		ereport(LOG,
				(errmsg("We are in split brain, and \"%s\" node is the best candidate for leader/coordinator"
						,otherLeaderNode->nodeName),
				 errdetail("re-initializing the local watchdog cluster state")));
		/* broadcast the message about I am not the true leader node */
		send_cluster_service_message(NULL, pkt, CLUSTER_IAM_NOT_TRUE_LEADER);
		set_state(WD_JOINING);
	}
	else
	{
		/* I am the best candidate for the leader node */
		ereport(LOG,
				(errmsg("We are in split brain, and I am the best candidate for leader/coordinator"),
				 errdetail("asking the remote node \"%s\" to step down", otherLeaderNode->nodeName)));
		send_cluster_service_message(otherLeaderNode, pkt, CLUSTER_IAM_TRUE_LEADER);
	}

}

static void
start_escalated_node(void)
{
	int			wait_secs = MAX_SECS_ESC_PROC_EXIT_WAIT;

	if (g_cluster.localNode->escalated == true) /* already escalated */
		return;

	while (g_cluster.de_escalation_pid > 0 && wait_secs-- > 0)
	{
		/*
		 * de_escalation process was already running and we are escalating
		 * again. give some time to de-escalation process to exit normally
		 */
		ereport(LOG,
				(errmsg("waiting for de-escalation process to exit before starting escalation")));
		if (sigchld_request)
			wd_child_signal_handler();
		sleep(1);
	}
	if (g_cluster.de_escalation_pid > 0)
		ereport(LOG,
				(errmsg("de-escalation process does not exited in time."),
				 errdetail("starting the escalation anyway")));

	g_cluster.escalation_pid = fork_escalation_process();
	if (g_cluster.escalation_pid > 0)
	{
		g_cluster.localNode->escalated = true;
		set_watchdog_node_escalated();
		ereport(LOG,
				(errmsg("escalation process started with PID:%d", g_cluster.escalation_pid)));
		if (strlen(g_cluster.localNode->delegate_ip) > 0)
			g_cluster.clusterLeaderInfo.holding_vip = true;
	}
	else
	{
		ereport(LOG,
				(errmsg("failed to start escalation process")));
	}
}

static void
resign_from_escalated_node(void)
{
	int			wait_secs = MAX_SECS_ESC_PROC_EXIT_WAIT;

	if (g_cluster.localNode->escalated == false)
		return;

	while (g_cluster.escalation_pid > 0 && wait_secs-- > 0)
	{
		/*
		 * escalation process was already running and we are resigning from
		 * it. wait for the escalation process to exit normally
		 */
		ereport(LOG,
				(errmsg("waiting for escalation process to exit before starting de-escalation")));
		if (sigchld_request)
			wd_child_signal_handler();
		sleep(1);
	}
	if (g_cluster.escalation_pid > 0)
		ereport(LOG,
				(errmsg("escalation process does not exited in time"),
				 errdetail("starting the de-escalation anyway")));
	g_cluster.de_escalation_pid = fork_plunging_process();
	g_cluster.clusterLeaderInfo.holding_vip = false;
	g_cluster.localNode->escalated = false;
	reset_watchdog_node_escalated();
}

/*
 * state machine function for state participate in elections
 */
static int
watchdog_state_machine_voting(WD_EVENTS event, WatchdogNode * wdNode, WDPacketData * pkt, WDCommandData * clusterCommand)
{
	switch (event)
	{
		case WD_EVENT_WD_STATE_CHANGED:
			set_timeout(MAX_SECS_WAIT_FOR_REPLY_FROM_NODE);
			break;

		case WD_EVENT_TIMEOUT:
			set_state(WD_JOINING);
			break;

		case WD_EVENT_PACKET_RCV:
			{
				if (pkt == NULL)
				{
					ereport(LOG,
							(errmsg("packet is NULL")));
					break;
				}
				switch (pkt->type)
				{
					case WD_STAND_FOR_COORDINATOR_MESSAGE:
						{
							/* Check the node priority */
							if (wdNode->wd_priority >= g_cluster.localNode->wd_priority)
							{
								reply_with_minimal_message(wdNode, WD_ACCEPT_MESSAGE, pkt);
							}
							else
							{
								reply_with_minimal_message(wdNode, WD_REJECT_MESSAGE, pkt);
								set_state(WD_STAND_FOR_COORDINATOR);
							}
						}
						break;
					case WD_IAM_COORDINATOR_MESSAGE:
						set_state(WD_JOINING);
						break;
					case WD_DECLARE_COORDINATOR_MESSAGE:
						/* Check the node priority */
						if (wdNode->wd_priority >= g_cluster.localNode->wd_priority)
						{
							reply_with_minimal_message(wdNode, WD_ACCEPT_MESSAGE, pkt);
							set_state(WD_INITIALIZING);
						}
						else
						{
							reply_with_minimal_message(wdNode, WD_REJECT_MESSAGE, pkt);
							set_state(WD_STAND_FOR_COORDINATOR);
						}
						break;
					default:
						standard_packet_processor(wdNode, pkt);
						break;
				}
			}
			break;

		default:
			break;
	}
	return 0;
}

static int
watchdog_state_machine_standby(WD_EVENTS event, WatchdogNode * wdNode, WDPacketData * pkt, WDCommandData * clusterCommand)
{
	switch (event)
	{
		case WD_EVENT_WD_STATE_CHANGED:
			send_cluster_command(WD_LEADER_NODE, WD_JOIN_COORDINATOR_MESSAGE, 5);
			/* Also reset my priority as per the original configuration */
			g_cluster.localNode->wd_priority = pool_config->wd_priority;
			set_timeout(BEACON_MESSAGE_INTERVAL_SECONDS);
			break;

		case WD_EVENT_TIMEOUT:
			set_timeout(BEACON_MESSAGE_INTERVAL_SECONDS);
			break;

		case WD_EVENT_WD_STATE_REQUIRE_RELOAD:

			ereport(LOG,
					(errmsg("re-sending join coordinator message to leader node: \"%s\"", WD_LEADER_NODE->nodeName)));

			send_cluster_command(WD_LEADER_NODE, WD_JOIN_COORDINATOR_MESSAGE, 5);
			break;

		case WD_EVENT_COMMAND_FINISHED:
		{
			if (clusterCommand->commandPacket.type == WD_JOIN_COORDINATOR_MESSAGE)
			{
				if (clusterCommand->commandStatus == COMMAND_FINISHED_ALL_REPLIED ||
					clusterCommand->commandStatus == COMMAND_FINISHED_TIMEOUT)
				{
					register_watchdog_state_change_interrupt();

					ereport(LOG,
							(errmsg("successfully joined the watchdog cluster as standby node"),
							 errdetail("our join coordinator request is accepted by cluster leader node \"%s\"", WD_LEADER_NODE->nodeName)));
					/* broadcast our new state change to the cluster */
					send_message_of_type(NULL, WD_INFO_MESSAGE, NULL);

				}
				else
				{
					ereport(NOTICE,
							(errmsg("our join coordinator is rejected by node \"%s\"", wdNode->nodeName),
							 errhint("rejoining the cluster.")));

					if (WD_LEADER_NODE->has_lost_us)
					{
						ereport(LOG,
								(errmsg("leader node \"%s\" thinks we are lost, and \"%s\" is not letting us join",WD_LEADER_NODE->nodeName,wdNode->nodeName),
								 errhint("please verify the watchdog life-check and network is working properly")));
						set_state(WD_NETWORK_ISOLATION);
					}
					else
					{
						set_state(WD_JOINING);
					}
				}
			}
		}
			break;

		case WD_EVENT_I_AM_APPEARING_LOST:
		{
			/* The remote node has lost us, and if it
			 * was our coordinator we might already be
			 * removed from it's standby list
			 * So re-Join the cluster
			 */
			if (WD_LEADER_NODE == wdNode)
			{
				ereport(LOG,
						(errmsg("we are lost on the leader node \"%s\"",wdNode->nodeName)));
				set_state(WD_JOINING);
			}
		}
			break;

		case WD_EVENT_I_AM_APPEARING_FOUND:
			{
				ereport(DEBUG1,
					(errmsg("updating remote node \"%s\" with node info message", wdNode->nodeName)));

				send_message_of_type(wdNode, WD_INFO_MESSAGE, NULL);
			}
			break;

		case WD_EVENT_REMOTE_NODE_LOST:
			{
				/*
				 * we have lost one remote connected node check if the node
				 * was coordinator
				 */
				if (WD_LEADER_NODE == NULL)
				{
					ereport(LOG,
							(errmsg("We have lost the cluster leader node \"%s\"", wdNode->nodeName)));
					set_state(WD_JOINING);
				}
			}
			break;

		case WD_EVENT_PACKET_RCV:
			{
				switch (pkt->type)
				{
					case WD_ADD_NODE_MESSAGE:
					{
						/* In case we received the ADD node message from
						 * our coordinator. Reset the cluster state
						 */
						if (wdNode == WD_LEADER_NODE)
						{
							ereport(LOG,
									(errmsg("received ADD NODE message from the leader node \"%s\"", wdNode->nodeName),
									 errdetail("re-joining the cluster")));
							set_state(WD_JOINING);
						}
						standard_packet_processor(wdNode, pkt);
					}
						break;

					case WD_FAILOVER_END:
						{
							register_backend_state_sync_req_interrupt();
						}
						break;

					case WD_STAND_FOR_COORDINATOR_MESSAGE:
						{
							if (WD_LEADER_NODE == NULL)
							{
								reply_with_minimal_message(wdNode, WD_ACCEPT_MESSAGE, pkt);
								set_state(WD_PARTICIPATE_IN_ELECTION);
							}
							else
							{
								ereport(LOG,
										(errmsg("We are connected to leader node \"%s\" and another node \"%s\" is trying to become a leader",WD_LEADER_NODE->nodeName, wdNode->nodeName)));
								reply_with_minimal_message(wdNode, WD_ERROR_MESSAGE, pkt);
								/* Ask leader to re-send its node info */
								send_message_of_type(WD_LEADER_NODE, WD_REQ_INFO_MESSAGE, NULL);
							}
						}
						break;

					case WD_DECLARE_COORDINATOR_MESSAGE:
						{
							if (wdNode != WD_LEADER_NODE)
							{
								/*
								 * we already have a leader node and we got a
								 * new node trying to be leader
								 */
								ereport(LOG,
										(errmsg("We are connected to leader node \"%s\" and another node \"%s\" is trying to declare itself as a leader",WD_LEADER_NODE->nodeName, wdNode->nodeName)));
								reply_with_minimal_message(wdNode, WD_ERROR_MESSAGE, pkt);
								/* Ask leader to re-send its node info */
								send_message_of_type(WD_LEADER_NODE, WD_REQ_INFO_MESSAGE, NULL);

							}
						}
						break;

					case WD_IAM_COORDINATOR_MESSAGE:
						{
							/*
							 * if the message is received from coordinator
							 * reply with info, otherwise reject
							 */
							if (wdNode != WD_LEADER_NODE)
							{
								ereport(LOG,
										(errmsg("\"%s\" is our coordinator node, but \"%s\" is also announcing as a coordinator",
												WD_LEADER_NODE->nodeName, wdNode->nodeName),
										 errdetail("broadcasting the cluster in split-brain message")));

								send_cluster_service_message(NULL, pkt, CLUSTER_IN_SPLIT_BRAIN);
							}
							else if (check_debug_request_do_not_reply_beacon() == false)
							{
								send_message_of_type(wdNode, WD_INFO_MESSAGE, pkt);
								beacon_message_received_from_node(wdNode, pkt);
							}
						}
						break;

					default:
						standard_packet_processor(wdNode, pkt);
						break;
				}
			}
			break;

		default:
			break;
	}

	/*
	 * before returning from the function make sure that we are connected with
	 * the leader node
	 */
	if (WD_LEADER_NODE)
	{
		struct timeval currTime;

		gettimeofday(&currTime, NULL);
		int			last_rcv_sec = WD_TIME_DIFF_SEC(currTime, WD_LEADER_NODE->last_rcv_time);

		if (last_rcv_sec >= (3 * BEACON_MESSAGE_INTERVAL_SECONDS))
		{
			/* we have missed atleast two beacons from leader node */
			ereport(WARNING,
					(errmsg("we have not received a beacon message from leader node \"%s\" and it has not replied to our info request",
							WD_LEADER_NODE->nodeName),
					 errdetail("re-initializing the cluster")));
			set_state(WD_JOINING);
		}
		else if (last_rcv_sec >= (2 * BEACON_MESSAGE_INTERVAL_SECONDS))
		{
			/*
			 * We have not received a last beacon from leader ask for the
			 * node info from leader node
			 */
			ereport(WARNING,
					(errmsg("we have not received a beacon message from leader node \"%s\"",
							WD_LEADER_NODE->nodeName),
					 errdetail("requesting info message from leader node")));
			send_message_of_type(WD_LEADER_NODE, WD_REQ_INFO_MESSAGE, NULL);
		}
	}
	return 0;
}


/*
 * The function identifies the current quorum state
 * quorum values:
 * -1:
 *     quorum is lost or does not exists
 * 0:
 *     The quorum is on the edge. (when participating cluster is configured
 *     with even number of nodes, and we have exactly 50% nodes
 * 1:
 *     quorum exists
 */
static void
update_quorum_status(void)
{
	int			quorum_status = g_cluster.quorum_status;

	if (g_cluster.clusterLeaderInfo.standby_nodes_count > get_minimum_remote_nodes_required_for_quorum())
	{
		g_cluster.quorum_status = 1;
	}
	else if (g_cluster.clusterLeaderInfo.standby_nodes_count == get_minimum_remote_nodes_required_for_quorum())
	{
		if (g_cluster.memberRemoteNodeCount % 2 != 0)
		{
			if (pool_config->enable_consensus_with_half_votes)
				g_cluster.quorum_status = 0;	/* on the edge */
			else
				g_cluster.quorum_status = -1;
		}
		else
			g_cluster.quorum_status = 1;
	}
	else
	{
		g_cluster.quorum_status = -1;
	}
	g_cluster.localNode->quorum_status = g_cluster.quorum_status;
	if (g_cluster.quorum_status != quorum_status)
	{
		watchdog_state_machine(WD_EVENT_CLUSTER_QUORUM_CHANGED, NULL, NULL, NULL);
	}
}

/*
 * returns the minimum number of remote nodes required for quorum
 */
static int
get_minimum_remote_nodes_required_for_quorum(void)
{
	/*
	 * Even number of remote nodes, That means total number of nodes are odd,
	 * so minimum quorum is just remote/2.
	 */
	if (g_cluster.memberRemoteNodeCount % 2 == 0)
			return (g_cluster.memberRemoteNodeCount / 2);

	/*
	 * Total nodes including self are even, So we return 50% nodes as quorum
	 * requirements
	 */
	return ((g_cluster.memberRemoteNodeCount - 1) / 2);
}

/*
 * returns the minimum number of votes required for consensus
 */
static int
get_minimum_votes_to_resolve_consensus(void)
{
	/*
	 * Since get_minimum_remote_nodes_required_for_quorum() returns
	 * the number of remote nodes required to complete the quorum
	 * that is always one less than the total number of nodes required
	 * for the cluster to build quorum or consensus, reason being
	 * in get_minimum_remote_nodes_required_for_quorum()
	 * we always consider the local node as a valid pre-casted vote.
	 * But when it comes to count the number of votes required to build
	 * consensus for any type of decision, for example for building the
	 * consensus on backend failover, the local node can vote on either
	 * side. So it's vote is not explicitly counted and for the consensus
	 * we actually need one more vote than the total number of remote nodes
	 * required for the quorum
	 *
	 * For example
	 * If Total nodes in cluster = 4
	 * 		remote node will be = 3
	 * 		get_minimum_remote_nodes_required_for_quorum() return = 1
	 *		Minimum number of votes required for consensus will be
	 *
	 *		if(pool_config->enable_consensus_with_half_votes = true)
	 *			(exact 50% n/2) ==> 4/2 = 2
	 *
	 *		if(pool_config->enable_consensus_with_half_votes = false)
	 *			(exact 50% +1 ==> (n/2)+1) ==> (4/2)+1 = 3
	 *
	 */

	int required_node_count = get_minimum_remote_nodes_required_for_quorum()  + 1;
	/*
	 * When the total number of nodes in the watchdog cluster including the
	 * local node are even, The number of votes required for the consensus
	 * depends on the enable_consensus_with_half_votes.
	 * So for even number of nodes when enable_consensus_with_half_votes is
	 * not allowed than we would add one more vote than exact 50%
	 */
	if (g_cluster.memberRemoteNodeCount % 2 != 0)
	{
		if (pool_config->enable_consensus_with_half_votes == false)
			required_node_count += 1;
	}

	return required_node_count;
}

/*
 * sets the state of local watchdog node, and fires a state change event
 * if the new and old state differs
 */
static int
set_state(WD_STATES newState)
{
	WD_STATES	oldState = get_local_node_state();

	g_cluster.localNode->state = newState;
	if (oldState != newState)
	{
		gettimeofday(&g_cluster.localNode->current_state_time, NULL);

		/*
		 * if we changing from the coordinator state, do the de-escalation if
		 * required
		 */
		if (oldState == WD_COORDINATOR)
		{
			resign_from_escalated_node();
			clear_standby_nodes_list();
			clear_all_failovers();
		}

		ereport(LOG,
				(errmsg("watchdog node state changed from [%s] to [%s]", wd_state_names[oldState], wd_state_names[newState])));
		watchdog_state_machine(WD_EVENT_WD_STATE_CHANGED, NULL, NULL, NULL);
		/* send out the info message to all nodes */
		send_message_of_type(NULL, WD_INFO_MESSAGE, NULL);
	}
	return 0;
}


static void
allocate_resultNodes_in_command(WDCommandData * ipcCommand)
{
	MemoryContext oldCxt;
	int			i;

	if (ipcCommand->nodeResults != NULL)
		return;

	oldCxt = MemoryContextSwitchTo(ipcCommand->memoryContext);
	ipcCommand->nodeResults = palloc0((sizeof(WDCommandNodeResult) * g_cluster.remoteNodeCount));
	for (i = 0; i < g_cluster.remoteNodeCount; i++)
	{
		ipcCommand->nodeResults[i].wdNode = &g_cluster.remoteNodes[i];
	}
	MemoryContextSwitchTo(oldCxt);
}


static void
process_remote_online_recovery_command(WatchdogNode * wdNode, WDPacketData * pkt)
{
	char	   *func_name;
	int			node_count = 0;
	int		   *node_id_list = NULL;
	unsigned char flags;

	if (pkt->data == NULL || pkt->len == 0)
	{
		ereport(LOG,
				(errmsg("watchdog is unable to process pgpool online recovery command"),
				 errdetail("command packet contains no data")));
		reply_with_minimal_message(wdNode, WD_ERROR_MESSAGE, pkt);
		return;
	}

	ereport(LOG,
			(errmsg("watchdog received online recovery request from \"%s\"", wdNode->nodeName)));

	if (parse_wd_node_function_json(pkt->data, pkt->len, &func_name, &node_id_list, &node_count, &flags))
	{
		if (strcasecmp(WD_FUNCTION_START_RECOVERY, func_name) == 0)
		{
			if (*InRecovery != RECOVERY_INIT)
			{
				reply_with_minimal_message(wdNode, WD_REJECT_MESSAGE, pkt);
			}
			else
			{
				*InRecovery = RECOVERY_ONLINE;
				if (Req_info->conn_counter == 0)
				{
					reply_with_minimal_message(wdNode, WD_ACCEPT_MESSAGE, pkt);
				}
				else if (pool_config->recovery_timeout <= 0)
				{
					if (ensure_conn_counter_validity() == 0)
						reply_with_minimal_message(wdNode, WD_ACCEPT_MESSAGE, pkt);
					else
						reply_with_minimal_message(wdNode, WD_REJECT_MESSAGE, pkt);
				}
				else
				{
					WDFunctionCommandData *wd_func_command;
					MemoryContext oldCxt = MemoryContextSwitchTo(TopMemoryContext);

					wd_func_command = palloc(sizeof(WDFunctionCommandData));
					wd_func_command->commandType = pkt->type;
					wd_func_command->commandID = pkt->command_id;
					wd_func_command->funcName = MemoryContextStrdup(TopMemoryContext, func_name);
					wd_func_command->wdNode = wdNode;

					/* Add this command for timer tick */
					add_wd_command_for_timer_events(pool_config->recovery_timeout, true, wd_func_command);

					MemoryContextSwitchTo(oldCxt);
				}
			}
		}
		else if (strcasecmp(WD_FUNCTION_END_RECOVERY, func_name) == 0)
		{
			*InRecovery = RECOVERY_INIT;
			reply_with_minimal_message(wdNode, WD_ACCEPT_MESSAGE, pkt);
			kill(getppid(), SIGUSR2);
		}
		else
		{
			ereport(LOG,
					(errmsg("watchdog failed to process online recovery request"),
					 errdetail("invalid command [%s] in online recovery request from \"%s\"", func_name, wdNode->nodeName)));
			reply_with_minimal_message(wdNode, WD_ERROR_MESSAGE, pkt);
		}
	}
	else
	{
		ereport(LOG,
				(errmsg("watchdog failed to process online recovery request"),
				 errdetail("invalid data in online recovery request from \"%s\"", wdNode->nodeName)));
		reply_with_minimal_message(wdNode, WD_ERROR_MESSAGE, pkt);
	}

	if (func_name)
		pfree(func_name);
	if (node_id_list)
		pfree(node_id_list);
}


static bool
reply_is_received_for_pgpool_replicate_command(WatchdogNode * wdNode, WDPacketData * pkt, WDCommandData * ipcCommand)
{
	int			i;
	WDCommandNodeResult *nodeResult = NULL;

	/* get the result node for */
	ereport(DEBUG1,
			(errmsg("watchdog node \"%s\" has replied for pgpool-II replicate command packet", wdNode->nodeName)));

	for (i = 0; i < g_cluster.remoteNodeCount; i++)
	{
		nodeResult = &ipcCommand->nodeResults[i];
		if (nodeResult->wdNode == wdNode)
			break;
		nodeResult = NULL;
	}
	if (nodeResult == NULL)
	{
		ereport(WARNING,
				(errmsg("unable to find result node for pgpool-II replicate command packet received from watchdog node \"%s\"", wdNode->nodeName)));
		return true;
	}

	nodeResult->result_type = pkt->type;
	nodeResult->cmdState = COMMAND_STATE_REPLIED;
	ipcCommand->commandReplyFromCount++;
	ereport(DEBUG2,
			(errmsg("watchdog node \"%s\" has replied for pgpool-II replicate command packet", wdNode->nodeName),
			 errdetail("command was sent to %d nodes and %d nodes have replied to it", ipcCommand->commandSendToCount, ipcCommand->commandReplyFromCount)));

	if (pkt->type != WD_ACCEPT_MESSAGE)
	{
		/* reject message from any node finishes the command */
		ipcCommand->commandStatus = COMMAND_FINISHED_NODE_REJECTED;
		wd_command_is_complete(ipcCommand);
		cleanUpIPCCommand(ipcCommand);
	}
	else if (ipcCommand->commandReplyFromCount >= ipcCommand->commandSendToCount)
	{
		/*
		 * we have received results from all nodes analyze the result
		 */
		ipcCommand->commandStatus = COMMAND_FINISHED_ALL_REPLIED;
		wd_command_is_complete(ipcCommand);
		cleanUpIPCCommand(ipcCommand);
	}

	/* do not process this packet further */
	return true;
}

/*
 * return true if want to cancel timer,
 */
static bool
process_wd_command_timer_event(bool timer_expired, WDFunctionCommandData * wd_func_command)
{
	if (wd_func_command->commandType == WD_IPC_ONLINE_RECOVERY_COMMAND)
	{
		if (wd_func_command->funcName && strcasecmp("START_RECOVERY", wd_func_command->funcName) == 0)
		{
			if (Req_info->conn_counter == 0)
			{
				WDPacketData emptyPkt;

				emptyPkt.command_id = wd_func_command->commandID;
				reply_with_minimal_message(wd_func_command->wdNode, WD_ACCEPT_MESSAGE, &emptyPkt);
				return true;
			}
			else if (timer_expired)
			{
				WDPacketData emptyPkt;

				emptyPkt.command_id = wd_func_command->commandID;

				if (ensure_conn_counter_validity() == 0)
					reply_with_minimal_message(wd_func_command->wdNode, WD_ACCEPT_MESSAGE, &emptyPkt);
				else
					reply_with_minimal_message(wd_func_command->wdNode, WD_REJECT_MESSAGE, &emptyPkt);
				return true;
			}
			return false;
		}
	}
	/* Just remove the timer. */
	return true;
}

static void
process_wd_func_commands_for_timer_events(void)
{
	struct timeval currTime;
	ListCell   *lc;
	List	   *timers_to_del = NIL;

	if (g_cluster.wd_timer_commands == NULL)
		return;

	gettimeofday(&currTime, NULL);

	/*
	 * Take care online recovery
	 */
	foreach(lc, g_cluster.wd_timer_commands)
	{
		WDCommandTimerData *timerData = lfirst(lc);

		if (timerData)
		{
			bool		del = false;

			if (WD_TIME_DIFF_SEC(currTime, timerData->startTime) >= timerData->expire_sec)
			{
				del = process_wd_command_timer_event(true, timerData->wd_func_command);

			}
			else if (timerData->need_tics)
			{
				del = process_wd_command_timer_event(false, timerData->wd_func_command);
			}
			if (del)
				timers_to_del = lappend(timers_to_del, timerData);
		}
	}
	foreach(lc, timers_to_del)
	{
		g_cluster.wd_timer_commands = list_delete_ptr(g_cluster.wd_timer_commands, lfirst(lc));
	}

	list_free(timers_to_del);
}

static void
add_wd_command_for_timer_events(unsigned int expire_secs, bool need_tics, WDFunctionCommandData * wd_func_command)
{
	/* create a new Timer struct */
	MemoryContext oldCtx = MemoryContextSwitchTo(TopMemoryContext);
	WDCommandTimerData *timerData = palloc(sizeof(WDCommandTimerData));

	gettimeofday(&timerData->startTime, NULL);
	timerData->expire_sec = expire_secs;
	timerData->need_tics = need_tics;
	timerData->wd_func_command = wd_func_command;

	g_cluster.wd_timer_commands = lappend(g_cluster.wd_timer_commands, timerData);

	MemoryContextSwitchTo(oldCtx);

}

#define WD_VERIFY_RECEIVED_CONFIG_PARAMETER_VAL_INT(config_obj, wdNode, parameter) \
do { \
	if (config_obj->parameter != pool_config->parameter) \
	{ \
		ereport(WARNING, \
			(errmsg("configurations value for \"%s\" on node \"%s\" is different", #parameter, wdNode->nodeName), \
				errdetail("\"%s\" on this node is %d while on \"%s\" is %d", \
				   #parameter, \
				   pool_config->parameter, \
				   wdNode->nodeName, \
				   config_obj->parameter))); \
	} \
} while(0)
#define WD_VERIFY_RECEIVED_CONFIG_PARAMETER_VAL_BOOL(config_obj,wdNode, parameter) \
do { \
	if (config_obj->parameter != pool_config->parameter) \
	{ \
		ereport(WARNING, \
			(errmsg("configurations value for \"%s\" on node \"%s\" is different", #parameter, wdNode->nodeName), \
				errdetail("\"%s\" on this node is %s while on \"%s\" is %s", \
					#parameter, \
					pool_config->parameter?"ON":"OFF", \
					wdNode->nodeName, \
					config_obj->parameter?"ON":"OFF"))); \
	} \
} while(0)

static void
verify_pool_configurations(WatchdogNode * wdNode, POOL_CONFIG * config)
{
	int			i;

	WD_VERIFY_RECEIVED_CONFIG_PARAMETER_VAL_INT(config, wdNode, num_init_children);
	WD_VERIFY_RECEIVED_CONFIG_PARAMETER_VAL_INT(config, wdNode, listen_backlog_multiplier);
	WD_VERIFY_RECEIVED_CONFIG_PARAMETER_VAL_INT(config, wdNode, child_life_time);
	WD_VERIFY_RECEIVED_CONFIG_PARAMETER_VAL_INT(config, wdNode, connection_life_time);
	WD_VERIFY_RECEIVED_CONFIG_PARAMETER_VAL_INT(config, wdNode, child_max_connections);
	WD_VERIFY_RECEIVED_CONFIG_PARAMETER_VAL_INT(config, wdNode, client_idle_limit);
	WD_VERIFY_RECEIVED_CONFIG_PARAMETER_VAL_INT(config, wdNode, max_pool);
	WD_VERIFY_RECEIVED_CONFIG_PARAMETER_VAL_INT(config, wdNode, health_check_timeout);
	WD_VERIFY_RECEIVED_CONFIG_PARAMETER_VAL_INT(config, wdNode, health_check_period);
	WD_VERIFY_RECEIVED_CONFIG_PARAMETER_VAL_INT(config, wdNode, health_check_max_retries);
	WD_VERIFY_RECEIVED_CONFIG_PARAMETER_VAL_INT(config, wdNode, health_check_retry_delay);
	WD_VERIFY_RECEIVED_CONFIG_PARAMETER_VAL_INT(config, wdNode, recovery_timeout);
	WD_VERIFY_RECEIVED_CONFIG_PARAMETER_VAL_INT(config, wdNode, search_primary_node_timeout);
	WD_VERIFY_RECEIVED_CONFIG_PARAMETER_VAL_INT(config, wdNode, client_idle_limit_in_recovery);

	WD_VERIFY_RECEIVED_CONFIG_PARAMETER_VAL_BOOL(config, wdNode, replication_mode);
	WD_VERIFY_RECEIVED_CONFIG_PARAMETER_VAL_BOOL(config, wdNode, enable_pool_hba);
	WD_VERIFY_RECEIVED_CONFIG_PARAMETER_VAL_BOOL(config, wdNode, load_balance_mode);
	WD_VERIFY_RECEIVED_CONFIG_PARAMETER_VAL_BOOL(config, wdNode, replication_stop_on_mismatch);
	WD_VERIFY_RECEIVED_CONFIG_PARAMETER_VAL_BOOL(config, wdNode, allow_clear_text_frontend_auth);
	WD_VERIFY_RECEIVED_CONFIG_PARAMETER_VAL_BOOL(config, wdNode, failover_if_affected_tuples_mismatch);
	WD_VERIFY_RECEIVED_CONFIG_PARAMETER_VAL_BOOL(config, wdNode, failover_on_backend_error);
	WD_VERIFY_RECEIVED_CONFIG_PARAMETER_VAL_BOOL(config, wdNode, replicate_select);
	WD_VERIFY_RECEIVED_CONFIG_PARAMETER_VAL_BOOL(config, wdNode, connection_cache);
	WD_VERIFY_RECEIVED_CONFIG_PARAMETER_VAL_BOOL(config, wdNode, insert_lock);
	WD_VERIFY_RECEIVED_CONFIG_PARAMETER_VAL_BOOL(config, wdNode, memory_cache_enabled);
	WD_VERIFY_RECEIVED_CONFIG_PARAMETER_VAL_BOOL(config, wdNode, clear_memqcache_on_escalation);

	WD_VERIFY_RECEIVED_CONFIG_PARAMETER_VAL_BOOL(config, wdNode, failover_when_quorum_exists);
	WD_VERIFY_RECEIVED_CONFIG_PARAMETER_VAL_BOOL(config, wdNode, failover_require_consensus);
	WD_VERIFY_RECEIVED_CONFIG_PARAMETER_VAL_BOOL(config, wdNode, allow_multiple_failover_requests_from_node);

	if (config->backend_desc->num_backends != pool_config->backend_desc->num_backends)
	{
		ereport(WARNING,
				(errmsg("number of configured backends on node \"%s\" are different", wdNode->nodeName),
				 errdetail("this node has %d backends while on \"%s\" number of configured backends are %d",
						   pool_config->backend_desc->num_backends,
						   wdNode->nodeName,
						   config->backend_desc->num_backends)));
	}
	for (i = 0; i < pool_config->backend_desc->num_backends; i++)
	{
		if (strncasecmp(pool_config->backend_desc->backend_info[i].backend_hostname, config->backend_desc->backend_info[i].backend_hostname, sizeof(pool_config->backend_desc->backend_info[i].backend_hostname)))
		{
			ereport(WARNING,
					(errmsg("configurations value for backend[%d] \"hostname\" on node \"%s\" is different", i, wdNode->nodeName),
					 errdetail("\"backend_hostname%d\" on this node is %s while on \"%s\" is %s",
							   i,
							   pool_config->backend_desc->backend_info[i].backend_hostname,
							   wdNode->nodeName,
							   config->backend_desc->backend_info[i].backend_hostname)));
		}
		if (config->backend_desc->backend_info[i].backend_port != pool_config->backend_desc->backend_info[i].backend_port)
		{
			ereport(WARNING,
					(errmsg("configurations value for backend[%d] \"port\" on node \"%s\" is different", i, wdNode->nodeName),
					 errdetail("\"backend_port%d\" on this node is %d while on \"%s\" is %d",
							   i,
							   pool_config->backend_desc->backend_info[i].backend_port,
							   wdNode->nodeName,
							   config->backend_desc->backend_info[i].backend_port)));
		}
	}

	if (config->wd_nodes.num_wd != pool_config->wd_nodes.num_wd)
	{
		ereport(WARNING,
				(errmsg("the number of configured watchdog nodes on node \"%s\" are different", wdNode->nodeName),
				 errdetail("this node has %d watchdog nodes while \"%s\" is configured with %d watchdog nodes",
						   pool_config->wd_nodes.num_wd,
						   wdNode->nodeName,
						   config->wd_nodes.num_wd)));
	}
}

static bool
get_authhash_for_node(WatchdogNode * wdNode, char *authhash)
{
	if (strlen(pool_config->wd_authkey))
	{
		char		nodeStr[WD_MAX_PACKET_STRING + 1];
		int			len = snprintf(nodeStr, WD_MAX_PACKET_STRING, "state=%d wd_port=%d",
								   wdNode->state, wdNode->wd_port);


		/* calculate hash from packet */
		wd_calc_hash(nodeStr, len, authhash);
		if (authhash[0] == '\0')
			ereport(WARNING,
					(errmsg("failed to calculate wd_authkey hash from a send packet")));
		return true;
	}
	return false;
}

static bool
verify_authhash_for_node(WatchdogNode * wdNode, char *authhash)
{
	if (strlen(pool_config->wd_authkey))
	{
		char		calculated_authhash[WD_AUTH_HASH_LEN + 1];

		char		nodeStr[WD_MAX_PACKET_STRING];
		int			len = snprintf(nodeStr, WD_MAX_PACKET_STRING, "state=%d wd_port=%d",
								   wdNode->state, wdNode->wd_port);


		/* calculate hash from packet */
		wd_calc_hash(nodeStr, len, calculated_authhash);
		if (calculated_authhash[0] == '\0')
			ereport(WARNING,
					(errmsg("failed to calculate wd_authkey hash from a receive packet")));
		return (strcmp(calculated_authhash, authhash) == 0);
	}
	/* authkey is not enabled. */
	return true;
}

/*
 * function authenticates the IPC command by looking for the
 * auth key in the JSON data of IPC command.
 * For IPC commands coming from outer world the function validates the
 * authkey in JSON packet with configured pool_config->wd_authkey.
 * if internal_client_only is true then the JSON data must contain the
 * shared key present in the pgpool-II shared memory. This can be used
 * to restrict certain watchdog IPC functions for outside of pgpool-II
 */
static bool
check_IPC_client_authentication(json_value * rootObj, bool internal_client_only)
{
	char	   *packet_auth_key;
	unsigned int packet_key;
	bool		has_shared_key;
	unsigned int *shared_key = get_ipc_shared_key();

	if (json_get_int_value_for_key(rootObj, WD_IPC_SHARED_KEY, (int *) &packet_key))
	{
		ereport(DEBUG2,
				(errmsg("IPC JSON data packet does not contain shared key")));
		has_shared_key = false;
	}
	else
	{
		has_shared_key = true;
	}

	if (internal_client_only)
	{

		if (shared_key == NULL)
		{
			ereport(LOG,
					(errmsg("shared key not initialized")));
			return false;
		}

		if (has_shared_key == false)
		{
			ereport(LOG,
					(errmsg("invalid JSON data packet"),
					 errdetail("authentication shared key not found in JSON data")));
			return false;
		}
		/* compare if shared keys match */
		if (*shared_key != packet_key)
			return false;

		/* providing a valid shared key for internal clients is enough */
		return true;
	}

	/* If no authentication is required, no need to look further */
	if (g_cluster.ipc_auth_needed == false)
		return true;

	/* if shared key is provided and it matched, we are good */
	if (has_shared_key == true && *shared_key == packet_key)
		return true;

	/* shared key is out of question validate the authKey values */
	packet_auth_key = json_get_string_value_for_key(rootObj, WD_IPC_AUTH_KEY);

	if (packet_auth_key == NULL)
	{
		ereport(DEBUG1,
				(errmsg("invalid JSON data packet"),
				 errdetail("authentication key not found in JSON data")));
		return false;
	}

	/* compare the packet key with configured auth key */
	if (strcmp(pool_config->wd_authkey, packet_auth_key) != 0)
		return false;
	return true;
}

/*
 * function to check authentication of IPC command based on the command type
 * this one also informs the calling client about the failure
 */

static bool
check_and_report_IPC_authentication(WDCommandData * ipcCommand)
{
	json_value *root = NULL;
	bool		internal_client_only = false;
	bool		ret;

	if (ipcCommand == NULL)
		return false;			/* should never happen */

	/* first identify the command type */
	switch (ipcCommand->sourcePacket.type)
	{
		case WD_NODE_STATUS_CHANGE_COMMAND:
		case WD_REGISTER_FOR_NOTIFICATION:
		case WD_GET_NODES_LIST_COMMAND:
		case WD_GET_RUNTIME_VARIABLE_VALUE:
			internal_client_only = false;
			break;

		case WD_IPC_FAILOVER_COMMAND:
		case WD_IPC_ONLINE_RECOVERY_COMMAND:
		case WD_EXECUTE_CLUSTER_COMMAND:
		case WD_GET_LEADER_DATA_REQUEST:
			/* only allowed internally. */
			internal_client_only = true;
			break;

		default:
			/* unknown command, ignore it */
			return true;
			break;
	}

	if (internal_client_only == false && g_cluster.ipc_auth_needed == false)
	{
		/* no need to look further */
		return true;
	}

	if (ipcCommand->sourcePacket.len <= 0 || ipcCommand->sourcePacket.data == NULL)
	{
		ereport(LOG,
				(errmsg("authentication failed"),
				 errdetail("IPC command contains no data")));
		ipcCommand->errorMessage = MemoryContextStrdup(ipcCommand->memoryContext,
													   "authentication failed: invalid data");

		return false;
	}

	root = json_parse(ipcCommand->sourcePacket.data, ipcCommand->sourcePacket.len);
	/* The root node must be object */
	if (root == NULL || root->type != json_object)
	{
		json_value_free(root);
		ereport(LOG,
				(errmsg("authentication failed"),
				 errdetail("IPC command contains an invalid data")));

		ipcCommand->errorMessage = MemoryContextStrdup(ipcCommand->memoryContext,
													   "authentication failed: invalid data");

		return false;
	}

	ret = check_IPC_client_authentication(root, internal_client_only);
	json_value_free(root);

	if (ret == false)
	{
		ereport(WARNING,
				(errmsg("authentication failed"),
				 errdetail("invalid IPC key")));
		ipcCommand->errorMessage = MemoryContextStrdup(ipcCommand->memoryContext,
													   "authentication failed: invalid KEY");
	}
	return ret;
}

static void
print_watchdog_node_info(WatchdogNode * wdNode)
{
	ereport(DEBUG2,
			(errmsg("state: \"%s\" Host: \"%s\" Name: \"%s\" WD Port:%d PP Port: %d priority:%d",
					wd_state_names[wdNode->state],
					wdNode->hostname
					,wdNode->nodeName
					,wdNode->wd_port
					,wdNode->pgpool_port
					,wdNode->wd_priority)));
}

static void
print_packet_node_info(WDPacketData * pkt, WatchdogNode * wdNode, bool sending)
{
	int			i;
	packet_types *pkt_type = NULL;

	/*
	 * save the cpu cycles if our log level would swallow this message
	 */
	if (pool_config->log_min_messages > DEBUG1)
		return;

	for (i = 0;; i++)
	{
		if (all_packet_types[i].type == WD_NO_MESSAGE)
			break;

		if (all_packet_types[i].type == pkt->type)
		{
			pkt_type = &all_packet_types[i];
			break;
		}
	}

	ereport(DEBUG1,
			(errmsg("%s packet, watchdog node:[%s] command id:[%d] type:[%s] state:[%s]",
					sending ? "sending" : "received",
					wdNode->nodeName,
					pkt->command_id,
					pkt_type ? pkt_type->name : "UNKNOWN",
					wd_state_names[get_local_node_state()])));
}

static void
print_packet_info(WDPacketData * pkt, bool sending)
{
	int			i;
	packet_types *pkt_type = NULL;

	/*
	 * save the cpu cycles if our log level would swallow this message
	 */
	if (pool_config->log_min_messages > DEBUG2)
		return;

	for (i = 0;; i++)
	{
		if (all_packet_types[i].type == WD_NO_MESSAGE)
			break;

		if (all_packet_types[i].type == pkt->type)
		{
			pkt_type = &all_packet_types[i];
			break;
		}
	}

	ereport(DEBUG2,
			(errmsg("%s watchdog packet, command id:[%d] type:[%s] state :[%s]",
					sending ? "sending" : "received",
					pkt->command_id,
					pkt_type ? pkt_type->name : "UNKNOWN",
					wd_state_names[get_local_node_state()])));
}

static int
send_command_packet_to_remote_nodes(WDCommandData * ipcCommand, bool source_included)
{
	int			i;

	ipcCommand->commandSendToCount = 0;
	ipcCommand->commandReplyFromCount = 0;
	ipcCommand->commandSendToErrorCount = 0;
	allocate_resultNodes_in_command(ipcCommand);
	ereport(DEBUG2,
			(errmsg("sending the %c type message to \"%s\"",
					ipcCommand->commandPacket.type,
					ipcCommand->sendToNode ? ipcCommand->sendToNode->nodeName : "ALL NODES")));
	for (i = 0; i < g_cluster.remoteNodeCount; i++)
	{
		WDCommandNodeResult *nodeResult = &ipcCommand->nodeResults[i];

		if (ipcCommand->sendToNode != NULL && ipcCommand->sendToNode != nodeResult->wdNode)
		{
			/*
			 * The command is intended for specific node and this is not the
			 * one
			 */
			nodeResult->cmdState = COMMAND_STATE_DO_NOT_SEND;
		}
		else if (source_included == false && ipcCommand->sourceWdNode == nodeResult->wdNode &&
				 ipcCommand->commandSource == COMMAND_SOURCE_REMOTE)
		{
			ereport(DEBUG1,
					(errmsg("not sending the %c type message to command originator node \"%s\"",
							ipcCommand->commandPacket.type, nodeResult->wdNode->nodeName)));

			/*
			 * The message is not supposed to be sent to the watchdog node
			 * that started this command
			 */
			nodeResult->cmdState = COMMAND_STATE_DO_NOT_SEND;
		}
		else if (is_node_active(nodeResult->wdNode) == false)
		{
			nodeResult->cmdState = COMMAND_STATE_DO_NOT_SEND;
		}
		else if (is_node_reachable(nodeResult->wdNode) == false)
		{
			nodeResult->cmdState = COMMAND_STATE_SEND_ERROR;
			ipcCommand->commandSendToErrorCount++;
		}
		else if (send_message_to_node(nodeResult->wdNode, &ipcCommand->commandPacket) == true)
		{
			ereport(DEBUG2,
					(errmsg("%c type message written to socket for node \"%s\"",
							ipcCommand->commandPacket.type, nodeResult->wdNode->nodeName)));

			nodeResult->cmdState = COMMAND_STATE_SENT;
			ipcCommand->commandSendToCount++;
		}
		else
		{
			nodeResult->cmdState = COMMAND_STATE_SEND_ERROR;
			ipcCommand->commandSendToErrorCount++;
		}
	}
	return ipcCommand->commandSendToCount;
}

static void
set_cluster_leader_node(WatchdogNode * wdNode)
{
	if (WD_LEADER_NODE != wdNode)
	{
		if (wdNode == NULL)
			ereport(LOG,
					(errmsg("removing the %s node \"%s\" from watchdog cluster leader",
							(g_cluster.localNode == WD_LEADER_NODE) ? "local" : "remote",
							WD_LEADER_NODE->nodeName)));
		else
			ereport(LOG,
					(errmsg("setting the %s node \"%s\" as watchdog cluster leader",
							(g_cluster.localNode == wdNode) ? "local" : "remote",
							wdNode->nodeName)));
		g_cluster.clusterLeaderInfo.leaderNode = wdNode;
	}
}

static WatchdogNode*
getLeaderWatchdogNode(void)
{
	return g_cluster.clusterLeaderInfo.leaderNode;
}

static int
update_cluster_memberships(void)
{
	int i;
	g_cluster.memberRemoteNodeCount = g_cluster.remoteNodeCount;
	for (i = 0; i < g_cluster.remoteNodeCount; i++)
	{
		WatchdogNode *wdNode = &(g_cluster.remoteNodes[i]);
		if (wdNode->membership_status != WD_NODE_MEMBERSHIP_ACTIVE)
			g_cluster.memberRemoteNodeCount--;
	}
	return g_cluster.memberRemoteNodeCount;
}

static int
revoke_cluster_membership_of_node(WatchdogNode* wdNode, WD_NODE_MEMBERSHIP_STATUS revoke_status)
{
	if (wdNode->membership_status == WD_NODE_MEMBERSHIP_ACTIVE)
	{
		wdNode->membership_status = revoke_status;

		ereport(LOG,
				(errmsg("revoking the membership of [%s] node:\"%s\" [node_id:%d]",
							wd_state_names[wdNode->state], wdNode->nodeName,wdNode->pgpool_node_id),
				 errdetail("membership revoke reason: \"%s\"",
						   wd_cluster_membership_status[wdNode->membership_status])));

		g_cluster.memberRemoteNodeCount--;
	}
	return g_cluster.memberRemoteNodeCount;
}

static int
restore_cluster_membership_of_node(WatchdogNode* wdNode)
{
	if (wdNode->membership_status != WD_NODE_MEMBERSHIP_ACTIVE)
	{
		ereport(LOG,
				(errmsg("Restoring cluster membership of node:\"%s\"",wdNode->nodeName),
				 errdetail("membership of node was revoked because it was \"%s\"",
				   wd_cluster_membership_status[wdNode->membership_status])));

		wdNode->membership_status = WD_NODE_MEMBERSHIP_ACTIVE;
		/* reset the lost time on the node */
		wdNode->lost_time.tv_sec = 0;
		wdNode->lost_time.tv_usec = 0;
		g_cluster.memberRemoteNodeCount++;
	}
	return g_cluster.memberRemoteNodeCount;
}

static void
reset_lost_timers(void)
{
	int i;
	for (i = 0; i < g_cluster.remoteNodeCount; i++)
	{
		WatchdogNode *wdNode = &(g_cluster.remoteNodes[i]);
		wdNode->lost_time.tv_sec = 0;
		wdNode->lost_time.tv_usec = 0;
	}
}

static int
standby_node_join_cluster(WatchdogNode * wdNode)
{
	if (get_local_node_state() == WD_COORDINATOR)
	{
		int			i;
		/* Just rest the lost time stamp*/
		/* set the timestamp on node to track for how long this node is lost */
		wdNode->lost_time.tv_sec = 0;
		wdNode->lost_time.tv_usec = 0;
		/* First check if the node is already in the List */
		for (i = 0; i < g_cluster.clusterLeaderInfo.standby_nodes_count; i++)
		{
			WatchdogNode *node = g_cluster.clusterLeaderInfo.standbyNodes[i];

			if (node && node == wdNode)
			{
				/* The node is already in the standby list */
				return g_cluster.clusterLeaderInfo.standby_nodes_count;
			}
		}
		/* okay the node is not in the list */
		ereport(LOG,
				(errmsg("adding watchdog node \"%s\" to the standby list", wdNode->nodeName)));
		g_cluster.clusterLeaderInfo.standbyNodes[g_cluster.clusterLeaderInfo.standby_nodes_count] = wdNode;
		g_cluster.clusterLeaderInfo.standby_nodes_count++;
	}
	g_cluster.localNode->standby_nodes_count = g_cluster.clusterLeaderInfo.standby_nodes_count;
	return g_cluster.clusterLeaderInfo.standby_nodes_count;
}

static int
standby_node_left_cluster(WatchdogNode * wdNode)
{
	if (get_local_node_state() == WD_COORDINATOR)
	{
		int			i;
		bool		removed = false;
		int			standby_nodes_count = g_cluster.clusterLeaderInfo.standby_nodes_count;

		for (i = 0; i < standby_nodes_count; i++)
		{
			WatchdogNode *node = g_cluster.clusterLeaderInfo.standbyNodes[i];

			if (node)
			{
				if (removed)
				{
					/* move this to previous index */
					g_cluster.clusterLeaderInfo.standbyNodes[i - 1] = node;
					g_cluster.clusterLeaderInfo.standbyNodes[i] = NULL;
				}
				else if (node == wdNode)
				{
					/*
					 * okay we have found the node in the list.
					 */
					ereport(LOG,
							(errmsg("removing watchdog node \"%s\" from the standby list", wdNode->nodeName)));
					/* set the timestamp on node to track for how long this node is lost */
					gettimeofday(&wdNode->lost_time, NULL);
					g_cluster.clusterLeaderInfo.standbyNodes[i] = NULL;
					g_cluster.clusterLeaderInfo.standby_nodes_count--;
					removed = true;
				}
			}
		}
	}
	g_cluster.localNode->standby_nodes_count = g_cluster.clusterLeaderInfo.standby_nodes_count;
	return g_cluster.clusterLeaderInfo.standby_nodes_count;
}

static void
clear_standby_nodes_list(void)
{
	int			i;

	ereport(DEBUG1,
			(errmsg("removing all watchdog nodes from the standby list"),
			 errdetail("standby list contains %d nodes", g_cluster.clusterLeaderInfo.standby_nodes_count)));
	for (i = 0; i < g_cluster.remoteNodeCount; i++)
	{
		g_cluster.clusterLeaderInfo.standbyNodes[i] = NULL;
	}
	g_cluster.clusterLeaderInfo.standby_nodes_count = 0;
	g_cluster.localNode->standby_nodes_count = 0;
}

static void update_missed_beacon_count(WDCommandData* ipcCommand, bool clear)
{
	int i;
	for (i=0; i< g_cluster.remoteNodeCount; i++)
	{
		if (clear)
		{
			WatchdogNode* wdNode = &(g_cluster.remoteNodes[i]);
			wdNode->missed_beacon_count = 0;
		}
		else
		{
			WDCommandNodeResult* nodeResult = &ipcCommand->nodeResults[i];
			if (ipcCommand->commandStatus == COMMAND_IN_PROGRESS )
				return;

			if (nodeResult->cmdState == COMMAND_STATE_SENT)
			{
				if (nodeResult->wdNode->state == WD_STANDBY)
				{
					nodeResult->wdNode->missed_beacon_count++;
					if (nodeResult->wdNode->missed_beacon_count > 1)
						ereport(LOG,
							(errmsg("remote node \"%s\" is not replying to our beacons",nodeResult->wdNode->nodeName),
							 errdetail("missed beacon reply count:%d",nodeResult->wdNode->missed_beacon_count)));
				}
				else
					nodeResult->wdNode->missed_beacon_count = 0;
			}
			if (nodeResult->cmdState == COMMAND_STATE_REPLIED)
			{
				if (nodeResult->wdNode->missed_beacon_count > 0)
					ereport(LOG,
							(errmsg("remote node \"%s\" is replying again after missing %d beacons",nodeResult->wdNode->nodeName,
									nodeResult->wdNode->missed_beacon_count)));
				nodeResult->wdNode->missed_beacon_count = 0;
			}
		}
	}
}

static void
update_failover_timeout(WatchdogNode * wdNode, POOL_CONFIG *pool_config)
{
	int failover_command_timeout;
	if (get_local_node_state() != WD_COORDINATOR)
		return;

	failover_command_timeout = pool_config->health_check_period + (pool_config->health_check_retry_delay * pool_config->health_check_max_retries);

	if (pool_config->health_check_params)
	{
		int i;
		for (i = 0 ; i < pool_config->backend_desc->num_backends; i++)
		{
			int pn_failover_command_timeout = pool_config->health_check_params[i].health_check_period +
							(pool_config->health_check_params[i].health_check_retry_delay *
							pool_config->health_check_params[i].health_check_max_retries);

			if (failover_command_timeout < pn_failover_command_timeout)
				failover_command_timeout = pn_failover_command_timeout;
		}
	}

	if (g_cluster.localNode == wdNode)
	{
		/* Reset*/
		g_cluster.failover_command_timeout = failover_command_timeout;
	}
	else if (g_cluster.failover_command_timeout < failover_command_timeout)
		g_cluster.failover_command_timeout = failover_command_timeout;

	if (g_cluster.failover_command_timeout < FAILOVER_COMMAND_FINISH_TIMEOUT)
		g_cluster.failover_command_timeout = FAILOVER_COMMAND_FINISH_TIMEOUT;

	ereport(LOG,(errmsg("Setting failover command timeout to %d",failover_command_timeout)));
}

#ifdef WATCHDOG_DEBUG
/*
 * Node down request file. In the file, each line consists of watchdog
 * debug command.  The possible commands are same as the defines below
 * for example to stop Pgpool-II from sending the reply to beacon messages
 * from the leader node write DO_NOT_REPLY_TO_BEACON in watchdog_debug_requests
 *
 *
 * echo "DO_NOT_REPLY_TO_BEACON" > pgpool_logdir/watchdog_debug_requests
 */

typedef struct watchdog_debug_commands
{
	char		command[100];
	unsigned int code;
}			watchdog_debug_commands;

unsigned int watchdog_debug_command = 0;


#define WATCHDOG_DEBUG_FILE	"watchdog_debug_requests"

#define DO_NOT_REPLY_TO_BEACON 	1
#define DO_NOT_SEND_BEACON 		2
#define KILL_ALL_COMMUNICATION	4
#define KILL_ALL_RECEIVERS		8
#define KILL_ALL_SENDERS		16


watchdog_debug_commands wd_debug_commands[] = {
	{"DO_NOT_REPLY_TO_BEACON", DO_NOT_REPLY_TO_BEACON},
	{"DO_NOT_SEND_BEACON",     DO_NOT_SEND_BEACON},
	{"KILL_ALL_COMMUNICATION", KILL_ALL_COMMUNICATION},
	{"KILL_ALL_RECEIVERS",     KILL_ALL_RECEIVERS},
	{"KILL_ALL_SENDERS",       KILL_ALL_SENDERS},
	{"", 0}
};

static bool
check_debug_request_kill_all_communication(void)
{
	return (watchdog_debug_command & KILL_ALL_COMMUNICATION);
}
static bool
check_debug_request_kill_all_receivers(void)
{
	return (watchdog_debug_command & KILL_ALL_RECEIVERS);
}
static bool
check_debug_request_kill_all_senders(void)
{
	return (watchdog_debug_command & KILL_ALL_SENDERS);
}

static bool
check_debug_request_do_not_send_beacon(void)
{
	return (watchdog_debug_command & DO_NOT_SEND_BEACON);
}

static bool
check_debug_request_do_not_reply_beacon(void)
{
	return (watchdog_debug_command & DO_NOT_REPLY_TO_BEACON);
}
/*
 * Check watchdog debug request options file for debug commands
 * each line should contain only one command
 *
 * Possible commands
 * 		DO_NOT_REPLY_TO_BEACON
 *		DO_NOT_SEND_BEACON
 *		KILL_ALL_COMMUNICATION
 *		KILL_ALL_RECEIVERS
 *		KILL_ALL_SENDERS
 */

static void
load_watchdog_debug_test_option(void)
{
	static char wd_debug_request_file[POOLMAXPATHLEN];
	FILE	   *fd;
	int			i;
#define MAXLINE 128
	char		readbuf[MAXLINE];

	watchdog_debug_command = 0;

	if (wd_debug_request_file[0] == '\0')
	{
		snprintf(wd_debug_request_file, sizeof(wd_debug_request_file),
				 "%s/%s", pool_config->logdir, WATCHDOG_DEBUG_FILE);
	}

	fd = fopen(wd_debug_request_file, "r");
	if (!fd)
	{
		ereport(DEBUG3,
				(errmsg("load_watchdog_debug_test_option: failed to open file %s",
						wd_debug_request_file),
				 errdetail("%m")));
		return;
	}

	for (i = 0;; i++)
	{
		int cmd = 0;
		bool valid_command = false;
		readbuf[MAXLINE - 1] = '\0';
		if (fgets(readbuf, MAXLINE - 1, fd) == 0)
			break;
		for (cmd =0 ;; cmd++)
		{
			if (strlen(wd_debug_commands[cmd].command) == 0 || wd_debug_commands[cmd].code == 0)
				break;

			if (strncasecmp(wd_debug_commands[cmd].command,readbuf,strlen(wd_debug_commands[cmd].command)) == 0)
			{
				ereport(DEBUG3,
						(errmsg("Watchdog DEBUG COMMAND %d: \"%s\" request found",
								cmd,wd_debug_commands[cmd].command)));

				watchdog_debug_command |= wd_debug_commands[cmd].code;
				valid_command = true;
				break;
			}
		}
		if (!valid_command)
			ereport(WARNING,
				(errmsg("%s file contains invalid command",
							wd_debug_request_file),
					 errdetail("\"%s\" not recognized", readbuf)));
	}

	fclose(fd);
}
#else
/*
 * All these command checks return false when WATCHDOG_DEBUG is
 * not enabled
 */
static bool
check_debug_request_do_not_send_beacon(void)
{return false;}
static bool
check_debug_request_do_not_reply_beacon(void)
{return false;}
static bool
check_debug_request_kill_all_communication(void)
{return false;}
static bool
check_debug_request_kill_all_receivers(void)
{return false;}
static bool
check_debug_request_kill_all_senders(void)
{return false;}
#endif
