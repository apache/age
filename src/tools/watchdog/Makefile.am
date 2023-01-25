parser_incdir = $(top_srcdir)/src/include/parser
AM_CPPFLAGS = -D_GNU_SOURCE -DPOOL_PRIVATE -I $(parser_incdir) -I @PGSQL_INCLUDE_DIR@ 
bin_PROGRAMS = wd_cli

bin_SCRIPTS = watchdog_setup


dist_wd_cli_SOURCES = wd_cli.c \
		../fe_port.c
nodist_wd_cli_SOURCES = ssl_utils.c \
		wd_ipc_conn.c \
		wd_commands.c \
		json_writer.c \
		json.c \
		pool_config_variables.c \
		pool_config.c \
		pool_path.c \
		fe_memutils.c \
		stringinfo.h \
		stringinfo.c \
		strlcpy.c \
		socket_stream.c \
		regex_array.c \
		psprintf.c \
		md5.c

DEFS = @DEFS@ \
    -DDEFAULT_CONFIGDIR=\"$(sysconfdir)\" -DPOOL_TOOLS

watchdog_setup:
	ln -s ../../test/watchdog_setup .
psprintf.c: ../../../src/utils/psprintf.c
	rm -f $@ && ln -s $< .
md5.c: ../../../src/auth/md5.c
	rm -f $@ && ln -s $< .
md5.h: ../../../src/include/auth/md5.h
	rm -f $@ && ln -s $< .
socket_stream.c: ../../../src/utils/socket_stream.c
	rm -f $@ && ln -s $< .
strlcpy.c: ../../../src/utils/strlcpy.c
	rm -f $@ && ln -s $< .
regex_array.c: ../../../src/utils/regex_array.c
	rm -f $@ && ln -s $< .
wd_ipc_conn.c: ../../../src/watchdog/wd_ipc_conn.c
	rm -f $@ && ln -s $< .
wd_commands.c: ../../../src/watchdog/wd_commands.c
	rm -f $@ && ln -s $< .
wd_utils.c: ../../../src/watchdog/wd_utils.c
	rm -f $@ && ln -s $< .
ssl_utils.c: ../../../src/utils/ssl_utils.c
	rm -f $@ && ln -s $< .
base64.h: ../../../src/include/utils/base64.h
	rm -f $@ && ln -s $< .
ssl_utils.h: ../../../src/include/utils/ssl_utils.h
	rm -f $@ && ln -s $< .
json_writer.c: ../../../src/utils/json_writer.c
	rm -f $@ && ln -s $< .
json.c: ../../../src/utils/json.c
	rm -f $@ && ln -s $< .
stringinfo.c: ../../../src/parser/stringinfo.c
	rm -f $@ && ln -s $< .
stringinfo.h: ../../../src/include/parser/stringinfo.h
	rm -f $@ && ln -s $< .
pool_config_variables.c: ../../../src/config/pool_config_variables.c
	rm -f $@ && ln -s $< .
pool_config.c: ../../../src/config/pool_config.c
	rm -f $@ && ln -s $< .
pool_path.c: ../../../src/utils/pool_path.c
	rm -f $@ && ln -s $< .
fe_memutils.c: ../../../src/tools/fe_memutils.c
	rm -f $@ && ln -s $< .

clean-local:
	-rm -f $(nodist_wd_cli_SOURCES) $(bin_SCRIPTS)
