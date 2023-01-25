/*
 * $Header$
 *
 * Provides CLI interface to interact with Pgpool-II watchdog
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
#include "version.h"
#include "parser/stringinfo.h"
#include "utils/json.h"
#include "utils/json_writer.h"
#include "utils/fe_ports.h"

#include "watchdog/wd_ipc_conn.h"
#include "watchdog/wd_lifecheck.h"
#include "watchdog/wd_commands.h"

#ifdef HAVE_GETOPT_H
#include <getopt.h>
#else
#include "utils/getopt_long.h"
#endif


#define LIFECHECK_GETNODE_WAIT_SEC_COUNT 5	/* max number of seconds the
											 * lifecheck process should waits
											 * before giving up while fetching
											 * the configured watchdog node
											 * information from watchdog
											 * process through IPC channel */

const char *progname = NULL;
LifeCheckCluster *gslifeCheckCluster = NULL;


static void usage(void);
static bool validate_number(char* ptr);
const char *get_progname(const char *argv0);
static void print_lifecheck_cluster(bool include_nodes, bool verbose);
static void print_node_info(LifeCheckNode* lifeCheckNode, bool verbose);

static bool fetch_watchdog_nodes_data(char *authkey, bool debug);
static bool inform_node_is_alive(LifeCheckNode * node, char *message, char* authkey);
static bool inform_node_is_dead(LifeCheckNode * node, char *message, char* authkey);

static void load_watchdog_nodes_from_json(char *json_data, int len);
static char *get_node_status_change_json(int nodeID, int nodeStatus, char *message, char *authKey);

static void print_node_info(LifeCheckNode* lifeCheckNode, bool verbose);
static LifeCheckNode* get_node_by_options(char *node_name, char* node_host, int node_port, int node_id);

int
main(int argc, char **argv)
{
	LifeCheckNode* lifeCheckNode;
	char*		conf_file_path = NULL;
	char*		node_host = NULL;
	char*		node_name = NULL;
	char*		wd_authkey = NULL;
	char*		socket_dir = NULL;
	char*		message = NULL;
	int			node_wd_port = -1;
	int			node_id = -1;
	int			port = -1;
	int			ch;
	int			optindex;
	bool		debug = false;
	bool		info_req = false;
	bool		inform_status = false;
	bool		verbose = false;
	bool		all_nodes = false;
	bool		status_ALIVE = false;
	bool		status_DEAD = false;
	
	/* here we put all the allowed long options for all utilities */
	static struct option long_options[] = {
		{"help", no_argument, NULL, '?'},
		{"all", no_argument, NULL, 'a'},
		{"debug", no_argument, NULL, 'd'},
		{"config-file", required_argument, NULL, 'f'},
		{"node-host", required_argument, NULL, 'H'},
		{"info", no_argument, NULL, 'i'},
		{"inform", required_argument, NULL, 'I'},
		{"auth-key", required_argument, NULL, 'k'},
		{"message", required_argument, NULL, 'm'},
		{"node-id", required_argument, NULL, 'n'},
		{"node-name", required_argument, NULL, 'N'},
		{"node-port", required_argument, NULL, 'P'},
		{"ipc-port", required_argument, NULL, 'p'},
		{"socket-dir", required_argument, NULL, 's'},
		{"version", no_argument, NULL, 'V'},
		{"verbose", no_argument, NULL, 'v'},
		{NULL, 0, NULL, 0}
	};

	/* Identify the utility app */
	progname = get_progname(argv[0]);

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

	while ((ch = getopt_long(argc, argv, "?aAdDf:H:iI:k:m:n:N:p:P:s:Vv", long_options, &optindex)) != -1)
	{
		switch (ch)
		{
			case 'd':
				debug = true;
				break;

			case 'a':
				all_nodes = true;
				break;

			case 'i': /* Info Request */
			{
				info_req = true;
				if (inform_status)
				{
					fprintf(stderr, "ERROR: Invalid option, 'info' and 'inform' are mutually exclusive options\n");
					exit(EXIT_FAILURE);
				}
			}
				break;

			case 'I':
				if (!optarg)
				{
					usage();
					exit(EXIT_FAILURE);
				}
				inform_status = true;
				if (info_req)
				{
					fprintf(stderr, "ERROR: Invalid option, 'info' and 'inform' are mutually exclusive options\n");
					exit(EXIT_FAILURE);
				}
				if (strcasecmp("DEAD",optarg) == 0)
					status_DEAD = true;
				else if (strcasecmp("ALIVE",optarg) == 0)
					status_ALIVE = true;
				else
				{
					fprintf(stderr, "ERROR: Invalid node status \"%s\", Allowed options are DEAD or ALIVE''\n",optarg);
					exit(EXIT_FAILURE);
				}
				break;

			case 'n':
				if (!optarg)
				{
					usage();
					exit(EXIT_FAILURE);
				}
				if (validate_number(optarg) == false)
				{
					fprintf(stderr, "ERROR: Invalid value %s, node-id can only contain numeric values\n",optarg);
					exit(EXIT_FAILURE);
				}
				node_id = atoi(optarg);
				break;

			case 'H':
				if (!optarg)
				{
					usage();
					exit(EXIT_FAILURE);
				}
				node_host = pstrdup(optarg);
				break;

			case 'N':
				if (!optarg)
				{
					usage();
					exit(EXIT_FAILURE);
				}
				node_name = pstrdup(optarg);
				break;

			case 'P':
				if (!optarg)
				{
					usage();
					exit(EXIT_FAILURE);
				}
				if (validate_number(optarg) == false)
				{
					fprintf(stderr, "ERROR: Invalid value %s, node-port can only contain numeric values\n",optarg);
					exit(EXIT_FAILURE);
				}
				node_wd_port = atoi(optarg);
				break;

			case 's':			/* socket dir */
				if (!optarg)
				{
					usage();
					exit(EXIT_FAILURE);
				}
				socket_dir = pstrdup(socket_dir);
				break;

			case 'm':			/* message */
				if (!optarg)
				{
					usage();
					exit(EXIT_FAILURE);
				}
				message = pstrdup(optarg);
				break;

			case 'f':			/* specify configuration file */
				if (!optarg)
				{
					usage();
					exit(EXIT_FAILURE);
				}
				conf_file_path = pstrdup(optarg);
				break;

			case 'k':			/* specify authkey */
				if (!optarg)
				{
					usage();
					exit(EXIT_FAILURE);
				}
				wd_authkey = pstrdup(optarg);
				break;

			case 'v':
				verbose = true;
				break;

			case 'V':
				fprintf(stderr, "%s for %s version %s (%s),\n", progname, PACKAGE, VERSION, PGPOOLVERSION);
				exit(0);
				break;

			case 'p':
				if (validate_number(optarg) == false)
				{
					fprintf(stderr, "ERROR: Invalid value %s, port can only contain numeric values\n",optarg);
					exit(EXIT_FAILURE);
				}
				port = atoi(optarg);
				if (port <= 1024 || port > 65535)
				{
					fprintf(stderr, "ERROR: Invalid port number \"%s\", must be between 1024 and 65535\n", optarg);
					exit(0);
				}
				break;

			case '?':
			default:
				
				/*
				 * getopt_long should already have emitted a complaint
				 */
				fprintf(stderr, "Try \"%s --help\" for more information.\n\n", progname);
				exit(EXIT_FAILURE);
		}
	}

	if (!info_req && !inform_status)
	{
		fprintf(stderr, "ERROR: Missing operation. Try %s --help\" for more information.\n\n", progname);
		exit(EXIT_FAILURE);
	}

	if (inform_status && all_nodes)
	{
		if (all_nodes)
		{
			fprintf(stderr, "ERROR: Invalid option \"-a --all\" for inform status operation. Try %s --help\" for more information.\n\n", progname);
			exit(EXIT_FAILURE);
		}
		if (node_name == NULL && node_host == NULL && node_wd_port < 0 && node_id < 0)
		{
			fprintf(stderr, "ERROR: Missing node search criteria. Try %s --help\" for more information.\n\n", progname);
			exit(EXIT_FAILURE);
		}
	}

	if (conf_file_path)
	{
		if (pool_init_config())
		{
			fprintf(stderr, "pool_init_config() failed\n\n");
			exit(EXIT_FAILURE);
		}
		if (pool_get_config(conf_file_path, CFGCXT_INIT) == false)
		{
			fprintf(stderr, "ERROR: Unable to get configuration. Exiting...\n\n");
			exit(EXIT_FAILURE);
		}

		if (debug)
			printf("DEBUG: From config %s:%d\n",pool_config->wd_ipc_socket_dir,
					pool_config->wd_nodes.wd_node_info[pool_config->pgpool_node_id].wd_port);

		pfree(conf_file_path);
		/* only use values from pg_config that are not provided explicitly*/
		if (wd_authkey == NULL)
			wd_authkey = pstrdup(pool_config->wd_authkey);
		if (port < 0)
			port = pool_config->wd_nodes.wd_node_info[pool_config->pgpool_node_id].wd_port;
		if (socket_dir == NULL)
			socket_dir = pstrdup(pool_config->wd_ipc_socket_dir);
	}

	if (port < 0)
		port = 9898;

	if (socket_dir == NULL)
		socket_dir = pstrdup("/tmp");

	if(debug)
	{
		fprintf(stderr, "DEBUG: setting IPC address to %s:%d\n",socket_dir,port);
	}

	wd_set_ipc_address(socket_dir,port);
	wd_ipc_conn_initialize();

	if(debug)
	{
		char c_node_id[10],c_wd_port[10];
		snprintf(c_node_id, sizeof(c_node_id), "%d",node_id);
		snprintf(c_wd_port, sizeof(c_wd_port), "%d",node_wd_port);

		fprintf(stderr, "DEBUG: OPERATION:%s ALL NODE CRITERIA = %s\n",
				info_req?"\"INFO REQUEST\"":"\"INFORM NODE STATUS\"",
				all_nodes?"TRUE":"FALSE"
				);
		fprintf(stderr, "DEBUG: Search criteria:[ID=%s AND Name=%s AND Host=%s AND WDPort=%s]\n",
				(node_id < 0)?"ANY":c_node_id,
				node_name?node_name:"ANY",
				node_host?node_host:"ANY",
				(node_wd_port < 0)?"ANY":c_wd_port);
	}

	fetch_watchdog_nodes_data(wd_authkey, debug);

	if (info_req)
	{
		if (all_nodes)
		{
			print_lifecheck_cluster(true, verbose);
			exit(0);
		}
		if (node_name == NULL && node_host == NULL && node_wd_port < 0 && node_id < 0)
		{
			fprintf(stderr, "WARNING: Missing node search criteria. applying --all option\n");
			print_lifecheck_cluster(true, verbose);
			exit(0);
		}
	}

	lifeCheckNode = get_node_by_options(node_name, node_host, node_wd_port, node_id);
	if (!lifeCheckNode)
	{
		char c_node_id[10],c_wd_port[10];
		fprintf(stderr, "ERROR: unable to find the node with the requested criteria\n");
		snprintf(c_node_id, sizeof(c_node_id), "%d",node_id);
		snprintf(c_wd_port, sizeof(c_wd_port), "%d",node_wd_port);
		fprintf(stderr, "Criteria:[ID=%s AND Name=%s AND Host=%s AND WDPort=%s]\n",
				(node_id < 0)?"ANY":c_node_id,
				node_name?node_name:"ANY",
				node_host?node_host:"ANY",
				(node_wd_port < 0)?"ANY":c_wd_port);
		exit(EXIT_FAILURE);

	}
	if (info_req)
	{
		print_lifecheck_cluster(false, verbose);
		print_node_info(lifeCheckNode, verbose);
		exit (0);
	}

	if (status_DEAD)
	{
		if (inform_node_is_dead(lifeCheckNode, message, wd_authkey))
		{
			fprintf(stderr,"INFO: informed watchdog about node id %d is dead\n",node_id);
			exit(0);
		}
		fprintf(stderr,"ERROR: failed to inform watchdog about node id %d is dead\n",node_id);
		exit(EXIT_FAILURE);
	}
	else if (status_ALIVE)
	{
		if (inform_node_is_alive(lifeCheckNode, message, wd_authkey))
		{
			fprintf(stderr,"INFO: informed watchdog about node id %d is alive\n",node_id);
			exit(0);
		}
		fprintf(stderr,"ERROR: failed to inform watchdog about node id %d is alive\n",node_id);
		exit(EXIT_FAILURE);
	}

	return 0;
}

const char *
get_progname(const char *argv0)
{
	return "wd_cli";
}


static bool
validate_number(char* ptr)
{
	while (*ptr)
	{
		if (isdigit(*ptr) == 0)
			return false;
		ptr++;
	}
	return true;
}

static bool
inform_node_status(LifeCheckNode * node, char *message, char* authkey)
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

	fprintf(stderr,"INFO: informing the node status change to watchdog");
	fprintf(stderr,"INFO: node id :%d status = \"%s\" message:\"%s\"", node->ID, new_status, message);

	json_data = get_node_status_change_json(node->ID, node_status, message, authkey);
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
fetch_watchdog_nodes_data(char *authkey, bool debug)
{
	char	   *json_data = wd_get_watchdog_nodes_json(authkey, -1);

	if (json_data == NULL)
	{
		ereport(ERROR,
				(errmsg("get node list command reply contains no data")));
		return false;
	}

	if(debug)
		printf("DEBUG:************\n%s\n************\n",json_data);

	load_watchdog_nodes_from_json(json_data, strlen(json_data));
	pfree(json_data);
	return true;
}

static void
load_watchdog_nodes_from_json(char *json_data, int len)
{
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
	gslifeCheckCluster = malloc(sizeof(LifeCheckCluster));
	gslifeCheckCluster->nodeCount = nodeCount;
	gslifeCheckCluster->lifeCheckNodes = malloc(sizeof(LifeCheckNode) * gslifeCheckCluster->nodeCount);
	for (i = 0; i < nodeCount; i++)
	{
		WDNodeInfo *nodeInfo = parse_watchdog_node_info_from_wd_node_json(value->u.array.values[i]);

		gslifeCheckCluster->lifeCheckNodes[i].nodeState = NODE_EMPTY;
		gslifeCheckCluster->lifeCheckNodes[i].wdState = nodeInfo->state;
		strcpy(gslifeCheckCluster->lifeCheckNodes[i].stateName, nodeInfo->stateName);
		gslifeCheckCluster->lifeCheckNodes[i].ID = nodeInfo->id;
		strcpy(gslifeCheckCluster->lifeCheckNodes[i].hostName, nodeInfo->hostName);
		strcpy(gslifeCheckCluster->lifeCheckNodes[i].nodeName, nodeInfo->nodeName);
		gslifeCheckCluster->lifeCheckNodes[i].wdPort = nodeInfo->wd_port;
		gslifeCheckCluster->lifeCheckNodes[i].pgpoolPort = nodeInfo->pgpool_port;
		gslifeCheckCluster->lifeCheckNodes[i].retry_lives = pool_config->wd_life_point;
		pfree(nodeInfo);
	}
	json_value_free(root);
}


static bool
inform_node_is_dead(LifeCheckNode * node, char *message, char* authkey)
{
	node->nodeState = NODE_DEAD;
	return inform_node_status(node, message, authkey);
}

static bool
inform_node_is_alive(LifeCheckNode * node, char *message, char* authkey)
{
	node->nodeState = NODE_ALIVE;
	return inform_node_status(node, message, authkey);
}

static LifeCheckNode*
get_node_by_options(char *node_name, char* node_host, int node_port, int node_id)
{
	int i;
	if (!gslifeCheckCluster)
		return NULL;
	for (i = 0; i < gslifeCheckCluster->nodeCount; i++)
	{
		if (node_id >= 0 && node_id != gslifeCheckCluster->lifeCheckNodes[i].ID)
			continue;
		if (node_port >= 0 && node_port != gslifeCheckCluster->lifeCheckNodes[i].wdPort)
			continue;
		if (node_name && strcasecmp(gslifeCheckCluster->lifeCheckNodes[i].nodeName, node_name) != 0 )
			continue;
		if (node_host && strcasecmp(gslifeCheckCluster->lifeCheckNodes[i].hostName, node_host) != 0 )
			continue;

		return &gslifeCheckCluster->lifeCheckNodes[i];
	}
	return NULL;
}


static void
print_lifecheck_cluster(bool include_nodes, bool verbose)
{
	int			i;
	if (!gslifeCheckCluster)
	{
		fprintf(stdout,"ERROR: node information not found\n");
		return;
	}
	fprintf(stdout,"Total Watchdog nodes configured for lifecheck:    %d\n", gslifeCheckCluster->nodeCount);
	if (verbose)
		fprintf(stdout,"*****************\n");
	if(!include_nodes)
		return;

	for (i = 0; i < gslifeCheckCluster->nodeCount; i++)
		print_node_info(&gslifeCheckCluster->lifeCheckNodes[i], verbose);
}



static void
print_node_info(LifeCheckNode* lifeCheckNode, bool verbose)
{
	if (verbose)
	{
		fprintf(stdout,"Node ID:           %d\n",lifeCheckNode->ID);
		fprintf(stdout,"Node Status code:  %d\n",lifeCheckNode->wdState);
		fprintf(stdout,"Node Status:       %s\n",lifeCheckNode->stateName);
		fprintf(stdout,"Node Name:         %s\n",lifeCheckNode->nodeName);
		fprintf(stdout,"Node Host:         %s\n",lifeCheckNode->hostName);
		fprintf(stdout,"Node WD Port:      %d\n",lifeCheckNode->wdPort);
		fprintf(stdout,"Node Pgpool Port:  %d\n\n",lifeCheckNode->pgpoolPort);
	}
	else
	{
		fprintf(stdout,"%d %d \"%s\"", lifeCheckNode->ID,
				lifeCheckNode->nodeState,
				lifeCheckNode->stateName),
		fprintf(stdout,"\"%s\"",lifeCheckNode->nodeName),
		fprintf(stdout,"\"%s\" %d %d\n",
				lifeCheckNode->hostName,
				lifeCheckNode->wdPort,
				lifeCheckNode->pgpoolPort);
	}
}

static char *
get_node_status_change_json(int nodeID, int nodeStatus, char *message, char *authKey)
{
	char	   *json_str;
	JsonNode   *jNode = jw_create_with_object(true);
	
	if (authKey != NULL && strlen(authKey) > 0)
		jw_put_string(jNode, WD_IPC_AUTH_KEY, authKey); /* put the auth key */
	
	/* add the node ID */
	jw_put_int(jNode, "NodeID", nodeID);
	/* add the node status */
	jw_put_int(jNode, "NodeStatus", nodeStatus);
	/* add the node message if any */
	if (message)
		jw_put_string(jNode, "Message", message);
	
	jw_finish_document(jNode);
	json_str = pstrdup(jw_get_json_string(jNode));
	jw_destroy(jNode);
	return json_str;
}

static void
usage(void)
{
	
	fprintf(stderr, "\nWatchdog CLI for ");
	fprintf(stderr, "%s version %s (%s)\n", PACKAGE, VERSION, PGPOOLVERSION);

	fprintf(stderr, "\nUsage:\n");
	fprintf(stderr, "  %s [ operation] [ options] [node search criteria]\n",progname);

	fprintf(stderr, "\n Operations:\n");
	fprintf(stderr, "  -i, --info                  Get the node status for nodes based on node search criteria\n");
	fprintf(stderr, "  -I, --inform=NEW-STATUS     Inform Pgpool-II about new watchdog node status\n");
	fprintf(stderr, "                              Allowed values are DEAD and ALIVE\n");

	fprintf(stderr, "\n Node search options:\n");

	fprintf(stderr, "  -a, --all                Search all nodes (only available with --info option)\n");
	fprintf(stderr, "  -n, --node-id=ID         Search watchdog node with node_id\n");
	fprintf(stderr, "  -N, --node-name=Name     Search watchdog node with name\n");
	fprintf(stderr, "  -H, --node-host=Host     Search watchdog node with Host\n");
	fprintf(stderr, "  -P, --node-port=port     Search watchdog node with wd_port\n");

	fprintf(stderr, "\n Options:\n");

	fprintf(stderr, "  -f, --config-file=CONFIG_FILE\n");
	fprintf(stderr, "                      Set the path to the pgpool.conf configuration file\n");
	fprintf(stderr, "  -k, --auth-key=KEY\n");
	fprintf(stderr, "                      watchdog auth key\n");
	fprintf(stderr, "                      This over rides the pgpool.conf->wd_authkey value\n");
	fprintf(stderr, "  -s, --socket-dir=WD_IPC_SOCKET_DIRECTORY\n");
	fprintf(stderr, "                      Path to the WD IPC socket directory\n");
	fprintf(stderr, "                      This over rides the pgpool.conf->wd_ipc_socket_dir value\n");
	fprintf(stderr, "  -p, --ipc-port=WD_IPC_PORT\n");
	fprintf(stderr, "                      Port number of watchdog IPC socket\n");
	fprintf(stderr, "                      This over rides the pgpool.conf->wd_port value\n");
	fprintf(stderr, "  -m, --message=message string\n");
	fprintf(stderr, "                      Message to be passed to Pgpool-II along with new node status\n");
	fprintf(stderr, "  -v, --verbose       Output verbose messages\n");
	fprintf(stderr, "  -V, --version       Output Version information\n");
	fprintf(stderr, "  -d, --debug         Enable debug output\n");
	fprintf(stderr, "  -h, --help          Print this help\n\n");
}
