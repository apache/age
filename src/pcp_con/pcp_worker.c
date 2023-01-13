/* -*-pcp_worker.c-*- */
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
 * pcp_worker.c: PCP worker child process main
 *
 */


#include "config.h"

#include <arpa/inet.h>
#include <signal.h>

#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/time.h>

#ifdef HAVE_FCNTL_H
#include <fcntl.h>
#endif

#include "pool.h"
#include "pool_config.h"

#include "pcp/pcp_stream.h"
#include "pcp/pcp.h"
#include "pcp/pcp_worker.h"
#include "pcp/recovery.h"
#include "auth/md5.h"
#include "auth/pool_auth.h"
#include "context/pool_process_context.h"
#include "context/pool_session_context.h"
#include "utils/pool_process_reporting.h"
#include "utils/palloc.h"
#include "utils/memutils.h"
#include "utils/ps_status.h"
#include "utils/elog.h"
#include "watchdog/wd_json_data.h"
#include "watchdog/wd_internal_commands.h"
#include "main/pool_internal_comms.h"


#define MAX_FILE_LINE_LEN    512

extern char *pcp_conf_file;		/* global variable defined in main.c holds the
								 * path for pcp.conf */
volatile sig_atomic_t pcp_worker_wakeup_request = 0;
PCP_CONNECTION *volatile pcp_frontend = NULL;

static RETSIGTYPE die(int sig);
static RETSIGTYPE wakeup_handler_child(int sig);

static void unset_nonblock(int fd);
static int	user_authenticate(char *buf, char *passwd_file, char *salt, int salt_len);
static void process_authentication(PCP_CONNECTION * frontend, char *buf, char *salt, int *random_salt);
static void send_md5salt(PCP_CONNECTION * frontend, char *salt);

static void pcp_process_command(char tos, char *buf, int buf_len);

static int	pool_detach_node(int node_id, bool gracefully, bool switchover);
static int	pool_promote_node(int node_id, bool gracefully);
static void inform_process_count(PCP_CONNECTION * frontend);
static void inform_process_info(PCP_CONNECTION * frontend, char *buf);
static void inform_watchdog_info(PCP_CONNECTION * frontend, char *buf);
static void inform_node_info(PCP_CONNECTION * frontend, char *buf);
static void inform_node_count(PCP_CONNECTION * frontend);
static void process_reload_config(PCP_CONNECTION * frontend,char scope);
static void inform_health_check_stats(PCP_CONNECTION *frontend, char *buf);
static void process_detach_node(PCP_CONNECTION * frontend, char *buf, char tos);
static void process_attach_node(PCP_CONNECTION * frontend, char *buf);
static void process_recovery_request(PCP_CONNECTION * frontend, char *buf);
static void process_status_request(PCP_CONNECTION * frontend);
static void process_promote_node(PCP_CONNECTION * frontend, char *buf, char tos);
static void process_shutdown_request(PCP_CONNECTION * frontend, char mode, char tos);
static void process_set_configuration_parameter(PCP_CONNECTION * frontend, char *buf, int len);

static void pcp_worker_will_go_down(int code, Datum arg);

static void do_pcp_flush(PCP_CONNECTION * frontend);
static void do_pcp_read(PCP_CONNECTION * pc, void *buf, int len);

/*
 * main entry pont of pcp worker child process
 */
void
pcp_worker_main(int port)
{
	sigjmp_buf	local_sigjmp_buf;
	MemoryContext PCPMemoryContext;
	volatile int authenticated = 0;

	char		salt[4];
	int			random_salt = 0;
	struct timeval uptime;
	char		tos;
	int			rsize;

	ereport(DEBUG1,
			(errmsg("I am PCP worker child with pid:%d", getpid())));

	/* Identify myself via ps */
	init_ps_display("", "", "", "");

	gettimeofday(&uptime, NULL);
	srandom((unsigned int) (getpid() ^ uptime.tv_usec));

	/* set up signal handlers */
	signal(SIGTERM, die);
	signal(SIGINT, die);
	signal(SIGQUIT, die);
	signal(SIGCHLD, SIG_DFL);
	signal(SIGUSR2, wakeup_handler_child);
	signal(SIGUSR1, SIG_IGN);
	signal(SIGHUP, SIG_IGN);
	signal(SIGPIPE, SIG_IGN);
	signal(SIGALRM, SIG_IGN);
	/* Create per loop iteration memory context */
	PCPMemoryContext = AllocSetContextCreate(TopMemoryContext,
											 "PCP_worker_main_loop",
											 ALLOCSET_DEFAULT_MINSIZE,
											 ALLOCSET_DEFAULT_INITSIZE,
											 ALLOCSET_DEFAULT_MAXSIZE);

	MemoryContextSwitchTo(TopMemoryContext);

	/*
	 * install the call back for preparation of pcp worker child exit
	 */
	on_system_exit(pcp_worker_will_go_down, (Datum) NULL);

	/* Initialize my backend status */
	pool_initialize_private_backend_status();

	/* Initialize process context */
	pool_init_process_context();

	pcp_frontend = pcp_open(port);
	unset_nonblock(pcp_frontend->fd);

	if (sigsetjmp(local_sigjmp_buf, 1) != 0)
	{
		error_context_stack = NULL;
		EmitErrorReport();

		MemoryContextSwitchTo(TopMemoryContext);
		FlushErrorState();
	}
	/* We can now handle ereport(ERROR) */
	PG_exception_stack = &local_sigjmp_buf;

	for (;;)
	{
		char	*buf = NULL;

		MemoryContextSwitchTo(PCPMemoryContext);
		MemoryContextResetAndDeleteChildren(PCPMemoryContext);

		errno = 0;

		/* read a PCP packet */
		do_pcp_read(pcp_frontend, &tos, 1);
		do_pcp_read(pcp_frontend, &rsize, sizeof(int));

		rsize = ntohl(rsize);

		if (rsize <= 0 || rsize >= MAX_PCP_PACKET_LENGTH)
			ereport(FATAL,
					(errmsg("invalid PCP packet"),
					 errdetail("incorrect packet length (%d)", rsize)));

		if ((rsize - sizeof(int)) > 0)
		{
			buf = (char *) palloc(rsize - sizeof(int));
			do_pcp_read(pcp_frontend, buf, rsize - sizeof(int));
		}

		ereport(DEBUG1,
				(errmsg("received PCP packet"),
				 errdetail("PCP packet type of service '%c'", tos)));

		if (tos == 'R')			/* authentication */
		{
			set_ps_display("PCP: processing authentication", false);
			process_authentication(pcp_frontend, buf, salt, &random_salt);
			authenticated = 1;
			continue;
		}
		if (tos == 'M')			/* md5 salt */
		{
			set_ps_display("PCP: processing authentication", false);
			send_md5salt(pcp_frontend, salt);
			random_salt = 1;
			continue;
		}
		/* is this connection authenticated? if not disconnect immediately */
		if (!authenticated)
			ereport(FATAL,
					(errmsg("authentication failed for new PCP connection"),
					 errdetail("connection not authorized")));

		/* process a request */
		pcp_process_command(tos, buf, rsize);
	}
	exit(0);
}


/* pcp command processor */
static void
pcp_process_command(char tos, char *buf, int buf_len)
{
	/* The request is recovery or pcp shutdown request? */
	if (tos == 'O' || tos == 'T')
	{
		/* Prevent those pcp requests while processing failover/failback request */
		if (Req_info->switching)
		{
			if (Req_info->request_queue_tail != Req_info->request_queue_head)
			{
				POOL_REQUEST_KIND reqkind;

				reqkind = Req_info->request[(Req_info->request_queue_head + 1) % MAX_REQUEST_QUEUE_SIZE].kind;

				if (reqkind == NODE_UP_REQUEST)
					ereport(ERROR,
							(errmsg("failed to process PCP request at the moment"),
							 errdetail("failback is in progress")));
				else if (reqkind == NODE_DOWN_REQUEST)
					ereport(ERROR,
							(errmsg("failed to process PCP request at the moment"),
							 errdetail("failover is in progress")));
				else if (reqkind == PROMOTE_NODE_REQUEST)
					ereport(ERROR,
							(errmsg("failed to process PCP request at the moment"),
							 errdetail("promote node operation is in progress")));

				ereport(ERROR,
						(errmsg("failed to process PCP request at the moment"),
						 errdetail("operation is in progress")));
			}
		}
	}

	switch (tos)
	{
		case 'A':				/* set configuration parameter */
			set_ps_display("PCP: processing set configuration parameter request", false);
			process_set_configuration_parameter(pcp_frontend, buf, buf_len);
			break;

		case 'L':				/* node count */
			set_ps_display("PCP: processing node count request", false);
			inform_node_count(pcp_frontend);
			break;

		case 'H':				/* health check stats */
			set_ps_display("PCP: processing health check stats request", false);
			inform_health_check_stats(pcp_frontend, buf);
			break;

		case 'I':				/* node info */
			set_ps_display("PCP: processing node info request", false);
			inform_node_info(pcp_frontend, buf);
			break;

		case 'N':				/* process count */
			set_ps_display("PCP: processing process count request", false);
			inform_process_count(pcp_frontend);
			break;

		case 'P':				/* process info */
			set_ps_display("PCP: processing process info request", false);
			inform_process_info(pcp_frontend, buf);
			break;

		case 'W':				/* watchdog info */
			set_ps_display("PCP: processing watchdog info request", false);
			inform_watchdog_info(pcp_frontend, buf);
			break;

		case 'D':				/* detach node */
		case 'd':				/* detach node gracefully */
			set_ps_display("PCP: processing detach node request", false);
			process_detach_node(pcp_frontend, buf, tos);
			break;

		case 'C':				/* attach node */
			set_ps_display("PCP: processing attach node request", false);
			process_attach_node(pcp_frontend, buf);
			break;

		case 'T':
		case 't':
			set_ps_display("PCP: processing shutdown request", false);
			process_shutdown_request(pcp_frontend, buf[0], tos);
			break;

		case 'O':				/* recovery request */
			set_ps_display("PCP: processing recovery request", false);
			process_recovery_request(pcp_frontend, buf);
			break;

		case 'B':				/* status request */
			set_ps_display("PCP: processing status request request", false);
			process_status_request(pcp_frontend);
			break;

		case 'Z':				/*reload config file */
			set_ps_display("PCP: processing reload config request", false);
			process_reload_config(pcp_frontend, buf[0]);
			break;

		case 'J':				/* promote node */
		case 'j':				/* promote node gracefully */
			set_ps_display("PCP: processing promote node request", false);
			process_promote_node(pcp_frontend, buf, tos);
			break;

		case 'F':
			ereport(DEBUG1,
					(errmsg("PCP processing request, stop online recovery")));
			break;

		case 'X':				/* disconnect */
			ereport(DEBUG1,
					(errmsg("PCP processing request, client disconnecting"),
					 errdetail("closing PCP connection, and exiting child")));
			pcp_close(pcp_frontend);
			pcp_frontend = NULL;
			/* This child has done its part. Rest in peace now */
			exit(0);
			break;

		default:
			ereport(FATAL,
					(errmsg("PCP processing request"),
					 errdetail("unknown PCP packet type \"%c\"", tos)));
	}
}

static RETSIGTYPE
die(int sig)
{
	ereport(DEBUG1,
			(errmsg("PCP worker child receives shutdown request signal %d", sig)));
	if (sig == SIGTERM)
	{
		ereport(DEBUG1,
				(errmsg("PCP worker child receives smart shutdown request."),
				 errdetail("waiting for the child to die its natural death")));
	}
	else if (sig == SIGINT)
	{
		ereport(DEBUG1,
				(errmsg("PCP worker child receives fast shutdown request.")));
		exit(0);
	}
	else if (sig == SIGQUIT)
	{
		ereport(DEBUG1,
				(errmsg("PCP worker child receives immediate shutdown request.")));
		exit(0);
	}
	else
		exit(1);
}


static RETSIGTYPE
wakeup_handler_child(int sig)
{
	pcp_worker_wakeup_request = 1;
}

/*
 * unset non-block flag
 */
static void
unset_nonblock(int fd)
{
	int			var;

	/* set fd to non-blocking */
	var = fcntl(fd, F_GETFL, 0);
	if (var == -1)
	{
		ereport(FATAL,
				(errmsg("unable to connect"),
				 errdetail("fcntl system call failed with error : \"%m\"")));

	}
	if (fcntl(fd, F_SETFL, var & ~O_NONBLOCK) == -1)
	{
		ereport(FATAL,
				(errmsg("unable to connect"),
				 errdetail("fcntl system call failed with error : \"%m\"")));
	}
}

/*
 * see if received username and password matches with one in the file
 */
static int
user_authenticate(char *buf, char *passwd_file, char *salt, int salt_len)
{
	FILE	   *fp = NULL;
	char		packet_username[MAX_USER_PASSWD_LEN + 1];
	char		packet_password[MAX_USER_PASSWD_LEN + 1];
	char		encrypt_buf[(MD5_PASSWD_LEN + 1) * 2];
	char		file_username[MAX_USER_PASSWD_LEN + 1];
	char		file_password[MAX_USER_PASSWD_LEN + 1];
	char	   *index = NULL;
	static char line[MAX_FILE_LINE_LEN + 1];
	int			i,
				len;

	/* strcpy() should be OK, but use strncpy() to be extra careful */
	strncpy(packet_username, buf, MAX_USER_PASSWD_LEN);
	index = (char *) memchr(buf, '\0', MAX_USER_PASSWD_LEN);
	if (index == NULL)
	{
		ereport(FATAL,
				(errmsg("failed to authenticate PCP user"),
				 errdetail("error while reading authentication packet")));
		return 0;
	}
	strncpy(packet_password, ++index, MAX_USER_PASSWD_LEN);

	fp = fopen(passwd_file, "r");
	if (fp == NULL)
	{
		ereport(FATAL,
				(errmsg("failed to authenticate PCP user"),
				 errdetail("could not open %s. reason: %m", passwd_file)));
		return 0;
	}

	/* for now, I don't care if duplicate username exists in the config file */
	while ((fgets(line, MAX_FILE_LINE_LEN, fp)) != NULL)
	{
		i = 0;
		len = 0;

		if (line[0] == '\n' || line[0] == '#')
			continue;

		while (line[i] != ':')
		{
			len++;
			if (++i > MAX_USER_PASSWD_LEN)
			{
				fclose(fp);
				ereport(FATAL,
						(errmsg("failed to authenticate PCP user"),
						 errdetail("username read from file \"%s\" is larger than maximum allowed username length [%d]", passwd_file, MAX_USER_PASSWD_LEN)));
				return 0;
			}
		}
		memcpy(file_username, line, len);
		file_username[len] = '\0';

		if (strcmp(packet_username, file_username) != 0)
			continue;

		i++;
		len = 0;
		while (line[i] != '\n' && line[i] != '\0')
		{
			len++;
			if (++i > MAX_USER_PASSWD_LEN)
			{
				fclose(fp);
				ereport(FATAL,
						(errmsg("failed to authenticate PCP user"),
						 errdetail("password read from file \"%s\" is larger than maximum allowed password length [%d]", passwd_file, MAX_USER_PASSWD_LEN)));
				return 0;
			}
		}

		memcpy(file_password, line + strlen(file_username) + 1, len);
		file_password[len] = '\0';

		pool_md5_encrypt(file_password, file_username, strlen(file_username),
						 encrypt_buf + MD5_PASSWD_LEN + 1);
		encrypt_buf[(MD5_PASSWD_LEN + 1) * 2 - 1] = '\0';

		pool_md5_encrypt(encrypt_buf + MD5_PASSWD_LEN + 1, salt, salt_len,
						 encrypt_buf);
		encrypt_buf[MD5_PASSWD_LEN] = '\0';

		if (strcmp(encrypt_buf, packet_password) == 0)
		{
			fclose(fp);
			return 1;
		}
	}
	fclose(fp);
	ereport(FATAL,
			(errmsg("authentication failed for user \"%s\"", packet_username),
			 errdetail("username and/or password does not match")));

	return 0;
}


/* Detach a node */
static int
pool_detach_node(int node_id, bool gracefully, bool switchover)
{
	int		flag = 0;

	if (switchover)
		flag = REQ_DETAIL_PROMOTE;

	if (!gracefully)
	{
		degenerate_backend_set_ex(&node_id, 1, flag | REQ_DETAIL_SWITCHOVER | REQ_DETAIL_CONFIRMED, true, false);
		return 0;
	}

	/*
	 * Check if the NODE DOWN can be executed on the given node id.
	 */
	degenerate_backend_set_ex(&node_id, 1, REQ_DETAIL_SWITCHOVER | REQ_DETAIL_CONFIRMED, true, true);

	/*
	 * Wait until all frontends exit
	 */
	*InRecovery = RECOVERY_DETACH;	/* This will ensure that new incoming
									 * connection requests are blocked */

	if (wait_connection_closed())
	{
		/* wait timed out */
		finish_recovery();
		return -1;
	}

	pcp_worker_wakeup_request = 0;

	/*
	 * Now all frontends have gone. Let's do failover.
	 */
	degenerate_backend_set_ex(&node_id, 1, flag | REQ_DETAIL_SWITCHOVER | REQ_DETAIL_CONFIRMED, false, false);

	/*
	 * Wait for failover completed.
	 */

	while (!pcp_worker_wakeup_request)
	{
		struct timeval t = {1, 0};

		select(0, NULL, NULL, NULL, &t);
	}
	pcp_worker_wakeup_request = 0;

	/*
	 * Start to accept incoming connections and send SIGUSR2 to pgpool parent
	 * to distribute SIGUSR2 all pgpool children.
	 */
	finish_recovery();

	return 0;
}

/* Promote a node */
static int
pool_promote_node(int node_id, bool gracefully)
{
	if (!gracefully)
	{
		promote_backend(node_id, REQ_DETAIL_CONFIRMED); /* send promote request */
		return 0;
	}

	/*
	 * Wait until all frontends exit
	 */
	*InRecovery = RECOVERY_PROMOTE; /* This will ensure that new incoming
									 * connection requests are blocked */

	if (wait_connection_closed())
	{
		/* wait timed out */
		finish_recovery();
		return -1;
	}

	/*
	 * Now all frontends have gone. Let's do failover.
	 */
	promote_backend(node_id, REQ_DETAIL_CONFIRMED); /* send promote request */

	/*
	 * Wait for failover completed.
	 */
	pcp_worker_wakeup_request = 0;

	while (!pcp_worker_wakeup_request)
	{
		struct timeval t = {1, 0};

		select(0, NULL, NULL, NULL, &t);
	}
	pcp_worker_wakeup_request = 0;

	/*
	 * Start to accept incoming connections and send SIGUSR2 to pgpool parent
	 * to distribute SIGUSR2 all pgpool children.
	 */
	finish_recovery();
	return 0;
}

static void
inform_process_count(PCP_CONNECTION * frontend)
{
	int			wsize;
	int			process_count;
	char		process_count_str[16];
	int		   *process_list = NULL;
	char		code[] = "CommandComplete";
	char	   *mesg = NULL;
	int			i;
	int			total_port_len = 0;

	process_list = pool_get_process_list(&process_count);

	mesg = (char *) palloc(8 * process_count);	/* PID is at most 7 characters long */

	snprintf(process_count_str, sizeof(process_count_str), "%d", process_count);

	for (i = 0; i < process_count; i++)
	{
		char		process_id[8];

		snprintf(process_id, sizeof(process_id), "%d", process_list[i]);
		snprintf(mesg + total_port_len, strlen(process_id) + 1, "%s", process_id);
		total_port_len += strlen(process_id) + 1;
	}

	pcp_write(frontend, "n", 1);
	wsize = htonl(sizeof(code) +
				  strlen(process_count_str) + 1 +
				  total_port_len +
				  sizeof(int));
	pcp_write(frontend, &wsize, sizeof(int));
	pcp_write(frontend, code, sizeof(code));
	pcp_write(frontend, process_count_str, strlen(process_count_str) + 1);
	pcp_write(frontend, mesg, total_port_len);
	do_pcp_flush(frontend);

	pfree(process_list);
	pfree(mesg);

	ereport(DEBUG1,
			(errmsg("PCP: informing process count"),
			 errdetail("%d process(es) found", process_count)));
}

/*
 * pcp_process_info
 */
static void
inform_process_info(PCP_CONNECTION * frontend, char *buf)
{
	int			proc_id;
	int			wsize;
	int			num_proc = pool_config->num_init_children;
	int			i;
	int			*offsets;
	int			n;
	POOL_REPORT_POOLS *pools;

	proc_id = atoi(buf);

	if ((proc_id != 0) && (pool_get_process_info(proc_id) == NULL))
	{
		ereport(ERROR,
				(errmsg("informing process info failed"),
				 errdetail("invalid process ID : %s", buf)));
	}
	else
	{
		/* First, send array size of connection_info */
		char		arr_code[] = "ArraySize";
		char		con_info_size[16];

		/* Finally, indicate that all data is sent */
		char		fin_code[] = "CommandComplete";

		pools = get_pools(&num_proc);

		if (proc_id == 0)
		{
			snprintf(con_info_size, sizeof(con_info_size), "%d", num_proc);
		}
		else
		{
			snprintf(con_info_size, sizeof(con_info_size), "%d", pool_config->max_pool * NUM_BACKENDS);
		}

		pcp_write(frontend, "p", 1);
		wsize = htonl(sizeof(arr_code) +
					  strlen(con_info_size) + 1 +
					  sizeof(int));
		pcp_write(frontend, &wsize, sizeof(int));
		pcp_write(frontend, arr_code, sizeof(arr_code));
		pcp_write(frontend, con_info_size, strlen(con_info_size) + 1);
		do_pcp_flush(frontend);

		offsets = pool_report_pools_offsets(&n);

		/* Second, send process information for all connection_info */
		for (i = 0; i < num_proc; i++)
		{
			int			j;
			char		code[] = "ProcessInfo";
			int			pool_pid;

			pool_pid = atoi(pools[i].pool_pid);

			if (proc_id != 0 && pool_pid != proc_id)
				continue;

			pcp_write(frontend, "p", 1);

			wsize = 0;
			for (j = 0; j < n; j++)
			{
				wsize += strlen((char *)&pools[i] + offsets[j]) + 1;
			}
			wsize += sizeof(code) + sizeof(int);
			wsize = htonl(wsize);

			/* send packet length to frontend */
			pcp_write(frontend, &wsize, sizeof(int));
			/* send "this is a record" to frontend */
			pcp_write(frontend, code, sizeof(code));

			/* send each process info data to frontend */
			for (j = 0; j < n; j++)
			{
				pcp_write(frontend, (char *)&pools[i] + offsets[j], strlen((char *)&pools[i] + offsets[j]) + 1);
			}

			do_pcp_flush(frontend);
		}

		pcp_write(frontend, "p", 1);
		wsize = htonl(sizeof(fin_code) +
					  sizeof(int));
		pcp_write(frontend, &wsize, sizeof(int));
		pcp_write(frontend, fin_code, sizeof(fin_code));
		do_pcp_flush(frontend);
		ereport(DEBUG1,
				(errmsg("PCP informing process info"),
				 errdetail("retrieved process information from shared memory")));

		pfree(pools);
	}
}

static void
inform_watchdog_info(PCP_CONNECTION * frontend, char *buf)
{
	int			wd_index;
	int			json_data_len;
	int			wsize;
	char		code[] = "CommandComplete";
	char	   *json_data;

	if (!pool_config->use_watchdog)
		ereport(ERROR,
				(errmsg("PCP: informing watchdog info failed"),
				 errdetail("watchdog is not enabled")));

	wd_index = atoi(buf);

	json_data = wd_internal_get_watchdog_nodes_json(wd_index);
	if (json_data == NULL)
		ereport(ERROR,
				(errmsg("PCP: informing watchdog info failed"),
				 errdetail("invalid watchdog index")));

	ereport(DEBUG2,
			(errmsg("PCP: informing watchdog info"),
			 errdetail("retrieved node information from IPC socket")));

	/*
	 * This is the violation of PCP protocol but I think in future we should
	 * shift to more adaptable protocol for data transmission.
	 */
	json_data_len = strlen(json_data);
	wsize = htonl(sizeof(code) +
				  json_data_len + 1 +
				  sizeof(int));
	pcp_write(frontend, "w", 1);

	pcp_write(frontend, &wsize, sizeof(int));
	pcp_write(frontend, code, sizeof(code));

	pcp_write(frontend, json_data, json_data_len + 1);
	do_pcp_flush(frontend);

	pfree(json_data);
}

static void
inform_node_info(PCP_CONNECTION * frontend, char *buf)
{
	POOL_REPORT_NODES *nodes;
	int			nrows;
	int			node_id;
	int			wsize;
	int			i;


	node_id = atoi(buf);

	if ((node_id != -1) && (pool_get_node_info(node_id) == NULL))
	{
		ereport(ERROR,
				(errmsg("informing node info failed"),
				 errdetail("invalid node ID : %s", buf)));
	}
	else
	{
		/* First, send array size of node_info */
		char		arr_code[] = "ArraySize";
		char		node_info_size[16];

		/* Finally, indicate that all data is sent */
		char		fin_code[] = "CommandComplete";

		nodes = get_nodes(&nrows, node_id);

		if (node_id == -1)
		{
			snprintf(node_info_size, sizeof(node_info_size), "%d", NUM_BACKENDS);
		}
		else
		{
			snprintf(node_info_size, sizeof(node_info_size), "%d", 1);
		}

		pcp_write(frontend, "i", 1);
		wsize = htonl(sizeof(arr_code) +
					  strlen(node_info_size) + 1 +
					  sizeof(int));
		pcp_write(frontend, &wsize, sizeof(int));
		pcp_write(frontend, arr_code, sizeof(arr_code));
		pcp_write(frontend, node_info_size, strlen(node_info_size) + 1);
		do_pcp_flush(frontend);

		/* Second, send process information for all connection_info */
		for (i = 0; i < NUM_BACKENDS ; i++)
		{
			char		port_str[6];
			char		status[2];
			char		quarantine[2];
			char		weight_str[20];
			char		role_str[10];
			char		standby_delay_str[20];
			char		standby_delay_by_time_str[4];
			char		status_changed_time_str[20];
			char		code[] = "NodeInfo";
			BackendInfo *bi = NULL;
			SERVER_ROLE role;

			if (node_id != -1 && node_id != i)
				continue;

			bi = pool_get_node_info(i);

			if (bi == NULL)
				ereport(ERROR,
						(errmsg("informing node info failed"),
						errdetail("invalid node ID")));

			snprintf(port_str, sizeof(port_str), "%d", bi->backend_port);
			snprintf(status, sizeof(status), "%d", bi->backend_status);
			snprintf(quarantine, sizeof(quarantine), "%d", bi->quarantine);
			snprintf(weight_str, sizeof(weight_str), "%f", bi->backend_weight);

			if (STREAM)
			{
				if (Req_info->primary_node_id == i)
					role = ROLE_PRIMARY;
				else
					role = ROLE_STANDBY;
			}
			else
			{
				if (Req_info->main_node_id == i)
					role = ROLE_MAIN;
				else
					role = ROLE_REPLICA;
			}
			snprintf(role_str, sizeof(role_str), "%d", role);

			snprintf(standby_delay_by_time_str, sizeof(standby_delay_by_time_str), "%d", bi->standby_delay_by_time);

			snprintf(standby_delay_str, sizeof(standby_delay_str), UINT64_FORMAT, bi->standby_delay);

			snprintf(status_changed_time_str, sizeof(status_changed_time_str), UINT64_FORMAT, bi->status_changed_time);

			pcp_write(frontend, "i", 1);
			wsize = htonl(sizeof(code) +
						  strlen(bi->backend_hostname) + 1 +
						  strlen(port_str) + 1 +
						  strlen(status) + 1 +
						  strlen(quarantine) + 1 +
						  strlen(nodes[i].pg_status) + 1 +
						  strlen(weight_str) + 1 +
						  strlen(role_str) + 1 +
						  strlen(nodes[i].pg_role) + 1 +
						  strlen(standby_delay_by_time_str) + 1 +
						  strlen(standby_delay_str) + 1 +
						  strlen(bi->replication_state) + 1 +
						  strlen(bi->replication_sync_state) + 1 +
						  strlen(status_changed_time_str) + 1 +
						  sizeof(int));
			pcp_write(frontend, &wsize, sizeof(int));
			pcp_write(frontend, code, sizeof(code));
			pcp_write(frontend, bi->backend_hostname, strlen(bi->backend_hostname) + 1);
			pcp_write(frontend, port_str, strlen(port_str) + 1);
			pcp_write(frontend, status, strlen(status) + 1);
			pcp_write(frontend, quarantine, strlen(quarantine) + 1);
			pcp_write(frontend, nodes[i].pg_status, strlen(nodes[i].pg_status) + 1);
			pcp_write(frontend, weight_str, strlen(weight_str) + 1);
			pcp_write(frontend, role_str, strlen(role_str) + 1);
			pcp_write(frontend, nodes[i].pg_role, strlen(nodes[i].pg_role) + 1);
			pcp_write(frontend, standby_delay_by_time_str, strlen(standby_delay_by_time_str) + 1);
			pcp_write(frontend, standby_delay_str, strlen(standby_delay_str) + 1);
			pcp_write(frontend, bi->replication_state, strlen(bi->replication_state) + 1);
			pcp_write(frontend, bi->replication_sync_state, strlen(bi->replication_sync_state) + 1);
			pcp_write(frontend, status_changed_time_str, strlen(status_changed_time_str) + 1);
			do_pcp_flush(frontend);
		}

		pcp_write(frontend, "i", 1);
		wsize = htonl(sizeof(fin_code) +
					  sizeof(int));
		pcp_write(frontend, &wsize, sizeof(int));
		pcp_write(frontend, fin_code, sizeof(fin_code));
		do_pcp_flush(frontend);
		ereport(DEBUG1,
				(errmsg("PCP informing node info"),
				 errdetail("retrieved node information from shared memory")));

		pfree(nodes);
	}
}

/*
 * Send out health check stats data to pcp client.  node id is provided as a
 * string in buf parameter.
 *
 * The protocol starts with 'h', followed by 4-byte packet length integer in
 * network byte order including self.  Each data is represented as a null
 * terminated string. The order of each data is defined in
 * POOL_HEALTH_CHECK_STATS struct.
 */
static void
inform_health_check_stats(PCP_CONNECTION *frontend, char *buf)
{
	POOL_HEALTH_CHECK_STATS *stats;
	POOL_HEALTH_CHECK_STATS *s;
	int		*offsets;
	int		n;
	int		nrows;
	int		i;
	int		node_id;
	bool	node_id_ok = false;
	int		wsize;
	char	code[] = "CommandComplete";

	node_id = atoi(buf);

	if (node_id < 0 || node_id > NUM_BACKENDS)
	{
		ereport(ERROR,
				(errmsg("informing health check stats info failed"),
				 errdetail("invalid node ID %d", node_id)));
	}
	
	stats = get_health_check_stats(&nrows);

	for (i = 0; i < nrows; i++)
	{
		if (atoi(stats[i].node_id) == node_id)
		{
			node_id_ok = true;
			s = &stats[i];
			break;
		}
	}

	if (!node_id_ok)
	{
		ereport(ERROR,
				(errmsg("informing health check stats info failed"),
				 errdetail("stats data for node ID %d does not exist", node_id)));
	}

	pcp_write(frontend, "h", 1);	/* indicate that this is a reply to health check stats request */

	wsize = sizeof(code) + sizeof(int);

	/* Calculate total packet length */
	offsets = pool_health_check_stats_offsets(&n);

	for (i = 0; i < n; i++)
	{
		wsize += strlen((char *)s + offsets[i]) + 1;
	}
	wsize = htonl(wsize);	/* convert to network byte order */

	/* send packet length to frontend */
	pcp_write(frontend, &wsize, sizeof(int));
	/* send "Command Complete" to frontend */
	pcp_write(frontend, code, sizeof(code));

	/* send each health check stats data to frontend */
	for (i = 0; i < n; i++)
	{
		pcp_write(frontend, (char *)s + offsets[i], strlen((char *)s + offsets[i]) + 1);
	}
	pfree(stats);
	do_pcp_flush(frontend);
}

static void
inform_node_count(PCP_CONNECTION * frontend)
{
	int			wsize;
	char		mesg[16];
	char		code[] = "CommandComplete";
	int			node_count = pool_get_node_count();

	snprintf(mesg, sizeof(mesg), "%d", node_count);

	pcp_write(frontend, "l", 1);
	wsize = htonl(sizeof(code) +
				  strlen(mesg) + 1 +
				  sizeof(int));
	pcp_write(frontend, &wsize, sizeof(int));
	pcp_write(frontend, code, sizeof(code));
	pcp_write(frontend, mesg, strlen(mesg) + 1);
	do_pcp_flush(frontend);

	ereport(DEBUG1,
			(errmsg("PCP: informing node count"),
			 errdetail("%d node(s) found", node_count)));
}
static void
process_reload_config(PCP_CONNECTION * frontend, char scope)
{
	char            code[] = "CommandComplete";
	int wsize;

	if (scope == 'c' && pool_config->use_watchdog)
	{
		ereport(LOG,
				(errmsg("PCP: sending command to watchdog to reload config cluster")));

		if (wd_execute_cluster_command(WD_COMMAND_RELOAD_CONFIG_CLUSTER, NULL) != COMMAND_OK)
			ereport(ERROR,
					(errmsg("PCP: error while processing reload config request for cluster"),
					 errdetail("failed to propagate reload config command through watchdog")));
	}

	if(pool_signal_parent(SIGHUP) == -1)
	{
	   ereport(ERROR,
			   (errmsg("process reload config request failed"),
				errdetail("failed to signal pgpool parent process")));
	}

	pcp_write(frontend, "z", 1);
	wsize = htonl(sizeof(code) + sizeof(int));
	pcp_write(frontend, &wsize, sizeof(int));
	pcp_write(frontend, code, sizeof(code));
	do_pcp_flush(frontend);
}

static void
process_detach_node(PCP_CONNECTION * frontend, char *buf, char tos)
{
	int			node_id;
	int			wsize;
	char		code[] = "CommandComplete";
	bool		gracefully;

	if (tos == 'D')
		gracefully = false;
	else
		gracefully = true;

	node_id = atoi(buf);
	ereport(DEBUG1,
			(errmsg("PCP: processing detach node"),
			 errdetail("detaching Node ID %d", node_id)));

	pool_detach_node(node_id, gracefully, false);

	pcp_write(frontend, "d", 1);
	wsize = htonl(sizeof(code) + sizeof(int));
	pcp_write(frontend, &wsize, sizeof(int));
	pcp_write(frontend, code, sizeof(code));
	do_pcp_flush(frontend);
}

static void
process_attach_node(PCP_CONNECTION * frontend, char *buf)
{
	int			node_id;
	int			wsize;
	char		code[] = "CommandComplete";

	node_id = atoi(buf);
	ereport(DEBUG1,
			(errmsg("PCP: processing attach node"),
			 errdetail("attaching Node ID %d", node_id)));

	send_failback_request(node_id, true, REQ_DETAIL_CONFIRMED);

	pcp_write(frontend, "c", 1);
	wsize = htonl(sizeof(code) + sizeof(int));
	pcp_write(frontend, &wsize, sizeof(int));
	pcp_write(frontend, code, sizeof(code));
	do_pcp_flush(frontend);
}


static void
process_recovery_request(PCP_CONNECTION * frontend, char *buf)
{
	int			wsize;
	char		code[] = "CommandComplete";
	int			node_id = atoi(buf);

	if ((node_id < 0) || (node_id >= pool_config->backend_desc->num_backends))
		ereport(ERROR,
				(errmsg("process recovery request failed"),
				 errdetail("node id %d is not valid", node_id)));

	if ((!REPLICATION &&
		 !(STREAM)) ||
		(STREAM &&
		 node_id == PRIMARY_NODE_ID))
	{
		if (STREAM)
			ereport(ERROR,
					(errmsg("process recovery request failed"),
					 errdetail("primary server cannot be recovered by online recovery.")));
		else
			ereport(ERROR,
					(errmsg("process recovery request failed"),
					 errdetail("recovery request is only allowed in replication and streaming replication modes.")));
	}
	else
	{
		if (pcp_mark_recovery_in_progress() == false)
			ereport(FATAL,
					(errmsg("process recovery request failed"),
					 errdetail("pgpool-II is already processing another recovery request.")));

		ereport(DEBUG1,
				(errmsg("PCP: processing recovery request"),
				 errdetail("start online recovery")));

		PG_TRY();
		{
			start_recovery(node_id);
			finish_recovery();
			pcp_write(frontend, "c", 1);
			wsize = htonl(sizeof(code) + sizeof(int));
			pcp_write(frontend, &wsize, sizeof(int));
			pcp_write(frontend, code, sizeof(code));
			do_pcp_flush(frontend);
			pcp_mark_recovery_finished();
		}
		PG_CATCH();
		{
			finish_recovery();
			pcp_mark_recovery_finished();
			PG_RE_THROW();

		} PG_END_TRY();
	}
	do_pcp_flush(frontend);
}

static void
process_status_request(PCP_CONNECTION * frontend)
{
	int			nrows = 0;
	int			i;
	POOL_REPORT_CONFIG *status = get_config(&nrows);
	int			len = 0;

	/* First, send array size of connection_info */
	char		arr_code[] = "ArraySize";
	char		code[] = "ProcessConfig";

	/* Finally, indicate that all data is sent */
	char		fin_code[] = "CommandComplete";

	pcp_write(frontend, "b", 1);
	len = htonl(sizeof(arr_code) + sizeof(int) + sizeof(int));
	pcp_write(frontend, &len, sizeof(int));
	pcp_write(frontend, arr_code, sizeof(arr_code));
	len = htonl(nrows);
	pcp_write(frontend, &len, sizeof(int));

	do_pcp_flush(frontend);

	for (i = 0; i < nrows; i++)
	{
		pcp_write(frontend, "b", 1);
		len = htonl(sizeof(int)
					+ sizeof(code)
					+ strlen(status[i].name) + 1
					+ strlen(status[i].value) + 1
					+ strlen(status[i].desc) + 1
			);

		pcp_write(frontend, &len, sizeof(int));
		pcp_write(frontend, code, sizeof(code));
		pcp_write(frontend, status[i].name, strlen(status[i].name) + 1);
		pcp_write(frontend, status[i].value, strlen(status[i].value) + 1);
		pcp_write(frontend, status[i].desc, strlen(status[i].desc) + 1);
	}

	pcp_write(frontend, "b", 1);
	len = htonl(sizeof(fin_code) + sizeof(int));
	pcp_write(frontend, &len, sizeof(int));
	pcp_write(frontend, fin_code, sizeof(fin_code));
	do_pcp_flush(frontend);

	pfree(status);
	ereport(DEBUG1,
			(errmsg("PCP: processing status request"),
			 errdetail("retrieved status information")));
}

/*
 * Process promote node request.  This function is tricky. If promote option
 * is sent from client, calls pool_detach_node() so that failover script
 * actually promote the specified node and detach current primary.
 */
static void
process_promote_node(PCP_CONNECTION * frontend, char *buf, char tos)
{
	int			node_id;
	int			wsize;
	char		code[] = "CommandComplete";
	bool		gracefully;
	char		node_id_buf[64];
	char		*p;
	char		promote_option;

	if (tos == 'J')
		gracefully = false;
	else
		gracefully = true;

	p = node_id_buf;
	while (*buf && *buf != ' ')
		*p++ = *buf++;
	*p = '\0';
	buf += 2;
	promote_option = *buf;

	node_id = atoi(node_id_buf);

	if ((node_id < 0) || (node_id >= pool_config->backend_desc->num_backends))
		ereport(ERROR,
				(errmsg("could not process promote request"),
				 errdetail("node id %d is not valid", node_id)));

	/* Promoting node is only possible in Streaming Replication mode */
	if (!STREAM)
	{
		ereport(FATAL,
				(errmsg("invalid pgpool mode for process promote request"),
				 errdetail("not in streaming replication mode, can't promote node id %d", node_id)));

	}

	if (node_id == REAL_PRIMARY_NODE_ID)
	{
		ereport(FATAL,
				(errmsg("invalid promote request"),
				 errdetail("specified node is already primary node, can't promote node id %d", node_id)));

	}

	ereport(LOG, (errmsg("pcp_promote_node: promote option: %c", promote_option)));

	if (promote_option == 's')
	{
		ereport(DEBUG1,
			(errmsg("PCP: processing promote node"),
			 errdetail("promoting Node ID %d and shutdown primary node %d", node_id, REAL_PRIMARY_NODE_ID)));
		pool_detach_node(node_id, gracefully, true);
	}
	else
	{
		ereport(DEBUG1,
				(errmsg("PCP: processing promote node"),
				 errdetail("promoting Node ID %d", node_id)));
		pool_promote_node(node_id, gracefully);
	}

	pcp_write(frontend, "d", 1);
	wsize = htonl(sizeof(code) + sizeof(int));
	pcp_write(frontend, &wsize, sizeof(int));
	pcp_write(frontend, code, sizeof(code));
	do_pcp_flush(frontend);
}

static void
process_authentication(PCP_CONNECTION * frontend, char *buf, char *salt, int *random_salt)
{
	int			wsize;
	int			authenticated;

	if (*random_salt)
	{
		authenticated = user_authenticate(buf, pcp_conf_file, salt, 4);
	}
	if (!*random_salt || !authenticated)
	{
		ereport(FATAL,
				(errmsg("authentication failed"),
				 errdetail("username and/or password does not match")));

		*random_salt = 0;
	}
	else
	{
		char		code[] = "AuthenticationOK";

		pcp_write(frontend, "r", 1);
		wsize = htonl(sizeof(code) + sizeof(int));
		pcp_write(frontend, &wsize, sizeof(int));
		pcp_write(frontend, code, sizeof(code));
		do_pcp_flush(frontend);
		*random_salt = 0;

		ereport(DEBUG1,
				(errmsg("PCP: processing authentication request"),
				 errdetail("authentication OK")));
	}
}

static void
send_md5salt(PCP_CONNECTION * frontend, char *salt)
{
	int			wsize;

	ereport(DEBUG1,
			(errmsg("PCP: sending md5 salt to client")));

	pool_random_salt(salt);

	pcp_write(frontend, "m", 1);
	wsize = htonl(sizeof(int) + 4);
	pcp_write(frontend, &wsize, sizeof(int));
	pcp_write(frontend, salt, 4);
	do_pcp_flush(frontend);
}

static void
process_shutdown_request(PCP_CONNECTION * frontend, char mode, char tos)
{
	char		code[] = "CommandComplete";
	int			len;

	ereport(DEBUG1,
			(errmsg("PCP: processing shutdown request"),
			 errdetail("shutdown mode \"%c\"", mode)));

	/* quickly bail out if invalid mode is specified
	 * because we do not want to propagate the command
	 * with invalid mode over the watchdog network */
	if (mode != 's' && mode != 'i' && mode != 'f' )
	{
		ereport(ERROR,
				(errmsg("PCP: error while processing shutdown request"),
				 errdetail("invalid shutdown mode \"%c\"", mode)));
	}

	if (tos == 't' && pool_config->use_watchdog)
	{
		WDExecCommandArg wdExecCommandArg;
		List *args_list = NULL;

		strncpy(wdExecCommandArg.arg_name, "mode", sizeof(wdExecCommandArg.arg_name) - 1);
		snprintf(wdExecCommandArg.arg_value, sizeof(wdExecCommandArg.arg_value) - 1, "%c",mode);
		args_list = lappend(args_list,&wdExecCommandArg);

		ereport(LOG,
				(errmsg("PCP: sending command to watchdog to shutdown cluster")));

		if (wd_execute_cluster_command(WD_COMMAND_SHUTDOWN_CLUSTER, args_list) != COMMAND_OK)
			ereport(ERROR,
					(errmsg("PCP: error while processing shutdown cluster request"),
					 errdetail("failed to propagate shutdown command through watchdog")));

		list_free(args_list);
	}

	pcp_write(frontend, "t", 1);
	len = htonl(sizeof(code) + sizeof(int));
	pcp_write(frontend, &len, sizeof(int));
	pcp_write(frontend, code, sizeof(code));
	do_pcp_flush(frontend);
	terminate_pgpool(mode, true);
}

static void
process_set_configuration_parameter(PCP_CONNECTION * frontend, char *buf, int len)
{
	char	   *param_name;
	char	   *param_value;
	int			wsize;
	char		code[] = "CommandComplete";

	param_name = buf;
	if (param_name == NULL)
		ereport(ERROR,
				(errmsg("PCP: set configuration parameter failed"),
				 errdetail("invalid pcp packet received from client")));

	param_value = (char *) memchr(buf, '\0', len);
	if (param_value == NULL)
		ereport(ERROR,
				(errmsg("set configuration parameter failed"),
				 errdetail("invalid pcp packet received from client")));

	param_value += 1;
	ereport(LOG,
			(errmsg("set configuration parameter, \"%s TO %s\"", param_name, param_value)));

	if (strcasecmp(param_name, "client_min_messages") == 0)
	{
		const char *ordered_valid_values[] = {"debug5", "debug4", "debug3", "debug2", "debug1", "log", "commerror", "info", "notice", "warning", "error", NULL};
		bool		found = false;
		int			i;

		for (i = 0;; i++)
		{
			char	   *valid_val = (char *) ordered_valid_values[i];

			if (!valid_val)
				break;

			if (!strcasecmp(param_value, valid_val))
			{
				found = true;
				pool_config->client_min_messages = i + 10;
				ereport(DEBUG1,
						(errmsg("PCP setting parameter \"%s\" to \"%s\"", param_name, param_value)));
				break;
			}
		}
		if (!found)
			ereport(ERROR,
					(errmsg("PCP: set configuration parameter failed"),
					 errdetail("invalid value \"%s\" for parameter \"%s\"", param_value, param_name)));
	}
	else
		ereport(ERROR,
				(errmsg("PCP: set configuration parameter failed"),
				 errdetail("invalid parameter \"%s\"", param_name)));

	pcp_write(frontend, "a", 1);
	wsize = htonl(sizeof(code) + sizeof(int));
	pcp_write(frontend, &wsize, sizeof(int));
	pcp_write(frontend, code, sizeof(code));
	do_pcp_flush(frontend);
}

/*
 * Wrapper around pcp_flush which throws FATAL error when pcp_flush fails
 */
static void
do_pcp_flush(PCP_CONNECTION * frontend)
{
	if (pcp_flush(frontend) < 0)
		ereport(FATAL,
				(errmsg("failed to flush data to client"),
				 errdetail("pcp_flush failed with error : \"%m\"")));
}

/*
 * Wrapper around pcp_read which throws FATAL error when read fails
 */
static void
do_pcp_read(PCP_CONNECTION * pc, void *buf, int len)
{
	if (pcp_read(pc, buf, len))
		ereport(FATAL,
				(errmsg("unable to read from client"),
				 errdetail("pcp_read failed with error : \"%m\"")));
}

int
send_to_pcp_frontend(char *data, int len, bool flush)
{
	int			ret;

	if (processType != PT_PCP_WORKER || pcp_frontend == NULL)
		return -1;
	ret = pcp_write(pcp_frontend, data, len);
	if (flush && !ret)
		ret = pcp_flush(pcp_frontend);
	return ret;
}

int
pcp_frontend_exists(void)
{
	if (processType != PT_PCP_WORKER || pcp_frontend == NULL)
		return -1;
	return 0;
}

static void
pcp_worker_will_go_down(int code, Datum arg)
{
	if (processType != PT_PCP_WORKER)
	{
		/* should never happen */
		ereport(WARNING,
				(errmsg("pcp_worker_will_go_down called from invalid process")));
		return;
	}
	if (pcp_frontend)
		pcp_close(pcp_frontend);
	processState = EXITING;
	POOL_SETMASK(&UnBlockSig);

}
