dnl Process this file with autoconf to produce a configure script.
AC_INIT([pgpool-II], [4.4.1], [pgpool-hackers@pgpool.net], [pgpool-II])

dnl Checks for programs.
AC_PROG_CC
AC_PROG_CC_C99()

# Error out if the compiler does not support C99, as the codebase
# relies on that.
if test "$ac_cv_prog_cc_c99" = no; then
    AC_MSG_ERROR([C compiler "$CC" does not support C99])
fi

AM_INIT_AUTOMAKE([foreign no-dependencies subdir-objects])

dnl Disallow to rebuild configure.
AM_MAINTAINER_MODE(disable)

# AC_PROG_RANLIB
AR_FLAGS="cr"
AC_SUBST(AR_FLAGS)
AC_PROG_LIBTOOL

AM_PROG_LEX
AC_PROG_YACC
AM_PROG_CC_C_O

dnl suggested by libtoolize --force
AC_CONFIG_MACRO_DIR([m4])
# Check for docbook
PGAC_PROG_NSGMLS
PGAC_PROG_JADE
PGAC_CHECK_DOCBOOK(4.2)
PGAC_PATH_DOCBOOK_STYLESHEETS
PGAC_PATH_COLLATEINDEX
AC_CHECK_PROGS(XSLTPROC, xsltproc)
AC_CHECK_PROGS(XMLLINT, xmllint)
AC_CHECK_PROGS(OSX, [osx sgml2xml sx])
AC_CHECK_PROGS(PERL, perl)
AC_CHECK_PROGS(LYNX, lynx)
AC_CHECK_PROGS(SUNIFDEF, sunifdef)

AC_SUBST([XSLTPROC_HTML_FLAGS])
AC_SUBST([STYLE])
AM_CONDITIONAL([STYLE_ENV_SET], [test ! -z "$STYLE"])

dnl Check compiler option
dnl check -Wall option
OLD_CFLAGS=$CFLAGS
CFLAGS="$CFLAGS -Wall"
AC_MSG_CHECKING(for -Wall option)
AC_CACHE_VAL(ac_cv_wall,
  AC_TRY_COMPILE([], [char a;], ac_cv_wall=yes, ac_cv_wall=no))
echo $ac_cv_wall
if test $ac_cv_wall = no; then
   CFLAGS=$OLD_CFLAGS
fi

dnl check -Wmissing-prototypes
OLD_CFLAGS=$CFLAGS
CFLAGS="$CFLAGS -Wmissing-prototypes"
AC_MSG_CHECKING(for -Wmissing-prototypes option)
AC_CACHE_VAL(ac_cv_wmissing_prototypes,
  AC_TRY_COMPILE([], [char a;], ac_cv_wmissing_prototypes=yes,
  ac_cv_wmissing_prototypes=no))
echo $ac_cv_wmissing_prototypes
if test $ac_cv_wmissing_prototypes = no; then
   CFLAGS=$OLD_CFLAGS
fi


dnl check -Wmissing-prototypes
OLD_CFLAGS=$CFLAGS
CFLAGS="$CFLAGS -Wmissing-declarations"
AC_MSG_CHECKING(for -Wmissing-declarations option)
AC_CACHE_VAL(ac_cv_wmissing_declarations,
  AC_TRY_COMPILE([], [char a;], ac_cv_wmissing_declarations=yes,
  ac_cv_wmissing_declarations=no))
echo $ac_cv_wmissing_declarations
if test $ac_cv_wmissing_declarations = no; then
   CFLAGS=$OLD_CFLAGS
fi

dnl Checks for libraries.
AC_CHECK_LIB(m,        main)
AC_CHECK_LIB(nsl,      main)
AC_CHECK_LIB(socket,   main)
AC_CHECK_LIB(ipc,      main)
AC_CHECK_LIB(IPC,      main)
AC_CHECK_LIB(lc,       main)
AC_CHECK_LIB(BSD,      main)
AC_CHECK_LIB(gen,      main)
AC_CHECK_LIB(PW,       main)
AC_CHECK_LIB(resolv,   main)
AC_CHECK_LIB(crypt,   main)

AC_CHECK_FUNCS([strlcat, strlcpy])

dnl Checks for header files.
AC_HEADER_STDC
AC_HEADER_SYS_WAIT
AC_CHECK_HEADERS(fcntl.h unistd.h getopt.h netinet/tcp.h netinet/in.h netdb.h sys/param.h sys/types.h sys/socket.h sys/un.h sys/time.h sys/sem.h sys/shm.h sys/select.h crypt.h sys/pstat.h)
AC_CHECK_HEADER([termios.h], [AC_DEFINE(HAVE_TERMIOS_H,1,checking termios)])
dnl Checks for typedefs, structures, and compiler characteristics.
AC_C_CONST
AC_TYPE_PID_T
AC_HEADER_TIME


dnl ===== Copied from PostgreSQL's configure start =====
dnl
dnl Check to see if we have a working 64-bit integer type.
dnl This breaks down into two steps:
dnl (1) figure out if the compiler has a 64-bit int type with working
dnl arithmetic, and if so
dnl (2) see whether snprintf() can format the type correctly.  (Currently,
dnl snprintf is the only library routine we really need for int8 support.)
dnl It's entirely possible to have a compiler that handles a 64-bit type
dnl when the C library doesn't; this is fairly likely when using gcc on
dnl an older platform, for example.
dnl If there is no native snprintf() or it does not handle the 64-bit type,
dnl we force our own version of snprintf() to be used instead.
dnl Note this test must be run after our initial check for snprintf/vsnprintf.
m4_include([c-compiler.m4])
m4_include([c-library.m4])
m4_include([general.m4])

dnl This mazic variable is set to "yes" in PostgreSQL's configure.in
dnl if PORTNAME is "win32".
dnl pgpool-II does not suppport Windows at the moment, so we set to "no".
pgac_need_repl_snprintf=no
AC_CHECK_FUNCS(snprintf, [], pgac_need_repl_snprintf=yes)
AC_CHECK_FUNCS(vsnprintf, [], pgac_need_repl_snprintf=yes)

PGAC_TYPE_64BIT_INT([long int])

if test x"$HAVE_LONG_INT_64" = x"no" ; then
  PGAC_TYPE_64BIT_INT([long long int])
fi

dnl If we need to use "long long int", figure out whether nnnLL notation works.

if test x"$HAVE_LONG_LONG_INT_64" = xyes ; then
  AC_TRY_COMPILE([
#define INT64CONST(x)  x##LL
long long int foo = INT64CONST(0x1234567890123456);
],
	[],
	[AC_DEFINE(HAVE_LL_CONSTANTS, 1, [Define to 1 if constants of type 'long long int' should have the suffix LL.])],
	[])
fi

# If we found "long int" is 64 bits, assume snprintf handles it.  If
# we found we need to use "long long int", better check.  We cope with
# snprintfs that use %lld, %qd, or %I64d as the format.  If none of these
# work, fall back to our own snprintf emulation (which we know uses %lld).

if test "$HAVE_LONG_LONG_INT_64" = yes ; then
  if test $pgac_need_repl_snprintf = no; then
    PGAC_FUNC_SNPRINTF_LONG_LONG_INT_FORMAT
    if test "$LONG_LONG_INT_FORMAT" = ""; then
      # Force usage of our own snprintf, since system snprintf is broken
      pgac_need_repl_snprintf=yes
      LONG_LONG_INT_FORMAT='%lld'
    fi
  else
    # Here if we previously decided we needed to use our own snprintf
    LONG_LONG_INT_FORMAT='%lld'
  fi
  LONG_LONG_UINT_FORMAT=`echo "$LONG_LONG_INT_FORMAT" | sed 's/d$/u/'`
  INT64_FORMAT="\"$LONG_LONG_INT_FORMAT\""
  UINT64_FORMAT="\"$LONG_LONG_UINT_FORMAT\""
else
  # Here if we are not using 'long long int' at all
  INT64_FORMAT='"%ld"'
  UINT64_FORMAT='"%lu"'
fi

# disable useless truncation warnings from gcc 8+
NOT_THE_CFLAGS=""
PGAC_PROG_CC_VAR_OPT(NOT_THE_CFLAGS, [-Wformat-truncation])
if test -n "$NOT_THE_CFLAGS"; then
  CFLAGS="$CFLAGS -Wno-format-truncation"
fi
PGAC_PROG_CC_VAR_OPT(NOT_THE_CFLAGS, [-Wstringop-truncation])
if test -n "$NOT_THE_CFLAGS"; then
  CFLAGS="$CFLAGS -Wno-stringop-truncation"
fi
AC_DEFINE_UNQUOTED(PACKAGE, "$PACKAGE",
                   [Name of package])

AC_DEFINE_UNQUOTED(VERSION, "$VERSION",
                   [Version number of package])

AC_DEFINE_UNQUOTED(INT64_FORMAT, $INT64_FORMAT,
                   [Define to the appropriate snprintf format for 64-bit ints, if any.])

AC_DEFINE_UNQUOTED(UINT64_FORMAT, $UINT64_FORMAT,
                   [Define to the appropriate snprintf format for unsigned 64-bit ints, if any.])

# Now we have checked all the reasons to replace snprintf
if test $pgac_need_repl_snprintf = yes; then
  AC_DEFINE(USE_REPL_SNPRINTF, 1, [Use replacement snprintf() functions.])
  AC_LIBOBJ(snprintf)
fi
AM_CONDITIONAL(use_repl_snprintf, test $pgac_need_repl_snprintf = yes)
# Need a #define for the size of Datum (unsigned long)
AC_CHECK_SIZEOF([unsigned long])

# And check size of void *, size_t (enables tweaks for > 32bit address space)
AC_CHECK_SIZEOF([void *])
AC_CHECK_SIZEOF([size_t])

# Decide whether float4 is passed by value: user-selectable, enabled by default
AC_MSG_CHECKING([whether to build with float4 passed by value])
PGAC_ARG_BOOL(enable, float4-byval, yes, [disable float4 passed by value],
              [AC_DEFINE([USE_FLOAT4_BYVAL], 1,
                         [Define to 1 if you want float4 values to be passed by value. (--enable-float4-byval)])
               float4passbyval=true],
              [float4passbyval=false])
AC_MSG_RESULT([$enable_float4_byval])
AC_DEFINE_UNQUOTED([FLOAT4PASSBYVAL], [$float4passbyval], [float4 values are passed by value if 'true', by reference if 'false'])

# Decide whether float8 is passed by value.
# Note: this setting also controls int8 and related types such as timestamp.
# If sizeof(Datum) >= 8, this is user-selectable, enabled by default.
# If not, trying to select it is an error.
AC_MSG_CHECKING([whether to build with float8 passed by value])
if test $ac_cv_sizeof_unsigned_long -ge 8 ; then
  PGAC_ARG_BOOL(enable, float8-byval, yes, [disable float8 passed by value])
else
  PGAC_ARG_BOOL(enable, float8-byval, no, [disable float8 passed by value])
  if test "$enable_float8_byval" = yes ; then
    AC_MSG_ERROR([--enable-float8-byval is not supported on 32-bit platforms.])
  fi
fi
if test "$enable_float8_byval" = yes ; then
  AC_DEFINE([USE_FLOAT8_BYVAL], 1,
            [Define to 1 if you want float8, int8, etc values to be passed by value. (--enable-float8-byval)])
  float8passbyval=true
else
  float8passbyval=false
fi
AC_MSG_RESULT([$enable_float8_byval])
AC_DEFINE_UNQUOTED([FLOAT8PASSBYVAL], [$float8passbyval], [float8, int8, and related values are passed by value if 'true', by reference if 'false'])

dnl Rest of part cannot be executed by autocnf-2.59.
dnl AC_CHECK_ALIGNOF is supported autoconf-2.60 or higher.
dnl ===== Copied from PostgreSQL's configure start =====



dnl Checks for sockaddr_storage structure, members and necessary types
m4_include([ac_func_accept_argtypes.m4])
AC_CHECK_TYPES([struct sockaddr_storage], [], [],
[#include <sys/types.h>
#ifdef HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif
])
AC_CHECK_MEMBERS([struct sockaddr_storage.ss_family,
                  struct sockaddr_storage.__ss_family,
                  struct sockaddr_storage.ss_len,
                  struct sockaddr_storage.__ss_len,
                  struct sockaddr.sa_len], [], [],
[#include <sys/types.h>
#ifdef HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif
])

AC_CHECK_TYPES([union semun],[],[],[#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/sem.h>])

dnl Checks for library functions.
AC_TYPE_SIGNAL
AC_FUNC_VPRINTF
AC_FUNC_WAIT3
AC_FUNC_ACCEPT_ARGTYPES
AC_CHECK_FUNCS(setsid select socket sigprocmask strdup strerror strftime strtok asprintf vasprintf gai_strerror hstrerror pstat setproctitle syslog)

PGAC_C_TYPES_COMPATIBLE

dnl Checks for pg_config command.
AC_CHECK_PROGS(PGCONFIG, pg_config)

if test -z $PGCONFIG
then
  PGSQL_INCLUDE_DIR=/usr/local/pgsql/include
  PGSQL_LIB_DIR=/usr/local/pgsql/lib
  PGSQL_BIN_DIR=/usr/local/pgsql/bin
else
  PGSQL_INCLUDE_DIR=`pg_config --includedir`
  PGSQL_LIB_DIR=`pg_config --libdir`
  PGSQL_BIN_DIR=`pg_config --bindir`
fi

AC_ARG_WITH(pgsql,
    [  --with-pgsql=DIR     site header files for PostgreSQL in DIR],
    [
	case "$withval" in
	"" | y | ye | yes | n | no)
	    AC_MSG_ERROR([*** You must supply an argument to the --with-pgsql option.])
	  ;;
	esac
	PGSQL_INCLUDE_DIR="$withval"/include
	PGSQL_LIB_DIR="$withval"/lib
	PGSQL_BIN_DIR="$withval"/bin
    ])

AC_ARG_WITH(pgsql-includedir,
    [  --with-pgsql-includedir=DIR     site header files for PostgreSQL in DIR],
    [
	case "$withval" in
	"" | y | ye | yes | n | no)
	    AC_MSG_ERROR([*** You must supply an argument to the --with-pgsql-includedir option.])
	  ;;
	esac
	PGSQL_INCLUDE_DIR="$withval"
    ])

AC_ARG_WITH(pgsql-libdir,
    [  --with-pgsql-libdir=DIR     site library files for PostgreSQL in DIR],
    [
	case "$withval" in
	"" | y | ye | yes | n | no)
	    AC_MSG_ERROR([*** You must supply an argument to the --with-pgsql-libdir option.])
	  ;;
	esac
	PGSQL_LIB_DIR="$withval"
    ])

AC_ARG_WITH(pgsql-bindir,
    [  --with-pgsql-bindir=DIR     site executable files for PostgreSQL in DIR],
    [
	case "$withval" in
	"" | y | ye | yes | n | no)
	    AC_MSG_ERROR([*** You must supply an argument to the --with-pgsql-bindir option.])
	  ;;
	esac
	PGSQL_BIN_DIR="$withval"
    ])

AC_ARG_WITH(sunifdef,
    [  --with-sunifdef=DIR     install path for sunifdef utility ],
    [
	case "$withval" in
	"" | y | ye | yes | n | no)
	    AC_MSG_ERROR([*** You must supply an argument to the --with-sunifdef option.])
	  ;;
	esac
	SUNIFDEF="$withval"/sunifdef
    ])

AC_ARG_WITH(openssl,
    [  --with-openssl     build with OpenSSL support],
    [],
    [openssl=no])

if test "$with_openssl" = yes || test "$with_openssl" = auto; then
    AC_CHECK_HEADERS(openssl/ssl.h,
        [AC_DEFINE([USE_SSL], 1,
            [Define to 1 to build with SSL support. (--with-openssl)])],
	[
            if test "$with_openssl" = yes; then
                AC_MSG_ERROR([header file <openssl/ssl.h> is required for SSL])
            else
                AC_MSG_WARN([header file <openssl/ssl.h> is required for SSL])
            fi
        ])

   AC_CHECK_LIB(crypto, CRYPTO_new_ex_data, [], [AC_MSG_ERROR([library 'crypto' is required for OpenSSL])])
   AC_CHECK_LIB(ssl,    SSL_new, [], [AC_MSG_ERROR([library 'ssl' is required for OpenSSL])])
fi

AC_ARG_WITH(ldap,
    [  --with-ldap     build with LDAP support],
    [AC_DEFINE([USE_LDAP], 1, [Define to 1 to build with LDAP support. (--with-ldap)])])
if test "$with_ldap" = yes ; then
  AC_CHECK_LIB(ldap, ldap_bind, [],
       [AC_MSG_ERROR([library 'ldap' is required for LDAP])],
       [$EXTRA_LDAP_LIBS])
  AC_CHECK_HEADERS(ldap.h, [],
                    [AC_MSG_ERROR([header file <ldap.h> is required for LDAP])])
  AC_CHECK_FUNCS([ldap_initialize])
fi

AC_ARG_WITH(pam,
    [  --with-pam     build with PAM support],
    [AC_DEFINE([USE_PAM], 1, [Define to 1 to build with PAM support. (--with-pam)])])
if test "$with_pam" = yes ; then
   AC_CHECK_LIB(pam, pam_start, [], [AC_MSG_ERROR([library 'pam' is required for PAM])])
   AC_CHECK_HEADERS(security/pam_appl.h, [],
                    [AC_CHECK_HEADERS(pam/pam_appl.h, [],
                                      [AC_MSG_ERROR([header file <security/pam_appl.h> or <pam/pam_appl.h> is required for PAM.])])])
fi


AC_ARG_WITH(memcached,
    [  --with-memcached=DIR     site header files for libmemcached in DIR],
    [
  case "$withval" in
  "" | y | ye | yes | n | no)
      AC_MSG_ERROR([*** You must supply an argument to the --with-memcached option.])
    ;;
    *)

  MEMCACHED_INCLUDE_DIR="$withval"/include
  MEMCACHED_LIB_DIR="$withval"/lib
  OLD_CFLAGS="$CFLAGS"
  CFLAGS="$CFLAGS -I$MEMCACHED_INCLUDE_DIR"
    AC_CHECK_HEADERS(
      [libmemcached/memcached.h],
        [AC_DEFINE([USE_MEMCACHED], 1, [Define to 1 to build with memcached support])],
        [AC_MSG_ERROR([header file <libmemcached/memcached.h> is required for memcached support])])
  CFLAGS=$OLD_CFLAGS
  AC_CHECK_LIB(memcached, memcached_create, [], [AC_MSG_ERROR(libmemcached is not installed)])
  MEMCACHED_INCLUDE_OPT="-I $MEMCACHED_INCLUDE_DIR"
  MEMCACHED_LINK_OPT="-L$MEMCACHED_LIB_DIR"
  MEMCACHED_RPATH_OPT="-rpath $MEMCACHED_LIB_DIR"
  ;;
  esac
  ])

CFLAGS="$CFLAGS -fno-strict-aliasing"

AC_SUBST(MEMCACHED_INCLUDE_OPT)
AC_SUBST(MEMCACHED_LINK_OPT)
AC_SUBST(MEMCACHED_RPATH_OPT)

OLD_LDFLAGS="$LDFLAGS"
LDFLAGS="-L$PGSQL_LIB_DIR $LDFLAGS"
OLD_LIBS="$LIBS"
AC_CHECK_LIB(pq, PQexecPrepared, [], [AC_MSG_ERROR(libpq is not installed or libpq is old)])
AC_CHECK_FUNCS(PQprepare)
AC_CHECK_FUNCS(PQpingParams)
LDFLAGS="$OLD_LDFLAGS"
LIBS="$OLD_LIBS"

AC_SUBST(PGSQL_INCLUDE_DIR)
AC_SUBST(PGSQL_LIB_DIR)
AC_SUBST(PGSQL_BIN_DIR)
AC_SUBST(MEMCACHED_DIR)

# --enable(disable)-rpath option
AC_ARG_ENABLE(rpath,
    [  --disable-rpath     do not embed shared library search path in executables],
    [case "${enableval}" in
      yes) rpath=yes ;;
      no)  rpath=no ;;
      esac],
    [rpath=yes]
)
AM_CONDITIONAL([enable_rpath], test x$rpath = xyes)

# Decide whether to use row lock against the sequence table for insert_lock.
# This lock method is compatible with pgpool-II 3.0 series(until 3.0.4).
AC_MSG_CHECKING([whether to use row lock against the sequence table for insert_lock])
PGAC_ARG_BOOL(enable, sequence-lock, no, [insert_lock compatible with pgpool-II 3.0 series (until 3.0.4)])
if test "$enable_sequence_lock" = yes && test "$enable_table_lock" = yes ; then
    AC_MSG_ERROR([--enable-table-lock cannot be enabled at the same time.])
fi
if test "$enable_sequence_lock" = yes ; then
    AC_DEFINE([USE_SEQUENCE_LOCK], 1,
            [Define to 1 if you want to use row lock against the sequence table for insert_lock. (--enable-sequence-lock)])
fi
AC_MSG_RESULT([$enable_sequence_lock])

# Decide whether to use table lock against the target table for insert_lock.
# This lock method is compatible with pgpool-II 2.2 and 2.3 series.
AC_MSG_CHECKING([whether to use table lock against the target table for insert_lock])
PGAC_ARG_BOOL(enable, table-lock, no, [insert_lock compatible with pgpool-II 2.2 and 2.3 series])
if test "$enable_table_lock" = yes && test "$enable_sequence_lock" = yes ; then
    AC_MSG_ERROR([--enable-sequence-lock cannot be enabled at the same time.])
fi
if test "$enable_table_lock" = yes ; then
    AC_DEFINE([USE_TABLE_LOCK], 1,
            [Define to 1 if you want to use table lock against the target table for insert_lock. (--enable-table-lock)])
fi
AC_MSG_RESULT([$enable_table_lock])

PGAC_ARG_BOOL(enable, cassert, no, [build with assertion checks])
if test "$enable_cassert" = yes ; then
        AC_DEFINE([USE_ASSERT_CHECKING], 1,
                [Define to 1 to build with assertion checks. (--enable-cassert)])
fi
AC_MSG_RESULT([enable cassert = $enable_cassert])

AM_CONFIG_HEADER(src/include/config.h)

AC_OUTPUT([Makefile doc/Makefile  doc/src/Makefile doc/src/sgml/Makefile doc.ja/Makefile  doc.ja/src/Makefile doc.ja/src/sgml/Makefile src/Makefile src/include/Makefile src/parser/Makefile src/libs/Makefile src/libs/pcp/Makefile src/tools/Makefile src/tools/pgmd5/Makefile src/tools/pgenc/Makefile src/tools/pcp/Makefile src/tools/pgproto/Makefile src/tools/watchdog/Makefile src/watchdog/Makefile])
