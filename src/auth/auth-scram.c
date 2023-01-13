/* -*-pgsql-c-*- */
/*
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
 * auth_scram.c: SCRAM authentication stuff
 * borrowed from PostgreSQL source src/backend/libpq/auth-scram.c
 *
 */
/*-------------------------------------------------------------------------
 *
 * auth-scram.c
 *	  Server-side implementation of the SASL SCRAM-SHA-256 mechanism.
 *
 * See the following RFCs for more details:
 * - RFC 5802: https://tools.ietf.org/html/rfc5802
 * - RFC 5803: https://tools.ietf.org/html/rfc5803
 * - RFC 7677: https://tools.ietf.org/html/rfc7677
 *
 * Here are some differences:
 *
 * - Username from the authentication exchange is not used. The client
 *	 should send an empty string as the username.
 *
 * - If the password isn't valid UTF-8, or contains characters prohibited
 *	 by the SASLprep profile, we skip the SASLprep pre-processing and use
 *	 the raw bytes in calculating the hash.
 *
 * - Channel binding is not supported yet.
 *
 *
 * The password stored in pg_authid consists of the iteration count, salt,
 * StoredKey and ServerKey.
 *
 * SASLprep usage
 * --------------
 *
 * One notable difference to the SCRAM specification is that while the
 * specification dictates that the password is in UTF-8, and prohibits
 * certain characters, we are more lenient.  If the password isn't a valid
 * UTF-8 string, or contains prohibited characters, the raw bytes are used
 * to calculate the hash instead, without SASLprep processing.  This is
 * because PostgreSQL supports other encodings too, and the encoding being
 * used during authentication is undefined (client_encoding isn't set until
 * after authentication).  In effect, we try to interpret the password as
 * UTF-8 and apply SASLprep processing, but if it looks invalid, we assume
 * that it's in some other encoding.
 *
 * In the worst case, we misinterpret a password that's in a different
 * encoding as being Unicode, because it happens to consists entirely of
 * valid UTF-8 bytes, and we apply Unicode normalization to it.  As long
 * as we do that consistently, that will not lead to failed logins.
 * Fortunately, the UTF-8 byte sequences that are ignored by SASLprep
 * don't correspond to any commonly used characters in any of the other
 * supported encodings, so it should not lead to any significant loss in
 * entropy, even if the normalization is incorrectly applied to a
 * non-UTF-8 password.
 *
 * Error handling
 * --------------
 *
 * Don't reveal user information to an unauthenticated client.  We don't
 * want an attacker to be able to probe whether a particular username is
 * valid.  In SCRAM, the server has to read the salt and iteration count
 * from the user's password verifier, and send it to the client.  To avoid
 * revealing whether a user exists, when the client tries to authenticate
 * with a username that doesn't exist, or doesn't have a valid SCRAM
 * verifier in pg_authid, we create a fake salt and iteration count
 * on-the-fly, and proceed with the authentication with that.  In the end,
 * we'll reject the attempt, as if an incorrect password was given.  When
 * we are performing a "mock" authentication, the 'doomed' flag in
 * scram_state is set.
 *
 * In the error messages, avoid printing strings from the client, unless
 * you check that they are pure ASCII.  We don't want an unauthenticated
 * attacker to be able to spam the logs with characters that are not valid
 * to the encoding being used, whatever that is.  We cannot avoid that in
 * general, after logging in, but let's do what we can here.
 *
 *
 * Portions Copyright (c) 1996-2017, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * src/backend/libpq/auth-scram.c
 *
 *-------------------------------------------------------------------------
 */

#include <unistd.h>
#include <string.h>
#include <errno.h>
#include "pool.h"
#include "auth/scram-common.h"
#include "utils/sha2.h"
#include "auth/pool_passwd.h"
#include "auth/scram.h"
#include "auth/pool_auth.h"
#include "utils/base64.h"
#include "utils/elog.h"
#include "utils/palloc.h"
#include "utils/memutils.h"

#include "parser/stringinfo.h"
#include "utils/palloc.h"
#include "utils/elog.h"
#include "utils/memutils.h"

#define MOCK_AUTH_NONCE_LEN		32

typedef enum
{
	FE_SCRAM_INIT,
	FE_SCRAM_NONCE_SENT,
	FE_SCRAM_PROOF_SENT,
	FE_SCRAM_FINISHED
} fe_scram_state_enum;

typedef struct
{
	fe_scram_state_enum state;

	/* These are supplied by the user */
	const char *username;
	char	   *password;

	/* We construct these */
	uint8		SaltedPassword[SCRAM_KEY_LEN];
	char	   *client_nonce;
	char	   *client_first_message_bare;
	char	   *client_final_message_without_proof;

	/* These come from the server-first message */
	char	   *server_first_message;
	char	   *salt;
	int			saltlen;
	int			iterations;
	char	   *nonce;

	/* These come from the server-final message */
	char	   *server_final_message;
	char		ServerSignature[SCRAM_KEY_LEN];
} fe_scram_state;


/*
 * Status data for a SCRAM authentication exchange.  This should be kept
 * internal to this file.
 */
typedef enum
{
	SCRAM_AUTH_INIT,
	SCRAM_AUTH_SALT_SENT,
	SCRAM_AUTH_FINISHED
} scram_state_enum;

typedef struct
{
	scram_state_enum state;

	const char *username;		/* username from startup packet */

	char		cbind_flag;

	int			iterations;
	char	   *salt;			/* base64-encoded */
	uint8		StoredKey[SCRAM_KEY_LEN];
	uint8		ServerKey[SCRAM_KEY_LEN];

	/* Fields of the first message from client */
	char	   *client_first_message_bare;
	char	   *client_username;
	char	   *client_nonce;

	/* Fields from the last message from client */
	char	   *client_final_message_without_proof;
	char	   *client_final_nonce;
	char		ClientProof[SCRAM_KEY_LEN];

	/* Fields generated in the server */
	char	   *server_first_message;
	char	   *server_nonce;

	/*
	 * If something goes wrong during the authentication, or we are performing
	 * a "mock" authentication (see comments at top of file), the 'doomed'
	 * flag is set.  A reason for the failure, for the server log, is put in
	 * 'logdetail'.
	 */
	bool		doomed;
	char	   *logdetail;
} scram_state;


static bool read_server_first_message(fe_scram_state *state, char *input);
static bool read_server_final_message(fe_scram_state *state, char *input);
static char *build_client_first_message(fe_scram_state *state);
static char *build_client_final_message(fe_scram_state *state);
static bool verify_server_signature(fe_scram_state *state);
static void calculate_client_proof(fe_scram_state *state,
					   const char *client_final_message_without_proof,
					   uint8 *result);

static void read_client_first_message(scram_state *state, char *input);
static void read_client_final_message(scram_state *state, char *input);
static char *build_server_first_message(scram_state *state);
static char *build_server_final_message(scram_state *state);
static bool verify_client_proof(scram_state *state);
static bool verify_final_nonce(scram_state *state);
static bool parse_scram_verifier(const char *verifier, int *iterations,
					 char **salt, uint8 *stored_key, uint8 *server_key);
static void mock_scram_verifier(const char *username, int *iterations,
					char **salt, uint8 *stored_key, uint8 *server_key);
static bool is_scram_printable(char *p);
static char *sanitize_char(char c);
static char *GetMockAuthenticationNonce(void);
static char *scram_mock_salt(const char *username);

/*
 * pg_be_scram_init
 *
 * Initialize a new SCRAM authentication exchange status tracker.  This
 * needs to be called before doing any exchange.  It will be filled later
 * after the beginning of the exchange with verifier data.
 *
 * 'username' is the username provided by the client in the startup message.
 * 'shadow_pass' is the role's password verifier, from pg_authid.rolpassword.
 * If 'shadow_pass' is NULL, we still perform an authentication exchange, but
 * it will fail, as if an incorrect password was given.
 */
void *
pg_be_scram_init(const char *username, const char *shadow_pass)
{
	scram_state *state;
	bool		got_verifier;

	state = (scram_state *) palloc0(sizeof(scram_state));
	state->state = SCRAM_AUTH_INIT;
	state->username = username;

	/*
	 * Parse the stored password verifier.
	 */
	if (shadow_pass)
	{
		int			password_type = get_password_type(shadow_pass);

		if (password_type == PASSWORD_TYPE_SCRAM_SHA_256)
		{
			if (parse_scram_verifier(shadow_pass, &state->iterations, &state->salt,
									 state->StoredKey, state->ServerKey))
				got_verifier = true;
			else
			{
				/*
				 * The password looked like a SCRAM verifier, but could not be
				 * parsed.
				 */
				ereport(LOG,
						(errmsg("invalid SCRAM verifier for user \"%s\"",
								username)));
				got_verifier = false;
			}
		}
		else
		{
			/*
			 * The user doesn't have SCRAM verifier. (You cannot do SCRAM
			 * authentication with an MD5 hash.)
			 */
			state->logdetail = psprintf(_("User \"%s\" does not have a valid SCRAM verifier."),
										state->username);
			got_verifier = false;
		}
	}
	else
	{
		/*
		 * The caller requested us to perform a dummy authentication.  This is
		 * considered normal, since the caller requested it, so don't set log
		 * detail.
		 */
		got_verifier = false;
	}

	/*
	 * If the user did not have a valid SCRAM verifier, we still go through
	 * the motions with a mock one, and fail as if the client supplied an
	 * incorrect password.  This is to avoid revealing information to an
	 * attacker.
	 */
	if (!got_verifier)
	{
		mock_scram_verifier(username, &state->iterations, &state->salt,
							state->StoredKey, state->ServerKey);
		state->doomed = true;
	}

	return state;
}

/*
 * Continue a SCRAM authentication exchange.
 *
 * 'input' is the SCRAM payload sent by the client.  On the first call,
 * 'input' contains the "Initial Client Response" that the client sent as
 * part of the SASLInitialResponse message, or NULL if no Initial Client
 * Response was given.  (The SASL specification distinguishes between an
 * empty response and non-existing one.)  On subsequent calls, 'input'
 * cannot be NULL.  For convenience in this function, the caller must
 * ensure that there is a null terminator at input[inputlen].
 *
 * The next message to send to client is saved in 'output', for a length
 * of 'outputlen'.  In the case of an error, optionally store a palloc'd
 * string at *logdetail that will be sent to the postmaster log (but not
 * the client).
 */
int
pg_be_scram_exchange(void *opaq, char *input, int inputlen,
					 char **output, int *outputlen, char **logdetail)
{
	scram_state *state = (scram_state *) opaq;
	int			result;

	*output = NULL;

	/*
	 * If the client didn't include an "Initial Client Response" in the
	 * SASLInitialResponse message, send an empty challenge, to which the
	 * client will respond with the same data that usually comes in the
	 * Initial Client Response.
	 */
	if (input == NULL)
	{
		Assert(state->state == SCRAM_AUTH_INIT);

		*output = pstrdup("");
		*outputlen = 0;
		return SASL_EXCHANGE_CONTINUE;
	}

	/*
	 * Check that the input length agrees with the string length of the input.
	 * We can ignore inputlen after this.
	 */
	if (inputlen == 0)
		ereport(ERROR,
				(errcode(ERRCODE_PROTOCOL_VIOLATION),
				 errmsg("malformed SCRAM message"),
				 errdetail("The message is empty.")));
	if (inputlen != strlen(input))
		ereport(ERROR,
				(errcode(ERRCODE_PROTOCOL_VIOLATION),
				 errmsg("malformed SCRAM message"),
				 errdetail("Message length does not match input length.")));

	switch (state->state)
	{
		case SCRAM_AUTH_INIT:

			/*
			 * Initialization phase.  Receive the first message from client
			 * and be sure that it parsed correctly.  Then send the challenge
			 * to the client.
			 */
			read_client_first_message(state, input);

			/* prepare message to send challenge */
			*output = build_server_first_message(state);

			state->state = SCRAM_AUTH_SALT_SENT;
			result = SASL_EXCHANGE_CONTINUE;
			break;

		case SCRAM_AUTH_SALT_SENT:

			/*
			 * Final phase for the server.  Receive the response to the
			 * challenge previously sent, verify, and let the client know that
			 * everything went well (or not).
			 */
			read_client_final_message(state, input);

			if (!verify_final_nonce(state))
				ereport(ERROR,
						(errcode(ERRCODE_PROTOCOL_VIOLATION),
						 errmsg("invalid SCRAM response"),
						 errdetail("Nonce does not match.")));

			/*
			 * Now check the final nonce and the client proof.
			 *
			 * If we performed a "mock" authentication that we knew would fail
			 * from the get go, this is where we fail.
			 *
			 * The SCRAM specification includes an error code,
			 * "invalid-proof", for authentication failure, but it also allows
			 * erroring out in an application-specific way.  We choose to do
			 * the latter, so that the error message for invalid password is
			 * the same for all authentication methods.  The caller will call
			 * ereport(), when we return SASL_EXCHANGE_FAILURE with no output.
			 *
			 * NB: the order of these checks is intentional.  We calculate the
			 * client proof even in a mock authentication, even though it's
			 * bound to fail, to thwart timing attacks to determine if a role
			 * with the given name exists or not.
			 */
			if (!verify_client_proof(state) || state->doomed)
			{
				result = SASL_EXCHANGE_FAILURE;
				break;
			}

			/* Build final message for client */
			*output = build_server_final_message(state);

			/* Success! */
			result = SASL_EXCHANGE_SUCCESS;
			state->state = SCRAM_AUTH_FINISHED;
			break;

		default:
			elog(ERROR, "invalid SCRAM exchange state");
			result = SASL_EXCHANGE_FAILURE;
	}

	if (result == SASL_EXCHANGE_FAILURE && state->logdetail && logdetail)
		*logdetail = state->logdetail;

	if (*output)
		*outputlen = strlen(*output);

	return result;
}

/*
 * Construct a verifier string for SCRAM, stored in pg_authid.rolpassword.
 *
 * The result is palloc'd, so caller is responsible for freeing it.
 */
char *
pg_be_scram_build_verifier(const char *password)
{
	char		saltbuf[SCRAM_DEFAULT_SALT_LEN];
	char	   *result;

	/* Generate random salt */
	pool_random(saltbuf, SCRAM_DEFAULT_SALT_LEN);

	result = scram_build_verifier(saltbuf, SCRAM_DEFAULT_SALT_LEN,
								  SCRAM_DEFAULT_ITERATIONS, password);


	return result;
}

/*
 * Verify a plaintext password against a SCRAM verifier.  This is used when
 * performing plaintext password authentication for a user that has a SCRAM
 * verifier stored in pg_authid.
 */
bool
scram_verify_plain_password(const char *username, const char *password,
							const char *verifier)
{
	char	   *encoded_salt;
	char	   *salt;
	int			saltlen;
	int			iterations;
	uint8		salted_password[SCRAM_KEY_LEN];
	uint8		stored_key[SCRAM_KEY_LEN];
	uint8		server_key[SCRAM_KEY_LEN];
	uint8		computed_key[SCRAM_KEY_LEN];

	if (!parse_scram_verifier(verifier, &iterations, &encoded_salt,
							  stored_key, server_key))
	{
		/*
		 * The password looked like a SCRAM verifier, but could not be parsed.
		 */
		ereport(LOG,
				(errmsg("invalid SCRAM verifier for user \"%s\"", username)));
		return false;
	}

	salt = palloc(pg_b64_dec_len(strlen(encoded_salt)));
	saltlen = pg_b64_decode(encoded_salt, strlen(encoded_salt), salt);
	if (saltlen == -1)
	{
		ereport(LOG,
				(errmsg("invalid SCRAM verifier for user \"%s\"", username)));

		pfree(encoded_salt);
		pfree(salt);
		return false;
	}

	/* Compute Server Key based on the user-supplied plaintext password */
	scram_SaltedPassword(password, salt, saltlen, iterations, salted_password);
	scram_ServerKey(salted_password, computed_key);

	/*
	 * Compare the verifier's Server Key with the one computed from the
	 * user-supplied password.
	 */
	pfree(encoded_salt);
	pfree(salt);
	return memcmp(computed_key, server_key, SCRAM_KEY_LEN) == 0;
}


/*
 * Parse and validate format of given SCRAM verifier.
 *
 * Returns true if the SCRAM verifier has been parsed, and false otherwise.
 */
static bool
parse_scram_verifier(const char *verifier, int *iterations, char **salt,
					 uint8 *stored_key, uint8 *server_key)
{
	char	   *v;
	char	   *p;
	char	   *scheme_str;
	char	   *salt_str;
	char	   *iterations_str;
	char	   *storedkey_str;
	char	   *serverkey_str;
	int			decoded_len;
	char	   *decoded_salt_buf;

	/*
	 * The verifier is of form:
	 *
	 * SCRAM-SHA-256$<iterations>:<salt>$<storedkey>:<serverkey>
	 */
	v = pstrdup(verifier);
	if ((scheme_str = strtok(v, "$")) == NULL)
		goto invalid_verifier;
	if ((iterations_str = strtok(NULL, ":")) == NULL)
		goto invalid_verifier;
	if ((salt_str = strtok(NULL, "$")) == NULL)
		goto invalid_verifier;
	if ((storedkey_str = strtok(NULL, ":")) == NULL)
		goto invalid_verifier;
	if ((serverkey_str = strtok(NULL, "")) == NULL)
		goto invalid_verifier;

	/* Parse the fields */
	if (strcmp(scheme_str, "SCRAM-SHA-256") != 0)
		goto invalid_verifier;

	errno = 0;
	*iterations = strtol(iterations_str, &p, 10);
	if (*p || errno != 0)
		goto invalid_verifier;

	/*
	 * Verify that the salt is in Base64-encoded format, by decoding it,
	 * although we return the encoded version to the caller.
	 */
	decoded_salt_buf = palloc(pg_b64_dec_len(strlen(salt_str)));
	decoded_len = pg_b64_decode(salt_str, strlen(salt_str), decoded_salt_buf);
	pfree(decoded_salt_buf);

	if (decoded_len < 0)
		goto invalid_verifier;
	*salt = pstrdup(salt_str);

	/*
	 * Decode StoredKey and ServerKey.
	 */
	if (pg_b64_dec_len(strlen(storedkey_str) != SCRAM_KEY_LEN))
		goto invalid_verifier;
	decoded_len = pg_b64_decode(storedkey_str, strlen(storedkey_str),
								(char *) stored_key);
	if (decoded_len != SCRAM_KEY_LEN)
		goto invalid_verifier;

	if (pg_b64_dec_len(strlen(serverkey_str) != SCRAM_KEY_LEN))
		goto invalid_verifier;
	decoded_len = pg_b64_decode(serverkey_str, strlen(serverkey_str),
								(char *) server_key);
	if (decoded_len != SCRAM_KEY_LEN)
		goto invalid_verifier;

	pfree(v);

	return true;

invalid_verifier:
	pfree(v);
	*salt = NULL;
	return false;
}

static void
mock_scram_verifier(const char *username, int *iterations, char **salt,
					uint8 *stored_key, uint8 *server_key)
{
	char	   *raw_salt;
	char	   *encoded_salt;
	int			encoded_len;

	/* Generate deterministic salt */
	raw_salt = scram_mock_salt(username);

	encoded_salt = (char *) palloc(pg_b64_enc_len(SCRAM_DEFAULT_SALT_LEN) + 1);
	encoded_len = pg_b64_encode(raw_salt, SCRAM_DEFAULT_SALT_LEN, encoded_salt);
	encoded_salt[encoded_len] = '\0';

	*salt = encoded_salt;
	*iterations = SCRAM_DEFAULT_ITERATIONS;

	/* StoredKey and ServerKey are not used in a doomed authentication */
	memset(stored_key, 0, SCRAM_KEY_LEN);
	memset(server_key, 0, SCRAM_KEY_LEN);
}

/*
 * Read the value in a given SCRAM exchange message for given attribute.
 */
static char *
read_attr_value(char **input, char attr)
{
	char	   *begin = *input;
	char	   *end;

	if (*begin != attr)
		ereport(ERROR,
				(errcode(ERRCODE_PROTOCOL_VIOLATION),
				 errmsg("malformed SCRAM message"),
				 errdetail("Expected attribute \"%c\" but found \"%s\".",
						   attr, sanitize_char(*begin))));
	begin++;

	if (*begin != '=')
		ereport(ERROR,
				(errcode(ERRCODE_PROTOCOL_VIOLATION),
				 errmsg("malformed SCRAM message"),
				 errdetail("Expected character \"=\" for attribute \"%c\".", attr)));
	begin++;

	end = begin;
	while (*end && *end != ',')
		end++;

	if (*end)
	{
		*end = '\0';
		*input = end + 1;
	}
	else
		*input = end;

	return begin;
}

static bool
is_scram_printable(char *p)
{
	/*------
	 * Printable characters, as defined by SCRAM spec: (RFC 5802)
	 *
	 *	printable		= %x21-2B / %x2D-7E
	 *					  ;; Printable ASCII except ",".
	 *					  ;; Note that any "printable" is also
	 *					  ;; a valid "value".
	 *------
	 */
	for (; *p; p++)
	{
		if (*p < 0x21 || *p > 0x7E || *p == 0x2C /* comma */ )
			return false;
	}
	return true;
}

/*
 * Convert an arbitrary byte to printable form.  For error messages.
 *
 * If it's a printable ASCII character, print it as a single character.
 * otherwise, print it in hex.
 *
 * The returned pointer points to a static buffer.
 */
static char *
sanitize_char(char c)
{
	static char buf[5];

	if (c >= 0x21 && c <= 0x7E)
		snprintf(buf, sizeof(buf), "'%c'", c);
	else
		snprintf(buf, sizeof(buf), "0x%02x", (unsigned char) c);
	return buf;
}

/*
 * Read the next attribute and value in a SCRAM exchange message.
 *
 * Returns NULL if there is attribute.
 */
static char *
read_any_attr(char **input, char *attr_p)
{
	char	   *begin = *input;
	char	   *end;
	char		attr = *begin;

	/*------
	 * attr-val		   = ALPHA "=" value
	 *					 ;; Generic syntax of any attribute sent
	 *					 ;; by server or client
	 *------
	 */
	if (!((attr >= 'A' && attr <= 'Z') ||
		  (attr >= 'a' && attr <= 'z')))
		ereport(ERROR,
				(errcode(ERRCODE_PROTOCOL_VIOLATION),
				 errmsg("malformed SCRAM message"),
				 errdetail("Attribute expected, but found invalid character \"%s\".",
						   sanitize_char(attr))));
	if (attr_p)
		*attr_p = attr;
	begin++;

	if (*begin != '=')
		ereport(ERROR,
				(errcode(ERRCODE_PROTOCOL_VIOLATION),
				 errmsg("malformed SCRAM message"),
				 errdetail("Expected character \"=\" for attribute \"%c\".", attr)));
	begin++;

	end = begin;
	while (*end && *end != ',')
		end++;

	if (*end)
	{
		*end = '\0';
		*input = end + 1;
	}
	else
		*input = end;

	return begin;
}

/*
 * Read and parse the first message from client in the context of a SCRAM
 * authentication exchange message.
 *
 * At this stage, any errors will be reported directly with ereport(ERROR).
 */
static void
read_client_first_message(scram_state *state, char *input)
{
	input = pstrdup(input);

	/*------
	 * The syntax for the client-first-message is: (RFC 5802)
	 *
	 * saslname		   = 1*(value-safe-char / "=2C" / "=3D")
	 *					 ;; Conforms to <value>.
	 *
	 * authzid		   = "a=" saslname
	 *					 ;; Protocol specific.
	 *
	 * cb-name		   = 1*(ALPHA / DIGIT / "." / "-")
	 *					  ;; See RFC 5056, Section 7.
	 *					  ;; E.g., "tls-server-end-point" or
	 *					  ;; "tls-unique".
	 *
	 * gs2-cbind-flag  = ("p=" cb-name) / "n" / "y"
	 *					 ;; "n" -> client doesn't support channel binding.
	 *					 ;; "y" -> client does support channel binding
	 *					 ;;		   but thinks the server does not.
	 *					 ;; "p" -> client requires channel binding.
	 *					 ;; The selected channel binding follows "p=".
	 *
	 * gs2-header	   = gs2-cbind-flag "," [ authzid ] ","
	 *					 ;; GS2 header for SCRAM
	 *					 ;; (the actual GS2 header includes an optional
	 *					 ;; flag to indicate that the GSS mechanism is not
	 *					 ;; "standard", but since SCRAM is "standard", we
	 *					 ;; don't include that flag).
	 *
	 * username		   = "n=" saslname
	 *					 ;; Usernames are prepared using SASLprep.
	 *
	 * reserved-mext  = "m=" 1*(value-char)
	 *					 ;; Reserved for signaling mandatory extensions.
	 *					 ;; The exact syntax will be defined in
	 *					 ;; the future.
	 *
	 * nonce		   = "r=" c-nonce [s-nonce]
	 *					 ;; Second part provided by server.
	 *
	 * c-nonce		   = printable
	 *
	 * client-first-message-bare =
	 *					 [reserved-mext ","]
	 *					 username "," nonce ["," extensions]
	 *
	 * client-first-message =
	 *					 gs2-header client-first-message-bare
	 *
	 * For example:
	 * n,,n=user,r=fyko+d2lbbFgONRv9qkxdawL
	 *
	 * The "n,," in the beginning means that the client doesn't support
	 * channel binding, and no authzid is given.  "n=user" is the username.
	 * However, in PostgreSQL the username is sent in the startup packet, and
	 * the username in the SCRAM exchange is ignored.  libpq always sends it
	 * as an empty string.  The last part, "r=fyko+d2lbbFgONRv9qkxdawL" is
	 * the client nonce.
	 *------
	 */

	/* read gs2-cbind-flag */
	state->cbind_flag = *input;
	switch (*input)
	{
		case 'n':
			/* Client does not support channel binding */
			input++;
			break;
		case 'y':
			/* Client supports channel binding, but we're not doing it today */
			input++;
			break;
		case 'p':

			/*
			 * Client requires channel binding.  We don't support it.
			 *
			 * RFC 5802 specifies a particular error code,
			 * e=server-does-support-channel-binding, for this.  But it can
			 * only be sent in the server-final message, and we don't want to
			 * go through the motions of the authentication, knowing it will
			 * fail, just to send that error message.
			 */
			ereport(ERROR,
					(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
					 errmsg("client requires SCRAM channel binding, but it is not supported")));
		default:
			ereport(ERROR,
					(errcode(ERRCODE_PROTOCOL_VIOLATION),
					 errmsg("malformed SCRAM message"),
					 errdetail("Unexpected channel-binding flag \"%s\".",
							   sanitize_char(*input))));
	}
	if (*input != ',')
		ereport(ERROR,
				(errcode(ERRCODE_PROTOCOL_VIOLATION),
				 errmsg("malformed SCRAM message"),
				 errdetail("Comma expected, but found character \"%s\".",
						   sanitize_char(*input))));
	input++;

	/*
	 * Forbid optional authzid (authorization identity).  We don't support it.
	 */
	if (*input == 'a')
		ereport(ERROR,
				(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
				 errmsg("client uses authorization identity, but it is not supported")));
	if (*input != ',')
		ereport(ERROR,
				(errcode(ERRCODE_PROTOCOL_VIOLATION),
				 errmsg("malformed SCRAM message"),
				 errdetail("Unexpected attribute \"%s\" in client-first-message.",
						   sanitize_char(*input))));
	input++;

	state->client_first_message_bare = pstrdup(input);

	/*
	 * Any mandatory extensions would go here.  We don't support any.
	 *
	 * RFC 5802 specifies error code "e=extensions-not-supported" for this,
	 * but it can only be sent in the server-final message.  We prefer to fail
	 * immediately (which the RFC also allows).
	 */
	if (*input == 'm')
		ereport(ERROR,
				(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
				 errmsg("client requires an unsupported SCRAM extension")));

	/*
	 * Read username.  Note: this is ignored.  We use the username from the
	 * startup message instead, still it is kept around if provided as it
	 * proves to be useful for debugging purposes.
	 */
	state->client_username = read_attr_value(&input, 'n');

	/* read nonce and check that it is made of only printable characters */
	state->client_nonce = read_attr_value(&input, 'r');
	if (!is_scram_printable(state->client_nonce))
		ereport(ERROR,
				(errcode(ERRCODE_PROTOCOL_VIOLATION),
				 errmsg("non-printable characters in SCRAM nonce")));

	/*
	 * There can be any number of optional extensions after this.  We don't
	 * support any extensions, so ignore them.
	 */
	while (*input != '\0')
		read_any_attr(&input, NULL);

	/* success! */
}

/*
 * Verify the final nonce contained in the last message received from
 * client in an exchange.
 */
static bool
verify_final_nonce(scram_state *state)
{
	int			client_nonce_len = strlen(state->client_nonce);
	int			server_nonce_len = strlen(state->server_nonce);
	int			final_nonce_len = strlen(state->client_final_nonce);

	if (final_nonce_len != client_nonce_len + server_nonce_len)
		return false;
	if (memcmp(state->client_final_nonce, state->client_nonce, client_nonce_len) != 0)
		return false;
	if (memcmp(state->client_final_nonce + client_nonce_len, state->server_nonce, server_nonce_len) != 0)
		return false;

	return true;
}

/*
 * Verify the client proof contained in the last message received from
 * client in an exchange.
 */
static bool
verify_client_proof(scram_state *state)
{
	uint8		ClientSignature[SCRAM_KEY_LEN];
	uint8		ClientKey[SCRAM_KEY_LEN];
	uint8		client_StoredKey[SCRAM_KEY_LEN];
	scram_HMAC_ctx ctx;
	int			i;

	/* calculate ClientSignature */
	scram_HMAC_init(&ctx, state->StoredKey, SCRAM_KEY_LEN);
	scram_HMAC_update(&ctx,
					  state->client_first_message_bare,
					  strlen(state->client_first_message_bare));
	scram_HMAC_update(&ctx, ",", 1);
	scram_HMAC_update(&ctx,
					  state->server_first_message,
					  strlen(state->server_first_message));
	scram_HMAC_update(&ctx, ",", 1);
	scram_HMAC_update(&ctx,
					  state->client_final_message_without_proof,
					  strlen(state->client_final_message_without_proof));
	scram_HMAC_final(ClientSignature, &ctx);

	/* Extract the ClientKey that the client calculated from the proof */
	for (i = 0; i < SCRAM_KEY_LEN; i++)
		ClientKey[i] = state->ClientProof[i] ^ ClientSignature[i];

	/* Hash it one more time, and compare with StoredKey */
	scram_H(ClientKey, SCRAM_KEY_LEN, client_StoredKey);

	if (memcmp(client_StoredKey, state->StoredKey, SCRAM_KEY_LEN) != 0)
		return false;

	return true;
}

/*
 * Build the first server-side message sent to the client in a SCRAM
 * communication exchange.
 */
static char *
build_server_first_message(scram_state *state)
{
	/*------
	 * The syntax for the server-first-message is: (RFC 5802)
	 *
	 * server-first-message =
	 *					 [reserved-mext ","] nonce "," salt ","
	 *					 iteration-count ["," extensions]
	 *
	 * nonce		   = "r=" c-nonce [s-nonce]
	 *					 ;; Second part provided by server.
	 *
	 * c-nonce		   = printable
	 *
	 * s-nonce		   = printable
	 *
	 * salt			   = "s=" base64
	 *
	 * iteration-count = "i=" posit-number
	 *					 ;; A positive number.
	 *
	 * Example:
	 *
	 * r=fyko+d2lbbFgONRv9qkxdawL3rfcNHYJY1ZVvWVs7j,s=QSXCR+Q6sek8bf92,i=4096
	 *------
	 */

	/*
	 * Per the spec, the nonce may consist of any printable ASCII characters.
	 * For convenience, however, we don't use the whole range available,
	 * rather, we generate some random bytes, and base64 encode them.
	 */
	char		raw_nonce[SCRAM_RAW_NONCE_LEN];
	int			encoded_len;

	pool_random(raw_nonce, SCRAM_RAW_NONCE_LEN);

	state->server_nonce = palloc(pg_b64_enc_len(SCRAM_RAW_NONCE_LEN) + 1);
	encoded_len = pg_b64_encode(raw_nonce, SCRAM_RAW_NONCE_LEN, state->server_nonce);
	state->server_nonce[encoded_len] = '\0';

	state->server_first_message =
		psprintf("r=%s%s,s=%s,i=%u",
				 state->client_nonce, state->server_nonce,
				 state->salt, state->iterations);

	return pstrdup(state->server_first_message);
}


/*
 * Read and parse the final message received from client.
 */
static void
read_client_final_message(scram_state *state, char *input)
{
	char		attr;
	char	   *channel_binding;
	char	   *value;
	char	   *begin,
			   *proof;
	char	   *p;
	char	   *client_proof;

	begin = p = pstrdup(input);

	/*------
	 * The syntax for the server-first-message is: (RFC 5802)
	 *
	 * gs2-header	   = gs2-cbind-flag "," [ authzid ] ","
	 *					 ;; GS2 header for SCRAM
	 *					 ;; (the actual GS2 header includes an optional
	 *					 ;; flag to indicate that the GSS mechanism is not
	 *					 ;; "standard", but since SCRAM is "standard", we
	 *					 ;; don't include that flag).
	 *
	 * cbind-input	 = gs2-header [ cbind-data ]
	 *					 ;; cbind-data MUST be present for
	 *					 ;; gs2-cbind-flag of "p" and MUST be absent
	 *					 ;; for "y" or "n".
	 *
	 * channel-binding = "c=" base64
	 *					 ;; base64 encoding of cbind-input.
	 *
	 * proof		   = "p=" base64
	 *
	 * client-final-message-without-proof =
	 *					 channel-binding "," nonce [","
	 *					 extensions]
	 *
	 * client-final-message =
	 *					 client-final-message-without-proof "," proof
	 *------
	 */

	/*
	 * Read channel-binding.  We don't support channel binding, so it's
	 * expected to always be "biws", which is "n,,", base64-encoded, or
	 * "eSws", which is "y,,".  We also have to check whether the flag is the
	 * same one that the client originally sent.
	 */
	channel_binding = read_attr_value(&p, 'c');
	if (!(strcmp(channel_binding, "biws") == 0 && state->cbind_flag == 'n') &&
		!(strcmp(channel_binding, "eSws") == 0 && state->cbind_flag == 'y'))
		ereport(ERROR,
				(errcode(ERRCODE_PROTOCOL_VIOLATION),
				 (errmsg("unexpected SCRAM channel-binding attribute in client-final-message"))));
	state->client_final_nonce = read_attr_value(&p, 'r');

	/* ignore optional extensions */
	do
	{
		proof = p - 1;
		value = read_any_attr(&p, &attr);
	} while (attr != 'p');

	client_proof = palloc(pg_b64_dec_len(strlen(value)));
	if (pg_b64_decode(value, strlen(value), client_proof) != SCRAM_KEY_LEN)
		ereport(ERROR,
				(errcode(ERRCODE_PROTOCOL_VIOLATION),
				 errmsg("malformed SCRAM message"),
				 errdetail("Malformed proof in client-final-message.")));
	memcpy(state->ClientProof, client_proof, SCRAM_KEY_LEN);
	pfree(client_proof);

	if (*p != '\0')
		ereport(ERROR,
				(errcode(ERRCODE_PROTOCOL_VIOLATION),
				 errmsg("malformed SCRAM message"),
				 errdetail("Garbage found at the end of client-final-message.")));

	state->client_final_message_without_proof = palloc(proof - begin + 1);
	memcpy(state->client_final_message_without_proof, input, proof - begin);
	state->client_final_message_without_proof[proof - begin] = '\0';
}

/*
 * Build the final server-side message of an exchange.
 */
static char *
build_server_final_message(scram_state *state)
{
	uint8		ServerSignature[SCRAM_KEY_LEN];
	char	   *server_signature_base64;
	int			siglen;
	scram_HMAC_ctx ctx;
	char *res;

	/* calculate ServerSignature */
	scram_HMAC_init(&ctx, state->ServerKey, SCRAM_KEY_LEN);
	scram_HMAC_update(&ctx,
					  state->client_first_message_bare,
					  strlen(state->client_first_message_bare));
	scram_HMAC_update(&ctx, ",", 1);
	scram_HMAC_update(&ctx,
					  state->server_first_message,
					  strlen(state->server_first_message));
	scram_HMAC_update(&ctx, ",", 1);
	scram_HMAC_update(&ctx,
					  state->client_final_message_without_proof,
					  strlen(state->client_final_message_without_proof));
	scram_HMAC_final(ServerSignature, &ctx);

	server_signature_base64 = palloc(pg_b64_enc_len(SCRAM_KEY_LEN) + 1);
	siglen = pg_b64_encode((const char *) ServerSignature,
						   SCRAM_KEY_LEN, server_signature_base64);
	server_signature_base64[siglen] = '\0';

	/*------
	 * The syntax for the server-final-message is: (RFC 5802)
	 *
	 * verifier		   = "v=" base64
	 *					 ;; base-64 encoded ServerSignature.
	 *
	 * server-final-message = (server-error / verifier)
	 *					 ["," extensions]
	 *
	 *------
	 */
	res = psprintf("v=%s", server_signature_base64);
	pfree(server_signature_base64);
	return res;
}


/*
 * Deterministically generate salt for mock authentication, using a SHA256
 * hash based on the username and a cluster-level secret key.  Returns a
 * pointer to a static buffer of size SCRAM_DEFAULT_SALT_LEN.
 */
static char *
GetMockAuthenticationNonce(void)
{
	static char mockNonce[MOCK_AUTH_NONCE_LEN] = "pgpool-II random nonce string";

	return mockNonce;
}
static char *
scram_mock_salt(const char *username)
{
	pg_sha256_ctx ctx;
	static uint8 sha_digest[PG_SHA256_DIGEST_LENGTH];
	char	   *mock_auth_nonce = GetMockAuthenticationNonce();

	/*
	 * Generate salt using a SHA256 hash of the username and the cluster's
	 * mock authentication nonce.  (This works as long as the salt length is
	 * not larger the SHA256 digest length. If the salt is smaller, the caller
	 * will just ignore the extra data.)
	 */
	StaticAssertStmt(PG_SHA256_DIGEST_LENGTH >= SCRAM_DEFAULT_SALT_LEN,
					 "salt length greater than SHA256 digest length");

	pg_sha256_init(&ctx);
	pg_sha256_update(&ctx, (uint8 *) username, strlen(username));
	pg_sha256_update(&ctx, (uint8 *) mock_auth_nonce, MOCK_AUTH_NONCE_LEN);
	pg_sha256_final(&ctx, sha_digest);

	return (char *) sha_digest;
}

/* scram client auth code */

/*
 * Initialize SCRAM exchange status.
 */
void *
pg_fe_scram_init(const char *username, const char *password)
{
	fe_scram_state *state;

	state = (fe_scram_state *) palloc(sizeof(fe_scram_state));
	if (!state)
		return NULL;
	memset(state, 0, sizeof(fe_scram_state));
	state->state = FE_SCRAM_INIT;
	state->username = username;

	state->password = pstrdup(password);

	return state;
}

/*
 * Free SCRAM exchange status
 */
void
pg_fe_scram_free(void *opaq)
{
	fe_scram_state *state = (fe_scram_state *) opaq;

	if (state->password)
		pfree(state->password);

	/* client messages */
	if (state->client_nonce)
		pfree(state->client_nonce);
	if (state->client_first_message_bare)
		pfree(state->client_first_message_bare);
	if (state->client_final_message_without_proof)
		pfree(state->client_final_message_without_proof);

	/* first message from server */
	if (state->server_first_message)
		pfree(state->server_first_message);
	if (state->salt)
		pfree(state->salt);
	if (state->nonce)
		pfree(state->nonce);

	/* final message from server */
	if (state->server_final_message)
		pfree(state->server_final_message);

	pfree(state);
}

/*
 * Exchange a SCRAM message with backend.
 */
void
pg_fe_scram_exchange(void *opaq, char *input, int inputlen,
					 char **output, int *outputlen,
					 bool *done, bool *success)
{
	fe_scram_state *state = (fe_scram_state *) opaq;

	*done = false;
	*success = false;
	*output = NULL;
	*outputlen = 0;

	/*
	 * Check that the input length agrees with the string length of the input.
	 * We can ignore inputlen after this.
	 */
	if (state->state != FE_SCRAM_INIT)
	{
		if (inputlen == 0)
		{
			ereport(ERROR,
					(errmsg("malformed SCRAM message from server (empty message)")));
			goto error;
		}
		if (inputlen != strlen(input))
		{
			ereport(ERROR,
					(errmsg("malformed SCRAM message (length mismatch)")));

			goto error;
		}
	}

	switch (state->state)
	{
		case FE_SCRAM_INIT:
			/* Begin the SCRAM handshake, by sending client nonce */
			*output = build_client_first_message(state);
			if (*output == NULL)
				goto error;

			*outputlen = strlen(*output);
			*done = false;
			state->state = FE_SCRAM_NONCE_SENT;
			break;

		case FE_SCRAM_NONCE_SENT:
			/* Receive salt and server nonce, send response. */
			if (!read_server_first_message(state, input))
				goto error;

			*output = build_client_final_message(state);
			if (*output == NULL)
				goto error;

			*outputlen = strlen(*output);
			*done = false;
			state->state = FE_SCRAM_PROOF_SENT;
			break;

		case FE_SCRAM_PROOF_SENT:
			/* Receive server signature */
			if (!read_server_final_message(state, input))
				goto error;

			/*
			 * Verify server signature, to make sure we're talking to the
			 * genuine server.  XXX: A fake server could simply not require
			 * authentication, though.  There is currently no option in libpq
			 * to reject a connection, if SCRAM authentication did not happen.
			 */
			if (verify_server_signature(state))
				*success = true;
			else
			{
				*success = false;
				ereport(ERROR,
						(errmsg("incorrect server signature")));
			}
			*done = true;
			state->state = FE_SCRAM_FINISHED;
			break;

		default:
			/* shouldn't happen */
			ereport(ERROR,
					(errmsg("invalid SCRAM exchange state")));
			goto error;
	}
	return;

error:
	*done = true;
	*success = false;
	return;
}

/*
 * Build the first exchange message sent by the client.
 */
static char *
build_client_first_message(fe_scram_state *state)
{
	char		raw_nonce[SCRAM_RAW_NONCE_LEN + 1];
	char	   *buf;
	char		buflen;
	int			encoded_len;

	/*
	 * Generate a "raw" nonce.  This is converted to ASCII-printable form by
	 * base64-encoding it.
	 */
	pool_random(raw_nonce, SCRAM_RAW_NONCE_LEN);

	state->client_nonce = palloc(pg_b64_enc_len(SCRAM_RAW_NONCE_LEN) + 1);

	encoded_len = pg_b64_encode(raw_nonce, SCRAM_RAW_NONCE_LEN, state->client_nonce);
	state->client_nonce[encoded_len] = '\0';

	/*
	 * Generate message.  The username is left empty as the backend uses the
	 * value provided by the startup packet.  Also, as this username is not
	 * prepared with SASLprep, the message parsing would fail if it includes
	 * '=' or ',' characters.
	 */
	buflen = 8 + strlen(state->client_nonce) + 1;
	buf = palloc(buflen);

	snprintf(buf, buflen, "n,,n=,r=%s", state->client_nonce);

	state->client_first_message_bare = pstrdup(buf + 3);

	return buf;
}

/*
 * Build the final exchange message sent from the client.
 */
static char *
build_client_final_message(fe_scram_state *state)
{
	StringInfoData buf;
	uint8		client_proof[SCRAM_KEY_LEN];
	char	   *result;

	initStringInfo(&buf);

	/*
	 * Construct client-final-message-without-proof.  We need to remember it
	 * for verifying the server proof in the final step of authentication.
	 */
	appendStringInfo(&buf, "c=biws,r=%s", state->nonce);

	state->client_final_message_without_proof = pstrdup(buf.data);

	/* Append proof to it, to form client-final-message. */
	calculate_client_proof(state,
						   state->client_final_message_without_proof,
						   client_proof);
	/* /// */
	appendStringInfoString(&buf, ",p=");

	/*
	 * char *encoded_data = palloc(pg_b64_enc_len(SCRAM_KEY_LEN) + 1); int len
	 * = encoded_data = pg_b64_encode((char *) client_proof, SCRAM_KEY_LEN,
	 * encoded_data); encoded_data[len] = '\0'; appendStringInfoString(&buf,
	 * encoded_data);
	 */

	enlargeStringInfo(&buf, pg_b64_enc_len(SCRAM_KEY_LEN));
	buf.len += pg_b64_encode((char *) client_proof,
							 SCRAM_KEY_LEN,
							 buf.data + buf.len);
	buf.data[buf.len] = '\0';

	result = pstrdup(buf.data);

	pfree(buf.data);
	return result;
}

/*
 * Read the first exchange message coming from the server.
 */
static bool
read_server_first_message(fe_scram_state *state, char *input)
{
	char	   *iterations_str;
	char	   *endptr;
	char	   *encoded_salt;
	char	   *nonce;

	state->server_first_message = pstrdup(input);

	/* parse the message */
	nonce = read_attr_value(&input, 'r');
	if (nonce == NULL)
	{
		/* read_attr_value() has generated an error string */
		return false;
	}

	/* Verify immediately that the server used our part of the nonce */
	if (strlen(nonce) < strlen(state->client_nonce) ||
		memcmp(nonce, state->client_nonce, strlen(state->client_nonce)) != 0)
	{
		ereport(ERROR,
				(errmsg("invalid SCRAM response from server(nonce mismatch)")));
		return false;
	}

	state->nonce = pstrdup(nonce);

	encoded_salt = read_attr_value(&input, 's');
	if (encoded_salt == NULL)
	{
		/* read_attr_value() has generated an error string */
		return false;
	}
	state->salt = palloc(pg_b64_dec_len(strlen(encoded_salt)));

	state->saltlen = pg_b64_decode(encoded_salt,
								   strlen(encoded_salt),
								   state->salt);

	iterations_str = read_attr_value(&input, 'i');
	if (iterations_str == NULL)
	{
		/* read_attr_value() has generated an error string */
		return false;
	}
	state->iterations = strtol(iterations_str, &endptr, 10);
	if (*endptr != '\0' || state->iterations < 1)
	{
		ereport(ERROR,
				(errmsg("malformed SCRAM message from server (invalid iteration count)")));
		return false;
	}

	if (*input != '\0')
		ereport(WARNING,
				(errmsg("malformed SCRAM message from server (garbage at end of server-first-message)")));

	return true;
}

/*
 * Read the final exchange message coming from the server.
 */
static bool
read_server_final_message(fe_scram_state *state, char *input)
{
	char	   *encoded_server_signature;
	int			server_signature_len;

	state->server_final_message = pstrdup(input);

	/* Check for error result. */
	if (*input == 'e')
	{
		char	   *err = read_attr_value(&input, 'e');

		ereport(ERROR,
				(errmsg("error received from server in SCRAM exchange: %s", err)));
		return false;
	}

	/* Parse the message. */
	encoded_server_signature = read_attr_value(&input, 'v');
	if (encoded_server_signature == NULL)
	{
		/* read_attr_value() has generated an error message */
		return false;
	}

	if (*input != '\0')
		ereport(WARNING,
				(errmsg("malformed SCRAM message (garbage at end of server-final-message)")));

	server_signature_len = pg_b64_decode(encoded_server_signature,
										 strlen(encoded_server_signature),
										 state->ServerSignature);
	if (server_signature_len != SCRAM_KEY_LEN)
	{
		ereport(ERROR,
				(errmsg("malformed SCRAM message from server (invalid server signature)")));
		return false;
	}

	return true;
}

/*
 * Calculate the client proof, part of the final exchange message sent
 * by the client.
 */
static void
calculate_client_proof(fe_scram_state *state,
					   const char *client_final_message_without_proof,
					   uint8 *result)
{
	uint8		StoredKey[SCRAM_KEY_LEN];
	uint8		ClientKey[SCRAM_KEY_LEN];
	uint8		ClientSignature[SCRAM_KEY_LEN];
	int			i;
	scram_HMAC_ctx ctx;

	/*
	 * Calculate SaltedPassword, and store it in 'state' so that we can reuse
	 * it later in verify_server_signature.
	 */
	scram_SaltedPassword(state->password, state->salt, state->saltlen,
						 state->iterations, state->SaltedPassword);

	scram_ClientKey(state->SaltedPassword, ClientKey);
	scram_H(ClientKey, SCRAM_KEY_LEN, StoredKey);

	scram_HMAC_init(&ctx, StoredKey, SCRAM_KEY_LEN);
	scram_HMAC_update(&ctx,
					  state->client_first_message_bare,
					  strlen(state->client_first_message_bare));
	scram_HMAC_update(&ctx, ",", 1);
	scram_HMAC_update(&ctx,
					  state->server_first_message,
					  strlen(state->server_first_message));
	scram_HMAC_update(&ctx, ",", 1);
	scram_HMAC_update(&ctx,
					  client_final_message_without_proof,
					  strlen(client_final_message_without_proof));
	scram_HMAC_final(ClientSignature, &ctx);

	for (i = 0; i < SCRAM_KEY_LEN; i++)
		result[i] = ClientKey[i] ^ ClientSignature[i];
}

/*
 * Validate the server signature, received as part of the final exchange
 * message received from the server.
 */
static bool
verify_server_signature(fe_scram_state *state)
{
	uint8		expected_ServerSignature[SCRAM_KEY_LEN];
	uint8		ServerKey[SCRAM_KEY_LEN];
	scram_HMAC_ctx ctx;

	scram_ServerKey(state->SaltedPassword, ServerKey);

	/* calculate ServerSignature */
	scram_HMAC_init(&ctx, ServerKey, SCRAM_KEY_LEN);
	scram_HMAC_update(&ctx,
					  state->client_first_message_bare,
					  strlen(state->client_first_message_bare));
	scram_HMAC_update(&ctx, ",", 1);
	scram_HMAC_update(&ctx,
					  state->server_first_message,
					  strlen(state->server_first_message));
	scram_HMAC_update(&ctx, ",", 1);
	scram_HMAC_update(&ctx,
					  state->client_final_message_without_proof,
					  strlen(state->client_final_message_without_proof));
	scram_HMAC_final(expected_ServerSignature, &ctx);

	if (memcmp(expected_ServerSignature, state->ServerSignature, SCRAM_KEY_LEN) != 0)
		return false;

	return true;
}

/*
 * Build a new SCRAM verifier.
 */
char *
pg_fe_scram_build_verifier(const char *password)
{
	char		saltbuf[SCRAM_DEFAULT_SALT_LEN];
	char	   *result;

	/* Generate a random salt */
	pool_random(saltbuf, SCRAM_DEFAULT_SALT_LEN);

	result = scram_build_verifier(saltbuf, SCRAM_DEFAULT_SALT_LEN,
								  SCRAM_DEFAULT_ITERATIONS, password);

	return result;
}
