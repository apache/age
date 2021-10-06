//
// Created by Shoaib on 10/3/2021.
//

#ifndef AG_LOAD_LABELS_H
#define AG_LOAD_LABELS_H


#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include "postgresql/libpq-fe.h"

// include <csv.h>
#include <csv.h>

#include "cypher_executor.h"

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
} csv_vertex_reader;


void vertex_field_cb(void *field, size_t field_len, void *data);
void vertex_row_cb(int delim __attribute__((unused)), void *data);

int create_labels_from_csv_file(char *file_path, char *graph_name,
                   char *object_name, PGconn *conn );


void create_label(PGconn *conn, char* label_name, int label_type);


void vertex_field_cb(void *field, size_t field_len, void *data);
void vertex_row_cb(int delim __attribute__((unused)), void *data);

int create_labels_from_csv_file(char *file_path, char *graph_name,
                   char *object_name, PGconn *conn );
#endif //AG_LOAD_LABELS_H