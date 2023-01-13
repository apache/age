#!/usr/bin/env bash
#-------------------------------------------------------------------
# test script for trusted_server_command
#
source $TESTLIBS
TESTDIR=testdir
PSQL=$PGBIN/psql

# set trusted_servers and trusted_server_command
function set_param
{
    n=0
    while [ $n -lt 2 ]
    do
	echo "trusted_servers = 'localhost'" >> pgpool$n/etc/pgpool.conf
	echo "trusted_server_command = 'ping -q -c3 %h'" >> pgpool$n/etc/pgpool.conf
	n=`expr $n + 1`
    done
}

# wait for watchdog starting up by looking for "lifecheck started" in
# the pgpool.log.  argument: $log: absolute path to the pgpool.log.
function wait_for_watchdog_startup
{
    while :
    do
	grep "lifecheck started" $log >/dev/null
	if [ $? = 0 ];then
	    break;
	fi
	sleep 1
    done
}


#-------------------------------------------------------------------
dir=`pwd`

failed=false
export PCPPASSFILE=$dir/$TESTDIR/pgpool0/pcppass

cd $dir
rm -fr $TESTDIR
mkdir $TESTDIR
cd $TESTDIR

$WATCHDOG_SETUP -wn 2 || exit 1

set_param

./startall

cd pgpool0
source ./bashrc.ports
export PGPORT=$PGPOOL_PORT
cd ..

echo -n "waiting for watchdog node 0 starting up... "
log=$dir/$TESTDIR/pgpool0/log/pgpool.log
wait_for_watchdog_startup $log
echo "done."

sleep 10

grep "watchdog failed to ping host:" $log >/dev/null
if [ $? -eq 0 ];then
	failed=true
fi

./shutdownall

if [ $failed = "true" ];then
    exit 1
fi
exit 0
