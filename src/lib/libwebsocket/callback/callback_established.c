/*
/*
 * callback_established.c
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
 *	  src/lib/libwebsocket/callback/callback_established.c
 */

#include "graph_protocol_stddef.h"
#include "interface/pgwired_facade.h"
#include "libwebsocket/lws.h"
#include "libwebsocket/callback/lws_callback.h"


uint8 lws_callback_established(struct per_session_data *pss) {
    /* generate a block of output before travis times us out */
    lwsl_warn("LWS_CALLBACK_ESTABLISHED\n");
    pss->ring = lws_ring_create(
            sizeof(struct msg),
            RING_DEPTH,
            minimal_destroy_message);
    if (!pss->ring)
        return CALLBACK_ESTABLISHED_RING_IS_FALSE;
    pss->tail = 0;

    return CALLBACK_SUCCESS;
}