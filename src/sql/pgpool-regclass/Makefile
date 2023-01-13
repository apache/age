MODULES = pgpool-regclass
DATA_built = pgpool-regclass.sql
DATA = uninstall_pgpool-regclass.sql

EXTENSION = pgpool_regclass
DATA = pgpool_regclass--1.0.sql

# if you are using PostgreSQL 8.0 or later,
# using pg_config is recommended.
# if you are not, comment out following line and...
USE_PGXS = true
# set top_builddir to the PostgreSQL build source tree top.
# (for example /usr/local/src/postgresql-8.4)
top_builddir = ../..

ifdef USE_PGXS
PG_CONFIG ?= pg_config
PGXS = $(shell $(PG_CONFIG) --pgxs)
include $(PGXS)
else
subdir = contrib/pgpool-regclass
#top_builddir = ../..
include $(top_builddir)/src/Makefile.global
include $(top_srcdir)/contrib/contrib-global.mk
endif
