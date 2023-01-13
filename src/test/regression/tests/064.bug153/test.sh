#!/usr/bin/env bash
#-------------------------------------------------------------------
# Test script for bug reported in bug#153
# CTE queries including DMLs are incorrectly handled.

source $TESTLIBS
TESTDIR=testdir
PSQL=$PGBIN/psql

for mode in s r
do
	rm -fr $TESTDIR
	mkdir $TESTDIR
	cd $TESTDIR

# create test environment
	echo -n "creating test environment..."
	$PGPOOL_SETUP -m $mode -n 2 || exit 1
	echo "done."

	source ./bashrc.ports

	echo "backend_weight0 = 0" >> etc/pgpool.conf
	echo "backend_weight1 = 1" >> etc/pgpool.conf

	./startall

	export PGPORT=$PGPOOL_PORT

	wait_for_pgpool_startup

	$PSQL test <<EOF
CREATE TABLE foo(x serial);
-- Below should be executed on the primary node in streaming replication mode.
-- Below should be executed on both nodes in native replication mode.
WITH r AS (INSERT INTO foo VALUES (default) RETURNING x) SELECT x FROM r;

-- Below should be executed on the primary node in streaming replication mode.
-- Below should be executed on both nodes in native replication mode.
WITH t AS (SELECT nextval('foo_x_seq')) SELECT * FROM t;

-- Below should be executed on node 1.
WITH s AS (SELECT 1) SELECT * FROM s;
EOF

# check if read query is load balanced.
	fgrep "SELECT 1" log/pgpool.log |grep "DB node id: 1">/dev/null 2>&1
	if [ $? != 0 ];then
	# expected result not found
		echo 'fail: WITH s AS (SELECT 1) SELECT * FROM s;'
		./shutdownall
		exit 1
	fi

	if [ $mode = "r" ];then
	    fgrep "SELECT 1" log/pgpool.log |grep "DB node id: 0">/dev/null 2>&1
	    if [ $? = 0 ];then
		# expected result not found
		echo 'fail: WITH s AS (SELECT 1) SELECT * FROM s;'
		./shutdownall
		exit 1
	    fi
	fi

	echo ok: read query load balance works.

# check if data modifying WITH worked
	fgrep "WITH r AS" log/pgpool.log |grep "DB node id: 0">/dev/null 2>&1
	if [ $? != 0 ];then
	# expected result not found
		echo 'fail: data modifying WITH.'
		./shutdownall
		exit 1
	fi

	if [ $mode = "r" ];then
	    fgrep "WITH r AS" log/pgpool.log |grep "DB node id: 1">/dev/null 2>&1
	    if [ $? != 0 ];then
		# expected result not found
		echo 'fail: data modifying WITH in replication mode.'
		./shutdownall
		exit 1
	    fi
	fi

# check if WITH SELECT including data modifying function call worked.
	fgrep "WITH t AS" log/pgpool.log |grep "DB node id: 0">/dev/null 2>&1
	if [ $? != 0 ];then
	# expected result not found
		echo 'fail: WITH SELECT including data modifying function call.'
		./shutdownall
		exit 1
	fi

	if [ $mode = "r" ];then
	    fgrep "WITH t AS" log/pgpool.log |grep "DB node id: 1">/dev/null 2>&1
	    if [ $? != 0 ];then
		# expected result not found
		echo 'fail: WITH SELECT including data modifying function call.'
		./shutdownall
		exit 1
	    fi
	fi
	./shutdownall

	cd ..
done

exit 0
