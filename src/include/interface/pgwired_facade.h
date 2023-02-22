/*
 * agwired.h 
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
 *	  src/include/interface/agwired.h
 */
#ifndef AGWIRED_H
#define AGWIRED_H

#define GRAPHIDARRAYOID 7001
#define GRAPHIDOID 7002
#define VERTEXARRAYOID 7011
#define VERTEXOID 7012
#define EDGEARRAYOID 7021
#define EDGEOID 7022
#define GRAPHPATHARRAYOID 7031
#define GRAPHPATHOID 7032
#define ROWIDARRAYOID 7061
#define ROWIDOID 7062

#include <libpq-fe.h>

PGconn *connect_ag(const char *conn_str);
PGresult *exec_cypher(PGconn *conn, char *cypher_str);

#endif
