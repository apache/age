/* -*-pgsql-c-*- */
/*
 *
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
 */

#include <unistd.h>
#include <arpa/inet.h>
#include <time.h>
#include "pool.h"
#include "utils/elog.h"
#include "context/pool_process_context.h"
#include "pool_config.h"		/* remove me afterwards */

static POOL_PROCESS_CONTEXT process_context_d;
static POOL_PROCESS_CONTEXT * process_context;

/*
 * Initialize per process context
 */
void
pool_init_process_context(void)
{
	process_context = &process_context_d;

	if (!process_info)
		ereport(FATAL,
				(return_code(1),
				 errmsg("process info is not set")));
	process_context->process_info = process_info;

	if (!pool_config->backend_desc)
		ereport(FATAL,
				(return_code(1),
				 errmsg("backend desc is not set")));

	process_context->backend_desc = pool_config->backend_desc;

	process_context->proc_id = my_proc_id;

	process_context->local_session_id = 0;	/* initialize local session
											 * counter */

	process_context->last_alarm_handler = SIG_IGN;
	process_context->last_alarm_time = 0;
	process_context->last_alarm_second = 0;
	process_context->undo_alarm_second = 0;
}

/*
 * Return process context
 */
POOL_PROCESS_CONTEXT *
pool_get_process_context(void)
{
	return process_context;
}

/*
 * Return my process info
 */
ProcessInfo *
pool_get_my_process_info(void)
{
	if (!process_context)
		ereport(FATAL,
				(return_code(1),
				 errmsg("process context is not initialized")));

	return &process_context->process_info[process_context->proc_id];
}

/*
 * Increment local session id
 */
void
pool_increment_local_session_id(void)
{
	POOL_PROCESS_CONTEXT *p = pool_get_process_context();

	if (!p)
		ereport(ERROR,
				(errmsg("failed to get process context")));

	p->local_session_id++;
}

/*
 * Return byte size of connection info(ConnectionInfo) on shmem.
 */
size_t
pool_coninfo_size(void)
{
	size_t			size;

	size = pool_config->num_init_children *
		pool_config->max_pool *
		MAX_NUM_BACKENDS *
		sizeof(ConnectionInfo);

	ereport(DEBUG1,
			(errmsg("pool_coninfo_size: num_init_children (%d) * max_pool (%d) * MAX_NUM_BACKENDS (%d) * sizeof(ConnectionInfo) (%zu) = %zu bytes requested for shared memory",
					pool_config->num_init_children,
					pool_config->max_pool,
					MAX_NUM_BACKENDS,
					sizeof(ConnectionInfo),
					size)));
	return size;
}

/*
 * Return number of elements of connection info(ConnectionInfo) on shmem.
 */
int
pool_coninfo_num(void)
{
	int			nelm;

	nelm = pool_config->num_init_children *
		pool_config->max_pool *
		MAX_NUM_BACKENDS;

	return nelm;
}

/*
 * Return pointer to i th child, j th connection pool and k th backend
 * of connection info on shmem.
 */
ConnectionInfo *
pool_coninfo(int child, int connection_pool, int backend)
{
	if (child < 0 || child >= pool_config->num_init_children)
	{
		ereport(WARNING,
				(errmsg("failed to get connection info, invalid child number: %d", child)));
		return NULL;
	}

	if (connection_pool < 0 || connection_pool >= pool_config->max_pool)
	{
		ereport(WARNING,
				(errmsg("failed to get connection info, invalid connection pool number: %d", connection_pool)));
		return NULL;
	}

	if (backend < 0 || backend >= MAX_NUM_BACKENDS)
	{
		ereport(WARNING,
				(errmsg("failed to get connection info, invalid backend number: %d", backend)));
		return NULL;
	}

	return &con_info[child * pool_config->max_pool * MAX_NUM_BACKENDS +
					 connection_pool * MAX_NUM_BACKENDS +
					 backend];
}

/*
 * Return pointer to child which has OS process id pid, j th connection
 * pool and k th backend of connection info on shmem.
 */
ConnectionInfo *
pool_coninfo_pid(int pid, int connection_pool, int backend)
{
	int			child = -1;
	int			i;

	for (i = 0; i < pool_config->num_init_children; i++)
	{
		if (process_info[i].pid == pid)
		{
			child = i;
			break;
		}
	}

	if (child < 0)
		elog(ERROR, "failed to get child pid, invalid child PID:%d", pid);

	if (child < 0 || child >= pool_config->num_init_children)
		elog(ERROR, "failed to get child pid, invalid child no:%d", child);

	if (connection_pool < 0 || connection_pool >= pool_config->max_pool)
		elog(ERROR, "failed to get child pid, invalid connection pool no:%d", connection_pool);

	if (backend < 0 || backend >= MAX_NUM_BACKENDS)
		elog(ERROR, "failed to get child pid, invalid backend no:%d", backend);

	return &con_info[child * pool_config->max_pool * MAX_NUM_BACKENDS +
					 connection_pool * MAX_NUM_BACKENDS +
					 backend];
}

/*
 * locate and return the shared memory ConnectionInfo having the
 * backend connection with the pid
 * if the connection is found the *backend_node_id contains the backend node id
 * of the backend node that has the connection
 */
ConnectionInfo *
pool_coninfo_backend_pid(int backend_pid, int *backend_node_id)
{
	int			child;

	/*
	 * look for the child process that has the backend with the pid
	 */

	ereport(DEBUG1,
			(errmsg("searching for the connection with backend pid:%d", backend_pid)));

	for (child = 0; child < pool_config->num_init_children; child++)
	{
		int			pool;

		if (process_info[child].pid)
		{
			ProcessInfo *pi = pool_get_process_info(process_info[child].pid);

			for (pool = 0; pool < pool_config->max_pool; pool++)
			{
				int			backend_id;

				for (backend_id = 0; backend_id < NUM_BACKENDS; backend_id++)
				{
					int			poolBE = pool * MAX_NUM_BACKENDS + backend_id;

					if (ntohl(pi->connection_info[poolBE].pid) == backend_pid)
					{
						ereport(DEBUG1,
								(errmsg("found the connection with backend pid:%d on backend node %d", backend_pid, backend_id)));
						*backend_node_id = backend_id;
						return &pi->connection_info[poolBE];
					}
				}
			}
		}

	}
	return NULL;
}

/*
 * sets the flag to mark that the connection will be terminated by the
 * backend and it should not be considered as a backend node failure.
 * This flag is used to handle pg_terminate_backend()
 */
void
pool_set_connection_will_be_terminated(ConnectionInfo * connInfo)
{
	connInfo->swallow_termination = 1;
}

void
pool_unset_connection_will_be_terminated(ConnectionInfo * connInfo)
{
	connInfo->swallow_termination = 0;
}

/*
 * Set frontend connected flag
 */
void
pool_coninfo_set_frontend_connected(int proc_id, int pool_index)
{
	ConnectionInfo *con;
	int			i;

	for (i = 0; i < NUM_BACKENDS; i++)
	{
		if (!VALID_BACKEND(i))
			continue;

		con = pool_coninfo(proc_id, pool_index, i);

		if (con == NULL)
		{
			elog(WARNING, "failed to get connection info while marking the frontend is connected for pool");
			return;
		}
		con->connected = true;
		con->client_connection_time = time(NULL);
	}
}

/*
 * Unset frontend connected flag
 */
void
pool_coninfo_unset_frontend_connected(int proc_id, int pool_index)
{
	ConnectionInfo *con;
	int			i;

	for (i = 0; i < NUM_BACKENDS; i++)
	{
		if (!VALID_BACKEND(i))
			continue;

		con = pool_coninfo(proc_id, pool_index, i);

		if (con == NULL)
		{
			elog(WARNING, "failed to get connection info while marking the frontend is not connected for pool");
			return;
		}
		con->connected = false;
		con->client_disconnection_time = time(NULL);
	}
}

/*
 * Set an alarm clock and a signal handler.
 * For pool_alarm_undo(), the alarm second and the old handler
 * are saved, and the remaining time is calculated.
 */
void
pool_alarm(pool_sighandler_t handler, unsigned int second)
{
	POOL_PROCESS_CONTEXT *p = pool_get_process_context();
	time_t		now = time(NULL);

	alarm(second);
	p->last_alarm_handler = pool_signal(SIGALRM, handler);

	if (p->last_alarm_second)
	{
		p->undo_alarm_second = p->last_alarm_second - (now - p->last_alarm_time);
		if (p->undo_alarm_second <= 0)
			p->undo_alarm_second = 1;
	}

	p->last_alarm_time = now;
	p->last_alarm_second = second;
}

/*
 * Undo the alarm signal handler using the remaining time.
 */
void
pool_undo_alarm(void)
{
	POOL_PROCESS_CONTEXT *p = pool_get_process_context();

	if (p->undo_alarm_second)
	{
		alarm(p->undo_alarm_second);
		pool_signal(SIGALRM, p->last_alarm_handler);
		p->undo_alarm_second = 0;
	}
	else
	{
		alarm(0);
		pool_signal(SIGALRM, SIG_IGN);
	}
}
