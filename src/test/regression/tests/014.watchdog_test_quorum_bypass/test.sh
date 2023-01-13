#!/usr/bin/env bash
#-------------------------------------------------------------------
# test script for watchdog
#
# Please note that to successfully run the test, "HEALTHCHECK_DEBUG"
# must be defined before compiling main/health_check.c.
#
# test pcp_detach bypass failover_when_quorum_exists and failover_require_consensus
#
source $TESTLIBS
LEADER_DIR=leader
num_tests=2
success_count=0
PSQL=$PGBIN/psql
PG_CTL=$PGBIN/pg_ctl

rm -fr $LEADER_DIR
rm -fr $STANDBY_DIR
rm -fr $STANDBY2_DIR

mkdir $LEADER_DIR
mkdir $STANDBY_DIR
mkdir $STANDBY2_DIR


# dir in leader directory
cd $LEADER_DIR

# create leader environment
echo -n "creating leader pgpool and PostgreSQL clusters..."
$PGPOOL_SETUP -m s -n 2 -p 11000|| exit 1
echo "leader setup done."


source ./bashrc.ports
cat ../leader.conf >> etc/pgpool.conf
echo 0 > etc/pgpool_node_id

./startall
wait_for_pgpool_startup


# back to test root dir
cd ..


# First test check if pgpool-II became a leader.
echo "Waiting for the pgpool leader..."
for i in 1 2 3 4 5 6 7 8 9 10
do
	grep "I am the cluster leader node" $LEADER_DIR/log/pgpool.log > /dev/null 2>&1
	if [ $? = 0 ];then
		success_count=$(( success_count + 1 ))
		echo "Leader brought up successfully."
		break;
	fi
	echo "[check] $i times"
	sleep 2
done

#export PCPPASSFILE=/home/usama/work/community/pgpool2/src/test/regression/tests/067.bug231/testdir/pcppass
$PGPOOL_INSTALL_DIR/bin/pcp_detach_node -w -h localhost -p $PCP_PORT 1 2>&1

wait_for_pgpool_startup

$PSQL -p 11000 -c "show pool_nodes" test|grep standby|grep down >/dev/null 2>&1
if [ $? = 0 ];then
    echo "Failover was successfully executed"
	success_count=$(( success_count + 1 ))
fi

cd leader
./shutdownall

echo "$success_count out of $num_tests successful";

if test $success_count -eq $num_tests
then
    exit 0
fi

exit 1
