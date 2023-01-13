/*
 * nodeModifyGraph.c
 *	  routines to handle ModifyGraph nodes.
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
 *	  src/backend/executor/nodeModifyGraph.c
 */

#include "postgres.h"

#include "access/htup_details.h"
#include "access/xact.h"
#include "catalog/ag_graph_fn.h"
#include "catalog/pg_type.h"
#include "executor/executor.h"
#include "executor/nodeModifyGraph.h"
#include "miscadmin.h"
#include "nodes/nodeFuncs.h"
#include "parser/parse_relation.h"
#include "pgstat.h"
#include "utils/arrayaccess.h"
#include "utils/builtins.h"
#include "utils/lsyscache.h"
#include "utils/rel.h"
#include "access/heapam.h"
#include "executor/execCypherCreate.h"
#include "executor/execCypherSet.h"
#include "executor/execCypherDelete.h"
#include "executor/execCypherMerge.h"
#include "catalog/ag_label_fn.h"

bool		enable_multiple_update = true;
bool		auto_gather_graphmeta = false;

#define DatumGetItemPointer(X)	 ((ItemPointer) DatumGetPointer(X))

static TupleTableSlot *ExecModifyGraph(PlanState *pstate);
static void initGraphWRStats(ModifyGraphState *mgstate, GraphWriteOp op);
static List *ExecInitGraphPattern(List *pattern, ModifyGraphState *mgstate);
static List *ExecInitGraphSets(List *sets, ModifyGraphState *mgstate);
static List *ExecInitGraphDelExprs(List *exprs, ModifyGraphState *mgstate);

/* eager */
static void reflectModifiedProp(ModifyGraphState *mgstate);

/* common */
static bool isEdgeArrayOfPath(List *exprs, char *variable);

static void openResultRelInfosIndices(ModifyGraphState *mgstate);

ModifyGraphState *
ExecInitModifyGraph(ModifyGraph *mgplan, EState *estate, int eflags)
{
	TupleTableSlot *slot;
	ModifyGraphState *mgstate;
	ResultRelInfo *resultRelInfo;
	ListCell   *l;

	Assert(!(eflags & (EXEC_FLAG_BACKWARD | EXEC_FLAG_MARK)));

	mgstate = makeNode(ModifyGraphState);
	mgstate->ps.plan = (Plan *) mgplan;
	mgstate->ps.state = estate;
	mgstate->ps.ExecProcNode = ExecModifyGraph;

	/* Tuple desc for result is the same as the subplan. */
	slot = ExecAllocTableSlot(&estate->es_tupleTable, NULL, &TTSOpsMinimalTuple);
	mgstate->ps.ps_ResultTupleSlot = slot;

	/*
	 * We don't use ExecInitResultTypeTL because we need to get the
	 * information of the subplan, not the current plan.
	 */
	mgstate->ps.ps_ResultTupleDesc = ExecTypeFromTL(mgplan->subplan->targetlist);
	ExecSetSlotDescriptor(slot, mgstate->ps.ps_ResultTupleDesc);
	ExecAssignExprContext(estate, &mgstate->ps);

	mgstate->elemTupleSlot = ExecInitExtraTupleSlot(estate, NULL,
													&TTSOpsMinimalTuple);

	mgstate->done = false;
	mgstate->child_done = false;
	mgstate->eagerness = mgplan->eagerness;
	mgstate->modify_cid = GetCurrentCommandId(false) +
		(mgplan->nr_modify * MODIFY_CID_MAX);

	mgstate->subplan = ExecInitNode(mgplan->subplan, estate, eflags);
	AssertArg(mgplan->operation != GWROP_MERGE ||
			  IsA(mgstate->subplan, NestLoopState));

	mgstate->graphid = get_graph_path_oid();
	mgstate->pattern = ExecInitGraphPattern(mgplan->pattern, mgstate);
	mgstate->exprs = ExecInitGraphDelExprs(mgplan->exprs, mgstate);

	mgstate->numResultRelInfo = list_length(mgplan->resultRelations);
	mgstate->resultRelInfo = (ResultRelInfo *)
		palloc(mgstate->numResultRelInfo * sizeof(ResultRelInfo));

	resultRelInfo = mgstate->resultRelInfo;
	foreach(l, mgplan->resultRelations)
	{
		Index		resultRelation = lfirst_int(l);

		ExecInitResultRelation(estate, resultRelInfo, resultRelation);
		resultRelInfo++;
	}

	openResultRelInfosIndices(mgstate);

	/* For Set Operation. */
	mgstate->sets = ExecInitGraphSets(mgplan->sets, mgstate);
	mgstate->update_cols = NULL;

	/* Initialize for EPQ. */
	EvalPlanQualInit(&mgstate->mt_epqstate, estate, NULL, NIL,
					 mgplan->epqParam);
	mgstate->mt_arowmarks = (List **) palloc0(sizeof(List *) * 1);
	EvalPlanQualSetPlan(&mgstate->mt_epqstate, mgplan->subplan,
						mgstate->mt_arowmarks[0]);

	/* Fill eager action information */
	if (mgstate->eagerness ||
		(mgstate->sets != NIL && enable_multiple_update) ||
		mgstate->exprs != NIL)
	{
		HASHCTL		ctl;

		memset(&ctl, 0, sizeof(ctl));
		ctl.keysize = sizeof(Graphid);
		ctl.entrysize = sizeof(ModifiedElemEntry);
		ctl.hcxt = CurrentMemoryContext;

		mgstate->elemTable =
			hash_create("modified object table", 128, &ctl,
						HASH_ELEM | HASH_BLOBS | HASH_CONTEXT);
	}
	else
	{
		/* We will not use eager action */
		mgstate->elemTable = NULL;
	}
	mgstate->tuplestorestate = tuplestore_begin_heap(false, false, eager_mem);

	mgstate->delete_graph_state = NULL;

	switch (mgplan->operation)
	{
		case GWROP_CREATE:
			mgstate->execProc = ExecCreateGraph;
			break;
		case GWROP_DELETE:
			{
				DeleteGraphState *deleteGraphState =
				palloc(sizeof(DeleteGraphState));
				List	   *edge_label_oids = get_all_edge_labels_per_graph(
																			estate->es_snapshot, mgstate->graphid);
				int			num_edge_labels = list_length(edge_label_oids);

				if (num_edge_labels > 0)
				{
					ResultRelInfo *edge_label_resultRelInfos =
					(ResultRelInfo *) palloc(
											 num_edge_labels * sizeof(ResultRelInfo));
					ResultRelInfo *resultRelInfoForDel =
					edge_label_resultRelInfos;
					ListCell   *lc;

					foreach(lc, edge_label_oids)
					{
						Oid			label_oid = lfirst_oid(lc);
						Relation	relation = table_open(label_oid, RowExclusiveLock);

						InitResultRelInfo(resultRelInfoForDel,
										  relation,
										  0,	/* dummy rangetable index */
										  NULL,
										  0);
						ExecOpenIndices(resultRelInfoForDel, false);
						resultRelInfoForDel++;
					}

					deleteGraphState->edge_labels = edge_label_resultRelInfos;
				}
				else
				{
					deleteGraphState->edge_labels = NULL;
				}
				deleteGraphState->num_edge_labels = num_edge_labels;
				list_free(edge_label_oids);

				deleteGraphState->cached_values =
					palloc0(sizeof(Datum) * Anum_table_edge_prop_map);
				deleteGraphState->cached_isnull =
					palloc0(sizeof(bool) * Anum_table_edge_prop_map);

				mgstate->delete_graph_state = deleteGraphState;
				mgstate->execProc = ExecDeleteGraph;
				break;
			}
		case GWROP_SET:
			mgstate->execProc = ExecSetGraph;
			break;
		case GWROP_MERGE:
			mgstate->execProc = ExecMergeGraph;
			break;
		default:
			elog(ERROR, "unknown operation");
	}

	initGraphWRStats(mgstate, mgplan->operation);
	return mgstate;
}

static void
reflectTupleChanges(PlanState *pstate, TupleTableSlot *result)
{
	ModifyGraphState *mgstate = castNode(ModifyGraphState, pstate);
	ModifyGraph *plan = (ModifyGraph *) mgstate->ps.plan;
	TupleDesc	tupDesc = result->tts_tupleDescriptor;
	int			natts = tupDesc->natts;
	int			i;

	for (i = 0; i < natts; i++)
	{
		Oid			type;
		Datum		orig_elem;
		Datum		elem;

		if (result->tts_isnull[i])
			continue;

		orig_elem = result->tts_values[i];
		type = TupleDescAttr(tupDesc, i)->atttypid;
		if (type == VERTEXOID)
		{
			Datum		graphid;
			bool		found;

			graphid = getVertexIdDatum(orig_elem);
			elem = getElementFromEleTable(mgstate, type, orig_elem, graphid,
										  &found);
			if (!found)
			{
				continue;
			}
		}
		else if (type == EDGEOID)
		{
			Datum		graphid;
			bool		found;

			graphid = getEdgeIdDatum(orig_elem);
			elem = getElementFromEleTable(mgstate, type, orig_elem, graphid,
										  &found);
			if (!found)
			{
				continue;
			}
		}
		else if (type == GRAPHPATHOID)
		{
			/*
			 * When deleting the graphpath, edge array of graphpath is deleted
			 * first and vertex array is deleted in the next plan. So, the
			 * graphpath must be passed to the next plan for deleting vertex
			 * array of the graphpath.
			 */
			if (isEdgeArrayOfPath(mgstate->exprs,
								  NameStr(TupleDescAttr(tupDesc, i)->attname)))
				continue;

			elem = getPathFinal(mgstate, orig_elem);
		}
		else if (type == EDGEARRAYOID && plan->operation == GWROP_DELETE)
		{
			/*
			 * The edges are used only for removal, not for result output.
			 *
			 * This assumes that there are only variable references in the
			 * target list.
			 */
			continue;
		}
		else
		{
			continue;
		}

		setSlotValueByAttnum(result, elem, i + 1);
	}
}

static TupleTableSlot *
ExecModifyGraph(PlanState *pstate)
{
	ModifyGraphState *mgstate = castNode(ModifyGraphState, pstate);
	ModifyGraph *plan = (ModifyGraph *) mgstate->ps.plan;
	EState	   *estate = mgstate->ps.state;

	if (mgstate->done)
		return NULL;

	if (!mgstate->child_done)
	{
		for (;;)
		{
			CommandId	svCid;
			TupleTableSlot *slot;

			/* ExecInsertIndexTuples() uses per-tuple context. Reset it here. */
			ResetPerTupleExprContext(estate);

			svCid = estate->es_snapshot->curcid;

			switch (plan->operation)
			{
				case GWROP_MERGE:
				case GWROP_DELETE:
					estate->es_snapshot->curcid =
						mgstate->modify_cid + MODIFY_CID_NLJOIN_MATCH;
					break;
				default:
					estate->es_snapshot->curcid =
						mgstate->modify_cid + MODIFY_CID_LOWER_BOUND;
					break;
			}

			slot = ExecProcNode(mgstate->subplan);

			estate->es_snapshot->curcid = svCid;

			if (TupIsNull(slot))
				break;

			slot = mgstate->execProc(mgstate, slot);

			if (mgstate->eagerness)
			{
				Assert(slot != NULL);

				tuplestore_puttupleslot(mgstate->tuplestorestate, slot);
			}
			else if (slot != NULL)
			{
				return slot;
			}
			else
			{
				Assert(plan->last == true);
			}
		}

		mgstate->child_done = true;

		if (mgstate->elemTable != NULL
			&& plan->operation != GWROP_DELETE
			&& plan->operation != GWROP_SET)
			reflectModifiedProp(mgstate);
	}

	if (mgstate->eagerness)
	{
		TupleTableSlot *result;

		/* don't care about scan direction */
		result = mgstate->ps.ps_ResultTupleSlot;
		tuplestore_gettupleslot(mgstate->tuplestorestate, true, false, result);

		if (TupIsNull(result))
			return result;

		slot_getallattrs(result);

		if (mgstate->elemTable == NULL ||
			hash_get_num_entries(mgstate->elemTable) < 1)
			return result;

		reflectTupleChanges(pstate, result);

		return result;
	}

	mgstate->done = true;

	return NULL;
}

void
ExecEndModifyGraph(ModifyGraphState *mgstate)
{
	CommandId	used_cid;

	if (mgstate->delete_graph_state != NULL)
	{
		DeleteGraphState *delete_graph_state = mgstate->delete_graph_state;
		ResultRelInfo *resultRelInfo = delete_graph_state->edge_labels;

		if (resultRelInfo != NULL)
		{
			int			i;

			for (i = 0; i < delete_graph_state->num_edge_labels; i++)
			{
				ExecCloseIndices(resultRelInfo);
				table_close(resultRelInfo->ri_RelationDesc, RowExclusiveLock);
				resultRelInfo++;
			}
			pfree(delete_graph_state->edge_labels);
		}
		pfree(delete_graph_state->cached_values);
		pfree(delete_graph_state->cached_isnull);
		pfree(delete_graph_state);
	}

	if (mgstate->update_cols != NULL)
	{
		pfree(mgstate->update_cols);
	}

	tuplestore_end(mgstate->tuplestorestate);

	if (mgstate->elemTable != NULL)
		hash_destroy(mgstate->elemTable);

	/*
	 * clean out the tuple table
	 */
	ExecClearTuple(mgstate->ps.ps_ResultTupleSlot);

	/*
	 * Terminate EPQ execution if active
	 */
	EvalPlanQualEnd(&mgstate->mt_epqstate);

	ExecEndNode(mgstate->subplan);
	ExecFreeExprContext(&mgstate->ps);

	/*
	 * ModifyGraph plan uses multi-level CommandId for supporting visibitliy
	 * between cypher Clauses. Need to raise the cid to see the modifications
	 * made by this ModifyGraph plan in the next command.
	 */
	used_cid = mgstate->modify_cid + MODIFY_CID_MAX;
	while (used_cid > GetCurrentCommandId(true))
	{
		CommandCounterIncrement();
	}
}

static void
initGraphWRStats(ModifyGraphState *mgstate, GraphWriteOp op)
{
	if (mgstate->pattern != NIL)
	{
		Assert(op == GWROP_CREATE || op == GWROP_MERGE);

		graphWriteStats.insertVertex = 0;
		graphWriteStats.insertEdge = 0;
	}
	if (mgstate->exprs != NIL)
	{
		Assert(op == GWROP_DELETE);

		graphWriteStats.deleteVertex = 0;
		graphWriteStats.deleteEdge = 0;
	}
	if (mgstate->sets != NIL)
	{
		Assert(op == GWROP_SET || op == GWROP_MERGE);

		graphWriteStats.updateProperty = 0;
	}
}

static List *
ExecInitGraphPattern(List *pattern, ModifyGraphState *mgstate)
{
	ModifyGraph *plan = (ModifyGraph *) mgstate->ps.plan;
	GraphPath  *gpath;
	ListCell   *le;

	if (plan->operation != GWROP_MERGE)
		return pattern;

	AssertArg(list_length(pattern) == 1);

	gpath = linitial(pattern);

	foreach(le, gpath->chain)
	{
		Node	   *elem = lfirst(le);

		if (IsA(elem, GraphVertex))
		{
			GraphVertex *gvertex = (GraphVertex *) elem;

			gvertex->es_expr = ExecInitExpr((Expr *) gvertex->expr,
											(PlanState *) mgstate);
		}
		else
		{
			GraphEdge  *gedge = (GraphEdge *) elem;

			Assert(IsA(elem, GraphEdge));

			gedge->es_expr = ExecInitExpr((Expr *) gedge->expr,
										  (PlanState *) mgstate);
		}
	}

	return pattern;
}

static List *
ExecInitGraphSets(List *sets, ModifyGraphState *mgstate)
{
	ListCell   *ls;

	foreach(ls, sets)
	{
		GraphSetProp *gsp = lfirst(ls);

		gsp->es_elem = ExecInitExpr((Expr *) gsp->elem, (PlanState *) mgstate);
		gsp->es_expr = ExecInitExpr((Expr *) gsp->expr, (PlanState *) mgstate);
	}

	return sets;
}

static List *
ExecInitGraphDelExprs(List *exprs, ModifyGraphState *mgstate)
{
	ListCell   *lc;

	foreach(lc, exprs)
	{
		GraphDelElem *gde = lfirst(lc);

		gde->es_elem = ExecInitExpr((Expr *) gde->elem, (PlanState *) mgstate);
	}

	return exprs;
}

Datum
getElementFromEleTable(ModifyGraphState *mgstate, Oid type_oid, Datum orig_elem,
					   Datum gid, bool *found)
{
	ModifyGraph *plan = (ModifyGraph *) mgstate->ps.plan;
	ModifiedElemEntry *entry;

	entry = hash_search(mgstate->elemTable, &gid, HASH_FIND, found);

	/* Unmodified or deleted */
	if (!(*found) || plan->operation == GWROP_DELETE)
		return (Datum) 0;
	else
		return entry->elem;
}

Datum
getPathFinal(ModifyGraphState *mgstate, Datum origin)
{
	Datum		vertices_datum;
	Datum		edges_datum;
	AnyArrayType *arrVertices;
	AnyArrayType *arrEdges;
	int			nvertices;
	int			nedges;
	Datum	   *vertices;
	Datum	   *edges;
	int16		typlen;
	bool		typbyval;
	char		typalign;
	array_iter	it;
	int			i;
	Datum		value;
	bool		isnull;
	bool		modified = false;
	bool		isdeleted = false;
	Datum		result;

	getGraphpathArrays(origin, &vertices_datum, &edges_datum);

	arrVertices = DatumGetAnyArrayP(vertices_datum);
	arrEdges = DatumGetAnyArrayP(edges_datum);

	nvertices = ArrayGetNItems(AARR_NDIM(arrVertices), AARR_DIMS(arrVertices));
	nedges = ArrayGetNItems(AARR_NDIM(arrEdges), AARR_DIMS(arrEdges));
	Assert(nvertices == nedges + 1);

	vertices = palloc(nvertices * sizeof(Datum));
	edges = palloc(nedges * sizeof(Datum));

	get_typlenbyvalalign(AARR_ELEMTYPE(arrVertices), &typlen,
						 &typbyval, &typalign);
	array_iter_setup(&it, arrVertices);
	for (i = 0; i < nvertices; i++)
	{
		Datum		vertex;
		Datum		graphid;
		bool		found;

		value = array_iter_next(&it, &isnull, i, typlen, typbyval, typalign);
		Assert(!isnull);

		graphid = getVertexIdDatum(value);
		vertex = getElementFromEleTable(mgstate, VERTEXOID, value, graphid,
										&found);

		if (!found)
		{
			vertex = value;
		}

		if (vertex == (Datum) 0)
		{
			if (i == 0)
				isdeleted = true;

			if (isdeleted)
				continue;
			else
				elog(ERROR, "cannot delete a vertex in graphpath");
		}

		if (found)
		{
			modified = true;
		}

		vertices[i] = vertex;
	}

	get_typlenbyvalalign(AARR_ELEMTYPE(arrEdges), &typlen,
						 &typbyval, &typalign);
	array_iter_setup(&it, arrEdges);
	for (i = 0; i < nedges; i++)
	{
		Datum		edge;
		Datum		graphid;
		bool		found;

		value = array_iter_next(&it, &isnull, i, typlen, typbyval, typalign);
		Assert(!isnull);

		graphid = getEdgeIdDatum(value);
		edge = getElementFromEleTable(mgstate, EDGEOID, value, graphid, &found);

		if (!found)
		{
			edge = value;
		}

		if (edge == (Datum) 0)
		{
			if (isdeleted)
				continue;
			else
				elog(ERROR, "cannot delete a edge in graphpath.");
		}

		if (found)
		{
			modified = true;
		}

		edges[i] = edge;
	}

	if (isdeleted)
		result = (Datum) 0;
	else if (modified)
		result = makeGraphpathDatum(vertices, nvertices, edges, nedges);
	else
		result = origin;

	pfree(vertices);
	pfree(edges);

	return result;
}

static void
reflectModifiedProp(ModifyGraphState *mgstate)
{
	HASH_SEQ_STATUS seq;
	ModifiedElemEntry *entry;

	Assert(mgstate->elemTable != NULL);

	hash_seq_init(&seq, mgstate->elemTable);
	while ((entry = hash_seq_search(&seq)) != NULL)
	{
		ItemPointer ctid;
		Datum		gid = PointerGetDatum(entry->key);
		Oid			type;

		type = get_labid_typeoid(mgstate->graphid,
								 GraphidGetLabid(DatumGetGraphid(gid)));

		ctid = LegacyUpdateElemProp(mgstate, type, gid, entry->elem);

		if (mgstate->eagerness)
		{
			Datum		property;
			Datum		newelem;

			if (type == VERTEXOID)
				property = getVertexPropDatum(entry->elem);
			else if (type == EDGEOID)
				property = getEdgePropDatum(entry->elem);
			else
				elog(ERROR, "unexpected graph type %d", type);

			newelem = makeModifiedElem(entry->elem, type, gid, property,
									   PointerGetDatum(ctid));

			pfree(DatumGetPointer(entry->elem));
			entry->elem = newelem;
		}
	}
}

ResultRelInfo *
getResultRelInfo(ModifyGraphState *mgstate, Oid relid)
{
	ResultRelInfo *resultRelInfo = mgstate->resultRelInfo;
	int			i;

	for (i = 0; i < mgstate->numResultRelInfo; i++)
	{
		if (RelationGetRelid(resultRelInfo->ri_RelationDesc) == relid)
			return resultRelInfo;
		resultRelInfo++;
	}

	elog(ERROR, "invalid object ID %u for the target label", relid);
}

Datum
findVertex(TupleTableSlot *slot, GraphVertex *gvertex, Graphid *vid)
{
	bool		isnull;
	Datum		vertex;

	if (gvertex->resno == InvalidAttrNumber)
		return (Datum) 0;

	vertex = slot_getattr(slot, gvertex->resno, &isnull);
	if (isnull)
		return (Datum) 0;

	if (vid != NULL)
		*vid = DatumGetGraphid(getVertexIdDatum(vertex));

	return vertex;
}

Datum
findEdge(TupleTableSlot *slot, GraphEdge *gedge, Graphid *eid)
{
	bool		isnull;
	Datum		edge;

	if (gedge->resno == InvalidAttrNumber)
		return (Datum) 0;

	edge = slot_getattr(slot, gedge->resno, &isnull);
	if (isnull)
		return (Datum) 0;

	if (eid != NULL)
		*eid = DatumGetGraphid(getEdgeIdDatum(edge));

	return edge;
}

AttrNumber
findAttrInSlotByName(TupleTableSlot *slot, char *name)
{
	TupleDesc	tupDesc = slot->tts_tupleDescriptor;
	int			i;

	for (i = 0; i < tupDesc->natts; i++)
	{
		Form_pg_attribute attr = TupleDescAttr(tupDesc, i);

		if (namestrcmp(&(attr->attname), name) == 0 && !attr->attisdropped)
			return attr->attnum;
	}

	ereport(ERROR,
			(errcode(ERRCODE_INVALID_NAME),
			 errmsg("variable \"%s\" does not exist", name)));
}

void
setSlotValueByName(TupleTableSlot *slot, Datum value, char *name)
{
	AttrNumber	attno;

	if (slot == NULL)
		return;

	attno = findAttrInSlotByName(slot, name);

	setSlotValueByAttnum(slot, value, attno);
}

void
setSlotValueByAttnum(TupleTableSlot *slot, Datum value, int attnum)
{
	if (slot == NULL)
		return;

	AssertArg(attnum > 0 && attnum <= slot->tts_tupleDescriptor->natts);

	slot->tts_values[attnum - 1] = value;
	slot->tts_isnull[attnum - 1] = (value == (Datum) 0) ? true : false;
}

Datum *
makeDatumArray(int len)
{
	if (len == 0)
		return NULL;

	return palloc(len * sizeof(Datum));
}

static bool
isEdgeArrayOfPath(List *exprs, char *variable)
{
	ListCell   *lc;

	foreach(lc, exprs)
	{
		GraphDelElem *gde = castNode(GraphDelElem, lfirst(lc));

		if (exprType(gde->elem) == EDGEARRAYOID &&
			strcmp(gde->variable, variable) == 0)
			return true;
	}

	return false;
}

/*
 * openResultRelInfosIndices
 */
static void
openResultRelInfosIndices(ModifyGraphState *mgstate)
{
	int			index;
	ResultRelInfo *resultRelInfo = mgstate->resultRelInfo;

	for (index = 0; index < mgstate->numResultRelInfo; index++)
	{
		ExecOpenIndices(resultRelInfo, false);
		resultRelInfo++;
	}
}
