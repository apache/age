/*
 * execGraphVle.c
 *	  GraphDatabase VLE Executor.
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
 *	  src/backend/executor/execGraphVle.c
 */

#include "postgres.h"

#include "executor/execGraphVle.h"
#include "executor/executor.h"
#include "ag_const.h"
#include "catalog/pg_inherits.h"
#include "utils/lsyscache.h"
#include "catalog/ag_graph_fn.h"
#include "access/table.h"
#include "access/relscan.h"
#include "access/tableam.h"
#include "access/skey.h"
#include "utils/fmgroids.h"

#define VAR_START_VID	0
#define VAR_END_VID		1
#define VAR_EDGE_IDS	2
#define VAR_EDGES		3
#define VAR_VERTICES	4

#define VLERel(vleplan) ((CypherRel *) ((vleplan)->vle_rel))

#define MAXIMUM_OUTPUT_DEPTH_UNLIMITED  (INT_MAX)

static TupleTableSlot *ExecGraphVLE(PlanState *pstate);

static bool ExecGraphVLEDFS(GraphVLEState *vle_state, Graphid start_id);

static void array_clear(ArrayBuildState *astate);
static void array_pop(ArrayBuildState *astate);
static inline bool array_has(ArrayBuildState *astate, Graphid edge_id);

/*
 * Idea for Edge uniqueness.
 *
 * VLEDepthCtx have rel_index. so, we can use rel_index to identify the target
 * table.
 *
 * Bitmap set designed to 2-byte. but, label id is 6-byte...
 */

typedef struct VLEDepthCtx
{
	TableScanDesc desc;
	int			rel_index;
	Graphid		start_id;
	Graphid		end_id;

	Graphid		prev_end_id;
	uint8		direction_rotate;
} VLEDepthCtx;

static inline bool is_over_max_depth(GraphVLEState *vle_state, int depth);
static bool create_scan_desc(GraphVLEState *vle_state, VLEDepthCtx *vle_depth_ctx);
static bool create_none_direction_scan_desc(GraphVLEState *vle_state,
											VLEDepthCtx *vle_depth_ctx);
static void free_scan_desc(VLEDepthCtx *vle_depth_ctx);

GraphVLEState *
ExecInitGraphVLE(GraphVLE *vleplan, EState *estate, int eflags)
{
	GraphVLEState *vle_state;
	CypherRel  *vel_rel = VLERel(vleplan);	/* For searching labels. */
	List	   *scan_label_oids = NIL;
	char	   *label_name;
	Oid			label_rel_id;
	ResultRelInfo *target_rel_infos;
	ListCell   *list_cell;
	A_Indices  *vle_var_len;

	/* check for unsupported flags */
	Assert(!(eflags & (EXEC_FLAG_BACKWARD | EXEC_FLAG_MARK)));

	/*
	 * create state structure
	 */
	vle_state = makeNode(GraphVLEState);
	vle_state->ps.plan = (Plan *) vleplan;
	vle_state->ps.state = estate;
	vle_state->ps.ExecProcNode = ExecGraphVLE;

	vle_state->subplan = ExecInitNode(vleplan->subplan, estate, eflags);
	vle_state->need_new_sp_tuple = true;

	vle_var_len = (A_Indices *) (VLERel(vleplan)->varlen);
	vle_state->minimum_output_depth = ((A_Const *) vle_var_len->lidx)->val.val.ival;
	if (vle_var_len->uidx != NULL)
	{
		vle_state->maximum_output_depth = ((A_Const *) vle_var_len->uidx)->val.val.ival;
	}
	else
	{
		vle_state->maximum_output_depth = MAXIMUM_OUTPUT_DEPTH_UNLIMITED;
	}
	vle_state->cypher_rel_direction = VLERel(vleplan)->direction;

	/*
	 * initialize tuple type and projection info
	 */
	vle_state->slot = ExecAllocTableSlot(&estate->es_tupleTable,
										 NULL,
										 &TTSOpsMinimalTuple);
	vle_state->ps.ps_ResultTupleSlot = vle_state->slot;

	ExecAssignProjectionInfo(&vle_state->ps, NULL);
	vle_state->ps.ps_ResultTupleDesc = ExecTypeFromTL(vleplan->subplan->targetlist);
	ExecSetSlotDescriptor(vle_state->slot,
						  vle_state->ps.ps_ResultTupleDesc);
	ExecAssignExprContext(estate, &vle_state->ps);

	vle_state->use_vertex_output = vle_state->ps.ps_ResultTupleDesc->natts > VAR_VERTICES;

	/* P-Map Jsonb */
	if (VLERel(vleplan)->prop_map)
	{
		ExprContext *econtext = vle_state->ps.ps_ExprContext;
		bool		is_null = false;
		ExprState  *prop_map_expr = ExecInitExpr((Expr *) VLERel(vleplan)->prop_map,
												 &vle_state->ps);

		vle_state->jsonb_filter = DatumGetJsonbP(ExecEvalExpr(prop_map_expr,
															  econtext,
															  &is_null));
		ResetExprContext(econtext);
	}
	else
	{
		vle_state->jsonb_filter = NULL;
	}


	/*
	 * Find all target labels.
	 */
	label_name = (vel_rel->types == NIL) ?
		AG_EDGE : getCypherName(linitial(vel_rel->types));
	label_rel_id = get_laboid_relid(get_labname_laboid(label_name,
													   get_graph_path_oid()));
	scan_label_oids = lappend_oid(scan_label_oids, label_rel_id);
	if (!vel_rel->only)
	{
		scan_label_oids = list_concat_unique_oid(scan_label_oids,
												 find_all_inheritors(label_rel_id,
																	 AccessShareLock,
																	 NULL));
	}

	/*
	 * Fill ResultRelInfos.
	 */
	target_rel_infos = (ResultRelInfo *) palloc(
												list_length(scan_label_oids) * sizeof(ResultRelInfo));
	vle_state->target_rel_infos = target_rel_infos;
	vle_state->num_target_rel_info = list_length(scan_label_oids);

	/* Will be filled by below logic. */
	vle_state->current_scan_tuple = NULL;

	foreach(list_cell, scan_label_oids)
	{
		Oid			label_oid = lfirst_oid(list_cell);
		Relation	relation = table_open(label_oid, AccessShareLock);

		if (vle_state->current_scan_tuple == NULL)
		{
			vle_state->current_scan_tuple = table_slot_create(relation, NULL);
		}
		InitResultRelInfo(target_rel_infos,
						  relation,
						  0,	/* dummy rangetable index */
						  NULL,
						  0);
		ExecOpenIndices(target_rel_infos, false);
		target_rel_infos++;
	}

	list_free(scan_label_oids);

	/*
	 * edge_ids(2) : for edge uniqueness.
	 *
	 * edges(3) : output edge array.
	 *
	 * vertices(4).
	 */
	vle_state->edge_ids = initArrayResult(GRAPHIDOID,
										  CurrentMemoryContext,
										  false);
	vle_state->edges = initArrayResult(EDGEOID,
									   CurrentMemoryContext,
									   false);
	if (vle_state->use_vertex_output)
	{
		vle_state->vertices = initArrayResult(VERTEXOID,
											  CurrentMemoryContext,
											  false);
	}
	else
	{
		vle_state->vertices = NULL;
	}

	return vle_state;
}

static TupleTableSlot *
ExecGraphVLE(PlanState *pstate)
{
	/*
	 * =====================
	 *
	 * Alex: Many columns in tuple. but, really needed?
	 *
	 * - execProc returns below types.
	 *
	 * Case #1 (7). {prev, curr, ids | next, id} + {edges | edge} graphid,
	 * graphid, graphid[], edge[]
	 *
	 * Case #2 (9) {prev, curr, ids, edges | next, id, edge} + {vertices |
	 * vertex} graphid, graphid, graphid[], edge[], vertex[]
	 *
	 * i.e., start, "end", ARRAY[id], ARRAY[ROW(id, start, "end", properties,
	 * ctid)::edge]
	 *
	 * See genVLESubselect().
	 *
	 *
	 * - depth if, VLE low index start from 0. then, depth start from 0. but,
	 * larger than start from 1.
	 */
	GraphVLEState *vle_state = castNode(GraphVLEState, pstate);
	ExprContext *econtext = vle_state->ps.ps_ExprContext;

	for (;;)
	{
		/* fetch new subplan tuple. */
		if (vle_state->need_new_sp_tuple)
		{
			vle_state->subplan_tuple = ExecProcNode(vle_state->subplan);
			vle_state->need_new_sp_tuple = false;

			/* no more exists.. */
			if (TupIsNull(vle_state->subplan_tuple))
				return NULL;

			array_clear(vle_state->edges);
			array_clear(vle_state->edge_ids);

			vle_state->first_start_id = DatumGetGraphid(vle_state->subplan_tuple->tts_values[VAR_START_VID]);

			if (vle_state->use_vertex_output)
			{
				array_clear(vle_state->vertices);

				accumArrayResult(vle_state->vertices,
								 get_vertex_from_graphid(vle_state->first_start_id),
								 false,
								 VERTEXOID,
								 CurrentMemoryContext);
			}

			if (0 >= vle_state->minimum_output_depth &&
				0 <= vle_state->maximum_output_depth)
			{
				return vle_state->subplan_tuple;
			}
		}

		/* Do DFS. */
		if (!ExecGraphVLEDFS(vle_state,
							 vle_state->first_start_id))
		{
			vle_state->need_new_sp_tuple = true;
			continue;
		}
		else
		{
			MemoryContext tupmctx = econtext->ecxt_per_tuple_memory;

			vle_state->subplan_tuple->tts_values[VAR_END_VID] = vle_state->last_end_id;
			vle_state->subplan_tuple->tts_values[VAR_EDGE_IDS] = makeArrayResult(vle_state->edge_ids,
																				 tupmctx);
			vle_state->subplan_tuple->tts_values[VAR_EDGES] = makeArrayResult(vle_state->edges,
																			  tupmctx);

			if (vle_state->use_vertex_output)
			{
				int			ndims;
				int			dims[1];
				int			lbs[1];

				ndims = (vle_state->vertices->nelems > 0) ? 1 : 0;
				/* latest vertex is added somewhere. */
				dims[0] = vle_state->vertices->nelems - 1;
				lbs[0] = 1;
				vle_state->subplan_tuple->tts_values[VAR_VERTICES] = makeMdArrayResult(vle_state->vertices,
																					   ndims,
																					   dims,
																					   lbs,
																					   tupmctx,
																					   vle_state->vertices->private_cxt);
			}

			return vle_state->subplan_tuple;
		}
	}

	/* No more tuples. */
	return NULL;
}

/*
 * ExecGraphVLEDFS
 *
 * Adds edges, edge_ids using condition cypher relationship. Returns false if
 * there is no tuple that corresponds to the condition.
 */
static bool
ExecGraphVLEDFS(GraphVLEState *vle_state, Graphid start_id)
{
	VLEDepthCtx *vle_depth_ctx = NULL;
	Graphid		edge_id;

	/* is first time? */
	if (vle_state->table_scan_desc_list == NIL)
	{
		vle_depth_ctx = (VLEDepthCtx *) palloc(sizeof(VLEDepthCtx));
		vle_depth_ctx->desc = NULL;
		vle_depth_ctx->rel_index = 0;
		vle_depth_ctx->start_id = start_id;
		vle_depth_ctx->end_id = start_id;

		/* None-directional scanning. */
		vle_depth_ctx->direction_rotate = 0;
		vle_depth_ctx->prev_end_id = start_id;

		create_scan_desc(vle_state, vle_depth_ctx); /* never not failing */
		vle_state->table_scan_desc_list = lappend(vle_state->table_scan_desc_list,
												  vle_depth_ctx);
	}

	for (;;)
	{
		int			vle_scan_depth;
		bool		return_as_results;
		Graphid		new_start_id,
					new_end_id;

		vle_depth_ctx = llast(vle_state->table_scan_desc_list);

		if (!table_scan_getnextslot(vle_depth_ctx->desc,
									ForwardScanDirection,
									vle_state->current_scan_tuple))
		{
			/* find next target relation */
			if (!create_scan_desc(vle_state, vle_depth_ctx))
			{
				/* move back depth. */
				free_scan_desc(vle_depth_ctx);
				vle_state->table_scan_desc_list = list_delete_last(vle_state->table_scan_desc_list);
				if (vle_state->table_scan_desc_list == NIL)
				{
					/* terminate current tuple scan */
					return false;
				}
			}
			continue;
		}

		/*
		 * Fetch all attributes.
		 *
		 * todo: Make sure it is necessary from the results and branch out.
		 */
		slot_getallattrs(vle_state->current_scan_tuple);

		/* Property filtering. */
		if (vle_state->jsonb_filter)
		{
			bool		isnull;
			Jsonb	   *val = DatumGetJsonbP(slot_getattr(vle_state->current_scan_tuple,
														  Anum_table_edge_prop_map,
														  &isnull));
			Jsonb	   *tmpl = vle_state->jsonb_filter;
			JsonbIterator *it1,
					   *it2;

			it1 = JsonbIteratorInit(&val->root);
			it2 = JsonbIteratorInit(&tmpl->root);

			if (!JsonbDeepContains(&it1, &it2))
				continue;
		}

		edge_id = vle_state->current_scan_tuple->tts_values[Anum_table_edge_id - 1];

		vle_scan_depth = list_length(vle_state->table_scan_desc_list);

		while (vle_state->edge_ids->nelems >= vle_scan_depth)
		{
			array_pop(vle_state->edges);
			array_pop(vle_state->edge_ids);
			if (vle_state->use_vertex_output)
				array_pop(vle_state->vertices);
		}

		/* It is un-efficient. but, easy. */
		if (array_has(vle_state->edge_ids, edge_id))
		{
			continue;
		}

		/* Will be used from ExecGraphVLE() */
		new_start_id = DatumGetGraphid(vle_state->current_scan_tuple->tts_values[Anum_table_edge_start - 1]);
		new_end_id = DatumGetGraphid(vle_state->current_scan_tuple->tts_values[Anum_table_edge_end - 1]);

		if (vle_state->cypher_rel_direction == CYPHER_REL_DIR_RIGHT)
		{
			vle_state->last_end_id = new_end_id;
		}
		else if (vle_state->cypher_rel_direction == CYPHER_REL_DIR_LEFT)
		{
			vle_state->last_end_id = new_start_id;
		}
		else
		{
			if (vle_depth_ctx->prev_end_id == new_end_id)
			{
				vle_state->last_end_id = new_start_id;
			}
			else
			{
				vle_state->last_end_id = new_end_id;
			}
		}

		accumArrayResult(vle_state->edges,
						 make_edge_from_tuple(vle_state->current_scan_tuple),
						 false,
						 EDGEOID,
						 CurrentMemoryContext);
		accumArrayResult(vle_state->edge_ids,
						 edge_id,
						 false,
						 GRAPHIDOID,
						 CurrentMemoryContext);

		if (vle_state->use_vertex_output)
		{
			accumArrayResult(vle_state->vertices,
							 get_vertex_from_graphid(vle_state->last_end_id),
							 false,
							 VERTEXOID,
							 CurrentMemoryContext);
		}

		return_as_results = vle_state->minimum_output_depth <= vle_scan_depth &&
			vle_state->maximum_output_depth >= vle_scan_depth;

		if (!is_over_max_depth(vle_state, vle_scan_depth + 1))
		{
			/* Move next depth */
			VLEDepthCtx *top_vle_depth_ctx = vle_depth_ctx;

			vle_depth_ctx = (VLEDepthCtx *) palloc(sizeof(VLEDepthCtx));
			vle_depth_ctx->desc = NULL;
			vle_depth_ctx->rel_index = 0;
			vle_depth_ctx->start_id = new_start_id;
			vle_depth_ctx->end_id = new_end_id;

			/* None-directional scanning. */
			vle_depth_ctx->direction_rotate = 0;
			vle_depth_ctx->prev_end_id = top_vle_depth_ctx->prev_end_id == new_start_id ?
				new_end_id : new_start_id;

			create_scan_desc(vle_state, vle_depth_ctx); /* never not failing */
			vle_state->table_scan_desc_list = lappend(vle_state->table_scan_desc_list,
													  vle_depth_ctx);
		}

		if (return_as_results)
		{
			break;
		}
	}

	return true;
}

static void
free_scan_desc(VLEDepthCtx *vle_depth_ctx)
{
	if (vle_depth_ctx->desc)
	{
		table_endscan(vle_depth_ctx->desc);
	}
	pfree(vle_depth_ctx);
}

static bool
create_scan_desc(GraphVLEState *vle_state,
				 VLEDepthCtx *vle_depth_ctx)
{
	ScanKeyData scan_key_data;
	ResultRelInfo *result_rel_info = vle_state->target_rel_infos + vle_depth_ctx->rel_index;
	uint32		cypher_rel_direction = vle_state->cypher_rel_direction;

	if (vle_state->cypher_rel_direction == CYPHER_REL_DIR_NONE)
	{
		return create_none_direction_scan_desc(vle_state, vle_depth_ctx);
	}

	if (vle_depth_ctx->desc != NULL)
	{
		table_endscan(vle_depth_ctx->desc);
		vle_depth_ctx->desc = NULL;

		result_rel_info++;
		vle_depth_ctx->rel_index++;

		if (vle_depth_ctx->rel_index >= vle_state->num_target_rel_info)
		{
			return false;
		}
	}

	Assert(cypher_rel_direction != CYPHER_REL_DIR_NONE);

	if (cypher_rel_direction == CYPHER_REL_DIR_RIGHT)
	{
		/* CYPHER_REL_DIR_RIGHT, CYPHER_REL_DIR_NONE */
		ScanKeyInit(&scan_key_data,
					Anum_table_edge_start,
					BTEqualStrategyNumber,
					F_GRAPHID_EQ,
					vle_depth_ctx->end_id);
	}
	else if (cypher_rel_direction == CYPHER_REL_DIR_LEFT)
	{
		/* CYPHER_REL_DIR_LEFT, CYPHER_REL_DIR_NONE */
		ScanKeyInit(&scan_key_data,
					Anum_table_edge_end,
					BTEqualStrategyNumber,
					F_GRAPHID_EQ,
					vle_depth_ctx->start_id);
	}

	/* Create TableScanDesc */
	vle_depth_ctx->desc = table_beginscan(result_rel_info->ri_RelationDesc,
										  vle_state->ps.state->es_snapshot,
										  1,
										  &scan_key_data);

	return true;
}

static bool
create_none_direction_scan_desc(GraphVLEState *vle_state,
								VLEDepthCtx *vle_depth_ctx)
{
	ScanKeyData scan_key_data;
	ResultRelInfo *result_rel_info = vle_state->target_rel_infos + vle_depth_ctx->rel_index;

	if (vle_depth_ctx->desc != NULL)
	{
		table_endscan(vle_depth_ctx->desc);
		vle_depth_ctx->desc = NULL;

		if (vle_depth_ctx->direction_rotate > 0)
		{
			vle_depth_ctx->direction_rotate = -1;
			result_rel_info++;
			vle_depth_ctx->rel_index++;

			if (vle_depth_ctx->rel_index >= vle_state->num_target_rel_info)
			{
				return false;
			}
		}

		vle_depth_ctx->direction_rotate++;
	}

	if (vle_depth_ctx->direction_rotate == 0)
	{
		/* CYPHER_REL_DIR_RIGHT, CYPHER_REL_DIR_NONE */
		ScanKeyInit(&scan_key_data,
					Anum_table_edge_start,
					BTEqualStrategyNumber,
					F_GRAPHID_EQ,
					vle_depth_ctx->prev_end_id);
	}
	else
	{
		/* CYPHER_REL_DIR_LEFT, CYPHER_REL_DIR_NONE */
		ScanKeyInit(&scan_key_data,
					Anum_table_edge_end,
					BTEqualStrategyNumber,
					F_GRAPHID_EQ,
					vle_depth_ctx->prev_end_id);
	}

	/* Create TableScanDesc */
	vle_depth_ctx->desc = table_beginscan(result_rel_info->ri_RelationDesc,
										  vle_state->ps.state->es_snapshot,
										  1,
										  &scan_key_data);

	return true;
}

static inline bool
is_over_max_depth(GraphVLEState *vle_state, int depth)
{
	return vle_state->maximum_output_depth < depth;
}

void
ExecReScanGraphVLE(GraphVLEState *vle_state)
{
	vle_state->need_new_sp_tuple = true;
	ExecReScan(vle_state->subplan);
}

void
ExecEndGraphVLE(GraphVLEState *vle_state)
{
	int			i;
	ListCell   *lc;
	ResultRelInfo *result_rel_info = vle_state->target_rel_infos;

	foreach(lc, vle_state->table_scan_desc_list)
	{
		VLEDepthCtx *vle_depth_ctx = lfirst(lc);

		free_scan_desc(vle_depth_ctx);
	}
	list_free(vle_state->table_scan_desc_list);

	ExecDropSingleTupleTableSlot(vle_state->current_scan_tuple);

	for (i = 0; i < vle_state->num_target_rel_info; i++)
	{
		ExecCloseIndices(result_rel_info);
		table_close(result_rel_info->ri_RelationDesc, AccessShareLock);
		result_rel_info++;
	}
	pfree(vle_state->target_rel_infos);

	/*
	 * clean out the tuple table
	 */
	ExecClearTuple(vle_state->ps.ps_ResultTupleSlot);
	ExecEndNode(vle_state->subplan);
	ExecFreeExprContext(&vle_state->ps);
}

static void
array_clear(ArrayBuildState *astate)
{
	if (!astate->typbyval)
	{
		int			i;

		for (i = 0; i < astate->nelems; i++)
			pfree(DatumGetPointer(astate->dvalues[i]));
	}

	astate->nelems = 0;
}

static void
array_pop(ArrayBuildState *astate)
{
	if (astate->nelems > 0)
	{
		astate->nelems--;
		if (!astate->typbyval)
			pfree(DatumGetPointer(astate->dvalues[astate->nelems]));
	}
}

static inline bool
array_has(ArrayBuildState *astate, Graphid edge_id)
{
	int			i;

	for (i = 0; i < astate->nelems; i++)
	{
		Graphid		cur_array_gid = DatumGetGraphid(astate->dvalues[i]);

		if (cur_array_gid == edge_id)
			return true;
	}

	return false;
}
