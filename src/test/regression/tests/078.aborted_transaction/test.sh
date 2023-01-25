#!/usr/bin/env bash
#-------------------------------------------------------------------
# test script for aborted transaction.

source $TESTLIBS
TESTDIR=testdir
PSQL=$PGBIN/psql
PG_CTL=$PGBIN/pg_ctl

rm -fr $TESTDIR
mkdir $TESTDIR
cd $TESTDIR

# create test environment
echo -n "creating test environment..."
$PGPOOL_SETUP || exit 1
echo "done."
echo "backend_weight1 = 0" >> etc/pgpool.conf

source ./bashrc.ports
export PGPORT=$PGPOOL_PORT

./startall
wait_for_pgpool_startup

$PSQL -e test > results.txt 2>&1 <<EOF
BEGIN;
SELECT * FROM non_existing_table;
SELECT 1;
END;
BEGIN;
aaa;
SELECT 1;
END;
SELECT 1;
BEGIN;
SAVEPOINT s1;
aaa;
ROLLBACK TO s1;
SELECT 1;
END;
EOF

if cmp ../expected.txt results.txt >/dev/null
then
    echo "test succeeded"
else
    echo "test failed"
    diff -c ../expected.txt results.txt
    ./shutdownall
    exit 1
fi
./shutdownall
exit 0
