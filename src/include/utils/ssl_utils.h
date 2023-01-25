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
 *
 */

#ifndef SSL_UTILS_H
#define SSL_UTILS_H

extern void calculate_hmac_sha256(const char *data, int len, char *buf);
extern int aes_decrypt_with_password(unsigned char *ciphertext, int ciphertext_len,
						  const char *password, unsigned char *plaintext);
extern int aes_encrypt_with_password(unsigned char *plaintext, int plaintext_len,
						  const char *password, unsigned char *ciphertext);

#endif
