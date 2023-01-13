#!/usr/bin/env bash
#-------------------------------------------------------------------
# test script for pg_terminate_backend command.

WHOAMI=`whoami`
source $TESTLIBS
TESTDIR=testdir
PSQL=$PGBIN/psql

for mode in s r n
do
	rm -fr $TESTDIR
	mkdir $TESTDIR
	cd $TESTDIR

	# create test environment
	echo -n "creating test environment..."
	$PGPOOL_SETUP -m $mode -n 2 || exit 1
	echo "done."

	source ./bashrc.ports

	export PGPORT=$PGPOOL_PORT


	echo "log_min_messages = 'debug5'" >> etc/pgpool.conf
	# start pgpool-II
	./startall

	sleep 1

	$PSQL test -p $PGPORT -c "SELECT pg_sleep(10);" &

	sleep 2
	# get process id which query is executed
	PID=`ps -efw |grep "postgres:" |grep SELECT | awk 'NR==1 {print $2}'`

	$PSQL test -p $PGPORT -c "SELECT pg_terminate_backend($PID)"

	grep "found the pg_terminate_backend request" log/pgpool.log
	if [ $? -ne 0 ];then
		echo "pgpool cannot recognize a node which is executing pg_terminate_backend command."
		./shutdownall
		exit 1
	fi

	sleep 5

	grep "failover" log/pgpool.log
	if [ $? -eq 0 ];then
		echo "Failed pg_terminate_backend. failover is executing."
		./shutdownall
		exit 1
	fi

	sleep 10

	COUNT_UP=`grep "connection_count_up" log/pgpool.log |wc -l`
	COUNT_DOWN=`grep "connection_count_down" log/pgpool.log |wc -l`

	date
	echo "count_up: ${COUNT_UP}"
	echo "count_down: ${COUNT_DOWN}"
	if [ ${COUNT_UP} != ${COUNT_DOWN} ];then
		echo "connection count is not matched."
		./shutdownall
		exit 1
	fi

	./shutdownall

	cd ..
done

exit 0
