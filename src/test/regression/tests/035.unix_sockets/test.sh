#!/usr/bin/env bash
#-------------------------------------------------------------------
# test script for unix_socket_directories, unix_socket_group and unix_socket_permissions.
# unix_socket_group test works if UNIX_SOCK_GROUP exists and the user running
# this test belongs to it. Therefore, we usually comment out it.
#
source $TESTLIBS
TESTDIR=testdir
PSQL=$PGBIN/psql

for mode in s
do
	rm -fr $TESTDIR
	mkdir $TESTDIR
	cd $TESTDIR

# create test environment
	echo -n "creating test environment..."
	$PGPOOL_SETUP -m $mode -n 2 || exit 1
	echo "done."

	source ./bashrc.ports
	dir=`pwd`
	UNIX_SOCK_PATH1=/tmp
	if [ -d $HOME/tmp ];then
	    UNIX_SOCK_PATH2=$HOME/tmp
	else
	    UNIX_SOCK_PATH2=$HOME
	fi
	UNIX_SOCK_FILE=.s.PGSQL.$PGPOOL_PORT
	UNIX_SOCK_GROUP=wheel
	USER=`whoami`

	echo "unix_socket_directories = '$UNIX_SOCK_PATH1,$UNIX_SOCK_PATH2'" >> etc/pgpool.conf
<<COMMENT_OUT
	echo "unix_socket_group = '$UNIX_SOCK_GROUP'" >> etc/pgpool.conf
COMMENT_OUT
	echo "unix_socket_permissions = 0770" >> etc/pgpool.conf

	./startall

	export PGPORT=$PGPOOL_PORT

	wait_for_pgpool_startup

	echo check: multiple unix domain sokets
	if [ ! -e $UNIX_SOCK_PATH1/$UNIX_SOCK_FILE ]; then
		echo "fail: not exist $UNIX_SOCK_PATH1/$UNIX_SOCK_FILE"
		./shutdownall
		exit 1
	fi

	if [ ! -e $UNIX_SOCK_PATH2/$UNIX_SOCK_FILE ]; then
		echo "fail: not exist $UNIX_SOCK_PATH2/$UNIX_SOCK_FILE"
		./shutdownall
		exit 1
	fi
	echo ok: multiple unix domain sockets

	echo check: connect to unix domain sockets
	res=`$PSQL -t -h $UNIX_SOCK_PATH1 test -c "SELECT 1"`
	if [ $res -ne 1 ]; then
		echo "fail: cannot connect to $UNIX_SOCK_PATH2/$UNIX_SOCK_FILE"
		./shutdownall
		exit 1
	fi

	res=`$PSQL -t -h $UNIX_SOCK_PATH1 test -c "SELECT 1"`
	if [ $res -ne 1 ]; then
		echo "fail: cannot connect to $UNIX_SOCK_PATH2/$UNIX_SOCK_FILE"
		./shutdownall
		exit 1
	fi
	echo ok: connect to unix domain sockets

<<COMMENT_OUT
	echo check: socket files owner group
	ls -la $UNIX_SOCK_PATH1 | grep "$USER $UNIX_SOCK_GROUP.*$UNIX_SOCK_FILE"
	if [ $? -ne 0 ]; then
		echo fail: owner group setting $UNIX_SOCK_PATH1
		./shutdownall
		exit 1
	fi

	ls -la $UNIX_SOCK_PATH2 | grep "$USER $UNIX_SOCK_GROUP.*$UNIX_SOCK_FILE"
	if [ $? -ne 0 ]; then
		echo fail: owner group setting $UNIX_SOCK_PATH2
		./shutdownall
		exit 1
	fi
	echo ok: socket files owner group
COMMENT_OUT

	echo check: socket files permission
	ls -la $UNIX_SOCK_PATH1 | grep "srwxrwx---.*$UNIX_SOCK_FILE"
	if [ $? -ne 0 ]; then
		echo fail: permission setting $UNIX_SOCK_PATH1
		./shutdownall
		exit 1
	fi

	ls -la $UNIX_SOCK_PATH2 | grep "srwxrwx---.*$UNIX_SOCK_FILE"
	if [ $? -ne 0 ]; then
		echo fail: permission setting $UNIX_SOCK_PATH2
		./shutdownall
		exit 1
	fi
	echo ok: socket files permission
	echo ok: unix_socket_directories and related parameters are working.

	./shutdownall

done
exit 0
