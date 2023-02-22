/*
 * conn_manager.h 
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
 *	  src/include/communication/conn_manager.h
 */

#ifndef CONN_MANAGER_H
#define CONN_MANAGER_H

#define TCP_BUF_SIZE 2048
#define CLIENT_CONNECTION_MAX 20

#include "interface/translator.h"

#include "communication/bolt/receiver.h"
#include "communication/bolt/sender.h"
#include "communication/ag/receiver.h"
#include "communication/ag/sender.h"


#define MAX_CLIENT_CONNECTION 50

typedef enum {
    WEBSOCKET,
    IPC_BOLT
} CLIENT_REQUESTED_PROTOCOL;

typedef struct {
    uint64_t c_conn_id;
    struct sockaddr_in c_address;
    int *c_sock;
    CLIENT_REQUESTED_PROTOCOL c_protocol;
    char c_handshake_request[TCP_BUF_SIZE];
} client_info;

extern client_info cinfo[MAX_CLIENT_CONNECTION];

extern size_t client_info_index;




void accept_sock(int *new_socket, const int *listen_fd, struct sockaddr_in *address,
                 int addrlen);

ssize_t recv_data(const int *conn, char dest[]);

ssize_t send_data(const int *conn, const char src[], size_t src_len);

size_t listening_connection(int *server_sock, int *client_sock, int port);

CLIENT_REQUESTED_PROTOCOL checking_connection_procotol(client_info cinfo);

#endif
