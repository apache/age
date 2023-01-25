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
 * Process pgPool-II "SHOW" queries.
 */
#include "pool.h"
#include "main/health_check.h"
#include "protocol/pool_proto_modules.h"
#include "protocol/pool_process_query.h"
#include "utils/elog.h"
#include "utils/pool_stream.h"
#include "utils/statistics.h"
#include "pool_config.h"
#include "query_cache/pool_memqcache.h"
#include "version.h"
#include "protocol/pool_pg_utils.h"

#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <netinet/in.h>

static void send_row_description_and_data_rows(POOL_CONNECTION * frontend, POOL_CONNECTION_POOL * backend,
											   short num_fields, char **field_names, int *offsettbl,
											   char *data, int row_size, int nrows);
static void write_one_field(POOL_CONNECTION * frontend, char *field);
static void write_one_field_v2(POOL_CONNECTION * frontend, char *field);
static char *db_node_status(int node);
static char *db_node_role(int node);

void
send_row_description(POOL_CONNECTION * frontend, POOL_CONNECTION_POOL * backend,
					 short num_fields, char **field_names)
{
	static char *cursorname = "blank";
	static int	oid = 0;
	static short fsize = -1;
	static int	mod = 0;
	short		n;
	int			i;
	short		s;
	int			len;
	short		colnum;

	if (MAJOR(backend) == PROTO_MAJOR_V2)
	{
		/* cursor response */
		pool_write(frontend, "P", 1);
		pool_write(frontend, cursorname, strlen(cursorname) + 1);
	}

	/* row description */
	pool_write(frontend, "T", 1);

	if (MAJOR(backend) == PROTO_MAJOR_V3)
	{
		/*
		 * information about computed byte in section "RowDescription (B)"
		 * here:
		 * http://www.postgresql.org/docs/current/static/protocol-message-formats.html
		 */

		len = 6;				/* int32 + int16 */

		for (i = 0; i < num_fields; i++)
		{
			/* String + '\0' + 3* int32 + 3* int16 */
			len += strlen(field_names[i]) + 1 + 18;
		}

		len = htonl(len);
		pool_write(frontend, &len, sizeof(len));
	}

	n = htons(num_fields);
	pool_write(frontend, &n, sizeof(short));

	for (i = 0; i < num_fields; i++)
	{
		pool_write(frontend, field_names[i], strlen(field_names[i]) + 1);	/* field name */

		if (MAJOR(backend) == PROTO_MAJOR_V3)
		{
			pool_write(frontend, &oid, sizeof(oid));	/* table oid */
			colnum = htons(i);
			pool_write(frontend, &colnum, sizeof(colnum));	/* column number */
		}

		pool_write(frontend, &oid, sizeof(oid));	/* data type oid */
		s = htons(fsize);
		pool_write(frontend, &s, sizeof(fsize));	/* field size */
		pool_write(frontend, &mod, sizeof(mod));	/* modifier */

		if (MAJOR(backend) == PROTO_MAJOR_V3)
		{
			s = htons(0);
			pool_write(frontend, &s, sizeof(fsize));	/* field format (text) */
		}
	}
	pool_flush(frontend);
}

/*
 * Send the command complete and ready for query message
 * to frontend.
 * If the num_row is lower than 0, it is not included
 * to the command complete message.
 */
void
send_complete_and_ready(POOL_CONNECTION * frontend, POOL_CONNECTION_POOL * backend, const char *message, const int num_rows)
{
	int			len;
	int			msg_len;
	char		msg[64];

	if (num_rows >= 0)
		msg_len = snprintf(msg, sizeof(msg), "%s %d", message, num_rows);
	else
		msg_len = snprintf(msg, sizeof(msg), "%s", message);

	/* complete command response */
	pool_write(frontend, "C", 1);
	if (MAJOR(backend) == PROTO_MAJOR_V3)
	{
		len = htonl(4 + msg_len + 1);
		pool_write(frontend, &len, sizeof(len));
	}
	pool_write(frontend, msg, msg_len + 1);

	/* ready for query */
	pool_write(frontend, "Z", 1);
	if (MAJOR(backend) == PROTO_MAJOR_V3)
	{
		len = htonl(5);
		pool_write(frontend, &len, sizeof(len));
		pool_write(frontend, "I", 1);
	}

	pool_flush(frontend);
}

POOL_REPORT_CONFIG *
get_config(int *nrows)
{
	int			i,
				j;
	int			len;

/*
 * Report data buffer.
 * 128 is the max number of configuration items.
 * In addition, we need MAX_NUM_BACKENDS*4
 * for backend descriptions.
 */
#define MAXITEMS (256 + MAX_NUM_BACKENDS*4)

	POOL_REPORT_CONFIG *status = palloc0(MAXITEMS * sizeof(POOL_REPORT_CONFIG));

	/*
	 * we initialize the array with NULL values so when looping on it, we can
	 * use it as stop condition
	 */

	i = 0;

	/* CLUSTERING MODE */

	StrNCpy(status[i].name, "backend_clustering_mode", POOLCONFIG_MAXNAMELEN);
	snprintf(status[i].value, POOLCONFIG_MAXVALLEN, "%d", pool_config->backend_clustering_mode);
	StrNCpy(status[i].desc, "clustering mode", POOLCONFIG_MAXDESCLEN);
	i++;

	/* CONNECTIONS */

	/* - pgpool Connection Settings - */
	StrNCpy(status[i].name, "listen_addresses", POOLCONFIG_MAXNAMELEN);
	*(status[i].value) = '\0';
	for (j = 0; j < pool_config->num_listen_addresses; j++)
	{
		len = POOLCONFIG_MAXVALLEN - strlen(status[i].value);
		strncat(status[i].value, pool_config->listen_addresses[j], len);
		len = POOLCONFIG_MAXVALLEN - strlen(status[i].value);
		if (j != pool_config->num_listen_addresses - 1)
			strncat(status[i].value, ",", len);
	}
	StrNCpy(status[i].desc, "host name(s) or IP address(es) to listen on", POOLCONFIG_MAXDESCLEN);
	i++;

	StrNCpy(status[i].name, "port", POOLCONFIG_MAXNAMELEN);
	snprintf(status[i].value, POOLCONFIG_MAXVALLEN, "%d", pool_config->port);
	StrNCpy(status[i].desc, "pgpool accepting port number", POOLCONFIG_MAXDESCLEN);
	i++;

	StrNCpy(status[i].name, "unix_socket_directories", POOLCONFIG_MAXNAMELEN);
	*(status[i].value) = '\0';
	for (j = 0; j < pool_config->num_unix_socket_directories; j++)
	{
		len = POOLCONFIG_MAXVALLEN - strlen(status[i].value);
		strncat(status[i].value, pool_config->unix_socket_directories[j], len);
		len = POOLCONFIG_MAXVALLEN - strlen(status[i].value);
		if (j != pool_config->num_unix_socket_directories - 1)
			strncat(status[i].value, ",", len);
	}
	StrNCpy(status[i].desc, "pgpool socket directories", POOLCONFIG_MAXDESCLEN);
	i++;

	StrNCpy(status[i].name, "unix_socket_group", POOLCONFIG_MAXNAMELEN);
	snprintf(status[i].value, POOLCONFIG_MAXVALLEN, "%s", pool_config->unix_socket_group);
	StrNCpy(status[i].desc, "owning user of the unix sockets", POOLCONFIG_MAXDESCLEN);
	i++;

	StrNCpy(status[i].name, "unix_socket_permissions", POOLCONFIG_MAXNAMELEN);
	snprintf(status[i].value, POOLCONFIG_MAXVALLEN, "%04o", pool_config->unix_socket_permissions);
	StrNCpy(status[i].desc, "access permissions of the unix sockets.", POOLCONFIG_MAXDESCLEN);
	i++;

	StrNCpy(status[i].name, "pcp_listen_addresses", POOLCONFIG_MAXNAMELEN);
	*(status[i].value) = '\0';
	for (j = 0; j < pool_config->num_pcp_listen_addresses; j++)
	{
		len = POOLCONFIG_MAXVALLEN - strlen(status[i].value);
		strncat(status[i].value, pool_config->pcp_listen_addresses[j], len);
		len = POOLCONFIG_MAXVALLEN - strlen(status[i].value);
		if (j != pool_config->num_pcp_listen_addresses - 1)
			strncat(status[i].value, ",", len);
	}
	StrNCpy(status[i].desc, "host name(s) or IP address(es) for pcp process to listen on", POOLCONFIG_MAXDESCLEN);
	i++;


	StrNCpy(status[i].name, "pcp_port", POOLCONFIG_MAXNAMELEN);
	snprintf(status[i].value, POOLCONFIG_MAXVALLEN, "%d", pool_config->pcp_port);
	StrNCpy(status[i].desc, "PCP port # to bind", POOLCONFIG_MAXDESCLEN);
	i++;

	StrNCpy(status[i].name, "pcp_socket_dir", POOLCONFIG_MAXNAMELEN);
	snprintf(status[i].value, POOLCONFIG_MAXVALLEN, "%s", pool_config->pcp_socket_dir);
	StrNCpy(status[i].desc, "PCP socket directory", POOLCONFIG_MAXDESCLEN);
	i++;

	/* # - Authentication - */
	StrNCpy(status[i].name, "enable_pool_hba", POOLCONFIG_MAXNAMELEN);
	snprintf(status[i].value, POOLCONFIG_MAXVALLEN, "%d", pool_config->enable_pool_hba);
	StrNCpy(status[i].desc, "if true, use pool_hba.conf for client authentication", POOLCONFIG_MAXDESCLEN);
	i++;

	StrNCpy(status[i].name, "pool_passwd", POOLCONFIG_MAXNAMELEN);
	snprintf(status[i].value, POOLCONFIG_MAXVALLEN, "%s", pool_config->pool_passwd);
	StrNCpy(status[i].desc, "file name of pool_passwd for md5 authentication", POOLCONFIG_MAXDESCLEN);
	i++;

	StrNCpy(status[i].name, "authentication_timeout", POOLCONFIG_MAXNAMELEN);
	snprintf(status[i].value, POOLCONFIG_MAXVALLEN, "%d", pool_config->authentication_timeout);
	StrNCpy(status[i].desc, "maximum time in seconds to complete client authentication", POOLCONFIG_MAXNAMELEN);
	i++;

	StrNCpy(status[i].name, "allow_clear_text_frontend_auth", POOLCONFIG_MAXNAMELEN);
	snprintf(status[i].value, POOLCONFIG_MAXVALLEN, "%d", pool_config->allow_clear_text_frontend_auth);
	StrNCpy(status[i].desc, "allow to use clear text password auth when pool_passwd does not contain password", POOLCONFIG_MAXDESCLEN);
	i++;

	/* - SSL Connections - */
	StrNCpy(status[i].name, "ssl", POOLCONFIG_MAXNAMELEN);
	snprintf(status[i].value, POOLCONFIG_MAXVALLEN, "%d", pool_config->ssl);
	StrNCpy(status[i].desc, "SSL support", POOLCONFIG_MAXDESCLEN);
	i++;

	StrNCpy(status[i].name, "ssl_key", POOLCONFIG_MAXNAMELEN);
	snprintf(status[i].value, POOLCONFIG_MAXVALLEN, "%s", pool_config->ssl_key);
	StrNCpy(status[i].desc, "SSL private key file", POOLCONFIG_MAXDESCLEN);
	i++;

	StrNCpy(status[i].name, "ssl_cert", POOLCONFIG_MAXNAMELEN);
	snprintf(status[i].value, POOLCONFIG_MAXVALLEN, "%s", pool_config->ssl_cert);
	StrNCpy(status[i].desc, "SSL public certificate file", POOLCONFIG_MAXDESCLEN);
	i++;

	StrNCpy(status[i].name, "ssl_ca_cert", POOLCONFIG_MAXNAMELEN);
	snprintf(status[i].value, POOLCONFIG_MAXVALLEN, "%s", pool_config->ssl_ca_cert);
	StrNCpy(status[i].desc, "single PEM format file containing CA root certificate(s)", POOLCONFIG_MAXDESCLEN);
	i++;

	StrNCpy(status[i].name, "ssl_ca_cert_dir", POOLCONFIG_MAXNAMELEN);
	snprintf(status[i].value, POOLCONFIG_MAXVALLEN, "%s", pool_config->ssl_ca_cert_dir);
	StrNCpy(status[i].desc, "directory containing CA root certificate(s)", POOLCONFIG_MAXDESCLEN);
	i++;

	StrNCpy(status[i].name, "ssl_crl_file", POOLCONFIG_MAXNAMELEN);
	snprintf(status[i].value, POOLCONFIG_MAXVALLEN, "%s", pool_config->ssl_crl_file);
	StrNCpy(status[i].desc, "SSL certificate revocation list file", POOLCONFIG_MAXDESCLEN);
	i++;

	StrNCpy(status[i].name, "ssl_ciphers", POOLCONFIG_MAXNAMELEN);
	snprintf(status[i].value, POOLCONFIG_MAXVALLEN, "%s", pool_config->ssl_ciphers);
	StrNCpy(status[i].desc, "allowed SSL ciphers", POOLCONFIG_MAXDESCLEN);
	i++;

	StrNCpy(status[i].name, "ssl_prefer_server_ciphers", POOLCONFIG_MAXNAMELEN);
	snprintf(status[i].value, POOLCONFIG_MAXVALLEN, "%d", pool_config->ssl_prefer_server_ciphers);
	StrNCpy(status[i].desc, "Use server's SSL cipher preferences", POOLCONFIG_MAXDESCLEN);
	i++;

	StrNCpy(status[i].name, "ssl_ecdh_curve", POOLCONFIG_MAXNAMELEN);
	snprintf(status[i].value, POOLCONFIG_MAXVALLEN, "%s", pool_config->ssl_ecdh_curve);
	StrNCpy(status[i].desc, "the curve to use in ECDH key exchange", POOLCONFIG_MAXDESCLEN);
	i++;

	StrNCpy(status[i].name, "ssl_dh_params_file", POOLCONFIG_MAXNAMELEN);
	snprintf(status[i].value, POOLCONFIG_MAXVALLEN, "%s", pool_config->ssl_dh_params_file);
	StrNCpy(status[i].desc, "path to the Diffie-Hellman parameters contained file", POOLCONFIG_MAXDESCLEN);
	i++;

	StrNCpy(status[i].name, "ssl_passphrase_command", POOLCONFIG_MAXNAMELEN);
	snprintf(status[i].value, POOLCONFIG_MAXVALLEN, "%s", pool_config->ssl_passphrase_command);
	StrNCpy(status[i].desc, "external command to be invoked when a passphrase for decrypting an SSL file such as a private key needs to be obtained", POOLCONFIG_MAXDESCLEN);
	i++;

	/* POOLS */

	/* - Pool size -  */

	StrNCpy(status[i].name, "listen_backlog_multiplier", POOLCONFIG_MAXNAMELEN);
	snprintf(status[i].value, POOLCONFIG_MAXVALLEN, "%d", pool_config->listen_backlog_multiplier);
	StrNCpy(status[i].desc, "determines the size of the queue for pending connections", POOLCONFIG_MAXDESCLEN);
	i++;

	StrNCpy(status[i].name, "serialize_accept", POOLCONFIG_MAXNAMELEN);
	snprintf(status[i].value, POOLCONFIG_MAXVALLEN, "%d", pool_config->serialize_accept);
	StrNCpy(status[i].desc, "whether to serialize accept() call", POOLCONFIG_MAXDESCLEN);
	i++;

	StrNCpy(status[i].name, "reserved_connections", POOLCONFIG_MAXNAMELEN);
	snprintf(status[i].value, POOLCONFIG_MAXVALLEN, "%d", pool_config->reserved_connections);
	StrNCpy(status[i].desc, "number of reserved connections", POOLCONFIG_MAXDESCLEN);
	i++;

	StrNCpy(status[i].name, "max_pool", POOLCONFIG_MAXNAMELEN);
	snprintf(status[i].value, POOLCONFIG_MAXVALLEN, "%d", pool_config->max_pool);
	StrNCpy(status[i].desc, "max # of connection pool per child", POOLCONFIG_MAXDESCLEN);
	i++;

	StrNCpy(status[i].name, "process_management_mode", POOLCONFIG_MAXNAMELEN);
	snprintf(status[i].value, POOLCONFIG_MAXVALLEN, "%d", pool_config->process_management);
	StrNCpy(status[i].desc, "process management mode", POOLCONFIG_MAXDESCLEN);
	i++;

	StrNCpy(status[i].name, "process_management_strategy", POOLCONFIG_MAXNAMELEN);
	snprintf(status[i].value, POOLCONFIG_MAXVALLEN, "%d", pool_config->process_management_strategy);
	StrNCpy(status[i].desc, "process management strategy", POOLCONFIG_MAXDESCLEN);
	i++;

	StrNCpy(status[i].name, "num_init_children", POOLCONFIG_MAXNAMELEN);
	snprintf(status[i].value, POOLCONFIG_MAXVALLEN, "%d", pool_config->num_init_children);
	StrNCpy(status[i].desc, "# of children initially pre-forked", POOLCONFIG_MAXDESCLEN);
	i++;

	StrNCpy(status[i].name, "min_spare_children", POOLCONFIG_MAXNAMELEN);
	snprintf(status[i].value, POOLCONFIG_MAXVALLEN, "%d", pool_config->min_spare_children);
	StrNCpy(status[i].desc, "min # of spare children waitting for connection", POOLCONFIG_MAXDESCLEN);
	i++;

	StrNCpy(status[i].name, "max_spare_children", POOLCONFIG_MAXNAMELEN);
	snprintf(status[i].value, POOLCONFIG_MAXVALLEN, "%d", pool_config->max_spare_children);
	StrNCpy(status[i].desc, "max # of spare children waitting for connection", POOLCONFIG_MAXDESCLEN);
	i++;

	/* - Life time - */
	StrNCpy(status[i].name, "child_life_time", POOLCONFIG_MAXNAMELEN);
	snprintf(status[i].value, POOLCONFIG_MAXVALLEN, "%d", pool_config->child_life_time);
	StrNCpy(status[i].desc, "if idle for this seconds, child exits", POOLCONFIG_MAXDESCLEN);
	i++;

	StrNCpy(status[i].name, "child_max_connections", POOLCONFIG_MAXNAMELEN);
	snprintf(status[i].value, POOLCONFIG_MAXVALLEN, "%d", pool_config->child_max_connections);
	StrNCpy(status[i].desc, "if max_connections received, child exits", POOLCONFIG_MAXDESCLEN);
	i++;

	StrNCpy(status[i].name, "connection_life_time", POOLCONFIG_MAXNAMELEN);
	snprintf(status[i].value, POOLCONFIG_MAXVALLEN, "%d", pool_config->connection_life_time);
	StrNCpy(status[i].desc, "if idle for this seconds, connection closes", POOLCONFIG_MAXDESCLEN);
	i++;

	StrNCpy(status[i].name, "client_idle_limit", POOLCONFIG_MAXNAMELEN);
	snprintf(status[i].value, POOLCONFIG_MAXVALLEN, "%d", pool_config->client_idle_limit);
	StrNCpy(status[i].desc, "if idle for this seconds, child connection closes", POOLCONFIG_MAXDESCLEN);
	i++;

	/* LOGS */

	/* - Where to log - */
	StrNCpy(status[i].name, "log_destination", POOLCONFIG_MAXNAMELEN);
	snprintf(status[i].value, POOLCONFIG_MAXVALLEN, "%s", pool_config->log_destination_str);
	StrNCpy(status[i].desc, "logging destination", POOLCONFIG_MAXDESCLEN);
	i++;

	/* - What to log - */
	StrNCpy(status[i].name, "log_line_prefix", POOLCONFIG_MAXNAMELEN);
	snprintf(status[i].value, POOLCONFIG_MAXVALLEN, "%s", pool_config->log_line_prefix);
	StrNCpy(status[i].desc, "printf-style string to output at beginning of each log line", POOLCONFIG_MAXDESCLEN);
	i++;

	StrNCpy(status[i].name, "log_error_verbosity", POOLCONFIG_MAXNAMELEN);
	snprintf(status[i].value, POOLCONFIG_MAXVALLEN, "%d", pool_config->log_error_verbosity);
	StrNCpy(status[i].desc, "controls how much detail about error should be emitted", POOLCONFIG_MAXDESCLEN);
	i++;

	StrNCpy(status[i].name, "client_min_messages", POOLCONFIG_MAXNAMELEN);
	snprintf(status[i].value, POOLCONFIG_MAXVALLEN, "%d", pool_config->client_min_messages);
	StrNCpy(status[i].desc, "controls which message should be sent to client", POOLCONFIG_MAXDESCLEN);
	i++;

	StrNCpy(status[i].name, "log_min_messages", POOLCONFIG_MAXNAMELEN);
	snprintf(status[i].value, POOLCONFIG_MAXVALLEN, "%d", pool_config->log_min_messages);
	StrNCpy(status[i].desc, "controls which message should be emitted to server log", POOLCONFIG_MAXDESCLEN);
	i++;

	StrNCpy(status[i].name, "log_connections", POOLCONFIG_MAXNAMELEN);
	snprintf(status[i].value, POOLCONFIG_MAXVALLEN, "%d", pool_config->log_connections);
	StrNCpy(status[i].desc, "if true, print incoming connections to the log", POOLCONFIG_MAXDESCLEN);
	i++;

	StrNCpy(status[i].name, "log_disconnections", POOLCONFIG_MAXNAMELEN);
	snprintf(status[i].value, POOLCONFIG_MAXVALLEN, "%d", pool_config->log_disconnections);
	StrNCpy(status[i].desc, "if true, print closing connections to the log", POOLCONFIG_MAXDESCLEN);
	i++;

	StrNCpy(status[i].name, "log_hostname", POOLCONFIG_MAXNAMELEN);
	snprintf(status[i].value, POOLCONFIG_MAXVALLEN, "%d", pool_config->log_hostname);
	StrNCpy(status[i].desc, "if true, resolve hostname for ps and log print", POOLCONFIG_MAXDESCLEN);
	i++;

	StrNCpy(status[i].name, "log_statement", POOLCONFIG_MAXNAMELEN);
	snprintf(status[i].value, POOLCONFIG_MAXVALLEN, "%d", pool_config->log_statement);
	StrNCpy(status[i].desc, "if non 0, logs all SQL statements", POOLCONFIG_MAXDESCLEN);
	i++;

	StrNCpy(status[i].name, "log_per_node_statement", POOLCONFIG_MAXNAMELEN);
	snprintf(status[i].value, POOLCONFIG_MAXVALLEN, "%d", pool_config->log_per_node_statement);
	StrNCpy(status[i].desc, "if non 0, logs all SQL statements on each node", POOLCONFIG_MAXDESCLEN);
	i++;

	StrNCpy(status[i].name, "log_client_messages", POOLCONFIG_MAXNAMELEN);
	snprintf(status[i].value, POOLCONFIG_MAXVALLEN, "%d", pool_config->log_client_messages);
	StrNCpy(status[i].desc, "if non 0, logs any client messages", POOLCONFIG_MAXDESCLEN);
	i++;

	StrNCpy(status[i].name, "log_standby_delay", POOLCONFIG_MAXNAMELEN);
	snprintf(status[i].value, POOLCONFIG_MAXVALLEN, "%d", pool_config->log_standby_delay);
	StrNCpy(status[i].desc, "how to log standby delay", POOLCONFIG_MAXDESCLEN);
	i++;

	/* - Syslog specific -  */
	StrNCpy(status[i].name, "syslog_facility", POOLCONFIG_MAXNAMELEN);
	snprintf(status[i].value, POOLCONFIG_MAXVALLEN, "LOCAL%d", (pool_config->syslog_facility / 8) - 16);
	StrNCpy(status[i].desc, "syslog local facility", POOLCONFIG_MAXDESCLEN);
	i++;

	StrNCpy(status[i].name, "syslog_ident", POOLCONFIG_MAXNAMELEN);
	snprintf(status[i].value, POOLCONFIG_MAXVALLEN, "%s", pool_config->syslog_ident);
	StrNCpy(status[i].desc, "syslog program ident string", POOLCONFIG_MAXDESCLEN);
	i++;

	/* FILE LOCATIONS */

	StrNCpy(status[i].name, "pid_file_name", POOLCONFIG_MAXNAMELEN);
	snprintf(status[i].value, POOLCONFIG_MAXVALLEN, "%s", pool_config->pid_file_name);
	StrNCpy(status[i].desc, "path to pid file", POOLCONFIG_MAXDESCLEN);
	i++;

	StrNCpy(status[i].name, "logdir", POOLCONFIG_MAXNAMELEN);
	snprintf(status[i].value, POOLCONFIG_MAXVALLEN, "%s", pool_config->logdir);
	StrNCpy(status[i].desc, "PgPool status file logging directory", POOLCONFIG_MAXDESCLEN);
	i++;

	/* CONNECTION POOLING */

	StrNCpy(status[i].name, "connection_cache", POOLCONFIG_MAXNAMELEN);
	snprintf(status[i].value, POOLCONFIG_MAXVALLEN, "%d", pool_config->connection_cache);
	StrNCpy(status[i].desc, "if true, cache connection pool", POOLCONFIG_MAXDESCLEN);
	i++;

	StrNCpy(status[i].name, "reset_query_list", POOLCONFIG_MAXNAMELEN);
	*(status[i].value) = '\0';
	for (j = 0; j < pool_config->num_reset_queries; j++)
	{
		len = POOLCONFIG_MAXVALLEN - strlen(status[i].value);
		strncat(status[i].value, pool_config->reset_query_list[j], len);
		len = POOLCONFIG_MAXVALLEN - strlen(status[i].value);
		if (j != pool_config->num_reset_queries - 1)
			strncat(status[i].value, ";", len);
	}
	StrNCpy(status[i].desc, "queries issued at the end of session", POOLCONFIG_MAXDESCLEN);
	i++;

	/* REPLICATION MODE */

	StrNCpy(status[i].name, "replicate_select", POOLCONFIG_MAXNAMELEN);
	snprintf(status[i].value, POOLCONFIG_MAXVALLEN, "%d", pool_config->replicate_select);
	StrNCpy(status[i].desc, "non 0 if SELECT statement is replicated", POOLCONFIG_MAXDESCLEN);
	i++;

	StrNCpy(status[i].name, "insert_lock", POOLCONFIG_MAXNAMELEN);
	snprintf(status[i].value, POOLCONFIG_MAXVALLEN, "%d", pool_config->insert_lock);
	StrNCpy(status[i].desc, "insert lock", POOLCONFIG_MAXDESCLEN);
	i++;

	StrNCpy(status[i].name, "lobj_lock_table", POOLCONFIG_MAXNAMELEN);
	snprintf(status[i].value, POOLCONFIG_MAXVALLEN, "%s", pool_config->lobj_lock_table);
	StrNCpy(status[i].desc, "table name used for large object replication control", POOLCONFIG_MAXDESCLEN);
	i++;

	/* - Degenerate handling - */
	StrNCpy(status[i].name, "replication_stop_on_mismatch", POOLCONFIG_MAXNAMELEN);
	snprintf(status[i].value, POOLCONFIG_MAXVALLEN, "%d", pool_config->replication_stop_on_mismatch);
	StrNCpy(status[i].desc, "stop replication mode on fatal error", POOLCONFIG_MAXDESCLEN);
	i++;

	StrNCpy(status[i].name, "failover_if_affected_tuples_mismatch", POOLCONFIG_MAXNAMELEN);
	snprintf(status[i].value, POOLCONFIG_MAXVALLEN, "%d", pool_config->failover_if_affected_tuples_mismatch);
	StrNCpy(status[i].desc, "failover if affected tuples are mismatch", POOLCONFIG_MAXDESCLEN);
	i++;

	/* LOAD BALANCING MODE */

	StrNCpy(status[i].name, "load_balance_mode", POOLCONFIG_MAXNAMELEN);
	snprintf(status[i].value, POOLCONFIG_MAXVALLEN, "%d", pool_config->load_balance_mode);
	StrNCpy(status[i].desc, "non 0 if operating in load balancing mode", POOLCONFIG_MAXDESCLEN);
	i++;

	StrNCpy(status[i].name, "ignore_leading_white_space", POOLCONFIG_MAXNAMELEN);
	snprintf(status[i].value, POOLCONFIG_MAXVALLEN, "%d", pool_config->ignore_leading_white_space);
	StrNCpy(status[i].desc, "ignore leading white spaces", POOLCONFIG_MAXDESCLEN);
	i++;

	StrNCpy(status[i].name, "read_only_function_list", POOLCONFIG_MAXNAMELEN);
	*(status[i].value) = '\0';
	for (j = 0; j < pool_config->num_read_only_function_list; j++)
	{
		len = POOLCONFIG_MAXVALLEN - strlen(status[i].value);
		strncat(status[i].value, pool_config->read_only_function_list[j], len);
		len = POOLCONFIG_MAXVALLEN - strlen(status[i].value);
		if (j != pool_config->num_read_only_function_list - 1)
			strncat(status[i].value, ",", len);
	}
	StrNCpy(status[i].desc, "functions those do not write to database", POOLCONFIG_MAXDESCLEN);
	i++;

	StrNCpy(status[i].name, "write_function_list", POOLCONFIG_MAXNAMELEN);
	*(status[i].value) = '\0';
	for (j = 0; j < pool_config->num_write_function_list; j++)
	{
		len = POOLCONFIG_MAXVALLEN - strlen(status[i].value);
		strncat(status[i].value, pool_config->write_function_list[j], len);
		len = POOLCONFIG_MAXVALLEN - strlen(status[i].value);
		if (j != pool_config->num_write_function_list - 1)
			strncat(status[i].value, ",", len);
	}
	StrNCpy(status[i].desc, "functions those write to database", POOLCONFIG_MAXDESCLEN);
	i++;

	StrNCpy(status[i].name, "primary_routing_query_pattern_list", POOLCONFIG_MAXNAMELEN);
	*(status[i].value) = '\0';
	for (j = 0; j < pool_config->num_primary_routing_query_pattern_list; j++)
	{
		len = POOLCONFIG_MAXVALLEN - strlen(status[i].value);
		strncat(status[i].value, pool_config->primary_routing_query_pattern_list[j], len);
		len = POOLCONFIG_MAXVALLEN - strlen(status[i].value);
		if (j != pool_config->num_primary_routing_query_pattern_list - 1)
			strncat(status[i].value, ";", len);
	}
	StrNCpy(status[i].desc, "query patterns that should be sent to primary node", POOLCONFIG_MAXDESCLEN);
	i++;

	StrNCpy(status[i].name, "disable_load_balance_on_write", POOLCONFIG_MAXNAMELEN);
	snprintf(status[i].value, POOLCONFIG_MAXVALLEN, "%d", pool_config->disable_load_balance_on_write);
	StrNCpy(status[i].desc, "Load balance behavior when write query is received", POOLCONFIG_MAXDESCLEN);
	i++;

	StrNCpy(status[i].name, "dml_adaptive_object_relationship_list", POOLCONFIG_MAXNAMELEN);
	snprintf(status[i].value, POOLCONFIG_MAXVALLEN, "%s", pool_config->dml_adaptive_object_relationship_list);
	StrNCpy(status[i].desc, "list of relationships between objects", POOLCONFIG_MAXDESCLEN);
	i++;

	StrNCpy(status[i].name, "statement_level_load_balance", POOLCONFIG_MAXNAMELEN);
	snprintf(status[i].value, POOLCONFIG_MAXVALLEN, "%d", pool_config->statement_level_load_balance);
	StrNCpy(status[i].desc, "statement level load balancing", POOLCONFIG_MAXDESCLEN);
	i++;

	/* - Streaming - */
	StrNCpy(status[i].name, "sr_check_period", POOLCONFIG_MAXNAMELEN);
	snprintf(status[i].value, POOLCONFIG_MAXVALLEN, "%d", pool_config->sr_check_period);
	StrNCpy(status[i].desc, "sr check period", POOLCONFIG_MAXDESCLEN);
	i++;

	StrNCpy(status[i].name, "sr_check_user", POOLCONFIG_MAXNAMELEN);
	snprintf(status[i].value, POOLCONFIG_MAXVALLEN, "%s", pool_config->sr_check_user);
	StrNCpy(status[i].desc, "sr check user", POOLCONFIG_MAXDESCLEN);
	i++;
#ifdef NOT_USED					/* for security reason */
	StrNCpy(status[i].name, "sr_check_password", POOLCONFIG_MAXNAMELEN);
	snprintf(status[i].value, POOLCONFIG_MAXVALLEN, "%s", pool_config->sr_check_password);
	StrNCpy(status[i].desc, "sr check password", POOLCONFIG_MAXDESCLEN);
	i++;
#endif
	StrNCpy(status[i].name, "sr_check_database", POOLCONFIG_MAXNAMELEN);
	snprintf(status[i].value, POOLCONFIG_MAXVALLEN, "%s", pool_config->sr_check_database);
	StrNCpy(status[i].desc, "sr check database", POOLCONFIG_MAXDESCLEN);
	i++;

	StrNCpy(status[i].name, "delay_threshold", POOLCONFIG_MAXNAMELEN);
	snprintf(status[i].value, POOLCONFIG_MAXVALLEN, INT64_FORMAT, pool_config->delay_threshold);
	StrNCpy(status[i].desc, "standby delay threshold", POOLCONFIG_MAXDESCLEN);
	i++;

	StrNCpy(status[i].name, "delay_threshold_by_time", POOLCONFIG_MAXNAMELEN);
	snprintf(status[i].value, POOLCONFIG_MAXVALLEN, "%d", pool_config->delay_threshold_by_time);
	StrNCpy(status[i].desc, "standby delay threshold by time", POOLCONFIG_MAXDESCLEN);
	i++;

	StrNCpy(status[i].name, "prefer_lower_delay_standby", POOLCONFIG_MAXNAMELEN);
	snprintf(status[i].value, POOLCONFIG_MAXVALLEN, "%d", pool_config->prefer_lower_delay_standby);
	StrNCpy(status[i].desc, "load balancing considering streaming delay", POOLCONFIG_MAXDESCLEN);
	i++;

	/* - Special commands - */
	StrNCpy(status[i].name, "follow_primary_command", POOLCONFIG_MAXNAMELEN);
	snprintf(status[i].value, POOLCONFIG_MAXVALLEN, "%s", pool_config->follow_primary_command);
	StrNCpy(status[i].desc, "follow primary command", POOLCONFIG_MAXDESCLEN);
	i++;

	StrNCpy(status[i].name, "database_redirect_preference_list", POOLCONFIG_MAXNAMELEN);
	snprintf(status[i].value, POOLCONFIG_MAXVALLEN, "%s", pool_config->database_redirect_preference_list);
	StrNCpy(status[i].desc, "redirect by database name", POOLCONFIG_MAXDESCLEN);
	i++;

	StrNCpy(status[i].name, "app_name_redirect_preference_list", POOLCONFIG_MAXNAMELEN);
	snprintf(status[i].value, POOLCONFIG_MAXVALLEN, "%s", pool_config->app_name_redirect_preference_list);
	StrNCpy(status[i].desc, "redirect by application name", POOLCONFIG_MAXDESCLEN);
	i++;

	StrNCpy(status[i].name, "allow_sql_comments", POOLCONFIG_MAXNAMELEN);
	snprintf(status[i].value, POOLCONFIG_MAXVALLEN, "%d", pool_config->allow_sql_comments);
	StrNCpy(status[i].desc, "allow SQL comments", POOLCONFIG_MAXDESCLEN);
	i++;

	/* HEALTH CHECK */

	StrNCpy(status[i].name, "health_check_period", POOLCONFIG_MAXNAMELEN);
	snprintf(status[i].value, POOLCONFIG_MAXVALLEN, "%d", pool_config->health_check_period);
	StrNCpy(status[i].desc, "health check period", POOLCONFIG_MAXDESCLEN);
	i++;

	StrNCpy(status[i].name, "health_check_timeout", POOLCONFIG_MAXNAMELEN);
	snprintf(status[i].value, POOLCONFIG_MAXVALLEN, "%d", pool_config->health_check_timeout);
	StrNCpy(status[i].desc, "health check timeout", POOLCONFIG_MAXDESCLEN);
	i++;

	StrNCpy(status[i].name, "health_check_user", POOLCONFIG_MAXNAMELEN);
	snprintf(status[i].value, POOLCONFIG_MAXVALLEN, "%s", pool_config->health_check_user);
	StrNCpy(status[i].desc, "health check user", POOLCONFIG_MAXDESCLEN);
	i++;
#ifdef NOT_USED					/* for security reason */
	StrNCpy(status[i].name, "health_check_password", POOLCONFIG_MAXNAMELEN);
	snprintf(status[i].value, POOLCONFIG_MAXVALLEN, "%s", pool_config->health_check_password);
	StrNCpy(status[i].desc, "health check password", POOLCONFIG_MAXDESCLEN);
	i++;
#endif
	StrNCpy(status[i].name, "health_check_database", POOLCONFIG_MAXNAMELEN);
	snprintf(status[i].value, POOLCONFIG_MAXVALLEN, "%s", pool_config->health_check_database);
	StrNCpy(status[i].desc, "health check database", POOLCONFIG_MAXDESCLEN);
	i++;

	StrNCpy(status[i].name, "health_check_max_retries", POOLCONFIG_MAXNAMELEN);
	snprintf(status[i].value, POOLCONFIG_MAXVALLEN, "%d", pool_config->health_check_max_retries);
	StrNCpy(status[i].desc, "health check max retries", POOLCONFIG_MAXDESCLEN);
	i++;

	StrNCpy(status[i].name, "health_check_retry_delay", POOLCONFIG_MAXNAMELEN);
	snprintf(status[i].value, POOLCONFIG_MAXVALLEN, "%d", pool_config->health_check_retry_delay);
	StrNCpy(status[i].desc, "health check retry delay", POOLCONFIG_MAXDESCLEN);
	i++;

	StrNCpy(status[i].name, "connect_timeout", POOLCONFIG_MAXNAMELEN);
	snprintf(status[i].value, POOLCONFIG_MAXVALLEN, "%d", pool_config->connect_timeout);
	StrNCpy(status[i].desc, "connect timeout", POOLCONFIG_MAXDESCLEN);
	i++;

	/* FAILOVER AND FAILBACK */

	StrNCpy(status[i].name, "failover_command", POOLCONFIG_MAXNAMELEN);
	snprintf(status[i].value, POOLCONFIG_MAXVALLEN, "%s", pool_config->failover_command);
	StrNCpy(status[i].desc, "failover command", POOLCONFIG_MAXDESCLEN);
	i++;

	StrNCpy(status[i].name, "failback_command", POOLCONFIG_MAXNAMELEN);
	snprintf(status[i].value, POOLCONFIG_MAXVALLEN, "%s", pool_config->failback_command);
	StrNCpy(status[i].desc, "failback command", POOLCONFIG_MAXDESCLEN);
	i++;

	StrNCpy(status[i].name, "failover_on_backend_error", POOLCONFIG_MAXNAMELEN);
	snprintf(status[i].value, POOLCONFIG_MAXVALLEN, "%d", pool_config->failover_on_backend_error);
	StrNCpy(status[i].desc, "failover on backend error", POOLCONFIG_MAXDESCLEN);
	i++;

	StrNCpy(status[i].name, "failover_on_backend_shutdown", POOLCONFIG_MAXNAMELEN);
	snprintf(status[i].value, POOLCONFIG_MAXVALLEN, "%d", pool_config->failover_on_backend_shutdown);
	StrNCpy(status[i].desc, "failover on backend shutdown", POOLCONFIG_MAXDESCLEN);
	i++;

	StrNCpy(status[i].name, "detach_false_primary", POOLCONFIG_MAXNAMELEN);
	snprintf(status[i].value, POOLCONFIG_MAXVALLEN, "%d", pool_config->detach_false_primary);
	StrNCpy(status[i].desc, "detach false primary", POOLCONFIG_MAXDESCLEN);
	i++;

	StrNCpy(status[i].name, "auto_failback", POOLCONFIG_MAXNAMELEN);
	snprintf(status[i].value, POOLCONFIG_MAXVALLEN, "%d", pool_config->auto_failback);
	StrNCpy(status[i].desc, "auto_failback", POOLCONFIG_MAXDESCLEN);
	i++;

	StrNCpy(status[i].name, "auto_failback_interval", POOLCONFIG_MAXNAMELEN);
	snprintf(status[i].value, POOLCONFIG_MAXVALLEN, "%d", pool_config->auto_failback_interval);
	StrNCpy(status[i].desc, "auto_failback_interval", POOLCONFIG_MAXDESCLEN);
	i++;

	/* ONLINE RECOVERY */

	StrNCpy(status[i].name, "recovery_user", POOLCONFIG_MAXNAMELEN);
	snprintf(status[i].value, POOLCONFIG_MAXVALLEN, "%s", pool_config->recovery_user);
	StrNCpy(status[i].desc, "online recovery user", POOLCONFIG_MAXDESCLEN);
	i++;

	StrNCpy(status[i].name, "recovery_1st_stage_command", POOLCONFIG_MAXNAMELEN);
	snprintf(status[i].value, POOLCONFIG_MAXVALLEN, "%s", pool_config->recovery_1st_stage_command);
	StrNCpy(status[i].desc, "execute a command in first stage.", POOLCONFIG_MAXDESCLEN);
	i++;

	StrNCpy(status[i].name, "recovery_2nd_stage_command", POOLCONFIG_MAXNAMELEN);
	snprintf(status[i].value, POOLCONFIG_MAXVALLEN, "%s", pool_config->recovery_2nd_stage_command);
	StrNCpy(status[i].desc, "execute a command in second stage.", POOLCONFIG_MAXDESCLEN);
	i++;

	StrNCpy(status[i].name, "recovery_timeout", POOLCONFIG_MAXNAMELEN);
	snprintf(status[i].value, POOLCONFIG_MAXVALLEN, "%d", pool_config->recovery_timeout);
	StrNCpy(status[i].desc, "max time in seconds to wait for the recovering node's postmaster", POOLCONFIG_MAXDESCLEN);
	i++;

	StrNCpy(status[i].name, "search_primary_node_timeout", POOLCONFIG_MAXNAMELEN);
	snprintf(status[i].value, POOLCONFIG_MAXVALLEN, "%d", pool_config->search_primary_node_timeout);
	StrNCpy(status[i].desc, "max time in seconds to search for primary node after failover", POOLCONFIG_MAXDESCLEN);
	i++;

	StrNCpy(status[i].name, "client_idle_limit_in_recovery", POOLCONFIG_MAXNAMELEN);
	snprintf(status[i].value, POOLCONFIG_MAXVALLEN, "%d", pool_config->client_idle_limit_in_recovery);
	StrNCpy(status[i].desc, "if idle for this seconds, child connection closes in recovery 2nd stage", POOLCONFIG_MAXDESCLEN);
	i++;

	/* OTHERS */

	StrNCpy(status[i].name, "relcache_expire", POOLCONFIG_MAXNAMELEN);
	snprintf(status[i].value, POOLCONFIG_MAXVALLEN, "%ld", pool_config->relcache_expire);
	StrNCpy(status[i].desc, "relation cache expiration time in seconds", POOLCONFIG_MAXDESCLEN);
	i++;

	StrNCpy(status[i].name, "relcache_size", POOLCONFIG_MAXNAMELEN);
	snprintf(status[i].value, POOLCONFIG_MAXVALLEN, "%d", pool_config->relcache_size);
	StrNCpy(status[i].desc, "number of relation cache entry", POOLCONFIG_MAXDESCLEN);
	i++;

	StrNCpy(status[i].name, "check_temp_table", POOLCONFIG_MAXNAMELEN);
	snprintf(status[i].value, POOLCONFIG_MAXVALLEN, "%d", pool_config->check_temp_table);
	StrNCpy(status[i].desc, "enable temporary table check", POOLCONFIG_MAXDESCLEN);
	i++;

	StrNCpy(status[i].name, "check_unlogged_table", POOLCONFIG_MAXNAMELEN);
	snprintf(status[i].value, POOLCONFIG_MAXVALLEN, "%d", pool_config->check_unlogged_table);
	StrNCpy(status[i].desc, "enable unlogged table check", POOLCONFIG_MAXDESCLEN);
	i++;

	StrNCpy(status[i].name, "enable_shared_relcache", POOLCONFIG_MAXNAMELEN);
	snprintf(status[i].value, POOLCONFIG_MAXVALLEN, "%d", pool_config->enable_shared_relcache);
	StrNCpy(status[i].desc, "If true, relation cache stored in memory cache", POOLCONFIG_MAXDESCLEN);
	i++;

	StrNCpy(status[i].name, "relcache_query_target", POOLCONFIG_MAXNAMELEN);
	snprintf(status[i].value, POOLCONFIG_MAXVALLEN, "%d", pool_config->relcache_query_target);
	StrNCpy(status[i].desc, "Target node to send relcache queries", POOLCONFIG_MAXDESCLEN);
	i++;

	/*
	 * add for watchdog
	 */
	StrNCpy(status[i].name, "use_watchdog", POOLCONFIG_MAXNAMELEN);
	snprintf(status[i].value, POOLCONFIG_MAXVALLEN, "%d", pool_config->use_watchdog);
	StrNCpy(status[i].desc, "non 0 if operating in use_watchdog", POOLCONFIG_MAXDESCLEN);
	i++;

	StrNCpy(status[i].name, "wd_ipc_socket_dir", POOLCONFIG_MAXNAMELEN);
	snprintf(status[i].value, POOLCONFIG_MAXVALLEN, "%s", pool_config->wd_ipc_socket_dir);
	StrNCpy(status[i].desc, "watchdog ipc socket directory", POOLCONFIG_MAXDESCLEN);
	i++;

	StrNCpy(status[i].name, "wd_lifecheck_method", POOLCONFIG_MAXNAMELEN);
	snprintf(status[i].value, POOLCONFIG_MAXVALLEN, "%d", pool_config->wd_lifecheck_method);
	StrNCpy(status[i].desc, "method of watchdog lifecheck", POOLCONFIG_MAXDESCLEN);
	i++;

	StrNCpy(status[i].name, "clear_memqcache_on_escalation", POOLCONFIG_MAXNAMELEN);
	snprintf(status[i].value, POOLCONFIG_MAXVALLEN, "%d", pool_config->clear_memqcache_on_escalation);
	StrNCpy(status[i].desc, "If true, clear all the query caches in shared memory when escalation occurs", POOLCONFIG_MAXDESCLEN);
	i++;

	StrNCpy(status[i].name, "wd_escalation_command", POOLCONFIG_MAXNAMELEN);
	snprintf(status[i].value, POOLCONFIG_MAXVALLEN, "%s", pool_config->wd_escalation_command);
	StrNCpy(status[i].desc, "command executed when escalation occurs", POOLCONFIG_MAXDESCLEN);
	i++;

	StrNCpy(status[i].name, "wd_de_escalation_command", POOLCONFIG_MAXNAMELEN);
	snprintf(status[i].value, POOLCONFIG_MAXVALLEN, "%s", pool_config->wd_de_escalation_command);
	StrNCpy(status[i].desc, "command executed when leader pgpool resigns occurs", POOLCONFIG_MAXDESCLEN);
	i++;

	StrNCpy(status[i].name, "trusted_servers", POOLCONFIG_MAXNAMELEN);
	snprintf(status[i].value, POOLCONFIG_MAXVALLEN, "%s", pool_config->trusted_servers);
	StrNCpy(status[i].desc, "upper server list to observe connection", POOLCONFIG_MAXDESCLEN);
	i++;

	StrNCpy(status[i].name, "trusted_server_command", POOLCONFIG_MAXNAMELEN);
	snprintf(status[i].value, POOLCONFIG_MAXVALLEN, "%s", pool_config->trusted_server_command);
	StrNCpy(status[i].desc, "command executed when upper servers are observed", POOLCONFIG_MAXDESCLEN);
	i++;

	StrNCpy(status[i].name, "delegate_ip", POOLCONFIG_MAXNAMELEN);
	snprintf(status[i].value, POOLCONFIG_MAXVALLEN, "%s", pool_config->delegate_ip);
	StrNCpy(status[i].desc, "delegate IP address of leader pgpool", POOLCONFIG_MAXDESCLEN);
	i++;

	StrNCpy(status[i].name, "wd_priority", POOLCONFIG_MAXNAMELEN);
	snprintf(status[i].value, POOLCONFIG_MAXVALLEN, "%d", pool_config->wd_priority);
	StrNCpy(status[i].desc, "watchdog priority", POOLCONFIG_MAXDESCLEN);
	i++;

	StrNCpy(status[i].name, "wd_interval", POOLCONFIG_MAXNAMELEN);
	snprintf(status[i].value, POOLCONFIG_MAXVALLEN, "%d", pool_config->wd_interval);
	StrNCpy(status[i].desc, "life check interval (second)", POOLCONFIG_MAXDESCLEN);
	i++;

	StrNCpy(status[i].name, "ping_path", POOLCONFIG_MAXNAMELEN);
	snprintf(status[i].value, POOLCONFIG_MAXVALLEN, "%s", pool_config->ping_path);
	StrNCpy(status[i].desc, "path to ping command", POOLCONFIG_MAXDESCLEN);
	i++;

	StrNCpy(status[i].name, "if_cmd_path", POOLCONFIG_MAXNAMELEN);
	snprintf(status[i].value, POOLCONFIG_MAXVALLEN, "%s", pool_config->if_cmd_path);
	StrNCpy(status[i].desc, "path to interface up/down command", POOLCONFIG_MAXDESCLEN);
	i++;

	StrNCpy(status[i].name, "if_up_cmd", POOLCONFIG_MAXNAMELEN);
	snprintf(status[i].value, POOLCONFIG_MAXVALLEN, "%s", pool_config->if_up_cmd);
	StrNCpy(status[i].desc, "virtual interface up command with full parameters", POOLCONFIG_MAXDESCLEN);
	i++;

	StrNCpy(status[i].name, "if_down_cmd", POOLCONFIG_MAXNAMELEN);
	snprintf(status[i].value, POOLCONFIG_MAXVALLEN, "%s", pool_config->if_down_cmd);
	StrNCpy(status[i].desc, "virtual interface down command with full parameters", POOLCONFIG_MAXDESCLEN);
	i++;

	StrNCpy(status[i].name, "arping_path", POOLCONFIG_MAXNAMELEN);
	snprintf(status[i].value, POOLCONFIG_MAXVALLEN, "%s", pool_config->arping_path);
	StrNCpy(status[i].desc, "path to arping command", POOLCONFIG_MAXDESCLEN);
	i++;

	StrNCpy(status[i].name, "arping_cmd", POOLCONFIG_MAXNAMELEN);
	snprintf(status[i].value, POOLCONFIG_MAXVALLEN, "%s", pool_config->arping_cmd);
	StrNCpy(status[i].desc, "send ARP REQUEST to neighbour host", POOLCONFIG_MAXDESCLEN);
	i++;

	StrNCpy(status[i].name, "wd_heartbeat_keepalive", POOLCONFIG_MAXNAMELEN);
	snprintf(status[i].value, POOLCONFIG_MAXVALLEN, "%d", pool_config->wd_heartbeat_keepalive);
	StrNCpy(status[i].desc, "interval time of sending heartbeat signal (sec)", POOLCONFIG_MAXDESCLEN);
	i++;

	StrNCpy(status[i].name, "wd_heartbeat_deadtime", POOLCONFIG_MAXNAMELEN);
	snprintf(status[i].value, POOLCONFIG_MAXVALLEN, "%d", pool_config->wd_heartbeat_deadtime);
	StrNCpy(status[i].desc, "deadtime interval for heartbeat signal (sec)", POOLCONFIG_MAXDESCLEN);
	i++;

	StrNCpy(status[i].name, "wd_life_point", POOLCONFIG_MAXNAMELEN);
	snprintf(status[i].value, POOLCONFIG_MAXVALLEN, "%d", pool_config->wd_life_point);
	StrNCpy(status[i].desc, "retry times of life check", POOLCONFIG_MAXDESCLEN);
	i++;

	StrNCpy(status[i].name, "wd_lifecheck_query", POOLCONFIG_MAXNAMELEN);
	snprintf(status[i].value, POOLCONFIG_MAXVALLEN, "%s", pool_config->wd_lifecheck_query);
	StrNCpy(status[i].desc, "lifecheck query to pgpool from watchdog", POOLCONFIG_MAXDESCLEN);
	i++;

	StrNCpy(status[i].name, "wd_lifecheck_dbname", POOLCONFIG_MAXNAMELEN);
	snprintf(status[i].value, POOLCONFIG_MAXVALLEN, "%s", pool_config->wd_lifecheck_dbname);
	StrNCpy(status[i].desc, "database name connected for lifecheck", POOLCONFIG_MAXDESCLEN);
	i++;

	StrNCpy(status[i].name, "wd_lifecheck_user", POOLCONFIG_MAXNAMELEN);
	snprintf(status[i].value, POOLCONFIG_MAXVALLEN, "%s", pool_config->wd_lifecheck_user);
	StrNCpy(status[i].desc, "watchdog user monitoring pgpools in lifecheck", POOLCONFIG_MAXDESCLEN);
	i++;

	StrNCpy(status[i].name, "wd_lifecheck_password", POOLCONFIG_MAXNAMELEN);
	snprintf(status[i].value, POOLCONFIG_MAXVALLEN, "%s", pool_config->wd_lifecheck_password);
	StrNCpy(status[i].desc, "password for watchdog user in lifecheck", POOLCONFIG_MAXDESCLEN);
	i++;

	for (j = 0; j < MAX_WATCHDOG_NUM; j++)
	{
		if (WD_INFO(j).pgpool_port == 0)
			continue;

		if (j == pool_config->pgpool_node_id)
		{
			snprintf(status[i].name, POOLCONFIG_MAXNAMELEN, "hostname%d", j);
			snprintf(status[i].value, POOLCONFIG_MAXVALLEN, "%s", WD_INFO(j).hostname);
			StrNCpy(status[i].desc, "hostname or IP address of local pgpool node", POOLCONFIG_MAXDESCLEN);
			i++;

			snprintf(status[i].name, POOLCONFIG_MAXNAMELEN, "pgpool_port%d", j);
			snprintf(status[i].value, POOLCONFIG_MAXVALLEN, "%d", WD_INFO(j).pgpool_port);
			StrNCpy(status[i].desc, "local pgpool port number", POOLCONFIG_MAXDESCLEN);
			i++;

			snprintf(status[i].name, POOLCONFIG_MAXNAMELEN, "wd_port%d", j);
			snprintf(status[i].value, POOLCONFIG_MAXVALLEN, "%d", WD_INFO(j).wd_port);
			StrNCpy(status[i].desc, "local watchdog port number", POOLCONFIG_MAXDESCLEN);
			i++;
		}
		else
		{
			snprintf(status[i].name, POOLCONFIG_MAXNAMELEN, "hostname%d", j);
			snprintf(status[i].value, POOLCONFIG_MAXVALLEN, "%s", WD_INFO(j).hostname);
			snprintf(status[i].desc, POOLCONFIG_MAXDESCLEN, "hostname or IP address of pgpool node #%d", j);
			i++;

			snprintf(status[i].name, POOLCONFIG_MAXNAMELEN, "pgpool_port%d", j);
			snprintf(status[i].value, POOLCONFIG_MAXVALLEN, "%d", WD_INFO(j).pgpool_port);
			snprintf(status[i].desc, POOLCONFIG_MAXDESCLEN, "pgpool port number of pgpool node #%d", j);
			i++;

			snprintf(status[i].name, POOLCONFIG_MAXNAMELEN, "wd_port%d", j);
			snprintf(status[i].value, POOLCONFIG_MAXVALLEN, "%d", WD_INFO(j).wd_port);
			snprintf(status[i].desc, POOLCONFIG_MAXDESCLEN, "watchdog port number of pgpool node #%d", j);
			i++;
		}
	}

	for (j = 0; j < MAX_WATCHDOG_NUM; j++)
	{
		if (pool_config->hb_ifs[j].dest_port == 0)
			continue;

		if (j == pool_config->pgpool_node_id)
		{
			snprintf(status[i].name, POOLCONFIG_MAXNAMELEN, "heartbeat_hostname%d", j);
			snprintf(status[i].value, POOLCONFIG_MAXVALLEN, "%s", pool_config->hb_ifs[j].addr);
			StrNCpy(status[i].desc, "IP address or hostname for receiving heartbeat", POOLCONFIG_MAXDESCLEN);
			i++;

			snprintf(status[i].name, POOLCONFIG_MAXNAMELEN, "heartbeat_device%d", j);
			snprintf(status[i].value, POOLCONFIG_MAXVALLEN, "%s", pool_config->hb_ifs[j].if_name);
			StrNCpy(status[i].desc, "name of NIC device for receiving hearbeat", POOLCONFIG_MAXDESCLEN);
			i++;

			snprintf(status[i].name, POOLCONFIG_MAXNAMELEN, "heartbeat_port%d", j);
			snprintf(status[i].value, POOLCONFIG_MAXVALLEN, "%d", pool_config->hb_ifs[j].dest_port);
			StrNCpy(status[i].desc, "port number for receiving heartbeat", POOLCONFIG_MAXDESCLEN);
			i++;
		}
		else
		{
			snprintf(status[i].name, POOLCONFIG_MAXNAMELEN, "heartbeat_hostname%d", j);
			snprintf(status[i].value, POOLCONFIG_MAXVALLEN, "%s", pool_config->hb_ifs[j].addr);
			snprintf(status[i].desc, POOLCONFIG_MAXDESCLEN, "destination IP address or hostname for sending heartbeat to pgpool node #%d", j);
			i++;

			snprintf(status[i].name, POOLCONFIG_MAXNAMELEN, "heartbeat_device%d", j);
			snprintf(status[i].value, POOLCONFIG_MAXVALLEN, "%s", pool_config->hb_ifs[j].if_name);
			snprintf(status[i].desc, POOLCONFIG_MAXDESCLEN, "name of NIC device for sending hearbeat to pgpool node #%d", j);
			i++;

			snprintf(status[i].name, POOLCONFIG_MAXNAMELEN, "heartbeat_port%d", j);
			snprintf(status[i].value, POOLCONFIG_MAXVALLEN, "%d", pool_config->hb_ifs[j].dest_port);
			snprintf(status[i].desc, POOLCONFIG_MAXDESCLEN, "destination port for sending heartbeat to pgpool node #%d", j);
			i++;
		}
	}

	StrNCpy(status[i].name, "wd_monitoring_interfaces_list", POOLCONFIG_MAXNAMELEN);
	*(status[i].value) = '\0';
	for (j = 0; j < pool_config->num_wd_monitoring_interfaces_list; j++)
	{
		len = POOLCONFIG_MAXVALLEN - strlen(status[i].value);
		strncat(status[i].value, pool_config->wd_monitoring_interfaces_list[j], len);
		len = POOLCONFIG_MAXVALLEN - strlen(status[i].value);
		if (j != pool_config->num_wd_monitoring_interfaces_list - 1)
			strncat(status[i].value, ",", len);
	}
	StrNCpy(status[i].desc, "interfaces to monitor by watchdog", POOLCONFIG_MAXDESCLEN);
	i++;

	/*
	 * end of watchdog
	 */

	StrNCpy(status[i].name, "memory_cache_enabled", POOLCONFIG_MAXNAMELEN);
	snprintf(status[i].value, POOLCONFIG_MAXVALLEN, "%d", pool_config->memory_cache_enabled);
	StrNCpy(status[i].desc, "If true, use the memory cache functionality, false by default", POOLCONFIG_MAXDESCLEN);
	i++;

	StrNCpy(status[i].name, "memqcache_method", POOLCONFIG_MAXNAMELEN);
	snprintf(status[i].value, POOLCONFIG_MAXVALLEN, "%d", pool_config->memqcache_method);
	StrNCpy(status[i].desc, "Cache store method. either shmem(shared memory) or Memcached. shmem by default", POOLCONFIG_MAXDESCLEN);
	i++;

	StrNCpy(status[i].name, "memqcache_memcached_host", POOLCONFIG_MAXNAMELEN);
	snprintf(status[i].value, POOLCONFIG_MAXVALLEN, "%s", pool_config->memqcache_memcached_host);
	StrNCpy(status[i].desc, "Memcached host name. Mandatory if memqcache_method=memcached", POOLCONFIG_MAXDESCLEN);
	i++;

	StrNCpy(status[i].name, "memqcache_memcached_port", POOLCONFIG_MAXNAMELEN);
	snprintf(status[i].value, POOLCONFIG_MAXVALLEN, "%d", pool_config->memqcache_memcached_port);
	StrNCpy(status[i].desc, "Memcached port number. Mandatory if memqcache_method=memcached", POOLCONFIG_MAXDESCLEN);
	i++;

	StrNCpy(status[i].name, "memqcache_total_size", POOLCONFIG_MAXNAMELEN);
	snprintf(status[i].value, POOLCONFIG_MAXVALLEN, "%ld", pool_config->memqcache_total_size);
	StrNCpy(status[i].desc, "Total memory size in bytes for storing memory cache. Mandatory if memqcache_method=shmem", POOLCONFIG_MAXDESCLEN);
	i++;

	StrNCpy(status[i].name, "memqcache_max_num_cache", POOLCONFIG_MAXNAMELEN);
	snprintf(status[i].value, POOLCONFIG_MAXVALLEN, "%d", pool_config->memqcache_max_num_cache);
	StrNCpy(status[i].desc, "Total number of cache entries", POOLCONFIG_MAXDESCLEN);
	i++;

	StrNCpy(status[i].name, "memqcache_expire", POOLCONFIG_MAXNAMELEN);
	snprintf(status[i].value, POOLCONFIG_MAXVALLEN, "%d", pool_config->memqcache_expire);
	StrNCpy(status[i].desc, "Memory cache entry life time specified in seconds. 60 by default", POOLCONFIG_MAXDESCLEN);
	i++;

	StrNCpy(status[i].name, "memqcache_auto_cache_invalidation", POOLCONFIG_MAXNAMELEN);
	snprintf(status[i].value, POOLCONFIG_MAXVALLEN, "%d", pool_config->memqcache_auto_cache_invalidation);
	StrNCpy(status[i].desc, "If true, invalidation of query cache is triggered by corresponding DDL/DML/DCL(and memqcache_expire).  If false, it is only triggered  by memqcache_expire.  True by default.", POOLCONFIG_MAXDESCLEN);
	i++;

	StrNCpy(status[i].name, "memqcache_maxcache", POOLCONFIG_MAXNAMELEN);
	snprintf(status[i].value, POOLCONFIG_MAXVALLEN, "%d", pool_config->memqcache_maxcache);
	StrNCpy(status[i].desc, "Maximum SELECT result size in bytes", POOLCONFIG_MAXDESCLEN);
	i++;

	StrNCpy(status[i].name, "memqcache_cache_block_size", POOLCONFIG_MAXNAMELEN);
	snprintf(status[i].value, POOLCONFIG_MAXVALLEN, "%d", pool_config->memqcache_cache_block_size);
	StrNCpy(status[i].desc, "Cache block size in bytes. 8192 by default", POOLCONFIG_MAXDESCLEN);
	i++;

	StrNCpy(status[i].name, "memqcache_cache_oiddir", POOLCONFIG_MAXNAMELEN);
	snprintf(status[i].value, POOLCONFIG_MAXVALLEN, "%s", pool_config->memqcache_oiddir);
	StrNCpy(status[i].desc, "Temporary work directory to record table oids", POOLCONFIG_MAXDESCLEN);
	i++;

	StrNCpy(status[i].name, "memqcache_stats_start_time", POOLCONFIG_MAXNAMELEN);
	snprintf(status[i].value, POOLCONFIG_MAXVALLEN, "%s", ctime(&pool_get_memqcache_stats()->start_time));
	StrNCpy(status[i].desc, "Start time of query cache stats", POOLCONFIG_MAXDESCLEN);
	i++;

	StrNCpy(status[i].name, "memqcache_no_cache_hits", POOLCONFIG_MAXNAMELEN);
	snprintf(status[i].value, POOLCONFIG_MAXVALLEN, "%lld", pool_get_memqcache_stats()->num_selects);
	StrNCpy(status[i].desc, "Number of SELECTs not hitting query cache", POOLCONFIG_MAXDESCLEN);
	i++;

	StrNCpy(status[i].name, "memqcache_cache_hits", POOLCONFIG_MAXNAMELEN);
	snprintf(status[i].value, POOLCONFIG_MAXVALLEN, "%lld", pool_get_memqcache_stats()->num_cache_hits);
	StrNCpy(status[i].desc, "Number of SELECTs hitting query cache", POOLCONFIG_MAXDESCLEN);
	i++;

	StrNCpy(status[i].name, "cache_safe_memqcache_table_list", POOLCONFIG_MAXNAMELEN);
	*(status[i].value) = '\0';
	for (j = 0; j < pool_config->num_cache_safe_memqcache_table_list; j++)
	{
		len = POOLCONFIG_MAXVALLEN - strlen(status[i].value);
		strncat(status[i].value, pool_config->cache_safe_memqcache_table_list[j], len);
		len = POOLCONFIG_MAXVALLEN - strlen(status[i].value);
		if (j != pool_config->num_cache_safe_memqcache_table_list - 1)
			strncat(status[i].value, ",", len);
	}
	StrNCpy(status[i].desc, "tables to memqcache", POOLCONFIG_MAXDESCLEN);
	i++;

	StrNCpy(status[i].name, "cache_unsafe_memqcache_table_list", POOLCONFIG_MAXNAMELEN);
	*(status[i].value) = '\0';
	for (j = 0; j < pool_config->num_cache_unsafe_memqcache_table_list; j++)
	{
		len = POOLCONFIG_MAXVALLEN - strlen(status[i].value);
		strncat(status[i].value, pool_config->cache_unsafe_memqcache_table_list[j], len);
		len = POOLCONFIG_MAXVALLEN - strlen(status[i].value);
		if (j != pool_config->num_cache_unsafe_memqcache_table_list - 1)
			strncat(status[i].value, ",", len);
	}
	StrNCpy(status[i].desc, "tables not to memqcache", POOLCONFIG_MAXDESCLEN);
	i++;

	/* BACKENDS */

	for (j = 0; j < NUM_BACKENDS; j++)
	{
		if (BACKEND_INFO(j).backend_port == 0)
			continue;

		snprintf(status[i].name, POOLCONFIG_MAXNAMELEN, "backend_hostname%d", j);
		snprintf(status[i].value, POOLCONFIG_MAXVALLEN, "%s", BACKEND_INFO(j).backend_hostname);
		snprintf(status[i].desc, POOLCONFIG_MAXDESCLEN, "backend #%d hostname", j);
		i++;

		snprintf(status[i].name, POOLCONFIG_MAXNAMELEN, "backend_port%d", j);
		snprintf(status[i].value, POOLCONFIG_MAXVALLEN, "%d", BACKEND_INFO(j).backend_port);
		snprintf(status[i].desc, POOLCONFIG_MAXDESCLEN, "backend #%d port number", j);
		i++;

		snprintf(status[i].name, POOLCONFIG_MAXNAMELEN, "backend_weight%d", j);
		snprintf(status[i].value, POOLCONFIG_MAXVALLEN, "%f", BACKEND_INFO(j).backend_weight / RAND_MAX);
		snprintf(status[i].desc, POOLCONFIG_MAXDESCLEN, "weight of backend #%d", j);
		i++;

		snprintf(status[i].name, POOLCONFIG_MAXNAMELEN, "backend_data_directory%d", j);
		snprintf(status[i].value, POOLCONFIG_MAXVALLEN, "%s", BACKEND_INFO(j).backend_data_directory);
		snprintf(status[i].desc, POOLCONFIG_MAXDESCLEN, "data directory for backend #%d", j);
		i++;

		snprintf(status[i].name, POOLCONFIG_MAXNAMELEN, "backend_status%d", j);
		snprintf(status[i].value, POOLCONFIG_MAXVALLEN, "%s", backend_status_to_str(&BACKEND_INFO(j)));
		snprintf(status[i].desc, POOLCONFIG_MAXDESCLEN, "status of backend #%d", j);
		i++;

		snprintf(status[i].name, POOLCONFIG_MAXNAMELEN, "standby_delay%d", j);
		snprintf(status[i].value, POOLCONFIG_MAXVALLEN, UINT64_FORMAT, BACKEND_INFO(j).standby_delay);
		snprintf(status[i].desc, POOLCONFIG_MAXDESCLEN, "standby delay of backend #%d", j);
		i++;

		snprintf(status[i].name, POOLCONFIG_MAXNAMELEN, "backend_flag%d", j);
		snprintf(status[i].value, POOLCONFIG_MAXVALLEN, "%s", pool_flag_to_str(BACKEND_INFO(j).flag));
		snprintf(status[i].desc, POOLCONFIG_MAXDESCLEN, "backend #%d flag", j);
		i++;

		snprintf(status[i].name, POOLCONFIG_MAXNAMELEN, "backend_application_name%d", j);
		snprintf(status[i].value, POOLCONFIG_MAXVALLEN, "%s", BACKEND_INFO(j).backend_application_name);
		snprintf(status[i].desc, POOLCONFIG_MAXDESCLEN, "application_name for backend #%d", j);
		i++;
	}

	*nrows = i;
	return status;
}

void
send_config_var_detail_row(POOL_CONNECTION * frontend, POOL_CONNECTION_POOL * backend, const char *name, const char *value, const char *description)
{
	int			size;
	int			hsize;
	static short num_fields = 3;

	if (MAJOR(backend) == PROTO_MAJOR_V2)
	{

		int			nbytes = (num_fields + 7) / 8;
		static unsigned char nullmap[2] = {0xff, 0xff};

		/* ascii row */
		pool_write(frontend, "D", 1);
		pool_write_and_flush(frontend, nullmap, nbytes);

		size = strlen(name);
		hsize = htonl(size + 4);
		pool_write(frontend, &hsize, sizeof(hsize));
		pool_write(frontend, (void *) name, size);

		size = strlen(value);
		hsize = htonl(size + 4);
		pool_write(frontend, &hsize, sizeof(hsize));
		pool_write(frontend, (void *) value, size);

		size = strlen(description);
		hsize = htonl(size + 4);
		pool_write(frontend, &hsize, sizeof(hsize));
		pool_write(frontend, (void *) description, size);
	}
	else
	{
		short		s;

		pool_write(frontend, "D", 1);
		size = 6;				/* int32 + int16; */
		size += 4 + strlen(name);	/* int32 + data; */
		size += 4 + strlen(value);	/* int32 + data; */
		size += 4 + strlen(description);	/* int32 + data; */
		hsize = htonl(size);
		pool_write(frontend, &hsize, sizeof(hsize));
		s = htons(num_fields);
		pool_write(frontend, &s, sizeof(s));

		size = strlen(name);
		hsize = htonl(size);
		pool_write(frontend, &hsize, sizeof(hsize));
		pool_write(frontend, (void *) name, size);

		size = strlen(value);
		hsize = htonl(size);
		pool_write(frontend, &hsize, sizeof(hsize));
		pool_write(frontend, (void *) value, size);

		size = strlen(description);
		hsize = htonl(size);
		pool_write(frontend, &hsize, sizeof(hsize));
		pool_write(frontend, (void *) description, size);
	}
}

void
send_config_var_value_only_row(POOL_CONNECTION * frontend, POOL_CONNECTION_POOL * backend, const char *value)
{
	int			size;
	int			hsize;
	static short num_fields = 1;

	if (MAJOR(backend) == PROTO_MAJOR_V2)
	{

		int			nbytes = (num_fields + 7) / 8;
		static unsigned char nullmap[2] = {0xff, 0xff};

		/* ascii row */
		pool_write(frontend, "D", 1);
		pool_write_and_flush(frontend, nullmap, nbytes);

		size = strlen(value);
		hsize = htonl(size + 4);
		pool_write(frontend, &hsize, sizeof(hsize));
		pool_write(frontend, (void *) value, size);

	}
	else
	{
		short		s;

		pool_write(frontend, "D", 1);
		size = 6;				/* int32 + int16; */
		size += 4 + strlen(value);	/* int32 + data; */
		hsize = htonl(size);
		pool_write(frontend, &hsize, sizeof(hsize));
		s = htons(num_fields);
		pool_write(frontend, &s, sizeof(s));

		size = strlen(value);
		hsize = htonl(size);
		pool_write(frontend, &hsize, sizeof(hsize));
		pool_write(frontend, (void *) value, size);
	}
}

void
config_reporting(POOL_CONNECTION * frontend, POOL_CONNECTION_POOL * backend)
{
	static char *field_names[] = {"item", "value", "description"};
	static unsigned char nullmap[2] = {0xff, 0xff};
	static short num_fields = 3;
	short		s;
	int			nbytes = (num_fields + 7) / 8;
	int			len;
	int			nrows;
	int			size;
	int			hsize;
	int			i;

	POOL_REPORT_CONFIG *status = get_config(&nrows);

	send_row_description(frontend, backend, num_fields, field_names);

	if (MAJOR(backend) == PROTO_MAJOR_V2)
	{
		/* ascii row */
		for (i = 0; i < nrows; i++)
		{
			pool_write(frontend, "D", 1);
			pool_write_and_flush(frontend, nullmap, nbytes);

			size = strlen(status[i].name);
			hsize = htonl(size + 4);
			pool_write(frontend, &hsize, sizeof(hsize));
			pool_write(frontend, status[i].name, size);

			size = strlen(status[i].value);
			hsize = htonl(size + 4);
			pool_write(frontend, &hsize, sizeof(hsize));
			pool_write(frontend, status[i].value, size);

			size = strlen(status[i].desc);
			hsize = htonl(size + 4);
			pool_write(frontend, &hsize, sizeof(hsize));
			pool_write(frontend, status[i].desc, size);
		}
	}
	else
	{
		/* data row */
		for (i = 0; i < nrows; i++)
		{
			pool_write(frontend, "D", 1);
			len = 6;			/* int32 + int16; */
			len += 4 + strlen(status[i].name);	/* int32 + data; */
			len += 4 + strlen(status[i].value); /* int32 + data; */
			len += 4 + strlen(status[i].desc);	/* int32 + data; */
			len = htonl(len);
			pool_write(frontend, &len, sizeof(len));
			s = htons(num_fields);
			pool_write(frontend, &s, sizeof(s));

			len = htonl(strlen(status[i].name));
			pool_write(frontend, &len, sizeof(len));
			pool_write(frontend, status[i].name, strlen(status[i].name));

			len = htonl(strlen(status[i].value));
			pool_write(frontend, &len, sizeof(len));
			pool_write(frontend, status[i].value, strlen(status[i].value));

			len = htonl(strlen(status[i].desc));
			pool_write(frontend, &len, sizeof(len));
			pool_write(frontend, status[i].desc, strlen(status[i].desc));
		}
	}

	send_complete_and_ready(frontend, backend, "SELECT", nrows);

	pfree(status);
}

/*
 * for SHOW pool_nodes
 * If node_id is -1, get all node info.
 */
POOL_REPORT_NODES *
get_nodes(int *nrows, int node_id)
{
	int			i;
	POOL_REPORT_NODES *nodes = palloc(NUM_BACKENDS * sizeof(POOL_REPORT_NODES));
	BackendInfo *bi = NULL;
	POOL_SESSION_CONTEXT *session_context = pool_get_session_context(true);
	struct tm	tm;

	for (i = 0; i < NUM_BACKENDS; i++)
	{
		if (node_id != -1 && node_id != i)
			continue;

		bi = pool_get_node_info(i);

		snprintf(nodes[i].node_id, POOLCONFIG_MAXIDLEN, "%d", i);
		StrNCpy(nodes[i].hostname, bi->backend_hostname, MAX_DB_HOST_NAMELEN);
		snprintf(nodes[i].port, POOLCONFIG_MAXPORTLEN, "%d", bi->backend_port);
		snprintf(nodes[i].status, POOLCONFIG_MAXSTATLEN, "%s", backend_status_to_str(bi));
		snprintf(nodes[i].lb_weight, POOLCONFIG_MAXWEIGHTLEN, "%f", bi->backend_weight / RAND_MAX);
		snprintf(nodes[i].select, POOLCONFIG_MAXWEIGHTLEN, UINT64_FORMAT, stat_get_select_count(i));
		if (session_context)
			snprintf(nodes[i].load_balance_node, POOLCONFIG_MAXWEIGHTLEN, "%s",
					 (session_context->load_balance_node_id == i) ? "true" : "false");
		else
			snprintf(nodes[i].load_balance_node, POOLCONFIG_MAXWEIGHTLEN, "%s", "false");

		snprintf(nodes[i].delay, POOLCONFIG_MAXWEIGHTLEN, "%d", 0);

		snprintf(nodes[i].pg_status, POOLCONFIG_MAXSTATLEN, "%s", db_node_status(i));

		if (STREAM)
		{
			if (i == REAL_PRIMARY_NODE_ID)
			{
				snprintf(nodes[i].role, POOLCONFIG_MAXWEIGHTLEN, "%s", "primary");
			}
			else
			{
				snprintf(nodes[i].role, POOLCONFIG_MAXWEIGHTLEN, "%s", "standby");

				if (bi->standby_delay_by_time)
				{
					snprintf(nodes[i].delay, POOLCONFIG_MAXWEIGHTLEN, "%.6f", ((float)bi->standby_delay)/1000000);
					if (bi->standby_delay >= 2*1000*1000)
						strcat(nodes[i].delay, " seconds");
					else
						strcat(nodes[i].delay, " second");
				}
				else
					snprintf(nodes[i].delay, POOLCONFIG_MAXWEIGHTLEN, UINT64_FORMAT, bi->standby_delay);
			}

			if (!strcmp(nodes[i].pg_status, "up"))
			{
				snprintf(nodes[i].pg_role, POOLCONFIG_MAXWEIGHTLEN, "%s", db_node_role(i));
			}
			else
			{
				snprintf(nodes[i].pg_role, POOLCONFIG_MAXWEIGHTLEN, "unknown");
			}
		}
		else
		{
			if (i == REAL_MAIN_NODE_ID)
				snprintf(nodes[i].role, POOLCONFIG_MAXWEIGHTLEN, "%s", "main");
			else
				snprintf(nodes[i].role, POOLCONFIG_MAXWEIGHTLEN, "%s", "replica");

			snprintf(nodes[i].pg_role, POOLCONFIG_MAXWEIGHTLEN, "%s", nodes[i].role);
		}

		/* status last changed */
		localtime_r(&bi->status_changed_time, &tm);
		strftime(nodes[i].last_status_change, POOLCONFIG_MAXDATELEN, "%F %T", &tm);

		/* from pg_stat_replication */
		snprintf(nodes[i].rep_state, POOLCONFIG_MAXWEIGHTLEN, "%s", bi->replication_state);
		snprintf(nodes[i].rep_sync_state, POOLCONFIG_MAXWEIGHTLEN, "%s", bi->replication_sync_state);
	}

	*nrows = i;

	return nodes;
}

/*
 * SHOW pool_nodes;
 */
void
nodes_reporting(POOL_CONNECTION * frontend, POOL_CONNECTION_POOL * backend)
{
	static char *field_names[] = {"node_id", "hostname", "port", "status", "pg_status", "lb_weight", "role",
								  "pg_role", "select_cnt", "load_balance_node", "replication_delay",
								  "replication_state", "replication_sync_state", "last_status_change"};

	static int offsettbl[] = {
		offsetof(POOL_REPORT_NODES, node_id),
		offsetof(POOL_REPORT_NODES, hostname),
		offsetof(POOL_REPORT_NODES, port),
		offsetof(POOL_REPORT_NODES, status),
		offsetof(POOL_REPORT_NODES, pg_status),
		offsetof(POOL_REPORT_NODES, lb_weight),
		offsetof(POOL_REPORT_NODES, role),
		offsetof(POOL_REPORT_NODES, pg_role),
		offsetof(POOL_REPORT_NODES, select),
		offsetof(POOL_REPORT_NODES, load_balance_node),
		offsetof(POOL_REPORT_NODES, delay),
		offsetof(POOL_REPORT_NODES, rep_state),
		offsetof(POOL_REPORT_NODES, rep_sync_state),
		offsetof(POOL_REPORT_NODES, last_status_change)
	};

	int	nrows;
	short		num_fields;
	POOL_REPORT_NODES *nodes;

	num_fields = sizeof(field_names) / sizeof(char *);
	nodes = get_nodes(&nrows, -1);

	send_row_description_and_data_rows(frontend, backend, num_fields, field_names, offsettbl,
									   (char *)nodes, sizeof(POOL_REPORT_NODES), nrows);

	pfree(nodes);
}


/*
 * Used by pcp_proc_info and SHOW pool_pools
 */
POOL_REPORT_POOLS *
get_pools(int *nrows)
{
	int			child,
				pool,
				poolBE,
				backend_id;

	ProcessInfo *pi = NULL;
	int			proc_id;

	int			lines = 0;

	POOL_REPORT_POOLS *pools = palloc0(
		pool_config->num_init_children * pool_config->max_pool * NUM_BACKENDS * sizeof(POOL_REPORT_POOLS)
	);

	for (child = 0; child < pool_config->num_init_children; child++)
	{
		int exist_live_connection = 0;

		pi = &process_info[child];
		proc_id = pi->pid;

		for (pool = 0; pool < pool_config->max_pool; pool++)
		{
			poolBE = pool * MAX_NUM_BACKENDS;
			if (pi->connection_info[poolBE].connected)
			{
				exist_live_connection = 1;
				break;
			}
		}


		for (pool = 0; pool < pool_config->max_pool; pool++)
		{
			int idle_duration = pi->connection_info[pool * MAX_NUM_BACKENDS].client_idle_duration;
			int cliet_idle_time = pool_config->client_idle_limit;

			if (pool_config->client_idle_limit > 0)
			{
				cliet_idle_time = pool_config->client_idle_limit - idle_duration;
			}

			for (backend_id = 0; backend_id < NUM_BACKENDS; backend_id++)
			{
				poolBE = pool * MAX_NUM_BACKENDS + backend_id;
				snprintf(pools[lines].pool_pid, sizeof(pools[lines].pool_pid), "%d", proc_id);

				if (pi->start_time)
				{
					if ((pool_config->child_life_time > 0)
						&& (pi->connected)
						&& (!exist_live_connection))
					{
						char proc_start_time[POOLCONFIG_MAXDATELEN + 1];
						int wait_for_connect_time = pool_config->child_life_time - pi->wait_for_connect;

						strftime(proc_start_time, sizeof(proc_start_time),
								 "%Y-%m-%d %H:%M:%S", localtime(&pi->start_time));
						snprintf(pools[lines].process_start_time, sizeof(pools[lines].process_start_time),
								 "%s (%d:%02d before process restarting)", proc_start_time,
								 wait_for_connect_time / 60,
								 wait_for_connect_time % 60);
					}
					else
					{
						strftime(pools[lines].process_start_time, sizeof(pools[lines].process_start_time),
								 "%Y-%m-%d %H:%M:%S", localtime(&pi->start_time));
					}
				}
				else
					*(pools[lines].process_start_time) = '\0';

				snprintf(pools[lines].pool_id, sizeof(pools[lines].pool_id), "%d", pool);

				snprintf(pools[lines].backend_id, sizeof(pools[lines].backend_id), "%d", backend_id);

				snprintf(pools[lines].client_connection_count, sizeof(pools[lines].client_connection_count),
						 "%d", pi->client_connection_count);

				if (pi->connection_info[poolBE].client_connection_time == 0)
				{
					*(pools[lines].client_connection_time) = '\0';
				}
				else
				{
					strftime(pools[lines].client_connection_time, sizeof(pools[lines].client_connection_time),
						 "%Y-%m-%d %H:%M:%S", localtime(&pi->connection_info[poolBE].client_connection_time));
				}

				if (pi->connection_info[poolBE].client_disconnection_time == 0)
				{
					*(pools[lines].client_disconnection_time) = '\0';
				}
				else
				{
					strftime(pools[lines].client_disconnection_time, sizeof(pools[lines].client_disconnection_time),
						 "%Y-%m-%d %H:%M:%S", localtime(&pi->connection_info[poolBE].client_disconnection_time));
				}

				if ((pool_config->client_idle_limit > 0)
					&& (pi->connection_info[poolBE].connected))
				{
					snprintf(pools[lines].client_idle_duration, sizeof(pools[lines].client_idle_duration),
							 "%d (%d:%02d before client disconnected)", idle_duration,
							 cliet_idle_time / 60,
							 cliet_idle_time % 60);
				}
				else
					snprintf(pools[lines].client_idle_duration, sizeof(pools[lines].client_idle_duration),
							 "%d", idle_duration);

				if (strlen(pi->connection_info[poolBE].database) == 0)
				{
					StrNCpy(pools[lines].database, "", POOLCONFIG_MAXIDENTLEN);
					StrNCpy(pools[lines].username, "", POOLCONFIG_MAXIDENTLEN);
					*(pools[lines].backend_connection_time) = '\0';
					snprintf(pools[lines].pool_majorversion, sizeof(pools[lines].pool_majorversion), "%d", 0);
					snprintf(pools[lines].pool_minorversion, sizeof(pools[lines].pool_minorversion), "%d", 0);
				}
				else
				{
					StrNCpy(pools[lines].database, pi->connection_info[poolBE].database, POOLCONFIG_MAXIDENTLEN);
					StrNCpy(pools[lines].username, pi->connection_info[poolBE].user, POOLCONFIG_MAXIDENTLEN);
					strftime(pools[lines].backend_connection_time, sizeof(pools[lines].backend_connection_time),
							 "%Y-%m-%d %H:%M:%S", localtime(&pi->connection_info[poolBE].create_time));
					snprintf(pools[lines].pool_majorversion, sizeof(pools[lines].pool_majorversion), "%d",
							 pi->connection_info[poolBE].major);
					snprintf(pools[lines].pool_minorversion, sizeof(pools[lines].pool_minorversion), "%d",
							 pi->connection_info[poolBE].minor);
				}
				snprintf(pools[lines].pool_counter, sizeof(pools[lines].pool_counter), "%d",
						 pi->connection_info[poolBE].counter);
				snprintf(pools[lines].pool_backendpid, sizeof(pools[lines].pool_backendpid), "%d",
						 ntohl(pi->connection_info[poolBE].pid));
				snprintf(pools[lines].pool_connected, sizeof(pools[lines].pool_connected), "%d",
						 pi->connection_info[poolBE].connected);

				switch(pi->status)
				{
					case WAIT_FOR_CONNECT:
						StrNCpy(pools[lines].status, "Wait for connection", POOLCONFIG_MAXPROCESSSTATUSLEN);
						break;
					case COMMAND_EXECUTE:
						StrNCpy(pools[lines].status, "Execute command", POOLCONFIG_MAXPROCESSSTATUSLEN);
						break;
					case IDLE:
						StrNCpy(pools[lines].status, "Idle", POOLCONFIG_MAXPROCESSSTATUSLEN);
						break;
					case IDLE_IN_TRANS:
						StrNCpy(pools[lines].status, "Idle in transaction", POOLCONFIG_MAXPROCESSSTATUSLEN);
						break;
					case CONNECTING:
						StrNCpy(pools[lines].status, "Connecting", POOLCONFIG_MAXPROCESSSTATUSLEN);
						break;
					default:
						*(pools[lines].status) = '\0';
				}

				lines++;
			}
		}
	}

	*nrows = lines;
	return pools;
}

/*
 * SHOW　pool_pools;
 */
void
pools_reporting(POOL_CONNECTION * frontend, POOL_CONNECTION_POOL * backend)
{
	short num_fields;
	static char *field_names[] = {"pool_pid", "start_time", "client_connection_count", "pool_id",
								  "backend_id", "database", "username", "backend_connection_time",
								  "client_connection_time", "client_disconnection_time", "client_idle_duration",
								  "majorversion", "minorversion", "pool_counter", "pool_backendpid", "pool_connected", "status"};
	int		n;
	int		*offsettbl;
	int		nrows;
	POOL_REPORT_POOLS *pools;

	num_fields = sizeof(field_names) / sizeof(char *);
	offsettbl = pool_report_pools_offsets(&n);
	pools = get_pools(&nrows);

	send_row_description_and_data_rows(frontend, backend, num_fields, field_names, offsettbl,
									   (char *)pools, sizeof(POOL_REPORT_POOLS), nrows);

	pfree(pools);
}

/*
 * Used by SHOW pool_processes
 */
POOL_REPORT_PROCESSES *
get_processes(int *nrows)
{
	int			child;
	int			pool;
	int			poolBE;
	ProcessInfo *pi = NULL;
	int			proc_id;

	POOL_REPORT_PROCESSES *processes = palloc0(pool_config->num_init_children * sizeof(POOL_REPORT_PROCESSES));

	for (child = 0; child < pool_config->num_init_children; child++)
	{
		int exist_live_connection = 0;

		pi = &process_info[child];
		proc_id = pi->pid;

		for (pool = 0; pool < pool_config->max_pool; pool++)
		{
			poolBE = pool * MAX_NUM_BACKENDS;
			if (pi->connection_info[poolBE].connected)
			{
				exist_live_connection = 1;
				break;
			}
		}

		snprintf(processes[child].pool_pid, POOLCONFIG_MAXCOUNTLEN, "%d", proc_id);
		if ((pool_config->child_life_time > 0)
			&& (pi->connected)
			&& (!exist_live_connection))
		{
			char proc_start_time[POOLCONFIG_MAXDATELEN + 1];
			int wait_for_connect_time = pool_config->child_life_time - pi->wait_for_connect;

			strftime(proc_start_time, sizeof(proc_start_time),
					 "%Y-%m-%d %H:%M:%S", localtime(&pi->start_time));
			snprintf(processes[child].process_start_time, sizeof(processes[child].process_start_time),
					 "%s (%d:%02d before process restarting)", proc_start_time,
					 wait_for_connect_time / 60,
					 wait_for_connect_time % 60);
		}
		else
		{
			strftime(processes[child].process_start_time, sizeof(processes[child].process_start_time),
					 "%Y-%m-%d %H:%M:%S", localtime(&pi->start_time));
		}
		snprintf(processes[child].client_connection_count, sizeof(processes[child].client_connection_count),
				 "%d", pi->client_connection_count);
		StrNCpy(processes[child].database, "", POOLCONFIG_MAXIDENTLEN);
		StrNCpy(processes[child].username, "", POOLCONFIG_MAXIDENTLEN);
		StrNCpy(processes[child].backend_connection_time, "", POOLCONFIG_MAXDATELEN);
		StrNCpy(processes[child].pool_counter, "", POOLCONFIG_MAXCOUNTLEN);

		for (pool = 0; pool < pool_config->max_pool; pool++)
		{
			poolBE = pool * MAX_NUM_BACKENDS;
			if (pi->connection_info[poolBE].connected &&
				strlen(pi->connection_info[poolBE].database) > 0 &&
				strlen(pi->connection_info[poolBE].user) > 0)
			{
				StrNCpy(processes[child].database, pi->connection_info[poolBE].database, POOLCONFIG_MAXIDENTLEN);
				StrNCpy(processes[child].username, pi->connection_info[poolBE].user, POOLCONFIG_MAXIDENTLEN);
				strftime(processes[child].backend_connection_time, POOLCONFIG_MAXDATELEN, "%Y-%m-%d %H:%M:%S", localtime(&pi->connection_info[poolBE].create_time));
				snprintf(processes[child].pool_counter, POOLCONFIG_MAXCOUNTLEN, "%d", pi->connection_info[poolBE].counter);
			}
		}
		switch(pi->status)
		{
			case WAIT_FOR_CONNECT:
				StrNCpy(processes[child].status, "Wait for connection", POOLCONFIG_MAXPROCESSSTATUSLEN);
				break;
			case COMMAND_EXECUTE:
				StrNCpy(processes[child].status, "Execute command", POOLCONFIG_MAXPROCESSSTATUSLEN);
				break;
			case IDLE:
				StrNCpy(processes[child].status, "Idle", POOLCONFIG_MAXPROCESSSTATUSLEN);
				break;
			case IDLE_IN_TRANS:
				StrNCpy(processes[child].status, "Idle in transaction", POOLCONFIG_MAXPROCESSSTATUSLEN);
				break;
			case CONNECTING:
				StrNCpy(processes[child].status, "Connecting", POOLCONFIG_MAXPROCESSSTATUSLEN);
				break;
			default:
				*(processes[child].status) = '\0';
		}
	}

	*nrows = child;

	return processes;
}

/*
 * SHOW pool_processes
 */
void
processes_reporting(POOL_CONNECTION * frontend, POOL_CONNECTION_POOL * backend)
{
	static char *field_names[] = {"pool_pid", "start_time", "client_connection_count",
								  "database", "username", "backend_connection_time", "pool_counter", "status"};

	static int offsettbl[] = {
		offsetof(POOL_REPORT_PROCESSES, pool_pid),
		offsetof(POOL_REPORT_PROCESSES, process_start_time),
		offsetof(POOL_REPORT_PROCESSES, client_connection_count),
		offsetof(POOL_REPORT_PROCESSES, database),
		offsetof(POOL_REPORT_PROCESSES, username),
		offsetof(POOL_REPORT_PROCESSES, backend_connection_time),
		offsetof(POOL_REPORT_PROCESSES, pool_counter),
		offsetof(POOL_REPORT_PROCESSES, status),
	};

	int	nrows;
	short		num_fields;
	POOL_REPORT_PROCESSES *processes;

	num_fields = sizeof(field_names) / sizeof(char *);
	processes = get_processes(&nrows);

	send_row_description(frontend, backend, num_fields, field_names);

	send_row_description_and_data_rows(frontend, backend, num_fields, field_names, offsettbl,
									   (char *)processes, sizeof(POOL_REPORT_PROCESSES), nrows);

	pfree(processes);
}

POOL_REPORT_VERSION *
get_version(void)
{
	POOL_REPORT_VERSION *version = palloc(sizeof(POOL_REPORT_VERSION));

	snprintf(version->version, POOLCONFIG_MAXVALLEN, "%s (%s)", VERSION, PGPOOLVERSION);

	return version;
}

/*
 * SHOW pool_version;
 */
void
version_reporting(POOL_CONNECTION * frontend, POOL_CONNECTION_POOL * backend)
{
	static char *field_names[] = {"pool_version"};
	static int offsettbl[] = {
		offsetof(POOL_REPORT_VERSION, version)
	};

	int	nrows = 1;
	short		num_fields;
	POOL_REPORT_VERSION *version;

	num_fields = sizeof(field_names) / sizeof(char *);
	version = get_version();

	send_row_description(frontend, backend, num_fields, field_names);

	send_row_description_and_data_rows(frontend, backend, num_fields, field_names, offsettbl,
									   (char *)version, sizeof(POOL_REPORT_VERSION), nrows);

	pfree(version);
}

/*
 * Show in memory cache reporting
 */
void
cache_reporting(POOL_CONNECTION * frontend, POOL_CONNECTION_POOL * backend)
{
	static char *field_names[] = {"num_cache_hits", "num_selects", "cache_hit_ratio", "num_hash_entries", "used_hash_entries", "num_cache_entries", "used_cache_entries_size", "free_cache_entries_size", "fragment_cache_entries_size"};
	short		num_fields = sizeof(field_names) / sizeof(char *);
	int			i;
	short		s;
	int			len;
	int			size;
	int			hsize;
	static unsigned char nullmap[2] = {0xff, 0xff};
	int			nbytes = (num_fields + 7) / 8;
	volatile	POOL_SHMEM_STATS *mystats;
	pool_sigset_t oldmask;
	double		ratio;

#define POOL_CACHE_STATS_MAX_STRING_LEN 32
	typedef struct
	{
		int			len;		/* length of string excluding null terminate */
		char		string[POOL_CACHE_STATS_MAX_STRING_LEN + 1];
	}			MY_STRING_CACHE_STATS;

	MY_STRING_CACHE_STATS *strp;

	strp = palloc(num_fields * sizeof(MY_STRING_CACHE_STATS));

	/*
	 * Get raw cache stat data
	 */
	POOL_SETMASK2(&BlockSig, &oldmask);
	pool_shmem_lock(POOL_MEMQ_SHARED_LOCK);

	PG_TRY();
	{
		mystats = pool_get_shmem_storage_stats();
	}
	PG_CATCH();
	{
		pool_shmem_unlock();
		POOL_SETMASK(&oldmask);
		PG_RE_THROW();
	}
	PG_END_TRY();
	pool_shmem_unlock();
	POOL_SETMASK(&oldmask);

	/*
	 * Convert to string
	 */
	i = 0;
	snprintf(strp[i++].string, POOL_CACHE_STATS_MAX_STRING_LEN + 1, "%lld", mystats->cache_stats.num_cache_hits);
	snprintf(strp[i++].string, POOL_CACHE_STATS_MAX_STRING_LEN + 1, "%lld", mystats->cache_stats.num_selects);
	if ((mystats->cache_stats.num_cache_hits + mystats->cache_stats.num_selects) == 0)
	{
		ratio = 0.0;
	}
	else
	{
		ratio = (double) mystats->cache_stats.num_cache_hits / (mystats->cache_stats.num_selects + mystats->cache_stats.num_cache_hits);
	}
	snprintf(strp[i++].string, POOL_CACHE_STATS_MAX_STRING_LEN + 1, "%.2f", ratio);
	snprintf(strp[i++].string, POOL_CACHE_STATS_MAX_STRING_LEN + 1, "%d", mystats->num_hash_entries);
	snprintf(strp[i++].string, POOL_CACHE_STATS_MAX_STRING_LEN + 1, "%d", mystats->used_hash_entries);
	snprintf(strp[i++].string, POOL_CACHE_STATS_MAX_STRING_LEN + 1, "%d", mystats->num_cache_entries);
	snprintf(strp[i++].string, POOL_CACHE_STATS_MAX_STRING_LEN + 1, "%ld", mystats->used_cache_entries_size);
	snprintf(strp[i++].string, POOL_CACHE_STATS_MAX_STRING_LEN + 1, "%ld", mystats->free_cache_entries_size);
	snprintf(strp[i++].string, POOL_CACHE_STATS_MAX_STRING_LEN + 1, "%ld", mystats->fragment_cache_entries_size);

	/*
	 * Calculate total data length
	 */
	len = 2;					/* number of fields (int16) */
	for (i = 0; i < num_fields; i++)
	{
		strp[i].len = strlen(strp[i].string);
		len += 4				/* length of string (int32) */
			+ strp[i].len;
	}

	/* Send row description */
	send_row_description(frontend, backend, num_fields, field_names);

	/* Send each field */
	if (MAJOR(backend) == PROTO_MAJOR_V2)
	{
		pool_write(frontend, "D", 1);
		pool_write(frontend, nullmap, nbytes);

		for (i = 0; i < num_fields; i++)
		{
			size = strp[i].len + 1;
			hsize = htonl(size + 4);
			pool_write(frontend, &hsize, sizeof(hsize));
			pool_write(frontend, strp[i].string, size);
		}
	}
	else
	{
		/* Kind */
		pool_write(frontend, "D", 1);
		/* Packet length */
		len = htonl(len + sizeof(int32));
		pool_write(frontend, &len, sizeof(len));
		/* Number of fields */
		s = htons(num_fields);
		pool_write(frontend, &s, sizeof(s));

		for (i = 0; i < num_fields; i++)
		{
			hsize = htonl(strp[i].len);
			pool_write(frontend, &hsize, sizeof(hsize));
			pool_write(frontend, strp[i].string, strp[i].len);
		}
	}

	send_complete_and_ready(frontend, backend, "SELECT", 1);

	pfree(strp);
}

/*
 * for SHOW health_check_stats
 */
POOL_HEALTH_CHECK_STATS *
get_health_check_stats(int *nrows)
{
	int			i;
	POOL_HEALTH_CHECK_STATS *stats = palloc0(NUM_BACKENDS * sizeof(POOL_HEALTH_CHECK_STATS));
	BackendInfo *bi = NULL;
	time_t		t;
	double		f;

	for (i = 0; i < NUM_BACKENDS; i++)
	{
		bi = pool_get_node_info(i);

		snprintf(stats[i].node_id, POOLCONFIG_MAXIDLEN, "%d", i);
		StrNCpy(stats[i].hostname, bi->backend_hostname, MAX_DB_HOST_NAMELEN);
		snprintf(stats[i].port, POOLCONFIG_MAXPORTLEN, "%d", bi->backend_port);
		snprintf(stats[i].status, POOLCONFIG_MAXSTATLEN, "%s", backend_status_to_str(bi));

		if (STREAM)
		{
			if (i == REAL_PRIMARY_NODE_ID)
			{
				snprintf(stats[i].role, POOLCONFIG_MAXWEIGHTLEN, "%s", "primary");
			}
			else
			{
				snprintf(stats[i].role, POOLCONFIG_MAXWEIGHTLEN, "%s", "standby");
			}
		}
		else
		{
			if (i == REAL_MAIN_NODE_ID)
				snprintf(stats[i].role, POOLCONFIG_MAXWEIGHTLEN, "%s", "main");
			else
				snprintf(stats[i].role, POOLCONFIG_MAXWEIGHTLEN, "%s", "replica");
		}

		/* status last changed */
		t = bi->status_changed_time;
		ereport(LOG,(errmsg("status_changed_time %ld", t)));
		strftime(stats[i].last_status_change, POOLCONFIG_MAXDATELEN, "%F %T", localtime(&t));

		snprintf(stats[i].total_count, POOLCONFIG_MAXLONGCOUNTLEN, UINT64_FORMAT, health_check_stats[i].total_count);
		snprintf(stats[i].success_count, POOLCONFIG_MAXLONGCOUNTLEN, UINT64_FORMAT, health_check_stats[i].success_count);
		snprintf(stats[i].fail_count, POOLCONFIG_MAXLONGCOUNTLEN, UINT64_FORMAT, health_check_stats[i].fail_count);
		snprintf(stats[i].skip_count, POOLCONFIG_MAXLONGCOUNTLEN, UINT64_FORMAT, health_check_stats[i].skip_count);
		snprintf(stats[i].retry_count, POOLCONFIG_MAXLONGCOUNTLEN, UINT64_FORMAT, health_check_stats[i].retry_count);
		snprintf(stats[i].max_retry_count, POOLCONFIG_MAXCOUNTLEN, "%d", health_check_stats[i].max_retry_count);

		if (pool_config->health_check_params[i].health_check_period > 0)
			f = (double)health_check_stats[i].retry_count /
				(health_check_stats[i].total_count - health_check_stats[i].skip_count);
		else
			f = 0.0;
		snprintf(stats[i].average_retry_count, POOLCONFIG_MAXWEIGHTLEN, "%f", f);

		if (pool_config->health_check_params[i].health_check_period > 0)
			f = (double)health_check_stats[i].total_health_check_duration /
				(health_check_stats[i].total_count - health_check_stats[i].skip_count);
		else
			f = 0.0;
		snprintf(stats[i].average_health_check_duration, POOLCONFIG_MAXWEIGHTLEN, "%f", f);

		snprintf(stats[i].max_health_check_duration, POOLCONFIG_MAXCOUNTLEN, "%d", health_check_stats[i].max_health_check_duration);
		snprintf(stats[i].min_health_check_duration, POOLCONFIG_MAXCOUNTLEN, "%d", health_check_stats[i].min_health_check_duration);

		t = health_check_stats[i].last_health_check;
		if (t > 0)
			strftime(stats[i].last_health_check, POOLCONFIG_MAXDATELEN, "%F %T", localtime(&t));

		t = health_check_stats[i].last_successful_health_check;
		if (t > 0)
			strftime(stats[i].last_successful_health_check, POOLCONFIG_MAXDATELEN, "%F %T", localtime(&t));

		t = health_check_stats[i].last_skip_health_check;
		if (t > 0)
			strftime(stats[i].last_skip_health_check, POOLCONFIG_MAXDATELEN, "%F %T", localtime(&t));

		t = health_check_stats[i].last_failed_health_check;
		if (t > 0)
			strftime(stats[i].last_failed_health_check, POOLCONFIG_MAXDATELEN, "%F %T", localtime(&t));
	}

	*nrows = i;

	return stats;
}

/*
 * SHOW health_check_stats;
 */
void
show_health_check_stats(POOL_CONNECTION * frontend, POOL_CONNECTION_POOL * backend)
{
	static char *field_names[] = {"node_id", "hostname", "port", "status", "role", "last_status_change",
								  "total_count", "success_count", "fail_count", "skip_count", "retry_count",
								  "average_retry_count", "max_retry_count", "max_duration", "min_duration",
								  "average_duration", "last_health_check", "last_successful_health_check",
								  "last_skip_health_check", "last_failed_health_check"};
	static int offsettbl[] = {
		offsetof(POOL_HEALTH_CHECK_STATS, node_id),
		offsetof(POOL_HEALTH_CHECK_STATS, hostname),
		offsetof(POOL_HEALTH_CHECK_STATS, port),
		offsetof(POOL_HEALTH_CHECK_STATS, status),
		offsetof(POOL_HEALTH_CHECK_STATS, role),
		offsetof(POOL_HEALTH_CHECK_STATS, last_status_change),
		offsetof(POOL_HEALTH_CHECK_STATS, total_count),
		offsetof(POOL_HEALTH_CHECK_STATS, success_count),
		offsetof(POOL_HEALTH_CHECK_STATS, fail_count),
		offsetof(POOL_HEALTH_CHECK_STATS, skip_count),
		offsetof(POOL_HEALTH_CHECK_STATS, retry_count),
		offsetof(POOL_HEALTH_CHECK_STATS, average_retry_count),
		offsetof(POOL_HEALTH_CHECK_STATS, max_retry_count),
		offsetof(POOL_HEALTH_CHECK_STATS, max_health_check_duration),
		offsetof(POOL_HEALTH_CHECK_STATS, min_health_check_duration),
		offsetof(POOL_HEALTH_CHECK_STATS, average_health_check_duration),
		offsetof(POOL_HEALTH_CHECK_STATS, last_health_check),
		offsetof(POOL_HEALTH_CHECK_STATS, last_successful_health_check),
		offsetof(POOL_HEALTH_CHECK_STATS, last_skip_health_check),
		offsetof(POOL_HEALTH_CHECK_STATS, last_failed_health_check),
	};

	int	nrows;
	short		num_fields;
	POOL_HEALTH_CHECK_STATS *stats;

	num_fields = sizeof(field_names) / sizeof(char *);
	stats = get_health_check_stats(&nrows);

	send_row_description_and_data_rows(frontend, backend, num_fields, field_names, offsettbl,
									   (char *)stats, sizeof(POOL_HEALTH_CHECK_STATS), nrows);

	pfree(stats);
}

/*
 * for SHOW backend_stats
 */
POOL_BACKEND_STATS *
get_backend_stats(int *nrows)
{
	int			i;
	POOL_BACKEND_STATS *backend_stats = palloc(NUM_BACKENDS * sizeof(POOL_BACKEND_STATS));
	BackendInfo *bi = NULL;

	pool_get_session_context(false);

	for (i = 0; i < NUM_BACKENDS; i++)
	{
		bi = pool_get_node_info(i);

		snprintf(backend_stats[i].node_id, POOLCONFIG_MAXIDLEN, "%d", i);
		StrNCpy(backend_stats[i].hostname, bi->backend_hostname, MAX_DB_HOST_NAMELEN);
		snprintf(backend_stats[i].port, POOLCONFIG_MAXPORTLEN, "%d", bi->backend_port);
		snprintf(backend_stats[i].status, POOLCONFIG_MAXSTATLEN, "%s", backend_status_to_str(bi));
		snprintf(backend_stats[i].select_cnt, POOLCONFIG_MAXWEIGHTLEN, UINT64_FORMAT, stat_get_select_count(i));
		snprintf(backend_stats[i].insert_cnt, POOLCONFIG_MAXWEIGHTLEN, UINT64_FORMAT, stat_get_insert_count(i));
		snprintf(backend_stats[i].update_cnt, POOLCONFIG_MAXWEIGHTLEN, UINT64_FORMAT, stat_get_update_count(i));
		snprintf(backend_stats[i].delete_cnt, POOLCONFIG_MAXWEIGHTLEN, UINT64_FORMAT, stat_get_delete_count(i));
		snprintf(backend_stats[i].ddl_cnt, POOLCONFIG_MAXWEIGHTLEN, UINT64_FORMAT, stat_get_ddl_count(i));
		snprintf(backend_stats[i].other_cnt, POOLCONFIG_MAXWEIGHTLEN, UINT64_FORMAT, stat_get_other_count(i));
		snprintf(backend_stats[i].panic_cnt, POOLCONFIG_MAXWEIGHTLEN, UINT64_FORMAT, stat_get_panic_count(i));
		snprintf(backend_stats[i].fatal_cnt, POOLCONFIG_MAXWEIGHTLEN, UINT64_FORMAT, stat_get_fatal_count(i));
		snprintf(backend_stats[i].error_cnt, POOLCONFIG_MAXWEIGHTLEN, UINT64_FORMAT, stat_get_error_count(i));

		if (STREAM)
		{
			if (i == REAL_PRIMARY_NODE_ID)
			{
				snprintf(backend_stats[i].role, POOLCONFIG_MAXWEIGHTLEN, "%s", "primary");
			}
			else
			{
				snprintf(backend_stats[i].role, POOLCONFIG_MAXWEIGHTLEN, "%s", "standby");
			}
		}
		else
		{
			if (i == REAL_MAIN_NODE_ID)
				snprintf(backend_stats[i].role, POOLCONFIG_MAXWEIGHTLEN, "%s", "main");
			else
				snprintf(backend_stats[i].role, POOLCONFIG_MAXWEIGHTLEN, "%s", "replica");
		}
	}

	*nrows = i;

	return backend_stats;
}

/*
 * SHOW backend_stats;
 */
void
show_backend_stats(POOL_CONNECTION * frontend, POOL_CONNECTION_POOL * backend)
{
	static char *field_names[] = {"node_id", "hostname", "port", "status", "role",
								  "select_cnt", "insert_cnt", "update_cnt", "delete_cnt", "ddl_cnt", "other_cnt",
								  "panic_cnt", "fatal_cnt", "error_cnt"};

	static int offsettbl[] = {
		offsetof(POOL_BACKEND_STATS, node_id),
		offsetof(POOL_BACKEND_STATS, hostname),
		offsetof(POOL_BACKEND_STATS, port),
		offsetof(POOL_BACKEND_STATS, status),
		offsetof(POOL_BACKEND_STATS, role),
		offsetof(POOL_BACKEND_STATS, select_cnt),
		offsetof(POOL_BACKEND_STATS, insert_cnt),
		offsetof(POOL_BACKEND_STATS, update_cnt),
		offsetof(POOL_BACKEND_STATS, delete_cnt),
		offsetof(POOL_BACKEND_STATS, ddl_cnt),
		offsetof(POOL_BACKEND_STATS, other_cnt),
		offsetof(POOL_BACKEND_STATS, panic_cnt),
		offsetof(POOL_BACKEND_STATS, fatal_cnt),
		offsetof(POOL_BACKEND_STATS, error_cnt),
	};

	int	nrows;
	short		num_fields;
	POOL_BACKEND_STATS *backend_stats;

	num_fields = sizeof(field_names) / sizeof(char *);
	backend_stats = get_backend_stats(&nrows);

	send_row_description_and_data_rows(frontend, backend, num_fields, field_names, offsettbl,
									   (char *)backend_stats, sizeof(POOL_BACKEND_STATS), nrows);

	pfree(backend_stats);
}

/*
 * Send row description and data rows.
 *
 * Params:
 * frontend/backend: connections to frontend and backend.
 *
 * num_fields: number of fields
 *
 * field_names: array of field names
 *
 * offsettbl: offset array for each "data" member. The number of array
 * elements must match with num_fields.
 *
 * data: string data to be displayed as row data, 2-dimensions array. The
 * number of array elements must match with num_fields * nrows.
 *
 * row_size: byte length of data for 1 row.
 *
 * nrows: number of rows in data. 
 */
static void send_row_description_and_data_rows(POOL_CONNECTION * frontend, POOL_CONNECTION_POOL * backend,
											   short num_fields, char **field_names, int *offsettbl,
											   char *data, int row_size, int nrows)
{
	int			i, j;
	short		s;
	int			len;
	unsigned char *nullmap;
	int			nbytes;

	nbytes = (num_fields + 7) / 8;

	send_row_description(frontend, backend, num_fields, field_names);

	if (MAJOR(backend) == PROTO_MAJOR_V2)
	{
		nullmap = palloc(nbytes);
		memset(nullmap, 0xff, nbytes);

		/* ascii row */
		for (i = 0; i < nrows; i++)
		{
			pool_write(frontend, "D", 1);
			pool_write_and_flush(frontend, nullmap, nbytes);

			for (j = 0; j < num_fields; j++)
			{
				write_one_field_v2(frontend, data + i * row_size + offsettbl[j]);
			}
		}
		pfree(nullmap);
	}
	else
	{
		/* data row */
		for (i = 0; i < nrows; i++)
		{
			pool_write(frontend, "D", 1);
			len = 6;			/* int32 + int16; */

			for (j = 0; j < num_fields; j++)
			{
				len += 4 + strlen(data + i * row_size + offsettbl[j]);
			}
			len = htonl(len);
			pool_write(frontend, &len, sizeof(len));
			s = htons(num_fields);
			pool_write(frontend, &s, sizeof(s));

			for (j = 0; j < num_fields; j++)
			{
				write_one_field(frontend, data + i * row_size + offsettbl[j]);
			}
		}
	}

	send_complete_and_ready(frontend, backend, "SELECT", nrows);
}

/* Write one field to frontend (v3) */
static void write_one_field(POOL_CONNECTION * frontend, char *field)
{
	int	size, hsize;

	size = strlen(field);
	hsize = htonl(size);
	pool_write(frontend, &hsize, sizeof(hsize));
	pool_write(frontend, field, size);
}

/* Write one field to frontend (v2) */
static void write_one_field_v2(POOL_CONNECTION * frontend, char *field)
{
	int	size, hsize;

	size = strlen(field);
	hsize = htonl(size + 4);
	pool_write(frontend, &hsize, sizeof(hsize));
	pool_write(frontend, field, size);
}

/*
 * Get DB node status.  Return values are "up", "down" or "unknown" (in case
 * when health check is not enabled).
 */
static
char *db_node_status(int node)
{
#ifdef HAVE_PQPINGPARAMS
	BackendInfo *bkinfo;
	int		i;
	char	portstr[32];
	char	timeoutstr[32];
#define PARAMS_ARRAY_SIZE	8
	const char *keywords[PARAMS_ARRAY_SIZE];
	const char *values[PARAMS_ARRAY_SIZE];
	PGPing	ret;
#endif

	/*
	 * If health check is not enabled, return "unknown".
	 */
	if (pool_config->health_check_params[node].health_check_period == 0)
	{
		return "unknown";
	}

#ifdef HAVE_PQPINGPARAMS
	i = 0;
	
	keywords[i] = "user";
	values[i] = pool_config->health_check_params[node].health_check_user;
	i++;

	/*
	 * If health check database is not defined, use "postgres" database.
	 */
	keywords[i] = "dbname";
	if (*pool_config->health_check_params[node].health_check_database == '\0')
		values[i]  = "postgres";
	else
		values[i] = pool_config->health_check_params[node].health_check_database;
	i++;

	bkinfo = pool_get_node_info(node);
	keywords[i] = "host";
	values[i] = bkinfo->backend_hostname;
	i++;

	keywords[i] = "port";
	snprintf(portstr, sizeof(portstr), "%d", bkinfo->backend_port);
	values[i] = portstr;
	i++;

	if (pool_config->connect_timeout > 0)
	{
		keywords[i] = "connect_timeout";
		snprintf(timeoutstr, sizeof(timeoutstr), "%d", pool_config->connect_timeout / 1000);
		values[i] = timeoutstr;
		i++;
	}

	keywords[i] = NULL;
	values[i] = NULL;

	ret = PQpingParams(keywords, values, 1);
	if (ret == PQPING_OK)
	{
		return "up";
	}
	return "down";
#else
	return "unknown";
#endif
}

/*
 * Get DB node role.  Return values are "primary", "standby" or "unknown" (in case
 * when sr check is not enabled).
 */
static
char *db_node_role(int node)
{
	BackendInfo *bkinfo;
	POOL_CONNECTION_POOL_SLOT *slots[MAX_NUM_BACKENDS];
	POOL_SELECT_RESULT *res;
	char	*user;
	char	*password;
	char	*dbname;
	char	*host;
	int		port;
	char	*sts;

	if (pool_config->sr_check_period == 0)
	{
		/* sr check is disabled */
		return "unknown";
	}

	bkinfo = pool_get_node_info(node);
	host = bkinfo->backend_hostname;
	port = bkinfo->backend_port;
	user = pool_config->sr_check_user;
	password = get_pgpool_config_user_password(user, pool_config->sr_check_password);
	dbname = pool_config->sr_check_database;
	if (*dbname == '\0')
		dbname = "postgres";

	/*
	 * Establish connection to backend.
	 */
	slots[node] = make_persistent_db_connection_noerror(node, host, port, dbname, user,
														password ? password : "", false);

	if (slots[node] == NULL)
		return "unknown";

	/*
	 * Query whether the node is in recovery.
	 */
	if (get_query_result(slots, node, "SELECT pg_is_in_recovery()", &res))
	{
		return "unknown";
	}

	if (res->data[0] && !strcmp(res->data[0], "t"))
	{
		sts = "standby";
	}
	else if (res->data[0] && !strcmp(res->data[0], "f"))
	{
		sts = "primary";
	}
	else
	{
		sts = "unknown";
	}
	free_select_result(res);

	/*
	 * Discard connection to backend.
	 */
	discard_persistent_db_connection(slots[node]);

	return sts;
}
