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


#include "load/ag_load_labels.h"

void vertex_field_cb(void *field, size_t field_len, void *data) {

    csv_vertex_reader *cr = (csv_vertex_reader *) data;

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
    cr->fields[cr->cur_field] = strndup((char *) field, field_len);
    cr->cur_field += 1;
}

void vertex_row_cb(int delim __attribute__((unused)), void *data) {

    csv_vertex_reader *cr = (csv_vertex_reader*)data;

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
int create_labels_from_csv_file(char *file_path,
                                char *graph_name,
                                char *object_name,
                                PGconn *conn ) {

    FILE *fp;
    struct csv_parser p;
    char buf[1024];
    size_t bytes_read;
    unsigned char options = 0;
    csv_vertex_reader cr;

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


    memset((void*)&cr, 0, sizeof(csv_vertex_reader));

    cr.alloc = 128;
    cr.fields = malloc(sizeof(char *) * cr.alloc);
    cr.fields_len = malloc(sizeof(size_t *) * cr.alloc);
    cr.header_row_length = 0;
    cr.curr_row_length = 0;
    cr.conn = conn;
    cr.graph_name = graph_name;
    cr.object_name = object_name;

    while ((bytes_read=fread(buf, 1, 1024, fp)) > 0) {
        if (csv_parse(&p, buf, bytes_read, vertex_field_cb, vertex_row_cb, &cr) != bytes_read) {
            fprintf(stderr, "Error while parsing file: %s\n", csv_strerror(csv_error(&p)));
        }
    }

    csv_fini(&p, vertex_field_cb, vertex_row_cb, &cr);

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