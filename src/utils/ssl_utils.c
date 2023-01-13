/*
 * $Header$
 *
 * Handles watchdog connection, and protocol communication with pgpool-II
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
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <sys/un.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/wait.h>
#include <errno.h>

#include "pool.h"
#include "auth/md5.h"
#include "utils/elog.h"
#include "utils/palloc.h"
#include "utils/memutils.h"
#include "pool_config.h"
#include "utils/ssl_utils.h"

#ifdef USE_SSL
static int	aes_get_key(const char *password, unsigned char *key, unsigned char *iv);
static int aes_encrypt(unsigned char *plaintext, int plaintext_len, unsigned char *key,
			unsigned char *iv, unsigned char *ciphertext);

static int aes_decrypt(unsigned char *ciphertext, int ciphertext_len, unsigned char *key,
			unsigned char *iv, unsigned char *plaintext);
#endif

#ifdef USE_SSL

int
aes_encrypt_with_password(unsigned char *plaintext, int plaintext_len,
						  const char *password, unsigned char *ciphertext)
{
	unsigned char key[EVP_MAX_KEY_LENGTH],
				iv[EVP_MAX_IV_LENGTH];

	/* First get the key and iv using the password */
	if (aes_get_key(password, key, iv) != 0)
		return -1;
	return aes_encrypt(plaintext, plaintext_len, key, iv, ciphertext);
}

int
aes_decrypt_with_password(unsigned char *ciphertext, int ciphertext_len,
						  const char *password, unsigned char *plaintext)
{
	unsigned char key[EVP_MAX_KEY_LENGTH],
				iv[EVP_MAX_IV_LENGTH];

	/* First get the key and iv using the password */
	if (aes_get_key(password, key, iv) != 0)
		return -1;
	return aes_decrypt(ciphertext, ciphertext_len, key,
					   iv, plaintext);
}

/*
 * key must be EVP_MAX_KEY_LENGTH length
 * iv must be EVP_MAX_IV_LENGTH length
 */
static int
aes_get_key(const char *password, unsigned char *key, unsigned char *iv)
{
	const unsigned char *salt = NULL;

	OpenSSL_add_all_algorithms();

	if (!EVP_BytesToKey(EVP_aes_256_cbc(), EVP_sha1(), salt,
						(unsigned char *) password,
						strlen(password), 1, key, iv))
	{
		ereport(ERROR,
				(errmsg("unable to generate AES key from password"),
				 errdetail("EVP_BytesToKey failed")));
		return 1;
	}

#ifdef DEBUG_ENCRYPT
	printf("Key: ");
	for (i = 0; i < EVP_aes_256_cbc()->key_len; ++i)
	{
		printf("%02x", key[i]);
	} printf("\n");
	printf("IV: ");
	for (i = 0; i < EVP_aes_256_cbc()->iv_len; ++i)
	{
		printf("%02x", iv[i]);
	} printf("\n");
#endif
	return 0;
}

/*
 * from: https://wiki.openssl.org/index.php/EVP_Symmetric_Encryption_and_Decryption
 */
static int
aes_encrypt(unsigned char *plaintext, int plaintext_len, unsigned char *key,
			unsigned char *iv, unsigned char *ciphertext)
{
	EVP_CIPHER_CTX *ctx = NULL;
	int			len;
	int			ciphertext_len;

	/* Create and initialise the context */
	if (!(ctx = EVP_CIPHER_CTX_new()))
		goto encrypt_error;

	/*
	 * Initialise the encryption operation. IMPORTANT - ensure you use a key
	 * and IV size appropriate for your cipher In this example we are using
	 * 256 bit AES (i.e. a 256 bit key). The IV size for *most* modes is the
	 * same as the block size. For AES this is 128 bits
	 */
	if (EVP_EncryptInit_ex(ctx, EVP_aes_256_cbc(), NULL, key, iv) != 1)
		goto encrypt_error;

	/*
	 * Provide the message to be encrypted, and obtain the encrypted output.
	 * EVP_EncryptUpdate can be called multiple times if necessary
	 */
	if (EVP_EncryptUpdate(ctx, ciphertext, &len, plaintext, plaintext_len) != 1)
		goto encrypt_error;
	ciphertext_len = len;

	/*
	 * Finalise the encryption. Further ciphertext bytes may be written at
	 * this stage.
	 */
	if (EVP_EncryptFinal_ex(ctx, ciphertext + len, &len) != 1)
		goto encrypt_error;
	ciphertext_len += len;

	/* Clean up */
	EVP_CIPHER_CTX_free(ctx);

	return ciphertext_len;

encrypt_error:
	if (ctx)
		EVP_CIPHER_CTX_free(ctx);
	return -1;
}

static int
aes_decrypt(unsigned char *ciphertext, int ciphertext_len, unsigned char *key,
			unsigned char *iv, unsigned char *plaintext)
{
	EVP_CIPHER_CTX *ctx = NULL;
	int			len;
	int			plaintext_len;

	/* Create and initialise the context */
	if (!(ctx = EVP_CIPHER_CTX_new()))
		goto decrypt_error;

	/*
	 * Initialise the decryption operation. IMPORTANT - ensure you use a key
	 * and IV size appropriate for your cipher In this example we are using
	 * 256 bit AES (i.e. a 256 bit key). The IV size for *most* modes is the
	 * same as the block size. For AES this is 128 bits
	 */
	if (EVP_DecryptInit_ex(ctx, EVP_aes_256_cbc(), NULL, key, iv) != 1)
		goto decrypt_error;

	/*
	 * Provide the message to be decrypted, and obtain the plaintext output.
	 * EVP_DecryptUpdate can be called multiple times if necessary
	 */
	if (EVP_DecryptUpdate(ctx, plaintext, &len, ciphertext, ciphertext_len) != 1)
		goto decrypt_error;
	plaintext_len = len;

	/*
	 * Finalise the decryption. Further plaintext bytes may be written at this
	 * stage.
	 */
	if (EVP_DecryptFinal_ex(ctx, plaintext + len, &len) != 1)
		goto decrypt_error;
	plaintext_len += len;

	/* Clean up */
	EVP_CIPHER_CTX_free(ctx);

	return plaintext_len;

decrypt_error:
	if (ctx)
		EVP_CIPHER_CTX_free(ctx);
	return -1;
}

/* HMAC SHA-256*/
void
calculate_hmac_sha256(const char *data, int len, char *buf)
{
	char	   *key = pool_config->wd_authkey;
	char		str[WD_AUTH_HASH_LEN / 2];
	unsigned int res_len = WD_AUTH_HASH_LEN;
	HMAC_CTX   *ctx = NULL;

#if (OPENSSL_VERSION_NUMBER >= 0x10100000L && !defined (LIBRESSL_VERSION_NUMBER))
	ctx = HMAC_CTX_new();
	HMAC_CTX_reset(ctx);
#else
	HMAC_CTX	ctx_obj;

	ctx = &ctx_obj;
	HMAC_CTX_init(ctx);
#endif
	HMAC_Init_ex(ctx, key, strlen(key), EVP_sha256(), NULL);
	HMAC_Update(ctx, (unsigned char *) data, len);
	HMAC_Final(ctx, (unsigned char *) str, &res_len);
#if (OPENSSL_VERSION_NUMBER >= 0x10100000L && !defined (LIBRESSL_VERSION_NUMBER))
	HMAC_CTX_reset(ctx);
	HMAC_CTX_free(ctx);
#else
	HMAC_CTX_cleanup(ctx);
#endif
	bytesToHex(str, 32, buf);
	buf[WD_AUTH_HASH_LEN] = '\0';
}

#else

int
aes_decrypt_with_password(unsigned char *ciphertext, int ciphertext_len,
						  const char *password, unsigned char *plaintext)
{
	ereport(ERROR,
			(errmsg("AES decryption is not supported by this build"),
			 errhint("Compile with --with-openssl to use AES.")));
	return -1;
}
int
aes_encrypt_with_password(unsigned char *plaintext, int plaintext_len,
						  const char *password, unsigned char *ciphertext)
{
	ereport(ERROR,
			(errmsg("AES encryption is not supported by this build"),
			 errhint("Compile with --with-openssl to use AES.")));
	return -1;
}
void
calculate_hmac_sha256(const char *data, int len, char *buf)
{
	ereport(ERROR,
			(errmsg("HMAC SHA256 encryption is not supported by this build"),
			 errhint("Compile with --with-openssl to use AES.")));
}
#endif
