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

#ifndef BUFFER_H

#define BUFFER_H

#define SKIP_TABS(p) while (*p == '\t') {p++;}

extern int	buffer_read_int(char *buf, char **bufp);
extern char *buffer_read_string(char *buf, char **bufp);
extern char buffer_read_char(char *buf, char **bufp);

#endif
