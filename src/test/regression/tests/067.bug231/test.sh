#!/usr/bin/env bash
#-------------------------------------------------------------------
# test script for for bug 231 (packet kind of backend 1 ['3'] does not
# match with main/majority nodes packet kind ['2']).
#
source $TESTLIBS
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
echo "backend_weight0 = 0" >> etc/pgpool.conf
echo "backend_weight1 = 1" >> etc/pgpool.conf

source ./bashrc.ports
./startall
export PGPORT=$PGPOOL_PORT
wait_for_pgpool_startup

export PGPORT=11000
$PSQL -c "DROP TABLE IF EXISTS t1" test
$PSQL -c "CREATE TABLE t1 (id int);" test
$PSQL -c "INSERT INTO t1 VALUES(1)" test

TestProg=TestReplGap
cp ../$TestProg.java .
javac $TestProg.java
java $TestProg > $TestProg.log  2>&1
if [ $? != 0 ];then
    ./shutdownall; exit 1;
fi
./shutdownall

exit 0
