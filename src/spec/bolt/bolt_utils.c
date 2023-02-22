/*
 * bolt_utils.c 
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
 *	  src/spec/bolt/bolt_utils.c
 */

#include "graph_protocol_stddef.h"

#include "spec/bolt/message/message_utils.h"
#include "spec/bolt/message/v440/tag.h"
#include "spec/bolt/message/v440/message.h"
#include "spec/bolt/message/v440/chunk.h"
#include "spec/bolt/message/v440/signature.h"

bool is_bolt_tag(uint8 c) {
    switch (c) {
        case 0x4E:
        case 0x52:
        case 0x72:
        case 0x50:
        case 0x44:
        case 0x54:
        case 0x74:
        case 0x46:
        case 0x66:
        case 0x64:
        case 0x45:
        case 0x58:
        case 0x59:
            return true;
        default:
            return false;
    }
}

bool is_bolt_signature(uint8 c) {
    switch (c) {
        case 0x01:
        case 0x02:
        case 0x0F:
        case 0x10:
        case 0x2F:
        case 0x3F:
        case 0x11:
        case 0x12:
        case 0x13:
        case 0x66:
        case 0x70:
        case 0x7E:
        case 0x7F:
        case 0x71:
            return true;
        default:
            return false;
    }
}


bool byte_to_bolt_signature(uint8 c) {
    switch (c) {
        case 0x01:
            return BS_HELLO;
        case 0x02:
            return BS_GOODBYE;
        case 0x0F:
            return BS_RESET;
        case 0x10:
            return BS_RUN;
        case 0x2F:
            return BS_DISCARD;
        case 0x3F:
            return BS_PULL;
        case 0x11:
            return BS_BEGIN;
        case 0x12:
            return BS_COMMIT;
        case 0x13:
            return BS_ROLLBACK;
        case 0x66:
            return BS_ROUTE;
        case 0x70:
            return BS_SUCCESS;
        case 0x7E:
            return BS_IGNORED;
        case 0x7F:
            return BS_FAILURE;
        case 0x71:
            return BS_RECORD;
        default:
            return false;
    }
}


