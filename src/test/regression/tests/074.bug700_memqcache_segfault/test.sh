#!/usr/bin/env bash
#-------------------------------------------------------------------
# test script for a memqcache bug 700.
#
# Pgpool-II crashes upon receiving CloseComplete.
# This only happens in other than streaming and logical replication mode.

source $TESTLIBS
WHOAMI=`whoami`
TESTDIR=testdir
PGPROTO=$PGPOOL_INSTALL_DIR/bin/pgproto

for mode in s i r n
do
    rm -fr $TESTDIR
    mkdir $TESTDIR
    cd $TESTDIR

    # create test environment
    echo -n "creating test environment in mode = $mode..."
    $PGPOOL_SETUP -m $mode -n 1 || exit 1
    echo "done."

    source ./bashrc.ports

    export PGPORT=$PGPOOL_PORT

    echo "memory_cache_enabled = on" >> etc/pgpool.conf
    echo "log_min_messages = debug1" >> etc/pgpool.conf
    ./startall
    wait_for_pgpool_startup

    $PGPROTO -d test -f ../pgproto.data > result.txt 2>&1
    cmp ../expected.txt result.txt
    if [ $? != 0 ];then
	./shutdownall
	exit 1
    fi
    ./shutdownall
    cd ..
done

exit 0
