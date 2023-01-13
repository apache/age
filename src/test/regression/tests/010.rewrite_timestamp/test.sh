#!/usr/bin/env bash
#-------------------------------------------------------------------
# test script for unit test of rewriting timestamp queries.
# requires Ruby

cd timestamp
make clean
rm result.txt
make 
make test > result.txt 
cmp ../expected.txt result.txt
if [ $? != 0 ];then
	echo NG
	exit 1
fi
cd ..
 
echo OK 
exit 0
