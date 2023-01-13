# contrib/pgpool_adm/Makefile

MODULE_big = pgpool_adm
OBJS = pgpool_adm.o
PG_CPPFLAGS = -I$(libpq_srcdir) -I../../include/pcp

EXTENSION = pgpool_adm
DATA = pgpool_adm--1.0.sql pgpool_adm--1.1.sql pgpool_adm--1.2.sql pgpool_adm--1.3.sql \
pgpool_adm--1.0--1.1.sql pgpool_adm--1.1--1.2.sql pgpool_adm--1.2--1.3.sql \
pgpool_adm--1.4.sql pgpool_adm--1.3--1.4.sql
SHLIB_LINK =  -L../../libs/pcp/.libs -lpcp -Wl,--as-needed -Wl,-rpath,'${prefix}/lib',--enable-new-dtags
# if you are using PostgreSQL 8.0 or later,
# using pg_config is recommended.
# if you are not, comment out following line and...
USE_PGXS = true
# set top_builddir to the PostgreSQL build source tree top.
# (for example /usr/local/src/postgresql-8.4)
top_builddir = ../..

ifdef USE_PGXS
PG_CONFIG ?= pg_config
PGXS := $(shell $(PG_CONFIG) --pgxs)
include $(PGXS)
else
subdir = contrib/pgpool_adm
#top_builddir = ../..
include $(top_builddir)/src/Makefile.global
include $(top_srcdir)/contrib/contrib-global.mk
endif
