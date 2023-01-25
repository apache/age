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


#ifndef pool_connection_pool_h
#define pool_connection_pool_h

extern POOL_CONNECTION_POOL * pool_connection_pool; /* connection pool */

extern int pool_init_cp(void);
extern POOL_CONNECTION_POOL * pool_create_cp(void);
extern POOL_CONNECTION_POOL * pool_get_cp(char *user, char *database, int protoMajor, int check_socket);
extern void pool_discard_cp(char *user, char *database, int protoMajor);
extern void pool_backend_timer(void);
extern void pool_connection_pool_timer(POOL_CONNECTION_POOL * backend);
extern RETSIGTYPE pool_backend_timer_handler(int sig);
extern int	connect_inet_domain_socket(int slot, bool retry);
extern int	connect_unix_domain_socket(int slot, bool retry);
extern int	connect_inet_domain_socket_by_port(char *host, int port, bool retry);
extern int	connect_unix_domain_socket_by_port(int port, char *socket_dir, bool retry);
extern int	pool_pool_index(void);
extern void close_all_backend_connections(void);
extern void update_pooled_connection_count(void);
#endif /* pool_connection_pool_h */
