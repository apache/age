//
// Created by Shoaib on 9/14/2021.
//


#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include "postgresql/libpq-fe.h"


// include <csv.h>
#include <csv.h>

#define AGE_VERTIX 1
#define AGE_EDGE 2


struct counts {
    long unsigned fields;
    long unsigned allvalues;
    long unsigned rows;
};

typedef struct {
    size_t row;
    char **header;
    size_t *header_len;
    size_t header_num;
    char **fields;
    size_t *fields_len;
    size_t alloc;
    size_t cur_field;
    int error;
    size_t header_row_length;
    size_t curr_row_length;
    PGconn *conn;
    char *graph_name;
    char *object_name;
} csv_reader;


void init_cypher(PGconn *conn);
void create_label(PGconn *conn, char* label_name, int label_type);
void start_transaction(PGconn *conn);
void commit_transaction(PGconn *conn);
void rollback_transaction(PGconn *conn);
void field_cb(void *field, size_t field_len, void *data);
void row_cb(int delim __attribute__((unused)), void *data);

void execute_cypher(PGconn *conn, char* cypher_str,
                    char* graph_name, size_t cypher_size);

int parse_csv_file(char *file_path, char *graph_name,
                   char *object_name, PGconn *conn );

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

void field_cb(void *field, size_t field_len, void *data) {

    csv_reader *cr = (csv_reader*)data;
    if (cr->error)
        return;

    // check for space to store this field
    if (cr->cur_field == cr->alloc) {
        cr->alloc *= 2;
        cr->fields = realloc(cr->fields, sizeof(char *) * cr->alloc);
        cr->fields_len = realloc(cr->header, sizeof(size_t *) * cr->alloc);
        if (cr->fields == NULL) {
            fprintf(stderr, "field_cb: failed to reallocate %zu bytes",
                    sizeof(char *) * cr->alloc);
            perror(NULL);
            cr->error = 1;
            return;
        }
    }
    cr->fields_len[cr->cur_field] = field_len;
    cr->curr_row_length += field_len;
    cr->fields[cr->cur_field] = strndup((char*)field, field_len);
    cr->cur_field += 1;
}

// Parser calls this function when it detects end of a row
void row_cb(int delim __attribute__((unused)), void *data) {

    csv_reader *cr = (csv_reader*)data;

    size_t json_row_length = 0;
    char *json_row_str = NULL;
    char *cypher = NULL;
    size_t i, n_fields, cypher_length;

    printf("Row %zu:\n", cr->row);

    if (cr->row == 0) {
        cr->header_num = cr->cur_field;
        cr->header_row_length = cr->curr_row_length;
        cr->header_len = (size_t* )malloc(sizeof(size_t *) * cr->cur_field);
        cr->header = malloc((sizeof (char*) * cr->cur_field));

        for (size_t i = 0; i<cr->cur_field; i++) {
            cr->header_len[i] = cr->fields_len[i];
            cr->header[i] = strndup(cr->fields[i], cr->header_len[i]);
        }
    }


    json_row_length += cr->header_row_length + (6 * cr->header_num);
    json_row_length += cr->curr_row_length + (6 * cr->cur_field);
    json_row_length += 2;

    printf("%zu \n", json_row_length);

    json_row_str = malloc(sizeof (char ) * json_row_length);
    //cr->json_str = realloc(cr->json_str, sizeof(char) * cr->json_length);

    strcpy(json_row_str, "{");


    n_fields = cr->cur_field - 1;

    for (i = 0; i < n_fields; ++i) {
        printf("%s:", cr->header[i]);
        printf("%s\n", cr->fields[i]);
        strncat(json_row_str, cr->header[i], cr->header_len[i]);
        strcat(json_row_str, ":");
        strcat(json_row_str, "'");
        strncat(json_row_str, cr->fields[i], cr->fields_len[i]);
        strcat(json_row_str, "'");
        strcat(json_row_str, ",");
        free(cr->fields[i]);
    }

    strncat(json_row_str, cr->header[i], cr->header_len[i]);
    strcat(json_row_str, ":");
    strcat(json_row_str, "'");
    strncat(json_row_str, cr->fields[i], cr->fields_len[i]);
    strcat(json_row_str, "'");

    //printf("\n");

    strcat(json_row_str, "}");

    cypher_length = json_row_length + 60;
    cypher = (char *) malloc(sizeof (char) * cypher_length);
    strcpy(cypher, "CREATE (:");
    strcat(cypher, cr->object_name);
    strcat(cypher, json_row_str);
    strcat(cypher, ")");

    if (cr->row == 0) {
        free(json_row_str);
        free(cypher);
        if (cr->error)
            return;
        cr->cur_field = 0;
        cr->curr_row_length = 0;
        cr->row += 1;
        return;
    }
    execute_cypher(cr->conn,
                   cypher,
                   cr->graph_name,
                   cypher_length);
    free(json_row_str);
    free(cypher);
    if (cr->error)
        return;


    cr->cur_field = 0;
    cr->curr_row_length = 0;
    cr->row += 1;
}

static int is_space(unsigned char c) {
    if(c == CSV_SPACE || c == CSV_TAB) return 1;
    return 0;
}

/*
static int is_comma(unsigned char c) {
    if (c == '\t') return 1;
    return 0;
}
 */

static int is_term(unsigned char c) {
    if (c == CSV_CR || c == CSV_LF) return 1;
    return 0;
}

int parse_csv_file(char *file_path,
                   char *graph_name,
                   char *object_name,
                   PGconn *conn ) {

    FILE *fp;
    struct csv_parser p;
    char buf[1024];
    size_t bytes_read;
    unsigned char options = 0;
    csv_reader cr;

    if (csv_init(&p, options) != 0) {
        fprintf(stderr, "Failed to initialize csv parser\n");
        exit(EXIT_FAILURE);
    }

    csv_set_space_func(&p, is_space);
    csv_set_term_func(&p, is_term);

    fp = fopen(file_path, "rb");
    if (!fp) {
        fprintf(stderr, "Failed to open %s: %s\n", file_path, strerror(errno));
        exit(EXIT_FAILURE);;
    }


    memset((void*)&cr, 0, sizeof(csv_reader));
    cr.alloc = 128;
    cr.fields = malloc(sizeof(char *) * cr.alloc);
    cr.fields_len = malloc(sizeof(size_t *) * cr.alloc);
    cr.header_row_length = 0;
    cr.curr_row_length = 0;
    cr.conn = conn;
    cr.graph_name = graph_name;
    cr.object_name = object_name;

    while ((bytes_read=fread(buf, 1, 1024, fp)) > 0) {
        if (csv_parse(&p, buf, bytes_read, field_cb, row_cb, &cr) != bytes_read) {
            fprintf(stderr, "Error while parsing file: %s\n", csv_strerror(csv_error(&p)));
        }
    }

    csv_fini(&p, field_cb, row_cb, &cr);

    if (ferror(fp)) {
        fprintf(stderr, "Error while reading file %s\n", file_path);
        fclose(fp);
        free(cr.fields);
        csv_free(&p);
        exit(EXIT_FAILURE);
    }

    fclose(fp);
    // printf("%s: %lu fields, %lu rows\n", file_path, c.fields, c.rows);
    free(cr.fields);
    csv_free(&p);
    return EXIT_SUCCESS;
}

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
    status = parse_csv_file(file_path,
                                graph_name,
                                node_label,
                                conn);
    commit_transaction(conn);
    PQfinish(conn);
    exit(EXIT_SUCCESS);


}

