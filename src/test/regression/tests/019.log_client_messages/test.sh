#!/usr/bin/env bash
#-------------------------------------------------------------------
# test script for log_client_messages
#
source $TESTLIBS
TESTDIR=`pwd`/testdir
PSQL=$PGBIN/psql
PGBENCH=$PGBENCH_PATH
WHOAMI=`whoami`

#test parameter
timeout=30
num_tests=14
success_count=0

rm -fr $TESTDIR
mkdir $TESTDIR
cd $TESTDIR

# create test environment
echo -n "creating test environment..."
$PGPOOL_SETUP -n 1  || exit 1
echo "done."

source ./bashrc.ports
export PGPORT=$PGPOOL_PORT

# log_client_messages parameter change
./startall
wait_for_pgpool_startup

echo "log_client_messages = on" >> etc/pgpool.conf
./pgpool_reload
sleep 1
wait_for_pgpool_startup

$PSQL -c "PGPOOL SHOW log_client_messages" test > psql_status 2>&1
if [ !`cat psql_status  |grep 'on'` ]
then
	./shutdownall
	exit 1
fi

$PGBENCH -i test

# pgproto test
cp -r ../tests  ./
tests=`(ls tests)`

for i in $tests
do
	echo -n "testing $i ..."
	timeout $timeout $PGPOOL_INSTALL_DIR/bin/pgproto -u $WHOAMI -p $PGPOOL_PORT -d test -f tests/$i
done

# send cancel request
# executing long running query
$PSQL -c "select pg_sleep(30);" test &
# get psql process pid
psqlpid=$!
sleep 2
# send CtrlC to psql to cancel the query
kill -INT $psqlpid
sleep 2



./shutdownall
#Bind
grep -A 1 -e "Bind message from frontend." log/pgpool.log |grep -E "portal: \".*\", statement: \".*\"" >/dev/null 2>&1
if [ $? = 0 ];then
	echo "success: Bind log"
	success_count=$(( success_count + 1 ))
fi
#CancelRequest
grep -A 1 -e "Cancel message from frontend." log/pgpool.log |grep -E "process id: .+" >/dev/null 2>&1
if [ $? = 0 ];then
	echo "success: CancelRequest log"
	success_count=$(( success_count + 1 ))
fi
#Close
grep -A 1 -e "Close message from frontend." log/pgpool.log |grep -E "(portal|statement): \".*\"" >/dev/null 2>&1
if [ $? = 0 ];then
	echo "success: Close log"
	success_count=$(( success_count + 1 ))
fi
#CopyData
grep -A 1 -e "CopyData message from frontend." log/pgpool.log |grep -E "count: [0-9]+"  >/dev/null 2>&1
if [ $? = 0 ];then
	echo "success: CopyData log"
	success_count=$(( success_count + 1 ))
fi
#CopyDone
grep -e "CopyDone message from frontend." log/pgpool.log >/dev/null 2>&1
if [ $? = 0 ];then
    echo "success: CopyDone log"
    success_count=$(( success_count + 1 ))
fi
#CopyFail
grep -e "CopyFail message from frontend." log/pgpool.log >/dev/null 2>&1
if [ $? = 0 ];then
	echo "success: CopyFail log"
	success_count=$(( success_count + 1 ))
fi
#Describe
grep -A 1 -e "Describe message from frontend." log/pgpool.log |grep -E "(portal|statement): \".*\"" >/dev/null 2>&1
if [ $? = 0 ];then
	echo "success: Describe log"
	success_count=$(( success_count + 1 ))
fi
#Execute
grep -A 1 -e "Execute message from frontend." log/pgpool.log |grep -E "portal: \".*\"" >/dev/null 2>&1
if [ $? = 0 ];then
	echo "success: Execute log"
	success_count=$(( success_count + 1 ))
fi
#Flush
grep -e "Flush message from frontend." log/pgpool.log >/dev/null 2>&1
if [ $? = 0 ];then
	echo "success: Flush log"
	success_count=$(( success_count + 1 ))
fi
#FunctionCall
grep -A 1 -e "FunctionCall message from frontend." log/pgpool.log |grep -E "oid: \".+\"" >/dev/null 2>&1
if [ $? = 0 ];then
	echo "success: FunctionCall log"
	success_count=$(( success_count + 1 ))
fi
#Parse
grep -A 1 -e "Parse message from frontend." log/pgpool.log |grep -E "statement: \".*\", query: \".*\"" >/dev/null 2>&1
if [ $? = 0 ];then
	echo "success: Parse log"
	success_count=$(( success_count + 1 ))
fi
#Query
grep -A 1 -e "Query message from frontend." log/pgpool.log |grep -E "query: \".*\"" >/dev/null 2>&1
if [ $? = 0 ];then
	echo "success: Query log"
	success_count=$(( success_count + 1 ))
fi
#Sync
grep -e "Sync message from frontend." log/pgpool.log >/dev/null 2>&1
if [ $? = 0 ];then
	echo "success: Sync log"
	success_count=$(( success_count + 1 ))
fi
#Terminate
grep -e "Terminate message from frontend." log/pgpool.log >/dev/null 2>&1
if [ $? = 0 ];then
	echo "success: Terminate log"
	success_count=$(( success_count + 1 ))
fi

echo "$success_count out of $num_tests successful";

if test $success_count -eq $num_tests
then
	exit 0;
fi

exit 1;
