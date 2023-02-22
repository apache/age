/*
 * pack_string.c 
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
 *	  src/spec/packstream/pack_string.c
 */

#include "graph_protocol_stddef.h"
#include "spec/packstream/v100/marker.h"
#include "spec/packstream/v100/coretypes.h"
#include "spec/packstream/pack_utils.h"
#include "spec/packstream/v100/packstream_v100.h"

char *write_tiny_string(char str[]) {

    uint8 marker = (uint8) PM_TINY_STRING;
    char *result;
    uint8 str_len = strlen(str);

    if (str_len > PM_TINY_STRING_MAX) {
        perror("parameter string is exceed number of tiny_string");
        return NULL;
    }

    result = malloc(PACK_MARKER_LENGTH + str_len);
    result[0] = (char) (marker + str_len);

    for (int i = 0; i < str_len; ++i) {
        result[1 + i] = str[i];
    }

    return result;
}

char *write_string8(char str[]) {
    uint8 marker = (uint8) PM_STRING8;

    char *result;
    uint8 str_len = strlen(str);

    if (str_len > PM_STRING8_MAX) {
        perror("parameter string is exceed number of string8");
        exit(1);
    }

    result = malloc(PACK_MARKER_LENGTH + STRING8_SIZE + str_len);
    result[0] = (char) (marker);
    result[1] = (char) str_len;

    memcpy(result + 2, str, str_len); // except null-terminated

    for (int i = 0; i < str_len; ++i) {
        result[i + 2] = str[i];
    }

    return result;
}

char *write_string16(char str[]) {
    char marker = (char) PM_STRING8;
    char *result;
    int str_len = strlen(str);

    if (str_len > PM_STRING16_MAX) {
        perror("parameter string is exceed number of string16");
        return NULL;
    }

    result = malloc(PACK_MARKER_LENGTH + str_len);
    result[0] = marker | str_len;
    for (int i = 0; i < str_len; ++i) {
        result[i + 1] = str[i];
    }
    return result;
}
