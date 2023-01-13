/*
 * execCypherDelete.c
 *	  routines to handle ModifyGraph delete nodes.
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
 *	  src/backend/executor/execCypherDelete.c
 */

#include "postgres.h"

#include "executor/execCypherDelete.h"
#include "executor/executor.h"
#include "executor/nodeModifyGraph.h"
#include "nodes/nodeFuncs.h"
#include "access/tableam.h"
#include "utils/lsyscache.h"
#include "access/heapam.h"
#include "pgstat.h"
#include "utils/arrayaccess.h"
#include "access/xact.h"
#include "commands/trigger.h"
#include "utils/fmgroids.h"

#define DatumGetItemPointer(X)		((ItemPointer) DatumGetPointer(X))
#define ItemPointerGetDatum(X)		PointerGetDatum(X)

static bool ExecDeleteEdgeOrVertex(ModifyGraphState *mgstate,
								   ResultRelInfo *resultRelInfo,
								   Graphid graphid, HeapTuple tuple,
								   Oid typeOid, bool required);
static void ExecDeleteGraphElement(ModifyGraphState *mgstate, Datum elem,
								   Oid type);

static void find_connected_edges(ModifyGraphState *mgstate, Graphid vertex_id);
static void find_connected_edges_internal(ModifyGraphState *mgstate,
										  ModifyGraph *plan,
										  EState *estate,
										  ResultRelInfo *resultRelInfo,
										  AttrNumber attr,
										  Datum vertex_id);

TupleTableSlot *
ExecDeleteGraph(ModifyGraphState *mgstate, TupleTableSlot *slot)
{
	ModifyGraph *plan = (ModifyGraph *) mgstate->ps.plan;
	ExprContext *econtext = mgstate->ps.ps_ExprContext;
	TupleDesc	tupDesc = slot->tts_tupleDescriptor;
	ListCell   *le;

	ResetExprContext(econtext);

	foreach(le, mgstate->exprs)
	{
		GraphDelElem *gde = castNode(GraphDelElem, lfirst(le));
		Oid			type;
		Datum		elem;
		bool		isNull;
		AttrNumber	attno = findAttrInSlotByName(slot, gde->variable);

		type = exprType((Node *) gde->elem);
		if (!(type == VERTEXOID || type == EDGEOID ||
			  type == VERTEXARRAYOID || type == EDGEARRAYOID))
			ereport(ERROR,
					(errcode(ERRCODE_DATATYPE_MISMATCH),
					 errmsg("expected node, relationship, or path")));

		econtext->ecxt_scantuple = slot;
		elem = ExecEvalExpr(gde->es_elem, econtext, &isNull);
		if (isNull)
		{
			/*
			 * This assumes that there are only variable references in the
			 * target list.
			 */
			if (type == EDGEARRAYOID)
				continue;
			else
				ereport(NOTICE,
						(errcode(ERRCODE_NULL_VALUE_NOT_ALLOWED),
						 errmsg("skipping deletion of NULL graph element")));

			continue;
		}

		/*
		 * NOTE: After all the graph elements to be removed are collected,
		 * they will be removed.
		 */
		ExecDeleteGraphElement(mgstate, elem, type);

		/*
		 * The graphpath must be passed to the next plan for deleting vertex
		 * array of the graphpath.
		 */
		if (type == EDGEARRAYOID &&
			TupleDescAttr(tupDesc, attno - 1)->atttypid == GRAPHPATHOID)
			continue;

		setSlotValueByAttnum(slot, (Datum) 0, attno);
	}

	return (plan->last ? NULL : slot);
}

/*
 * deleteElement
 * Delete the graph element.
 */
static bool
ExecDeleteEdgeOrVertex(ModifyGraphState *mgstate, ResultRelInfo *resultRelInfo,
					   Graphid graphid, HeapTuple tuple, Oid typeOid,
					   bool required)
{
	EPQState   *epqstate = &mgstate->mt_epqstate;
	EState	   *estate = mgstate->ps.state;
	Relation	resultRelationDesc;
	TM_Result	result;
	TM_FailureData tmfd;
	bool		hash_found;
	ItemPointer tupleid;

	tupleid = &tuple->t_self;

	hash_search(mgstate->elemTable, &graphid, HASH_FIND, &hash_found);
	if (hash_found)
		return false;

	resultRelationDesc = resultRelInfo->ri_RelationDesc;

	/* BEFORE ROW DELETE Triggers */
	if (resultRelInfo->ri_TrigDesc &&
		resultRelInfo->ri_TrigDesc->trig_delete_before_row)
	{
		bool		dodelete;

		dodelete = ExecBRDeleteTriggers(estate, epqstate, resultRelInfo,
										tupleid, NULL, NULL);
		if (!dodelete)
		{
			if (required)
			{
				elog(ERROR, "cannot delete required graph element, because of trigger action.");
			}
			/* "do nothing" */
			return false;
		}
	}

	/* see ExecDelete() */
	result = table_tuple_delete(resultRelationDesc,
								tupleid,
								mgstate->modify_cid + MODIFY_CID_OUTPUT,
								estate->es_snapshot,
								estate->es_crosscheck_snapshot,
								true,
								&tmfd,
								false);

	switch (result)
	{
		case TM_SelfModified:
			ereport(ERROR,
					(errcode(ERRCODE_INTERNAL_ERROR),
					 errmsg("modifying the same element more than once cannot happen")));
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

	/* AFTER ROW DELETE Triggers */
	ExecARDeleteTriggers(estate, resultRelInfo, tupleid, NULL,
						 NULL);

	if (typeOid == EDGEOID)
	{
		graphWriteStats.deleteEdge++;

		if (auto_gather_graphmeta)
		{
			bool		isnull;
			TupleDesc	tupleDesc = RelationGetDescr(resultRelInfo->ri_RelationDesc);
			Graphid		edge_start_id = DatumGetGraphid(
														heap_getattr(tuple,
																	 Anum_table_edge_start,
																	 tupleDesc,
																	 &isnull)
			);
			Graphid		edge_end_id = DatumGetGraphid(
													  heap_getattr(tuple,
																   Anum_table_edge_end,
																   tupleDesc,
																   &isnull)
			);

			agstat_count_edge_delete(
									 GraphidGetLabid(graphid),
									 GraphidGetLabid(edge_start_id),
									 GraphidGetLabid(edge_end_id)
				);
		}
	}
	else
	{
		Assert(typeOid == VERTEXOID);
		graphWriteStats.deleteVertex++;
	}

	hash_search(mgstate->elemTable, &graphid, HASH_ENTER, &hash_found);

	return true;
}

static void
ExecDeleteGraphElement(ModifyGraphState *mgstate, Datum elem, Oid type)
{
	if (type == VERTEXOID)
	{
		HeapTuple	tuple;
		Graphid		vertex_id = DatumGetGraphid(getVertexIdDatum(elem));
		Oid			rel_oid = get_labid_relid(mgstate->graphid,
											  GraphidGetLabid(vertex_id));
		ResultRelInfo *resultRelInfo = getResultRelInfo(mgstate, rel_oid);
		Datum	   *values = mgstate->delete_graph_state->cached_values;
		bool	   *isnull = mgstate->delete_graph_state->cached_isnull;

		find_connected_edges(mgstate, vertex_id);

		values[Anum_table_vertex_id - 1] = GraphidGetDatum(vertex_id);
		values[Anum_table_vertex_prop_map - 1] = getVertexPropDatum(elem);
		tuple = heap_form_tuple(
								RelationGetDescr(resultRelInfo->ri_RelationDesc),
								values, isnull);

		tuple->t_self = *(DatumGetItemPointer(getVertexTidDatum(elem)));
		ExecDeleteEdgeOrVertex(mgstate, resultRelInfo, vertex_id, tuple,
							   VERTEXOID, false);
		heap_freetuple(tuple);
	}
	else if (type == EDGEOID)
	{
		HeapTuple	tuple;
		Graphid		edge_id = DatumGetGraphid(getEdgeIdDatum(elem));
		Oid			rel_oid = get_labid_relid(mgstate->graphid,
											  GraphidGetLabid(edge_id));
		ResultRelInfo *resultRelInfo = getResultRelInfo(mgstate, rel_oid);
		Datum	   *values = mgstate->delete_graph_state->cached_values;
		bool	   *isnull = mgstate->delete_graph_state->cached_isnull;

		values[Anum_table_edge_id - 1] = GraphidGetDatum(edge_id);
		values[Anum_table_edge_start - 1] = getEdgeStartDatum(elem);
		values[Anum_table_edge_end - 1] = getEdgeEndDatum(elem);
		values[Anum_table_edge_prop_map - 1] = getEdgePropDatum(elem);
		tuple = heap_form_tuple(
								RelationGetDescr(resultRelInfo->ri_RelationDesc),
								values, isnull);

		tuple->t_self = *(DatumGetItemPointer(getEdgeTidDatum(elem)));
		ExecDeleteEdgeOrVertex(mgstate, resultRelInfo, edge_id, tuple,
							   EDGEOID, false);
		heap_freetuple(tuple);
	}
	else if (type == VERTEXARRAYOID)
	{
		AnyArrayType *vertices;
		int			nvertices;
		int16		typlen;
		bool		typbyval;
		char		typalign;
		array_iter	it;
		int			i;
		Datum		vtx;
		bool		isnull;

		vertices = DatumGetAnyArrayP(elem);
		nvertices = ArrayGetNItems(AARR_NDIM(vertices), AARR_DIMS(vertices));

		get_typlenbyvalalign(AARR_ELEMTYPE(vertices), &typlen,
							 &typbyval, &typalign);

		array_iter_setup(&it, vertices);
		for (i = 0; i < nvertices; i++)
		{
			vtx = array_iter_next(&it, &isnull, i, typlen, typbyval, typalign);
			if (!isnull)
			{
				ExecDeleteGraphElement(mgstate, vtx, VERTEXOID);
			}
		}
	}
	else if (type == EDGEARRAYOID)
	{
		AnyArrayType *edges;
		int			nedges;
		int16		typlen;
		bool		typbyval;
		char		typalign;
		array_iter	it;
		int			i;
		Datum		edge;
		bool		isnull;

		edges = DatumGetAnyArrayP(elem);
		nedges = ArrayGetNItems(AARR_NDIM(edges), AARR_DIMS(edges));

		get_typlenbyvalalign(AARR_ELEMTYPE(edges), &typlen,
							 &typbyval, &typalign);

		array_iter_setup(&it, edges);
		for (i = 0; i < nedges; i++)
		{
			edge = array_iter_next(&it, &isnull, i, typlen, typbyval, typalign);
			if (!isnull)
			{
				ExecDeleteGraphElement(mgstate, edge, EDGEOID);
			}
		}
	}
	else
	{
		elog(ERROR, "unexpected graph type %d", type);
	}
}

static void
find_connected_edges(ModifyGraphState *mgstate, Graphid vertex_id)
{
	ModifyGraph *plan = (ModifyGraph *) mgstate->ps.plan;
	DeleteGraphState *delete_graph_state = mgstate->delete_graph_state;
	EState	   *estate = mgstate->ps.state;
	Datum		datum_vertex_id = GraphidGetDatum(vertex_id);
	ResultRelInfo *resultRelInfo = delete_graph_state->edge_labels;
	CommandId	saved_command_id;
	int			i;

	saved_command_id = estate->es_snapshot->curcid;
	estate->es_snapshot->curcid =
		mgstate->modify_cid + MODIFY_CID_NLJOIN_MATCH;

	for (i = 0; i < delete_graph_state->num_edge_labels; i++)
	{
		find_connected_edges_internal(mgstate, plan, estate, resultRelInfo,
									  Anum_table_edge_start, datum_vertex_id);
		find_connected_edges_internal(mgstate, plan, estate, resultRelInfo,
									  Anum_table_edge_end, datum_vertex_id);
		resultRelInfo++;
	}

	estate->es_snapshot->curcid = saved_command_id;
}

static void
find_connected_edges_internal(ModifyGraphState *mgstate,
							  ModifyGraph *plan,
							  EState *estate,
							  ResultRelInfo *resultRelInfo,
							  AttrNumber attr,
							  Datum vertex_id)
{
	Relation	relation = resultRelInfo->ri_RelationDesc;
	HeapTuple	tup;
	TableScanDesc scanDesc;
	ScanKeyData skey;

	ScanKeyInit(&skey, attr,
				BTEqualStrategyNumber,
				F_GRAPHID_EQ, vertex_id);
	scanDesc = table_beginscan(relation, estate->es_snapshot, 1, &skey);
	while ((tup = heap_getnext(scanDesc, ForwardScanDirection)) != NULL)
	{
		bool		isnull;
		Graphid		gid;

		if (!plan->detach)
		{
			table_endscan(scanDesc);
			elog(ERROR, "vertices with edges can not be removed");
		}
		gid = DatumGetGraphid(heap_getattr(tup,
										   Anum_table_edge_id,
										   RelationGetDescr(relation),
										   &isnull));
		ExecDeleteEdgeOrVertex(mgstate,
							   resultRelInfo,
							   gid,
							   tup,
							   EDGEOID,
							   true);
	}
	table_endscan(scanDesc);
}
