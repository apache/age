#!/usr/bin/env bash
#-------------------------------------------------------------------
# test script for connection_life_time
#
source $TESTLIBS
TESTDIR=testdir
PSQL=$PGBIN/psql
PG_CTL=$PGBIN/pg_ctl
PGBENCH=$PGBIN/pgbench
export PGDATABASE=test

rm -fr $TESTDIR
mkdir $TESTDIR
cd $TESTDIR

# create test environment.
echo -n "creating test environment..."
$PGPOOL_SETUP || exit 1
echo "done."

dir=`pwd`

source ./bashrc.ports

timeout=5
echo "connection_life_time = $timeout" >> etc/pgpool.conf

./startall

export PGPORT=$PGPOOL_PORT

wait_for_pgpool_startup

# get backend pid
pid=`$PSQL -t -p 11000 -c "select pg_backend_pid()" test`
echo "backend pid is: $pid"
sleep `expr $timeout + 1`
ps $pid
if [ $? = 0 ];then
    echo "backend pid $pid still exists"
    ./shutdownall
    exit 1
fi

echo "ok with primary node = 0"

# swap node 0 and node 1
pg_ctl -D data0 -m f stop

wait_for_pgpool_startup

$PGPOOL_INSTALL_DIR/bin/pcp_recovery_node -h localhost -p $PCP_PORT -w 0

wait_for_pgpool_startup

# get backend pid
pid=`$PSQL -t -p 11000 -c "select pg_backend_pid()" test`
echo "backend pid is: $pid"
sleep `expr $timeout + 2`
ps $pid
if [ $? = 0 ];then
    echo "backend pid $pid still exists"
    ./shutdownall
    exit 1
fi

./shutdownall

exit 0
