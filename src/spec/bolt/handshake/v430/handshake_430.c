/*
 * handshake_4_3_0.c 
 * 
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
 *
 * IDENTIFICATION
 *	  src/spec/handshake/handshake_4_3_0.c
 */

#include "graph_protocol_stddef.h"

#include "spec/bolt/handshake/v430/handshake_v430.h"
#include "communication/socket_manager.h"
#include "spec/bolt/message/v440/message.h"
#include "spec/bolt/message/v440/chunk.h"
#include "stubs/stubs.h"


void bytestream_handshake(int *data_len, int *new_socket, char *buffer) {

//    *data_len = recv_data(new_socket, buffer);
//
//    bolt_server_identification bolt_server_ident;
//    bolt_server_ident = bolt_handshake(new_socket, buffer);
//
//    *data_len = send_data(new_socket, bolt_server_ident.avail_version,
//                          sizeof(bolt_server_identification));
}

char server_available_ver[4] = {0x00, 0x02, 0x04, 0x04};


size_t version_neogotiation(int *client_socket) {
    size_t data_len;

    data_len = send_data(client_socket, server_available_ver, 4);
    return data_len;
}

void simple_handshake(int listen_fd, int new_socket,
                      struct sockaddr_in address) {

    //socket var
    int addrlen = sizeof(address);
    char buffer[TCP_BUF_SIZE] = {0};
    int data_len;


    //chunking var
    BOLT_CHUNK cnk;
    char *src;
    char *cat;
    size_t src_len = 0;
    size_t cat_len = 0;

    data_len = version_neogotiation(&new_socket);

    //recv bolt:hello
    data_len = recv_data(&new_socket, buffer);

    //send bolt:hello-success
    cnk = bolt_hello_success("", 0);
    src = bolt_chunk_to_byte_array(&cnk);
    src_len = ntohs(cnk.header) + BOLT_CHUNK_HEADER_SIZE * 2;
    cat_len = concaternating_byte_array(&cat, cat_len, src, src_len);
    data_len = send_data(&new_socket, cat, cat_len);

    data_len = recv_data(&new_socket, buffer);

    char route_success[] = {
            0x00, 0x91, 0xb1, 0x70, 0xa1, 0x82, 0x72, 0x74, 0xa3, 0x87, 0x73, 0x65, 0x72, 0x76, 0x65, 0x72, 0x73, 0x93,
            0xa2, 0x89, 0x61, 0x64, 0x64, 0x72, 0x65, 0x73, 0x73, 0x65, 0x73, 0x91, 0x8e, 0x6c, 0x6f, 0x63, 0x61, 0x6c,
            0x68, 0x6f, 0x73, 0x74, 0x3a, 0x37, 0x36, 0x38, 0x37, 0x84, 0x72, 0x6f, 0x6c, 0x65, 0x85, 0x57, 0x52, 0x49,
            0x54, 0x45, 0xa2, 0x89, 0x61, 0x64, 0x64, 0x72, 0x65, 0x73, 0x73, 0x65, 0x73, 0x91, 0x8e, 0x6c, 0x6f, 0x63,
            0x61, 0x6c, 0x68, 0x6f, 0x73, 0x74, 0x3a, 0x37, 0x36, 0x38, 0x37, 0x84, 0x72, 0x6f, 0x6c, 0x65, 0x84, 0x52,
            0x45, 0x41, 0x44, 0xa2, 0x89, 0x61, 0x64, 0x64, 0x72, 0x65, 0x73, 0x73, 0x65, 0x73, 0x91, 0x8e, 0x6c, 0x6f,
            0x63, 0x61, 0x6c, 0x68, 0x6f, 0x73, 0x74, 0x3a, 0x37, 0x36, 0x38, 0x37, 0x84, 0x72, 0x6f, 0x6c, 0x65, 0x85,
            0x52, 0x4f, 0x55, 0x54, 0x45, 0x83, 0x74, 0x74, 0x6c, 0xc9, 0x01, 0x2c, 0x82, 0x64, 0x62, 0x85, 0x6e, 0x65,
            0x6f, 0x34, 0x6a, 0x00, 0x00
    };
    data_len = send_data(&new_socket, route_success, 149);

    data_len = recv_data(&new_socket, buffer);

    char reset_success[] = {
            0x00, 0x03, 0xb1, 0x70, 0xa0, 0x00, 0x00
    };
    data_len = send_data(&new_socket, reset_success, 7);


    //routed cluster, handshake again.
    shutdown(new_socket, 3);

    accept_sock(&new_socket, &listen_fd, &address, addrlen);
    data_len = recv_data(&new_socket, buffer);
    data_len = send_data(&new_socket, server_available_ver, 4);


    data_len = recv_data(&new_socket, buffer);

    // neo4j auth success
    cat_len = 0;
    cnk = bolt_hello_success("", 0);
    src = bolt_chunk_to_byte_array(&cnk);
    src_len = ntohs(cnk.header) + BOLT_CHUNK_HEADER_SIZE * 2;
    cat_len = concaternating_byte_array(&cat, cat_len, src, src_len);
    data_len = send_data(&new_socket, cat, cat_len);

//    data_len = send_data(&new_socket, hello_n4j_success_default_auth, 71);
    data_len = recv_data(&new_socket, buffer);



    //chunk-bolt:run-success(call db.ping~~)
    cat_len = 0;
    cnk = bolt_run_dbping_success("", 0);
    src = bolt_chunk_to_byte_array(&cnk);
    src_len = ntohs(cnk.header) + BOLT_CHUNK_HEADER_SIZE * 2;
    cat_len = concaternating_byte_array(&cat, cat_len, src, src_len);

    //chunk-bolt:record(call db.ping~~)
    cnk = bolt_run_dbping_record("", 0);
    src = bolt_chunk_to_byte_array(&cnk);
    src_len = ntohs(cnk.header) + BOLT_CHUNK_HEADER_SIZE * 2;
    cat_len = concaternating_byte_array(&cat, cat_len, src, src_len);

    //chunk-bolt:success(bookmark~~)
    cnk = bolt_run_dbping_bookmark_success(NULL, 0);
    src = bolt_chunk_to_byte_array(&cnk);
    src_len = ntohs(cnk.header) + BOLT_CHUNK_HEADER_SIZE * 2;
    cat_len = concaternating_byte_array(&cat, cat_len, src, src_len);

    data_len = send_data(&new_socket, cat, cat_len);

    cat_len = 0;

    // resetting conn

    data_len = recv_data(&new_socket, buffer);

    struct STUB s = stub_finder(SUCCESS);
    data_len = send_data(&new_socket, s.data, s.len);

    // C: RUN { .. }
    // C: PULL { ..}
    // pipelined client msg
    data_len = recv_data(&new_socket, buffer);


    char *cypher_query;
    size_t cypher_query_len;
    cypher_query_len = extract_cypher_string(buffer, &cypher_query);

    PGconn *conn = connect_ag("");
    PGresult *pg_result = exec_cypher(conn, cypher_query);
    char *pgresult_bytearray;

    int field_count = PQnfields(pg_result);
    char **field_names = malloc(sizeof(char *) * field_count);

    for (int i = 0; i < field_count; ++i) {
        char *fname = PQfname(pg_result, i);
        field_names[i] = malloc(strlen(fname) + 1);//with null
        field_names[i] = memcpy(field_names[i], fname, strlen(fname) + 1);
    }


    //
    src_len = 0;
    cat_len = 0;

    //run-success
    cnk = bolt_run_success(field_names, field_count);
    src = bolt_chunk_to_byte_array(&cnk);
    src_len = ntohs(cnk.header) + BOLT_CHUNK_HEADER_SIZE * 2;
    cat_len = concaternating_byte_array(&cat, cat_len, src, src_len);

    //record
    src_len = extract_cnk_from_pgresult(pg_result, &pgresult_bytearray);
    cat_len = concaternating_byte_array(&cat, cat_len, pgresult_bytearray, src_len);

    //pull-success
    cnk = bolt_pull_success(NULL, 0);
    src = bolt_chunk_to_byte_array(&cnk);
    src_len = ntohs(cnk.header) + BOLT_CHUNK_HEADER_SIZE * 2;
    cat_len = concaternating_byte_array(&cat, cat_len, src, src_len);


    data_len = send_data(&new_socket, cat, cat_len);
}
