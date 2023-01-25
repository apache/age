#!/usr/bin/env bash
#-------------------------------------------------------------------
# test script for native replication mode.
#
source $TESTLIBS
TESTDIR=testdir
PSQL=$PGBIN/psql
PGBENCH=$PGBENCH_PATH

rm -fr $TESTDIR
mkdir $TESTDIR
cd $TESTDIR

# create test environment
echo -n "creating test environment..."
$PGPOOL_SETUP -m r -n 2 || exit 1
echo "done."

source ./bashrc.ports

./startall

export PGPORT=$PGPOOL_PORT

wait_for_pgpool_startup

$PSQL test <<EOF
CREATE TABLE t1(i SERIAL, j TIMESTAMP WITH TIME ZONE DEFAULT CURRENT_TIMESTAMP);
CREATE TABLE t2(i SERIAL, j DATE DEFAULT CURRENT_DATE);
CREATE TABLE t3(i SERIAL, j TIME WITH TIME ZONE DEFAULT CURRENT_TIME);
CREATE TABLE t4(i SERIAL, j TIME WITH TIME ZONE DEFAULT LOCALTIME);
CREATE TABLE t5(i SERIAL, j TIMESTAMP WITH TIME ZONE DEFAULT LOCALTIMESTAMP);
EOF

cat > pgbench.sql <<EOF
INSERT INTO t1 VALUES (DEFAULT);
INSERT INTO t2 VALUES (DEFAULT);
INSERT INTO t3 VALUES (DEFAULT);
INSERT INTO t4 VALUES (DEFAULT);
INSERT INTO t5 VALUES (DEFAULT);
EOF

$PGBENCH -i test
$PGBENCH -f pgbench.sql -c 10 -t 10 test

# test with extended protocol (autocommit on)
# per [pgpool-general: 2144].
cp ../PgTester.java .
javac PgTester.java
export CLASSPATH=.:$JDBC_DRIVER
$PSQL -f ../create.sql test
env
$PSQL -f $PGPOOL_INSTALL_DIR/share/pgpool-II/insert_lock.sql test

java PgTester 0 &
java PgTester 10 &
java PgTester 100 &
java PgTester 1000 &
wait

$PSQL -p 11002 test <<EOF
\copy (SELECT * FROM t1 ORDER BY i) to 'dump_t1_n0.txt'
\copy (SELECT * FROM t2 ORDER BY i) to 'dump_t2_n0.txt'
\copy (SELECT * FROM t3 ORDER BY i) to 'dump_t3_n0.txt'
\copy (SELECT * FROM t4 ORDER BY i) to 'dump_t4_n0.txt'
\copy (SELECT * FROM t5 ORDER BY i) to 'dump_t5_n0.txt'
EOF

$PSQL -p 11003 test <<EOF
\copy (SELECT * FROM t1 ORDER BY i) to 'dump_t1_n1.txt'
\copy (SELECT * FROM t2 ORDER BY i) to 'dump_t2_n1.txt'
\copy (SELECT * FROM t3 ORDER BY i) to 'dump_t3_n1.txt'
\copy (SELECT * FROM t4 ORDER BY i) to 'dump_t4_n1.txt'
\copy (SELECT * FROM t5 ORDER BY i) to 'dump_t5_n1.txt'
EOF

$PSQL -p 11002 test <<EOF
\copy (SELECT * FROM sequencetester ORDER BY id) to 'dump_s0.txt'
EOF

$PSQL -p 11003 test <<EOF
\copy (SELECT * FROM sequencetester ORDER BY id) to 'dump_s1.txt'
EOF

# check if database contents are identical
cat dump_t1_n0.txt dump_t2_n0.txt dump_t3_n0.txt dump_t4_n0.txt dump_t5_n0.txt dump_s0.txt > dump0.txt
cat dump_t1_n1.txt dump_t2_n1.txt dump_t3_n1.txt dump_t4_n1.txt dump_t5_n1.txt dump_s1.txt > dump1.txt
diff dump0.txt dump1.txt
if [ $? != 0 ];then
	# contents are not identical
	./shutdownall
	exit 1
fi

./shutdownall

exit 0
