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

#ifndef AG_GRAPH_H
#define AG_GRAPH_H

#include "catalog/genbki.h"
#include "catalog/ag_graph_d.h"

CATALOG(ag_graph,7040,GraphRelationId) BKI_SCHEMA_MACRO
{
	Oid			oid;
	NameData	graphname;
	Oid			nspid;
} FormData_ag_graph;

/* ----------------
 *		Form_ag_graph corresponds to a pointer to a tuple with
 *		the format of ag_graph relation.
 * ----------------
 */
typedef FormData_ag_graph *Form_ag_graph;

#endif   /* AG_GRAPH_H */
