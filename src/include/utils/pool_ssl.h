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
 */

#ifndef pool_ssl_h
#define pool_ssl_h

#ifdef USE_SSL
/*
 *	Hardcoded DH parameters, used in ephemeral DH keying.
 *
 *	If you want to create your own hardcoded DH parameters
 *	for fun and profit, review "Assigned Number for SKIP
 *	Protocols" (http://www.skip-vpn.org/spec/numbers.html)
 *	for suggestions.
 */
#define FILE_DH2048 \
"-----BEGIN DH PARAMETERS-----\n\
MIIBCAKCAQEA9kJXtwh/CBdyorrWqULzBej5UxE5T7bxbrlLOCDaAadWoxTpj0BV\n\
89AHxstDqZSt90xkhkn4DIO9ZekX1KHTUPj1WV/cdlJPPT2N286Z4VeSWc39uK50\n\
T8X8dryDxUcwYc58yWb/Ffm7/ZFexwGq01uejaClcjrUGvC/RgBYK+X0iP1YTknb\n\
zSC0neSRBzZrM2w4DUUdD3yIsxx8Wy2O9vPJI8BD8KVbGI2Ou1WMuF040zT9fBdX\n\
Q6MdGGzeMyEstSr/POGxKUAYEY18hKcKctaGxAMZyAcpesqVDNmWn6vQClCbAkbT\n\
CD1mpF1Bn5x8vYlLIhkmuquiXsNV6TILOwIBAg==\n\
-----END DH PARAMETERS-----\n"
#endif

/*
 * Macro that allows to cast constness away from an expression, but doesn't
 * allow changing the underlying type.  Enforcement of the latter
 * currently only works for gcc like compilers.
 *
 * Please note IT IS NOT SAFE to cast constness away if the result will ever
 * be modified (it would be undefined behaviour). Doing so anyway can cause
 * compiler misoptimizations or runtime crashes (modifying readonly memory).
 * It is only safe to use when the the result will not be modified, but API
 * design or language restrictions prevent you from declaring that
 * (e.g. because a function returns both const and non-const variables).
 *
 * Note that this only works in function scope, not for global variables (it'd
 * be nice, but not trivial, to improve that).
 */
#if defined(HAVE__BUILTIN_TYPES_COMPATIBLE_P)
#define unconstify(underlying_type, expr) \
	(StaticAssertExpr(__builtin_types_compatible_p(__typeof(expr), const underlying_type), \
					  "wrong cast"), \
	  (underlying_type) (expr))
#else
#define unconstify(underlying_type, expr) \
	((underlying_type) (expr))
#endif


extern void pool_ssl_negotiate_serverclient(POOL_CONNECTION * cp);
extern void pool_ssl_negotiate_clientserver(POOL_CONNECTION * cp);
extern void pool_ssl_close(POOL_CONNECTION * cp);
extern int	pool_ssl_read(POOL_CONNECTION * cp, void *buf, int size);
extern int	pool_ssl_write(POOL_CONNECTION * cp, const void *buf, int size);
extern bool pool_ssl_pending(POOL_CONNECTION * cp);
extern int	SSL_ServerSide_init(void);


#endif /* pool_ssl_h */
