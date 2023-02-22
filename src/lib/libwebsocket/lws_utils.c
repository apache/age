/*
 * lws_utils.c 
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
 *	  src/lib/libwebsocket/lws_utils.c
 */

#include "libwebsocket/lws.h"
#include "stubs/stubs.h"

extern int interrupted;
extern int port;


void minimal_destroy_message(void *_msg) {
    struct msg *msg = _msg;

    free(msg->payload);
    msg->payload = NULL;
    msg->len = 0;
}

void sigint_handler(int sig) {
    interrupted = 1;
}

struct msg *mallocating_msg_with_length(uint8 *raw, size_t len) {
    struct msg *r = malloc(sizeof(struct msg *));
    r->payload = (uint8 *) raw;
    r->final = 1;
    r->first = 1;
    r->len = len;
    r->binary = LWS_WRITE_BINARY;
    return r;
}


struct msg *stub_lws_msg_maker(enum STUB_NAME stub_name) {
    struct STUB s = stub_finder(stub_name);
    return mallocating_msg_with_length(s.data, s.len);
}

