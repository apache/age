#!/usr/bin/env bash
#-------------------------------------------------------------------
# test script for load balancing.
#
source $TESTLIBS
TESTDIR=testdir
PG_CTL=$PGBIN/pg_ctl
PSQL="$PGBIN/psql -X "

version=`$PSQL --version|awk '{print $3}'`
major_version=`echo ${version%.*} | sed 's/\([0-9]*\).*/\1/'`

result=`echo "$major_version >= 10"|bc`
if [ $result == 1 ];then
	REPLAY_PAUSE="SELECT pg_wal_replay_pause();"
	REPLAY_RESUME="SELECT pg_wal_replay_resume();"
else
	REPLAY_PAUSE="SELECT pg_xlog_replay_pause();"
	REPLAY_RESUME="SELECT pg_xlog_replay_resume();"
fi

for mode in s
do
	rm -fr $TESTDIR
	mkdir $TESTDIR
	cd $TESTDIR

	# create test environment
	echo -n "creating test environment..."
	$PGPOOL_SETUP -m $mode -n 3 || exit 1
	echo "done."

	source ./bashrc.ports
	echo "app_name_redirect_preference_list = 'psql:1'" >> etc/pgpool.conf
	echo "delay_threshold = 10" >> etc/pgpool.conf
	echo "prefer_lower_delay_standby = on" >> etc/pgpool.conf
	echo "sr_check_period = 3" >> etc/pgpool.conf

	./startall

	export PGPORT=$PGPOOL_PORT

	wait_for_pgpool_startup

	$PSQL test <<EOF
CREATE TABLE t1(i INTEGER);
CREATE TABLE t2(i INTEGER);
EOF

	echo start: prefer_lower_delay_standby is on.

	$PSQL -p 11003 test -c "$REPLAY_PAUSE"

	$PSQL test <<EOF
PGPOOL SET log_min_messages TO DEBUG1;
INSERT INTO t1 VALUES (1), (2), (3);
SELECT pg_sleep(4);
SELECT * FROM t1;
EOF

	fgrep "SELECT * FROM t1;" log/pgpool.log |grep "DB node id: 2">/dev/null 2>&1
	if [ $? != 0 ];then
	# expected result not found
		echo fail: query is sent to primary node.
		./shutdownall
		exit 1
	fi

	echo ok: query is sent to another standby node.

	$PSQL -p 11003 test -c "$REPLAY_RESUME"

	echo start: prefer_lower_delay_standby is off.

	echo "prefer_lower_delay_standby = off" >> etc/pgpool.conf

	$PGPOOL_INSTALL_DIR/bin/pcp_reload_config -w -h localhost -p $PCP_PORT

	while :
	do
		$PSQL test -c "PGPOOL SHOW prefer_lower_delay_standby" |grep off
		if [ $? = 0 ]; then
			break
		fi
		sleep 1
	done

	$PSQL -p 11003 test -c "$REPLAY_PAUSE"

	$PSQL test <<EOF
PGPOOL SET log_min_messages TO DEBUG1;
INSERT INTO t2 VALUES (1), (2), (3);
SELECT pg_sleep(4);
SELECT * FROM t2;
EOF

	fgrep "SELECT * FROM t2;" log/pgpool.log |grep "DB node id: 0">/dev/null 2>&1
	if [ $? != 0 ];then
	# expected result not found
		echo fail: query is sent to standby node.
		./shutdownall
		exit 1
	fi

	echo ok: prefer lower delay standby works.

	./shutdownall

done
exit 0
