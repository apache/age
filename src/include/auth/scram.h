/* -*-pgsql-c-*- */
/*
 *
 * $Header$
 *
 * pgpool: a language independent connection pool server for PostgreSQL
 * written by Tatsuo Ishii
 *
 * Copyright (c) 2003-2018	PgPool Global Development Group
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
 * scram.h: SCRAM auth related definitions.
 * borrowed from PostgreSQL code src/include/libpq/scram.h
 *
 */
/*-------------------------------------------------------------------------
 *
 * scram.h
 *	  Interface to libpq/scram.c
 *
 * Portions Copyright (c) 1996-2017, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * src/include/libpq/scram.h
 *
 *-------------------------------------------------------------------------
 */
#ifndef PG_SCRAM_H
#define PG_SCRAM_H

/* Name of SCRAM-SHA-256 per IANA */
#define SCRAM_SHA_256_NAME "SCRAM-SHA-256"

/* Status codes for message exchange */
#define SASL_EXCHANGE_CONTINUE		0
#define SASL_EXCHANGE_SUCCESS		1
#define SASL_EXCHANGE_FAILURE		2

/* Routines dedicated to authentication */
extern void *pg_be_scram_init(const char *username, const char *shadow_pass);
extern int pg_be_scram_exchange(void *opaq, char *input, int inputlen,
					 char **output, int *outputlen, char **logdetail);

/* Routines to handle and check SCRAM-SHA-256 verifier */
extern char *pg_be_scram_build_verifier(const char *password);
extern bool scram_verify_plain_password(const char *username,
							const char *password, const char *verifier);
extern void *pg_fe_scram_init(const char *username, const char *password);
extern void pg_fe_scram_exchange(void *opaq, char *input, int inputlen,
					 char **output, int *outputlen,
					 bool *done, bool *success);
extern void pg_fe_scram_free(void *opaq);
extern char *pg_fe_scram_build_verifier(const char *password);

#endif							/* PG_SCRAM_H */
