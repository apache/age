/*-------------------------------------------------------------------------
 *
 * ag_graph.h
 *	  definition of the system "graph" relation (ag_graph)
 *	  along with the relation's initial contents.
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
 * src/include/catalog/ag_graph.h
 *
 * NOTES
 *	  the genbki.pl script reads this file and generates .bki
 *	  information from the DATA() statements.
 *
 *-------------------------------------------------------------------------
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

DECLARE_UNIQUE_INDEX_PKEY(ag_graph_oid_index, 7041, on ag_graph using btree(oid oid_ops));
#define GraphOidIndexId 7041
DECLARE_UNIQUE_INDEX(ag_graph_graphname_index, 7042, on ag_graph using btree(graphname name_ops));
#define GraphNameIndexId 7042

#endif							/* AG_GRAPH_H */
