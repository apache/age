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

#include "load/ag_load_edges.h"


void edge_field_cb(void *field, size_t field_len, void *data) {

    csv_edge_reader *cr = (csv_edge_reader*)data;
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
void edge_row_cb(int delim __attribute__((unused)), void *data) {

    csv_edge_reader *cr = (csv_edge_reader*)data;

    size_t json_row_length = 0;
    char *json_row_str = NULL;
    char *cypher = NULL;
    size_t i, n_fields, cypher_length;
    char *start_id;
    char *end_id;
    char *start_vertex_type;
    char *end_vertex_type;

    printf("Row %zu:\n", cr->row);

    if (cr->row == 0) {
        cr->header_num = cr->cur_field;
        cr->header_row_length = cr->curr_row_length;
        cr->header_len = (size_t* )malloc(sizeof(size_t *) * cr->cur_field);
        cr->header = malloc((sizeof (char*) * cr->cur_field));

        for ( i = 0; i<cr->cur_field; i++) {
            cr->header_len[i] = cr->fields_len[i];
            cr->header[i] = strndup(cr->fields[i], cr->header_len[i]);
        }
    }


    json_row_length += cr->header_row_length + (6 * cr->header_num);
    json_row_length += cr->curr_row_length + (6 * cr->cur_field);
    json_row_length += 2;

    json_row_str = malloc(sizeof(char) * json_row_length);
    strcpy(json_row_str, "{");

    n_fields = cr->cur_field - 1;
    start_id = strdup(cr->fields[0]);
    start_vertex_type = strdup(cr->fields[1]);
    end_id = strdup(cr->fields[2]);
    end_vertex_type = strdup(cr->fields[3]);

    for (i = 4; i < n_fields; ++i) {
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
    if (i == n_fields) {
        strncat(json_row_str, cr->header[i], cr->header_len[i]);
        strcat(json_row_str, ":");
        strcat(json_row_str, "'");
        strncat(json_row_str, cr->fields[i], cr->fields_len[i]);
        strcat(json_row_str, "'");
    }

    strcat(json_row_str, "}");

    cypher_length = json_row_length + 200;
    cypher = (char *) malloc(sizeof (char) * cypher_length);
    strcpy(cypher, "CREATE (:");
    strcat(cypher, start_vertex_type);
    strcat(cypher, " { id : \"");
    strcat(cypher, start_id);
    strcat(cypher, "\" })-[:");
    strcat(cypher, cr->edge_type);
    strcat(cypher, json_row_str);
    strcat(cypher, "]->(:");
    strcat(cypher, end_vertex_type);
    strcat(cypher, " { id : \"");
    strcat(cypher, end_id);
    strcat(cypher, "\" })");

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

int create_edges_from_csv_file(char *file_path,
                               char *graph_name,
                               char *edge_type,
                               PGconn *conn ) {

    FILE *fp;
    struct csv_parser p;
    char buf[1024];
    size_t bytes_read;
    unsigned char options = 0;
    csv_edge_reader cr;

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


    memset((void*)&cr, 0, sizeof(csv_edge_reader));
    cr.alloc = 128;
    cr.fields = malloc(sizeof(char *) * cr.alloc);
    cr.fields_len = malloc(sizeof(size_t *) * cr.alloc);
    cr.header_row_length = 0;
    cr.curr_row_length = 0;
    cr.conn = conn;
    cr.graph_name = graph_name;
    cr.edge_type = edge_type;

    while ((bytes_read=fread(buf, 1, 1024, fp)) > 0) {
        if (csv_parse(&p, buf, bytes_read, edge_field_cb, edge_row_cb, &cr) != bytes_read) {
            fprintf(stderr, "Error while parsing file: %s\n", csv_strerror(csv_error(&p)));
        }
    }

    csv_fini(&p, edge_field_cb, edge_row_cb, &cr);

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
