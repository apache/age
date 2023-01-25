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
 * pool_passwd.h: pool_passwd related definitions.
 *
 */

#ifndef POOL_PASSWD_H
#define POOL_PASSWD_H


#define POOL_PASSWD_FILENAME "pool_passwd"
#define POOL_PASSWD_LEN 35
#define MAX_POOL_PASSWD_LEN 132 /* In case of TEXT prefix(4byte) and plain text password(128byte)*/

#define MAX_USER_NAME_LEN	128
#define MAX_PGPASS_LEN		128
#define MAX_POOL_KEY_LEN	256


#define PASSWORD_MD5_PREFIX 	"md5"
#define PASSWORD_AES_PREFIX 	"AES"
#define PASSWORD_SCRAM_PREFIX	"SCRAM-SHA-256$"
#define PASSWORD_TEXT_PREFIX 	"TEXT"

typedef enum
{
	POOL_PASSWD_R,				/* open pool_passwd in read only mode. used by
								 * pgpool-II child main process */
	POOL_PASSWD_RW,				/* open pool_passwd in read/write mode. used
								 * by pg_md5 command */
}			POOL_PASSWD_MODE;

typedef enum PasswordType
{
	PASSWORD_TYPE_UNKNOWN = 0,
	PASSWORD_TYPE_PLAINTEXT,
	PASSWORD_TYPE_MD5,
	PASSWORD_TYPE_AES,
	PASSWORD_TYPE_SCRAM_SHA_256,
	PASSWORD_TYPE_TEXT_PREFIXED
} PasswordType;

typedef struct UserPassword
{
	char	   *userName;
	char	   *password;
	PasswordType passwordType;
}			UserPassword;

typedef struct PasswordMapping
{
	UserPassword pgpoolUser;
	UserPassword backendUser;
	bool		mappedUser;
}			PasswordMapping;

extern PasswordMapping * pool_get_user_credentials(char *username);
extern PasswordType get_password_type(const char *shadow_pass);
extern void pool_init_pool_passwd(char *pool_passwd_filename, POOL_PASSWD_MODE mode);
extern int	pool_create_passwdent(char *username, char *passwd);
extern char *pool_get_passwd(char *username);
extern void pool_delete_passwdent(char *username);
extern void pool_finish_pool_passwd(void);
extern void pool_reopen_passwd_file(void);
extern char *get_decrypted_password(const char *shadow_pass);
extern char *read_pool_key(char *key_file_path);
extern char *get_pgpool_config_user_password(char *username, char *password_in_config);
extern void delete_passwordMapping(PasswordMapping * pwdMapping);
extern int chceck_password_type_is_not_md5(char *username, char *password_in_config);
#endif							/* POOL_PASSWD_H */
