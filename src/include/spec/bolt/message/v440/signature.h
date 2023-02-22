/*
 * signature.h 
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
 *	  src/include/spec/bolt/message/v440/signature.h
 */
#ifndef BOLT_SIGNATURE_H
#define BOLT_SIGNATURE_H

#define BOLT_SIGNATURE_SIZE 1

typedef enum {
    BS_HELLO = 0x01,
    BS_GOODBYE = 0x02,
    BS_RESET = 0x0F,
    BS_RUN = 0x10,
    BS_DISCARD = 0x2F,
    BS_PULL = 0x3F,
    BS_BEGIN = 0x11,
    BS_COMMIT = 0x12,
    BS_ROLLBACK = 0x13,
    BS_SUCCESS = 0x70,
    BS_ROUTE = 0x66,
    BS_IGNORED = 0x7E,
    BS_FAILURE = 0x7F,
    BS_RECORD = 0x71
} BOLT_SIGNATURE;

char *write_bolt_signature(BOLT_SIGNATURE bs);


#endif
