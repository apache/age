//
// Created by Shoaib on 10/3/2021.
//

#ifndef AG_LOAD_LABELS_H
#define AG_LOAD_LABELS_H

#endif //AG_LOAD_LABELS_H

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
