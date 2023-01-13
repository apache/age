#!/usr/bin/env bash
#-------------------------------------------------------------------
# test script for bug reported in [pgpool-general: 1684].
# The bug occurs when all of conditions below are met:
# - replication mode
# - pgpool_catalog.insert_lock is created
# - extended protocol is used
# - number of DB nodes does not matter
#
# Fixed in: http://git.postgresql.org/gitweb/?p=pgpool2.git;a=commit;h=9dd90f45ef4194473c04099e6332af0a8c0d8411
# Note: to run the test DBD:Pg is required.
WHOAMI=`whoami`
source $TESTLIBS
TESTDIR=testdir
PSQL=$PGBIN/psql

rm -fr $TESTDIR
mkdir $TESTDIR
cd $TESTDIR

# create test environment
echo -n "creating test environment..."
$PGPOOL_SETUP -m r -n 1 --no-stop|| exit 1
echo "done."

source ./bashrc.ports

#./startall
wait_for_pgpool_startup

export PGPORT=$PGPOOL_PORT

# create insert_lock table

$PSQL  test <<EOF
DROP TABLE IF EXISTS pgpool_catalog.insert_lock;
CREATE SCHEMA pgpool_catalog;
CREATE TABLE pgpool_catalog.insert_lock(reloid OID PRIMARY KEY);
 
-- this row is used as the row lock target when pgpool inserts new oid
INSERT INTO pgpool_catalog.insert_lock VALUES (0);

-- allow "SELECT ... FOR UPDATE" and "INSERT ..." to all roles
GRANT SELECT ON pgpool_catalog.insert_lock TO PUBLIC;
GRANT UPDATE ON pgpool_catalog.insert_lock TO PUBLIC;
GRANT INSERT ON pgpool_catalog.insert_lock TO PUBLIC;
EOF

# create test table

$PSQL  test <<EOF
DROP TABLE IF EXISTS kd_test_serial_pgpool;
CREATE TABLE kd_test_serial_pgpool(id SERIAL, content TEXT);
EOF

cat > test.perl  <<EOF
use strict;
use DBI;
my \$dbh;
my \$sth;
\$dbh = DBI->connect("DBI:Pg:dbname=test;host=localhost;port=$PGPOOL_PORT","$WHOAMI",'');
if (\$dbh)
{
  \$sth = \$dbh->prepare("insert into kd_test_serial_pgpool(content)values (?)");      # hangs
  \$sth->execute('perl lkhzfjktfjktf');
  \$sth->finish;
}
\$dbh->disconnect();
EOF

# run test. This will hung if the bug is remained.
(perl test.perl)&
sleep 5
kill $!

if [ $? = 0 ];then
	./shutdownall
	exit 1
fi

./shutdownall
