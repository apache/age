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


#ifndef pool_internal_comms_h
#define pool_internal_comms_h

extern bool terminate_pgpool(char mode, bool error);
extern void notice_backend_error(int node_id, unsigned char flags);
extern bool degenerate_backend_set(int *node_id_set, int count,
								   unsigned char flags);
extern bool degenerate_backend_set_ex(int *node_id_set, int count,
									  unsigned char flags, bool error, bool test_only);
extern bool promote_backend(int node_id, unsigned char flags);
extern bool send_failback_request(int node_id, bool throw_error,
								  unsigned char flags);

extern void degenerate_all_quarantine_nodes(void);
extern bool close_idle_connections(void);

/* defined in pgpool_main.c */
extern void register_watchdog_quorum_change_interrupt(void);
extern void register_watchdog_state_change_interrupt(void);
extern void register_backend_state_sync_req_interrupt(void);
extern void register_inform_quarantine_nodes_req(void);
extern bool register_node_operation_request(POOL_REQUEST_KIND kind,
											int *node_id_set, int count, unsigned char flags);
#endif /* pool_internal_comms_h */
