/*
 * pack_utils.h 
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
 *	  src/include/spec/packstream/pack_utils.h
 */
#ifndef PACK_UTILS_H
#define PACK_UTILS_H

#include "graph_protocol_stddef.h"

typedef enum {
    PM_TINY_INT_MIN = -16,
} PACK_MARKER_MIN;

typedef enum {
    PM_TINY_STRING_MAX = 15,
    PM_TINY_INT_MAX = 127,
    PM_TINY_LIST_MAX = 15,
    PM_TINY_LIST8_MAX = 255,
    PM_TINY_DICT_MAX = 15,
    PM_TINY_STRUCT_MAX = 15,
    PM_STRING8_MAX = 255,
    PM_STRING16_MAX = 65535,
    PM_STRING32_MAX = 2147483647,
} PACK_MARKER_MAX;


int pack_appender(size_t dest_size, char **dest, char *pack);

bool is_marker_data_itself(uint8 c);

bool is_tiny_container_marker(uint8 c);

bool is_tiny_marker(uint8 c);

bool is_container_marker(uint8 c);

bool is_marker_with_sizebyte(uint8 c);

int extract_pack_length(const char pack[]);

#endif
