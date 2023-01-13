#!/usr/bin/env bash
#-------------------------------------------------------------------
# test script for bug#63
# On memory query cache occasionally segfaults.
#
# Fixed in: http://git.postgresql.org/gitweb/?p=pgpool2.git;a=commit;h=d428c5c27600dd82161dda6a240737d88db78732
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
cp ../jdbctest2.java .
javac jdbctest2.java
export CLASSPATH=.:$JDBC_DRIVER
./startall
wait_for_pgpool_startup

$PGBENCH -i test
java jdbctest2

if [ $? != 0 ];then
	./shutdownall
	exit 1
fi

./shutdownall
exit 0
