/* -*-pgpool_main-c-*- */
/*
 * $Header$
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
 */

/*
 * pool_internal_comms consists of functions that can be called
 * from any pgpool-II process to instruct pgpool-II main process to
 * perform a particular function
 */
#include <stdio.h>
#include <signal.h>
#include <unistd.h>

#include "utils/elog.h"
#include "utils/pool_signal.h"

#include "pool.h"
#include "main/pool_internal_comms.h"
#include "watchdog/wd_internal_commands.h"
#include "utils/palloc.h"
#include "utils/memutils.h"
#include "pool_config.h"

/*
 * sends the signal to pgpool-II main process to terminate Pgpool-II
 * process.
 */
bool terminate_pgpool(char mode, bool error)
{

	pid_t		ppid = getppid();
	int			sig;

	if (mode == 's')
	{
		ereport(DEBUG1,
				(errmsg("processing shutdown request"),
				 errdetail("sending SIGTERM to the parent process with PID:%d", ppid)));
		sig = SIGTERM;
	}
	else if (mode == 'f')
	{
		ereport(DEBUG1,
				(errmsg("processing shutdown request"),
				 errdetail("sending SIGINT to the parent process with PID:%d", ppid)));
		sig = SIGINT;
	}
	else if (mode == 'i')
	{
		ereport(DEBUG1,
				(errmsg("processing shutdown request"),
				 errdetail("sending SIGQUIT to the parent process with PID:%d", ppid)));
		sig = SIGQUIT;
	}
	else
	{
		ereport(error?ERROR:WARNING,
				(errmsg("error while processing shutdown request"),
				 errdetail("invalid shutdown mode \"%c\"", mode)));
		return false;
	}

	pool_signal_parent(sig);
	return true;
}

/*
 * degenerate_backend_set_ex:
 *
 * The function registers/verifies the node down operation request.
 * The request is then processed by failover function.
 *
 * node_id_set:	array of node ids to be registered for NODE DOWN operation
 * count:		number of elements in node_id_set array
 * error:		if set error is thrown as soon as any node id is found in
 *				node_id_set on which operation could not be performed.
 * test_only:	When set, function only checks if NODE DOWN operation can be
 *				executed on provided node ids and never registers the operation
 *				request.
 *				For test_only case function returns false or throws an error as
 *				soon as first non compliant node in node_id_set is found
 * switch_over: if set, the request is originated by switch over, not errors.
 *
 * wd_failover_id: The watchdog internal ID for this failover
 */
bool
degenerate_backend_set_ex(int *node_id_set, int count, unsigned char flags, bool error, bool test_only)
{
	int			i;
	int			node_id[MAX_NUM_BACKENDS];
	int			node_count = 0;
	int			elevel = LOG;

	if (error)
		elevel = ERROR;

	for (i = 0; i < count; i++)
	{
		if (node_id_set[i] < 0 || node_id_set[i] >= MAX_NUM_BACKENDS ||
			(!VALID_BACKEND(node_id_set[i]) && BACKEND_INFO(node_id_set[i]).quarantine == false))
		{
			if (node_id_set[i] < 0 || node_id_set[i] >= MAX_NUM_BACKENDS)
				ereport(elevel,
						(errmsg("invalid degenerate backend request, node id: %d is out of range. node id must be between [0 and %d]"
								,node_id_set[i], MAX_NUM_BACKENDS)));
			else
				ereport(elevel,
						(errmsg("invalid degenerate backend request, node id : %d status: [%d] is not valid for failover"
								,node_id_set[i], BACKEND_INFO(node_id_set[i]).backend_status)));
			if (test_only)
				return false;

			continue;
		}

		if (POOL_DISALLOW_TO_FAILOVER(BACKEND_INFO(node_id_set[i]).flag))
		{
			ereport(elevel,
					(errmsg("degenerate backend request for node_id: %d from pid [%d] is canceled because failover is disallowed on the node",
							node_id_set[i], getpid())));
			if (test_only)
				return false;
		}
		else
		{
			if (!test_only)		/* do not produce this log if we are in
								 * testing mode */
				ereport(LOG,
						(errmsg("received degenerate backend request for node_id: %d from pid [%d]",
								node_id_set[i], getpid())));

			node_id[node_count++] = node_id_set[i];
		}
	}

	if (node_count)
	{
		WDFailoverCMDResults res = FAILOVER_RES_PROCEED;

		/* If this was only a test. Inform the caller without doing anything */
		if (test_only)
			return true;

		if (!(flags & REQ_DETAIL_WATCHDOG))
		{
			int			x;

			for (x = 0; x < MAX_SEC_WAIT_FOR_CLUSTER_TRANSACTION; x++)
			{
				res = wd_degenerate_backend_set(node_id_set, count, flags);
				if (res != FAILOVER_RES_TRANSITION)
					break;
				sleep(1);
			}
		}
		if (res == FAILOVER_RES_TRANSITION)
		{
			/*
			 * What to do when cluster is still not stable Is proceeding to
			 * failover is the right choice ???
			 */
			ereport(NOTICE,
					(errmsg("received degenerate backend request for %d node(s) from pid [%d], But cluster is not in stable state"
							,node_count, getpid())));
		}
		if (res == FAILOVER_RES_PROCEED)
		{
			register_node_operation_request(NODE_DOWN_REQUEST, node_id, node_count, flags);
		}
		else if (res == FAILOVER_RES_NO_QUORUM)
		{
			ereport(LOG,
					(errmsg("degenerate backend request for %d node(s) from pid [%d], is changed to quarantine node request by watchdog"
							,node_count, getpid()),
					 errdetail("watchdog does not holds the quorum")));

			register_node_operation_request(NODE_QUARANTINE_REQUEST, node_id, node_count, flags);
		}
		else if (res == FAILOVER_RES_CONSENSUS_MAY_FAIL)
		{
			ereport(LOG,
					(errmsg("degenerate backend request for %d node(s) from pid [%d], is changed to quarantine node request by watchdog"
							,node_count, getpid()),
					 errdetail("watchdog is taking time to build consensus")));
			register_node_operation_request(NODE_QUARANTINE_REQUEST, node_id, node_count, flags);
		}
		else if (res == FAILOVER_RES_BUILDING_CONSENSUS)
		{
			ereport(LOG,
					(errmsg("degenerate backend request for node_id: %d from pid [%d], will be handled by watchdog, which is building consensus for request"
							,*node_id, getpid())));
		}
		else if (res == FAILOVER_RES_WILL_BE_DONE)
		{
			/* we will receive a sync request from leader watchdog node */
			ereport(LOG,
					(errmsg("degenerate backend request for %d node(s) from pid [%d], will be handled by watchdog"
							,node_count, getpid())));
		}
		else
		{
			ereport(elevel,
					(errmsg("degenerate backend request for %d node(s) from pid [%d] is canceled by other pgpool"
							,node_count, getpid())));
			return false;
		}
	}
	return true;
}

/*
 * wrapper over degenerate_backend_set_ex function to register
 * NODE down operation request
 */
bool
degenerate_backend_set(int *node_id_set, int count, unsigned char flags)
{
	return degenerate_backend_set_ex(node_id_set, count, flags, false, false);
}

/* send promote node request using SIGUSR1 */
bool
promote_backend(int node_id, unsigned char flags)
{
	WDFailoverCMDResults res = FAILOVER_RES_PROCEED;
	bool		ret = false;

	if (!SL_MODE)
	{
		return false;
	}

	if (node_id < 0 || node_id >= MAX_NUM_BACKENDS || !VALID_BACKEND(node_id))
	{
		if (node_id < 0 || node_id >= MAX_NUM_BACKENDS)
			ereport(LOG,
					(errmsg("invalid promote backend request, node id: %d is out of range. node id must be between [0 and %d]"
							,node_id, MAX_NUM_BACKENDS)));
		else
			ereport(LOG,
					(errmsg("invalid promote backend request, node id : %d status: [%d] not valid"
							,node_id, BACKEND_INFO(node_id).backend_status)));
		return false;
	}
	ereport(LOG,
			(errmsg("received promote backend request for node_id: %d from pid [%d]",
					node_id, getpid())));

	/* If this was only a test. Inform the caller without doing anything */
	if (!(flags & REQ_DETAIL_WATCHDOG))
	{
		int			x;

		for (x = 0; x < MAX_SEC_WAIT_FOR_CLUSTER_TRANSACTION; x++)
		{
			res = wd_promote_backend(node_id, flags);
			if (res != FAILOVER_RES_TRANSITION)
				break;
			sleep(1);
		}
	}
	if (res == FAILOVER_RES_TRANSITION)
	{
		/*
		 * What to do when cluster is still not stable Is proceeding to
		 * failover is the right choice ???
		 */
		ereport(NOTICE,
				(errmsg("promote backend request for node_id: %d from pid [%d], But cluster is not in stable state"
						,node_id, getpid())));
	}

	if (res == FAILOVER_RES_PROCEED)
	{
		ret = register_node_operation_request(PROMOTE_NODE_REQUEST, &node_id, 1, flags);
	}
	else if (res == FAILOVER_RES_WILL_BE_DONE)
	{
		ereport(LOG,
				(errmsg("promote backend request for node_id: %d from pid [%d], will be handled by watchdog"
						,node_id, getpid())));
	}
	else if (res == FAILOVER_RES_NO_QUORUM)
	{
		ereport(LOG,
				(errmsg("promote backend request for node_id: %d from pid [%d], is canceled because watchdog does not hold quorum"
						,node_id, getpid())));
	}
	else if (res == FAILOVER_RES_BUILDING_CONSENSUS)
	{
		ereport(LOG,
				(errmsg("promote backend request for node_id: %d from pid [%d], will be handled by watchdog, which is building consensus for request"
						,node_id, getpid())));
	}
	else
	{
		ereport(LOG,
				(errmsg("promote backend request for node_id: %d from pid [%d] is canceled  by other pgpool"
						,node_id, getpid())));
	}
	return ret;
}

/* send failback request using SIGUSR1 */
bool
send_failback_request(int node_id, bool throw_error, unsigned char flags)
{
	WDFailoverCMDResults res = FAILOVER_RES_PROCEED;
	bool		ret = false;

	if (node_id < 0 || node_id >= MAX_NUM_BACKENDS ||
		(RAW_MODE && BACKEND_INFO(node_id).backend_status != CON_DOWN && VALID_BACKEND(node_id)))
	{
		if (node_id < 0 || node_id >= MAX_NUM_BACKENDS)
			ereport(throw_error ? ERROR : LOG,
					(errmsg("invalid failback request, node id: %d is out of range. node id must be between [0 and %d]"
							,node_id, MAX_NUM_BACKENDS)));
		else
			ereport(throw_error ? ERROR : LOG,
					(errmsg("invalid failback request, node id : %d status: [%d] not valid for failback"
							,node_id, BACKEND_INFO(node_id).backend_status)));
		return false;
	}

	ereport(LOG,
			(errmsg("received failback request for node_id: %d from pid [%d]",
					node_id, getpid())));

	/* check we are trying to failback the quarantine node */
	if (BACKEND_INFO(node_id).quarantine)
	{
		/* set the update flags */
		ereport(LOG,
				(errmsg("failback request from pid [%d] is changed to update status request because node_id: %d was quarantined",
						getpid(), node_id)));
		flags |= REQ_DETAIL_UPDATE;
	}
	else
	{
		/*
		 * no need to go to watchdog if it's an update or already initiated
		 * from watchdog
		 */
		if (!(flags & REQ_DETAIL_WATCHDOG))
		{
			int			x;

			for (x = 0; x < MAX_SEC_WAIT_FOR_CLUSTER_TRANSACTION; x++)
			{
				res = wd_send_failback_request(node_id, flags);
				if (res != FAILOVER_RES_TRANSITION)
					break;
				sleep(1);
			}
		}
	}

	if (res == FAILOVER_RES_TRANSITION)
	{
		/*
		 * What to do when cluster is still not stable Is proceeding to
		 * failover is the right choice ???
		 */
		ereport(NOTICE,
				(errmsg("failback request for node_id: %d from pid [%d], But cluster is not in stable state"
						,node_id, getpid())));
	}

	if (res == FAILOVER_RES_PROCEED)
	{
		ret = register_node_operation_request(NODE_UP_REQUEST, &node_id, 1, flags);
	}
	else if (res == FAILOVER_RES_WILL_BE_DONE)
	{
		ereport(LOG,
				(errmsg("failback request for node_id: %d from pid [%d], will be handled by watchdog"
						,node_id, getpid())));
	}
	else
	{
		ereport(throw_error ? ERROR : LOG,
				(errmsg("failback request for node_id: %d from pid [%d] is canceled  by other pgpool"
						,node_id, getpid())));
	}
	return ret;
}

/*
 * Request failover. If "switch_over" is false, request all existing sessions
 * restarting.
 */
void
notice_backend_error(int node_id, unsigned char flags)
{
	int			n = node_id;

	if (getpid() == mypid)
	{
		ereport(LOG,
				(errmsg("notice_backend_error: called from pgpool main. ignored.")));
	}
	else
	{
		degenerate_backend_set(&n, 1, flags);
	}
}

void
degenerate_all_quarantine_nodes(void)
{
	int			i;

	for (i = 0; i < NUM_BACKENDS; i++)
	{
		if (BACKEND_INFO(i).quarantine && BACKEND_INFO(i).backend_status == CON_DOWN)
		{
			/* just send the request to watchdog */
			if (wd_degenerate_backend_set(&i, 1, REQ_DETAIL_UPDATE) == FAILOVER_RES_PROCEED)
				register_node_operation_request(NODE_DOWN_REQUEST, &i, 1, REQ_DETAIL_WATCHDOG | REQ_DETAIL_SWITCHOVER);
		}
	}
}


bool
close_idle_connections(void)
{
	return register_node_operation_request(CLOSE_IDLE_REQUEST, NULL, 0, 0);
}
