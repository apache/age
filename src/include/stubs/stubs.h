/*
 * stubs.h 
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
 *	  src/include/stubs/stubs.h
 */
#ifndef STUBS_H
#define STUBS_H

#include "graph_protocol_stddef.h"


enum STUB_NAME {
    SUCCESS, COMMIT, SHOW_DATABASES, BOOKMARK, SHOW_CURRENT_USER, CALL_DBMS_COMPONENTS
};

struct STUB {
    enum STUB_NAME name;
    uint8 *data;
    size_t len;
};

extern struct STUB STUBS[];

struct STUB stub_finder(enum STUB_NAME name);

struct msg *stub_lws_msg_maker(enum STUB_NAME stub_name);

#endif
