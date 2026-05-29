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

#include "postgres.h"

#include "fmgr.h"

/*
 * Compatibility stub for the old driver-facing two-step execution path.
 *
 * The previous implementation stored graph/query text in backend-global state
 * and then let a later cypher(NULL, NULL) call consume that state.  The parser
 * now requires cypher() to receive its graph and query text directly, keeping
 * cypher query execution on PostgreSQL's normal parse/plan lifecycle instead
 * of an AGE-managed side channel.
 */
PG_FUNCTION_INFO_V1(age_prepare_cypher);

Datum age_prepare_cypher(PG_FUNCTION_ARGS)
{
    PG_RETURN_BOOL(false);
}
