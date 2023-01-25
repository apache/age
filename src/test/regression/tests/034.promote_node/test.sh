#!/usr/bin/env bash
#-------------------------------------------------------------------
# test script for pcp_promote_mode.
#

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

for i in 1 2 3 4 5 6 7 8 9 10
do
    $PGPOOL_INSTALL_DIR/bin/pcp_node_info -h localhost -w -p $PCP_PORT
    if [ $? = 0 ];then
	break;
    fi
    sleep 1
done

# promote node 3
$PGPOOL_INSTALL_DIR/bin/pcp_promote_node -h localhost -w -p $PCP_PORT -s 3
wait_for_pgpool_startup

# check to see if alll nodes are up
echo -n "starting to check follow primary results: "
date
cnt=60
while [ $cnt -gt 0 ]
do
    # check to see if primary is now node 3
    $PGBIN/psql -c "show pool_nodes" test|grep down
    if [ $? != 0 ];then
	node=`$PGBIN/psql -c "show pool_nodes" test|grep primary|awk '{print $1}'`
	if [ "$node" = 3 ];then
	    echo "test succeeded"
	    ./shutdownall
	    exit 0
	fi
    fi
    cnt=`expr $cnt - 1`
    echo "cnt: $cnt"
    sleep 1
done
echo "test failed".
./shutdownall

exit 1
