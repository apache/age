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
 * PCP client program to execute pcp commands.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <string.h>

#include "utils/fe_ports.h"
#include "utils/pool_path.h"
#include "pcp/pcp.h"

#ifdef HAVE_GETOPT_H
#include <getopt.h>
#else
#include "utils/getopt_long.h"
#endif

const char *progname = NULL;
const char *get_progname(const char *argv0);
char	   *last_dir_separator(const char *filename);

static void usage(void);
static inline bool app_require_nodeID(void);
static inline bool app_support_cluster_mode(void);
static void output_watchdog_info_result(PCPResultInfo * pcpResInfo, bool verbose);
static void output_procinfo_result(PCPResultInfo * pcpResInfo, bool all, bool verbose);
static void output_proccount_result(PCPResultInfo * pcpResInfo, bool verbose);
static void output_poolstatus_result(PCPResultInfo * pcpResInfo, bool verbose);
static void output_nodeinfo_result(PCPResultInfo * pcpResInfo, bool all,  bool verbose);
static void output_health_check_stats_result(PCPResultInfo * pcpResInfo, bool verbose);
static void output_nodecount_result(PCPResultInfo * pcpResInfo, bool verbose);
static char *backend_status_to_string(BackendInfo * bi);
static char *format_titles(const char **titles, const char **types, int ntitles);

typedef enum
{
	PCP_ATTACH_NODE,
	PCP_DETACH_NODE,
	PCP_NODE_COUNT,
	PCP_NODE_INFO,
	PCP_HEALTH_CHECK_STATS,
	PCP_POOL_STATUS,
	PCP_PROC_COUNT,
	PCP_PROC_INFO,
	PCP_PROMOTE_NODE,
	PCP_RECOVERY_NODE,
	PCP_STOP_PGPOOL,
	PCP_WATCHDOG_INFO,
	PCP_RELOAD_CONFIG,
	UNKNOWN,
}			PCP_UTILITIES;

struct AppTypes
{
	const char *app_name;
	PCP_UTILITIES app_type;
	const char *allowed_options;
	const char *description;
};

struct AppTypes AllAppTypes[] =
{
	{"pcp_attach_node", PCP_ATTACH_NODE, "n:h:p:U:wWvd", "attach a node from pgpool-II"},
	{"pcp_detach_node", PCP_DETACH_NODE, "n:h:p:U:gwWvd", "detach a node from pgpool-II"},
	{"pcp_node_count", PCP_NODE_COUNT, "h:p:U:wWvd", "display the total number of nodes under pgpool-II's control"},
	{"pcp_node_info", PCP_NODE_INFO, "n:h:p:U:awWvd", "display a pgpool-II node's information"},
	{"pcp_health_check_stats", PCP_HEALTH_CHECK_STATS, "n:h:p:U:wWvd", "display a pgpool-II health check stats data"},
	{"pcp_pool_status", PCP_POOL_STATUS, "h:p:U:wWvd", "display pgpool configuration and status"},
	{"pcp_proc_count", PCP_PROC_COUNT, "h:p:U:wWvd", "display the list of pgpool-II child process PIDs"},
	{"pcp_proc_info", PCP_PROC_INFO, "h:p:P:U:awWvd", "display a pgpool-II child process' information"},
	{"pcp_promote_node", PCP_PROMOTE_NODE, "n:h:p:U:gswWvd", "promote a node as new main from pgpool-II"},
	{"pcp_recovery_node", PCP_RECOVERY_NODE, "n:h:p:U:wWvd", "recover a node"},
	{"pcp_stop_pgpool", PCP_STOP_PGPOOL, "m:h:p:U:s:wWvda", "terminate pgpool-II"},
	{"pcp_watchdog_info", PCP_WATCHDOG_INFO, "n:h:p:U:wWvd", "display a pgpool-II watchdog's information"},
	{"pcp_reload_config",PCP_RELOAD_CONFIG,"h:p:U:s:wWvd", "reload a pgpool-II config file"},
	{NULL, UNKNOWN, NULL, NULL},
};
struct AppTypes *current_app_type;

int
main(int argc, char **argv)
{
	char	   *host = NULL;
	int			port = 9898;
	char	   *user = NULL;
	char	   *pass = NULL;
	int			nodeID = -1;
	int			processID = 0;
	int			ch;
	char		shutdown_mode = 's';
	char		command_scope = 'l';
	int			optindex;
	int			i;
	bool		all = false;
	bool		debug = false;
	bool		need_password = true;
	bool		gracefully = false;
	bool		switchover = false;
	bool		verbose = false;
	PCPConnInfo *pcpConn;
	PCPResultInfo *pcpResInfo;

	/* here we put all the allowed long options for all utilities */
	static struct option long_options[] = {
		{"help", no_argument, NULL, '?'},
		{"debug", no_argument, NULL, 'd'},
		{"version", no_argument, NULL, 'V'},
		{"host", required_argument, NULL, 'h'},
		{"port", required_argument, NULL, 'p'},
		{"process-id", required_argument, NULL, 'P'},
		{"username", required_argument, NULL, 'U'},
		{"no-password", no_argument, NULL, 'w'},
		{"password", no_argument, NULL, 'W'},
		{"mode", required_argument, NULL, 'm'},
		{"scope", required_argument, NULL, 's'},
		{"gracefully", no_argument, NULL, 'g'},
		{"switchover", no_argument, NULL, 's'},
		{"verbose", no_argument, NULL, 'v'},
		{"all", no_argument, NULL, 'a'},
		{"node-id", required_argument, NULL, 'n'},
		{"watchdog-id", required_argument, NULL, 'n'},
		{NULL, 0, NULL, 0}
	};

	/* Identify the utility app */
	progname = get_progname(argv[0]);
	for (i = 0;; i++)
	{
		current_app_type = &AllAppTypes[i];
		if (current_app_type->app_type == UNKNOWN)
			break;
		if (strcmp(current_app_type->app_name, progname) == 0)
			break;
	}

	if (current_app_type->app_type == UNKNOWN)
	{
		fprintf(stderr, "%s is a invalid PCP utility\n", progname);
		exit(1);
	}

	if (argc > 1)
	{
		if (strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-?") == 0)
		{
			usage();
			exit(0);
		}
		else if (strcmp(argv[1], "-V") == 0
				 || strcmp(argv[1], "--version") == 0)
		{
			fprintf(stderr, "%s (%s) %s\n", progname, PACKAGE, VERSION);
			exit(0);
		}
	}

	while ((ch = getopt_long(argc, argv, current_app_type->allowed_options, long_options, &optindex)) != -1)
	{
		switch (ch)
		{
			case 'd':
				debug = true;
				break;

			case 'a':
				all = true;
				break;

			case 'w':
				need_password = false;
				break;

			case 'W':
				need_password = true;
				break;

			case 'g':
				gracefully = true;
				break;

			case 's':
				if (app_support_cluster_mode())
				{
					if (strcmp(optarg, "c") == 0 || strcmp(optarg, "cluster") == 0)
						command_scope = 'c';
					else if (strcmp(optarg, "l") == 0 || strcmp(optarg, "local") == 0)
						command_scope = 'l';
					else
					{
						fprintf(stderr, "%s: Invalid command scope \"%s\", must be either \"cluster\" or \"local\" \n", progname, optarg);
						exit(1);
					}
				}
				else
				{
					if (current_app_type->app_type == PCP_PROMOTE_NODE)
					{
						switchover = true;
					}
					else
					{
						fprintf(stderr, "Invalid argument \"%s\", Try \"%s --help\" for more information.\n", optarg, progname);
						exit(1);
					}
				}
				break;

			case 'm':
				if (current_app_type->app_type == PCP_STOP_PGPOOL)
				{
					if (strcmp(optarg, "s") == 0 || strcmp(optarg, "smart") == 0)
						shutdown_mode = 's';
					else if (strcmp(optarg, "f") == 0 || strcmp(optarg, "fast") == 0)
						shutdown_mode = 'f';
					else if (strcmp(optarg, "i") == 0 || strcmp(optarg, "immediate") == 0)
						shutdown_mode = 'i';
					else
					{
						fprintf(stderr, "%s: Invalid shutdown mode \"%s\", must be either \"smart\" \"immediate\" or \"fast\" \n", progname, optarg);
						exit(1);
					}
				}
				else
				{
					fprintf(stderr, "Invalid argument \"%s\", Try \"%s --help\" for more information.\n", optarg, progname);
					exit(1);
				}
				break;

			case 'v':
				verbose = true;
				break;

			case 'n':
				nodeID = atoi(optarg);
				if (current_app_type->app_type == PCP_WATCHDOG_INFO)
				{
					if (nodeID < 0)
					{
						fprintf(stderr, "%s: Invalid watchdog-id \"%s\", must be a positive number or zero for a local watchdog node\n", progname, optarg);
						exit(0);
					}
				}
				else
				{
					if (nodeID < 0 || nodeID > MAX_NUM_BACKENDS)
					{
						fprintf(stderr, "%s: Invalid node-id \"%s\", must be between 0 and %d\n", progname, optarg, MAX_NUM_BACKENDS);
						exit(0);
					}
				}
				break;

			case 'p':
				port = atoi(optarg);
				if (port <= 1024 || port > 65535)
				{
					fprintf(stderr, "%s: Invalid port number \"%s\", must be between 1024 and 65535\n", progname, optarg);
					exit(0);
				}
				break;

			case 'P':			/* PID */
				processID = atoi(optarg);
				if (processID <= 0)
				{
					fprintf(stderr, "%s: Invalid process-id \"%s\", must be greater than 0\n", progname, optarg);
					exit(0);
				}
				break;

			case 'h':
				host = strdup(optarg);
				break;

			case 'U':
				user = strdup(optarg);
				break;

			case '?':
			default:

				/*
				 * getopt_long should already have emitted a complaint
				 */
				fprintf(stderr, "Try \"%s --help\" for more information.\n\n", progname);
				exit(1);
		}
	}

	/*
	 * if we still have arguments, use it
	 */
	while (argc - optind >= 1)
	{
		if (current_app_type->app_type == PCP_PROC_INFO && processID <= 0)
		{
			processID = atoi(argv[optind]);
			if (processID <= 0)
			{
				fprintf(stderr, "%s: Invalid process-id \"%s\", must be greater than 0\n", progname, optarg);
				exit(0);
			}
		}
		else if (current_app_type->app_type == PCP_WATCHDOG_INFO && nodeID < 0)
		{
			nodeID = atoi(argv[optind]);
			if (nodeID < 0)
			{
				fprintf(stderr, "%s: Invalid watchdog-id \"%s\", must be a positive number or zero for local watchdog node\n", progname, optarg);
				exit(0);
			}
		}
		else if ((app_require_nodeID() || current_app_type->app_type == PCP_NODE_INFO) && nodeID < 0)
		{
			nodeID = atoi(argv[optind]);
			if (nodeID < 0 || nodeID > MAX_NUM_BACKENDS)
			{
				fprintf(stderr, "%s: Invalid node-id \"%s\", must be between 0 and %d\n", progname, optarg, MAX_NUM_BACKENDS);
				exit(0);
			}
		}
		else
			fprintf(stderr, "%s: Warning: extra command-line argument \"%s\" ignored\n",
					progname, argv[optind]);

		optind++;
	}

	if (nodeID < 0)
	{
		if (app_require_nodeID())
		{
			fprintf(stderr, "%s: missing node-id\n", progname);
			fprintf(stderr, "Try \"%s --help\" for more information.\n\n", progname);
			exit(1);
		}
		else if (current_app_type->app_type == PCP_WATCHDOG_INFO || current_app_type->app_type == PCP_NODE_INFO)
		{
			nodeID = -1;
		}
	}

	/* Get a new password if appropriate */
	if (need_password)
	{
		pass = simple_prompt("Password: ", 100, false);
		need_password = false;
	}

	pcpConn = pcp_connect(host, port, user, pass, debug ? stdout : NULL);
	if (PCPConnectionStatus(pcpConn) != PCP_CONNECTION_OK)
	{
		fprintf(stderr, "%s\n", pcp_get_last_error(pcpConn) ? pcp_get_last_error(pcpConn) : "Unknown Error");
		exit(1);
	}

	/*
	 * Okay the connection is successful not call the actual PCP function
	 */
	if (current_app_type->app_type == PCP_ATTACH_NODE)
	{
		pcpResInfo = pcp_attach_node(pcpConn, nodeID);
	}

	else if (current_app_type->app_type == PCP_DETACH_NODE)
	{
		if (gracefully)
			pcpResInfo = pcp_detach_node_gracefully(pcpConn, nodeID);
		else
			pcpResInfo = pcp_detach_node(pcpConn, nodeID);
	}

	else if (current_app_type->app_type == PCP_NODE_COUNT)
	{
		pcpResInfo = pcp_node_count(pcpConn);
	}

	else if (current_app_type->app_type == PCP_NODE_INFO)
	{
		pcpResInfo = pcp_node_info(pcpConn, nodeID);
	}

	else if (current_app_type->app_type == PCP_HEALTH_CHECK_STATS)
	{
		pcpResInfo = pcp_health_check_stats(pcpConn, nodeID);
	}

	else if (current_app_type->app_type == PCP_POOL_STATUS)
	{
		pcpResInfo = pcp_pool_status(pcpConn);
	}

	else if (current_app_type->app_type == PCP_PROC_COUNT)
	{
		pcpResInfo = pcp_process_count(pcpConn);
	}

	else if (current_app_type->app_type == PCP_PROC_INFO)
	{
		pcpResInfo = pcp_process_info(pcpConn, processID);
	}

	else if (current_app_type->app_type == PCP_PROMOTE_NODE)
	{
		if (gracefully)
			pcpResInfo = pcp_promote_node_gracefully(pcpConn, nodeID, switchover);
		else
			pcpResInfo = pcp_promote_node(pcpConn, nodeID, switchover);
	}

	else if (current_app_type->app_type == PCP_RECOVERY_NODE)
	{
		pcpResInfo = pcp_recovery_node(pcpConn, nodeID);
	}

	else if (current_app_type->app_type == PCP_STOP_PGPOOL)
	{
		pcpResInfo = pcp_terminate_pgpool(pcpConn, shutdown_mode, command_scope);
	}

	else if (current_app_type->app_type == PCP_WATCHDOG_INFO)
	{
		pcpResInfo = pcp_watchdog_info(pcpConn, nodeID);
	}

	else if (current_app_type->app_type == PCP_RELOAD_CONFIG)
	{
		pcpResInfo = pcp_reload_config(pcpConn,command_scope);
	}

	else
	{
		/* should never happen */
		fprintf(stderr, "%s: Invalid pcp process\n", progname);
		goto DISCONNECT_AND_EXIT;
	}

	if (pcpResInfo == NULL || PCPResultStatus(pcpResInfo) != PCP_RES_COMMAND_OK)
	{
		fprintf(stderr, "%s\n", pcp_get_last_error(pcpConn) ? pcp_get_last_error(pcpConn) : "Unknown Error");
		goto DISCONNECT_AND_EXIT;
	}

	if (pcp_result_is_empty(pcpResInfo))
	{
		fprintf(stdout, "%s -- Command Successful\n", progname);
	}
	else
	{
		if (current_app_type->app_type == PCP_NODE_COUNT)
			output_nodecount_result(pcpResInfo, verbose);

		if (current_app_type->app_type == PCP_NODE_INFO)
			output_nodeinfo_result(pcpResInfo, all, verbose);

		if (current_app_type->app_type == PCP_HEALTH_CHECK_STATS)
			output_health_check_stats_result(pcpResInfo, verbose);

		if (current_app_type->app_type == PCP_POOL_STATUS)
			output_poolstatus_result(pcpResInfo, verbose);

		if (current_app_type->app_type == PCP_PROC_COUNT)
			output_proccount_result(pcpResInfo, verbose);

		if (current_app_type->app_type == PCP_PROC_INFO)
			output_procinfo_result(pcpResInfo, all, verbose);

		else if (current_app_type->app_type == PCP_WATCHDOG_INFO)
			output_watchdog_info_result(pcpResInfo, verbose);
	}

DISCONNECT_AND_EXIT:

	pcp_disconnect(pcpConn);
	pcp_free_connection(pcpConn);

	return 0;
}

static void
output_nodecount_result(PCPResultInfo * pcpResInfo, bool verbose)
{
	if (verbose)
	{
		printf("Node Count\n");
		printf("____________\n");
		printf(" %d\n", pcp_get_int_data(pcpResInfo, 0));
	}
	else
		printf("%d\n", pcp_get_int_data(pcpResInfo, 0));
}

static void
output_nodeinfo_result(PCPResultInfo * pcpResInfo, bool all, bool verbose)
{
	bool		printed = false;
	int			i;
	char		last_status_change[20];
	struct tm	tm;
	char	   *frmt;
	int         array_size = pcp_result_slot_count(pcpResInfo);
	char		standby_delay_str[64];

	if (verbose)
	{
		const char *titles[] = {"Hostname", "Port", "Status", "Weight", "Status Name", "Backend Status Name", "Role", "Backend Role", "Replication Delay", "Replication State", "Replication Sync State", "Last Status Change"};
		const char *types[] = {"s", "d", "d", "f", "s", "s", "s", "s", "s", "s", "s", "s"};

		frmt = format_titles(titles, types, sizeof(titles)/sizeof(char *));
	}
	else
	{
		frmt = "%s %d %d %f %s %s %s %s %s %s %s %s\n";
	}

	for (i = 0; i < array_size; i++)
	{

		BackendInfo *backend_info = (BackendInfo *) pcp_get_binary_data(pcpResInfo, i);

		if (backend_info == NULL)
			break;
		if ((!all) && (backend_info->backend_hostname[0] == '\0'))
			continue;

		printed = true;
		localtime_r(&backend_info->status_changed_time, &tm);
		strftime(last_status_change, sizeof(last_status_change), "%F %T", &tm);

		if (backend_info->standby_delay_by_time)
		{
			snprintf(standby_delay_str, sizeof(standby_delay_str), "%.6f", ((float)backend_info->standby_delay)/1000000);
			if (verbose)
			{
				if (backend_info->standby_delay >= 2*1000*1000)
					strcat(standby_delay_str, " seconds");
				else
					strcat(standby_delay_str, " second");
			}
		}
		else
		{
			snprintf(standby_delay_str, sizeof(standby_delay_str), "%lu", backend_info->standby_delay);
		}

		printf(frmt,
			   backend_info->backend_hostname,
			   backend_info->backend_port,
			   backend_info->backend_status,
			   backend_info->backend_weight / RAND_MAX,
			   backend_status_to_string(backend_info),
			   backend_info->pg_backend_status,
			   role_to_str(backend_info->role),
			   backend_info->pg_role,
			   standby_delay_str,
			   backend_info->replication_state[0] == '\0' ? "none" : backend_info->replication_state,
			   backend_info->replication_sync_state[0] == '\0' ? "none" : backend_info->replication_sync_state,
			   last_status_change);
	}

	if (printed == false)
		printf("No node information available\n\n");
}

/*
 * Format and output health check stats
 */
static void
output_health_check_stats_result(PCPResultInfo * pcpResInfo, bool verbose)
{
	POOL_HEALTH_CHECK_STATS *stats = (POOL_HEALTH_CHECK_STATS *)pcp_get_binary_data(pcpResInfo, 0);

	if (verbose)
	{
		const char *titles[] = {"Node Id", "Host Name", "Port", "Status", "Role", "Last Status Change",
								"Total Count", "Success Count", "Fail Count", "Skip Count", "Retry Count",
								"Average Retry Count", "Max Retry Count", "Max Health Check Duration",
								"Minimum Health Check Duration", "Average Health Check Duration",
								"Last Health Check", "Last Successful Health Check",
								"Last Skip Health Check", "Last Failed Health Check"};
		const char *types[] = {"s", "s", "s", "s", "s", "s", "s", "s", "s", "s",
							   "s", "s", "s", "s", "s", "s", "s", "s", "s", "s"};
		char *format_string;

		format_string = format_titles(titles, types, sizeof(titles)/sizeof(char *));
		printf(format_string,
			   stats->node_id,
			   stats->hostname,
			   stats->port,
			   stats->status,
			   stats->role,
			   stats->last_status_change,
			   stats->total_count,
			   stats->success_count,
			   stats->fail_count,
			   stats->skip_count,
			   stats->retry_count,
			   stats->average_retry_count,
			   stats->max_retry_count,
			   stats->max_health_check_duration,
			   stats->min_health_check_duration,
			   stats->average_health_check_duration,
			   stats->last_health_check,
			   stats->last_successful_health_check,
			   stats->last_skip_health_check,
			   stats->last_failed_health_check);
	}
	else
	{
		printf("%s %s %s %s %s %s %s %s %s %s %s %s %s %s %s %s %s %s %s %s\n",
			   stats->node_id,
			   stats->hostname,
			   stats->port,
			   stats->status,
			   stats->role,
			   stats->last_status_change,
			   stats->total_count,
			   stats->success_count,
			   stats->fail_count,
			   stats->skip_count,
			   stats->retry_count,
			   stats->average_retry_count,
			   stats->max_retry_count,
			   stats->max_health_check_duration,
			   stats->min_health_check_duration,
			   stats->average_health_check_duration,
			   stats->last_health_check,
			   stats->last_successful_health_check,
			   stats->last_skip_health_check,
			   stats->last_failed_health_check);
	}
}

static void
output_poolstatus_result(PCPResultInfo * pcpResInfo, bool verbose)
{
	POOL_REPORT_CONFIG *status;
	int			i;
	int			array_size = pcp_result_slot_count(pcpResInfo);

	if (verbose)
	{
		for (i = 0; i < array_size; i++)
		{
			status = (POOL_REPORT_CONFIG *) pcp_get_binary_data(pcpResInfo, i);
			printf("Name [%3d]:\t%s\n", i, status ? status->name : "NULL");
			printf("Value:      \t%s\n", status ? status->value : "NULL");
			printf("Description:\t%s\n\n", status ? status->desc : "NULL");
		}
	}
	else
	{
		for (i = 0; i < array_size; i++)
		{
			status = (POOL_REPORT_CONFIG *) pcp_get_binary_data(pcpResInfo, i);
			if (status == NULL)
			{
				printf("****Data at %d slot is NULL\n", i);
				continue;
			}
			printf("name : %s\nvalue: %s\ndesc : %s\n\n", status->name, status->value, status->desc);
		}
	}
}

static void
output_proccount_result(PCPResultInfo * pcpResInfo, bool verbose)
{
	int			i;
	int			process_count = pcp_get_data_length(pcpResInfo, 0) / sizeof(int);
	int		   *process_list = (int *) pcp_get_binary_data(pcpResInfo, 0);

	if (verbose)
	{
		printf("No \t | \t PID\n");
		printf("_____________________\n");
		for (i = 0; i < process_count; i++)
			printf("%d \t | \t %d\n", i, process_list[i]);
		printf("\nTotal Processes:%d\n", process_count);
	}
	else
	{
		for (i = 0; i < process_count; i++)
			printf("%d ", process_list[i]);
		printf("\n");
	}
}

static void
output_procinfo_result(PCPResultInfo * pcpResInfo, bool all, bool verbose)
{
	bool		printed = false;
	int			i;
	char	   *format;
	int			array_size = pcp_result_slot_count(pcpResInfo);

	const char *titles[] = {
		"Database", "Username", "Start time", "Client connection count",
		"Major", "Minor", "Backend connection time", "Client connection time",
		"Client idle duration", "Client disconnection time", "Pool Counter", "Backend PID",
		"Connected", "PID", "Backend ID", "Status"
	};
	const char *types[] = {
		"s", "s", "s", "s",
		"s", "s", "s", "s",
		"s", "s", "s", "s",
		"s", "s", "s", "s"
	};


	if (verbose)
		format = format_titles(titles, types, sizeof(titles)/sizeof(char *));
	else
	{
		format = "%s %s %s %s %s %s %s %s %s %s %s %s %s %s %s %s\n";
	}

	for (i = 0; i < array_size; i++)
	{

		POOL_REPORT_POOLS *pools = (POOL_REPORT_POOLS *) pcp_get_binary_data(pcpResInfo, i);

		if (pools == NULL)
			break;
		if (((!all) && (pools->database[0] == '\0')) || (pools->pool_pid[0] == '\0'))
			continue;
		printed = true;
		printf(format,
			   pools->database,
			   pools->username,
			   pools->process_start_time,
			   pools->client_connection_count,
			   pools->pool_majorversion,
			   pools->pool_minorversion,
			   pools->backend_connection_time,
			   pools->client_connection_time,
			   pools->client_idle_duration,
			   pools->client_disconnection_time,
			   pools->pool_counter,
			   pools->pool_backendpid,
			   pools->pool_connected,
			   pools->pool_pid,
			   pools->backend_id,
			   pools->status);
	}
	if (printed == false)
		printf("No process information available\n\n");
}

static void
output_watchdog_info_result(PCPResultInfo * pcpResInfo, bool verbose)
{
	int			i;
	PCPWDClusterInfo *cluster = (PCPWDClusterInfo *) pcp_get_binary_data(pcpResInfo, 0);

	if (verbose)
	{
		char	   *quorumStatus;

		if (cluster->quorumStatus == 0)
			quorumStatus = "QUORUM IS ON THE EDGE";
		else if (cluster->quorumStatus == 1)
			quorumStatus = "QUORUM EXIST";
		else if (cluster->quorumStatus == -1)
			quorumStatus = "QUORUM ABSENT";
		else if (cluster->quorumStatus == -2)
			quorumStatus = "NO LEADER NODE";
		else
			quorumStatus = "UNKNOWN";

		printf("Watchdog Cluster Information \n");
		printf("Total Nodes              : %d\n", cluster->remoteNodeCount + 1);
		printf("Remote Nodes             : %d\n", cluster->remoteNodeCount);
		printf("Member Remote Nodes      : %d\n", cluster->memberRemoteNodeCount);
		printf("Alive Remote Nodes       : %d\n", cluster->aliveNodeCount);
		printf("Nodes required for quorum: %d\n", cluster->nodesRequiredForQuorum);
		printf("Quorum state             : %s\n", quorumStatus);
		printf("Local node escalation    : %s\n", cluster->escalated ? "YES" : "NO");
		printf("Leader Node Name         : %s\n", cluster->leaderNodeName);
		printf("Leader Host Name         : %s\n\n", cluster->leaderHostName);

		printf("Watchdog Node Information \n");
		for (i = 0; i < cluster->nodeCount; i++)
		{
			PCPWDNodeInfo *watchdog_info = &cluster->nodeList[i];

			printf("Node Name         : %s\n", watchdog_info->nodeName);
			printf("Host Name         : %s\n", watchdog_info->hostName);
			printf("Delegate IP       : %s\n", watchdog_info->delegate_ip);
			printf("Pgpool port       : %d\n", watchdog_info->pgpool_port);
			printf("Watchdog port     : %d\n", watchdog_info->wd_port);
			printf("Node priority     : %d\n", watchdog_info->wd_priority);
			printf("Status            : %d\n", watchdog_info->state);
			printf("Status Name       : %s\n", watchdog_info->stateName);
			printf("Membership Status : %s\n\n", watchdog_info->membership_status_string);
		}
	}
	else
	{
		printf("%d %d %s %s %s\n\n",
			   cluster->remoteNodeCount + 1,
			   cluster->memberRemoteNodeCount + 1,
			   cluster->escalated ? "YES" : "NO",
			   cluster->leaderNodeName,
			   cluster->leaderHostName);

		for (i = 0; i < cluster->nodeCount; i++)
		{
			PCPWDNodeInfo *watchdog_info = &cluster->nodeList[i];

			printf("%s %s %d %d %d %s %d %s\n",
				   watchdog_info->nodeName,
				   watchdog_info->hostName,
				   watchdog_info->pgpool_port,
				   watchdog_info->wd_port,
				   watchdog_info->state,
				   watchdog_info->stateName,
				   watchdog_info->membership_status,
				   watchdog_info->membership_status_string);
		}
	}
}

/* returns true if the current application requires node id argument */
static inline bool
app_require_nodeID(void)
{
	return (current_app_type->app_type == PCP_ATTACH_NODE ||
			current_app_type->app_type == PCP_DETACH_NODE ||
			current_app_type->app_type == PCP_HEALTH_CHECK_STATS ||
			current_app_type->app_type == PCP_PROMOTE_NODE ||
			current_app_type->app_type == PCP_RECOVERY_NODE);
}

static inline bool
app_support_cluster_mode(void)
{
	return (current_app_type->app_type == PCP_STOP_PGPOOL ||
			current_app_type->app_type == PCP_RELOAD_CONFIG);
}

static void
usage(void)
{
	fprintf(stderr, "%s - %s\n", progname, current_app_type->description);
	fprintf(stderr, "Usage:\n");
	fprintf(stderr, "%s [OPTION...] %s\n", progname,
			app_require_nodeID() ? "[node-id]" :
			(current_app_type->app_type == PCP_WATCHDOG_INFO) ? "[watchdog-id]" :
			(current_app_type->app_type == PCP_PROC_INFO) ? "[process-id]" : "");

	fprintf(stderr, "Options:\n");

	/*
	 * print the command options
	 */
	fprintf(stderr, "  -U, --username=NAME    username for PCP authentication\n");
	fprintf(stderr, "  -h, --host=HOSTNAME    pgpool-II host\n");
	fprintf(stderr, "  -p, --port=PORT        PCP port number\n");
	fprintf(stderr, "  -w, --no-password      never prompt for password\n");
	fprintf(stderr, "  -W, --password         force password prompt (should happen automatically)\n");

	/*
	 * Now the options not available for all utilities
	 */
	if (app_require_nodeID())
	{
		fprintf(stderr, "  -n, --node-id=NODEID   ID of a backend node\n");
	}

	if (current_app_type->app_type == PCP_NODE_INFO)
	{
		fprintf(stderr, "  -n, --node-id=NODEID   ID of a backend node\n");
		fprintf(stderr, "  -a, --all              display all backend nodes information\n");
	}

	if (current_app_type->app_type == PCP_STOP_PGPOOL)
	{
		fprintf(stderr, "  -m, --mode=MODE        MODE can be \"smart\", \"fast\", or \"immediate\"\n");
	}

	if (app_support_cluster_mode())
	{
		fprintf(stderr, "  -s, --scope=SCOPE      SCOPE can be \"cluster\", or \"local\"\n");
		fprintf(stderr, "                         cluster scope do operations on all Pgpool-II nodes\n");
		fprintf(stderr, "                         part of the watchdog cluster\n");
	}
	if (current_app_type->app_type == PCP_PROMOTE_NODE ||
		current_app_type->app_type == PCP_DETACH_NODE)
	{
		fprintf(stderr, "  -g, --gracefully       promote gracefully (optional)\n");
	}

	if (current_app_type->app_type == PCP_PROMOTE_NODE)
	{
		fprintf(stderr, "  -s, --switchover       switchover primary to specified node (optional)\n");
	}

	if (current_app_type->app_type == PCP_WATCHDOG_INFO)
	{
		fprintf(stderr, "  -n, --watchdog-id=ID   ID of a other pgpool to get information for\n");
		fprintf(stderr, "                         ID 0 for the local watchdog\n");
		fprintf(stderr, "                         If omitted then get information of all watchdog nodes\n");
	}
	if (current_app_type->app_type == PCP_PROC_INFO)
	{
		fprintf(stderr, "  -P, --process-id=PID   PID of the child process to get information for (optional)\n");
		fprintf(stderr, "  -a, --all              display all child processes and their available connection slots\n");
	}

	fprintf(stderr, "  -d, --debug            enable debug message (optional)\n");
	fprintf(stderr, "  -v, --verbose          output verbose messages\n");
	fprintf(stderr, "  -?, --help             print this help\n\n");
}


/*
 * Extracts the actual name of the program as called -
 * stripped of .exe suffix if any
 */
const char *
get_progname(const char *argv0)
{
	const char *nodir_name;
	char	   *progname;

	nodir_name = last_dir_separator(argv0);
	if (nodir_name)
		nodir_name++;
	else
		nodir_name = argv0;

	/*
	 * Make a copy in case argv[0] is modified by ps_status. Leaks memory, but
	 * called only once.
	 */
	progname = strdup(nodir_name);
	if (progname == NULL)
	{
		fprintf(stderr, "%s: out of memory\n", nodir_name);
		exit(1);				/* This could exit the postmaster */
	}

	return progname;
}

/*
 * Translate the BACKEND_STATUS enum value to string.
 * the function returns the constant string so should not be freed
 */
static char *
backend_status_to_string(BackendInfo * bi)
{
	char	   *statusName;

	switch (bi->backend_status)
	{

		case CON_UNUSED:
			statusName = BACKEND_STATUS_CON_UNUSED;
			break;

		case CON_CONNECT_WAIT:
			statusName = BACKEND_STATUS_CON_CONNECT_WAIT;
			break;

		case CON_UP:
			statusName = BACKEND_STATUS_CON_UP;
			break;

		case CON_DOWN:
			{
				if (bi->quarantine)
					statusName = BACKEND_STATUS_QUARANTINE;
				else
					statusName = BACKEND_STATUS_CON_DOWN;
			}
			break;

		default:
			statusName = "unknown";
			break;
	}
	return statusName;
}

/* Convert enum role to string */
char *
role_to_str(SERVER_ROLE role)
{
	static char *role_str[] = {"main", "replica", "primary", "standby"};

	if (role < ROLE_MAIN || role > ROLE_STANDBY)
		return "unknown";
	return role_str[role];
}

/*
 * Build format string for -v output mode.
 *
 * titles: title string array
 * types:  printf format type string array (example: "d")
 * ntitles: size of the array
 */
static char *
format_titles(const char **titles, const char **types, int ntitles)
{
	int	i;
	int	maxlen = 0;
	static char	formatbuf[8192];

	for(i = 0; i < ntitles; i++)
	{
		int l = strlen(titles[i]);
		maxlen = (l > maxlen)? l : maxlen;
	}

	*formatbuf = '\0';

	for(i = 0; i < ntitles; i++)
	{
		char buf[64];
		char buf2[64];

		snprintf(buf, sizeof(buf), "%%-%ds : %%%%%s", maxlen, types[i]);
		snprintf(buf2, sizeof(buf2), buf, titles[i], types[i]);
		strncat(formatbuf, buf2, sizeof(formatbuf) - strlen(formatbuf) - 2);
		strcat(formatbuf, "\n");
	}
	strcat(formatbuf, "\n");
	return formatbuf;
}
