/* -*-pgsql-c-*- */
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
 * child.c: child process main
 *
 */
#include "config.h"

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/un.h>
#include <arpa/inet.h>
#include <netdb.h>
#ifdef HAVE_NETINET_TCP_H
#include <netinet/tcp.h>
#endif
#ifdef HAVE_SYS_SELECT_H
#include <sys/select.h>
#endif

#include <signal.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/time.h>

#include "pool.h"
#include "pool_config.h"
#include "pool_config_variables.h"
#include "utils/palloc.h"
#include "utils/memutils.h"
#include "utils/pool_ssl.h"
#include "utils/pool_ipc.h"
#include "utils/pool_relcache.h"
#include "utils/pool_ip.h"
#include "utils/pool_stream.h"
#include "utils/elog.h"
#include "utils/ps_status.h"
#include "utils/timestamp.h"

#include "context/pool_process_context.h"
#include "context/pool_session_context.h"
#include "protocol/pool_connection_pool.h"
#include "protocol/pool_process_query.h"
#include "protocol/pool_pg_utils.h"
#include "auth/pool_auth.h"
#include "auth/md5.h"
#include "auth/pool_passwd.h"
#include "auth/pool_hba.h"

static StartupPacket *read_startup_packet(POOL_CONNECTION * cp);
static POOL_CONNECTION_POOL * connect_backend(StartupPacket *sp, POOL_CONNECTION * frontend);
static RETSIGTYPE die(int sig);
static RETSIGTYPE close_idle_connection(int sig);
static RETSIGTYPE wakeup_handler(int sig);
static RETSIGTYPE reload_config_handler(int sig);
static RETSIGTYPE authentication_timeout(int sig);
static void send_params(POOL_CONNECTION * frontend, POOL_CONNECTION_POOL * backend);
static int	connection_count_up(void);
static void connection_count_down(void);
static bool connect_using_existing_connection(POOL_CONNECTION * frontend,
								  POOL_CONNECTION_POOL * backend,
								  StartupPacket *sp);
static void check_restart_request(void);
static void check_exit_request(void);
static void enable_authentication_timeout(void);
static void disable_authentication_timeout(void);
static int	wait_for_new_connections(int *fds, SockAddr *saddr);
static void check_config_reload(void);
static void get_backends_status(unsigned int *valid_backends, unsigned int *down_backends);
static void validate_backend_connectivity(int front_end_fd);
static POOL_CONNECTION * get_connection(int front_end_fd, SockAddr *saddr);
static POOL_CONNECTION_POOL * get_backend_connection(POOL_CONNECTION * frontend);
static StartupPacket *StartupPacketCopy(StartupPacket *sp);
static void log_disconnections(char *database, char *username);
static void print_process_status(char *remote_host, char *remote_port);
static bool backend_cleanup(POOL_CONNECTION * volatile *frontend, POOL_CONNECTION_POOL * volatile backend, bool frontend_invalid);

static void child_will_go_down(int code, Datum arg);
static int opt_sort(const void *a, const void *b);

static bool unix_fds_not_isset(int* fds, int num_unix_fds, fd_set* opt);

/*
 * Non 0 means SIGTERM (smart shutdown) or SIGINT (fast shutdown) has arrived
 */
volatile sig_atomic_t exit_request = 0;
static volatile sig_atomic_t alarm_enabled = false;

/*
 * Ignore SIGUSR1 if requested. Used when DROP DATABASE is requested.
 */
volatile sig_atomic_t ignore_sigusr1 = 0;

/*
 * si modules use SIGUSR2
 */
volatile sig_atomic_t sigusr2_received = 0;

static int	idle;				/* non 0 means this child is in idle state */
static int	accepted = 0;

fd_set		readmask;
int			nsocks;
static int	child_inet_fd = 0;
static int	child_unix_fd = 0;

extern int	myargc;
extern char **myargv;

volatile sig_atomic_t got_sighup = 0;

char		remote_host[NI_MAXHOST];	/* client host */
char		remote_port[NI_MAXSERV];	/* client port */
POOL_CONNECTION *volatile child_frontend = NULL;

struct timeval startTime;

#ifdef DEBUG
bool		stop_now = false;
#endif

/*
* child main loop
*/
void
do_child(int *fds)
{
	sigjmp_buf	local_sigjmp_buf;
	POOL_CONNECTION_POOL *volatile backend = NULL;
	struct timeval now;
	struct timezone tz;

	/* counter for child_max_connections.  "volatile" declaration is necessary
	 * so that this is counted up even if long jump is issued due to
	 * ereport(ERROR).
	 */
	volatile	int			connections_count = 0;

	char		psbuf[NI_MAXHOST + 128];

	ereport(DEBUG2,
			(errmsg("I am Pgpool Child process with pid: %d", getpid())));

	/* Identify myself via ps */
	init_ps_display("", "", "", "");

	/* set up signal handlers */
	signal(SIGALRM, SIG_DFL);
	signal(SIGCHLD, SIG_DFL);
	signal(SIGTERM, die);
	signal(SIGINT, die);
	signal(SIGQUIT, die);
	signal(SIGHUP, reload_config_handler);
	signal(SIGUSR1, close_idle_connection);
	signal(SIGUSR2, wakeup_handler);
	signal(SIGPIPE, SIG_IGN);

	on_system_exit(child_will_go_down, (Datum) NULL);

	int		   *walk;
#ifdef NONE_BLOCK
	/* set listen fds to none-blocking */
	for (walk = fds; *walk != -1; walk++)
		socket_set_nonblock(*walk);
#endif
	for (walk = fds; *walk != -1; walk++)
	{
		if (*walk > nsocks)
			nsocks = *walk;
	}
	nsocks++;
	FD_ZERO(&readmask);
	for (walk = fds; *walk != -1; walk++)
		FD_SET(*walk, &readmask);

	/* Create per loop iteration memory context */
	ProcessLoopContext = AllocSetContextCreate(TopMemoryContext,
											   "pgpool_child_main_loop",
											   ALLOCSET_DEFAULT_MINSIZE,
											   ALLOCSET_DEFAULT_INITSIZE,
											   ALLOCSET_DEFAULT_MAXSIZE);

	MemoryContextSwitchTo(TopMemoryContext);

	/* Initialize my backend status */
	pool_initialize_private_backend_status();

	/* Initialize per process context */
	pool_init_process_context();

	/* initialize random seed */
	gettimeofday(&now, &tz);

#if defined(sun) || defined(__sun)
	srand((unsigned int) now.tv_usec);
#else
	srandom((unsigned int) now.tv_usec);
#endif

	/* initialize connection pool */
	if (pool_init_cp())
	{
		child_exit(POOL_EXIT_AND_RESTART);
	}

	/*
	 * Open pool_passwd in child process.  This is necessary to avoid the file
	 * descriptor race condition reported in [pgpool-general: 1141].
	 */
	if (strcmp("", pool_config->pool_passwd))
	{
		pool_reopen_passwd_file();
	}


	if (sigsetjmp(local_sigjmp_buf, 1) != 0)
	{
		POOL_PROCESS_CONTEXT *session;
		bool		frontend_invalid = getfrontendinvalid();

		disable_authentication_timeout();
		/* Since not using PG_TRY, must reset error stack by hand */
		error_context_stack = NULL;

		/*
		 * Do not emit an error when EOF was encountered on frontend
		 * connection before the session was initialized. This is the normal
		 * behavior of psql to close and reconnect the connection when some
		 * authentication method is used
		 */
		if (pool_get_session_context(true) ||
			!child_frontend ||
			child_frontend->socket_state != POOL_SOCKET_EOF)
			EmitErrorReport();

		/*
		 * process the cleanup in ProcessLoopContext which will get reset
		 * during the next loop iteration
		 */
		MemoryContextSwitchTo(ProcessLoopContext);

		if (accepted)
		{
			accepted = 0;
			connection_count_down();
			if (pool_config->log_disconnections)
				log_disconnections(child_frontend->database, child_frontend->username);
		}

		backend_cleanup(&child_frontend, backend, frontend_invalid);

		session = pool_get_process_context();

		if (session)
		{
			/* Destroy session context */
			pool_session_context_destroy();

			/* Mark this connection pool is not connected from frontend */
			pool_coninfo_unset_frontend_connected(pool_get_process_context()->proc_id, pool_pool_index());

			/* increment queries counter if necessary */
			if (pool_config->child_max_connections > 0)
				connections_count++;

			pool_get_my_process_info()->client_connection_count++;

			/* check if maximum connections count for this child reached */
			if ((pool_config->child_max_connections > 0) &&
				(connections_count >= pool_config->child_max_connections))
			{
				ereport(LOG,
						(errmsg("child exiting, %d connections reached", pool_config->child_max_connections)));
				child_exit(POOL_EXIT_AND_RESTART);
			}
		}

		if (child_frontend)
		{
			pool_close(child_frontend);
			child_frontend = NULL;
		}
		update_pooled_connection_count();
		MemoryContextSwitchTo(TopMemoryContext);
		FlushErrorState();
	}

	/* We can now handle ereport(ERROR) */
	PG_exception_stack = &local_sigjmp_buf;

	for (;;)
	{
		StartupPacket *sp;
		int			front_end_fd;
		SockAddr	saddr;
		int			con_count;

		/* reset per iteration memory context */
		MemoryContextSwitchTo(ProcessLoopContext);
		MemoryContextResetAndDeleteChildren(ProcessLoopContext);

		backend = NULL;
		idle = 1;

		/* pgpool stop request already sent? */
		check_stop_request();
		check_restart_request();
		check_exit_request();
		accepted = 0;
		/* Destroy session context for just in case... */
		pool_session_context_destroy();

		front_end_fd = wait_for_new_connections(fds, &saddr);
		pool_get_my_process_info()->wait_for_connect = 0;
		if (front_end_fd == OPERATION_TIMEOUT)
		{
			if (pool_config->child_life_time > 0 && pool_get_my_process_info()->connected)
			{
				ereport(DEBUG1,
						(errmsg("child life %d seconds expired", pool_config->child_life_time)));
				child_exit(POOL_EXIT_AND_RESTART);
			}
			continue;
		}

		if (front_end_fd == RETRY)
			continue;

		set_process_status(CONNECTING);
		/* Reset any exit if idle request even if it's pending */
		pool_get_my_process_info()->exit_if_idle = false;

		con_count = connection_count_up();

		if (pool_config->reserved_connections > 0)
		{
			/*
			 * Check if max connections from clients exceeded.
			 */
			if (con_count > (pool_config->num_init_children - pool_config->reserved_connections))
			{
				POOL_CONNECTION * cp;
				cp = pool_open(front_end_fd, false);
				if (cp == NULL)
				{
					connection_count_down();
					continue;
				}
				connection_count_down();
				pool_send_fatal_message(cp, 3, "53300",
										"Sorry, too many clients already",
										"",
										"",
										__FILE__, __LINE__);
				ereport(ERROR,
						(errcode(ERRCODE_TOO_MANY_CONNECTIONS),
						 errmsg("Sorry, too many clients already")));
				pool_close(cp);
				continue;
			}
		}

		accepted = 1;
		gettimeofday(&startTime, NULL);

		check_config_reload();
		validate_backend_connectivity(front_end_fd);
		child_frontend = get_connection(front_end_fd, &saddr);

		/* set frontend fd to blocking */
		socket_unset_nonblock(child_frontend->fd);

		/* reset busy flag */
		idle = 0;

		/* check backend timer is expired */
		if (backend_timer_expired)
		{
			pool_backend_timer();
			backend_timer_expired = 0;
		}

		backend = get_backend_connection(child_frontend);
		if (!backend)
		{
			if (pool_config->log_disconnections)
				log_disconnections(child_frontend->database, child_frontend->username);
			pool_close(child_frontend);
			child_frontend = NULL;
			continue;
		}
		pool_get_my_process_info()->connected = 1;

		/*
		 * show ps status
		 */
		sp = MAIN_CONNECTION(backend)->sp;
		snprintf(psbuf, sizeof(psbuf), "%s %s %s idle",
				 sp->user, sp->database, remote_ps_data);
		set_ps_display(psbuf, false);
		set_process_status(IDLE);

		/*
		 * Initialize per session context
		 */
		pool_init_session_context(child_frontend, backend);

		/*
		 * Set protocol versions
		 */
		pool_set_major_version(sp->major);
		pool_set_minor_version(sp->minor);

		/*
		 * Mark this connection pool is connected from frontend
		 */
		pool_coninfo_set_frontend_connected(pool_get_process_context()->proc_id, pool_pool_index());

		/* create memory context for query processing */
		QueryContext = AllocSetContextCreate(ProcessLoopContext,
											 "child_query_process",
											 ALLOCSET_DEFAULT_MINSIZE,
											 ALLOCSET_DEFAULT_INITSIZE,
											 ALLOCSET_DEFAULT_MAXSIZE);
		/* query process loop */
		for (;;)
		{
			POOL_STATUS status;

			/* Reset the query process memory context */
			MemoryContextSwitchTo(QueryContext);
			MemoryContextResetAndDeleteChildren(QueryContext);

			status = pool_process_query(child_frontend, backend, 0);
			if (status != POOL_CONTINUE)
			{
				backend_cleanup(&child_frontend, backend, false);
				break;
			}
		}

		/* Destroy session context */
		pool_session_context_destroy();

		/* Mark this connection pool is not connected from frontend */
		pool_coninfo_unset_frontend_connected(pool_get_process_context()->proc_id, pool_pool_index());
		update_pooled_connection_count();
		accepted = 0;
		connection_count_down();
		if (pool_config->log_disconnections)
			log_disconnections(sp->database, sp->user);

		/* increment queries counter if necessary */
		if (pool_config->child_max_connections > 0)
			connections_count++;

		pool_get_my_process_info()->client_connection_count++;

		/* check if maximum connections count for this child reached */
		if ((pool_config->child_max_connections > 0) &&
			(connections_count >= pool_config->child_max_connections))
		{
			ereport(LOG,
					(errmsg("child exiting, %d connections reached", pool_config->child_max_connections)));
			child_exit(POOL_EXIT_AND_RESTART);
		}
	}
	child_exit(POOL_EXIT_NO_RESTART);
}



/* -------------------------------------------------------------------
 * private functions
 * -------------------------------------------------------------------
 */

/*
 * function cleans up the backend connection, when process query returns with an error
 * return true if backend connection is cached
 */
static bool
backend_cleanup(POOL_CONNECTION * volatile *frontend, POOL_CONNECTION_POOL * volatile backend, bool frontend_invalid)
{
	StartupPacket *sp;
	bool		cache_connection = false;

	if (backend == NULL)
		return false;

	sp = MAIN_CONNECTION(backend)->sp;

	/*
	 * cach connection if connection cache configuration parameter is enabled
	 * and frontend connection is not invalid
	 */
	if (sp && pool_config->connection_cache != 0 && frontend_invalid == false)
	{
		if (*frontend)
		{
			MemoryContext oldContext = CurrentMemoryContext;

			PG_TRY();
			{
				if (pool_process_query(*frontend, backend, 1) == POOL_CONTINUE)
				{
					pool_connection_pool_timer(backend);
					cache_connection = true;
				}
			}
			PG_CATCH();
			{
				/* ignore the error message */
				MemoryContextSwitchTo(oldContext);
				FlushErrorState();
			}
			PG_END_TRY();
		}
	}

	if (cache_connection)
	{
		/*
		 * For those special databases, and when frontend client exits
		 * abnormally, we don't cache connection to backend.
		 */
		if ((sp &&
			 (!strcmp(sp->database, "template0") ||
			  !strcmp(sp->database, "template1") ||
			  !strcmp(sp->database, "postgres") ||
			  !strcmp(sp->database, "regression"))) ||
			(*frontend != NULL &&
			 ((*frontend)->socket_state == POOL_SOCKET_EOF ||
			  (*frontend)->socket_state == POOL_SOCKET_ERROR)))
			cache_connection = false;
	}

	/*
	 * Close frontend connection
	 */
	reset_connection();

	if (*frontend != NULL)
	{
		pool_close(*frontend);
		*frontend = NULL;
	}

	if (cache_connection == false)
	{
		pool_send_frontend_exits(backend);
		if (sp)
			pool_discard_cp(sp->user, sp->database, sp->major);
	}

	/* reset the config parameters */
	reset_all_variables(NULL, NULL);
	return cache_connection;
}

/*
 * Read startup packet
 *
 * Read the startup packet and parse the contents.
*/
static StartupPacket *
read_startup_packet(POOL_CONNECTION * cp)
{
	StartupPacket *sp;
	StartupPacket_v2 *sp2;
	int			protov;
	int			len;
	char	   *p;
	char **guc_options;
	int opt_num = 0;
	char *sp_sort;
	char *tmpopt;
	int i;


	sp = (StartupPacket *) palloc0(sizeof(*sp));
	enable_authentication_timeout();

	/* read startup packet length */
	pool_read_with_error(cp, &len, sizeof(len),
						 "startup packet length");

	len = ntohl(len);
	len -= sizeof(len);

	if (len <= 0 || len >= MAX_STARTUP_PACKET_LENGTH)
		ereport(ERROR,
				(errmsg("failed while reading startup packet"),
				 errdetail("incorrect packet length (%d)", len)));

	sp->startup_packet = palloc0(len);

	/* read startup packet */
	pool_read_with_error(cp, sp->startup_packet, len,
						 "startup packet");

	sp->len = len;
	memcpy(&protov, sp->startup_packet, sizeof(protov));
	sp->major = ntohl(protov) >> 16;
	sp->minor = ntohl(protov) & 0x0000ffff;
	cp->protoVersion = sp->major;

	switch (sp->major)
	{
		case PROTO_MAJOR_V2:	/* V2 */
			sp2 = (StartupPacket_v2 *) (sp->startup_packet);

			sp->database = palloc0(SM_DATABASE + 1);
			strncpy(sp->database, sp2->database, SM_DATABASE);

			sp->user = palloc0(SM_USER + 1);
			strncpy(sp->user, sp2->user, SM_USER);

			break;

		case PROTO_MAJOR_V3:	/* V3 */
			/* copy startup_packet */
			sp_sort = palloc0(len);
			memcpy(sp_sort,sp->startup_packet,len);

			p = sp_sort;
			p += sizeof(int);   /* skip protocol version info */
			/* count the number of options */
			while (*p)
			{
			p += (strlen(p) + 1); /* skip option name */
				p += (strlen(p) + 1); /* skip option value */
				opt_num ++;
			}
			guc_options = (char **)palloc0(opt_num * sizeof(char *));
			/* get guc_option name list */
			p = sp_sort + sizeof(int);
			for (i = 0; i < opt_num; i++)
			{
				guc_options[i] = p;
				p += (strlen(p) + 1); /* skip option name */
				p += (strlen(p) + 1); /* skip option value */
			}
			/* sort option name using quick sort */
			qsort( (void *)guc_options, opt_num, sizeof(char *), opt_sort );

			p = sp->startup_packet + sizeof(int);   /* skip protocol version info */
			for (i = 0; i < opt_num; i++)
			{
				tmpopt = guc_options[i];
				memcpy(p, tmpopt ,strlen(tmpopt) + 1); /* memcpy option name */
				p += (strlen(tmpopt) + 1);
				tmpopt += (strlen(tmpopt) + 1);
				memcpy(p, tmpopt ,strlen(tmpopt) + 1); /* memcpy option value */
				p += (strlen(tmpopt) + 1);
			}

			pfree(guc_options);
			pfree(sp_sort);

			p = sp->startup_packet;

			p += sizeof(int);	/* skip protocol version info */

			while (*p)
			{
				if (!strcmp("user", p))
				{
					p += (strlen(p) + 1);
					sp->user = pstrdup(p);
				}
				else if (!strcmp("database", p))
				{
					p += (strlen(p) + 1);
					sp->database = pstrdup(p);
				}

				/*
				 * From 9.0, the start up packet may include application name.
				 * After receiving such that packet, backend sends parameter
				 * status of application_name. Upon reusing connection to
				 * backend, we need to emulate this behavior of backend. So we
				 * remember this and send parameter status packet to frontend
				 * instead of backend in connect_using_existing_connection().
				 */
				else if (!strcmp("application_name", p))
				{
					p += (strlen(p) + 1);
					sp->application_name = p;
					ereport(DEBUG1,
							(errmsg("reading startup packet"),
							 errdetail("application_name: %s", p)));
				}
				else
				{
					ereport(DEBUG1,
							(errmsg("reading startup packet"),
							 errdetail("guc name: %s value: %s", p, p+strlen(p)+1)));
					p += (strlen(p) + 1);

				}

				p += (strlen(p) + 1);
			}
			break;

		case 1234:				/* cancel or SSL request */
			/* set dummy database, user info */
			sp->database = palloc0(1);
			sp->user = palloc(1);
			break;

		default:
			ereport(ERROR,
					(errmsg("failed while reading startup packet"),
					 errdetail("invalid major no: %d in startup packet", sp->major)));
	}

	/* Check a user name was given. */
	if (sp->major != 1234 &&
		(sp->user == NULL || sp->user[0] == '\0'))
	{
		pool_send_fatal_message(cp, sp->major, "28000",
								"no PostgreSQL user name specified in startup packet",
								"",
								"",
								__FILE__, __LINE__);
		ereport(FATAL,
				(errmsg("failed while reading startup packet"),
				 errdetail("no PostgreSQL user name specified in startup packet")));
	}

	/* The database defaults to their user name. */
	if (sp->database == NULL || sp->database[0] == '\0')
	{
		sp->database = pstrdup(sp->user);
	}

	ereport(DEBUG1,
			(errmsg("reading startup packet"),
			 errdetail("Protocol Major: %d Minor: %d database: %s user: %s",
					   sp->major, sp->minor, sp->database, sp->user)));

	disable_authentication_timeout();

	return sp;
}


/*
 * Reuse existing connection
 */
static bool
connect_using_existing_connection(POOL_CONNECTION * frontend,
								  POOL_CONNECTION_POOL * backend,
								  StartupPacket *sp)
{
	int			i,
				freed = 0;
	StartupPacket *topmem_sp = NULL;
	MemoryContext oldContext;
	MemoryContext frontend_auth_cxt;

	/*
	 * Save startup packet info
	 */
	for (i = 0; i < NUM_BACKENDS; i++)
	{
		if (VALID_BACKEND(i))
		{
			if (!freed)
			{
				oldContext = MemoryContextSwitchTo(TopMemoryContext);

				topmem_sp = StartupPacketCopy(sp);
				MemoryContextSwitchTo(oldContext);

				pool_free_startup_packet(backend->slots[i]->sp);
				backend->slots[i]->sp = NULL;

				freed = 1;
			}
			backend->slots[i]->sp = topmem_sp;
		}
	}

	/* Reuse existing connection to backend */
	frontend_auth_cxt = AllocSetContextCreate(CurrentMemoryContext,
															"frontend_auth",
															ALLOCSET_DEFAULT_SIZES);
	oldContext = MemoryContextSwitchTo(frontend_auth_cxt);

	pool_do_reauth(frontend, backend);

	MemoryContextSwitchTo(oldContext);
	MemoryContextDelete(frontend_auth_cxt);

	if (MAJOR(backend) == 3)
	{
		char		command_buf[1024];

		/*
		 * If we have received application_name in the start up packet, we
		 * send SET command to backend. Also we add or replace existing
		 * application_name data.
		 */
		if (sp->application_name)
		{
			snprintf(command_buf, sizeof(command_buf), "SET application_name TO '%s'", sp->application_name);

			for (i = 0; i < NUM_BACKENDS; i++)
			{
				if (VALID_BACKEND(i))
					if (do_command(frontend, CONNECTION(backend, i),
								   command_buf, MAJOR(backend),
								   MAIN_CONNECTION(backend)->pid,
								   MAIN_CONNECTION(backend)->key, 0) != POOL_CONTINUE)
					{
						ereport(ERROR,
								(errmsg("unable to process command for backend connection"),
								 errdetail("do_command returned DEADLOCK status")));
					}
			}

			pool_add_param(&MAIN(backend)->params, "application_name", sp->application_name);
			set_application_name_with_string(sp->application_name);
		}

		send_params(frontend, backend);
	}

	/* Send ReadyForQuery to frontend */
	pool_write(frontend, "Z", 1);

	if (MAJOR(backend) == 3)
	{
		int			len;
		char		tstate;

		len = htonl(5);
		pool_write(frontend, &len, sizeof(len));
		tstate = TSTATE(backend, MAIN_NODE_ID);
		pool_write(frontend, &tstate, 1);
	}

	pool_flush(frontend);

	return true;
}

/*
 * process cancel request
 */
void
cancel_request(CancelPacket * sp)
{
	int			len;
	int			fd;
	POOL_CONNECTION *con;
	int			i,
				j,
				k;
	ConnectionInfo *c = NULL;
	CancelPacket cp;
	bool		found = false;

	if (pool_config->log_client_messages)
		ereport(LOG,
				(errmsg("Cancel message from frontend."),
				 errdetail("process id: %d", ntohl(sp->pid))));
	ereport(DEBUG1,
			(errmsg("Cancel request received")));

	/* look for cancel key from shmem info */
	for (i = 0; i < pool_config->num_init_children; i++)
	{
		for (j = 0; j < pool_config->max_pool; j++)
		{
			for (k = 0; k < NUM_BACKENDS; k++)
			{
				c = pool_coninfo(i, j, k);
				ereport(DEBUG2,
						(errmsg("processing cancel request"),
						 errdetail("connection info: address:%p database:%s user:%s pid:%d key:%d i:%d",
								   c, c->database, c->user, ntohl(c->pid), ntohl(c->key), i)));
				if (c->pid == sp->pid && c->key == sp->key)
				{
					ereport(DEBUG1,
							(errmsg("processing cancel request"),
							 errdetail("found pid:%d key:%d i:%d", ntohl(c->pid), ntohl(c->key), i)));

					c = pool_coninfo(i, j, 0);
					found = true;
					goto found;
				}
			}
		}
	}

found:
	if (!found)
	{
		ereport(LOG,
				(errmsg("invalid cancel key: pid:%d key:%d", ntohl(sp->pid), ntohl(sp->key))));
		return;					/* invalid key */
	}

	for (i = 0; i < NUM_BACKENDS; i++, c++)
	{
		if (!VALID_BACKEND(i))
			continue;

		if (*(BACKEND_INFO(i).backend_hostname) == '/')
			fd = connect_unix_domain_socket(i, TRUE);
		else
			fd = connect_inet_domain_socket(i, TRUE);

		if (fd < 0)
		{
			ereport(LOG,
					(errmsg("Could not create socket for sending cancel request for backend %d", i)));
			return;
		}

		con = pool_open(fd, true);
		if (con == NULL)
			return;

		pool_set_db_node_id(con, i);

		len = htonl(sizeof(len) + sizeof(CancelPacket));
		pool_write(con, &len, sizeof(len));

		cp.protoVersion = sp->protoVersion;
		cp.pid = c->pid;
		cp.key = c->key;

		ereport(LOG,
				(errmsg("forwarding cancel request to backend"),
				 errdetail("canceling backend pid:%d key: %d", ntohl(cp.pid), ntohl(cp.key))));

		if (pool_write_and_flush_noerror(con, &cp, sizeof(CancelPacket)) < 0)
			ereport(WARNING,
					(errmsg("failed to send cancel request to backend %d", i)));

		pool_close(con);

		/*
		 * this is needed to ensure that the next DB node executes the query
		 * supposed to be canceled.
		 */
		sleep(1);
	}
}

static StartupPacket *
StartupPacketCopy(StartupPacket *sp)
{
	StartupPacket *new_sp;

	/* verify the length first */
	if (sp->len <= 0 || sp->len >= MAX_STARTUP_PACKET_LENGTH)
		ereport(ERROR,
				(errmsg("incorrect packet length (%d)", sp->len)));

	new_sp = (StartupPacket *) palloc0(sizeof(*sp));
	new_sp->startup_packet = palloc0(sp->len);
	memcpy(new_sp->startup_packet, sp->startup_packet, sp->len);
	new_sp->len = sp->len;

	new_sp->major = sp->major;
	new_sp->minor = sp->minor;

	new_sp->database = pstrdup(sp->database);
	new_sp->user = pstrdup(sp->user);

	if (new_sp->major == PROTO_MAJOR_V3 && sp->application_name)
	{
		/* adjust the application name pointer in new packet */
		new_sp->application_name = new_sp->startup_packet + (sp->application_name - sp->startup_packet);
	}
	return new_sp;
}

static POOL_CONNECTION_POOL * connect_backend(StartupPacket *sp, POOL_CONNECTION * frontend)
{
	POOL_CONNECTION_POOL *backend;
	StartupPacket *volatile topmem_sp = NULL;
	volatile bool	topmem_sp_set = false;
	int			i;

	/* connect to the backend */
	backend = pool_create_cp();
	if (backend == NULL)
	{
		pool_send_error_message(frontend, sp->major, "XX000", "all backend nodes are down, pgpool requires at least one valid node", "",
								"repair the backend nodes and restart pgpool", __FILE__, __LINE__);
		ereport(ERROR,
				(errmsg("unable to connect to backend"),
				 errdetail("all backend nodes are down, pgpool requires at least one valid node"),
				 errhint("repair the backend nodes and restart pgpool")));
	}

	PG_TRY();
	{
		MemoryContext frontend_auth_cxt;
		MemoryContext oldContext = MemoryContextSwitchTo(TopMemoryContext);

		topmem_sp = StartupPacketCopy(sp);
		MemoryContextSwitchTo(oldContext);

		for (i = 0; i < NUM_BACKENDS; i++)
		{
			if (VALID_BACKEND(i))
			{

				/* set DB node id */
				pool_set_db_node_id(CONNECTION(backend, i), i);

				/* mark this is a backend connection */
				CONNECTION(backend, i)->isbackend = 1;

				pool_ssl_negotiate_clientserver(CONNECTION(backend, i));

				/*
				 * save startup packet info
				 */
				CONNECTION_SLOT(backend, i)->sp = topmem_sp;
				topmem_sp_set = true;

				/* send startup packet */
				send_startup_packet(CONNECTION_SLOT(backend, i));
			}
		}

		/*
		 * do authentication stuff
		 */
		frontend_auth_cxt = AllocSetContextCreate(CurrentMemoryContext,
																"frontend_auth",
																ALLOCSET_DEFAULT_SIZES);
		oldContext = MemoryContextSwitchTo(frontend_auth_cxt);

		pool_do_auth(frontend, backend);

		MemoryContextSwitchTo(oldContext);
		MemoryContextDelete(frontend_auth_cxt);

	}
	PG_CATCH();
	{
		pool_discard_cp(sp->user, sp->database, sp->major);
		topmem_sp = NULL;
		PG_RE_THROW();
	}
	PG_END_TRY();

	/* At this point, we need to free previously allocated memory for the
	 * startup packet if no backend is up.
	 */
	if (!topmem_sp_set && topmem_sp != NULL)
		pfree(topmem_sp);

	return backend;
}

/*
 * signal handler for SIGTERM, SIGINT and SIGQUIT
 */
static RETSIGTYPE die(int sig)
{
	int			save_errno;

	save_errno = errno;

	POOL_SETMASK(&BlockSig);

#ifdef NOT_USED
	ereport(LOG,
			(errmsg("child process received shutdown request signal %d", sig)));
#endif

	exit_request = sig;

	switch (sig)
	{
		case SIGTERM:			/* smart shutdown */
			/* Refuse further requests by closing listen socket */
			if (child_inet_fd)
			{
#ifdef NOT_USED
				ereport(LOG,
						(errmsg("closing listen socket")));
#endif
				close(child_inet_fd);
			}
			close(child_unix_fd);

			if (idle == 0)
			{
#ifdef NOT_USED
				ereport(DEBUG1,
						(errmsg("smart shutdown request received, but child is not in idle state")));
#endif
			}
			break;

		case SIGINT:			/* fast shutdown */
		case SIGQUIT:			/* immediate shutdown */
			POOL_SETMASK(&UnBlockSig);
			child_exit(POOL_EXIT_NO_RESTART);
			break;
		default:
#ifdef NOT_USED
			ereport(LOG,
					(errmsg("child process received unknown signal: %d", sig),
					 errdetail("ignoring...")));
#endif
			break;
	}

	POOL_SETMASK(&UnBlockSig);

	errno = save_errno;
}

/*
 * signal handler for SIGUSR1
 * close all idle connections
 */
static RETSIGTYPE close_idle_connection(int sig)
{
	int			i,
				j;
	POOL_CONNECTION_POOL *p = pool_connection_pool;
	ConnectionInfo *info;
	int			save_errno = errno;

	/*
	 * DROP DATABASE is ongoing.
	 */
	if (ignore_sigusr1)
		return;

#ifdef NOT_USED
	ereport(DEBUG1,
			(errmsg("close connection request received")));
#endif

	for (j = 0; j < pool_config->max_pool; j++, p++)
	{
		if (!MAIN_CONNECTION(p))
			continue;
		if (!MAIN_CONNECTION(p)->sp)
			continue;
		if (MAIN_CONNECTION(p)->sp->user == NULL)
			continue;

		if (MAIN_CONNECTION(p)->closetime > 0)	/* idle connection? */
		{
#ifdef NOT_USED
			ereport(DEBUG1,
					(errmsg("closing idle connection"),
					 errdetail("user: %s database: %s", MAIN_CONNECTION(p)->sp->user, MAIN_CONNECTION(p)->sp->database)));
#endif

			pool_send_frontend_exits(p);

			for (i = 0; i < NUM_BACKENDS; i++)
			{
				if (!VALID_BACKEND(i))
					continue;

				if (i == 0)
				{
					/*
					 * only first backend allocated the memory for the start
					 * up packet
					 */
					pool_free_startup_packet(CONNECTION_SLOT(p, i)->sp);
					CONNECTION_SLOT(p, i)->sp = NULL;

				}
				pool_close(CONNECTION(p, i));
			}
			info = p->info;
			memset(p, 0, sizeof(POOL_CONNECTION_POOL));
			p->info = info;
			memset(p->info, 0, sizeof(ConnectionInfo));
		}
	}

	errno = save_errno;
}

/*
 * signal handler for SIGALRM
 *
 */
static RETSIGTYPE authentication_timeout(int sig)
{
	alarm_enabled = false;
	ereport(LOG,
			(errmsg("authentication timeout")));

	child_exit(POOL_EXIT_AND_RESTART);
}

static void
enable_authentication_timeout(void)
{
	if (pool_config->authentication_timeout <= 0)
		return;
	pool_alarm(authentication_timeout, pool_config->authentication_timeout);
	alarm_enabled = true;
}

static void
disable_authentication_timeout(void)
{
	if (alarm_enabled)
	{
		pool_undo_alarm();
		alarm_enabled = false;
	}
}


static void
send_params(POOL_CONNECTION * frontend, POOL_CONNECTION_POOL * backend)
{
	int			index;
	char	   *name,
			   *value;
	int			len,
				sendlen;

	index = 0;
	while (pool_get_param(&MAIN(backend)->params, index++, &name, &value) == 0)
	{
		pool_write(frontend, "S", 1);
		len = sizeof(sendlen) + strlen(name) + 1 + strlen(value) + 1;
		sendlen = htonl(len);
		pool_write(frontend, &sendlen, sizeof(sendlen));
		pool_write(frontend, name, strlen(name) + 1);
		pool_write(frontend, value, strlen(value) + 1);
	}

	if (pool_flush(frontend))
	{
		ereport(ERROR,
				(errmsg("unable to send params to frontend"),
				 errdetail("pool_flush failed")));
	}
}

/*
 * Do house keeping works when pgpool child process exits
 */
static void
child_will_go_down(int code, Datum arg)
{
	if (processType != PT_CHILD)
	{
		/* should never happen */
		ereport(WARNING,
				(errmsg("child_exit: called from invalid process. ignored.")));
		return;
	}

	/* count down global connection counter */
	if (accepted)
	{
		connection_count_down();
		if (pool_config->log_disconnections)
		{
			if (child_frontend)
				log_disconnections(child_frontend->database, child_frontend->username);
			else
				log_disconnections("","");
		}
	}

	if ((pool_config->memory_cache_enabled || pool_config->enable_shared_relcache)
		&& !pool_is_shmem_cache())
	{
		memcached_disconnect();
	}

	/* let backend know now we are exiting */
	if (pool_connection_pool)
		close_all_backend_connections();
}
void
child_exit(int code)
{
	if (processType != PT_CHILD)
	{
		/* should never happen */

		/*
		 * Remove call to ereport because child_exit() is called inside a
		 * signal handler.
		 */
#ifdef NOT_USED
		ereport(WARNING,
				(errmsg("child_exit: called from invalid process. ignored.")));
#endif
		return;
	}
	exit(code);
}



/*
 * Count up connection counter (from frontend to pgpool) in shared memory and
 * returns current counter value.  Please note that the returned value may not
 * be up to date since locking has been already released.
 */
static int
connection_count_up(void)
{
	pool_sigset_t oldmask;

	POOL_SETMASK2(&BlockSig, &oldmask);
	pool_semaphore_lock(CONN_COUNTER_SEM);
	Req_info->conn_counter++;
	elog(DEBUG5, "connection_count_up: number of connected children: %d", Req_info->conn_counter);
	pool_semaphore_unlock(CONN_COUNTER_SEM);
	POOL_SETMASK(&oldmask);
	return Req_info->conn_counter;
}

/*
 * Count down connection counter (from frontend to pgpool)
 * in shared memory
 */
static void
connection_count_down(void)
{
	pool_sigset_t oldmask;

	POOL_SETMASK2(&BlockSig, &oldmask);
	pool_semaphore_lock(CONN_COUNTER_SEM);

	/*
	 * Make sure that we do not decrement too much.  If failed to read a start
	 * up packet, or receive cancel request etc., connection_count_down() is
	 * called and goes back to the connection accept loop. Problem is, at the
	 * very beginning of the connection accept loop, if we have received a
	 * signal, we call child_exit() which calls connection_count_down() again.
	 */
	if (Req_info->conn_counter > 0)
		Req_info->conn_counter--;
	elog(DEBUG5, "connection_count_down: number of connected children: %d", Req_info->conn_counter);
	pool_semaphore_unlock(CONN_COUNTER_SEM);
	POOL_SETMASK(&oldmask);
}

/*
 * handle SIGUSR2
 * Wakeup all process
 */
static RETSIGTYPE wakeup_handler(int sig)
{
	sigusr2_received = 1;
}




/* SIGHUP handler */
static RETSIGTYPE reload_config_handler(int sig)
{
	got_sighup = 1;
}

/*
 * Exit myself if SIGTERM, SIGINT or SIGQUIT has been sent
 */
void
check_stop_request(void)
{
	/*
	 * If smart shutdown was requested but we are not in idle state, do not
	 * exit
	 */
	if (exit_request == SIGTERM && idle == 0)
		return;

	if (exit_request)
	{
		reset_variables();
		child_exit(POOL_EXIT_NO_RESTART);
	}
}

/*
 * Initialize my backend status and main node id.
 * We copy the backend status to private area so that
 * they are not changed while I am alive.
 */
void
pool_initialize_private_backend_status(void)
{
	int			i;

	ereport(DEBUG1,
			(errmsg("initializing backend status")));

	for (i = 0; i < MAX_NUM_BACKENDS; i++)
	{
		private_backend_status[i] = BACKEND_INFO(i).backend_status;
		/* my_backend_status is referred to by VALID_BACKEND macro. */
		my_backend_status[i] = &private_backend_status[i];
	}

	my_main_node_id = REAL_MAIN_NODE_ID;
}

static void
check_exit_request(void)
{
	/*
	 * Check if exit request is set because of spare children management.
	 */
	if (pool_get_my_process_info()->exit_if_idle)
	{
		ereport(LOG,
				(errmsg("Exit flag set"),
				 errdetail("Exiting myself")));

		pool_get_my_process_info()->exit_if_idle = 0;
		child_exit(POOL_EXIT_NO_RESTART);
	}
}

static void
check_restart_request(void)
{
	/*
	 * Check if restart request is set because of failback event happened.  If
	 * so, exit myself with exit code 1 to be restarted by pgpool parent.
	 */
	if (pool_get_my_process_info()->need_to_restart)
	{
		ereport(LOG,
				(errmsg("failover or failback event detected"),
				 errdetail("restarting myself")));

		pool_get_my_process_info()->need_to_restart = false;
		child_exit(POOL_EXIT_AND_RESTART);
	}
}

/*
 * wait_for_new_connections()
 * functions calls select on sockets and wait for new client
 * to connect, on successful connection returns the socket descriptor
 * and returns -1 if timeout has occurred
 */

static int
wait_for_new_connections(int *fds, SockAddr *saddr)
{
	fd_set		rmask;
	int			numfds;
	int			save_errno;

	int			fd = 0;
	int			afd;
	int		   *walk;
	int			on;

#ifdef ACCEPT_PERFORMANCE
	struct timeval now1,
				now2;
	static long atime;
	static int	cnt;
#endif

	struct timeval *timeout;
	struct timeval timeoutdata;

	for (walk = fds; *walk != -1; walk++)
		socket_set_nonblock(*walk);

	if (SERIALIZE_ACCEPT)
		set_ps_display("wait for accept lock", false);
	else
		set_ps_display("wait for connection request", false);

	set_process_status(WAIT_FOR_CONNECT);

	/*
	 * If child life time is disabled and serialize_accept is on, we serialize
	 * select() and accept() to avoid the "Thundering herd" problem.
	 */
	if (SERIALIZE_ACCEPT)
	{
		if (pool_config->connection_life_time == 0)
		{
			pool_semaphore_lock(ACCEPT_FD_SEM);
		}
		else
		{
			int sts;

			for (;;)
			{
				sts = pool_semaphore_lock_allow_interrupt(ACCEPT_FD_SEM);

				/* Interrupted by alarm */
				if (sts == -2)
				{
					/*
					 * Check if there are expired connection_life_time.
					 */
					if (backend_timer_expired)
					{
						/*
						 * We add 10 seconds to connection_life_time so that there's
						 * enough margin.
						 */
						int	seconds = pool_config->connection_life_time + 10;

						while (seconds-- > 0)
						{
							/* check backend timer is expired */
							if (backend_timer_expired)
							{
								pool_backend_timer();
								backend_timer_expired = 0;
								break;
							}
							sleep(1);
						}
					}
				}
				else	/* success or other error */
					break;
			}
		}

		set_ps_display("wait for connection request", false);
		ereport(DEBUG1,
				(errmsg("LOCKING select()")));
	}

	for (;;)
	{
		/* check backend timer is expired */
		if (backend_timer_expired)
		{
			pool_backend_timer();
			backend_timer_expired = 0;
		}

		/* prepare select */
		memcpy((char *) &rmask, (char *) &readmask, sizeof(fd_set));
		if (pool_config->child_life_time > 0)
		{
			timeoutdata.tv_sec = 1;
			timeoutdata.tv_usec = 0;
			timeout = &timeoutdata;
		}
		else
		{
			timeout = NULL;
		}

		numfds = select(nsocks, &rmask, NULL, NULL, timeout);

		/* not timeout*/
		if (numfds != 0)
			break;

		/* timeout */
		pool_get_my_process_info()->wait_for_connect++;

		if (pool_get_my_process_info()->wait_for_connect > pool_config->child_life_time)
			return OPERATION_TIMEOUT;

	}

	save_errno = errno;

	if (SERIALIZE_ACCEPT)
	{
		pool_semaphore_unlock(ACCEPT_FD_SEM);
		ereport(DEBUG1,
				(errmsg("UNLOCKING select()")));
	}

	/* check backend timer is expired */
	if (backend_timer_expired)
	{
		pool_backend_timer();
		backend_timer_expired = 0;
	}

	errno = save_errno;

	if (numfds == -1)
	{
		if (errno == EAGAIN || errno == EINTR)
			return RETRY;
		ereport(ERROR,
				(errmsg("failed to accept user connection"),
				 errdetail("select on socket failed with error : \"%m\"")));
	}

	for (walk = fds; *walk != -1; walk++)
	{
		if (FD_ISSET(*walk, &rmask))
		{
			fd = *walk;
			break;
		}
	}

	/*
	 * Note that some SysV systems do not work here. For those systems, we
	 * need some locking mechanism for the fd.
	 */
	memset(saddr, 0, sizeof(*saddr));
	saddr->salen = sizeof(saddr->addr);

#ifdef ACCEPT_PERFORMANCE
	gettimeofday(&now1, 0);
#endif

retry_accept:

	/* wait if recovery is started */
	while (*InRecovery == 1)
	{
		pause();
	}

	afd = accept(fd, (struct sockaddr *) &saddr->addr, &saddr->salen);

	save_errno = errno;
	/* check backend timer is expired */
	if (backend_timer_expired)
	{
		pool_backend_timer();
		backend_timer_expired = 0;
	}
	errno = save_errno;
	if (afd < 0)
	{
		if (errno == EINTR && *InRecovery)
			goto retry_accept;

		/*
		 * "Resource temporarily unavailable" (EAGAIN or EWOULDBLOCK) can be
		 * silently ignored. And EINTR can be ignored.
		 */
		if (errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR)
			ereport(ERROR,
					(errmsg("failed to accept user connection"),
					 errdetail("accept on socket failed with error : \"%m\"")));
		return RETRY;

	}

	/*
	 * Set no delay if AF_INET socket. Not sure if this is really necessary
	 * but PostgreSQL does this.
	 */
	if (unix_fds_not_isset(fds, pool_config->num_unix_socket_directories, &rmask))
	{
		on = 1;
		if (setsockopt(afd, IPPROTO_TCP, TCP_NODELAY,
					   (char *) &on,
					   sizeof(on)) < 0)
		{
			ereport(WARNING,
					(errmsg("wait_for_new_connections: setsockopt failed with error \"%m\"")));
			close(afd);
			return -1;
		}
	}

	/*
	 * Make sure that the socket is non blocking.
	 */
	socket_unset_nonblock(afd);

#ifdef ACCEPT_PERFORMANCE
	gettimeofday(&now2, 0);
	atime += (now2.tv_sec - now1.tv_sec) * 1000000 + (now2.tv_usec - now1.tv_usec);
	cnt++;
	if (cnt % 100 == 0)
	{
		ereport(LOG, (errmsg("cnt: %d atime: %ld", cnt, atime)));
	}
#endif
	return afd;
}

static bool
unix_fds_not_isset(int* fds, int num_unix_fds, fd_set* opt)
{
	int		i;
	for (i = 0; i < num_unix_fds; i++)
	{
		if (!FD_ISSET(fds[i], opt))
			continue;

		return false;
	}
	return true;
}

static void
check_config_reload(void)
{
	/* reload config file */
	if (got_sighup)
	{
		MemoryContext oldContext = MemoryContextSwitchTo(TopMemoryContext);

		pool_get_config(get_config_file_name(), CFGCXT_RELOAD);
		MemoryContextSwitchTo(oldContext);
		if (pool_config->enable_pool_hba)
		{
			load_hba(get_hba_file_name());
			if (strcmp("", pool_config->pool_passwd))
				pool_reopen_passwd_file();
		}
		got_sighup = 0;
	}
}

static void
get_backends_status(unsigned int *valid_backends, unsigned int *down_backends)
{
	int			i;

	*down_backends = 0;
	*valid_backends = 0;
	for (i = 0; i < NUM_BACKENDS; i++)
	{
		if (BACKEND_INFO(i).backend_status == CON_DOWN)
			(*down_backends)++;
		if (VALID_BACKEND(i))
			(*valid_backends)++;
	}
}

static void
validate_backend_connectivity(int front_end_fd)
{
	unsigned int valid_backends;
	unsigned int down_backends;
	bool		fatal_error = false;
	char	   *error_msg = NULL;
	char	   *error_detail = NULL;
	char	   *error_hint = NULL;

	get_backends_status(&valid_backends, &down_backends);

	if (valid_backends == 0)
	{
		fatal_error = true;
		error_msg = "pgpool is not accepting any new connections";
		error_detail = "all backend nodes are down, pgpool requires at least one valid node";
		error_hint = "repair the backend nodes and restart pgpool";
	}

	if (fatal_error)
	{
		/*
		 * check if we can inform the connecting client about the current
		 * situation before throwing the error
		 */

		if (front_end_fd > 0)
		{
			POOL_CONNECTION *cp;
			StartupPacket *volatile sp = NULL;

			/*
			 * we do not want to report socket error, as above errors will be
			 * more informative
			 */
			PG_TRY();
			{
				if ((cp = pool_open(front_end_fd, false)) == NULL)
				{
					close(front_end_fd);
					child_exit(POOL_EXIT_AND_RESTART);
				}
				sp = read_startup_packet(cp);
				ereport(DEBUG1,
						(errmsg("forwarding error message to frontend")));

				pool_send_error_message(cp, sp->major,
										(sp->major == PROTO_MAJOR_V3) ? "08S01" : NULL,
										error_msg,
										error_detail,
										error_hint,
										__FILE__,
										__LINE__);
				
			}
			PG_CATCH();
			{
				pool_free_startup_packet(sp);
				FlushErrorState();
				ereport(FATAL,
						(errmsg("%s", error_msg), errdetail("%s", error_detail), errhint("%s", error_hint)));
			}
			PG_END_TRY();
			pool_free_startup_packet(sp);
		}
		ereport(FATAL,
				(errmsg("%s", error_msg), errdetail("%s", error_detail), errhint("%s", error_hint)));
	}
	/* Every thing is good if we have reached this point */
}

/*
 * returns the POOL_CONNECTION object from socket descriptor
 * the socket must be already accepted
 */
static POOL_CONNECTION *
get_connection(int front_end_fd, SockAddr *saddr)
{
	POOL_CONNECTION *cp;

	ereport(DEBUG1,
			(errmsg("I am %d accept fd %d", getpid(), front_end_fd)));

	pool_getnameinfo_all(saddr, remote_host, remote_port);
	print_process_status(remote_host, remote_port);

	set_ps_display("accept connection", false);

	/* log who is connecting */
	if (pool_config->log_connections)
	{
		ereport(LOG,
				(errmsg("new connection received"),
				 errdetail("connecting host=%s%s%s",
						   remote_host, remote_port[0] ? " port=" : "", remote_port)));
	}


	/* set NODELAY and KEEPALIVE options if INET connection */
	if (saddr->addr.ss_family == AF_INET ||
		saddr->addr.ss_family == AF_INET6)
	{
		int			on = 1;

		if (setsockopt(front_end_fd, IPPROTO_TCP, TCP_NODELAY,
					   (char *) &on,
					   sizeof(on)) < 0)
			ereport(ERROR,
					(errmsg("failed to accept user connection"),
					 errdetail("setsockopt on socket failed with error : \"%m\"")));

		if (setsockopt(front_end_fd, SOL_SOCKET, SO_KEEPALIVE, (char *) &on, sizeof(on)) < 0)
			ereport(FATAL,
					(errmsg("failed to accept user connection"),
					 errdetail("setsockopt on socket failed with error : \"%m\"")));

	}

	if ((cp = pool_open(front_end_fd, false)) == NULL)
	{
		close(front_end_fd);
		ereport(ERROR,
				(errmsg("failed to accept user connection"),
				 errdetail("unable to open connection with remote end : \"%m\"")));
	}

	/* save ip address for hba */
	memcpy(&cp->raddr, saddr, sizeof(SockAddr));
	if (cp->raddr.addr.ss_family == 0)
		cp->raddr.addr.ss_family = AF_UNIX;

	return cp;
}

static POOL_CONNECTION_POOL *
get_backend_connection(POOL_CONNECTION * frontend)
{
	int			found = 0;
	StartupPacket *sp;
	POOL_CONNECTION_POOL *backend;

	/* read the startup packet */
retry_startup:
	sp = read_startup_packet(frontend);

	/* cancel request? */
	if (sp->major == 1234 && sp->minor == 5678)
	{
		cancel_request((CancelPacket *) sp->startup_packet);
		pool_free_startup_packet(sp);
		connection_count_down();
		return NULL;
	}

	/* SSL? */
	if (sp->major == 1234 && sp->minor == 5679 && !frontend->ssl_active)
	{
		ereport(DEBUG1,
				(errmsg("selecting backend connection"),
				 errdetail("SSLRequest from client")));

		pool_ssl_negotiate_serverclient(frontend);
		pool_free_startup_packet(sp);
		goto retry_startup;
	}

	/* GSSAPI? */
	if (sp->major == 1234 && sp->minor == 5680)
	{
		ereport(DEBUG1,
				(errmsg("selecting backend connection"),
				 errdetail("GSSAPI request from client")));

		/* sorry, Pgpool-II does not support GSSAPI yet */
		pool_write_and_flush(frontend, "N", 1);

		pool_free_startup_packet(sp);
		goto retry_startup;
	}

	frontend->protoVersion = sp->major;
	frontend->database = pstrdup(sp->database);
	frontend->username = pstrdup(sp->user);

	if (pool_config->enable_pool_hba)
	{
		/*
		 * do client authentication. Note that ClientAuthentication does not
		 * return if frontend was rejected; it simply terminates this process.
		 */
		MemoryContext frontend_auth_cxt = AllocSetContextCreate(CurrentMemoryContext,
										"frontend_auth",
										ALLOCSET_DEFAULT_SIZES);
		MemoryContext oldContext = MemoryContextSwitchTo(frontend_auth_cxt);

		ClientAuthentication(frontend);

		MemoryContextSwitchTo(oldContext);
		MemoryContextDelete(frontend_auth_cxt);
	}

	/*
	 * Ok, negotiation with frontend has been done. Let's go to the next step.
	 * Connect to backend if there's no existing connection which can be
	 * reused by this frontend. Authentication is also done in this step.
	 */

	/*
	 * Check if restart request is set because of failback event happened.  If
	 * so, close idle connections to backend and make a new copy of backend
	 * status.
	 */
	if (pool_get_my_process_info()->need_to_restart)
	{
		ereport(LOG,
				(errmsg("selecting backend connection"),
				 errdetail("failover or failback event detected, discarding existing connections")));

		pool_get_my_process_info()->need_to_restart = 0;
		close_idle_connection(0);
		pool_initialize_private_backend_status();
	}

	/*
	 * if there's no connection associated with user and database, we need to
	 * connect to the backend and send the startup packet.
	 */

	/* look for an existing connection */
	found = 0;

	backend = pool_get_cp(sp->user, sp->database, sp->major, 1);

	if (backend != NULL)
	{
		found = 1;

		/*
		 * existing connection associated with same user/database/major found.
		 * however we should make sure that the startup packet contents are
		 * identical. OPTION data and others might be different.
		 */
		if (sp->len != MAIN_CONNECTION(backend)->sp->len)
		{
			ereport(DEBUG1,
					(errmsg("selecting backend connection"),
					 errdetail("connection exists but startup packet length is not identical")));

			found = 0;
		}
		else if (memcmp(sp->startup_packet, MAIN_CONNECTION(backend)->sp->startup_packet, sp->len) != 0)
		{
			ereport(DEBUG1,
					(errmsg("selecting backend connection"),
					 errdetail("connection exists but startup packet contents is not identical")));
			found = 0;
		}

		if (found == 0)
		{
			/*
			 * we need to discard existing connection since startup packet is
			 * different
			 */
			pool_discard_cp(sp->user, sp->database, sp->major);
			backend = NULL;
		}
	}

	if (backend == NULL)
	{
		/* create a new connection to backend */
		backend = connect_backend(sp, frontend);
	}
	else
	{
		/* reuse existing connection */
		if (!connect_using_existing_connection(frontend, backend, sp))
			return NULL;
	}

	pool_free_startup_packet(sp);
	return backend;
}

static void
log_disconnections(char *database, char *username)
{
	struct timeval endTime;
	long diff;
	long		secs;
	int			msecs,
				hours,
				minutes,
				seconds;

	gettimeofday(&endTime, NULL);
	diff = (long) ((endTime.tv_sec - startTime.tv_sec) * 1000000  + (endTime.tv_usec - startTime.tv_usec));

	msecs = (int) (diff % 1000000) / 1000;
	secs = (long) (diff / 1000000);
	hours = secs / 3600;
	secs %= 3600;
	minutes = secs / 60;
	seconds = secs % 60;

	ereport(LOG,
			(errmsg("frontend disconnection: session time: %d:%02d:%02d.%03d "
					"user=%s database=%s host=%s%s%s",
					hours, minutes, seconds, msecs,
					username, database, remote_host, remote_port[0] ? " port=" : "", remote_port)));
}

static void
print_process_status(char *remote_host, char *remote_port)
{
	if (remote_port[0] == '\0')
		snprintf(remote_ps_data, sizeof(remote_ps_data), "%s", remote_port);
	else
		snprintf(remote_ps_data, sizeof(remote_ps_data), "%s(%s)", remote_host, remote_port);
}



int
send_to_pg_frontend(char *data, int len, bool flush)
{
	int			ret;

	if (processType != PT_CHILD || child_frontend == NULL)
		return -1;
	if (child_frontend->socket_state != POOL_SOCKET_VALID)
		return -1;
	ret = pool_write_noerror(child_frontend, data, len);
	if (flush && !ret)
		ret = pool_flush_it(child_frontend);
	return ret;
}

int
set_pg_frontend_blocking(bool blocking)
{
	if (processType != PT_CHILD || child_frontend == NULL)
		return -1;
	if (child_frontend->socket_state != POOL_SOCKET_VALID)
		return -1;
	if (blocking)
		socket_unset_nonblock(child_frontend->fd);
	else
		socket_set_nonblock(child_frontend->fd);
	return 0;
}

int
get_frontend_protocol_version(void)
{
	if (processType != PT_CHILD || child_frontend == NULL)
		return -1;
	return child_frontend->protoVersion;
}

int
pg_frontend_exists(void)
{
	if (processType != PT_CHILD || child_frontend == NULL)
		return -1;
	return 0;
}

static int opt_sort(const void *a, const void *b)
{
	return strcmp( *(char **)a, *(char **)b);
}

void
set_process_status(ProcessStatus status)
{
	pool_get_my_process_info()->status = status;
}
