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

/*
 *	 INTERFACE ROUTINES
 *		ExecDijkstra	 	- execute dijkstra's algorithm
 *		ExecInitDijkstra 	- initialize
 *		ExecEndDijkstra 	- shut down
 */

#include "postgres.h"

#include "access/htup_details.h"
#include "catalog/ag_vertex_d.h"
#include "catalog/pg_type.h"
#include "executor/executor.h"
#include "executor/nodeDijkstra.h"
#include "executor/tuptable.h"
#include "funcapi.h"
#include "lib/pairingheap.h"
#include "nodes/execnodes.h"
#include "nodes/memnodes.h"
#include "utils/array.h"
#include "utils/lsyscache.h"
#include "utils/memutils.h"
#include "utils/typcache.h"

typedef struct vnode
{
	Graphid		id;					/* hash key */
	double		weight;
	List	   *incoming_enodes;
	ListCell   *out_edge;
} vnode;

typedef struct enode
{
	Graphid		id;
	vnode	   *prev;
} enode;

static HeapTuple replace_vertexRow_graphid(TupleDesc tupleDesc,
										   HeapTuple vertexRow,
										   Datum graphid);
static enode *
new_enode(Graphid id, vnode *prev)
{
	enode *edge = palloc(sizeof(enode));
	edge->id = id;
	edge->prev = prev;
	return edge;
}

static void
vnode_add_enode(vnode *vertex, double weight, Graphid eid, vnode *prev)
{
	enode *edge;

	vertex->weight = weight;
	edge = new_enode(eid, prev);
	vertex->incoming_enodes = lappend(vertex->incoming_enodes, edge);
	vertex->out_edge = list_head(vertex->incoming_enodes);
}

static void
vnode_update_enode(vnode *vertex, double weight, Graphid eid, vnode *prev)
{
	list_free(vertex->incoming_enodes);
	vertex->incoming_enodes = NIL;
	vnode_add_enode(vertex, weight, eid, prev);
	vertex->out_edge = list_head(vertex->incoming_enodes);
}

static enode *
vnode_get_curr_enode(vnode *vertex)
{
	return vertex->out_edge != NULL ? lfirst(vertex->out_edge) : NULL;
}

static void
vnode_next_enode(vnode *vertex)
{
	if (vertex->out_edge == NULL)
		vertex->out_edge = list_head(vertex->incoming_enodes);
	else
		vertex->out_edge = lnext(vertex->incoming_enodes, vertex->out_edge);
}

/* returns true if out_edge is reset to the first incoming edge */
static bool
vnode_next_path(vnode *vertex)
{
	enode	   *edge;
	bool		last;

	if (vertex == NULL)
		return true;

	edge = vnode_get_curr_enode(vertex);
	last = vnode_next_path(edge->prev);
	if (last)
		vnode_next_enode(vertex);
	edge = vnode_get_curr_enode(vertex);
	if (edge == NULL)
	{
		vnode_next_enode(vertex);
		return true;
	}
	return false;
}

typedef struct dijkstra_pq_entry
{
	pairingheap_node ph_node;
	Graphid		to;
	double		weight;
} dijkstra_pq_entry;

static int
pq_cmp(const pairingheap_node *a, const pairingheap_node *b, void *arg)
{
	dijkstra_pq_entry *x = (dijkstra_pq_entry *) a;
	dijkstra_pq_entry *y = (dijkstra_pq_entry *) b;
	if (y->weight == x->weight)
		return 0;
	else if (y->weight > x->weight)
		return 1;
	else
		return -1;
}

static dijkstra_pq_entry *
pq_add(pairingheap *pq, MemoryContext pq_mcxt, Graphid to, double weight)
{
	dijkstra_pq_entry *n;

	Assert(MemoryContextIsValid(pq_mcxt));

	n = (dijkstra_pq_entry *) MemoryContextAlloc(pq_mcxt,
												 sizeof(dijkstra_pq_entry));
	n->to = to;
	n->weight = weight;
	pairingheap_add(pq, &n->ph_node);
	return n;
}

static Datum
eval_array(List *elems, ExprContext *econtext)
{
	int 		len;
	MemoryContext oldContext;
	Datum	   *values;
	bool	   *nulls;
	int			i;
	int			dims[1];
	int			lbs[1];
	ArrayType  *result;
	ListCell   *lc;
	Oid			element_typeid; /* common type of array elements */
	int16		elemlength;		/* typlen of the array element type */
	bool		elembyval;		/* is the element type pass-by-value? */
	char		elemalign;		/* typalign of the element type */

	len = list_length(elems);

	element_typeid = GRAPHIDOID;

	if (len == 0)
		return PointerGetDatum(construct_empty_array(element_typeid));

	oldContext = MemoryContextSwitchTo(econtext->ecxt_per_tuple_memory);

	values = palloc(len * sizeof(*values));
	nulls = palloc(len * sizeof(*nulls));
	i = 0;
	foreach(lc, elems)
	{
		Graphid *id = lfirst(lc);
		values[i] = UInt64GetDatum(*id);
		nulls[i] = false;
		i++;
	}

	dims[0] = len;
	lbs[0] = 1;

	get_typlenbyvalalign(element_typeid, &elemlength, &elembyval, &elemalign);
	result = construct_md_array(values, nulls, 1, dims, lbs, element_typeid,
								elemlength, elembyval, elemalign);

	MemoryContextSwitchTo(oldContext);

	return PointerGetDatum(result);
}

static TupleTableSlot *
proj_path(DijkstraState *node)
{
	Dijkstra   *plan;
	ProjectionInfo *projInfo;
	ExprContext *econtext;
	vnode	   *end;
	vnode	   *vertex;
	enode	   *edge;
	bool		found;
	double		weight;
	List	   *vertexes = NIL;
	List	   *edges = NIL;
	ListCell   *null_edge;
	TupleTableSlot *slot;
	Datum	   *tts_values;
	bool	   *tts_isnull;

	plan = (Dijkstra *) node->ps.plan;

	vertex = end = (vnode *) hash_search(node->visited_nodes, &node->target_id,
										 HASH_FIND, &found);
	Assert(found);

	weight = vertex->weight;
	while (vertex != NULL)
	{
		vertexes = lcons(&vertex->id, vertexes);
		edge = vnode_get_curr_enode(vertex);
		edges = lcons(&edge->id, edges);
		vertex = edge->prev;
	}

	node->n++;
	if (vnode_next_path((end)))
		node->n = node->max_n; /* no more path */

	null_edge = list_nth_cell(edges, 0);
	edges = list_delete_cell(edges, null_edge);

	projInfo = node->ps.ps_ProjInfo;
	slot = projInfo->pi_state.resultslot;
	econtext = projInfo->pi_exprContext;

	ExecClearTuple(slot);

	tts_values = slot->tts_values;
	tts_isnull = slot->tts_isnull;

	tts_values[0] = eval_array(vertexes, econtext);
	tts_isnull[0] = false;
	tts_values[1] = eval_array(edges, econtext);
	tts_isnull[1] = false;
	if (plan->weight_out)
	{
		tts_values[2] = (Datum) Float8GetDatum(weight);
		tts_isnull[2] = false;
	}
	else
	{
		tts_values[2] = (Datum) 0;
		tts_isnull[2] = true;
	}

	return ExecStoreVirtualTuple(slot);
}

static void
compute_limit(DijkstraState *node)
{
	if (node->limit)
	{
		ExprContext *econtext = node->ps.ps_ExprContext;
		Datum		val;
		bool		is_null;

		val = ExecEvalExprSwitchContext(node->limit, econtext, &is_null);
		if (is_null)
		{
			node->max_n = 1;
		}
		else
		{
			node->max_n = DatumGetInt64(val);
			if (node->max_n < 1)
				ereport(ERROR,
						(errcode(ERRCODE_INVALID_ROW_COUNT_IN_LIMIT_CLAUSE),
								errmsg("LIMIT must be larger than 0")));
		}
	}
	else
	{
		node->max_n = 1;
	}
}

/*
 * Helper function to replace the graphid portion of a vertex
 * row. It requires the vertex row and tuple descriptor be non NULL and
 * the attbyval attribute be set. It returns a pointer to the updated
 * vertex row as a HeapTuple on conclusion.
 */
static HeapTuple
replace_vertexRow_graphid(TupleDesc tupleDesc, HeapTuple vertexRow,
						  Datum graphid)
{
	HeapTupleHeader	vheader;
	Form_pg_attribute attribute;
	char			*vgdata;

	/* Verify input constraints */
	Assert(tupleDesc != NULL);
	Assert(vertexRow != NULL);

	attribute = TupleDescAttr(tupleDesc, Anum_ag_vertex_id - 1);

	/* This function only works for element 1, graphid, by value */
	Assert(attribute->attbyval);

	vheader = vertexRow->t_data;
	vgdata = (char *) vheader + vheader->t_hoff;
	vgdata = (char *) att_align_nominal(vgdata, attribute->attalign);

	store_att_byval(vgdata, graphid, attribute->attlen);

	return vertexRow;
}

static TupleTableSlot *
ExecDijkstra(PlanState *pstate)
{
	DijkstraState *node = castNode(DijkstraState, pstate);
	Dijkstra   *dijkstra;
	PlanState  *outerPlan;
	ExprContext *econtext;
	TupleTableSlot *outerTupleSlot;
	bool		is_null;
	Datum		start_vid;
	dijkstra_pq_entry *start_node;
	Datum		end_vid;
	vnode	   *vertex;

	dijkstra = (Dijkstra *) node->ps.plan;
	outerPlan = outerPlanState(node);
	econtext = node->ps.ps_ExprContext;

	/*
	 * Reset per-tuple memory context to free any expression evaluation
	 * storage allocated in the previous tuple cycle.
	 */
	ResetExprContext(econtext);

	if (node->is_executed)
	{
		if (node->n < node->max_n)
			return proj_path(node);
		else
			return NULL;
	}

	node->is_executed = true;

	compute_limit(node);

	start_vid = ExecEvalExpr(node->source, econtext, &is_null);
	start_node = pq_add(node->pq, node->pq_mcxt, DatumGetGraphid(start_vid),
						0.0);

	end_vid = ExecEvalExpr(node->target, econtext, &is_null);
	node->target_id = DatumGetGraphid(end_vid);

	vertex = hash_search(node->visited_nodes, &start_node->to, HASH_ENTER,
						 NULL);
	vertex->incoming_enodes = NIL;
	vnode_add_enode(vertex, 0.0, -1, NULL);

	while (!pairingheap_is_empty(node->pq))
	{
		bool		found;
		dijkstra_pq_entry *min_pq_entry;
		vnode	   *frontier;
		int			paramno;
		Datum		orig_param;
		ParamExecData *prm;

		min_pq_entry = (dijkstra_pq_entry *) pairingheap_remove_first(node->pq);
		if (min_pq_entry->to == node->target_id)
			return proj_path(node);

		frontier = (vnode *) hash_search(node->visited_nodes,
										 &min_pq_entry->to, HASH_FIND, &found);
		Assert(found);

		if (IsA(node->source->expr, FieldSelect))
			paramno = ((Param *) ((FieldSelect *) node->source->expr)->arg)->paramid;
		else
			paramno = ((Param *) node->source->expr)->paramid;

		prm = &(econtext->ecxt_param_exec_vals[paramno]);
		orig_param = prm->value;

		if (IsA(node->source->expr, FieldSelect))
		{
			HeapTuple vertexRow;
			vertexRow = replace_vertexRow_graphid(node->tupleDesc,
												  node->vertexRow,
												  min_pq_entry->to);
			prm->value = HeapTupleGetDatum(vertexRow);
		}
		else
			prm->value = UInt64GetDatum(min_pq_entry->to);

		outerPlan->chgParam = bms_add_member(outerPlan->chgParam, paramno);
		ExecReScan(outerPlan);

		pfree(min_pq_entry);

		for (;;)
		{
			Datum		to;
			Datum		eid;
			Datum		weight;
			Graphid		to_val;
			Graphid		eid_val;
			double		weight_val;
			double		new_weight;
			vnode	   *neighbor;

			outerTupleSlot = ExecProcNode(outerPlan);
			if (TupIsNull(outerTupleSlot))
				break;

			to = slot_getattr(outerTupleSlot, dijkstra->end_id, &is_null);
			to_val = DatumGetGraphid(to);

			eid = slot_getattr(outerTupleSlot, dijkstra->edge_id, &is_null);
			eid_val = DatumGetGraphid(eid);

			weight = slot_getattr(outerTupleSlot, dijkstra->weight, &is_null);
			weight_val = DatumGetFloat8(weight);
			if (weight_val < 0.0)
				ereport(ERROR,
						(errcode(ERRCODE_NUMERIC_VALUE_OUT_OF_RANGE),
								errmsg("WEIGHT must be larger than 0")));

			new_weight = frontier->weight + weight_val;

			neighbor = (vnode *) hash_search(node->visited_nodes, &to_val,
											 HASH_ENTER, &found);

			if (!found)
			{
				pq_add(node->pq, node->pq_mcxt, to_val, new_weight);

				neighbor->incoming_enodes = NIL;
				vnode_add_enode(neighbor, new_weight, eid_val, frontier);
			}
			else if (new_weight < neighbor->weight)
			{
				pq_add(node->pq, node->pq_mcxt, to_val, new_weight);

				vnode_update_enode(neighbor, new_weight, eid_val, frontier);
			}
			else if (node->max_n > 1 && new_weight == neighbor->weight)
			{
				/* add a same weight edge */
				vnode_add_enode(neighbor, new_weight, eid_val, frontier);
			}
		}

		/*
		 * Parameter is also used in the parent plan, so it must be restored.
		 */
		prm->value = orig_param;
	}

	node->n = node->max_n;
	return NULL;
}

DijkstraState *
ExecInitDijkstra(Dijkstra *node, EState *estate, int eflags)
{
	DijkstraState *dstate;
	HASHCTL		hash_ctl;
	PlanState  *outerPlan;

	/* check for unsupported flags */
	Assert(!(eflags & (EXEC_FLAG_BACKWARD | EXEC_FLAG_MARK)));

	/*
	 * create state structure
	 */
	dstate = makeNode(DijkstraState);
	dstate->ps.plan = (Plan *) node;
	dstate->ps.state = estate;
	dstate->ps.ExecProcNode = ExecDijkstra;

	/*
	 * Miscellaneous initialization
	 *
	 * create expression context for node
	 */
	ExecAssignExprContext(estate, &dstate->ps);
	dstate->n = 0;
	dstate->is_executed = false;
	dstate->pq = pairingheap_allocate(pq_cmp, NULL);
	dstate->pq_mcxt = AllocSetContextCreate(CurrentMemoryContext,
											"dijkstra's priority queue",
											ALLOCSET_DEFAULT_SIZES);
	hash_ctl.keysize = sizeof(Graphid);
	hash_ctl.entrysize = sizeof(vnode);
	hash_ctl.hcxt = CurrentMemoryContext;
	dstate->visited_nodes = hash_create("dijkstra's visited nodes",
										1024, &hash_ctl,
										HASH_ELEM | HASH_BLOBS | HASH_CONTEXT);

	dstate->source = ExecInitExpr((Expr *) node->source, (PlanState *) dstate);
	dstate->target = ExecInitExpr((Expr *) node->target, (PlanState *) dstate);
	dstate->limit = ExecInitExpr((Expr *) node->limit, (PlanState *) dstate);

	/*
	 * initialize child nodes
	 */
	outerPlan = ExecInitNode(outerPlan(node), estate, eflags);
	outerPlanState(dstate) = outerPlan;

	/*
	 * tuple table initialization
	 */
    ExecInitResultTupleSlotTL(&dstate->ps, &TTSOpsVirtual);
	dstate->selfTupleSlot = ExecInitExtraTupleSlot(estate, dstate->tupleDesc, &TTSOpsVirtual);

	/*
	 * ExecDijkstra was originally written without expectations that the
	 * source node might reside in a FieldSelect expression. This code
	 * is to address that deficit. Additionally, it is added here to
	 * help with memory conservation by providing a vertex row and the
	 * tuple descriptor for that vertex row for reuse.
	 */
	if (IsA(node->source, FieldSelect))
	{
		Datum       values[Natts_ag_vertex] = {0, 0, 0};
		bool        isnull[Natts_ag_vertex] = {false, true, true};
		TupleDesc	tupleDesc = lookup_rowtype_tupdesc(VERTEXOID, -1);

		Assert(tupleDesc->natts == Natts_ag_vertex);

		dstate->tupleDesc = tupleDesc;
		dstate->vertexRow = heap_form_tuple(tupleDesc, values, isnull);
	}
	else
	{
		dstate->tupleDesc = NULL;
		dstate->vertexRow = NULL;
	}

	/*
	 * initialize tuple type and projection info
	 */
	ExecAssignProjectionInfo(&dstate->ps, NULL);

	ExecSetSlotDescriptor(dstate->selfTupleSlot, ExecGetResultType(outerPlan));

	return dstate;
}

void
ExecEndDijkstra(DijkstraState *node)
{
	/* Release the tuple descriptor in the DijkstraState node */
	if (node->tupleDesc)
		ReleaseTupleDesc(node->tupleDesc);

	/* Release the vertexRow container in the DijkstraState node */
	if (node->vertexRow)
		heap_freetuple(node->vertexRow);

	/*
	 * Free the exprcontext
	 */
	ExecFreeExprContext(&node->ps);

	/*
	 * clean out the tuple table
	 */
	ExecClearTuple(node->ps.ps_ResultTupleSlot);
	ExecClearTuple(node->selfTupleSlot);

	/*
	 * close down subplans
	 */
	ExecEndNode(outerPlanState(node));
}

void
ExecReScanDijkstra(DijkstraState *node)
{
	PlanState  *outerPlan = outerPlanState(node);
	HASHCTL		hash_ctl;

	compute_limit(node);

	/*
	 * If outerPlan->chgParam is not null then plan will be automatically
	 * re-scanned by first ExecProcNode.
	 */
	if (outerPlan->chgParam == NULL)
		ExecReScan(outerPlan);

	node->n = 0;
	node->is_executed = false;

	/* reset hash table and priority queue */
	hash_destroy(node->visited_nodes);
	hash_ctl.keysize = sizeof(Graphid);
	hash_ctl.entrysize = sizeof(vnode);
	hash_ctl.hcxt = CurrentMemoryContext;
	node->visited_nodes = hash_create("dijkstra's visited nodes",
									  1024, &hash_ctl,
									  HASH_ELEM | HASH_BLOBS | HASH_CONTEXT);
	MemoryContextReset(node->pq_mcxt);
	pairingheap_reset(node->pq);

	ExecClearTuple(node->selfTupleSlot);
}
