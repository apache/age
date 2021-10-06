//
// Created by Shoaib on 10/3/2021.
//

#include "cypher_executor.h"

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

    res = PQexec(conn, "SET search_path = ag_catalog, \"$user\", public;");
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
    strcpy(cypher, "SELECT * FROM cypher( ");
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

    res = PQexec(conn, "SE");
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
