#!/usr/bin/env bash
#-------------------------------------------------------------------
# test script for bug#68
# On memory query cache occasionally segfaults.
#
# Fixed in: http://git.postgresql.org/gitweb/?p=pgpool2.git;a=commit;h=d6592ea7a95ed48855332037bca285cc6f6a3547
# requires Java PostgreSQL JDBC driver.
PGBENCH=$PGBENCH_PATH


WHOAMI=`whoami`
source $TESTLIBS
TESTDIR=testdir

rm -fr $TESTDIR
mkdir $TESTDIR
cd $TESTDIR

# create test environment
echo -n "creating test environment..."
$PGPOOL_SETUP -m s -n 1 || exit 1
echo "done."

source ./bashrc.ports

export PGPORT=$PGPOOL_PORT

echo "memory_cache_enabled = on" >> etc/pgpool.conf
echo "jdbc.url=jdbc:postgresql://localhost:$PGPOOL_PORT/test" > jdbctest.prop
echo "jdbc.user=$WHOAMI" >> jdbctest.prop
echo "jdbc.password=" >> jdbctest.prop
cp ../jdbctest3.java .
javac jdbctest3.java
export CLASSPATH=.:$JDBC_DRIVER
./startall
wait_for_pgpool_startup

$PGBENCH -i test
java jdbctest3

if [ $? != 0 ];then
	./shutdownall
	exit 1
fi

./shutdownall
exit 0
