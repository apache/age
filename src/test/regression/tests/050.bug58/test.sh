#!/usr/bin/env bash
#-------------------------------------------------------------------
# test script for bug#58
# 0000058: query cache invalidation does not fire for multiple DML in transaction
WHOAMI=`whoami`
source $TESTLIBS
TESTDIR=testdir
PSQL=$PGBIN/psql

rm -fr $TESTDIR
mkdir $TESTDIR
cd $TESTDIR

# create test environment
echo -n "creating test environment..."
$PGPOOL_SETUP -m s -n 1 --no-stop|| exit 1
echo "done."

source ./bashrc.ports

echo "memory_cache_enabled = on" >> etc/pgpool.conf

#./startall

export PGPORT=$PGPOOL_PORT

wait_for_pgpool_startup

$PSQL test <<EOF
CREATE TABLE t1(i INTEGER);
CREATE TABLE t2(i INTEGER);
SELECT * FROM t1;		-- this creates a cache entry
BEGIN;
DELETE FROM t2 WHERE i = 0;		-- this tries to invalidate cache entry for t2 but fails. Fine.
INSERT INTO t1(i) VALUES(1);	-- because of the bug, this does not invalidate the cache entry for t1
COMMIT;

SELECT * FROM t1;		-- because the cache entry for t1 remains, this returns outdated result
EOF

./shutdownall

#
# check if pgpool fetches outdated cache entry
grep "query result fetched from cache" log/pgpool.log >/dev/null 2>&1
if [ $? = 0 ];then
	# cache found! The bug reveals.
	exit 1
fi
exit 0
