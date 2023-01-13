#!/usr/bin/env bash
#-------------------------------------------------------------------
# test script for load balancing.
#
source $TESTLIBS
TESTDIR=testdir
PSQL=$PGBIN/psql
# sleep time after reload in seconds
st=10

for mode in s
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
	echo "disable_load_balance_on_write = 'dml_adaptive'" >> etc/pgpool.conf
	echo "write_function_list = 'currval,lastval,nextval,setval,f1'" >> etc/pgpool.conf
	echo "dml_adaptive_object_relationship_list= 't1:t2,f1():tF'" >> etc/pgpool.conf

	./startall

	export PGPORT=$PGPOOL_PORT

	wait_for_pgpool_startup

	$PSQL test <<EOF
CREATE TABLE t1(i INTEGER);
CREATE TABLE t2(i INTEGER);
CREATE TABLE t3(i INTEGER);
CREATE TABLE tF(i INTEGER);
CREATE FUNCTION f1(INTEGER) returns INTEGER AS 'SELECT \$1' LANGUAGE SQL;
EOF

$PSQL test <<EOF

BEGIN;
SELECT i, 'QUERY ID T1-1' FROM t2; 		-- LOAD balance
INSERT INTO t1 values(1);			-- NO LOAD balance
SELECT i, 'QUERY ID T1-2' FROM t1;		-- NO LOAD balance
SELECT i, 'QUERY ID T1-3' FROM t3;		-- LOAD balance
SELECT i, 'QUERY ID T1-4' FROM t2;		-- NO LOAD balance
COMMIT;
EOF

# check if simple load balance worked
	fgrep "SELECT i, 'QUERY ID T1-1' FROM t2;" log/pgpool.log |grep "DB node id: 1">/dev/null 2>&1
	if [ $? != 0 ];then
	# expected result not found
		echo fail: "SELECT i, QUERY ID T1-1 FROM t2" was not load balanced.
		./shutdownall
		exit 1
	fi
	echo ok: first read was load balanced.

# check if simple load balance worked
	fgrep "SELECT i, 'QUERY ID T1-2' FROM t1;" log/pgpool.log |grep "DB node id: 0">/dev/null 2>&1
	if [ $? != 0 ];then
	# expected result not found
		echo fail: "SELECT i, QUERY ID T1-2 FROM t1;" was wrongly load balanced.
		./shutdownall
		exit 1
	fi
	echo ok: adaptive load balance test 1 works.

# check if simple load balance worked
	fgrep "SELECT i, 'QUERY ID T1-3' FROM t3;" log/pgpool.log |grep "DB node id: 1">/dev/null 2>&1
	if [ $? != 0 ];then
	# expected result not found
		echo fail: "SELECT i, 'QUERY ID T1-3' FROM t3" was not load balanced.
		./shutdownall
		exit 1
	fi
	echo ok: unrelated read was load balanced.

# check if simple load balance worked
	fgrep "SELECT i, 'QUERY ID T1-4' FROM t2;" log/pgpool.log |grep "DB node id: 0">/dev/null 2>&1
	if [ $? != 0 ];then
	# expected result not found
		echo fail: "SELECT i, 'QUERY ID T1-4' FROM t2;" was wrongly load balanced.
		./shutdownall
		exit 1
	fi
	echo ok: adaptive load balance test 2 works.

$PSQL test <<EOF
BEGIN;
SELECT i, 'QUERY ID T2-5' FROM tF; 			-- LOAD balance
SELECT f1(190), 'QUERY ID T2-6';			-- NO LOAD balance
SELECT i, 'QUERY ID T2-7' FROM tF; 			-- NO LOAD balance
SELECT i, 'QUERY ID T2-8' FROM t1;			-- LOAD balance
SELECT i, 'QUERY ID T2-9' FROM t3;			-- LOAD balance
COMMIT;
EOF

# check if simple load balance worked
	fgrep "SELECT i, 'QUERY ID T2-5' FROM tF;" log/pgpool.log |grep "DB node id: 1">/dev/null 2>&1
	if [ $? != 0 ];then
	# expected result not found
		echo fail: "SELECT i, 'QUERY ID T2-5' FROM tF;" was not load balanced.
		./shutdownall
		exit 1
	fi
	echo ok: T2 first read was load balanced.

# check if simple load balance worked
	fgrep "SELECT i, 'QUERY ID T2-7' FROM tF;" log/pgpool.log |grep "DB node id: 0">/dev/null 2>&1
	if [ $? != 0 ];then
	# expected result not found
		echo fail: "SELECT i, 'QUERY ID T2-7' FROM tF;" was wrongly load balanced.
		./shutdownall
		exit 1
	fi
	echo ok: adaptive load balance test 3 works.

# check if simple load balance worked
	fgrep "SELECT i, 'QUERY ID T2-8' FROM t1;" log/pgpool.log |grep "DB node id: 1">/dev/null 2>&1
	if [ $? != 0 ];then
	# expected result not found
		echo fail: "SELECT i, 'QUERY ID T2-8' FROM t1;" was not load balanced.
		./shutdownall
		exit 1
	fi
	echo ok: T2 third read was load balanced.

# check if simple load balance worked
	fgrep "SELECT i, 'QUERY ID T2-9' FROM t3;" log/pgpool.log |grep "DB node id: 1">/dev/null 2>&1
	if [ $? != 0 ];then
	# expected result not found
		echo fail: "SELECT i, 'QUERY ID T2-9' FROM t3;" was not load balanced.
		./shutdownall
		exit 1
	fi
	echo ok: T2 final read was load balanced.

        ./shutdownall
        cd ..
done
exit 0
