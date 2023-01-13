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
 *
 *
 * pcp.h - pcp header file.
 */

#ifndef PCP_H
#define PCP_H

#include "pool_type.h"
#include "pool_config.h"


#define MAX_USER_PASSWD_LEN    128
/* The largest PCP packet a PCP frontend can send is
 * the user authentication packet, and the maximum size
 * of the PCP authentication packet can be
 * MAX_USER_PASSWD_LEN + MAX_USER_PASSWD_LEN + SIZE OF INT */
#define MAX_PCP_PACKET_LENGTH	260

typedef struct PCPWDNodeInfo
{
	int			state;
	int			membership_status;
	char		nodeName[WD_MAX_HOST_NAMELEN];
	char		hostName[WD_MAX_HOST_NAMELEN];	/* host name */
	char		stateName[WD_MAX_HOST_NAMELEN]; /* state name */
	char		membership_status_string[WD_MAX_HOST_NAMELEN]; /* membership status of this node */
	int			wd_port;		/* watchdog port */
	int			wd_priority;	/* node priority in leader election */
	int			pgpool_port;	/* pgpool port */
	char		delegate_ip[WD_MAX_HOST_NAMELEN];	/* delegate IP */
	int			id;
}			PCPWDNodeInfo;

typedef struct PCPWDClusterInfo
{
	int			remoteNodeCount;
	int			memberRemoteNodeCount;
	int			nodesRequiredForQuorum;
	int			quorumStatus;
	int			aliveNodeCount;
	bool		escalated;
	char		leaderNodeName[WD_MAX_HOST_NAMELEN];
	char		leaderHostName[WD_MAX_HOST_NAMELEN];
	int			nodeCount;
	PCPWDNodeInfo nodeList[1];
}			PCPWDClusterInfo;

/* --------------------------------
 * pcp.c
 * --------------------------------
 */

/* ------------------------------
 * pcp_error.c
 * ------------------------------
 */

#endif							/* PCP_H */
