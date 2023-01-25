#!/usr/bin/env bash
#-------------------------------------------------------------------
# test script for "invalid degenerate backend request" issue.
# ([pgpool-hackers: 4127] invalid degenerate backend request, node id : 2 status: [2] is not valid for failover)

echo "starting test"
date

source $TESTLIBS
TESTDIR=testdir
PSQL=$PGBIN/psql
PG_CTL=$PGBIN/pg_ctl
PCP_PROMOTE_NODE=$PGPOOL_INSTALL_DIR/bin/pcp_promote_node
PCP_NODE_INFO=$PGPOOL_INSTALL_DIR/bin/pcp_node_info

rm -fr $TESTDIR
mkdir $TESTDIR
cd $TESTDIR

# create test environment
echo -n "creating test environment..."
$PGPOOL_SETUP -n 3 || exit 1
echo "done."

# set node 1 down state
sed -i '2s/up/down/' log/pgpool_status

# remove "-D" option from startall script
sed -i 's/pgpool -D/pgpool/' startall

source ./bashrc.ports

export PGPORT=$PGPOOL_PORT

./startall
wait_for_pgpool_startup

echo === make sure that health check process for node 1 does not exist ===
ps x|grep "health check"

$PCP_NODE_INFO -w -p $PCP_PORT -a

echo === promote node 2 as a primary node ===
$PCP_PROMOTE_NODE -w -p $PCP_PORT --switchover 2
wait_for_pgpool_startup

date

# wait until node 1 up
ok=ng
u=2
for i in {1..30}
do
    $PCP_NODE_INFO -w -p $PCP_PORT 1|grep waiting
    if [ $? -eq 0 ];then
	ok=ok
	break;
    fi
    echo -n "`expr $u \* $i` "
    date
    sleep $u
done

if [ $ok != "ok" ];then
    echo === pcp_promote_node did not complete ===
    ./shutdownall
    exit 1
fi

echo === make sure that health check process for node 1 does exist ===
ps x|grep "health check"

echo === shutdown node 1 ===
$PG_CTL -D data1 stop
# we cannot use wait_for_pgpool_startup here because we want to detect the node 1 down by health check.
#wait_for_pgpool_startup
echo sleep 5
sleep 5
date

echo === make sure that node 1 is now down ===
ok=ng
u=2
for i in {1..15}
do
    $PCP_NODE_INFO -w -p $PCP_PORT 1|grep "down down"
    if [ $? -eq 0 ];then
	ok=ok
	break;
    fi
    echo `expr $u \* $i`
    sleep 2
done

if [ $ok != "ok" ];then
    echo === failover did not complete ===
    ./shutdownall
    exit 1
fi

./shutdownall
exit 0
