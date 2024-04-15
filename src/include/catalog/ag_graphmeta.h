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

#ifndef AG_GRAPHMETA_H
#define AG_GRAPHMETA_H

#include "catalog/genbki.h"
#include "catalog/ag_graphmeta_d.h"
#include "pgstat.h"

/* ----------------
 *		ag_graphmeta definition.  cpp turns this into
 *		typedef struct FormData_ag_graphmeta
 * ----------------
 */
CATALOG(ag_graphmeta,7055,GraphMetaRelationId) BKI_SCHEMA_MACRO
{
	Oid		graph;            /* graph oid */
	int16	edge;            /* edge label id */
	int16	start;            /* start vertex label id */
	int16	end;            /* end vertex label id */
	int64	edgecount;        /* # of edge between start and end */
} FormData_ag_graphmeta;

/* ----------------
 *		Form_ag_graphmeta corresponds to a pointer to a tuple with
 *		the format of ag_graphmeta relation.
 * ----------------
 */
typedef FormData_ag_graphmeta *Form_ag_graphmeta;

typedef struct AgStat_key
{
	Oid   graph;
	Labid edge;
	Labid start;
	Labid end;
}                             AgStat_key;

typedef struct AgStat_GraphMeta
{
	struct AgStat_key key;

	PgStat_Counter edges_inserted;
	PgStat_Counter edges_deleted;
}                             AgStat_GraphMeta;

#endif   /* AG_GRAPHMETA_H */
