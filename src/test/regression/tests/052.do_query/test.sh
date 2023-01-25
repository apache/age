#!/usr/bin/env bash
#-------------------------------------------------------------------
# test script for do_query.
#
# If do_query failed while executing in extended query mode, pgpool hangs.
# precondition to trigger the error: remove pgpool_regclass.so or some such.
#
BUG="do_query"
WHOAMI=`whoami`
source $TESTLIBS
TESTDIR=testdir
PGBENCH=$PGBENCH_PATH

rm -fr $TESTDIR
mkdir $TESTDIR
cd $TESTDIR

# create test environment
echo -n "creating test environment..."
$PGPOOL_SETUP -m s -n 1 --no-stop|| exit 1
echo "done."

source ./bashrc.ports

#./startall

export PGPORT=$PGPOOL_PORT

wait_for_pgpool_startup

echo "SELECT 1" > select.pgbench
$PGBENCH -i test

# run test. This will hung if the bug is remained.
($PGBENCH -c 10 -t 10 -M extended -f select.pgbench test)&
sleep 5
kill $!

if [ $? = 0 ];then
	./shutdownall
	exit 1
fi

./shutdownall
