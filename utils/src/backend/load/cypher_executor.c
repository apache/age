/*
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
 */

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include "postgresql/libpq-fe.h"
#include <csv.h>

#include "load/cypher_executor.h"

void init_cypher(PGconn *conn) {
    PGresult *res;
    res = PQexec(conn, "LOAD 'age';");
    printf("Executed %s\n", PQcmdStatus(res));

    if (PQresultStatus(res) != PGRES_COMMAND_OK)
    {
        fprintf(stderr, "LOAD failed: %s", PQerrorMessage(conn));
        PQclear(res);
        PQfinish(conn);
        exit(EXIT_FAILURE);
    }

    res = PQexec(conn, "SET search_path TO ag_catalog;");
    printf("Executed %s\n", PQcmdStatus(res));

    if (PQresultStatus(res) != PGRES_COMMAND_OK)
    {
        fprintf(stderr, "SET failed: %s", PQerrorMessage(conn));
        PQclear(res);
        PQfinish(conn);
        exit(EXIT_FAILURE);
    }
    PQclear(res);
}

void start_transaction(PGconn *conn) {
    PGresult *res;
    res = PQexec(conn, "BEGIN;");
    printf("Executed %s\n", PQcmdStatus(res));

    if (PQresultStatus(res) != PGRES_COMMAND_OK)
    {
        fprintf(stderr, "BEGIN failed: %s", PQerrorMessage(conn));
        PQclear(res);
        PQfinish(conn);
        exit(EXIT_FAILURE);
    }
    PQclear(res);
}

void commit_transaction(PGconn *conn) {
    PGresult *res;
    res = PQexec(conn, "COMMIT;");
    printf("Executed %s\n", PQcmdStatus(res));

    if (PQresultStatus(res) != PGRES_COMMAND_OK)
    {
        fprintf(stderr, "BEGIN failed: %s", PQerrorMessage(conn));
        PQclear(res);
        PQfinish(conn);
        exit(EXIT_FAILURE);
    }
    PQclear(res);
}

void rollback_transaction(PGconn *conn) {
    PGresult *res;
    res = PQexec(conn, "ROLLBACK;");
    printf("Executed %s\n", PQcmdStatus(res));

    if (PQresultStatus(res) != PGRES_COMMAND_OK)
    {
        fprintf(stderr, "BEGIN failed: %s", PQerrorMessage(conn));
        PQclear(res);
        PQfinish(conn);
        exit(EXIT_FAILURE);
    }
    PQclear(res);
}

void execute_cypher(PGconn *conn,
                    char* cypher_str,
                    char* graph_name,
                    size_t cypher_size) {

    PGresult *res;

    char *cypher = (char *) malloc(sizeof (char) * (cypher_size + 60));
    strcpy(cypher, "SELECT * FROM ag_catalog.cypher( ");
    strcat(cypher, "'");
    strcat(cypher, graph_name);
    strcat(cypher, "', $$ ");
    strcat(cypher, cypher_str);
    strcat(cypher, " $$) as (n agtype);");

    res = PQexec(conn, cypher);

    printf("%s \n", cypher);

    printf("Executed %s\n", PQcmdStatus(res));

    if (PQresultStatus(res) != PGRES_TUPLES_OK)
    {
        fprintf(stderr, "SELECT failed: %s", PQerrorMessage(conn));
        PQclear(res);
        rollback_transaction(conn);
        PQfinish(conn);
        exit(EXIT_FAILURE);
    }
    PQclear(res);
}

void create_label(PGconn *conn, char* label_name, int label_type) {

    PGresult *res;

    char *query = (char *) malloc(sizeof (char) * 100);

    strcpy(query, "SELECT ");

    if (label_type == AGE_VERTIX)
        strcat(query, "create_vlabel('");
    if(label_type == AGE_EDGE)
        strcat(query, "create_elabel('");
    strcat(query, label_name);
    strcat(query, "'); ");

    res = PQexec(conn, query);
    printf("Executed %s\n", PQcmdStatus(res));

    if (PQresultStatus(res) != PGRES_COMMAND_OK)
    {
        fprintf(stderr, "SET failed: %s", PQerrorMessage(conn));
        PQclear(res);
        PQfinish(conn);
        exit(EXIT_FAILURE);
    }
    PQclear(res);
}
