#!/usr/bin/env bash
#-------------------------------------------------------------------
# test script for the case when node 0 is not primary.
#
source $TESTLIBS
TESTDIR=testdir
PSQL=$PGBIN/psql
PG_CTL=$PGBIN/pg_ctl
export PGDATABASE=test

for mode in s
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

	./startall

	export PGPORT=$PGPOOL_PORT

	wait_for_pgpool_startup

	echo "Swapping the role of node 0 and node 1..."
	$PG_CTL -D data0 -m f stop
	wait_for_pgpool_startup
	$PGPOOL_INSTALL_DIR/bin/pcp_recovery_node -w -h localhost -p $PCP_PORT 0
	wait_for_pgpool_startup
	echo "Swapping the role of node 0 and node 1...done."
	$PSQL -c "show pool_nodes"

	# make sure that node 1 is main/primary
	test `$PSQL -c "show pool_nodes"|egrep "primary|main"|awk '{print $1}'` = 1 &&
	    test `$PSQL -c "show pool_nodes"|egrep "primary|main"|awk '{print $7}'` = "up"
	if [ $? != 0 ]
	then
	    echo "node 1 is not primary or main"
	    ./shutdownall
	    exit 1
	fi

	# try to issue write query
	$PSQL -c "create table t1(i int)"
	if [ $? != 0 ]
	then
	    echo "write query failed"
	    ./shutdownall
	    exit 1
	fi

	./shutdownall
	cd ..
done

exit 0
