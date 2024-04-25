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

#ifndef AG_EDGE_H
#define AG_EDGE_H

#include "catalog/genbki.h"
#include "catalog/ag_edge_d.h"

/* ----------------
 *		ag_edge definition.  cpp turns this into
 *		typedef struct FormData_ag_edge
 * ----------------
 */
CATALOG(ag_edge,7020,EdgeRelationId) BKI_BOOTSTRAP BKI_ROWTYPE_OID(7022,EdgeRelation_Rowtype_Id) BKI_SCHEMA_MACRO
{
	graphid		id;				/* id */
	graphid		start;			/* start */
	graphid		end;			/* end */
	jsonb		properties;		/* properties */
	tid			tid;			/* tid */
} FormData_ag_edge;

/* ----------------
 *		Form_ag_edge corresponds to a pointer to a tuple with
 *		the format of ag_edge relation.
 * ----------------
 */
typedef FormData_ag_edge *Form_ag_edge;

#endif   /* AG_EDGE_H */
