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

#ifndef AG_LABEL_FN_H
#define AG_LABEL_FN_H

#include "nodes/parsenodes.h"

extern Oid label_create_with_catalog(RangeVar *label, Oid relid, char labkind,
									 Oid labtablespace, bool is_fixed_id,
									 int32 fixed_id);
extern void label_drop_with_catalog(Oid laboid);

#endif	/* AG_LABEL_FN_H */
