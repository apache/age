/*-------------------------------------------------------------------------
 *
 * nodeSeqscan.c
 *	  Support routines for sequential scans of relations.
 *
 * Portions Copyright (c) 1996-2020, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *
 * IDENTIFICATION
 *	  src/backend/executor/nodeSeqscan.c
 *
 *-------------------------------------------------------------------------
 */
/*
 * INTERFACE ROUTINES
 *		ExecSeqScan				sequentially scans a relation.
 *		ExecSeqNext				retrieve next tuple in sequential order.
 *		ExecInitSeqScan			creates and initializes a seqscan node.
 *		ExecEndSeqScan			releases any storage allocated.
 *		ExecReScanSeqScan		rescans the relation
 *
 *		ExecSeqScanEstimate		estimates DSM space needed for parallel scan
 *		ExecSeqScanInitializeDSM initialize DSM for parallel scan
 *		ExecSeqScanReInitializeDSM reinitialize DSM for fresh parallel scan
 *		ExecSeqScanInitializeWorker attach to DSM info in parallel worker
 */
#include "postgres.h"

#include "access/relscan.h"
#include "access/tableam.h"
#include "executor/execdebug.h"
#include "executor/nodeSeqscan.h"
#include "optimizer/clauses.h"
#include "utils/graph.h"
#include "utils/memutils.h"
#include "utils/rel.h"

static TupleTableSlot *SeqNext(SeqScanState *node);

/* for pg_graph */
typedef struct SeqScanContext
{
	dlist_node	list;
	Bitmapset  *chgParam;
	TableScanDesc scanDesc;
} SeqScanContext;

static SeqScanContext *getCurrentContext(SeqScanState *node, bool create);

/* ----------------------------------------------------------------
 *						Scan Support
 * ----------------------------------------------------------------
 */

/* ----------------------------------------------------------------
 *		SeqNext
 *
 *		This is a workhorse for ExecSeqScan
 * ----------------------------------------------------------------
 */
static TupleTableSlot *
SeqNext(SeqScanState *node)
{
	TableScanDesc scandesc;
	EState	   *estate;
	ScanDirection direction;
	TupleTableSlot *slot;

	/*
	 * get information from the estate and scan state
	 */
	scandesc = node->ss.ss_currentScanDesc;
	estate = node->ss.ps.state;
	direction = estate->es_direction;
	slot = node->ss.ss_ScanTupleSlot;

	if (scandesc == NULL)
	{
		/*
		 * We reach here if the scan is not parallel, or if we're serially
		 * executing a scan that was planned to be parallel.
		 */
		scandesc = table_beginscan(node->ss.ss_currentRelation,
								   estate->es_snapshot,
								   0, NULL);
		node->ss.ss_currentScanDesc = scandesc;
	}

	/*
	 * get the next tuple from the table
	 */
	if (table_scan_getnextslot(scandesc, direction, slot))
		return slot;
	return NULL;
}

/*
 * SeqRecheck -- access method routine to recheck a tuple in EvalPlanQual
 */
static bool
SeqRecheck(SeqScanState *node, TupleTableSlot *slot)
{
	/*
	 * Note that unlike IndexScan, SeqScan never use keys in heap_beginscan
	 * (and this is very bad) - so, here we do not check are keys ok or not.
	 */
	return true;
}

/* ----------------------------------------------------------------
 *		ExecSeqScan(node)
 *
 *		Scans the relation sequentially and returns the next qualifying
 *		tuple.
 *		We call the ExecScan() routine and pass it the appropriate
 *		access method functions.
 * ----------------------------------------------------------------
 */
static TupleTableSlot *
ExecSeqScan(PlanState *pstate)
{
	SeqScanState *node = castNode(SeqScanState, pstate);

	return ExecScan(&node->ss,
					(ExecScanAccessMtd) SeqNext,
					(ExecScanRecheckMtd) SeqRecheck);
}


/* ----------------------------------------------------------------
 *		ExecInitSeqScan
 * ----------------------------------------------------------------
 */
SeqScanState *
ExecInitSeqScan(SeqScan *node, EState *estate, int eflags)
{
	SeqScanState *scanstate;

	/*
	 * Once upon a time it was possible to have an outerPlan of a SeqScan, but
	 * not any more.
	 */
	Assert(outerPlan(node) == NULL);
	Assert(innerPlan(node) == NULL);

	/*
	 * create state structure
	 */
	scanstate = makeNode(SeqScanState);
	scanstate->ss.ps.plan = (Plan *) node;
	scanstate->ss.ps.state = estate;
	scanstate->ss.ps.ExecProcNode = ExecSeqScan;

	/*
	 * Miscellaneous initialization
	 *
	 * create expression context for node
	 */
	ExecAssignExprContext(estate, &scanstate->ss.ps);

	/*
	 * open the scan relation
	 */
	scanstate->ss.ss_currentRelation =
		ExecOpenScanRelation(estate,
							 node->scanrelid,
							 eflags);

	/* and create slot with the appropriate rowtype */
	ExecInitScanTupleSlot(estate, &scanstate->ss,
						  RelationGetDescr(scanstate->ss.ss_currentRelation),
						  table_slot_callbacks(scanstate->ss.ss_currentRelation));

	/*
	 * Initialize result type and projection.
	 */
	ExecInitResultTypeTL(&scanstate->ss.ps);
	ExecAssignScanProjectionInfo(&scanstate->ss);

	/*
	 * initialize child expressions
	 */
	scanstate->ss.ps.qual =
		ExecInitQual(node->plan.qual, (PlanState *) scanstate);

    dlist_init(&scanstate->ctxs_head);
    scanstate->prev_ctx_node = &scanstate->ctxs_head.head;
	return scanstate;
}

/* ----------------------------------------------------------------
 *		ExecEndSeqScan
 *
 *		frees any storage allocated through C routines.
 * ----------------------------------------------------------------
 */
void
ExecEndSeqScan(SeqScanState *node)
{
	TableScanDesc scanDesc;

	/*
	 * get information from node
	 */
	scanDesc = node->ss.ss_currentScanDesc;

	/*
	 * Free the exprcontext
	 */
	ExecFreeExprContext(&node->ss.ps);

	/*
	 * clean out the tuple table
	 */
	if (node->ss.ps.ps_ResultTupleSlot)
		ExecClearTuple(node->ss.ps.ps_ResultTupleSlot);
	ExecClearTuple(node->ss.ss_ScanTupleSlot);

	if (!dlist_is_empty(&node->ctxs_head))
	{
		dlist_node *ctx_node;
		SeqScanContext *ctx;
		dlist_mutable_iter iter;

		/* scanDesc is the most recent value. Ignore the first context. */
		ctx_node = dlist_pop_head_node(&node->ctxs_head);
		ctx = dlist_container(SeqScanContext, list, ctx_node);
		pfree(ctx);

		dlist_foreach_modify(iter, &node->ctxs_head)
		{
			dlist_delete(iter.cur);

			ctx = dlist_container(SeqScanContext, list, iter.cur);

			if (ctx->scanDesc != NULL)
				table_endscan(ctx->scanDesc);

			pfree(ctx);
		}
	}
	node->prev_ctx_node = &node->ctxs_head.head;

	/*
	 * close heap scan
	 */
	if (scanDesc != NULL)
		table_endscan(scanDesc);
}

/* ----------------------------------------------------------------
 *						Join Support
 * ----------------------------------------------------------------
 */

/* ----------------------------------------------------------------
 *		ExecReScanSeqScan
 *
 *		Rescans the relation.
 * ----------------------------------------------------------------
 */
void
ExecReScanSeqScan(SeqScanState *node)
{
	TableScanDesc scan;

	scan = node->ss.ss_currentScanDesc;

	if (scan != NULL)
		table_rescan(scan,		/* scan desc */
					 NULL);		/* new scan keys */

	ExecScanReScan((ScanState *) node);
}

void
ExecNextSeqScanContext(SeqScanState *node)
{
	SeqScanContext *ctx;

	/* store the current context */
	ctx = getCurrentContext(node, true);
	ctx->chgParam = node->ss.ps.chgParam;
	ctx->scanDesc = node->ss.ss_currentScanDesc;

	/* make the current context previous context */
	node->prev_ctx_node = &ctx->list;

	ctx = getCurrentContext(node, false);
	if (ctx == NULL)
	{
		/* if there is no current context, initialize the current scan */
		node->ss.ps.chgParam = NULL;
		node->ss.ss_currentScanDesc = NULL;
	}
	else
	{
		/* if there is the current context already, use it */

		Assert(ctx->chgParam == NULL);
		node->ss.ps.chgParam = NULL;

		/* ctx->scanDesc can be NULL if ss_skipLabelScan */
		node->ss.ss_currentScanDesc = ctx->scanDesc;
	}
}

void
ExecPrevSeqScanContext(SeqScanState *node)
{
	SeqScanContext *ctx;
	dlist_node *ctx_node;

	/*
	 * Store the current ss_currentScanDesc. It will be reused when the current
	 * scan is re-scanned next time.
	 */
	ctx = getCurrentContext(node, true);

	/* if chgParam is not NULL, free it now */
	if (node->ss.ps.chgParam != NULL)
	{
		bms_free(node->ss.ps.chgParam);
		node->ss.ps.chgParam = NULL;
	}

	ctx->chgParam = NULL;
	ctx->scanDesc = node->ss.ss_currentScanDesc;

	/* make the previous context current context */
	ctx_node = node->prev_ctx_node;
	Assert(ctx_node != &node->ctxs_head.head);

	if (dlist_has_prev(&node->ctxs_head, ctx_node))
		node->prev_ctx_node = dlist_prev_node(&node->ctxs_head, ctx_node);
	else
		node->prev_ctx_node = &node->ctxs_head.head;

	/* restore */
	ctx = dlist_container(SeqScanContext, list, ctx_node);
	node->ss.ps.chgParam = ctx->chgParam;
	node->ss.ss_currentScanDesc = ctx->scanDesc;
}

static SeqScanContext *
getCurrentContext(SeqScanState *node, bool create)
{
	SeqScanContext *ctx;

	if (dlist_has_next(&node->ctxs_head, node->prev_ctx_node))
	{
		dlist_node *ctx_node;

		ctx_node = dlist_next_node(&node->ctxs_head, node->prev_ctx_node);
		ctx = dlist_container(SeqScanContext, list, ctx_node);
	}
	else if (create)
	{
		ctx = palloc(sizeof(*ctx));
		ctx->chgParam = NULL;
		ctx->scanDesc = NULL;

		dlist_push_tail(&node->ctxs_head, &ctx->list);
	}
	else
	{
		ctx = NULL;
	}

	return ctx;
}

/* ----------------------------------------------------------------
 *						Parallel Scan Support
 * ----------------------------------------------------------------
 */

/* ----------------------------------------------------------------
 *		ExecSeqScanEstimate
 *
 *		Compute the amount of space we'll need in the parallel
 *		query DSM, and inform pcxt->estimator about our needs.
 * ----------------------------------------------------------------
 */
void
ExecSeqScanEstimate(SeqScanState *node,
					ParallelContext *pcxt)
{
	EState	   *estate = node->ss.ps.state;

	node->pscan_len = table_parallelscan_estimate(node->ss.ss_currentRelation,
												  estate->es_snapshot);
	shm_toc_estimate_chunk(&pcxt->estimator, node->pscan_len);
	shm_toc_estimate_keys(&pcxt->estimator, 1);
}

/* ----------------------------------------------------------------
 *		ExecSeqScanInitializeDSM
 *
 *		Set up a parallel heap scan descriptor.
 * ----------------------------------------------------------------
 */
void
ExecSeqScanInitializeDSM(SeqScanState *node,
						 ParallelContext *pcxt)
{
	EState	   *estate = node->ss.ps.state;
	ParallelTableScanDesc pscan;

	pscan = shm_toc_allocate(pcxt->toc, node->pscan_len);
	table_parallelscan_initialize(node->ss.ss_currentRelation,
								  pscan,
								  estate->es_snapshot);
	shm_toc_insert(pcxt->toc, node->ss.ps.plan->plan_node_id, pscan);
	node->ss.ss_currentScanDesc =
		table_beginscan_parallel(node->ss.ss_currentRelation, pscan);
}

/* ----------------------------------------------------------------
 *		ExecSeqScanReInitializeDSM
 *
 *		Reset shared state before beginning a fresh scan.
 * ----------------------------------------------------------------
 */
void
ExecSeqScanReInitializeDSM(SeqScanState *node,
						   ParallelContext *pcxt)
{
	ParallelTableScanDesc pscan;

	pscan = node->ss.ss_currentScanDesc->rs_parallel;
	table_parallelscan_reinitialize(node->ss.ss_currentRelation, pscan);
}

/* ----------------------------------------------------------------
 *		ExecSeqScanInitializeWorker
 *
 *		Copy relevant information from TOC into planstate.
 * ----------------------------------------------------------------
 */
void
ExecSeqScanInitializeWorker(SeqScanState *node,
							ParallelWorkerContext *pwcxt)
{
	ParallelTableScanDesc pscan;

	pscan = shm_toc_lookup(pwcxt->toc, node->ss.ps.plan->plan_node_id, false);
	node->ss.ss_currentScanDesc =
		table_beginscan_parallel(node->ss.ss_currentRelation, pscan);
}
