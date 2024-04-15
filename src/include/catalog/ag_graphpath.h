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

#ifndef AG_GRAPHPATH_H
#define AG_GRAPHPATH_H

#include "catalog/genbki.h"
#include "catalog/ag_graphpath_d.h"

/* ----------------
 *		ag_graphpath definition.  cpp turns this into
 *		typedef struct FormData_ag_graphpath
 * ----------------
 */
CATALOG(ag_graphpath,7030,GraphPathRelationId) BKI_BOOTSTRAP BKI_ROWTYPE_OID(7032,GraphPathRelation_Rowtype_Id) BKI_SCHEMA_MACRO
{
	_vertex		vertices;		/* id */
	_edge		edges;			/* properties */
} FormData_ag_graphpath;

/* ----------------
 *		Form_ag_graphpath corresponds to a pointer to a tuple with
 *		the format of ag_graphpath relation.
 * ----------------
 */
typedef FormData_ag_graphpath *Form_ag_graphpath;

#endif   /* AG_GRAPHPATH_H */
