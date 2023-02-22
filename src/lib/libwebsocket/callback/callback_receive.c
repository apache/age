/*
 * callback_receive.c 
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
 *	  src/lib/libwebsocket/callback/callback_receive.c
 */

#include "graph_protocol_stddef.h"
#include "interface/pgwired_facade.h"
#include "libwebsocket/lws.h"
#include "libwebsocket/callback/lws_callback.h"

extern int in_transaction;

uint8 lws_callback_receive(
        struct lws *wsi, struct per_session_data *pss, size_t len, uint32 *n,
        const struct msg *pmsg, struct msg *amsg, void *in
) {



}
