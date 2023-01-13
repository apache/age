/*
 * execCypherCreate.c
 *	  routines to handle ModifyGraph create nodes.
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
 *	  src/backend/executor/execCypherCreate.c
 */

#include "postgres.h"

#include "executor/execCypherCreate.h"
#include "executor/executor.h"
#include "executor/nodeModifyGraph.h"
#include "utils/jsonb.h"
#include "utils/rel.h"
#include "access/tableam.h"
#include "pgstat.h"
#include "commands/trigger.h"

static TupleTableSlot *createPath(ModifyGraphState *mgstate, GraphPath *path,
								  TupleTableSlot *slot);
static Datum createVertex(ModifyGraphState *mgstate, GraphVertex *gvertex, Graphid *vid,
						  TupleTableSlot *slot);
static Datum createEdge(ModifyGraphState *mgstate, GraphEdge *gedge, Graphid start,
						Graphid end, TupleTableSlot *slot);

TupleTableSlot *
ExecCreateGraph(ModifyGraphState *mgstate, TupleTableSlot *slot)
{
	ModifyGraph *plan = (ModifyGraph *) mgstate->ps.plan;
	ExprContext *econtext = mgstate->ps.ps_ExprContext;
	ListCell   *lp;

	ResetExprContext(econtext);

	/* create a pattern, accumulated paths `slot` has */
	foreach(lp, plan->pattern)
	{
		GraphPath  *path = (GraphPath *) lfirst(lp);
		MemoryContext oldmctx;

		oldmctx = MemoryContextSwitchTo(econtext->ecxt_per_tuple_memory);

		slot = createPath(mgstate, path, slot);

		MemoryContextSwitchTo(oldmctx);
	}

	return (plan->last ? NULL : slot);
}


/* create a path and accumulate it to the given slot */
static TupleTableSlot *
createPath(ModifyGraphState *mgstate, GraphPath *path, TupleTableSlot *slot)
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

			if (gvertex->create)
				vertex = createVertex(mgstate, gvertex, &vid, slot);
			else
				vertex = findVertex(slot, gvertex, &vid);

			Assert(vertex != (Datum) 0);

			if (out)
				vertices[nvertices++] = vertex;

			if (gedge != NULL)
			{
				Datum		edge;

				if (gedge->direction == GRAPH_EDGE_DIR_LEFT)
				{
					edge = createEdge(mgstate, gedge, vid, prevvid, slot);
				}
				else
				{
					Assert(gedge->direction == GRAPH_EDGE_DIR_RIGHT);

					edge = createEdge(mgstate, gedge, prevvid, vid, slot);
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


/*
 * createVertex - creates a vertex of a given node
 *
 * NOTE: This function returns a vertex if it must be in the result(`slot`).
 */
static Datum
createVertex(ModifyGraphState *mgstate, GraphVertex *gvertex, Graphid *vid,
			 TupleTableSlot *slot)
{
	EState	   *estate = mgstate->ps.state;
	TupleTableSlot *elemTupleSlot = mgstate->elemTupleSlot;
	ResultRelInfo *resultRelInfo;
	Datum		vertex;
	Datum		vertexProp;
	List	   *recheckIndexes = NIL;

	resultRelInfo = getResultRelInfo(mgstate, gvertex->relid);

	vertex = findVertex(slot, gvertex, vid);

	vertexProp = getVertexPropDatum(vertex);
	if (!JB_ROOT_IS_OBJECT(DatumGetJsonbP(vertexProp)))
		ereport(ERROR,
				(errcode(ERRCODE_DATATYPE_MISMATCH),
				 errmsg("jsonb object is expected for property map")));

	ExecClearTuple(elemTupleSlot);

	ExecSetSlotDescriptor(elemTupleSlot,
						  RelationGetDescr(resultRelInfo->ri_RelationDesc));
	elemTupleSlot->tts_values[0] = GraphidGetDatum(*vid);
	elemTupleSlot->tts_values[1] = vertexProp;
	MemSet(elemTupleSlot->tts_isnull, false,
		   elemTupleSlot->tts_tupleDescriptor->natts * sizeof(bool));
	ExecStoreVirtualTuple(elemTupleSlot);

	ExecMaterializeSlot(elemTupleSlot);

	/*
	 * Constraints might reference the tableoid column, so initialize
	 * t_tableOid before evaluating them.
	 */
	elemTupleSlot->tts_tableOid = RelationGetRelid(resultRelInfo->ri_RelationDesc);

	/*
	 * BEFORE ROW INSERT Triggers.
	 */
	if (resultRelInfo->ri_TrigDesc &&
		resultRelInfo->ri_TrigDesc->trig_insert_before_row)
	{
		if (!ExecBRInsertTriggers(estate, resultRelInfo, elemTupleSlot))
		{
			elog(ERROR, "Trigger must not be NULL on Cypher Clause.");
		}
	}

	/*
	 * Check the constraints of the tuple
	 */
	if (resultRelInfo->ri_RelationDesc->rd_att->constr != NULL)
		ExecConstraints(resultRelInfo, elemTupleSlot, estate);

	/*
	 * insert the tuple normally
	 */
	table_tuple_insert(resultRelInfo->ri_RelationDesc, elemTupleSlot,
					   mgstate->modify_cid + MODIFY_CID_OUTPUT,
					   0, NULL);

	/* insert index entries for the tuple */
	if (resultRelInfo->ri_NumIndices > 0)
		recheckIndexes = ExecInsertIndexTuples(resultRelInfo, elemTupleSlot,
											   estate, false, false,
											   NULL, NIL);

	/* AFTER ROW INSERT Triggers */
	ExecARInsertTriggers(estate, resultRelInfo, elemTupleSlot, recheckIndexes,
						 NULL);
	list_free(recheckIndexes);

	vertex = makeGraphVertexDatum(elemTupleSlot->tts_values[0],
								  elemTupleSlot->tts_values[1],
								  PointerGetDatum(&elemTupleSlot->tts_tid));

	if (gvertex->resno > 0)
		setSlotValueByAttnum(slot, vertex, gvertex->resno);

	graphWriteStats.insertVertex++;

	return vertex;
}

static Datum
createEdge(ModifyGraphState *mgstate, GraphEdge *gedge, Graphid start,
		   Graphid end, TupleTableSlot *slot)
{
	EState	   *estate = mgstate->ps.state;
	TupleTableSlot *elemTupleSlot = mgstate->elemTupleSlot;
	ResultRelInfo *resultRelInfo;
	Graphid		id = 0;
	Datum		edge;
	Datum		edgeProp;
	List	   *recheckIndexes = NIL;

	resultRelInfo = getResultRelInfo(mgstate, gedge->relid);

	edge = findEdge(slot, gedge, &id);
	Assert(edge != (Datum) 0);

	edgeProp = getEdgePropDatum(edge);
	if (!JB_ROOT_IS_OBJECT(DatumGetJsonbP(edgeProp)))
		ereport(ERROR,
				(errcode(ERRCODE_DATATYPE_MISMATCH),
				 errmsg("jsonb object is expected for property map")));

	ExecClearTuple(elemTupleSlot);

	ExecSetSlotDescriptor(elemTupleSlot,
						  RelationGetDescr(resultRelInfo->ri_RelationDesc));
	elemTupleSlot->tts_values[0] = GraphidGetDatum(id);
	elemTupleSlot->tts_values[1] = GraphidGetDatum(start);
	elemTupleSlot->tts_values[2] = GraphidGetDatum(end);
	elemTupleSlot->tts_values[3] = edgeProp;
	MemSet(elemTupleSlot->tts_isnull, false,
		   elemTupleSlot->tts_tupleDescriptor->natts * sizeof(bool));
	ExecStoreVirtualTuple(elemTupleSlot);

	ExecMaterializeSlot(elemTupleSlot);
	elemTupleSlot->tts_tableOid = RelationGetRelid(resultRelInfo->ri_RelationDesc);

	/*
	 * BEFORE ROW INSERT Triggers.
	 */
	if (resultRelInfo->ri_TrigDesc &&
		resultRelInfo->ri_TrigDesc->trig_insert_before_row)
	{
		if (!ExecBRInsertTriggers(estate, resultRelInfo, elemTupleSlot))
		{
			elog(ERROR, "Trigger must not be NULL on Cypher Clause.");
		}
	}

	if (resultRelInfo->ri_RelationDesc->rd_att->constr != NULL)
		ExecConstraints(resultRelInfo, elemTupleSlot, estate);

	table_tuple_insert(resultRelInfo->ri_RelationDesc, elemTupleSlot,
					   mgstate->modify_cid + MODIFY_CID_OUTPUT,
					   0, NULL);

	if (resultRelInfo->ri_NumIndices > 0)
		recheckIndexes = ExecInsertIndexTuples(resultRelInfo, elemTupleSlot,
											   estate, false, false,
											   NULL, NIL);

	/* AFTER ROW INSERT Triggers */
	ExecARInsertTriggers(estate, resultRelInfo, elemTupleSlot, recheckIndexes,
						 NULL);
	list_free(recheckIndexes);

	edge = makeGraphEdgeDatum(elemTupleSlot->tts_values[0],
							  elemTupleSlot->tts_values[1],
							  elemTupleSlot->tts_values[2],
							  elemTupleSlot->tts_values[3],
							  PointerGetDatum(&elemTupleSlot->tts_tid));

	if (gedge->resno > 0)
		setSlotValueByAttnum(slot, edge, gedge->resno);

	graphWriteStats.insertEdge++;

	if (auto_gather_graphmeta)
	{
		agstat_count_edge_create(
								 GraphidGetLabid(id),
								 GraphidGetLabid(start),
								 GraphidGetLabid(end)
			);
	}

	return edge;
}
