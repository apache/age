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

#include "postgres.h"

#include "ag_const.h"
#include "access/heapam.h"
#include "access/htup_details.h"
#include "catalog/ag_graph_fn.h"
#include "catalog/ag_label.h"
#include "catalog/ag_label_fn.h"
#include "catalog/catalog.h"
#include "catalog/indexing.h"
#include "commands/sequence.h"
#include "utils/builtins.h"
#include "utils/graph.h"
#include "utils/rel.h"
#include "utils/lsyscache.h"
#include "utils/syscache.h"
#include "miscadmin.h"

static void InsertAgLabelTuple(Relation ag_label_desc, Oid laboid,
							   RangeVar *label, Oid relid, char labkind,
							   bool is_fixed_id, int32 fixed_id);
static uint16 GetNewLabelId(char *graphname, Oid graphid);

/* Potentially set by pg_upgrade_support functions */
Oid binary_upgrade_next_ag_label_oid = InvalidOid;

Oid
label_create_with_catalog(RangeVar *label, Oid relid, char labkind,
						  Oid labtablespace, bool is_fixed_id, int32 fixed_id)
{
	Relation	ag_label_desc;
	Oid			laboid;

	ag_label_desc = table_open(LabelRelationId, RowExclusiveLock);

	if (IsBinaryUpgrade)
	{
		if (!OidIsValid(binary_upgrade_next_ag_label_oid))
			ereport(ERROR,
					(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
							errmsg("ag_label OID value not set when in binary upgrade mode")));
		laboid = binary_upgrade_next_ag_label_oid;
		binary_upgrade_next_ag_label_oid = InvalidOid;
	}
	else
	{
		laboid = GetNewRelFileNode(labtablespace, ag_label_desc,
							   label->relpersistence);
	}

	InsertAgLabelTuple(ag_label_desc, laboid, label, relid, labkind,
					   is_fixed_id, fixed_id);

	table_close(ag_label_desc, RowExclusiveLock);

	return laboid;
}

/*
 * Remove ag_label row for the given laboid
 *
 * See DeleteRelationTuple()
 */
void
label_drop_with_catalog(Oid laboid)
{
	Relation	ag_label_desc;
	HeapTuple	tup;

	ag_label_desc = table_open(LabelRelationId, RowExclusiveLock);

	tup = SearchSysCache1(LABELOID, ObjectIdGetDatum(laboid));
	if (!HeapTupleIsValid(tup))
		elog(ERROR, "cache lookup failed for label %u", laboid);

	simple_heap_delete(ag_label_desc, &tup->t_self);

	ReleaseSysCache(tup);

	table_close(ag_label_desc, RowExclusiveLock);
}

/*
 * InsertAgLabelTuple - register the new label in ag_label
 *
 * See InsertPgClassTuple()
 */
static void
InsertAgLabelTuple(Relation ag_label_desc, Oid laboid, RangeVar *label,
				   Oid relid, char labkind, bool is_fixed_id, int32 fixed_id)
{
	Oid			graphid = get_graphname_oid(label->schemaname);
	char		labname[NAMEDATALEN]={'\0'};
	int32		labid;
	Datum		values[Natts_ag_label];
	bool		nulls[Natts_ag_label];
	HeapTuple	tup;

	AssertArg(labkind == LABEL_KIND_VERTEX || labkind == LABEL_KIND_EDGE);

	if (is_fixed_id)
	{
		labid = fixed_id;
	}
	else
		labid = (int32) GetNewLabelId(label->schemaname, graphid);
	strcpy(labname, label->relname);

	values[Anum_ag_label_oid - 1] = ObjectIdGetDatum(laboid);
	values[Anum_ag_label_labname - 1] = CStringGetDatum(labname);
	values[Anum_ag_label_graphid - 1] = ObjectIdGetDatum(graphid);
	values[Anum_ag_label_labid - 1] = Int32GetDatum(labid);
	values[Anum_ag_label_relid - 1] = ObjectIdGetDatum(relid);
	values[Anum_ag_label_labkind - 1] = CharGetDatum(labkind);

	memset(nulls, false, sizeof(nulls));

	tup = heap_form_tuple(RelationGetDescr(ag_label_desc), values, nulls);

	CatalogTupleInsert(ag_label_desc, tup);

	heap_freetuple(tup);
}

static uint16
GetNewLabelId(char *graphname, Oid graphid)
{
	char		sname[128];
	Datum		stext;
	uint16		labid;
	int			cnt;

	snprintf(sname, 128, "\"%s\".\"%s\"", graphname, AG_LABEL_SEQ);
	stext = CStringGetTextDatum(sname);

	cnt = 0;
	for (;;)
	{
		Datum val;

		val = DirectFunctionCall1(nextval, stext);
		labid = DatumGetUInt16(val);
		if (!labid_exists(graphid, labid))
			break;

		if (++cnt >= GRAPHID_LABID_MAX)
			ereport(ERROR,
					(errcode(ERRCODE_PROGRAM_LIMIT_EXCEEDED),
					 errmsg("no more new labels are available")));
	}

	return labid;
}
