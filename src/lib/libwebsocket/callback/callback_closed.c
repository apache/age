/*
 * callback_closed.c 
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
 *	  src/lib/libwebsocket/callback/callback_closed.c
 */

#include "graph_protocol_stddef.h"
#include "interface/pgwired_facade.h"
#include "libwebsocket/lws.h"
#include "libwebsocket/callback/lws_callback.h"


uint8 lws_callback_closed(struct per_session_data *pss, struct per_vhost_storage *vhd, struct lws *wsi) {
    lwsl_user("LWS_CALLBACK_CLOSED\n");
    lws_ring_destroy(pss->ring);

    if (*vhd->options & 1) {
        if (!*vhd->interrupted)
            *vhd->interrupted = 1 + pss->completed;
        lws_cancel_service(lws_get_context(wsi));
    }
    return CALLBACK_SUCCESS;
}
