/*
 * success.c 
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
 *	  src/spec/bolt/message/v440/success.c
 */

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

#include "libwebsockets.h"


BOLT_CHUNK bolt_pull_success(char recv[], int len) {
    BOLT_CHUNK cnk;

    size_t c = 0; //count
    char *b = NULL;


    // bookmark is only for the implicit transaction situation.

    c += pack_appender(c, &b, write_tiny_struct_marker(1));
    c += pack_appender(c, &b, write_bolt_signature(BS_SUCCESS));
//    c += pack_appender(c, &b, write_tiny_dict_marker(4));
//    c += pack_appender(c, &b, write_tiny_string("bookmark"));
//    c += pack_appender(c, &b, write_tiny_string("b"));

    c += pack_appender(c, &b, write_tiny_dict_marker(3));
    c += pack_appender(c, &b, write_tiny_string("type"));
    c += pack_appender(c, &b, write_tiny_string("r"));
    c += pack_appender(c, &b, write_tiny_string("db"));
    c += pack_appender(c, &b, write_tiny_string("neo4j"));
    c += pack_appender(c, &b, write_tiny_string("t_last"));
    c += pack_appender(c, &b, write_tiny_int(1));

    cnk.header = htons((unsigned short) (c));
    cnk.body = b;
    cnk.ender = (unsigned short) 0x0000;
    return cnk;
}

BOLT_CHUNK bolt_route_success(char recv[], int len) {
    BOLT_CHUNK cnk;

    size_t c = 0; //count
    char *b = NULL;

    char temp[3] = {0xc9, 0x01, 0x2c};

    c += pack_appender(c, &b, write_tiny_struct_marker(1));
    c += pack_appender(c, &b, write_bolt_signature(BS_SUCCESS));
    c += pack_appender(c, &b, write_tiny_dict_marker(1));
    c += pack_appender(c, &b, write_tiny_string("rt"));
    c += pack_appender(c, &b, write_tiny_dict_marker(3));
    c += pack_appender(c, &b, write_tiny_string("servers"));
    c += pack_appender(c, &b, write_tiny_list_marker(3));
    c += pack_appender(c, &b, write_tiny_dict_marker(2));
    c += pack_appender(c, &b, write_tiny_string("addresses"));
    c += pack_appender(c, &b, write_tiny_list_marker(1));
    c += pack_appender(c, &b, write_tiny_string("localhost:7681"));
    c += pack_appender(c, &b, write_tiny_string("role"));
    c += pack_appender(c, &b, write_tiny_string("WRITE"));


    c += pack_appender(c, &b, write_tiny_dict_marker(2));
    c += pack_appender(c, &b, write_tiny_string("addresses"));
    c += pack_appender(c, &b, write_tiny_list_marker(1));
    c += pack_appender(c, &b, write_tiny_string("localhost:7681"));
    c += pack_appender(c, &b, write_tiny_string("role"));
    c += pack_appender(c, &b, write_tiny_string("READ"));
    c += pack_appender(c, &b, write_tiny_dict_marker(2));
    c += pack_appender(c, &b, write_tiny_string("addresses"));

    c += pack_appender(c, &b, write_tiny_list_marker(1));
    c += pack_appender(c, &b, write_tiny_string("localhost:7681"));
    c += pack_appender(c, &b, write_tiny_string("role"));
    c += pack_appender(c, &b, write_tiny_string("ROUTE"));
    c += pack_appender(c, &b, write_tiny_string("ttl"));
//    c += pack_appender(c, &b, write_int16(300));

    c += pack_appender(c, &b, write_tiny_int(14));

    c += pack_appender(c, &b, write_tiny_string("db"));
    c += pack_appender(c, &b, write_tiny_string("neo4j"));


    cnk.header = htons((unsigned short) (c));
    cnk.body = b;
    cnk.ender = (unsigned short) 0x0000;

    return cnk;

}


BOLT_CHUNK bolt_run_success(char **field_str, int field_count) {

    BOLT_CHUNK cnk;

    size_t c = 0;
    char *b = NULL;

    char **field_str_backup = malloc(sizeof(char *) * field_count);
    memcpy(field_str_backup, field_str, sizeof(char *) * field_count);

    c += pack_appender(c, &b, write_tiny_struct_marker(1));
    c += pack_appender(c, &b, write_bolt_signature(BS_SUCCESS));
    c += pack_appender(c, &b, write_tiny_dict_marker(2));
    c += pack_appender(c, &b, write_tiny_string("fields"));
    c += pack_appender(c, &b, write_tiny_list_marker(field_count));

    for (int i = 0; i < field_count; ++i) {
        c += pack_appender(c, &b, write_tiny_string(field_str_backup[i]));
    }

    c += pack_appender(c, &b, write_tiny_string("t_first"));
    c += pack_appender(c, &b, write_tiny_int(2));
    c += pack_appender(c, &b, write_tiny_string("qid"));
    c += pack_appender(c, &b, write_tiny_int(4));

    cnk.header = htons((unsigned short) (c));
    cnk.body = b;
    cnk.ender = (unsigned short) 0x0000;

    free(field_str_backup);
    return cnk;
}


BOLT_CHUNK bolt_commit_success() {
    BOLT_CHUNK cnk;

    size_t c = 0; //count
    char *b = NULL;

    c += pack_appender(c, &b, write_tiny_struct_marker(1));
    c += pack_appender(c, &b, write_bolt_signature(BS_SUCCESS));
    c += pack_appender(c, &b, write_tiny_dict_marker(0));

    cnk.header = htons((unsigned short) (c));
    cnk.body = b;
    cnk.ender = (unsigned short) 0x0000;
    return cnk;
}


BOLT_CHUNK bolt_hello_success(char recv[], int len) {
    BOLT_CHUNK cnk;

    size_t c = 0; //count
    char *b = NULL;

    c += pack_appender(c, &b, write_tiny_struct_marker(1));
    c += pack_appender(c, &b, write_bolt_signature(BS_SUCCESS));
    c += pack_appender(c, &b, write_tiny_dict_marker(4));
    c += pack_appender(c, &b, write_tiny_string("server"));
    c += pack_appender(c, &b, write_tiny_string("agbolt_4.4.2"));
    c += pack_appender(c, &b, write_tiny_string("connection_id"));
    c += pack_appender(c, &b, write_tiny_string("bolt-1"));

    c += pack_appender(c, &b, write_tiny_string("hints"));
    c += pack_appender(c, &b, write_tiny_dict_marker(0));

    c += pack_appender(c, &b, write_tiny_string("patch_bolt"));
    c += pack_appender(c, &b, write_tiny_list_marker(1));
    c += pack_appender(c, &b, write_tiny_string("utc"));

    cnk.header = htons((unsigned short) (c));
    cnk.body = b;
    cnk.ender = (unsigned short) 0x0000;

    return cnk;
}


BOLT_CHUNK bolt_run_dbping_success(char recv[], int len) {

    BOLT_CHUNK cnk;

    size_t c = 0; //count
    char *b = NULL;

    c += pack_appender(c, &b, write_tiny_struct_marker(1));
    c += pack_appender(c, &b, write_bolt_signature(BS_SUCCESS));
    c += pack_appender(c, &b, write_tiny_dict_marker(2));
    c += pack_appender(c, &b, write_tiny_string("t_first"));
    c += pack_appender(c, &b, write_tiny_string("fields"));
    c += pack_appender(c, &b, write_tiny_list_marker(1));
    c += pack_appender(c, &b, write_tiny_string("success"));

    cnk.header = htons((unsigned short) (c));
    cnk.body = b;
    cnk.ender = (unsigned short) 0x0000;

    return cnk;

}


BOLT_CHUNK bolt_run_dbping_bookmark_success(char recv[], int len) {
    BOLT_CHUNK cnk;

    size_t c = 0; //count
    char *b = NULL;

    c += pack_appender(c, &b, write_tiny_struct_marker(1));
    c += pack_appender(c, &b, write_bolt_signature(BS_SUCCESS));
    c += pack_appender(c, &b, write_tiny_dict_marker(4));
    c += pack_appender(c, &b, write_tiny_string("bookmark"));
    c += pack_appender(c, &b, write_tiny_string("b"));
    c += pack_appender(c, &b, write_tiny_string("type"));
    c += pack_appender(c, &b, write_tiny_string("r"));
    c += pack_appender(c, &b, write_tiny_string("t_last"));
    c += pack_appender(c, &b, write_tiny_int(1));
    c += pack_appender(c, &b, write_tiny_string("db"));
    c += pack_appender(c, &b, write_tiny_string("neo4j"));
    cnk.header = htons((unsigned short) (c));
    cnk.body = b;
    cnk.ender = (unsigned short) 0x0000;
    return cnk;
}

