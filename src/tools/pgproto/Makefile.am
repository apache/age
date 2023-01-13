AM_CPPFLAGS = -D_GNU_SOURCE -I @PGSQL_INCLUDE_DIR@
bin_PROGRAMS = pgproto

pgproto_SOURCES = main.c read.c send.c extended_query.c buffer.c fe_memutils.c
pgproto_LDADD = -L@PGSQL_LIB_DIR@ -lpq

