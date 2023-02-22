/*
 * pack_integer.c 
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
 *	  src/spec/packstream/pack_integer.c
 */


#include "graph_protocol_stddef.h"
#include "spec/packstream/v100/marker.h"
#include "spec/packstream/v100/coretypes.h"
#include "spec/packstream/pack_utils.h"
#include "spec/packstream/v100/packstream_v100.h"


char *write_tiny_int(int integer) {
    char *result;

    result = malloc(PACK_MARKER_LENGTH);

    if (integer > PM_TINY_INT_MAX) {
        perror("parameter value is exceed range of tinyint");
        return NULL;
    } else if (integer < PM_TINY_INT_MIN) {
        perror("parameter value is under the range of tinyint");
        return NULL;
    }

    if (integer == 0) {
        result[0] = 0x00;
    } else if (integer < 0) {
        perror("skip impl");
    } else {
        result[0] = (char) (PM_TINY_INT | (integer & NIBBLE_LOW));
    }
    return result;
}

char *write_int8(int8 i) {
    char *result;
    result = malloc(PACK_MARKER_LENGTH + 1);

    union seperate {
        int8 c[2];
        int16 s;
    } a;
    a.s = htons(i);

    result[0] = (char) (PM_INT16);
    result[1] = a.c[0];
    result[2] = a.c[1];

    return result;
}

char *write_int16(int16 i) {
    char *result;
    result = malloc(PACK_MARKER_LENGTH + 2);

    union seperate {
        int8 c[2];
        int16 s;
    } a;
    a.s = htons(i);

    result[0] = (char) (PM_INT16);
    result[1] = a.c[0];
    result[2] = a.c[1];

    return result;
}
