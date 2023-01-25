/* -*-pgsql-c-*- */
/*
 *
 * $Header$
 *
 * pgpool: a language independent connection pool server for PostgreSQL
 * written by Tatsuo Ishii
 *
 * Copyright (c) 2003-2015	PgPool Global Development Group
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
 * pool_type.h.: type definition header file
 *
 */

#ifndef POOL_TYPE_H
#define POOL_TYPE_H

#include "config.h"
#include <sys/types.h>
#include <sys/socket.h>
#include <stddef.h>
#include "libpq-fe.h"
/* Define common boolean type. C++ and BEOS already has it so exclude them. */
#ifdef c_plusplus
#ifndef __cplusplus
#define __cplusplus
#endif							/* __cplusplus */
#endif							/* c_plusplus */

#ifndef __BEOS__
#ifndef __cplusplus
#ifndef bool
typedef char bool;
#endif
#ifndef true
#define true ((bool) 1)
#endif
#ifndef TRUE
#define TRUE ((bool) 1)
#endif
#ifndef false
#define false ((bool) 0)
#endif
#ifndef FALSE
#define FALSE ((bool) 0)
#endif
#endif							/* not C++ */
#endif							/* __BEOS__ */

/* ----------------------------------------------------------------
 *              Section 5:  offsetof, lengthof, endof, alignment
 * ----------------------------------------------------------------
 */
/*
 * offsetof
 *      Offset of a structure/union field within that structure/union.
 *
 *      XXX This is supposed to be part of stddef.h, but isn't on
 *      some systems (like SunOS 4).
 */
#ifndef offsetof
#define offsetof(type, field)   ((long) &((type *)0)->field)
#endif

#define PointerIsValid(pointer) ((const void*)(pointer) != NULL)
typedef signed char int8;		/* == 8 bits */
typedef signed short int16;		/* == 16 bits */
typedef signed int int32;		/* == 32 bits */

/*
 * uintN
 *		Unsigned integer, EXACTLY N BITS IN SIZE,
 *		used for numerical computations and the
 *		frontend/backend protocol.
 */
typedef unsigned char uint8;	/* == 8 bits */
typedef unsigned short uint16;	/* == 16 bits */
typedef unsigned int uint32;	/* == 32 bits */

#ifdef HAVE_LONG_INT_64
/* Plain "long int" fits, use it */
typedef long int int64;
typedef unsigned long int uint64;

#define pool_atoi64 atol
#elif defined(HAVE_LONG_LONG_INT_64)
/* We have working support for "long long int", use that */
typedef long long int int64;
typedef unsigned long long int uint64;
#define pool_atoi64 atoll
#else
/* neither HAVE_LONG_INT_64 nor HAVE_LONG_LONG_INT_64 */
#error must have a working 64-bit integer datatype
#endif

typedef enum
{
	LOAD_UNSELECTED = 0,
	LOAD_SELECTED
}			LOAD_BALANCE_STATUS;

extern int	assert_enabled;
extern void ExceptionalCondition(const char *conditionName,
					 const char *errorType,
					 const char *fileName, int lineNumber) __attribute__((noreturn));

#define MAXIMUM_ALIGNOF 8

#define TYPEALIGN(ALIGNVAL,LEN)  \
	(((intptr_t) (LEN) + ((ALIGNVAL) - 1)) & ~((intptr_t) ((ALIGNVAL) - 1)))

#define SHORTALIGN(LEN)			TYPEALIGN(ALIGNOF_SHORT, (LEN))
#define INTALIGN(LEN)			TYPEALIGN(ALIGNOF_INT, (LEN))
#define LONGALIGN(LEN)			TYPEALIGN(ALIGNOF_LONG, (LEN))
#define DOUBLEALIGN(LEN)		TYPEALIGN(ALIGNOF_DOUBLE, (LEN))
#define MAXALIGN(LEN)			TYPEALIGN(MAXIMUM_ALIGNOF, (LEN))

/*
 *  It seems that sockaddr_storage is now commonly used in place of sockaddr.
 *  So, define it if it is not define yet, and create new SockAddr structure
 *  that uses sockaddr_storage.
 */
#ifdef HAVE_STRUCT_SOCKADDR_STORAGE

#ifndef HAVE_STRUCT_SOCKADDR_STORAGE_SS_FAMILY
#ifdef HAVE_STRUCT_SOCKADDR_STORAGE___SS_FAMILY
#define ss_family __ss_family
#else
#error struct sockaddr_storage does not provide an ss_family member
#endif							/* HAVE_STRUCT_SOCKADDR_STORAGE___SS_FAMILY */
#endif							/* HAVE_STRUCT_SOCKADDR_STORAGE_SS_FAMILY */

#ifdef HAVE_STRUCT_SOCKADDR_STORAGE___SS_LEN
#define ss_len __ss_len
#define HAVE_STRUCT_SOCKADDR_STORAGE_SS_LEN 1
#endif							/* HAVE_STRUCT_SOCKADDR_STORAGE___SS_LEN */

#else							/* !HAVE_STRUCT_SOCKADDR_STORAGE */

/* Define a struct sockaddr_storage if we don't have one. */
struct sockaddr_storage
{
	union
	{
		struct sockaddr sa;		/* get the system-dependent fields */
		long int	ss_align;	/* ensures struct is properly aligned.
								 * original uses int64 */
		char		ss_pad[128];	/* ensures struct has desired size */
	}
				ss_stuff;
};

#define ss_family   ss_stuff.sa.sa_family
/* It should have an ss_len field if sockaddr has sa_len. */
#ifdef HAVE_STRUCT_SOCKADDR_SA_LEN
#define ss_len      ss_stuff.sa.sa_len
#define HAVE_STRUCT_SOCKADDR_STORAGE_SS_LEN 1
#endif
#endif							/* HAVE_STRUCT_SOCKADDR_STORAGE */

typedef struct
{
	struct sockaddr_storage addr;

	/*
	 * ACCEPT_TYPE_ARG3 - Third argument type of accept(). It is defined in
	 * ac_func_accept_argtypes.m4
	 */
	ACCEPT_TYPE_ARG3 salen;
}
SockAddr;


#define AUTH_REQ_OK         0	/* User is authenticated  */
#define AUTH_REQ_KRB4       1	/* Kerberos V4. Not supported any more. */
#define AUTH_REQ_KRB5       2	/* Kerberos V5. Not supported any more. */
#define AUTH_REQ_PASSWORD   3	/* Password */
#define AUTH_REQ_CRYPT      4	/* crypt password. Not supported any more. */
#define AUTH_REQ_MD5        5	/* md5 password */
#define AUTH_REQ_SCM_CREDS  6	/* transfer SCM credentials */
#define AUTH_REQ_GSS        7	/* GSSAPI without wrap() */
#define AUTH_REQ_GSS_CONT   8	/* Continue GSS exchanges */
#define AUTH_REQ_SSPI       9	/* SSPI negotiate without wrap() */
#define AUTH_REQ_SASL      10	/* Begin SASL authentication */
#define AUTH_REQ_SASL_CONT 11	/* Continue SASL authentication */
#define AUTH_REQ_SASL_FIN  12	/* Final SASL message */


typedef unsigned int AuthRequest;

#ifdef __GNUC__
#define PG_USE_INLINE
#endif

/* no special DLL markers on most ports */
#ifndef PGDLLIMPORT
#define PGDLLIMPORT
#endif
#ifndef PGDLLEXPORT
#define PGDLLEXPORT
#endif

#ifdef PG_USE_INLINE
#define STATIC_IF_INLINE static inline
#else
#define STATIC_IF_INLINE
#endif							/* PG_USE_INLINE */


typedef uint8 bits8;			/* >= 8 bits */
typedef uint16 bits16;			/* >= 16 bits */
typedef uint32 bits32;			/* >= 32 bits */

/*
 * stdint.h limits aren't guaranteed to be present and aren't guaranteed to
 * have compatible types with our fixed width types. So just define our own.
 */
#define PG_INT8_MIN		(-0x7F-1)
#define PG_INT8_MAX		(0x7F)
#define PG_UINT8_MAX	(0xFF)
#define PG_INT16_MIN	(-0x7FFF-1)
#define PG_INT16_MAX	(0x7FFF)
#define PG_UINT16_MAX	(0xFFFF)
#define PG_INT32_MIN	(-0x7FFFFFFF-1)
#define PG_INT32_MAX	(0x7FFFFFFF)
#define PG_UINT32_MAX	(0xFFFFFFFF)
#define PG_INT64_MIN	(-INT64CONST(0x7FFFFFFFFFFFFFFF) - 1)
#define PG_INT64_MAX	INT64CONST(0x7FFFFFFFFFFFFFFF)
#define PG_UINT64_MAX	UINT64CONST(0xFFFFFFFFFFFFFFFF)


typedef size_t Size;
typedef unsigned long Datum;	/* XXX sizeof(long) >= sizeof(void *) */

typedef void (*pg_on_exit_callback) (int code, Datum arg);

/*
 * NULL
  *		Null pointer.
   */
#ifndef NULL
#define NULL	((void *) 0)
#endif

/* ----------------------------------------------------------------
 *				Section 6:	assertions
 * ----------------------------------------------------------------
 */

/*
 * USE_ASSERT_CHECKING, if defined, turns on all the assertions.
 * - plai  9/5/90
 *
 * It should _NOT_ be defined in releases or in benchmark copies
 */

/*
 * Assert() can be used in both frontend and backend code. In frontend code it
 * just calls the standard assert, if it's available. If use of assertions is
 * not configured, it does nothing.
 */
#ifndef USE_ASSERT_CHECKING

#define Assert(condition)
#define AssertMacro(condition)	((void)true)
#define AssertArg(condition)
#define AssertState(condition)
#define Trap(condition, errorType)
#define TrapMacro(condition, errorType)	(true)

#elif defined(FRONTEND)
#include <assert.h>
#define Assert(p) assert(p)
#define AssertMacro(p)	((void) assert(p))
#define AssertArg(condition) assert(condition)
#define AssertState(condition) assert(condition)
#else							/* USE_ASSERT_CHECKING && !FRONTEND */

/*
 * Trap
 *		Generates an exception if the given condition is true.
 */
#define Trap(condition, errorType) \
	do { \
		if ((assert_enabled) && (condition)) \
		ExceptionalCondition(CppAsString(condition), (errorType), \
				__FILE__, __LINE__); \
	} while (0)

/*
 *	TrapMacro is the same as Trap but it's intended for use in macros:
 *
 *		#define foo(x) (AssertMacro(x != 0), bar(x))
 *
 *	Isn't CPP fun?
 */
#define TrapMacro(condition, errorType) \
	((bool) ((! assert_enabled) || ! (condition) || \
		(ExceptionalCondition(CppAsString(condition), (errorType), \
							  __FILE__, __LINE__), 0)))

#define Assert(condition) \
	Trap(!(condition), "FailedAssertion")

#define AssertMacro(condition) \
	((void) TrapMacro(!(condition), "FailedAssertion"))

#define AssertArg(condition) \
	Trap(!(condition), "BadArgument")

#define AssertState(condition) \
	Trap(!(condition), "BadState")
#endif							/* USE_ASSERT_CHECKING && !FRONTEND */


/*
 * Macros to support compile-time assertion checks.
 *
 * If the "condition" (a compile-time-constant expression) evaluates to false,
 * throw a compile error using the "errmessage" (a string literal).
 *
 * gcc 4.6 and up supports _Static_assert(), but there are bizarre syntactic
 * placement restrictions.	These macros make it safe to use as a statement
 * or in an expression, respectively.
 *
 * Otherwise we fall back on a kluge that assumes the compiler will complain
 * about a negative width for a struct bit-field.  This will not include a
 * helpful error message, but it beats not getting an error at all.
 */
#ifdef HAVE__STATIC_ASSERT
#define StaticAssertStmt(condition, errmessage) \
	do { _Static_assert(condition, errmessage); } while(0)
#define StaticAssertExpr(condition, errmessage) \
	({ StaticAssertStmt(condition, errmessage); true; })
#else							/* !HAVE__STATIC_ASSERT */
#define StaticAssertStmt(condition, errmessage) \
	((void) sizeof(struct { int static_assert_failure : (condition) ? 1 : -1; }))
#define StaticAssertExpr(condition, errmessage) \
	StaticAssertStmt(condition, errmessage)
#endif							/* HAVE__STATIC_ASSERT */


/*
 * Compile-time checks that a variable (or expression) has the specified type.
 *
 * AssertVariableIsOfType() can be used as a statement.
 * AssertVariableIsOfTypeMacro() is intended for use in macros, eg
 *		#define foo(x) (AssertVariableIsOfTypeMacro(x, int), bar(x))
 *
 * If we don't have __builtin_types_compatible_p, we can still assert that
 * the types have the same size.  This is far from ideal (especially on 32-bit
 * platforms) but it provides at least some coverage.
 */
#ifdef HAVE__BUILTIN_TYPES_COMPATIBLE_P
#define AssertVariableIsOfType(varname, typename) \
	StaticAssertStmt(__builtin_types_compatible_p(__typeof__(varname), typename), \
			CppAsString(varname) " does not have type " CppAsString(typename))
#define AssertVariableIsOfTypeMacro(varname, typename) \
	((void) StaticAssertExpr(__builtin_types_compatible_p(__typeof__(varname), typename), \
		CppAsString(varname) " does not have type " CppAsString(typename)))
#else							/* !HAVE__BUILTIN_TYPES_COMPATIBLE_P */
#define AssertVariableIsOfType(varname, typename) \
	StaticAssertStmt(sizeof(varname) == sizeof(typename), \
			CppAsString(varname) " does not have type " CppAsString(typename))
#define AssertVariableIsOfTypeMacro(varname, typename) \
																((void) StaticAssertExpr(sizeof(varname) == sizeof(typename),		\
																	 CppAsString(varname) " does not have type " CppAsString(typename)))
#endif							/* HAVE__BUILTIN_TYPES_COMPATIBLE_P */
/*
 * StrNCpy
 *	Like standard library function strncpy(), except that result string
 *	is guaranteed to be null-terminated --- that is, at most N-1 bytes
 *	of the source string will be kept.
 *	Also, the macro returns no result (too hard to do that without
 *	evaluating the arguments multiple times, which seems worse).
 *
 *	BTW: when you need to copy a non-null-terminated string (like a text
 *	datum) and add a null, do not do it with StrNCpy(..., len+1).  That
 *	might seem to work, but it fetches one byte more than there is in the
 *	text object.  One fine day you'll have a SIGSEGV because there isn't
 *	another byte before the end of memory.	Don't laugh, we've had real
 *	live bug reports from real live users over exactly this mistake.
 *	Do it honestly with "memcpy(dst,src,len); dst[len] = '\0';", instead.
 */
#define StrNCpy(dst,src,len) \
	do \
	{ \
		char * _dst = (dst); \
		Size _len = (len); \
\
		if (_len > 0) \
		{ \
			strncpy(_dst, (src), _len); \
			_dst[_len-1] = '\0'; \
		} \
	} while (0)


/* Get a bit mask of the bits set in non-long aligned addresses */
#define LONG_ALIGN_MASK (sizeof(long) - 1)
#define MEMSET_LOOP_LIMIT 1024

/*
 * MemSet
 *	Exactly the same as standard library function memset(), but considerably
 *	faster for zeroing small word-aligned structures (such as parsetree nodes).
 *	This has to be a macro because the main point is to avoid function-call
 *	overhead.	However, we have also found that the loop is faster than
 *	native libc memset() on some platforms, even those with assembler
 *	memset() functions.  More research needs to be done, perhaps with
 *	MEMSET_LOOP_LIMIT tests in configure.
 */
#define MemSet(start, val, len) \
	do \
	{ \
		/* must be void* because we don't know if it is integer aligned yet */ \
		void   *_vstart = (void *) (start); \
		int		_val = (val); \
		Size	_len = (len); \
\
		if ((((intptr_t) _vstart) & LONG_ALIGN_MASK) == 0 && \
			(_len & LONG_ALIGN_MASK) == 0 && \
			_val == 0 && \
			_len <= MEMSET_LOOP_LIMIT && \
			/* \
			 *	If MEMSET_LOOP_LIMIT == 0, optimizer should find \
			 *	the whole "if" false at compile time. \
			 */ \
			MEMSET_LOOP_LIMIT != 0) \
		{ \
			long *_start = (long *) _vstart; \
			long *_stop = (long *) ((char *) _start + _len); \
			while (_start < _stop) \
				*_start++ = 0; \
		} \
		else \
			memset(_vstart, _val, _len); \
	} while (0)

/*
 * MemSetAligned is the same as MemSet except it omits the test to see if
 * "start" is word-aligned.  This is okay to use if the caller knows a-priori
 * that the pointer is suitably aligned (typically, because he just got it
 * from palloc(), which always delivers a max-aligned pointer).
 */
#define MemSetAligned(start, val, len) \
	do \
	{ \
		long   *_start = (long *) (start); \
		int		_val = (val); \
		Size	_len = (len); \
\
		if ((_len & LONG_ALIGN_MASK) == 0 && \
			_val == 0 && \
			_len <= MEMSET_LOOP_LIMIT && \
			MEMSET_LOOP_LIMIT != 0) \
		{ \
			long *_stop = (long *) ((char *) _start + _len); \
			while (_start < _stop) \
				*_start++ = 0; \
		} \
		else \
			memset(_start, _val, _len); \
	} while (0)


/*
 * MemSetTest/MemSetLoop are a variant version that allow all the tests in
 * MemSet to be done at compile time in cases where "val" and "len" are
 * constants *and* we know the "start" pointer must be word-aligned.
 * If MemSetTest succeeds, then it is okay to use MemSetLoop, otherwise use
 * MemSetAligned.  Beware of multiple evaluations of the arguments when using
 * this approach.
 */
#define MemSetTest(val, len) \
	( ((len) & LONG_ALIGN_MASK) == 0 && \
	(len) <= MEMSET_LOOP_LIMIT && \
	MEMSET_LOOP_LIMIT != 0 && \
	(val) == 0 )

#define MemSetLoop(start, val, len) \
	do \
	{ \
		long * _start = (long *) (start); \
		long * _stop = (long *) ((char *) _start + (Size) (len)); \
	\
		while (_start < _stop) \
			*_start++ = 0; \
	} while (0)


#endif							/* POOL_TYPE_H */
