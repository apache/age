#!/usr/bin/env bash
#-------------------------------------------------------------------
# test script for a memqcache bug reported in pgpool-general-jp:1441.
# (do_query() hangs in certain condition)
# requires Java PostgreSQL JDBC driver.

source $TESTLIBS
WHOAMI=`whoami`
TESTDIR=testdir
PSQL=$PGBIN/psql
export CLASSPATH=.:$JDBC_DRIVER

rm -fr $TESTDIR
mkdir $TESTDIR
cd $TESTDIR

# create test environment
echo -n "creating test environment..."
$PGPOOL_SETUP -m s -n 2 || exit 1
echo "done."

source ./bashrc.ports

export PGPORT=$PGPOOL_PORT

echo "jdbc.url=jdbc:postgresql://localhost:$PGPOOL_PORT/test" > javatest.prop
echo "jdbc.user=$WHOAMI" >> javatest.prop
echo "jdbc.password=" >> javatest.prop


echo "memory_cache_enabled = on" >> etc/pgpool.conf
sh startall
wait_for_pgpool_startup

cp ../Sample.java .
$PSQL test <<EOF
DROP TABLE IF EXISTS t1;
CREATE TABLE t1(i int);
EOF
javac Sample.java
java Sample	# hang here if the bug bites you...
sh shutdownall
exit 0

