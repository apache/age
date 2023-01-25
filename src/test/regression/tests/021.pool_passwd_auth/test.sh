#!/usr/bin/env bash
#-------------------------------------------------------------------
# test script for scram and md5 authentication
# where password is stored in pool_passwd file
#

# This test is only valid with PostgreSQL 10 or later.
if [ $PGVERSION -le 9 ];then
    exit 0
fi

source $TESTLIBS
TESTDIR=testdir
PSQL=$PGBIN/psql
CREATEUSER=$PGBIN/createuser
PG_ENC=$PGPOOL_INSTALL_DIR/bin/pg_enc

rm -fr $TESTDIR
mkdir $TESTDIR
cd $TESTDIR

# create test environment
echo -n "creating test environment..."
$PGPOOL_SETUP -m s -n 2 || exit 1
echo "done."

source ./bashrc.ports

#echo "enable_pool_hba = on" >> etc/pgpool.conf
echo "allow_clear_text_frontend_auth = off" >> etc/pgpool.conf
#set max_init_children to 1 to make sure we reuse the
#connection to test wrong password rejection
echo "num_init_children = 1" >> etc/pgpool.conf
echo "enable_pool_hba = on" >> etc/pgpool.conf

# create pgpoolkey file
echo "pgpool secret key" > etc/pgpool_key
chmod 0600 etc/pgpool_key
export PGPOOLKEYFILE=$PWD/etc/pgpool_key

#create pool_passwd file using AES256 encrypted password
#echo "scram_user:scram_password" >> etc/pool_passwd
#echo "md5_user:md5_password" >> etc/pool_passwd
$PG_ENC -m -f etc/pgpool.conf -u scram_user scram_password
$PG_ENC -m -f etc/pgpool.conf -u md5_user md5_password

#copy the pool_hba to etc dir
cp ../pool_hba.conf etc/
./startall

export PGPORT=$PGPOOL_PORT
wait_for_pgpool_startup

#set passwords
$PSQL -c "SET password_encryption = 'scram-sha-256'; CREATE ROLE scram_user PASSWORD 'scram_password'; ALTER ROLE scram_user WITH LOGIN;" test
$PSQL -c "SET password_encryption = 'md5'; CREATE ROLE md5_user PASSWORD 'md5_password'; ALTER ROLE md5_user WITH LOGIN" test
$PSQL -c "CREATE ROLE trust_user PASSWORD 'trust_password';ALTER ROLE trust_user WITH LOGIN" test


if $PSQL -c "SELECT rolname,substring(rolpassword, 1, 14) FROM pg_authid where rolname ='scram_user'" test|grep "SCRAM-SHA-256" >/dev/null 2>&1
then
    echo "scram_user authenticated setup successfully"
else
    echo "scram authentication setup failed"
    failed=1
fi
if $PSQL -c "SELECT rolname,substring(rolpassword, 1, 4) FROM pg_authid where rolname ='md5_user'" test|grep "md5" >/dev/null 2>&1
then
    echo "md5 authenticated setup successfully"
else
    echo "md5 authentication setup failed"
    failed=1
fi
#create pgpass file
echo "127.0.0.1:$PGPORT:test:scram_user:scram_password" >> pgpass
echo "127.0.0.1:$PGPORT:test:md5_user:md5_password" >> pgpass
chmod 0600 pgpass
echo "127.0.0.1:$PGPORT:test:scram_user:scram_wrong_password" >> pgpasswrong
echo "127.0.0.1:$PGPORT:test:md5_user:md5_wrong_password" >> pgpasswrong
chmod 0600 pgpasswrong


export PGPASSFILE=$PWD/pgpass

if $PSQL -h 127.0.0.1 -U scram_user -c "SELECT user, inet_client_addr();" test|grep "scram_user" >/dev/null 2>&1
then
    echo "scram_user authenticated successfully"
else
    echo "scram authentication failed"
    failed=1
fi

if $PSQL -h 127.0.0.1 -U md5_user -c "SELECT user, inet_client_addr();" test|grep "md5_user" >/dev/null 2>&1
then
    echo "md5_user authenticated successfully"
else
    echo "md5 authentication failed"
    failed=1
fi
#now try using the wrong passwords1:
export PGPASSFILE=$PWD/pgpasswrong
echo "Trying wrong passwords now"
#try connect using scram_user and md5 user
if $PSQL -h 127.0.0.1 -U scram_user -c "SELECT user, inet_client_addr();" test|grep "scram_user" >/dev/null 2>&1
then
    failed=1
    echo "serious problem with scram authentication, password not verified by pgpool"
else
    echo "wrong password rejected properly for scram auth"
fi

if $PSQL -h 127.0.0.1 -U md5_user -c "SELECT user, inet_client_addr();" test|grep "md5_user" >/dev/null 2>&1
then
    failed=1
    echo "serious problem with md5 authentication, password not verified by pgpool"
else
    echo "wrong password rejected properly for md5 auth"
fi

./shutdownall
if [ -z "$failed" ]; then
    exit 0
fi

exit 1
