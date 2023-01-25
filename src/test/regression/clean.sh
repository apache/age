#! /bin/sh
# clean up test results
dir=`pwd`
export TESTLIBS=$dir/libs.sh
export PGPOOL_SETUP=$HOME/bin/pgpool_setup
export WATCHDOG_SETUP=$HOME/bin/watchdog_setup
log=$dir/log

rm -fr $log
rm -fr $dir/temp

cd tests
dirs=`ls`
for i in $dirs
do
	cd $i
	rm -fr testdir *~
	cd ..
done

rm -fr $dir/tests/004.watchdog/leader
rm -fr $dir/tests/004.watchdog/standby
cd $dir/tests/010.rewrite_timestamp/timestamp/; make clean >/dev/null 2>&1; cd $dir
rm -fr $dir/tests/011.watchdoc_quorum_failover/leader/
rm -fr $dir/tests/011.watchdoc_quorum_failover/standby/
rm -fr $dir/tests/011.watchdoc_quorum_failover/standby2/
rm -fr $dir/tests/012.watchdog_failover_when_quorum_exists/leader/
rm -fr $dir/tests/012.watchdog_failover_when_quorum_exists/standby/
rm -fr $dir/tests/012.watchdog_failover_when_quorum_exists/standby2/
rm -fr $dir/tests/013.watchdoc_test_failover_require_consensus/leader/
rm -fr $dir/tests/013.watchdoc_test_failover_require_consensus/standby/
rm -fr $dir/tests/013.watchdoc_test_failover_require_consensus/standby2/
rm -fr $dir/tests/014.watchdoc_test_quorum_bypass/leader/
rm -fr $dir/tests/015.watchdoc_test_master_and_backend_fail/leader/
rm -fr $dir/tests/015.watchdoc_test_master_and_backend_fail/standby/
rm -fr $dir/tests/015.watchdoc_test_master_and_backend_fail/standby2/
