

#include "graph_protocol_stddef.h"

#include "spec/bolt/message/v440/message.h"
#include "spec/bolt/message/v440/chunk.h"
#include "spec/bolt/message/v440/signature.h"
#include "spec/bolt/message/v440/tag.h"


#include "spec/packstream/v100/packstream_v100.h"
#include "spec/packstream/v100/coretypes.h"
#include "spec/packstream/v100/marker.h"
#include "spec/packstream/pack_utils.h"
#include "utils/utils.h"

#include "communication/socket_manager.h"
#include "stubs/stubs.h"
#include "spec/bolt/message/v440/tag.h"
#include "spec/bolt/message/message_utils.h"

#include "libwebsockets.h"
#include "libwebsocket/lws.h"

char *write_bolt_signature(BOLT_SIGNATURE bs) {
    char *result;
    result = malloc(BOLT_SIGNATURE_SIZE);
    result[0] = bs;
    return result;
}

char *write_bolt_tag(BOLT_TAG bt) {
    char *result;
    result = malloc(BOLT_TAG_SIZE);
    result[0] = bt;
    return result;
}


size_t extract_ws_cypher_string(char buf[], char **dest) {

    uint8 marker_byte = buf[4];
    size_t sizebyte_len;
    size_t size;

    union extract_by_union {
        uint8 c[8];
        uint16 s[4];
        uint32 i[2];
        uint64 d[1];
    } extract_size;

    if (PM_TINY_STRING == (marker_byte & NIBBLE_HIGH)) {
        sizebyte_len = 0;
        size = marker_byte & NIBBLE_LOW;

        *dest = (char *) malloc(sizebyte_len + 1);
        memcpy(*dest, &buf[6], sizebyte_len);
        (*dest)[sizebyte_len] = '\0';

    } else {
        switch (marker_byte) {
            case PM_STRING8:
                extract_size.c[0] = buf[5];
                sizebyte_len = extract_size.c[0];
                size = extract_size.c[0];

                *dest = (char *) malloc(sizebyte_len + 1);
                memcpy(*dest, &buf[6], sizebyte_len);
                (*dest)[sizebyte_len] = '\0';
                break;
            case PM_STRING16:
                extract_size.c[0] = buf[5];
                extract_size.c[1] = buf[6];
                size = extract_size.s[0];
                break;
            case PM_STRING32:
                extract_size.c[0] = buf[5];
                extract_size.c[1] = buf[6];
                extract_size.c[2] = buf[7];
                extract_size.c[3] = buf[8];
                size = extract_size.i[0];
                break;
            default:
                size = 0;
                perror("undefined size from socket");
                break;
        }
    }
    return size;
}

size_t extract_cypher_string(char buf[], char **dest) {

    uint8 marker_byte = buf[4];
    uint8 size_byte = buf[5];
    unsigned short *short_size;
    size_t size;

    if (PM_TINY_STRING == (marker_byte & NIBBLE_HIGH)) {
        size = marker_byte & NIBBLE_LOW;
    } else {
        switch (marker_byte) {
            case PM_STRING8:
                size = size_byte;
                break;
            case PM_STRING16:
                short_size = memcpy(short_size, &buf[5], 2);
                size = *short_size;
                break;
            default:
                size = 0;
                perror("undefined size from socket");
                break;
        }
    }
    *dest = (char *) malloc(size);
    memcpy(*dest, &buf[6], size);
    return size;
}


char *bolt_chunk_to_byte_array(BOLT_CHUNK *cnk) {

    size_t header = ntohs(cnk->header);
    char *byte_array = malloc(sizeof(cnk->header) + header + sizeof(cnk->ender));
    memcpy(&byte_array[0], &cnk->header, BOLT_CHUNK_HEADER_SIZE);
    memcpy(&byte_array[2], cnk->body, header);
    memcpy(&byte_array[2 + header], &cnk->ender, BOLT_CHUNK_HEADER_SIZE);
    return byte_array;
}

size_t extract_cnk_from_pgresult(const PGresult *r, char **dest_bytearray) {

    assert(r);

    ExecStatusType pg_exec_status = PQresultStatus(r);

    int tuple_count;
    int field_count;
    char *tuple_value;
    size_t tuple_value_len;

    char *pack_concat = NULL;
    size_t pack_concat_len = 0;

    char *cat = NULL;
    size_t cat_size = 0;
    size_t bytearray_size = 0;
    char *bytearray_record = NULL;

    BOLT_CHUNK **record_all;
    while (1) {
        switch (pg_exec_status) {
            case PGRES_COMMAND_OK:
            case PGRES_TUPLES_OK:
                tuple_count = PQntuples(r);
                field_count = PQnfields(r);
                record_all = malloc(sizeof(BOLT_CHUNK *) * tuple_count);
                break;
            case PGRES_EMPTY_QUERY:
            case PGRES_COPY_OUT:
            case PGRES_COPY_IN:
            case PGRES_BAD_RESPONSE:
            case PGRES_NONFATAL_ERROR:
            case PGRES_FATAL_ERROR:
            case PGRES_COPY_BOTH:
            case PGRES_SINGLE_TUPLE:
            case PGRES_PIPELINE_SYNC:
            case PGRES_PIPELINE_ABORTED:
                pg_exec_status = PQresultStatus(r);
                printf("pg_exec_status : %d\n", pg_exec_status);
                perror("pg_exec failed. exit.\n");
                exit(1);
        }
        break;
    }

    size_t bolt_chunk_size = 0;
    Oid field_oid;
    int nid = 0;
    struct node_info ni;

    for (int i = 0; i < tuple_count; ++i) {
        pack_concat = NULL;
        pack_concat_len = 0;
        pack_concat_len += pack_appender(pack_concat_len, &pack_concat, write_tiny_struct_marker(1));
        pack_concat_len += pack_appender(pack_concat_len, &pack_concat, write_bolt_signature(BS_RECORD));
        pack_concat_len += pack_appender(pack_concat_len, &pack_concat, write_tiny_list_marker(1));

        for (int j = 0; j < field_count; ++j) {
            tuple_value = PQgetvalue(r, i, j);
            tuple_value_len = strlen(tuple_value);
            field_oid = PQftype(r, j);

            switch (field_oid) {
                case VERTEXOID:
                    fprintf(stdout, "VERTEXOID\n");

                    //node structure
                    ni = node_string_to_structure(tuple_value);
                    printf("label:%s\nnid:%d\nprop:%s\n\n\n", ni.label, ni.nid, ni.properties);
                    pack_concat_len += pack_appender(pack_concat_len, &pack_concat, write_tiny_struct_marker(3));
                    pack_concat_len += pack_appender(pack_concat_len, &pack_concat, write_bolt_tag(BT_NODE));
                    pack_concat_len += pack_appender(pack_concat_len, &pack_concat, write_int16((int16) ni.nid));
                    pack_concat_len += pack_appender(pack_concat_len, &pack_concat, write_tiny_list_marker(1));
                    pack_concat_len += pack_appender(pack_concat_len, &pack_concat, write_string8(ni.label));

                    //property parse(temporal)
                    pack_concat_len += pack_appender(pack_concat_len, &pack_concat, write_tiny_dict_marker(1));
                    pack_concat_len += pack_appender(pack_concat_len, &pack_concat, write_tiny_string("test_key"));
                    pack_concat_len += pack_appender(pack_concat_len, &pack_concat, write_tiny_string("test_val"));
                    break;
                case GRAPHIDARRAYOID :
                    fprintf(stdout, "GRAPHIDARRAYOID\n");
                    break;
                case GRAPHIDOID:
                    fprintf(stdout, "GRAPHIDOID\n");
                    break;
                case VERTEXARRAYOID :
                    fprintf(stdout, "VERTEXARRAYOID\n");
                    break;
                case EDGEARRAYOID :
                    fprintf(stdout, "EDGEARRAYOID\n");
                    break;
                case EDGEOID :
                    fprintf(stdout, "EDGEOID\n");
                    break;
                case GRAPHPATHARRAYOID :
                    fprintf(stdout, "GRAPHPATHARRAYOID\n");
                    break;
                case GRAPHPATHOID:
                    fprintf(stdout, "GRAPHPATHOID\n");
                    break;
                case ROWIDARRAYOID :
                    fprintf(stdout, "ROWIDARRAYOID\n");
                    break;
                case ROWIDOID :
                    fprintf(stdout, "ROWIDOID\n");
                    break;
                default:
                    fprintf(stdout, "field_oid :::: %d\n", field_oid);
                    if (tuple_value_len > 15) {
                        pack_concat_len += pack_appender(pack_concat_len, &pack_concat, write_string8(tuple_value));
                    } else {
                        pack_concat_len += pack_appender(pack_concat_len, &pack_concat, write_tiny_string(tuple_value));
                    }
                    break;
            }
        }

        record_all[i] = malloc(sizeof(BOLT_CHUNK));
        record_all[i]->header = pack_concat_len;
        record_all[i]->header = htons(record_all[i]->header);
        record_all[i]->body = malloc(pack_concat_len);
        memcpy(record_all[i]->body, pack_concat, pack_concat_len);
        record_all[i]->ender = 0x0000;

        bytearray_record = bolt_chunk_to_byte_array(record_all[i]);
        bytearray_size = (pack_concat_len + BOLT_CHUNK_HEADER_SIZE + BOLT_CHUNK_ENDHEADER_SIZE);

        concaternating_byte_array(&cat, cat_size, bytearray_record, bytearray_size);
        cat_size += bytearray_size;
    }

    *dest_bytearray = cat;
    return cat_size;
}


/**
 * for example, libpq pg_exec output is such formed char*. <br>
 * movie[13.1]{"title": "The Matrix", "tagline": "Welcome to the Real World", "released": 1999} <br>
 * movie as label name. <br>
 * 13.1 as node id. <br>
 * { ... } as properties. <br>
 * @param libpq_node_str_arr
 */
struct node_info node_string_to_structure(char libpq_node_str_arr[]) {
    char *str1;
    char *token;
    char *saveptr1;
    const char *delim = "[]";
    struct node_info ni;


    str1 = libpq_node_str_arr;
    for (int j = 1;; j++, str1 = NULL) {
        token = strtok_r(str1, delim, &saveptr1);
        switch (j) {
            case 1:
                ni.label = token;
                break;
            case 2:
                (*strrchr(token, '.')) = '0';
                ni.nid = atoi(token);
                break;
            case 3:
                ni.properties = token;
                break;
        }
        if (token == NULL)
            break;
    }
    return ni;
}