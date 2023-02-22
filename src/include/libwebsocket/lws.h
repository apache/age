/*
 * lws.h 
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
 *	  src/include/libwebsocket/lws.h
 */
#ifndef LWS_H
#define LWS_H


#include <libwebsockets.h>
#include "graph_protocol_stddef.h"

#define RING_DEPTH 4096


/* one of these created for each message */
struct msg {
    void *payload; /* is malloc'd */
    size_t len;
    char binary;
    char first;
    char final;
};

struct per_session_data {
    struct lws_ring *ring;
    uint32_t msglen;
    uint32_t tail;
    uint8_t completed: 1;
    uint8_t flow_controlled: 1;
    uint8_t write_consume_pending: 1;
    struct pg_conn *conn;
};

struct per_vhost_storage {
    struct lws_context *context;
    struct lws_vhost *vhost;
    int *interrupted;
    int *options;
};


int callback_minimal_server_echo(struct lws *wsi, enum lws_callback_reasons reason,
                                 void *user, void *in, size_t len);

int ws_server(int32 port);


//lws_utils
void minimal_destroy_message(void *_msg);

void sigint_handler(int sig);


//lws_lejp_parser

struct lejp_json {
    char *key;
    void *value;
    struct lejp_json *next;
};

int lws_lejp_parser(char json_str[]);


#endif
