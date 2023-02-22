/*
 * marker.h 
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
 *	  src/include/spec/packstream/v100/marker.h
 */
#ifndef MARKER_H
#define MARKER_H

#define PACK_MARKER_LENGTH 1
#define PACK_SIZE_LENGTH 1

typedef enum {
    PM_NULL = 0xC0,
    PM_FALSE = 0xC2,
    PM_TRUE = 0xC3,

    PM_TINY_INT = 0x00, // 0xF0(-16) ~ 0x7F(127)
    PM_INT8 = 0xC8,
    PM_INT16 = 0xC9,
    PM_INT32 = 0xCA,
    PM_INT64 = 0xCB,

    PM_FLOAT64 = 0xC1,

    PM_BYTES8 = 0xCC,
    PM_BYTES16 = 0xCD,
    PM_BYTES32 = 0xCE,

    PM_TINY_STRING = 0x80,
    PM_STRING8 = 0xD0,
    PM_STRING16 = 0xD1,
    PM_STRING32 = 0xD3,

    PM_TINY_LIST = 0x90,
    PM_LIST8 = 0xD4,
    PM_LIST16 = 0xD5,
    PM_LIST32 = 0xD6,

    PM_TINY_DICT = 0xA0,
    PM_DICT8 = 0xD8,
    PM_DICT16 = 0xD9,
    PM_DICT32 = 0xDA,

    PM_TINY_STRUCT = 0xB0,
    PM_STRUCT8 = 0xDC,
    PM_STRUCT16 = 0xDD,
    PM_STRUCT32 = 0xDE,

} PACK_MARKER;

#endif
