#!/usr/bin/env bash
#-------------------------------------------------------------------
# test script for database_redirect_preference_list and app_name_redirect_preference_list.
# requires pgbench.
#
source $TESTLIBS
TESTDIR=testdir
PSQL=$PGBIN/psql
CREATEDB=$PGBIN/createdb
PGBENCH=$PGBENCH_PATH

function getnode()
{
	grep $1 log/pgpool.log | grep SELECT | grep LOG | awk '{print $10}'
}

rm -fr $TESTDIR
mkdir $TESTDIR
cd $TESTDIR

# create test environment
echo -n "creating test environment..."
$PGPOOL_SETUP -m s -n 3 || exit 1
echo "done."

source ./bashrc.ports

echo "database_redirect_preference_list = 'postgres:primary,test:1,mydb[5-9]:2,test2:standby,test3:primary(0.0),test4:standby(0.0),test5:primary(1.0)'" >> etc/pgpool.conf

# disable delay_threshold so that replication delay does not affect the tests.
echo "delay_threshold = 0" >> etc/pgpool.conf

./startall

export PGPORT=$PGPOOL_PORT
wait_for_pgpool_startup

$PSQL -c "show pool_nodes" postgres

$CREATEDB mydb6
$CREATEDB test2
$CREATEDB test3
$CREATEDB test4
$CREATEDB test5

# check to see if all databases have been replicated

for p in 3 4
do
    # set standby port
    myport=`expr $PGPOOL_PORT + $p`

    for r in 1 2 3 4 5
    do
	ok=true
	for i in mydb6 test2 test3 test4 test5
	do
	    echo "try to connect to $i:$myport"
	    $PSQL -p $myport -c "select 1" $i
	    if [ $? != 0 ];then
		ok=false
		break
	    fi
	done
	if [ $ok = "false" ];then
	    sleep 1
	else
	    break
	fi
    done
done

$PGBENCH -i postgres

ok=yes

# should be redirect to primary (node 0)
$PSQL -c "SELECT 'test1'" postgres

test `getnode "'test1'"` -eq 0 || ok=ng
echo $ok

# should be redirect to node 1
$PSQL -c "SELECT 'test2'" test

test `getnode "'test2'"` -eq 1 || ok=ng
echo $ok

# should be redirect to node 2
$PSQL -c "SELECT 'test3'" mydb6

test `getnode "'test3'"` -eq 2 || ok=ng

echo $ok
# should be redirect to either node 1 or 2
$PSQL -c "SELECT 'test4'" test2

test `getnode "'test4'"` -eq 1 -o `getnode "test4"` -eq 2 || ok=ng
echo $ok

# should be redirect to either node 1 or 2
$PSQL -c "SELECT 'test5'" test3

test `getnode "'test5'"` -eq 1 -o `getnode "test5"` -eq 2 || ok=ng
echo $ok

# should be redirect to primary (node 0)
$PSQL -c "SELECT 'test6'" test4

test `getnode "'test6'"` -eq 0 || ok=ng
echo $ok

# should be redirect to primary (node 0)
$PSQL -c "SELECT 'test7'" test5

test `getnode "'test7'"` -eq 0 || ok=ng

echo $ok

echo "app_name_redirect_preference_list = 'psql:primary,pgbench:standby'" >> etc/pgpool.conf

./pgpool_reload
sleep 10

wait_for_pgpool_startup

# should be redirect to node 0 because application name is psql
$PSQL -c "SELECT 'test8'" mydb6

test `getnode "'test8'"` -eq 0 || ok=ng

echo $ok
# should be redirect to either node 1 or 2
$PGBENCH -t 1 -f ../select.pgbench postgres

test `getnode "'test9'"` -eq 1 -o `getnode "test9"` -eq 2 || ok=ng
echo $ok


echo "app_name_redirect_preference_list = 'psql:primary(0.0),pgbench:standby(1.0)'" >> etc/pgpool.conf

./pgpool_reload
sleep 10

wait_for_pgpool_startup

# should be redirect to either node 1 or 2
$PSQL -c "SELECT 'test10'" mydb6

test `getnode "'test10'"` -eq 1 -o `getnode "test10"` -eq 2 || ok=ng
echo $ok

# should be redirect to either node 1 or 2
$PGBENCH -t 1 -f ../select1.pgbench postgres

test `getnode "'test11'"` -eq 1 -o `getnode "test11"` -eq 2 || ok=ng
echo $ok

./shutdownall

if [ $ok = "yes" ];then
	exit 0
fi
exit 1
