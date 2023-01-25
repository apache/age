#!/usr/bin/env bash
#-------------------------------------------------------------------
# test script for enable_consensus_with_half_votes
#
source $TESTLIBS
TESTDIR=testdir
PSQL=$PGBIN/psql

# set enable_consensus_with_half_votes to on or off depending on the $val arg.
function set_param
{
    n=0
    while [ $n -lt $nodes ]
    do
	echo "enable_consensus_with_half_votes = $val" >> pgpool$n/etc/pgpool.conf
	n=`expr $n + 1`
    done
}

# wait for watchdog starting up by looking for "lifecheck started" in
# the pgpool.log.  argument: $log: absolute path to the pgpool.log.
function wait_for_watchdog_startup
{
    while :
    do
	grep "lifecheck started" $log >/dev/null
	if [ $? = 0 ];then
	    break;
	fi
	sleep 1
    done
}

# return 0 if quorum exists.
function quorum_exists
{
    $PGPOOL_INSTALL_DIR/bin/pcp_watchdog_info -v -w -h localhost -p $PCP_PORT >/dev/null 2>&1
    if [ $? != 0 ];then
	echo "pcp_watchdog_info is not still available"
	for i in 1 2 3 4 5
	do
	    $PGPOOL_INSTALL_DIR/bin/pcp_watchdog_info -v -w -h localhost -p $PCP_PORT >/dev/null 2>&1
	    if [ $? = 0 ];then
		break;
	    fi
	    echo "waiting for pcp_watchdog_info becomes available"
	    sleep 1
	done
    fi

    $PGPOOL_INSTALL_DIR/bin/pcp_watchdog_info -v -w -h localhost -p $PCP_PORT |grep QUORUM|egrep 'EXIST|EDGE'>/dev/null
}

# shutdown half of nodes for even total number of nodes.
# shutdown (n + 1)/2 nodes for even number of total nodes n.
# args: nodes: number of total nodes
function shutdown_nodes
{
    cdir=`pwd`
    n=`expr \( $nodes + 1 \) / 2`
    while [ $n -gt 0 ]
    do
	cd $dir/$TESTDIR/pgpool$n
	echo "shutdown node pgpool$n"
	./shutdownall
	sleep 1
	n=`expr $n - 1`
    done
    cd $cdir
}

# return 0 if nodes is even
function nodes_even
{
    test `expr $nodes % 2` = 0
}

#-----------------------------------------------------------------==
dir=`pwd`

failed=false
#export CHECK_TIME_WAIT=true
export PCPPASSFILE=$dir/$TESTDIR/pgpool0/pcppass

for nodes in 2 3 4
do
    cd $dir
    rm -fr $TESTDIR
    mkdir $TESTDIR
    cd $TESTDIR

    $WATCHDOG_SETUP -wn $nodes || exit 1

    val="off"
    set_param

    ./startall

    cd pgpool0
    source ./bashrc.ports
    export PGPORT=$PGPOOL_PORT
    cd ..

    echo -n "waiting for watchdog node 0 starting up... "
    log=$dir/$TESTDIR/pgpool0/log/pgpool.log
    wait_for_watchdog_startup $log
    echo "done."

    echo "=== Testing total nodes: $nodes. enable_consensus_with_half_of_the_votes: $val ==="
    quorum_exists
    shutdown_nodes

    sleep 5

    if quorum_exists
    then
	echo "Quorum exists. Test failed"
	failed=true
    else
	echo "Quorum does not exist. Test succeeded"
    fi

    ./shutdownall
    sleep 1
    val="on"
    set_param
    ./startall
    wait_for_watchdog_startup $log

    echo "Testing total nodes: $nodes. enable_consensus_with_half_of_the_votes: $val"
    shutdown_nodes

    sleep 5

    if quorum_exists
    then
	if nodes_even
	then
	    echo "Quorum exists. Test succeeded"
	else
	    echo "Quorum exists. Test failed"
	    failed=true
	fi
    else
	if nodes_even
	then
	    echo "Quorum does not exist. Test failed"
	    failed=true
	else
	    echo "Quorum does not exist. Test succeeded"
	fi
    fi

    ./shutdownall
    sleep 10
done

if [ $failed = "true" ];then
    exit 1
fi
exit 0
