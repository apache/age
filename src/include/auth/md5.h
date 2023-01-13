/*-------------------------------------------------------------------------
 *
 * md5.h
 *	  Interface to md5.c
 *
 * Portions Copyright (c) 1996-2020, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * $Header$
 *
 *-------------------------------------------------------------------------
 */

/*
 *  This file is imported from PostgreSQL 8.1.3.
 *  Modified by Taiki Yamaguchi <yamaguchi@sraoss.co.jp>
 */

#ifndef MD5_H
#define MD5_H

#define MD5_PASSWD_LEN 32

#define WD_AUTH_HASH_LEN 64

extern int	pool_md5_hash(const void *buff, size_t len, char *hexsum);
extern int	pool_md5_encrypt(const char *passwd, const char *salt, size_t salt_len, char *buf);
extern void bytesToHex(char *b, int len, char *s);
extern bool pg_md5_encrypt(const char *passwd, const char *salt, size_t salt_len, char *buf);

#endif
