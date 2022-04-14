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
#else
/* PG11 */
#define TableScanDesc HeapScanDesc

#define table_open(a, b) heap_open(a, b)
#define table_close(a, b) heap_close(a, b)

#define table_beginscan(a, b, c, d) heap_beginscan(a, b, c, d)
#define table_endscan(a) heap_endscan(a)

#define TM_FailureData HeapUpdateFailureData
#define TM_Result HTSU_Result

#define TM_Ok HeapTupleMayBeUpdated
#define TM_SelfModified HeapTupleSelfUpdated
#define TM_Updated HeapTupleUpdated
#define TM_Invisible HeapTupleInvisible

#define QTW_EXAMINE_RTES_BEFORE QTW_EXAMINE_RTES

#endif

#endif //PG_COMPAT_H