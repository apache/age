/*
 * server_state_4_4_0.h 
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
 *	  src/include/state/server/server_state_4_4_0.h
 */
#ifndef SPEC_BOLT_SERVER_STATE_4_4_0_H
#define SPEC_BOLT_SERVER_STATE_4_4_0_H


typedef enum {
    DISCONNECTED //no socket connection
    , CONNECTED // protocol handshake is completed successfully
    , DEFUNCT //the socket connection is permanently closed
    , READY //ready to accept a RUN message
    , STREAMING //Auto-commit Transaction, a result is available for streaming from the server
    , TX_READY //Explicit Transaction, ready to accept a RUN message
    , TX_STREAMING //Explicit Transaction, a result is available for streaming from the server
    , FAILED //a connection is in a temporarily unusable state
    , INTERRUPTED //the server got an <INTERRUPT> signal
} BOLT_SERVER_STATE;
#endif
