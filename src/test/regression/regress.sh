#!/usr/bin/env bash
#
# pgpool-II regression test driver.
#
# usage: regress.sh [test_name]
# -i install directory of pgpool
# -b pgbench path
# -p installation path of Postgres
# -j JDBC driver path
# -m install (install pgpool-II and use that for tests) / noinstall : Default install
# -s unix socket directory
# -c test pgpool using sample scripts and config files
# -d start pgpool with debug option

dir=`pwd`
MODE=install
PG_INSTALL_DIR=/usr/local/pgsql/bin
PGPOOL_PATH=/usr/local
JDBC_DRIVER=/usr/local/pgsql/share/postgresql-9.2-1003.jdbc4.jar
#export USE_REPLICATION_SLOT=true
export log=$dir/log
fail=0
ok=0
timeout=0
PGSOCKET_DIR=/tmp

CRED=$(tput setaf 1)
CGREEN=$(tput setaf 2)
CBLUE=$(tput setaf 4)
CNORM=$(tput sgr0)

# timeout for each test (5 min.)
TIMEOUT=300

function install_pgpool
{
	echo "creating pgpool-II temporary installation ..."
        PGPOOL_PATH=$dir/temp/installed

	test -d $log || mkdir $log
        
	make install WATCHDOG_DEBUG=1 -C $dir/../../ -e prefix=${PGPOOL_PATH} >& regression.log 2>&1

	if [ $? != 0 ];then
	    echo "make install failed"
	    exit 1
	fi
	
	echo "moving pgpool_setup to temporary installation path ..."
        cp $dir/../pgpool_setup ${PGPOOL_PATH}/pgpool_setup
	export PGPOOL_SETUP=$PGPOOL_PATH/pgpool_setup
	echo "moving watchdog_setup to temporary installation path ..."
        cp $dir/../watchdog_setup ${PGPOOL_PATH}/watchdog_setup
	export WATCHDOG_SETUP=$PGPOOL_PATH/watchdog_setup
}

function verify_pginstallation
{
	# PostgreSQL bin directory
	PGBIN=`$PG_INSTALL_DIR/pg_config --bindir`
	if [ -z $PGBIN ]; then
		echo "$0: cannot locate pg_config"
		exit 1
	fi

	# PostgreSQL lib directory
	PGLIB=`$PG_INSTALL_DIR/pg_config --libdir`
	if [ -z $PGLIB ]; then
		echo "$0: cannot locate pg_config"
		exit 1
	fi
}

function export_env_vars
{
	if [[ -z "$PGPOOL_PATH" ]]; then
		# check if pgpool is in the path
		PGPOOL_PATH=/usr/local
		export PGPOOL_SETUP=$HOME/bin/pgpool_setup
		export WATCHDOG_SETUP=$HOME/bin/watchdog_setup
 	fi
	
	if [[ -z "$PGBENCH_PATH" ]]; then
		if [ -x $PGBIN/pgbench ]; then
			PGBENCH_PATH=$PGBIN/pgbench
		else
			PGBENCH_PATH=`which pgbench`
		fi
	fi

	if [ ! -x $PGBENCH_PATH ]; then
		echo "$0] cannot locate pgbench"; exit 1
 	fi
	
	echo "using pgpool-II at "$PGPOOL_PATH

	export PGPOOL_VERSION=`$PGPOOL_PATH/bin/pgpool --version 2>&1`

	export PGPOOL_INSTALL_DIR=$PGPOOL_PATH
	# where to look for pgpool.conf.sample files.
	export PGPOOLDIR=${PGPOOLDIR:-"$PGPOOL_INSTALL_DIR/etc"}

	PGPOOLLIB=${PGPOOL_INSTALL_DIR}/lib
	if [ -z "$LD_LIBRARY_PATH" ];then
	    export LD_LIBRARY_PATH=${PGPOOLLIB}:${PGLIB}
	else
	    export LD_LIBRARY_PATH=${PGPOOLLIB}:${PGLIB}:${LD_LIBRARY_PATH}
	fi

	export TESTLIBS=$dir/libs.sh
	export PGBIN=$PGBIN
	export JDBC_DRIVER=$JDBC_DRIVER
	export PGBENCH_PATH=$PGBENCH_PATH
	export PGSOCKET_DIR=$PGSOCKET_DIR
	export PGVERSION=`$PGBIN/initdb -V|awk '{print $3}'|sed 's/\..*//'`
	export LANG=C

	export ENABLE_TEST=true
}
function print_info
{
	echo ${CBLUE}"*************************"${CNORM}

	echo "REGRESSION MODE          : "${CBLUE}$MODE${CNORM}
	echo "Pgpool-II version        : "${CBLUE}$PGPOOL_VERSION${CNORM}
	echo "Pgpool-II install path   : "${CBLUE}$PGPOOL_PATH${CNORM}
	echo "PostgreSQL bin           : "${CBLUE}$PGBIN${CNORM}
	echo "PostgreSQL Major version : "${CBLUE}$PGVERSION${CNORM}
	echo "pgbench                  : "${CBLUE}$PGBENCH_PATH${CNORM}
	echo "PostgreSQL jdbc          : "${CBLUE}$JDBC_DRIVER${CNORM}
	echo ${CBLUE}"*************************"${CNORM}
}

function print_usage
{
	printf "Usage:\n"
	printf "  %s: [Options]... [test_name]\n" $(basename $0) >&2
	printf "\nOptions:\n"
	printf "  -p   DIRECTORY           Postgres installed directory\n" >&2
	printf "  -b   PATH                pgbench installed path, if different from Postgres installed directory\n" >&2
	printf "  -i   DIRECTORY           pgpool installed directory, if already installed pgpool is to be used for tests\n" >&2
	printf "  -m   install/noinstall   make install pgpool to temp directory for executing regression tests [Default: install]\n" >&2
	printf "  -j   FILE                Postgres jdbc jar file path\n" >&2
	printf "  -s   DIRECTORY           unix socket directory\n" >&2
	printf "  -t   TIMEOUT             timeout value for each test (sec)\n" >&2
	printf "  -c                       test pgpool using sample scripts and config files\n" >&2
	printf "  -d                       start pgpool with debug option\n" >&2
	printf "  -?                       print this help and then exit\n\n" >&2
	printf "Please read the README for details on adding new tests\n" >&2

}

trap "echo ; exit 0" SIGINT SIGQUIT

while getopts "p:m:i:j:b:s:t:cd?" OPTION
do
  case $OPTION in
    p)  PG_INSTALL_DIR="$OPTARG";;
    m)  MODE="$OPTARG";;
    i)  PGPOOL_PATH="$OPTARG";;
    j)  JDBC_DRIVER="$OPTARG";;
    b)  PGBENCH_PATH="$OPTARG";;
    s)  PGSOCKET_DIR="$OPTARG";;
    t)  TIMEOUT="$OPTARG";;
    c)  export TEST_SAMPLES="true";;
    d)  export PGPOOLDEBUG="true";;
    ?)  print_usage
        exit 2;;
  esac
done

shift $(($OPTIND - 1))
if [ "$MODE" = "install" ]; then
	install_pgpool

elif [ "$MODE" = "noinstall" ]; then
	echo not installing pgpool for the tests ...
	if [[ -n "$PGPOOL_INSTALL_DIR" ]]; then
		PGPOOL_PATH=$PGPOOL_INSTALL_DIR
	fi
        export PGPOOL_SETUP=$PGPOOL_PATH/bin/pgpool_setup
        export WATCHDOG_SETUP=$PGPOOL_PATH/bin/watchdog_setup
else
	echo $MODE : Invalid mode
	exit -1
fi 

verify_pginstallation
export_env_vars
print_info
source $TESTLIBS

#Start executing tests
rm -fr $log
mkdir $log

cd tests

if [ $# -eq 1 ];then
	dirs=`ls|grep $1`
else
	dirs=`ls`
fi

for i in $dirs
do
	cd $i
	echo -n "testing $i..."
	clean_all
	timeout $TIMEOUT ./test.sh > $log/$i 2>&1
	rtn=$?

	check_segfault
	if [ $? -eq 0 ];then
		echo "fail: Segmentation fault detected" >> $log/$i
		rtn=1
	fi

	if [ $rtn = 0 ];then
		echo ${CGREEN}"ok."${CNORM}
		ok=`expr $ok + 1`
	elif [ $rtn = 124 ];then
		echo "timeout."
		timeout=`expr $timeout + 1`
	else
		echo ${CRED}"failed."${CNORM}
		fail=`expr $fail + 1`
	fi

	cd ..

done

total=`expr $ok + $fail`

echo "out of $total ok:$ok failed:$fail timeout:$timeout"
