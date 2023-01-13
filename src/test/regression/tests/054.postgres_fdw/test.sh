#!/usr/bin/env bash
#-------------------------------------------------------------------
# Check to see if postgres_fdw works with pgpool-II.  Since
# postgres_fdw sets search_path to pg_catalog, pgpool_regclass() should
# be registered as "pg_catalog.pgpool_regclass".
#
# Requires PostgreSQL 9.3 or later and contrib/postgres_fdw be
# installed.
# Fixed in: http://git.postgresql.org/gitweb/?p=pgpool2.git;a=commit;h=5e65429cd325ef2cbec22d114939cf07b3bcfa2e
WHOAMI=`whoami`
source $TESTLIBS
TESTDIR=testdir
PG_CTL=$PGBIN/pg_ctl
PSQL=$PGBIN/psql

rm -fr $TESTDIR
mkdir $TESTDIR
cd $TESTDIR

# create test environment
echo -n "creating test environment..."
$PGPOOL_SETUP -m r -n 2 --no-stop|| exit 1
echo "done."

source ./bashrc.ports

export PGPORT=$PGPOOL_PORT

wait_for_pgpool_startup

# create foreign table
$PSQL -p 11003 test <<EOF
CREATE EXTENSION postgres_fdw;
CREATE SERVER pgpool FOREIGN DATA WRAPPER postgres_fdw OPTIONS (dbname 'test', port '11000');
CREATE USER MAPPING FOR PUBLIC SERVER pgpool;
CREATE FOREIGN TABLE fr1(i INTEGER) SERVER pgpool;
EOF

# create remote table
$PSQL -p 11002 test <<EOF
CREATE TABLE fr1(i INTEGER);
EOF

# detach node #1 so that pgpool does not access
# postgres_fdw. Otherwise it will go into an infinite loop.
$PGPOOL_INSTALL_DIR/bin/pcp_detach_node -w -h localhost -p $PCP_PORT -n 1

if [ $? != 0 ];then
	echo "pcp_detach_node failed"
	exit 1
fi

# assure node #1 is detached 
sleep 1

# access foreign table 11003(PostgreSQL)->11000(pgpool)->11002(PostgreSQL)
$PSQL -p 11003 test <<EOF
SELECT * FROM fr1;
INSERT INTO fr1 VALUES(1); -- should call pgpool_regclass
EOF

grep 'do_query: error message from backend: function "pgpool_regclass(cstring)" does not exist. Exit this session.' log/pgpool.log >/dev/null 2>&1

if [ $? = 0 ];then
	./shutdownall
	exit 1
fi

./shutdownall
exit 0
