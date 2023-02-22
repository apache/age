/*
 * structure.h 
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
 *	  src/include/spec/message/common/structure.h
 */
#ifndef BOLT_TAG_H
#define BOLT_TAG_H

#define BOLT_TAG_SIZE 1

typedef enum {
    BT_NODE = 0x4E,
    BT_RELATIONSHIP = 0x52,
    BT_UNBOUNDRELATIONSHIP = 0x72,
    BT_PATH = 0x50,
    BT_DATE = 0x44,
    BT_TIME = 0x54,
    BT_LOCALTIME = 0x74,
    BT_DATETIME = 0x46,
    BT_DATETIMEZONEID = 0x66,
    BT_LOCALDATETIME = 0x64,
    BT_DURATION = 0x45,
    BT_POINT2D = 0x58,
    BT_POINT3D = 0x59
} BOLT_TAG;

#endif
