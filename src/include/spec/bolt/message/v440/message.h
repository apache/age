/*
 * message_4_4_0.h 
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
 *	  src/include/spec/message/message_4_4_0.h
 */
#ifndef SPEC_BOLT_MESSAGE_440_H
#define SPEC_BOLT_MESSAGE_440_H

#include <libpq-fe.h>
#include "graph_protocol_stddef.h"
#include "chunk.h"


#define BOLT_CHUNK_HEADER_SIZE 2
#define BOLT_CHUNK_ENDHEADER_SIZE 2


size_t extract_cnk_from_pgresult(const PGresult *r, char **dest_bytearray);

char *bolt_chunk_to_byte_array(BOLT_CHUNK *cnk);

size_t extract_ws_cypher_string(char buf[], char **dest);

size_t extract_cypher_string(char buf[], char **dest);



//failed

//ignored

//record
BOLT_CHUNK bolt_run_dbping_record(char recv[], int len);

//success
BOLT_CHUNK bolt_run_success(char **field_str, int field_count);

BOLT_CHUNK bolt_pull_success(char recv[], int len);

BOLT_CHUNK bolt_commit_success();

BOLT_CHUNK bolt_route_success(char recv[], int len);

BOLT_CHUNK bolt_hello_success(char recv[], int len);

BOLT_CHUNK bolt_run_dbping_success(char recv[], int len);

BOLT_CHUNK bolt_run_dbping_bookmark_success(char recv[], int len);

#endif
