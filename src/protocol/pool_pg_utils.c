/*
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
 *
 */


#include <string.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <unistd.h>

#include "protocol/pool_pg_utils.h"
#include "protocol/pool_connection_pool.h"
#include "utils/palloc.h"
#include "utils/memutils.h"
#include "utils/pool_ipc.h"
#include "utils/pool_stream.h"
#include "utils/pool_ssl.h"
#include "utils/elog.h"
#include "utils/pool_relcache.h"
#include "auth/pool_auth.h"
#include "context/pool_session_context.h"

#include "pool_config.h"
#include "pool_config_variables.h"

static int	choose_db_node_id(char *str);
static void free_persistent_db_connection_memory(POOL_CONNECTION_POOL_SLOT * cp);
static void si_enter_critical_region(void);
static void si_leave_critical_region(void);

/*
 * create a persistent connection
 */
POOL_CONNECTION_POOL_SLOT *
make_persistent_db_connection(
							  int db_node_id, char *hostname, int port, char *dbname, char *user, char *password, bool retry)
{
	POOL_CONNECTION_POOL_SLOT *cp;
	int			fd;

#define MAX_USER_AND_DATABASE	1024

	/* V3 startup packet */
	typedef struct
	{
		int			protoVersion;
		char		data[MAX_USER_AND_DATABASE];
	}			StartupPacket_v3;

	static StartupPacket_v3 * startup_packet;
	int			len,
				len1;

	cp = palloc0(sizeof(POOL_CONNECTION_POOL_SLOT));
	startup_packet = palloc0(sizeof(*startup_packet));
	startup_packet->protoVersion = htonl(0x00030000);	/* set V3 proto
														 * major/minor */

	/*
	 * create socket
	 */
	if (*hostname == '/')
	{
		fd = connect_unix_domain_socket_by_port(port, hostname, retry);
	}
	else
	{
		fd = connect_inet_domain_socket_by_port(hostname, port, retry);
	}

	if (fd < 0)
	{
		free_persistent_db_connection_memory(cp);
		pfree(startup_packet);
		ereport(ERROR,
				(errmsg("failed to make persistent db connection"),
				 errdetail("connection to host:\"%s:%d\" failed", hostname, port)));
	}

	cp->con = pool_open(fd, true);
	cp->closetime = 0;
	cp->con->isbackend = 1;
	pool_set_db_node_id(cp->con, db_node_id);

	pool_ssl_negotiate_clientserver(cp->con);

	/*
	 * build V3 startup packet
	 */
	len = snprintf(startup_packet->data, sizeof(startup_packet->data), "user") + 1;
	len1 = snprintf(&startup_packet->data[len], sizeof(startup_packet->data) - len, "%s", user) + 1;
	if (len1 >= (sizeof(startup_packet->data) - len))
	{
		pool_close(cp->con);
		free_persistent_db_connection_memory(cp);
		pfree(startup_packet);
		ereport(ERROR,
				(errmsg("failed to make persistent db connection"),
				 errdetail("user name is too long")));
	}

	len += len1;
	len1 = snprintf(&startup_packet->data[len], sizeof(startup_packet->data) - len, "database") + 1;
	if (len1 >= (sizeof(startup_packet->data) - len))
	{
		pool_close(cp->con);
		free_persistent_db_connection_memory(cp);
		pfree(startup_packet);
		ereport(ERROR,
				(errmsg("failed to make persistent db connection"),
				 errdetail("user name is too long")));
	}

	len += len1;
	len1 = snprintf(&startup_packet->data[len], sizeof(startup_packet->data) - len, "%s", dbname) + 1;
	if (len1 >= (sizeof(startup_packet->data) - len))
	{
		pool_close(cp->con);
		free_persistent_db_connection_memory(cp);
		pfree(startup_packet);
		ereport(ERROR,
				(errmsg("failed to make persistent db connection"),
				 errdetail("database name is too long")));
	}
	len += len1;
	startup_packet->data[len++] = '\0';

	cp->sp = palloc(sizeof(StartupPacket));

	cp->sp->startup_packet = (char *) startup_packet;
	cp->sp->len = len + 4;
	cp->sp->major = 3;
	cp->sp->minor = 0;
	cp->sp->database = pstrdup(dbname);
	cp->sp->user = pstrdup(user);

	/*
	 * send startup packet
	 */
	PG_TRY();
	{
		send_startup_packet(cp);
		connection_do_auth(cp, password);
	}
	PG_CATCH();
	{
		pool_close(cp->con);
		free_persistent_db_connection_memory(cp);
		PG_RE_THROW();
	}
	PG_END_TRY();

	return cp;
}

/*
 * make_persistent_db_connection_noerror() is a wrapper over
 * make_persistent_db_connection() which does not ereports in case of an error
 */
POOL_CONNECTION_POOL_SLOT *
make_persistent_db_connection_noerror(
									  int db_node_id, char *hostname, int port, char *dbname, char *user, char *password, bool retry)
{
	POOL_CONNECTION_POOL_SLOT *slot = NULL;
	MemoryContext oldContext = CurrentMemoryContext;

	PG_TRY();
	{
		slot = make_persistent_db_connection(db_node_id,
											 hostname,
											 port,
											 dbname,
											 user,
											 password, retry);
	}
	PG_CATCH();
	{
		/*
		 * We used to call EmitErrorReport() to log the error and send an
		 * error message to frontend.  However if pcp frontend program
		 * receives an ERROR, it stops processing and terminates, which is not
		 * good. This is problematic especially with pcp_node_info, since it
		 * calls db_node_role(), and db_node_role() calls this function. So if
		 * the target PostgreSQL is down, EmitErrorRepor() sends ERROR message
		 * to pcp frontend and it stops (see process_pcp_response() in
		 * src/libs/pcp/pcp.c. To fix this, just eliminate calling
		 * EmitErrorReport(). This will suppress ERROR message but as you can
		 * see the comment in this function "does not ereports in case of an
		 * error", this should have been the right behavior in the first
		 * place.
		 */
		MemoryContextSwitchTo(oldContext);
		FlushErrorState();
		slot = NULL;
	}
	PG_END_TRY();
	return slot;
}

/*
 * Free memory of POOL_CONNECTION_POOL_SLOT.  Should only be used in
 * make_persistent_db_connection and discard_persistent_db_connection.
 */
static void
free_persistent_db_connection_memory(POOL_CONNECTION_POOL_SLOT * cp)
{
	if (!cp)
		return;
	if (!cp->sp)
	{
		pfree(cp);
		return;
	}
	if (cp->sp->startup_packet)
		pfree(cp->sp->startup_packet);
	if (cp->sp->database)
		pfree(cp->sp->database);
	if (cp->sp->user)
		pfree(cp->sp->user);
	pfree(cp->sp);
	pfree(cp);
}

/*
 * Discard connection and memory allocated by
 * make_persistent_db_connection().
 */
void
discard_persistent_db_connection(POOL_CONNECTION_POOL_SLOT * cp)
{
	int			len;

	if (cp == NULL)
		return;

	pool_write(cp->con, "X", 1);
	len = htonl(4);
	pool_write(cp->con, &len, sizeof(len));

	/*
	 * XXX we cannot call pool_flush() here since backend may already close
	 * the socket and pool_flush() automatically invokes fail over handler.
	 * This could happen in copy command (remember the famous "lost
	 * synchronization with server, resetting connection" message)
	 */
	socket_set_nonblock(cp->con->fd);
	pool_flush_it(cp->con);
	socket_unset_nonblock(cp->con->fd);

	pool_close(cp->con);
	free_persistent_db_connection_memory(cp);
}

/*
 * send startup packet
 */
void
send_startup_packet(POOL_CONNECTION_POOL_SLOT * cp)
{
	int			len;

	len = htonl(cp->sp->len + sizeof(len));
	pool_write(cp->con, &len, sizeof(len));
	pool_write_and_flush(cp->con, cp->sp->startup_packet, cp->sp->len);
}

void
pool_free_startup_packet(StartupPacket *sp)
{
	if (sp)
	{
		if (sp->startup_packet)
			pfree(sp->startup_packet);
		if (sp->database)
			pfree(sp->database);
		if (sp->user)
			pfree(sp->user);
		pfree(sp);
	}
	sp = NULL;
}

/*
 * Select load balancing node. This function is called when:
 * 1) client connects
 * 2) the node previously selected for the load balance node is down
 */
int
select_load_balancing_node(void)
{
	int			selected_slot;
	double		total_weight,
				r;
	int			i;
	int			index_db = -1,
				index_app = -1;
	POOL_SESSION_CONTEXT *ses = pool_get_session_context(false);
	int			tmp;
	int			no_load_balance_node_id = -2;
	uint64		lowest_delay;
	int 		lowest_delay_nodes[NUM_BACKENDS];

	/*
	 * -2 indicates there's no database_redirect_preference_list. -1 indicates
	 * database_redirect_preference_list exists and any of standby nodes
	 * specified.
	 */
	int			suggested_node_id = -2;

#if defined(sun) || defined(__sun)
	r = (((double) rand()) / RAND_MAX);
#else
	r = (((double) random()) / RAND_MAX);
#endif

	/*
	 * Check database_redirect_preference_list
	 */
	if (SL_MODE && pool_config->redirect_dbnames)
	{
		char	   *database = MAIN_CONNECTION(ses->backend)->sp->database;

		/*
		 * Check to see if the database matches any of
		 * database_redirect_preference_list
		 */
		index_db = regex_array_match(pool_config->redirect_dbnames, database);
		if (index_db >= 0)
		{
			/* Matches */
			ereport(DEBUG1,
					(errmsg("selecting load balance node db matched"),
					 errdetail("dbname: %s index is %d dbnode is %s weight is %f", database, index_db,
							   pool_config->db_redirect_tokens->token[index_db].right_token,
							   pool_config->db_redirect_tokens->token[index_db].weight_token)));

			tmp = choose_db_node_id(pool_config->db_redirect_tokens->token[index_db].right_token);
			if (tmp == -1 || (tmp >= 0 && VALID_BACKEND_RAW(tmp)))
				suggested_node_id = tmp;
		}
	}

	/*
	 * Check app_name_redirect_preference_list
	 */
	if (SL_MODE && pool_config->redirect_app_names)
	{
		char	   *app_name = MAIN_CONNECTION(ses->backend)->sp->application_name;

		/*
		 * Check only if application name is set. Old applications may not
		 * have application name.
		 */
		if (app_name && strlen(app_name) > 0)
		{
			/*
			 * Check to see if the application name matches any of
			 * app_name_redirect_preference_list.
			 */
			index_app = regex_array_match(pool_config->redirect_app_names, app_name);
			if (index_app >= 0)
			{

				/*
				 * if the application name matches any of
				 * app_name_redirect_preference_list,
				 * database_redirect_preference_list will be ignored.
				 */
				index_db = -1;

				/* Matches */
				ereport(DEBUG1,
						(errmsg("selecting load balance node db matched"),
						 errdetail("app_name: %s index is %d dbnode is %s weight is %f", app_name, index_app,
								   pool_config->app_name_redirect_tokens->token[index_app].right_token,
								   pool_config->app_name_redirect_tokens->token[index_app].weight_token)));

				tmp = choose_db_node_id(pool_config->app_name_redirect_tokens->token[index_app].right_token);
				if (tmp == -1 || (tmp >= 0 && VALID_BACKEND_RAW(tmp)))
					suggested_node_id = tmp;
			}
		}
	}

	if (suggested_node_id >= 0)
	{
		/*
		 * If pgpool is running in Streaming Replication mode and delay_threshold
		 * and prefer_lower_delay_standby are true, we choose the least delayed
		 * node if suggested_node is standby and delayed over delay_threshold.
		 */
		if (STREAM &&
			pool_config->delay_threshold &&
			pool_config->prefer_lower_delay_standby &&
			(suggested_node_id != PRIMARY_NODE_ID) &&
			(((BACKEND_INFO(suggested_node_id).standby_delay_by_time == false && BACKEND_INFO(suggested_node_id).standby_delay > pool_config->delay_threshold)) ||
			 ((BACKEND_INFO(suggested_node_id).standby_delay_by_time && BACKEND_INFO(suggested_node_id).standby_delay > pool_config->delay_threshold_by_time * 1000000))))
		{
			ereport(DEBUG1,
				(errmsg("selecting load balance node"),
				 errdetail("suggested backend %d is streaming delayed over delay_threshold", suggested_node_id)));

			/*
			 * The new load balancing node is seleted from the
			 * nodes which have the lowest delay.
			 */
			lowest_delay = pool_config->delay_threshold;

			/* Initialize */
			total_weight = 0.0;
			for (i = 0; i < NUM_BACKENDS; i++)
			{
				lowest_delay_nodes[i] = 0;
			}

			for (i = 0; i < NUM_BACKENDS; i++)
			{
				if (VALID_BACKEND_RAW(i) &&
					(i != PRIMARY_NODE_ID) &&
					(BACKEND_INFO(i).backend_weight > 0.0))
				{
					if (lowest_delay == BACKEND_INFO(i).standby_delay)
					{
						lowest_delay_nodes[i] = 1;
						total_weight += BACKEND_INFO(i).backend_weight;
					}
					else if (lowest_delay > BACKEND_INFO(i).standby_delay)
					{
						int ii;
						lowest_delay = BACKEND_INFO(i).standby_delay;
						for (ii = 0; ii < NUM_BACKENDS; ii++)
						{
							lowest_delay_nodes[ii] = 0;
						}
						lowest_delay_nodes[i] = 1;
						total_weight = BACKEND_INFO(i).backend_weight;
					}
				}
			}

#if defined(sun) || defined(__sun)
			r = (((double) rand()) / RAND_MAX) * total_weight;
#else
			r = (((double) random()) / RAND_MAX) * total_weight;
#endif

			selected_slot = PRIMARY_NODE_ID;
			total_weight = 0.0;
			for (i = 0; i < NUM_BACKENDS; i++)
			{
				if (lowest_delay_nodes[i] == 0)
					continue;

				if (selected_slot == PRIMARY_NODE_ID)
					selected_slot = i;

				if (r >= total_weight)
					selected_slot = i;
				else
					break;

				total_weight += BACKEND_INFO(i).backend_weight;
			}

			ereport(DEBUG1,
					(errmsg("selecting load balance node"),
					 errdetail("selected backend id is %d", selected_slot)));
			return selected_slot;
		}

		/*
		 * If the weight is bigger than random rate then send to
		 * suggested_node_id. If the weight is less than random rate then
		 * choose load balance node from other nodes.
		 */
		if ((index_db >= 0 && r <= pool_config->db_redirect_tokens->token[index_db].weight_token) ||
			(index_app >= 0 && r <= pool_config->app_name_redirect_tokens->token[index_app].weight_token))
		{
			ereport(DEBUG1,
					(errmsg("selecting load balance node"),
					 errdetail("selected backend id is %d", suggested_node_id)));
			return suggested_node_id;
		}
		else
			no_load_balance_node_id = suggested_node_id;
	}

	/* In case of sending to standby */
	if (suggested_node_id == -1)
	{
		/* If the weight is less than random rate then send to primary. */
		if ((index_db >= 0 && r > pool_config->db_redirect_tokens->token[index_db].weight_token) ||
			(index_app >= 0 && r > pool_config->app_name_redirect_tokens->token[index_app].weight_token))
		{
			ereport(DEBUG1,
					(errmsg("selecting load balance node"),
					 errdetail("selected backend id is %d", PRIMARY_NODE_ID)));
			return PRIMARY_NODE_ID;
		}
	}

	/* Choose a backend in random manner with weight */
	selected_slot = MAIN_NODE_ID;
	total_weight = 0.0;

	for (i = 0; i < NUM_BACKENDS; i++)
	{
		if (VALID_BACKEND_RAW(i))
		{
			if (i == no_load_balance_node_id)
				continue;
			if (suggested_node_id == -1)
			{
				if (i != PRIMARY_NODE_ID)
					total_weight += BACKEND_INFO(i).backend_weight;
			}
			else
				total_weight += BACKEND_INFO(i).backend_weight;
		}
	}

#if defined(sun) || defined(__sun)
	r = (((double) rand()) / RAND_MAX) * total_weight;
#else
	r = (((double) random()) / RAND_MAX) * total_weight;
#endif

	total_weight = 0.0;
	for (i = 0; i < NUM_BACKENDS; i++)
	{
		if ((suggested_node_id == -1 && i == PRIMARY_NODE_ID) || i == no_load_balance_node_id)
			continue;

		if (VALID_BACKEND_RAW(i) && BACKEND_INFO(i).backend_weight > 0.0)
		{
			if (r >= total_weight)
				selected_slot = i;
			else
				break;
			total_weight += BACKEND_INFO(i).backend_weight;
		}
	}

	/*
	 * If Streaming Replication mode and delay_threshold and
	 * prefer_lower_delay_standby is true, we elect the most lower delayed
	 * node if suggested_node is standby and delayed over delay_threshold.
	 */
	if (STREAM &&
		pool_config->delay_threshold &&
		pool_config->prefer_lower_delay_standby &&
		(BACKEND_INFO(selected_slot).standby_delay > pool_config->delay_threshold))
	{
		ereport(DEBUG1,
				(errmsg("selecting load balance node"),
				 errdetail("backend id %d is streaming delayed over delay_threshold", selected_slot)));

		lowest_delay = pool_config->delay_threshold;
		total_weight = 0.0;
		for (i = 0; i < NUM_BACKENDS; i++)
		{
			lowest_delay_nodes[i] = 0;
		}

		for (i = 0; i < NUM_BACKENDS; i++)
		{
			if ((i != PRIMARY_NODE_ID) &&
				VALID_BACKEND_RAW(i) &&
				(BACKEND_INFO(i).backend_weight > 0.0))
			{
				if (lowest_delay == BACKEND_INFO(i).standby_delay)
				{
					lowest_delay_nodes[i] = 1;
					total_weight += BACKEND_INFO(i).backend_weight;
				}
				else if (lowest_delay > BACKEND_INFO(i).standby_delay)
				{
					int ii;
					lowest_delay = BACKEND_INFO(i).standby_delay;
					for (ii = 0; ii < NUM_BACKENDS; ii++)
					{
						lowest_delay_nodes[ii] = 0;
					}
					lowest_delay_nodes[i] = 1;
					total_weight = BACKEND_INFO(i).backend_weight;
				}
			}
		}

#if defined(sun) || defined(__sun)
		r = (((double) rand()) / RAND_MAX) * total_weight;
#else
		r = (((double) random()) / RAND_MAX) * total_weight;
#endif

		selected_slot = PRIMARY_NODE_ID;

		total_weight = 0.0;
		for (i = 0; i < NUM_BACKENDS; i++)
		{
			if (lowest_delay_nodes[i] == 0)
				continue;

			if (selected_slot == PRIMARY_NODE_ID)
			{
				selected_slot = i;
			}

			if (r >= total_weight)
				selected_slot = i;
			else
				break;

			total_weight += BACKEND_INFO(i).backend_weight;
		}
	}

	ereport(DEBUG1,
			(errmsg("selecting load balance node"),
			 errdetail("selected backend id is %d", selected_slot)));
	return selected_slot;
}

/*
 * Returns PostgreSQL version.
 * The returned PgVersion struct is in static memory.
 * Caller must not modify it.
 *
 * Note:
 * Must be called while query context already exists.
 * If there's something goes wrong, this raises FATAL. So never returns to caller.
 *
 */
PGVersion *
Pgversion(POOL_CONNECTION_POOL * backend)
{
#define VERSION_BUF_SIZE	10
	static	PGVersion	pgversion;
	static	POOL_RELCACHE *relcache;
	char	*result;
	char	*p;
	char	buf[VERSION_BUF_SIZE];
	int		i;
	int		major;
	int		minor;

	/*
	 * First, check local cache. If cache is set, just return it.
	 */
	if (pgversion.major != 0)
	{
		ereport(DEBUG5,
				(errmsg("Pgversion: local cache returned")));

		return &pgversion;
	}

	if (!relcache)
	{
		/*
		 * Create relcache.
		 */
		relcache = pool_create_relcache(pool_config->relcache_size, "SELECT version()",
										string_register_func, string_unregister_func, false);
		if (relcache == NULL)
		{
			ereport(FATAL,
					(errmsg("Pgversion: unable to create relcache while getting PostgreSQL version.")));
			return NULL;
		}
	}

	/*
	 * Search relcache.
	 */
	result = (char *)pool_search_relcache(relcache, backend, "version");
	if (result == 0)
	{
		ereport(FATAL,
				(errmsg("Pgversion: unable to search relcache while getting PostgreSQL version.")));
		return NULL;
	}

	ereport(DEBUG5,
			(errmsg("Pgversion: version string: %s", result)));

	/*
	 * Extract major version number.  We create major version as "version" *
	 * 10.  For example, for V10, the major version number will be 100, for
	 * V9.6 it will be 96, and so on.  For alpha or beta version, the version
	 * string could be something like "12beta1". In this case we assume that
	 * atoi(3) is smart enough to stop at the first character which is not a
	 * valid digit (in our case 'b')). So "12beta1" should be converted to 12.
	 */
	p = strchr(result, ' ');
	if (p == NULL)
	{
		ereport(FATAL,
				(errmsg("Pgversion: unable to find the first space in the version string: %s", result)));
		return NULL;
	}

	p++;
	i = 0;
	while (i < VERSION_BUF_SIZE - 1 && p && *p != '.')
	{
		buf[i++] = *p++;
	}
	buf[i] = '\0';
	major = atoi(buf);
	ereport(DEBUG5,
			(errmsg("Pgversion: major version: %d", major)));

	/* Assuming PostgreSQL V100 is the final release:-) */
	if (major < 6 || major > 100)
	{
		ereport(FATAL,
				(errmsg("Pgversion: wrong major version: %d", major)));
		return NULL;
	}

	/*
	 * If major version is 10 or above, we are done to extract major.
	 * Otherwise extract below decimal point part.
	 */
	if (major >= 10)
	{
		major *= 10;
	}
	else
	{
		p++;
		i = 0;
		while (i < VERSION_BUF_SIZE -1 && p && *p != '.' && *p != ' ')
		{
			buf[i++] = *p++;
		}
		buf[i] = '\0';
		major = major * 10 + atoi(buf);
		ereport(DEBUG5,
				(errmsg("Pgversion: major version: %d", major)));
		pgversion.major = major;
	}

	/*
	 * Extract minor version.
	 */
	p++;
	i = 0;
	while (i < VERSION_BUF_SIZE -1 && p && *p != '.' && *p != ' ')
	{
		buf[i++] = *p++;
	}
	buf[i] = '\0';
	minor = atoi(buf);
	ereport(DEBUG5,
			(errmsg("Pgversion: minor version: %d", minor)));

	if (minor < 0 || minor > 100)
	{
		ereport(FATAL,
				(errmsg("Pgversion: wrong minor version: %d", minor)));
		return NULL;
	}


	/*
	 * Ok, everything looks good. Set the local cache.
	 */
	pgversion.major = major;
	pgversion.minor = minor;
	strncpy(pgversion.version_string, result, sizeof(pgversion.version_string) - 1);

	return &pgversion;
}

/*
 * Given db node specified in pgpool.conf, returns appropriate physical
 * DB node id.
 * Acceptable db node specifications are:
 *
 * primary: primary node
 * standby: any of standby node
 * numeric: physical node id
 *
 * If specified node does exist, returns MAIN_NODE_ID.  If "standby" is
 * specified, returns -1. Caller should choose one of standby nodes
 * appropriately.
 */
static int
choose_db_node_id(char *str)
{
	int			node_id = MAIN_NODE_ID;

	if (!strcmp("primary", str) && PRIMARY_NODE_ID >= 0)
	{
		node_id = PRIMARY_NODE_ID;
	}
	else if (!strcmp("standby", str))
	{
		node_id = -1;
	}
	else
	{
		int			tmp = atoi(str);

		if (tmp >= 0 && tmp < NUM_BACKENDS)
			node_id = tmp;
	}
	return node_id;
}
/*
 *---------------------------------------------------------------------------------
 * Snapshot Isolation modules
 * Pgpool-II's native replication mode has not consider snapshot isolation
 * among backend nodes.  This leads to database inconsistency among backend
 * nodes. A new mode called "snapshot_isolation" solves the problem.
 *
 * Consider following example. S1, S2 are concurrent sessions. N1, N2 are
 * backend nodes. The initial value of t1.i is 0. S2/N2 does see the row
 * having i = 1 and successfully deletes the row. However S2/N1 does not see
 * the row having i = 1 since S1/N1 has not committed yet at the when S2/S1
 * tries to delete the row. As a result, t1.i is not consistent among N1 and
 * N2. There's no way to avoid this in the native replication mode. So we
 * propose the new mode which uses snapshot isolation (i.e. REPEATABLE READ or
 * SERIALIZABLE mode in PostgreSQL) and controlling the timing of snapshot
 * acquisition in a transaction and commit timing. The algorithm was proposed
 * in "Pangea: An Eager Database Replication Middleware guaranteeing Snapshot
 * Isolation without Modification of Database Servers"
 * (http://www.vldb.org/pvldb/vol2/vldb09-694.pdf).
 *
 * S1/N1: BEGIN;
 * S1/N2: BEGIN;
 * S1/N1: UPDATE t1 SET i = i + 1;	-- i = 1
 * S1/N2: UPDATE t1 SET i = i + 1;  -- i = 1
 * S1/N2: COMMIT;
 * S2/N1: BEGIN;
 * S2/N2: BEGIN;
 * S2/N1: DELETE FROM t1 WHERE i = 1;
 * S2/N2: DELETE FROM t1 WHERE i = 1;
 * S1/N1: COMMIT;
 * S2/N2: COMMIT;
 * S2/N1: COMMIT;
 *---------------------------------------------------------------------------------
 */

#ifdef SI_DEBUG_LOG
#define SI_DEBUG_LOG_LEVEL	LOG
#else
#define SI_DEBUG_LOG_LEVEL	DEBUG5
#endif

/*
 * Enter critical region. All SI operations must be protected by this.
 */
static void
si_enter_critical_region(void)
{
	elog(SI_DEBUG_LOG_LEVEL, "si_enter_critical_region called");
	pool_semaphore_lock(SI_CRITICAL_REGION_SEM);
}

/*
 * Leave critical region.
 */
static void
si_leave_critical_region(void)
{
	elog(SI_DEBUG_LOG_LEVEL, "si_leave_critical_region called");
	pool_semaphore_unlock(SI_CRITICAL_REGION_SEM);
}

/*
 * Returns true if snapshot is already prepared in this session.
 */
bool
si_snapshot_prepared(void)
{
	POOL_SESSION_CONTEXT *session;

	session = pool_get_session_context(true);
	return session->si_state == SI_SNAPSHOT_PREPARED;
}

/*
 * Returns true if the command will acquire snapshot.
 */
bool
si_snapshot_acquire_command(Node *node)
{
	return !is_start_transaction_query(node) &&
		!IsA(node, VariableSetStmt) &&
		!IsA(node, VariableShowStmt);
}

/*
 * Acquire snapshot
 */
void
si_acquire_snapshot(void)
{
	POOL_SESSION_CONTEXT *session;

	session = pool_get_session_context(true);

	if (session->si_state == SI_NO_SNAPSHOT)
	{
		for (;;)
		{
			si_enter_critical_region();
			elog(SI_DEBUG_LOG_LEVEL, "si_acquire_snapshot called: counter: %d", si_manage_info->snapshot_counter);

			if (si_manage_info->commit_counter > 0)
			{
				si_manage_info->commit_waiting_children[my_proc_id] = getpid();
				si_leave_critical_region();
				elog(SI_DEBUG_LOG_LEVEL, "si_acquire_snapshot left critical region");
				sleep(1);
			}
			else
				break;
		}
		si_manage_info->snapshot_counter++;

		si_leave_critical_region();
	}
}

/*
 * Notice that snapshot is acquired
 */
void
si_snapshot_acquired(void)
{
	POOL_SESSION_CONTEXT *session;
	int		i;

	session = pool_get_session_context(true);

	if (session->si_state == SI_NO_SNAPSHOT)
	{
		si_enter_critical_region();

		elog(SI_DEBUG_LOG_LEVEL, "si_snapshot_acquired called: counter: %d", si_manage_info->snapshot_counter);

		si_manage_info->snapshot_counter--;

		if (si_manage_info->snapshot_counter == 0)
		{
			/* wakeup all waiting children */
			for (i = 0; i < pool_config->num_init_children ; i++)
			{
				pid_t pid = si_manage_info->snapshot_waiting_children[i];
				if (pid > 0)
				{
					elog(SI_DEBUG_LOG_LEVEL, "si_snapshot_acquired: send SIGUSR2 to %d", pid);
					kill(pid, SIGUSR2);
					si_manage_info->snapshot_waiting_children[i] = 0;
				}
			}
		}

		si_leave_critical_region();
		session->si_state = SI_SNAPSHOT_PREPARED;
	}
}

/*
 * Commit request
 */
void
si_commit_request(void)
{
	POOL_SESSION_CONTEXT *session;

	session = pool_get_session_context(true);

	elog(SI_DEBUG_LOG_LEVEL, "si_commit_request called");

	if (session->si_state == SI_SNAPSHOT_PREPARED)
	{
		for (;;)
		{
			si_enter_critical_region();
			if (si_manage_info->snapshot_counter > 0)
			{
				si_manage_info->snapshot_waiting_children[my_proc_id] = getpid();
				si_leave_critical_region();
				sleep(1);
			}
			else
				break;
		}

		elog(SI_DEBUG_LOG_LEVEL, "si_commit_request: commit_counter: %d", si_manage_info->commit_counter);
		si_manage_info->commit_counter++;

		si_leave_critical_region();
	}
}

/*
 * Notice that commit is done
 */
void
si_commit_done(void)
{
	POOL_SESSION_CONTEXT *session;
	int		i;

	session = pool_get_session_context(true);

	elog(SI_DEBUG_LOG_LEVEL, "si_commit_done called");

	if (session->si_state == SI_SNAPSHOT_PREPARED)
	{
		si_enter_critical_region();

		elog(SI_DEBUG_LOG_LEVEL, "si_commit_done: commit_counter: %d", si_manage_info->commit_counter);
		si_manage_info->commit_counter--;

		if (si_manage_info->commit_counter == 0)
		{
			/* wakeup all waiting children */
			for (i = 0; i < pool_config->num_init_children ; i++)
			{
				pid_t pid = si_manage_info->commit_waiting_children[i];
				if (pid > 0)
				{
					elog(SI_DEBUG_LOG_LEVEL, "si_commit_done: send SIGUSR2 to %d", pid);
					kill(pid, SIGUSR2);
					si_manage_info->commit_waiting_children[i] = 0;
				}
			}
		}

		si_leave_critical_region();
		session->si_state = SI_NO_SNAPSHOT;
	}
}
