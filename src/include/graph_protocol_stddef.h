/*
 * adapter_stddef.h
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
 *	  src/include/adapter_stddef.h
 */
#ifndef ADAPTER_STDDEF_H
#define ADAPTER_STDDEF_H

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <limits.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <netinet/in.h>
#include <assert.h>
#include <fcntl.h>
#include <errno.h>

#ifndef NIBBLE_HIGH
#define NIBBLE_HIGH (0b11110000)
#endif

#ifndef NIBBLE_LOW
#define NIBBLE_LOW (0b00001111)
#endif

#ifndef HAVE_INT8
#define HAVE_INT8
typedef signed char int8;
#endif

#ifndef HAVE_INT16
#define HAVE_INT16
typedef signed short int16;
#endif

#ifndef HAVE_INT32
#define HAVE_INT32
typedef signed int int32;
#endif

#ifndef HAVE_INT64
#define HAVE_INT64
typedef long int int64;
#endif

#ifndef HAVE_UINT8
#define HAVE_UINT8
typedef unsigned char uint8;
#endif

#ifndef HAVE_UINT16
#define HAVE_UINT16
typedef unsigned short uint16;
#endif

#ifndef HAVE_UINT32
#define HAVE_UINT32
typedef unsigned int uint32;
#endif

#ifndef HAVE_UINT64
#define HAVE_UINT64
typedef unsigned long int uint64;
#endif

#endif
