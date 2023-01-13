#!/usr/bin/env bash
#-------------------------------------------------------------------
# test script for load balancing.
#
source $TESTLIBS
TESTDIR=testdir
PSQL=$PGBIN/psql

# sleep time after reload in seconds
st=10

for mode in s r i
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
	echo "write_function_list = 'f1,public.f2'" >> etc/pgpool.conf

	./startall

	export PGPORT=$PGPOOL_PORT

	wait_for_pgpool_startup

	$PSQL test <<EOF
CREATE TABLE t1(i INTEGER);
CREATE TABLE t2(i INTEGER);
CREATE FUNCTION f1(INTEGER) returns INTEGER AS 'SELECT \$1' LANGUAGE SQL;
CREATE FUNCTION f2(INTEGER) returns INTEGER AS 'SELECT \$1' LANGUAGE SQL;
SELECT * FROM t1;		-- this load balances
SELECT f1(1);			-- this does not load balance
SELECT public.f2(1);	-- this does not load balance
EOF

# check if simple load balance worked
	fgrep "SELECT * FROM t1;" log/pgpool.log |grep "DB node id: 1">/dev/null 2>&1
	if [ $? != 0 ];then
	# expected result not found
		echo fail: select is sent to zero-weight node.
		./shutdownall
		exit 1
	fi
	echo ok: load balance works.

# check if write function list worked
	fgrep "SELECT f1(1);" log/pgpool.log |grep "DB node id: 0">/dev/null 2>&1
	if [ $? != 0 ];then
	# expected result not found
		echo fail: write function is sent to node 1.
		./shutdownall
		exit 1
	fi
	fgrep "SELECT public.f2(1);" log/pgpool.log |grep "DB node id: 0">/dev/null 2>&1
	if [ $? != 0 ];then
	# expected result not found
		echo fail: write function is sent to node 1.
		./shutdownall
		exit 1
	fi
	echo ok: write function list works.

	echo "read_only_function_list = 'f1,public.f2'" >> etc/pgpool.conf
	echo "write_function_list = ''" >> etc/pgpool.conf

	./pgpool_reload
	sleep $st

	$PSQL test <<EOF
SELECT f1(1);			-- this does load balance
SELECT public.f2(1);	-- this does load balance
EOF

# check if read_only function list worked
	fgrep "SELECT f1(1);" log/pgpool.log |grep "DB node id: 1">/dev/null 2>&1
	if [ $? != 0 ];then
	# expected result not found
		echo fail: read_only function is sent to zero-weight node.
		./shutdownall
		exit 1
	fi
	fgrep "SELECT public.f2(1);" log/pgpool.log |grep "DB node id: 1">/dev/null 2>&1
	if [ $? != 0 ];then
	# expected result not found
		echo fail: read_only function is sent to zero-weight node.
		./shutdownall
		exit 1
	fi
	echo ok: read_only function list works.

# check if primary routing query pattern list worked
	./shutdownall
	echo "primary_routing_query_pattern_list = 'SELECT \'a\'\;;SELECT 1\;;SELECT \'\;\'\;;SELECT \* FROM t1\;;^.*t2.*\;$;^.*f1.*$'" >> etc/pgpool.conf
	./startall
	wait_for_pgpool_startup

queries=`cat << EOF
SELECT * FROM t1;
SELECT 'a';
SELECT 1;
SELECT ';';
SELECT * FROM t2;
SELECT f1(1);
EOF
`
	echo "$queries" | while read query; do
		$PSQL test -c "$query"

		# If native replication mode, all queries are sent to primary node only.
		# If query match both primary_routing_query_pattern_list and read_only_function_list,
		# read_only_function_list will be ignored, and query is sent to primary node only.
		#
		# If replication node, all queries are load-balanced.
		if [[ $mode = "s" ]];then
			node_id=0
		else
			node_id=1
		fi
		fgrep "${query}" log/pgpool.log | grep "DB node id: "`echo $node_id` > /dev/null 2>&1

		if [ $? != 0 ];then
			# expected result not found
			echo "fail: primary routing query: ${query} is load-balanced."
			./shutdownall
			exit 1
		fi
	done

	if [ $? -eq 1 ]; then
		exit 1
	fi
	echo ok: primary routing query pattern list works.

	# check if statement level load balance worked
	./shutdownall
	echo "read_only_function_list = ''" >> etc/pgpool.conf
	echo "write_function_list = ''" >> etc/pgpool.conf
	echo "statement_level_load_balance = on" >> etc/pgpool.conf
	echo "log_min_messages = debug1" >> etc/pgpool.conf

	./startall
	sleep $st

	$PSQL test <<EOF
SELECT 3333;
SELECT 4444;
EOF

	n=`grep "selecting load balance node" log/pgpool.log | wc -l`
	if [ $n != 3 ]; then
	# expected result not found
		echo "fail: statement level load balance doesn't work."
		./shutdownall
		exit 1
	fi
	echo ok: statement level load balance works.

# in replication mode if load_balance_mode = off, SELECT query inside
# an explicit transaction should be sent to main node only.
	if [ $mode = "r" ];then
		./shutdownall
		echo "load_balance_mode = off" >> etc/pgpool.conf
		./startall
		wait_for_pgpool_startup

		$PSQL test <<EOF
BEGIN;
SELECT 1;
END;
EOF

		ok=0
		fgrep "SELECT 1;" log/pgpool.log |grep "DB node id: 0">/dev/null 2>&1
		if [ $? = 0 ];then
			fgrep "SELECT 1;" log/pgpool.log |grep "DB node id: 1">/dev/null 2>&1		
			if [ $? != 0 ];then
			# the SELECT should not be executed on node 1
				echo ok: select is sent to only main node when not load-balanced.
				ok=1
			fi
		# the SELECT should be executed on node 0
		fi

# in replication mode if load_balance_mode = off, SELECT query
# including writing function should be sent to all the nodes.
# per [pgpool-general: 2221].
		echo "write_function_list = 'f1'" >> etc/pgpool.conf
		echo "read_only_function_list = ''" >> etc/pgpool.conf
		./pgpool_reload
		sleep $st
		$PSQL test <<EOF
SELECT f1(2);		-- this should be sent to all the nodes
EOF
		fgrep "SELECT f1(2);" log/pgpool.log |grep "DB node id: 0">/dev/null 2>&1
		if [ $? = 0 ];then
			fgrep "SELECT f1(2);" log/pgpool.log |grep "DB node id: 1">/dev/null 2>&1		
			if [ $? = 0 ];then
			# the SELECT should be executed on node 0 & 1
				echo ok: write function is sent to all nodes.
				ok=`expr $ok + 1`
			fi
		# the SELECT should be executed on node 0
		fi

		if [ $ok != 2 ];then
		    exit 1;
		fi
	fi

# -------------------------------------------------------------------------------
# check the case when write_function_list and read_only_function_list are both empty.
# In this case pg_proc.provolatile is checked. If it is 'v' (volatile), then the
# function is regarded doing writes.
# -------------------------------------------------------------------------------
	echo "write_function_list = ''" >> etc/pgpool.conf
	echo "read_only_function_list = ''" >> etc/pgpool.conf
	./pgpool_reload
	sleep $st

	$PSQL test <<EOF
SELECT f1(1);
SELECT public.f2(1);
EOF

	fgrep "SELECT f1(1);" log/pgpool.log |grep "DB node id: 0">/dev/null 2>&1
	if [ $? != 0 ];then
	# expected result not found
		echo fail: volatile function is sent to node 1.
		./shutdownall
		exit 1
	fi
	fgrep "SELECT public.f2(1);" log/pgpool.log |grep "DB node id: 0">/dev/null 2>&1
	if [ $? != 0 ];then
	# expected result not found
		echo fail: volatile function is sent to node 1.
		./shutdownall
		exit 1
	fi
	echo ok: volatile function check works.

	./shutdownall

	cd ..

done

exit 0
