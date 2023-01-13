MODULES = pgpool-recovery
DATA_built = pgpool-recovery.sql
DATA = uninstall_pgpool-recovery.sql

EXTENSION = pgpool_recovery
DATA = pgpool_recovery--1.1.sql pgpool_recovery--1.2.sql pgpool_recovery--1.1--1.2.sql \
	pgpool_recovery--1.3.sql pgpool_recovery--1.2--1.3.sql \
	pgpool_recovery--1.4.sql pgpool_recovery--1.3--1.4.sql

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
subdir = contrib/pgpool-recovery
#top_builddir = ../..
include $(top_builddir)/src/Makefile.global
include $(top_srcdir)/contrib/contrib-global.mk
endif
