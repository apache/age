/*
 * Copyright (c) 2017	Tatsuo Ishii
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

#ifndef SEND_H
#define SEND_H

extern void send_char(char c, PGconn *conn);
extern void send_int(int intval, PGconn *conn);
extern void send_int16(short shortval, PGconn *conn);
extern void send_string(char *buf, PGconn *conn);
extern void send_byte(char *buf, int len, PGconn *conn);

#endif
