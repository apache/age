//
// Created by Shoaib on 10/3/2021.
//

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include "postgresql/libpq-fe.h"

// include <csv.h>
#include <csv.h>

#include "load/ag_load_labels.h"
<<<<<<< HEAD
#include "load/ag_load_edges.h"
=======
>>>>>>> code refactered


int main(int argc, char** argv) {

    char *host_name = NULL;
    char *port_number = NULL;
    char *user_id = NULL;
    char *user_pwd = NULL;
    char *db_name = NULL;
    char *graph_name = NULL;
    char *node_label = NULL;
    char *file_path = NULL;
    int node_edge_flag = 0;

    PGconn     *conn;

    int opt;
    int lib_ver;
    int status;

    while((opt = getopt(argc, argv, "h:p:u:w:Wd:g:ven:f:")) != -1)
    {
        switch(opt)
        {
            case 'h':
                host_name = optarg;
                break;
            case 'p':
                port_number = optarg;
                break;
            case 'u':
                user_id = optarg;
                break;
            case 'w':
                user_pwd = optarg;
                break;
            case 'd':
                db_name = optarg;
                break;
            case 'g':
                graph_name = optarg;
                break;
            case 'v':
                node_edge_flag = AGE_VERTIX;
                break;
            case 'e':
                node_edge_flag = AGE_EDGE;
                break;
            case 'n':
                node_label = optarg;
                break;
            case 'f':
                file_path = optarg;
                break;
            default:
                exit(EXIT_FAILURE);
        }
    }

    if (host_name == NULL) {
        host_name = "localhost";
    }

    if (port_number == NULL) {
        port_number = "5432";
    }

    if (db_name == NULL) {
        db_name = "postgres";
    }

    if (graph_name == NULL) {
        printf("Please provide graph name\n");
        exit(EXIT_FAILURE);
    }

    if (file_path == NULL) {
        printf("Please provide file path\n");
        exit(EXIT_FAILURE);
    }

    if (node_label == NULL) {
        printf("Please provide node label\n");
        exit(EXIT_FAILURE);
    }



    printf("Hello, World!\n");
    printf("total number of arguments are: %d\n", argc);

    printf("Graph: %s\n", graph_name);
    printf("File Path: %s\n", file_path);


    lib_ver = PQlibVersion();
    printf("Using LIBPQ Version: %d\n", lib_ver);

    conn = PQsetdbLogin(host_name,
                        port_number,
                        NULL,
                        NULL,
                        db_name,
                        user_id,
                        user_pwd);

    if (PQstatus(conn) != CONNECTION_OK)
    {
        fprintf(stderr, "Connection to database failed: %s",
                PQerrorMessage(conn));
        PQfinish(conn);
        exit(EXIT_FAILURE);
    }

    init_cypher(conn);

    /*
    start_transaction(conn);
    char* cypher = "CREATE (:Person {name: 'Shoaib', title: 'Developer'})";
    execute_cypher(conn, cypher, "graph", 100);
    rollback_transaction(conn);
    */
    start_transaction(conn);
<<<<<<< HEAD
    if (node_edge_flag == AGE_VERTIX) {
        status = create_labels_from_csv_file(file_path,
                                             graph_name,
                                             node_label,
                                             conn);
    }
    else if (node_edge_flag == AGE_EDGE) {
        status = create_edges_from_csv_file(file_path,
                                             graph_name,
                                             node_label,
                                             conn);
    }
=======
    status = parse_csv_file(file_path,
                                graph_name,
                                node_label,
                                conn);
>>>>>>> code refactered
    commit_transaction(conn);
    PQfinish(conn);
    exit(EXIT_SUCCESS);


}