#!/usr/bin/env bash
#-------------------------------------------------------------------
# test script for testing the feature of detach_false_primary.
#
source $TESTLIBS
TESTDIR=testdir
PSQL=$PGBIN/psql
PG_CTL=$PGBIN/pg_ctl
export PGDATABASE=test

rm -fr $TESTDIR
mkdir $TESTDIR
cd $TESTDIR

version=`$PSQL --version|awk '{print $3}'`
result=`echo "$version >= 9.6"|bc`
if [ $result = 0 ];then
    echo "PostgreSQL version $version is 9.5 or before. Skipping test."
    exit 0
fi

# create test environment
echo -n "creating test environment..."
$PGPOOL_SETUP -m s -n 3 -s || exit 1
echo "done."

source ./bashrc.ports

echo "detach_false_primary=on" >> etc/pgpool.conf
echo "sr_check_period = 1" >> etc/pgpool.conf
./startall
export PGPORT=$PGPOOL_PORT
wait_for_pgpool_startup

# promote #3 node to create false primary
$PG_CTL -D data2 promote

sleep 10
wait_for_pgpool_startup
$PSQL -c "show pool_nodes" postgres > show_pool_nodes
cat show_pool_nodes
primary_node=`grep primary show_pool_nodes|grep -v standby|awk '{print $1}'`
if [ $primary_node != 0 ];then
    echo "primary node is not 0"
    ./shutdownall
    exit 1
fi

false_primary_node=`grep down show_pool_nodes|awk '{print $1}'`
if [ $false_primary_node != 2 ];then
    echo "false primary node is not 2"
    ./shutdownall
    exit 1
fi

./shutdownall

#
# test with watchdog enabled
#

# wipe out everything
cd ..
rm -fr $TESTDIR
mkdir $TESTDIR
cd $TESTDIR

# create 3 node pgpool with 3 backends.
$WATCHDOG_SETUP -wn 3 -n 3

# enable detach_false_primary
for i in 0 1 2
do
    echo "detach_false_primary = on" >> pgpool$i/etc/pgpool.conf
done

# start only pgpool0 and backend so that the quorum is absent.
cd pgpool0
source ./bashrc.ports
./startall
cd ..
export PGPORT=$PGPOOL_PORT
wait_for_pgpool_startup

# promote #3 node to create false primary
$PG_CTL -D pgpool0/data2 promote

wait_for_pgpool_startup

$PSQL -c "show pool_nodes" postgres|grep down
if [ $? = 0 ];then
    echo "node is down despite that the quorum is absent"
    ./shutdownall
    exit 1
fi

# start pgpool1 and pgpool2 so that the quorum exists.
echo "testing the case when the quorum exists"
cd pgpool1
./startall
cd ..
cd pgpool2
./startall
cd ..

wait_for_pgpool_startup

$PGPOOL_INSTALL_DIR/bin/pcp_watchdog_info -v -w -h localhost -p $PCP_PORT

date
for i in {1..30}
do
    cnt=`$PGPOOL_INSTALL_DIR/bin/pcp_watchdog_info -v -w -h localhost -p $PCP_PORT|egrep "MASTER|LEADER|STANDBY"|wc -l`
    if [ $cnt = 3 ];then
	echo "watchdog is ready"
	break
    fi
    sleep 1
done
date

ok=n
for i in {1..10}
do
    $PSQL -c "show pool_nodes" postgres
    $PSQL -c "show pool_nodes" postgres|grep down
    if [ $? = 0 ];then
	ok=y
	break
    fi
    sleep 1
done

if [ $ok != 'y' ];then
    echo "node is not down despite that the quorum exists"
    ./shutdownall
    exit 1
fi

./shutdownall

exit 0
