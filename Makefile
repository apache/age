#-------------------------------------------------------------------------
#
# Makefile for src/bin/psql
#
# Portions Copyright (c) 1996-2022, PostgreSQL Global Development Group
# Portions Copyright (c) 1994, Regents of the University of California
#
# src/bin/psql/Makefile
#
#-------------------------------------------------------------------------

PGFILEDESC = "psql - the PostgreSQL interactive terminal"
PGAPPICON=win32

subdir = src/bin/psql
top_builddir = ../../..

PG_CONFIG = pg_config

ifdef USE_PGXS
PGXS := $(shell $(PG_CONFIG) --pgxs)
include $(PGXS)
else
subdir = bin/
top_builddir = ../..
include $(top_builddir)/src/Makefile.global
include $(top_srcdir)/src/Makefile.global
endif

override CPPFLAGS := -I. -I$(srcdir) -I$(libpq_srcdir)  -I$(top_srcdir)/src/include -I$(top_srcdir)/src/interfaces/libpq $(CPPFLAGS)
LDFLAGS_INTERNAL += -L$(top_builddir)/src/fe_utils -lpgfeutils $(libpq_pgport)

OBJS = \
	$(WIN32RES) \
	command.o \
	common.o \
	copy.o \
	crosstabview.o \
	describe.o \
	help.o \
	input.o \
	large_obj.o \
	mainloop.o \
	prompt.o \
	psqlscanslash.o \
	cypher.o \
	cypher.tab.o \
	sql_help.o \
	startup.o \
	stringutils.o \
	tab-complete.o \
	variables.o


all: psql

psql: $(OBJS)
	flex -b -Cfe -p -p -o'cypher.c' cypher.l
	bison -d cypher.y
	$(CC) $(CFLAGS) $(OBJS) $(LDFLAGS) $(LDFLAGS_EX) $(LIBS) -o agesql


