/*
 * sender.c
 *
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 *
 * IDENTIFICATION
 *	  src/communication/ag/sender.c
 */


#include "interface/pgwired_facade.h"
#include "graph_protocol_stddef.h"


#include <libpq-fe.h>
#include <assert.h>

void pq_result_check(PGresult *result) {
    ExecStatusType s;
    s = PQresultStatus(result);
    switch (s) {
        case PGRES_TUPLES_OK:
            fprintf(stdout, "%s", "\ntuples ok\n");
            break;
        case PGRES_SINGLE_TUPLE:
        case PGRES_EMPTY_QUERY:
        case PGRES_COMMAND_OK:
        case PGRES_COPY_OUT:
        case PGRES_COPY_IN:
        case PGRES_BAD_RESPONSE:
        case PGRES_NONFATAL_ERROR:
        case PGRES_FATAL_ERROR:
        case PGRES_COPY_BOTH:
        case PGRES_PIPELINE_SYNC:
        case PGRES_PIPELINE_ABORTED:
            fprintf(stdout, "%s", PQresultErrorMessage(result));
            fprintf(stdout, "%s", PQresStatus(s));
        default:
            break;
    }
}

void pq_connection_check(PGconn *conn) {
    ConnStatusType status_type = PQstatus(conn);
    switch (status_type) {
        case CONNECTION_OK:
            break;
        case CONNECTION_BAD:
            perror("ConnStatusType is CONNECTION_BAD");
            exit(1);
        case CONNECTION_STARTED:
            break;
        case CONNECTION_MADE:
            break;
        case CONNECTION_AWAITING_RESPONSE:
            break;
        case CONNECTION_AUTH_OK:
            break;
        case CONNECTION_SETENV:
            break;
        case CONNECTION_SSL_STARTUP:
            break;
        case CONNECTION_NEEDED:
            break;
        case CONNECTION_CHECK_WRITABLE:
            break;
        case CONNECTION_CONSUME:
            break;
        case CONNECTION_GSS_STARTUP:
            break;
        case CONNECTION_CHECK_TARGET:
            break;
        case CONNECTION_CHECK_STANDBY:
            break;
    }
}

PGconn *connect_ag(const char *conn_str) {

    printf("%s", conn_str);
    if (conn_str == NULL) {
        perror("conn_str is null");
    }

    PGconn *conn = PQconnectdb(conn_str);
    assert(conn);
    return conn;
}

PGresult *execute_cypher(PGconn *conn, char *cypher) {
    PGresult *result = PQexec(conn, cypher);
    assert(result);
    return result;
}

PGresult *exec_cypher(PGconn *conn, char *cypher_str) {
    PGresult *result;
//    conn = connect_ag(conn_str);
    pq_connection_check(conn);
    result = execute_cypher(conn, cypher_str);
    pq_result_check(result);
    return result;
}