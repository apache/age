#!/usr/bin/env bash
#-------------------------------------------------------------------
# test script for memqcache with memcached.
# need to configure --with-memcached=/usr.
# requires Java PostgreSQL JDBC driver.
PGBENCH=$PGBENCH_PATH

WHOAMI=`whoami`
source $TESTLIBS
TESTDIR=testdir
PSQL=$PGBIN/psql


rm -fr $TESTDIR
mkdir $TESTDIR
cd $TESTDIR

# create test environment
echo -n "creating test environment..."
$PGPOOL_SETUP -n 2 || exit 1
echo "done."

#echo "memory_cache_enabled = off" >> etc/pgpool.conf
echo "log_min_messages = debug1" >> etc/pgpool.conf
echo "enable_shared_relcache = on" >> etc/pgpool.conf
echo "relcache_expire = 60" >> etc/pgpool.conf

for mode in shmem memcached
do
	if [ $mode = "shmem" ];then
		echo "memqcache_method = 'shmem'" >> etc/pgpool.conf
	elif [ $mode = "memcached" ];then
		echo "memqcache_method = 'memcached'" >> etc/pgpool.conf
	fi

	source ./bashrc.ports

	export PGPORT=$PGPOOL_PORT

	./startall
	wait_for_pgpool_startup

    # initialize tables
    $PGBENCH -i test

    $PGBENCH -C -S -c 2 -t 10 test

	./shutdownall

	echo "SELECT query don not use query cache"
	grep "committing SELECT results to cache storage" log/pgpool.log > /dev/null 2>&1
	if [ $? != 0 ];then
		echo "... ok."
	else
		echo "... failed."
		exit 1
	fi

	echo "relation cache use query cache"
	grep "committing relation cache to cache storage" log/pgpool.log > /dev/null 2>&1
	if [ $? = 0 ];then
		echo "... ok."
	else
		echo "... failed."
		exit 1
	fi
done

exit 0
