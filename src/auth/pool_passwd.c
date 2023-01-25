/* -*-pgsql-c-*- */
/*
 *
 * $Header$
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
 * Module to handle pool_passwd
 */

#include <string.h>
#include <errno.h>

#include "pool.h"
#include "auth/pool_passwd.h"
#include "auth/md5.h"
#include "utils/ssl_utils.h"
#include "utils/base64.h"
#ifndef POOL_PRIVATE
#include "utils/elog.h"
#else
#include "utils/fe_ports.h"
#endif
#include <sys/stat.h>


static FILE *passwd_fd = NULL;	/* File descriptor for pool_passwd */
static char saved_passwd_filename[POOLMAXPATHLEN + 1];
static char *userMatchesString(char *buf, char *user);
static POOL_PASSWD_MODE pool_passwd_mode;

/*
 * Initialize this module.
 * If pool_passwd does not exist yet, create it.
 * Open pool_passwd.
 */
void
pool_init_pool_passwd(char *pool_passwd_filename, POOL_PASSWD_MODE mode)
{
	char	   *openmode;

	if (passwd_fd)
		return;

	if (pool_passwd_filename == NULL)
	{
		saved_passwd_filename[0] = '\0'; /* indicate pool_passwd is disabled */
		return;
	}

	pool_passwd_mode = mode;

	if (saved_passwd_filename[0] == '\0')
	{
		int			len = strlen(pool_passwd_filename);

		memcpy(saved_passwd_filename, pool_passwd_filename, len);
		saved_passwd_filename[len] = '\0';
	}

	if (mode == POOL_PASSWD_R)
		openmode = "r";
	else
		openmode = "r+";

	passwd_fd = fopen(pool_passwd_filename, openmode);
	if (!passwd_fd)
	{
		if (errno == ENOENT)
		{
			/* The file does not exist yet. Create it. */
			passwd_fd = fopen(pool_passwd_filename, "w+");
			if (passwd_fd)
				return;
		}
		ereport(ERROR,
				(errmsg("initializing pool password, failed to open file:\"%s\"", pool_passwd_filename),
				 errdetail("file open failed with error:\"%m\"")));
	}
}

/*
 * Update passwd. If the user does not exist, create a new entry.
 * Returns 0 on success non 0 otherwise.
 */
int
pool_create_passwdent(char *username, char *passwd)
{
#define LINE_LEN \
		MAX_USER_NAME_LEN + 1 + MAX_POOL_PASSWD_LEN + 2
	char 	linebuf[LINE_LEN];
	char 	*writebuf = NULL;
	int		len;
	bool 	updated = false;

	if (!passwd_fd)
		ereport(ERROR,
				(errmsg("error updating password, password file descriptor is NULL")));

	if (pool_passwd_mode != POOL_PASSWD_RW)
		ereport(ERROR,
				(errmsg("pool_create_passwdent should be called with pool_passwd opened with read/write mode")));

	len = strlen(passwd);
	if (len <= 0)
		ereport(ERROR,
				(errmsg("error updating password, invalid password length:%d", len)));

	rewind(passwd_fd);

	while (!feof(passwd_fd) && !ferror(passwd_fd))
	{
		char       *t = linebuf;
		int         len;

		if (fgets(linebuf, sizeof(linebuf), passwd_fd) == NULL)
			break;

		len = strlen(linebuf);
		if (len == 0)
			continue;

		if (!updated && userMatchesString(t, username))
		{
			len = snprintf(linebuf, sizeof(linebuf), "%s:%s\n", username, passwd);
			updated = true;
		}

		if (writebuf == NULL)
			writebuf = palloc0(len + 1);
		else
		{
			writebuf = repalloc(writebuf, (len + strlen(writebuf) + 1));
		}
		strcat(writebuf, linebuf);
	}

	if (!updated)
	{
		/* password was not update, append this at the end */
		len = snprintf(linebuf, sizeof(linebuf), "%s:%s\n", username, passwd);
		if (writebuf == NULL)
			writebuf = palloc0(len + 1);
		else
		{
			writebuf = repalloc(writebuf, (len + strlen(writebuf) + 1));
		}
		strcat(writebuf, linebuf);
	}

	if(!writebuf)
		return 0;

	fclose(passwd_fd);
	passwd_fd = fopen(saved_passwd_filename, "w+");
	if (!passwd_fd)
	{
		ereport(ERROR,
				(errmsg("pool_passwd reopen failed")));
	}

	/* write pool_passwd file.  */
	fwrite(writebuf, 1, strlen(writebuf), passwd_fd);
	pfree(writebuf);
	return 0;

#undef LINE_LEN
}

/*
 * Get password in pool_passwd by username. Returns NULL if specified
 * entry does not exist or error occurred.
 * Returned password is on the static memory.
 */
char *
pool_get_passwd(char *username)
{
	int			c;
	char		name[MAX_USER_NAME_LEN + 1];
	static char passwd[MAX_POOL_PASSWD_LEN + 1];
	char	   *p;
	int			readlen;

	if (!username)
		ereport(ERROR,
				(errmsg("unable to get password, username is NULL")));

	if (!passwd_fd)
	{
		if (strlen(saved_passwd_filename))
			ereport(ERROR,
				(errmsg("unable to get password, password file descriptor is NULL")));
		else
			return NULL;
	}

	rewind(passwd_fd);
	name[0] = '\0';

	while (!feof(passwd_fd))
	{
		p = name;
		readlen = 0;

		while (readlen < (sizeof(name) - 1))
		{
			c = fgetc(passwd_fd);
			if (c == EOF)
				break;
			else if (c == ':')
				break;

			readlen++;
			*p++ = c;
		}
		*p = '\0';

		if (!strcmp(username, name))
		{
			/* User name found. Return password. */
			p = passwd;
			readlen = 0;

			while ((c = fgetc(passwd_fd)) != EOF &&
				   c != '\n' && readlen < (sizeof(passwd) - 1))
			{
				*p++ = c;
				readlen++;
			}
			*p = '\0';
			return passwd;
		}
		else
		{
			/* Skip password */
			while ((c = fgetc(passwd_fd)) != EOF &&
				   c != '\n')
				;
		}
	}
	return NULL;
}

/*
 * return the next token if the current token matches the
 * user
 */
static char *
getNextToken(char *buf, char **token)
{
#define MAX_TOKEN_LEN 128
	char	   *tbuf,
			   *p;
	bool		bslash = false;
	char		tok[MAX_TOKEN_LEN + 1];
	int			readlen = 0;

	*token = NULL;
	if (buf == NULL)
		return NULL;

	tbuf = buf;
	p = tok;
	while (*tbuf != 0 && readlen < MAX_TOKEN_LEN)
	{
		if (*tbuf == '\\' && !bslash)
		{
			tbuf++;
			bslash = true;
		}

		if (*tbuf == ':' && !bslash)
		{
			*p = '\0';
			if (readlen)
				*token = pstrdup(tok);
			return tbuf + 1;
		}
		/* just copy to the tok */
		bslash = false;
		*p++ = *tbuf++;
		readlen++;
	}
	*p = '\0';
	if (readlen)
		*token = pstrdup(tok);
	return NULL;
}

/*
 * return the next token if the current token matches the
 * user
 */
static char *
userMatchesString(char *buf, char *user)
{
	char	   *tbuf,
			   *ttok;
	bool		bslash = false;

	if (buf == NULL || user == NULL)
		return NULL;
	tbuf = buf;
	ttok = user;
	while (*tbuf != 0)
	{
		if (*tbuf == '\\' && !bslash)
		{
			tbuf++;
			bslash = true;
		}
		if (*tbuf == ':' && *ttok == 0 && !bslash)
			return tbuf + 1;
		bslash = false;
		if (*ttok == 0)
			return NULL;
		if (*tbuf == *ttok)
		{
			tbuf++;
			ttok++;
		}
		else
			return NULL;
	}
	return NULL;
}

/*
 * user:password[:user:password]
 */
PasswordMapping *
pool_get_user_credentials(char *username)
{
	PasswordMapping *pwdMapping = NULL;
	char		buf[1024];

	if (!username)
		ereport(ERROR,
				(errmsg("unable to get password, username is NULL")));
	if (!passwd_fd)
	{
		if (strlen(saved_passwd_filename))
			ereport(WARNING,
				(errmsg("unable to get password, password file descriptor is NULL")));
		return NULL;
	}

	rewind(passwd_fd);

	while (!feof(passwd_fd) && !ferror(passwd_fd))
	{
		char	   *t = buf;
		char	   *tok;
		int			len;

		if (fgets(buf, sizeof(buf), passwd_fd) == NULL)
			break;

		len = strlen(buf);
		if (len == 0)
			continue;

		/* Remove trailing newline */
		if (buf[len - 1] == '\n')
			buf[len - 1] = 0;

		if ((t = userMatchesString(t, username)) == NULL)
			continue;
		/* Get the password */
		t = getNextToken(t, &tok);
		if (tok)
		{
			pwdMapping = palloc0(sizeof(PasswordMapping));
			pwdMapping->pgpoolUser.password = tok;
			pwdMapping->pgpoolUser.passwordType = get_password_type(pwdMapping->pgpoolUser.password);
			pwdMapping->pgpoolUser.userName = (char *) pstrdup(username);
			pwdMapping->mappedUser = false;
		}
		else
			continue;
		/* Get backend user */
		t = getNextToken(t, &tok);
		if (tok)
		{
			/* check if we also have the password */
			char	   *pwd;

			t = getNextToken(t, &pwd);
			if (pwd)
			{
				pwdMapping->backendUser.password = pwd;
				pwdMapping->backendUser.userName = tok;
				pwdMapping->backendUser.passwordType = get_password_type(pwdMapping->backendUser.password);
				pwdMapping->mappedUser = true;
			}
			else
				pfree(tok);
		}
		break;
	}
	return pwdMapping;
}

void
delete_passwordMapping(PasswordMapping * pwdMapping)
{
	if (!pwdMapping)
		return;
	if (pwdMapping->pgpoolUser.password)
		pfree(pwdMapping->pgpoolUser.password);
	if (pwdMapping->pgpoolUser.userName)
		pfree(pwdMapping->pgpoolUser.userName);

	if (pwdMapping->backendUser.password)
		pfree(pwdMapping->backendUser.password);
	if (pwdMapping->backendUser.userName)
		pfree(pwdMapping->backendUser.userName);

	pfree(pwdMapping);
}

/*
 * Delete the entry by username. If specified entry does not exist,
 * does nothing.
 */
void
pool_delete_passwdent(char *username)
{
}

/*
 * Finish this moil. Close pool_passwd.
 */
void
pool_finish_pool_passwd(void)
{
	if (passwd_fd)
	{
		fclose(passwd_fd);
		passwd_fd = NULL;
	}
}

void
pool_reopen_passwd_file(void)
{
	pool_finish_pool_passwd();
	pool_init_pool_passwd(saved_passwd_filename, pool_passwd_mode);
}

/*
 * function first uses the password in the argument, if the argument is empty
 * string or NULL, it looks for the password for uset in pool_passwd file.
 * The returned password is always in plain text or md5 hashed and palloc'd
 * (if not null)
 */
char *
get_pgpool_config_user_password(char *username, char *password_in_config)
{
	PasswordType passwordType = PASSWORD_TYPE_UNKNOWN;
	char	   *password = NULL;
	PasswordMapping *password_mapping = NULL;

	/*
	 * if the password specified in config is empty string or NULL look for the
	 * password in pool_passwd file
	 */
	if (password_in_config == NULL || strlen(password_in_config) == 0)
	{
		password_mapping = pool_get_user_credentials(username);
		if (password_mapping == NULL)
		{
			return NULL;
		}
		passwordType = password_mapping->pgpoolUser.passwordType;
		password = password_mapping->pgpoolUser.password;
	}
	else
	{
		passwordType = get_password_type(password_in_config);
		password = password_in_config;
	}

	if (passwordType == PASSWORD_TYPE_AES)
	{
		/*
		 * decrypt the stored AES password for comparing it
		 */
		password = get_decrypted_password(password);
		if (password == NULL)
		{
			ereport(WARNING,
					(errmsg("could not get the password for user:%s", username),
					 errdetail("unable to decrypt password from pool_passwd"),
					 errhint("verify the valid pool_key exists")));
		}
		else
		{
			delete_passwordMapping(password_mapping);

			/*
			 * the password returned by get_decrypted_password() is already
			 * palloc'd
			 */
			return password;
		}
	}

	if (passwordType == PASSWORD_TYPE_TEXT_PREFIXED)
	{
		/* convert the TEXT prefixed password to plain text password */
		passwordType = PASSWORD_TYPE_PLAINTEXT;
		if (password)
			password = (char*)(password + strlen(PASSWORD_TEXT_PREFIX));
	}

	if (password && strlen(password) && (passwordType != PASSWORD_TYPE_PLAINTEXT &&
										 passwordType != PASSWORD_TYPE_MD5))
	{
		/*
		 * Could not find either plain text or md5 hashed password in
		 * pool_passwd corresponding to the user.  This is normal. Just use
		 * empty password in pgpool.conf.
		 */
		ereport(DEBUG5,
				(errmsg("could not get the password for user:%s", username),
				 errdetail("username \"%s\" has invalid password type: %d", username, passwordType)));
		password = NULL;
	}
	if (password)
		password = (char *) pstrdup(password);

	delete_passwordMapping(password_mapping);

	return password;
}

#ifndef POOL_PRIVATE
char *
get_decrypted_password(const char *shadow_pass)
{
	if (get_password_type(shadow_pass) == PASSWORD_TYPE_AES)
	{
		unsigned char b64_dec[MAX_PGPASS_LEN * 2];
		unsigned char plaintext[MAX_PGPASS_LEN];
		int			len;
		char	   *pwd;
		const char *enc_key = (const char *) get_pool_key();

		if (enc_key == NULL)
			return NULL;

		pwd = (char *) shadow_pass + 3;

		if ((len = strlen(pwd)) == 0)
			return NULL;

		if ((len = pg_b64_decode((const char *) pwd, len, (char *) b64_dec)) == 0)
		{
			ereport(WARNING,
					(errmsg("base64 decoding failed")));
			return NULL;
		}
		if ((len = aes_decrypt_with_password(b64_dec, len,
											 enc_key, plaintext)) <= 0)
		{
			ereport(WARNING,
					(errmsg("decryption failed")));
			return NULL;
		}
		plaintext[len] = 0;
		return pstrdup((const char *) plaintext);
	}
	return NULL;
}
#else
char *
get_decrypted_password(const char *shadow_pass)
{
	ereport(ERROR,
			(errmsg("unable to decrypt password")));
	return NULL;
}
#endif

PasswordType
get_password_type(const char *shadow_pass)
{
	if (strncmp(shadow_pass, PASSWORD_MD5_PREFIX, strlen(PASSWORD_MD5_PREFIX)) == 0)
	{
		if (strlen(shadow_pass) == MD5_PASSWD_LEN + strlen(PASSWORD_MD5_PREFIX))
			return PASSWORD_TYPE_MD5;
		return PASSWORD_TYPE_PLAINTEXT;
	}
	if (strncmp(shadow_pass, PASSWORD_AES_PREFIX, strlen(PASSWORD_AES_PREFIX)) == 0)
		return PASSWORD_TYPE_AES;
	if (strncmp(shadow_pass, PASSWORD_SCRAM_PREFIX, strlen(PASSWORD_SCRAM_PREFIX)) == 0)
		return PASSWORD_TYPE_SCRAM_SHA_256;
	if (strncmp(shadow_pass, PASSWORD_TEXT_PREFIX, strlen(PASSWORD_TEXT_PREFIX)) == 0)
		return PASSWORD_TYPE_TEXT_PREFIXED;

	return PASSWORD_TYPE_PLAINTEXT;
}

/*
 * Get a key from the pgpool key file. return the palloc'd.
 * value
 */
char *
read_pool_key(char *key_file_path)
{
	FILE	   *fp;
	struct stat stat_buf;
	char	   *key = NULL;

#define LINELEN MAX_POOL_KEY_LEN
	char		buf[LINELEN];

	if (strlen(key_file_path) == 0)
		return NULL;

	/*
	 * To prevent file-swapping due to file race conditions,
	 * we open the key file before checking it by stat().
	 */
	/* If password file cannot be opened, ignore it. */
	if ( (fp = fopen(key_file_path, "r")) == NULL)
		return NULL;

	if (fstat(fileno(fp), &stat_buf) != 0)
	{
		ereport(WARNING,
				(errmsg("failed to stat pool key file"),
				 errdetail("fstat failed with reason: \"%m\"")));
		fclose(fp);
		return NULL;
	}

	if (!S_ISREG(stat_buf.st_mode))
	{
		ereport(WARNING,
				(errmsg("pool key file \"%s\" is not a plain file\n", key_file_path)));
		fclose(fp);
		return NULL;
	}

	/* If password file is insecure, alert the user. */
	if (stat_buf.st_mode & (S_IRWXG | S_IRWXO))
	{
		ereport(WARNING,
				(errmsg("pool key file \"%s\" has group or world access; permissions should be u=rw (0600) or less",
						key_file_path)));
		/* do we want to allow unsecure pool key file ? */
		/* fclose(fp); */
		/* return NULL; */
	}

	while (!feof(fp) && !ferror(fp))
	{
		int			len;

		if (fgets(buf, sizeof(buf), fp) == NULL)
			break;

		len = strlen(buf);
		if (len == 0)
			continue;

		/* Remove trailing newline */
		if (buf[len - 1] == '\n')
			buf[len - 1] = 0;
		key = pstrdup(buf);
		break;
	}

	fclose(fp);
	return key;

#undef LINELEN
}

/*
 * Check password type is md5 hashed or not. recovery_password and
 * wd_lifecheck_password are not allowed to be md5 hashed format.
 * The kind of returns of this function is follow;
 *  0: password is not md5 hashed
 * -1: password is md5 hashed
 * -2: password is not found
 */
int
chceck_password_type_is_not_md5(char *username, char *password_in_config)
{
	PasswordType passwordType = PASSWORD_TYPE_UNKNOWN;

	/*
	 * if the password specified in config is empty string or NULL look for the
	 * password in pool_passwd file
	 */
	if (password_in_config == NULL || strlen(password_in_config) == 0)
	{
		PasswordMapping *password_mapping = NULL;
		password_mapping = pool_get_user_credentials(username);
		if (password_mapping == NULL)
		{
			return -2;
		}
		passwordType = password_mapping->pgpoolUser.passwordType;
		delete_passwordMapping(password_mapping);
	}
	else
	{
		passwordType = get_password_type(password_in_config);
	}

	/* if the password type is MD5 hash return -1*/
	if (passwordType == PASSWORD_TYPE_MD5)
	{
		return -1;
	}

	return 0;
}
