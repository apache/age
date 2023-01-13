#!/usr/bin/env bash
#-------------------------------------------------------------------
# test script for snapshot isolation mode.
#
source $TESTLIBS
TESTDIR=testdir
PSQL=$PGBIN/psql
PG_CTL=$PGBIN/pg_ctl
PGBENCH=$PGBIN/pgbench
export PGDATABASE=test

rm -fr $TESTDIR
mkdir $TESTDIR
cd $TESTDIR

# create test environment.
echo -n "creating test environment..."
$PGPOOL_SETUP -m i|| exit 1
echo "done."

dir=`pwd`

source ./bashrc.ports

./startall

export PGPORT=$PGPOOL_PORT

wait_for_pgpool_startup

$PSQL <<EOF
DROP TABLE t1;
CREATE TABLE t1(i int);
INSERT INTO t1 VALUES(0);
DROP TABLE log;
CREATE TABLE log(i int);
EOF

# Do updating in simple query mode.
$PGBENCH -n -c 1 -T 30 -f ../inconsistency1.sql&

# Do SELECT INTO while updating. This will create different rows among
# node 0 log table and node 1 log table if we cannot keep global
# snapshot isolation visibly.
$PGBENCH -n -c 1 -T 30 -f ../inconsistency2.sql&
wait

# Ok let's see if rows in the log tables are identical.
psql -p 11002 -c "\copy log to '11002.txt'"
psql -p 11003 -c "\copy log to '11003.txt'"
cmp 11002.txt 11003.txt >/dev/null

if [ $? != 0 ];then
    echo "Transaction results are inconsistent (simple query)."
    ./shutdownall
    exit 1
fi
echo "Transaction results are consistent (simple query)."

$PSQL <<EOF
DROP TABLE t1;
CREATE TABLE t1(i int);
INSERT INTO t1 VALUES(0);
DROP TABLE log;
CREATE TABLE log(i int);
EOF

rm -f 11002.txt 11003.txt

# Do updating in extended query mode.
$PGBENCH -M extended -n -c 1 -T 30 -f ../inconsistency1.sql&

# Do SELECT INTO while updating. This will create different rows among
# node 0 log table and node 1 log table if we cannot keep global
# snapshot isolation visibly.
$PGBENCH -M extended -n -c 1 -T 30 -f ../inconsistency2.sql&
wait

# Ok let's see if rows in the log tables are identical.
psql -p 11002 -c "\copy log to '11002.txt'"
psql -p 11003 -c "\copy log to '11003.txt'"
cmp 11002.txt 11003.txt >/dev/null

if [ $? != 0 ];then
    echo "Transaction results inconsistent (extended query)."
    ./shutdownall
    exit 1
fi
echo "Transaction results are consistent (extended query)."

# Test #2. VACUUM after SELECT.

psql test > results.txt 2>&1 <<EOF
SELECT count(*) FROM t1;
VACUUM t1;
EOF

cmp ../expected.txt results.txt >/dev/null
if [ $? != 0 ];then
    echo "test #2 failed."
    ./shutdownall
    exit 1
fi

./shutdownall

exit 0
