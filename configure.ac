#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.71])
AC_INIT([agbolt],[0.1])
AC_CONFIG_SRCDIR([config.h.in])
AC_CONFIG_HEADERS([config.h])

AM_INIT_AUTOMAKE([foreign no-dependencies subdir-objects])

# Checks for programs.
AC_PROG_CC

# Checks for libraries.

# Checks for header files.
AC_CHECK_HEADERS([unistd.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_TYPE_PID_T

# Checks for library functions.
AC_FUNC_FORK

# postgres dependancy check
AC_CHECK_PROGS(PGCONFIG, pg_config)
PGSQL_INCLUDE_DIR=`pg_config --includedir`
PGSQL_LIB_DIR=`pg_config --libdir`
PGSQL_BIN_DIR=`pg_config --bindir`
PGSQL_INCLUDE_SERVER_DIR=`pg_config --includedir-server`

AC_SUBST(PGSQL_INCLUDE_DIR)
AC_SUBST(PGSQL_LIB_DIR)
AC_SUBST(PGSQL_BIN_DIR)
AC_SUBST(PGSQL_INCLUDE_SERVER_DIR)

AM_CONDITIONAL([enable_rpath], test x$rpath = xyes)

AC_CONFIG_FILES([Makefile])
AC_OUTPUT