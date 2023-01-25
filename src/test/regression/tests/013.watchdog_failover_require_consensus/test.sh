#!/usr/bin/env bash
#-------------------------------------------------------------------
# test script for watchdog
#
# Please note that to successfully run the test, "HEALTHCHECK_DEBUG"
# must be defined before compiling main/health_check.c.
#
# test failover_require_consensus
#
source $TESTLIBS
LEADER_DIR=leader
STANDBY_DIR=standby
STANDBY2_DIR=standby2
num_tests=7
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


# copy the configurations from leader to standby
cp -r etc ../$STANDBY_DIR/

# copy the configurations from leader to standby2
cp -r etc ../$STANDBY2_DIR/

source ./bashrc.ports
cat ../leader.conf >> etc/pgpool.conf
echo 0 > etc/pgpool_node_id

./startall
wait_for_pgpool_startup


# back to test root dir
cd ..


# create standby environment and start pgpool

mkdir $STANDBY_DIR/log
echo -n "creating standby pgpool..."
cat standby.conf >> $STANDBY_DIR/etc/pgpool.conf
# since we are using the same pgpool-II conf as of leader. so change the pid file path in standby pgpool conf
echo "pid_file_name = '$PWD/pgpool2.pid'" >> $STANDBY_DIR/etc/pgpool.conf
echo "logdir = $STANDBY_DIR/log" >> $STANDBY_DIR/etc/pgpool.conf
echo 1 > $STANDBY_DIR/etc/pgpool_node_id
# start the standby pgpool-II by hand
$PGPOOL_INSTALL_DIR/bin/pgpool -D -n -f $STANDBY_DIR/etc/pgpool.conf -F $STANDBY_DIR/etc/pcp.conf -a $STANDBY_DIR/etc/pool_hba.conf > $STANDBY_DIR/log/pgpool.log 2>&1 &

# create standby2 environment but do not start pgpool
mkdir $STANDBY2_DIR/log
echo -n "creating standby2 pgpool..."
cat standby2.conf >> $STANDBY2_DIR/etc/pgpool.conf
# since we are using the same pgpool-II conf as of leader. so change the pid file path in standby pgpool conf
echo "pid_file_name = '$PWD/pgpool3.pid'" >> $STANDBY2_DIR/etc/pgpool.conf
echo "logdir = $STANDBY2_DIR/log" >> $STANDBY2_DIR/etc/pgpool.conf
echo 2 > $STANDBY2_DIR/etc/pgpool_node_id
# start the standby pgpool-II by hand
$PGPOOL_INSTALL_DIR/bin/pgpool -D -n -f $STANDBY2_DIR/etc/pgpool.conf -F $STANDBY2_DIR/etc/pcp.conf -a $STANDBY2_DIR/etc/pool_hba.conf > $STANDBY2_DIR/log/pgpool.log 2>&1 &

# First test check if both pgpool-II have found their correct place in watchdog cluster.
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

# now check if standby1 has successfully joined connected to the leader.
echo "Waiting for the standby1 to join cluster..."
for i in 1 2 3 4 5 6 7 8 9 10
do
	grep "successfully joined the watchdog cluster as standby node" $STANDBY_DIR/log/pgpool.log > /dev/null 2>&1
	if [ $? = 0 ];then
		success_count=$(( success_count + 1 ))
		echo "Standby successfully connected."
		break;
	fi
	echo "[check] $i times"
	sleep 2
done

# now check if standby2 has successfully joined connected to the leader.
echo "Waiting for the standby2 to join cluster..."
for i in 1 2 3 4 5 6 7 8 9 10
do
	grep "successfully joined the watchdog cluster as standby node" $STANDBY2_DIR/log/pgpool.log > /dev/null 2>&1
	if [ $? = 0 ];then
		success_count=$(( success_count + 1 ))
		echo "Standby2 successfully connected."
		break;
	fi
	echo "[check] $i times"
	sleep 2
done

# raise an artificial communication error on standby for DB node 1
echo "1	down" > $STANDBY_DIR/log/backend_down_request
echo "Checking if the standby successfully process the failover request..."
for i in 1 2 3 4 5 6 7 8 9 10
do
	grep -i "building consensus for request" $STANDBY_DIR/log/pgpool.log
	if [ $? = 0 ];then
		success_count=$(( success_count + 1 ))
		echo "Fake DB error generated and leader is waiting for consensus"
		break;
	fi
	echo "[check] $i times"
	sleep 2
done
echo "Checking if the Leader receives the failover request and waiting for consensus..."
for i in 1 2 3 4 5 6 7 8 9 10
do
	grep -i "failover request noted" $LEADER_DIR/log/pgpool.log
	if [ $? = 0 ];then
		success_count=$(( success_count + 1 ))
		echo "Fake DB error delivered to leader. and leader is waiting for consensus"
		break;
	fi
	echo "[check] $i times"
	sleep 2
done

# raise an artificial communication error again on the standby for DB node 1
echo "1	down" > $STANDBY_DIR/log/backend_down_request
# standby should quarantine the node now
echo "Checking if the node is quarantined on standby..."
for i in 1 2 3 4 5 6 7 8 9 10
do
	grep -i "changed to quarantine node request" $STANDBY_DIR/log/pgpool.log
	if [ $? = 0 ];then
		success_count=$(( success_count + 1 ))
		echo "Node is quarantined because of second error but no consensus"
		break;
	fi
	echo "[check] $i times"
	sleep 2
done


# raise an artificial communication on second standby to check if failover is executed this time
echo "1	down" > $STANDBY2_DIR/log/backend_down_request

#give some time to pgpool-II to execute failover
sleep 5
# check to see if all Pgpool-II agrees that the failover request is
# executed
echo "Checking if all Pgpool-II agrees that the failover request is executed"
for i in 1 2 3 4 5 6 7 8 9 10
do
    n=0
    for p in 11000 11100 11200
    do
	$PSQL -p $p -c "show pool_nodes" test|grep standby|grep down >/dev/null 2>&1
	if [ $? = 0 ];then
	    n=$(( n + 1))
	fi
    done
    if [ $n -eq 3 ];then
	success_count=$(( success_count + 1 ))
	echo "All Pgpool-II agrees that the failover request is executed"
	break;
    fi
    echo "[check] $i times"
    sleep 2
done


# we are done. Just stop the standby pgpool-II
$PGPOOL_INSTALL_DIR/bin/pgpool -f $STANDBY_DIR/etc/pgpool.conf -m f stop
$PGPOOL_INSTALL_DIR/bin/pgpool -f $STANDBY2_DIR/etc/pgpool.conf -m f stop
cd leader
./shutdownall

echo "$success_count out of $num_tests successful";

if test $success_count -eq $num_tests
then
    exit 0
fi

exit 1
