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

#ifndef GRAPHCMDS_H
#define GRAPHCMDS_H

#include "nodes/params.h"
#include "nodes/parsenodes.h"

extern void CreateGraphCommand(CreateGraphStmt *stmt, const char *queryString,
							   int stmt_location, int stmt_len);
extern void RemoveGraphById(Oid graphid);
extern ObjectAddress RenameGraph(const char *oldname, const char *newname);

extern void CreateLabelCommand(CreateLabelStmt *labelStmt,
							   const char *queryString, int stmt_location,
							   int stmt_len, ParamListInfo params);
extern ObjectAddress RenameLabel(RenameStmt *stmt);
extern void CheckLabelType(ObjectType type, Oid laboid, const char *command);
extern void CheckInheritLabel(CreateStmt *stmt);

extern bool RangeVarIsLabel(RangeVar *rel);
extern bool RelationIsLabel(Relation rel);

extern void CreateConstraintCommand(CreateConstraintStmt *constraintStmt,
									const char *queryString, int stmt_location,
									int stmt_len, ParamListInfo params);
extern void DropConstraintCommand(DropConstraintStmt *constraintStmt,
								  const char *queryString, int stmt_location,
								  int stmt_len, ParamListInfo params);

extern Oid DisableIndexCommand(DisableIndexStmt *disableStmt);

extern bool isEmptyLabel(char *label_name);
extern void deleteRelatedEdges(const char *vlab);

#endif	/* GRAPHCMDS_H */
