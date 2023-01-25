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

#ifndef statistics_h
#define statistics_h

extern size_t	stat_shared_memory_size(void);
extern void		stat_set_stat_area(void *address);
extern void		stat_init_stat_area(void);
extern void		stat_count_up(int backend_node_id, Node *parsetree);
extern void		error_stat_count_up(int backend_node_id, char *str);
extern uint64	stat_get_select_count(int backend_node_id);
extern uint64	stat_get_insert_count(int backend_node_id);
extern uint64	stat_get_update_count(int backend_node_id);
extern uint64	stat_get_delete_count(int backend_node_id);
extern uint64	stat_get_ddl_count(int backend_node_id);
extern uint64	stat_get_other_count(int backend_node_id);
extern uint64	stat_get_panic_count(int backend_node_id);
extern uint64	stat_get_fatal_count(int backend_node_id);
extern uint64	stat_get_error_count(int backend_node_id);

#endif /* statistics_h */
