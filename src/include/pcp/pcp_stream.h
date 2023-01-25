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
 * pcp_stream.h - pcp_stream header file.
 */

#ifndef PCP_STREAM_H
#define PCP_STREAM_H

#include "pool.h"

#define READBUFSZ              1024
#define WRITEBUFSZ             8192


typedef struct
{
	int			fd;				/* fd for connection */

	char	   *wbuf;			/* write buffer for the connection */
	int			wbufsz;			/* write buffer size */
	int			wbufpo;			/* buffer offset */

	char	   *hp;				/* pending data buffer head address */
	int			po;				/* pending data offset */
	int			bufsz;			/* pending data buffer size */
	int			len;			/* pending data length */
}			PCP_CONNECTION;

extern PCP_CONNECTION * pcp_open(int fd);
extern void pcp_close(PCP_CONNECTION * pc);
extern int	pcp_read(PCP_CONNECTION * pc, void *buf, int len);
extern int	pcp_write(PCP_CONNECTION * pc, void *buf, int len);
extern int	pcp_flush(PCP_CONNECTION * pc);

#define UNIX_DOMAIN_PATH "/tmp"

#endif							/* PCP_STREAM_H */
