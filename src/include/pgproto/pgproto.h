/*
 * Copyright (c) 2017-2018	Tatsuo Ishii
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
 */

#ifndef PGPROTO_H
#define PGPROTO_H

/*
 * The default file name for protocol data to be sent from frontend to backend
 */
#define PGPROTODATA "pgproto.data"

#define MAXENTRIES 128

/*
 * Protocol data representation read from the data file.
 * We assume that the protocol version is V3.
 */
typedef struct
{
	char		type;			/* message type */
	int			size;			/* message length excluding this (in host
								 * order) */
	char		message[1];		/* actual message (variable length) */
}			PROTO_DATA;

extern int	read_nap;

#endif							/* PGPROTO_H */
