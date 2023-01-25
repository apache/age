#!/usr/bin/env bash
#-------------------------------------------------------------------
# test script for jdbc test
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

	echo "log_client_messages = on" >> etc/pgpool.conf

	source ./bashrc.ports

# create Java property file
	cat > pgpool.properties <<EOF
pgpooltest.host=localhost
pgpooltest.port=$PGPOOL_PORT
pgpooltest.user=$USER
pgpooltest.password=
pgpooltest.dbname=test
pgpooltest.options=
pgpooltest.tests=autocommit batch column lock select update insert CreateTempTable PrepareThreshold
EOF
	cp ../*.java .
	cp ../*.sql .
	cp ../run.sh .
	cp -r ../expected .
	cp ../expected.txt .

	./startall

	export PGPORT=$PGPOOL_PORT

	wait_for_pgpool_startup

	sh run.sh > result.txt 2>&1
	cmp result.txt expected.txt
	if [ $? != 0 ];then
	    ./shutdownall; exit 1;
	fi

	./shutdownall

	cd ..

done

exit 0
