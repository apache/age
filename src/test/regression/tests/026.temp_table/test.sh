#!/usr/bin/env bash
#-------------------------------------------------------------------
# test script for temporary table tracing.

WHOAMI=`whoami`
source $TESTLIBS
TESTDIR=testdir
PSQL=$PGBIN/psql

rm -fr $TESTDIR
mkdir $TESTDIR
cd $TESTDIR

# create test environment
echo -n "creating test environment..."
$PGPOOL_SETUP -n 2 || exit 1
echo "done."

source ./bashrc.ports
export PGPORT=$PGPOOL_PORT
./startall
wait_for_pgpool_startup

# test1: default (check_temp_table = catalog)

$PSQL -a -f ../temp_table.sql test > results.txt

cmp results.txt ../expected.txt || exit 1

# test2: trace

echo "check_temp_table = trace" >> etc/pgpool.conf

./pgpool_reload

$PSQL -a -f ../temp_table.sql test > results.txt

cmp results.txt ../expected.txt || exit 1

./shutdownall

exit 0
