/*
 * execCypherSet.c
 *	  routines to handle ModifyGraph set nodes.
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
 * IDENTIFICATION
 *	  src/backend/executor/execCypherSet.c
 */

#include "postgres.h"

#include "executor/execCypherSet.h"
#include "nodes/nodeFuncs.h"
#include "executor/executor.h"
#include "executor/nodeModifyGraph.h"
#include "utils/datum.h"
#include "access/tableam.h"
#include "utils/lsyscache.h"
#include "access/xact.h"
#include "catalog/ag_vertex_d.h"
#include "catalog/ag_edge_d.h"
#include "commands/trigger.h"

#define DatumGetItemPointer(X)	 ((ItemPointer) DatumGetPointer(X))

static TupleTableSlot *copyVirtualTupleTableSlot(TupleTableSlot *dstslot,
												 TupleTableSlot *srcslot);
static void findAndReflectNewestValue(ModifyGraphState *mgstate,
									  TupleTableSlot *slot);
static void updateElementTable(ModifyGraphState *mgstate, Datum gid,
							   Datum newelem);
static Datum GraphTableTupleUpdate(ModifyGraphState *mgstate,
								   Oid tts_value_type, Datum tts_value,
								   int attidx);

/*
 * LegacyExecSetGraph
 *
 * It is used for Merge statements or Eager.
 */
TupleTableSlot *
LegacyExecSetGraph(ModifyGraphState *mgstate, TupleTableSlot *slot, GSPKind kind)
{
	ModifyGraph *plan = (ModifyGraph *) mgstate->ps.plan;
	ExprContext *econtext = mgstate->ps.ps_ExprContext;
	ListCell   *ls;
	TupleTableSlot *result = mgstate->ps.ps_ResultTupleSlot;

	/*
	 * The results of previous clauses should be preserved. So, shallow
	 * copying is used.
	 */
	copyVirtualTupleTableSlot(result, slot);

	/*
	 * Reflect the newest value all types of scantuple before evaluating
	 * expression.
	 */
	findAndReflectNewestValue(mgstate, econtext->ecxt_scantuple);
	findAndReflectNewestValue(mgstate, econtext->ecxt_innertuple);
	findAndReflectNewestValue(mgstate, econtext->ecxt_outertuple);

	foreach(ls, mgstate->sets)
	{
		GraphSetProp *gsp = lfirst(ls);
		Oid			elemtype;
		Datum		elem_datum;
		Datum		expr_datum;
		bool		isNull;
		Datum		gid;
		Datum		tid;
		Datum		newelem;
		MemoryContext oldmctx;
		AttrNumber	attnum;

		if (gsp->kind != kind)
		{
			Assert(kind != GSP_NORMAL);
			continue;
		}

		elemtype = exprType((Node *) gsp->es_elem->expr);
		if (elemtype != VERTEXOID && elemtype != EDGEOID)
			elog(ERROR, "expected node or relationship");

		/* store intermediate results in tuple memory context */
		oldmctx = MemoryContextSwitchTo(econtext->ecxt_per_tuple_memory);

		/* get original graph element */
		attnum = ((Var *) gsp->es_elem->expr)->varattno;
		elem_datum = ExecEvalExpr(gsp->es_elem, econtext, &isNull);
		if (isNull)
			ereport(ERROR,
					(errcode(ERRCODE_NULL_VALUE_NOT_ALLOWED),
					 errmsg("updating NULL is not allowed")));

		/* evaluate SET expression */
		if (elemtype == VERTEXOID)
		{
			gid = getVertexIdDatum(elem_datum);
			tid = getVertexTidDatum(elem_datum);
		}
		else
		{
			Assert(elemtype == EDGEOID);

			gid = getEdgeIdDatum(elem_datum);
			tid = getEdgeTidDatum(elem_datum);
		}

		expr_datum = ExecEvalExpr(gsp->es_expr, econtext, &isNull);
		if (isNull)
			ereport(ERROR,
					(errcode(ERRCODE_NULL_VALUE_NOT_ALLOWED),
					 errmsg("property map cannot be NULL")));

		newelem = makeModifiedElem(elem_datum, elemtype, gid, expr_datum, tid);

		MemoryContextSwitchTo(oldmctx);

		updateElementTable(mgstate, gid, newelem);

		/*
		 * To use the modified data in the next iteration, modifying the data
		 * in the ExprContext.
		 */
		setSlotValueByAttnum(econtext->ecxt_scantuple, newelem, attnum);
		setSlotValueByAttnum(econtext->ecxt_innertuple, newelem, attnum);
		setSlotValueByAttnum(econtext->ecxt_outertuple, newelem, attnum);
		setSlotValueByAttnum(result, newelem, attnum);
	}

	return (plan->last ? NULL : result);
}

/*
 * ExecSetGraph
 */
TupleTableSlot *
ExecSetGraph(ModifyGraphState *mgstate, TupleTableSlot *slot)
{
	ModifyGraph *plan = (ModifyGraph *) mgstate->ps.plan;
	bool	   *update_cols = mgstate->update_cols;
	int			i;
	ListCell   *lc;

	if (update_cols == NULL)
	{
		update_cols = palloc(sizeof(bool) * slot->tts_tupleDescriptor->natts);
		mgstate->update_cols = update_cols;

		for (i = 0; i < slot->tts_tupleDescriptor->natts; i++)
		{
			update_cols[i] = false;
			foreach(lc, mgstate->sets)
			{
				char	   *attr_name =
				NameStr(slot->tts_tupleDescriptor->attrs[i].attname);
				GraphSetProp *gsp = lfirst(lc);

				if (strcmp(gsp->variable, attr_name) == 0)
				{
					update_cols[i] = true;
					break;
				}
			}
		}
	}

	for (i = 0; i < slot->tts_tupleDescriptor->natts; i++)
	{
		if (update_cols[i])
		{
			Datum		cur_datum = slot->tts_values[i];
			Oid			element_type = slot->tts_tupleDescriptor->attrs[i].atttypid;
			Datum		affected_datum = GraphTableTupleUpdate(mgstate,
															   element_type,
															   cur_datum,
															   i);

			if (affected_datum != (Datum) 0)
			{
				slot->tts_values[i] = affected_datum;
			}
		}
	}

	return (plan->last ? NULL : slot);
}

static TupleTableSlot *
copyVirtualTupleTableSlot(TupleTableSlot *dstslot, TupleTableSlot *srcslot)
{
	int			natts = srcslot->tts_tupleDescriptor->natts;

	ExecClearTuple(dstslot);
	ExecSetSlotDescriptor(dstslot, srcslot->tts_tupleDescriptor);

	/* shallow copy */
	memcpy(dstslot->tts_values, srcslot->tts_values, natts * sizeof(Datum));
	memcpy(dstslot->tts_isnull, srcslot->tts_isnull, natts * sizeof(bool));

	ExecStoreVirtualTuple(dstslot);

	return dstslot;
}

/*
 * findAndReflectNewestValue
 *
 * If a tuple with already updated exists, the data is taken from the elemTable
 * in ModifyGraphState and reflecting in the tuple data currently working on.
 */
static void
findAndReflectNewestValue(ModifyGraphState *mgstate, TupleTableSlot *slot)
{
	int			i;

	if (slot == NULL)
		return;

	for (i = 0; i < slot->tts_tupleDescriptor->natts; i++)
	{
		bool		found;
		Datum		finalValue;
		Oid			type_oid;

		if (slot->tts_isnull[i] ||
			slot->tts_tupleDescriptor->attrs[i].attisdropped)
			continue;

		type_oid = slot->tts_tupleDescriptor->attrs[i].atttypid;
		switch (type_oid)
		{
			case VERTEXOID:
				{
					Datum		graphid = getVertexIdDatum(slot->tts_values[i]);

					finalValue = getElementFromEleTable(mgstate, type_oid, 0,
														graphid,
														&found);
					if (!found)
					{
						continue;
					}
				}
				break;
			case EDGEOID:
				{
					Datum		graphid = getEdgeIdDatum(slot->tts_values[i]);

					finalValue = getElementFromEleTable(mgstate, type_oid, 0,
														graphid,
														&found);
					if (!found)
					{
						continue;
					}
				}
				break;
			case GRAPHPATHOID:
				finalValue = getPathFinal(mgstate, slot->tts_values[i]);
				break;
			default:
				continue;
		}

		setSlotValueByAttnum(slot, finalValue, i + 1);
	}
}

/*
 * GraphTableTupleUpdate
 * 		Update the tuple in the graph table.
 *
 * See ExecUpdate()
 */
static Datum
GraphTableTupleUpdate(ModifyGraphState *mgstate, Oid tts_value_type,
					  Datum tts_value, int attidx)
{
	EState	   *estate = mgstate->ps.state;
	EPQState   *epqstate = &mgstate->mt_epqstate;
	TupleTableSlot *elemTupleSlot = mgstate->elemTupleSlot;
	Datum	   *tts_values;
	ResultRelInfo *resultRelInfo;
	Relation	resultRelationDesc;
	LockTupleMode lockmode;
	TM_Result	result;
	TM_FailureData tmfd;
	bool		update_indexes;
	Datum		gid;
	Oid			relid;
	ItemPointer ctid;
	bool		hash_found;
	ModifiedElemEntry *entry;
	Datum		inserted_datum;
	List	   *recheckIndexes = NIL;

	if (tts_value_type == VERTEXOID)
	{
		gid = getVertexIdDatum(tts_value);
		ctid = DatumGetItemPointer(getVertexTidDatum(tts_value));
	}
	else
	{
		ctid = DatumGetItemPointer(getEdgeTidDatum(tts_value));
		gid = getEdgeIdDatum(tts_value);
	}

	hash_search(mgstate->elemTable, &gid, HASH_FIND,
				&hash_found);
	if (hash_found)
	{
		if (!enable_multiple_update)
		{
			ereport(WARNING,
					(errcode(ERRCODE_INTERNAL_ERROR),
					 errmsg("graph element(%hu," UINT64_FORMAT ") has been SET multiple times",
							GraphidGetLabid(DatumGetGraphid(gid)),
							GraphidGetLocid(DatumGetGraphid(gid)))));
		}
		return (Datum) 0;
	}

	relid = get_labid_relid(mgstate->graphid,
							GraphidGetLabid(DatumGetGraphid(gid)));
	resultRelInfo = getResultRelInfo(mgstate, relid);

	resultRelationDesc = resultRelInfo->ri_RelationDesc;

	/*
	 * Create a tuple to store. Attributes of vertex/edge label are not the
	 * same with those of vertex/edge.
	 */
	ExecClearTuple(elemTupleSlot);
	ExecSetSlotDescriptor(elemTupleSlot,
						  RelationGetDescr(resultRelInfo->ri_RelationDesc));

	tts_values = elemTupleSlot->tts_values;

	if (tts_value_type == VERTEXOID)
	{
		tts_values[Anum_ag_vertex_id - 1] = gid;
		tts_values[Anum_ag_vertex_properties - 1] = getVertexPropDatum(tts_value);
	}
	else
	{
		Assert(tts_value_type == EDGEOID);

		tts_values[Anum_ag_edge_id - 1] = gid;
		tts_values[Anum_ag_edge_start - 1] = getEdgeStartDatum(tts_value);
		tts_values[Anum_ag_edge_end - 1] = getEdgeEndDatum(tts_value);
		tts_values[Anum_ag_edge_properties - 1] = getEdgePropDatum(tts_value);
	}
	MemSet(elemTupleSlot->tts_isnull, false,
		   elemTupleSlot->tts_tupleDescriptor->natts * sizeof(bool));
	ExecStoreVirtualTuple(elemTupleSlot);

	/* BEFORE ROW UPDATE Triggers */
	if (resultRelInfo->ri_TrigDesc &&
		resultRelInfo->ri_TrigDesc->trig_update_before_row)
	{
		if (!ExecBRUpdateTriggers(estate, epqstate, resultRelInfo,
								  ctid, NULL, elemTupleSlot))
			return (Datum) 0;
	}

lreplace:
	ExecMaterializeSlot(elemTupleSlot);
	elemTupleSlot->tts_tableOid = RelationGetRelid(resultRelationDesc);

	if (resultRelationDesc->rd_att->constr)
		ExecConstraints(resultRelInfo, elemTupleSlot, estate);

	result = table_tuple_update(resultRelationDesc, ctid, elemTupleSlot,
								mgstate->modify_cid + MODIFY_CID_SET,
								estate->es_snapshot,
								estate->es_crosscheck_snapshot,
								true /* wait for commit */ ,
								&tmfd, &lockmode, &update_indexes);

	switch (result)
	{
		case TM_Ok:
			break;
		case TM_Updated:
			{
				TupleTableSlot *epqslot;
				TupleTableSlot *inputslot;
				ModifyGraph *plan = (ModifyGraph *) mgstate->ps.plan;

				if (plan->nr_modify > 0)
				{
					ereport(ERROR,
							(errcode(ERRCODE_T_R_SERIALIZATION_FAILURE),
							 errmsg("could not serialize access due to concurrent update")));
				}

				if (IsolationUsesXactSnapshot())
					ereport(ERROR,
							(errcode(ERRCODE_T_R_SERIALIZATION_FAILURE),
							 errmsg("could not serialize access due to concurrent update")));

				/*
				 * Already know that we're going to need to do EPQ, so fetch
				 * tuple directly into the right slot.
				 */
				inputslot = EvalPlanQualSlot(epqstate, resultRelationDesc,
											 resultRelInfo->ri_RangeTableIndex);

				result = table_tuple_lock(resultRelationDesc, ctid,
										  estate->es_snapshot,
										  inputslot, GetCurrentCommandId(true),
										  lockmode, LockWaitBlock,
										  TUPLE_LOCK_FLAG_FIND_LAST_VERSION,
										  &tmfd);

				switch (result)
				{
					case TM_Ok:
						Assert(tmfd.traversed);

						epqslot = EvalPlanQual(epqstate,
											   resultRelationDesc,
											   resultRelInfo->ri_RangeTableIndex,
											   inputslot);

						if (TupIsNull(epqslot))
							/* Tuple not passing quals anymore, exiting... */
							return (Datum) 0;

						slot_getallattrs(epqslot);
						Assert(!epqslot->tts_isnull[attidx]);
						tts_value = epqslot->tts_values[attidx];

						if (tts_value_type == VERTEXOID)
						{
							tts_values[Anum_ag_vertex_properties - 1] = getVertexPropDatum(tts_value);
						}
						else
						{
							tts_values[Anum_ag_edge_start - 1] = getEdgeStartDatum(tts_value);
							tts_values[Anum_ag_edge_end - 1] = getEdgeEndDatum(tts_value);
							tts_values[Anum_ag_edge_properties - 1] = getEdgePropDatum(tts_value);
						}

						goto lreplace;
					case TM_Deleted:
						/* tuple already deleted; nothing to do */
						return (Datum) 0;

					case TM_SelfModified:

						/*
						 * This can be reached when following an update chain
						 * from a tuple updated by another session, reaching a
						 * tuple that was already updated in this transaction.
						 * If previously modified by this command, ignore the
						 * redundant update, otherwise error out.
						 *
						 * See also TM_SelfModified response to
						 * table_tuple_update() above.
						 */
						if (tmfd.cmax != estate->es_output_cid)
							ereport(ERROR,
									(errcode(ERRCODE_TRIGGERED_DATA_CHANGE_VIOLATION),
									 errmsg("tuple to be updated was already modified by an operation triggered by the current command"),
									 errhint("Consider using an AFTER trigger instead of a BEFORE trigger to propagate changes to other rows.")));
						return (Datum) 0;
					default:
						/* see table_tuple_lock call in ExecDelete() */
						elog(ERROR, "unexpected table_tuple_lock status: %u",
							 result);
				}
				break;
			}
		default:
			elog(ERROR, "unrecognized heap_update status: %u", result);
	}

	if (resultRelInfo->ri_NumIndices > 0 && update_indexes)
		recheckIndexes = ExecInsertIndexTuples(resultRelInfo,
											   elemTupleSlot,
											   estate,
											   true,
											   false,
											   NULL,
											   NIL);

	graphWriteStats.updateProperty++;

	/* AFTER ROW UPDATE Triggers */
	ExecARUpdateTriggers(estate, resultRelInfo, ctid, NULL, elemTupleSlot,
						 recheckIndexes, NULL);

	list_free(recheckIndexes);

	entry = hash_search(mgstate->elemTable, &gid, HASH_ENTER, &hash_found);

	if (tts_value_type == VERTEXOID)
	{
		inserted_datum = makeGraphVertexDatum(gid,
											  tts_values[Anum_ag_vertex_properties - 1],
											  PointerGetDatum(&elemTupleSlot->tts_tid));
	}
	else
	{
		inserted_datum = makeGraphEdgeDatum(gid,
											tts_values[Anum_ag_edge_start - 1],
											tts_values[Anum_ag_edge_end - 1],
											tts_values[Anum_ag_edge_properties - 1],
											PointerGetDatum(&elemTupleSlot->tts_tid));
	}
	entry->elem = inserted_datum;
	return inserted_datum;
}

/* See ExecUpdate() */
ItemPointer
LegacyUpdateElemProp(ModifyGraphState *mgstate, Oid elemtype, Datum gid,
					 Datum elem_datum)
{
	EState	   *estate = mgstate->ps.state;
	EPQState   *epqstate = &mgstate->mt_epqstate;
	TupleTableSlot *elemTupleSlot = mgstate->elemTupleSlot;
	Oid			relid;
	ItemPointer ctid;
	ResultRelInfo *resultRelInfo;
	Relation	resultRelationDesc;
	LockTupleMode lockmode;
	TM_Result	result;
	TM_FailureData tmfd;
	bool		update_indexes;
	List	   *recheckIndexes = NIL;

	relid = get_labid_relid(mgstate->graphid,
							GraphidGetLabid(DatumGetGraphid(gid)));
	resultRelInfo = getResultRelInfo(mgstate, relid);
	resultRelationDesc = resultRelInfo->ri_RelationDesc;

	/*
	 * Create a tuple to store. Attributes of vertex/edge label are not the
	 * same with those of vertex/edge.
	 */
	ExecClearTuple(elemTupleSlot);
	ExecSetSlotDescriptor(elemTupleSlot,
						  RelationGetDescr(resultRelInfo->ri_RelationDesc));
	if (elemtype == VERTEXOID)
	{
		elemTupleSlot->tts_values[0] = gid;
		elemTupleSlot->tts_values[1] = getVertexPropDatum(elem_datum);

		ctid = (ItemPointer) DatumGetPointer(getVertexTidDatum(elem_datum));
	}
	else
	{
		Assert(elemtype == EDGEOID);

		elemTupleSlot->tts_values[0] = gid;
		elemTupleSlot->tts_values[1] = getEdgeStartDatum(elem_datum);
		elemTupleSlot->tts_values[2] = getEdgeEndDatum(elem_datum);
		elemTupleSlot->tts_values[3] = getEdgePropDatum(elem_datum);

		ctid = (ItemPointer) DatumGetPointer(getEdgeTidDatum(elem_datum));
	}
	MemSet(elemTupleSlot->tts_isnull, false,
		   elemTupleSlot->tts_tupleDescriptor->natts * sizeof(bool));
	ExecStoreVirtualTuple(elemTupleSlot);

	/* BEFORE ROW UPDATE Triggers */
	if (resultRelInfo->ri_TrigDesc &&
		resultRelInfo->ri_TrigDesc->trig_update_before_row)
	{
		if (!ExecBRUpdateTriggers(estate, epqstate, resultRelInfo,
								  ctid, NULL, elemTupleSlot))
		{
			elog(ERROR, "Trigger must not be NULL on Cypher Clause.");
			return NULL;
		}
	}

	ExecMaterializeSlot(elemTupleSlot);
	elemTupleSlot->tts_tableOid = RelationGetRelid(resultRelationDesc);

	if (resultRelationDesc->rd_att->constr)
		ExecConstraints(resultRelInfo, elemTupleSlot, estate);

	result = table_tuple_update(resultRelationDesc, ctid, elemTupleSlot,
								mgstate->modify_cid + MODIFY_CID_SET,
								estate->es_snapshot,
								estate->es_crosscheck_snapshot,
								true /* wait for commit */ ,
								&tmfd, &lockmode, &update_indexes);

	switch (result)
	{
		case TM_SelfModified:
			ereport(WARNING,
					(errcode(ERRCODE_INTERNAL_ERROR),
					 errmsg("graph element(%hu," UINT64_FORMAT ") has been SET multiple times",
							GraphidGetLabid(DatumGetGraphid(gid)),
							GraphidGetLocid(DatumGetGraphid(gid)))));
		case TM_Ok:
			break;
		case TM_Updated:
			/* TODO: A solution to concurrent update is needed. */
			ereport(ERROR,
					(errcode(ERRCODE_T_R_SERIALIZATION_FAILURE),
					 errmsg("could not serialize access due to concurrent update")));
		default:
			elog(ERROR, "unrecognized heap_update status: %u", result);
	}

	if (resultRelInfo->ri_NumIndices > 0 && update_indexes)
		recheckIndexes = ExecInsertIndexTuples(resultRelInfo,
											   elemTupleSlot,
											   estate,
											   true,
											   false,
											   NULL,
											   NIL);

	graphWriteStats.updateProperty++;

	/* AFTER ROW UPDATE Triggers */
	ExecARUpdateTriggers(estate, resultRelInfo, ctid, NULL, elemTupleSlot,
						 recheckIndexes, NULL);

	list_free(recheckIndexes);

	return &elemTupleSlot->tts_tid;
}

Datum
makeModifiedElem(Datum elem, Oid elemtype,
				 Datum id, Datum prop_map, Datum tid)
{
	Datum		result;

	if (elemtype == VERTEXOID)
	{
		result = makeGraphVertexDatum(id, prop_map, tid);
	}
	else
	{
		Datum		start;
		Datum		end;

		start = getEdgeStartDatum(elem);
		end = getEdgeEndDatum(elem);

		result = makeGraphEdgeDatum(id, start, end, prop_map, tid);
	}

	return result;
}

static void
updateElementTable(ModifyGraphState *mgstate, Datum gid, Datum newelem)
{
	ModifiedElemEntry *entry;
	bool		found;

	entry = hash_search(mgstate->elemTable, &gid, HASH_ENTER, &found);
	if (found)
	{
		if (enable_multiple_update)
			pfree(DatumGetPointer(entry->elem));
		else
			ereport(WARNING,
					(errcode(ERRCODE_INTERNAL_ERROR),
					 errmsg("graph element(%hu," UINT64_FORMAT ") has been SET multiple times",
							GraphidGetLabid(entry->key),
							GraphidGetLocid(entry->key))));
	}

	entry->elem = datumCopy(newelem, false, -1);
}
