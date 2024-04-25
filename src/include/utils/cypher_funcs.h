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

#ifndef CYPHER_FUNCS_H
#define CYPHER_FUNCS_H

#include "fmgr.h"

/* scalar */
extern Datum jsonb_head(PG_FUNCTION_ARGS);
extern Datum jsonb_last(PG_FUNCTION_ARGS);
extern Datum jsonb_length(PG_FUNCTION_ARGS);
extern Datum jsonb_toboolean(PG_FUNCTION_ARGS);

/* list */
extern Datum jsonb_keys(PG_FUNCTION_ARGS);
extern Datum jsonb_tail(PG_FUNCTION_ARGS);

/* mathematical - numeric */
extern Datum jsonb_abs(PG_FUNCTION_ARGS);
extern Datum jsonb_ceil(PG_FUNCTION_ARGS);
extern Datum jsonb_floor(PG_FUNCTION_ARGS);
extern Datum jsonb_rand(PG_FUNCTION_ARGS);
extern Datum jsonb_round(PG_FUNCTION_ARGS);
extern Datum jsonb_sign(PG_FUNCTION_ARGS);

/* mathematical - logarithmic */
extern Datum jsonb_exp(PG_FUNCTION_ARGS);
extern Datum jsonb_log(PG_FUNCTION_ARGS);
extern Datum jsonb_log10(PG_FUNCTION_ARGS);
extern Datum jsonb_sqrt(PG_FUNCTION_ARGS);

/* mathematical - trigonometric */
extern Datum jsonb_acos(PG_FUNCTION_ARGS);
extern Datum jsonb_asin(PG_FUNCTION_ARGS);
extern Datum jsonb_atan(PG_FUNCTION_ARGS);
extern Datum jsonb_atan2(PG_FUNCTION_ARGS);
extern Datum jsonb_cos(PG_FUNCTION_ARGS);
extern Datum jsonb_cot(PG_FUNCTION_ARGS);
extern Datum jsonb_degrees(PG_FUNCTION_ARGS);
extern Datum jsonb_radians(PG_FUNCTION_ARGS);
extern Datum jsonb_sin(PG_FUNCTION_ARGS);
extern Datum jsonb_tan(PG_FUNCTION_ARGS);

/* string */
extern Datum jsonb_left(PG_FUNCTION_ARGS);
extern Datum jsonb_ltrim(PG_FUNCTION_ARGS);
extern Datum jsonb_replace(PG_FUNCTION_ARGS);
extern Datum jsonb_reverse(PG_FUNCTION_ARGS);
extern Datum jsonb_right(PG_FUNCTION_ARGS);
extern Datum jsonb_rtrim(PG_FUNCTION_ARGS);
extern Datum jsonb_substr_no_len(PG_FUNCTION_ARGS);
extern Datum jsonb_substr(PG_FUNCTION_ARGS);
extern Datum jsonb_tolower(PG_FUNCTION_ARGS);
extern Datum jsonb_tostring(PG_FUNCTION_ARGS);
extern Datum jsonb_toupper(PG_FUNCTION_ARGS);
extern Datum jsonb_trim(PG_FUNCTION_ARGS);
extern Datum jsonb_string_starts_with(PG_FUNCTION_ARGS);
extern Datum jsonb_string_ends_with(PG_FUNCTION_ARGS);
extern Datum jsonb_string_contains(PG_FUNCTION_ARGS);
extern Datum jsonb_string_regex(PG_FUNCTION_ARGS);

/* for array supports */
extern Datum array_head(PG_FUNCTION_ARGS);
extern Datum array_last(PG_FUNCTION_ARGS);
extern Datum array_tail(PG_FUNCTION_ARGS);

/* utility */
extern Datum get_last_graph_write_stats(PG_FUNCTION_ARGS);

#endif	/* CYPHER_FUNCS_H */