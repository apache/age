#!/usr/bin/env bash
#-------------------------------------------------------------------
# Testing memory leak in extended query protocol case. To detect the
# memory leak, we perform pgbench -S for sometime and see how much the
# pgpool child process is growing by using ps command.

PGBENCH=$PGBENCH_PATH
WHOAMI=`whoami`
source $TESTLIBS
TESTDIR=testdir

for mode in s r n
do
	rm -fr $TESTDIR
	mkdir $TESTDIR
	cd $TESTDIR

	# create test environment
	echo -n "creating test environment..."
	$PGPOOL_SETUP -m $mode -n 2 || exit 1
	echo "done."

	source ./bashrc.ports

	export PGPORT=$PGPOOL_PORT

	# set pgpool number of child to 1
	echo "num_init_children = 1" >> etc/pgpool.conf

	# start pgpool-II
	./startall

	sleep 1

	# initialize tables
	$PGBENCH -i test

	$PGBENCH -S -T 1 test
	
	# find pgpool-II child process id and grab initial process size (virtual size)
	foo=`ps x|grep "pgpool: wait for connection request"`
	pid=`echo $foo|awk '{print $1}'`
	init_size=`ps l $pid|tail -1|awk '{print $7}'`
	echo "init_size: $init_size"

	# run pgbench for a while
	$PGBENCH -M extended -S -T 30 test

	after_size=`ps l $pid|tail -1|awk '{print $7}'`
	delta=`expr $after_size - $init_size`

	echo "initial process size: $init_size after size: $after_size delta: $delta"

	test $delta -eq 0

	if [ $? != 0 ];then
		echo "memory leak in $delta KB in mode:$mode"
		./shutdownall
		exit 1
	fi

	./shutdownall

	cd ..
done

exit 0
