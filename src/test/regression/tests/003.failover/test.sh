#!/usr/bin/env bash
#-------------------------------------------------------------------
# test script for failover
#
source $TESTLIBS
TESTDIR=testdir
PG_CTL=$PGBIN/pg_ctl
PSQL="$PGBIN/psql -X "

for mode in s r
do
	rm -fr $TESTDIR
	mkdir $TESTDIR
	cd $TESTDIR

# create test environment
	echo -n "creating test environment..."
	$PGPOOL_SETUP -m $mode -n 2|| exit 1
	echo "done."

	# disable replication delay check so that comparison between
	# expected and actual result is not confused.
	echo "sr_check_period = 0" >> etc/pgpool.conf
	./startall
	wait_for_pgpool_startup

	source ./bashrc.ports

	export PGPORT=$PGPOOL_PORT

	$PSQL -c "show pool_nodes" test

	# trigger failover on node 1
	$PG_CTL -D data1 -m f stop
	wait_for_pgpool_startup
	$PSQL -c "show pool_nodes" test |sed -e 's/true /false/' -e 's/....-..-.. ..:..:../XXXX-XX-XX XX:XX:XX/g'|
	    sed -e 's/streaming/         /' | sed -e 's/async/     /'> result

	# check the output of "show pool_nodes".
	LANG=C $PSQL -f ../create_expected.sql -v mode="'$mode'" test | tail -n 6 > expected
	cmp result expected > /dev/null 2>&1
	if [ $? != 0 ];then
		./shutdownall
		exit 1
	fi
		
	./shutdownall

	./startall
	wait_for_pgpool_startup
	# trigger failover on node 0
	$PG_CTL -D data0 -m f stop
	wait_for_failover_done
	$PSQL -c "show pool_nodes" test |sed -e 's/true /false/' -e 's/....-..-.. ..:..:../XXXX-XX-XX XX:XX:XX/g'> result
	if [ ! -s result ];then
		./shutdownall
		echo "result file is empty."
		exit 1
	fi

	# check the output of "show pool_nodes".
	LANG=C $PSQL -f ../create_expected_node0.sql -v mode="'$mode'" test | tail -n 6 > expected
	cmp result expected > /dev/null 2>&1
	if [ $? != 0 ];then
		./shutdownall
		exit 1
	fi

	./shutdownall

	cd ..
done

exit 0
