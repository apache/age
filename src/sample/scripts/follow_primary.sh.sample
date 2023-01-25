#!/bin/bash
# This script is run after failover_command to synchronize the Standby with the new Primary.
# First try pg_rewind. If pg_rewind failed, use pg_basebackup.

set -o xtrace

# Special values:
# 1)  %d = node id
# 2)  %h = hostname
# 3)  %p = port number
# 4)  %D = node database cluster path
# 5)  %m = new primary node id
# 6)  %H = new primary node hostname
# 7)  %M = old main node id
# 8)  %P = old primary node id
# 9)  %r = new primary port number
# 10) %R = new primary database cluster path
# 11) %N = old primary node hostname
# 12) %S = old primary node port number
# 13) %% = '%' character

NODE_ID="$1"
NODE_HOST="$2"
NODE_PORT="$3"
NODE_PGDATA="$4"
NEW_PRIMARY_NODE_ID="$5"
NEW_PRIMARY_NODE_HOST="$6"
OLD_MAIN_NODE_ID="$7"
OLD_PRIMARY_NODE_ID="$8"
NEW_PRIMARY_NODE_PORT="$9"
NEW_PRIMARY_NODE_PGDATA="${10}"

PGHOME=/usr/pgsql-15
ARCHIVEDIR=/var/lib/pgsql/archivedir
REPLUSER=repl
PCP_USER=pgpool
PGPOOL_PATH=/usr/bin
PCP_PORT=9898
REPL_SLOT_NAME=${NODE_HOST//[-.]/_}
POSTGRESQL_STARTUP_USER=postgres
SSH_KEY_FILE=id_rsa_pgpool
SSH_OPTIONS="-o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null -i ~/.ssh/${SSH_KEY_FILE}"

echo follow_primary.sh: start: Standby node ${NODE_ID}

# Check the connection status of Standby
${PGHOME}/bin/pg_isready -h ${NODE_HOST} -p ${NODE_PORT} > /dev/null 2>&1

if [ $? -ne 0 ]; then
    echo follow_primary.sh: node_id=${NODE_ID} is not running. skipping follow primary command
    exit 0
fi

# Test passwordless SSH
ssh -T ${SSH_OPTIONS} ${POSTGRESQL_STARTUP_USER}@${NEW_PRIMARY_NODE_HOST} ls /tmp > /dev/null

if [ $? -ne 0 ]; then
    echo follow_main.sh: passwordless SSH to ${POSTGRESQL_STARTUP_USER}@${NEW_PRIMARY_NODE_HOST} failed. Please setup passwordless SSH.
    exit 1
fi

# Get PostgreSQL major version
PGVERSION=`${PGHOME}/bin/initdb -V | awk '{print $3}' | sed 's/\..*//' | sed 's/\([0-9]*\)[a-zA-Z].*/\1/'`

if [ $PGVERSION -ge 12 ]; then
    RECOVERYCONF=${NODE_PGDATA}/myrecovery.conf
else
    RECOVERYCONF=${NODE_PGDATA}/recovery.conf
fi

# Synchronize Standby with the new Primary.
echo follow_primary.sh: pg_rewind for node ${NODE_ID}

# Run checkpoint command to update control file before running pg_rewind
${PGHOME}/bin/psql -h ${NEW_PRIMARY_NODE_HOST} -p ${NEW_PRIMARY_NODE_PORT} postgres -c "checkpoint;"

# Create replication slot "${REPL_SLOT_NAME}"
${PGHOME}/bin/psql -h ${NEW_PRIMARY_NODE_HOST} -p ${NEW_PRIMARY_NODE_PORT} postgres \
    -c "SELECT pg_create_physical_replication_slot('${REPL_SLOT_NAME}');"  >/dev/null 2>&1

if [ $? -ne 0 ]; then
    echo follow_primary.sh: create replication slot \"${REPL_SLOT_NAME}\" failed. You may need to create replication slot manually.
fi

ssh -T ${SSH_OPTIONS} ${POSTGRESQL_STARTUP_USER}@${NODE_HOST} "

    set -o errexit

    ${PGHOME}/bin/pg_ctl -w -m f -D ${NODE_PGDATA} stop

    ${PGHOME}/bin/pg_rewind -D ${NODE_PGDATA} --source-server=\"user=${POSTGRESQL_STARTUP_USER} host=${NEW_PRIMARY_NODE_HOST} port=${NEW_PRIMARY_NODE_PORT} dbname=postgres\"

    [ -d \"${NODE_PGDATA}\" ] && rm -rf ${NODE_PGDATA}/pg_replslot/*

    cat > ${RECOVERYCONF} << EOT
primary_conninfo = 'host=${NEW_PRIMARY_NODE_HOST} port=${NEW_PRIMARY_NODE_PORT} user=${REPLUSER} application_name=${NODE_HOST} passfile=''/var/lib/pgsql/.pgpass'''
recovery_target_timeline = 'latest'
restore_command = 'scp ${SSH_OPTIONS} ${NEW_PRIMARY_NODE_HOST}:${ARCHIVEDIR}/%f %p'
primary_slot_name = '${REPL_SLOT_NAME}'
EOT

    if [ ${PGVERSION} -ge 12 ]; then
        sed -i -e \"\\\$ainclude_if_exists = '$(echo ${RECOVERYCONF} | sed -e 's/\//\\\//g')'\" \
               -e \"/^include_if_exists = '$(echo ${RECOVERYCONF} | sed -e 's/\//\\\//g')'/d\" ${NODE_PGDATA}/postgresql.conf
        touch ${NODE_PGDATA}/standby.signal
    else
        echo \"standby_mode = 'on'\" >> ${RECOVERYCONF}
    fi

    ${PGHOME}/bin/pg_ctl -l /dev/null -w -D ${NODE_PGDATA} start

"

# If pg_rewind failed, try pg_basebackup 
if [ $? -ne 0 ]; then
    echo follow_primary.sh: end: pg_rewind failed. Try pg_basebackup.

    ssh -T ${SSH_OPTIONS} ${POSTGRESQL_STARTUP_USER}@${NODE_HOST} "

        set -o errexit

        [ -d \"${NODE_PGDATA}\" ] && rm -rf ${NODE_PGDATA}
        [ -d \"${ARCHIVEDIR}\" ] && rm -rf ${ARCHIVEDIR}/*
        ${PGHOME}/bin/pg_basebackup -h ${NEW_PRIMARY_NODE_HOST} -U $REPLUSER -p ${NEW_PRIMARY_NODE_PORT} -D ${NODE_PGDATA} -X stream

        cat > ${RECOVERYCONF} << EOT
primary_conninfo = 'host=${NEW_PRIMARY_NODE_HOST} port=${NEW_PRIMARY_NODE_PORT} user=${REPLUSER} application_name=${NODE_HOST} passfile=''/var/lib/pgsql/.pgpass'''
recovery_target_timeline = 'latest'
restore_command = 'scp ${SSH_OPTIONS} ${NEW_PRIMARY_NODE_HOST}:${ARCHIVEDIR}/%f %p'
primary_slot_name = '${REPL_SLOT_NAME}'
EOT

        if [ ${PGVERSION} -ge 12 ]; then
            sed -i -e \"\\\$ainclude_if_exists = '$(echo ${RECOVERYCONF} | sed -e 's/\//\\\//g')'\" \
                   -e \"/^include_if_exists = '$(echo ${RECOVERYCONF} | sed -e 's/\//\\\//g')'/d\" ${NODE_PGDATA}/postgresql.conf
            touch ${NODE_PGDATA}/standby.signal
        else
            echo \"standby_mode = 'on'\" >> ${RECOVERYCONF}
        fi
        sed -i \
            -e \"s/#*port = .*/port = ${NODE_PORT}/\" \
            -e \"s@#*archive_command = .*@archive_command = 'cp \\\"%p\\\" \\\"${ARCHIVEDIR}/%f\\\"'@\" \
            ${NODE_PGDATA}/postgresql.conf
    "

    if [ $? -ne 0 ]; then

        # drop replication slot
        ${PGHOME}/bin/psql -h ${NEW_PRIMARY_NODE_HOST} -p ${NEW_PRIMARY_NODE_PORT} postgres \
            -c "SELECT pg_drop_replication_slot('${REPL_SLOT_NAME}');"  >/dev/null 2>&1

        if [ $? -ne 0 ]; then
            echo ERROR: follow_primary.sh: drop replication slot \"${REPL_SLOT_NAME}\" failed. You may need to drop replication slot manually.
        fi

        echo follow_primary.sh: end: pg_basebackup failed
        exit 1
    fi

    # start Standby node on ${NODE_HOST}
    ssh -T ${SSH_OPTIONS} ${POSTGRESQL_STARTUP_USER}@${NODE_HOST} $PGHOME/bin/pg_ctl -l /dev/null -w -D ${NODE_PGDATA} start

fi

# If start Standby successfully, attach this node
if [ $? -eq 0 ]; then

    # Run pcp_attact_node to attach Standby node to Pgpool-II.
    ${PGPOOL_PATH}/pcp_attach_node -w -h localhost -U $PCP_USER -p ${PCP_PORT} -n ${NODE_ID}

    if [ $? -ne 0 ]; then
        echo ERROR: follow_primary.sh: end: pcp_attach_node failed
        exit 1
    fi

else

    # If start Standby failed, drop replication slot "${REPL_SLOT_NAME}"
    ${PGHOME}/bin/psql -h ${NEW_PRIMARY_NODE_HOST} -p ${NEW_PRIMARY_NODE_PORT} postgres \
        -c "SELECT pg_drop_replication_slot('${REPL_SLOT_NAME}');"  >/dev/null 2>&1

    if [ $? -ne 0 ]; then
        echo ERROR: follow_primary.sh: drop replication slot \"${REPL_SLOT_NAME}\" failed. You may need to drop replication slot manually.
    fi

    echo ERROR: follow_primary.sh: end: follow primary command failed
    exit 1
fi

echo follow_primary.sh: end: follow primary command is completed successfully
exit 0
