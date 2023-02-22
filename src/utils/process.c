/*
 * process.c 
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
 *	  src/utils/process.c
 */

#include "utils/process.h"

pid_t fork_bolt_connection(const int *server_sock, const int *client_sock) {
    pid_t pid;
    pid = fork();

    if (pid == -1) {
        perror("fork failed");
        close(*client_sock);
        close(*server_sock);
        exit(1);
    }

    if (pid == 0) {
        puts("child");
        close(*server_sock);
    } else {
        puts("server");
        close(*client_sock);
    }
    return pid;
}

