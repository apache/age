#!/usr/bin/env bash
#-------------------------------------------------------------------
# test script for memqcache.
# requires Java PostgreSQL JDBC driver.
PGBENCH=$PGBENCH_PATH

WHOAMI=`whoami`
source $TESTLIBS
TESTDIR=testdir

PSQL=$PGBIN/psql

for mode in s r n
do
	rm -fr $TESTDIR
	mkdir $TESTDIR
	cd $TESTDIR

# create test environment
	echo -n "creating test environment..."
	$PGPOOL_SETUP -m $mode -n 2 || exit 1
	echo "done."

	echo "memory_cache_enabled = on" >> etc/pgpool.conf
	echo "cache_safe_memqcache_table_list = 'cache_safe_v'" >> etc/pgpool.conf
	echo "cache_unsafe_memqcache_table_list = 'cache_unsafe_t'" >> etc/pgpool.conf

	source ./bashrc.ports

	export PGPORT=$PGPOOL_PORT

	echo "jdbc.url=jdbc:postgresql://localhost:$PGPOOL_PORT/test" > jdbctest.prop
	echo "jdbc.user=$WHOAMI" >> jdbctest.prop
	echo "jdbc.password=" >> jdbctest.prop
	cp ../jdbctest.java .
	javac jdbctest.java
	export CLASSPATH=.:$JDBC_DRIVER
	./startall
	wait_for_pgpool_startup

	$PSQL test <<EOF
CREATE SCHEMA other_schema;
CREATE TABLE t1 (i int);
CREATE TABLE cache_unsafe_t (i int);
CREATE TABLE with_modify (i int);
CREATE TABLE explain_analyze (i int);
CREATE VIEW normal_v AS SELECT * FROM t1;
CREATE VIEW cache_safe_v AS SELECT * FROM t1;
CREATE FUNCTION public.immutable_func(INTEGER) returns INTEGER AS 'SELECT \$1' LANGUAGE SQL IMMUTABLE;
CREATE FUNCTION other_schema.volatile_func(INTEGER) returns INTEGER AS 'SELECT \$1' LANGUAGE SQL VOLATILE;
SELECT pg_sleep(2);	-- Sleep for a while to make sure object creations are replicated
SELECT * FROM t1;
SELECT * FROM t1;
SELECT * FROM cache_unsafe_t;
SELECT * FROM cache_unsafe_t;
SELECT * FROM normal_v;
SELECT * FROM normal_v;
SELECT * FROM cache_safe_v;
SELECT * FROM cache_safe_v;
SELECT * FROM with_modify;
WITH cte AS (INSERT INTO with_modify values(1) RETURNING *) SELECT * FROM with_modify;
WITH cte AS (INSERT INTO with_modify values(1) RETURNING *) SELECT * FROM with_modify;
SELECT * FROM with_modify;
SELECT public.immutable_func(1);
SELECT public.immutable_func(1);
SELECT other_schema.volatile_func(1);
SELECT other_schema.volatile_func(1);
SELECT * FROM explain_analyze;
EXPLAIN ANALYZE INSERT INTO explain_analyze VALUES(1);
SELECT * FROM explain_analyze;
SELECT CURRENT_TIMESTAMP;
SELECT CURRENT_USER;
SELECT '2022-07-05 10:00:00'::TIMESTAMP;
SELECT '2022-07-05 10:00:00'::TIMESTAMP;
SELECT '2022-07-05 10:00:00'::TIME;
SELECT '2022-07-05 10:00:00'::TIME;
SELECT '2022-07-05 10:00:00'::DATE;
SELECT '2022-07-05 10:00:00'::DATE;
SELECT '2022-07-05 10:00:00'::TIMESTAMPTZ;
SELECT '2022-07-05 10:00:00'::TIMESTAMPTZ;
SELECT '2022-07-05 10:00:00'::TIMETZ;
SELECT '2022-07-05 10:00:00'::TIMETZ;
SELECT to_timestamp(0);
SELECT to_timestamp(0);
EOF

	success=true
	grep "fetched from cache" log/pgpool.log | grep t1 > /dev/null || success=false
	grep "fetched from cache" log/pgpool.log | grep cache_unsafe_t > /dev/null && success=false
	grep "fetched from cache" log/pgpool.log | grep normal_v > /dev/null && success=false
	grep "fetched from cache" log/pgpool.log | grep cache_safe_v > /dev/null || success=false
	grep "fetched from cache" log/pgpool.log | grep with_modify > /dev/null && success=false
	grep "fetched from cache" log/pgpool.log | grep immutable_func > /dev/null || success=false
	grep "fetched from cache" log/pgpool.log | grep volatile_func > /dev/null && success=false
	grep "fetched from cache" log/pgpool.log | grep explain_analyze > /dev/null && success=false
	grep "fetched from cache" log/pgpool.log | grep CURRENT_TIMESTAMP > /dev/null && success=false
	grep "fetched from cache" log/pgpool.log | grep CURRENT_USER > /dev/null && success=false
	grep "fetched from cache" log/pgpool.log | grep 'TIMESTAMP;' > /dev/null || success=false
	grep "fetched from cache" log/pgpool.log | grep 'TIME;' > /dev/null || success=false
	grep "fetched from cache" log/pgpool.log | grep 'DATE;' > /dev/null || success=false
	grep "fetched from cache" log/pgpool.log | grep 'TIMESTAMPTZ;' > /dev/null && success=false
	grep "fetched from cache" log/pgpool.log | grep 'TIMETZ;' > /dev/null && success=false
	grep "fetched from cache" log/pgpool.log | grep 'to_timestamp' > /dev/null && success=false

	if [ $success = false ];then
		./shutdownall
		exit 1
	fi
	    
	java jdbctest > result.txt 2>&1
	cmp ../expected.txt result.txt
	if [ $? != 0 ];then
		./shutdownall
		exit 1
	fi

	./shutdownall

	cd ..
done

exit 0
