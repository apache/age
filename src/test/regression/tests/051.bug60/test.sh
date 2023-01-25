#!/usr/bin/env bash
#-------------------------------------------------------------------
# test script for bug reported in bug track #60
# The bug occurs when all of conditions below are met:
#
# - streaming replication mode
# - run in an explicit transaction
# - primary fails on commit
#
# Note that pgpool disconnects the session as expected but the
# reporter thinks pgpool should not disconnects the session, just
# report an error. If the script is executed under native replication
# mode, session is not disconnect.  So I think the complain is fair
# enough.
#
# Fixed in: http://git.postgresql.org/gitweb/?p=pgpool2.git;a=commit;h=e7e350465a6f33562322304b56f02cdbb1492641
#
WHOAMI=`whoami`
source $TESTLIBS
TESTDIR=testdir
PSQL=$PGBIN/psql

rm -fr $TESTDIR
mkdir $TESTDIR
cd $TESTDIR

# create test environment
echo -n "creating test environment..."
$PGPOOL_SETUP -m s -n 2 || exit 1
echo "done."

source ./bashrc.ports

./startall
wait_for_pgpool_startup

export PGPORT=$PGPOOL_PORT

# set up trigger
$PSQL -f ../database-setup.sql test

# trigger the bug
$PSQL -f ../bug.sql test

# trigger the bug in extended protocol mode
perl <<EOF
use strict;
use DBI;
my \$dbh;
my \$sth;
\$dbh = DBI->connect("DBI:Pg:dbname=test;host=localhost;port=$PGPOOL_PORT","$WHOAMI",'');
if (\$dbh)
{
  \$dbh->{AutoCommit} = 0;
  \$sth = \$dbh->prepare("insert into my_table ( col1 ) values (?)");
  \$sth->execute('ouch');
#  \$sth = \$dbh->prepare("insert into t1 ( i ) values (?)");
#  \$sth->execute(1);

  \$sth->finish;
  \$dbh->commit;
}
\$dbh->disconnect();
EOF

# clean up
#psql -f ../database-clean.sql test

./shutdownall

#
# check if kind mismatch error occurs
#
grep "kind mismatch among backends." log/pgpool.log >/dev/null 2>&1
if [ $? = 0 ];then
	# kind mismatch message found! The bug reveals.
	exit 1
fi
exit 0
