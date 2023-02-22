/*
 * agbolt_ws_hook.c 
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
 *	  src/communication/agbolt_ws_hook.c
 */

#include "graph_protocol_stddef.h"

#include "communication/agbolt_ws_hook.h"

#include "interface/pgwired_facade.h"
#include "utils/utils.h"
#include "utils/config_helper.h"

#include "spec/bolt/handshake/v430/handshake_v430.h"
#include "spec/bolt/message/v440/signature.h"
#include "spec/bolt/message/v440/message.h"
#include "spec/bolt/message/v440/chunk.h"
#include "spec/bolt/message/v440/tag.h"

#include "stubs/stubs.h"

#include "libwebsocket/lws.h"


extern struct garph_protocol_conf garph_protocol_conf;


extern int in_transaction;

uint8 server_ver[] = {0x00, 0x02, 0x04, 0x04};
uint8 ident[] = {0x60, 0x60, 0xb0, 0x17};


struct msg *mock_match_matrix_return_result(struct pg_conn *conn, const struct msg *m);

struct msg *mallocating_msg_with_length(uint8 *raw, size_t len);

struct msg *ws_cypher_parser(struct pg_conn *conn, const struct msg *m) {

    BOLT_CHUNK cnk;
    char *src;
    char *cat;
    size_t src_len = 0;
    size_t cat_len = 0;

    src_len = 0;
    cat_len = 0;

    size_t extracted;
    char *extracted_cypher;
    extracted = extract_ws_cypher_string(m->payload, &extracted_cypher);

    char *cypher = concaternating_default_graphpath(garph_protocol_conf.graph_path, extracted_cypher);
    assert(conn);

    PGresult *pg_result = exec_cypher(conn, cypher);
    assert(pg_result);

    char *pgresult_bytearray;
    int field_count = PQnfields(pg_result);
    char **field_names = malloc(sizeof(char *) * field_count);

    printf("field: %d", field_count);

    for (int i = 0; i < field_count; ++i) {
        char *fname = PQfname(pg_result, i);
        field_names[i] = malloc(strlen(fname) + 1);//with null
        field_names[i] = memcpy(field_names[i], fname, strlen(fname) + 1);
    }

    //commit-success
    if (in_transaction == 1) {
        cnk = bolt_commit_success();
        src = bolt_chunk_to_byte_array(&cnk);
        src_len = ntohs(cnk.header) + BOLT_CHUNK_HEADER_SIZE * 2;
        cat_len = concaternating_byte_array(&cat, cat_len, src, src_len);
    }

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

    assert(cat);

    return mallocating_msg_with_length(cat, cat_len);
}

struct msg *agbolt_ws_hook(struct pg_conn *conn, const struct msg *m) {
    assert(conn); // conn assertion

    struct msg *result_msg;

    if (((char *) (m->payload))[3] == 0x10) // run
    {
        switch (m->len) {
            case 74:// bolt-RUN:CALL dbms.components() YIELD name, versions, edition + bolt-PULL: n
                result_msg = stub_lws_msg_maker(CALL_DBMS_COMPONENTS);
                break;
            case 35: // SHOW DATABASES.......=
                result_msg = stub_lws_msg_maker(SHOW_DATABASES);
                break;
            case 49: // bolt-RUN: CALL dbms.showCurrentUser() + bolt-PULL: n
                result_msg = stub_lws_msg_maker(SHOW_CURRENT_USER);
                break;
            case 536: // COLLECT(label)[..1000]} AS result.UNION ALL.CALL db.relationshi ....
            case 44: // ......CALL dbms.procedures()
            case 43: //  .....CALL dbms.functions()
                result_msg = stub_lws_msg_maker(SUCCESS);
                break;
            default:
                result_msg = ws_cypher_parser(conn, m);
                break;
        }
        return result_msg;
    }

    switch (m->len) {

        case 77: // explain.. match(n) where n.title = 'The Matrix' return(n);
            perror("unintended explain query.");
            return stub_lws_msg_maker(SUCCESS);
        case 70: //match(n) where n.title = 'The Matrix' return(n);

            return stub_lws_msg_maker(SHOW_CURRENT_USER);
        case 63: // begin
            return stub_lws_msg_maker(SUCCESS);
        case 65: // begin
            return stub_lws_msg_maker(SUCCESS);

        case 75: // begin
            return stub_lws_msg_maker(SUCCESS);
        case 35:
            return stub_lws_msg_maker(SHOW_DATABASES);
        case 20:
            return ws_handshake();
        case 6:
            if (((char *) (m->payload))[3] == 0x12) {
                return stub_lws_msg_maker(COMMIT);
            }
            return stub_lws_msg_maker(SUCCESS);
        case 103:
        case 104:
        case 135:
        case 136:
        case 137:
            return ws_hello();
        case 32:
            return ws_route();
        case 74:// bolt-RUN:CALL dbms.components() YIELD name, versions, edition + bolt-PULL: n
            return stub_lws_msg_maker(CALL_DBMS_COMPONENTS);
        case 68:
            return stub_lws_msg_maker(SUCCESS);
        default:
            perror("non of case detected");
            return stub_lws_msg_maker(SUCCESS);
    }
}


struct msg *ws_route() {
    BOLT_CHUNK cnk;
    char *src;
    char *cat;
    size_t src_len = 0;
    size_t cat_len = 0;
    cnk = bolt_route_success("", 0);
    src = bolt_chunk_to_byte_array(&cnk);
    src_len = ntohs(cnk.header) + BOLT_CHUNK_HEADER_SIZE * 2;
    cat_len = concaternating_byte_array(&cat, cat_len, src, src_len);
    return mallocating_msg_with_length(cat, cat_len);
}

struct msg *ws_hello() {
    BOLT_CHUNK cnk;
    char *src;
    char *cat;
    size_t src_len = 0;
    size_t cat_len = 0;
    cnk = bolt_hello_success("", 0);
    src = bolt_chunk_to_byte_array(&cnk);
    src_len = ntohs(cnk.header) + BOLT_CHUNK_HEADER_SIZE * 2;
    cat_len = concaternating_byte_array(&cat, cat_len, src, src_len);

    return mallocating_msg_with_length(cat, cat_len);
}

struct msg *ws_handshake() {
    struct msg *server_version = malloc(sizeof(struct msg));
    server_version->payload = (uint8 *) server_ver;
    server_version->final = 1;
    server_version->first = 1;
    server_version->len = 4;
    server_version->binary = LWS_WRITE_BINARY;

    return server_version;
}

