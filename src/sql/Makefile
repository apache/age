# $Header$
#
# pgpool: a language independent connection pool server for PostgreSQL
# written by Tatsuo Ishii
#
# Copyright (c) 2003-2013	PgPool Global Development Group
#
# Permission to use, copy, modify, and distribute this software and
# its documentation for any purpose and without fee is hereby
# granted, provided that the above copyright notice appear in all
# copies and that both that copyright notice and this permission
# notice appear in supporting documentation, and that the name of the
# author not be used in advertising or publicity pertaining to
# distribution of the software without specific, written prior
# permission. The author makes no representations about the
# suitability of this software for any purpose.  It is provided "as
# is" without express or implied warranty.
#
# Makefile for sql/ directory.

subdir = sql

# if you are using PostgreSQL 8.0 or later,
# using pg_config is recommended.
# if you are not, comment out following line and...
USE_PGXS = true
# set top_builddir to the PostgreSQL build source tree top.
# (for example /usr/local/src/postgresql-9.0)
top_builddir = ..

ifdef USE_PGXS
PG_CONFIG ?= pg_config
PGXS = $(shell $(PG_CONFIG) --pgxs)
include $(PGXS)
else
subdir = contrib
include $(top_builddir)/src/Makefile.global
include $(top_srcdir)/contrib/contrib-global.mk
endif

SUBDIRS = \
		pgpool-recovery \
		pgpool-regclass \
		pgpool_adm

$(recurse)
