#!/usr/bin/env bash
#-------------------------------------------------------------------
# test script for for bug 230 (pgpool does not return result set if
# the statement fetchSize is smaller than the result row count.)
#
source $TESTLIBS
TESTDIR=testdir
PSQL=$PGBIN/psql
export CLASSPATH=.:$JDBC_DRIVER

for mode in s r n
do
	echo "===== mode:$mode ====="
	rm -fr $TESTDIR
	mkdir $TESTDIR
	cd $TESTDIR

# create test environment
	echo -n "creating test environment..."

	# to avoid replication lag problem when testing under streaming
	# replication mode, set number of cluster to 1 when in the mode
	if [ $mode = "s" ];then
		n=1
	else
		n=2
	fi

	$PGPOOL_SETUP -m $mode -n $n || exit 1
	echo "done."

	source ./bashrc.ports

	./startall

	export PGPORT=$PGPOOL_PORT

	wait_for_pgpool_startup

	cp ../Sample.java .
	javac Sample.java
	java Sample > sample.log  2>&1
	if [ $? != 0 ];then
	    ./shutdownall; exit 1;
	fi

	./shutdownall

	cd ..

done

exit 0
