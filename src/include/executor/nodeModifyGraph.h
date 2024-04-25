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

#ifndef NODEMODIFYGRAPH_H
#define NODEMODIFYGRAPH_H

#include "nodes/execnodes.h"

/* for visibility between Cypher clauses */
typedef enum ModifyCid
{
	MODIFY_CID_LOWER_BOUND,		/* for previous clause */
	MODIFY_CID_OUTPUT,			/* for CREATE, MERGE, DELETE */
	MODIFY_CID_SET,				/* for SET, ON MATCH SET, ON CREATE SET */
	MODIFY_CID_NLJOIN_MATCH,	/* for DELETE JOIN, MERGE JOIN */
	MODIFY_CID_MAX
} ModifyCid;

extern bool enable_multiple_update;

extern ModifyGraphState *ExecInitModifyGraph(ModifyGraph *mgplan,
											 EState *estate, int eflags);
extern void ExecEndModifyGraph(ModifyGraphState *mgstate);

#endif
