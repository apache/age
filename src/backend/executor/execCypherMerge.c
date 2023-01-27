/*
 * execCypherMerge.c
 *	  routines to handle ModifyGraph merge nodes.
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
 *	  src/backend/executor/execCypherMerge.c
 */

#include "postgres.h"

#include "executor/execCypherMerge.h"
#include "executor/execCypherSet.h"
#include "executor/nodeModifyGraph.h"
#include "executor/executor.h"
#include "utils/jsonb.h"
#include "utils/rel.h"
#include "access/tableam.h"
#include "pgstat.h"
#include "access/xact.h"
#include "commands/trigger.h"

static bool isMatchedMergePattern(PlanState *planstate);
static TupleTableSlot *createMergePath(ModifyGraphState *mgstate,
									   GraphPath *path, TupleTableSlot *slot);
static Datum createMergeVertex(ModifyGraphState *mgstate,
							   GraphVertex *gvertex,
							   Graphid *vid, TupleTableSlot *slot);
static Datum createMergeEdge(ModifyGraphState *mgstate, GraphEdge *gedge,
							 Graphid start, Graphid end, TupleTableSlot *slot);


TupleTableSlot *
ExecMergeGraph(ModifyGraphState *mgstate, TupleTableSlot *slot)
{
	ModifyGraph *plan = (ModifyGraph *) mgstate->ps.plan;
	ExprContext *econtext = mgstate->ps.ps_ExprContext;
	GraphPath  *path = (GraphPath *) linitial(mgstate->pattern);

	ResetExprContext(econtext);
	econtext->ecxt_scantuple = slot;

	if (isMatchedMergePattern(mgstate->subplan))
	{
		if (mgstate->sets != NIL)
		{
			slot = LegacyExecSetGraph(mgstate, slot, GSP_ON_MATCH);
		}
	}
	else
	{
		MemoryContext oldmctx;

		oldmctx = MemoryContextSwitchTo(econtext->ecxt_per_tuple_memory);

		slot = createMergePath(mgstate, path, slot);

		MemoryContextSwitchTo(oldmctx);

		if (mgstate->sets != NIL)
		{
			slot = LegacyExecSetGraph(mgstate, slot, GSP_ON_CREATE);
		}
	}

	return (plan->last ? NULL : slot);
}

/* tricky but efficient */
static bool
isMatchedMergePattern(PlanState *planstate)
{
	Assert(IsA(planstate, NestLoopState));

	return ((NestLoopState *) planstate)->nl_MatchedOuter;
}

static TupleTableSlot *
createMergePath(ModifyGraphState *mgstate, GraphPath *path,
				TupleTableSlot *slot)
{
	bool		out = (path->variable != NULL);
	int			pathlen;
	Datum	   *vertices = NULL;
	Datum	   *edges = NULL;
	int			nvertices;
	int			nedges;
	ListCell   *le;
	Graphid		prevvid = 0;
	GraphEdge  *gedge = NULL;

	if (out)
	{
		pathlen = list_length(path->chain);
		Assert(pathlen % 2 == 1);

		vertices = makeDatumArray((pathlen / 2) + 1);
		edges = makeDatumArray(pathlen / 2);

		nvertices = 0;
		nedges = 0;
	}

	foreach(le, path->chain)
	{
		Node	   *elem = (Node *) lfirst(le);

		if (IsA(elem, GraphVertex))
		{
			GraphVertex *gvertex = (GraphVertex *) elem;
			Graphid		vid;
			Datum		vertex;

			vertex = findVertex(slot, gvertex, &vid);
			if (vertex == (Datum) 0)
				vertex = createMergeVertex(mgstate, gvertex, &vid, slot);

			if (out)
				vertices[nvertices++] = vertex;

			if (gedge != NULL)
			{
				Datum		edge;

				edge = findEdge(slot, gedge, NULL);
				Assert(edge == (Datum) 0);

				if (gedge->direction == GRAPH_EDGE_DIR_LEFT)
				{
					edge = createMergeEdge(mgstate, gedge, vid, prevvid, slot);
				}
				else
				{
					Assert(gedge->direction == GRAPH_EDGE_DIR_RIGHT);

					edge = createMergeEdge(mgstate, gedge, prevvid, vid, slot);
				}

				if (out)
					edges[nedges++] = edge;
			}

			prevvid = vid;
		}
		else
		{
			Assert(IsA(elem, GraphEdge));

			gedge = (GraphEdge *) elem;
		}
	}

	/* make a graphpath and set it to the slot */
	if (out)
	{
		Datum		graphpath;

		Assert(nvertices == nedges + 1);
		Assert(pathlen == nvertices + nedges);

		graphpath = makeGraphpathDatum(vertices, nvertices, edges, nedges);

		setSlotValueByName(slot, graphpath, path->variable);
	}

	return slot;
}

/* See ExecInsert() */
static Datum
createMergeVertex(ModifyGraphState *mgstate, GraphVertex *gvertex,
				  Graphid *vid, TupleTableSlot *slot)
{
	EState	   *estate = mgstate->ps.state;
	ExprContext *econtext = mgstate->ps.ps_ExprContext;
	ResultRelInfo *resultRelInfo;
	bool		isNull;
	Datum		vertex;
	Datum		vertexId;
	Datum		vertexProp;
	TupleTableSlot *insertSlot = mgstate->elemTupleSlot;
	List	   *recheckIndexes = NIL;

	resultRelInfo = getResultRelInfo(mgstate, gvertex->relid);

	vertex = ExecEvalExpr(gvertex->es_expr, econtext, &isNull);
	if (isNull)
		ereport(ERROR,
				(errcode(ERRCODE_NULL_VALUE_NOT_ALLOWED),
				 errmsg("NULL is not allowed in MERGE")));

	vertexId = getVertexIdDatum(vertex);
	*vid = DatumGetGraphid(vertexId);

	vertexProp = getVertexPropDatum(vertex);
	if (!JB_ROOT_IS_OBJECT(DatumGetJsonbP(vertexProp)))
		ereport(ERROR,
				(errcode(ERRCODE_DATATYPE_MISMATCH),
				 errmsg("jsonb object is expected for property map")));

	ExecClearTuple(insertSlot);

	ExecSetSlotDescriptor(insertSlot,
						  RelationGetDescr(resultRelInfo->ri_RelationDesc));
	insertSlot->tts_values[0] = vertexId;
	insertSlot->tts_values[1] = vertexProp;
	MemSet(insertSlot->tts_isnull, false,
		   insertSlot->tts_tupleDescriptor->natts * sizeof(bool));
	ExecStoreVirtualTuple(insertSlot);

	ExecMaterializeSlot(insertSlot);
	insertSlot->tts_tableOid = RelationGetRelid(resultRelInfo->ri_RelationDesc);

	/*
	 * BEFORE ROW INSERT Triggers.
	 */
	if (resultRelInfo->ri_TrigDesc &&
		resultRelInfo->ri_TrigDesc->trig_insert_before_row)
	{
		if (!ExecBRInsertTriggers(estate, resultRelInfo, insertSlot))
		{
			elog(ERROR, "Trigger must not be NULL on Cypher Clause.");
		}
	}

	if (resultRelInfo->ri_RelationDesc->rd_att->constr != NULL)
		ExecConstraints(resultRelInfo, insertSlot, estate);

	table_tuple_insert(resultRelInfo->ri_RelationDesc, insertSlot,
					   mgstate->modify_cid + MODIFY_CID_OUTPUT,
					   0, NULL);

	if (resultRelInfo->ri_NumIndices > 0)
		recheckIndexes = ExecInsertIndexTuples(resultRelInfo, insertSlot,
											   estate, false, false, NULL, NIL);

	/* AFTER ROW INSERT Triggers */
	ExecARInsertTriggers(estate, resultRelInfo, insertSlot, recheckIndexes,
						 NULL);
	list_free(recheckIndexes);

	vertex = makeGraphVertexDatum(insertSlot->tts_values[0],
								  insertSlot->tts_values[1],
								  PointerGetDatum(&insertSlot->tts_tid));

	if (gvertex->resno > 0)
		setSlotValueByAttnum(slot, vertex, gvertex->resno);

	graphWriteStats.insertVertex++;

	return vertex;
}

static Datum
createMergeEdge(ModifyGraphState *mgstate, GraphEdge *gedge, Graphid start,
				Graphid end, TupleTableSlot *slot)
{
	EState	   *estate = mgstate->ps.state;
	ExprContext *econtext = mgstate->ps.ps_ExprContext;
	ResultRelInfo *resultRelInfo;
	bool		isNull;
	Datum		edge;
	Datum		edgeProp;
	TupleTableSlot *insertSlot = mgstate->elemTupleSlot;
	List	   *recheckIndexes = NIL;

	resultRelInfo = getResultRelInfo(mgstate, gedge->relid);

	edge = ExecEvalExpr(gedge->es_expr, econtext, &isNull);
	if (isNull)
		ereport(ERROR,
				(errcode(ERRCODE_NULL_VALUE_NOT_ALLOWED),
				 errmsg("NULL is not allowed in MERGE")));

	edgeProp = getEdgePropDatum(edge);
	if (!JB_ROOT_IS_OBJECT(DatumGetJsonbP(edgeProp)))
		ereport(ERROR,
				(errcode(ERRCODE_DATATYPE_MISMATCH),
				 errmsg("jsonb object is expected for property map")));

	ExecClearTuple(insertSlot);

	ExecSetSlotDescriptor(insertSlot,
						  RelationGetDescr(resultRelInfo->ri_RelationDesc));
	insertSlot->tts_values[0] = getEdgeIdDatum(edge);
	insertSlot->tts_values[1] = GraphidGetDatum(start);
	insertSlot->tts_values[2] = GraphidGetDatum(end);
	insertSlot->tts_values[3] = edgeProp;
	MemSet(insertSlot->tts_isnull, false,
		   insertSlot->tts_tupleDescriptor->natts * sizeof(bool));
	ExecStoreVirtualTuple(insertSlot);

	ExecMaterializeSlot(insertSlot);

	insertSlot->tts_tableOid = RelationGetRelid(resultRelInfo->ri_RelationDesc);

	/*
	 * BEFORE ROW INSERT Triggers.
	 */
	if (resultRelInfo->ri_TrigDesc &&
		resultRelInfo->ri_TrigDesc->trig_insert_before_row)
	{
		if (!ExecBRInsertTriggers(estate, resultRelInfo, insertSlot))
		{
			elog(ERROR, "Trigger must not be NULL on Cypher Clause.");
		}
	}

	if (resultRelInfo->ri_RelationDesc->rd_att->constr != NULL)
		ExecConstraints(resultRelInfo, insertSlot, estate);

	table_tuple_insert(resultRelInfo->ri_RelationDesc, insertSlot,
					   mgstate->modify_cid + MODIFY_CID_OUTPUT,
					   0, NULL);

	if (resultRelInfo->ri_NumIndices > 0)
		recheckIndexes = ExecInsertIndexTuples(resultRelInfo, insertSlot,
											   estate, false, false, NULL, NIL);

	/* AFTER ROW INSERT Triggers */
	ExecARInsertTriggers(estate, resultRelInfo, insertSlot, recheckIndexes,
						 NULL);
	list_free(recheckIndexes);

	edge = makeGraphEdgeDatum(insertSlot->tts_values[0],
							  insertSlot->tts_values[1],
							  insertSlot->tts_values[2],
							  insertSlot->tts_values[3],
							  PointerGetDatum(&insertSlot->tts_tid));

	if (gedge->resno > 0)
		setSlotValueByAttnum(slot, edge, gedge->resno);

	graphWriteStats.insertEdge++;

	if (auto_gather_graphmeta)
	{
		agstat_count_edge_create(
								 GraphidGetLabid(GraphidGetDatum(getEdgeIdDatum(edge))),
								 GraphidGetLabid(start),
								 GraphidGetLabid(end)
			);
	}

	return edge;
}
