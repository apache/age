/*
 *	md5.c
 *
 *	Implements	the  MD5 Message-Digest Algorithm as specified in
 *	RFC  1321.	This  implementation  is a simple one, in that it
 *	needs  every  input  byte  to  be  buffered  before doing any
 *	calculations.  I  do  not  expect  this  file  to be used for
 *	general  purpose  MD5'ing  of large amounts of data, only for
 *	generating hashed passwords from limited input.
 *
 *	Sverre H. Huseby <sverrehu@online.no>
 *
 *	Portions Copyright (c) 1996-2005, PostgreSQL Global Development Group
 *	Portions Copyright (c) 1994, Regents of the University of California
 *
 *  This file is imported from PostgreSQL 8.1.3., and modified by
 *  Taiki Yamaguchi <yamaguchi@sraoss.co.jp>
 *
 * IDENTIFICATION
 *	$Header$
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "pool.h"
#include "auth/md5.h"
#include "utils/palloc.h"


#ifdef NOT_USED
typedef unsigned char uint8;	/* == 8 bits */
typedef unsigned int uint32;	/* == 32 bits */
#endif

#define S11 7
#define S12 12
#define S13 17
#define S14 22
#define S21 5
#define S22 9
#define S23 14
#define S24 20
#define S31 4
#define S32 11
#define S33 16
#define S34 23
#define S41 6
#define S42 10
#define S43 15
#define S44 21

#define F(x, y, z) (((x) & (y)) | ((~x) & (z)))
#define G(x, y, z) (((x) & (z)) | ((y) & (~z)))
#define H(x, y, z) ((x) ^ (y) ^ (z))
#define I(x, y, z) ((y) ^ ((x) | (~z)))

#define ROTATE_LEFT(x, n) (((x) << (n)) | ((x) >> (32 - (n))))

#define FF(a, b, c, d, x, s, ac) { \
(a) += F((b), (c), (d)) + (x) + (uint32)(ac); \
(a) = ROTATE_LEFT((a), (s)); \
(a) += (b); \
}

#define GG(a, b, c, d, x, s, ac) { \
(a) += G((b), (c), (d)) + (x) + (uint32)(ac); \
(a) = ROTATE_LEFT((a), (s)); \
(a) += (b); \
}

#define HH(a, b, c, d, x, s, ac) { \
(a) += H((b), (c), (d)) + (x) + (uint32)(ac); \
(a) = ROTATE_LEFT((a), (s)); \
(a) += (b); \
}

#define II(a, b, c, d, x, s, ac) { \
(a) += I((b), (c), (d)) + (x) + (uint32)(ac); \
(a) = ROTATE_LEFT((a), (s)); \
(a) += (b); \
}

const uint32 T[64] =
{
	0xd76aa478, 0xe8c7b756, 0x242070db, 0xc1bdceee, 0xf57c0faf, 0x4787c62a, 0xa8304613, 0xfd469501,
	0x698098d8, 0x8b44f7af, 0xffff5bb1, 0x895cd7be, 0x6b901122, 0xfd987193, 0xa679438e, 0x49b40821,

	0xf61e2562, 0xc040b340, 0x265e5a51, 0xe9b6c7aa, 0xd62f105d, 0x02441453, 0xd8a1e681, 0xe7d3fbc8,
	0x21e1cde6, 0xc33707d6, 0xf4d50d87, 0x455a14ed, 0xa9e3e905, 0xfcefa3f8, 0x676f02d9, 0x8d2a4c8a,

	0xfffa3942, 0x8771f681, 0x6d9d6122, 0xfde5380c, 0xa4beea44, 0x4bdecfa9, 0xf6bb4b60, 0xbebfbc70,
	0x289b7ec6, 0xeaa127fa, 0xd4ef3085, 0x04881d05, 0xd9d4d039, 0xe6db99e5, 0x1fa27cf8, 0xc4ac5665,

	0xf4292244, 0x432aff97, 0xab9423a7, 0xfc93a039, 0x655b59c3, 0x8f0ccc92, 0xffeff47d, 0x85845dd1,
	0x6fa87e4f, 0xfe2ce6e0, 0xa3014314, 0x4e0811a1, 0xf7537e82, 0xbd3af235, 0x2ad7d2bb, 0xeb86d391
};


/*
 *	PRIVATE FUNCTIONS
 */


/*
 *	The returned array is allocated using malloc.  the caller should free it
 *	when it is no longer needed.
 */
static uint8 *
createPaddedCopyWithLength(uint8 *b, uint32 *l)
{
	/*
	 * uint8 *b  - message to be digested uint32 *l - length of b
	 */
	uint8	   *ret;
	uint32		q;
	uint32		len,
				newLen448;
	uint32		len_high,
				len_low;		/* 64-bit value split into 32-bit sections */

	len = ((b == NULL) ? 0 : *l);
	newLen448 = len + 64 - (len % 64) - 8;
	if (newLen448 <= len)
		newLen448 += 64;

	*l = newLen448 + 8;
	if ((ret = (uint8 *) malloc(sizeof(uint8) * *l)) == NULL)
		return NULL;

	if (b != NULL)
		memcpy(ret, b, sizeof(uint8) * len);

	/* pad */
	ret[len] = 0x80;
	for (q = len + 1; q < newLen448; q++)
		ret[q] = 0x00;

	/* append length as a 64 bit bitcount */
	len_low = len;
	/* split into two 32-bit values */
	/* we only look at the bottom 32-bits */
	len_high = len >> 29;
	len_low <<= 3;
	q = newLen448;
	ret[q++] = (len_low & 0xff);
	len_low >>= 8;
	ret[q++] = (len_low & 0xff);
	len_low >>= 8;
	ret[q++] = (len_low & 0xff);
	len_low >>= 8;
	ret[q++] = (len_low & 0xff);
	ret[q++] = (len_high & 0xff);
	len_high >>= 8;
	ret[q++] = (len_high & 0xff);
	len_high >>= 8;
	ret[q++] = (len_high & 0xff);
	len_high >>= 8;
	ret[q] = (len_high & 0xff);

	return ret;
}

static void
doTheRounds(uint32 X[16], uint32 state[4])
{
	uint32		a,
				b,
				c,
				d;

	a = state[0];
	b = state[1];
	c = state[2];
	d = state[3];

	/* round 1 */
	FF(a, b, c, d, X[0], S11, T[0]);
	FF(d, a, b, c, X[1], S12, T[1]);
	FF(c, d, a, b, X[2], S13, T[2]);
	FF(b, c, d, a, X[3], S14, T[3]);
	FF(a, b, c, d, X[4], S11, T[4]);
	FF(d, a, b, c, X[5], S12, T[5]);
	FF(c, d, a, b, X[6], S13, T[6]);
	FF(b, c, d, a, X[7], S14, T[7]);
	FF(a, b, c, d, X[8], S11, T[8]);
	FF(d, a, b, c, X[9], S12, T[9]);
	FF(c, d, a, b, X[10], S13, T[10]);
	FF(b, c, d, a, X[11], S14, T[11]);
	FF(a, b, c, d, X[12], S11, T[12]);
	FF(d, a, b, c, X[13], S12, T[13]);
	FF(c, d, a, b, X[14], S13, T[14]);
	FF(b, c, d, a, X[15], S14, T[15]);

	GG(a, b, c, d, X[1], S21, T[16]);
	GG(d, a, b, c, X[6], S22, T[17]);
	GG(c, d, a, b, X[11], S23, T[18]);
	GG(b, c, d, a, X[0], S24, T[19]);
	GG(a, b, c, d, X[5], S21, T[20]);
	GG(d, a, b, c, X[10], S22, T[21]);
	GG(c, d, a, b, X[15], S23, T[22]);
	GG(b, c, d, a, X[4], S24, T[23]);
	GG(a, b, c, d, X[9], S21, T[24]);
	GG(d, a, b, c, X[14], S22, T[25]);
	GG(c, d, a, b, X[3], S23, T[26]);
	GG(b, c, d, a, X[8], S24, T[27]);
	GG(a, b, c, d, X[13], S21, T[28]);
	GG(d, a, b, c, X[2], S22, T[29]);
	GG(c, d, a, b, X[7], S23, T[30]);
	GG(b, c, d, a, X[12], S24, T[31]);

	HH(a, b, c, d, X[5], S31, T[32]);
	HH(d, a, b, c, X[8], S32, T[33]);
	HH(c, d, a, b, X[11], S33, T[34]);
	HH(b, c, d, a, X[14], S34, T[35]);
	HH(a, b, c, d, X[1], S31, T[36]);
	HH(d, a, b, c, X[4], S32, T[37]);
	HH(c, d, a, b, X[7], S33, T[38]);
	HH(b, c, d, a, X[10], S34, T[39]);
	HH(a, b, c, d, X[13], S31, T[40]);
	HH(d, a, b, c, X[0], S32, T[41]);
	HH(c, d, a, b, X[3], S33, T[42]);
	HH(b, c, d, a, X[6], S34, T[43]);
	HH(a, b, c, d, X[9], S31, T[44]);
	HH(d, a, b, c, X[12], S32, T[45]);
	HH(c, d, a, b, X[15], S33, T[46]);
	HH(b, c, d, a, X[2], S34, T[47]);

	II(a, b, c, d, X[0], S41, T[48]);
	II(d, a, b, c, X[7], S42, T[49]);
	II(c, d, a, b, X[14], S43, T[50]);
	II(b, c, d, a, X[5], S44, T[51]);
	II(a, b, c, d, X[12], S41, T[52]);
	II(d, a, b, c, X[3], S42, T[53]);
	II(c, d, a, b, X[10], S43, T[54]);
	II(b, c, d, a, X[1], S44, T[55]);
	II(a, b, c, d, X[8], S41, T[56]);
	II(d, a, b, c, X[15], S42, T[57]);
	II(c, d, a, b, X[6], S43, T[58]);
	II(b, c, d, a, X[13], S44, T[59]);
	II(a, b, c, d, X[4], S41, T[60]);
	II(d, a, b, c, X[11], S42, T[61]);
	II(c, d, a, b, X[2], S43, T[62]);
	II(b, c, d, a, X[9], S44, T[63]);

	state[0] += a;
	state[1] += b;
	state[2] += c;
	state[3] += d;
}

static int
calculateDigestFromBuffer(uint8 *b, uint32 len, uint8 sum[16])
{
	/*
	 * uint8 *b      - message to be digested uint32 len    - length of b
	 * uint8 sum[16] - md5 digest calculated from b
	 */

	register uint32 i,
				j,
				k,
				newI;
	uint32		l;
	uint8	   *input;
	register uint32 *wbp;
	uint32		workBuff[16],
				state[4];

	l = len;

	state[0] = 0x67452301;
	state[1] = 0xEFCDAB89;
	state[2] = 0x98BADCFE;
	state[3] = 0x10325476;

	if ((input = createPaddedCopyWithLength(b, &l)) == NULL)
		return 0;

	for (i = 0;;)
	{
		if ((newI = i + 16 * 4) > l)
			break;
		k = i + 3;
		for (j = 0; j < 16; j++)
		{
			wbp = (workBuff + j);
			*wbp = input[k--];
			*wbp <<= 8;
			*wbp |= input[k--];
			*wbp <<= 8;
			*wbp |= input[k--];
			*wbp <<= 8;
			*wbp |= input[k];
			k += 7;
		}
		doTheRounds(workBuff, state);
		i = newI;
	}
	free(input);

	j = 0;
	for (i = 0; i < 4; i++)
	{
		k = state[i];
		sum[j++] = (k & 0xff);
		k >>= 8;
		sum[j++] = (k & 0xff);
		k >>= 8;
		sum[j++] = (k & 0xff);
		k >>= 8;
		sum[j++] = (k & 0xff);
	}
	return 1;
}

void
bytesToHex(char *b, int len, char *s)
{
	static const char *hex = "0123456789abcdef";
	int			q,
				w;

	for (q = 0, w = 0; q < len; q++)
	{
		s[w++] = hex[(b[q] >> 4) & 0x0F];
		s[w++] = hex[b[q] & 0x0F];
	}
	s[w] = '\0';
}

/*
 *	PUBLIC FUNCTIONS
 */

/*
 *	pool_md5_hash
 *
 *	Calculates the MD5 sum of the bytes in a buffer.
 *
 *	SYNOPSIS	  int pool_md5_hash(const void *buff, size_t len, char *hexsum)
 *
 *	INPUT		  buff	  the buffer containing the bytes that you want
 *						  the MD5 sum of.
 *				  len	  number of bytes in the buffer.
 *
 *	OUTPUT		  hexsum  the MD5 sum as a '\0'-terminated string of
 *						  hexadecimal digits.  an MD5 sum is 16 bytes long.
 *						  each byte is represented by two hexadecimal
 *						  characters.  you thus need to provide an array
 *						  of 33 characters, including the trailing '\0'.
 *
 *	RETURNS		  false on failure (out of memory for internal buffers) or
 *				  true on success.
 *
 *	STANDARDS	  MD5 is described in RFC 1321.
 *
 *	AUTHOR		  Sverre H. Huseby <sverrehu@online.no>
 *  MODIFIED by   Taiki Yamaguchi <yamaguchi@sraoss.co.jp>
 *
 */
int
pool_md5_hash(const void *buff, size_t len, char *hexsum)
{
	uint8		sum[16];

	if (!calculateDigestFromBuffer((uint8 *) buff, len, sum))
		return 0;				/* failed */

	bytesToHex((char *) sum, 16, hexsum);
	return 1;					/* success */
}

/*
 * Computes MD5 checksum of "passwd" (a null-terminated string) followed
 * by "salt" (which need not be null-terminated).
 *
 * Output format is a 32-hex-digit MD5 checksum.
 * Hence, the output buffer "buf" must be at least 33 bytes long.
 *
 * Returns 1 if okay, 0 on error (out of memory).
 */
int
pool_md5_encrypt(const char *passwd, const char *salt, size_t salt_len,
				 char *buf)
{
	size_t		passwd_len = strlen(passwd);
	char	   *crypt_buf = malloc(passwd_len + salt_len);
	int			ret;

	if (!crypt_buf)
		return 0;				/* failed */

	/*
	 * Place salt at the end because it may be known by users trying to crack
	 * the MD5 output.
	 */
	strcpy(crypt_buf, passwd);
	memcpy(crypt_buf + passwd_len, salt, salt_len);

	ret = pool_md5_hash(crypt_buf, passwd_len + salt_len, buf);

	free(crypt_buf);

	return ret;
}

/*
 * Computes MD5 checksum of "passwd" (a null-terminated string) followed
 * by "salt" (which need not be null-terminated).
 *
 * Output format is "md5" followed by a 32-hex-digit MD5 checksum.
 * Hence, the output buffer "buf" must be at least 36 bytes long.
 *
 * Returns TRUE if okay, FALSE on error (out of memory).
 */
bool
pg_md5_encrypt(const char *passwd, const char *salt, size_t salt_len,
			   char *buf)
{
	size_t		passwd_len = strlen(passwd);

	/* +1 here is just to avoid risk of unportable malloc(0) */
	char	   *crypt_buf = palloc(passwd_len + salt_len + 1);
	bool		ret;

	/*
	 * Place salt at the end because it may be known by users trying to crack
	 * the MD5 output.
	 */
	memcpy(crypt_buf, passwd, passwd_len);
	memcpy(crypt_buf + passwd_len, salt, salt_len);

	strcpy(buf, "md5");
	ret = pool_md5_hash(crypt_buf, passwd_len + salt_len, buf + 3);

	pfree(crypt_buf);

	return ret;
}
