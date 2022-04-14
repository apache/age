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

#ifndef PG_COMPAT_H
#define PG_COMPAT_H

#define PG11_LT (PG_VERSION_NUM < 110000)
#define PG11_GE (PG_VERSION_NUM >= 110000)
#define PG12_LT (PG_VERSION_NUM < 120000)
#define PG12_GE (PG_VERSION_NUM >= 120000)
#define PG13_LT (PG_VERSION_NUM < 130000)
#define PG13_GE (PG_VERSION_NUM >= 130000)

#define PG11 (PG11_GE && PG12_LT)
#define PG12 (PG12_GE && PG13_LT)

#if PG12_GE
/* Here will used for PG12 and later versions. */
#define ExecInsertIndexTuplesCompat(slot, tupleid, estate, noDupErr, \
                                    specConflict, arbiterIndexes) \
    ExecInsertIndexTuples(slot, estate, noDupErr, specConflict, arbiterIndexes)

#define GetSysCacheOid2Compat GetSysCacheOid2
#define GetSysCacheOid3Compat GetSysCacheOid3

#define CreateTemplateTupleDescCompat CreateTemplateTupleDesc

#define addRangeTableEntryForRelationCompat addRangeTableEntryForRelation

#else
/* PG11 */
/*
 * Introduce access/{table.h, relation.h}, for generic functions from heapam.h.
 * 4b21acf522d751ba5b6679df391d5121b6c4a35f
 *
 * tableam: Add and use scan APIs.
 * c2fe139c201c48f1133e9fbea2dd99b8efe2fadd
 */
#define TableScanDesc HeapScanDesc

#define table_open(a, b) heap_open(a, b)
#define table_close(a, b) heap_close(a, b)

#define table_beginscan(a, b, c, d) heap_beginscan(a, b, c, d)
#define table_endscan(a) heap_endscan(a)

/*
 * tableam: Add tuple_{insert, delete, update, lock} and use.
 * 5db6df0c0117ff2a4e0cd87594d2db408cd5022f
 */
#define TM_FailureData HeapUpdateFailureData
#define TM_Result HTSU_Result

#define TM_Ok HeapTupleMayBeUpdated
#define TM_SelfModified HeapTupleSelfUpdated
#define TM_Updated HeapTupleUpdated
#define TM_Invisible HeapTupleInvisible

#define ExecInsertIndexTuplesCompat ExecInsertIndexTuples

/*
 * Split QTW_EXAMINE_RTES flag into QTW_EXAMINE_RTES_BEFORE/_AFTER.
 * 18c0da88a5d9da566c3bfac444366b73bd0b57da
 */
#define QTW_EXAMINE_RTES_BEFORE QTW_EXAMINE_RTES

/*
 * Rejigger materializing and fetching a HeapTuple from a slot.
 * 763f2edd92095b1ca2f4476da073a28505c13820
 */
#define ExecFetchSlotHeapTuple(slot, materialize, shouldFree) \
    ExecMaterializeSlot(slot)

/*
 * Split ExecStoreTuple into ExecStoreHeapTuple and ExecStoreBufferHeapTuple.
 * 29c94e03c7d05d2b29afa1de32795ce178531246
 */
#define ExecStoreHeapTuple(tuple, slot, shouldFree) \
    ExecStoreTuple(tuple, slot, InvalidBuffer, shouldFree)

/*
 * Remove WITH OIDS support, change oid catalog column visibility.
 * 578b229718e8f15fa779e20f086c4b6bb3776106
 */
#define GetSysCacheOid2Compat(cacheId, oidcol, key1, key2) \
    GetSysCacheOid2(cacheId, key1, key2)
#define GetSysCacheOid3Compat(cacheId, oidcol, key1, key2, key3) \
    GetSysCacheOid3(cacheId, key1, key2, key3)

#define CreateTemplateTupleDescCompat(natts) \
    CreateTemplateTupleDesc(natts, false)

/*
 * Create an RTE field to record the query's lock mode for each relation.
 * fdba460a26af919c0b366755d119f384706e670a
 */
#define addRangeTableEntryForRelationCompat(pstate, rel, lockmode, alias, inh, \
                                            inFromCl) \
    addRangeTableEntryForRelation(pstate, rel, alias, inh, inFromCl)
#endif

#endif //PG_COMPAT_H