/* -*-pgsql-c-*- */
/*
 * $Header$
 *
 * pgpool: a language independent connection pool server for PostgreSQL
 * written by Tatsuo Ishii
 *
 * Portions Copyright (c) 2003-2008, PgPool Global Development Group
 * Portions Copyright (c) 2003-2004, PostgreSQL Global Development Group
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
#ifndef IPC_H
#define IPC_H

#define IPCProtection	(0600)	/* access/modify by user only */

extern void shmem_exit(int code);
extern void on_shmem_exit(void (*function) (int code, Datum arg), Datum arg);
extern void on_exit_reset(void);

/*pool_sema.c*/
extern void pool_semaphore_create(int numSems);
extern void pool_semaphore_lock(int semNum);
extern int	pool_semaphore_lock_allow_interrupt(int semNum);
extern void pool_semaphore_unlock(int semNum);

#endif							/* IPC_H */
