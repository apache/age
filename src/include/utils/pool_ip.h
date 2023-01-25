/* -*-pgsql-c-*- */
/*
 *
 * $Header$
 *
 * This file was imported from PostgreSQL 8.0.8 source code.
 * See below for the copyright and description.
 *
 * pgpool: a language independent connection pool server for PostgreSQL
 * written by Tatsuo Ishii
 *
 * Portions Copyright (c) 2003-2020	PgPool Global Development Group
 * Portions Copyright (c) 2003-2005, PostgreSQL Global Development Group
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
 * pool_ip.h.: Definitions for IPv6-aware network access.
 *
 */

#ifndef POOL_IP_H
#define POOL_IP_H

#include "pool_type.h"

extern int SockAddr_cidr_mask(struct sockaddr_storage *mask,
				   char *numbits, int family);

typedef void (*PgIfAddrCallback) (struct sockaddr *addr, struct sockaddr *netmask, void *cb_data);

extern int getaddrinfo_all(const char *hostname, const char *servname,
				const struct addrinfo *hintp,
				struct addrinfo **result);
extern void freeaddrinfo_all(int hint_ai_family, struct addrinfo *ai);

extern int getnameinfo_all(const struct sockaddr_storage *addr, int salen,
				char *node, int nodelen,
				char *service, int servicelen,
				int flags);

extern int rangeSockAddr(const struct sockaddr_storage *addr,
			  const struct sockaddr_storage *netaddr,
			  const struct sockaddr_storage *netmask);


/* imported from PostgreSQL getaddrinfo.c */
#ifndef HAVE_GAI_STRERROR
extern const char *gai_strerror(int errcode);
#endif							/* HAVE_GAI_STRERROR */

extern void promote_v4_to_v6_addr(struct sockaddr_storage *addr);
extern void promote_v4_to_v6_mask(struct sockaddr_storage *addr);

extern int	pg_foreach_ifaddr(PgIfAddrCallback callback, void *cb_data);

extern void pool_getnameinfo_all(SockAddr *saddr, char *remote_host, char *remote_port);

#define IS_AF_INET(fam) ((fam) == AF_INET)
#define IS_AF_UNIX(fam) ((fam) == AF_UNIX)

#endif							/* IP_H */
