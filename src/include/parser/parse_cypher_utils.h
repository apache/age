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

#ifndef PG_GRAPH_PARSE_CYPHER_UTILS_H
#define PG_GRAPH_PARSE_CYPHER_UTILS_H

#include "parser/parse_node.h"

extern Node *makeJsonbFuncAccessor(ParseState *pstate, Node *expr, List *path);
extern bool IsJsonbAccessor(Node *expr);
extern void getAccessorArguments(Node *node, Node **expr, List **path);
extern bool ConvertReservedColumnRefForIndex(Node *node, Oid relid);

extern Alias *makeAliasNoDup(char *aliasname, List *colnames);
extern Alias *makeAliasOptUnique(char *aliasname);
extern char *genUniqueName(void);

extern  void makeExtraFromNSItem(ParseNamespaceItem *nsitem, RangeTblRef **rtr,
								 bool visible);
extern void addNSItemToJoinlist(ParseState *pstate, ParseNamespaceItem *nsitem,
								bool visible);

extern Var *make_var(ParseState *pstate, ParseNamespaceItem *nsitem,
					 AttrNumber attnum, int location);

#endif