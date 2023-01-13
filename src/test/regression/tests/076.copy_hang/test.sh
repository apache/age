#!/usr/bin/env bash
#-------------------------------------------------------------------
# test script for copy plus error case.
# It was reported that following sequece of copy command cause psql hang.
#
# CREATE TEMP TABLE vistest (a text);  
# COPY vistest FROM stdin CSV FREEZE;
# Enter data to be copied followed by a newline.
# End with a backslash and a period on a line by itself, or an EOF signal.
# >> p
# >> g
# >> \.
#
# In the normal case an error should be returned to psql.
#ERROR:  cannot perform COPY FREEZE because the table was not created or truncated in the current subtransaction

source $TESTLIBS
TESTDIR=testdir
PSQL=$PGBIN/psql

rm -fr $TESTDIR
mkdir $TESTDIR
cd $TESTDIR

# create test environment
echo -n "creating test environment..."
$PGPOOL_SETUP -n 3 || exit 1
echo "done."

source ./bashrc.ports

export PGPORT=$PGPOOL_PORT

./startall
wait_for_pgpool_startup

# execute COPY

timeout 10 $PSQL test <<EOF
CREATE TEMP TABLE vistest (a text);  
COPY vistest FROM stdin CSV FREEZE;
p
g
\\.
\\q
EOF

if [ ! $? -eq 0 ];then
    echo ...timed out.
    ./shutdownall
    exit 1
fi
echo ...ok.
./shutdownall
