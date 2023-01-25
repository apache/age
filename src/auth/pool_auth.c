/* -*-pgsql-c-*- */
/*
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
 * pool_auth.c: authentication stuff
 *
*/

#include "pool.h"
#include <unistd.h>
#include "context/pool_session_context.h"
#include "protocol/pool_process_query.h"
#include "protocol/pool_proto_modules.h"
#include "utils/pool_stream.h"
#include "pool_config.h"
#include "auth/pool_auth.h"
#include "auth/pool_hba.h"
#include "auth/pool_passwd.h"
#include "auth/scram.h"
#include "utils/elog.h"
#include "utils/palloc.h"
#include "utils/memutils.h"
#include "auth/md5.h"
#include <unistd.h>

#ifdef HAVE_CRYPT_H
#include <crypt.h>
#endif
#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#ifdef HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif
#ifdef HAVE_PARAM_H
#include <param.h>
#endif
#ifdef USE_SSL
#include <openssl/rand.h>
#endif

#include <errno.h>
#include <string.h>
#include <stdlib.h>

#define AUTHFAIL_ERRORCODE "28000"
#define MAX_SASL_PAYLOAD_LEN 1024


static POOL_STATUS pool_send_backend_key_data(POOL_CONNECTION * frontend, int pid, int key, int protoMajor);
static int	do_clear_text_password(POOL_CONNECTION * backend, POOL_CONNECTION * frontend, int reauth, int protoMajor);
static void pool_send_auth_fail(POOL_CONNECTION * frontend, POOL_CONNECTION_POOL * cp);
static int	do_crypt(POOL_CONNECTION * backend, POOL_CONNECTION * frontend, int reauth, int protoMajor);
static int do_md5(POOL_CONNECTION * backend, POOL_CONNECTION * frontend, int reauth, int protoMajor,
	   char *storedPassword, PasswordType passwordType);
static int	do_md5_single_backend(POOL_CONNECTION * backend, POOL_CONNECTION * frontend, int reauth, int protoMajor);
static void send_md5auth_request(POOL_CONNECTION * frontend, int protoMajor, char *salt);
static int	read_password_packet(POOL_CONNECTION * frontend, int protoMajor, char *password, int *pwdSize);
static int	send_password_packet(POOL_CONNECTION * backend, int protoMajor, char *password);
static int	send_auth_ok(POOL_CONNECTION * frontend, int protoMajor);
static void sendAuthRequest(POOL_CONNECTION * frontend, int protoMajor, int32 auth_req_type, char *extradata, int extralen);
static long PostmasterRandom(void);

static int	pg_SASL_continue(POOL_CONNECTION * backend, char *payload, int payloadlen, void *sasl_state, bool final);
static void *pg_SASL_init(POOL_CONNECTION * backend, char *payload, int payloadlen, char *username, char *storedPassword);
static bool do_SCRAM(POOL_CONNECTION * frontend, POOL_CONNECTION * backend, int protoMajor, int message_length,
		 char *username, char *storedPassword, PasswordType passwordType);
static void authenticate_frontend_md5(POOL_CONNECTION * backend, POOL_CONNECTION * frontend, int reauth, int protoMajor);
static void authenticate_frontend_cert(POOL_CONNECTION * frontend);
static void authenticate_frontend_SCRAM(POOL_CONNECTION * backend, POOL_CONNECTION * frontend, int reauth);
static void authenticate_frontend_clear_text(POOL_CONNECTION * frontend);
static bool get_auth_password(POOL_CONNECTION * backend, POOL_CONNECTION * frontend, int reauth,
				  char **password, PasswordType *passwordType);

/*
 * Do authentication. Assuming the only caller is
 * make_persistent_db_connection().
 */
void
connection_do_auth(POOL_CONNECTION_POOL_SLOT * cp, char *password)
{
	char		kind;
	int			length;
	int			auth_kind;
	char		state;
	char	   *p;
	int			pid,
				key;
	bool		keydata_done;

	/*
	 * read kind expecting 'R' packet (authentication response)
	 */
	pool_read_with_error(cp->con, &kind, sizeof(kind),
						 "authentication message response type");

	if (kind != 'R')
	{
		char	   *msg;
		int			sts = 0;

		if (kind == 'E' || kind == 'N')
		{
			sts = pool_extract_error_message(false, cp->con, cp->sp->major, false, &msg);
		}

		if (sts == 1)			/* succeeded in extracting error/notice
								 * message */
		{
			ereport(ERROR,
					(errmsg("failed to authenticate"),
					 errdetail("%s", msg)));
			pfree(msg);
		}
		ereport(ERROR,
				(errmsg("failed to authenticate"),
				 errdetail("invalid authentication message response type, Expecting 'R' and received '%c'", kind)));
	}

	/* read message length */
	pool_read_with_error(cp->con, &length, sizeof(length),
						 "authentication message response length");

	length = ntohl(length);

	/* read auth kind */
	pool_read_with_error(cp->con, &auth_kind, sizeof(auth_kind),
						 "authentication method from response");
	auth_kind = ntohl(auth_kind);
	ereport(DEBUG1,
			(errmsg("authenticate kind = %d", auth_kind)));

	if (auth_kind == AUTH_REQ_OK)	/* trust authentication? */
	{
		cp->con->auth_kind = AUTH_REQ_OK;
	}
	else if (auth_kind == AUTH_REQ_PASSWORD)	/* clear text password? */
	{
		kind = send_password_packet(cp->con, PROTO_MAJOR_V3, password);
		if (kind != AUTH_REQ_OK)
			ereport(ERROR,
					(errmsg("password authentication failed for user:%s", cp->sp->user),
					 errdetail("backend replied with invalid kind")));

		cp->con->auth_kind = AUTH_REQ_OK;
	}
	else if (auth_kind == AUTH_REQ_CRYPT)	/* crypt password? */
	{
		char		salt[3];
		char	   *crypt_password;

		pool_read_with_error(cp->con, &salt, 2, "crypt salt");
		salt[2] = '\0';

		crypt_password = crypt(password, salt);
		if (crypt_password == NULL)
			ereport(ERROR,
					(errmsg("crypt authentication failed for user:%s", cp->sp->user),
					 errdetail("failed to encrypt the password")));

		/* Send password packet to backend and receive auth response */
		kind = send_password_packet(cp->con, PROTO_MAJOR_V3, crypt_password);
		if (kind != AUTH_REQ_OK)
			ereport(ERROR,
					(errmsg("crypt authentication failed for user:%s", cp->sp->user),
					 errdetail("backend replied with invalid kind")));

		cp->con->auth_kind = AUTH_REQ_OK;
	}
	else if (auth_kind == AUTH_REQ_MD5) /* md5 password? */
	{
		char		salt[4];
		char	   *buf,
				   *buf1;

		pool_read_with_error(cp->con, &salt, sizeof(salt), "authentication md5 salt");

		buf = palloc0(2 * (MD5_PASSWD_LEN + 4));	/* hash + "md5" + '\0' */

		/* set buffer address for building md5 password */
		buf1 = buf + MD5_PASSWD_LEN + 4;

		/*
		 * If the supplied password is already in md5 hash format, we just
		 * copy it. Otherwise calculate the md5 hash value.
		 */
		if (!strncmp("md5", password, 3) && (strlen(password) - 3) == MD5_PASSWD_LEN)
			memcpy(buf1, password + 3, MD5_PASSWD_LEN + 1);
		else
			pool_md5_encrypt(password, cp->sp->user, strlen(cp->sp->user), buf1);

		pool_md5_encrypt(buf1, salt, 4, buf + 3);
		memcpy(buf, "md5", 3);

		/* Send password packet to backend and receive auth response */
		kind = send_password_packet(cp->con, PROTO_MAJOR_V3, buf);
		pfree(buf);
		if (kind != AUTH_REQ_OK)
			ereport(ERROR,
					(errmsg("md5 authentication failed for user:%s", cp->sp->user),
					 errdetail("backend replied with invalid kind")));

		cp->con->auth_kind = AUTH_REQ_OK;
	}
	else if (auth_kind == AUTH_REQ_SASL)
	{
		if (do_SCRAM(NULL, cp->con, PROTO_MAJOR_V3, length, cp->sp->user, password, PASSWORD_TYPE_PLAINTEXT) == false)
		{
			ereport(ERROR,
					(errmsg("failed to authenticate with backend"),
					 errdetail("SCRAM authentication failed for user:%s", cp->sp->user)));
		}
		ereport(DEBUG1,
				(errmsg("SCRAM authentication successful for user:%s", cp->sp->user)));
		cp->con->auth_kind = AUTH_REQ_OK;
	}
	else
	{
		ereport(ERROR,
				(errmsg("failed to authenticate"),
				 errdetail("auth kind %d is not yet supported", auth_kind)));
	}

	/*
	 * Read backend key data and wait until Ready for query arriving or error
	 * happens.
	 */

	keydata_done = false;

	for (;;)
	{
		pool_read_with_error(cp->con, &kind, sizeof(kind),
							 "authentication message kind");

		switch (kind)
		{
			case 'K':			/* backend key data */
				keydata_done = true;
				ereport(DEBUG1,
						(errmsg("authenticate backend: key data received")));

				/* read message length */
				pool_read_with_error(cp->con, &length, sizeof(length), "message length for authentication kind 'K'");
				if (ntohl(length) != 12)
				{
					ereport(ERROR,
							(errmsg("failed to authenticate"),
							 errdetail("invalid backend key data length. received %d bytes when expecting 12 bytes"
									   ,ntohl(length))));
				}

				/* read pid */
				pool_read_with_error(cp->con, &pid, sizeof(pid), "pid for authentication kind 'K'");
				cp->pid = pid;

				/* read key */
				pool_read_with_error(cp->con, &key, sizeof(key),
									 "key for authentication kind 'K'");
				cp->key = key;
				break;

			case 'Z':			/* Ready for query */
				/* read message length */
				pool_read_with_error(cp->con, &length, sizeof(length),
									 "message length for authentication kind 'Z'");
				length = ntohl(length);

				/* read transaction state */
				pool_read_with_error(cp->con, &state, sizeof(state),
									 "transaction state  for authentication kind 'Z'");

				ereport(DEBUG1,
						(errmsg("authenticate backend: transaction state: %c", state)));

				cp->con->tstate = state;

				if (!keydata_done)
				{
					ereport(ERROR,
							(errmsg("failed to authenticate"),
							 errdetail("ready for query arrived before receiving keydata")));
				}
				return;
				break;

			case 'S':			/* parameter status */
			case 'N':			/* notice response */
			case 'E':			/* error response */
				/* Just throw away data */
				pool_read_with_error(cp->con, &length, sizeof(length),
									 "backend message length");

				length = ntohl(length);
				length -= 4;

				p = pool_read2(cp->con, length);
				if (p == NULL)
					ereport(ERROR,
							(errmsg("failed to authenticate"),
							 errdetail("unable to read data from socket")));

				break;

			default:
				ereport(ERROR,
						(errmsg("failed to authenticate"),
						 errdetail("unknown authentication message response received '%c'", kind)));
				break;
		}
	}
	ereport(ERROR,
			(errmsg("failed to authenticate")));
}

/*
 * After sending the start up packet to the backend, do the
 * authentication against backend. if success return 0 otherwise non
 * 0.
*/
int
pool_do_auth(POOL_CONNECTION * frontend, POOL_CONNECTION_POOL * cp)
{
	signed char kind;
	int			pid;
	int			key;
	int			protoMajor;
	int			length;
	int			authkind;
	int			i;
	int			message_length = 0;
	StartupPacket *sp;


	protoMajor = MAIN_CONNECTION(cp)->sp->major;

	kind = pool_read_kind(cp);
	if (kind < 0)
		ereport(ERROR,
				(errmsg("invalid authentication packet from backend"),
				 errdetail("failed to get response kind")));

	/* error response? */
	if (kind == 'E')
	{
		/*
		 * we assume error response at this stage is likely version protocol
		 * mismatch (v3 frontend vs. v2 backend). So we throw a V2 protocol
		 * error response in the hope that v3 frontend will negotiate again
		 * using v2 protocol.
		 */

		ErrorResponse(frontend, cp);

		ereport(ERROR,
				(errmsg("backend authentication failed"),
				 errdetail("backend response with kind \'E\' when expecting \'R\'"),
				 errhint("This issue can be caused by version mismatch (current version %d)", protoMajor)));
	}
	else if (kind != 'R')
		ereport(ERROR,
				(errmsg("backend authentication failed"),
				 errdetail("backend response with kind \'%c\' when expecting \'R\'", kind)));

	/*
	 * message length (v3 only)
	 */
	if (protoMajor == PROTO_MAJOR_V3)
	{
		message_length = pool_read_message_length(cp);
		if (message_length <= 0)
			ereport(ERROR,
					(errmsg("invalid authentication packet from backend"),
					 errdetail("failed to get the authentication packet length"),
					 errhint("This is likely caused by the inconsistency of auth method among DB nodes. \
								Please check the previous error messages (hint: length field) \
								from pool_read_message_length and recheck the pg_hba.conf settings.")));
	}

	/*-------------------------------------------------------------------------
	 * read authentication request kind.
	 *
	 * 0: authentication ok
	 * 1: kerberos v4
	 * 2: kerberos v5
	 * 3: clear text password
	 * 4: crypt password
	 * 5: md5 password
	 * 6: scm credential
	 *
	 * in replication mode, we only support  kind = 0, 3. this is because "salt"
	 * cannot be replicated.
	 * in non replication mode, we support kind = 0, 3, 4, 5
	 *-------------------------------------------------------------------------
	 */

	authkind = pool_read_int(cp);
	if (authkind < 0)
		ereport(ERROR,
				(errmsg("invalid authentication packet from backend"),
				 errdetail("failed to get auth kind")));

	authkind = ntohl(authkind);

	ereport(DEBUG1,
			(errmsg("authentication backend"),
			 errdetail("auth kind:%d", authkind)));
	/* trust? */
	if (authkind == AUTH_REQ_OK)
	{
		int			msglen;

		pool_write(frontend, "R", 1);

		if (protoMajor == PROTO_MAJOR_V3)
		{
			msglen = htonl(8);
			pool_write(frontend, &msglen, sizeof(msglen));
		}

		msglen = htonl(0);
		pool_write_and_flush(frontend, &msglen, sizeof(msglen));
		MAIN(cp)->auth_kind = AUTH_REQ_OK;
	}

	/* clear text password authentication? */
	else if (authkind == AUTH_REQ_PASSWORD)
	{
		for (i = 0; i < NUM_BACKENDS; i++)
		{
			if (!VALID_BACKEND(i))
				continue;

			ereport(DEBUG1,
					(errmsg("authentication backend"),
					 errdetail("trying clear text password authentication")));

			authkind = do_clear_text_password(CONNECTION(cp, i), frontend, 0, protoMajor);

			if (authkind < 0)
			{
				ereport(DEBUG1,
						(errmsg("authentication backend"),
						 errdetail("clear text password failed in slot %d", i)));
				pool_send_auth_fail(frontend, cp);
				ereport(ERROR,
						(errmsg("failed to authenticate with backend"),
						 errdetail("do_clear_text_password failed in slot %d", i)));
			}
		}
	}

	/* crypt authentication? */
	else if (authkind == AUTH_REQ_CRYPT)
	{
		for (i = 0; i < NUM_BACKENDS; i++)
		{
			if (!VALID_BACKEND(i))
				continue;

			ereport(DEBUG1,
					(errmsg("authentication backend"),
					 errdetail("trying crypt authentication")));

			authkind = do_crypt(CONNECTION(cp, i), frontend, 0, protoMajor);

			if (authkind < 0)
			{
				pool_send_auth_fail(frontend, cp);
				ereport(ERROR,
						(errmsg("failed to authenticate with backend"),
						 errdetail("do_crypt_text_password failed in slot %d", i)));
			}
		}
	}

	/* md5 authentication? */
	else if (authkind == AUTH_REQ_MD5)
	{
		char	   *password = NULL;
		PasswordType passwordType = PASSWORD_TYPE_UNKNOWN;

		/*
		 * check if we can use md5 authentication.
		 */
		if (!RAW_MODE && NUM_BACKENDS > 1)
		{
			if (get_auth_password(MAIN(cp), frontend, 0,
								  &password, &passwordType) == false)
			{
				/*
				 * We do not have any password, we can still get the password
				 * from client using plain text authentication if it is
				 * allowed by user
				 */
				if (frontend->pool_hba == NULL && pool_config->allow_clear_text_frontend_auth)
				{
					ereport(LOG,
							(errmsg("using clear text authentication with frontend"),
							 errdetail("backend will still use md5 auth"),
							 errhint("you can disable this behavior by setting allow_clear_text_frontend_auth to off")));
					authenticate_frontend_clear_text(frontend);
					/* now check again if we have a password now */
					if (get_auth_password(MAIN(cp), frontend, 0,
										  &password, &passwordType) == false)
					{
						ereport(ERROR,
								(errmsg("failed to authenticate with backend using md5"),
								 errdetail("unable to get the password")));
					}
				}
			}
			/* we have a password to use, validate the password type */
			if (passwordType != PASSWORD_TYPE_PLAINTEXT && passwordType != PASSWORD_TYPE_MD5
				&& passwordType != PASSWORD_TYPE_AES)
			{
				ereport(ERROR,
						(errmsg("failed to authenticate with backend using md5"),
						 errdetail("valid password not found")));
			}
		}

		for (i = 0; i < NUM_BACKENDS; i++)
		{
			if (!VALID_BACKEND(i))
				continue;

			ereport(DEBUG1,
					(errmsg("authentication backend: %d", i),
					 errdetail("trying md5 authentication")));

			authkind = do_md5(CONNECTION(cp, i), frontend, 0, protoMajor, password, passwordType);

			if (authkind < 0)
			{
				pool_send_auth_fail(frontend, cp);
				ereport(ERROR,
						(errmsg("failed to authenticate with backend"),
						 errdetail("MD5 authentication failed in slot [%d].", i)));
			}
		}
	}

	/* SCRAM authentication? */
	else if (authkind == AUTH_REQ_SASL)
	{
		char	   *password;
		PasswordType passwordType = PASSWORD_TYPE_UNKNOWN;

		if (get_auth_password(MAIN(cp), frontend, 0,
							  &password, &passwordType) == false)
		{
			/*
			 * We do not have any password, we can still get the password from
			 * client using plain text authentication if it is allowed by user
			 */
			if (frontend->pool_hba == NULL && pool_config->allow_clear_text_frontend_auth)
			{
				ereport(LOG,
						(errmsg("using clear text authentication with frontend"),
						 errdetail("backend will still use SCRAM auth"),
						 errhint("you can disable this behavior by setting allow_clear_text_frontend_auth to off")));
				authenticate_frontend_clear_text(frontend);
				/* now check again if we have a password now */
				if (get_auth_password(MAIN(cp), frontend, 0,
									  &password, &passwordType) == false)
				{
					ereport(ERROR,
							(errmsg("failed to authenticate with backend using SCRAM"),
							 errdetail("unable to get the password")));
				}
			}
		}

		/*
		 * if we have encrypted password, Decrypt it before going any further
		 */
		if (passwordType == PASSWORD_TYPE_AES)
		{
			password = get_decrypted_password(password);
			if (password == NULL)
				ereport(ERROR,
						(errmsg("SCRAM authentication failed"),
						 errdetail("unable to decrypt password from pool_passwd"),
						 errhint("verify the valid pool_key exists")));
			/* we have converted the password to plain text */
			passwordType = PASSWORD_TYPE_PLAINTEXT;
		}

		/* we have a password to use, validate the password type */
		if (passwordType != PASSWORD_TYPE_PLAINTEXT)
		{
			ereport(ERROR,
					(errmsg("failed to authenticate with backend using SCRAM"),
					 errdetail("valid password not found")));
		}

		for (i = 0; i < NUM_BACKENDS; i++)
		{
			if (!VALID_BACKEND(i))
				continue;

			ereport(DEBUG1,
					(errmsg("authentication backend %d", i),
					 errdetail("trying SCRAM authentication")));

			if (do_SCRAM(frontend, CONNECTION(cp, i), protoMajor, message_length, frontend->username, password, passwordType) == false)
			{
				pool_send_auth_fail(frontend, cp);
				ereport(ERROR,
						(errmsg("failed to authenticate with backend"),
						 errdetail("SCRAM authentication failed in slot [%d].", i)));
			}
			ereport(DEBUG1,
					(errmsg("SCRAM authentication successful for backend %d", i)));

		}

		send_auth_ok(frontend, protoMajor);
		authkind = 0;
	}

	else
	{
		ereport(ERROR,
				(errmsg("failed to authenticate with backend"),
				 errdetail("unsupported auth kind received from backend: authkind:%d", authkind)));
	}

	if (authkind != 0)
		ereport(ERROR,
				(errmsg("failed to authenticate with backend"),
				 errdetail("invalid auth kind received from backend: authkind:%d", authkind)));

	/*
	 * authentication ok. now read pid and secret key from the backend
	 */
	for (;;)
	{
		char	   *message = NULL;

		kind = pool_read_kind(cp);

		if (kind < 0)
		{
			ereport(ERROR,
					(errmsg("authentication failed from backend"),
					 errdetail("failed to read kind before BackendKeyData")));
		}
		else if (kind == 'K')
			break;

		if (protoMajor == PROTO_MAJOR_V3)
		{
			switch (kind)
			{
				case 'S':
					/* process parameter status */
					if (ParameterStatus(frontend, cp) != POOL_CONTINUE)
						return -1;
					pool_flush(frontend);
					break;

				case 'N':
					if (pool_extract_error_message(false, MAIN(cp), protoMajor, true, &message) == 1)
					{
						ereport(NOTICE,
								(errmsg("notice from backend"),
								 errdetail("BACKEND NOTICE: \"%s\"", message)));
						pfree(message);
					}
					/* process notice message */
					if (SimpleForwardToFrontend(kind, frontend, cp))
						ereport(ERROR,
								(errmsg("authentication failed"),
								 errdetail("failed to forward message to frontend")));
					pool_flush(frontend);
					break;

					/* process error message */
				case 'E':

					if (pool_extract_error_message(false, MAIN(cp), protoMajor, true, &message) == 1)
					{
						ereport(LOG,
								(errmsg("backend throws an error message"),
								 errdetail("%s", message)));
					}

					SimpleForwardToFrontend(kind, frontend, cp);

					pool_flush(frontend);

					ereport(ERROR,
							(errmsg("authentication failed, backend node replied with an error"),
							 errdetail("SERVER ERROR:\"%s\"", message ? message : "could not extract backend message")));

					if (message)
						pfree(message);
					break;

				default:
					ereport(ERROR,
							(errmsg("authentication failed"),
							 errdetail("unknown response \"%c\" before processing BackendKeyData", kind)));
					break;
			}
		}
		else
		{
			/* V2 case */
			switch (kind)
			{
				case 'N':
					/* process notice message */
					NoticeResponse(frontend, cp);
					break;

					/* process error message */
				case 'E':
					ErrorResponse(frontend, cp);
					/* fallthrough */
				default:
					ereport(ERROR,
							(errmsg("authentication failed"),
							 errdetail("invalid response \"%c\" before processing BackendKeyData", kind)));
					break;
			}
		}
	}

	/*
	 * message length (V3 only)
	 */
	if (protoMajor == PROTO_MAJOR_V3)
	{
		if ((length = pool_read_message_length(cp)) != 12)
		{
			ereport(ERROR,
					(errmsg("authentication failed"),
					 errdetail("invalid messages length(%d) for BackendKeyData", length)));
		}
	}

	/*
	 * OK, read pid and secret key
	 */
	sp = MAIN_CONNECTION(cp)->sp;
	pid = -1;

	for (i = 0; i < NUM_BACKENDS; i++)
	{
		if (VALID_BACKEND(i))
		{
			/* read pid */
			if (pool_read(CONNECTION(cp, i), &pid, sizeof(pid)) < 0)
			{
				ereport(ERROR,
						(errmsg("authentication failed"),
						 errdetail("failed to read pid in slot %d", i)));
			}
			ereport(DEBUG1,
					(errmsg("authentication backend"),
					 errdetail("cp->info[i]:%p pid:%u", &cp->info[i], ntohl(pid))));

			CONNECTION_SLOT(cp, i)->pid = cp->info[i].pid = pid;

			/* read key */
			if (pool_read(CONNECTION(cp, i), &key, sizeof(key)) < 0)
			{
				ereport(ERROR,
						(errmsg("authentication failed"),
						 errdetail("failed to read key in slot %d", i)));
			}
			CONNECTION_SLOT(cp, i)->key = cp->info[i].key = key;

			cp->info[i].major = sp->major;
			cp->info[i].minor = sp->minor;
			strlcpy(cp->info[i].database, sp->database, sizeof(cp->info[i].database));
			strlcpy(cp->info[i].user, sp->user, sizeof(cp->info[i].user));
			cp->info[i].counter = 1;
			CONNECTION(cp, i)->con_info = &cp->info[i];
			cp->info[i].swallow_termination = 0;
		}
	}

	if (pid == -1)
	{
		ereport(ERROR,
				(errmsg("authentication failed"),
				 errdetail("pool_do_auth: all backends are down")));
	}
	if (pool_send_backend_key_data(frontend, pid, key, protoMajor))
		ereport(ERROR,
				(errmsg("authentication failed"),
				 errdetail("failed to send backend data to frontend")));
	return 0;
}

/*
* do re-authentication for reused connection. if success return 0 otherwise throws ereport.
*/
int
pool_do_reauth(POOL_CONNECTION * frontend, POOL_CONNECTION_POOL * cp)
{
	int			protoMajor;
	int			msglen;

	protoMajor = MAJOR(cp);

	/*
	 * if hba is enabled we would already have passed authentication
	 */
	if (!frontend->frontend_authenticated)
	{
		switch (MAIN(cp)->auth_kind)
		{
			case AUTH_REQ_OK:
				/* trust */
				break;

			case AUTH_REQ_PASSWORD:
				/* clear text password */
				do_clear_text_password(MAIN(cp), frontend, 1, protoMajor);
				break;

			case AUTH_REQ_CRYPT:
				/* crypt password */
				do_crypt(MAIN(cp), frontend, 1, protoMajor);
				break;

			case AUTH_REQ_MD5:
				/* md5 password */
				authenticate_frontend_md5(MAIN(cp), frontend, 1, protoMajor);
				break;
			case AUTH_REQ_SASL:
				/* SCRAM */
				authenticate_frontend_SCRAM(MAIN(cp), frontend, 1);
				break;
			default:
				ereport(ERROR,
						(errmsg("authentication failed"),
						 errdetail("unknown authentication request code %d", MAIN(cp)->auth_kind)));
		}
	}

	pool_write(frontend, "R", 1);

	if (protoMajor == PROTO_MAJOR_V3)
	{
		msglen = htonl(8);
		pool_write(frontend, &msglen, sizeof(msglen));
	}

	msglen = htonl(0);
	pool_write_and_flush(frontend, &msglen, sizeof(msglen));
	pool_send_backend_key_data(frontend, MAIN_CONNECTION(cp)->pid, MAIN_CONNECTION(cp)->key, protoMajor);
	return 0;
}

/*
* send authentication failure message text to frontend
*/
static void
pool_send_auth_fail(POOL_CONNECTION * frontend, POOL_CONNECTION_POOL * cp)
{
	int			messagelen;
	char	   *errmessage;
	int			protoMajor;

	bool		send_error_to_frontend = true;

	protoMajor = MAJOR(cp);

	messagelen = strlen(MAIN_CONNECTION(cp)->sp->user) + 100;
	errmessage = (char *) palloc(messagelen + 1);

	snprintf(errmessage, messagelen, "password authentication failed for user \"%s\"",
			 MAIN_CONNECTION(cp)->sp->user);
	if (send_error_to_frontend)
		pool_send_fatal_message(frontend, protoMajor, "XX000", errmessage,
								"", "", __FILE__, __LINE__);
	pfree(errmessage);
}

/*
 * Send backend key data to frontend. if success return 0 otherwise non 0.
 */
static POOL_STATUS pool_send_backend_key_data(POOL_CONNECTION * frontend, int pid, int key, int protoMajor)
{
	char		kind;
	int			len;

	/* Send backend key data */
	kind = 'K';
	pool_write(frontend, &kind, 1);
	if (protoMajor == PROTO_MAJOR_V3)
	{
		len = htonl(12);
		pool_write(frontend, &len, sizeof(len));
	}
	ereport(DEBUG1,
			(errmsg("sending backend key data"),
			 errdetail("send pid %d to frontend", ntohl(pid))));

	pool_write(frontend, &pid, sizeof(pid));
	pool_write_and_flush(frontend, &key, sizeof(key));

	return 0;
}

static void
authenticate_frontend_clear_text(POOL_CONNECTION * frontend)
{
	static int	size;
	char		password[MAX_PASSWORD_SIZE];
	char		userPassword[MAX_PASSWORD_SIZE];
	char	   *storedPassword = NULL;
	char	   *userPwd = NULL;

	sendAuthRequest(frontend, frontend->protoVersion, AUTH_REQ_PASSWORD, NULL, 0);

	/* Read password packet */
	read_password_packet(frontend, frontend->protoVersion, password, &size);

	/* save the password in frontend */
	frontend->auth_kind = AUTH_REQ_PASSWORD;
	frontend->pwd_size = size;
	memcpy(frontend->password, password, frontend->pwd_size);
	frontend->password[size] = 0;	/* Null terminate the password string */
	frontend->passwordType = PASSWORD_TYPE_PLAINTEXT;

	if (!frontend->passwordMapping)
	{
		/*
		 * if the password is not present in pool_passwd just bail out from
		 * here
		 */
		return;
	}

	storedPassword = frontend->passwordMapping->pgpoolUser.password;
	userPwd = password;

	/*
	 * If we have md5 password stored in pool_passwd, convert the user
	 * supplied password to md5 for comparison
	 */
	if (frontend->passwordMapping->pgpoolUser.passwordType == PASSWORD_TYPE_MD5)
	{
		pg_md5_encrypt(password,
					   frontend->username,
					   strlen(frontend->username), userPassword);
		userPwd = userPassword;
		size = strlen(userPwd);
	}
	else if (frontend->passwordMapping->pgpoolUser.passwordType == PASSWORD_TYPE_AES)
	{
		/*
		 * decrypt the stored AES password for comparing it
		 */
		storedPassword = get_decrypted_password(storedPassword);
		if (storedPassword == NULL)
			ereport(ERROR,
					(errmsg("clear text authentication failed"),
					 errdetail("unable to decrypt password from pool_passwd"),
					 errhint("verify the valid pool_key exists")));
	}
	else if (frontend->passwordMapping->pgpoolUser.passwordType == PASSWORD_TYPE_TEXT_PREFIXED)
	{
		storedPassword = frontend->passwordMapping->pgpoolUser.password + strlen(PASSWORD_TEXT_PREFIX);
	}
	else if (frontend->passwordMapping->pgpoolUser.passwordType != PASSWORD_TYPE_PLAINTEXT)
	{
		ereport(ERROR,
				(errmsg("clear text authentication failed"),
				 errdetail("password type stored in pool_passwd can't be used for clear text authentication")));
	}

	if (memcmp(userPwd, storedPassword, size))
	{
		/* Password does not match */
		ereport(ERROR,
				(errmsg("clear text authentication failed"),
				 errdetail("password does not match")));
	}
	ereport(DEBUG1,
			(errmsg("clear text authentication successful with frontend")));

	if (frontend->passwordMapping->pgpoolUser.passwordType == PASSWORD_TYPE_AES)
		pfree(storedPassword);

	frontend->frontend_authenticated = true;
}

/*
 * perform clear text password authentication
 */
static int
do_clear_text_password(POOL_CONNECTION * backend, POOL_CONNECTION * frontend, int reauth, int protoMajor)
{
	static int	size;
	char	   *pwd = NULL;
	int			kind;
	PasswordType passwordType = PASSWORD_TYPE_UNKNOWN;

	if (reauth && frontend->frontend_authenticated)
	{
		/* frontend and backend are both authenticated already */
		return 0;
	}

	if (get_auth_password(backend, frontend, reauth, &pwd, &passwordType) == false)
	{
		/*
		 * We do not have any password, we can still get the password
		 * from client using plain text authentication if it is
		 * allowed by user
		 */

		if (frontend->pool_hba == NULL ||
			frontend->pool_hba->auth_method == uaPassword ||
			pool_config->allow_clear_text_frontend_auth )
		{
			ereport(DEBUG1,
				(errmsg("using clear text authentication with frontend"),
					 errdetail("backend is using password authentication")));

			authenticate_frontend_clear_text(frontend);

			/* now check again if we have a password now */

			if (get_auth_password(backend, frontend, reauth, &pwd, &passwordType) == false)
			{
				ereport(FATAL,
						(return_code(2),
							errmsg("clear text password authentication failed"),
							errdetail("unable to get the password for user: \"%s\"", frontend->username)));
			}
		}
	}

	if (passwordType == PASSWORD_TYPE_AES)
	{
		/*
		 * decrypt the stored AES password for comparing it
		 */
		pwd = get_decrypted_password(pwd);
		if (pwd == NULL)
			ereport(ERROR,
					(errmsg("clear text password authentication failed"),
					 errdetail("unable to decrypt password from pool_passwd"),
					 errhint("verify the valid pool_key exists")));
		/* we have converted the password to plain text */
		passwordType = PASSWORD_TYPE_PLAINTEXT;
	}

	if (pwd == NULL || passwordType != PASSWORD_TYPE_PLAINTEXT)
	{
		/* If we still do not have a password. we can't proceed */
		ereport(ERROR,
				(errmsg("clear text password authentication failed"),
				 errdetail("unable to get the password")));
		return 0;
	}

	size = strlen(pwd);

	/* connection reusing? */
	if (reauth)
	{
		if (size != backend->pwd_size)
			ereport(ERROR,
					(errmsg("clear text password authentication failed"),
					 errdetail("password size does not match")));

		if (memcmp(pwd, backend->password, backend->pwd_size) != 0)
			ereport(ERROR,
					(errmsg("clear text password authentication failed"),
					 errdetail("password does not match")));

		return 0;
	}

	kind = send_password_packet(backend, protoMajor, pwd);

	/* if authenticated, save info */
	if (kind == AUTH_REQ_OK)
	{
		if (IS_MAIN_NODE_ID(backend->db_node_id))
		{
			send_auth_ok(frontend, protoMajor);
		}

		backend->auth_kind = AUTH_REQ_PASSWORD;
		backend->pwd_size = size;
		memcpy(backend->password, pwd, backend->pwd_size);
		backend->passwordType = PASSWORD_TYPE_PLAINTEXT;
	}
	return kind;
}

/*
 * perform crypt authentication
 */
static int
do_crypt(POOL_CONNECTION * backend, POOL_CONNECTION * frontend, int reauth, int protoMajor)
{
	char		salt[2];
	static int	size;
	static char password[MAX_PASSWORD_SIZE];
	char		response;
	int			kind;
	int			len;

	if (!reauth)
	{
		/* read salt */
		pool_read(backend, salt, sizeof(salt));
	}
	else
	{
		memcpy(salt, backend->salt, sizeof(salt));
	}

	/* main? */
	if (IS_MAIN_NODE_ID(backend->db_node_id))
	{
		pool_write(frontend, "R", 1);	/* authentication */
		if (protoMajor == PROTO_MAJOR_V3)
		{
			len = htonl(10);
			pool_write(frontend, &len, sizeof(len));
		}
		kind = htonl(4);		/* crypt authentication */
		pool_write(frontend, &kind, sizeof(kind));	/* indicating crypt
													 * authentication */
		pool_write_and_flush(frontend, salt, sizeof(salt)); /* salt */

		/* read password packet */
		if (protoMajor == PROTO_MAJOR_V2)
		{
			pool_read(frontend, &size, sizeof(size));
		}
		else
		{
			char		k;

			pool_read(frontend, &k, sizeof(k));
			if (k != 'p')
			{
				ereport(ERROR,
						(errmsg("crypt authentication failed"),
						 errdetail("invalid password packet. Packet does not starts with \"p\"")));
			}
			pool_read(frontend, &size, sizeof(size));
		}

		if ((ntohl(size) - 4) > sizeof(password))
		{
			ereport(ERROR,
					(errmsg("crypt authentication failed"),
					 errdetail("password is too long, password size is %d", ntohl(size) - 4)));
		}

		pool_read(frontend, password, ntohl(size) - 4);
	}

	/* connection reusing? */
	if (reauth)
	{
		ereport(DEBUG1,
				(errmsg("performing crypt authentication"),
				 errdetail("size: %d saved_size: %d", (ntohl(size) - 4), backend->pwd_size)));

		if ((ntohl(size) - 4) != backend->pwd_size)
			ereport(ERROR,
					(errmsg("crypt authentication failed"),
					 errdetail("password size does not match")));


		if (memcmp(password, backend->password, backend->pwd_size) != 0)
			ereport(ERROR,
					(errmsg("crypt authentication failed"),
					 errdetail("password does not match")));

		return 0;
	}

	/* send password packet to backend */
	if (protoMajor == PROTO_MAJOR_V3)
		pool_write(backend, "p", 1);
	pool_write(backend, &size, sizeof(size));
	pool_write_and_flush(backend, password, ntohl(size) - 4);
	pool_read(backend, &response, sizeof(response));

	if (response != 'R')
	{
		if (response == 'E')	/* Backend has thrown an error instead */
		{
			char	   *message = NULL;

			if (pool_extract_error_message(false, backend, protoMajor, false, &message) == 1)
			{
				ereport(ERROR,
						(errmsg("crypt authentication failed"),
						 errdetail("%s", message ? message : "backend throws authentication error")));
			}
			if (message)
				pfree(message);
		}
		ereport(ERROR,
				(errmsg("crypt authentication failed"),
				 errdetail("invalid packet from backend. backend does not return R while processing clear text password authentication")));
	}

	if (protoMajor == PROTO_MAJOR_V3)
	{
		pool_read(backend, &len, sizeof(len));

		if (ntohl(len) != 8)
			ereport(ERROR,
					(errmsg("crypt authentication failed"),
					 errdetail("invalid packet from backend. incorrect authentication packet size (%d)", ntohl(len))));
	}

	/* expect to read "Authentication OK" response. kind should be 0... */
	pool_read(backend, &kind, sizeof(kind));

	/* if authenticated, save info */
	if (kind == 0)
	{
		int			msglen;

		pool_write(frontend, "R", 1);

		if (protoMajor == PROTO_MAJOR_V3)
		{
			msglen = htonl(8);
			pool_write(frontend, &msglen, sizeof(msglen));
		}

		msglen = htonl(0);
		pool_write_and_flush(frontend, &msglen, sizeof(msglen));

		backend->auth_kind = 4;
		backend->pwd_size = ntohl(size) - 4;
		memcpy(backend->password, password, backend->pwd_size);
		memcpy(backend->salt, salt, sizeof(salt));
	}
	return kind;
}

/*
 * Do the SCRAM authentication with the frontend using the stored
 * password in the pool_passwd file.
 */
static void
authenticate_frontend_SCRAM(POOL_CONNECTION * backend, POOL_CONNECTION * frontend, int reauth)
{
	void	   *scram_opaq;
	char	   *output = NULL;
	int			outputlen = 0;
	char	   *input;
	int			inputlen;
	int			result;
	bool		initial;
	char	   *logdetail = NULL;
	char	   *shadow_pass;

	PasswordType storedPasswordType = PASSWORD_TYPE_UNKNOWN;
	char	   *storedPassword = NULL;

	if (get_auth_password(backend, frontend, reauth,&storedPassword, &storedPasswordType) == false)
	{
		ereport(FATAL,
				(return_code(2),
				 errmsg("SCRAM authentication failed"),
				 errdetail("pool_passwd file does not contain an entry for \"%s\"", frontend->username)));
	}

	if (storedPasswordType == PASSWORD_TYPE_AES)
	{
		/*
		 * decrypt the stored AES password for comparing it
		 */
		storedPassword = get_decrypted_password(storedPassword);
		if (storedPassword == NULL)
			ereport(ERROR,
					(errmsg("SCRAM authentication failed"),
					 errdetail("unable to decrypt password from pool_passwd"),
					 errhint("verify the valid pool_key exists")));
		/* we have converted the password to plain text */
		storedPasswordType = PASSWORD_TYPE_PLAINTEXT;
	}

	if (storedPasswordType != PASSWORD_TYPE_PLAINTEXT)
	{
		ereport(ERROR,
				(errmsg("SCRAM authentication failed"),
				 errdetail("username \"%s\" has invalid password type", frontend->username)));
	}

	shadow_pass = pg_be_scram_build_verifier(storedPassword);
	if (!shadow_pass)
		ereport(ERROR,
				(errmsg("authentication failed"),
				 errdetail("failed to build the scram verifier")));

	/*
	 * SASL auth is not supported for protocol versions before 3, because it
	 * relies on the overall message length word to determine the SASL payload
	 * size in AuthenticationSASLContinue and PasswordMessage messages.  (We
	 * used to have a hard rule that protocol messages must be parsable
	 * without relying on the length word, but we hardly care about older
	 * protocol version anymore.)
	 */
	if (frontend->protoVersion < PROTO_MAJOR_V3)
		ereport(FATAL,
				(errmsg("SASL authentication is not supported in protocol version 2")));

	/*
	 * Send the SASL authentication request to user.  It includes the list of
	 * authentication mechanisms (which is trivial, because we only support
	 * SCRAM-SHA-256 at the moment).  The extra "\0" is for an empty string to
	 * terminate the list.
	 */

	sendAuthRequest(frontend, frontend->protoVersion, AUTH_REQ_SASL, SCRAM_SHA_256_NAME "\0",
					strlen(SCRAM_SHA_256_NAME) + 2);

	/*
	 * Initialize the status tracker for message exchanges.
	 *
	 * If the user doesn't exist, or doesn't have a valid password, or it's
	 * expired, we still go through the motions of SASL authentication, but
	 * tell the authentication method that the authentication is "doomed".
	 * That is, it's going to fail, no matter what.
	 *
	 * This is because we don't want to reveal to an attacker what usernames
	 * are valid, nor which users have a valid password.
	 */
	scram_opaq = pg_be_scram_init(frontend->username, shadow_pass);

	/*
	 * Loop through SASL message exchange.  This exchange can consist of
	 * multiple messages sent in both directions.  First message is always
	 * from the client.  All messages from client to server are password
	 * packets (type 'p').
	 */
	initial = true;
	do
	{
		static int	size;
		static char data[MAX_PASSWORD_SIZE];

		/* Read password packet */
		read_password_packet(frontend, frontend->protoVersion, data, &size);

		ereport(DEBUG4,
				(errmsg("Processing received SASL response of length %d", size)));

		/*
		 * The first SASLInitialResponse message is different from the others.
		 * It indicates which SASL mechanism the client selected, and contains
		 * an optional Initial Client Response payload.  The subsequent
		 * SASLResponse messages contain just the SASL payload.
		 */
		if (initial)
		{
			const char *selected_mech;
			char	   *ptr = data;

			/*
			 * We only support SCRAM-SHA-256 at the moment, so anything else
			 * is an error.
			 */
			selected_mech = data;
			if (strcmp(selected_mech, SCRAM_SHA_256_NAME) != 0)
			{
				ereport(ERROR,
						(errmsg("client selected an invalid SASL authentication mechanism")));
			}
			/* get the length of trailing optional data */
			ptr += strlen(selected_mech) + 1;
			memcpy(&inputlen, ptr, sizeof(int));
			inputlen = ntohl(inputlen);
			if (inputlen == -1)
				input = NULL;
			else
				input = ptr + 4;

			initial = false;
		}
		else
		{
			inputlen = size;
			input = data;
		}
		Assert(input == NULL || input[inputlen] == '\0');

		/*
		 * we pass 'logdetail' as NULL when doing a mock authentication,
		 * because we should already have a better error message in that case
		 */
		result = pg_be_scram_exchange(scram_opaq, input, inputlen,
									  &output, &outputlen,
									  &logdetail);

		/* input buffer no longer used */

		if (output)
		{
			/*
			 * Negotiation generated data to be sent to the client.
			 */
			ereport(DEBUG4,
					(errmsg("sending SASL challenge of length %u", outputlen)));

			if (result == SASL_EXCHANGE_SUCCESS)
				sendAuthRequest(frontend, frontend->protoVersion, AUTH_REQ_SASL_FIN, output, outputlen);
			else
				sendAuthRequest(frontend, frontend->protoVersion, AUTH_REQ_SASL_CONT, output, outputlen);

			pfree(output);
		}
	} while (result == SASL_EXCHANGE_CONTINUE);

	/* Oops, Something bad happened */
	if (result != SASL_EXCHANGE_SUCCESS)
	{
		ereport(ERROR,
				(errmsg("authentication failed"),
				 errdetail("username \"%s\" or password does not exist in backend", frontend->username)));
	}
	frontend->frontend_authenticated = true;
}

void
authenticate_frontend(POOL_CONNECTION * frontend)
{
	switch (frontend->pool_hba->auth_method)
	{
		case uaMD5:
			authenticate_frontend_md5(NULL, frontend, 0, frontend->protoVersion);
			break;
		case uaCert:
			authenticate_frontend_cert(frontend);
			break;
		case uaSCRAM:
			authenticate_frontend_SCRAM(NULL, frontend, 0);
			break;
		case uaPassword:
			authenticate_frontend_clear_text(frontend);
			break;
		case uaImplicitReject:
		case uaReject:
			ereport(ERROR,
					(errmsg("authentication with pgpool failed for user \"%s\" rejected", frontend->username)));
			break;
		case uaTrust:
			frontend->frontend_authenticated = true;
			break;
#ifdef USE_PAM
		case uaPAM:
			break;
#endif							/* USE_PAM */
#ifdef USE_LDAP
		case uaLDAP:
			break;
#endif							/* USE_LDAP */

	}
}

#ifdef USE_SSL
static void
authenticate_frontend_cert(POOL_CONNECTION * frontend)
{
	if (frontend->client_cert_loaded == true && frontend->cert_cn)
	{
		ereport(DEBUG1,
				(errmsg("connecting user is \"%s\" and ssl certificate CN is \"%s\"", frontend->username, frontend->cert_cn)));
		if (strcasecmp(frontend->username, frontend->cert_cn) == 0)
		{
			frontend->frontend_authenticated = true;
			ereport(LOG,
					(errmsg("SSL certificate authentication for user \"%s\" with Pgpool-II is successful", frontend->username)));
			return;
		}
		else
		{
			frontend->frontend_authenticated = false;
			ereport(LOG,
					(errmsg("SSL certificate authentication for user \"%s\" failed", frontend->username)));
		}
	}
	ereport(ERROR,
			(errmsg("CERT authentication failed"),
			 errdetail("no valid certificate presented")));

}
#else
static void
authenticate_frontend_cert(POOL_CONNECTION * frontend)
{
	ereport(ERROR,
			(errmsg("CERT authentication failed"),
			 errdetail("CERT authentication is not supported without SSL")));
}
#endif

static void
authenticate_frontend_md5(POOL_CONNECTION * backend, POOL_CONNECTION * frontend, int reauth, int protoMajor)
{
	char		salt[4];
	static int	size;
	char		password[MAX_PASSWORD_SIZE];
	char		userPassword[MAX_PASSWORD_SIZE];
	char		encbuf[POOL_PASSWD_LEN + 1];
	char	   *md5;

	PasswordType storedPasswordType = PASSWORD_TYPE_UNKNOWN;
	char	   *storedPassword = NULL;

	if (RAW_MODE || NUM_BACKENDS == 1)
	{
		if (backend)
			do_md5_single_backend(backend, frontend, reauth, protoMajor);
		return;					/* This will be handled later */
	}

	if (get_auth_password(backend, frontend, reauth,&storedPassword, &storedPasswordType) == false)
	{
		ereport(FATAL,
				(return_code(2),
				 errmsg("md5 authentication failed"),
				 errdetail("pool_passwd file does not contain an entry for \"%s\"", frontend->username)));
	}

	pool_random_salt(salt);
	send_md5auth_request(frontend, frontend->protoVersion, salt);

	/* Read password packet */
	read_password_packet(frontend, frontend->protoVersion, password, &size);

	/* If we have clear text password stored in pool_passwd, convert it to md5 */
	if (storedPasswordType == PASSWORD_TYPE_PLAINTEXT || storedPasswordType == PASSWORD_TYPE_AES)
	{
		char	   *pwd;

		if (storedPasswordType == PASSWORD_TYPE_AES)
		{
			pwd = get_decrypted_password(storedPassword);
			if (pwd == NULL)
				ereport(ERROR,
						(errmsg("md5 authentication failed"),
						 errdetail("unable to decrypt password from pool_passwd"),
						 errhint("verify the valid pool_key exists")));
		}
		else
		{
			pwd = storedPassword;
		}
		pg_md5_encrypt(pwd,
					   frontend->username,
					   strlen(frontend->username), userPassword);

		if (storedPasswordType == PASSWORD_TYPE_AES)
			pfree(pwd);

		md5 = userPassword;
	}
	else if (storedPasswordType == PASSWORD_TYPE_MD5)
	{
		md5 = storedPassword;
	}
	else
	{
		ereport(FATAL,
				(return_code(2),
				 errmsg("md5 authentication failed"),
				 errdetail("unable to get the password for \"%s\"", frontend->username)));
	}

	/* Check the password using my salt + pool_passwd */
	pg_md5_encrypt(md5 + strlen("md5"), salt, sizeof(salt), encbuf);
	if (strcmp(password, encbuf))
	{
		/* Password does not match */
		ereport(ERROR,
				(errmsg("md5 authentication failed"),
				 errdetail("password does not match")));
	}
	ereport(DEBUG1,
			(errmsg("md5 authentication successful with frontend")));

	frontend->frontend_authenticated = true;
}

/*
 * perform MD5 authentication
 */
static int
do_md5_single_backend(POOL_CONNECTION * backend, POOL_CONNECTION * frontend, int reauth, int protoMajor)
{
	char		salt[4];
	static int	size;
	static char password[MAX_PASSWORD_SIZE];
	int			kind;

	if (!reauth)
	{
		/* read salt from backend */
		pool_read(backend, salt, sizeof(salt));
		ereport(DEBUG1,
				(errmsg("performing md5 authentication"),
				 errdetail("DB node id: %d salt: %hhx%hhx%hhx%hhx", backend->db_node_id,
						   salt[0], salt[1], salt[2], salt[3])));
	}
	else
	{
		/* Use the saved salt */
		memcpy(salt, backend->salt, sizeof(salt));
	}

	/* Send md5 auth request to frontend */
	send_md5auth_request(frontend, protoMajor, salt);

	/* Read password packet */
	read_password_packet(frontend, protoMajor, password, &size);

	/* connection reusing? compare it with saved password */
	if (reauth)
	{
		if (backend->passwordType != PASSWORD_TYPE_MD5)
			ereport(ERROR,
					(errmsg("md5 authentication failed"),
					 errdetail("invalid password type")));

		if (size != backend->pwd_size)
			ereport(ERROR,
					(errmsg("md5 authentication failed"),
					 errdetail("password does not match")));

		if (memcmp(password, backend->password, backend->pwd_size) != 0)
			ereport(ERROR,
					(errmsg("md5 authentication failed"),
					 errdetail("password does not match")));
		return 0;
	}
	else
	{
		/* Send password packet to backend and receive auth response */
		kind = send_password_packet(backend, protoMajor, password);
		if (kind < 0)
			ereport(ERROR,
					(errmsg("md5 authentication failed"),
					 errdetail("backend replied with invalid kind")));

		/* If authenticated, reply back to frontend and save info */
		if (kind == AUTH_REQ_OK)
		{
			send_auth_ok(frontend, protoMajor);
			backend->passwordType = PASSWORD_TYPE_MD5;
			backend->auth_kind = AUTH_REQ_MD5;
			backend->pwd_size = size;
			memcpy(backend->password, password, backend->pwd_size);
			memcpy(backend->salt, salt, sizeof(salt));
		}
	}
	return kind;
}

static bool
get_auth_password(POOL_CONNECTION * backend, POOL_CONNECTION * frontend, int reauth,
				  char **password, PasswordType *passwordType)
{
	/* First preference is to use the pool_passwd file */
	if (frontend->passwordMapping == NULL)
		frontend->passwordMapping = pool_get_user_credentials(frontend->username);

	if (frontend->passwordMapping == NULL)
	{
		/*
		 * check if we have password stored in the frontend connection. that
		 * could come by using the clear text auth
		 */
		if (frontend->pwd_size > 0 && frontend->passwordType == PASSWORD_TYPE_PLAINTEXT)
		{
			*password = frontend->password;
			*passwordType = frontend->passwordType;
			return true;
		}
		else if (reauth && backend && backend->pwd_size > 0)
		{
			*password = backend->password;
			*passwordType = backend->passwordType;
			return true;
		}
	}
	else
	{
		if (frontend->passwordMapping->pgpoolUser.passwordType == PASSWORD_TYPE_TEXT_PREFIXED)
		{
			/* convert the TEXT prefixed password to plain text password */
			*passwordType = PASSWORD_TYPE_PLAINTEXT;
			*password = frontend->passwordMapping->pgpoolUser.password + strlen(PASSWORD_TEXT_PREFIX);
		}
		else
		{
			*password = frontend->passwordMapping->pgpoolUser.password;
			*passwordType = frontend->passwordMapping->pgpoolUser.passwordType;
		}
		return true;
	}
	return false;
}

/*
 * perform MD5 authentication
 */
static int
do_md5(POOL_CONNECTION * backend, POOL_CONNECTION * frontend, int reauth, int protoMajor,
	   char *storedPassword, PasswordType passwordType)
{
	char		salt[4];
	static char userPassword[MAX_PASSWORD_SIZE];
	int			kind;
	bool		password_decrypted = false;
	char		encbuf[POOL_PASSWD_LEN + 1];
	char	   *pool_passwd = NULL;

	if (RAW_MODE || NUM_BACKENDS == 1)
		return do_md5_single_backend(backend, frontend, reauth, protoMajor);

	if (passwordType == PASSWORD_TYPE_AES)
	{
		/*
		 * decrypt the stored AES password for comparing it
		 */
		storedPassword = get_decrypted_password(storedPassword);
		if (storedPassword == NULL)
			ereport(ERROR,
					(errmsg("md5 authentication failed"),
					 errdetail("unable to decrypt password from pool_passwd"),
					 errhint("verify the valid pool_key exists")));
		/* we have converted the password to plain text */
		passwordType = PASSWORD_TYPE_PLAINTEXT;
		password_decrypted = true;
	}
	if (passwordType == PASSWORD_TYPE_PLAINTEXT)
	{
		pg_md5_encrypt(storedPassword,
					   frontend->username,
					   strlen(frontend->username), userPassword);
		pool_passwd = userPassword;
	}
	else if (passwordType == PASSWORD_TYPE_MD5)
	{
		pool_passwd = storedPassword;
	}
	else
	{
		ereport(ERROR,
				(errmsg("md5 authentication failed"),
				 errdetail("unable to get the password")));
	}

	/* main? */
	if (IS_MAIN_NODE_ID(backend->db_node_id) && frontend->frontend_authenticated == false)
	{
		/*
		 * If frontend is not authenticated and do it it first. but if we have
		 * already received the password from frontend using the clear text
		 * auth, we may not need to authenticate it
		 */
		if (pool_config->allow_clear_text_frontend_auth &&
			frontend->auth_kind == AUTH_REQ_PASSWORD &&
			frontend->pwd_size > 0 &&
			frontend->passwordType == PASSWORD_TYPE_PLAINTEXT)
		{
			ereport(DEBUG2,
					(errmsg("MD5 authentication using the password from frontend")));
			/* save this password in backend for the re-auth */
			backend->pwd_size = frontend->pwd_size;
			memcpy(backend->password, frontend->password, frontend->pwd_size);
			backend->password[backend->pwd_size] = 0;	/* null terminate */
			backend->passwordType = frontend->passwordType;
		}
		else
		{
			authenticate_frontend_md5(backend, frontend, reauth, protoMajor);
		}
	}

	if (!reauth)
	{
		/*
		 * now authenticate the backend
		 */

		/* Read salt */
		pool_read(backend, salt, sizeof(salt));

		ereport(DEBUG2,
				(errmsg("performing md5 authentication"),
				 errdetail("DB node id: %d salt: %hhx%hhx%hhx%hhx", backend->db_node_id,
						   salt[0], salt[1], salt[2], salt[3])));

		/* Encrypt password in pool_passwd using the salt */
		pg_md5_encrypt(pool_passwd + strlen("md5"), salt, sizeof(salt), encbuf);

		/* Send password packet to backend and receive auth response */
		kind = send_password_packet(backend, protoMajor, encbuf);
		if (kind < 0)
			ereport(ERROR,
					(errmsg("md5 authentication failed"),
					 errdetail("backend replied with invalid kind")));
	}

	if (!reauth && kind == 0)
	{
		if (IS_MAIN_NODE_ID(backend->db_node_id))
		{
			/* Send auth ok to frontend */
			send_auth_ok(frontend, protoMajor);
		}

		/* Save the auth info */
		backend->auth_kind = AUTH_REQ_MD5;
	}

	if (password_decrypted && storedPassword)
		pfree(storedPassword);

	return 0;
}

/*
 * Send an authentication request packet to the frontend.
 */
static void
sendAuthRequest(POOL_CONNECTION * frontend, int protoMajor, int32 auth_req_type, char *extradata, int extralen)
{
	int			kind = htonl(auth_req_type);

	pool_write(frontend, "R", 1);	/* authentication */
	if (protoMajor == PROTO_MAJOR_V3)
	{
		int			len = 8 + extralen;

		len = htonl(len);
		pool_write(frontend, &len, sizeof(len));
	}
	pool_write(frontend, &kind, sizeof(kind));
	if (extralen > 0)
		pool_write_and_flush(frontend, extradata, extralen);
	else
		pool_flush(frontend);
}

/*
 * Send md5 authentication request packet to frontend
 */
static void
send_md5auth_request(POOL_CONNECTION * frontend, int protoMajor, char *salt)
{
	sendAuthRequest(frontend, protoMajor, AUTH_REQ_MD5, salt, 4);
}


/*
 * Read password packet from frontend
 */
static int
read_password_packet(POOL_CONNECTION * frontend, int protoMajor, char *password, int *pwdSize)
{
	int			size;

	/* Read password packet */
	if (protoMajor == PROTO_MAJOR_V2)
	{
		pool_read(frontend, &size, sizeof(size));
	}
	else
	{
		char		k;

		pool_read(frontend, &k, sizeof(k));
		if (k != 'p')
			ereport(ERROR,
					(errmsg("authentication failed"),
					 errdetail("invalid authentication packet. password packet does not start with \"p\"")));

		pool_read(frontend, &size, sizeof(size));
	}

	*pwdSize = ntohl(size) - 4;
	if (*pwdSize > MAX_PASSWORD_SIZE)
	{
		ereport(ERROR,
				(errmsg("authentication failed"),
				 errdetail("invalid authentication packet. password is too long. password length is %d", *pwdSize)));

		/*
		 * We do not read to throw away packet here. Since it is possible that
		 * it's a denial of service attack.
		 */
	}
	else if (*pwdSize <= 0)
		ereport(ERROR,
				(errmsg("authentication failed"),
				 errdetail("invalid authentication packet. invalid password length. password length is %d", *pwdSize)));

	pool_read(frontend, password, *pwdSize);

	password[*pwdSize] = '\0';

	return 0;
}

/*
 * Send password packet to backend and receive authentication response
 * packet.  Return value is the last field of authentication
 * response. If it's 0, authentication was successful.
 * "password" must be null-terminated.
 */
static int
send_password_packet(POOL_CONNECTION * backend, int protoMajor, char *password)
{
	int			size;
	int			len;
	int			kind;
	char		response;

	/* Send password packet to backend */
	if (protoMajor == PROTO_MAJOR_V3)
		pool_write(backend, "p", 1);
	size = htonl(sizeof(size) + strlen(password) + 1);
	pool_write(backend, &size, sizeof(size));
	pool_write_and_flush(backend, password, strlen(password) + 1);

	pool_read(backend, &response, sizeof(response));

	if (response != 'R')
	{
		if (response == 'E')	/* Backend has thrown an error instead */
		{
			char	   *message = NULL;

			if (pool_extract_error_message(false, backend, protoMajor, false, &message) == 1)
			{
				ereport(ERROR,
						(errmsg("authentication failed"),
						 errdetail("%s", message ? message : "backend throws authentication error")));
			}
			if (message)
				pfree(message);
		}
		ereport(ERROR,
				(errmsg("authentication failed"),
				 errdetail("invalid backend response. Response does not replied with \"R\"")));
	}

	if (protoMajor == PROTO_MAJOR_V3)
	{
		pool_read(backend, &len, sizeof(len));

		if (ntohl(len) != 8)
			ereport(ERROR,
					(errmsg("authentication failed"),
					 errdetail("invalid authentication packet. incorrect authentication packet size (%d)", ntohl(len))));
	}

	/* Expect to read "Authentication OK" response. kind should be 0... */
	pool_read(backend, &kind, sizeof(kind));

	return kind;
}

/*
 * Send auth ok to frontend
 */
static int
send_auth_ok(POOL_CONNECTION * frontend, int protoMajor)
{
	int			msglen;

	pool_write(frontend, "R", 1);

	if (protoMajor == PROTO_MAJOR_V3)
	{
		msglen = htonl(8);
		pool_write(frontend, &msglen, sizeof(msglen));
	}

	msglen = htonl(0);
	pool_write_and_flush(frontend, &msglen, sizeof(msglen));
	return 0;
}


void
pool_random(void *buf, size_t len)
{
	int			ret = 0;
#ifdef USE_SSL
	ret = RAND_bytes(buf, len);
#endif
	/* if RND_bytes fails or not present use the old technique */
	if (ret == 0)
	{
		int			i;
		char	   *ptr = buf;

		for (i = 0; i < len; i++)
		{
			long		rand = PostmasterRandom();

			ptr[i] = (rand % 255) + 1;
		}
	}
}

/*
 *  pool_random_salt
 */
void
pool_random_salt(char *md5Salt)
{
	pool_random(md5Salt, 4);
}

/*
 * PostmasterRandom
 */
static long
PostmasterRandom(void)
{
	extern struct timeval random_start_time;
	static unsigned int random_seed = 0;

	/*
	 * Select a random seed at the time of first receiving a request.
	 */
	if (random_seed == 0)
	{
		do
		{
			struct timeval random_stop_time;

			gettimeofday(&random_stop_time, NULL);

			/*
			 * We are not sure how much precision is in tv_usec, so we swap
			 * the high and low 16 bits of 'random_stop_time' and XOR them
			 * with 'random_start_time'. On the off chance that the result is
			 * 0, we loop until it isn't.
			 */
			random_seed = random_start_time.tv_usec ^
				((random_stop_time.tv_usec << 16) |
				 ((random_stop_time.tv_usec >> 16) & 0xffff));
		}
		while (random_seed == 0);

		srandom(random_seed);
	}

	return random();
}


static bool
do_SCRAM(POOL_CONNECTION * frontend, POOL_CONNECTION * backend, int protoMajor, int message_length,
		 char *username, char *storedPassword, PasswordType passwordType)
{
	/* read the packet first */
	void	   *sasl_state = NULL;
	int			payload_len = message_length - 4 - 4;
	int			auth_kind = AUTH_REQ_SASL;
	char		payload[MAX_SASL_PAYLOAD_LEN];

	if (passwordType != PASSWORD_TYPE_PLAINTEXT)
	{
		ereport(ERROR,
				(errmsg("SCRAM authentication failed"),
				 errdetail("invalid password type")));
	}
	if (storedPassword == NULL)
	{
		ereport(ERROR,
				(errmsg("SCRAM authentication failed"),
				 errdetail("password not found")));
	}

	/* main? */
	if (frontend && IS_MAIN_NODE_ID(backend->db_node_id) && frontend->frontend_authenticated == false)
	{
		/*
		 * If frontend is not authenticated and do it it first. but if we have
		 * already received the password from frontend using the clear text
		 * auth, we may not need to authenticate it
		 */
		if (pool_config->allow_clear_text_frontend_auth &&
			frontend->auth_kind == AUTH_REQ_PASSWORD &&
			frontend->pwd_size > 0 &&
			frontend->passwordType == PASSWORD_TYPE_PLAINTEXT)
		{
			ereport(DEBUG2,
					(errmsg("SCRAM authentication using the password from frontend")));
			/* save this password in backend for the re-auth */
			backend->pwd_size = frontend->pwd_size;
			memcpy(backend->password, frontend->password, frontend->pwd_size);
			backend->password[backend->pwd_size] = 0;	/* null terminate */
			backend->passwordType = frontend->passwordType;
		}
		else
		{
			authenticate_frontend_SCRAM(backend, frontend, 0);
		}
	}

	for (;;)
	{
		char		kind;
		int			len;

		/*
		 * at this point we have already read kind, message length and
		 * authkind
		 */
		if (payload_len > MAX_SASL_PAYLOAD_LEN)
			ereport(ERROR,
					(errmsg("invalid authentication data too big")));

		pool_read(backend, payload, payload_len);

		switch (auth_kind)
		{
			case AUTH_REQ_OK:
				/* Save the auth info in backend */
				backend->auth_kind = AUTH_REQ_SASL;
				if (sasl_state)
					pg_fe_scram_free(sasl_state);
				return true;
				break;
			case AUTH_REQ_SASL:

				/*
				 * The request contains the name (as assigned by IANA) of the
				 * authentication mechanism.
				 */
				sasl_state = pg_SASL_init(backend, payload, payload_len, username, storedPassword);
				if (!sasl_state)
				{
					ereport(ERROR,
							(errmsg("invalid authentication request from server")));
					return false;
				}
				break;

			case AUTH_REQ_SASL_CONT:
			case AUTH_REQ_SASL_FIN:
				if (sasl_state == NULL)
				{
					ereport(ERROR,
							(errmsg("invalid authentication request from server: AUTH_REQ_SASL_CONT without AUTH_REQ_SASL")));
					return false;
				}
				if (pg_SASL_continue(backend, payload, payload_len, sasl_state, (auth_kind == AUTH_REQ_SASL_FIN)) != 0)
				{
					/* Use error message, if set already */
					ereport(ERROR,
							(errmsg("error in SASL authentication")));

				}
				break;
			default:
				ereport(ERROR,
						(errmsg("invalid authentication request from server: unknown auth kind %d", auth_kind)));
		}
		/* Read next backend */
		pool_read(backend, &kind, sizeof(kind));
		pool_read(backend, &len, sizeof(len));
		if (kind != 'R')
			ereport(ERROR,
					(errmsg("backend authentication failed"),
					 errdetail("backend response with kind \'%c\' when expecting \'R\'", kind)));
		message_length = ntohl(len);
		if (message_length < 8)
			ereport(ERROR,
					(errmsg("backend authentication failed"),
					 errdetail("backend response with no data ")));

		pool_read(backend, &auth_kind, sizeof(auth_kind));
		auth_kind = ntohl(auth_kind);
		payload_len = message_length - 4 - 4;
	}
	return false;
}

static void *
pg_SASL_init(POOL_CONNECTION * backend, char *payload, int payloadlen, char *username, char *storedPassword)
{
	char	   *initialresponse = NULL;
	int			initialresponselen;
	bool		done;
	bool		success;
	const char *selected_mechanism;
	char	   *mechanism_buf = payload;
	void	   *sasl_state = NULL;
	int			size;
	int			send_msg_len;

	/*
	 * Parse the list of SASL authentication mechanisms in the
	 * AuthenticationSASL message, and select the best mechanism that we
	 * support.  (Only SCRAM-SHA-256 is supported at the moment.)
	 */
	selected_mechanism = NULL;
	for (;;)
	{
		/* An empty string indicates end of list */
		if (mechanism_buf[0] == '\0')
			break;

		/*
		 * If we have already selected a mechanism, just skip through the rest
		 * of the list.
		 */
		if (selected_mechanism)
			continue;

		/*
		 * Do we support this mechanism?
		 */
		if (strcmp(mechanism_buf, SCRAM_SHA_256_NAME) == 0)
		{
			/*
			 * This is the password which we need to send to the PG backend
			 * for authentication. It is stored in the file
			 */
			if (storedPassword == NULL || storedPassword[0] == '\0')
			{
				ereport(ERROR,
						(errmsg("password not found")));
			}

			sasl_state = pg_fe_scram_init(username, storedPassword);
			if (!sasl_state)
				ereport(ERROR,
						(errmsg("SASL authentication error\n")));
			selected_mechanism = SCRAM_SHA_256_NAME;
		}
		mechanism_buf += strlen(mechanism_buf) + 1;
	}

	if (!selected_mechanism)
	{
		ereport(ERROR,
				(errmsg("none of the server's SASL authentication mechanisms are supported\n")));
	}

	/* Get the mechanism-specific Initial Client Response, if any */
	pg_fe_scram_exchange(sasl_state,
						 NULL, -1,
						 &initialresponse, &initialresponselen,
						 &done, &success);

	if (done && !success)
		ereport(ERROR,
				(errmsg("SASL authentication error")));

	send_msg_len = strlen(selected_mechanism) + 1;
	if (initialresponse)
	{
		send_msg_len += 4;
		send_msg_len += initialresponselen;
	}

	size = htonl(send_msg_len + 4);

	pool_write(backend, "p", 1);
	pool_write(backend, &size, sizeof(size));
	pool_write(backend, (void *) selected_mechanism, strlen(selected_mechanism) + 1);
	if (initialresponse)
	{
		size = htonl(initialresponselen);
		pool_write(backend, &size, sizeof(size));
		pool_write(backend, initialresponse, initialresponselen);
	}
	pool_flush(backend);

	if (initialresponse)
		pfree(initialresponse);

	return sasl_state;
}

/*
 * Exchange a message for SASL communication protocol with the backend.
 * This should be used after calling pg_SASL_init to set up the status of
 * the protocol.
 */
static int
pg_SASL_continue(POOL_CONNECTION * backend, char *payload, int payloadlen, void *sasl_state, bool final)
{
	char	   *output;
	int			outputlen;
	bool		done;
	bool		success;
	char	   *challenge;

	/* Read the SASL challenge from the AuthenticationSASLContinue message. */
	challenge = palloc(payloadlen + 1);
	memcpy(challenge, payload, payloadlen);
	challenge[payloadlen] = '\0';

	/* For safety and convenience, ensure the buffer is NULL-terminated. */

	pg_fe_scram_exchange(sasl_state,
						 challenge, payloadlen,
						 &output, &outputlen,
						 &done, &success);
	pfree(challenge);			/* don't need the input anymore */

	if (final && !done)
	{
		if (outputlen != 0)
			pfree(output);

		ereport(ERROR,
				(errmsg("AuthenticationSASLFinal received from server, but SASL authentication was not completed")));
		return -1;
	}
	if (outputlen != 0)
	{
		/*
		 * Send the SASL response to the server.
		 */
		int			size = htonl(outputlen + 4);

		pool_write(backend, "p", 1);
		pool_write(backend, &size, sizeof(size));
		pool_write_and_flush(backend, output, outputlen);
		pfree(output);

		return 0;
	}

	if (done && !success)
		return -1;

	return 0;
}
