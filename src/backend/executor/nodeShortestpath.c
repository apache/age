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

#include "access/hash.h"
#include "access/htup_details.h"
#include "catalog/ag_vertex_d.h"
#include "catalog/pg_type.h"
#include "executor/executor.h"
#include "executor/hashjoin.h"
#include "executor/nodeHash2Side.h"
#include "executor/nodeShortestpath.h"
#include "miscadmin.h"
#include "utils/lsyscache.h"
#include "utils/memutils.h"
#include "access/tupdesc.h"

#include "utils/typcache.h"
#include "funcapi.h"

/*
 * States of the ExecShortestpath state machine
 */
#define SP_GET_PARAMETER        1
#define SP_ROTATE_PLANSTATE     2
#define SP_BUILD_HASHTABLE      3
#define SP_NEED_NEW_OUTER       4
#define SP_SCAN_BUCKET			5
#define SP_NEED_NEW_BATCH       6

static TupleTableSlot *ExecShortestpathOuterGetTuple(ShortestpathState *spstate,
													 uint32            *hashvalue);
static TupleTableSlot *ExecShortestpathProcOuterNode(PlanState         *node,
													 ShortestpathState *spstate);
static bool ExecShortestpathRescanOuterNode(Hash2SideState    *node,
											ShortestpathState *spstate);
static bool ExecShortestpathNewBatch(ShortestpathState *spstate);
static Datum ExecShortestpathProjectEvalArray(Oid            element_typeid,
											  const unsigned char *elems,
											  long           len,
											  ExprContext   *econtext);
static TupleTableSlot *ExecShortestpathProject(ShortestpathState *node,
											   unsigned char     *outerids,
											   long               lenOuterids,
											   unsigned char     *innerids,
											   long               lenInnerids,
											   int                sizeGraphid,
											   int                sizeRowid);
static HeapTuple replace_vertexRow_graphid(TupleDesc tupleDesc,
										   HeapTuple vertexRow,
										   Datum graphid);

/* ----------------------------------------------------------------
 *		ExecShortestpath
 *
 *		This function implements the Hybrid Hash2Sidejoin algorithm.
 *
 *		Note: the relation we build hash table on is the "inner"
 *			  the other one is "outer".
 * ----------------------------------------------------------------
 */
static TupleTableSlot *				/* return: a tuple or NULL */
ExecShortestpath(PlanState *pstate)
{
	ShortestpathState *node = castNode(ShortestpathState, pstate);
	Hash2SideState *outerNode;
	Hash2SideState *innerNode;
	ExprState      *joinqual;
	ExprState      *otherqual;
	ExprContext    *econtext;
	HashJoinTable   keytable;
	HashJoinTable   outertable;
	HashJoinTable   hashtable;
	TupleTableSlot *outerTupleSlot;
	MinimalTuple    outerTuple;
	MinimalTuple    innerTuple;
	uint32          hashvalue;
	int             batchno;
	bool            isNull;

	/*
	 * get information from Shortestpath node
	 */
	joinqual = node->js.joinqual;
	otherqual = node->js.ps.qual;
	outerNode = node->outerNode;
	innerNode = node->innerNode;
	keytable = node->sp_KeyTable;
	outertable = node->sp_OuterTable;
	hashtable = node->sp_HashTable;
	econtext = node->js.ps.ps_ExprContext;

	/*
	 * Reset per-tuple memory context to free any expression evaluation
	 * storage allocated in the previous tuple cycle.
	 */
	ResetExprContext(econtext);

	/*
	 * run the hash join state machine
	 */
	for (;;)
	{
		/*
		 * It's possible to iterate this loop many times before returning a
		 * tuple, in some pathological cases such as needing to move much of
		 * the current batch to a later batch.  So let's check for interrupts
		 * each time through.
		 */
		CHECK_FOR_INTERRUPTS();

		switch (node->sp_JoinState)
		{
			case SP_GET_PARAMETER:

				outerNode->hops = 0;
				innerNode->hops = 0;

				if (node->startVid == 0)
					node->startVid = DatumGetGraphid(ExecEvalExpr(node->source, econtext, &isNull));
				if (node->endVid == 0)
					node->endVid = DatumGetGraphid(ExecEvalExpr(node->target, econtext, &isNull));

				node->sp_JoinState = SP_ROTATE_PLANSTATE;

				hashtable = ExecHash2SideTableCreate(outerNode,
													 node->sp_HashOperators,
													 1,
													 1,
													 outerNode->hops,
													 outerNode->spacePeak);
				hashvalue = hash_any((unsigned char *) &(node->startVid), sizeof(Graphid));
				ExecHash2SideTableInsertGraphid(hashtable,
												node->startVid,
												hashvalue,
												outerNode,
												node,
												NULL);
				hashtable->growEnabled = false;
				hashtable->totalTuples += 1;
				outerNode->totalPaths += 1;
				outerNode->hashtable = hashtable;
				hashtable = ExecHash2SideTableCreate(innerNode,
													 node->sp_HashOperators,
													 1,
													 1,
													 innerNode->hops,
													 innerNode->spacePeak);
				hashvalue = hash_any((unsigned char *) &(node->endVid), sizeof(Graphid));
				ExecHash2SideTableInsertGraphid(hashtable,
												node->endVid,
												hashvalue,
												innerNode,
												node,
												NULL);
				hashtable->growEnabled = false;
				hashtable->totalTuples += 1;
				innerNode->totalPaths += 1;
				innerNode->hashtable = hashtable;
				hashtable = NULL;

				if (node->minhops == 0 && node->startVid == node->endVid)
				{
					TupleTableSlot *result;
					result = ExecShortestpathProject(node,
													 (unsigned char *) (&(node->startVid)),
													 0,
													 (unsigned char *) (&(node->endVid)),
													 0,
													 sizeof(Graphid),
													 node->sp_RowidSize);
					node->numResults++;
					return result;
				}

				/* FALL THRU */

			case SP_ROTATE_PLANSTATE:

				if (node->numResults > 0 || node->hops >= node->maxhops ||
					outerNode->totalPaths == 0 || innerNode->totalPaths == 0)
				{
					return NULL;
				}

				node->hops++;
				if (node->hops >= node->sp_Hops)
				{
					node->sp_Hops *= 2;
					node->sp_OuterTuple = (MinimalTuple) repalloc(node->sp_OuterTuple,
																  HJTUPLE_OVERHEAD +
																  MAXALIGN(SizeofMinimalTupleHeader) +
																  MAXALIGN(outerNode->ps.plan->plan_width *
																		   node->sp_Hops +
																		   sizeof(Graphid)));
					node->sp_Vertexids = (unsigned char *) repalloc(node->sp_Vertexids,
																	sizeof(Graphid) * (node->sp_Hops + 1));
					node->sp_Edgeids = (unsigned char *) repalloc(node->sp_Edgeids,
																  node->sp_RowidSize * node->sp_Hops);
				}

				Assert(node->outerNode->hashtable != NULL &&
					   node->innerNode->hashtable != NULL);

				if ((outerNode->totalPaths > innerNode->totalPaths) ||
					(outerNode->totalPaths == innerNode->totalPaths &&
					 outerNode->ps.plan->plan_rows > innerNode->ps.plan->plan_rows))
				{
					node->outerNode = innerNode;
					node->innerNode = outerNode;
					outerNode = node->outerNode;
					innerNode = node->innerNode;
					if (innerNode->keytable)
					{
						if (innerNode->keytable->spacePeak > innerNode->spacePeak)
							innerNode->spacePeak = innerNode->keytable->spacePeak;
						ExecHash2SideTableDestroy(innerNode->keytable);
						innerNode->keytable = NULL;
					}
					if (innerNode->hashtable->nbatch > 1)
					{
						int nbuckets;
						int nbatch;

						ExecChooseHash2SideTableSize(innerNode->hashtable->totalTuples,
													 innerNode->totalPaths,
													 innerNode->ps.plan->plan_width,
													 innerNode->hops,
													 &nbuckets,
													 &nbatch);
						if (nbatch != innerNode->hashtable->nbatch ||
							nbuckets != innerNode->hashtable->nbuckets)
						{
							int i;
							if (innerNode->hashtable->spacePeak > innerNode->spacePeak)
								innerNode->spacePeak = innerNode->hashtable->spacePeak;
							hashtable = ExecHash2SideTableCreate(innerNode,
																 node->sp_HashOperators,
																 innerNode->hashtable->totalTuples,
																 innerNode->totalPaths,
																 innerNode->hops,
																 innerNode->spacePeak);
							for (i = 0; i < innerNode->hashtable->nbatch; i++)
							{
								if (i != innerNode->hashtable->curbatch &&
									innerNode->hashtable->innerBatchFile[i])
								{
									TupleTableSlot *slot;
									if (BufFileSeek(innerNode->hashtable->innerBatchFile[i], 0, 0L, SEEK_SET))
										ereport(ERROR,
												(errcode_for_file_access(),
														errmsg("could not rewind hash-join temporary file: %m")));
									while ((slot = ExecShortestpathGetSavedTuple(
											innerNode->hashtable->innerBatchFile[i],
											&hashvalue,
											node->sp_HashTupleSlot)))
									{
										/*
										 * NOTE: some tuples may be sent to future batches.  Also, it is
										 * possible for hashtable->nbatch to be increased here!
										 */
										if (ExecHash2SideTableInsert(hashtable,
																	 slot,
																	 hashvalue,
																	 innerNode,
																	 node,
																	 NULL))
										{
											bool shouldFree;
											MinimalTuple tuple = ExecFetchSlotMinimalTuple(slot, &shouldFree);
											hashtable->totalTuples += 1;
											if (tuple->t_len !=
												sizeof(*tuple) + sizeof(Graphid) + node->sp_RowidSize)
												innerNode->totalPaths += 1;

											if (shouldFree)
											{
												heap_free_minimal_tuple(tuple);
											}
										}
									}
								}
								else
								{
									HashMemoryChunk oldchunks = innerNode->hashtable->chunks;
									innerNode->hashtable->chunks = NULL;

									/* so, let's scan through the old chunks, and all tuples in each chunk */
									while (oldchunks != NULL)
									{
										HashMemoryChunk nextchunk = oldchunks->next.unshared;

										/* position within the buffer (up to oldchunks->used) */
										size_t		idx = 0;

										/* process all tuples stored in this chunk (and then free it) */
										while (idx < oldchunks->used)
										{
											HashJoinTuple hashTuple = (HashJoinTuple) (HASH_CHUNK_DATA(oldchunks) + idx);
											MinimalTuple  tuple = HJTUPLE_MINTUPLE(hashTuple);
											int           hashTupleSize = (HJTUPLE_OVERHEAD + tuple->t_len);

											if (*((Graphid*)(tuple+1)) != 0)
											{
												if (ExecHash2SideTableInsertTuple(hashtable,
																				  tuple,
																				  hashTuple->hashvalue,
																				  innerNode,
																				  node,
																				  NULL))
												{
													hashtable->totalTuples += 1;
													if (tuple->t_len !=
														sizeof(*tuple) + sizeof(Graphid) + node->sp_RowidSize)
														innerNode->totalPaths += 1;
												}
											}
											innerNode->hashtable->spaceUsed -= hashTupleSize;

											/* next tuple in this chunk */
											idx += MAXALIGN(hashTupleSize);
										}

										/* we're done with this chunk - free it and proceed to the next one */
										pfree(oldchunks);
										oldchunks = nextchunk;
									}
								}
							}
							/* Account for the buckets in spaceUsed (reported in EXPLAIN ANALYZE) */
							hashtable->spaceUsed += hashtable->nbuckets * sizeof(HashJoinTuple);
							if (hashtable->spaceUsed > hashtable->spacePeak)
								hashtable->spacePeak = hashtable->spaceUsed;
							if (innerNode->hashtable->spacePeak > innerNode->spacePeak)
								innerNode->spacePeak = innerNode->hashtable->spacePeak;
							ExecHash2SideTableDestroy(innerNode->hashtable);
							hashtable->growEnabled = false;
							innerNode->hashtable = hashtable;
							hashtable = NULL;
						}
					}
				}

				node->sp_JoinState = SP_BUILD_HASHTABLE;

				/* FALL THRU */

			case SP_BUILD_HASHTABLE:

				Assert(keytable == NULL && outertable == NULL && hashtable == NULL);

				hashtable = innerNode->hashtable;
				node->sp_HashTable = hashtable;

				(void) MultiExecProcNode((PlanState *) innerNode);

				/*
				 * need to remember whether nbatch has increased since we
				 * began scanning the outer relation
				 */
				hashtable->nbatch_outstart = hashtable->nbatch;

				outerNode->hops++;
				if (outerNode->keytable)
				{
					if (outerNode->keytable->spacePeak > outerNode->spacePeak)
						outerNode->spacePeak = outerNode->keytable->spacePeak;
					ExecHash2SideTableDestroy(outerNode->keytable);
					outerNode->keytable = NULL;
					node->sp_KeyTable = NULL;
				}
				outerNode->keytable = outerNode->hashtable;
				if (outerNode->keytable->spacePeak > outerNode->spacePeak)
					outerNode->spacePeak = outerNode->keytable->spacePeak;
				if (hashtable->nbatch > 1){
					outerNode->hashtable = ExecHash2SideTableClone(outerNode,
																   node->sp_HashOperators,
																   hashtable,
																   outerNode->spacePeak);
				}
				else
				{
					outerNode->hashtable = ExecHash2SideTableCreate(outerNode,
																	node->sp_HashOperators,
																	outerNode->keytable->totalTuples +
																	outerNode->totalPaths *
																	outerNode->ps.plan->plan_rows,
																	outerNode->totalPaths *
																	outerNode->ps.plan->plan_rows,
																	outerNode->hops,
																	outerNode->spacePeak);
				}
				node->sp_KeyTable = outerNode->keytable;
				node->sp_OuterTable = outerNode->hashtable;
				keytable = node->sp_KeyTable;
				outertable = node->sp_OuterTable;
				node->sp_CurKeyBatch = 0;
				node->sp_CurKeyIdx = 0;

				if (keytable->innerBatchFile != NULL)
				{
					if (keytable->innerBatchFile[0] != NULL)
					{
						if (BufFileSeek(keytable->innerBatchFile[0], 0, 0L, SEEK_SET))
							ereport(ERROR,
									(errcode_for_file_access(),
											errmsg("could not rewind hash-join temporary file: %m")));
					}
				}
				ExecShortestpathRescanOuterNode(outerNode, node);
				outerTupleSlot = ExecShortestpathProcOuterNode((PlanState *) outerNode, node);
				while (!TupIsNull(outerTupleSlot))
				{
					/*
					 * We have to compute the tuple's hash value.
					 */
					bool shouldFree;
					MemoryContext oldContext;
					ExprContext *econtext = node->js.ps.ps_ExprContext;
					econtext->ecxt_outertuple = outerTupleSlot;
					oldContext = MemoryContextSwitchTo(outerTupleSlot->tts_mcxt);
					outerTuple = ExecFetchSlotMinimalTuple(outerTupleSlot, &shouldFree);
					MemoryContextSwitchTo(oldContext);

					memcpy(node->sp_OuterTuple + 1,
						   outerTuple + 1,
						   sizeof(Graphid) + node->sp_RowidSize);

					hashvalue = hash_any((unsigned char *) (outerTuple + 1), sizeof(Graphid));

					if (shouldFree)
					{
						heap_free_minimal_tuple(outerTuple);
					}

					if (ExecHash2SideTableInsertTuple(outertable,
													  node->sp_OuterTuple,
													  hashvalue,
													  outerNode,
													  node,
													  NULL))
					{
						outertable->totalTuples += 1;
						outerNode->totalPaths += 1;
					}
					/*
					 * That tuple couldn't match because of a NULL, so discard it and
					 * continue with the next one.
					 */
					outerTupleSlot = ExecShortestpathProcOuterNode((PlanState *) outerNode, node);
				}

				/* resize the hash table if needed (NTUP_PER_BUCKET exceeded) */
				if (outertable->nbuckets != outertable->nbuckets_optimal)
				{
					outertable->spaceUsed -= outertable->nbuckets * sizeof(HashJoinTuple);
					ExecHash2SideIncreaseNumBuckets(outertable, outerNode);
					/* Account for the buckets in spaceUsed (reported in EXPLAIN ANALYZE) */
					outertable->spaceUsed += outertable->nbuckets * sizeof(HashJoinTuple);
					if (outertable->spaceUsed > outertable->spacePeak)
						outertable->spacePeak = outertable->spaceUsed;
				}
				outertable->growEnabled = false;

				node->sp_CurOuterChunks = outertable->chunks;
				node->sp_CurOuterIdx = 0;

				node->sp_JoinState = SP_NEED_NEW_OUTER;

				/* FALL THRU */

			case SP_NEED_NEW_OUTER:

				/*
				 * We don't have an outer tuple, try to get the next one
				 */
				outerTupleSlot = ExecShortestpathOuterGetTuple(node, &hashvalue);
				if (TupIsNull(outerTupleSlot))
				{
					node->sp_JoinState = SP_NEED_NEW_BATCH;
					continue;
				}

				/*
				 * Find the corresponding bucket for this tuple in the main
				 * hash table or skew hash table.
				 */
				node->sp_CurHashValue = hashvalue;
				ExecHash2SideGetBucketAndBatch(hashtable, hashvalue,
											   &node->sp_CurBucketNo, &batchno);
				node->sp_CurTuple = NULL;

				/* OK, let's scan the bucket for matches */
				node->sp_JoinState = SP_SCAN_BUCKET;

				/* FALL THRU */

			case SP_SCAN_BUCKET:

				/*
				 * Scan the selected hash bucket for matches to current outer
				 */
				if (!ExecScanHash2SideBucket(innerNode, node, econtext))
				{
					/* out of matches; check for possible outer-join fill */
					node->sp_JoinState = SP_NEED_NEW_OUTER;
					continue;
				}

				/*
				 * We've got a match, but still need to test non-hashed quals.
				 * ExecScanHash2SideBucket already set up all the state needed to
				 * call ExecQual.
				 *
				 * If we pass the qual, then save state for next call and have
				 * ExecProject form the projection, store it in the tuple
				 * table, and return the slot.
				 *
				 * Only the joinquals determine tuple match status, but all
				 * quals must pass to actually return the tuple.
				 */
				if (joinqual == NULL || ExecQual(joinqual, econtext))
				{
					HeapTupleHeaderSetMatch(HJTUPLE_MINTUPLE(node->sp_CurTuple));

					if (otherqual == NULL || ExecQual(otherqual, econtext))
					{
						bool outerShouldFree, innerShouldFree;
						TupleTableSlot *result;
						int             outerHops;
						int             innerHops;
						if (outerPlanState(node) == (PlanState *) outerNode)
						{
							outerTuple = ExecFetchSlotMinimalTuple(econtext->ecxt_outertuple, &outerShouldFree);
							innerTuple = ExecFetchSlotMinimalTuple(econtext->ecxt_innertuple, &innerShouldFree);
							outerHops = outerNode->hops;
							innerHops = innerNode->hops;
						}
						else
						{
							outerTuple = ExecFetchSlotMinimalTuple(econtext->ecxt_innertuple, &outerShouldFree);
							innerTuple = ExecFetchSlotMinimalTuple(econtext->ecxt_outertuple, &innerShouldFree);
							outerHops = innerNode->hops;
							innerHops = outerNode->hops;
						}
						result = ExecShortestpathProject(node,
														 (unsigned char *) (outerTuple + 1),
														 outerHops,
														 (unsigned char *) (innerTuple + 1),
														 innerHops,
														 sizeof(Graphid),
														 node->sp_RowidSize);

						if (outerShouldFree)
						{
							heap_free_minimal_tuple(outerTuple);
						}

						if (innerShouldFree)
						{
							heap_free_minimal_tuple(innerTuple);
						}

						node->numResults++;
						if (node->numResults >= node->limit)
						{
							node->sp_KeyTable = NULL;
							node->sp_OuterTable = NULL;
							node->sp_HashTable = NULL;
							keytable = node->sp_KeyTable;
							outertable = node->sp_OuterTable;
							hashtable = node->sp_HashTable;
							node->sp_JoinState = SP_ROTATE_PLANSTATE;
						}
						return result;
					}
					else
						InstrCountFiltered2(node, 1);
				}
				else
					InstrCountFiltered1(node, 1);
				break;

			case SP_NEED_NEW_BATCH:

				/*
				 * Try to advance to next batch.  Done if there are no more.
				 */
				if (!ExecShortestpathNewBatch(node))
				{
					node->sp_KeyTable = NULL;
					node->sp_OuterTable = NULL;
					node->sp_HashTable = NULL;
					keytable = node->sp_KeyTable;
					outertable = node->sp_OuterTable;
					hashtable = node->sp_HashTable;
					node->sp_JoinState = SP_ROTATE_PLANSTATE;
					continue;
				}
				node->sp_JoinState = SP_NEED_NEW_OUTER;
				break;

			default:
				elog(ERROR, "unrecognized shortestpath state: %d",
					 (int) node->sp_JoinState);
		}
	}
}

/* ----------------------------------------------------------------
 *		ExecInitShortestpath
 *
 *		Init routine for Shortestpath node.
 * ----------------------------------------------------------------
 */
ShortestpathState *
ExecInitShortestpath(Shortestpath *node, EState *estate, int eflags)
{
	ShortestpathState *spstate;
	Plan	   *outerNode;
	Plan       *innerNode;
	List	   *lclauses;
	List	   *rclauses;
	List	   *hoperators;
	ListCell   *l;
	Hash2SideState *outerH2SNode;
	Hash2SideState *innerH2SNode;
	HeapTuple		vertexRow = NULL;
	TupleDesc		tupleDesc = NULL;

	/* check for unsupported flags */
	Assert(!(eflags & (EXEC_FLAG_BACKWARD | EXEC_FLAG_MARK)));

	/*
	 * create state structure
	 */
	spstate = makeNode(ShortestpathState);
	spstate->js.ps.plan = (Plan *) node;
	spstate->js.ps.state = estate;
	spstate->js.ps.ExecProcNode = ExecShortestpath;

	/*
	 * Miscellaneous initialization
	 *
	 * create expression context for node
	 */
	ExecAssignExprContext(estate, &spstate->js.ps);

	/*
	 * initialize child expressions
	 */
	spstate->js.ps.qual =
			ExecInitQual(node->join.plan.qual, (PlanState *) spstate);
	spstate->js.jointype = node->join.jointype;
	spstate->js.joinqual =
			ExecInitQual(node->join.joinqual, (PlanState *) spstate);
	spstate->hashclauses =
			ExecInitQual(node->hashclauses, (PlanState *) spstate);

	spstate->source     = ExecInitExpr((Expr *) node->source, (PlanState *) spstate);
	spstate->target     = ExecInitExpr((Expr *) node->target, (PlanState *) spstate);
	spstate->minhops    = node->minhops;
	spstate->maxhops    = node->maxhops;
	spstate->limit      = node->limit;
	spstate->startVid   = 0;
	spstate->endVid     = 0;
	spstate->hops       = 0;
	spstate->numResults = 0;

	/*
	 * initialize child nodes
	 *
	 * Note: we could suppress the REWIND flag for the inner input, which
	 * would amount to betting that the hash will be a single batch.  Not
	 * clear if this would be a win or not.
	 */
	outerNode = outerPlan(node);
	innerNode = innerPlan(node);

	outerPlanState(spstate) = ExecInitNode(outerNode, estate, eflags);
	innerPlanState(spstate) = ExecInitNode(innerNode, estate, eflags);

	((Hash2SideState *) outerPlanState(spstate))->key = spstate->source;
	((Hash2SideState *) innerPlanState(spstate))->key = spstate->target;

	((Hash2SideState *) outerPlanState(spstate))->spstate = (PlanState *) spstate;
	((Hash2SideState *) innerPlanState(spstate))->spstate = (PlanState *) spstate;

	/*
	 * tuple table initialization
	 */
	ExecInitResultTupleSlotTL(&spstate->js.ps, &TTSOpsMinimalTuple);
	spstate->sp_OuterTupleSlot = ExecInitExtraTupleSlot(estate, NULL, &TTSOpsMinimalTuple);
    ExecSetSlotDescriptor(spstate->sp_OuterTupleSlot, ExecGetResultType(outerPlanState(spstate)));

	/*
	 * now for some voodoo.  our temporary tuple slot is actually the result
	 * tuple slot of the Hash2Side node (which is our inner plan).  we can do this
	 * because Hash2Side nodes don't return tuples via ExecProcNode() -- instead
	 * the hash join node uses ExecScanHash2SideBucket() to get at the contents of
	 * the hash table.  -cim 6/9/91
	 */
	{
		Hash2SideState *hashstate = (Hash2SideState *) innerPlanState(spstate);
		TupleTableSlot *slot = hashstate->ps.ps_ResultTupleSlot;

		spstate->sp_HashTupleSlot = slot;
		((Hash2SideState *) outerPlanState(spstate))->slot = slot;
		((Hash2SideState *) innerPlanState(spstate))->slot = slot;
	}

	/*
	 * initialize tuple type and projection info
	 */
	ExecAssignProjectionInfo(&spstate->js.ps, NULL);

	ExecSetSlotDescriptor(spstate->sp_OuterTupleSlot,
						  ExecGetResultType(outerPlanState(spstate)));

	/*
	 * initialize hash-specific info
	 */
	spstate->sp_KeyTable = NULL;
	spstate->sp_OuterTable = NULL;
	spstate->sp_HashTable = NULL;

	spstate->sp_CurHashValue = 0;
	spstate->sp_CurBucketNo = 0;
	spstate->sp_CurSkewBucketNo = INVALID_SKEW_BUCKET_NO;
	spstate->sp_CurTuple = NULL;

	/*
	 * Deconstruct the hash clauses into outer and inner argument values, so
	 * that we can evaluate those subexpressions separately.  Also make a list
	 * of the hash operator OIDs, in preparation for looking up the hash
	 * functions to use.
	 */
	lclauses = NIL;
	rclauses = NIL;
	hoperators = NIL;
	foreach(l, node->hashclauses)
	{
		OpExpr	   *hclause = lfirst_node(OpExpr, l);

		lclauses = lappend(lclauses, ExecInitExpr(linitial(hclause->args),
												  (PlanState *) spstate));
		rclauses = lappend(rclauses, ExecInitExpr(lsecond(hclause->args),
												  (PlanState *) spstate));
		hoperators = lappend_oid(hoperators, hclause->opno);
	}
	spstate->sp_OuterHashKeys = lclauses;
	spstate->sp_InnerHashKeys = rclauses;
	spstate->sp_HashOperators = hoperators;
	/* child Hash2Side node needs to evaluate inner hash keys, too */
	((Hash2SideState *) innerPlanState(spstate))->hashkeys = rclauses;

	spstate->sp_Hops = (node->maxhops < 32) ? node->maxhops : 32;
	spstate->sp_RowidSize = outerNode->plan_width - sizeof(Graphid);
	spstate->sp_GraphidTuple = (MinimalTuple) palloc(HJTUPLE_OVERHEAD +
													 MAXALIGN(SizeofMinimalTupleHeader) +
													 MAXALIGN(outerNode->plan_width));
	spstate->sp_OuterTuple = (MinimalTuple) palloc(HJTUPLE_OVERHEAD +
												   MAXALIGN(SizeofMinimalTupleHeader) +
												   MAXALIGN(outerNode->plan_width * spstate->sp_Hops +
															sizeof(Graphid)));
	spstate->sp_Vertexids = (unsigned char *) palloc(sizeof(Graphid) * (spstate->sp_Hops + 1));
	spstate->sp_Edgeids = (unsigned char *) palloc(spstate->sp_RowidSize * spstate->sp_Hops);

	memset(spstate->sp_GraphidTuple, 0, sizeof(*(spstate->sp_GraphidTuple)) + outerNode->plan_width);
	spstate->sp_GraphidTuple->t_len = sizeof(*(spstate->sp_GraphidTuple)) + outerNode->plan_width;
	spstate->sp_GraphidTuple->t_infomask2 = 2;
	spstate->sp_GraphidTuple->t_hoff = MAXALIGN(sizeof(*(spstate->sp_GraphidTuple))) + MINIMAL_TUPLE_OFFSET;

	spstate->sp_JoinState = SP_GET_PARAMETER;

	spstate->outerNode = (Hash2SideState *)outerPlanState(spstate);
	spstate->innerNode = (Hash2SideState *)innerPlanState(spstate);

	outerH2SNode = spstate->outerNode;
	innerH2SNode = spstate->innerNode;

	/*
	 * Shortestpath was originally written without expectations that the
	 * start and end nodes might reside in FieldSelect expressions. This
	 * code is to address that deficit. Additionally, it is added here to
	 * help with memory conservation by providing two vertex rows that
	 * can reused - one for the start, the other for the end.
	 */
	if (IsA(outerH2SNode->key->expr, FieldSelect) ||
		IsA(innerH2SNode->key->expr, FieldSelect))
	{
		Datum		values[Natts_ag_vertex] = {0, 0, 0};
		bool		isnull[Natts_ag_vertex] = {false, true, true};

		tupleDesc = lookup_rowtype_tupdesc(VERTEXOID, -1);
		Assert(tupleDesc->natts == Natts_ag_vertex);

		vertexRow = heap_form_tuple(tupleDesc, values, isnull);
	}

	if (IsA(outerH2SNode->key->expr, FieldSelect))
	{
		FieldSelect		*fs = (FieldSelect *) outerH2SNode->key->expr;
		outerH2SNode->isFieldSelect = true;
		outerH2SNode->correctedParam = (Param *) (fs->arg);
	}
	else
	{
		outerH2SNode->isFieldSelect = false;
		outerH2SNode->correctedParam = (Param *) outerH2SNode->key->expr;
	}
	outerH2SNode->vertexRow = vertexRow;
	outerH2SNode->tupleDesc =  tupleDesc;

	if (IsA(innerH2SNode->key->expr, FieldSelect))
	{
		FieldSelect		*fs = (FieldSelect *) innerH2SNode->key->expr;
		innerH2SNode->isFieldSelect = true;
		innerH2SNode->correctedParam = (Param *) (fs->arg);
	}
	else
	{
		innerH2SNode->isFieldSelect = false;
		innerH2SNode->correctedParam = (Param *) innerH2SNode->key->expr;
	}
	innerH2SNode->vertexRow = vertexRow;
	innerH2SNode->tupleDesc = tupleDesc;

	return spstate;
}

/* ----------------------------------------------------------------
 *		ExecEndShortestpath
 *
 *		clean up routine for Shortestpath node
 * ----------------------------------------------------------------
 */
void
ExecEndShortestpath(ShortestpathState *node)
{
	/* Release the tuple descriptor in the Hash2SideState nodes */
	if (node->outerNode->tupleDesc)
		ReleaseTupleDesc(node->outerNode->tupleDesc);
	else if (node->innerNode->tupleDesc)
		ReleaseTupleDesc(node->innerNode->tupleDesc);

	/* Release the vertexRow container in the Hash2SideState nodes */
	if (node->outerNode->vertexRow)
		heap_freetuple(node->outerNode->vertexRow);
	else if (node->innerNode->vertexRow)
		heap_freetuple(node->innerNode->vertexRow);

	/*
	 * Free the exprcontext
	 */
	ExecFreeExprContext(&node->js.ps);

	/*
	 * clean out the tuple table
	 */
	ExecClearTuple(node->js.ps.ps_ResultTupleSlot);
	ExecClearTuple(node->sp_OuterTupleSlot);
	ExecClearTuple(node->sp_HashTupleSlot);

	pfree(node->sp_GraphidTuple);
	pfree(node->sp_OuterTuple);
	pfree(node->sp_Vertexids);
	pfree(node->sp_Edgeids);

	/*
	 * clean up subtrees
	 */
	ExecEndNode(outerPlanState(node));
	ExecEndNode(innerPlanState(node));
}

/*
 * ExecShortestpathOuterGetTuple
 *
 *		get the next outer tuple for hashjoin: either by
 *		executing the outer plan node in the first pass, or from
 *		the temp files for the hashjoin batches.
 *
 * Returns a null slot if no more outer tuples (within the current batch).
 *
 * On success, the tuple's hash value is stored at *hashvalue --- this is
 * either originally computed, or re-read from the temp file.
 */
static TupleTableSlot *
ExecShortestpathOuterGetTuple(ShortestpathState *spstate, uint32 *hashvalue)
{
	HashMemoryChunk chunks = spstate->sp_CurOuterChunks;
	TupleTableSlot *slot;
	HashJoinTuple   hashTuple;
	MinimalTuple    tuple;

	while (chunks != NULL)
	{
		if (chunks->used <= spstate->sp_CurOuterIdx)
		{
			chunks = chunks->next.unshared;
			spstate->sp_CurOuterChunks = chunks;
			spstate->sp_CurOuterIdx = 0;
			continue;
		}

		hashTuple = (HashJoinTuple) (HASH_CHUNK_DATA(chunks) + spstate->sp_CurOuterIdx);
		tuple = HJTUPLE_MINTUPLE(hashTuple);
		spstate->sp_CurOuterIdx += MAXALIGN(HJTUPLE_OVERHEAD + tuple->t_len);

		if (*((Graphid*)(tuple+1)) == 0 ||
			tuple->t_len == sizeof(*tuple) + sizeof(Graphid) + spstate->sp_RowidSize)
			continue;

		slot = ExecStoreMinimalTuple(tuple, spstate->sp_OuterTupleSlot, false);

		if (!TupIsNull(slot))
		{
			/*
			 * We have to compute the tuple's hash value.
			 */
			ExprContext *econtext = spstate->js.ps.ps_ExprContext;

			*hashvalue = hashTuple->hashvalue;
			econtext->ecxt_outertuple = slot;
			return slot;
		}
	}

	/* End of this batch */
	return NULL;
}

static TupleTableSlot *
ExecShortestpathProcOuterNode(PlanState *node, ShortestpathState *spstate)
{
	TupleTableSlot *slot;

	slot = ExecProcNode(node);
	while (TupIsNull(slot))
	{
		if(!ExecShortestpathRescanOuterNode((Hash2SideState *) node, spstate))
		{
			return NULL;
		}
		slot = ExecProcNode(node);
	}

	return slot;
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

static bool
ExecShortestpathRescanOuterNode(Hash2SideState *node, ShortestpathState *spstate)
{
	HashJoinTable   keytable = spstate->sp_KeyTable;
	TupleTableSlot *slot;

	while (spstate->sp_CurKeyBatch < keytable->nbatch)
	{
		if (spstate->sp_CurKeyBatch == keytable->curbatch)
		{
			while (keytable->chunks != NULL)
			{
				HashMemoryChunk oldchunks = keytable->chunks;
				HashMemoryChunk nextchunk = oldchunks->next.unshared;

				if(spstate->sp_CurKeyIdx < oldchunks->used)
				{
					break;
				}
				pfree(oldchunks);
				keytable->chunks = nextchunk;
				spstate->sp_CurKeyIdx = 0;
			}
			if (keytable->chunks != NULL)
			{
				HashMemoryChunk  oldchunks = keytable->chunks;
				HashJoinTuple    hashTuple = (HashJoinTuple) (HASH_CHUNK_DATA(oldchunks) + spstate->sp_CurKeyIdx);
				MinimalTuple     tuple = HJTUPLE_MINTUPLE(hashTuple);
				int              hashTupleSize = (HJTUPLE_OVERHEAD + tuple->t_len);
				int              paramno;
				ParamExecData   *prm;
				Graphid			*graphid;

				keytable->spaceUsed -= hashTupleSize;

				/* next tuple in this chunk */
				spstate->sp_CurKeyIdx += MAXALIGN(hashTupleSize);

				graphid = (Graphid *) (tuple+1);
				if (*graphid == 0) continue;
				if ((node->hops > 1 &&
					 tuple->t_len == sizeof(*tuple) + sizeof(Graphid) + spstate->sp_RowidSize))
				{
					ExecHash2SideTableInsertGraphid(spstate->sp_OuterTable,
													*graphid,
													hashTuple->hashvalue,
													node,
													spstate,
													NULL);
					spstate->sp_OuterTable->totalTuples += 1;
					continue;
				}

				memcpy(spstate->sp_OuterTuple, tuple, sizeof(*tuple));
				memcpy(((unsigned char*)(spstate->sp_OuterTuple + 1)) +
					   sizeof(Graphid) + spstate->sp_RowidSize,
					   tuple + 1,
					   tuple->t_len - sizeof(*tuple));
				spstate->sp_OuterTuple->t_len += sizeof(Graphid) + spstate->sp_RowidSize;
				paramno = node->correctedParam->paramid;
				prm = &(spstate->js.ps.ps_ExprContext->ecxt_param_exec_vals[paramno]);

				if (node->isFieldSelect)
				{
					node->vertexRow = replace_vertexRow_graphid(node->tupleDesc,
																node->vertexRow,
																*graphid);
					prm->value = HeapTupleGetDatum(node->vertexRow);
				}
				else
					prm->value = *graphid;

				outerPlanState(node)->chgParam = bms_add_member(outerPlanState(node)->chgParam,
																paramno);
				ExecReScan(outerPlanState(node));

				if (node->hops > 1)
				{
					ExecHash2SideTableInsertGraphid(spstate->sp_OuterTable,
													*graphid,
													hashTuple->hashvalue,
													node,
													spstate,
													NULL);
					spstate->sp_OuterTable->totalTuples += 1;
				}

				return true;
			}
		}
		else if (keytable->innerBatchFile[spstate->sp_CurKeyBatch])
		{
			uint32 hashvalue;

			slot = ExecShortestpathGetSavedTuple(keytable->innerBatchFile[spstate->sp_CurKeyBatch],
												 &hashvalue,
												 spstate->sp_OuterTupleSlot);
			if (!TupIsNull(slot))
			{
				bool shouldFree;
				MinimalTuple   tuple = ExecFetchSlotMinimalTuple(slot, &shouldFree);
				int            paramno;
				ParamExecData *prm;
				Graphid		   *graphid;

				graphid = (Graphid *) (tuple+1);
				if (*graphid == 0) continue;
				if ((node->hops > 1 &&
					 tuple->t_len == sizeof(*tuple) + sizeof(Graphid) + spstate->sp_RowidSize))
				{
					ExecHash2SideTableInsertGraphid(spstate->sp_OuterTable,
													*graphid,
													hashvalue,
													node,
													spstate,
													NULL);
					spstate->sp_OuterTable->totalTuples += 1;
					continue;
				}

				memcpy(spstate->sp_OuterTuple, tuple, sizeof(*tuple));
				memcpy(((unsigned char*)(spstate->sp_OuterTuple + 1)) +
					   sizeof(Graphid) + spstate->sp_RowidSize,
					   tuple + 1,
					   tuple->t_len - sizeof(*tuple));

				if (shouldFree)
				{
					heap_free_minimal_tuple(tuple);
				}

				spstate->sp_OuterTuple->t_len += sizeof(Graphid) + spstate->sp_RowidSize;
				paramno = node->correctedParam->paramid;
				prm = &(spstate->js.ps.ps_ExprContext->ecxt_param_exec_vals[paramno]);

				if (node->isFieldSelect)
				{
					node->vertexRow = replace_vertexRow_graphid(node->tupleDesc,
																node->vertexRow,
																*graphid);
					prm->value = HeapTupleGetDatum(node->vertexRow);
				}
				else
					prm->value = *graphid;

				outerPlanState(node)->chgParam = bms_add_member(outerPlanState(node)->chgParam,
																paramno);
				ExecReScan(outerPlanState(node));

				if (node->hops > 1)
				{
					ExecHash2SideTableInsertGraphid(spstate->sp_OuterTable,
													*graphid,
													hashvalue,
													node,
													spstate,
													NULL);
					spstate->sp_OuterTable->totalTuples += 1;
				}

				return true;
			}

			if (keytable->innerBatchFile[spstate->sp_CurKeyBatch])
			{
				BufFileClose(keytable->innerBatchFile[spstate->sp_CurKeyBatch]);
				keytable->innerBatchFile[spstate->sp_CurKeyBatch] = NULL;
			}
		}
		spstate->sp_CurKeyBatch++;
		if (spstate->sp_CurKeyBatch < keytable->nbatch)
		{
			spstate->sp_CurKeyIdx = 0;
			if (keytable->innerBatchFile[spstate->sp_CurKeyBatch])
			{
				if (BufFileSeek(keytable->innerBatchFile[spstate->sp_CurKeyBatch], 0, 0L, SEEK_SET))
					ereport(ERROR,
							(errcode_for_file_access(),
									errmsg("could not rewind hash-join temporary file: %m")));
			}
		}
	}

	return false;
}

/*
 * ExecShortestpathNewBatch
 *		switch to a new hashjoin batch
 *
 * Returns true if successful, false if there are no more batches.
 */
static bool
ExecShortestpathNewBatch(ShortestpathState *spstate)
{
	HashJoinTable   outertable = spstate->sp_OuterTable;
	HashJoinTable   hashtable = spstate->sp_HashTable;
	int             nextbatch;
	BufFile        *innerFile;
	TupleTableSlot *slot;
	uint32          hashvalue;

	Assert(hashtable->nbatch > 0 && outertable->nbatch >= hashtable->nbatch &&
		   outertable->nbatch % hashtable->nbatch == 0);

	nextbatch = outertable->curbatch + hashtable->nbatch;
	if (nextbatch >= outertable->nbatch)
	{
		nextbatch = nextbatch % hashtable->nbatch + 1;
		if (nextbatch >= hashtable->nbatch) return false;
	}

	if (outertable->curbatch >= 0)
	{
		if (outertable->innerBatchFile[outertable->curbatch] == NULL)
		{
			HashMemoryChunk oldchunks = outertable->chunks;
			outertable->chunks = NULL;

			/* so, let's scan through the old chunks, and all tuples in each chunk */
			while (oldchunks != NULL)
			{
				HashMemoryChunk nextchunk = oldchunks->next.unshared;

				/* position within the buffer (up to oldchunks->used) */
				size_t		idx = 0;

				/* process all tuples stored in this chunk (and then free it) */
				while (idx < oldchunks->used)
				{
					HashJoinTuple hashTuple = (HashJoinTuple) (HASH_CHUNK_DATA(oldchunks) + idx);
					MinimalTuple  tuple = HJTUPLE_MINTUPLE(hashTuple);
					int           hashTupleSize = (HJTUPLE_OVERHEAD + tuple->t_len);

					if (*((Graphid*)(tuple+1)) != 0)
						ExecShortestpathSaveTuple(tuple,
												  hashTuple->hashvalue,
												  &outertable->innerBatchFile[outertable->curbatch]);

					outertable->spaceUsed -= hashTupleSize;

					/* next tuple in this chunk */
					idx += MAXALIGN(hashTupleSize);
				}

				/* we're done with this chunk - free it and proceed to the next one */
				pfree(oldchunks);
				oldchunks = nextchunk;
			}
		}
	}
	outertable->curbatch = nextbatch;
	ExecHash2SideTableReset(outertable);
	innerFile = outertable->innerBatchFile[nextbatch];
	if (innerFile != NULL)
	{
		long saved = 0;
		if (BufFileSeek(innerFile, 0, 0L, SEEK_SET))
			ereport(ERROR,
					(errcode_for_file_access(),
							errmsg("could not rewind hash-join temporary file: %m")));

		while ((slot = ExecShortestpathGetSavedTuple(innerFile,
													 &hashvalue,
													 spstate->outerNode->slot)))
		{
			/*
			 * NOTE: some tuples may be sent to future batches.  Also, it is
			 * possible for outertable->nbatch to be increased here!
			 */
			if (!ExecHash2SideTableInsert(outertable,
										  slot,
										  hashvalue,
										  spstate->outerNode,
										  spstate,
										  &saved))
				saved++;
		}
		if (saved > 0)
		{
			BufFileClose(innerFile);
			outertable->innerBatchFile[nextbatch] = NULL;
		}
	}
	spstate->sp_CurOuterChunks = outertable->chunks;
	spstate->sp_CurOuterIdx = 0;

	if (nextbatch < hashtable->nbatch)
	{
		if (hashtable->curbatch >= 0)
		{
			if (hashtable->innerBatchFile[hashtable->curbatch] == NULL)
			{
				HashMemoryChunk oldchunks = hashtable->chunks;
				hashtable->chunks = NULL;

				/* so, let's scan through the old chunks, and all tuples in each chunk */
				while (oldchunks != NULL)
				{
					HashMemoryChunk nextchunk = oldchunks->next.unshared;

					/* position within the buffer (up to oldchunks->used) */
					size_t		idx = 0;

					/* process all tuples stored in this chunk (and then free it) */
					while (idx < oldchunks->used)
					{
						HashJoinTuple hashTuple = (HashJoinTuple) (HASH_CHUNK_DATA(oldchunks) + idx);
						MinimalTuple  tuple = HJTUPLE_MINTUPLE(hashTuple);
						int           hashTupleSize = (HJTUPLE_OVERHEAD + tuple->t_len);

						if (*((Graphid*)(tuple+1)) != 0)
							ExecShortestpathSaveTuple(tuple,
													  hashTuple->hashvalue,
													  &hashtable->innerBatchFile[hashtable->curbatch]);

						hashtable->spaceUsed -= hashTupleSize;

						/* next tuple in this chunk */
						idx += MAXALIGN(hashTupleSize);
					}

					/* we're done with this chunk - free it and proceed to the next one */
					pfree(oldchunks);
					oldchunks = nextchunk;
				}
			}
		}
		hashtable->curbatch = nextbatch;
		ExecHash2SideTableReset(hashtable);
		innerFile = hashtable->innerBatchFile[nextbatch];
		if (innerFile != NULL)
		{
			long saved = 0;
			if (BufFileSeek(innerFile, 0, 0L, SEEK_SET))
				ereport(ERROR,
						(errcode_for_file_access(),
								errmsg("could not rewind hash-join temporary file: %m")));

			while ((slot = ExecShortestpathGetSavedTuple(innerFile,
														 &hashvalue,
														 spstate->innerNode->slot)))
			{
				/*
				 * NOTE: some tuples may be sent to future batches.  Also, it is
				 * possible for hashtable->nbatch to be increased here!
				 */
				if (!ExecHash2SideTableInsert(hashtable,
											  slot,
											  hashvalue,
											  spstate->innerNode,
											  spstate,
											  &saved))
					saved++;
			}
			if (saved > 0)
			{
				BufFileClose(innerFile);
				hashtable->innerBatchFile[nextbatch] = NULL;
			}
		}
	}

	return true;
}

/*
 * ExecShortestpathSaveTuple
 *		save a tuple to a batch file.
 *
 * The data recorded in the file for each tuple is its hash value,
 * then the tuple in MinimalTuple format.
 *
 * Note: it is important always to call this in the regular executor
 * context, not in a shorter-lived context; else the temp file buffers
 * will get messed up.
 */
void
ExecShortestpathSaveTuple(MinimalTuple tuple, uint32 hashvalue,
						  BufFile **fileptr)
{
	BufFile    *file = *fileptr;
	size_t		written;

	if (file == NULL)
	{
		/* First write to this batch file, so open it. */
		file = BufFileCreateTemp(false);
		*fileptr = file;
	}

	written = BufFileWrite(file, (void *) &hashvalue, sizeof(uint32));
	if (written != sizeof(uint32))
		ereport(ERROR,
				(errcode_for_file_access(),
						errmsg("could not write to hash-join temporary file: %m")));

	written = BufFileWrite(file, (void *) tuple, tuple->t_len);
	if (written != tuple->t_len)
		ereport(ERROR,
				(errcode_for_file_access(),
						errmsg("could not write to hash-join temporary file: %m")));
}

/*
 * ExecShortestpathGetSavedTuple
 *		read the next tuple from a batch file.  Return NULL if no more.
 *
 * On success, *hashvalue is set to the tuple's hash value, and the tuple
 * itself is stored in the given slot.
 */
TupleTableSlot *
ExecShortestpathGetSavedTuple(BufFile *file,
							  uint32 *hashvalue,
							  TupleTableSlot *tupleSlot)
{
	uint32		header[2];
	size_t		nread;
	MinimalTuple tuple;

	/*
	 * We check for interrupts here because this is typically taken as an
	 * alternative code path to an ExecProcNode() call, which would include
	 * such a check.
	 */
	CHECK_FOR_INTERRUPTS();

	/*
	 * Since both the hash value and the MinimalTuple length word are uint32,
	 * we can read them both in one BufFileRead() call without any type
	 * cheating.
	 */
	nread = BufFileRead(file, (void *) header, sizeof(header));
	if (nread == 0)				/* end of file */
	{
		ExecClearTuple(tupleSlot);
		return NULL;
	}
	if (nread != sizeof(header))
		ereport(ERROR,
				(errcode_for_file_access(),
						errmsg("could not read from hash-join temporary file: %m")));
	*hashvalue = header[0];
	tuple = (MinimalTuple) palloc(header[1]);
	tuple->t_len = header[1];
	nread = BufFileRead(file,
						(void *) ((char *) tuple + sizeof(uint32)),
						header[1] - sizeof(uint32));
	if (nread != header[1] - sizeof(uint32))
		ereport(ERROR,
				(errcode_for_file_access(),
						errmsg("could not read from hash-join temporary file: %m")));
	return ExecStoreMinimalTuple(tuple, tupleSlot, true);
}

Datum
ExecShortestpathProjectEvalArray(Oid element_typeid, const unsigned char *elems,
								 long len, ExprContext *econtext)
{
	MemoryContext  oldContext;
	Datum         *values;
	bool          *nulls;
	int            i;
	int            dims[1];
	int            lbs[1];
	ArrayType     *result;
	int16          elemlength;     /* typlen of the array element type */
	bool           elembyval;      /* is the element type pass-by-value? */
	char           elemalign;      /* typalign of the element type */

	oldContext = MemoryContextSwitchTo(econtext->ecxt_per_tuple_memory);

	get_typlenbyvalalign(element_typeid, &elemlength, &elembyval, &elemalign);

	if (len <= 0)
	{
		result = construct_empty_array(element_typeid);
	}
	else
	{
		values = palloc(len * sizeof(*values));
		nulls = palloc(len * sizeof(*nulls));
		for (i = 0; i < len; i++)
		{
			if (elembyval == true)
			{
				values[i] = UInt64GetDatum(*((Datum*) elems));
			}
			else
			{
				values[i] = (Datum)elems;
			}
			nulls[i] = false;
			elems += elemlength;
		}
		dims[0] = len;
		lbs[0] = 1;
		result = construct_md_array((Datum *) values, nulls, 1, dims, lbs, element_typeid,
									elemlength, elembyval, elemalign);
	}

	MemoryContextSwitchTo(oldContext);

	return PointerGetDatum(result);
}

TupleTableSlot *
ExecShortestpathProject(ShortestpathState *node,
						unsigned char     *outerids,
						long               lenOuterids,
						unsigned char     *innerids,
						long               lenInnerids,
						int                sizeGraphid,
						int                sizeRowid)
{
	ProjectionInfo *projInfo;
	ExprContext    *econtext;
	TupleTableSlot *slot;
	Datum          *tts_values;
	bool           *tts_isnull;
	unsigned char  *oids;
	unsigned char  *iids;
	unsigned char  *vids;
	unsigned char  *eids;
	long            lenVids;
	long            lenEids;
	long            i;

	oids = outerids + (sizeGraphid + sizeRowid) * lenOuterids;
	iids = innerids;
	vids = node->sp_Vertexids;
	eids = node->sp_Edgeids;
	lenVids = 0;
	lenEids = 0;
	for (i = 0; i < lenOuterids; i++)
	{
		memcpy(vids, oids, sizeGraphid);
		vids += sizeGraphid;
		oids -= sizeRowid;
		lenVids++;
		memcpy(eids, oids, sizeRowid);
		eids += sizeRowid;
		oids -= sizeGraphid;
		lenEids++;
	}
	memcpy(vids, iids, sizeGraphid);
	vids += sizeGraphid;
	iids += sizeGraphid;
	lenVids++;
	for (i = 0; i < lenInnerids; i++)
	{
		memcpy(eids, iids, sizeRowid);
		eids += sizeRowid;
		iids += sizeRowid;
		lenEids++;
		memcpy(vids, iids, sizeGraphid);
		vids += sizeGraphid;
		iids += sizeGraphid;
		lenVids++;
	}

	projInfo = node->js.ps.ps_ProjInfo;
	slot = projInfo->pi_state.resultslot;
	econtext = projInfo->pi_exprContext;

	ExecClearTuple(slot);

	tts_values = slot->tts_values;
	tts_isnull = slot->tts_isnull;

	tts_values[0] = ExecShortestpathProjectEvalArray(GRAPHIDOID, node->sp_Vertexids, lenVids, econtext);
	tts_isnull[0] = false;
	tts_values[1] = ExecShortestpathProjectEvalArray(ROWIDOID, node->sp_Edgeids, lenEids, econtext);
	tts_isnull[1] = false;

	return ExecStoreVirtualTuple(slot);
}

void
ExecReScanShortestpath(ShortestpathState *node)
{
	node->sp_KeyTable = NULL;
	node->sp_OuterTable = NULL;
	node->sp_HashTable = NULL;
	node->sp_JoinState = SP_GET_PARAMETER;

	/* Always reset intra-tuple state */
	node->sp_CurHashValue = 0;
	node->sp_CurBucketNo = 0;
	node->sp_CurSkewBucketNo = INVALID_SKEW_BUCKET_NO;
	node->sp_CurTuple = NULL;

	if (node->js.ps.chgParam != NULL)
	{
		Param *sParam = (Param *) node->source->expr;
		Param *tParam = (Param *) node->target->expr;

		/*
		 * If we have FieldSelect expressions grab the Param expression
		 * from the FieldSelect's arg value.
		 */
		if (IsA(sParam, FieldSelect))
			sParam = ((Param *) ((FieldSelect *)sParam)->arg);
		if (IsA(tParam, FieldSelect))
			tParam = ((Param *) ((FieldSelect *)tParam)->arg);

		if (bms_is_member(sParam->paramid, node->js.ps.chgParam))
			node->startVid = 0;
		if (bms_is_member(tParam->paramid, node->js.ps.chgParam))
			node->endVid = 0;
	}

	node->hops       = 0;
	node->numResults = 0;

	node->outerNode = (Hash2SideState *)outerPlanState(node);
	node->innerNode = (Hash2SideState *)innerPlanState(node);

	ExecReScan(node->js.ps.lefttree);
	ExecReScan(node->js.ps.righttree);
}
