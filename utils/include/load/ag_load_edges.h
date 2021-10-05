//
// Created by Shoaib on 10/5/2021.
//

<<<<<<< HEAD
#ifndef AG_LOAD_EDGES_H
#define AG_LOAD_EDGES_H

#endif //AG_LOAD_EDGES_H

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include "postgresql/libpq-fe.h"

// include <csv.h>
#include <csv.h>

#include "cypher_executor.h"


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
    char *start_vertex;
    char *end_vertex;
    char *edge_type;
} csv_edge_reader;


void edge_field_cb(void *field, size_t field_len, void *data);
void edge_row_cb(int delim __attribute__((unused)), void *data);

int create_edges_from_csv_file(char *file_path, char *graph_name,
                                char *edge_type, PGconn *conn );
=======
#ifndef INC_0001_PATCH_TO_CREATE_LABELS_FOR_VERTICES_AND_EDGES_PATCH_AG_LOAD_EDGES_H
#define INC_0001_PATCH_TO_CREATE_LABELS_FOR_VERTICES_AND_EDGES_PATCH_AG_LOAD_EDGES_H

#endif //INC_0001_PATCH_TO_CREATE_LABELS_FOR_VERTICES_AND_EDGES_PATCH_AG_LOAD_EDGES_H
>>>>>>> added code for edges
