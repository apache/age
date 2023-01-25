/* -*-pgsql-c-*- */
/*
 *
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
 * pool_steam.h.: pool_stream.c related header file
 *
 */

#ifndef SOCKET_STREAM_H
#define SOCKET_STREAM_H

extern void socket_set_nonblock(int fd);
extern void socket_unset_nonblock(int fd);

extern int	socket_read(int sock, void *buf, size_t len, int timeout);
extern int	socket_write(int fd, void *buf, size_t len);


#endif							/* SOCKET_STREAM_H */
