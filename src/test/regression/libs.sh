#-------------------------------------------
# wait for pgpool comes up
#-------------------------------------------
function wait_for_pgpool_startup {
	timeout=20

	while [ $timeout -gt  0 ]
	do
		$PGBIN/psql -p $PGPOOL_PORT -c "show pool_nodes" test >/dev/null 2>&1
		if [ $? = 0 ];then
			break;
		fi
		timeout=`expr $timeout - 1`
		sleep 1
	done
}

#-------------------------------------------
# wait for primary/main failover done
#-------------------------------------------
function wait_for_failover_done {
	timeout=20

	while [ $timeout -gt  0 ]
	do
		$PGBIN/psql -p $PGPOOL_PORT -c "show pool_nodes" test >/dev/null 2>&1
		if [ $? = 0 ];then
		    $PGBIN/psql -p $PGPOOL_PORT -c "show pool_nodes" test |egrep -i "primary|main">/dev/null 2>&1
		    if [ $? = 0 ];then
			break;
		    fi
		fi
		timeout=`expr $timeout - 1`
		echo "timeout: $timeout"
		sleep 1
	done
}

#-------------------------------------------
# clean remaining processes and sockets
#-------------------------------------------
function clean_all {
	pgrep pgpool | xargs kill -9 > /dev/null 2>&1
	pgrep postgres | xargs kill -9 > /dev/null 2>&1
	rm -f $PGSOCKET_DIR/.s.PGSQL.*
}

#-------------------------------------------
# check segmentation fault
#-------------------------------------------
function check_segfault {
	egrep -i "segmentation fault|segfault" log/pgpool.log >/dev/null 2>&1
}
