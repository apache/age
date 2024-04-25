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

#ifndef NODESHORTESTPATH_H
#define NODESHORTESTPATH_H

#include "nodes/execnodes.h"
#include "storage/buffile.h"

extern ShortestpathState *ExecInitShortestpath(Shortestpath *node, EState *estate, int eflags);
extern void ExecEndShortestpath(ShortestpathState *node);
extern void ExecReScanShortestpath(ShortestpathState *node);

extern void ExecShortestpathSaveTuple(MinimalTuple tuple, uint32 hashvalue,
									  BufFile **fileptr);
extern TupleTableSlot *ExecShortestpathGetSavedTuple(BufFile           *file,
													 uint32            *hashvalue,
													 TupleTableSlot    *tupleSlot);

#endif   /* NODESHORTESTPATH_H */
