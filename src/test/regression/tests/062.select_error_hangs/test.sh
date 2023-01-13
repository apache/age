#!/usr/bin/env bash
#-------------------------------------------------------------------
# test script for bug reported in [pgpool-general-jp: 1360].
# The bug occurs when all of conditions below are met:
# - replication mode
# - replicate_select = off
# - a few backend nodes
# - a SELECT error occurring in a transaction block
source $TESTLIBS
TESTDIR=testdir
PSQL=$PGBIN/psql
PGBENCH=$PGBENCH_PATH


rm -fr $TESTDIR
mkdir $TESTDIR
cd $TESTDIR

# create test environment
echo -n "creating test environment..."
$PGPOOL_SETUP -m r -n 2 --no-stop || exit 1
echo "done."

source ./bashrc.ports

export PGPORT=$PGPOOL_PORT

wait_for_pgpool_startup

# create test script

cat > pgbench.sql  <<EOF
BEGIN;
SELECT 1 FROM not_existing_table;
END;
EOF

# create pgbench loop script
cat > pgbench_loop.sh <<EOF
for i in \`seq 1 100\`
do
$PGBENCH -M \$1 -n -t 1  -f pgbench.sql test >/dev/null 2>&1
done
exit 0
EOF

# run test. This will hung if the bug is remained.

for mode in simple extended
do
	echo -n "test $mode mode "
	timeout 30 sh pgbench_loop.sh $mode

	if [ ! $? -eq 0 ];then
		echo ...timed out.
		./shutdownall
		exit 1
	fi
	echo ...ok.
done

./shutdownall
