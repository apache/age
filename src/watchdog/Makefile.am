top_builddir = ../..
AM_CPPFLAGS = -D_GNU_SOURCE -I @PGSQL_INCLUDE_DIR@
ARFLAGS = cr
WATCHDOG_DEBUG=0
noinst_LIBRARIES = lib-watchdog.a

lib_watchdog_a_SOURCES = \
	watchdog.c \
	wd_if.c \
	wd_lifecheck.c \
	wd_commands.c \
	wd_internal_commands.c \
	wd_ipc_conn.c \
	wd_json_data.c \
	wd_ping.c \
	wd_heartbeat.c \
	wd_utils.c \
	wd_escalation.c

DEFS = @DEFS@ -DWATCHDOG_DEBUG_OPTS=$(WATCHDOG_DEBUG)
