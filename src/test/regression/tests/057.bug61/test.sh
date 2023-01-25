#!/usr/bin/env bash
#-------------------------------------------------------------------
# test script for bug#61
# Child process xxxx was terminated by segmentation fault
#
# Fixed in: http://git.postgresql.org/gitweb/?p=pgpool2.git;a=commit;h=d493dd2b7d2065fb554654b137ecd587564f0043
source $TESTLIBS
TESTDIR=testdir
PG_CTL=$PGBIN/pg_ctl
PSQL=$PGBIN/psql
PGBENCH=$PGBENCH_PATH

rm -fr $TESTDIR
mkdir $TESTDIR
cd $TESTDIR

# create test environment
echo -n "creating test environment..."
$PGPOOL_SETUP -m s -n 1 || exit 1
echo "done."

source ./bashrc.ports

export PGPORT=$PGPOOL_PORT

./startall
wait_for_pgpool_startup

$PGBENCH -i test

# Pre 9.4 libpq does not allow to specify NULL username
echo "psql: FATAL: no PostgreSQL user name specified in startup packet" > expected.txt
echo "\\q"|$PSQL -p $PGPOOL_PORT -U '' test > psql.log 2>&1
psqlresult=$?

cat psql.log
diff -b psql.log expected.txt
diffresult=$?

./shutdownall

if [ $psqlresult -ne 0 ];then
	if [ $diffresult -eq 0 ]; then
		# must be pre 9.4
		exit 0
	else
		# pre 9.4 but does not work well or there's something wrong.
		exit 1
	fi
fi
# other case is all good

exit 0
