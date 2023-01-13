/* -*-pgsql-c-*- */
/*
 * $Header$
 *
 * pgpool: a language independent connection pool server for PostgreSQL
 * written by Tatsuo Ishii
 *
 * Copyright (c) 2003-2022	PgPool Global Development Group
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
 * pg_enc command main
 *
 */
#include "pool.h"
#include "pool_config.h"
#include "auth/pool_passwd.h"
#include "utils/ssl_utils.h"
#include "utils/pool_path.h"
#include "utils/base64.h"
#include "version.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <termios.h>
#ifdef HAVE_GETOPT_H
#include <getopt.h>
#else
#include "utils/getopt_long.h"
#endif
#include <pwd.h>
#include <libgen.h>

#define MAX_ENCODED_PASSWD_LEN (MAX_POOL_KEY_LEN * 2)

static void print_usage(const char prog[], int exit_code);
static void set_tio_attr(int enable);
static void print_encrypted_password(char *pg_pass, char *pool_key);
static void update_pool_passwd(char *conf_file, char *username, char *password, char *key);
static void process_input_file(char *conf_file, char *input_file, char *key, bool updatepasswd);
static bool get_pool_key_filename(char *poolKeyFile);

int
main(int argc, char *argv[])
{
#define PRINT_USAGE(exit_code)	print_usage(argv[0], exit_code)

	char		conf_file[POOLMAXPATHLEN + 1];
	char		input_file[POOLMAXPATHLEN + 1];
	char		enc_key[MAX_POOL_KEY_LEN + 1];
	char		pg_pass[MAX_PGPASS_LEN + 1];
	char		username[MAX_USER_NAME_LEN + 1];
	char		key_file_path[POOLMAXPATHLEN + sizeof(POOLKEYFILE) + 1];
	int			opt;
	int			optindex;
	bool		updatepasswd = false;
	bool		prompt = false;
	bool		prompt_for_key = false;
	bool		use_input_file = false;
	char	   *pool_key = NULL;

	static struct option long_options[] = {
		{"help", no_argument, NULL, 'h'},
		{"prompt", no_argument, NULL, 'p'},
		{"prompt-for-key", no_argument, NULL, 'P'},
		{"update-pass", no_argument, NULL, 'm'},
		{"username", required_argument, NULL, 'u'},
		{"enc-key", required_argument, NULL, 'K'},
		{"key-file", required_argument, NULL, 'k'},
		{"config-file", required_argument, NULL, 'f'},
		{"input-file", required_argument, NULL, 'i'},
		{NULL, 0, NULL, 0}
	};

	snprintf(conf_file, sizeof(conf_file), "%s/%s", DEFAULT_CONFIGDIR, POOL_CONF_FILE_NAME);

	/*
	 * initialize username buffer with zeros so that we can use strlen on it
	 * later to check if a username was given on the command line
	 */
	memset(username, 0, sizeof(username));
	memset(enc_key, 0, sizeof(enc_key));
	memset(key_file_path, 0, sizeof(key_file_path));

	while ((opt = getopt_long(argc, argv, "hPpmf:i:u:k:K:", long_options, &optindex)) != -1)
	{
		switch (opt)
		{
			case 'p':			/* prompt for postgres password */
				prompt = true;
				break;

			case 'P':			/* prompt for encryption key */
				prompt_for_key = true;
				break;

			case 'm':			/* update password file */
				updatepasswd = true;
				break;

			case 'f':			/* specify configuration file */
				if (!optarg)
				{
					PRINT_USAGE(EXIT_SUCCESS);
				}
				strlcpy(conf_file, optarg, sizeof(conf_file));
				break;

			case 'i':			/* specify users password input file */
				if (!optarg)
				{
					PRINT_USAGE(EXIT_SUCCESS);
				}
				strlcpy(input_file, optarg, sizeof(input_file));
				use_input_file = true;
				break;

			case 'k':			/* specify key file for encrypting
								 * pool_password entries */
				if (!optarg)
				{
					PRINT_USAGE(EXIT_SUCCESS);
				}
				strlcpy(key_file_path, optarg, sizeof(key_file_path));
				break;

			case 'K':			/* specify configuration file */
				if (!optarg)
				{
					PRINT_USAGE(EXIT_SUCCESS);
				}
				strlcpy(enc_key, optarg, sizeof(enc_key));
				break;

			case 'u':
				if (!optarg)
				{
					PRINT_USAGE(EXIT_SUCCESS);
				}
				/* check the input limit early */
				if (strlen(optarg) > sizeof(username))
				{
					fprintf(stderr, "Error: input exceeds maximum username length!\n\n");
					exit(EXIT_FAILURE);
				}
				strlcpy(username, optarg, sizeof(username));
				break;

			default:
				PRINT_USAGE(EXIT_SUCCESS);
				break;
		}
	}

	if (!use_input_file)
	{
		/* Prompt for password. */
		if (prompt || optind >= argc)
		{
			char		buf[MAX_PGPASS_LEN];
			int			len;

			set_tio_attr(1);
			printf("db password: ");
			if (!fgets(buf, sizeof(buf), stdin))
			{
				int			eno = errno;

				fprintf(stderr, "Couldn't read input from stdin. (fgets(): %s)",
						strerror(eno));

				exit(EXIT_FAILURE);
			}
			printf("\n");
			set_tio_attr(0);

			/* Remove LF at the end of line, if there is any. */
			len = strlen(buf);
			if (len > 0 && buf[len - 1] == '\n')
			{
				buf[len - 1] = '\0';
				len--;
			}
			stpncpy(pg_pass, buf, sizeof(pg_pass));
		}

		/* Read password from argv. */
		else
		{
			int			len;

			len = strlen(argv[optind]);

			if (len > MAX_PGPASS_LEN)
			{
				fprintf(stderr, "Error: Input exceeds maximum password length given:%d max allowed:%d!\n\n", len, MAX_PGPASS_LEN);
				PRINT_USAGE(EXIT_FAILURE);
			}

			stpncpy(pg_pass, argv[optind], sizeof(pg_pass));
		}
	}

	/* prompt for key, overrides all key related arguments */
	if (prompt_for_key)
	{
		char		buf[MAX_POOL_KEY_LEN];
		int			len;

		/* we need to read the encryption key from stdin */
		set_tio_attr(1);
		printf("encryption key: ");
		if (!fgets(buf, sizeof(buf), stdin))
		{
			int			eno = errno;

			fprintf(stderr, "Couldn't read input from stdin. (fgets(): %s)",
					strerror(eno));

			exit(EXIT_FAILURE);
		}
		printf("\n");
		set_tio_attr(0);
		/* Remove LF at the end of line, if there is any. */
		len = strlen(buf);
		if (len > 0 && buf[len - 1] == '\n')
		{
			buf[len - 1] = '\0';
			len--;
		}
		if (len == 0)
		{
			fprintf(stderr, "encryption key not provided\n");
			exit(EXIT_FAILURE);
		}
		stpncpy(enc_key, buf, sizeof(enc_key));
	}
	else
	{
		/* check if we already have not got the key from command line argument */
		if (strlen(enc_key) == 0)
		{
			/* read from file */
			if (strlen(key_file_path) == 0)
			{
				get_pool_key_filename(key_file_path);
			}

			fprintf(stdout, "trying to read key from file %s\n", key_file_path);

			pool_key = read_pool_key(key_file_path);
		}
		else
		{
			pool_key = enc_key;
		}
	}

	if (pool_key == NULL)
	{
		fprintf(stderr, "encryption key not provided\n");
		exit(EXIT_FAILURE);
	}

	/* Use input file */
	if (use_input_file)
	{
		process_input_file(conf_file, input_file, pool_key, updatepasswd);
		return EXIT_SUCCESS;
	}

	if (updatepasswd)
		update_pool_passwd(conf_file, username, pg_pass, pool_key);
	else
		print_encrypted_password(pg_pass, pool_key);

	if (pool_key != enc_key)
		free(pool_key);

	return EXIT_SUCCESS;
}

static void
print_encrypted_password(char *pg_pass, char *pool_key)
{
		unsigned char ciphertext[MAX_ENCODED_PASSWD_LEN];
		unsigned char b64_enc[MAX_ENCODED_PASSWD_LEN];
		int			len;
		int			cypher_len;

		cypher_len = aes_encrypt_with_password((unsigned char *) pg_pass,
											   strlen(pg_pass), pool_key, ciphertext);

		/* generate the hash for the given username */
		len = pg_b64_encode((const char *) ciphertext, cypher_len, (char *) b64_enc);
		b64_enc[len] = 0;
		fprintf(stdout, "\n%s\n", b64_enc);
		fprintf(stdout, "pool_passwd string: AES%s\n", b64_enc);

#ifdef DEBUG_ENCODING
		unsigned char b64_dec[MAX_ENCODED_PASSWD_LEN];
		unsigned char plaintext[MAX_PGPASS_LEN];

		len = pg_b64_decode(b64_enc, len, b64_dec);
		len = aes_decrypt_with_password(b64_dec, len,
										pool_key, plaintext);
		plaintext[len] = 0;
#endif
}

static void
process_input_file(char *conf_file, char *input_file, char *key, bool updatepasswd)
{
	FILE	*input_file_fd;
	char	*buf = NULL;
	char	username[MAX_USER_NAME_LEN + 1];
	char	password[MAX_PGPASS_LEN + 1];
	char	*pch;
	size_t	len=0;
	int 	nread = 0;
	int		line_count;

	fprintf(stdout, "trying to read username:password pairs from file %s\n", input_file);
	input_file_fd = fopen(input_file, "r");
	if (input_file_fd == NULL)
	{
		fprintf(stderr, "failed to open input_file \"%s\" (%m)\n\n", input_file);
		exit(EXIT_FAILURE);
	}

	line_count = 0;
	while ((nread = getline(&buf, &len, input_file_fd)) != -1)
	{
		line_count++;
		/* Remove trailing newline */
		if (buf[nread - 1] == '\n')
			buf[nread-- - 1] = '\0';

		memset(username, 0, MAX_USER_NAME_LEN + 1);
		memset(password, 0, MAX_PGPASS_LEN + 1);

		/* Check blank line */
		if (nread == 0 || *buf == '\n')
			goto clear_buffer;

		/* Split username and passwords */
		pch = buf;
		while( pch && pch != buf + nread && *pch != ':')
			pch++;
		if (*pch == ':')
			pch++;
		if (!strlen(pch))
		{
			fprintf(stderr, "LINE#%02d: invalid username:password pair\n", line_count);
			goto clear_buffer;
		}

		if( (pch-buf) > sizeof(username))
		{
			fprintf(stderr, "LINE#%02d: input exceeds maximum username length %d\n",line_count,  MAX_USER_NAME_LEN);
			goto clear_buffer;
		}
		strncpy(username, buf, pch - buf - 1);

		if (strlen(pch) >= sizeof(password))
		{
			fprintf(stderr, "LINE#%02d: input exceeds maximum password length %d\n",line_count, MAX_PGPASS_LEN);
			goto clear_buffer;
		}
		strncpy(password, pch, sizeof(password) -1);

		if (updatepasswd)
			update_pool_passwd(conf_file, username, password, key);
		else
			print_encrypted_password(password, key);
clear_buffer:
		free(buf);
		buf = NULL;
		len = 0;
	}

	fclose(input_file_fd);
}

static void
update_pool_passwd(char *conf_file, char *username, char *password, char *key)
{
	struct passwd *pw;
	char		pool_passwd[MAX_PGPASS_LEN + 1];
	char		dirnamebuf[POOLMAXPATHLEN + 1];
	char	   *dirp;
	char	   *user = username;

	unsigned char ciphertext[MAX_ENCODED_PASSWD_LEN];
	unsigned char b64_enc[MAX_ENCODED_PASSWD_LEN];
	int			len;

	if (pool_init_config())
	{
		fprintf(stderr, "pool_init_config() failed\n\n");
		exit(EXIT_FAILURE);
	}
	if (pool_get_config(conf_file, CFGCXT_RELOAD) == false)
	{
		fprintf(stderr, "Unable to get configuration. Exiting...\n\n");
		exit(EXIT_FAILURE);
	}

	strlcpy(dirnamebuf, conf_file, sizeof(dirnamebuf));
	dirp = dirname(dirnamebuf);
	snprintf(pool_passwd, sizeof(pool_passwd), "%s/%s",
			 dirp, pool_config->pool_passwd);
	pool_init_pool_passwd(pool_passwd, POOL_PASSWD_RW);

	if (username == NULL || strlen(username) == 0)
	{
		/* get the user information from the current uid */
		pw = getpwuid(getuid());
		if (!pw)
		{
			fprintf(stderr, "getpwuid() failed\n\n");
			exit(EXIT_FAILURE);
		}
		user = pw->pw_name;
	}

	/* generate the hash for the given username */
	int			cypher_len = aes_encrypt_with_password((unsigned char *) password, strlen(password), key, ciphertext);

	if (cypher_len <= 0)
	{
		fprintf(stderr, "password encryption failed\n\n");
		exit(EXIT_FAILURE);
	}

	/* copy the prefix at the start of string */
	strcpy((char *) b64_enc, (char *) PASSWORD_AES_PREFIX);
	len = pg_b64_encode((const char *) ciphertext, cypher_len, (char *) b64_enc + strlen(PASSWORD_AES_PREFIX));
	if (cypher_len <= 0)
	{
		fprintf(stderr, "base64 encoding failed\n\n");
		exit(EXIT_FAILURE);
	}
	len += strlen(PASSWORD_AES_PREFIX);
	b64_enc[len] = 0;

	pool_create_passwdent(user, (char *) b64_enc);
	pool_finish_pool_passwd();
}

static void
print_usage(const char prog[], int exit_code)
{
	char		homedir[POOLMAXPATHLEN];
	FILE	   *stream = (exit_code == EXIT_SUCCESS) ? stdout : stderr;

	if (!get_home_directory(homedir, sizeof(homedir)))
		strncpy(homedir, "USER-HOME-DIR", POOLMAXPATHLEN);

	fprintf(stream, "%s version %s (%s),\n", PACKAGE, VERSION, PGPOOLVERSION);
	fprintf(stream, "  password encryption utility for Pgpool\n\n");
	fprintf(stream, "Usage:\n");
	fprintf(stream, "  %s [OPTIONS] <PASSWORD>\n", prog);
	fprintf(stream, "  -k, --key-file=KEY_FILE\n");
	fprintf(stream, "                       Set the path to the encryption key file.\n");
	fprintf(stream, "                       Default: %s/%s\n", homedir, POOLKEYFILE);
	fprintf(stream, "                       Can be overridden by the %s environment variable.\n", POOLKEYFILEENV);
	fprintf(stream, "  -K, --enc-key=ENCRYPTION_KEY\n");
	fprintf(stream, "                       Encryption key to be used for encrypting database passwords.\n");
	fprintf(stream, "  -f, --config-file=CONFIG_FILE\n");
	fprintf(stream, "                       Specifies the pgpool.conf file.\n");
	fprintf(stream, "  -i, --input-file=INPUT-FILE\n");
	fprintf(stream, "                       Specify file containing username and password pairs.\n");
	fprintf(stream, "  -p, --prompt         Prompt for database password using standard input.\n");
	fprintf(stream, "  -P, --prompt-for-key Prompt for encryption key using standard input.\n");
	fprintf(stream, "  -m, --update-pass    Create encrypted password entry in the pool_passwd file.\n");
	fprintf(stream, "  -u, --username       The username for the pool_password entry.\n");
	fprintf(stream, "  -h, --help           Print this help.\n\n");

	exit(exit_code);
}


static void
set_tio_attr(int set)
{
	struct termios tio;
	static struct termios tio_save;


	if (!isatty(0))
	{
		fprintf(stderr, "stdin is not tty\n");
		exit(EXIT_FAILURE);
	}

	if (set)
	{
		if (tcgetattr(0, &tio) < 0)
		{
			fprintf(stderr, "set_tio_attr(set): tcgetattr failed\n");
			exit(EXIT_FAILURE);
		}

		tio_save = tio;

		tio.c_iflag &= ~(BRKINT | ISTRIP | IXON);
		tio.c_lflag &= ~(ICANON | IEXTEN | ECHO | ECHOE | ECHOK | ECHONL);
		tio.c_cc[VMIN] = 1;
		tio.c_cc[VTIME] = 0;

		if (tcsetattr(0, TCSANOW, &tio) < 0)
		{
			fprintf(stderr, "(set_tio_attr(set): tcsetattr failed\n");
			exit(EXIT_FAILURE);
		}
	}
	else
	{
		if (tcsetattr(0, TCSANOW, &tio_save) < 0)
		{
			fprintf(stderr, "set_tio_attr(reset): tcsetattr failed\n");
			exit(EXIT_FAILURE);
		}
	}
}

static bool
get_pool_key_filename(char *poolKeyFile)
{
	char	   *passfile_env;

	if ((passfile_env = getenv(POOLKEYFILEENV)) != NULL)
	{
		/* use the literal path from the environment, if set */
		strlcpy(poolKeyFile, passfile_env, POOLMAXPATHLEN);
	}
	else
	{
		char		homedir[POOLMAXPATHLEN];

		if (!get_home_directory(homedir, sizeof(homedir)))
			return false;
		snprintf(poolKeyFile, POOLMAXPATHLEN + sizeof(POOLKEYFILE) + 1, "%s/%s", homedir, POOLKEYFILE);
	}
	return true;
}
