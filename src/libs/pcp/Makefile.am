AM_CPPFLAGS = -D_GNU_SOURCE -DPOOL_PRIVATE -I @PGSQL_INCLUDE_DIR@

lib_LTLIBRARIES = libpcp.la
libpcp_la_LDFLAGS = -version-info 2:0:0
dist_libpcp_la_SOURCES = pcp.c \
					../../utils/pool_path.c \
					../../tools/fe_port.c \
					../../tools/fe_memutils.c \
					../../utils/strlcpy.c \
					../../utils/pool_health_check_stats.c
nodist_libpcp_la_SOURCES = pcp_stream.c \
					md5.c \
					json.c

include_HEADERS = ../../../src/include/pcp/pcp.h \
					../../../src/include/pcp/libpcp_ext.h \
					../../../src/include/pool_type.h \
					../../../src/include/utils/pool_process_reporting.h

md5.c: ../../../src/auth/md5.c
	rm -f $@ && ln -s $< .
pcp_stream.c: ../../../src/utils/pcp/pcp_stream.c
	rm -f $@ && ln -s $< .
json.c: ../../../src/utils/json.c
	rm -f $@ && ln -s $< .

clean-local:
	-rm -f $(nodist_libpcp_la_SOURCES)
