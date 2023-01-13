#!/usr/bin/env bash
#-------------------------------------------------------------------
# Testing loadbalance failure using DEALLOCATE and EXECUTE command
# case with streaming replication mode.

WHOAMI=`whoami`
source $TESTLIBS
TESTDIR=testdir
PSQL=$PGBIN/psql
num_tests=6
success_count=0


#for mode in s r n
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

	export PGPORT=$PGPOOL_PORT

	# set backend_weight , loadbalance to standby only
	echo "backend_weight0 = 0" >> etc/pgpool.conf
	echo "backend_weight1 = 1" >> etc/pgpool.conf

	# start pgpool-II
	./startall

	sleep 1

	# run test1 select query
	$PSQL -p 11000 test <<EOF
PREPARE test1 AS SELECT 1;
EXECUTE test1;
DEALLOCATE test1;
EOF

	# run test2 update query
	$PSQL -p 11000 test <<EOF
CREATE TABLE test_tbl(id int, name text);
PREPARE test2 AS UPDATE test_tbl SET id =2;
EXECUTE test2;
DEALLOCATE test2;
DEALLOCATE all;
EOF

	expect1=`fgrep "PREPARE test1" log/pgpool.log  | awk '{print substr($0, index($0, "DB node id:"),13)}'`
	expect2=`fgrep "PREPARE test2" log/pgpool.log  | awk '{print substr($0, index($0, "DB node id:"),13)}'`

	#test1 result
	result=`fgrep "EXECUTE test1" log/pgpool.log  | awk '{print substr($0, index($0, "DB node id:"),13)}' `
	if [  "$expect1" = "$result" ]; then
        success_count=$(( success_count + 1 ))
        echo "PREPARE and EXECUTE with SELECT query successfully."
	fi
	result=`fgrep "DEALLOCATE test1" log/pgpool.log  | awk '{print substr($0, index($0, "DB node id:"),13)}' `
	if [  "$expect1" = "$result" ]; then
        success_count=$(( success_count + 1 ))
        echo "PREPARE and DEALLOCATE with SELECT query successfully."
	fi

	#test2 result
	result=`fgrep "EXECUTE test2" log/pgpool.log  | awk '{print substr($0, index($0, "DB node id:"),13)}' `
	if [  "$expect2" = "$result" ]; then
        success_count=$(( success_count + 1 ))
        echo "PREPARE and EXECUTE with UPDATE query successfully."
	fi
	result=`fgrep "DEALLOCATE test2" log/pgpool.log  | awk '{print substr($0, index($0, "DB node id:"),13)}' `
	if [  "$expect2" = "$result" ]; then
        success_count=$(( success_count + 1 ))
        echo "PREPARE and DEALLOCATE with UPDATE query successfully."
	fi

	# DEALLOCATE all;
	grep -E "DB node id: 0 .*DEALLOCATE all" log/pgpool.log
	if [  $? -eq 0 ]; then
        success_count=$(( success_count + 1 ))
        echo "node0 DEALLOCATE all query successfully."
	fi
	grep -E "DB node id: 1 .*DEALLOCATE all" log/pgpool.log
	if [  $? -eq 0 ]; then
        success_count=$(( success_count + 1 ))
        echo "node1 DEALLOCATE all query successfully."
	fi

	./shutdownall

	cd ..
done

echo "$success_count out of $num_tests successful";

if test $success_count -eq $num_tests
then
    exit 0
fi
exit 1
