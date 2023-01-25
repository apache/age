#!/usr/bin/env bash
#-------------------------------------------------------------------
# test script for auto failback
#
source $TESTLIBS
TESTDIR=testdir
PG_CTL=$PGBIN/pg_ctl
PSQL="$PGBIN/psql"

rm -fr $TESTDIR
mkdir $TESTDIR
cd $TESTDIR

# create test environment
echo -n "creating test environment..."
$PGPOOL_SETUP -m s -n 2 || exit 1
echo "done."

echo "auto_failback = on" >> etc/pgpool.conf
echo "auto_failback_interval = 60" >> etc/pgpool.conf
echo "sr_check_period = 2" >> etc/pgpool.conf
echo "health_check_period = 2" >> etc/pgpool.conf

source ./bashrc.ports

export PGPORT=$PGPOOL_PORT


./startall
wait_for_pgpool_startup

$PSQL -c "show pool_nodes" test |grep down
if [ $? = 0 ];then
	./shutdownall
	exit 1
fi

# trigger failover on node 1
$PG_CTL -D data1 -w -m f stop
wait_for_pgpool_startup
echo "backend node 1 is stopped"

$PSQL -c "show pool_nodes" test |grep down
if [ $? != 0 ];then
	./shutdownall
	exit 1
fi

# node 1 restart
$PG_CTL -D data1 -w -m f start
echo "backend node 1 is restarted"
sleep 10;

# check auto_failback is success
$PSQL -c "show pool_nodes" test |grep down
if [ $? = 0 ];then
	./shutdownall
	exit 1
fi
echo "auto_failback is success"

# check auto_failback_interval

echo `date` ":test auto_failback_interval"
# trigger failover on node 1
$PG_CTL -D data1 -w -m f stop > /dev/null
wait_for_pgpool_startup
echo `date` ":backend node 1 is stopped"
$PSQL -c "show pool_nodes" test |grep down
if [ $? != 0 ];then
	./shutdownall
	exit 1
fi
$PG_CTL -D data1 -w -m f start > /dev/null
echo `date` ":backend node 1 is restarted"

echo `date` ":sleep 20[sec]"
sleep 20;
$PSQL -c "show pool_nodes" test |grep down
if [ $? != 0 ];then
	./shutdownall
	exit 1
fi
echo `date` ":backend node status is still detached"

echo `date` ":sleep 40[sec]"
sleep 40;
$PSQL -c "show pool_nodes" test |grep down
if [ $? != 0 ];then
	./shutdownall
	exit 1
fi
echo `date` ":the time of auto_failback_intervall passed and auto_failback is success."


./shutdownall

exit 0
