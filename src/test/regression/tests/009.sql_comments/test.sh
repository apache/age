#!/usr/bin/env bash
#-------------------------------------------------------------------
# test script for allow_sql_comments
#
source $TESTLIBS
TESTDIR=testdir
PSQL=$PGBIN/psql

function getnode()
{
	grep $1 log/pgpool.log | grep SELECT | grep LOG: |awk '{print $10}'
}

rm -fr $TESTDIR
mkdir $TESTDIR
cd $TESTDIR

# create test environment
echo -n "creating test environment..."
$PGPOOL_SETUP -m s -n 2 || exit 1
echo "done."

source ./bashrc.ports

echo "backend_weight0 = 0" >> etc/pgpool.conf
echo "backend_weight1 = 1" >> etc/pgpool.conf
echo "allow_sql_comments = off" >> etc/pgpool.conf

./startall

export PGPORT=$PGPOOL_PORT
wait_for_pgpool_startup

ok=yes

# should be redirect to node 1
$PSQL -c "SELECT 'case1'" test

test `getnode "case1"` -eq 1 || ok=ng

# should be redirect to node 0
$PSQL -c "/* comment */SELECT 'case2'" test

test `getnode "case2"` -eq 0 || ok=ng

# should be redirect to node 0
$PSQL -c "/*No LOAD BALANCE*/SELECT 'case3'" test

test `getnode "case3"` -eq 0 || ok=ng

echo "allow_sql_comments = on" >> etc/pgpool.conf

./pgpool_reload
sleep 1

wait_for_pgpool_startup

# should be redirect to node 1
$PSQL -c "SELECT 'case4'" test

test `getnode "case4"` -eq 1 || ok=ng

# should be redirect to node 1
$PSQL -c "/* comment */SELECT 'case5'" test

test `getnode "case5"` -eq 1 || ok=ng

# should be redirect to node 1. this comment is old feature.
$PSQL -c "/*No LOAD BALANCE*/SELECT 'case6'" test

test `getnode "case6"` -eq 1 || ok=ng


./shutdownall

if [ $ok = "yes" ];then
	exit 0
fi
exit 1
