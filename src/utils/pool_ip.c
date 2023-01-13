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
 * Portions Copyright (c) 2003-2019	PgPool Global Development Group
 * Portions Copyright (c) 1996-2005, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
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
 * ------------------------------
 *
 *
 * This file and the IPV6 implementation were initially provided by
 * Nigel Kukard <nkukard@lbsd.net>, Linux Based Systems Design
 * http://www.lbsd.net.
 *
 * pool_ip.c.: IPv6-aware network access.
 *
 */

#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <ifaddrs.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/un.h>

#include "pool.h"
#include "utils/pool_ip.h"
#include "pool_config.h"
#include "utils/elog.h"
static int rangeSockAddrAF_INET(const struct sockaddr_in *addr,
					 const struct sockaddr_in *netaddr,
					 const struct sockaddr_in *netmask);

static int rangeSockAddrAF_INET6(const struct sockaddr_in6 *addr,
					  const struct sockaddr_in6 *netaddr,
					  const struct sockaddr_in6 *netmask);

static int getaddrinfo_unix(const char *path,
				 const struct addrinfo *hintsp,
				 struct addrinfo **result);

static int getnameinfo_unix(const struct sockaddr_un *sa, int salen,
				 char *node, int nodelen,
				 char *service, int servicelen,
				 int flags);

/*
 * pool_getnameinfo_all - get name info for Unix, IPv4 and IPv6 sockets
 * caller MUST allocate NI_MAXHOST, NI_MAXSERV bytes for remote_host and remote_port
 */
void
pool_getnameinfo_all(SockAddr *saddr, char *remote_host, char *remote_port)
{
	remote_host[0] = '\0';
	remote_port[0] = '\0';

	if (getnameinfo_all(&saddr->addr, saddr->salen,
						remote_host, NI_MAXHOST,
						remote_port, NI_MAXSERV,
						(pool_config->log_hostname ? 0 : NI_NUMERICHOST) | NI_NUMERICSERV))
	{
		int			ret = getnameinfo_all(&saddr->addr, saddr->salen,
										  remote_host, NI_MAXHOST,
										  remote_port, NI_MAXSERV,
										  NI_NUMERICHOST | NI_NUMERICSERV);

		if (ret)
			ereport(WARNING,
					(errmsg("getnameinfo failed with error: \"%s\"", gai_strerror(ret))));
	}
}

/*
 *	getaddrinfo_all - get address info for Unix, IPv4 and IPv6 sockets
 */
int
getaddrinfo_all(const char *hostname, const char *servname,
				const struct addrinfo *hintp, struct addrinfo **result)
{
	/* not all versions of getaddrinfo() zero *result on failure */
	*result = NULL;

	if (hintp->ai_family == AF_UNIX)
		return getaddrinfo_unix(servname, hintp, result);

	/* NULL has special meaning to getaddrinfo(). */
	return getaddrinfo((!hostname || hostname[0] == '\0') ? NULL : hostname,
					   servname, hintp, result);
}


/*
 *	freeaddrinfo_all - free addrinfo structures for IPv4, IPv6, or Unix
 *
 * Note: the ai_family field of the original hint structure must be passed
 * so that we can tell whether the addrinfo struct was built by the system's
 * getaddrinfo() routine or our own getaddrinfo_unix() routine.  Some versions
 * of getaddrinfo() might be willing to return AF_UNIX addresses, so it's
 * not safe to look at ai_family in the addrinfo itself.
 */
void
freeaddrinfo_all(int hint_ai_family, struct addrinfo *ai)
{
	if (hint_ai_family == AF_UNIX)
	{
		/* struct was built by getaddrinfo_unix (see getaddrinfo_all) */
		while (ai != NULL)
		{
			struct addrinfo *p = ai;

			ai = ai->ai_next;
			free(p->ai_addr);
			free(p);
		}
	}
	else
	{
		/* struct was built by getaddrinfo() */
		if (ai != NULL)
			freeaddrinfo(ai);
	}
}


/*
 *	getnameinfo_all - get name info for Unix, IPv4 and IPv6 sockets
 *
 * The API of this routine differs from the standard getnameinfo() definition
 * in two ways: first, the addr parameter is declared as sockaddr_storage
 * rather than struct sockaddr, and second, the node and service fields are
 * guaranteed to be filled with something even on failure return.
 */
int
getnameinfo_all(const struct sockaddr_storage *addr, int salen,
				char *node, int nodelen,
				char *service, int servicelen,
				int flags)
{
	int			rc;

	if (addr && addr->ss_family == AF_UNIX)
	{
		rc = getnameinfo_unix((const struct sockaddr_un *) addr, salen,
							  node, nodelen,
							  service, servicelen,
							  flags);
	}
	else
	{
		rc = getnameinfo((const struct sockaddr *) addr, salen,
						 node, nodelen,
						 service, servicelen,
						 flags);

	}

	if (rc != 0)
	{
		if (node)
			strncpy(node, "???", nodelen);
		if (service)
			strncpy(service, "???", servicelen);
	}

	return rc;
}


#ifndef HAVE_GAI_STRERROR
const char *
gai_strerror(int errcode)
{
#ifdef HAVE_HSTRERROR
	int			hcode;

	switch (errcode)
	{
		case EAI_NONAME:
			hcode = HOST_NOT_FOUND;
			break;
		case EAI_AGAIN:
			hcode = TRY_AGAIN;
			break;
		case EAI_FAIL:
		default:
			hcode = NO_RECOVERY;
			break;
	}

	return hstrerror(hcode);
#else							/* !HAVE_HSTRERROR */

	switch (errcode)
	{
		case EAI_NONAME:
			return "Unknown host";
		case EAI_AGAIN:
			return "Host name lookup failure";
			/* Errors below are probably WIN32 only */
#ifdef EAI_BADFLAGS
		case EAI_BADFLAGS:
			return "Invalid argument";
#endif
#ifdef EAI_FAMILY
		case EAI_FAMILY:
			return "Address family not supported";
#endif
#ifdef EAI_MEMORY
		case EAI_MEMORY:
			return "Not enough memory";
#endif
#ifdef EAI_NODATA
#ifndef WIN32_ONLY_COMPILER		/* MSVC complains because another case has the
								 * same value */
		case EAI_NODATA:
			return "No host data of that type was found";
#endif
#endif
#ifdef EAI_SERVICE
		case EAI_SERVICE:
			return "Class type not found";
#endif
#ifdef EAI_SOCKTYPE
		case EAI_SOCKTYPE:
			return "Socket type not supported";
#endif
		default:
			return "Unknown server error";
	}
#endif							/* HAVE_HSTRERROR */
}
#endif							/* HAVE_GAI_STRERROR */


/*
 *	getaddrinfo_unix - get unix socket info using IPv6-compatible API
 *
 *	Bugs: only one addrinfo is set even though hintsp is NULL or
 *		  ai_socktype is 0
 *		  AI_CANONNAME is not supported.
 *
 */
static int
getaddrinfo_unix(const char *path, const struct addrinfo *hintsp,
				 struct addrinfo **result)
{
	struct addrinfo hints;
	struct addrinfo *aip;
	struct sockaddr_un *unp;

	*result = NULL;

	memset(&hints, 0, sizeof(hints));

	if (strlen(path) >= sizeof(unp->sun_path))
		return EAI_FAIL;

	if (hintsp == NULL)
	{
		hints.ai_family = AF_UNIX;
		hints.ai_socktype = SOCK_STREAM;
	}
	else
		memcpy(&hints, hintsp, sizeof(hints));

	if (hints.ai_socktype == 0)
		hints.ai_socktype = SOCK_STREAM;

	if (hints.ai_family != AF_UNIX)
	{
		/* shouldn't have been called */
		return EAI_FAIL;
	}

	aip = calloc(1, sizeof(struct addrinfo));
	if (aip == NULL)
		return EAI_MEMORY;

	unp = calloc(1, sizeof(struct sockaddr_un));
	if (unp == NULL)
	{
		free(aip);
		return EAI_MEMORY;
	}

	aip->ai_family = AF_UNIX;
	aip->ai_socktype = hints.ai_socktype;
	aip->ai_protocol = hints.ai_protocol;
	aip->ai_next = NULL;
	aip->ai_canonname = NULL;
	*result = aip;

	unp->sun_family = AF_UNIX;
	aip->ai_addr = (struct sockaddr *) unp;
	aip->ai_addrlen = sizeof(struct sockaddr_un);

	strcpy(unp->sun_path, path);

#ifdef HAVE_STRUCT_SOCKADDR_STORAGE_SS_LEN
	unp->sun_len = sizeof(struct sockaddr_un);
#endif

	return 0;
}

/*
 * Convert an address to a hostname.
 */
static int
getnameinfo_unix(const struct sockaddr_un *sa, int salen,
				 char *node, int nodelen,
				 char *service, int servicelen,
				 int flags)
{
	int			ret = -1;

	/* Invalid arguments. */
	if (sa == NULL || sa->sun_family != AF_UNIX ||
		(node == NULL && service == NULL))
		return EAI_FAIL;

	/* We don't support those. */
	if ((node && !(flags & NI_NUMERICHOST))
		|| (service && !(flags & NI_NUMERICSERV)))
		return EAI_FAIL;

	if (node)
	{
		ret = snprintf(node, nodelen, "%s", "[local]");
		if (ret == -1 || ret > nodelen)
			return EAI_MEMORY;
	}

	if (service)
	{
		ret = snprintf(service, servicelen, "%s", sa->sun_path);
		if (ret == -1 || ret > servicelen)
			return EAI_MEMORY;
	}

	return 0;
}


static int
rangeSockAddrAF_INET(const struct sockaddr_in *addr,
					 const struct sockaddr_in *netaddr,
					 const struct sockaddr_in *netmask)
{
	if (((addr->sin_addr.s_addr ^ netaddr->sin_addr.s_addr) &
		 netmask->sin_addr.s_addr) == 0)
		return 1;
	else
		return 0;
}


static int
rangeSockAddrAF_INET6(const struct sockaddr_in6 *addr,
					  const struct sockaddr_in6 *netaddr,
					  const struct sockaddr_in6 *netmask)
{
	int			i;

	for (i = 0; i < 16; i++)
	{
		if (((addr->sin6_addr.s6_addr[i] ^ netaddr->sin6_addr.s6_addr[i]) &
			 netmask->sin6_addr.s6_addr[i]) != 0)
			return 0;
	}

	return 1;
}

/*
 *	SockAddr_cidr_mask - make a network mask of the appropriate family
 *	  and required number of significant bits
 *
 * numbits can be null, in which case the mask is fully set.
 *
 * The resulting mask is placed in *mask, which had better be big enough.
 *
 * Return value is 0 if okay, -1 if not.
 */
int
SockAddr_cidr_mask(struct sockaddr_storage *mask, char *numbits, int family)
{
	long		bits;
	char	   *endptr;

	if (numbits == NULL)
	{
		bits = (family == AF_INET) ? 32 : 128;
	}
	else
	{
		bits = strtol(numbits, &endptr, 10);
		if (*numbits == '\0' || *endptr != '\0')
			return -1;
	}

    switch (family)
	{
		case AF_INET:
			{
				struct sockaddr_in mask4;
				long            maskl;

				if (bits < 0 || bits > 32)
					return -1;
				/* avoid "x << 32", which is not portable */
				if (bits > 0)
					maskl = (0xffffffffUL << (32 - (int) bits))
						& 0xffffffffUL;
				else
					maskl = 0;
				memset(&mask4, 0, sizeof(mask4));
				mask4.sin_addr.s_addr = htonl(maskl);
				memcpy(mask, &mask4, sizeof(mask4));
				break;
			}

		case AF_INET6:
			{
				struct sockaddr_in6 mask6;
				int			i;

				memset(&mask6, 0, sizeof(mask6));

				if (bits < 0 || bits > 128)
					return -1;
				for (i = 0; i < 16; i++)
				{
					if (bits <= 0)
						mask6.sin6_addr.s6_addr[i] = 0;
					else if (bits >= 8)
						mask6.sin6_addr.s6_addr[i] = 0xff;
					else
					{
						mask6.sin6_addr.s6_addr[i] =
							(0xff << (8 - (int) bits)) & 0xff;
					}
					bits -= 8;
				}
				memcpy(mask, &mask6, sizeof(mask6));
				break;
			}
		default:
			return -1;
	}

	mask->ss_family = family;
	return 0;
}


/*
 * promote_v4_to_v6_addr --- convert an AF_INET addr to AF_INET6, using
 *		the standard convention for IPv4 addresses mapped into IPv6 world
 *
 * The passed addr is modified in place; be sure it is large enough to
 * hold the result!  Note that we only worry about setting the fields
 * that rangeSockAddr will look at.
 */
void
promote_v4_to_v6_addr(struct sockaddr_storage *addr)
{
	struct sockaddr_in addr4;
	struct sockaddr_in6 addr6;
	uint32		ip4addr;

	memcpy(&addr4, addr, sizeof(addr4));
	ip4addr = ntohl(addr4.sin_addr.s_addr);

	memset(&addr6, 0, sizeof(addr6));

	addr6.sin6_family = AF_INET6;

	addr6.sin6_addr.s6_addr[10] = 0xff;
	addr6.sin6_addr.s6_addr[11] = 0xff;
	addr6.sin6_addr.s6_addr[12] = (ip4addr >> 24) & 0xFF;
	addr6.sin6_addr.s6_addr[13] = (ip4addr >> 16) & 0xFF;
	addr6.sin6_addr.s6_addr[14] = (ip4addr >> 8) & 0xFF;
	addr6.sin6_addr.s6_addr[15] = (ip4addr) & 0xFF;

	memcpy(addr, &addr6, sizeof(addr6));
}

/*
 * promote_v4_to_v6_mask --- convert an AF_INET netmask to AF_INET6, using
 *		the standard convention for IPv4 addresses mapped into IPv6 world
 *
 * This must be different from promote_v4_to_v6_addr because we want to
 * set the high-order bits to 1's not 0's.
 *
 * The passed addr is modified in place; be sure it is large enough to
 * hold the result!  Note that we only worry about setting the fields
 * that rangeSockAddr will look at.
 */
void
promote_v4_to_v6_mask(struct sockaddr_storage *addr)
{
	struct sockaddr_in addr4;
	struct sockaddr_in6 addr6;
	uint32		ip4addr;
	int			i;

	memcpy(&addr4, addr, sizeof(addr4));
	ip4addr = ntohl(addr4.sin_addr.s_addr);

	memset(&addr6, 0, sizeof(addr6));

	addr6.sin6_family = AF_INET6;

	for (i = 0; i < 12; i++)
		addr6.sin6_addr.s6_addr[i] = 0xff;

	addr6.sin6_addr.s6_addr[12] = (ip4addr >> 24) & 0xFF;
	addr6.sin6_addr.s6_addr[13] = (ip4addr >> 16) & 0xFF;
	addr6.sin6_addr.s6_addr[14] = (ip4addr >> 8) & 0xFF;
	addr6.sin6_addr.s6_addr[15] = (ip4addr) & 0xFF;

	memcpy(addr, &addr6, sizeof(addr6));
}

/*
 * range_sockaddr - is addr within the subnet specified by netaddr/netmask ?
 *
 * Note: caller must already have verified that all three addresses are
 * in the same address family; and AF_UNIX addresses are not supported.
 */
int
rangeSockAddr(const struct sockaddr_storage *addr,
			  const struct sockaddr_storage *netaddr,
			  const struct sockaddr_storage *netmask)
{
	if (addr->ss_family == AF_INET)
		return rangeSockAddrAF_INET((const struct sockaddr_in *) addr,
									(const struct sockaddr_in *) netaddr,
									(const struct sockaddr_in *) netmask);
	else if (addr->ss_family == AF_INET6)
		return rangeSockAddrAF_INET6((const struct sockaddr_in6 *) addr,
									 (const struct sockaddr_in6 *) netaddr,
									 (const struct sockaddr_in6 *) netmask);
	else
		return 0;
}

/*
 * Run the callback function for the addr/mask, after making sure the
 * mask is sane for the addr.
 */
static void
run_ifaddr_callback(PgIfAddrCallback callback, void *cb_data,
					struct sockaddr *addr, struct sockaddr *mask)
{
	struct sockaddr_storage fullmask;

	if (!addr)
		return;

	/* Check that the mask is valid */
	if (mask)
	{
		if (mask->sa_family != addr->sa_family)
		{
			mask = NULL;
		}
		else if (mask->sa_family == AF_INET)
		{
			if (((struct sockaddr_in *) mask)->sin_addr.s_addr == INADDR_ANY)
				mask = NULL;
		}
		else if (mask->sa_family == AF_INET6)
		{
			if (IN6_IS_ADDR_UNSPECIFIED(&((struct sockaddr_in6 *) mask)->sin6_addr))
				mask = NULL;
		}
	}

	/* If mask is invalid, generate our own fully-set mask */
	if (!mask)
	{
		SockAddr_cidr_mask(&fullmask, NULL, addr->sa_family);
		mask = (struct sockaddr *) &fullmask;
	}

	(*callback) (addr, mask, cb_data);
}

/*
 * Enumerate the system's network interface addresses and call the callback
 * for each one.  Returns 0 if successful, -1 if trouble.
 *
 * This version uses the getifaddrs() interface, which is available on
 * BSDs, AIX, and modern Linux.
 */
int
pg_foreach_ifaddr(PgIfAddrCallback callback, void *cb_data)
{
	struct ifaddrs *ifa,
			   *l;

	if (getifaddrs(&ifa) < 0)
		return -1;

	for (l = ifa; l; l = l->ifa_next)
		run_ifaddr_callback(callback, cb_data,
							l->ifa_addr, l->ifa_netmask);

	freeifaddrs(ifa);
	return 0;
}
