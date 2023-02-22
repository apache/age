/*
 * pack_dict.c 
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
 *	  src/spec/packstream/pack_dict.c
 */

#include "graph_protocol_stddef.h"
#include "spec/packstream/v100/marker.h"
#include "spec/packstream/v100/coretypes.h"
#include "spec/packstream/pack_utils.h"
#include "spec/packstream/v100/packstream_v100.h"


char *write_tiny_dict_marker(int element_count) {
    char *result;

    if (element_count > PM_TINY_DICT_MAX) {
        perror("parameter value is exceed range of tiny_list");
        return NULL;
    }

    result = malloc(PACK_MARKER_LENGTH);
    result[0] = (char) (PM_TINY_DICT | (NIBBLE_LOW & element_count));

    return result;
}

