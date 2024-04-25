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

#include "executor/execdebug.h"
#include "executor/nodeNestloopVle.h"
#include "fmgr.h"
#include "miscadmin.h"
#include "nodes/pg_list.h"
#include "utils/array.h"
#include "utils/graph.h"
#include "utils/memutils.h"

#define OUTER_CURR_VID_VARNO	1
#define OUTER_EIDS_VARNO		2
#define OUTER_EDGES_VARNO		3
#define OUTER_VERTICES_VARNO	4
#define INNER_NEXT_VID_VARNO	0
#define INNER_EID_VARNO			1
#define INNER_EDGE_VARNO		2
#define INNER_VERTEX_VARNO		3

typedef struct NestLoopVLEContext
{
	dlist_node	list;
	Datum		outer_var_datum;
	bool		outer_var_isnull;
} NestLoopVLEContext;


/* hops */
static int getInitialCurhops(NestLoopVLE *node);
static bool canFollowEdge(NestLoopVLEState *node);
static bool needResult(NestLoopVLEState *node);
/* result values */
static void pushPathElementOuter(NestLoopVLEState *node, TupleTableSlot *slot);
static void pushPathElementInner(NestLoopVLEState *node, TupleTableSlot *slot);
static void popPathElement(NestLoopVLEState *node);
/* additional ArrayBuildState operations */
static bool arrayResultHas(ArrayBuildState *astate, Datum elem);
static void arrayResultPop(ArrayBuildState *astate);
static void arrayResultClear(ArrayBuildState *astate);
/* context */
static void storeOuterVar(NestLoopVLEState *node, TupleTableSlot *slot);
static void restoreOuterVar(NestLoopVLEState *node, TupleTableSlot *slot);
static void setNextOuterVar(TupleTableSlot *outer_slot,
							TupleTableSlot *inner_slot);
static void fetchOuterVars(NestLoopVLE *nlv, ExprContext *econtext,
						  TupleTableSlot *outerTupleSlot, PlanState *innerPlan);
/* result slot */
static void adjustResult(NestLoopVLEState *node, TupleTableSlot *slot);
/* cleanup */
static void freePlanStateChgParam(PlanState *node);


static TupleTableSlot *
ExecNestLoopVLE(PlanState *pstate)
{
	NestLoopVLEState *node = castNode(NestLoopVLEState, pstate);
	NestLoopVLE *nlv;
	PlanState  *innerPlan;
	PlanState  *outerPlan;
	TupleTableSlot *outerTupleSlot;
	TupleTableSlot *innerTupleSlot;
	ExprState  *otherqual;
	ExprContext *econtext;

	CHECK_FOR_INTERRUPTS();

	/*
	 * get information from the node
	 */
	ENLV1_printf("getting info from node");

	nlv = (NestLoopVLE *) node->nls.js.ps.plan;
	otherqual = node->nls.js.ps.qual;

	/*
	 * outerPlan is only for;
	 * 1) the first edge in the path if node->curhops == 1
	 * 2) the first vertex in the path if node->curhops == 0
	 */
	outerPlan = outerPlanState(node);
	/* innerPlan is for the rest of <vertex, edge> pairs in the path. */
	innerPlan = innerPlanState(node);

	econtext = node->nls.js.ps.ps_ExprContext;

	/*
	 * Reset per-tuple memory context to free any expression evaluation
	 * storage allocated in the previous tuple cycle.
	 */
	ResetExprContext(econtext);

	/*
	 * Ok, everything is setup for the join so now loop until we return a
	 * qualifying join tuple.
	 */
	ENLV1_printf("entering main loop");

	/*
	 *      outer              inner               inner         ...
	 *
	 * prev  ids  curr |          id  next |          id  next |
	 *   -----------   |   () ----------   |   () ----------   | ...
	 *      edges      | vertex  edge      | vertex  edge      |
	 */
	for (;;)
	{
		/*
		 * If we don't have an outer tuple, get the next one and reset the
		 * inner scan.
		 */
		if (node->nls.nl_NeedNewOuter)
		{
			ENL1_printf("getting new outer tuple");
			outerTupleSlot = ExecProcNode(outerPlan);

			/*
			 * if there are no more outer tuples, then the join is complete..
			 */
			if (TupIsNull(outerTupleSlot))
			{
				ENLV1_printf("no outer tuple, ending join");
				return NULL;
			}

			if (canFollowEdge(node))
			{
				ENLV1_printf("saving new outer tuple information");
				econtext->ecxt_outertuple = outerTupleSlot;
				node->nls.nl_NeedNewOuter = false;

				/*
				 * Store eid to guarantee edge-uniqueness, and edge to generate
				 * edge array for each result.
				 */
				pushPathElementOuter(node, outerTupleSlot);

				fetchOuterVars(nlv, econtext, outerTupleSlot, innerPlan);

				/*
				 * now rescan the inner plan
				 */
				ENLV1_printf("rescanning inner plan");
				ExecReScan(innerPlan);

				/*
				 * in the case that <curhops, minHops> is either <0, 0> or
				 * <1, 1> (which is the starting point)
				 */
				if (needResult(node))
				{
					node->curhops++;

					/*
					 * It is safe to return outerTupleSlot instead of
					 * ps_ResultTupleSlot because the upper plan will only
					 * access the first 3~5 columns of ps_ResultTupleSlot
					 * which is the same with those of outerTupleSlot.
					 */
					return outerTupleSlot;
				}
				else
				{
					node->curhops++;
				}
			}
			else
			{
				/*
				 * Here is only for the case that <curhops, minHops, maxHops>
				 * is either <0, 0, 0> or <1, 1, 1>. innerPlan will never be
				 * executed. (node->nls.nl_NeedNewOuter == true)
				 */
				Assert(node->curhops == nlv->minHops);
				Assert(node->curhops == nlv->maxHops);

				/* It is safe to return outerTupleSlot here too. */
				return outerTupleSlot;
			}
		}
		else
		{
			/*
			 * Inner loop will use this outer tuple to access the outer
			 * variable and make a result.
			 */
			outerTupleSlot = econtext->ecxt_outertuple;
		}

		/*
		 * we have an outerTuple, try to get the next inner tuple.
		 */
		ENLV1_printf("getting new inner tuple");

		innerTupleSlot = ExecProcNode(innerPlan);
		econtext->ecxt_innertuple = innerTupleSlot;

		if (TupIsNull(innerTupleSlot))
		{
			if (node->curhops == getInitialCurhops(nlv) + 1)
			{
				ENLV1_printf("no inner tuple, need new outer tuple");
				node->nls.nl_NeedNewOuter = true;
			}
			else
			{
				ENLV1_printf("no inner tuple, go to previous inner plan");

				ExecPrevContext(innerPlan);

				restoreOuterVar(node, outerTupleSlot);

				/*
				 * We do this at here so that the remaning scans for the
				 * previous edge can use the right outer variable. We don't
				 * pass innerPlan because chgParam's are properly managed by
				 * ExecProcNode() and ExecNextContext()/ExecPrevContext().
				 * The remaning scans will see chgParam's for them and be able
				 * to determine whether they have to re-scan or not.
				 */
				fetchOuterVars(nlv, econtext, outerTupleSlot, NULL);
			}

			popPathElement(node);

			node->curhops--;

			/*
			 * return to top of loop for a new outer tuple or inner tuple of
			 * the previous edge.
			 */
			continue;
		}

		/*
		 * at this point we have a new pair of inner and outer tuples so we
		 * test the inner and outer tuples to see if they satisfy the node's
		 * qualification.
		 *
		 * Only the joinquals determine MatchedOuter status, but all quals
		 * must pass to actually return the tuple.
		 */
		ENLV1_printf("testing qualification");

		if (!arrayResultHas(node->eids,
							innerTupleSlot->tts_values[INNER_EID_VARNO]))
		{
			if (ExecQual(otherqual, econtext))
			{
				TupleTableSlot *result;

				/*
				 * qualification was satisfied so we project and return the
				 * slot containing the result tuple using ExecProject().
				 */
				ENLV1_printf("qualification succeeded, projecting tuple");

				if (canFollowEdge(node))
				{
					/*
					 * Store eid to guarantee edge-uniqueness, and edge/vertex
					 * to generate edge/vertex array for each result.
					 */
					pushPathElementInner(node, innerTupleSlot);

					/*
					 * Store the current outer variable so that the remaining
					 * scans for the current edge can use it later.
					 * SeqScan uses it for each tuple. Index(Only)Scan uses it
					 * when runtime keys are evaluated while re-scanning.
					 *
					 * See fetchOuterVars() and ExecIndexEvalRuntimeKeys().
					 */
					storeOuterVar(node, outerTupleSlot);

					/*
					 * Set the outer variable to the next vid so that;
					 * 1) ExecProject() can use the value of
					 *    INNER_NEXT_VID_VARNO for OUTER_CURR_VID_VARNO.
					 * 2) innerScan for the next edge can use it.
					 */
					setNextOuterVar(outerTupleSlot, innerTupleSlot);

					/*
					 * The result will have outerTupleSlot + innerTupleSlot.
					 *
					 * If innerScan is re-scanned for the next edge,
					 * ecxt_per_tuple_memory in Index(Only)Scan is reset.
					 * This means that the values in the slot from the
					 * innerPlan will be no longer valid after the reset.
					 * So, we should project the result at here.
					 */
					if (needResult(node))
						result = ExecProject(node->nls.js.ps.ps_ProjInfo);
					else
						result = NULL;

					ExecNextContext(innerPlan);

					fetchOuterVars(nlv, econtext, outerTupleSlot, innerPlan);

					/*
					 * now rescan the inner plan
					 *
					 * For now, there are three kinds of innerPlan.
					 *
					 * 1. scan0 (SeqScan|IndexScan|IndexOnlyScan)
					 *
					 * ExecReScan(innerPlan) re-scans scan0 immediately if a
					 * scan descriptor for it already exists. If not, the first
					 * ExecProcNode(innerPlan) will re-scan it.
					 *
					 * 2. Result (may not exist depending on queries)
					 *      Append (label hierarchy)
					 *        scan0
					 *        scan1 *
					 *        scan2
					 *
					 * ExecReScan(innerPlan) does not re-scan scan0~2 because
					 * re-scaning Result/Append does not re-scan their subplans
					 * immediately. Instead, the first ExecProcNode() over
					 * scan0~2 will re-scan them if their chgParam is set.
					 * (Although ExecReScan() over them is called by calling
					 * ExecProcNode(), it considers the condition described at
					 * 1 above.)
					 *
					 * Let's assume that we need to get the next edge of the
					 * current edge while scanning scan1.
					 * a. ExecNextContext(innerPlan) stores the scan context of
					 *    the current edge which is now previous edge.
					 * b. fetchOuterVars() fetchs outer variables and then sets
					 *    chgParam for Result/Append.
					 * c. ExecReScan(innerPlan) and future
					 *    ExecProcNode(innerPlan) calls eventually unset all
					 *    the chgParam's.
					 * d. ExecPrevContext(innerPlan) restores the scan context
					 *    of the previous edge.
					 * If we don't care about chgParam's, the first
					 * ExecProcNode(scan2) will not be re-scanned because its
					 * chgParam is unset.
					 * This is why storing/restoring chgParam's is important.
					 *
					 * 3. NestLoop (if vertices have to be returned)
					 *      <the same with 1 or 2 above, for vertices>
					 *      <the same with 1 or 2 above, for edges>
					 */
					ENLV1_printf("rescanning inner plan");
					ExecReScan(innerPlan);

					node->curhops++;

					if (result != NULL)
					{
						adjustResult(node, result);
						return result;
					}
				}
				else
				{
					/* NOTE: Can we eliminate edge/vertex copy here? */
					pushPathElementInner(node, innerTupleSlot);

					/*
					 * It is okay not to store the current outer variable
					 * because it is already fetched.
					 */
					setNextOuterVar(outerTupleSlot, innerTupleSlot);

					result = ExecProject(node->nls.js.ps.ps_ProjInfo);
					adjustResult(node, result);

					popPathElement(node);

					return result;
				}
			}
			else
			{
				InstrCountFiltered2(node, 1);
			}
		}
		else
		{
			InstrCountFiltered1(node, 1);
		}

		/*
		 * Tuple fails qual, so free per-tuple memory and try again.
		 */
		ResetExprContext(econtext);

		ENLV1_printf("qualification failed, looping");
	}
}

/* ----------------------------------------------------------------
 *		ExecInitNestLoopVLE
 * ----------------------------------------------------------------
 */
NestLoopVLEState *
ExecInitNestLoopVLE(NestLoopVLE *node, EState *estate, int eflags)
{
	NestLoopVLEState *nlvstate;
	TupleDesc	innerTupleDesc;
	Oid			element_type;

	/* check for unsupported flags */
	Assert(!(eflags & (EXEC_FLAG_BACKWARD | EXEC_FLAG_MARK)));

	NLV1_printf("ExecInitNestLoopVLE: %s\n", "initializing node");

	/*
	 * create state structure
	 */
	nlvstate = makeNode(NestLoopVLEState);
	nlvstate->nls.js.ps.plan = (Plan *) node;
	nlvstate->nls.js.ps.state = estate;
	nlvstate->nls.js.ps.ExecProcNode = ExecNestLoopVLE;

	/*
	 * Miscellaneous initialization
	 *
	 * create expression context for node
	 */
	ExecAssignExprContext(estate, &nlvstate->nls.js.ps);

	/*
	 * initialize child expressions
	 */
	nlvstate->nls.js.ps.qual =
		ExecInitQual(node->nl.join.plan.qual, (PlanState *) nlvstate);
	nlvstate->nls.js.jointype = node->nl.join.jointype;

	/*
	 * initialize child nodes
	 *
	 * If we have no parameters to pass into the inner rel from the outer,
	 * tell the inner child that cheap rescans would be good.  If we do have
	 * such parameters, then there is no point in REWIND support at all in the
	 * inner child, because it will always be rescanned with fresh parameter
	 * values.
	 */
	outerPlanState(nlvstate) = ExecInitNode(outerPlan(node), estate, eflags);
	if (node->nl.nestParams == NIL)
		eflags |= EXEC_FLAG_REWIND;
	else
		eflags &= ~EXEC_FLAG_REWIND;
	innerPlanState(nlvstate) = ExecInitNode(innerPlan(node), estate, eflags);


	if (node->nl.join.jointype != JOIN_VLE)
		elog(ERROR, "unrecognized join type: %d", (int) node->nl.join.jointype);

	/*
	 * tuple table initialization / initialize tuple type and projection info
	 */
	ExecInitResultTupleSlotTL(&nlvstate->nls.js.ps, &TTSOpsVirtual);
	ExecAssignProjectionInfo(&nlvstate->nls.js.ps, NULL);

	nlvstate->curhops = getInitialCurhops(node);

	innerTupleDesc =
			innerPlanState(nlvstate)->ps_ResultTupleSlot->tts_tupleDescriptor;
	element_type = TupleDescAttr(innerTupleDesc, INNER_EID_VARNO)->atttypid;
	nlvstate->eids = initArrayResult(element_type, CurrentMemoryContext, false);
	/*
	 * {prev, curr, ids | next, id} + {edges | edge}
	 * See genVLESubselect().
	 */
	if (list_length(nlvstate->nls.js.ps.plan->targetlist) >= 7)
	{
		element_type = TupleDescAttr(innerTupleDesc, INNER_EDGE_VARNO)->atttypid;
		nlvstate->edges = initArrayResult(element_type, CurrentMemoryContext,
										  false);
	}
	/*
	 * {prev, curr, ids, edges | next, id, edge} + {vertices | vertex}
	 * See genVLESubselect().
	 */
	if (list_length(nlvstate->nls.js.ps.plan->targetlist) == 9)
	{
		element_type = TupleDescAttr(innerTupleDesc, INNER_VERTEX_VARNO)->atttypid;
		nlvstate->vertices = initArrayResult(element_type, CurrentMemoryContext,
											 false);
	}

	dlist_init(&nlvstate->ctxs_head);
	nlvstate->prev_ctx_node = &nlvstate->ctxs_head.head;

	/*
	 * finally, wipe the current outer tuple clean.
	 */
	nlvstate->nls.nl_NeedNewOuter = true;

	NLV1_printf("ExecInitNestLoopVLE: %s\n", "node initialized");

	return nlvstate;
}

/* ----------------------------------------------------------------
 *		ExecEndNestLoopVLE
 *
 *		closes down scans and frees allocated storage
 * ----------------------------------------------------------------
 */
void
ExecEndNestLoopVLE(NestLoopVLEState *node)
{
	dlist_mutable_iter iter;
	int			ctx_depth;
	NestLoopVLE *nlv = (NestLoopVLE *) node->nls.js.ps.plan;

	NLV1_printf("ExecEndNestLoopVLE: %s\n", "ending node processing");

	/*
	 * Free the exprcontext
	 */
	ExecFreeExprContext(&node->nls.js.ps);

	/*
	 * clean out the tuple table
	 */
	ExecClearTuple(node->nls.js.ps.ps_ResultTupleSlot);

	arrayResultClear(node->eids);
	if (node->edges != NULL)
		arrayResultClear(node->edges);
	if (node->vertices != NULL)
		arrayResultClear(node->vertices);

	dlist_foreach_modify(iter, &node->ctxs_head)
	{
		NestLoopVLEContext *ctx;

		dlist_delete(iter.cur);

		ctx = dlist_container(NestLoopVLEContext, list, iter.cur);
		pfree(ctx);
	}
	node->prev_ctx_node = &node->ctxs_head.head;

	/*
	 * Back out our context stack, if necessary. It is important to note that
	 * the context depth is calculated ONLY to make it easier to follow. It is
	 * built from the fact that curhops can start from 0 or 1 and that for
	 * the first two increments, it does NOT push a context on the stack.
	 * Note that negative values for depth equate to a depth of zero.
	 *
	 * Additionally, we need to free any chgParam BMSets that might have
	 * been pending in the PlanState nodes.
	 *
	 * We do all of this because the LIMIT clause interrupts processing,
	 * leaving the VLE contexts in incomplete states. This causes memory
	 * issues that can crash the session.
	 */

	ctx_depth = node->curhops - (getInitialCurhops(nlv) + 1);
	while (ctx_depth > 0)
	{
		freePlanStateChgParam(innerPlanState(node));
		ExecPrevContext(innerPlanState(node));
		ctx_depth--;
	}

	/*
	 * close down subplans
	 */
	ExecEndNode(outerPlanState(node));
	ExecEndNode(innerPlanState(node));

	NLV1_printf("ExecEndNestLoopVLE: %s\n", "node processing ended");
}

/*
 * Recursive function to decend a tree of PlanState nodes and free up
 * their chgParam Bitmapsets.
 */
static void
freePlanStateChgParam(PlanState *node)
{
	PlanState *ps = NULL;

	/* base case, NULL leaf, nothing to do */
	if (node == NULL)
		return;

	/* error out if the stack gets too deep */
	check_stack_depth();

	/* we need to deal with each case differently */
	switch (nodeTag(node))
	{
		case T_SeqScanState:
			ps = &((SeqScanState *) node)->ss.ps;
			break;
		case T_IndexScanState:
			ps = &((IndexScanState *) node)->ss.ps;
			break;
		case T_IndexOnlyScanState:
			ps = &((IndexOnlyScanState *) node)->ss.ps;
			break;
		case T_ResultState:
			ps = &((ResultState *) node)->ps;
			break;
		case T_NestLoopState:
			ps = &((NestLoopState *) node)->js.ps;
			break;
		case T_AppendState:
			ps = &((AppendState *) node)->ps;
			{
				AppendState *as = (AppendState *) node;
				int			i;

				for (i = 0; i < as->as_nplans; i++)
					freePlanStateChgParam(as->appendplans[i]);
			}
			break;
		default:
			elog(ERROR, "unrecognized node type: %d", (int) nodeTag(node));
	}

	/* recurse on the PlanState's children */
	freePlanStateChgParam(ps->lefttree);
	freePlanStateChgParam(ps->righttree);

	/* if chgParam is not NULL, free it now */
	if (ps->chgParam != NULL)
	{
		bms_free(ps->chgParam);
		ps->chgParam = NULL;
	}
}

/* ----------------------------------------------------------------
 *		ExecReScanNestLoopVLE
 * ----------------------------------------------------------------
 */
void
ExecReScanNestLoopVLE(NestLoopVLEState *node)
{
	PlanState  *outerPlan = outerPlanState(node);

	/*
	 * If outerPlan->chgParam is not null then plan will be automatically
	 * re-scanned by first ExecProcNode.
	 */
	if (outerPlan->chgParam == NULL)
		ExecReScan(outerPlan);

	/*
	 * innerPlan is re-scanned for each new outer tuple and MUST NOT be
	 * re-scanned from here or you'll get troubles from inner index scans when
	 * outer Vars are used as run-time keys...
	 */

	node->nls.nl_NeedNewOuter = true;

	node->curhops = getInitialCurhops((NestLoopVLE *) node->nls.js.ps.plan);

	arrayResultClear(node->eids);
	if (node->edges != NULL)
		arrayResultClear(node->edges);
	if (node->vertices != NULL)
		arrayResultClear(node->vertices);

	node->prev_ctx_node = &node->ctxs_head.head;
}

static int
getInitialCurhops(NestLoopVLE *node)
{
	return node->minHops == 0 ? 0 : 1;
}

static bool
canFollowEdge(NestLoopVLEState *node)
{
	NestLoopVLE *nlv = (NestLoopVLE *) node->nls.js.ps.plan;

	/* infinite (-1) */
	if (nlv->maxHops < 0)
		return true;

	if (node->curhops < nlv->maxHops)
		return true;

	return false;
}

static bool
needResult(NestLoopVLEState *node)
{
	NestLoopVLE *nlv = (NestLoopVLE *) node->nls.js.ps.plan;

	return node->curhops >= nlv->minHops;
}

static void
pushPathElementOuter(NestLoopVLEState *node, TupleTableSlot *slot)
{
	FormData_pg_attribute *attrs = slot->tts_tupleDescriptor->attrs;
	IntArray	upper;
	Datum		value;
	bool		isnull;
	/* zero-length VLE does not have the first edge and its ID in outerPlan */
	if (node->curhops == 0)
		return;

	upper.indx[0] = 1;
	value = array_get_element(slot->tts_values[OUTER_EIDS_VARNO],
							  1,
							  upper.indx,
							  attrs[OUTER_EIDS_VARNO].attlen,
							  node->eids->typlen,
							  node->eids->typbyval,
							  node->eids->typalign,
							  &isnull);
	Assert(!isnull);
	accumArrayResult(node->eids, value, isnull, node->eids->element_type,
					 CurrentMemoryContext);

	if (node->edges != NULL)
	{
		value = array_get_element(slot->tts_values[OUTER_EDGES_VARNO],
								  1, upper.indx,
								  attrs[OUTER_EDGES_VARNO].attlen,
								  node->edges->typlen, node->edges->typbyval,
								  node->edges->typalign, &isnull);
		Assert(!isnull);
		accumArrayResult(node->edges, value, isnull, node->edges->element_type,
						 CurrentMemoryContext);
	}
}

static void
pushPathElementInner(NestLoopVLEState *node, TupleTableSlot *slot)
{
	FormData_pg_attribute *attrs = slot->tts_tupleDescriptor->attrs;

	accumArrayResult(node->eids, slot->tts_values[INNER_EID_VARNO],
					 slot->tts_isnull[INNER_EID_VARNO],
					 attrs[INNER_EID_VARNO].atttypid,
					 CurrentMemoryContext);
	if (node->edges != NULL)
		accumArrayResult(node->edges, slot->tts_values[INNER_EDGE_VARNO],
						 slot->tts_isnull[INNER_EDGE_VARNO],
						 attrs[INNER_EDGE_VARNO].atttypid,
						 CurrentMemoryContext);
	if (node->vertices != NULL)
		accumArrayResult(node->vertices, slot->tts_values[INNER_VERTEX_VARNO],
						 slot->tts_isnull[INNER_VERTEX_VARNO],
						 attrs[INNER_VERTEX_VARNO].atttypid,
						 CurrentMemoryContext);
}

static void
popPathElement(NestLoopVLEState *node)
{
	arrayResultPop(node->eids);
	if (node->edges != NULL)
		arrayResultPop(node->edges);
	if (node->vertices != NULL)
		arrayResultPop(node->vertices);
}

static bool
arrayResultHas(ArrayBuildState *astate, Datum elem)
{
	int			i;

	for (i = 0; i < astate->nelems; i++)
	{
		Graphid		cur_array_gid = DatumGetGraphid(astate->dvalues[i]);
		Graphid		elem_gid = DatumGetGraphid(elem);

		if (cur_array_gid == elem_gid)
			return true;
	}

	return false;
}

static void
arrayResultPop(ArrayBuildState *astate)
{
	if (astate->nelems > 0)
	{
		astate->nelems--;
		if (!astate->typbyval)
			pfree(DatumGetPointer(astate->dvalues[astate->nelems]));
	}
}

static void
arrayResultClear(ArrayBuildState *astate)
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
storeOuterVar(NestLoopVLEState *node, TupleTableSlot *slot)
{
	dlist_node *ctx_node;
	NestLoopVLEContext *ctx;

	if (dlist_has_next(&node->ctxs_head, node->prev_ctx_node))
	{
		ctx_node = dlist_next_node(&node->ctxs_head, node->prev_ctx_node);
		ctx = dlist_container(NestLoopVLEContext, list, ctx_node);
	}
	else
	{
		ctx = palloc(sizeof(*ctx));
		ctx_node = &ctx->list;

		dlist_push_tail(&node->ctxs_head, ctx_node);
	}

	/* here, we assume that the outer variable is graphid which is typbyval */
	ctx->outer_var_datum = slot->tts_values[OUTER_CURR_VID_VARNO];
	ctx->outer_var_isnull = slot->tts_isnull[OUTER_CURR_VID_VARNO];

	node->prev_ctx_node = ctx_node;
}

static void
restoreOuterVar(NestLoopVLEState *node, TupleTableSlot *slot)
{
	dlist_node *ctx_node;
	NestLoopVLEContext *ctx;

	ctx_node = node->prev_ctx_node;
	Assert(ctx_node != &node->ctxs_head.head);

	if (dlist_has_prev(&node->ctxs_head, ctx_node))
		node->prev_ctx_node = dlist_prev_node(&node->ctxs_head, ctx_node);
	else
		node->prev_ctx_node = &node->ctxs_head.head;

	ctx = dlist_container(NestLoopVLEContext, list, ctx_node);
	/* here, we assume that the outer variable is graphid which is typbyval */
	slot->tts_values[OUTER_CURR_VID_VARNO] = ctx->outer_var_datum;
	slot->tts_isnull[OUTER_CURR_VID_VARNO] = ctx->outer_var_isnull;
}

static void
setNextOuterVar(TupleTableSlot *outer_slot, TupleTableSlot *inner_slot)
{
	/* here, we assume that the outer variable is graphid which is typbyval */
	outer_slot->tts_values[OUTER_CURR_VID_VARNO]
			= inner_slot->tts_values[INNER_NEXT_VID_VARNO];
	outer_slot->tts_isnull[OUTER_CURR_VID_VARNO]
			= inner_slot->tts_isnull[INNER_NEXT_VID_VARNO];
}

/*
 * fetch the values of any outer Vars that must be passed to the inner scan,
 * and store them in the appropriate PARAM_EXEC slots.
 */
static void
fetchOuterVars(NestLoopVLE *nlv, ExprContext *econtext,
			   TupleTableSlot *outerTupleSlot, PlanState *innerPlan)
{
	ListCell   *lc;

	foreach(lc, nlv->nl.nestParams)
	{
		NestLoopParam *nlp = (NestLoopParam *) lfirst(lc);
		int			paramno = nlp->paramno;
		ParamExecData *prm;

		prm = &(econtext->ecxt_param_exec_vals[paramno]);
		/* Param value should be an OUTER_VAR var */
		Assert(IsA(nlp->paramval, Var));
		Assert(nlp->paramval->varno == OUTER_VAR);
		Assert(nlp->paramval->varattno > 0);
		prm->value = slot_getattr(outerTupleSlot,
								  nlp->paramval->varattno,
								  &(prm->isnull));
		/* Flag parameter value as changed */
		if (innerPlan != NULL)
			innerPlan->chgParam = bms_add_member(innerPlan->chgParam, paramno);
	}
}

/*
 * The following attributes from outerTupleSlot are invalid when it is created.
 *
 * OUTER_EIDS_VARNO
 * OUTER_EDGES_VARNO
 * OUTER_VERTICES_VARNO
 *
 * This function replaces the values with proper values of the current hop.
 */
static void
adjustResult(NestLoopVLEState *node, TupleTableSlot *slot)
{
	ExprContext *econtext = node->nls.js.ps.ps_ExprContext;
	MemoryContext tupmctx = econtext->ecxt_per_tuple_memory;

	slot->tts_values[OUTER_EIDS_VARNO] = makeArrayResult(node->eids, tupmctx);
	slot->tts_isnull[OUTER_EIDS_VARNO] = false;
	if (node->edges != NULL)
	{
		slot->tts_values[OUTER_EDGES_VARNO] = makeArrayResult(node->edges,
															  tupmctx);
		slot->tts_isnull[OUTER_EDGES_VARNO] = false;
	}
	if (node->vertices != NULL)
	{
		slot->tts_values[OUTER_VERTICES_VARNO] = makeArrayResult(node->vertices,
																 tupmctx);
		slot->tts_isnull[OUTER_VERTICES_VARNO] = false;
	}
}
