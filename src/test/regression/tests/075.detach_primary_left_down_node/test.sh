#!/usr/bin/env bash
#-------------------------------------------------------------------
# test script for a case: detach primary node left down node.
#
# reported: https://www.pgpool.net/pipermail/pgpool-hackers/2021-June/003916.html

source $TESTLIBS
WHOAMI=`whoami`
TESTDIR=testdir

rm -fr $TESTDIR
mkdir $TESTDIR
cd $TESTDIR

# create test environment
echo -n "creating test environment..."
$PGPOOL_SETUP -n 4 || exit 1
echo "done."

source ./bashrc.ports
export PGPORT=$PGPOOL_PORT

./startall
wait_for_pgpool_startup

# detach node 0
$PGPOOL_INSTALL_DIR/bin/pcp_detach_node -w -h localhost -p $PCP_PORT 0
wait_for_pgpool_startup

# check to see if alll nodes are up
echo -n "starting to check follow primary results: "
date
cnt=60
while [ $cnt -gt 0 ]
do
    $PGBIN/psql -c "show pool_nodes" test 2>&1|grep -E 'down|error'
    if [ $? != 0 ];then
	echo "test succeeded"
	./shutdownall
	exit 0
    fi
    cnt=`expr $cnt - 1`
    echo "cnt: $cnt"
    sleep 1
done
echo "test failed".
./shutdownall

exit 1
