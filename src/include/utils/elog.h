/*-------------------------------------------------------------------------
 *
 * elog.h
 *	  POSTGRES error reporting/logging definitions.
 *
 *
 * Portions Copyright (c) 2003-2014, PgPool Global Development Group
 * Portions Copyright (c) 1996-2013, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * src/include/utils/elog.h
 *
 *-------------------------------------------------------------------------
 */
#ifndef ELOG_H
#define ELOG_H

#include <setjmp.h>
#include <stdlib.h>
#include "parser/stringinfo.h"
#include "utils/palloc.h"
#define PG_TEXTDOMAIN(domain) (domain "-PGPOOL")

#define exprLocation(x)  errcode_ign(0)
#define _(x) (x)
#define gettext(x) (x)
#define dgettext(d,x) (x)
#define ngettext(s,p,n) ((n) == 1 ? (s) : (p))
#define dngettext(d,s,p,n) ((n) == 1 ? (s) : (p))

typedef enum
{
	DestNone,					/* results are discarded */
	DestDebug,					/* results go to debugging output */
	DestRemote,					/* results sent to frontend process */
	DestRemoteExecute,			/* sent to frontend, in Execute command */
	DestSPI,					/* results sent to SPI manager */
	DestTuplestore,				/* results sent to Tuplestore */
	DestIntoRel,				/* results sent to relation (SELECT INTO) */
	DestCopyOut,				/* results sent to COPY TO code */
	DestSQLFunction,			/* results sent to SQL-language func mgr */
	DestTransientRel			/* results sent to transient relation */
} CommandDest;

/*
 * Identifiers of error message fields.  Kept here to keep common
 * between frontend and backend, and also to export them to libpq
 * applications.
 */
#define PG_DIAG_SEVERITY		'S'
#define PG_DIAG_SQLSTATE		'C'
#define PG_DIAG_MESSAGE_PRIMARY 'M'
#define PG_DIAG_MESSAGE_DETAIL	'D'
#define PG_DIAG_MESSAGE_HINT	'H'
#define PG_DIAG_STATEMENT_POSITION 'P'
#define PG_DIAG_INTERNAL_POSITION 'p'
#define PG_DIAG_INTERNAL_QUERY	'q'
#define PG_DIAG_CONTEXT			'W'
#define PG_DIAG_SCHEMA_NAME		's'
#define PG_DIAG_TABLE_NAME		't'
#define PG_DIAG_COLUMN_NAME		'c'
#define PG_DIAG_DATATYPE_NAME	'd'
#define PG_DIAG_CONSTRAINT_NAME 'n'
#define PG_DIAG_SOURCE_FILE		'F'
#define PG_DIAG_SOURCE_LINE		'L'
#define PG_DIAG_SOURCE_FUNCTION 'R'
#if defined(WIN32) || defined(__CYGWIN__)

#ifdef BUILDING_DLL
#define PGDLLIMPORT __declspec (dllexport)
#else							/* not BUILDING_DLL */
#define PGDLLIMPORT __declspec (dllimport)
#endif

#ifdef _MSC_VER
#define PGDLLEXPORT __declspec (dllexport)
#else
#define PGDLLEXPORT
#endif
#else							/* not CYGWIN, not MSVC, not MingW */
#define PGDLLIMPORT
#define PGDLLEXPORT
#endif

#define PGUNSIXBIT(val) (((val) & 0x3F) + '0')

/* Error level codes */
#define DEBUG5		10			/* Debugging messages, in categories of
								 * decreasing detail. */
#define DEBUG4		11
#define DEBUG3		12
#define DEBUG2		13
#define DEBUG1		14			/* used by GUC debug_* variables */
#define LOG			15			/* Server operational messages; sent only to
								 * server log by default. */
#define COMMERROR	16			/* Client communication problems; same as LOG
								 * for server reporting, but never sent to
								 * client. */
#define INFO		17			/* Messages specifically requested by user (eg
								 * VACUUM VERBOSE output); always sent to
								 * client regardless of client_min_messages,
								 * but by default not sent to server log. */
#define NOTICE		18			/* Helpful messages to users about query
								 * operation; sent to client and server log by
								 * default. */
#define WARNING		19			/* Warnings.  NOTICE is for expected messages
								 * like implicit sequence creation by SERIAL.
								 * WARNING is for unexpected messages. */
#define ERROR		20			/* user error - abort transaction; return to
								 * known state */
/* Save ERROR value in PGERROR so it can be restored when Win32 includes
 * modify it.  We have to use a constant rather than ERROR because macros
 * are expanded only when referenced outside macros.
 */

#ifdef WIN32
#define PGERROR		20
#endif
#define FATAL		21			/* fatal error - abort process */
#define PANIC		22			/* take down the other backends with me */
/* pgpool-II extension. This is same as ERROR but sets the
 * do not cache connection flag before transforming to ERROR.
 */
#define FRONTEND_ERROR			23	/* transformed to ERROR at errstart */
#define FRONTEND_ONLY_ERROR		24	/* this is treated as LOG message
									 * internally for pgpool-II but forwarded
									 * to frontend clients just like normal
									 * errors followed by readyForQuery
									 * message */

 /* #define DEBUG DEBUG1 */	/* Backward compatibility with pre-7.3 */
#define POOL_EXIT_NO_RESTART	0	/* child exiting with this error will not
									 * be restarted by pgpool main process but
									 * the pgpool main will remain unaffected
									 * by this */
#define POOL_EXIT_AND_RESTART	1	/* child process exit with this error code
									 * will be restarted by the pgpool main */
#define POOL_EXIT_FATAL			3	/* This exit code from child also takes
									 * down the pgpool main with it */



/* Which __func__ symbol do we have, if any? */
#ifdef HAVE_FUNCNAME__FUNC
#define PG_FUNCNAME_MACRO	__func__
#else
#ifdef HAVE_FUNCNAME__FUNCTION
#define PG_FUNCNAME_MACRO	__FUNCTION__
#else
#define PG_FUNCNAME_MACRO	NULL
#endif
#endif


/*----------
 * New-style error reporting API: to be used in this way:
 *		ereport(ERROR,
 *				(errcode(ERRCODE_UNDEFINED_CURSOR),
 *				 errmsg("portal \"%s\" not found", stmt->portalname),
 *				 ... other errxxx() fields as needed ...));
 *
 * The error level is required, and so is a primary error message (errmsg
 * or errmsg_internal).  All else is optional.	errcode() defaults to
 * ERRCODE_INTERNAL_ERROR if elevel is ERROR or more, ERRCODE_WARNING
 * if elevel is WARNING, or ERRCODE_SUCCESSFUL_COMPLETION if elevel is
 * NOTICE or below.
 *
 * ereport_domain() allows a message domain to be specified, for modules that
 * wish to use a different message catalog from the backend's.	To avoid having
 * one copy of the default text domain per .o file, we define it as NULL here
 * and have errstart insert the default text domain.  Modules can either use
 * ereport_domain() directly, or preferably they can override the TEXTDOMAIN
 * macro.
 *
 * If elevel >= ERROR, the call will not return; we try to inform the compiler
 * of that via pg_unreachable().  However, no useful optimization effect is
 * obtained unless the compiler sees elevel as a compile-time constant, else
 * we're just adding code bloat.  So, if __builtin_constant_p is available,
 * use that to cause the second if() to vanish completely for non-constant
 * cases.  We avoid using a local variable because it's not necessary and
 * prevents gcc from making the unreachability deduction at optlevel -O0.
 *----------
 */
#ifdef HAVE__BUILTIN_CONSTANT_P
#define ereport_domain(elevel, domain, rest)	\
	do { \
		if (errstart(elevel, __FILE__, __LINE__, PG_FUNCNAME_MACRO, domain)) \
			errfinish rest; \
		if (__builtin_constant_p(elevel) && (elevel) >= ERROR && (elevel) != FRONTEND_ONLY_ERROR) \
			pg_unreachable(); \
	} while(0)
#else							/* !HAVE__BUILTIN_CONSTANT_P */
#define ereport_domain(elevel, domain, rest)	\
	do { \
		const int elevel_ = (elevel); \
		if (errstart(elevel_, __FILE__, __LINE__, PG_FUNCNAME_MACRO, domain)) \
			errfinish rest; \
		if (elevel_ >= ERROR  && elevel_ != FRONTEND_ONLY_ERROR) \
			pg_unreachable(); \
	} while(0)
#endif							/* HAVE__BUILTIN_CONSTANT_P */

#define ereport(elevel, rest)	\
	ereport_domain(elevel, TEXTDOMAIN, rest)

#define TEXTDOMAIN NULL

extern bool message_level_is_interesting(int elevel);
extern bool errstart(int elevel, const char *filename, int lineno,
		 const char *funcname, const char *domain);
extern void errfinish(int dummy,...);

#define errcode(sqlerrcode) \
	errcode_ign(0)
extern int	errcode_ign(int sqlerrcode);

extern int	pool_error_code(const char *errcode);

extern int	return_code(int retcode);
extern int	get_return_code(void);

extern int
errmsg(const char *fmt,...)
/* This extension allows gcc to check the format string for consistency with
   the supplied arguments. */
__attribute__((format(PG_PRINTF_ATTRIBUTE, 1, 2)));

extern int
errmsg_internal(const char *fmt,...)
/* This extension allows gcc to check the format string for consistency with
   the supplied arguments. */
__attribute__((format(PG_PRINTF_ATTRIBUTE, 1, 2)));

extern int
errmsg_plural(const char *fmt_singular, const char *fmt_plural,
			  unsigned long n,...)
/* This extension allows gcc to check the format string for consistency with
   the supplied arguments. */
__attribute__((format(PG_PRINTF_ATTRIBUTE, 1, 4)))
__attribute__((format(PG_PRINTF_ATTRIBUTE, 2, 4)));

extern int
errdetail(const char *fmt,...)
/* This extension allows gcc to check the format string for consistency with
   the supplied arguments. */
__attribute__((format(PG_PRINTF_ATTRIBUTE, 1, 2)));

extern int
errdetail_internal(const char *fmt,...)
/* This extension allows gcc to check the format string for consistency with
   the supplied arguments. */
__attribute__((format(PG_PRINTF_ATTRIBUTE, 1, 2)));

extern int
errdetail_log(const char *fmt,...)
/* This extension allows gcc to check the format string for consistency with
   the supplied arguments. */
__attribute__((format(PG_PRINTF_ATTRIBUTE, 1, 2)));

extern int
errdetail_plural(const char *fmt_singular, const char *fmt_plural,
				 unsigned long n,...)
/* This extension allows gcc to check the format string for consistency with
   the supplied arguments. */
__attribute__((format(PG_PRINTF_ATTRIBUTE, 1, 4)))
__attribute__((format(PG_PRINTF_ATTRIBUTE, 2, 4)));

extern int
errhint(const char *fmt,...)
/* This extension allows gcc to check the format string for consistency with
   the supplied arguments. */
__attribute__((format(PG_PRINTF_ATTRIBUTE, 1, 2)));

/*
 * errcontext() is typically called in error context callback functions, not
 * within an ereport() invocation. The callback function can be in a different
 * module than the ereport() call, so the message domain passed in errstart()
 * is not usually the correct domain for translating the context message.
 * set_errcontext_domain() first sets the domain to be used, and
 * errcontext_msg() passes the actual message.
 */
#define errcontext	set_errcontext_domain(TEXTDOMAIN),	errcontext_msg

extern int	set_errcontext_domain(const char *domain);
extern int
errcontext_msg(const char *fmt,...)
/* This extension allows gcc to check the format string for consistency with
   the supplied arguments. */
__attribute__((format(PG_PRINTF_ATTRIBUTE, 1, 2)));

extern int	errhidestmt(bool hide_stmt);

extern int	errfunction(const char *funcname);
extern int	errposition(int cursorpos);

#define pg_unreachable() exit(0)

extern bool getfrontendinvalid(void);
extern int	geterrcode(void);
extern int	geterrposition(void);
extern int	getinternalerrposition(void);


/*----------
 * Old-style error reporting API: to be used in this way:
 *		elog(ERROR, "portal \"%s\" not found", stmt->portalname);
 *----------
 */
#ifdef HAVE__VA_ARGS
/*
 * If we have variadic macros, we can give the compiler a hint about the
 * call not returning when elevel >= ERROR.  See comments for ereport().
 * Note that historically elog() has called elog_start (which saves errno)
 * before evaluating "elevel", so we preserve that behavior here.
 */
#ifdef HAVE__BUILTIN_CONSTANT_P
#define elog(elevel, ...)  \
	do { \
		elog_start(__FILE__, __LINE__, PG_FUNCNAME_MACRO); \
		elog_finish(elevel, __VA_ARGS__); \
		if (__builtin_constant_p(elevel) && (elevel) >= ERROR  && (elevel) != FRONTEND_ONLY_ERROR) \
			pg_unreachable(); \
	} while(0)
#else							/* !HAVE__BUILTIN_CONSTANT_P */
#define elog(elevel, ...)  \
	do { \
		int		elevel_; \
		elog_start(__FILE__, __LINE__, PG_FUNCNAME_MACRO); \
		elevel_ = (elevel); \
		elog_finish(elevel_, __VA_ARGS__); \
		if (elevel_ >= ERROR  && (elevel) != FRONTEND_ONLY_ERROR) \
			pg_unreachable(); \
	} while(0)
#endif							/* HAVE__BUILTIN_CONSTANT_P */
#else							/* !HAVE__VA_ARGS */
#define elog  \
	elog_start(__FILE__, __LINE__, PG_FUNCNAME_MACRO), \
	elog_finish
#endif							/* HAVE__VA_ARGS */

extern void elog_start(const char *filename, int lineno, const char *funcname);
extern void
elog_finish(int elevel, const char *fmt,...)
/* This extension allows gcc to check the format string for consistency with
   the supplied arguments. */
__attribute__((format(PG_PRINTF_ATTRIBUTE, 2, 3)));


/* Support for attaching context information to error reports */

typedef struct ErrorContextCallback
{
	struct ErrorContextCallback *previous;
	void		(*callback) (void *arg);
	void	   *arg;
} ErrorContextCallback;

extern PGDLLIMPORT ErrorContextCallback *error_context_stack;


/*----------
 * API for catching ereport(ERROR) exits.  Use these macros like so:
 *
 *		PG_TRY();
 *		{
 *			... code that might throw ereport(ERROR) ...
 *		}
 *		PG_CATCH();
 *		{
 *			... error recovery code ...
 *		}
 *		PG_END_TRY();
 *
 * (The braces are not actually necessary, but are recommended so that
 * pg_indent will indent the construct nicely.)  The error recovery code
 * can optionally do PG_RE_THROW() to propagate the same error outwards.
 *
 * Note: while the system will correctly propagate any new ereport(ERROR)
 * occurring in the recovery section, there is a small limit on the number
 * of levels this will work for.  It's best to keep the error recovery
 * section simple enough that it can't generate any new errors, at least
 * not before popping the error stack.
 *
 * Note: an ereport(FATAL) will not be caught by this construct; control will
 * exit straight through proc_exit().  Therefore, do NOT put any cleanup
 * of non-process-local resources into the error recovery section, at least
 * not without taking thought for what will happen during ereport(FATAL).
 * The PG_ENSURE_ERROR_CLEANUP macros provided by storage/ipc.h may be
 * helpful in such cases.
 *----------
 */
#define PG_TRY()  \
	do { \
		sigjmp_buf *save_exception_stack = PG_exception_stack; \
		ErrorContextCallback *save_context_stack = error_context_stack; \
		sigjmp_buf local_sigjmp_buf; \
		if (sigsetjmp(local_sigjmp_buf, 0) == 0) \
		{ \
			PG_exception_stack = &local_sigjmp_buf

#define PG_CATCH()	\
		} \
		else \
		{ \
			PG_exception_stack = save_exception_stack; \
			error_context_stack = save_context_stack

#define PG_END_TRY()  \
		} \
		PG_exception_stack = save_exception_stack; \
		error_context_stack = save_context_stack; \
	} while (0)

/*
 * gcc understands __attribute__((noreturn)); for other compilers, insert
 * pg_unreachable() so that the compiler gets the point.
 */
#ifdef __GNUC__
#define PG_RE_THROW()  \
	pg_re_throw()
#else
#define PG_RE_THROW()  \
	(pg_re_throw(), pg_unreachable())
#endif

extern PGDLLIMPORT sigjmp_buf *PG_exception_stack;


/* Stuff that error handlers might want to use */

/*
 * ErrorData holds the data accumulated during any one ereport() cycle.
 * Any non-NULL pointers must point to palloc'd data.
 * (The const pointers are an exception; we assume they point at non-freeable
 * constant strings.)
 */
typedef struct ErrorData
{
	int			elevel;			/* error level */
	bool		output_to_server;	/* will report to server log? */
	bool		output_to_client;	/* will report to client? */
	bool		show_funcname;	/* true to force funcname inclusion */
	bool		hide_stmt;		/* true to prevent STATEMENT: inclusion */
	const char *filename;		/* __FILE__ of ereport() call */
	int			lineno;			/* __LINE__ of ereport() call */
	const char *funcname;		/* __func__ of ereport() call */
	const char *domain;			/* message domain */
	const char *context_domain; /* message domain for context message */
	int			sqlerrcode;		/* encoded ERRSTATE */
	bool		frontend_invalid;	/* true when frontend connection is not
									 * valid */
	char	   *pgpool_errcode; /* error code to be sent to client */
	char	   *message;		/* primary error message */
	char	   *detail;			/* detail error message */
	char	   *detail_log;		/* detail error message for server log only */
	char	   *hint;			/* hint message */
	char	   *context;		/* context message */
	char	   *schema_name;	/* name of schema */
	char	   *table_name;		/* name of table */
	char	   *column_name;	/* name of column */
	char	   *datatype_name;	/* name of datatype */
	char	   *constraint_name;	/* name of constraint */
	int			cursorpos;		/* cursor index into query string */
	int			retcode;		/* return code to be used in exit() code */
	int			internalpos;	/* cursor index into internalquery */
	char	   *internalquery;	/* text of internally-generated query */
	char	   *client_error_code;	/* error code to report to client */
	int			saved_errno;	/* errno at entry */

	/* context containing associated non-constant strings */
	MemoryContext assoc_context;
} ErrorData;

extern void EmitErrorReport(void);
extern ErrorData *CopyErrorData(void);
extern void FreeErrorData(ErrorData *edata);
extern void FlushErrorState(void);
extern void ReThrowError(ErrorData *edata) __attribute__((noreturn));
extern void pg_re_throw(void) __attribute__((noreturn));

extern char *GetErrorContextStack(void);
extern void proc_exit(int);

/* Hook for intercepting messages before they are sent to the server log */
typedef void (*emit_log_hook_type) (ErrorData *edata);
extern PGDLLIMPORT emit_log_hook_type emit_log_hook;


/* GUC-configurable parameters */

typedef enum
{
	PGERROR_TERSE,				/* single-line error messages */
	PGERROR_DEFAULT,			/* recommended style */
	PGERROR_VERBOSE				/* all the facts, ma'am */
}			PGErrorVerbosity;


/* Log destination bitmap */
#define LOG_DESTINATION_STDERR	 1
#define LOG_DESTINATION_SYSLOG	 2
#define LOG_DESTINATION_EVENTLOG 4
#define LOG_DESTINATION_CSVLOG	 8

extern bool in_error_recursion_trouble(void);

#ifdef HAVE_SYSLOG
extern void set_syslog_parameters(const char *ident, int facility);
#endif

/*
 * Write errors to stderr (or by equal means when stderr is
 * not available). Used before ereport/elog can be used
 * safely (memory context, GUC load etc)
 */
extern void
write_stderr(const char *fmt,...)
/* This extension allows gcc to check the format string for consistency with
   the supplied arguments. */
__attribute__((format(PG_PRINTF_ATTRIBUTE, 1, 2)));

void		shmem_exit(int code);
void		on_exit_reset(void);
void		cancel_shmem_exit(pg_on_exit_callback function, Datum arg);
void		on_proc_exit(pg_on_exit_callback function, Datum arg);
void		on_shmem_exit(pg_on_exit_callback function, Datum arg);
void		on_system_exit(pg_on_exit_callback function, Datum arg);


#endif							/* ELOG_H */
