#!/usr/bin/env bash
#-------------------------------------------------------------------
# Testing cancel query.
# [pgpool-hackers: 694] pgpool-II 3.4 can't cancel query

PSQL=$PGBIN/psql

source $TESTLIBS
TESTDIR=testdir

rm -fr $TESTDIR
mkdir $TESTDIR
cd $TESTDIR

# create test environment
echo -n "creating test environment..."
$PGPOOL_SETUP -m n -n 1 || exit 1
echo "done."

source ./bashrc.ports

export PGPORT=$PGPOOL_PORT

# start pgpool-II
./startall

sleep 1

# executing long running query
$PSQL -c "select pg_sleep(10000);" test &

# get psql process pid
psqlpid=$!

sleep 2

# send CtrlC to psql to cancel the query
kill -INT $psqlpid

sleep 2

RESULT=`ps -p $psqlpid | grep psql`

./shutdownall

if [ -z "$RESULT" ]; then
echo -n "test passed."
exit 0
fi

echo -n "test failed. psql process still exists"
echo -n "$RESULT"

exit 1
