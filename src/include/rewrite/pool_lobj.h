/* -*-pgsql-c-*- */
/*
 *
 * $Header$
 *
 * pgpool: a language independent connection pool server for PostgreSQL
 * written by Tatsuo Ishii
 *
 * Copyright (c) 2003-2010	PgPool Global Development Group
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
 * pool_lobj.h.: pool_lobj.c related header file
 *
 */

#ifndef POOL_LOBJ_H
#define POOL_LOBJ_H
#include "pool.h"

extern char *pool_rewrite_lo_creat(char kind, char *packet, int packet_len, POOL_CONNECTION * frontend, POOL_CONNECTION_POOL * backend, int *len);

#endif							/* POOL_LOBJ_H */
