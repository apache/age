/*
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

#ifndef ps_status_h
#define ps_status_h

#include "pool.h"
#include <netdb.h>

extern char remote_ps_data[NI_MAXHOST + NI_MAXSERV + 2];	/* used for set_ps_display */

extern char **save_ps_display_args(int argc, char **argv);
extern void init_ps_display(const char *username, const char *dbname,
				const char *host_info, const char *initial_str);
extern void set_ps_display(const char *activity, bool force);
extern const char *get_ps_display(int *displen);
extern void pool_ps_idle_display(POOL_CONNECTION_POOL * backend);


#endif /* ps_status_h */
