/*
 * $Header$
 *
 * Handles watchdog connection, and protocol communication with pgpool-II
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
 */
#include <pthread.h>
#include <stdio.h>
#include <errno.h>
#include <ctype.h>
#include <time.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <netdb.h>
#include <sys/wait.h>

#include "pool.h"
#include "pool_config.h"
#include "utils/elog.h"
#include "utils/json.h"
#include "utils/json_writer.h"
#include "utils/elog.h"
#include "utils/palloc.h"
#include "utils/memutils.h"
#include "utils/pool_signal.h"
#include "utils/ps_status.h"

#include "watchdog/wd_utils.h"
#include "watchdog/wd_lifecheck.h"
#include "watchdog/wd_ipc_defines.h"
#include "watchdog/wd_internal_commands.h"
#include "watchdog/wd_json_data.h"

#include "libpq-fe.h"

#define LIFECHECK_GETNODE_WAIT_SEC_COUNT 5	/* max number of seconds the
											 * lifecheck process should waits
											 * before giving up while fetching
											 * the configured watchdog node
											 * information from watchdog
											 * process through IPC channel */

/*
 * thread argument for lifecheck of pgpool
 */
typedef struct
{
	LifeCheckNode *lifeCheckNode;
	int			retry;			/* retry times (not used?) */
	char		*password;
}			WdPgpoolThreadArg;

typedef struct WdUpstreamConnectionData
{
	char	   *hostname;		/* host name of server */
	pid_t		pid;			/* pid of ping process */
	bool		reachable;		/* true if last ping was successful */
}			WdUpstreamConnectionData;


List	   *g_trusted_server_list = NIL;

static void wd_initialize_trusted_servers_list(void);
static bool wd_ping_all_server(void);
static WdUpstreamConnectionData * wd_get_server_from_pid(pid_t pid);

static void *thread_ping_pgpool(void *arg);
static PGconn *create_conn(char *hostname, int port, char *password);

static pid_t lifecheck_main(void);
static void check_pgpool_status(void);
static void check_pgpool_status_by_query(void);
static void check_pgpool_status_by_hb(void);
static int	ping_pgpool(PGconn *conn);
static int	is_parent_alive(void);
static bool fetch_watchdog_nodes_data(void);
static int	wd_check_heartbeat(LifeCheckNode * node);

static void load_watchdog_nodes_from_json(char *json_data, int len);
static void spawn_lifecheck_children(void);

static RETSIGTYPE lifecheck_exit_handler(int sig);
static RETSIGTYPE reap_handler(int sig);
static pid_t wd_reaper_lifecheck(pid_t pid, int status);

static bool lifecheck_kill_all_children(int sig);
static const char *lifecheck_child_name(pid_t pid);
static void reaper(void);
static int	is_wd_lifecheck_ready(void);
static int	wd_lifecheck(void);
static int	wd_ping_pgpool(LifeCheckNode * node, char *password);
static pid_t fork_lifecheck_child(void);


LifeCheckCluster *gslifeCheckCluster = NULL;	/* lives in shared memory */

pid_t	   *g_hb_receiver_pid = NULL;	/* Array of heart beat receiver child
										 * pids */
pid_t	   *g_hb_sender_pid = NULL; /* Array of heart beat sender child pids */
static volatile sig_atomic_t sigchld_request = 0;


/*
 * handle SIGCHLD
 */
static RETSIGTYPE reap_handler(int sig)
{
	POOL_SETMASK(&BlockSig);
	sigchld_request = 1;
	POOL_SETMASK(&UnBlockSig);
}


static const char *
lifecheck_child_name(pid_t pid)
{
	int			i;

	for (i = 0; i < pool_config->num_hb_dest_if; i++)
	{
		if (g_hb_receiver_pid && pid == g_hb_receiver_pid[i])
			return "heartBeat receiver";
		else if (g_hb_sender_pid && pid == g_hb_sender_pid[i])
			return "heartBeat sender";
	}
	/* Check if it was a ping to trusted server process */
	WdUpstreamConnectionData *server = wd_get_server_from_pid(pid);

	if (server)
		return "trusted host ping process";
	return "unknown";
}

static void
reaper(void)
{
	pid_t		pid;
	int			status;

	ereport(DEBUG1,
			(errmsg("Lifecheck child reaper handler")));

	/* clear SIGCHLD request */
	sigchld_request = 0;

	while ((pid = pool_waitpid(&status)) > 0)
	{
		/* First check if it is the trusted server ping process */
		WdUpstreamConnectionData *server = wd_get_server_from_pid(pid);

		if (server)
		{
			server->reachable = (status == 0);
			server->pid = 0;
		}
		else
			wd_reaper_lifecheck(pid, status);
	}
}

static pid_t
wd_reaper_lifecheck(pid_t pid, int status)
{
	int			i;
	bool		restart_child = true;
	const char *proc_name = lifecheck_child_name(pid);

	if (WIFEXITED(status))
	{
		if (WEXITSTATUS(status) == POOL_EXIT_FATAL)
			ereport(LOG,
					(errmsg("lifecheck child process (%s) with pid: %d exit with FATAL ERROR.", proc_name, pid)));
		else if (WEXITSTATUS(status) == POOL_EXIT_NO_RESTART)
		{
			restart_child = false;
			ereport(LOG,
					(errmsg("lifecheck child process (%s) with pid: %d exit with SUCCESS.", lifecheck_child_name(pid), pid)));
		}
	}
	if (WIFSIGNALED(status))
	{
		/* Child terminated by segmentation fault. Report it */
		if (WTERMSIG(status) == SIGSEGV)
			ereport(WARNING,
					(errmsg("lifecheck child process (%s) with pid: %d was terminated by segmentation fault", proc_name, pid)));
		else
			ereport(LOG,
					(errmsg("lifecheck child process (%s) with pid: %d exits with status %d by signal %d", proc_name, pid, status, WTERMSIG(status))));
	}
	else
		ereport(LOG,
				(errmsg("lifecheck child process (%s) with pid: %d exits with status %d", proc_name, pid, status)));

	if (g_hb_receiver_pid == NULL && g_hb_sender_pid == NULL)
		return -1;

	for (i = 0; i < pool_config->num_hb_dest_if; i++)
	{
		if (g_hb_receiver_pid && pid == g_hb_receiver_pid[i])
		{
			if (restart_child)
			{
				g_hb_receiver_pid[i] = wd_hb_receiver(1, &(pool_config->hb_dest_if[i]));
				ereport(LOG,
						(errmsg("fork a new %s process with pid: %d", proc_name, g_hb_receiver_pid[i])));
			}
			else
				g_hb_receiver_pid[i] = 0;

			return g_hb_receiver_pid[i];
		}

		else if (g_hb_sender_pid && pid == g_hb_sender_pid[i])
		{
			if (restart_child)
			{
				g_hb_sender_pid[i] = wd_hb_sender(1, &(pool_config->hb_dest_if[i]));
				ereport(LOG,
						(errmsg("fork a new %s process with pid: %d", proc_name, g_hb_sender_pid[i])));
			}
			else
				g_hb_sender_pid[i] = 0;

			return g_hb_sender_pid[i];
		}
	}
	return -1;
}

static bool
lifecheck_kill_all_children(int sig)
{
	bool		ret = false;

	if (pool_config->wd_lifecheck_method == LIFECHECK_BY_HB
		&& g_hb_receiver_pid && g_hb_sender_pid)
	{
		int			i;

		for (i = 0; i < pool_config->num_hb_dest_if; i++)
		{
			pid_t		pid_child = g_hb_receiver_pid[i];

			if (pid_child > 0)
			{
				kill(pid_child, sig);
				ret = true;
			}

			pid_child = g_hb_sender_pid[i];

			if (pid_child > 0)
			{
				kill(pid_child, sig);
				ret = true;
			}
		}
	}
	return ret;
}

static RETSIGTYPE
lifecheck_exit_handler(int sig)
{
	pid_t		wpid;
	bool		child_killed;

	POOL_SETMASK(&AuthBlockSig);
	ereport(DEBUG1,
			(errmsg("lifecheck child receives shutdown request signal %d, forwarding to all children", sig)));

	if (sig == SIGTERM)			/* smart shutdown */
	{
		ereport(DEBUG1,
				(errmsg("lifecheck child receives smart shutdown request")));
	}
	else if (sig == SIGINT)
	{
		ereport(DEBUG1,
				(errmsg("lifecheck child receives fast shutdown request")));
	}
	else if (sig == SIGQUIT)
	{
		ereport(DEBUG1,
				(errmsg("lifecheck child receives immediate shutdown request")));
	}

	child_killed = lifecheck_kill_all_children(sig);

	POOL_SETMASK(&UnBlockSig);

	if (child_killed)
	{
		do
		{
			wpid = wait(NULL);
		} while (wpid > 0 || (wpid == -1 && errno == EINTR));

		if (wpid == -1 && errno != ECHILD)
			ereport(WARNING,
					(errmsg("wait() on lifecheck children failed"),
					 errdetail("%m")));

		if (g_hb_receiver_pid)
			pfree(g_hb_receiver_pid);

		if (g_hb_sender_pid)
			pfree(g_hb_sender_pid);

		g_hb_receiver_pid = NULL;
		g_hb_sender_pid = NULL;
	}
	exit(0);
}

pid_t
initialize_watchdog_lifecheck(void)
{
	if (!pool_config->use_watchdog)
		return 0;

	if (pool_config->wd_lifecheck_method == LIFECHECK_BY_EXTERNAL)
		return 0;

	return fork_lifecheck_child();
}

/*
 * fork a child for lifecheck
 */
static pid_t
fork_lifecheck_child(void)
{
	pid_t		pid;

	pid = fork();

	if (pid == 0)
	{
		on_exit_reset();
		SetProcessGlobalVariables(PT_LIFECHECK);

		/* call lifecheck child main */
		POOL_SETMASK(&UnBlockSig);
		lifecheck_main();
	}
	else if (pid == -1)
	{
		ereport(FATAL,
				(errmsg("fork() failed"),
				 errdetail("%m")));
	}

	return pid;
}


/* main entry point of watchdog lifecheck process*/
static pid_t
lifecheck_main(void)
{
	sigjmp_buf	local_sigjmp_buf;
	int			i;

	ereport(DEBUG1,
			(errmsg("I am watchdog lifecheck child with pid:%d", getpid())));

	/* Identify myself via ps */
	init_ps_display("", "", "", "");

	pool_signal(SIGTERM, lifecheck_exit_handler);
	pool_signal(SIGINT, lifecheck_exit_handler);
	pool_signal(SIGQUIT, lifecheck_exit_handler);
	pool_signal(SIGCHLD, reap_handler);
	pool_signal(SIGHUP, SIG_IGN);
	pool_signal(SIGPIPE, SIG_IGN);

	/* Create per loop iteration memory context */
	ProcessLoopContext = AllocSetContextCreate(TopMemoryContext,
											   "wd_lifecheck_main_loop",
											   ALLOCSET_DEFAULT_MINSIZE,
											   ALLOCSET_DEFAULT_INITSIZE,
											   ALLOCSET_DEFAULT_MAXSIZE);

	MemoryContextSwitchTo(TopMemoryContext);

	set_ps_display("lifecheck", false);

	/*
	 * Get the list of watchdog node to monitor from watchdog process
	 */
	for (i = 0; i < LIFECHECK_GETNODE_WAIT_SEC_COUNT; i++)
	{
		if (fetch_watchdog_nodes_data() == true)
			break;
		sleep(1);
	}

	if (!gslifeCheckCluster)
		ereport(ERROR,
				(errmsg("unable to initialize lifecheck, watchdog not responding")));

	spawn_lifecheck_children();

	wd_initialize_trusted_servers_list();

	/* wait until ready to go */
	while (WD_OK != is_wd_lifecheck_ready())
	{
		sleep(pool_config->wd_interval * 10);
	}

	ereport(LOG,
			(errmsg("watchdog: lifecheck started")));

	if (sigsetjmp(local_sigjmp_buf, 1) != 0)
	{
		/* Since not using PG_TRY, must reset error stack by hand */
		error_context_stack = NULL;

		EmitErrorReport();
		MemoryContextSwitchTo(TopMemoryContext);
		FlushErrorState();
		sleep(pool_config->wd_heartbeat_keepalive);
	}

	/* We can now handle ereport(ERROR) */
	PG_exception_stack = &local_sigjmp_buf;

	/* watchdog loop */
	for (;;)
	{
		MemoryContextSwitchTo(ProcessLoopContext);
		MemoryContextResetAndDeleteChildren(ProcessLoopContext);

		if (sigchld_request)
			reaper();

		/* pgpool life check */
		wd_lifecheck();
		sleep(pool_config->wd_interval);
	}

	return 0;
}

static void
spawn_lifecheck_children(void)
{
	if (pool_config->wd_lifecheck_method == LIFECHECK_BY_HB)
	{
		int			i;

		g_hb_receiver_pid = palloc0(sizeof(pid_t) * pool_config->num_hb_dest_if);
		g_hb_sender_pid = palloc0(sizeof(pid_t) * pool_config->num_hb_dest_if);

		for (i = 0; i < pool_config->num_hb_dest_if; i++)
		{
			/* heartbeat receiver process */
			g_hb_receiver_pid[i] = wd_hb_receiver(1, &(pool_config->hb_dest_if[i]));

			/* heartbeat sender process */
			g_hb_sender_pid[i] = wd_hb_sender(1, &(pool_config->hb_dest_if[i]));
		}
	}
}

static void
print_lifecheck_cluster(void)
{
	int			i;

	if (!gslifeCheckCluster)
		return;
	ereport(LOG,
			(errmsg("%d watchdog nodes are configured for lifecheck", gslifeCheckCluster->nodeCount)));
	for (i = 0; i < gslifeCheckCluster->nodeCount; i++)
	{
		ereport(LOG,
				(errmsg("watchdog nodes ID:%d Name:\"%s\"", gslifeCheckCluster->lifeCheckNodes[i].ID, gslifeCheckCluster->lifeCheckNodes[i].nodeName),
				 errdetail("Host:\"%s\" WD Port:%d pgpool-II port:%d",
						   gslifeCheckCluster->lifeCheckNodes[i].hostName,
						   gslifeCheckCluster->lifeCheckNodes[i].wdPort,
						   gslifeCheckCluster->lifeCheckNodes[i].pgpoolPort)));
	}
}

static bool
inform_node_status(LifeCheckNode * node, char *message)
{
	int			node_status,
				x;
	char	   *json_data;
	WDIPCCmdResult *res = NULL;
	char	   *new_status;

	if (node->nodeState == NODE_DEAD)
	{
		new_status = "NODE DEAD";
		node_status = WD_LIFECHECK_NODE_STATUS_DEAD;
	}
	else if (node->nodeState == NODE_ALIVE)
	{
		new_status = "NODE ALIVE";
		node_status = WD_LIFECHECK_NODE_STATUS_ALIVE;
	}
	else
		return false;

	ereport(LOG,
			(errmsg("informing the node status change to watchdog"),
			 errdetail("node id :%d status = \"%s\" message:\"%s\"", node->ID, new_status, message)));

	json_data = get_lifecheck_node_status_change_json(node->ID, node_status, message, pool_config->wd_authkey);
	if (json_data == NULL)
		return false;

	for (x = 0; x < MAX_SEC_WAIT_FOR_CLUSTER_TRANSACTION; x++)
	{
		res = issue_command_to_watchdog(WD_NODE_STATUS_CHANGE_COMMAND, 0, json_data, strlen(json_data), false);
		if (res)
			break;
		sleep(1);
	}
	pfree(json_data);
	if (res)
	{
		pfree(res);
		return true;
	}
	return false;
}

static bool
fetch_watchdog_nodes_data(void)
{
	char	   *json_data = wd_internal_get_watchdog_nodes_json(-1);

	if (json_data == NULL)
	{
		ereport(ERROR,
				(errmsg("get node list command reply contains no data")));
		return false;
	}
	load_watchdog_nodes_from_json(json_data, strlen(json_data));
	pfree(json_data);
	return true;
}

static void
load_watchdog_nodes_from_json(char *json_data, int len)
{
	size_t shmem_size;
	json_value *root;
	json_value *value;
	int			i,
				nodeCount;

	root = json_parse(json_data, len);

	/* The root node must be object */
	if (root == NULL || root->type != json_object)
	{
		json_value_free(root);
		ereport(ERROR,
				(errmsg("unable to parse json data for node list")));
	}

	if (json_get_int_value_for_key(root, "NodeCount", &nodeCount))
	{
		json_value_free(root);
		ereport(ERROR,
				(errmsg("invalid json data"),
				 errdetail("unable to find NodeCount node from data")));
	}

	/* find the WatchdogNodes array */
	value = json_get_value_for_key(root, "WatchdogNodes");
	if (value == NULL)
	{
		json_value_free(root);
		ereport(ERROR,
				(errmsg("invalid json data"),
				 errdetail("unable to find WatchdogNodes node from data")));
	}
	if (value->type != json_array)
	{
		json_value_free(root);
		ereport(ERROR,
				(errmsg("invalid json data"),
				 errdetail("WatchdogNodes node does not contains Array")));
	}
	if (nodeCount != value->u.array.length)
	{
		json_value_free(root);
		ereport(ERROR,
				(errmsg("invalid json data"),
				 errdetail("WatchdogNodes array contains %d nodes while expecting %d", value->u.array.length, nodeCount)));
	}

	/* okay we are done, put this in shared memory */
	shmem_size = MAXALIGN(sizeof(LifeCheckCluster));
	shmem_size += MAXALIGN(sizeof(LifeCheckNode) * nodeCount);

	gslifeCheckCluster = pool_shared_memory_create(shmem_size);
	gslifeCheckCluster->nodeCount = nodeCount;
	gslifeCheckCluster->lifeCheckNodes = (LifeCheckNode*)((char*)gslifeCheckCluster + MAXALIGN(sizeof(LifeCheckCluster)));
	for (i = 0; i < nodeCount; i++)
	{
		WDNodeInfo *nodeInfo = parse_watchdog_node_info_from_wd_node_json(value->u.array.values[i]);
		
		gslifeCheckCluster->lifeCheckNodes[i].wdState = nodeInfo->state;
		strcpy(gslifeCheckCluster->lifeCheckNodes[i].stateName, nodeInfo->stateName);
		gslifeCheckCluster->lifeCheckNodes[i].nodeState = NODE_EMPTY; /* This is local health check state*/
		gslifeCheckCluster->lifeCheckNodes[i].ID = nodeInfo->id;
		strcpy(gslifeCheckCluster->lifeCheckNodes[i].hostName, nodeInfo->hostName);
		strcpy(gslifeCheckCluster->lifeCheckNodes[i].nodeName, nodeInfo->nodeName);
		gslifeCheckCluster->lifeCheckNodes[i].wdPort = nodeInfo->wd_port;
		gslifeCheckCluster->lifeCheckNodes[i].pgpoolPort = nodeInfo->pgpool_port;
		gslifeCheckCluster->lifeCheckNodes[i].retry_lives = pool_config->wd_life_point;
		pfree(nodeInfo);
	}
	print_lifecheck_cluster();
	json_value_free(root);
}


static int
is_wd_lifecheck_ready(void)
{
	int			rtn = WD_OK;
	int			i;
	char	   *password = NULL;

	for (i = 0; i < gslifeCheckCluster->nodeCount; i++)
	{
		LifeCheckNode *node = &gslifeCheckCluster->lifeCheckNodes[i];

		/* query mode */
		if (pool_config->wd_lifecheck_method == LIFECHECK_BY_QUERY)
		{
			if (!password)
				password = get_pgpool_config_user_password(pool_config->wd_lifecheck_user,
														   pool_config->wd_lifecheck_password);
			if (wd_ping_pgpool(node, password) == WD_NG)
			{
				ereport(DEBUG1,
						(errmsg("watchdog checking life check is ready"),
						 errdetail("pgpool:%d at \"%s:%d\" has not started yet",
								   i, node->hostName, node->pgpoolPort)));
				rtn = WD_NG;
			}
		}
		/* heartbeat mode */
		else if (pool_config->wd_lifecheck_method == LIFECHECK_BY_HB)
		{
			if (node->ID == pool_config->pgpool_node_id)	/* local node */
				continue;

			if (!WD_TIME_ISSET(node->hb_last_recv_time) ||
				!WD_TIME_ISSET(node->hb_send_time))
			{
				ereport(DEBUG1,
						(errmsg("watchdog checking life check is ready"),
						 errdetail("pgpool:%d at \"%s:%d\" has not send the heartbeat signal yet",
								   i, node->hostName, node->pgpoolPort)));
				rtn = WD_NG;
			}
		}
		/* otherwise */
		else
		{
			ereport(ERROR,
					(errmsg("checking if watchdog is ready, unknown watchdog mode \"%d\"",
							pool_config->wd_lifecheck_method)));
		}
	}

	if (password)
		pfree(password);

	return rtn;
}

/*
 * Check if pgpool is living
 */
static int
wd_lifecheck(void)
{
	/* check upper connection */
	if (strlen(pool_config->trusted_servers))
	{
		if (wd_ping_all_server() == false)
		{
			LifeCheckNode *node = &gslifeCheckCluster->lifeCheckNodes[0];

			if (node->nodeState != NODE_DEAD)
			{
				node->nodeState = NODE_DEAD;
				ereport(WARNING,
						(errmsg("watchdog lifecheck, failed to connect to any trusted servers")));

				inform_node_status(node, "trusted server is unreachable");
			}
			return WD_NG;
		}
	}

	/* skip lifecheck during recovery execution */
	if (*InRecovery != RECOVERY_INIT)
	{
		return WD_OK;
	}

	/* check and update pgpool status */
	check_pgpool_status();

	return WD_OK;
}

/*
 * check and update pgpool status
 */
static void
check_pgpool_status()
{
	/* query mode */
	if (pool_config->wd_lifecheck_method == LIFECHECK_BY_QUERY)
	{
		check_pgpool_status_by_query();
	}
	/* heartbeat mode */
	else if (pool_config->wd_lifecheck_method == LIFECHECK_BY_HB)
	{
		check_pgpool_status_by_hb();
	}
}

static void
check_pgpool_status_by_hb(void)
{
	int			i;
	struct timeval tv;
	LifeCheckNode *node = &gslifeCheckCluster->lifeCheckNodes[0];

	gettimeofday(&tv, NULL);

	/* about myself */
	/* parent is dead so it's orphan.... */
	if (is_parent_alive() == WD_NG && node->nodeState != NODE_DEAD)
	{
		node->nodeState = NODE_DEAD;
		ereport(LOG,
				(errmsg("checking pgpool status by heartbeat"),
				 errdetail("lifecheck failed. pgpool (%s:%d) seems not to be working",
						   node->hostName, node->pgpoolPort)));

		inform_node_status(node, "parent process is dead");

	}


	for (i = 1; i < gslifeCheckCluster->nodeCount; i++)
	{
		node = &gslifeCheckCluster->lifeCheckNodes[i];
		ereport(DEBUG1,
				(errmsg("watchdog life checking by heartbeat"),
				 errdetail("checking pgpool %d (%s:%d)",
						   i, node->hostName, node->pgpoolPort)));

		if (wd_check_heartbeat(node) == WD_NG)
		{
			ereport(DEBUG2,
					(errmsg("checking pgpool status by heartbeat"),
					 errdetail("lifecheck failed. pgpool: %d at \"%s:%d\" seems not to be working",
							   i, node->hostName, node->pgpoolPort)));

			if (node->nodeState != NODE_DEAD)
			{
				node->nodeState = NODE_DEAD;
				inform_node_status(node, "No heartbeat signal from node");
			}
		}
		else
		{
			ereport(DEBUG1,
					(errmsg("checking pgpool status by heartbeat"),
					 errdetail("OK; status OK")));
		}
	}
}

static void
check_pgpool_status_by_query(void)
{
	pthread_attr_t attr;
	pthread_t	thread[MAX_WATCHDOG_NUM];
	WdPgpoolThreadArg thread_arg[MAX_WATCHDOG_NUM];
	LifeCheckNode *node;
	int			rc,
				i;
	char	   *password = get_pgpool_config_user_password(pool_config->wd_lifecheck_user,
														   pool_config->wd_lifecheck_password);

	/* thread init */
	pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);

	/* send queries to all pgpools using threads */
	for (i = 0; i < gslifeCheckCluster->nodeCount; i++)
	{
		node = &gslifeCheckCluster->lifeCheckNodes[i];
		thread_arg[i].lifeCheckNode = node;
		thread_arg[i].password = password;
		rc = watchdog_thread_create(&thread[i], &attr, thread_ping_pgpool, (void *) &thread_arg[i]);
		if (rc)
		{
			ereport(WARNING,
					(errmsg("failed to create thread for checking pgpool status by query for  %d (%s:%d)",
							i, node->hostName, node->pgpoolPort),
					 errdetail("pthread_create failed with error code %d: %s",rc, strerror(rc))));
		}
	}

	pthread_attr_destroy(&attr);

	/* check results of queries */
	for (i = 0; i < gslifeCheckCluster->nodeCount; i++)
	{
		int			result;

		node = &gslifeCheckCluster->lifeCheckNodes[i];

		ereport(DEBUG1,
				(errmsg("checking pgpool status by query"),
				 errdetail("checking pgpool %d (%s:%d)",
						   i, node->hostName, node->pgpoolPort)));

		rc = pthread_join(thread[i], (void **) &result);
		if ((rc != 0) && (errno == EINTR))
		{
			usleep(100);
			continue;
		}

		if (result == WD_OK)
		{
			ereport(DEBUG1,
					(errmsg("checking pgpool status by query"),
					 errdetail("WD_OK: status: %d", node->nodeState)));

			/* life point init */
			node->retry_lives = pool_config->wd_life_point;
		}
		else
		{
			ereport(DEBUG1,
					(errmsg("checking pgpool status by query"),
					 errdetail("NG; status: %d life:%d", node->nodeState, node->retry_lives)));
			if (node->retry_lives > 0)
			{
				node->retry_lives--;
			}

			/* pgpool goes down */
			if (node->retry_lives <= 0)
			{
				ereport(LOG,
						(errmsg("checking pgpool status by query"),
						 errdetail("lifecheck failed %d times. pgpool %d (%s:%d) seems not to be working",
								   pool_config->wd_life_point, i, node->hostName, node->pgpoolPort)));

				if (node->nodeState == NODE_DEAD)
					continue;
				node->nodeState = NODE_DEAD;
				/* It's me! */
				if (i == pool_config->pgpool_node_id)
					inform_node_status(node, "parent process is dead");
				else
					inform_node_status(node, "unable to connect to node");
			}
		}
	}

	if (password)
		pfree(password);
}

/*
 * Thread function to send lifecheck query to pgpool
 * Used in wd_lifecheck.
 */
static void *
thread_ping_pgpool(void *arg)
{
	uintptr_t	rtn;
	WdPgpoolThreadArg *thread_arg = (WdPgpoolThreadArg *) arg;
	rtn = (uintptr_t) wd_ping_pgpool(thread_arg->lifeCheckNode,thread_arg->password);

	pthread_exit((void *) rtn);
}

/*
 * Create connection to pgpool
 */
static PGconn *
create_conn(char *hostname, int port, char *password)
{
	static char conninfo[1024];
	PGconn	   *conn;


	if (strlen(pool_config->wd_lifecheck_dbname) == 0)
	{
		ereport(WARNING,
				(errmsg("watchdog life checking, wd_lifecheck_dbname is empty")));
		return NULL;
	}

	if (strlen(pool_config->wd_lifecheck_user) == 0)
	{
		ereport(WARNING,
				(errmsg("watchdog life checking, wd_lifecheck_user is empty")));
		return NULL;
	}

	snprintf(conninfo, sizeof(conninfo),
			 "host='%s' port='%d' dbname='%s' user='%s' password='%s' connect_timeout='%d'",
			 hostname,
			 port,
			 pool_config->wd_lifecheck_dbname,
			 pool_config->wd_lifecheck_user,
			 password ? password : "",
			 pool_config->wd_interval / 2 + 1);
	conn = PQconnectdb(conninfo);

	if (PQstatus(conn) != CONNECTION_OK)
	{
		ereport(DEBUG1,
				(errmsg("watchdog life checking"),
				 errdetail("Connection to database failed: %s", PQerrorMessage(conn))));
		PQfinish(conn);
		return NULL;
	}
	return conn;
}


/*
 * Check if pgpool is alive using heartbeat signal.
 */
static int
wd_check_heartbeat(LifeCheckNode * node)
{
	int			interval;
	struct timeval tv;

	if (!WD_TIME_ISSET(node->hb_last_recv_time) ||
		!WD_TIME_ISSET(node->hb_send_time))
	{
		ereport(DEBUG1,
				(errmsg("watchdog checking if pgpool is alive using heartbeat"),
				 errdetail("pgpool (%s:%d) was restarted and has not send the heartbeat signal yet",
						   node->hostName, node->pgpoolPort)));
		return WD_OK;
	}

	gettimeofday(&tv, NULL);

	interval = WD_TIME_DIFF_SEC(tv, node->hb_last_recv_time);
	ereport(DEBUG1,
			(errmsg("watchdog checking if pgpool is alive using heartbeat"),
			 errdetail("the last heartbeat from \"%s:%d\" received %d seconds ago",
					   node->hostName, node->pgpoolPort, interval)));

	if (interval > pool_config->wd_heartbeat_deadtime)
	{
		return WD_NG;
	}

	if (node->nodeState == NODE_DEAD)
	{
		node->nodeState = NODE_ALIVE;
		inform_node_status(node, "Heartbeat signal found");
	}
	return WD_OK;
}

/*
 * Check if pgpool can accept the lifecheck query.
 */
static int
wd_ping_pgpool(LifeCheckNode * node, char* password)
{
	PGconn	   *conn;

	conn = create_conn(node->hostName, node->pgpoolPort, password);
	if (conn == NULL)
	{
		if(chceck_password_type_is_not_md5(pool_config->wd_lifecheck_user, pool_config->wd_lifecheck_password) == -1)
		{
			ereport(ERROR,
					(errmsg("the password of wd_lifecheck_user %s is invalid format",
						pool_config->recovery_user),
					errdetail("wd_lifecheck_password is not allowed to be md5 hashed format")));
		}
		return WD_NG;
	}
	return ping_pgpool(conn);
}

/* inner function for issuing lifecheck query */
static int
ping_pgpool(PGconn *conn)
{
	int			rtn = WD_NG;
	int			status = PGRES_FATAL_ERROR;
	PGresult   *res = (PGresult *) NULL;

	if (!conn)
	{
		return WD_NG;
	}

	res = PQexec(conn, pool_config->wd_lifecheck_query);

	status = PQresultStatus(res);
	if (res != NULL)
	{
		PQclear(res);
	}

	if ((status != PGRES_NONFATAL_ERROR) &&
		(status != PGRES_FATAL_ERROR))
	{
		rtn = WD_OK;
	}
	PQfinish(conn);

	return rtn;
}

static int
is_parent_alive()
{
	if (mypid == getppid())
		return WD_OK;
	else
		return WD_NG;
}


static void
wd_initialize_trusted_servers_list(void)
{
	char	   *token;
	char	   *tmpString;
	const char *delimi = ",";

	if (g_trusted_server_list)
		return;

	if (strlen(pool_config->trusted_servers) <= 0)
		return;

	/* This has to be created in TopMemoryContext */
	MemoryContext oldCxt = MemoryContextSwitchTo(TopMemoryContext);

	tmpString = pstrdup(pool_config->trusted_servers);
	for (token = strtok(tmpString, delimi); token != NULL; token = strtok(NULL, delimi))
	{
		WdUpstreamConnectionData *server = palloc(sizeof(WdUpstreamConnectionData));

		server->pid = 0;
		server->reachable = false;
		server->hostname = pstrdup(token);
		g_trusted_server_list = lappend(g_trusted_server_list, server);

		ereport(LOG,
				(errmsg("watchdog lifecheck trusted server \"%s\" added for the availability check", token)));
	}
	pfree(tmpString);
	MemoryContextSwitchTo(oldCxt);
}

static bool
wd_ping_all_server(void)
{
	ListCell   *lc;
	pid_t		pid;
	int			status;
	int			ping_process = 0;

	POOL_SETMASK(&BlockSig);

	foreach(lc, g_trusted_server_list)
	{
		WdUpstreamConnectionData *server = (WdUpstreamConnectionData *) lfirst(lc);

		if (server->pid <= 0)
			server->pid = wd_trusted_server_command(server->hostname);

		if (server->pid > 0)
			ping_process++;
	}

	while (ping_process > 0)
	{
		pid = waitpid(0, &status, 0);
		if (pid > 0)
		{
			/* find the server object associated with this pid */
			WdUpstreamConnectionData *server = wd_get_server_from_pid(pid);

			if (server)
			{
				ping_process--;
				server->reachable = (status == 0);
				server->pid = 0;
				if (server->reachable)
				{
					/* one reachable server is all we need */
					POOL_SETMASK(&UnBlockSig);
					return true;
				}
				if (WIFEXITED(status) == 0 || WEXITSTATUS(status) != 0)
				{
					ereport(WARNING,
							(errmsg("watchdog failed to ping host: \"%s\"", server->hostname)));
				}
			}
			else
			{
				/* It was not a ping host child process */
				wd_reaper_lifecheck(pid, status);
			}
		}
		if (pid == -1)			/* wait pid error */
		{
			if (errno == EINTR)
				continue;
			ereport(WARNING,
					(errmsg("failed to check the ping status of trusted servers"),
					 errdetail("waitpid failed with reason: %m")));
			break;
		}
	}
	POOL_SETMASK(&UnBlockSig);
	return false;
}

static WdUpstreamConnectionData * wd_get_server_from_pid(pid_t pid)
{
	ListCell   *lc;

	foreach(lc, g_trusted_server_list)
	{
		WdUpstreamConnectionData *server = (WdUpstreamConnectionData *) lfirst(lc);

		if (server->pid == pid)
		{
			return server;
		}
	}
	return NULL;
}
