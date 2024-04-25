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

#include <math.h>
#include <limits.h>

#include "access/hash.h"
#include "access/htup_details.h"
#include "commands/tablespace.h"
#include "executor/execdebug.h"
#include "executor/hashjoin.h"
#include "executor/nodeHash2Side.h"
#include "executor/nodeShortestpath.h"
#include "miscadmin.h"
#include "utils/dynahash.h"
#include "utils/memutils.h"
#include "utils/lsyscache.h"
#include "utils/syscache.h"


static void ExecHash2SideIncreaseNumBatches(HashJoinTable hashtable);

static void *dense_alloc(HashJoinTable hashtable, Size size);

/* ----------------------------------------------------------------
 *		ExecHash2Side
 *
 *		stub for pro forma compliance
 * ----------------------------------------------------------------
 */
static TupleTableSlot *
ExecHash2Side(PlanState *node)
{
	return ExecProcNode(outerPlanState(node));
}

/* ----------------------------------------------------------------
 *		MultiExecHash2Side
 *
 *		build hash table for hashjoin, doing partitioning if more
 *		than one batch is required.
 * ----------------------------------------------------------------
 */
Node *
MultiExecHash2Side(Hash2SideState *node)
{
	HashJoinTable   hashtable;
	TupleTableSlot *slot;
	uint32          hashvalue;

	/*
	 * get state info from node
	 */
	hashtable = node->hashtable;

	/* resize the hash table if needed (NTUP_PER_BUCKET exceeded) */
	if (hashtable->nbuckets != hashtable->nbuckets_optimal)
	{
		hashtable->spaceUsed -= hashtable->nbuckets * sizeof(HashJoinTuple);
		ExecHash2SideIncreaseNumBuckets(hashtable, node);
		/* Account for the buckets in spaceUsed (reported in EXPLAIN ANALYZE) */
		hashtable->spaceUsed += hashtable->nbuckets * sizeof(HashJoinTuple);
		if (hashtable->spaceUsed > hashtable->spacePeak)
			hashtable->spacePeak = hashtable->spaceUsed;
	}

	if (hashtable->curbatch != 0)
	{
		if (hashtable->curbatch > 0)
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
		hashtable->curbatch = 0;
		ExecHash2SideTableReset(hashtable);
		if (hashtable->innerBatchFile[0] != NULL)
		{
			long saved = 0;
			if (BufFileSeek(hashtable->innerBatchFile[0], 0, 0L, SEEK_SET))
				ereport(ERROR,
						(errcode_for_file_access(),
								errmsg("could not rewind hash-join temporary file: %m")));

			while ((slot = ExecShortestpathGetSavedTuple(hashtable->innerBatchFile[0],
														 &hashvalue,
														 node->slot)))
			{
				/*
				 * NOTE: some tuples may be sent to future batches.  Also, it is
				 * possible for hashtable->nbatch to be increased here!
				 */
				if (!ExecHash2SideTableInsert(hashtable,
											  slot,
											  hashvalue,
											  node,
											  (ShortestpathState *) node->spstate,
											  &saved))
					saved++;
			}
			if (saved > 0)
			{
				BufFileClose(hashtable->innerBatchFile[0]);
				hashtable->innerBatchFile[0] = NULL;
			}
		}
	}

	/*
	 * We do not return the hash table directly because it's not a subtype of
	 * Node, and so would violate the MultiExecProcNode API.  Instead, our
	 * parent Shortestpath node is expected to know how to fish it out of our node
	 * state.  Ugly but not really worth cleaning up, since Hashjoin knows
	 * quite a bit more about Hash besides that.
	 */
	return NULL;
}

/* ----------------------------------------------------------------
 *		ExecInitHash2Side
 *
 *		Init routine for Hash2Side node
 * ----------------------------------------------------------------
 */
Hash2SideState *
ExecInitHash2Side(Hash2Side *node, EState *estate, int eflags)
{
	Hash2SideState  *hashstate;

	/* check for unsupported flags */
	Assert(!(eflags & (EXEC_FLAG_BACKWARD | EXEC_FLAG_MARK)));

	/*
	 * create state structure
	 */
	hashstate = makeNode(Hash2SideState);
	hashstate->ps.plan = (Plan *) node;
	hashstate->ps.state = estate;
	hashstate->ps.ExecProcNode = ExecHash2Side;
	hashstate->keytable = NULL;
	hashstate->hashtable = NULL;
	hashstate->hashkeys = NIL;	/* will be set by parent Shortestpath */
	hashstate->spstate = NULL;
	hashstate->totalPaths = 0;
	hashstate->hops = 0;
	hashstate->spacePeak = 0;

	/*
	 * Miscellaneous initialization
	 *
	 * create expression context for node
	 */
	ExecAssignExprContext(estate, &hashstate->ps);

	ExecInitResultTupleSlotTL(&hashstate->ps, &TTSOpsMinimalTuple);
	/*
	 * initialize child expressions
	 */
	hashstate->ps.qual =
			ExecInitQual(node->plan.qual, (PlanState *) hashstate);

	/*
	 * initialize child nodes
	 */
	outerPlanState(hashstate) = ExecInitNode(outerPlan(node), estate, eflags);

	hashstate->ps.ps_ProjInfo = NULL;

	return hashstate;
}

/* ---------------------------------------------------------------
 *		ExecEndHash2Side
 *
 *		clean up routine for Hash2Side node
 * ----------------------------------------------------------------
 */
void
ExecEndHash2Side(Hash2SideState *node)
{
	PlanState  *outerPlan;

	if (node->keytable)
	{
		ExecHash2SideTableDestroy(node->keytable);
		node->keytable = NULL;
	}
	if (node->hashtable)
	{
		ExecHash2SideTableDestroy(node->hashtable);
		node->hashtable = NULL;
	}

	/*
	 * free exprcontext
	 */
	ExecFreeExprContext(&node->ps);

	/*
	 * shut down the subplan
	 */
	outerPlan = outerPlanState(node);
	ExecEndNode(outerPlan);
}


/* ----------------------------------------------------------------
 *		ExecHash2SideTableCreate
 *
 *		create an empty hashtable data structure for hashjoin.
 * ----------------------------------------------------------------
 */
HashJoinTable
ExecHash2SideTableCreate(Hash2SideState *node, List *hashOperators,
						 double ntuples, double npaths, long hops, Size spacePeak)
{
	HashJoinTable hashtable;
	Plan	   *outerNode;
	int			nbuckets;
	int			nbatch;
	int			log2_nbuckets;
	int			nkeys;
	int			i;
	ListCell   *ho;
	MemoryContext oldcxt;

	/*
	 * Get information about the size of the relation to be hashed (it's the
	 * "outer" subtree of this node, but the inner relation of the hashjoin).
	 * Compute the appropriate size of the hash table.
	 */
	outerNode = outerPlan(node->ps.plan);

	ExecChooseHash2SideTableSize(ntuples, node->totalPaths, outerNode->plan_width, hops,
								 &nbuckets, &nbatch);

	/* nbuckets must be a power of 2 */
	log2_nbuckets = my_log2(nbuckets);
	Assert(nbuckets == (1 << log2_nbuckets));

	/*
	 * Initialize the hash table control block.
	 *
	 * The hashtable control block is just palloc'd from the executor's
	 * per-query memory context.
	 */
	hashtable = (HashJoinTable) palloc(sizeof(HashJoinTableData));
	hashtable->nbuckets = nbuckets;
	hashtable->nbuckets_original = nbuckets;
	hashtable->nbuckets_optimal = nbuckets;
	hashtable->log2_nbuckets = log2_nbuckets;
	hashtable->log2_nbuckets_optimal = log2_nbuckets;
	hashtable->buckets.unshared = NULL;
	hashtable->keepNulls = false;
	hashtable->skewEnabled = false;
	hashtable->skewBucket = NULL;
	hashtable->skewBucketLen = 0;
	hashtable->nSkewBuckets = 0;
	hashtable->skewBucketNums = NULL;
	hashtable->nbatch = nbatch;
	hashtable->curbatch = 0;
	hashtable->nbatch_original = nbatch;
	hashtable->nbatch_outstart = nbatch;
	hashtable->growEnabled = true;
	hashtable->totalTuples = 0;
	hashtable->skewTuples = 0;
	hashtable->innerBatchFile = NULL;
	hashtable->outerBatchFile = NULL;
	hashtable->spaceUsed = 0;
	hashtable->spacePeak = spacePeak;
	hashtable->spaceAllowed = work_mem * 1024L;
	hashtable->spaceUsedSkew = 0;
	hashtable->spaceAllowedSkew =
			hashtable->spaceAllowed * SKEW_HASH_MEM_PERCENT / 100;
	hashtable->chunks = NULL;
	node->totalPaths = 0;

#ifdef HJDEBUG
	printf("Shortestpath %p: initial nbatch = %d, nbuckets = %d\n",
		   hashtable, nbatch, nbuckets);
#endif

	/*
	 * Get info about the hash functions to be used for each hash key. Also
	 * remember whether the join operators are strict.
	 */
	nkeys = list_length(hashOperators);
	hashtable->outer_hashfunctions =
			(FmgrInfo *) palloc(nkeys * sizeof(FmgrInfo));
	hashtable->inner_hashfunctions =
			(FmgrInfo *) palloc(nkeys * sizeof(FmgrInfo));
	hashtable->hashStrict = (bool *) palloc(nkeys * sizeof(bool));
	i = 0;
	foreach(ho, hashOperators)
	{
		Oid			hashop = lfirst_oid(ho);
		Oid			left_hashfn;
		Oid			right_hashfn;

		if (!get_op_hash_functions(hashop, &left_hashfn, &right_hashfn))
			elog(ERROR, "could not find hash function for hash operator %u",
				 hashop);
		fmgr_info(left_hashfn, &hashtable->outer_hashfunctions[i]);
		fmgr_info(right_hashfn, &hashtable->inner_hashfunctions[i]);
		hashtable->hashStrict[i] = op_strict(hashop);
		i++;
	}

	/*
	 * Create temporary memory contexts in which to keep the hashtable working
	 * storage.  See notes in executor/hashjoin.h.
	 */
	hashtable->hashCxt = AllocSetContextCreate(CurrentMemoryContext,
											   "Hash2SideTableContext",
											   ALLOCSET_DEFAULT_SIZES);

	hashtable->batchCxt = AllocSetContextCreate(hashtable->hashCxt,
												"Hash2SideBatchContext",
												ALLOCSET_DEFAULT_SIZES);

	/* Allocate data that will live for the life of the hashjoin */

	oldcxt = MemoryContextSwitchTo(hashtable->hashCxt);

	if (nbatch > 1)
	{
		/*
		 * allocate and initialize the file arrays in hashCxt
		 */
		hashtable->innerBatchFile = (BufFile **)
				palloc0(nbatch * sizeof(BufFile *));
		hashtable->outerBatchFile = (BufFile **)
				palloc0(nbatch * sizeof(BufFile *));
		/* The files will not be opened until needed... */
		/* ... but make sure we have temp tablespaces established for them */
		PrepareTempTablespaces();
	}

	/*
	 * Prepare context for the first-scan space allocations; allocate the
	 * hashbucket array therein, and set each bucket "empty".
	 */
	MemoryContextSwitchTo(hashtable->batchCxt);

	hashtable->buckets.unshared = (HashJoinTuple *)
			palloc0(nbuckets * sizeof(HashJoinTuple));

	MemoryContextSwitchTo(oldcxt);

	return hashtable;
}

HashJoinTable
ExecHash2SideTableClone(Hash2SideState *node, List *hashOperators,
						HashJoinTable sourcetable, Size spacePeak)
{
	HashJoinTable  hashtable;
	int			   nkeys;
	int			   i;
	ListCell      *ho;
	MemoryContext  oldcxt;

	/*
	 * Initialize the hash table control block.
	 *
	 * The hashtable control block is just palloc'd from the executor's
	 * per-query memory context.
	 */
	hashtable = (HashJoinTable) palloc(sizeof(HashJoinTableData));
	hashtable->nbuckets = sourcetable->nbuckets;
	hashtable->nbuckets_original = sourcetable->nbuckets;
	hashtable->nbuckets_optimal = sourcetable->nbuckets;
	hashtable->log2_nbuckets = sourcetable->log2_nbuckets;
	hashtable->log2_nbuckets_optimal = sourcetable->log2_nbuckets;
	hashtable->buckets.unshared = NULL;
	hashtable->keepNulls = false;
	hashtable->skewEnabled = false;
	hashtable->skewBucket = NULL;
	hashtable->skewBucketLen = 0;
	hashtable->nSkewBuckets = 0;
	hashtable->skewBucketNums = NULL;
	hashtable->nbatch = sourcetable->nbatch;
	hashtable->curbatch = 0;
	hashtable->nbatch_original = sourcetable->nbatch;
	hashtable->nbatch_outstart = sourcetable->nbatch;
	hashtable->growEnabled = true;
	hashtable->totalTuples = 0;
	hashtable->skewTuples = 0;
	hashtable->innerBatchFile = NULL;
	hashtable->outerBatchFile = NULL;
	hashtable->spaceUsed = 0;
	hashtable->spacePeak = spacePeak;
	hashtable->spaceAllowed = work_mem * 1024L;
	hashtable->spaceUsedSkew = 0;
	hashtable->spaceAllowedSkew =
			hashtable->spaceAllowed * SKEW_HASH_MEM_PERCENT / 100;
	hashtable->chunks = NULL;
	node->totalPaths = 0;

#ifdef HJDEBUG
	printf("Shortestpath %p: initial nbatch = %d, nbuckets = %d\n",
		   hashtable, sourcetable->nbatch, sourcetable->nbuckets);
#endif

	/*
	 * Get info about the hash functions to be used for each hash key. Also
	 * remember whether the join operators are strict.
	 */
	nkeys = list_length(hashOperators);
	hashtable->outer_hashfunctions =
			(FmgrInfo *) palloc(nkeys * sizeof(FmgrInfo));
	hashtable->inner_hashfunctions =
			(FmgrInfo *) palloc(nkeys * sizeof(FmgrInfo));
	hashtable->hashStrict = (bool *) palloc(nkeys * sizeof(bool));
	i = 0;
	foreach(ho, hashOperators)
	{
		Oid			hashop = lfirst_oid(ho);
		Oid			left_hashfn;
		Oid			right_hashfn;

		if (!get_op_hash_functions(hashop, &left_hashfn, &right_hashfn))
			elog(ERROR, "could not find hash function for hash operator %u",
				 hashop);
		fmgr_info(left_hashfn, &hashtable->outer_hashfunctions[i]);
		fmgr_info(right_hashfn, &hashtable->inner_hashfunctions[i]);
		hashtable->hashStrict[i] = op_strict(hashop);
		i++;
	}

	/*
	 * Create temporary memory contexts in which to keep the hashtable working
	 * storage.  See notes in executor/hashjoin.h.
	 */
	hashtable->hashCxt = AllocSetContextCreate(CurrentMemoryContext,
											   "Hash2SideTableContext",
											   ALLOCSET_DEFAULT_SIZES);

	hashtable->batchCxt = AllocSetContextCreate(hashtable->hashCxt,
												"Hash2SideBatchContext",
												ALLOCSET_DEFAULT_SIZES);

	/* Allocate data that will live for the life of the hashjoin */

	oldcxt = MemoryContextSwitchTo(hashtable->hashCxt);

	if (hashtable->nbatch > 1)
	{
		/*
		 * allocate and initialize the file arrays in hashCxt
		 */
		hashtable->innerBatchFile = (BufFile **)
				palloc0(hashtable->nbatch * sizeof(BufFile *));
		hashtable->outerBatchFile = (BufFile **)
				palloc0(hashtable->nbatch * sizeof(BufFile *));
		/* The files will not be opened until needed... */
		/* ... but make sure we have temp tablespaces established for them */
		PrepareTempTablespaces();
	}

	/*
	 * Prepare context for the first-scan space allocations; allocate the
	 * hashbucket array therein, and set each bucket "empty".
	 */
	MemoryContextSwitchTo(hashtable->batchCxt);

	hashtable->buckets.unshared = (HashJoinTuple *)
			palloc0(hashtable->nbuckets * sizeof(HashJoinTuple));

	MemoryContextSwitchTo(oldcxt);

	return hashtable;
}

/*
 * Compute appropriate size for hashtable given the estimated size of the
 * relation to be hashed (number of rows and average row width).
 *
 * This is exported so that the planner's costsize.c can use it.
 */

/* Target bucket loading (tuples per bucket) */
#define NTUP_PER_BUCKET			1

void
ExecChooseHash2SideTableSize(double ntuples,
							 double npaths,
							 int tupwidth,
							 long hops,
							 int *numbuckets,
							 int *numbatches)
{
	int			tupsize;
	double		inner_rel_bytes;
	long		bucket_bytes;
	long		hash_table_bytes;
	long		max_pointers;
	long		mppow2;
	int			nbatch = 1;
	int			nbuckets;
	double		dbuckets;

	/* Force a plausible relation size if no info */
	if (npaths <= 0.0)
		npaths = 1000.0;
	if (ntuples <= npaths)
		ntuples = npaths;

	/*
	 * Estimate tupsize based on footprint of tuple in hashtable... note this
	 * does not allow for any palloc overhead.  The manipulations of spaceUsed
	 * don't count palloc overhead either.
	 */
	tupsize = HJTUPLE_OVERHEAD +
			  MAXALIGN(SizeofMinimalTupleHeader) +
			  MAXALIGN(tupwidth);
	inner_rel_bytes = (ntuples - npaths) * tupsize;
	tupsize = HJTUPLE_OVERHEAD +
			  MAXALIGN(SizeofMinimalTupleHeader) +
			  MAXALIGN(tupwidth * hops + sizeof(Graphid));
	inner_rel_bytes += npaths * tupsize;

	/*
	 * Target in-memory hashtable size is work_mem kilobytes.
	 */
	hash_table_bytes = work_mem * 1024L;

	/*
	 * Set nbuckets to achieve an average bucket load of NTUP_PER_BUCKET when
	 * memory is filled, assuming a single batch; but limit the value so that
	 * the pointer arrays we'll try to allocate do not exceed work_mem nor
	 * MaxAllocSize.
	 *
	 * Note that both nbuckets and nbatch must be powers of 2 to make
	 * ExecHash2SideGetBucketAndBatch fast.
	 */
	max_pointers = (work_mem * 1024L) / sizeof(HashJoinTuple);
	max_pointers = Min(max_pointers, MaxAllocSize / sizeof(HashJoinTuple));
	/* If max_pointers isn't a power of 2, must round it down to one */
	mppow2 = 1L << my_log2(max_pointers);
	if (max_pointers != mppow2)
		max_pointers = mppow2 / 2;

	/* Also ensure we avoid integer overflow in nbatch and nbuckets */
	/* (this step is redundant given the current value of MaxAllocSize) */
	max_pointers = Min(max_pointers, INT_MAX / 2);

	dbuckets = ceil(ntuples / NTUP_PER_BUCKET);
	dbuckets = Min(dbuckets, max_pointers);
	nbuckets = (int) dbuckets;
	/* don't let nbuckets be really small, though ... */
	nbuckets = Max(nbuckets, 1024);
	/* ... and force it to be a power of 2. */
	nbuckets = 1 << my_log2(nbuckets);

	/*
	 * If there's not enough space to store the projected number of tuples and
	 * the required bucket headers, we will need multiple batches.
	 */
	bucket_bytes = sizeof(HashJoinTuple) * nbuckets;
	if (inner_rel_bytes + bucket_bytes > hash_table_bytes)
	{
		/* We'll need multiple batches */
		long		lbuckets;
		double		dbatch;
		int			minbatch;
		long		bucket_size;

		/*
		 * Estimate the number of buckets we'll want to have when work_mem is
		 * entirely full.  Each bucket will contain a bucket pointer plus
		 * NTUP_PER_BUCKET tuples, whose projected size already includes
		 * overhead for the hash code, pointer to the next tuple, etc.
		 */
		bucket_size = (tupsize * NTUP_PER_BUCKET + sizeof(HashJoinTuple));
		lbuckets = 1L << my_log2(hash_table_bytes / bucket_size);
		lbuckets = Min(lbuckets, max_pointers);
		nbuckets = (int) lbuckets;
		nbuckets = 1 << my_log2(nbuckets);
		bucket_bytes = nbuckets * sizeof(HashJoinTuple);

		/*
		 * Buckets are simple pointers to hashjoin tuples, while tupsize
		 * includes the pointer, hash code, and MinimalTupleData.  So buckets
		 * should never really exceed 25% of work_mem (even for
		 * NTUP_PER_BUCKET=1); except maybe for work_mem values that are not
		 * 2^N bytes, where we might get more because of doubling. So let's
		 * look for 50% here.
		 */
		Assert(bucket_bytes <= hash_table_bytes / 2);

		/* Calculate required number of batches. */
		dbatch = ceil(inner_rel_bytes / (hash_table_bytes - bucket_bytes));
		dbatch = Min(dbatch, max_pointers);
		minbatch = (int) dbatch;
		nbatch = pg_nextpower2_32(Max(2, minbatch));
	}

	Assert(nbuckets > 0);
	Assert(nbatch > 0);

	*numbuckets = nbuckets;
	*numbatches = nbatch;
}


/* ----------------------------------------------------------------
 *		ExecHash2SideTableDestroy
 *
 *		destroy a hash table
 * ----------------------------------------------------------------
 */
void
ExecHash2SideTableDestroy(HashJoinTable hashtable)
{
	int			i;

	/*
	 * Make sure all the temp files are closed.  We skip batch 0, since it
	 * can't have any temp files (and the arrays might not even exist if
	 * nbatch is only 1).
	 */
	for (i = 0; i < hashtable->nbatch; i++)
	{
		if (hashtable->innerBatchFile)
			if (hashtable->innerBatchFile[i])
				BufFileClose(hashtable->innerBatchFile[i]);
		if (hashtable->outerBatchFile)
			if (hashtable->outerBatchFile[i])
				BufFileClose(hashtable->outerBatchFile[i]);
	}

	if (hashtable->outer_hashfunctions)
		pfree(hashtable->outer_hashfunctions);
	if (hashtable->inner_hashfunctions)
		pfree(hashtable->inner_hashfunctions);
	if (hashtable->hashStrict)
		pfree(hashtable->hashStrict);

	/* Release working memory (batchCxt is a child, so it goes away too) */
	MemoryContextDelete(hashtable->hashCxt);

	/* And drop the control block */
	pfree(hashtable);
}

/*
 * ExecHash2SideIncreaseNumBatches
 *		increase the original number of batches in order to reduce
 *		current memory consumption
 */
static void
ExecHash2SideIncreaseNumBatches(HashJoinTable hashtable)
{
	int			oldnbatch = hashtable->nbatch;
	int			curbatch = hashtable->curbatch;
	int			nbatch;
	MemoryContext oldcxt;
	long		ninmemory;
	long		nfreed;
	HashMemoryChunk oldchunks;

	/* do nothing if we've decided to shut off growth */
	if (!hashtable->growEnabled)
		return;

	/* safety check to avoid overflow */
	if (oldnbatch > Min(INT_MAX / 2, MaxAllocSize / (sizeof(void *) * 2)))
		return;

	nbatch = oldnbatch * 2;
	Assert(nbatch > 1);

#ifdef HJDEBUG
	printf("Shortestpath %p: increasing nbatch to %d because space = %zu\n",
		   hashtable, nbatch, hashtable->spaceUsed);
#endif

	oldcxt = MemoryContextSwitchTo(hashtable->hashCxt);

	if (hashtable->innerBatchFile == NULL)
	{
		/* we had no file arrays before */
		hashtable->innerBatchFile = (BufFile **)
				palloc0(nbatch * sizeof(BufFile *));
		hashtable->outerBatchFile = (BufFile **)
				palloc0(nbatch * sizeof(BufFile *));
		/* time to establish the temp tablespaces, too */
		PrepareTempTablespaces();
	}
	else
	{
		/* enlarge arrays and zero out added entries */
		hashtable->innerBatchFile = (BufFile **)
				repalloc(hashtable->innerBatchFile, nbatch * sizeof(BufFile *));
		hashtable->outerBatchFile = (BufFile **)
				repalloc(hashtable->outerBatchFile, nbatch * sizeof(BufFile *));
		MemSet(hashtable->innerBatchFile + oldnbatch, 0,
			   (nbatch - oldnbatch) * sizeof(BufFile *));
		MemSet(hashtable->outerBatchFile + oldnbatch, 0,
			   (nbatch - oldnbatch) * sizeof(BufFile *));
	}

	MemoryContextSwitchTo(oldcxt);

	hashtable->nbatch = nbatch;

	/*
	 * Scan through the existing hash table entries and dump out any that are
	 * no longer of the current batch.
	 */
	ninmemory = nfreed = 0;

	/* If know we need to resize nbuckets, we can do it while rebatching. */
	if (hashtable->nbuckets_optimal != hashtable->nbuckets)
	{
		/* we never decrease the number of buckets */
		Assert(hashtable->nbuckets_optimal > hashtable->nbuckets);

		hashtable->nbuckets = hashtable->nbuckets_optimal;
		hashtable->log2_nbuckets = hashtable->log2_nbuckets_optimal;

		hashtable->buckets.unshared = repalloc(hashtable->buckets.unshared,
											   sizeof(HashJoinTuple) * hashtable->nbuckets);
	}

	/*
	 * We will scan through the chunks directly, so that we can reset the
	 * buckets now and not have to keep track which tuples in the buckets have
	 * already been processed. We will free the old chunks as we go.
	 */
	memset(hashtable->buckets.unshared, 0, sizeof(HashJoinTuple) * hashtable->nbuckets);
	oldchunks = hashtable->chunks;
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
			MinimalTuple tuple = HJTUPLE_MINTUPLE(hashTuple);
			int			hashTupleSize = (HJTUPLE_OVERHEAD + tuple->t_len);
			int			bucketno;
			int			batchno;

			ninmemory++;
			ExecHash2SideGetBucketAndBatch(hashtable, hashTuple->hashvalue,
										   &bucketno, &batchno);

			if (batchno == curbatch)
			{
				/* keep tuple in memory - copy it into the new chunk */
				HashJoinTuple copyTuple;

				copyTuple = (HashJoinTuple) dense_alloc(hashtable, hashTupleSize);
				memcpy(copyTuple, hashTuple, hashTupleSize);

				/* and add it back to the appropriate bucket */
				copyTuple->next.unshared = hashtable->buckets.unshared[bucketno];
				hashtable->buckets.unshared[bucketno] = copyTuple;
			}
			else
			{
				/* dump it out */
				Assert(batchno > curbatch);
				ExecShortestpathSaveTuple(HJTUPLE_MINTUPLE(hashTuple),
										  hashTuple->hashvalue,
										  &hashtable->innerBatchFile[batchno]);

				hashtable->spaceUsed -= hashTupleSize;
				nfreed++;
			}

			/* next tuple in this chunk */
			idx += MAXALIGN(hashTupleSize);

			/* allow this loop to be cancellable */
			CHECK_FOR_INTERRUPTS();
		}

		/* we're done with this chunk - free it and proceed to the next one */
		pfree(oldchunks);
		oldchunks = nextchunk;
	}

#ifdef HJDEBUG
	printf("Shortestpath %p: freed %ld of %ld tuples, space now %zu\n",
		   hashtable, nfreed, ninmemory, hashtable->spaceUsed);
#endif

	/*
	 * If we dumped out either all or none of the tuples in the table, disable
	 * further expansion of nbatch.  This situation implies that we have
	 * enough tuples of identical hashvalues to overflow spaceAllowed.
	 * Increasing nbatch will not fix it since there's no way to subdivide the
	 * group any more finely. We have to just gut it out and hope the server
	 * has enough RAM.
	 */
	if (nfreed == 0 || nfreed == ninmemory)
	{
		hashtable->growEnabled = false;
#ifdef HJDEBUG
		printf("Shortestpath %p: disabling further increase of nbatch\n",
			   hashtable);
#endif
	}
}

/*
 * ExecHash2SideIncreaseNumBuckets
 *		increase the original number of buckets in order to reduce
 *		number of tuples per bucket
 */
void
ExecHash2SideIncreaseNumBuckets(HashJoinTable hashtable, Hash2SideState *node)
{
	HashMemoryChunk    chunk;
	ShortestpathState *spstate = (ShortestpathState *) node->spstate;

	/* do nothing if not an increase (it's called increase for a reason) */
	if (hashtable->nbuckets >= hashtable->nbuckets_optimal)
		return;

#ifdef HJDEBUG
	printf("Shortestpath %p: increasing nbuckets %d => %d\n",
		   hashtable, hashtable->nbuckets, hashtable->nbuckets_optimal);
#endif

	hashtable->nbuckets = hashtable->nbuckets_optimal;
	hashtable->log2_nbuckets = hashtable->log2_nbuckets_optimal;

	Assert(hashtable->nbuckets > 1);
	Assert(hashtable->nbuckets <= (INT_MAX / 2));
	Assert(hashtable->nbuckets == (1 << hashtable->log2_nbuckets));

	/*
	 * Just reallocate the proper number of buckets - we don't need to walk
	 * through them - we can walk the dense-allocated chunks (just like in
	 * ExecHash2SideIncreaseNumBatches, but without all the copying into new
	 * chunks)
	 */
	hashtable->buckets.unshared =
			(HashJoinTuple *) repalloc(hashtable->buckets.unshared,
									   hashtable->nbuckets * sizeof(HashJoinTuple));

	memset(hashtable->buckets.unshared, 0, hashtable->nbuckets * sizeof(HashJoinTuple));

	/* scan through all tuples in all chunks to rebuild the hash table */
	for (chunk = hashtable->chunks; chunk != NULL; chunk = chunk->next.unshared)
	{
		/* process all tuples stored in this chunk */
		size_t		idx = 0;

		while (idx < chunk->used)
		{
			HashJoinTuple hashTuple = (HashJoinTuple) (HASH_CHUNK_DATA(chunk) + idx);
			MinimalTuple  tuple = HJTUPLE_MINTUPLE(hashTuple);
			HashJoinTuple cursorTuple;
			MinimalTuple  body;
			int           bucketno;
			int           batchno;

			/* advance index past the tuple */
			idx += MAXALIGN(HJTUPLE_OVERHEAD + tuple->t_len);

			if (*((Graphid*)(tuple+1)) != 0)
			{
				ExecHash2SideGetBucketAndBatch(hashtable, hashTuple->hashvalue,
											   &bucketno, &batchno);

				if (tuple->t_len != sizeof(*tuple) + sizeof(Graphid) + spstate->sp_RowidSize)
				{
					for (cursorTuple = hashtable->buckets.unshared[bucketno];
						 cursorTuple != NULL;
						 cursorTuple = cursorTuple->next.unshared)
					{
						body = HJTUPLE_MINTUPLE(cursorTuple);
						if ((spstate->limit == 1 ||
							 body->t_len == sizeof(*tuple) + sizeof(Graphid) + spstate->sp_RowidSize) &&
							*((Graphid*)(body+1)) == *((Graphid*)(tuple+1)))
						{
							hashtable->totalTuples -= 1;
							node->totalPaths -= 1;
							*((Graphid*)(tuple+1)) = 0;
							break;
						}
					}
					if (cursorTuple != NULL) continue;
				}

				/* add the tuple to the proper bucket */
				hashTuple->next.unshared = hashtable->buckets.unshared[bucketno];
				hashtable->buckets.unshared[bucketno] = hashTuple;

				if (tuple->t_len == sizeof(*tuple) + sizeof(Graphid) + spstate->sp_RowidSize)
				{
					cursorTuple = hashtable->buckets.unshared[bucketno];
					while (cursorTuple->next.unshared != NULL)
					{
						body = HJTUPLE_MINTUPLE(cursorTuple->next.unshared);
						if (*((Graphid*)(body+1)) == *((Graphid*)(tuple+1)))
						{
							hashtable->totalTuples -= 1;
							if (body->t_len != sizeof(*tuple) + sizeof(Graphid) + spstate->sp_RowidSize)
								node->totalPaths -= 1;
							*((Graphid*)(body+1)) = 0;
							cursorTuple->next = cursorTuple->next.unshared->next;
							continue;
						}
						cursorTuple = cursorTuple->next.unshared;
					}
				}
			}
		}

		/* allow this loop to be cancellable */
		CHECK_FOR_INTERRUPTS();
	}
}


/*
 * ExecHash2SideTableInsert
 *		insert a tuple into the hash table depending on the hash value
 *		it may just go to a temp file for later batches
 *
 * Note: the passed TupleTableSlot may contain a regular, minimal, or virtual
 * tuple; the minimal case in particular is certain to happen while reloading
 * tuples from batch files.  We could save some cycles in the regular-tuple
 * case by not forcing the slot contents into minimal form; not clear if it's
 * worth the messiness required.
 */
bool
ExecHash2SideTableInsert(HashJoinTable hashtable,
						 TupleTableSlot *slot,
						 uint32 hashvalue,
						 Hash2SideState *node,
						 ShortestpathState *spstate,
						 long *saved)
{
	bool shouldFree;
	MinimalTuple tuple = ExecFetchSlotMinimalTuple(slot, &shouldFree);
	bool inserted =  ExecHash2SideTableInsertTuple(hashtable,
												   tuple,
												   hashvalue,
												   node,
												   spstate,
												   saved);

	if (shouldFree)
	{
		heap_free_minimal_tuple(tuple);
	}
	return inserted;
}

bool
ExecHash2SideTableInsertTuple(HashJoinTable hashtable,
							  MinimalTuple tuple,
							  uint32 hashvalue,
							  Hash2SideState *node,
							  ShortestpathState *spstate,
							  long *saved)
{
	int bucketno;
	int batchno;

	ExecHash2SideGetBucketAndBatch(hashtable, hashvalue,
								   &bucketno, &batchno);

	/*
	 * decide whether to put the tuple in the hash table or a temp file
	 */
	if (batchno == hashtable->curbatch)
	{
		/*
		 * put the tuple in hash table
		 */
		HashJoinTuple hashTuple;
		MinimalTuple body;
		int			hashTupleSize;
		double		ntuples = (hashtable->totalTuples - hashtable->skewTuples);

		if (tuple->t_len != sizeof(*tuple) + sizeof(Graphid) + spstate->sp_RowidSize)
		{
			for (hashTuple = hashtable->buckets.unshared[bucketno];
				 hashTuple != NULL;
				 hashTuple = hashTuple->next.unshared)
			{
				body = HJTUPLE_MINTUPLE(hashTuple);
				if ((spstate->limit == 1 ||
					 body->t_len == sizeof(*tuple) + sizeof(Graphid) + spstate->sp_RowidSize) &&
					*((Graphid*)(body+1)) == *((Graphid*)(tuple+1)))
				{
					return false;
				}
			}
		}

		/* Create the HashJoinTuple */
		hashTupleSize = HJTUPLE_OVERHEAD + tuple->t_len;
		hashTuple = (HashJoinTuple) dense_alloc(hashtable, hashTupleSize);

		hashTuple->hashvalue = hashvalue;
		memcpy(HJTUPLE_MINTUPLE(hashTuple), tuple, tuple->t_len);

		/*
		 * We always reset the tuple-matched flag on insertion.  This is okay
		 * even when reloading a tuple from a batch file, since the tuple
		 * could not possibly have been matched to an outer tuple before it
		 * went into the batch file.
		 */
		HeapTupleHeaderClearMatch(HJTUPLE_MINTUPLE(hashTuple));

		/* Push it onto the front of the bucket's list */
		hashTuple->next.unshared = hashtable->buckets.unshared[bucketno];
		hashtable->buckets.unshared[bucketno] = hashTuple;

		if (tuple->t_len == sizeof(*tuple) + sizeof(Graphid) + spstate->sp_RowidSize)
		{
			hashTuple = hashtable->buckets.unshared[bucketno];
			while (hashTuple->next.unshared != NULL)
			{
				body = HJTUPLE_MINTUPLE(hashTuple->next.unshared);
				if (*((Graphid*)(body+1)) == *((Graphid*)(tuple+1)))
				{
					hashtable->totalTuples -= 1;
					if (body->t_len != sizeof(*tuple) + sizeof(Graphid) + spstate->sp_RowidSize)
						node->totalPaths -= 1;
					*((Graphid*)(body+1)) = 0;
					hashTuple->next = hashTuple->next.unshared->next;
					continue;
				}
				hashTuple = hashTuple->next.unshared;
			}
		}

		/*
		 * Increase the (optimal) number of buckets if we just exceeded the
		 * NTUP_PER_BUCKET threshold, but only when there's still a single
		 * batch.
		 */
		if (hashtable->nbatch == 1 &&
			ntuples > (hashtable->nbuckets_optimal * NTUP_PER_BUCKET))
		{
			/* Guard against integer overflow and alloc size overflow */
			if (hashtable->nbuckets_optimal <= INT_MAX / 2 &&
				hashtable->nbuckets_optimal * 2 <= MaxAllocSize / sizeof(HashJoinTuple))
			{
				hashtable->nbuckets_optimal *= 2;
				hashtable->log2_nbuckets_optimal += 1;
			}
		}

		/* Account for space used, and back off if we've used too much */
		hashtable->spaceUsed += hashTupleSize;
		if (hashtable->spaceUsed > hashtable->spacePeak)
			hashtable->spacePeak = hashtable->spaceUsed;
		if (hashtable->spaceUsed +
			hashtable->nbuckets_optimal * sizeof(HashJoinTuple)
			> hashtable->spaceAllowed)
			ExecHash2SideIncreaseNumBatches(hashtable);
	}
	else
	{
		/*
		 * put the tuple into a temp file for later batches
		 */
		Assert(batchno > hashtable->curbatch);
		ExecShortestpathSaveTuple(tuple,
								  hashvalue,
								  &hashtable->innerBatchFile[batchno]);
		if (saved != NULL) (*saved)++;
	}

	return true;
}

bool
ExecHash2SideTableInsertGraphid(HashJoinTable hashtable,
								Graphid id,
								uint32 hashvalue,
								Hash2SideState *node,
								ShortestpathState *spstate,
								long *saved)
{
	*((Graphid*)(spstate->sp_GraphidTuple+1)) = id;

	return ExecHash2SideTableInsertTuple(hashtable,
										 spstate->sp_GraphidTuple,
										 hashvalue,
										 node,
										 spstate,
										 saved);
}

/*
 * ExecHash2SideGetBucketAndBatch
 *		Determine the bucket number and batch number for a hash value
 *
 * Note: on-the-fly increases of nbatch must not change the bucket number
 * for a given hash code (since we don't move tuples to different hash
 * chains), and must only cause the batch number to remain the same or
 * increase.  Our algorithm is
 *		bucketno = hashvalue MOD nbuckets
 *		batchno = (hashvalue DIV nbuckets) MOD nbatch
 * where nbuckets and nbatch are both expected to be powers of 2, so we can
 * do the computations by shifting and masking.  (This assumes that all hash
 * functions are good about randomizing all their output bits, else we are
 * likely to have very skewed bucket or batch occupancy.)
 *
 * nbuckets and log2_nbuckets may change while nbatch == 1 because of dynamic
 * bucket count growth.  Once we start batching, the value is fixed and does
 * not change over the course of the join (making it possible to compute batch
 * number the way we do here).
 *
 * nbatch is always a power of 2; we increase it only by doubling it.  This
 * effectively adds one more bit to the top of the batchno.
 */
void
ExecHash2SideGetBucketAndBatch(HashJoinTable hashtable,
							   uint32 hashvalue,
							   int *bucketno,
							   int *batchno)
{
	uint32		nbuckets = (uint32) hashtable->nbuckets;
	uint32		nbatch = (uint32) hashtable->nbatch;

	if (nbatch > 1)
	{
		/* we can do MOD by masking, DIV by shifting */
		*bucketno = hashvalue & (nbuckets - 1);
		*batchno = (hashvalue >> hashtable->log2_nbuckets) & (nbatch - 1);
	}
	else
	{
		*bucketno = hashvalue & (nbuckets - 1);
		*batchno = 0;
	}
}

/*
 * ExecScanHash2SideBucket
 *		scan a hash bucket for matches to the current outer tuple
 *
 * The current outer tuple must be stored in econtext->ecxt_outertuple.
 *
 * On success, the inner tuple is stored into spstate->sp_CurTuple and
 * econtext->ecxt_innertuple, using spstate->sp_HashTupleSlot as the slot
 * for the latter.
 */
bool
ExecScanHash2SideBucket(Hash2SideState *node,
						ShortestpathState *spstate,
						ExprContext *econtext)
{
	ExprState  *spclauses = spstate->hashclauses;
	HashJoinTable hashtable = spstate->sp_HashTable;
	HashJoinTuple hashTuple = spstate->sp_CurTuple;
	uint32		hashvalue = spstate->sp_CurHashValue;

	/*
	 * sp_CurTuple is the address of the tuple last returned from the current
	 * bucket, or NULL if it's time to start scanning a new bucket.
	 *
	 * If the tuple hashed to a skew bucket then scan the skew bucket
	 * otherwise scan the standard hashtable bucket.
	 */
	if (hashTuple != NULL)
		hashTuple = hashTuple->next.unshared;
	else
		hashTuple = hashtable->buckets.unshared[spstate->sp_CurBucketNo];

	while (hashTuple != NULL)
	{
		if (hashTuple->hashvalue == hashvalue)
		{
			TupleTableSlot *inntuple;
			MinimalTuple    tuple;

			tuple = HJTUPLE_MINTUPLE(hashTuple);
			/* insert hashtable's tuple into exec slot so ExecQual sees it */
			inntuple = ExecStoreMinimalTuple(tuple,
											 spstate->sp_HashTupleSlot,
											 false);	/* do not pfree */
			econtext->ecxt_innertuple = inntuple;

			/* reset temp memory each time to avoid leaks from qual expr */
			ResetExprContext(econtext);

			if ((node->hops < 1 ||
				 tuple->t_len != sizeof(*tuple) + sizeof(Graphid) + spstate->sp_RowidSize))
			{
				if (ExecQual(spclauses, econtext))
				{
					spstate->sp_CurTuple = hashTuple;
					return true;
				}
			}
		}

		hashTuple = hashTuple->next.unshared;
	}

	/*
	 * no match
	 */
	return false;
}

/*
 * ExecHash2SideTableReset
 *
 *		reset hash table header for new batch
 */
void
ExecHash2SideTableReset(HashJoinTable hashtable)
{
	MemoryContext oldcxt;
	int			nbuckets = hashtable->nbuckets;

	/*
	 * Release all the hash buckets and tuples acquired in the prior pass, and
	 * reinitialize the context for a new pass.
	 */
	MemoryContextReset(hashtable->batchCxt);
	oldcxt = MemoryContextSwitchTo(hashtable->batchCxt);

	/* Reallocate and reinitialize the hash bucket headers. */
	hashtable->buckets.unshared = (HashJoinTuple *)
			palloc0(nbuckets * sizeof(HashJoinTuple));

	hashtable->spaceUsed = 0;

	MemoryContextSwitchTo(oldcxt);

	/* Forget the chunks (the memory was freed by the context reset above). */
	hashtable->chunks = NULL;
}

void
ExecReScanHash2Side(Hash2SideState *node)
{
	if (node->keytable)
	{
		if (node->keytable->spacePeak > node->spacePeak)
			node->spacePeak = node->keytable->spacePeak;
		ExecHash2SideTableDestroy(node->keytable);
		node->keytable = NULL;
	}
	if (node->hashtable)
	{
		if (node->hashtable->spacePeak > node->spacePeak)
			node->spacePeak = node->hashtable->spacePeak;
		ExecHash2SideTableDestroy(node->hashtable);
		node->hashtable = NULL;
	}
	ExecReScan(node->ps.lefttree);
}

/*
 * Allocate 'size' bytes from the currently active HashMemoryChunk
 */
static void *
dense_alloc(HashJoinTable hashtable, Size size)
{
	HashMemoryChunk newChunk;
	char	   *ptr;

	/* just in case the size is not already aligned properly */
	size = MAXALIGN(size);

	/*
	 * If tuple size is larger than of 1/4 of chunk size, allocate a separate
	 * chunk.
	 */
	if (size > HASH_CHUNK_THRESHOLD)
	{
		/* allocate new chunk and put it at the beginning of the list */
		newChunk = (HashMemoryChunk) MemoryContextAlloc(hashtable->batchCxt,
														HASH_CHUNK_HEADER_SIZE + size);
		newChunk->maxlen = size;
		newChunk->used = 0;
		newChunk->ntuples = 0;

		/*
		 * Add this chunk to the list after the first existing chunk, so that
		 * we don't lose the remaining space in the "current" chunk.
		 */
		if (hashtable->chunks != NULL)
		{
			newChunk->next = hashtable->chunks->next;
			hashtable->chunks->next.unshared = newChunk;
		}
		else
		{
			newChunk->next.unshared = hashtable->chunks;
			hashtable->chunks = newChunk;
		}

		newChunk->used += size;
		newChunk->ntuples += 1;

		return HASH_CHUNK_DATA(newChunk);
	}

	/*
	 * See if we have enough space for it in the current chunk (if any). If
	 * not, allocate a fresh chunk.
	 */
	if ((hashtable->chunks == NULL) ||
		(hashtable->chunks->maxlen - hashtable->chunks->used) < size)
	{
		/* allocate new chunk and put it at the beginning of the list */
		newChunk = (HashMemoryChunk) MemoryContextAlloc(hashtable->batchCxt,
														HASH_CHUNK_HEADER_SIZE + HASH_CHUNK_SIZE);

		newChunk->maxlen = HASH_CHUNK_SIZE;
		newChunk->used = size;
		newChunk->ntuples = 1;

		newChunk->next.unshared = hashtable->chunks;
		hashtable->chunks = newChunk;

		return HASH_CHUNK_DATA(newChunk);
	}

	/* There is enough space in the current chunk, let's add the tuple */
	ptr = HASH_CHUNK_DATA(hashtable->chunks) + hashtable->chunks->used;
	hashtable->chunks->used += size;
	hashtable->chunks->ntuples += 1;

	/* return pointer to the start of the tuple memory */
	return ptr;
}

/*
 * Copy the instrumentation data from 'hashtable' into a HashInstrumentation
 * struct.
 */
void
ExecHash2SideGetInstrumentation(HashInstrumentation *instrument,
								HashJoinTable hashtable)
{
	instrument->nbuckets = hashtable->nbuckets;
	instrument->nbuckets_original = hashtable->nbuckets_original;
	instrument->nbatch = hashtable->nbatch;
	instrument->nbatch_original = hashtable->nbatch_original;
	instrument->space_peak = hashtable->spacePeak;
}