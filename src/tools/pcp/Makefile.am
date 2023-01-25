libs_dir = $(top_srcdir)/src/libs
AM_CPPFLAGS = -D_GNU_SOURCE -DPOOL_PRIVATE -I @PGSQL_INCLUDE_DIR@

bin_PROGRAMS =  pcp_stop_pgpool \
				pcp_node_count \
				pcp_node_info \
				pcp_health_check_stats \
				pcp_proc_count \
				pcp_proc_info \
				pcp_detach_node \
				pcp_attach_node \
				pcp_recovery_node \
				pcp_promote_node \
				pcp_pool_status \
				pcp_watchdog_info\
				pcp_reload_config

client_sources = pcp_frontend_client.c ../fe_memutils.c ../../utils/sprompt.c ../../utils/pool_path.c

pcp_stop_pgpool_SOURCES = $(client_sources)
pcp_stop_pgpool_LDADD = $(libs_dir)/pcp/libpcp.la
pcp_node_count_SOURCES = $(client_sources)
pcp_node_count_LDADD = $(libs_dir)/pcp/libpcp.la
pcp_node_info_SOURCES = $(client_sources)
pcp_health_check_stats_LDADD = $(libs_dir)/pcp/libpcp.la
pcp_health_check_stats_SOURCES = $(client_sources) ../../utils/pool_health_check_stats.c
pcp_node_info_LDADD = $(libs_dir)/pcp/libpcp.la
pcp_proc_count_SOURCES = $(client_sources)
pcp_proc_count_LDADD = $(libs_dir)/pcp/libpcp.la
pcp_proc_info_SOURCES = $(client_sources)
pcp_proc_info_LDADD = $(libs_dir)/pcp/libpcp.la
pcp_detach_node_SOURCES = $(client_sources)
pcp_detach_node_LDADD = $(libs_dir)/pcp/libpcp.la
pcp_attach_node_SOURCES = $(client_sources)
pcp_attach_node_LDADD = $(libs_dir)/pcp/libpcp.la
pcp_recovery_node_SOURCES = $(client_sources)
pcp_recovery_node_LDADD = $(libs_dir)/pcp/libpcp.la
pcp_pool_status_SOURCES = $(client_sources)
pcp_pool_status_LDADD = $(libs_dir)/pcp/libpcp.la
pcp_promote_node_SOURCES = $(client_sources)
pcp_promote_node_LDADD = $(libs_dir)/pcp/libpcp.la
pcp_watchdog_info_SOURCES = $(client_sources)
pcp_watchdog_info_LDADD = $(libs_dir)/pcp/libpcp.la
pcp_reload_config_SOURCES = $(client_sources)
pcp_reload_config_LDADD = $(libs_dir)/pcp/libpcp.la

