#!/usr/bin/env bash
#-------------------------------------------------------------------
# Test script for bug reported in bug#136.
# Tables including spaces does not work with pgpool-II's internal queries
# which are used to extract information from PostgreSQL system catalogs.
# The table name in trouble is not only spaces but also mixed-case-table-name.
source $TESTLIBS
TESTDIR=testdir
PSQL=$PGBIN/psql

for mode in s r
do
	rm -fr $TESTDIR
	mkdir $TESTDIR
	cd $TESTDIR

# create test environment
	echo -n "creating test environment..."
	$PGPOOL_SETUP -m $mode -n 2 || exit 1
	echo "done."

	echo "memory_cache_enabled = on" >> etc/pgpool.conf

	source ./bashrc.ports

	./startall

	export PGPORT=$PGPOOL_PORT

	wait_for_pgpool_startup

	$PSQL test <<EOF
CREATE TABLE t1(i INTEGER);
CREATE TABLE "t1 t2"(i INTEGER);
CREATE TABLE "tT"(i INTEGER);
INSERT INTO t1 VALUES(1);
SELECT * FROM t1;
INSERT INTO "t1 t2"VALUES(1);
SELECT * FROM "t1 t2";
INSERT INTO "tT" VALUES(1);
SELECT * FROM "tT";
EOF

	if [ $? != 0 ];then
	    ./shutdownall
	    exit 1
	fi

	./shutdownall

	cd ..

done

exit 0
