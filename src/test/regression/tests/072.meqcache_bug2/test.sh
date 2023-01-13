#!/usr/bin/env bash
#-------------------------------------------------------------------
# test script for a memqcache bug.
#
# If an execute message is issued right after Sync message and query
# cache hits, Pgpool-II hangs.

source $TESTLIBS
WHOAMI=`whoami`
TESTDIR=testdir
PGPROTO=$PGPOOL_INSTALL_DIR/bin/pgproto

rm -fr $TESTDIR
mkdir $TESTDIR
cd $TESTDIR

# create test environment
echo -n "creating test environment..."
$PGPOOL_SETUP -m s -n 2 || exit 1
echo "done."

source ./bashrc.ports

export PGPORT=$PGPOOL_PORT

echo "memory_cache_enabled = on" >> etc/pgpool.conf
sh startall
wait_for_pgpool_startup

$PGPROTO -d test -f ../extended_query_test.data	# hang here if the bug bites you...
sh shutdownall
exit 0
