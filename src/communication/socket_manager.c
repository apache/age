/*
 * conn_manager.c 
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
 *	  src/communication/conn_manager.c
 */

#include "communication/socket_manager.h"
#include "utils/process.h"


client_info cinfo[MAX_CLIENT_CONNECTION];
size_t client_info_index = 0;



void accept_sock(int *new_socket, const int *listen_fd,
                 struct sockaddr_in *address, int addrlen) {
    *new_socket = accept(*listen_fd, (struct sockaddr *) address,
                         (socklen_t *) &addrlen);

    if (*new_socket < 0) {
        perror("accept failed : ");
        exit(EXIT_FAILURE);
    }
}

ssize_t recv_data(const int *conn, char dest[]) {
    ssize_t len = 0;
    char buf[TCP_BUF_SIZE];
    len = recv(*conn, buf, TCP_BUF_SIZE, 0);
    memcpy(dest, buf, len);
    return len;
}

ssize_t send_data(const int *conn, const char src[], size_t src_len) {
    return send(*conn, src, src_len, 0);
}

size_t listening_connection(int *server_sock, int *client_sock, int port) {

    pid_t pid;
    struct sigaction act;
//    int state;
//    act.sa_handler = read_childproc;
//    sigemptyset(&act.sa_mask);
//    act.sa_flags = 0;
//    state = sigaction(SIGCHLD, &act, 0);

    char msg[TCP_BUF_SIZE];
    struct sockaddr_in server_address;
    struct sockaddr_in client_address;
    socklen_t client_address_size;
    client_address_size = sizeof(client_address);
    int optval = 1;


    *server_sock = socket(PF_INET, SOCK_STREAM, 0);
    if (*server_sock == -1) {
        perror("socket failed \n");
        exit(EXIT_FAILURE);
    }
    memset(&server_address, 0, sizeof(server_address));
    server_address.sin_family = AF_INET;
    server_address.sin_addr.s_addr = htonl(INADDR_ANY);
    server_address.sin_port = htons(port);

    setsockopt(*server_sock, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));

    if (bind(*server_sock, (struct sockaddr *) &server_address, sizeof(server_address)) == -1) {
        perror("bind failed : \n");
        exit(EXIT_FAILURE);
    }

    if (listen(*server_sock, 5) == -1) {
        perror("listen failed\n");
        exit(EXIT_FAILURE);
    }

    *client_sock = accept(*server_sock, (struct sockaddr *) &client_address, &client_address_size);
    if (*client_sock == -1) {
        perror("accept failed\n");
    }

    cinfo[client_info_index].c_address = client_address;
    cinfo[client_info_index].c_sock = client_sock;
    cinfo[client_info_index].c_conn_id = client_info_index;

    return client_info_index;
}


CLIENT_REQUESTED_PROTOCOL checking_connection_procotol(client_info client_info) {

    size_t recv_len;
    char buf[TCP_BUF_SIZE];
    const char http_get[] = {0x47, 0x45, 0x54}; // 0x47=G, 0x45=E, 0x54=T
    const char bolt_id[] = {0x60, 0x60, 0xB0, 0x17};

    recv_len = recv(*client_info.c_sock, buf, TCP_BUF_SIZE, 0);
    memcpy(client_info.c_handshake_request, buf, recv_len);

    if (strncmp(client_info.c_handshake_request, http_get, 3) == 0) {
        client_info.c_protocol = WEBSOCKET;
    } else if (strncmp(client_info.c_handshake_request, bolt_id, 4) == 0) {
        client_info.c_protocol = IPC_BOLT;
    } else {
        perror("no supported protocol detected");
    }

    return client_info.c_protocol;
}

