/*
 * $Header$
 *
 * Handles watchdog connection, and protocol communication with pgpool-II
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

#include <pthread.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <netdb.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <ctype.h>
#include <errno.h>

#include <sys/socket.h>
#ifdef __linux__
#include <linux/netlink.h>
#include <linux/rtnetlink.h>
#include <linux/if.h>
#else							/* __linux__ */
#include <net/route.h>
#include <net/if.h>

#ifdef AF_LINK
#include <net/if_dl.h>
#endif
#endif							/* __linux__ */
#include <arpa/inet.h>
#include <ifaddrs.h>

#ifdef __FreeBSD__
#include <netinet/in.h>
#endif

#include "pool.h"

#include "utils/elog.h"
#include "pool_config.h"
#include "watchdog/watchdog.h"
#include "watchdog/wd_utils.h"

#ifndef __linux__
#define IFF_LOWER_UP	0x10000
#endif

static int	exec_if_cmd(char *path, char *command);


List *
get_all_local_ips(void)
{
	struct ifaddrs *ifAddrStruct = NULL;
	struct ifaddrs *ifa = NULL;
	void	   *tmpAddrPtr = NULL;
	List	   *local_addresses = NULL;

	getifaddrs(&ifAddrStruct);
	for (ifa = ifAddrStruct; ifa != NULL; ifa = ifa->ifa_next)
	{
		if (!ifa->ifa_addr)
			continue;

		if (ifa->ifa_addr->sa_family == AF_INET)
		{
			char	   *addressBuffer;

			if (!strncasecmp("lo", ifa->ifa_name, 2))
				continue;		/* We do not need loop back addresses */

			tmpAddrPtr = &((struct sockaddr_in *) ifa->ifa_addr)->sin_addr;
			addressBuffer = palloc(INET_ADDRSTRLEN);
			inet_ntop(AF_INET, tmpAddrPtr, addressBuffer, INET_ADDRSTRLEN);
			local_addresses = lappend(local_addresses, addressBuffer);
		}
	}
	if (ifAddrStruct != NULL)
		freeifaddrs(ifAddrStruct);
	return local_addresses;
}

#define WD_TRY_PING_AT_IPUP 3
int
wd_IP_up(void)
{
	int			rtn = WD_OK;
	char		path[WD_MAX_PATH_LEN];
	char	   *command;
	int			i;

	if (strlen(pool_config->delegate_ip) == 0)
	{
		ereport(LOG,
				(errmsg("trying to acquire the delegate IP address, but delegate IP is not configured")));
		return WD_OK;
	}

	command = wd_get_cmd(pool_config->if_up_cmd);
	if (command)
	{

		/* If if_up_cmd starts with "/", the setting specified in "if_cmd_path" will be ignored */
		if (command[0] == '/')
			snprintf(path, sizeof(path), "%s", command);
		else
			snprintf(path, sizeof(path), "%s/%s", pool_config->if_cmd_path, command);

		rtn = exec_if_cmd(path, pool_config->if_up_cmd);
		pfree(command);
	}
	else
	{
		ereport(LOG,
				(errmsg("failed to acquire the delegate IP address"),
				 errdetail("unable to parse the if_up_cmd:\"%s\"", pool_config->if_up_cmd)));
		return WD_NG;
	}

	if (rtn == WD_OK)
	{
		command = wd_get_cmd(pool_config->arping_cmd);
		if (command)
		{
			/* If arping_cmd starts with "/", the setting specified in "arping_path" will be ignored */
			if (command[0] == '/')
				snprintf(path, sizeof(path), "%s", command);
			else
				snprintf(path, sizeof(path), "%s/%s", pool_config->arping_path, command);

			rtn = exec_if_cmd(path, pool_config->arping_cmd);
			pfree(command);
		}
		else
		{
			rtn = WD_NG;
			ereport(LOG,
					(errmsg("failed to acquire the delegate IP address"),
					 errdetail("unable to parse the arping_cmd:\"%s\"", pool_config->arping_cmd)));
		}
	}

	if (rtn == WD_OK)
	{
		for (i = 0; i < WD_TRY_PING_AT_IPUP; i++)
		{
			if (wd_is_ip_exists(pool_config->delegate_ip) == true)
				break;
			ereport(LOG,
					(errmsg("waiting for the delegate IP address to become active"),
					 errdetail("waiting... count: %d", i + 1)));
		}

		if (i >= WD_TRY_PING_AT_IPUP)
			rtn = WD_NG;
	}

	if (rtn == WD_OK)
		ereport(LOG,
				(errmsg("successfully acquired the delegate IP:\"%s\"", pool_config->delegate_ip),
				 errdetail("'if_up_cmd' returned with success")));
	else
		ereport(LOG,
				(errmsg("failed to acquire the delegate IP address"),
				 errdetail("'if_up_cmd' failed")));
	return rtn;
}

int
wd_IP_down(void)
{
	int			rtn = WD_OK;
	char		path[WD_MAX_PATH_LEN];
	char	   *command;

	if (strlen(pool_config->delegate_ip) == 0)
	{
		ereport(LOG,
				(errmsg("trying to release the delegate IP address, but delegate IP is not configured")));
		return WD_OK;
	}

	command = wd_get_cmd(pool_config->if_down_cmd);
	if (command)
	{
		/* If if_down_cmd starts with "/", the setting specified in "if_cmd_path" will be ignored */
		if (command[0] == '/')
			snprintf(path, sizeof(path), "%s", command);
		else
			snprintf(path, sizeof(path), "%s/%s", pool_config->if_cmd_path, command);

		rtn = exec_if_cmd(path, pool_config->if_down_cmd);
		pfree(command);
	}
	else
	{
		ereport(LOG,
				(errmsg("failed to release the delegate IP address"),
				 errdetail("unable to parse the if_down_cmd:\"%s\"", pool_config->if_down_cmd)));
		return WD_NG;
	}

	if (rtn == WD_OK)
	{
		ereport(LOG,
				(errmsg("successfully released the delegate IP:\"%s\"", pool_config->delegate_ip),
				 errdetail("'if_down_cmd' returned with success")));
	}
	else
	{
		ereport(LOG,
				(errmsg("failed to release the delegate IP:\"%s\"", pool_config->delegate_ip),
				 errdetail("'if_down_cmd' failed")));
	}
	return rtn;
}


char *
wd_get_cmd(char *cmd)
{
	char	   *command = NULL;

	if (cmd && *cmd)
	{
		char	   *tmp_str = pstrdup(cmd);
		char	   *token = strtok(tmp_str, " ");

		if (token)
			command = pstrdup(token);
		pfree(tmp_str);
	}
	return command;
}

static int
exec_if_cmd(char *path, char *command)
{
	int			pfd[2];
	int			status;
	char	   *args[24];
	int			pid,
				i = 0;
	char	   *buf;
	char	   *bp,
			   *ep;

	if (pipe(pfd) == -1)
	{
		ereport(WARNING,
				(errmsg("while executing interface up/down command, pipe open failed"),
				 errdetail("%m")));
		return WD_NG;
	}

	buf = string_replace(command, "$_IP_$", pool_config->delegate_ip);

	bp = buf;
	while (*bp == ' ')
	{
		bp++;
	}
	while (*bp != '\0')
	{
		ep = strchr(bp, ' ');
		if (ep != NULL)
		{
			*ep = '\0';
		}
		args[i++] = bp;
		if (ep != NULL)
		{
			bp = ep + 1;
			while (*bp == ' ')
			{
				bp++;
			}
		}
		else
		{
			break;
		}
	}
	args[i++] = NULL;

	pid = fork();
	if (pid == -1)
	{
		ereport(FATAL,
				(errmsg("failed to execute interface up/down command"),
				 errdetail("fork() failed with reason: \"%m\"")));
	}
	if (pid == 0)
	{
		on_exit_reset();
		SetProcessGlobalVariables(PT_WATCHDOG_UTILITY);
		close(STDOUT_FILENO);
		dup2(pfd[1], STDOUT_FILENO);
		close(pfd[0]);
		status = execv(path, args);
		exit(0);
	}
	else
	{
		pfree(buf);
		close(pfd[1]);
		for (;;)
		{
			int			result;

			result = waitpid(pid, &status, 0);
			if (result < 0)
			{
				if (errno == EINTR)
					continue;

				ereport(DEBUG1,
						(errmsg("watchdog exec waitpid()failed"),
						 errdetail("waitpid() system call failed with reason \"%m\"")));

				return WD_NG;
			}

			if (WIFEXITED(status) == 0 || WEXITSTATUS(status) != 0)
			{
				ereport(DEBUG1,
						(errmsg("watchdog exec interface up/down command failed"),
						 errdetail("'%s' failed. exit status: %d", command, WEXITSTATUS(status))));

				return WD_NG;
			}
			else
				break;
		}
		close(pfd[0]);
	}
	ereport(DEBUG1,
			(errmsg("watchdog exec interface up/down command: '%s' succeeded", command)));

	return WD_OK;
}


int
create_monitoring_socket(void)
{
	int			sock = -1;
#ifdef __linux__
	sock = socket(AF_NETLINK, SOCK_DGRAM, NETLINK_ROUTE);

#else
	sock = socket(PF_ROUTE, SOCK_RAW, AF_UNSPEC);
#endif
	if (sock < 0)
		ereport(ERROR,
				(errmsg("watchdog: VIP monitoring failed to create socket"),
				 errdetail("socket() failed with error \"%m\"")));

#ifdef __linux__
	struct sockaddr_nl addr;

	memset(&addr, 0x00, sizeof(addr));
	addr.nl_family = AF_NETLINK;
	addr.nl_groups = RTMGRP_IPV4_IFADDR | RTMGRP_LINK;

	if (bind(sock, (struct sockaddr *) &addr, sizeof(addr)) < 0)
	{
		close(sock);
		ereport(ERROR,
				(errmsg("watchdog: VIP monitoring failed to bind socket"),
				 errdetail("bind() failed with error \"%m\"")));
	}
#endif

	return sock;
}

#ifdef __linux__
bool
read_interface_change_event(int sock, bool *link_event, bool *deleted)
{
	char		buffer[4096];
	int			len;
	struct iovec iov;
	struct msghdr hdr;
	struct nlmsghdr *nlhdr;
	struct ifinfomsg *ifimsg;

	*deleted = false;
	*link_event = false;

	iov.iov_base = buffer;
	iov.iov_len = sizeof(buffer);

	memset(&hdr, 0, sizeof(hdr));
	hdr.msg_iov = &iov;
	hdr.msg_iovlen = 1;

	len = recvmsg(sock, &hdr, 0);
	if (len < 0)
	{
		ereport(DEBUG1,
				(errmsg("VIP monitoring failed to receive from socket"),
				 errdetail("recvmsg() failed with error \"%m\"")));
		return false;
	}

	nlhdr = (struct nlmsghdr *) buffer;

	for (; NLMSG_OK(nlhdr, len); nlhdr = NLMSG_NEXT(nlhdr, len))
	{
		if (nlhdr->nlmsg_type == NLMSG_DONE)
			break;

		ifimsg = NLMSG_DATA(nlhdr);

		switch (nlhdr->nlmsg_type)
		{
			case RTM_DELLINK:
				*deleted = true;	/* fallthrough */
			case RTM_NEWLINK:
				if (!(ifimsg->ifi_flags & IFF_LOWER_UP) || !(ifimsg->ifi_flags & IFF_RUNNING))
					*deleted = true;
				else
					*link_event = true;
				return true;
				break;

			case RTM_DELADDR:
				*deleted = true;	/* fallthrough */
			case RTM_NEWADDR:
				*link_event = false;
				return true;
				break;
			default:
				ereport(DEBUG2,
						(errmsg("unknown nlmsg_type=%d", nlhdr->nlmsg_type)));
		}
	}
	return false;
}

#else							/* For non linux chaps */
#if defined(__OpenBSD__) || defined(__FreeBSD__)
#define SALIGN (sizeof(long) - 1)
#else
#define SALIGN (sizeof(int32_t) - 1)
#endif

#define SA_RLEN(sa) ((sa)->sa_len ? (((sa)->sa_len + SALIGN) & ~SALIGN) : (SALIGN + 1))
/* With the help from https://github.com/miniupnp/miniupnp/blob/master/minissdpd/ifacewatch.c */

bool
read_interface_change_event(int sock, bool *link_event, bool *deleted)
{
	char		buffer[1024];
	int			len;
	struct rt_msghdr *nlhdr;

	*deleted = false;
	*link_event = false;

	len = recv(sock, buffer, sizeof(buffer), 0);
	if (len < 0)
	{
		ereport(DEBUG1,
				(errmsg("VIP monitoring failed to receive from socket"),
				 errdetail("recv() failed with error \"%m\"")));
		return false;
	}

	nlhdr = (struct rt_msghdr *) buffer;
	switch (nlhdr->rtm_type)
	{
		case RTM_DELETE:
			*deleted = true;	/* fallthrough */
		case RTM_ADD:
			*link_event = true;
			return true;
			break;

		case RTM_DELADDR:
			*deleted = true;	/* fallthrough */
		case RTM_NEWADDR:
			*link_event = false;
			return true;
			break;
		default:
			ereport(DEBUG2,
					(errmsg("unknown nlmsg_type=%d", nlhdr->rtm_type)));
	}
	return false;
}
#endif

bool
is_interface_up(struct ifaddrs *ifa)
{
	bool		result = false;

	if (ifa->ifa_flags & IFF_RUNNING)
	{
		ereport(DEBUG1,
				(errmsg("network interface \"%s\" link is active", ifa->ifa_name)));

		if (ifa->ifa_flags & IFF_LOWER_UP)
		{
			ereport(DEBUG1,
					(errmsg("network interface \"%s\" link is up", ifa->ifa_name)));
			result = true;
		}
		else
			ereport(NOTICE,
					(errmsg("network interface \"%s\" link is down", ifa->ifa_name)));
	}
	else
		ereport(NOTICE,
				(errmsg("network interface \"%s\" link is inactive", ifa->ifa_name)));

	return result;
}
