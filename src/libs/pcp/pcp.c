/*
 * $Header$
 *
 * Handles PCP connection, and protocol communication with pgpool-II
 * These are client APIs. Server program should use APIs in pcp_stream.c
 *
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
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <netdb.h>
#include <unistd.h>
#include <stdarg.h>

#include "pool.h"
#include "pcp/pcp.h"
#include "pcp/pcp_stream.h"
#include "utils/pool_path.h"
#include "utils/palloc.h"
#include "utils/pool_process_reporting.h"
#include "utils/json.h"
#include "auth/md5.h"

#define PCPPASSFILE ".pcppass"
#define DefaultHost  "localhost"


static int	pcp_authorize(PCPConnInfo * pcpConn, char *username, char *password);

static void pcp_internal_error(PCPConnInfo * pcpConn, const char *fmt,...);

static PCPResultInfo * _pcp_detach_node(PCPConnInfo * pcpConn, int nid, bool gracefully);
static PCPResultInfo * _pcp_promote_node(PCPConnInfo * pcpConn, int nid, bool gracefully, bool promote);
static PCPResultInfo * process_pcp_response(PCPConnInfo * pcpConn, char sentMsg);
static void setCommandSuccessful(PCPConnInfo * pcpConn);
static void setResultStatus(PCPConnInfo * pcpConn, ResultStateType resultState);
static void setResultBinaryData(PCPResultInfo * res, unsigned int slotno, void *value, int datalen, void (*free_func) (struct PCPConnInfo *, void *));
static int	setNextResultBinaryData(PCPResultInfo * res, void *value, int datalen, void (*free_func) (struct PCPConnInfo *, void *));
static void setResultIntData(PCPResultInfo * res, unsigned int slotno, int value);

static void process_node_info_response(PCPConnInfo * pcpConn, char *buf, int len);
static void	process_health_check_stats_response(PCPConnInfo * pcpConn, char *buf, int len);
static void process_command_complete_response(PCPConnInfo * pcpConn, char *buf, int len);
static void process_watchdog_info_response(PCPConnInfo * pcpConn, char *buf, int len);
static void process_process_info_response(PCPConnInfo * pcpConn, char *buf, int len);
static void process_pool_status_response(PCPConnInfo * pcpConn, char *buf, int len);
static void process_pcp_node_count_response(PCPConnInfo * pcpConn, char *buf, int len);
static void process_process_count_response(PCPConnInfo * pcpConn, char *buf, int len);
static void process_salt_info_response(PCPConnInfo * pcpConn, char *buf, int len);
static void process_error_response(PCPConnInfo * pcpConn, char toc, char *buff);


static void setResultSlotCount(PCPConnInfo * pcpConn, unsigned int slotCount);
static int	PCPFlush(PCPConnInfo * pcpConn);

static bool getPoolPassFilename(char *pgpassfile);
static char *PasswordFromFile(PCPConnInfo * pcpConn, char *hostname, char *port, char *username);
static char *pwdfMatchesString(char *buf, char *token);

/* --------------------------------
 * pcp_connect - open connection to pgpool using given arguments
 *
 * return 0 on success, -1 otherwise
 * --------------------------------
 */

/* Check if PCP connection is connected and authenticated
 * return 1 on successful 0 otherwise
 */

PCPConnInfo *
pcp_connect(char *hostname, int port, char *username, char *password, FILE *Pfdebug)
{
	struct sockaddr_in addr;
	struct sockaddr_un unix_addr;
	struct hostent *hp;
	char	   *password_from_file = NULL;
	char		os_user[256];
	PCPConnInfo *pcpConn = palloc0(sizeof(PCPConnInfo));
	int			fd;
	int			on = 1;
	int			len;

	pcpConn->connState = PCP_CONNECTION_NOT_CONNECTED;
	pcpConn->Pfdebug = Pfdebug;

	if (hostname == NULL || *hostname == '\0' || *hostname == '/')
	{
		char	   *path;

		fd = socket(AF_UNIX, SOCK_STREAM, 0);

		if (fd < 0)
		{
			pcp_internal_error(pcpConn,
							   "ERROR: failed to create UNIX domain socket. socket error \"%s\"", strerror(errno));
			pcpConn->connState = PCP_CONNECTION_BAD;

			return pcpConn;
		}

		memset(&unix_addr, 0, sizeof(unix_addr));
		unix_addr.sun_family = AF_UNIX;

		if (hostname == NULL || *hostname == '\0')
		{
			path = UNIX_DOMAIN_PATH;
			hostname = path;
		}
		else
		{
			path = hostname;
		}

		snprintf(unix_addr.sun_path, sizeof(unix_addr.sun_path), "%s/.s.PGSQL.%d",
				 path, port);

		if (connect(fd, (struct sockaddr *) &unix_addr, sizeof(unix_addr)) < 0)
		{
			close(fd);

			pcp_internal_error(pcpConn,
							   "ERROR: connection to socket \"%s\" failed with error \"%s\"", unix_addr.sun_path, strerror(errno));
			pcpConn->connState = PCP_CONNECTION_BAD;
			return pcpConn;
		}
	}
	else
	{
		fd = socket(AF_INET, SOCK_STREAM, 0);
		if (fd < 0)
		{
			pcp_internal_error(pcpConn,
							   "ERROR: failed to create INET domain socket with error \"%s\"", strerror(errno));
			pcpConn->connState = PCP_CONNECTION_BAD;
			return pcpConn;
		}

		if (setsockopt(fd, IPPROTO_TCP, TCP_NODELAY,
					   (char *) &on, sizeof(on)) < 0)
		{
			close(fd);

			pcp_internal_error(pcpConn,
							   "ERROR: set socket option failed with error \"%s\"", strerror(errno));
			pcpConn->connState = PCP_CONNECTION_BAD;
			return pcpConn;
		}

		memset((char *) &addr, 0, sizeof(addr));
		addr.sin_family = AF_INET;
		hp = gethostbyname(hostname);
		if ((hp == NULL) || (hp->h_addrtype != AF_INET))
		{
			close(fd);
			pcp_internal_error(pcpConn,
							   "ERROR: could not retrieve hostname. gethostbyname failed with error \"%s\"", strerror(errno));
			pcpConn->connState = PCP_CONNECTION_BAD;
			return pcpConn;

		}
		memmove((char *) &(addr.sin_addr),
				(char *) hp->h_addr,
				hp->h_length);
		addr.sin_port = htons(port);

		len = sizeof(struct sockaddr_in);
		if (connect(fd, (struct sockaddr *) &addr, len) < 0)
		{
			close(fd);
			pcp_internal_error(pcpConn,
							   "ERROR: connection to host \"%s\" failed with error \"%s\"", hostname, strerror(errno));
			pcpConn->connState = PCP_CONNECTION_BAD;
			return pcpConn;
		}
	}

	pcpConn->pcpConn = pcp_open(fd);
	if (pcpConn->pcpConn == NULL)
	{
		close(fd);
		pcp_internal_error(pcpConn,
						   "ERROR: failed to allocate memory");
		pcpConn->connState = PCP_CONNECTION_BAD;
		return pcpConn;
	}
	pcpConn->connState = PCP_CONNECTION_CONNECTED;

	/*
	 * If username is not provided. Use the os user name and do not complain
	 * if it (getting os user name) gets failed
	 */
	if (username == NULL && get_os_username(os_user, sizeof(os_user)))
		username = os_user;

	/*
	 * If password is not provided. lookup in pcppass file
	 */
	if (password == NULL || *password == '\0')
	{
		char		port_str[100];

		snprintf(port_str, sizeof(port_str), "%d", port);
		password_from_file = PasswordFromFile(pcpConn, hostname, port_str, username);
		password = password_from_file;
	}

	if (pcp_authorize(pcpConn, username, password) < 0)
	{
		pcp_close(pcpConn->pcpConn);
		pcpConn->pcpConn = NULL;
		pcpConn->connState = PCP_CONNECTION_AUTH_ERROR;
	}
	else
		pcpConn->connState = PCP_CONNECTION_OK;

	if (password_from_file)
		pfree(password_from_file);

	return pcpConn;
}

static void
process_salt_info_response(PCPConnInfo * pcpConn, char *buf, int len)
{
	char	   *salt = palloc((sizeof(char) * 4));

	memcpy(salt, buf, 4);
	if (setNextResultBinaryData(pcpConn->pcpResInfo, (void *) salt, 4, NULL) < 0)
	{
		pcp_internal_error(pcpConn,

						   "command failed. invalid response");
		setResultStatus(pcpConn, PCP_RES_BAD_RESPONSE);
	}
	else
	{
		setCommandSuccessful(pcpConn);
	}
}

/* --------------------------------
 * pcp_authorize - authenticate with pgpool using username and password
 *
 * return 0 on success, -1 otherwise
 * --------------------------------
 */
static int
pcp_authorize(PCPConnInfo * pcpConn, char *username, char *password)
{
	int			wsize;
	char		salt[4];
	char	   *salt_ptr;
	char		encrypt_buf[(MD5_PASSWD_LEN + 1) * 2];
	char		md5[MD5_PASSWD_LEN + 1];
	PCPResultInfo *pcpRes;

	if (password == NULL)
		password = "";

	if (username == NULL)
		username = "";

	if (PCPConnectionStatus(pcpConn) != PCP_CONNECTION_CONNECTED)
	{
		pcp_internal_error(pcpConn,
						   "ERROR: PCP authorization failed. invalid connection state.");
		return -1;
	}

	if (strlen(username) >= MAX_USER_PASSWD_LEN)
	{
		pcp_internal_error(pcpConn,
						   "ERROR: PCP authorization failed. username too long.");
		return -1;
	}

	/* request salt */
	pcp_write(pcpConn->pcpConn, "M", 1);
	wsize = htonl(sizeof(int));
	pcp_write(pcpConn->pcpConn, &wsize, sizeof(int));
	if (PCPFlush(pcpConn) < 0)
		return -1;

	pcpRes = process_pcp_response(pcpConn, 'M');
	if (PCPResultStatus(pcpRes) != PCP_RES_COMMAND_OK)
		return -1;

	salt_ptr = pcp_get_binary_data(pcpRes, 0);
	if (salt_ptr == NULL)
		return -1;
	memcpy(salt, salt_ptr, 4);

	/* encrypt password */
	pool_md5_hash(password, strlen(password), md5);
	md5[MD5_PASSWD_LEN] = '\0';

	pool_md5_encrypt(md5, username, strlen(username),
					 encrypt_buf + MD5_PASSWD_LEN + 1);
	encrypt_buf[(MD5_PASSWD_LEN + 1) * 2 - 1] = '\0';

	pool_md5_encrypt(encrypt_buf + MD5_PASSWD_LEN + 1, salt, 4,
					 encrypt_buf);
	encrypt_buf[MD5_PASSWD_LEN] = '\0';

	pcp_write(pcpConn->pcpConn, "R", 1);
	wsize = htonl((strlen(username) + 1 + strlen(encrypt_buf) + 1) + sizeof(int));
	pcp_write(pcpConn->pcpConn, &wsize, sizeof(int));
	pcp_write(pcpConn->pcpConn, username, strlen(username) + 1);
	pcp_write(pcpConn->pcpConn, encrypt_buf, strlen(encrypt_buf) + 1);
	if (PCPFlush(pcpConn) < 0)
		return -1;
	pcpRes = process_pcp_response(pcpConn, 'R');
	if (PCPResultStatus(pcpRes) != PCP_RES_COMMAND_OK)
		return -1;
	pcp_free_result(pcpConn);
	return 0;
}

static PCPResultInfo * process_pcp_response(PCPConnInfo * pcpConn, char sentMsg)
{
	char		toc;
	int			rsize;
	char	   *buf;

	/* create empty result */
	if (pcpConn->pcpResInfo == NULL)
	{
		pcpConn->pcpResInfo = palloc0(sizeof(PCPResultInfo));
		pcpConn->pcpResInfo->resultSlots = 1;
	}

	while (1)
	{
		if (pcp_read(pcpConn->pcpConn, &toc, 1))
		{
			pcp_internal_error(pcpConn,
							   "ERROR: unable to read data from socket.");
			setResultStatus(pcpConn, PCP_RES_ERROR);
			return pcpConn->pcpResInfo;
		}

		if (pcp_read(pcpConn->pcpConn, &rsize, sizeof(int)))
		{
			pcp_internal_error(pcpConn,
							   "ERROR: unable to read data from socket.");
			setResultStatus(pcpConn, PCP_RES_ERROR);
			return pcpConn->pcpResInfo;
		}
		rsize = ntohl(rsize);
		buf = (char *) palloc(rsize);

		if (pcp_read(pcpConn->pcpConn, buf, rsize - sizeof(int)))
		{
			pfree(buf);
			pcp_internal_error(pcpConn,
							   "ERROR: unable to read data from socket.");
			setResultStatus(pcpConn, PCP_RES_ERROR);
			return pcpConn->pcpResInfo;
		}

		if (pcpConn->Pfdebug)
			fprintf(pcpConn->Pfdebug, "DEBUG: recv: tos=\"%c\", len=%d\n", toc, rsize);

		switch (toc)
		{
			case 'r':			/* Authentication Response */
				{
					if (sentMsg != 'R')
					{
						setResultStatus(pcpConn, PCP_RES_BAD_RESPONSE);
					}
					else if (strcmp(buf, "AuthenticationOK") == 0)
					{
						pcpConn->connState = PCP_CONNECTION_OK;
						setResultStatus(pcpConn, PCP_RES_COMMAND_OK);
					}
					else
					{
						pcp_internal_error(pcpConn,
										   "ERROR: authentication failed. reason=\"%s\"", buf);
						setResultStatus(pcpConn, PCP_RES_BACKEND_ERROR);
					}
				}
				break;
			case 'm':
				if (sentMsg != 'M')
					setResultStatus(pcpConn, PCP_RES_BAD_RESPONSE);
				else
					process_salt_info_response(pcpConn, buf, rsize);
				break;

			case 'E':
				setResultStatus(pcpConn, PCP_RES_BACKEND_ERROR);
				process_error_response(pcpConn, toc, buf);
				break;

			case 'N':
				process_error_response(pcpConn, toc, buf);
				pfree(buf);
				continue;
				break;

			case 'i':
				if (sentMsg != 'I')
					setResultStatus(pcpConn, PCP_RES_BAD_RESPONSE);
				else
					process_node_info_response(pcpConn, buf, rsize);
				break;

			case 'h':
				if (sentMsg != 'H')
					setResultStatus(pcpConn, PCP_RES_BAD_RESPONSE);
				else
					process_health_check_stats_response(pcpConn, buf, rsize);
				break;

			case 'l':
				if (sentMsg != 'L')
					setResultStatus(pcpConn, PCP_RES_BAD_RESPONSE);
				else
					process_pcp_node_count_response(pcpConn, buf, rsize);
				break;

			case 'c':
				if (sentMsg != 'C' && sentMsg != 'O')
					setResultStatus(pcpConn, PCP_RES_BAD_RESPONSE);
				else
					process_command_complete_response(pcpConn, buf, rsize);
				break;

			case 'd':
				if (sentMsg != 'D' && sentMsg != 'J')
					setResultStatus(pcpConn, PCP_RES_BAD_RESPONSE);
				else
					process_command_complete_response(pcpConn, buf, rsize);
				break;

			case 'a':
				if (sentMsg != 'A')
					setResultStatus(pcpConn, PCP_RES_BAD_RESPONSE);
				else
					process_command_complete_response(pcpConn, buf, rsize);
				break;

			case 'z':
				if (sentMsg != 'Z')
					setResultStatus(pcpConn, PCP_RES_BAD_RESPONSE);
				else
					process_command_complete_response(pcpConn, buf, rsize);
				break;

			case 'w':
				if (sentMsg != 'W')
					setResultStatus(pcpConn, PCP_RES_BAD_RESPONSE);
				else
					process_watchdog_info_response(pcpConn, buf, rsize);
				break;

			case 'p':
				if (sentMsg != 'P')
					setResultStatus(pcpConn, PCP_RES_BAD_RESPONSE);
				else
					process_process_info_response(pcpConn, buf, rsize);
				break;

			case 'n':
				if (sentMsg != 'N')
					setResultStatus(pcpConn, PCP_RES_BAD_RESPONSE);
				else
					process_process_count_response(pcpConn, buf, rsize);
				break;

			case 'b':
				if (sentMsg != 'B')
					setResultStatus(pcpConn, PCP_RES_BAD_RESPONSE);
				else
					process_pool_status_response(pcpConn, buf, rsize);
				break;

			case 't':
				if (sentMsg != 'T')
					setResultStatus(pcpConn, PCP_RES_BAD_RESPONSE);
				else
					setResultStatus(pcpConn, PCP_RES_COMMAND_OK);
				break;

			default:
				setResultStatus(pcpConn, PCP_RES_BAD_RESPONSE);
				pcp_internal_error(pcpConn,
								   "ERROR: invalid PCP packet type =\"%c\"", toc);
				break;
		}
		pfree(buf);
		if (PCPResultStatus(pcpConn->pcpResInfo) != PCP_RES_INCOMPLETE)
			break;
	}
	return pcpConn->pcpResInfo;
}

static void
process_error_response(PCPConnInfo * pcpConn, char toc, char *buf)
{
	/* For time we only support sev, error message and details */
	char	   *errorSev = NULL;
	char	   *errorMsg = NULL;
	char	   *errorDet = NULL;
	char	   *e = buf;

	if (toc != 'E' && toc != 'N')
		return;

	while (*e)
	{
		char		type = *e;

		e++;
		if (*e == 0)
			break;

		if (type == 'M')
			errorMsg = e;
		else if (type == 'S')
			errorSev = e;
		else if (type == 'D')
			errorDet = e;
		else
			e += strlen(e) + 1;
		if (errorDet && errorSev && errorMsg)	/* we have all what we need */
			break;
	}
	if (!errorSev && !errorMsg)
		return;

	if (toc != 'E')				/* This is not an error report it as debug */
	{
		if (pcpConn->Pfdebug)
			fprintf(pcpConn->Pfdebug,
					"BACKEND %s:  %s\n%s%s%s", errorSev, errorMsg,
					errorDet ? "DETAIL:  " : "",
					errorDet ? errorDet : "",
					errorDet ? "\n" : "");
	}
	else
	{
		pcp_internal_error(pcpConn,
						   "%s:  %s\n%s%s%s", errorSev, errorMsg,
						   errorDet ? "DETAIL:  " : "",
						   errorDet ? errorDet : "",
						   errorDet ? "\n" : "");
		setResultStatus(pcpConn, PCP_RES_BACKEND_ERROR);

	}
}

/* --------------------------------
 * pcp_disconnect - close connection to pgpool
 * --------------------------------
 */
void
pcp_disconnect(PCPConnInfo * pcpConn)
{
	int			wsize;

	if (PCPConnectionStatus(pcpConn) != PCP_CONNECTION_OK)
	{
		pcp_internal_error(pcpConn, "invalid PCP connection");
		return;
	}

	pcp_write(pcpConn->pcpConn, "X", 1);
	wsize = htonl(sizeof(int));
	pcp_write(pcpConn->pcpConn, &wsize, sizeof(int));
	if (PCPFlush(pcpConn) < 0)
		return;
	if (pcpConn->Pfdebug)
		fprintf(pcpConn->Pfdebug, "DEBUG: send: tos=\"X\", len=%d\n", (int) sizeof(int));

	pcp_close(pcpConn->pcpConn);
	pcpConn->connState = PCP_CONNECTION_NOT_CONNECTED;
	pcpConn->pcpConn = NULL;
}

/* --------------------------------
 * pcp_terminate_pgpool - send terminate packet
 *
 * return 0 on success, -1 otherwise
 * --------------------------------
 */
PCPResultInfo *
pcp_terminate_pgpool(PCPConnInfo * pcpConn, char mode, char command_scope)
{
	int			wsize;

	if (PCPConnectionStatus(pcpConn) != PCP_CONNECTION_OK)
	{
		pcp_internal_error(pcpConn, "invalid PCP connection");
		return NULL;
	}
	if (command_scope == 'l')	/*local only*/
		pcp_write(pcpConn->pcpConn, "T", 1);
	else
		pcp_write(pcpConn->pcpConn, "t", 1);
	wsize = htonl(sizeof(int) + sizeof(char));
	pcp_write(pcpConn->pcpConn, &wsize, sizeof(int));
	pcp_write(pcpConn->pcpConn, &mode, sizeof(char));
	if (PCPFlush(pcpConn) < 0)
		return NULL;
	if (pcpConn->Pfdebug)
		fprintf(pcpConn->Pfdebug, "DEBUG: send: tos=\"T\", len=%d\n", ntohl(wsize));

	return process_pcp_response(pcpConn, 'T');
}

static void
process_pcp_node_count_response(PCPConnInfo * pcpConn, char *buf, int len)
{
	if (strcmp(buf, "CommandComplete") == 0)
	{
		char	   *index = NULL;

		index = (char *) memchr(buf, '\0', len);
		if (index != NULL)
		{
			int			ret;

			index += 1;
			ret = atoi(index);
			setResultIntData(pcpConn->pcpResInfo, 0, ret);
			setCommandSuccessful(pcpConn);
			return;
		}
		else
			pcp_internal_error(pcpConn,
							   "command failed. invalid response");
	}
	else
		pcp_internal_error(pcpConn,
						   "command failed with reason: \"%s\"", buf);
	setResultStatus(pcpConn, PCP_RES_BAD_RESPONSE);
}

/* --------------------------------
 * pcp_node_count - get number of nodes currently connected to pgpool
 *
 * return array of node IDs on success, -1 otherwise
 * --------------------------------
 */
PCPResultInfo *
pcp_node_count(PCPConnInfo * pcpConn)
{
	int			wsize;

	if (PCPConnectionStatus(pcpConn) != PCP_CONNECTION_OK)
	{
		pcp_internal_error(pcpConn,
						   "invalid PCP connection");
		return NULL;
	}
	pcp_write(pcpConn->pcpConn, "L", 1);
	wsize = htonl(sizeof(int));
	pcp_write(pcpConn->pcpConn, &wsize, sizeof(int));
	if (PCPFlush(pcpConn) < 0)
		return NULL;
	if (pcpConn->Pfdebug)
		fprintf(pcpConn->Pfdebug, "DEBUG: send: tos=\"L\", len=%d\n", ntohl(wsize));

	return process_pcp_response(pcpConn, 'L');
}

static void
process_node_info_response(PCPConnInfo * pcpConn, char *buf, int len)
{
	char       *index;
	BackendInfo *backend_info = NULL;

	if (strcmp(buf, "ArraySize") == 0)
	{
		int			ci_size;

		index = (char *) memchr(buf, '\0', len);
		if (index == NULL)
			goto INVALID_RESPONSE;
		index += 1;
		ci_size = atoi(index);

		setResultStatus(pcpConn, PCP_RES_INCOMPLETE);
		setResultSlotCount(pcpConn, ci_size);
		pcpConn->pcpResInfo->nextFillSlot = 0;
		return;
	}
	else if (strcmp(buf, "NodeInfo") == 0)
	{
		char	   *index = NULL;

		if (PCPResultStatus(pcpConn->pcpResInfo) != PCP_RES_INCOMPLETE)
			goto INVALID_RESPONSE;

		backend_info = (BackendInfo *) palloc(sizeof(BackendInfo));

		index = (char *) memchr(buf, '\0', len);
		if (index == NULL)
			goto INVALID_RESPONSE;
		index += 1;
		strlcpy(backend_info->backend_hostname, index, sizeof(backend_info->backend_hostname));

		index = (char *) memchr(index, '\0', len);
		if (index == NULL)
			goto INVALID_RESPONSE;
		index += 1;
		backend_info->backend_port = atoi(index);

		index = (char *) memchr(index, '\0', len);
		if (index == NULL)
			goto INVALID_RESPONSE;
		index += 1;
		backend_info->backend_status = atoi(index);

		index = (char *) memchr(index, '\0', len);
		if (index == NULL)
			goto INVALID_RESPONSE;
		index += 1;
		backend_info->quarantine = atoi(index);

		index = (char *) memchr(index, '\0', len);
		if (index == NULL)
			goto INVALID_RESPONSE;
		index += 1;
		strlcpy(backend_info->pg_backend_status, index, sizeof(backend_info->pg_backend_status));

		index = (char *) memchr(index, '\0', len);
		if (index == NULL)
			goto INVALID_RESPONSE;
		index += 1;
		backend_info->backend_weight = atof(index);

		index = (char *) memchr(index, '\0', len);
		if (index == NULL)
			goto INVALID_RESPONSE;
		index++;
		backend_info->role = atoi(index);

		index = (char *) memchr(index, '\0', len);
		if (index == NULL)
			goto INVALID_RESPONSE;
		index++;

		strlcpy(backend_info->pg_role, index, sizeof(backend_info->pg_role));

		index = (char *) memchr(index, '\0', len);
		if (index == NULL)
			goto INVALID_RESPONSE;

		index++;
		backend_info->standby_delay_by_time = atol(index);

		index = (char *) memchr(index, '\0', len);
		if (index == NULL)
			goto INVALID_RESPONSE;

		index++;
		backend_info->standby_delay = atol(index);

		index = (char *) memchr(index, '\0', len);
		if (index == NULL)
			goto INVALID_RESPONSE;

		index++;
		strlcpy(backend_info->replication_state, index, sizeof(backend_info->replication_state));

		index = (char *) memchr(index, '\0', len);
		if (index == NULL)
			goto INVALID_RESPONSE;

		index++;
		strlcpy(backend_info->replication_sync_state, index, sizeof(backend_info->replication_sync_state));

		index = (char *) memchr(index, '\0', len);
		if (index == NULL)
			goto INVALID_RESPONSE;

		index++;
		backend_info->status_changed_time = atol(index);

		index = (char *) memchr(index, '\0', len);
		if (index == NULL)
			goto INVALID_RESPONSE;

		if (setNextResultBinaryData(pcpConn->pcpResInfo, (void *) backend_info, sizeof(BackendInfo), NULL) < 0)
			goto INVALID_RESPONSE;

		return;
	}
	else if (strcmp(buf, "CommandComplete") == 0)
	{
		setResultStatus(pcpConn, PCP_RES_COMMAND_OK);
		return;
	}

INVALID_RESPONSE:

	if (backend_info)
		pfree(backend_info);
	pcp_internal_error(pcpConn,
					   "command failed. invalid response");
	setResultStatus(pcpConn, PCP_RES_BAD_RESPONSE);

}

/* --------------------------------
 * pcp_node_info - get information of node pointed by given argument
 *
 * return structure of node information on success, -1 otherwise
 * --------------------------------
 */
PCPResultInfo *
pcp_node_info(PCPConnInfo * pcpConn, int nid)
{
	int			wsize;
	char		node_id[16];

	if (PCPConnectionStatus(pcpConn) != PCP_CONNECTION_OK)
	{
		pcp_internal_error(pcpConn,
						   "invalid PCP connection");
		return NULL;
	}

	snprintf(node_id, sizeof(node_id), "%d", nid);

	pcp_write(pcpConn->pcpConn, "I", 1);
	wsize = htonl(strlen(node_id) + 1 + sizeof(int));
	pcp_write(pcpConn->pcpConn, &wsize, sizeof(int));
	pcp_write(pcpConn->pcpConn, node_id, strlen(node_id) + 1);
	if (PCPFlush(pcpConn) < 0)
		return NULL;
	if (pcpConn->Pfdebug)
		fprintf(pcpConn->Pfdebug, "DEBUG: send: tos=\"I\", len=%d\n", ntohl(wsize));

	return process_pcp_response(pcpConn, 'I');
}


/* --------------------------------
 * pcp_health_check_stats - get information of health check stats pointed by given argument
 *
 * return structure of node information on success, -1 otherwise
 * --------------------------------
 */
PCPResultInfo *
pcp_health_check_stats(PCPConnInfo * pcpConn, int nid)
{
	int			wsize;
	char		node_id[16];

	if (PCPConnectionStatus(pcpConn) != PCP_CONNECTION_OK)
	{
		pcp_internal_error(pcpConn,
						   "invalid PCP connection");
		return NULL;
	}

	snprintf(node_id, sizeof(node_id), "%d", nid);

	pcp_write(pcpConn->pcpConn, "H", 1);
	wsize = htonl(strlen(node_id) + 1 + sizeof(int));
	pcp_write(pcpConn->pcpConn, &wsize, sizeof(int));
	pcp_write(pcpConn->pcpConn, node_id, strlen(node_id) + 1);
	if (PCPFlush(pcpConn) < 0)
		return NULL;
	if (pcpConn->Pfdebug)
		fprintf(pcpConn->Pfdebug, "DEBUG: send: tos=\"L\", len=%d\n", ntohl(wsize));

	return process_pcp_response(pcpConn, 'H');
}

PCPResultInfo *
pcp_reload_config(PCPConnInfo * pcpConn,char command_scope)
{
	int                     wsize;
/*
 * pcp packet format for pcp_reload_config
 * z[size][command_scope]
 */
	if (PCPConnectionStatus(pcpConn) != PCP_CONNECTION_OK)
	{
	   pcp_internal_error(pcpConn, "invalid PCP connection");
	   return NULL;
	}

	pcp_write(pcpConn->pcpConn, "Z", 1);
	wsize = htonl(sizeof(int) + sizeof(char));
	pcp_write(pcpConn->pcpConn, &wsize, sizeof(int));
	pcp_write(pcpConn->pcpConn, &command_scope, sizeof(char));
	if (PCPFlush(pcpConn) < 0)
	   return NULL;
	if (pcpConn->Pfdebug)
	   fprintf(pcpConn->Pfdebug, "DEBUG: send: tos=\"Z\", len=%d\n", ntohl(wsize));

	return process_pcp_response(pcpConn, 'Z');
}


/*
 * Process health check response from PCP server.
 * pcpConn: connection to the server
 * buf:		returned data from server
 * len:		length of the data
 */
static void
process_health_check_stats_response
(PCPConnInfo * pcpConn, char *buf, int len)
{
	POOL_HEALTH_CHECK_STATS *stats;
	int		*offsets;
	int		n;
	int		i;
	char	*p;
	int		maxstr;
	char	c[] = "CommandComplete";

	if (strcmp(buf, c) != 0)
	{
		pcp_internal_error(pcpConn,
						   "command failed. invalid response");
		setResultStatus(pcpConn, PCP_RES_BAD_RESPONSE);
		return;
	}
	buf += sizeof(c);

	/* Allocate health stats memory */
	stats = palloc0(sizeof(POOL_HEALTH_CHECK_STATS));
	p = (char *)stats;

	/* Calculate total packet length */
	offsets = pool_health_check_stats_offsets(&n);

	for (i = 0; i < n; i++)
	{
		if (i == n -1)
			maxstr = sizeof(POOL_HEALTH_CHECK_STATS) - offsets[i];
		else
			maxstr = offsets[i + 1] - offsets[i];

		StrNCpy(p + offsets[i], buf, maxstr -1);
		buf += strlen(buf) + 1;
	}

	if (setNextResultBinaryData(pcpConn->pcpResInfo, (void *) stats, sizeof(POOL_HEALTH_CHECK_STATS), NULL) < 0)
	{
		if (stats)
			pfree(stats);
		pcp_internal_error(pcpConn,
						   "command failed. invalid response");
		setResultStatus(pcpConn, PCP_RES_BAD_RESPONSE);
	}
	else
		setCommandSuccessful(pcpConn);

}

static void
process_process_count_response(PCPConnInfo * pcpConn, char *buf, int len)
{
	if (strcmp(buf, "CommandComplete") == 0)
	{
		int			process_count;
		int		   *process_list = NULL;
		char	   *index = NULL;
		int			i;

		index = (char *) memchr(buf, '\0', len);
		if (index == NULL)
		{
			pcp_internal_error(pcpConn,
							   "command failed. invalid response");
			setResultStatus(pcpConn, PCP_RES_BAD_RESPONSE);
			return;
		}
		index += 1;
		process_count = atoi(index);

		process_list = (int *) palloc(sizeof(int) * process_count);

		for (i = 0; i < process_count; i++)
		{
			index = (char *) memchr(index, '\0', len);
			if (index == NULL)
			{
				pcp_internal_error(pcpConn,
								   "command failed. invalid response");
				setResultStatus(pcpConn, PCP_RES_BAD_RESPONSE);
				pfree(process_list);
				return;
			}
			index += 1;
			process_list[i] = atoi(index);
		}
		setResultSlotCount(pcpConn, 1);
		if (setNextResultBinaryData(pcpConn->pcpResInfo, process_list, (sizeof(int) * process_count), NULL) < 0)
		{
			pcp_internal_error(pcpConn,
							   "command failed. invalid response");
			setResultStatus(pcpConn, PCP_RES_BAD_RESPONSE);
		}
		else
		{
			setCommandSuccessful(pcpConn);
		}
	}
	else
	{
		pcp_internal_error(pcpConn,
						   "command failed with reason: \"%s\"", buf);
		setResultStatus(pcpConn, PCP_RES_BAD_RESPONSE);
	}
}

/* --------------------------------
 * pcp_node_count - get number of nodes currently connected to pgpool
 *
 * return array of pids on success, NULL otherwise
 * --------------------------------
 */

PCPResultInfo *
pcp_process_count(PCPConnInfo * pcpConn)
{
	int			wsize;

	if (PCPConnectionStatus(pcpConn) != PCP_CONNECTION_OK)
	{
		pcp_internal_error(pcpConn, "invalid PCP connection");
		return NULL;
	}

	pcp_write(pcpConn->pcpConn, "N", 1);
	wsize = htonl(sizeof(int));
	pcp_write(pcpConn->pcpConn, &wsize, sizeof(int));
	if (PCPFlush(pcpConn) < 0)
		return NULL;
	if (pcpConn->Pfdebug)
		fprintf(pcpConn->Pfdebug, "DEBUG: send: tos=\"N\", len=%d\n", ntohl(wsize));

	return process_pcp_response(pcpConn, 'N');
}

static void
process_process_info_response(PCPConnInfo * pcpConn, char *buf, int len)
{
	char	   *index;
	int			*offsets;
	int			i, n;
	int			maxstr;
	char		*p;
	POOL_REPORT_POOLS	*pools = NULL;

	offsets = pool_report_pools_offsets(&n);

	if (strcmp(buf, "ArraySize") == 0)
	{
		int			ci_size;

		index = (char *) memchr(buf, '\0', len);
		if (index == NULL)
			goto INVALID_RESPONSE;
		index += 1;
		ci_size = atoi(index);

		setResultStatus(pcpConn, PCP_RES_INCOMPLETE);
		setResultSlotCount(pcpConn, ci_size);
		pcpConn->pcpResInfo->nextFillSlot = 0;
		return;
	}
	else if (strcmp(buf, "ProcessInfo") == 0)
	{
		if (PCPResultStatus(pcpConn->pcpResInfo) != PCP_RES_INCOMPLETE)
			goto INVALID_RESPONSE;

		pools = palloc0(sizeof(POOL_REPORT_POOLS));
		p = (char *)pools;
		buf += strlen(buf) + 1;

		for (i = 0; i < n; i++)
		{
			if (i == n -1)
				maxstr = sizeof(POOL_REPORT_POOLS) - offsets[i];
			else
				maxstr = offsets[i + 1] - offsets[i];

			StrNCpy(p + offsets[i], buf, maxstr -1);
			buf += strlen(buf) + 1;
		}

		if (setNextResultBinaryData(pcpConn->pcpResInfo, (void *) pools, sizeof(POOL_REPORT_POOLS), NULL) < 0)
			goto INVALID_RESPONSE;

		return;
	}

	else if (strcmp(buf, "CommandComplete") == 0)
	{
		setResultStatus(pcpConn, PCP_RES_COMMAND_OK);
		return;
	}

INVALID_RESPONSE:

	if (pools)
	{
		pfree(pools);
	}
	pcp_internal_error(pcpConn,
					   "command failed. invalid response");
	setResultStatus(pcpConn, PCP_RES_BAD_RESPONSE);
}

/* --------------------------------
 * pcp_process_info - get information of node pointed by given argument
 *
 * return structure of process information on success, -1 otherwise
 * --------------------------------
 */
PCPResultInfo *
pcp_process_info(PCPConnInfo * pcpConn, int pid)
{
	int			wsize;
	char		process_id[16];

	if (PCPConnectionStatus(pcpConn) != PCP_CONNECTION_OK)
	{
		pcp_internal_error(pcpConn, "invalid PCP connection");
		return NULL;
	}

	snprintf(process_id, sizeof(process_id), "%d", pid);

	pcp_write(pcpConn->pcpConn, "P", 1);
	wsize = htonl(strlen(process_id) + 1 + sizeof(int));
	pcp_write(pcpConn->pcpConn, &wsize, sizeof(int));
	pcp_write(pcpConn->pcpConn, process_id, strlen(process_id) + 1);
	if (PCPFlush(pcpConn) < 0)
		return NULL;
	if (pcpConn->Pfdebug)
		fprintf(pcpConn->Pfdebug, "DEBUG: send: tos=\"P\", len=%d\n", ntohl(wsize));

	return process_pcp_response(pcpConn, 'P');
}

/* --------------------------------
 * pcp_detach_node - detach a node given by the argument from pgpool's control
 *
 * return 0 on success, -1 otherwise
 * --------------------------------
 */
PCPResultInfo *
pcp_detach_node(PCPConnInfo * pcpConn, int nid)
{
	return _pcp_detach_node(pcpConn, nid, FALSE);
}

/* --------------------------------

 * and detach a node given by the argument from pgpool's control
 *
 * return 0 on success, -1 otherwise
 * --------------------------------
 */
PCPResultInfo *
pcp_detach_node_gracefully(PCPConnInfo * pcpConn, int nid)
{
	return _pcp_detach_node(pcpConn, nid, TRUE);
}

static PCPResultInfo *
_pcp_detach_node(PCPConnInfo * pcpConn, int nid, bool gracefully)
{
	int			wsize;
	char		node_id[16];
	char	   *sendchar;

	if (PCPConnectionStatus(pcpConn) != PCP_CONNECTION_OK)
	{
		pcp_internal_error(pcpConn, "invalid PCP connection");
		return NULL;
	}

	snprintf(node_id, sizeof(node_id), "%d", nid);

	if (gracefully)
		sendchar = "d";
	else
		sendchar = "D";

	pcp_write(pcpConn->pcpConn, sendchar, 1);
	wsize = htonl(strlen(node_id) + 1 + sizeof(int));
	pcp_write(pcpConn->pcpConn, &wsize, sizeof(int));
	pcp_write(pcpConn->pcpConn, node_id, strlen(node_id) + 1);
	if (PCPFlush(pcpConn) < 0)
		return NULL;
	if (pcpConn->Pfdebug)
		fprintf(pcpConn->Pfdebug, "DEBUG: send: tos=\"D\", len=%d\n", ntohl(wsize));

	return process_pcp_response(pcpConn, 'D');
}

static void
process_command_complete_response(PCPConnInfo * pcpConn, char *buf, int len)
{
	if (strcmp(buf, "CommandComplete") == 0)
	{
		setCommandSuccessful(pcpConn);
	}
	else
	{
		pcp_internal_error(pcpConn,
						   "command failed with reason: \"%s\"", buf);
		setResultStatus(pcpConn, PCP_RES_BAD_RESPONSE);
	}
}

/* --------------------------------
 * pcp_attach_node - attach a node given by the argument from pgpool's control
 *
 * return 0 on success, -1 otherwise
 * --------------------------------
 */
PCPResultInfo *
pcp_attach_node(PCPConnInfo * pcpConn, int nid)
{
	int			wsize;
	char		node_id[16];

	if (PCPConnectionStatus(pcpConn) != PCP_CONNECTION_OK)
	{
		pcp_internal_error(pcpConn, "invalid PCP connection");
		return NULL;
	}

	snprintf(node_id, sizeof(node_id), "%d", nid);

	pcp_write(pcpConn->pcpConn, "C", 1);
	wsize = htonl(strlen(node_id) + 1 + sizeof(int));
	pcp_write(pcpConn->pcpConn, &wsize, sizeof(int));
	pcp_write(pcpConn->pcpConn, node_id, strlen(node_id) + 1);
	if (PCPFlush(pcpConn) < 0)
		return NULL;
	if (pcpConn->Pfdebug)
		fprintf(pcpConn->Pfdebug, "DEBUG: send: tos=\"C\", len=%d\n", ntohl(wsize));

	return process_pcp_response(pcpConn, 'C');
}


/* --------------------------------
 * pcp_pool_status - return setup parameters and status
 *
 * returns and array of POOL_REPORT_CONFIG, NULL otherwise
 * --------------------------------
 */
static void
process_pool_status_response(PCPConnInfo * pcpConn, char *buf, int len)
{
	char	   *index;
	POOL_REPORT_CONFIG *status = NULL;

	if (strcmp(buf, "ArraySize") == 0)
	{
		int			ci_size;

		index = (char *) memchr(buf, '\0', len) + 1;
		ci_size = ntohl(*((int *) index));

		setResultStatus(pcpConn, PCP_RES_INCOMPLETE);
		setResultSlotCount(pcpConn, ci_size);
		pcpConn->pcpResInfo->nextFillSlot = 0;
		return;
	}
	else if (strcmp(buf, "ProcessConfig") == 0)
	{
		if (PCPResultStatus(pcpConn->pcpResInfo) != PCP_RES_INCOMPLETE)
			goto INVALID_RESPONSE;

		status = palloc(sizeof(POOL_REPORT_CONFIG));

		index = (char *) memchr(buf, '\0', len);
		if (index == NULL)
			goto INVALID_RESPONSE;
		index += 1;
		strlcpy(status->name, index, POOLCONFIG_MAXNAMELEN + 1);

		index = (char *) memchr(index, '\0', len);
		if (index == NULL)
			goto INVALID_RESPONSE;
		index += 1;
		strlcpy(status->value, index, POOLCONFIG_MAXVALLEN + 1);

		index = (char *) memchr(index, '\0', len);
		if (index == NULL)
			goto INVALID_RESPONSE;
		index += 1;
		strlcpy(status->desc, index, POOLCONFIG_MAXDESCLEN + 1);

		if (setNextResultBinaryData(pcpConn->pcpResInfo, (void *) status, sizeof(POOL_REPORT_CONFIG), NULL) < 0)
			goto INVALID_RESPONSE;
		return;
	}
	else if (strcmp(buf, "CommandComplete") == 0)
	{
		setResultStatus(pcpConn, PCP_RES_COMMAND_OK);
		return;
	}

INVALID_RESPONSE:

	if (status)
		pfree(status);
	pcp_internal_error(pcpConn,
					   "command failed. invalid response");
	setResultStatus(pcpConn, PCP_RES_BAD_RESPONSE);
}

PCPResultInfo *
pcp_pool_status(PCPConnInfo * pcpConn)
{
	int			wsize;

	if (PCPConnectionStatus(pcpConn) != PCP_CONNECTION_OK)
	{
		pcp_internal_error(pcpConn, "invalid PCP connection");
		return NULL;
	}

	pcp_write(pcpConn->pcpConn, "B", 1);
	wsize = htonl(sizeof(int));
	pcp_write(pcpConn->pcpConn, &wsize, sizeof(int));
	if (PCPFlush(pcpConn) < 0)
		return NULL;
	if (pcpConn->Pfdebug)
		fprintf(pcpConn->Pfdebug, "DEBUG pcp_pool_status: send: tos=\"B\", len=%d\n", ntohl(wsize));
	return process_pcp_response(pcpConn, 'B');
}


PCPResultInfo *
pcp_recovery_node(PCPConnInfo * pcpConn, int nid)
{
	int			wsize;
	char		node_id[16];

	if (PCPConnectionStatus(pcpConn) != PCP_CONNECTION_OK)
	{
		pcp_internal_error(pcpConn, "invalid PCP connection");
		return NULL;
	}

	snprintf(node_id, sizeof(node_id), "%d", nid);

	pcp_write(pcpConn->pcpConn, "O", 1);
	wsize = htonl(strlen(node_id) + 1 + sizeof(int));
	pcp_write(pcpConn->pcpConn, &wsize, sizeof(int));
	pcp_write(pcpConn->pcpConn, node_id, strlen(node_id) + 1);
	if (PCPFlush(pcpConn) < 0)
		return NULL;
	if (pcpConn->Pfdebug)
		fprintf(pcpConn->Pfdebug, "DEBUG: send: tos=\"D\", len=%d\n", ntohl(wsize));

	return process_pcp_response(pcpConn, 'O');
}

/* --------------------------------
 * pcp_promote_node - promote a node given by the argument as new pgpool's main node
 *
 * return 0 on success, -1 otherwise
 * --------------------------------
 */
PCPResultInfo *
pcp_promote_node(PCPConnInfo * pcpConn, int nid, bool promote)
{
	return _pcp_promote_node(pcpConn, nid, FALSE, promote);
}

/* --------------------------------

 * and promote a node given by the argument as new pgpool's main node
 *
 * return 0 on success, -1 otherwise
 * --------------------------------
 */
PCPResultInfo *
pcp_promote_node_gracefully(PCPConnInfo * pcpConn, int nid, bool switchover)
{
	return _pcp_promote_node(pcpConn, nid, TRUE, switchover);
}

static PCPResultInfo *
_pcp_promote_node(PCPConnInfo * pcpConn, int nid, bool gracefully, bool switchover)
{
	int			wsize;
	char		node_id[16];
	char	   *sendchar;
	char		*switchover_option;	/* n: just change node status, s: switchover primary */

	if (PCPConnectionStatus(pcpConn) != PCP_CONNECTION_OK)
	{
		pcp_internal_error(pcpConn, "invalid PCP connection");
		return NULL;
	}

	snprintf(node_id, sizeof(node_id), "%d ", nid);

	if (gracefully)
		sendchar = "j";
	else
		sendchar = "J";

	if (switchover)
		switchover_option = "s";
	else
		switchover_option = "n";

	pcp_write(pcpConn->pcpConn, sendchar, 1);

	/* caluculate send buffer size */
	wsize = sizeof(char);	/* protocol. 'j' or 'J' */
	wsize += strlen(node_id);	/* node id + space */
	wsize += sizeof(char);	/* promote option */
	wsize += sizeof(int);	/* buffer length */
	wsize = htonl(wsize);

	pcp_write(pcpConn->pcpConn, &wsize, sizeof(int));
	pcp_write(pcpConn->pcpConn, node_id, strlen(node_id) + 1);
	pcp_write(pcpConn->pcpConn, switchover_option, 1);

	if (PCPFlush(pcpConn) < 0)
		return NULL;
	if (pcpConn->Pfdebug)
		fprintf(pcpConn->Pfdebug, "DEBUG: send: tos=\"E\", len=%d\n", ntohl(wsize));

	return process_pcp_response(pcpConn, 'J');
}

static void
process_watchdog_info_response(PCPConnInfo * pcpConn, char *buf, int len)
{
	char	   *json_data = NULL;
	PCPWDClusterInfo *wd_cluster_info = NULL;
	int			clusterDataSize = 0;

	if (strcmp(buf, "CommandComplete") == 0)
	{
		int			tempVal;
		char	   *ptr;

		json_data = (char *) memchr(buf, '\0', len);
		if (json_data == NULL)
			goto INVALID_RESPONSE;
		json_data += 1;

		json_value *root;
		json_value *value;
		int			i,
					nodeCount;

		root = json_parse(json_data, len);

		/* The root node must be object */
		if (root == NULL || root->type != json_object)
		{
			json_value_free(root);
			goto INVALID_RESPONSE;
		}

		if (json_get_int_value_for_key(root, "NodeCount", &nodeCount))
		{
			json_value_free(root);
			goto INVALID_RESPONSE;
		}

		/* find the WatchdogNodes array */
		value = json_get_value_for_key(root, "WatchdogNodes");
		if (value == NULL)
		{
			json_value_free(root);
			goto INVALID_RESPONSE;
		}
		if (value->type != json_array)
		{
			json_value_free(root);
			goto INVALID_RESPONSE;
		}
		if (nodeCount != value->u.array.length)
		{
			json_value_free(root);
			goto INVALID_RESPONSE;
		}

		/* create the cluster object */
		clusterDataSize = sizeof(PCPWDClusterInfo) + (sizeof(PCPWDNodeInfo) * nodeCount);
		wd_cluster_info = malloc(clusterDataSize);

		wd_cluster_info->nodeCount = nodeCount;

		if (json_get_int_value_for_key(root, "RemoteNodeCount", &wd_cluster_info->remoteNodeCount))
		{
			json_value_free(root);
			goto INVALID_RESPONSE;
		}
		if (json_get_int_value_for_key(root, "MemberRemoteNodeCount", &wd_cluster_info->memberRemoteNodeCount))
		{
			wd_cluster_info->memberRemoteNodeCount = -1;
		}
		if (json_get_int_value_for_key(root, "NodesRequireForQuorum", &wd_cluster_info->nodesRequiredForQuorum))
		{
			wd_cluster_info->nodesRequiredForQuorum = -1;
		}

		if (json_get_int_value_for_key(root, "QuorumStatus", &wd_cluster_info->quorumStatus))
		{
			json_value_free(root);
			goto INVALID_RESPONSE;
		}
		if (json_get_int_value_for_key(root, "AliveNodeCount", &wd_cluster_info->aliveNodeCount))
		{
			json_value_free(root);
			goto INVALID_RESPONSE;
		}
		if (json_get_int_value_for_key(root, "Escalated", &tempVal))
		{
			json_value_free(root);
			goto INVALID_RESPONSE;
		}
		wd_cluster_info->escalated = tempVal == 0 ? false : true;

		ptr = json_get_string_value_for_key(root, "LeaderNodeName");
		if (ptr == NULL)
		{
			json_value_free(root);
			goto INVALID_RESPONSE;
		}
		strncpy(wd_cluster_info->leaderNodeName, ptr, sizeof(wd_cluster_info->leaderNodeName) - 1);

		ptr = json_get_string_value_for_key(root, "LeaderHostName");
		if (ptr == NULL)
		{
			json_value_free(root);
			goto INVALID_RESPONSE;
		}
		strncpy(wd_cluster_info->leaderHostName, ptr, sizeof(wd_cluster_info->leaderHostName) - 1);

		/* Get watchdog nodes data */
		for (i = 0; i < nodeCount; i++)
		{
			char	   *ptr;
			json_value *nodeInfoValue = value->u.array.values[i];
			PCPWDNodeInfo *wdNodeInfo = &wd_cluster_info->nodeList[i];

			if (nodeInfoValue->type != json_object)
			{
				json_value_free(root);
				goto INVALID_RESPONSE;
			}

			if (json_get_int_value_for_key(nodeInfoValue, "ID", &wdNodeInfo->id))
			{
				json_value_free(root);
				goto INVALID_RESPONSE;
			}

			ptr = json_get_string_value_for_key(nodeInfoValue, "NodeName");
			if (ptr == NULL)
			{
				json_value_free(root);
				goto INVALID_RESPONSE;
			}
			strncpy(wdNodeInfo->nodeName, ptr, sizeof(wdNodeInfo->nodeName) - 1);

			ptr = json_get_string_value_for_key(nodeInfoValue, "HostName");
			if (ptr == NULL)
			{
				json_value_free(root);
				goto INVALID_RESPONSE;
			}
			strncpy(wdNodeInfo->hostName, ptr, sizeof(wdNodeInfo->hostName) - 1);

			ptr = json_get_string_value_for_key(nodeInfoValue, "DelegateIP");
			if (ptr == NULL)
			{
				json_value_free(root);
				goto INVALID_RESPONSE;
			}
			strncpy(wdNodeInfo->delegate_ip, ptr, sizeof(wdNodeInfo->delegate_ip) - 1);

			if (json_get_int_value_for_key(nodeInfoValue, "Membership", &wdNodeInfo->membership_status))
			{
				/* would be from the older version. No need to panic */
				wdNodeInfo->membership_status = 0;
			}

			ptr = json_get_string_value_for_key(nodeInfoValue, "MembershipString");
			if (ptr == NULL)
			{
				strncpy(wdNodeInfo->membership_status_string, "NOT-Available",
						sizeof(wdNodeInfo->membership_status_string) - 1);
			}
			else
				strncpy(wdNodeInfo->membership_status_string, ptr,
						sizeof(wdNodeInfo->membership_status_string) - 1);

			if (json_get_int_value_for_key(nodeInfoValue, "WdPort", &wdNodeInfo->wd_port))
			{
				json_value_free(root);
				goto INVALID_RESPONSE;
			}

			if (json_get_int_value_for_key(nodeInfoValue, "PgpoolPort", &wdNodeInfo->pgpool_port))
			{
				json_value_free(root);
				goto INVALID_RESPONSE;
			}

			if (json_get_int_value_for_key(nodeInfoValue, "State", &wdNodeInfo->state))
			{
				json_value_free(root);
				goto INVALID_RESPONSE;
			}

			ptr = json_get_string_value_for_key(nodeInfoValue, "StateName");
			if (ptr == NULL)
			{
				json_value_free(root);
				goto INVALID_RESPONSE;
			}
			strncpy(wdNodeInfo->stateName, ptr, sizeof(wdNodeInfo->stateName) - 1);

			if (json_get_int_value_for_key(nodeInfoValue, "Priority", &wdNodeInfo->wd_priority))
			{
				json_value_free(root);
				goto INVALID_RESPONSE;
			}

		}
		json_value_free(root);

		if (setNextResultBinaryData(pcpConn->pcpResInfo, (void *) wd_cluster_info, clusterDataSize, NULL) < 0)
			goto INVALID_RESPONSE;

		setCommandSuccessful(pcpConn);
	}
	else
	{
		pcp_internal_error(pcpConn,
						   "command failed with reason: \"%s\"\n", buf);
		setResultStatus(pcpConn, PCP_RES_BAD_RESPONSE);
	}
	return;

INVALID_RESPONSE:

	if (wd_cluster_info)
		pfree(wd_cluster_info);
	pcp_internal_error(pcpConn,
					   "command failed. invalid response\n");
	setResultStatus(pcpConn, PCP_RES_BAD_RESPONSE);
}

/* --------------------------------
 * pcp_watchdog_info - get information of watchdog
 *
 * return structure of watchdog information on success, -1 otherwise
 * --------------------------------
 */
PCPResultInfo *
pcp_watchdog_info(PCPConnInfo * pcpConn, int nid)
{
	int			wsize;
	char		wd_index[16];

	if (PCPConnectionStatus(pcpConn) != PCP_CONNECTION_OK)
	{
		pcp_internal_error(pcpConn, "invalid PCP connection");
		return NULL;
	}

	snprintf(wd_index, sizeof(wd_index), "%d", nid);

	pcp_write(pcpConn->pcpConn, "W", 1);
	wsize = htonl(strlen(wd_index) + 1 + sizeof(int));
	pcp_write(pcpConn->pcpConn, &wsize, sizeof(int));
	pcp_write(pcpConn->pcpConn, wd_index, strlen(wd_index) + 1);
	if (PCPFlush(pcpConn) < 0)
		return NULL;
	if (pcpConn->Pfdebug)
		fprintf(pcpConn->Pfdebug, "DEBUG: send: tos=\"W\", len=%d\n", ntohl(wsize));

	return process_pcp_response(pcpConn, 'W');
}

PCPResultInfo *
pcp_set_backend_parameter(PCPConnInfo * pcpConn, char *parameter_name, char *value)
{
	int			wsize;
	char		null_chr = 0;

	if (PCPConnectionStatus(pcpConn) != PCP_CONNECTION_OK)
	{
		pcp_internal_error(pcpConn, "invalid PCP connection");
		return NULL;
	}
	if (pcpConn->Pfdebug)
		fprintf(pcpConn->Pfdebug, "DEBUG: seting: \"%s = %s\"\n", parameter_name, value);

	pcp_write(pcpConn->pcpConn, "A", 1);
	wsize = htonl(strlen(parameter_name) + 1 +
				  strlen(value) + 1 +
				  sizeof(int));
	pcp_write(pcpConn->pcpConn, &wsize, sizeof(int));
	pcp_write(pcpConn->pcpConn, parameter_name, strlen(parameter_name));
	pcp_write(pcpConn->pcpConn, &null_chr, 1);
	pcp_write(pcpConn->pcpConn, value, strlen(value));
	pcp_write(pcpConn->pcpConn, &null_chr, 1);
	if (PCPFlush(pcpConn) < 0)
		return NULL;
	if (pcpConn->Pfdebug)
		fprintf(pcpConn->Pfdebug, "DEBUG: send: tos=\"A\", len=%d\n", ntohl(wsize));

	return process_pcp_response(pcpConn, 'A');
}

/*
 * pcpAddInternalNotice - produce an internally-generated notice message
 *
 * A format string and optional arguments can be passed.
 *
 * The supplied text is taken as primary message (ie., it should not include
 * a trailing newline, and should not be more than one line).
 */
static void
pcp_internal_error(PCPConnInfo * pcpConn, const char *fmt,...)
{
	char		msgBuf[1024];
	va_list		args;

	if (pcpConn == NULL)
		return;					/* nobody home to receive notice? */

	/* Format the message */
	va_start(args, fmt);
	vsnprintf(msgBuf, sizeof(msgBuf), fmt, args);
	va_end(args);
	msgBuf[sizeof(msgBuf) - 1] = '\0';	/* make real sure it's terminated */
	if (pcpConn->errMsg)
		pfree(pcpConn->errMsg);
	pcpConn->errMsg = pstrdup(msgBuf);
}

ConnStateType
PCPConnectionStatus(const PCPConnInfo * conn)
{
	if (!conn)
		return PCP_CONNECTION_BAD;
	return conn->connState;
}

ResultStateType
PCPResultStatus(const PCPResultInfo * res)
{
	if (!res)
		return PCP_RES_ERROR;
	return res->resultStatus;
}

static void
setResultStatus(PCPConnInfo * pcpConn, ResultStateType resultState)
{
	if (pcpConn && pcpConn->pcpResInfo)
		pcpConn->pcpResInfo->resultStatus = resultState;
}

static void
setCommandSuccessful(PCPConnInfo * pcpConn)
{
	setResultStatus(pcpConn, PCP_RES_COMMAND_OK);
}

static void
setResultSlotCount(PCPConnInfo * pcpConn, unsigned int slotCount)
{
	if (pcpConn && pcpConn->pcpResInfo && slotCount > 0)
	{
		if (pcpConn->pcpResInfo->resultSlots == 0)
			pcpConn->pcpResInfo->resultSlots = 1;

		if (slotCount > pcpConn->pcpResInfo->resultSlots)
		{
			pcpConn->pcpResInfo = repalloc(pcpConn->pcpResInfo, sizeof(PCPResultInfo) + (sizeof(PCPResultSlot) * (slotCount - 1)));
		}
		pcpConn->pcpResInfo->resultSlots = slotCount;
	}
}

static void
setResultIntData(PCPResultInfo * res, unsigned int slotno, int value)
{
	if (res)
	{
		res->resultSlot[slotno].datalen = 0;
		res->resultSlot[slotno].isint = 1;
		res->resultSlot[slotno].data.integer = value;
		res->resultSlot[slotno].free_func = NULL;
	}
}

static void
setResultBinaryData(PCPResultInfo * res, unsigned int slotno, void *value, int datalen, void (*free_func) (struct PCPConnInfo *, void *))
{
	if (res)
	{
		res->resultSlot[slotno].datalen = datalen;
		res->resultSlot[slotno].isint = 0;
		res->resultSlot[slotno].data.ptr = value;
		res->resultSlot[slotno].free_func = free_func;
	}
}

static int
setNextResultBinaryData(PCPResultInfo * res, void *value, int datalen, void (*free_func) (struct PCPConnInfo *, void *))
{
	if (res && res->nextFillSlot < res->resultSlots)
	{
		setResultBinaryData(res, res->nextFillSlot, value, datalen, free_func);
		res->nextFillSlot++;
		return res->nextFillSlot;
	}
	return -1;
}

static int
PCPFlush(PCPConnInfo * pcpConn)
{
	int			ret = pcp_flush(pcpConn->pcpConn);

	if (ret)
		pcp_internal_error(pcpConn,
						   "ERROR: sending data to backend failed with error \"%s\"", strerror(errno));
	return ret;
}

int
pcp_result_slot_count(PCPResultInfo * res)
{
	if (res)
		return res->resultSlots;
	return 0;
}

/* Returns 1 if ResultInfo has no data. 0 otherwise */
int
pcp_result_is_empty(PCPResultInfo * res)
{
	if (res)
	{
		if (res->resultSlots <= 1 && res->resultSlot[0].isint == 0 && res->resultSlot[0].datalen <= 0)
			return 1;
		return 0;
	}
	return 1;
}

void *
pcp_get_binary_data(const PCPResultInfo * res, unsigned int slotno)
{
	if (res && slotno < res->resultSlots && !res->resultSlot[slotno].isint)
	{
		return res->resultSlot[slotno].data.ptr;
	}
	return NULL;
}

int
pcp_get_int_data(const PCPResultInfo * res, unsigned int slotno)
{
	if (res && slotno < res->resultSlots && res->resultSlot[slotno].isint)
	{
		return res->resultSlot[slotno].data.integer;
	}
	return 0;
}

int
pcp_get_data_length(const PCPResultInfo * res, unsigned int slotno)
{
	if (res && slotno < res->resultSlots)
	{
		return res->resultSlot[slotno].datalen;
	}
	return 0;
}

void
pcp_free_result(PCPConnInfo * pcpConn)
{
	if (pcpConn && pcpConn->pcpResInfo)
	{
		PCPResultInfo *pcpRes = pcpConn->pcpResInfo;
		int			i;

		for (i = 0; i < pcpRes->resultSlots; i++)
		{
			if (pcpRes->resultSlot[i].isint)
				continue;
			if (pcpRes->resultSlot[i].data.ptr == NULL)
				continue;

			if (pcpRes->resultSlot[i].free_func)
				pcpRes->resultSlot[i].free_func(pcpConn, pcpRes->resultSlot[i].data.ptr);
			else
				pfree(pcpRes->resultSlot[i].data.ptr);
			pcpRes->resultSlot[i].data.ptr = NULL;
		}
		pfree(pcpConn->pcpResInfo);
		pcpConn->pcpResInfo = NULL;
	}
}

void
pcp_free_connection(PCPConnInfo * pcpConn)
{
	if (pcpConn)
	{
		pcp_free_result(pcpConn);
		if (pcpConn->errMsg)
			pfree(pcpConn->errMsg);
		/* Should we also Disconnect it? */
		pfree(pcpConn);
	}
}

char *
pcp_get_last_error(PCPConnInfo * pcpConn)
{
	if (pcpConn)
		return pcpConn->errMsg;
	return NULL;
}

/*
 * get the password file name which could be either pointed by PCPPASSFILE
 * environment variable or resides in user home directory.
 */
static bool
getPoolPassFilename(char *pgpassfile)
{
	char	   *passfile_env;

	if ((passfile_env = getenv("PCPPASSFILE")) != NULL)
	{
		/* use the literal path from the environment, if set */
		strlcpy(pgpassfile, passfile_env, MAXPGPATH);
	}
	else
	{
		char		homedir[MAXPGPATH];

		if (!get_home_directory(homedir, sizeof(homedir)))
			return false;
		snprintf(pgpassfile, MAXPGPATH + sizeof(PCPPASSFILE) + 1, "%s/%s", homedir, PCPPASSFILE);
	}
	return true;
}

/*
 * Get a password from the password file. Return value is malloc'd.
 * format = hostname:port:username:password
 */
static char *
PasswordFromFile(PCPConnInfo * pcpConn, char *hostname, char *port, char *username)
{
	FILE	   *fp;
	char		pgpassfile[MAXPGPATH + sizeof(PCPPASSFILE) + 1];
	struct stat stat_buf;
#define LINELEN NAMEDATALEN*5
	char		buf[LINELEN];

	if (username == NULL || strlen(username) == 0)
		return NULL;

	if (hostname == NULL)
		hostname = DefaultHost;
	else if (strcmp(hostname, UNIX_DOMAIN_PATH) == 0)
		hostname = DefaultHost;

	if (!getPoolPassFilename(pgpassfile))
		return NULL;

	/* If password file cannot be opened, ignore it. */
	if (stat(pgpassfile, &stat_buf) != 0)
		return NULL;

	if (!S_ISREG(stat_buf.st_mode))
	{
		if (pcpConn->Pfdebug)
			fprintf(pcpConn->Pfdebug, "WARNING: password file \"%s\" is not a plain file\n", pgpassfile);
		return NULL;
	}

	/* If password file is insecure, alert the user and ignore it. */
	if (stat_buf.st_mode & (S_IRWXG | S_IRWXO))
	{
		if (pcpConn->Pfdebug)
			fprintf(pcpConn->Pfdebug, "WARNING: password file \"%s\" has group or world access; permissions should be u=rw (0600) or less\n",
					pgpassfile);
		return NULL;
	}

	fp = fopen(pgpassfile, "r");
	if (fp == NULL)
		return NULL;

	while (!feof(fp) && !ferror(fp))
	{
		char	   *t = buf,
				   *ret,
				   *p1,
				   *p2;
		int			len;

		if (fgets(buf, sizeof(buf), fp) == NULL)
			break;

		len = strlen(buf);
		if (len == 0)
			continue;

		/* Remove trailing newline */
		if (buf[len - 1] == '\n')
			buf[len - 1] = 0;

		if ((t = pwdfMatchesString(t, hostname)) == NULL ||
			(t = pwdfMatchesString(t, port)) == NULL ||
			(t = pwdfMatchesString(t, username)) == NULL)
			continue;
		ret = pstrdup(t);
		fclose(fp);

		/* De-escape password. */
		for (p1 = p2 = ret; *p1 != ':' && *p1 != '\0'; ++p1, ++p2)
		{
			if (*p1 == '\\' && p1[1] != '\0')
				++p1;
			*p2 = *p1;
		}
		*p2 = '\0';

		return ret;
	}

	fclose(fp);
	return NULL;

#undef LINELEN
}

/*
 * Helper function for PasswordFromFile borrowed from PG
 * returns a pointer to the next token or NULL if the current
 * token doesn't match
 */
static char *
pwdfMatchesString(char *buf, char *token)
{
	char	   *tbuf,
			   *ttok;
	bool		bslash = false;

	if (buf == NULL || token == NULL)
		return NULL;
	tbuf = buf;
	ttok = token;
	if (tbuf[0] == '*' && tbuf[1] == ':')
		return tbuf + 2;
	while (*tbuf != 0)
	{
		if (*tbuf == '\\' && !bslash)
		{
			tbuf++;
			bslash = true;
		}
		if (*tbuf == ':' && *ttok == 0 && !bslash)
			return tbuf + 1;
		bslash = false;
		if (*ttok == 0)
			return NULL;
		if (*tbuf == *ttok)
		{
			tbuf++;
			ttok++;
		}
		else
			return NULL;
	}
	return NULL;
}
