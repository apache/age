/*
 * lws.c 
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
 *	  src/lib/libwebsocket/lws.c
 */

#include "libwebsocket/lws.h"


//int port = 7681;
int interrupted;
int options;

struct per_session_data pss = {
        NULL, 0, 0, 0, 0, 0, NULL};

struct lws_protocols protocols[] = {
        {
                "lws-minimal-server-echo",
                callback_minimal_server_echo,
                sizeof(struct per_session_data),
                1024,
                0,
                &pss,
                0
        },
        LWS_PROTOCOL_LIST_TERM
};


static const struct lws_protocol_vhost_options pvo_options = {
        NULL,
        NULL,
        "options",        /* pvo name */
        (void *) &options    /* pvo value */
};

static const struct lws_protocol_vhost_options pvo_interrupted = {
        &pvo_options,
        NULL,
        "interrupted",        /* pvo name */
        (void *) &interrupted    /* pvo value */
};

static const struct lws_protocol_vhost_options pvo = {
        NULL,                /* "next" pvo linked-list */
        &pvo_interrupted,        /* "child" pvo linked-list */
        "lws-minimal-server-echo",    /* protocol name we belong to on this vhost */
        ""                /* ignored */
};

static const struct lws_extension extensions[] = {
        {
                "permessage-deflate",
                lws_extension_callback_pm_deflate,
                "permessage-deflate"
                "; client_no_context_takeover"
                "; client_max_window_bits"
        },
        {NULL, NULL, NULL /* terminator */ }
};


int ws_server(int32 port) {

    struct lws_context_creation_info info;
    struct lws_context *context;

    int n = 0, logs = LLL_USER | LLL_ERR | LLL_WARN | LLL_NOTICE;

    signal(SIGINT, sigint_handler);

    lws_set_log_level(logs, NULL);

    memset(&info, 0, sizeof(struct lws_context_creation_info)); /* otherwise uninitialized garbage */
    info.port = port;
    info.protocols = protocols;
    info.pvo = &pvo;
    info.extensions = extensions;
    info.pt_serv_buf_size = 32 * 1024;
    info.options = LWS_SERVER_OPTION_VALIDATE_UTF8 |
                   LWS_SERVER_OPTION_HTTP_HEADERS_SECURITY_BEST_PRACTICES_ENFORCE;

    context = lws_create_context(&info);

    if (!context) {
        lwsl_err("lws init failed\n");
        return 1;
    }

    while (n >= 0 && !interrupted) {
        n = lws_service(context, 0);
    }

    lws_context_destroy(context);
    lwsl_user("Completed %s\n", interrupted == 2 ? "OK" : "failed");
    return interrupted != 2;
}
