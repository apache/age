/*
 * message_utils.h 
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
 *	  src/include/spec/bolt/message/message_utils.h
 */
#ifndef BOLT_MESSAGE_UTILS_H
#define BOLT_MESSAGE_UTILS_H

#include "graph_protocol_stddef.h"

#include "spec/bolt/message/v440/signature.h"


struct node_info {
    char *label;
    int32 nid;
    char *properties;
};


char *bolt_signature_to_string(BOLT_SIGNATURE bs);

bool is_bolt_tag(uint8 c);

bool is_bolt_signature(uint8 c);

bool byte_to_bolt_signature(uint8 c);

struct node_info node_string_to_structure(char libpq_node_str_arr[]);

#endif
