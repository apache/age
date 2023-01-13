#!/usr/bin/env bash
#-------------------------------------------------------------------
# test script for the case when node 0 is down (and DISALLOW_FAILOVER is being set).
#
source $TESTLIBS
TESTDIR=testdir
PSQL=$PGBIN/psql
PG_CTL=$PGBIN/pg_ctl
export PGDATABASE=test

for mode in s r n
do
	rm -fr $TESTDIR
	mkdir $TESTDIR
	cd $TESTDIR

	echo "===== testing mode: $mode ======"

	# create test environment
	echo -n "creating test environment..."
	$PGPOOL_SETUP -m $mode -n 2 || exit 1
	echo "done."

	source ./bashrc.ports

	#
	# Test case 1: when pgpool starts node 0 is already down and
	# pool_status recorded the fact.
	# Only streaming replication mode failed with read-only transaction.
	# Other mode should succeed.
	#
	# edit startall so that it does not use "-D" with pgpool.
	sed -i 's/pgpool -D/pgpool/' startall

	# set down status for node 0.
	echo -e "down \nup" > log/pgpool_status
	
	./startall

	export PGPORT=$PGPOOL_PORT

	wait_for_pgpool_startup

	$PSQL -c "show pool_nodes" postgres
	# try to issue write query
	$PSQL -c "create table t1(i int)" postgres  > psql_status 2>&1
	if [ $? != 0 ]
	then
	    if [ $mode = "s" ]
	    then
		grep -i read-only psql_status >/dev/null
		if [ $? != 0 ]
		then
		    echo -n "Writing query in streamin replication mode failed due to other than read-only transaction in test case 1. reason: "
		    cat psql_status
		    ./shutdownall
		    exit 1
		fi
	    else
		echo -n "Writing query in mode $mode failed in test case 1. reason: "
		cat psql_status
		./shutdownall
		exit 1
	    fi
	else
	    $PSQL -c "drop table t1" postgres
	fi
	echo "Writing query in mode $mode succeeded in test case 1"

	./shutdownall

	#
	# Test case 2: server starts then node 0 is shutting down.
	# In all mode this test should succeed.
	#
	# remove pgpool_status to trigger failover.
	rm log/pgpool_status
	# set down request file
	./startall
	wait_for_pgpool_startup
	# shutdown node 0
	$PG_CTL -D data0 -m f stop
	wait_for_pgpool_startup
	$PSQL -c "show pool_nodes" postgres
	# try to issue write query
	$PSQL -c "create table t1(i int)" postgres > psql_status 2>&1
	if [ $? != 0 ]
	then
	    echo -n "Writing query in mode $mode failed in test case 2. reason: "
	    cat psql_status
	else
	    echo "Writing query in mode $mode succeeded in test case 2"
	fi

	./shutdownall

	$PSQL -c "drop table t1" postgres

	#
	# Test case 3: in addition 2 set DISALLOW_TO_FAILOVER flag to node 0.
	#

	# Set DISALLOW_TO_FAILOVER flag to node 0
	echo "backend_flag0 = DISALLOW_TO_FAILOVER" >> etc/pgpool.conf
	# remove pgpool_status to trigger failover.
	rm log/pgpool_status
	# set down request file
	./startall
	wait_for_pgpool_startup
	# shutdown node 0
	$PG_CTL -D data0 -m f stop
	wait_for_pgpool_startup
	$PSQL -c "show pool_nodes" postgres
	# try to issue write query expecting error: executing failover on backend.
	$PSQL -c "create table t1(i int)" postgres > psql_status 2>&1
	if [ $? != 0 ]
	then
	    grep -i "executing failover on backend" psql_status >/dev/null
	    if [ $? != 0 ]
	    then
		echo -n "Writing query in mode $mode failed due to other than \"executing failover on backend\" in test case 3. reason: "
		cat psql_status
		./shutdownall
		exit 1
	    else
		echo "Writing query in mode $mode failed by \"executing failover on backend\" as expected in test case 3"
	    fi
	else
	    echo "Writing query succeeded in test case 3 mode $mode. Suspicious access."
	    ./shutdownall
	    exit 1
	fi

	$PSQL -c "drop table t1" postgres

	./shutdownall

	cd ..
done

exit 0
