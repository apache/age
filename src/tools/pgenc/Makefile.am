AM_CPPFLAGS = -D_GNU_SOURCE -DPOOL_PRIVATE -I @PGSQL_INCLUDE_DIR@
bin_PROGRAMS = pg_enc

dist_pg_enc_SOURCES = pg_enc.c \
		../fe_port.c
nodist_pg_enc_SOURCES = ssl_utils.c \
		md5.c \
		base64.c \
		pool_passwd.c \
		strlcpy.c \
		regex_array.c \
		pool_config_variables.c \
		pool_config.c \
		fe_memutils.c \
		pool_path.c

DEFS = @DEFS@ \
    -DDEFAULT_CONFIGDIR=\"$(sysconfdir)\" -DPOOL_TOOLS

pool_passwd.c: ../../../src/auth/pool_passwd.c
	rm -f $@ && ln -s $< .
pool_path.c: ../../../src/utils/pool_path.c
	rm -f $@ && ln -s $< .
md5.c: ../../../src/auth/md5.c
	rm -f $@ && ln -s $< .
md5.h: ../../../src/include/auth/md5.h
	rm -f $@ && ln -s $< .
base64.c: ../../../src/utils/base64.c
	rm -f $@ && ln -s $< .
ssl_utils.c: ../../../src/utils/ssl_utils.c
	rm -f $@ && ln -s $< .
base64.h: ../../../src/include/utils/base64.h
	rm -f $@ && ln -s $< .
ssl_utils.h: ../../../src/include/utils/ssl_utils.h
	rm -f $@ && ln -s $< .
strlcpy.c: ../../../src/utils/strlcpy.c
	rm -f $@ && ln -s $< .
regex_array.c: ../../../src/utils/regex_array.c
	rm -f $@ && ln -s $< .
pool_config_variables.c: ../../../src/config/pool_config_variables.c
	rm -f $@ && ln -s $< .
pool_config.c: ../../../src/config/pool_config.c
	rm -f $@ && ln -s $< .
fe_memutils.c: ../../../src/tools/fe_memutils.c
	rm -f $@ && ln -s $< .

clean-local:
	-rm -f $(nodist_pg_enc_SOURCES)
