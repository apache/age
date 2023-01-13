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
 *
 * Process Parse, Bind, Execute message.
 */

#ifndef EXTENDED_QUERY_H
#define EXTENDED_QUERY_H

extern void process_parse(char *buf, PGconn *conn);
extern void process_bind(char *buf, PGconn *conn);
extern void process_execute(char *buf, PGconn *conn);
extern void process_describe(char *buf, PGconn *conn);
extern void process_close(char *buf, PGconn *conn);

#endif
