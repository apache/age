#!/usr/bin/env bash
#-------------------------------------------------------------------
# Test script for bug reported in bug#152
# In memory query cache fails in extended query protocol.

source $TESTLIBS
TESTDIR=testdir
PSQL=$PGBIN/psql
export CLASSPATH=.:$JDBC_DRIVER

for mode in s r
do
	rm -fr $TESTDIR
	mkdir $TESTDIR
	cd $TESTDIR

# create test environment
	echo -n "creating test environment..."
	$PGPOOL_SETUP -m $mode -n 1 || exit 1
	echo "done."

	source ./bashrc.ports

	echo "memory_cache_enabled = on" >> etc/pgpool.conf
	echo "log_client_messages = on" >> etc/pgpool.conf

	./startall

	export PGPORT=$PGPOOL_PORT

	wait_for_pgpool_startup

	cp ../run.sh .
	cp ../Main.java .

	sh run.sh

	if [ $? != 0 ];then
	    ./shutdownall
	    exit 1
	fi
	./shutdownall

	cd ..
done

exit 0
