/*-------------------------------------------------------------------------
 *
 * nodeNestloop.c
 *	  routines to support nest-loop joins
 *
 * Portions Copyright (c) 1996-2020, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *
 * IDENTIFICATION
 *	  src/backend/executor/nodeNestloop.c
 *
 *-------------------------------------------------------------------------
 */
/*
 *	 INTERFACE ROUTINES
 *		ExecNestLoop	 - process a nestloop join of two plans
 *		ExecInitNestLoop - initialize the join
 *		ExecEndNestLoop  - shut down the join
 */

#include "postgres.h"

#include "access/xact.h"
#include "executor/execdebug.h"
#include "executor/nodeModifyGraph.h"
#include "executor/nodeNestloop.h"
#include "miscadmin.h"
#include "utils/memutils.h"
#include "utils/snapmgr.h"


typedef struct NestLoopContext
{
	dlist_node	list;
	TupleTableSlot *outer_tupleslot;
	HeapTuple	outer_tuple;
} NestLoopContext;


/* ----------------------------------------------------------------
 *		ExecNestLoop(node)
 *
 * old comments
 *		Returns the tuple joined from inner and outer tuples which
 *		satisfies the qualification clause.
 *
 *		It scans the inner relation to join with current outer tuple.
 *
 *		If none is found, next tuple from the outer relation is retrieved
 *		and the inner relation is scanned from the beginning again to join
 *		with the outer tuple.
 *
 *		NULL is returned if all the remaining outer tuples are tried and
 *		all fail to join with the inner tuples.
 *
 *		NULL is also returned if there is no tuple from inner relation.
 *
 *		Conditions:
 *		  -- outerTuple contains current tuple from outer relation and
 *			 the right son(inner relation) maintains "cursor" at the tuple
 *			 returned previously.
 *				This is achieved by maintaining a scan position on the outer
 *				relation.
 *
 *		Initial States:
 *		  -- the outer child and the inner child
 *			   are prepared to return the first tuple.
 * ----------------------------------------------------------------
 */
static TupleTableSlot *
ExecNestLoop(PlanState *pstate)
{
	NestLoopState *node = castNode(NestLoopState, pstate);
	NestLoop   *nl;
	PlanState  *innerPlan;
	PlanState  *outerPlan;
	TupleTableSlot *outerTupleSlot;
	TupleTableSlot *innerTupleSlot;
	ExprState  *joinqual;
	ExprState  *otherqual;
	ExprContext *econtext;
	ListCell   *lc;

	CHECK_FOR_INTERRUPTS();

	/*
	 * get information from the node
	 */
	ENL1_printf("getting info from node");

	nl = (NestLoop *) node->js.ps.plan;
	joinqual = node->js.joinqual;
	otherqual = node->js.ps.qual;
	outerPlan = outerPlanState(node);
	innerPlan = innerPlanState(node);
	econtext = node->js.ps.ps_ExprContext;

	/*
	 * Reset per-tuple memory context to free any expression evaluation
	 * storage allocated in the previous tuple cycle.
	 */
	ResetExprContext(econtext);

	/*
	 * Ok, everything is setup for the join so now loop until we return a
	 * qualifying join tuple.
	 */
	ENL1_printf("entering main loop");

	for (;;)
	{
		/*
		 * If we don't have an outer tuple, get the next one and reset the
		 * inner scan.
		 */
		if (node->nl_NeedNewOuter)
		{
			ENL1_printf("getting new outer tuple");
			outerTupleSlot = ExecProcNode(outerPlan);

			/*
			 * if there are no more outer tuples, then the join is complete..
			 */
			if (TupIsNull(outerTupleSlot))
			{
				ENL1_printf("no outer tuple, ending join");
				return NULL;
			}

			ENL1_printf("saving new outer tuple information");
			econtext->ecxt_outertuple = outerTupleSlot;
			node->nl_NeedNewOuter = false;
			node->nl_MatchedOuter = false;

			/*
			 * fetch the values of any outer Vars that must be passed to the
			 * inner scan, and store them in the appropriate PARAM_EXEC slots.
			 */
			foreach(lc, nl->nestParams)
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
				innerPlan->chgParam = bms_add_member(innerPlan->chgParam,
													 paramno);
			}

			/*
			 * now rescan the inner plan
			 */
			ENL1_printf("rescanning inner plan");
			ExecReScan(innerPlan);
		}

		/*
		 * we have an outerTuple, try to get the next inner tuple.
		 */
		ENL1_printf("getting new inner tuple");

		innerTupleSlot = ExecProcNode(innerPlan);
		econtext->ecxt_innertuple = innerTupleSlot;

		if (TupIsNull(innerTupleSlot))
		{
			ENL1_printf("no inner tuple, need new outer tuple");

			node->nl_NeedNewOuter = true;

			if (!node->nl_MatchedOuter &&
				(node->js.jointype == JOIN_LEFT ||
				 node->js.jointype == JOIN_CYPHER_MERGE ||
				 node->js.jointype == JOIN_CYPHER_DELETE ||
				 node->js.jointype == JOIN_ANTI))
			{
				/*
				 * We are doing an outer join and there were no join matches
				 * for this outer tuple.  Generate a fake join tuple with
				 * nulls for the inner tuple, and return it if it passes the
				 * non-join quals.
				 */
				econtext->ecxt_innertuple = node->nl_NullInnerTupleSlot;

				ENL1_printf("testing qualification for outer-join tuple");

				if (otherqual == NULL || ExecQual(otherqual, econtext))
				{
					/*
					 * qualification was satisfied so we project and return
					 * the slot containing the result tuple using
					 * ExecProject().
					 */
					ENL1_printf("qualification succeeded, projecting tuple");

					return ExecProject(node->js.ps.ps_ProjInfo);
				}
				else
					InstrCountFiltered2(node, 1);
			}

			/*
			 * Otherwise just return to top of loop for a new outer tuple.
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
		ENL1_printf("testing qualification");

		if (ExecQual(joinqual, econtext))
		{
			node->nl_MatchedOuter = true;

			/* In an antijoin, we never return a matched tuple */
			if (node->js.jointype == JOIN_ANTI)
			{
				node->nl_NeedNewOuter = true;
				continue;		/* return to top of loop */
			}

			/*
			 * If we only need to join to the first matching inner tuple, then
			 * consider returning this one, but after that continue with next
			 * outer tuple.
			 */
			if (node->js.single_match)
				node->nl_NeedNewOuter = true;

			if (otherqual == NULL || ExecQual(otherqual, econtext))
			{
				/*
				 * qualification was satisfied so we project and return the
				 * slot containing the result tuple using ExecProject().
				 */
				ENL1_printf("qualification succeeded, projecting tuple");

				return ExecProject(node->js.ps.ps_ProjInfo);
			}
			else
				InstrCountFiltered2(node, 1);
		}
		else
			InstrCountFiltered1(node, 1);

		/*
		 * Tuple fails qual, so free per-tuple memory and try again.
		 */
		ResetExprContext(econtext);

		ENL1_printf("qualification failed, looping");
	}
}

/* ----------------------------------------------------------------
 *		ExecInitNestLoop
 * ----------------------------------------------------------------
 */
NestLoopState *
ExecInitNestLoop(NestLoop *node, EState *estate, int eflags)
{
	NestLoopState *nlstate;

	/* check for unsupported flags */
	Assert(!(eflags & (EXEC_FLAG_BACKWARD | EXEC_FLAG_MARK)));

	NL1_printf("ExecInitNestLoop: %s\n",
			   "initializing node");

	/*
	 * create state structure
	 */
	nlstate = makeNode(NestLoopState);
	nlstate->js.ps.plan = (Plan *) node;
	nlstate->js.ps.state = estate;
	nlstate->js.ps.ExecProcNode = ExecNestLoop;

	/*
	 * Miscellaneous initialization
	 *
	 * create expression context for node
	 */
	ExecAssignExprContext(estate, &nlstate->js.ps);

	/*
	 * initialize child nodes
	 *
	 * If we have no parameters to pass into the inner rel from the outer,
	 * tell the inner child that cheap rescans would be good.  If we do have
	 * such parameters, then there is no point in REWIND support at all in the
	 * inner child, because it will always be rescanned with fresh parameter
	 * values.
	 */
	outerPlanState(nlstate) = ExecInitNode(outerPlan(node), estate, eflags);
	if (node->nestParams == NIL)
		eflags |= EXEC_FLAG_REWIND;
	else
		eflags &= ~EXEC_FLAG_REWIND;
	innerPlanState(nlstate) = ExecInitNode(innerPlan(node), estate, eflags);

	/*
	 * Initialize result slot, type and projection.
	 */
	ExecInitResultTupleSlotTL(&nlstate->js.ps, &TTSOpsVirtual);
	ExecAssignProjectionInfo(&nlstate->js.ps, NULL);

	/*
	 * initialize child expressions
	 */
	nlstate->js.ps.qual =
		ExecInitQual(node->join.plan.qual, (PlanState *) nlstate);
	nlstate->js.jointype = node->join.jointype;
	nlstate->js.joinqual =
		ExecInitQual(node->join.joinqual, (PlanState *) nlstate);

	/*
	 * detect whether we need only consider the first matching inner tuple
	 */
	nlstate->js.single_match = (node->join.inner_unique ||
								node->join.jointype == JOIN_SEMI);

	/* set up null tuples for outer joins, if needed */
	switch (node->join.jointype)
	{
		case JOIN_INNER:
		case JOIN_SEMI:
		case JOIN_VLE:
			break;
		case JOIN_LEFT:
		case JOIN_ANTI:
		case JOIN_CYPHER_MERGE:
		case JOIN_CYPHER_DELETE:
			nlstate->nl_NullInnerTupleSlot =
				ExecInitNullTupleSlot(estate,
									  ExecGetResultType(innerPlanState(nlstate)),
									  &TTSOpsVirtual);
			break;
		default:
			elog(ERROR, "unrecognized join type: %d",
				 (int) node->join.jointype);
	}

	dlist_init(&nlstate->ctxs_head);
	nlstate->prev_ctx_node = &nlstate->ctxs_head.head;

	/*
	 * finally, wipe the current outer tuple clean.
	 */
	nlstate->nl_NeedNewOuter = true;
	nlstate->nl_MatchedOuter = false;

	NL1_printf("ExecInitNestLoop: %s\n",
			   "node initialized");

	return nlstate;
}

/* ----------------------------------------------------------------
 *		ExecEndNestLoop
 *
 *		closes down scans and frees allocated storage
 * ----------------------------------------------------------------
 */
void
ExecEndNestLoop(NestLoopState *node)
{
	dlist_mutable_iter iter;

	NL1_printf("ExecEndNestLoop: %s\n",
			   "ending node processing");

	/*
	 * Free the exprcontext
	 */
	ExecFreeExprContext(&node->js.ps);

	/*
	 * clean out the tuple table
	 */
	ExecClearTuple(node->js.ps.ps_ResultTupleSlot);

	dlist_foreach_modify(iter, &node->ctxs_head)
	{
		NestLoopContext *ctx;

		dlist_delete(iter.cur);

		ctx = dlist_container(NestLoopContext, list, iter.cur);
		pfree(ctx);
	}
	node->prev_ctx_node = &node->ctxs_head.head;

	/*
	 * close down subplans
	 */
	ExecEndNode(outerPlanState(node));
	ExecEndNode(innerPlanState(node));

	NL1_printf("ExecEndNestLoop: %s\n",
			   "node processing ended");
}

/* ----------------------------------------------------------------
 *		ExecReScanNestLoop
 * ----------------------------------------------------------------
 */
void
ExecReScanNestLoop(NestLoopState *node)
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

	node->nl_NeedNewOuter = true;
	node->nl_MatchedOuter = false;
}

void
ExecNextNestLoopContext(NestLoopState *node)
{
	ExprContext *econtext = node->js.ps.ps_ExprContext;
	dlist_node *ctx_node;
	NestLoopContext *ctx;
	TupleTableSlot *slot;

	/*
	 * This nested loop is supposed to be for vertex-edge join to get vertices
	 * for graphpath results, and that means it is the top most (and only)
	 * innerPlan of NestLoopVLE. Since it is already executed at this point,
	 * its chgParam is already NULL. So, we don't need to manage chgParam.
	 */
	Assert(node->js.ps.chgParam == NULL);
	Assert(node->js.jointype == JOIN_INNER);

	/* get the current context */
	if (dlist_has_next(&node->ctxs_head, node->prev_ctx_node))
	{
		ctx_node = dlist_next_node(&node->ctxs_head, node->prev_ctx_node);
		ctx = dlist_container(NestLoopContext, list, ctx_node);
	}
	else
	{
		ctx = palloc(sizeof(*ctx));
		ctx_node = &ctx->list;

		dlist_push_tail(&node->ctxs_head, ctx_node);
	}

	slot = econtext->ecxt_outertuple;
	if (TTS_IS_HEAPTUPLE(slot))
	{
		HeapTupleTableSlot *heapTupleTableSlot = (HeapTupleTableSlot *) slot;

		/*
		 * If tts_tuple is the same with the stored one, remove it from the
		 * slot to keep this copy from ExecStoreTuple()/ExecClearTuple() in
		 * the next nested loop.
		 *
		 * This can happen when there is a matched result with the tuple.
		 */
		if (heapTupleTableSlot->tuple == ctx->outer_tuple)
		{
			slot->tts_flags |= TTS_FLAG_EMPTY;
			slot->tts_nvalid = 0;
			ItemPointerSetInvalid(&heapTupleTableSlot->tuple->t_self);
		}
	}
	else
	{
		/*
		 * Keep the latest outer tuple slot to 1) set ecxt_outertuple to it
		 * later when continuing the current nested loop after the end of the
		 * next nested loop, and 2) use the original slot rather than a
		 * temporary slot that requires extra resources.
		 * We get the slot through ecxt_outertuple instead of
		 * outerPlanState(node)->ps_ResultTupleSlot because
		 * outerPlanState(node) can be AppendState.
		 */
		ctx->outer_tupleslot = slot;
		/*
		 * We need to copy and store the current outer tuple here because;
		 * 1) there might be a chance to unpin the underlying buffer that the
		 *    slot relies on while doing ExecStoreTuple() in the next nested
		 *    loop, and
		 * 2) when continuing the current nested loop later, the inner plan
		 *    needs the right outer variables that are in the slot.
		 * The tuple has to be stored in CurrentMemoryContext.
		 */
		ctx->outer_tuple = ExecCopySlotHeapTuple(slot);
	}
	/*
	 * We don't need to care about the inner plan and nl_NeedNewOuter because
	 * the next execution of the current nested loop must execute the inner
	 * plan.
	 */

	/* make the current context previous context */
	node->prev_ctx_node = ctx_node;

	/*
	 * We don't have to restore the current outer tuple slot because it will be
	 * filled with values of the first scan result of the outer plan.
	 */

	ExecNextContext(outerPlanState(node));
	ExecNextContext(innerPlanState(node));

	node->nl_NeedNewOuter = true;
}

void
ExecPrevNestLoopContext(NestLoopState *node)
{
	NestLoop   *nl = (NestLoop *) node->js.ps.plan;
	ExprContext *econtext = node->js.ps.ps_ExprContext;
	dlist_node *ctx_node;
	NestLoopContext *ctx;
	TupleTableSlot *slot;
	ListCell   *lc;

	/*
	 * We don't have to store the current outer tuple slot because of the same
	 * reason above.
	 */

	/* if chgParam is not NULL, free it now */
	if (node->js.ps.chgParam != NULL)
	{
		bms_free(node->js.ps.chgParam);
		node->js.ps.chgParam = NULL;
	}

	/* make the previous context current context */
	ctx_node = node->prev_ctx_node;
	Assert(ctx_node != &node->ctxs_head.head);

	if (dlist_has_prev(&node->ctxs_head, ctx_node))
		node->prev_ctx_node = dlist_prev_node(&node->ctxs_head, ctx_node);
	else
		node->prev_ctx_node = &node->ctxs_head.head;

	ctx = dlist_container(NestLoopContext, list, ctx_node);
	slot = ctx->outer_tupleslot;
	econtext->ecxt_outertuple = slot;
	/*
	 * Pass true to shouldFree here because the tuple must be freed when
	 * ExecStoreTuple(), ExecClearTuple(), or ExecResetTupleTable() is called.
	 */
	ExecForceStoreHeapTuple(ctx->outer_tuple, slot, true);
	/* restore outer variables */
	foreach(lc, nl->nestParams)
	{
		NestLoopParam *nlp = (NestLoopParam *) lfirst(lc);
		int			paramno = nlp->paramno;
		ParamExecData *prm;

		prm = &(econtext->ecxt_param_exec_vals[paramno]);
		/* Param value should be an OUTER_VAR var */
		Assert(IsA(nlp->paramval, Var));
		Assert(nlp->paramval->varno == OUTER_VAR);
		Assert(nlp->paramval->varattno > 0);
		prm->value = slot_getattr(slot,
								  nlp->paramval->varattno,
								  &(prm->isnull));
	}

	ExecPrevContext(outerPlanState(node));
	ExecPrevContext(innerPlanState(node));

	/*
	 * The next execution of the current nested loop must execute the inner
	 * plan.
	 */
	node->nl_NeedNewOuter = false;
}
