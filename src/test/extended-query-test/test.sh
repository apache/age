#!/usr/bin/env bash

dir=`pwd`
MODE=install
PG_INSTALL_DIR=/usr/local/pgsql/bin
PGPOOL_PATH=/usr/local
JDBC_DRIVER=/usr/local/pgsql/share/postgresql-9.2-1003.jdbc4.jar
export log=$dir/log
PGSOCKET_DIR=/tmp

timeout=30
export PGPORT=11000
export PGHOST=127.0.0.1
export PGDATABASE=test
#export PGPOOL_INSTALL_DIR=$HOME/work/pgpool-II/current
#export PGPOOLDEBUG=true
PGPROTO=/usr/local/bin/pgproto

testdir=$dir/tests
expected=$dir/expected
extra_scripts=$dir/extra_scripts
export PGPOOLLOG=$dir/testdata/log/pgpool.log
results=$dir/results

function install_pgpool
{
    echo "creating pgpool-II temporary installation ..."
    PGPOOL_PATH=$dir/temp/installed

    test -d $log || mkdir $log

    make install -C $dir/../../ -e prefix=${PGPOOL_PATH} >& regression.log 2>&1

    if [ $? != 0 ];then
	echo "make install failed"
	exit 1
    fi
    
    echo "moving pgpool_setup to temporary installation path ..."
    cp $dir/../pgpool_setup ${PGPOOL_PATH}/pgpool_setup
    export PGPOOL_SETUP=$PGPOOL_PATH/pgpool_setup
}

function verify_pginstallation
{
    # PostgreSQL bin directory
    PGBIN=`$PG_INSTALL_DIR/pg_config --bindir`
    if [ -z $PGBIN ]; then
	echo "$0: cannot locate pg_config"
	exit 1
    fi
}

function export_env_vars
{
    if [[ -z "$PGPOOL_PATH" ]]; then
	# check if pgpool is in the path
	PGPOOL_PATH=/usr/local
	export PGPOOL_SETUP=$HOME/bin/pgpool_setup
    fi

    if [[ -z "$PGBENCH_PATH" ]]; then
	if [ -x $PGBIN/pgbench ]; then
	    PGBENCH_PATH=$PGBIN/pgbench
	else
	    PGBENCH_PATH=`which pgbench`
	fi
    fi

    if [ ! -x $PGBENCH_PATH ]; then
	echo "$0] cannot locate pgbench"; exit 1
    fi

    echo "using pgpool-II at "$PGPOOL_PATH
    export PGPOOL_INSTALL_DIR=$PGPOOL_PATH

    PGPOOLLIB=${PGPOOL_INSTALL_DIR}/lib
    if [ -z "$LD_LIBRARY_PATH" ];then
	export LD_LIBRARY_PATH=$PGPOOLLIB
    else
	export LD_LIBRARY_PATH=${PGPOOLLIB}:${LD_LIBRARY_PATH}
    fi

    export TESTLIBS=$dir/libs.sh
    export PGBIN=$PGBIN
    export JDBC_DRIVER=$JDBC_DRIVER
    export PGBENCH_PATH=$PGBENCH_PATH
    export PGSOCKET_DIR=$PGSOCKET_DIR
}

function print_info
{
    echo ${CBLUE}"*************************"${CNORM}

    echo "REGRESSION MODE : "${CBLUE}$MODE${CNORM}
    echo "PGPOOL-II       : "${CBLUE}$PGPOOL_PATH${CNORM}
    echo "PostgreSQL bin  : "${CBLUE}$PGBIN${CNORM}
    echo "pgbench         : "${CBLUE}$PGBENCH_PATH${CNORM}
    echo "PostgreSQL jdbc : "${CBLUE}$JDBC_DRIVER${CNORM}
    echo ${CBLUE}"*************************"${CNORM}
}

install_pgpool
verify_pginstallation
export_env_vars
print_info

okcnt=0
failcnt=0
timeoutcnt=0

# Selecting load balancing node per session or per statement
lb_level=(session statement)
specified_tests=$1
for lb in ${lb_level[@]}
do
    cd $dir
    rm -f $results/*
    test ! -d $results && mkdir $results

    diffs=$dir/diffs
    rm -f $diffs

    if [ $# -gt 0 ];then
	tests=`(cd tests;ls |grep $specified_tests)`
    else
	tests=`(cd tests;ls)`
    fi
    rm -fr testdata
    mkdir testdata
    cd testdata
    echo -n "*** creating test database with 2 nodes..."
    $PGPOOL_SETUP > /dev/null 2>&1
    echo "done."
    cp etc/pgpool.conf pgpool.conf.back

    # change load balancing level
    if [ $lb == 'statement' ]; then
	echo "statement_level_load_balance = on"
	echo "statement_level_load_balance = on" >> etc/pgpool.conf
    else
	echo "statement_level_load_balance = off"
    fi

    for i in $tests
    do
	echo -n "testing $i ... "

	# check if modification to pgpool.conf specified.
	d=/tmp/diff$$
	grep '^##' $testdir/$i > $d
	if [ -s $d ]
	then
	    sed -e 's/^##//' $d >> etc/pgpool.conf
	fi
	rm -f $d

	./startall >/dev/null 2>&1

	while :
	do
	    psql -c "select 1" test >/dev/null 2>&1
	    if [ $? = 0 ]
	    then
		break
	    fi
	    sleep 1
	done

	timeout $timeout $PGPROTO -f $testdir/$i > $results/$i 2>&1
	if [ $? = 124 ]
	then
	    echo "timeout."
	    timeoutcnt=`expr $timeoutcnt + 1`
	else
	    sed -e 's/L [0-9]*/L xxx/g' $expected/$i > expected_tmp
	    sed -e 's/L [0-9]*/L xxx/g' $results/$i > results_tmp
	    cmp expected_tmp results_tmp >/dev/null 2>&1
	    if [ $? != 0 ]
	    then
		echo "failed."
		echo "=== $i ===" >> $diffs
		diff -c expected_tmp results_tmp >> $diffs
		failcnt=`expr $failcnt + 1`
	    else
		extra_fail=0
		# execute extra scripts if exists.
		if [ -x $extra_scripts/$i ]
		then
		    $extra_scripts/$i > $results/$i.extra 2>&1

		    if [ $? != 0 ]
		    then
			echo "extra test failed."
			extra_fail=1
			failcnt=`expr $failcnt + 1`
		    fi
		fi

		if [ $extra_fail = 0 ]
		then
		    echo "ok."
		    okcnt=`expr $okcnt + 1`
		fi
	    fi
	    rm expected_tmp results_tmp
	fi
	grep pool_check_pending_message_and_reply log/pgpool.log
	./shutdownall >/dev/null 2>&1
	cp pgpool.conf.back etc/pgpool.conf
	process=`ps x|grep pgpool|grep idle`
	if [ ! -z $process ]
	then
	    echo "Some process remains. Aborting tests"
	    exit 1
	fi
    done
done

## Test statement_level_load_balance with 3 nodes

cd $dir
testdir=$dir/tests_n3
if [ $# -gt 0 ];then
    tests_n3=`(cd tests_n3;ls | grep $specified_tests)`
else
    tests_n3=`(cd tests_n3;ls)`
fi

if [ "$tests_n3" = "" ];then
    echo "No test data for 3 nodes. Skipping 3 nodes tests."
else
    rm -fr testdata
    mkdir testdata
    cd testdata
    echo -n "creating test database with 3 nodes..."
    $PGPOOL_SETUP -n 3 > /dev/null 2>&1
    echo "done."
    cp etc/pgpool.conf pgpool.conf.back

    for i in $tests_n3
    do
	echo -n "testing $i ... "

	# check if modification to pgpool.conf specified.
	d=/tmp/diff$$
	grep '^##' $testdir/$i > $d
	if [ -s $d ]
	then
	    sed -e 's/^##//' $d >> etc/pgpool.conf
	fi
	rm -f $d

	./startall >/dev/null 2>&1

	while :
	do
	    psql -c "select 1" test >/dev/null 2>&1
	    if [ $? = 0 ]
	    then
		break
	    fi
	    sleep 1
	done

	timeout $timeout $PGPROTO -f $testdir/$i > $results/$i 2>&1
	if [ $? = 124 ]
	then
	    echo "timeout."
	    timeoutcnt=`expr $timeoutcnt + 1`
	else
	    sed -e 's/L [0-9]*/L xxx/g' $expected/$i > expected_tmp
	    sed -e 's/L [0-9]*/L xxx/g' $results/$i > results_tmp
	    cmp expected_tmp results_tmp >/dev/null 2>&1
	    if [ $? != 0 ]
	    then
		echo "failed."
		echo "=== $i ===" >> $diffs
		diff -c expected_tmp results_tmp >> $diffs
		failcnt=`expr $failcnt + 1`
	    else
		extra_fail=0
		# execute extra scripts if exists.
		if [ -x $extra_scripts/$i ]
		then
		    $extra_scripts/$i > $results/$i.extra 2>&1

		    if [ $? != 0 ]
		    then
			echo "extra test failed."
			extra_fail=1
			failcnt=`expr $failcnt + 1`
		    fi
		fi

		if [ $extra_fail = 0 ]
		then
		    echo "ok."
		    okcnt=`expr $okcnt + 1`
		fi
	    fi
	    rm expected_tmp results_tmp
	fi
	grep pool_check_pending_message_and_reply log/pgpool.log
	./shutdownall >/dev/null 2>&1
	cp pgpool.conf.back etc/pgpool.conf
	process=`ps x|grep pgpool|grep idle`
	if [ ! -z "$process" ]
	then
	    echo "Some process remains. Aborting tests"
	    exit 1
	fi

    done
fi
######
total=`expr $okcnt + $failcnt + $timeoutcnt`
echo "out of $total ok: $okcnt failed: $failcnt timeout: $timeoutcnt."
