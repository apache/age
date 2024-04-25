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

#include "utils/cypher_empty_funcs.h"
#include "utils/fmgrprotos.h"
#include "utils/jsonb.h"

Datum
cypher_to_jsonb(PG_FUNCTION_ARGS)
{
	return to_jsonb(fcinfo);
}

Datum
cypher_isempty_jsonb(PG_FUNCTION_ARGS)
{
	Jsonb	   *jb = PG_GETARG_JSONB_P(0);

	if (JB_ROOT_IS_SCALAR(jb))
	{
		JsonbValue *sjv;

		sjv = getIthJsonbValueFromContainer(&jb->root, 0);
		if (sjv->type == jbvString)
		{
			PG_RETURN_BOOL(sjv->val.string.len <= 0);
		}
	}
	else if (JB_ROOT_IS_ARRAY(jb) || JB_ROOT_IS_OBJECT(jb))
	{
		PG_RETURN_BOOL(JB_ROOT_COUNT(jb) <= 0);
	}

	ereport(ERROR,
			(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
			 errmsg("isEmpty(): list or object or string is expected but %s",
					JsonbToCString(NULL, &jb->root, VARSIZE(jb)))));
}
