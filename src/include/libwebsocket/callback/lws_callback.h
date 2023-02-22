/*
 * lws_callback.h 
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
 *	  src/include/libwebsocket/callback/lws_callback.h
 */
#ifndef LWS_CALLBACK_H
#define LWS_CALLBACK_H


enum CALLBACK_STATUS {
    CALLBACK_SUCCESS,
    CALLBACK_INIT_VHD_IS_FALSE,
    CALLBACK_ESTABLISHED_RING_IS_FALSE,
    CALLBACK_BREAK
};


//init
uint8 lws_callback_init(struct lws *wsi, struct per_vhost_storage *vhd, void *in);

struct pg_conn *get_postgres_connection(char *conn_string);

//establish
uint8 lws_callback_established(struct per_session_data *pss);

//writable

//receive
uint8 lws_callback_receive(
        struct lws *wsi, struct per_session_data *pss, size_t len, uint32 *n,
        const struct msg *pmsg, struct msg *amsg, void *in);


//closed
uint8 lws_callback_closed(struct per_session_data *pss, struct per_vhost_storage *vhd, struct lws *wsi);


#endif


