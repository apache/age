#!/usr/bin/env bash
#----------------------------------------------------------------------------------------
# test script for dml adaptive.
set -e

#---------------------------------------------------------------------------------------

HOST_IP="127.0.0.1"
WHOAMI=`whoami`
BASE_PORT=${BASE_PORT:-"11000"}
CLUSTERS_NUM=${CLUSTERS_NUM:-"2"}

BASE_DIR=$(cd `dirname $0`; pwd)
TEST_DIR=$BASE_DIR/testdir
LOG_FILE=$BASE_DIR/test.log

PG_INSTALL_PATH=${PG_INSTALL_PATH:-"/usr/local/pgsql"}
PG_USER=${PG_USER:-"postgres"}
PG_REPLICATION_USER=${PG_REPLICATION_USER:-"repl"}
PG_VERSION=""

#---------------------------------------------------------------------------------------

function check_version()
{
	echo "check PostgreSQL version ..."

	# get PostgreSQL major version
	vstr=`$PG_INSTALL_PATH/bin/initdb -V|awk '{print $3}'|sed 's/\./ /g'`

	set +e
	# check if alpha or beta
	echo $vstr|egrep "[a-z]" > /dev/null
	if [ $? = 0 ];then
		vstr=`echo $vstr|sed 's/\([0-9]*\).*/\1/'`
		major1=`echo $vstr|awk '{print $1}'`
		major2=`echo $vstr|awk '{print $2}'`
		if [ -z $major2 ];then
		major2=0
		fi
	else
		vstr=`echo $vstr|sed 's/\./ /g'`
		major1=`echo $vstr|awk '{print $1}'`
		major2=`echo $vstr|awk '{print $2}'`
	fi
	set -e

	major1=`expr $major1 \* 10`
	PG_VERSION=`expr $major1 + $major2`
	echo PostgreSQL major version: $PG_VERSION
	if [ $PG_VERSION -lt 100 ];then
		echo "in order to make the script run normally, please make sure PostgreSQL major version greater than 10.0"
		exit 1
	fi

	echo "done"
}

#-------------------------------------------
# create PostgreSQL cluster
#-------------------------------------------
function initdb_primary_cluster()
{
	echo "initdb_primary_cluster ..."

	echo -n "creating database cluster $TEST_DIR/data-primary..."

	INITDB_ARG="--no-locale -E UTF_8"
	$PG_INSTALL_PATH/bin/initdb -D $TEST_DIR/data-primary $INITDB_ARG -U $PG_USER

	echo "done"
}

function set_primary_postgresql_conf()
{
	echo "set_primary_postgresql_conf ..."

	PG_CONF=$TEST_DIR/data-primary/postgresql.conf
	PG_HBA_CONF_0=$TEST_DIR/data-primary/pg_hba.conf
	PORT=`expr $BASE_PORT + 0`

	echo "listen_addresses = '*'" >> $PG_CONF
	echo "port = $PORT" >> $PG_CONF

	echo "archive_mode = on" >> $PG_CONF
	echo "archive_command = 'cp %p $TEST_DIR/archivedir/%f </dev/null'" >> $PG_CONF
	mkdir $TEST_DIR/archivedir

	echo "max_wal_senders = 10" >> $PG_CONF
	echo "max_replication_slots = 10" >> $PG_CONF

	echo "wal_level = replica" >> $PG_CONF
	echo "wal_keep_segments = 512" >> $PG_CONF

	echo "done"
}

function create_role()
{
	echo "create_role ..."

	PORT=`expr $BASE_PORT + 0`
	$PG_INSTALL_PATH/bin/psql  -h $HOST_IP -p $PORT -U $PG_USER -d postgres -c "CREATE ROLE $PG_REPLICATION_USER REPLICATION LOGIN"

	echo "done"
}

function create_standby()
{
	echo "create_standby ..."

	CLUSTER_DIR=$TEST_DIR/data-standby
	PG_CONF=$CLUSTER_DIR/postgresql.conf
	PORT_PRIMARY=`expr $BASE_PORT + 0`
	PORT_STANDBY=`expr $BASE_PORT + 1`

	$PG_INSTALL_PATH/bin/pg_basebackup -h $HOST_IP -p $PORT_PRIMARY -U $PG_REPLICATION_USER -Fp -Xs -Pv -R -D $CLUSTER_DIR

	echo "port = $PORT_STANDBY" >> $PG_CONF

	if [ $PG_VERSION -lt 120 ];then
		# PG_VERSION < 12.0
		sed -i "s/primary_conninfo = '/primary_conninfo = 'application_name=standby01 /g" $CLUSTER_DIR/recovery.conf
	else
		# PG_VERSION >= 12.0
		sed -i "s/primary_conninfo = '/primary_conninfo = 'application_name=standby01 /g" $CLUSTER_DIR/postgresql.auto.conf
	fi

	echo "done"
}

function set_sync_primary_postgresql_conf()
{
	echo "set_sync_primary_postgresql_conf ..."

	CLUSTER_DIR=$TEST_DIR/data-primary
	PG_CONF=$CLUSTER_DIR/postgresql.conf

	echo "synchronous_commit = on" >> $PG_CONF
	echo "synchronous_standby_names = 'standby01'" >> $PG_CONF

	echo "done"
}

function start_primary()
{
	echo "start_primary ..."

	CLUSTER_DIR=$TEST_DIR/data-primary

	$PG_INSTALL_PATH/bin/pg_ctl -D $CLUSTER_DIR start

	echo "done"
}

function start_pg_all()
{
	echo "start_pg_all ..."

	CLUSTER_DIR_PRIMARY=$TEST_DIR/data-primary
	CLUSTER_DIR_STANDBY=$TEST_DIR/data-standby

	$PG_INSTALL_PATH/bin/pg_ctl -D $CLUSTER_DIR_PRIMARY restart
	sleep 1
	$PG_INSTALL_PATH/bin/pg_ctl -D $CLUSTER_DIR_STANDBY start
	sleep 1

	echo "done"
}

function stop_pg_all()
{
	echo "stop_pg_all ..."

	CLUSTER_DIR_PRIMARY=$TEST_DIR/data-primary
	CLUSTER_DIR_STANDBY=$TEST_DIR/data-standby

	$PG_INSTALL_PATH/bin/pg_ctl -D $CLUSTER_DIR_STANDBY stop
	sleep 1
	$PG_INSTALL_PATH/bin/pg_ctl -D $CLUSTER_DIR_PRIMARY stop
	sleep 1

	echo "done"
}

function create_streaming_replication()
{
	echo "create_streaming_replication ..."

	initdb_primary_cluster
	set_primary_postgresql_conf
	start_primary
	create_role
	create_standby
	set_sync_primary_postgresql_conf
	start_pg_all

	echo "done"
}

function set_pool_conf()
{
echo "set_pool_conf ..."

PORT_PRIMARY=`expr $BASE_PORT + 0`
PORT_STANDBY=`expr $BASE_PORT + 1`
PORT_POOL=`expr $BASE_PORT + 2`
PORT_PCP=`expr $BASE_PORT + 3`
TEST_DIR=$TEST_DIR

rm -fr $TEST_DIR/pgpool.conf
cp $POOL_INSTALL_PATH/etc/pgpool.conf.sample-stream $TEST_DIR/pgpool.conf

cat >> $TEST_DIR/pgpool.conf <<'EOF'
port = __PORT_POOL__
pcp_port = __PORT_PCP__

backend_hostname0 = '127.0.0.1'
backend_port0 = __PORT_PRIMARY__
backend_weight0 = 0

backend_hostname1 = '127.0.0.1'
backend_port1 = __PORT_STANDBY__
backend_weight1 = 1

log_per_node_statement = on

pid_file_name = '/__TEST_DIR__/pgpool.pid'
write_function_list = 'currval,lastval,nextval,setval,insert_tb_f_func'
disable_load_balance_on_write = 'dml_adaptive'
dml_adaptive_object_relationship_list= 'tb_t1:tb_t2,insert_tb_f_func():tb_f,tb_v:tb_v_view'

sr_check_period = 0
sr_check_user = '__PG_USER__'
health_check_user = '__PG_USER__'
EOF

/bin/sed -i \
	 -e "/__PORT_PRIMARY__/s@__PORT_PRIMARY__@$PORT_PRIMARY@" \
	 -e "/__PORT_STANDBY__/s@__PORT_STANDBY__@$PORT_STANDBY@" \
	 -e "/__PORT_POOL__/s@__PORT_POOL__@$PORT_POOL@" \
	 -e "/__PORT_PCP__/s@__PORT_PCP__@$PORT_PCP@" \
	 -e "/__TEST_DIR__/s@__TEST_DIR__@$TEST_DIR@" \
	 -e "/__PG_USER__/s@__PG_USER__@$PG_USER@" \
	$TEST_DIR/pgpool.conf

	echo "done"
}

function start_pool()
{
	echo "start_pool ..."

	rm -rf /tmp/.s.PGSQL.110*
	$POOL_INSTALL_PATH/bin/pgpool -D -n -f $TEST_DIR/pgpool.conf 2>&1 | cat > $TEST_DIR/pgpool.log &

	echo "done"
}

function stop_pool()
{
	echo "stop_pool ..."

	$POOL_INSTALL_PATH/bin/pgpool -D -n -f $TEST_DIR/pgpool.conf stop 2>&1 | cat >> $TEST_DIR/pgpool.log &
	rm -rf /tmp/.s.PGSQL.110*

	echo "done"
}

function test_dml_adaptive()
{
echo "test_dml_adaptive ..."

PORT_PRIMARY=`expr $BASE_PORT + 0`
PORT_POOL=`expr $BASE_PORT + 2`

# Create test data
cat >> $TEST_DIR/create.sql <<'EOF'
-- for test dml
create table tb_dml_insert (a int);
create table tb_dml_update (a int);
create table tb_dml_delete (a int);

insert into tb_dml_insert values (1);
insert into tb_dml_update values (1);
insert into tb_dml_delete values (1);

-- for test trigger
create table tb_t1(a int);
create table tb_t2(b int);

CREATE OR REPLACE FUNCTION insert_tb_t2_func() RETURNS TRIGGER AS $example_table$
    BEGIN
        INSERT INTO tb_t2 VALUES (1);
        RETURN NEW;
    END;
$example_table$ LANGUAGE plpgsql;

CREATE TRIGGER insert_tb_t2_trigger AFTER INSERT ON tb_t1
FOR EACH ROW EXECUTE PROCEDURE insert_tb_t2_func();

-- for test function
create table tb_f (f int);

CREATE FUNCTION insert_tb_f_func(integer) RETURNS void AS $$
    INSERT INTO tb_f VALUES ($1);
$$ LANGUAGE SQL;

-- for test view
create table tb_v(v int);
CREATE VIEW tb_v_view AS SELECT * FROM tb_v;
EOF

# Test
cat >> $TEST_DIR/test.sql <<'EOF'
show pool_nodes;

\x
pgpool show disable_load_balance_on_write;
pgpool show dml_adaptive_object_relationship_list;
\x

-- test DML
begin ;
insert into tb_dml_insert values (1);
select * from tb_dml_insert ;
commit ;

begin ;
update tb_dml_update SET a = 2;
select * from tb_dml_update ;
commit ;

begin ;
delete from tb_dml_delete;
select * from tb_dml_delete;
commit ;

-- test trigger
begin ;
insert into tb_t1 values (1);
select * from tb_t2 ;
commit ;

-- test function
begin ;
select insert_tb_f_func(6);
select * from tb_f ;
commit ;

-- test view
begin ;
insert into tb_v values (8);
select * from tb_v_view ;
commit ;
EOF

# Drop test data
cat >> $TEST_DIR/drop.sql <<'EOF'
drop view tb_v_view ;
drop table tb_v;

drop function insert_tb_f_func;
drop table tb_f;

drop trigger insert_tb_t2_trigger ON tb_t1 ;
drop function insert_tb_t2_func;
drop table tb_t1;
drop table tb_t2;

drop table tb_dml_insert;
drop table tb_dml_update;
drop table tb_dml_delete;
EOF

$PG_INSTALL_PATH/bin/psql -h $HOST_IP -p $PORT_PRIMARY -U $PG_USER -d postgres -f $TEST_DIR/create.sql
$PG_INSTALL_PATH/bin/psql -h $HOST_IP -p $PORT_POOL -U $PG_USER -d postgres -f $TEST_DIR/test.sql
$PG_INSTALL_PATH/bin/psql -h $HOST_IP -p $PORT_PRIMARY -U $PG_USER -d postgres -f $TEST_DIR/drop.sql

echo "done"
}

function check_test_result()
{
	echo "check test result ..."

	# check "pgpool show disable_load_balance_on_write;"
	fgrep "disable_load_balance_on_write | " $LOG_FILE |grep "dml_adaptive">/dev/null 2>&1
	if [ $? != 0 ];then
	# expected result not found
		echo failed: pgpool show disable_load_balance_on_write.
		exit 1
	fi

	# check "pgpool show dml_adaptive_object_relationship_list;"
	fgrep "dml_adaptive_object_relationship_list |" $LOG_FILE |grep "tb_t1:tb_t2,insert_tb_f_func():tb_f,tb_v:tb_v_view">/dev/null 2>&1
	if [ $? != 0 ];then
	# expected result not found
		echo failed: pgpool show dml_adaptive_object_relationship_list.
		exit 1
	fi

	# check if dml adaptive worked
	fgrep "standby" $LOG_FILE |grep "true">/dev/null 2>&1
	if [ $? != 0 ];then
	# expected result not found
		echo failed: load_balance_node is not standby.
		exit 1
	fi
	fgrep "select * from tb_dml_insert ;" $TEST_DIR/pgpool.log |grep "DB node id: 0">/dev/null 2>&1
	if [ $? != 0 ];then
	# expected result not found
		echo failed: "\"select * from tb_dml_insert ;\"" is no sent to primary node.
		exit 1
	fi
	fgrep "select * from tb_dml_update ;" $TEST_DIR/pgpool.log |grep "DB node id: 0">/dev/null 2>&1
	if [ $? != 0 ];then
	# expected result not found
		echo failed: "\"select * from tb_dml_update ;\"" is no sent to primary node.
		exit 1
	fi
	fgrep "select * from tb_dml_delete;" $TEST_DIR/pgpool.log |grep "DB node id: 0">/dev/null 2>&1
	if [ $? != 0 ];then
	# expected result not found
		echo failed: "\"select * from tb_dml_delete;\"" is no sent to primary node.
		exit 1
	fi
	fgrep "select * from tb_t2 ;" $TEST_DIR/pgpool.log |grep "DB node id: 0">/dev/null 2>&1
	if [ $? != 0 ];then
	# expected result not found
		echo failed: "\"select * from tb_t2 ;\"" is no sent to primary node.
		exit 1
	fi
	fgrep "select * from tb_f ;" $TEST_DIR/pgpool.log |grep "DB node id: 0">/dev/null 2>&1
	if [ $? != 0 ];then
	# expected result not found
		echo failed: "\"select * from tb_f ;\"" is no sent to primary node.
		exit 1
	fi
	fgrep "select * from tb_v_view ;" $TEST_DIR/pgpool.log |grep "DB node id: 0">/dev/null 2>&1
	if [ $? != 0 ];then
	# expected result not found
		echo failed: "\"select * from tb_v_view ;\"" is no sent to primary node.
		exit 1
	fi

	echo "success: dml simple query test pass."
}

function install_temp_pgpool
{
	echo "creating pgpool-II temporary installation ..."

	POOL_INSTALL_PATH=$TEST_DIR/pgpool_temp_installed

	test -d $POOL_INSTALL_PATH || mkdir $POOL_INSTALL_PATH

	make install -C $BASE_DIR/../../../ -e prefix=${POOL_INSTALL_PATH} > ${POOL_INSTALL_PATH}/install.log 2>&1
	# make install -C $BASE_DIR/../../../ -e prefix=${POOL_INSTALL_PATH}

	if [ $? != 0 ];then
	    echo "pgpool make install failed"
	    exit 1
	fi

	echo "done"
}

function run_test()
{
	echo "run_test ..."

	rm -fr $TEST_DIR && mkdir $TEST_DIR && cd $TEST_DIR

	install_temp_pgpool
	create_streaming_replication
	set_pool_conf
	start_pool
	test_dml_adaptive

	stop_pool
	stop_pg_all

	echo "run_test done"
}

function print_usage
{
	printf "Usage:\n"
	printf "  %s [Options]...\n" $(basename $0) >&2
	printf "\nOptions:\n"
	printf "  -p   DIRECTORY           Postgres installed directory\n" >&2
	printf "  -h                       print this help and then exit\n\n" >&2
}

function main
{
	check_version

	echo "running test ..."
	run_test > $LOG_FILE 2>&1
	echo "test done"

	set +e
	check_test_result
}

# ------------------------------------------- main --------------------------------------------

while getopts "p:h" OPTION
do
case $OPTION in
	p)  PG_INSTALL_PATH="$OPTARG";;
	h)  print_usage
		exit 2;;
esac
done

main