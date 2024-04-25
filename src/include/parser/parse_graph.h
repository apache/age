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

#ifndef PARSE_GRAPH_H
#define PARSE_GRAPH_H

#include "parser/parse_node.h"

extern bool enable_eager;

extern Query *transformCypherSubPattern(ParseState *pstate,
										CypherSubPattern *subpat);
extern Query *transformCypherProjection(ParseState *pstate,
										CypherClause *clause);
extern Query *transformCypherMatchClause(ParseState *pstate,
										 CypherClause *clause);
extern Query *transformCypherCreateClause(ParseState *pstate,
										  CypherClause *clause);
extern Query *transformCypherDeleteClause(ParseState *pstate,
										  CypherClause *clause);
extern Query *transformCypherSetClause(ParseState *pstate,
									   CypherClause *clause);
extern Query *transformCypherMergeClause(ParseState *pstate,
										 CypherClause *clause);
extern Query *transformCypherLoadClause(ParseState *pstate,
										CypherClause *clause);
extern Query *transformCypherUnwindClause(ParseState *pstate,
										  CypherClause *clause);

#endif	/* PARSE_GRAPH_H */
