/* -*-pgsql-c-*- */
/*
 * $Header$
 *
 * pgpool: a language independent connection pool server for PostgreSQL
 * written by Tatsuo Ishii
 *
 * Copyright (c) 2003-2010	PgPool Global Development Group
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
 * pool_lobj.c: Transparently translate lo_creat call to lo_create so
 * that large objects replicated safely.
 * lo_create anyway.
 */
#include "config.h"
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <netinet/in.h>

#include "pool.h"
#include "rewrite/pool_lobj.h"
#include "utils/pool_relcache.h"
#include "protocol/pool_process_query.h"
#include "utils/elog.h"
#include "pool_config.h"

/*
 * Rewrite lo_creat call to lo_create call if:
 * 1) it's a lo_creat function call
 * 2) PostgreSQL has lo_create
 * 3) In replication mode
 * 4) lobj_lock_table exists and writable to everyone
 *
 * The argument for lo_create is created by fetching max(loid)+1 from
 * pg_largeobject. To avoid race condition, we lock lobj_lock_table.
 *
 * Caller should call this only if protocol is V3 or higher(for
 * now. There's no reason for this function not working with V2
 * protocol).  Return value is a rewritten packet without kind and
 * length. This is allocated in a static memory. New packet length is
 * set to *len.
 */
char *
pool_rewrite_lo_creat(char kind, char *packet, int packet_len,
					  POOL_CONNECTION * frontend, POOL_CONNECTION_POOL * backend, int *len)
{
#define LO_CREAT_OID_QUERY "SELECT oid FROM pg_catalog.pg_proc WHERE proname = 'lo_creat' and pronamespace = (SELECT oid FROM pg_catalog.pg_namespace WHERE nspname = 'pg_catalog')"

#define LO_CREATE_OID_QUERY "SELECT oid FROM pg_catalog.pg_proc WHERE proname = 'lo_create' and pronamespace = (SELECT oid FROM pg_catalog.pg_namespace WHERE nspname = 'pg_catalog')"

#define LO_CREATE_PACKET_LENGTH sizeof(int32)*3+sizeof(int16)*4

#define LOCK_QUERY "LOCK TABLE %s IN SHARE ROW EXCLUSIVE MODE"

#define GET_MAX_LOBJ_KEY "SELECT coalesce(max(loid)::INTEGER, 0)+1 FROM pg_catalog.pg_largeobject"

	static char rewritten_packet[LO_CREATE_PACKET_LENGTH];

	static POOL_RELCACHE * relcache_lo_creat;
	static POOL_RELCACHE * relcache_lo_create;

	int			lo_creat_oid;
	int			lo_create_oid;
	int			orig_fcall_oid;
	POOL_STATUS status;
	char		qbuf[1024];
	char	   *p;
	POOL_SELECT_RESULT *result;
	int			lobjid;
	int32		int32val;
	int16		int16val;
	int16		result_format_code;

	if (kind != 'F')
		return NULL;			/* not function call */

	if (!strcmp(pool_config->lobj_lock_table, ""))
		return NULL;			/* no lock table */

	if (!REPLICATION)
		return NULL;			/* not in replication mode */

	/*
	 * If relcache does not exist, create it.
	 */
	if (!relcache_lo_creat)
	{
		relcache_lo_creat = pool_create_relcache(1, LO_CREAT_OID_QUERY,
												 int_register_func, int_unregister_func,
												 false);
		if (relcache_lo_creat == NULL)
		{
			ereport(WARNING,
					(errmsg("unable to create relcache, while rewriting LO CREATE")));
			return NULL;
		}
	}

	/*
	 * Get lo_creat oid
	 */
	lo_creat_oid = (int) (intptr_t) pool_search_relcache(relcache_lo_creat, backend, "pg_proc");

	memmove(&orig_fcall_oid, packet, sizeof(int32));
	orig_fcall_oid = ntohl(orig_fcall_oid);

	ereport(DEBUG1,
			(errmsg("rewriting LO CREATE"),
			 errdetail("orig_fcall_oid:% d lo_creat_oid: %d", orig_fcall_oid, lo_creat_oid)));

	/*
	 * This function call is calling lo_creat?
	 */
	if (orig_fcall_oid != lo_creat_oid)
		return NULL;

	/*
	 * If relcache does not exist, create it.
	 */
	if (!relcache_lo_create)
	{
		relcache_lo_create = pool_create_relcache(1, LO_CREATE_OID_QUERY,
												  int_register_func, int_unregister_func,
												  false);
		if (relcache_lo_create == NULL)
		{
			ereport(LOG,
					(errmsg("rewriting LO CREATE, unable to create relcache")));
			return NULL;
		}
	}

	/*
	 * Get lo_create oid
	 */
	lo_create_oid = (int) (intptr_t) pool_search_relcache(relcache_lo_create, backend, "pg_proc");

	ereport(DEBUG1,
			(errmsg("rewriting LO CREATE"),
			 errdetail("lo_creat_oid: %d lo_create_oid: %d", lo_creat_oid, lo_create_oid)));

	/*
	 * Parse input packet
	 */
	memmove(&int16val, packet + packet_len - sizeof(int16), sizeof(int16));
	result_format_code = ntohs(int16val);

	/* sanity check */
	if (result_format_code != 0 && result_format_code != 1)
	{
		ereport(LOG,
				(errmsg("rewriting LO CREATE, invalid return format code: %d", int16val)));
		return NULL;
	}
	ereport(DEBUG1,
			(errmsg("rewriting LO CREATE"),
			 errdetail("return format code: %d", int16val)));

	/*
	 * Ok, do it...
	 */
	/* issue lock table command to lob_lock_table */
	snprintf(qbuf, sizeof(qbuf), "LOCK TABLE %s IN SHARE ROW EXCLUSIVE MODE", pool_config->lobj_lock_table);
	per_node_statement_log(backend, MAIN_NODE_ID, qbuf);
	status = do_command(frontend, MAIN(backend), qbuf, MAJOR(backend), MAIN_CONNECTION(backend)->pid,
						MAIN_CONNECTION(backend)->key, 0);
	if (status == POOL_END)
	{
		ereport(WARNING,
				(errmsg("rewriting LO CREATE, failed to execute LOCK")));
		return NULL;
	}

	/*
	 * If transaction state is E, do_command failed to execute command
	 */
	if (TSTATE(backend, MAIN_NODE_ID) == 'E')
	{
		ereport(LOG,
				(errmsg("failed while rewriting LO CREATE"),
				 errdetail("failed to execute: %s", qbuf)));
		return NULL;
	}

	/* get max lobj id */
	per_node_statement_log(backend, MAIN_NODE_ID, GET_MAX_LOBJ_KEY);
	do_query(MAIN(backend), GET_MAX_LOBJ_KEY, &result, MAJOR(backend));

	if (!result)
	{
		ereport(LOG,
				(errmsg("failed while rewriting LO CREATE"),
				 errdetail("failed to execute: %s", GET_MAX_LOBJ_KEY)));
		return NULL;
	}

	lobjid = atoi(result->data[0]);
	ereport(DEBUG1,
			(errmsg("rewriting LO CREATE"),
			 errdetail("lobjid:%d", lobjid)));

	free_select_result(result);

	/* sanity check */
	if (lobjid <= 0)
	{
		ereport(WARNING,
				(errmsg("rewriting LO CREATE, wrong lob id: %d", lobjid)));
		return NULL;
	}

	/*
	 * Create lo_create call packet
	 */
	p = rewritten_packet;

	*len = LO_CREATE_PACKET_LENGTH;

	int32val = htonl(lo_create_oid);
	memmove(p, &int32val, sizeof(int32));	/* lo_create oid */
	p += sizeof(int32);

	int16val = htons(1);
	memmove(p, &int16val, sizeof(int16));	/* number of argument format code */
	p += sizeof(int16);

	int16val = htons(1);
	memmove(p, &int16val, sizeof(int16));	/* format code */
	p += sizeof(int16);

	int16val = htons(1);
	memmove(p, &int16val, sizeof(int16));	/* number of arguments */
	p += sizeof(int16);

	int32val = htonl(4);
	memmove(p, &int32val, sizeof(int32));	/* argument length */
	p += sizeof(int32);

	int32val = htonl(lobjid);
	memmove(p, &int32val, sizeof(int32));	/* argument(lobj id) */
	p += sizeof(int32);

	int16val = htons(result_format_code);
	memmove(p, &int16val, sizeof(int16));	/* result format code */

	return rewritten_packet;
}
