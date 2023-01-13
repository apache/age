AUTOMAKE_OPTIONS = subdir-objects
libs_dir = $(top_srcdir)/src/libs
AM_CPPFLAGS = -D_GNU_SOURCE -I @PGSQL_INCLUDE_DIR@
bin_PROGRAMS = pgpool
HEALTHCHECK_DEBUG=0

pgpool_SOURCES = main/main.c \
	main/pool_globals.c \
	main/pgpool_main.c \
	main/health_check.c \
	main/pool_internal_comms.c \
	main/pgpool_logger.c \
	config/pool_config.l \
	config/pool_config_variables.c \
	pcp_con/pcp_child.c \
	pcp_con/pcp_worker.c \
	pcp_con/recovery.c \
	auth/md5.c \
	auth/pool_auth.c \
	auth/pool_passwd.c \
	auth/pool_hba.c \
	auth/auth-scram.c \
	protocol/pool_proto2.c \
	protocol/child.c \
	protocol/pool_pg_utils.c \
	protocol/pool_process_query.c \
	protocol/pool_connection_pool.c \
	protocol/pool_proto_modules.c \
	query_cache/pool_memqcache.c \
	protocol/CommandComplete.c \
	context/pool_session_context.c \
	context/pool_process_context.c \
	context/pool_query_context.c \
	streaming_replication/pool_worker_child.c \
	rewrite/pool_timestamp.c \
	rewrite/pool_lobj.c \
	utils/pool_select_walker.c \
	utils/strlcpy.c \
	utils/psprintf.c \
	utils/pool_params.c \
	utils/ps_status.c \
	utils/pool_shmem.c \
	utils/pool_sema.c \
	utils/pool_signal.c \
	utils/pool_path.c \
	utils/pool_ip.c \
	utils/pool_relcache.c \
	utils/pool_process_reporting.c \
	utils/pool_ssl.c \
	utils/pool_stream.c \
	utils/socket_stream.c \
	utils/getopt_long.c \
	utils/mmgr/mcxt.c \
	utils/mmgr/aset.c \
	utils/error/elog.c \
	utils/error/assert.c \
    utils/pcp/pcp_stream.c \
	utils/regex_array.c \
	utils/json_writer.c \
	utils/json.c \
	utils/scram-common.c \
	utils/base64.c \
	utils/sha2.c \
	utils/ssl_utils.c \
	utils/statistics.c \
	utils/pool_health_check_stats.c

DEFS = @DEFS@ \
	-DDEFAULT_CONFIGDIR=\"$(sysconfdir)\" \
	-DPGSQL_BIN_DIR=\"$(PGSQL_BIN_DIR)\"

sysconf_DATA = sample/pgpool.conf.sample \
			   sample/pcp.conf.sample \
			   sample/pool_hba.conf.sample \
			   sample/scripts/failover.sh.sample \
			   sample/scripts/follow_primary.sh.sample \
			   sample/scripts/pgpool_remote_start.sample \
			   sample/scripts/recovery_1st_stage.sample \
			   sample/scripts/replication_mode_recovery_1st_stage.sample \
			   sample/scripts/replication_mode_recovery_2nd_stage.sample \
			   sample/scripts/escalation.sh.sample \
			   sample/scripts/aws_eip_if_cmd.sh.sample \
			   sample/scripts/aws_rtb_if_cmd.sh.sample


pkgdata_DATA = sql/insert_lock.sql \
			   sample/pgpool.pam

# suggested by libtoolize
#ACLOCAL_AMFLAGS = -I m4

pgpool_LDADD = -L@PGSQL_LIB_DIR@ -lpq parser/libsql-parser.a \
						parser/nodes.o \
						watchdog/lib-watchdog.a \
						-lpthread
if enable_rpath
  pgpool_LDFLAGS = -rpath @PGSQL_LIB_DIR@ -rpath $(libdir)
else
  pgpool_LDFLAGS = 
endif

AM_YFLAGS = -d

EXTRA_DIST = sample/pgpool.pam \
		sample/scripts/failover.sh.sample \
		sample/scripts/follow_primary.sh.sample \
		sample/scripts/pgpool_remote_start.sample \
		sample/scripts/recovery_1st_stage.sample \
		sample/scripts/replication_mode_recovery_1st_stage.sample \
		sample/scripts/replication_mode_recovery_2nd_stage.sample \
		sample/scripts/escalation.sh.sample \
		sample/scripts/aws_eip_if_cmd.sh.sample \
		sample/scripts/aws_rtb_if_cmd.sh.sample \
		sample/pgpool.conf.sample sample/pool_hba.conf.sample \
		sample/pcp.conf.sample \
		sql/Makefile \
		sql/insert_lock.sql \
		sql/pgpool-recovery/pgpool-recovery.c \
		sql/pgpool-recovery/pgpool-recovery.sql.in \
		sql/pgpool-recovery/uninstall_pgpool-recovery.sql \
		sql/pgpool-recovery/pgpool_recovery--1.4.sql \
		sql/pgpool-recovery/pgpool_recovery--1.3--1.4.sql \
		sql/pgpool-recovery/pgpool_recovery--1.3.sql \
		sql/pgpool-recovery/pgpool_recovery--1.2--1.3.sql \
		sql/pgpool-recovery/pgpool_recovery--1.2.sql \
		sql/pgpool-recovery/pgpool_recovery--1.1--1.2.sql \
		sql/pgpool-recovery/pgpool_recovery--1.1.sql \
		sql/pgpool-recovery/pgpool_recovery.control \
		sql/pgpool-recovery/Makefile \
		sql/pgpool-regclass/pgpool-regclass.c \
		sql/pgpool-regclass/pgpool-regclass.sql.in \
		sql/pgpool-regclass/uninstall_pgpool-regclass.sql \
		sql/pgpool-regclass/pgpool_regclass--1.0.sql \
		sql/pgpool-regclass/pgpool_regclass.control \
		sql/pgpool-regclass/Makefile \
		sql/pgpool_adm/pgpool_adm.c \
		sql/pgpool_adm/pgpool_adm.h \
		sql/pgpool_adm/pgpool_adm.control \
		sql/pgpool_adm/pgpool_adm--1.1.sql \
		sql/pgpool_adm/pgpool_adm--1.0--1.1.sql \
		sql/pgpool_adm/pgpool_adm--1.0.sql \
		sql/pgpool_adm/pgpool_adm--1.2.sql \
		sql/pgpool_adm/pgpool_adm--1.1--1.2.sql \
		sql/pgpool_adm/pgpool_adm--1.3.sql \
		sql/pgpool_adm/pgpool_adm--1.2--1.3.sql \
		sql/pgpool_adm/pgpool_adm--1.4.sql \
		sql/pgpool_adm/pgpool_adm--1.3--1.4.sql \
		sql/pgpool_adm/Makefile \
		test/parser/expected/copy.out test/parser/expected/create.out \
		test/parser/expected/cursor.out test/parser/expected/delete.out \
		test/parser/expected/drop.out test/parser/expected/insert.out \
		test/parser/expected/misc.out test/parser/expected/prepare.out \
		test/parser/expected/privileges.out test/parser/expected/scanner.out \
		test/parser/expected/select.out \
		test/parser/expected/transaction.out test/parser/expected/update.out \
		test/parser/expected/v84.out test/parser/expected/v90.out \
		test/parser/expected/var.out \
		test/parser/input/alter.sql \
		test/parser/input/copy.sql test/parser/input/create.sql \
		test/parser/input/cursor.sql test/parser/input/delete.sql \
		test/parser/input/drop.sql test/parser/input/insert.sql \
		test/parser/input/misc.sql test/parser/input/prepare.sql \
		test/parser/input/privileges.sql test/parser/input/scanner.sql \
		test/parser/input/select.sql \
		test/parser/input/transaction.sql test/parser/input/update.sql \
		test/parser/input/v84.sql test/parser/input/v90.sql \
		test/parser/input/var.sql \
		test/parser/.cvsignore test/parser/Makefile \
		test/parser/README test/parser/main.c \
		test/parser/pool.h test/parser/run-test \
		test/parser/parse_schedule \
		test/C/Makefile test/C/test_extended.c \
		test/pdo-test/README.euc_jp test/pdo-test/collections.inc test/pdo-test/def.inc \
		test/pdo-test/pdotest.php test/pdo-test/regsql.inc \
		test/pdo-test/SQLlist/test1.sql test/pdo-test/SQLlist/test2.sql \
		test/pdo-test/mod/database.inc test/pdo-test/mod/errorhandler.inc \
		test/pgpool_setup.in test/watchdog_setup.in test/regression \
		redhat/pgpool.init \
		redhat/pgpool_rhel.sysconfig redhat/pgpool_rhel6.sysconfig \
	   	redhat/pgpool.conf.sample.patch \
		redhat/pgpool.service \
		pgpool.spec

bin_SCRIPTS = test/pgpool_setup test/watchdog_setup

%: %.in
	sed -e 's#[@][@]PGPOOL_INSTALL_DIR[@][@]#$(prefix)#g' \
		-e 's#[@][@]PGPOOL_CONFIG_DIR[@][@]#$(sysconfdir)#g' \
		-e 's#[@][@]PGSQL_BIN_DIR[@][@]#$(PGSQL_BIN_DIR)#g' \
		-e 's#[@][@]PGSQL_LIB_DIR[@][@]#$(PGSQL_LIB_DIR)#g' $^ > $@
	chmod +x $@

clean-local:
	-rm -f test/pgpool_setup test/watchdog_setup

SUBDIRS = parser libs watchdog . tools include
