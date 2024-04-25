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

#ifndef CYPHER_OPS_H
#define CYPHER_OPS_H

#include "fmgr.h"

/* operators for jsonb */
extern Datum jsonb_add(PG_FUNCTION_ARGS);
extern Datum jsonb_sub(PG_FUNCTION_ARGS);
extern Datum jsonb_mul(PG_FUNCTION_ARGS);
extern Datum jsonb_div(PG_FUNCTION_ARGS);
extern Datum jsonb_mod(PG_FUNCTION_ARGS);
extern Datum jsonb_pow(PG_FUNCTION_ARGS);
extern Datum jsonb_uplus(PG_FUNCTION_ARGS);
extern Datum jsonb_uminus(PG_FUNCTION_ARGS);

/* coercion from numeric to graphid */
extern Datum numeric_graphid(PG_FUNCTION_ARGS);

#endif	/* CYPHER_OPS_H */
