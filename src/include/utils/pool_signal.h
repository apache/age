/* -*-pgsql-c-*- */
/*
 *
 * $Header$
 *
 * pgpool: a language independent connection pool server for PostgreSQL
 * written by Tatsuo Ishii
 *
 * Portions Copyright (c) 2003-2008,	PgPool Global Development Group
 * Portions Copyright (c) 2004, PostgreSQL Global Development Group
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
 * pool_signal.h.: pool_signal definition header file
 *
 */

#ifndef POOL_SIGNAL_H
#define POOL_SIGNAL_H

/*
 * Signal stuff. Stolen from PostgreSQL source code.
 */
#include "config.h"
#include <signal.h>

#ifdef HAVE_SIGPROCMASK
#define pool_sigset_t sigset_t
#else
#define pool_sigset_t int
#endif

#ifdef _SYS_SIGNAL_H_
/* This appears to be a BSD variant which also uses a different type name */
typedef sig_t sighandler_t;
#endif

extern pool_sigset_t UnBlockSig,
BlockSig,
AuthBlockSig;


#ifdef HAVE_SIGPROCMASK

#define POOL_SETMASK(mask)	sigprocmask(SIG_SETMASK, mask, NULL)
#define POOL_SETMASK2(mask, oldmask)	sigprocmask(SIG_SETMASK, mask, oldmask)
#else

#ifndef WIN32
#define POOL_SETMASK(mask)	sigsetmask(*((int*)(mask)))
#define POOL_SETMASK2(mask, oldmask)	do {oldmask = POOL_SETMASK(mask)} while (0)
#else
#define POOL_SETMASK(mask)		pqsigsetmask(*((int*)(mask)))
#define POOL_SETMASK2(mask, oldmask)	do {oldmask = POOL_SETMASK(mask)} while (0)
int			pqsigsetmask(int mask);
#endif
#endif
typedef void (*pool_sighandler_t) (int);
extern pool_sighandler_t pool_signal(int signo, pool_sighandler_t func);
extern void poolinitmask(void);
extern int	pool_signal_parent(int sig);
#endif							/* POOL_SIGNAL_H */
