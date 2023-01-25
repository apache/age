#AM_CPPFLAGS = -D_GNU_SOURCE -I @PGSQL_INCLUDE_DIR@
AUTOMAKE_OPTIONS = subdir-objects

# suggested by libtoolize
ACLOCAL_AMFLAGS = -I m4

docs:
	$(MAKE) -C doc all && $(MAKE) -C doc.ja all && $(MAKE)
.PHONY: docs

SUBDIRS = src
DIST_SUBDIRS = src doc doc.ja


