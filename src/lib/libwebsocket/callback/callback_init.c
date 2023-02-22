/*
 * callback_init.c 
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
 *	  src/lib/libwebsocket/callback/callback_init.c
 */

#include "graph_protocol_stddef.h"
#include "interface/pgwired_facade.h"
#include "libwebsocket/lws.h"
#include "libwebsocket/callback/lws_callback.h"

uint8 lws_callback_init(struct lws *wsi, struct per_vhost_storage *vhd, void *in) {

    vhd = lws_protocol_vh_priv_zalloc(lws_get_vhost(wsi),
                                      lws_get_protocol(wsi),
                                      sizeof(struct per_vhost_storage));

    if (!vhd)
        return CALLBACK_INIT_VHD_IS_FALSE;

    vhd->context = lws_get_context(wsi);
    vhd->vhost = lws_get_vhost(wsi);

    /* get the pointers we were passed in pvo */
    vhd->interrupted = (int *) lws_pvo_search(
            (const struct lws_protocol_vhost_options *) in,
            "interrupted")->value;

    vhd->options = (int *) lws_pvo_search(
            (const struct lws_protocol_vhost_options *) in,
            "options")->value;

    return CALLBACK_SUCCESS;
}

struct pg_conn *get_postgres_connection(char *conn_string) {
    return connect_ag(conn_string);
}
