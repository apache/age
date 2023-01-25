/*-------------------------------------------------------------------------
 *
 * pgpool_adm.h
 *
 *
 * Copyright (c) 2002-2018, PostgreSQL Global Development Group
 *
 * Author: Jehan-Guillaume (ioguix) de Rorthais <jgdr@dalibo.com>
 *
 * IDENTIFICATION
 *	  contrib/pgpool_adm/pgpool_adm.h
 *
 *-------------------------------------------------------------------------
 */

#ifndef PGPOOL_ADM_H
#define PGPOOL_ADM_H

PG_MODULE_MAGIC;


Datum		_pcp_node_info(PG_FUNCTION_ARGS);
Datum		_pcp_health_check_stats(PG_FUNCTION_ARGS);
Datum		_pcp_pool_status(PG_FUNCTION_ARGS);
Datum		_pcp_node_count(PG_FUNCTION_ARGS);
Datum		_pcp_attach_node(PG_FUNCTION_ARGS);
Datum		_pcp_detach_node(PG_FUNCTION_ARGS);

PG_FUNCTION_INFO_V1(_pcp_node_info);
PG_FUNCTION_INFO_V1(_pcp_health_check_stats);
PG_FUNCTION_INFO_V1(_pcp_pool_status);
PG_FUNCTION_INFO_V1(_pcp_node_count);
PG_FUNCTION_INFO_V1(_pcp_attach_node);
PG_FUNCTION_INFO_V1(_pcp_detach_node);

#endif
