/*
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
 */

#ifndef AG_GRAPHOID_H
#define AG_GRAPHOID_H

#define graphoid int32

#define INVALID_AG_GRAPH_ID 0

#define DatumGetGraphOid(X) ((graphoid) (X))
#define GraphOidGetDatum(X) ((Datum) (X))
#define AG_GETARG_GRAPHOID(n) PG_GETARG_INT32(n)
#define F_GRAPHOIDEQ F_INT4EQ
#define GRAPHOIDOID INT4OID

#define graph_oid_is_valid(graph_oid) (graph_oid != INVALID_AG_GRAPH_ID)

#define get_next_graph_oid() (DatumGetInt32(DirectFunctionCall1(nextval_oid, ag_graph_id_seq())))

#endif