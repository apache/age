#
# Makefile for example programs
#

subdir = .
top_builddir = /usr/local/src/pgsql/current/pgsql
include $(top_builddir)/src/Makefile.global

ifeq ($(PORTNAME), win32)
LDLIBS += -lws2_32
endif

override CPPFLAGS := -I$(libpq_srcdir) $(CPPFLAGS)
override LDLIBS := $(libpq_pgport) $(LDLIBS)


PROGS = test_extended

all: $(PROGS)

clean: 
	rm -f $(PROGS)
