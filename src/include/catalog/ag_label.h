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

#ifndef AG_LABEL_H
#define AG_LABEL_H

#include "catalog/genbki.h"
#include "catalog/ag_label_d.h"

/* ----------------
 *		ag_label definition.  cpp turns this into
 *		typedef struct FormData_ag_label
 * ----------------
 */
CATALOG(ag_label,7045,LabelRelationId) BKI_SCHEMA_MACRO
{
	Oid			oid;			/* oid */
	NameData	labname;		/* label name */
	Oid         graphid;		/* graph oid */
	int32       labid;			/* label ID in a graph */
	Oid         relid;			/* table oid under the label */
	char        labkind;		/* see LABEL_KIND_XXX constants below */
} FormData_ag_label;

/* ----------------
 *		Form_ag_label corresponds to a pointer to a tuple with
 *		the format of ag_label relation.
 * ----------------
 */
typedef FormData_ag_label *Form_ag_label;

#define LABEL_KIND_VERTEX	'v'
#define LABEL_KIND_EDGE		'e'

#endif   /* AG_LABEL_H */
