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

#ifndef NODEHASH2SIDE_H
#define NODEHASH2SIDE_H

#include "nodes/execnodes.h"

extern Hash2SideState *ExecInitHash2Side(Hash2Side *node, EState *estate, int eflags);
extern Node *MultiExecHash2Side(Hash2SideState *node);
extern void ExecEndHash2Side(Hash2SideState *node);
extern void ExecReScanHash2Side(Hash2SideState *node);

extern HashJoinTable ExecHash2SideTableCreate(Hash2SideState *node, List *hashOperators,
											  double ntuples, double npaths, long hops, Size spacePeak);
extern HashJoinTable ExecHash2SideTableClone(Hash2SideState *node, List *hashOperators,
											 HashJoinTable sourcetable, Size spacePeak);
extern void ExecHash2SideTableDestroy(HashJoinTable hashtable);
extern void ExecHash2SideIncreaseNumBuckets(HashJoinTable hashtable, Hash2SideState *node);
extern bool ExecHash2SideTableInsert(HashJoinTable hashtable,
									 TupleTableSlot *slot,
									 uint32 hashvalue,
									 Hash2SideState *node,
									 ShortestpathState *spstate,
									 long *saved);
extern bool ExecHash2SideTableInsertTuple(HashJoinTable hashtable,
										  MinimalTuple tuple,
										  uint32 hashvalue,
										  Hash2SideState *node,
										  ShortestpathState *spstate,
										  long *saved);
extern bool ExecHash2SideTableInsertGraphid(HashJoinTable hashtable,
											Graphid id,
											uint32 hashvalue,
											Hash2SideState *node,
											ShortestpathState *spstate,
											long *saved);
extern void ExecHash2SideGetBucketAndBatch(HashJoinTable hashtable,
										   uint32 hashvalue,
										   int *bucketno,
										   int *batchno);
extern bool ExecScanHash2SideBucket(Hash2SideState *node,
									ShortestpathState *spstate,
									ExprContext *econtext);
extern void ExecHash2SideTableReset(HashJoinTable hashtable);
extern void ExecChooseHash2SideTableSize(double ntuples,
										 double npaths,
										 int tupwidth,
										 long hops,
										 int *numbuckets,
										 int *numbatches);

extern void ExecHash2SideGetInstrumentation(HashInstrumentation *instrument,
											HashJoinTable hashtable);

#endif   /* NODEHASH2SIDE_H */
