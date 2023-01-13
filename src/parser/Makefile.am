parser_incdir = $(top_srcdir)/src/include/parser
AM_CPPFLAGS = -D_GNU_SOURCE -I $(parser_incdir) -I @PGSQL_INCLUDE_DIR@
noinst_LIBRARIES = libsql-parser.a

libsql_parser_a_SOURCES  = \
	copyfuncs.c \
	gram.y \
	gram_minimal.y \
	keywords.c \
	kwlookup.c \
	list.c \
	makefuncs.c \
	nodes.c \
	outfuncs.c \
	parser.c \
	scansup.c \
	stringinfo.c \
	value.c \
	$(top_srcdir)/src/utils/mmgr/mcxt.c \
	$(top_srcdir)/src/utils/mmgr/aset.c \
	$(top_srcdir)/src/utils/error/elog.c \
	wchar.c scan.c

if use_repl_snprintf
  libsql_parser_a_SOURCES  += snprintf.c
endif

EXTRA_DIST = scan.c scan.l
ARFLAGS = cr

AM_YFLAGS = -d -Wno-yacc

gram.c: gram.y scan.c
gram_minimal.c: gram_minimal.y scan.c

scan.c: scan.l
	$(LEX) -o'scan.c' $<

generate_parsers: gram_template.y
	$(SUNIFDEF) --undef pgpool_minimal_parser gram_template.y > gram.y||:
	$(SUNIFDEF) --define pgpool_minimal_parser gram_template.y > gram_minimal.y||:

.PHONY: generate_parsers 
