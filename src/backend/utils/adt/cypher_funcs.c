/*
 * cypher_funcs.c
 *	  Functions in Cypher expressions.
 *
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
 *
 * IDENTIFICATION
 *	  src/backend/utils/adt/cypher_funcs.c
 */

#include "postgres.h"

#include "access/attnum.h"
#include "access/htup.h"
#include "access/htup_details.h"
#include "access/tupdesc.h"
#include "catalog/pg_collation.h"
#include "catalog/pg_type.h"
#include "funcapi.h"
#include "utils/arrayaccess.h"
#include "utils/builtins.h"
#include "utils/timestamp.h"
#include "utils/cypher_funcs.h"
#include "utils/jsonb.h"
#include "utils/memutils.h"
#include "utils/typcache.h"
#include "parser/scansup.h"
#include "utils/date.h"
#include "utils/datetime.h"
#include "pgtime.h"
#include <string.h>
#include <math.h>

/* global variable - see postgres.c*/
extern GraphWriteStats graphWriteStats;

#define FUNC_JSONB_MAX_ARGS 3

#define AARR_FREE_IF_COPY(array,n) \
	do { \
		if (!VARATT_IS_EXPANDED_HEADER(array)) \
			PG_FREE_IF_COPY(array, n); \
	} while (0)


typedef struct FunctionCallJsonbInfo
{
	PGFunction	fn;
	const char *funcname;
	int			nargs;
	Jsonb	   *args[FUNC_JSONB_MAX_ARGS];
	Oid			argtypes[FUNC_JSONB_MAX_ARGS];
	Oid			rettype;
} FunctionCallJsonbInfo;

static Jsonb *FunctionCallJsonb(FunctionCallJsonbInfo *fcjinfo);
static Datum jsonb_to_datum(Jsonb *j, Oid type);
static bool is_numeric_integer(Numeric n);
static void ereport_invalid_jsonb_param(FunctionCallJsonbInfo *fcjinfo);
static char *type_to_jsonb_type_str(Oid type);
static Jsonb *datum_to_jsonb(Datum d, Oid type);
static bool int_to_bool(int32 num, bool *result);
static bool string_to_bool(const char *str, bool *result);
static Datum range(int start, int end, int step);
static Datum datum_to_text(Datum d, Oid typeid);
static Datum datum_to_integer(Datum d, Oid typeid);
static Datum datum_to_float(Datum d, Oid typeid);
static int32 string_to_int(char* s);
static float4 string_to_float(char* s);
static void get_cstring_substr(char *c, char *res, int32 start, int32 len);
static Timestamp dt2local(Timestamp dt, int timezone);
static Timestamp get_timestamp_for_timezone(text* zone, TimestampTz timestamp);

Datum
jsonb_head(PG_FUNCTION_ARGS)
{
	Jsonb	   *j = PG_GETARG_JSONB_P(0);
	JsonbValue *jv;

	if (!JB_ROOT_IS_ARRAY(j) || JB_ROOT_IS_SCALAR(j))
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
				 errmsg("head(): list is expected but %s",
						JsonbToCString(NULL, &j->root, VARSIZE(j)))));

	jv = getIthJsonbValueFromContainer(&j->root, 0);

	if (jv == NULL)
		PG_RETURN_NULL();

	PG_RETURN_JSONB_P(JsonbValueToJsonb(jv));
}

Datum
jsonb_last(PG_FUNCTION_ARGS)
{
	Jsonb	   *j = PG_GETARG_JSONB_P(0);
	uint32		cnt;
	JsonbValue *jv;

	if (!JB_ROOT_IS_ARRAY(j) || JB_ROOT_IS_SCALAR(j))
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
				 errmsg("last(): list is expected but %s",
						JsonbToCString(NULL, &j->root, VARSIZE(j)))));

	cnt = JB_ROOT_COUNT(j);
	if (cnt == 0)
		PG_RETURN_NULL();

	jv = getIthJsonbValueFromContainer(&j->root, cnt - 1);

	PG_RETURN_JSONB_P(JsonbValueToJsonb(jv));
}

Datum
jsonb_length(PG_FUNCTION_ARGS)
{
	Jsonb	   *j = PG_GETARG_JSONB_P(0);
	int			cnt = -1;
	Datum		n;
	JsonbValue	jv;

	if (JB_ROOT_IS_SCALAR(j))
	{
		JsonbValue *sjv;

		sjv = getIthJsonbValueFromContainer(&j->root, 0);
		if (sjv->type == jbvString)
			cnt = sjv->val.string.len;
	}
	else if (JB_ROOT_IS_ARRAY(j))
	{
		cnt = (int) JB_ROOT_COUNT(j);
	}

	if (cnt < 0)
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
				 errmsg("length(): list or string is expected but %s",
						JsonbToCString(NULL, &j->root, VARSIZE(j)))));

	n = DirectFunctionCall1(int4_numeric, Int32GetDatum(cnt));
	jv.type = jbvNumeric;
	jv.val.numeric = DatumGetNumeric(n);

	PG_RETURN_JSONB_P(JsonbValueToJsonb(&jv));
}

Datum
jsonb_toboolean(PG_FUNCTION_ARGS)
{
	Jsonb	   *j = PG_GETARG_JSONB_P(0);

	if (JB_ROOT_IS_SCALAR(j))
	{
		JsonbValue *jv;

		jv = getIthJsonbValueFromContainer(&j->root, 0);
		if (jv->type == jbvString)
		{
			if (jv->val.string.len == 4 &&
				pg_strncasecmp(jv->val.string.val, "true", 4) == 0)
				PG_RETURN_BOOL(true);
			else if (jv->val.string.len == 5 &&
					 pg_strncasecmp(jv->val.string.val, "false", 5) == 0)
				PG_RETURN_BOOL(false);
			else
				PG_RETURN_NULL();
		}
	}

	ereport(ERROR,
			(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
			 errmsg("toBoolean(): string is expected but %s",
					JsonbToCString(NULL, &j->root, VARSIZE(j)))));
	PG_RETURN_NULL();
}

Datum
jsonb_keys(PG_FUNCTION_ARGS)
{
	Jsonb	   *j = PG_GETARG_JSONB_P(0);
	JsonbParseState *jpstate = NULL;
	JsonbIterator *it;
	JsonbValue	jv;
	JsonbIteratorToken tok;
	JsonbValue *ajv;

	if (!JB_ROOT_IS_OBJECT(j))
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
				 errmsg("keys(): map is expected but %s",
						JsonbToCString(NULL, &j->root, VARSIZE(j)))));

	pushJsonbValue(&jpstate, WJB_BEGIN_ARRAY, NULL);

	it = JsonbIteratorInit(&j->root);
	tok = JsonbIteratorNext(&it, &jv, false);
	while (tok != WJB_DONE)
	{
		if (tok == WJB_KEY)
			pushJsonbValue(&jpstate, WJB_ELEM, &jv);

		tok = JsonbIteratorNext(&it, &jv, true);
	}

	ajv = pushJsonbValue(&jpstate, WJB_END_ARRAY, NULL);

	PG_RETURN_JSONB_P(JsonbValueToJsonb(ajv));
}

Datum
jsonb_tail(PG_FUNCTION_ARGS)
{
	Jsonb	   *j = PG_GETARG_JSONB_P(0);
	JsonbParseState *jpstate = NULL;
	JsonbValue *ajv;

	if (!JB_ROOT_IS_ARRAY(j) || JB_ROOT_IS_SCALAR(j))
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
				 errmsg("tail(): list is expected but %s",
						JsonbToCString(NULL, &j->root, VARSIZE(j)))));

	pushJsonbValue(&jpstate, WJB_BEGIN_ARRAY, NULL);

	if (JB_ROOT_COUNT(j) > 1)
	{
		JsonbIterator *it;
		JsonbValue	jv;
		JsonbIteratorToken tok;
		bool		push = false;

		it = JsonbIteratorInit(&j->root);
		tok = JsonbIteratorNext(&it, &jv, false);
		while (tok != WJB_DONE)
		{
			if (tok == WJB_ELEM)
			{
				if (push)
					pushJsonbValue(&jpstate, WJB_ELEM, &jv);
				else
					push = true;
			}

			tok = JsonbIteratorNext(&it, &jv, true);
		}
	}

	ajv = pushJsonbValue(&jpstate, WJB_END_ARRAY, NULL);

	PG_RETURN_JSONB_P(JsonbValueToJsonb(ajv));
}

Datum
jsonb_abs(PG_FUNCTION_ARGS)
{
	FunctionCallJsonbInfo fcjinfo;

	fcjinfo.fn = numeric_abs;
	fcjinfo.funcname = "abs";
	fcjinfo.nargs = 1;
	fcjinfo.args[0] = PG_GETARG_JSONB_P(0);
	fcjinfo.argtypes[0] = NUMERICOID;
	fcjinfo.rettype = NUMERICOID;

	PG_RETURN_JSONB_P(FunctionCallJsonb(&fcjinfo));
}

Datum
jsonb_ceil(PG_FUNCTION_ARGS)
{
	FunctionCallJsonbInfo fcjinfo;

	fcjinfo.fn = numeric_ceil;
	fcjinfo.funcname = "ceil";
	fcjinfo.nargs = 1;
	fcjinfo.args[0] = PG_GETARG_JSONB_P(0);
	fcjinfo.argtypes[0] = NUMERICOID;
	fcjinfo.rettype = NUMERICOID;

	PG_RETURN_JSONB_P(FunctionCallJsonb(&fcjinfo));
}

Datum
jsonb_floor(PG_FUNCTION_ARGS)
{
	FunctionCallJsonbInfo fcjinfo;

	fcjinfo.fn = numeric_floor;
	fcjinfo.funcname = "floor";
	fcjinfo.nargs = 1;
	fcjinfo.args[0] = PG_GETARG_JSONB_P(0);
	fcjinfo.argtypes[0] = NUMERICOID;
	fcjinfo.rettype = NUMERICOID;

	PG_RETURN_JSONB_P(FunctionCallJsonb(&fcjinfo));
}

Datum
jsonb_rand(PG_FUNCTION_ARGS)
{
	FunctionCallJsonbInfo fcjinfo;

	fcjinfo.fn = drandom;
	fcjinfo.funcname = "rand";
	fcjinfo.nargs = 0;
	fcjinfo.rettype = FLOAT8OID;

	PG_RETURN_JSONB_P(FunctionCallJsonb(&fcjinfo));
}

Datum
jsonb_round(PG_FUNCTION_ARGS)
{
	Jsonb	   *j = PG_GETARG_JSONB_P(0);

	if (JB_ROOT_IS_SCALAR(j))
	{
		JsonbValue *jv;

		jv = getIthJsonbValueFromContainer(&j->root, 0);
		if (jv->type == jbvNumeric)
		{
			Datum		n;
			JsonbValue	njv;

			n = DirectFunctionCall2(numeric_round,
									NumericGetDatum(jv->val.numeric),
									Int32GetDatum(0));

			njv.type = jbvNumeric;
			njv.val.numeric = DatumGetNumeric(n);

			PG_RETURN_JSONB_P(JsonbValueToJsonb(&njv));
		}
	}

	ereport(ERROR,
			(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
			 errmsg("round(): number is expected but %s",
					JsonbToCString(NULL, &j->root, VARSIZE(j)))));
	PG_RETURN_NULL();
}

Datum
jsonb_sign(PG_FUNCTION_ARGS)
{
	FunctionCallJsonbInfo fcjinfo;

	fcjinfo.fn = numeric_sign;
	fcjinfo.funcname = "sign";
	fcjinfo.nargs = 1;
	fcjinfo.args[0] = PG_GETARG_JSONB_P(0);
	fcjinfo.argtypes[0] = NUMERICOID;
	fcjinfo.rettype = NUMERICOID;

	PG_RETURN_JSONB_P(FunctionCallJsonb(&fcjinfo));
}

Datum
jsonb_exp(PG_FUNCTION_ARGS)
{
	FunctionCallJsonbInfo fcjinfo;

	fcjinfo.fn = numeric_exp;
	fcjinfo.funcname = "exp";
	fcjinfo.nargs = 1;
	fcjinfo.args[0] = PG_GETARG_JSONB_P(0);
	fcjinfo.argtypes[0] = NUMERICOID;
	fcjinfo.rettype = NUMERICOID;

	PG_RETURN_JSONB_P(FunctionCallJsonb(&fcjinfo));
}

Datum
jsonb_log(PG_FUNCTION_ARGS)
{
	FunctionCallJsonbInfo fcjinfo;

	fcjinfo.fn = numeric_ln;
	fcjinfo.funcname = "log";
	fcjinfo.nargs = 1;
	fcjinfo.args[0] = PG_GETARG_JSONB_P(0);
	fcjinfo.argtypes[0] = NUMERICOID;
	fcjinfo.rettype = NUMERICOID;

	PG_RETURN_JSONB_P(FunctionCallJsonb(&fcjinfo));
}

Datum
jsonb_sqrt(PG_FUNCTION_ARGS)
{
	FunctionCallJsonbInfo fcjinfo;

	fcjinfo.fn = numeric_sqrt;
	fcjinfo.funcname = "sqrt";
	fcjinfo.nargs = 1;
	fcjinfo.args[0] = PG_GETARG_JSONB_P(0);
	fcjinfo.argtypes[0] = NUMERICOID;
	fcjinfo.rettype = NUMERICOID;

	PG_RETURN_JSONB_P(FunctionCallJsonb(&fcjinfo));
}

Datum
jsonb_acos(PG_FUNCTION_ARGS)
{
	FunctionCallJsonbInfo fcjinfo;

	fcjinfo.fn = dacos;
	fcjinfo.funcname = "acos";
	fcjinfo.nargs = 1;
	fcjinfo.args[0] = PG_GETARG_JSONB_P(0);
	fcjinfo.argtypes[0] = FLOAT8OID;
	fcjinfo.rettype = FLOAT8OID;

	PG_RETURN_JSONB_P(FunctionCallJsonb(&fcjinfo));
}

Datum
jsonb_asin(PG_FUNCTION_ARGS)
{
	FunctionCallJsonbInfo fcjinfo;

	fcjinfo.fn = dasin;
	fcjinfo.funcname = "asin";
	fcjinfo.nargs = 1;
	fcjinfo.args[0] = PG_GETARG_JSONB_P(0);
	fcjinfo.argtypes[0] = FLOAT8OID;
	fcjinfo.rettype = FLOAT8OID;

	PG_RETURN_JSONB_P(FunctionCallJsonb(&fcjinfo));
}

Datum
jsonb_atan(PG_FUNCTION_ARGS)
{
	FunctionCallJsonbInfo fcjinfo;

	fcjinfo.fn = datan;
	fcjinfo.funcname = "atan";
	fcjinfo.nargs = 1;
	fcjinfo.args[0] = PG_GETARG_JSONB_P(0);
	fcjinfo.argtypes[0] = FLOAT8OID;
	fcjinfo.rettype = FLOAT8OID;

	PG_RETURN_JSONB_P(FunctionCallJsonb(&fcjinfo));
}

Datum
jsonb_atan2(PG_FUNCTION_ARGS)
{
	FunctionCallJsonbInfo fcjinfo;

	fcjinfo.fn = datan2;
	fcjinfo.funcname = "atan2";
	fcjinfo.nargs = 2;
	fcjinfo.args[0] = PG_GETARG_JSONB_P(0);
	fcjinfo.argtypes[0] = FLOAT8OID;
	fcjinfo.args[1] = PG_GETARG_JSONB_P(1);
	fcjinfo.argtypes[1] = FLOAT8OID;
	fcjinfo.rettype = FLOAT8OID;

	PG_RETURN_JSONB_P(FunctionCallJsonb(&fcjinfo));
}

Datum
jsonb_cos(PG_FUNCTION_ARGS)
{
	FunctionCallJsonbInfo fcjinfo;

	fcjinfo.fn = dcos;
	fcjinfo.funcname = "cos";
	fcjinfo.nargs = 1;
	fcjinfo.args[0] = PG_GETARG_JSONB_P(0);
	fcjinfo.argtypes[0] = FLOAT8OID;
	fcjinfo.rettype = FLOAT8OID;

	PG_RETURN_JSONB_P(FunctionCallJsonb(&fcjinfo));
}

Datum
jsonb_cot(PG_FUNCTION_ARGS)
{
	FunctionCallJsonbInfo fcjinfo;

	fcjinfo.fn = dcot;
	fcjinfo.funcname = "cot";
	fcjinfo.nargs = 1;
	fcjinfo.args[0] = PG_GETARG_JSONB_P(0);
	fcjinfo.argtypes[0] = FLOAT8OID;
	fcjinfo.rettype = FLOAT8OID;

	PG_RETURN_JSONB_P(FunctionCallJsonb(&fcjinfo));
}

Datum
jsonb_degrees(PG_FUNCTION_ARGS)
{
	FunctionCallJsonbInfo fcjinfo;

	fcjinfo.fn = degrees;
	fcjinfo.funcname = "degrees";
	fcjinfo.nargs = 1;
	fcjinfo.args[0] = PG_GETARG_JSONB_P(0);
	fcjinfo.argtypes[0] = FLOAT8OID;
	fcjinfo.rettype = FLOAT8OID;

	PG_RETURN_JSONB_P(FunctionCallJsonb(&fcjinfo));
}

Datum
jsonb_radians(PG_FUNCTION_ARGS)
{
	FunctionCallJsonbInfo fcjinfo;

	fcjinfo.fn = radians;
	fcjinfo.funcname = "radians";
	fcjinfo.nargs = 1;
	fcjinfo.args[0] = PG_GETARG_JSONB_P(0);
	fcjinfo.argtypes[0] = FLOAT8OID;
	fcjinfo.rettype = FLOAT8OID;

	PG_RETURN_JSONB_P(FunctionCallJsonb(&fcjinfo));
}

Datum
jsonb_sin(PG_FUNCTION_ARGS)
{
	FunctionCallJsonbInfo fcjinfo;

	fcjinfo.fn = dsin;
	fcjinfo.funcname = "sin";
	fcjinfo.nargs = 1;
	fcjinfo.args[0] = PG_GETARG_JSONB_P(0);
	fcjinfo.argtypes[0] = FLOAT8OID;
	fcjinfo.rettype = FLOAT8OID;

	PG_RETURN_JSONB_P(FunctionCallJsonb(&fcjinfo));
}

Datum
jsonb_tan(PG_FUNCTION_ARGS)
{
	FunctionCallJsonbInfo fcjinfo;

	fcjinfo.fn = dtan;
	fcjinfo.funcname = "tan";
	fcjinfo.nargs = 1;
	fcjinfo.args[0] = PG_GETARG_JSONB_P(0);
	fcjinfo.argtypes[0] = FLOAT8OID;
	fcjinfo.rettype = FLOAT8OID;

	PG_RETURN_JSONB_P(FunctionCallJsonb(&fcjinfo));
}

Datum
jsonb_left(PG_FUNCTION_ARGS)
{
	FunctionCallJsonbInfo fcjinfo;

	fcjinfo.fn = text_left;
	fcjinfo.funcname = "left";
	fcjinfo.nargs = 2;
	fcjinfo.args[0] = PG_GETARG_JSONB_P(0);
	fcjinfo.argtypes[0] = TEXTOID;
	fcjinfo.args[1] = PG_GETARG_JSONB_P(1);
	fcjinfo.argtypes[1] = INT4OID;
	fcjinfo.rettype = TEXTOID;

	PG_RETURN_JSONB_P(FunctionCallJsonb(&fcjinfo));
}

Datum
jsonb_ltrim(PG_FUNCTION_ARGS)
{
	FunctionCallJsonbInfo fcjinfo;

	fcjinfo.fn = ltrim1;
	fcjinfo.funcname = "lTrim";
	fcjinfo.nargs = 1;
	fcjinfo.args[0] = PG_GETARG_JSONB_P(0);
	fcjinfo.argtypes[0] = TEXTOID;
	fcjinfo.rettype = TEXTOID;

	PG_RETURN_JSONB_P(FunctionCallJsonb(&fcjinfo));
}

Datum
jsonb_replace(PG_FUNCTION_ARGS)
{
	FunctionCallJsonbInfo fcjinfo;

	fcjinfo.fn = replace_text;
	fcjinfo.funcname = "replace";
	fcjinfo.nargs = 3;
	fcjinfo.args[0] = PG_GETARG_JSONB_P(0);
	fcjinfo.argtypes[0] = TEXTOID;
	fcjinfo.args[1] = PG_GETARG_JSONB_P(1);
	fcjinfo.argtypes[1] = TEXTOID;
	fcjinfo.args[2] = PG_GETARG_JSONB_P(2);
	fcjinfo.argtypes[2] = TEXTOID;
	fcjinfo.rettype = TEXTOID;

	PG_RETURN_JSONB_P(FunctionCallJsonb(&fcjinfo));
}

Datum
jsonb_reverse(PG_FUNCTION_ARGS)
{
	FunctionCallJsonbInfo fcjinfo;

	fcjinfo.fn = text_reverse;
	fcjinfo.funcname = "reverse";
	fcjinfo.nargs = 1;
	fcjinfo.args[0] = PG_GETARG_JSONB_P(0);
	fcjinfo.argtypes[0] = TEXTOID;
	fcjinfo.rettype = TEXTOID;

	PG_RETURN_JSONB_P(FunctionCallJsonb(&fcjinfo));
}

Datum
jsonb_right(PG_FUNCTION_ARGS)
{
	FunctionCallJsonbInfo fcjinfo;

	fcjinfo.fn = text_right;
	fcjinfo.funcname = "right";
	fcjinfo.nargs = 2;
	fcjinfo.args[0] = PG_GETARG_JSONB_P(0);
	fcjinfo.argtypes[0] = TEXTOID;
	fcjinfo.args[1] = PG_GETARG_JSONB_P(1);
	fcjinfo.argtypes[1] = INT4OID;
	fcjinfo.rettype = TEXTOID;

	PG_RETURN_JSONB_P(FunctionCallJsonb(&fcjinfo));
}

Datum
jsonb_rtrim(PG_FUNCTION_ARGS)
{
	FunctionCallJsonbInfo fcjinfo;

	fcjinfo.fn = rtrim1;
	fcjinfo.funcname = "rTrim";
	fcjinfo.nargs = 1;
	fcjinfo.args[0] = PG_GETARG_JSONB_P(0);
	fcjinfo.argtypes[0] = TEXTOID;
	fcjinfo.rettype = TEXTOID;

	PG_RETURN_JSONB_P(FunctionCallJsonb(&fcjinfo));
}

Datum
jsonb_substr_no_len(PG_FUNCTION_ARGS)
{
	Jsonb	   *sj = PG_GETARG_JSONB_P(0);
	Jsonb	   *ij = PG_GETARG_JSONB_P(1);

	if (JB_ROOT_IS_SCALAR(sj) && JB_ROOT_IS_SCALAR(ij))
	{
		JsonbValue *sjv;
		JsonbValue *ijv;

		sjv = getIthJsonbValueFromContainer(&sj->root, 0);
		ijv = getIthJsonbValueFromContainer(&ij->root, 0);

		if (sjv->type == jbvString &&
			ijv->type == jbvNumeric && is_numeric_integer(ijv->val.numeric))
		{
			text	   *s;
			Datum		i;
			Datum		r;
			JsonbValue	rjv;

			s = cstring_to_text_with_len(sjv->val.string.val,
										 sjv->val.string.len);
			i = DirectFunctionCall1(numeric_int4,
									NumericGetDatum(ijv->val.numeric));
			i = Int32GetDatum(DatumGetInt32(i) + 1);

			r = DirectFunctionCall2(text_substr_no_len, PointerGetDatum(s), i);

			rjv.type = jbvString;
			rjv.val.string.val = TextDatumGetCString(r);
			rjv.val.string.len = strlen(rjv.val.string.val);

			PG_RETURN_JSONB_P(JsonbValueToJsonb(&rjv));
		}
	}

	ereport(ERROR,
			(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
			 errmsg("substring(): string, number is expected but %s, %s",
					JsonbToCString(NULL, &sj->root, VARSIZE(sj)),
					JsonbToCString(NULL, &ij->root, VARSIZE(ij)))));
	PG_RETURN_NULL();
}

Datum
jsonb_substr(PG_FUNCTION_ARGS)
{
	Jsonb	   *sj = PG_GETARG_JSONB_P(0);
	Jsonb	   *ij = PG_GETARG_JSONB_P(1);
	Jsonb	   *lj = PG_GETARG_JSONB_P(2);

	if (JB_ROOT_IS_SCALAR(sj) &&
		JB_ROOT_IS_SCALAR(ij) && JB_ROOT_IS_SCALAR(lj))
	{
		JsonbValue *sjv;
		JsonbValue *ijv;
		JsonbValue *ljv;

		sjv = getIthJsonbValueFromContainer(&sj->root, 0);
		ijv = getIthJsonbValueFromContainer(&ij->root, 0);
		ljv = getIthJsonbValueFromContainer(&lj->root, 0);

		if (sjv->type == jbvString &&
			ijv->type == jbvNumeric && is_numeric_integer(ijv->val.numeric) &&
			ljv->type == jbvNumeric && is_numeric_integer(ljv->val.numeric))
		{
			text	   *s;
			Datum		i;
			Datum		l;
			Datum		r;
			JsonbValue	rjv;

			s = cstring_to_text_with_len(sjv->val.string.val,
										 sjv->val.string.len);
			i = DirectFunctionCall1(numeric_int4,
									NumericGetDatum(ijv->val.numeric));
			i = Int32GetDatum(DatumGetInt32(i) + 1);
			l = DirectFunctionCall1(numeric_int4,
									NumericGetDatum(ljv->val.numeric));

			r = DirectFunctionCall3(text_substr, PointerGetDatum(s), i, l);

			rjv.type = jbvString;
			rjv.val.string.val = TextDatumGetCString(r);
			rjv.val.string.len = strlen(rjv.val.string.val);

			PG_RETURN_JSONB_P(JsonbValueToJsonb(&rjv));
		}
	}

	ereport(ERROR,
			(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
			 errmsg("substring(): string, number, number is expected "
					"but %s, %s, %s",
					JsonbToCString(NULL, &sj->root, VARSIZE(sj)),
					JsonbToCString(NULL, &ij->root, VARSIZE(ij)),
					JsonbToCString(NULL, &lj->root, VARSIZE(lj)))));
	PG_RETURN_NULL();
}

Datum
jsonb_tolower(PG_FUNCTION_ARGS)
{
	FunctionCallJsonbInfo fcjinfo;

	fcjinfo.fn = lower;
	fcjinfo.funcname = "toLower";
	fcjinfo.nargs = 1;
	fcjinfo.args[0] = PG_GETARG_JSONB_P(0);
	fcjinfo.argtypes[0] = TEXTOID;
	fcjinfo.rettype = TEXTOID;

	PG_RETURN_JSONB_P(FunctionCallJsonb(&fcjinfo));
}

Datum
jsonb_tostring(PG_FUNCTION_ARGS)
{
	Jsonb	   *j = PG_GETARG_JSONB_P(0);

	if (JB_ROOT_IS_SCALAR(j))
	{
		JsonbValue *jv;
		JsonbValue	sjv;

		jv = getIthJsonbValueFromContainer(&j->root, 0);
		if (jv->type == jbvString)
		{
			PG_RETURN_JSONB_P(j);
		}
		else if (jv->type == jbvNumeric)
		{
			Datum		s;

			s = DirectFunctionCall1(numeric_out,
									NumericGetDatum(jv->val.numeric));

			sjv.type = jbvString;
			sjv.val.string.val = DatumGetCString(s);
			sjv.val.string.len = strlen(sjv.val.string.val);

			PG_RETURN_JSONB_P(JsonbValueToJsonb(&sjv));
		}
		else if (jv->type == jbvBool)
		{
			sjv.type = jbvString;

			if (jv->val.boolean)
			{
				sjv.val.string.len = 4;
				sjv.val.string.val = "true";
			}
			else
			{
				sjv.val.string.len = 5;
				sjv.val.string.val = "false";
			}

			PG_RETURN_JSONB_P(JsonbValueToJsonb(&sjv));
		}
	}

	ereport(ERROR,
			(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
			 errmsg("toString(): string, number, or bool is expected but %s",
					JsonbToCString(NULL, &j->root, VARSIZE(j)))));
	PG_RETURN_NULL();
}

Datum
jsonb_toupper(PG_FUNCTION_ARGS)
{
	FunctionCallJsonbInfo fcjinfo;

	fcjinfo.fn = upper;
	fcjinfo.funcname = "toUpper";
	fcjinfo.nargs = 1;
	fcjinfo.args[0] = PG_GETARG_JSONB_P(0);
	fcjinfo.argtypes[0] = TEXTOID;
	fcjinfo.rettype = TEXTOID;

	PG_RETURN_JSONB_P(FunctionCallJsonb(&fcjinfo));
}

Datum
jsonb_trim(PG_FUNCTION_ARGS)
{
	FunctionCallJsonbInfo fcjinfo;

	fcjinfo.fn = btrim1;
	fcjinfo.funcname = "trim";
	fcjinfo.nargs = 1;
	fcjinfo.args[0] = PG_GETARG_JSONB_P(0);
	fcjinfo.argtypes[0] = TEXTOID;
	fcjinfo.rettype = TEXTOID;

	PG_RETURN_JSONB_P(FunctionCallJsonb(&fcjinfo));
}

static Jsonb *
FunctionCallJsonb(FunctionCallJsonbInfo *fcjinfo)
{
	FunctionCallInfo fcinfo;
	int			i;
	Datum		datum;
	Jsonb	   *result;

	if (fcjinfo->nargs > FUNC_JSONB_MAX_ARGS)
	{
		elog(ERROR, "unexpected number of arguments: %d", fcjinfo->nargs);
	}

	fcinfo = palloc(SizeForFunctionCallInfo(fcjinfo->nargs));
	InitFunctionCallInfoData(*fcinfo, NULL, fcjinfo->nargs,
							 DEFAULT_COLLATION_OID, NULL, NULL);

	for (i = 0; i < fcjinfo->nargs; i++)
	{
		if (!JB_ROOT_IS_SCALAR(fcjinfo->args[i]))
			ereport_invalid_jsonb_param(fcjinfo);

		fcinfo->args[i].value = jsonb_to_datum(fcjinfo->args[i], fcjinfo->argtypes[i]);
		fcinfo->args[i].isnull = false;
	}

	datum = (*fcjinfo->fn) (fcinfo);
	result = datum_to_jsonb(datum, fcjinfo->rettype);

	pfree(fcinfo);
	return result;
}

static Datum
jsonb_to_datum(Jsonb *j, Oid type)
{
	JsonbValue *jv;
	Datum		retval = 0;

	jv = getIthJsonbValueFromContainer(&j->root, 0);

	switch (type)
	{
		case INT4OID:
			if (jv->type == jbvNumeric && is_numeric_integer(jv->val.numeric))
				retval = DirectFunctionCall1(numeric_int4,
											 NumericGetDatum(jv->val.numeric));
			break;
		case TEXTOID:
			if (jv->type == jbvString)
			{
				text	   *t;

				t = cstring_to_text_with_len(jv->val.string.val,
											 jv->val.string.len);
				retval = PointerGetDatum(t);
			}
			break;
		case FLOAT8OID:
			if (jv->type == jbvNumeric)
				retval = DirectFunctionCall1(numeric_float8,
											 NumericGetDatum(jv->val.numeric));
			break;
		case NUMERICOID:
			if (jv->type == jbvNumeric)
				retval = NumericGetDatum(jv->val.numeric);
			break;
		default:
			elog(ERROR, "unexpected type: %s", format_type_be(type));
	}

	return retval;
}

static bool
is_numeric_integer(Numeric n)
{
	int32		scale;

	scale = DatumGetInt32(DirectFunctionCall1(numeric_scale,
											  NumericGetDatum(n)));

	PG_RETURN_BOOL(scale <= 0);
}

static void
ereport_invalid_jsonb_param(FunctionCallJsonbInfo *fcjinfo)
{
	switch (fcjinfo->nargs)
	{
		case 1:
			ereport(ERROR,
					(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
					 errmsg("%s(): %s is expected but %s",
							fcjinfo->funcname,
							type_to_jsonb_type_str(fcjinfo->argtypes[0]),
							JsonbToCString(NULL, &fcjinfo->args[0]->root,
										   VARSIZE(fcjinfo->args[0])))));
			break;
		case 2:
			ereport(ERROR,
					(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
					 errmsg("%s(): %s, %s is expected but %s, %s",
							fcjinfo->funcname,
							type_to_jsonb_type_str(fcjinfo->argtypes[0]),
							type_to_jsonb_type_str(fcjinfo->argtypes[1]),
							JsonbToCString(NULL, &fcjinfo->args[0]->root,
										   VARSIZE(fcjinfo->args[0])),
							JsonbToCString(NULL, &fcjinfo->args[1]->root,
										   VARSIZE(fcjinfo->args[1])))));
			break;
		case 3:
			ereport(ERROR,
					(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
					 errmsg("%s(): %s, %s, %s is expected but %s, %s, %s",
							fcjinfo->funcname,
							type_to_jsonb_type_str(fcjinfo->argtypes[0]),
							type_to_jsonb_type_str(fcjinfo->argtypes[1]),
							type_to_jsonb_type_str(fcjinfo->argtypes[2]),
							JsonbToCString(NULL, &fcjinfo->args[0]->root,
										   VARSIZE(fcjinfo->args[0])),
							JsonbToCString(NULL, &fcjinfo->args[1]->root,
										   VARSIZE(fcjinfo->args[1])),
							JsonbToCString(NULL, &fcjinfo->args[2]->root,
										   VARSIZE(fcjinfo->args[2])))));
			break;
		case 0:
		default:
			elog(ERROR, "unexpected number of arguments: %d", fcjinfo->nargs);
			break;
	}
}

static char *
type_to_jsonb_type_str(Oid type)
{
	switch (type)
	{
		case INT4OID:
		case FLOAT8OID:
		case NUMERICOID:
			return "number";
		case TEXTOID:
			return "string";
		default:
			elog(ERROR, "unexpected type: %s", format_type_be(type));
			return NULL;
	}
}

static Jsonb *
datum_to_jsonb(Datum d, Oid type)
{
	JsonbValue	jv;

	switch (type)
	{
		case TEXTOID:
			jv.type = jbvString;
			jv.val.string.val = TextDatumGetCString(d);
			jv.val.string.len = strlen(jv.val.string.val);
			break;
		case FLOAT8OID:
			{
				Datum		n;

				n = DirectFunctionCall1(float8_numeric, d);

				jv.type = jbvNumeric;
				jv.val.numeric = DatumGetNumeric(n);
			}
			break;
		case NUMERICOID:
			jv.type = jbvNumeric;
			jv.val.numeric = DatumGetNumeric(d);
			break;
		default:
			elog(ERROR, "unexpected type: %s", format_type_be(type));
			return NULL;
	}

	return JsonbValueToJsonb(&jv);
}

Datum
jsonb_string_starts_with(PG_FUNCTION_ARGS)
{
	Jsonb	   *lj = PG_GETARG_JSONB_P(0);
	Jsonb	   *rj = PG_GETARG_JSONB_P(1);

	if (JB_ROOT_IS_SCALAR(lj) && JB_ROOT_IS_SCALAR(rj))
	{
		JsonbValue *ljv;
		JsonbValue *rjv;

		ljv = getIthJsonbValueFromContainer(&lj->root, 0);
		rjv = getIthJsonbValueFromContainer(&rj->root, 0);

		if (ljv->type == jbvString && rjv->type == jbvString)
		{
			if (ljv->val.string.len < rjv->val.string.len)
				PG_RETURN_BOOL(false);

			if (strncmp(ljv->val.string.val,
						rjv->val.string.val,
						rjv->val.string.len) == 0)
				PG_RETURN_BOOL(true);
			else
				PG_RETURN_BOOL(false);
		}
	}

	ereport(ERROR,
			(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
			 errmsg("STARTS WITH: two string values expected but %s, %s",
					JsonbToCString(NULL, &lj->root, VARSIZE(lj)),
					JsonbToCString(NULL, &rj->root, VARSIZE(rj)))));
	PG_RETURN_NULL();
}

Datum
jsonb_string_ends_with(PG_FUNCTION_ARGS)
{
	Jsonb	   *lj = PG_GETARG_JSONB_P(0);
	Jsonb	   *rj = PG_GETARG_JSONB_P(1);

	if (JB_ROOT_IS_SCALAR(lj) && JB_ROOT_IS_SCALAR(rj))
	{
		JsonbValue *ljv;
		JsonbValue *rjv;

		ljv = getIthJsonbValueFromContainer(&lj->root, 0);
		rjv = getIthJsonbValueFromContainer(&rj->root, 0);

		if (ljv->type == jbvString && rjv->type == jbvString)
		{
			if (ljv->val.string.len < rjv->val.string.len)
				PG_RETURN_BOOL(false);

			if (strncmp(ljv->val.string.val + ljv->val.string.len - rjv->val.string.len,
						rjv->val.string.val,
						rjv->val.string.len) == 0)
				PG_RETURN_BOOL(true);
			else
				PG_RETURN_BOOL(false);
		}
	}

	ereport(ERROR,
			(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
			 errmsg("ENDS WITH: two string values expected but %s, %s",
					JsonbToCString(NULL, &lj->root, VARSIZE(lj)),
					JsonbToCString(NULL, &rj->root, VARSIZE(rj)))));
	PG_RETURN_NULL();
}

Datum
jsonb_string_contains(PG_FUNCTION_ARGS)
{
	Jsonb	   *lj = PG_GETARG_JSONB_P(0);
	Jsonb	   *rj = PG_GETARG_JSONB_P(1);

	if (JB_ROOT_IS_SCALAR(lj) && JB_ROOT_IS_SCALAR(rj))
	{
		JsonbValue *ljv;
		JsonbValue *rjv;

		ljv = getIthJsonbValueFromContainer(&lj->root, 0);
		rjv = getIthJsonbValueFromContainer(&rj->root, 0);

		if (ljv->type == jbvString && rjv->type == jbvString)
		{
			char	   *l;
			char	   *r;

			if (ljv->val.string.len < rjv->val.string.len)
				PG_RETURN_BOOL(false);

			l = pnstrdup(ljv->val.string.val, ljv->val.string.len);
			r = pnstrdup(rjv->val.string.val, rjv->val.string.len);

			if (strstr(l, r) == NULL)
				PG_RETURN_BOOL(false);
			else
				PG_RETURN_BOOL(true);
		}
	}

	ereport(ERROR,
			(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
			 errmsg("CONTAINS: two string values expected but %s, %s",
					JsonbToCString(NULL, &lj->root, VARSIZE(lj)),
					JsonbToCString(NULL, &rj->root, VARSIZE(rj)))));
	PG_RETURN_NULL();
}

Datum
jsonb_string_regex(PG_FUNCTION_ARGS)
{
	Jsonb	   *lj = PG_GETARG_JSONB_P(0);
	Jsonb	   *rj = PG_GETARG_JSONB_P(1);

	if (JB_ROOT_IS_SCALAR(lj) && JB_ROOT_IS_SCALAR(rj))
	{
		JsonbValue *ljv;
		JsonbValue *rjv;

		ljv = getIthJsonbValueFromContainer(&lj->root, 0);
		rjv = getIthJsonbValueFromContainer(&rj->root, 0);

		if (ljv->type == jbvString && rjv->type == jbvString)
		{
			text	   *lt;
			text	   *rt;
			Datum		result;

			lt = cstring_to_text_with_len(ljv->val.string.val,
										  ljv->val.string.len);
			rt = cstring_to_text_with_len(rjv->val.string.val,
										  rjv->val.string.len);

			result = DirectFunctionCall2Coll(textregexeq,
											 DEFAULT_COLLATION_OID,
											 PointerGetDatum(lt),
											 PointerGetDatum(rt));
			PG_RETURN_DATUM(result);
		}
	}

	ereport(ERROR,
			(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
			 errmsg("Regular Expression Pattern: two string values expected but %s, %s",
					JsonbToCString(NULL, &lj->root, VARSIZE(lj)),
					JsonbToCString(NULL, &rj->root, VARSIZE(rj)))));
	PG_RETURN_NULL();
}

/*
 * Function to return a row containing the columns for the respective values
 * of insertVertex, insertEdge, deleteVertex, deleteEdge, and updateProperty.
 * These values represent the status of the last Cypher graph write (insert,
 * delete, and/or update properties). They are only reset prior to a Cypher
 * graph write.
 */
Datum
get_last_graph_write_stats(PG_FUNCTION_ARGS)
{
	FuncCallContext *funcctx;

	if (SRF_IS_FIRSTCALL())
	{
		MemoryContext oldcontext;
		TupleDesc	tupdesc;

		funcctx = SRF_FIRSTCALL_INIT();
		oldcontext = MemoryContextSwitchTo(funcctx->multi_call_memory_ctx);

		tupdesc = CreateTemplateTupleDesc(5);

		TupleDescInitEntry(tupdesc, (AttrNumber) 1, "insertedvertices",
						   INT8OID, -1, 0);
		TupleDescInitEntry(tupdesc, (AttrNumber) 2, "insertededges",
						   INT8OID, -1, 0);
		TupleDescInitEntry(tupdesc, (AttrNumber) 3, "deletedvertices",
						   INT8OID, -1, 0);
		TupleDescInitEntry(tupdesc, (AttrNumber) 4, "deletededges",
						   INT8OID, -1, 0);
		TupleDescInitEntry(tupdesc, (AttrNumber) 5, "updatedproperties",
						   INT8OID, -1, 0);

		funcctx->attinmeta = TupleDescGetAttInMetadata(tupdesc);

		MemoryContextSwitchTo(oldcontext);
	}

	funcctx = SRF_PERCALL_SETUP();

	if (funcctx->call_cntr < 1)
	{
		Datum	   *dvalues;
		bool	   *nulls;
		HeapTuple	tuple;
		TupleDesc	tupdesc = funcctx->attinmeta->tupdesc;

		dvalues = (Datum *) palloc(sizeof(Datum) * 5);
		nulls = (bool *) palloc(sizeof(bool) * 5);

		memset(dvalues, 0, sizeof(Datum) * 5);
		memset(nulls, false, sizeof(bool) * 5);

		dvalues[0] = Int64GetDatum(graphWriteStats.insertVertex);
		dvalues[1] = Int64GetDatum(graphWriteStats.insertEdge);
		dvalues[2] = Int64GetDatum(graphWriteStats.deleteVertex);
		dvalues[3] = Int64GetDatum(graphWriteStats.deleteEdge);
		dvalues[4] = Int64GetDatum(graphWriteStats.updateProperty);

		tuple = heap_form_tuple(tupdesc, dvalues, nulls);

		pfree(dvalues);
		pfree(nulls);

		SRF_RETURN_NEXT(funcctx, HeapTupleGetDatum(tuple));
	}

	SRF_RETURN_DONE(funcctx);
}

Datum
array_head(PG_FUNCTION_ARGS)
{
	AnyArrayType *arr = PG_GETARG_ANY_ARRAY_P(0);
	Oid			element_type = AARR_ELEMTYPE(arr);
	TypeCacheEntry *typentry;
	int			typlen;
	bool		typbyval;
	char		typalign;
	Datum		rtnelt;
	const int	one = 1;

	typentry = (TypeCacheEntry *) fcinfo->flinfo->fn_extra;

	if (typentry == NULL ||
		typentry->type_id != element_type)
	{
		typentry = lookup_type_cache(element_type,
									 TYPECACHE_CMP_PROC_FINFO);
		if (!OidIsValid(typentry->cmp_proc_finfo.fn_oid))
			ereport(ERROR,
					(errcode(ERRCODE_UNDEFINED_FUNCTION),
					 errmsg("could not identify a comparison function for type %s",
							format_type_be(element_type))));
		fcinfo->flinfo->fn_extra = (void *) typentry;
	}

	typlen = typentry->typlen;
	typbyval = typentry->typbyval;
	typalign = typentry->typalign;

	rtnelt = array_get_element(PointerGetDatum(arr),
							   1,
							   (int *) &one,
							   -1,
							   typlen,
							   typbyval,
							   typalign,
							   &typbyval);

	PG_RETURN_DATUM(rtnelt);
}

Datum
array_last(PG_FUNCTION_ARGS)
{

	AnyArrayType *arr = PG_GETARG_ANY_ARRAY_P(0);
	int			ndims = AARR_NDIM(arr);
	int		   *dims = AARR_DIMS(arr);
	int			nitems = ArrayGetNItems(ndims, dims);
	Oid			element_type = AARR_ELEMTYPE(arr);
	TypeCacheEntry *typentry;
	int			typlen;
	bool		typbyval;
	char		typalign;
	Datum		rtnelt;

	typentry = (TypeCacheEntry *) fcinfo->flinfo->fn_extra;

	if (typentry == NULL ||
		typentry->type_id != element_type)
	{
		typentry = lookup_type_cache(element_type,
									 TYPECACHE_CMP_PROC_FINFO);
		if (!OidIsValid(typentry->cmp_proc_finfo.fn_oid))
			ereport(ERROR,
					(errcode(ERRCODE_UNDEFINED_FUNCTION),
					 errmsg("could not identify a comparison function for type %s",
							format_type_be(element_type))));
		fcinfo->flinfo->fn_extra = (void *) typentry;
	}

	typlen = typentry->typlen;
	typbyval = typentry->typbyval;
	typalign = typentry->typalign;

	rtnelt = array_get_element(PointerGetDatum(arr),
							   1,
							   &nitems,
							   -1,
							   typlen,
							   typbyval,
							   typalign,
							   &typbyval);

	PG_RETURN_DATUM(rtnelt);
}


Datum
array_tail(PG_FUNCTION_ARGS)
{

	AnyArrayType *arr = PG_GETARG_ANY_ARRAY_P(0);
	int			ndims = AARR_NDIM(arr);
	int		   *dims = AARR_DIMS(arr);
	int			nitems = ArrayGetNItems(ndims, dims);
	Oid			element_type = AARR_ELEMTYPE(arr);
	TypeCacheEntry *typentry;
	int			typlen;
	bool		typbyval;
	char		typalign;
	int			i;
	Datum		rtnelt;
	bool		isnull;

	ArrayBuildState *astate = NULL;
	array_iter	it;

	typentry = (TypeCacheEntry *) fcinfo->flinfo->fn_extra;

	if (typentry == NULL ||
		typentry->type_id != element_type)
	{
		typentry = lookup_type_cache(element_type,
									 TYPECACHE_CMP_PROC_FINFO);
		if (!OidIsValid(typentry->cmp_proc_finfo.fn_oid))
			ereport(ERROR,
					(errcode(ERRCODE_UNDEFINED_FUNCTION),
					 errmsg("could not identify a comparison function for type %s",
							format_type_be(element_type))));
		fcinfo->flinfo->fn_extra = (void *) typentry;
	}

	typlen = typentry->typlen;
	typbyval = typentry->typbyval;
	typalign = typentry->typalign;

	/* setup iterator for array and iterate once to ignore this element */
	array_iter_setup(&it, arr);
	array_iter_next(&it, &isnull, 0,
				typlen, typbyval, typalign);

	astate = initArrayResult(element_type, CurrentMemoryContext, false);

	/* iterate over the array */
	for (i = 1; i <nitems; i++)
	{
		/* get datum at index i */
		rtnelt = array_iter_next(&it, &isnull, i,
				typlen, typbyval, typalign);

		astate =
			accumArrayResult(astate, rtnelt, isnull,
							 element_type, CurrentMemoryContext);
	}

	PG_RETURN_ARRAYTYPE_P(makeArrayResult(astate, CurrentMemoryContext));
}


/*
 * str_size:
 *		returns the length of string/ text
 *		example: str_size("Hello") = 5
 */
Datum
str_size(PG_FUNCTION_ARGS)
{

	PG_RETURN_INT32(textlen(fcinfo));

}


/*
 * array_size:
 *		returns the total number of elements in an array
 *		example: array_size(['hello', 'GraphDB']) = 2
 */
Datum
array_size(PG_FUNCTION_ARGS)
{
	AnyArrayType *arr = PG_GETARG_ANY_ARRAY_P(0);
	int				ndims = AARR_NDIM(arr),
					*dims = AARR_DIMS(arr),
					result;

	/* Sanity check: does it look like an array at all? */
	if (ndims <= 0 || *dims > MAXDIM)
		PG_RETURN_NULL();

	result = ArrayGetNItems(ndims, dims);

	// /* free pointers */
	pfree(arr);
	pfree(dims);

	PG_RETURN_INT32(result);

}

/*
 * int_to_bool and string_to_bool are two helper functions that are used by toBoolean() and toBooleanOrNull() functions
 *	The helper functions return either true or false which is mapped to differennt conditions in the two cypher functions. 
 *	The mapping logic is specified aas comments in the respective cypher functions
 */

static 
bool int_to_bool(int32 num, bool *result)
{
	if (num == 0)
	{
		if (result)			
			*result = false;	/* suppress compiler warning */
		return true;
	}
	else if (num == 1)
	{
		if (result)
			*result = true;		/* suppress compiler warning */
		return true;
	}

	return false;
		
		
}

static 
bool string_to_bool(const char *str, bool *result)
{
	size_t		len;
	bool 		parseResult;

	/*
	* Skip leading and trailing whitespace
	*/
	while (isspace((unsigned char) *str))
		str++;

	len = strlen(str);
	while (len > 0 && isspace((unsigned char) str[len - 1]))
		len--;
	
	/* handle special case: boolean equivalent of an empty string is null */
	if(len==0)
	{
		*result = true; /* result = true translates to returning null in the calling function */
		return false;
	}


	switch (*str)
	{
		/*
		* boolin(yes) = true and boolin(no) = false, but the requirements state that toBoolean(yes) = null and toBoolean(no) = null.
		* So, we explicitely map these conditions.
		*/
		case 'y':
		case 'Y':
		{
			if (pg_strcasecmp(str, "yes") == 0)
			{
				*result = true;
				return false;
			}
			break;
		}			
		case 'n':
		case 'N':
		{
			if ((pg_strcasecmp(str, "no") == 0))
			{
				*result = true;
				return false;
			}
			break;
		}			
		default:
		{
			if(parse_bool_with_len(str, len, &parseResult))
			{
				*result = parseResult;
				return true;	
			}
		}
			
						
	}

	if (result)
		*result = false;		/* suppress compiler warning */
	return false;

}

/*
 * datum_toboolean:
 *		returns the boolean equivalent of integers, jsonb and booleans 
 *		example: toboolean(1) = true, toboolean(false) = false
 * 				 tolboolean(12.5) = ERROR
 */
Datum 
datum_toboolean(PG_FUNCTION_ARGS)
{
	
	Oid			typeid = get_fn_expr_argtype(fcinfo->flinfo, 0);
	bool    	result;
	int32 		num;

	switch(typeid)
	{
		case INT2OID:
		case INT4OID:
		case INT8OID:
		{
			num = PG_GETARG_INT32(0);

			if(int_to_bool(num, &result))
				PG_RETURN_BOOL(result);
			
			else 
				ereport(ERROR,
				(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
				errmsg("Invalid input integer for toBoolean(): %d", num)));	
			
			break;
		}			
		
		case BOOLOID:
		{
			PG_RETURN_BOOL(PG_GETARG_BOOL(0));
			break;
		}			

		default:
			break;
	}

	ereport(ERROR,
		(errcode(ERRCODE_UNDEFINED_PARAMETER),
		errmsg("Invalid input type for toBoolean()")));	
}


/*
 * string_tobooleanornull:
 *		returns the boolean or nullequivalent of strings 
 *		example: tobooleanornull('true') = true, tobooleanornull('f') = false
 * 				 tobooleanornull('') = null, tobooleanornull('hello') = null
 */
Datum 
string_tobooleanornull(PG_FUNCTION_ARGS)
{
	const text 	*in_text = DatumGetTextPP(PG_GETARG_DATUM(0));
	const char* in_str = text_to_cstring(in_text);
	bool result;

	if(string_to_bool(in_str, &result))
		PG_RETURN_BOOL(result);
	
	else		
		PG_RETURN_NULL();
	
}


/*
 * datum_tobooleanornull:
 *		returns the boolean equivalent of integers, jsonb and booleans 
 *		example: tobooleanornull(1) = true, tobooleanornull(false) = false
 * 				 tobooleanornull(12.5) = null
 */
Datum
datum_tobooleanornull(PG_FUNCTION_ARGS)
{
	Oid			typeid = get_fn_expr_argtype(fcinfo->flinfo, 0);
	bool		result;
	int32 		num;

	switch (typeid){
		case VARCHAROID:
		case BPCHAROID:
		case TEXTOID:
		{
			PG_RETURN_BOOL(string_tobooleanornull(fcinfo));
			break;
		}				

		case BOOLOID:
		{
			PG_RETURN_BOOL(PG_GETARG_BOOL(0));
			break;
		}			

		case INT2OID:
		case INT4OID:
		case INT8OID:
		{
			num = PG_GETARG_INT32(0);

			if(int_to_bool(num, &result))
				PG_RETURN_BOOL(result);
			
			else 
				PG_RETURN_NULL();			
			break;
		}			

		case JSONBOID:
		{
			PG_RETURN_BOOL(jsonb_toboolean(fcinfo));
			break;
		}
			

		default:
			break;
	}	
	PG_RETURN_NULL();	
}


/*
 * range:
 *		returns an array with the series from start to end with an optional step value 
 *		example: range(1,10) = [0,1,2,3,4,5,6,7,8,9,10], range(1,11,3) = [1,4,7,10]
 * 				 range(1,10, -3) = []
 */
Datum
range(int32 start, int32 end, int32 step)
{
	int32 		len = (int32) floor((end - start + step) / step),
				counter = 0;
	ArrayType 	*arr;

	/* sanity checks */
	if(len<=0)
	{
		arr = construct_array(NULL, 0, INT4OID, sizeof(int32), true, 'i');
	}

	else if(((start<=end) && step<0 ) || ((start>=end) && step>0))
	{
		arr = construct_array(NULL, 0, INT4OID, sizeof(int32), true, 'i');
	}

	else /* all sanity checks have passed, the series has to be generated and an array has to be created */
	{
		Datum 	*elements = (Datum *) palloc(sizeof(Datum) * len);
		if(step<0)
		{
			while(start>=end)
			{
				elements[counter++] = start;
				start+=step;
			}
		}
		else
		{
			while(start<=end)
			{
				elements[counter++] = start;
				start+=step;
			}
		}		

		arr = construct_array(elements, len, INT4OID, sizeof(int32), true, 'i');

		/* free pointers */
		pfree(elements);
	}
	

	PG_RETURN_POINTER(arr);
}

/* wrapper function which is used to map two integer inputs (start and end) with the range function. Default step value is created */
Datum
range_2_args(PG_FUNCTION_ARGS)
{
	int32 start = PG_GETARG_INT32(0);
	int32 end = PG_GETARG_INT32(1);
	int32 step;
	
	/* Setting the value of step */
	if(start<=end) /* if the progression is positive, step = 1 */
		step = 1;
	else /* if the progression is negative, step = -1 */
		step = -1;

	PG_RETURN_POINTER(range(start, end, step));
}

/* wrapper function which is used to map three integer inputs (start, end, step) with the range function.*/
Datum
range_3_args(PG_FUNCTION_ARGS)
{
	int32 start = PG_GETARG_INT32(0);
	int32 end = PG_GETARG_INT32(1);
	int32 step = PG_GETARG_INT32(2);

	/* Sanity checks */
	if(step==0)
	{
		ereport(ERROR,
		(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
		errmsg("Step value cannot be 0")));	
	}	

	PG_RETURN_POINTER(range(start, end, step));

}


/* utility function for tostringornull */
Datum
datum_to_text(Datum d, Oid typeid)
{

	char* s;
	switch (typeid){

		/* string types */
		case VARCHAROID:
		case BPCHAROID:
		case TEXTOID:
		{
			s = DatumGetCString(DirectFunctionCall1(textout, d));
			PG_RETURN_TEXT_P(cstring_to_text(s));	
			break;
		}				
		
		case CSTRINGOID:
		{
			s = DatumGetCString(DirectFunctionCall1(cstring_out, d));
			PG_RETURN_TEXT_P(cstring_to_text(s));	
			break;
		}

		case BOOLOID:
		{
			s = DatumGetCString(DirectFunctionCall1(boolout, d));
			PG_RETURN_TEXT_P(cstring_to_text(s));	
			break;
		}			

		/* integer, float and numeric types */
		case INT2OID:
		case INT4OID:
		case INT8OID:
		{
			s = DatumGetCString(DirectFunctionCall1(int4out, d));
			PG_RETURN_TEXT_P(cstring_to_text(s));	
			break;
		}			

		case NUMERICOID:
		{
			s = DatumGetCString(DirectFunctionCall1(numeric_out, d));
			PG_RETURN_TEXT_P(cstring_to_text(s));	
			break;
		}
		
		case FLOAT4OID:
		case FLOAT8OID:
		{
			s = DatumGetCString(DirectFunctionCall1(float8out, d));
			PG_RETURN_TEXT_P(cstring_to_text(s));	
			break;
		}

		/* date and time types */
		case DATEOID:
		{
			s = DatumGetCString(DirectFunctionCall1(date_out, d));
			PG_RETURN_TEXT_P(cstring_to_text(s));	
			break;
		}

		case TIMEOID:
		{
			s = DatumGetCString(DirectFunctionCall1(time_out, d));
			PG_RETURN_TEXT_P(cstring_to_text(s));	
			break;
		}

		case TIMESTAMPOID:
		{
			s = DatumGetCString(DirectFunctionCall1(timestamp_out, d));
			PG_RETURN_TEXT_P(cstring_to_text(s));	
			break;
		}

		/* unknown type */
		case UNKNOWNOID:
		{
			s = DatumGetCString(DirectFunctionCall1(unknownout, d));
			PG_RETURN_TEXT_P(cstring_to_text(s));	
			break;

		}			
		

		default:
			break;
	}	

	PG_RETURN_DATUM(0);	
}

/*
 * tostringornull:
 *		returns the string or null equivalent of a datum
 *		example: tostringornull(123) = '123', tostringornull(null) = null
 * 				 tostringornull(true) = 't'
 */
Datum
tostringornull(PG_FUNCTION_ARGS)
{
	Oid			typeid = get_fn_expr_argtype(fcinfo->flinfo, 0);
	Datum 		d = PG_GETARG_DATUM(0);

	PG_RETURN_DATUM(datum_to_text(d, typeid));	
}

/*
 * tostringlist:
 *		returns an array with the string equivalent of all the elements
 *		example: tostringlist([1,2,true]) = ['1','2','true']
 * 				 tostringlist([null,'1289',34.6]) = [null,'1289','34.6']
 */
Datum
jsonb_tostringlist(PG_FUNCTION_ARGS)
{

	Jsonb	   *j = PG_GETARG_JSONB_P(0);
	JsonbParseState *jpstate = NULL;
	JsonbValue *ajv;

	if (!JB_ROOT_IS_ARRAY(j) || JB_ROOT_IS_SCALAR(j))
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
				 errmsg("toStringList(): list is expected but %s",
						JsonbToCString(NULL, &j->root, VARSIZE(j)))));

	pushJsonbValue(&jpstate, WJB_BEGIN_ARRAY, NULL);

	if (JB_ROOT_COUNT(j) > 1)
	{
		JsonbIterator *it;
		JsonbValue	jv;
		JsonbValue *jv_new;
		JsonbValue	sjv;
		JsonbIteratorToken tok;
		int32 	counter = 0;

		it = JsonbIteratorInit(&j->root);
		tok = JsonbIteratorNext(&it, &jv, false);
		while (tok != WJB_DONE)
		{
			if (tok == WJB_ELEM)
			{
				jv_new = getIthJsonbValueFromContainer(&j->root, counter++);
				if (jv_new->type == jbvString)
				{
					jv = *jv_new;
				}
				else if (jv_new->type == jbvNumeric)
				{
					Datum		s;

					s = DirectFunctionCall1(numeric_out,
											NumericGetDatum(jv_new->val.numeric));
					
					sjv.type = jbvString;
					sjv.val.string.val = DatumGetCString(s);
					sjv.val.string.len = strlen(sjv.val.string.val);
					jv = sjv;
				}
				else if (jv_new->type == jbvBool)
				{
					sjv.type = jbvString;

					if (jv_new->val.boolean)
					{
						sjv.val.string.len = 4;
						sjv.val.string.val = "true";
					}
					else
					{
						sjv.val.string.len = 5;
						sjv.val.string.val = "false";
					}

					jv = sjv;
				}
				pushJsonbValue(&jpstate, WJB_ELEM, &jv);
			}

			tok = JsonbIteratorNext(&it, &jv, true);
		}
	}

	ajv = pushJsonbValue(&jpstate, WJB_END_ARRAY, NULL);

	PG_RETURN_JSONB_P(JsonbValueToJsonb(ajv));
}


Datum
array_tostringlist(PG_FUNCTION_ARGS)
{

	AnyArrayType 	*arr = PG_GETARG_ANY_ARRAY_P(0);
	int				ndims = AARR_NDIM(arr);
	int		   		*dims = AARR_DIMS(arr);
	int				nitems = ArrayGetNItems(ndims, dims);
	Oid				element_type = AARR_ELEMTYPE(arr);
	TypeCacheEntry 	*typentry;
	int				typlen;
	bool			typbyval;
	char			typalign;
	ArrayBuildState *astate = NULL;
	array_iter		it;
	Datum			rtnelt;
	bool			isnull;
	int				i;

	typentry = (TypeCacheEntry *) fcinfo->flinfo->fn_extra;

	if (typentry == NULL ||
		typentry->type_id != element_type)
	{
		typentry = lookup_type_cache(element_type,
									 TYPECACHE_CMP_PROC_FINFO);
		if (!OidIsValid(typentry->cmp_proc_finfo.fn_oid))
			ereport(ERROR,
					(errcode(ERRCODE_UNDEFINED_FUNCTION),
					 errmsg("could not identify a comparison function for type %s",
							format_type_be(element_type))));
		fcinfo->flinfo->fn_extra = (void *) typentry;
	}

	typlen = typentry->typlen;
	typbyval = typentry->typbyval;
	typalign = typentry->typalign;

	/* setup iterator for array */
	array_iter_setup(&it, arr);

	astate = initArrayResult(TEXTOID, CurrentMemoryContext, false);

	/* iterate over the array */
	for (i = 0; i <nitems; i++)
	{
		/* get datum at index i */
		rtnelt = array_iter_next(&it, &isnull, i,
					typlen, typbyval, typalign);

		/* handle special case where converting null values to numeric type throws an error */
		if(!isnull)
			rtnelt = datum_to_text(rtnelt, element_type);

		astate = accumArrayResult(astate, rtnelt, isnull,
							 TEXTOID, CurrentMemoryContext);

	}

	PG_RETURN_ARRAYTYPE_P(makeArrayResult(astate, CurrentMemoryContext));
}


/*
 * reverse:
 *		the two functions below returns the reverse of the input array
 *		example: reverse([1,2,true]) = [true,2,1]
 * 				 reverse([null,'1289',34.6]) = [34.6,'1289',null]
 */

/*  jsonb reverse function */
Datum
jsonb_array_reverse(PG_FUNCTION_ARGS)
{
	Jsonb	   *j = PG_GETARG_JSONB_P(0);
	JsonbParseState *jpstate = NULL;
	JsonbValue *ajv;
	int32 	counter = -1;

	if (!JB_ROOT_IS_ARRAY(j) || JB_ROOT_IS_SCALAR(j))
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
				 errmsg("reverse(): list is expected but %s",
						JsonbToCString(NULL, &j->root, VARSIZE(j)))));

	pushJsonbValue(&jpstate, WJB_BEGIN_ARRAY, NULL);

	if (JB_ROOT_COUNT(j) > 1)
	{
		JsonbIterator *it;
		JsonbValue	jv;
		JsonbValue *jv_new;
		JsonbIteratorToken tok;
		counter = (int32) JB_ROOT_COUNT(j) -1;

		it = JsonbIteratorInit(&j->root);
		tok = JsonbIteratorNext(&it, &jv, false);
		while (tok != WJB_DONE)
		{
			if (tok == WJB_ELEM)
			{
				jv_new = getIthJsonbValueFromContainer(&j->root, counter--);
				jv = *jv_new;
				pushJsonbValue(&jpstate, WJB_ELEM, &jv);
			}

			tok = JsonbIteratorNext(&it, &jv, true);
		}
	}

	ajv = pushJsonbValue(&jpstate, WJB_END_ARRAY, NULL);

	PG_RETURN_JSONB_P(JsonbValueToJsonb(ajv));
}

/*  array reverse function */
Datum
array_reverse(PG_FUNCTION_ARGS)
{

	AnyArrayType 	*arr = PG_GETARG_ANY_ARRAY_P(0);
	int				ndims = AARR_NDIM(arr);
	int		   		*dims = AARR_DIMS(arr);
	int				nitems = ArrayGetNItems(ndims, dims);
	Oid				element_type = AARR_ELEMTYPE(arr);
	TypeCacheEntry 	*typentry;
	int				typlen;
	bool			typbyval;
	char			typalign;
	ArrayBuildState *astate = NULL;
	array_iter		it;
	Datum*			datumArr;
	bool			isnull,
					*boolArr;
	int				i;

	typentry = (TypeCacheEntry *) fcinfo->flinfo->fn_extra;

	if (typentry == NULL ||
		typentry->type_id != element_type)
	{
		typentry = lookup_type_cache(element_type,
									 TYPECACHE_CMP_PROC_FINFO);
		if (!OidIsValid(typentry->cmp_proc_finfo.fn_oid))
			ereport(ERROR,
					(errcode(ERRCODE_UNDEFINED_FUNCTION),
					 errmsg("could not identify a comparison function for type %s",
							format_type_be(element_type))));
		fcinfo->flinfo->fn_extra = (void *) typentry;
	}

	typlen = typentry->typlen;
	typbyval = typentry->typbyval;
	typalign = typentry->typalign;

	datumArr = (Datum *) palloc((nitems + 1) * sizeof(Datum));	
	boolArr = (bool *) palloc((nitems + 1) * sizeof(bool));
	
	/* setup iterator for array*/
	array_iter_setup(&it, arr);

	/* iterate over the array */
	for (i = nitems-1; i >= 0; i--)
	{
		/* get datum at index i */
		datumArr[i] = array_iter_next(&it, &isnull, nitems-1-i,
				typlen, typbyval, typalign);
		
		boolArr[i] = isnull;
	}

	

	astate = initArrayResult(element_type, CurrentMemoryContext, false);	

	/* iterate over the array */
	for (i = 0; i < nitems; i++)
	{
		/* get datum at index i and null status at position 1*/

		astate =
			accumArrayResult(astate, datumArr[i], boolArr[i],
							 element_type, CurrentMemoryContext);
	}

	PG_RETURN_ARRAYTYPE_P(makeArrayResult(astate, CurrentMemoryContext));

}

/*
 * split:
 *		splits the text into array based on the delimiter given
 *		example: split('Hi/Hello/Welcome','/') =  ["Hi","Hello","Welcome"]
 */
Datum
split(PG_FUNCTION_ARGS)
{
	PG_RETURN_ARRAYTYPE_P(text_to_array(fcinfo));
}

/* Helper function to get local datetime */
static Timestamp
dt2local(Timestamp dt, int tz)
{
	dt -= (tz * USECS_PER_SEC);
	return dt;
}


/* Helper function to get timestamp for a particular timezone */
static Timestamp
get_timestamp_for_timezone(text* zone, TimestampTz timestamp)
{	
	Timestamp	result;
	int			tz;
	char		tzname[TZ_STRLEN_MAX + 1];
	char	   *lowzone;
	int			type,
				val;
	pg_tz	   *tzp;

	if (TIMESTAMP_NOT_FINITE(timestamp))
		PG_RETURN_TIMESTAMP(timestamp);

	/*
	 * Look up the requested timezone.  First we look in the timezone
	 * abbreviation table (to handle cases like "EST"), and if that fails, we
	 * look in the timezone database (to handle cases like
	 * "America/New_York").  (This matches the order in which timestamp input
	 * checks the cases; it's important because the timezone database unwisely
	 * uses a few zone names that are identical to offset abbreviations.)
	 */
	text_to_cstring_buffer(zone, tzname, sizeof(tzname));

	/* DecodeTimezoneAbbrev requires lowercase input */
	lowzone = downcase_truncate_identifier(tzname,
										   strlen(tzname),
										   false);

	type = DecodeTimezoneAbbrev(0, lowzone, &val, &tzp);

	if (type == 5 || type == 6)
	{
		/* fixed-offset abbreviation */
		tz = -val;
		result = dt2local(timestamp, tz);
	}
	else if (type == 7)
	{
		/* dynamic-offset abbreviation, resolve using specified time */
		int			isdst;

		tz = DetermineTimeZoneAbbrevOffsetTS(timestamp, tzname, tzp, &isdst);
		result = dt2local(timestamp, tz);
	}
	else
	{
		/* try it as a full zone name */
		tzp = pg_tzset(tzname);
		if (tzp)
		{
			/* Apply the timezone change */
			struct pg_tm tm;
			fsec_t		fsec;

			if (timestamp2tm(timestamp, &tz, &tm, &fsec, NULL, tzp) != 0)
				ereport(ERROR,
						(errcode(ERRCODE_DATETIME_VALUE_OUT_OF_RANGE),
						 errmsg("timestamp out of range")));
			if (tm2timestamp(&tm, fsec, NULL, &result) != 0)
				ereport(ERROR,
						(errcode(ERRCODE_DATETIME_VALUE_OUT_OF_RANGE),
						 errmsg("timestamp out of range")));
		}
		else
		{
			ereport(ERROR,
					(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
					 errmsg("time zone \"%s\" not recognized", tzname)));
			result = 0;			/* keep compiler quiet */
		}
	}

	if (!IS_VALID_TIMESTAMP(result))
		ereport(ERROR,
				(errcode(ERRCODE_DATETIME_VALUE_OUT_OF_RANGE),
				 errmsg("timestamp out of range")));

	PG_RETURN_TIMESTAMP(result);
}

/*
 * datetime:
 *		constructs a timestamp for the given timezone
 *		example: datetime(2020,5,26,10,45,56,0,0,'GMT') = "2020-05-26T10:45:56"
 */
Datum
datetime(PG_FUNCTION_ARGS)
{
	int32_t 	year,
				month,
				day,
				hour,
				min,
				seconds,
				millisec,
				nanosec;
	double		sec;
	text*	   	zone;	
	struct 		pg_tm tm;
	TimeOffset	date;
	TimeOffset	time;
	int			dterr;
	bool		bc = false;
	Timestamp	result;

	year = PG_GETARG_INT32(0);
	month = PG_GETARG_INT32(1);
	day = PG_GETARG_INT32(2);
	hour = PG_GETARG_INT32(3);
	min = PG_GETARG_INT32(4);
	seconds = PG_GETARG_INT32(5);
	millisec = PG_GETARG_INT32(6);
	nanosec = PG_GETARG_INT32(7);
	sec = (double) seconds + (double) (millisec*0.001) + (double) (nanosec*0.000001);

	zone = cstring_to_text(DatumGetCString(DirectFunctionCall1(unknownout, PG_GETARG_DATUM(8))));	

	tm.tm_year = year;
	tm.tm_mon = month;
	tm.tm_mday = day;

	/* Handle negative years as BC */
	if (tm.tm_year < 0)
	{
		bc = true;
		tm.tm_year = -tm.tm_year;
	}

	dterr = ValidateDate(DTK_DATE_M, false, false, bc, &tm);

	if (dterr != 0)
		ereport(ERROR,
				(errcode(ERRCODE_DATETIME_FIELD_OVERFLOW),
				 errmsg("date field value out of range: %d-%02d-%02d",
						year, month, day)));

	if (!IS_VALID_JULIAN(tm.tm_year, tm.tm_mon, tm.tm_mday))
		ereport(ERROR,
				(errcode(ERRCODE_DATETIME_VALUE_OUT_OF_RANGE),
				 errmsg("date out of range: %d-%02d-%02d",
						year, month, day)));

	date = date2j(tm.tm_year, tm.tm_mon, tm.tm_mday) - POSTGRES_EPOCH_JDATE;

	/* Check for time overflow */
	if (float_time_overflows(hour, min, sec))
		ereport(ERROR,
				(errcode(ERRCODE_DATETIME_FIELD_OVERFLOW),
				 errmsg("time field value out of range: %d:%02d:%02g",
						hour, min, sec)));

	/* This should match tm2time */
	time = (((hour * MINS_PER_HOUR + min) * SECS_PER_MINUTE)
			* USECS_PER_SEC) + (int64) rint(sec * USECS_PER_SEC);

	result = date * USECS_PER_DAY + time;
	/* check for major overflow */
	if ((result - time) / USECS_PER_DAY != date)
		ereport(ERROR,
				(errcode(ERRCODE_DATETIME_VALUE_OUT_OF_RANGE),
				 errmsg("timestamp out of range: %d-%02d-%02d %d:%02d:%02g",
						year, month, day,
						hour, min, sec)));

	/* check for just-barely overflow (okay except time-of-day wraps) */
	/* caution: we want to allow 1999-12-31 24:00:00 */
	if ((result < 0 && date > 0) ||
		(result > 0 && date < -1))
		ereport(ERROR,
				(errcode(ERRCODE_DATETIME_VALUE_OUT_OF_RANGE),
				 errmsg("timestamp out of range: %d-%02d-%02d %d:%02d:%02g",
						year, month, day,
						hour, min, sec)));

	/* final range check catches just-out-of-range timestamps */
	if (!IS_VALID_TIMESTAMP(result))
		ereport(ERROR,
				(errcode(ERRCODE_DATETIME_VALUE_OUT_OF_RANGE),
				 errmsg("timestamp out of range: %d-%02d-%02d %d:%02d:%02g",
						year, month, day,
						hour, min, sec)));

	PG_RETURN_TIMESTAMP(get_timestamp_for_timezone(zone, result));
}


/*
 * localdatetime:
 *		returns the current timestamp in a specified time zone
 *		example: localdatetime('Asia/Kolkata') =  "2023-02-28T19:59:13.485879"
 */
Datum
localdatetime(PG_FUNCTION_ARGS)
{
	text*	   	zone = cstring_to_text(DatumGetCString(DirectFunctionCall1(unknownout, PG_GETARG_DATUM(0))));	
	TimestampTz timestamp = now(fcinfo);
	PG_RETURN_TIMESTAMP(get_timestamp_for_timezone(zone, timestamp));
	
}


/*
 * get_time:
 *		returns the current local time with precision 6
 *		example: get_time() =  "21:28:54.677068"
 */
Datum
get_time(PG_FUNCTION_ARGS)
{
	TimeADT		result;
	struct pg_tm tt,
			   *tm = &tt;
	fsec_t		fsec;
	int			tz;

	GetCurrentTimeUsec(tm, &fsec, &tz);

	tm2time(tm, fsec, &result);
	AdjustTimeForTypmod(&result, 6);
	return result;
	PG_RETURN_TIMEADT(result);
}

/*
 * get_time_for_timezone:
 *		returns the current local time with precision 4
 *		example: get_time_for_timezone('GMT') =   "16:06:04.994725"
 */
Datum
get_time_for_timezone(PG_FUNCTION_ARGS)
{
	text*	   	zone = cstring_to_text(DatumGetCString(DirectFunctionCall1(unknownout, PG_GETARG_DATUM(0))));
	TimestampTz ts = now(fcinfo);
	Timestamp timestamp = get_timestamp_for_timezone(zone, ts);
	TimeADT		result;
	struct pg_tm tt,
			   *tm = &tt;
	fsec_t		fsec;

	if (TIMESTAMP_NOT_FINITE(timestamp))
		PG_RETURN_NULL();

	if (timestamp2tm(timestamp, NULL, tm, &fsec, NULL, NULL) != 0)
		ereport(ERROR,
				(errcode(ERRCODE_DATETIME_VALUE_OUT_OF_RANGE),
				 errmsg("timestamp out of range")));

	/*
	 * Could also do this with time = (timestamp / USECS_PER_DAY *
	 * USECS_PER_DAY) - timestamp;
	 */
	result = ((((tm->tm_hour * MINS_PER_HOUR + tm->tm_min) * SECS_PER_MINUTE) + tm->tm_sec) *
			  USECS_PER_SEC) + fsec;

	PG_RETURN_TIMEADT(result);
}


/* utility function for tointegerornull */
Datum
datum_to_integer(Datum d, Oid typeid)
{

	char* s;
	bool canConvert = true; /* a flag variable that reflects whether thee datum can be converted to cstring or not */
	switch (typeid){

		/* string types */
		case VARCHAROID:
		case BPCHAROID:
		case TEXTOID:
		{
			s = DatumGetCString(DirectFunctionCall1(textout, d));
			break;
		}				
		
		case CSTRINGOID:
		{
			s = DatumGetCString(DirectFunctionCall1(cstring_out, d));
			break;
		}

		case BOOLOID:
		{
			bool res = DatumGetBool(DirectFunctionCall1(boolout, d));
			PG_RETURN_INT32((int32) res);
		}			

		/* integer, float and numeric types */
		case INT2OID:
		case INT4OID:
		case INT8OID:
		{
			s = DatumGetCString(DirectFunctionCall1(int4out, d));
			break;

		}			

		case NUMERICOID:
		{
			s = DatumGetCString(DirectFunctionCall1(numeric_out, d));
			break;
		}
		
		case FLOAT4OID:
		case FLOAT8OID:
		{
			s = DatumGetCString(DirectFunctionCall1(float8out, d));
			break;
		}

		/* unknown type */
		case UNKNOWNOID:
		{
			s = DatumGetCString(DirectFunctionCall1(unknownout, d));			
			break;

		}					

		default:
		{
			canConvert = false;
			break;
		}
			
	}	

	if(canConvert)
		PG_RETURN_INT32(string_to_int(s));

	PG_RETURN_DATUM(0);	
}

/* utility function to covert cstring to integer */
static int32
string_to_int(char* s)
{
	int64 	res = 0,
			len = strlen(s),
			i = 0;
	bool 	flag = false;

	/* iterate over the string and check for non numeric characters */
	for(i = 0; i < len ; ++i)
	{
		if(isalpha(s[i]))
		{
			flag = true;
			break;
		}
	}
	
	if(!flag)
	{
		res = atoll(s);
		return res;
	}
	else
		return 0;
}

/* get substring of lengthlen from pos start. Used to get string elements from jsonb array */
static void
get_cstring_substr(char *c, char *res, int32 start, int32 len)
{
	int32  i;
	for(i = 0; i < len; i++)
		res[i] = *(c+i+start);
	
	res[len] = '\0'; /* null terminated c string*/
}

/*
 * tointegerlist:
 *		returns an array with the integer equivalent of all the elements
 *		example: tointegerlist(['1','2','true']) = [1'2,'true']
 * 				 tointegerlist([null,'1289',34.6]) = [null,1289,34]
 */
Datum
jsonb_tointegerlist(PG_FUNCTION_ARGS)
{

	Jsonb	   		*j = PG_GETARG_JSONB_P(0);
	JsonbParseState *jpstate = NULL;
	JsonbValue 		*ajv;

	if (!JB_ROOT_IS_ARRAY(j) || JB_ROOT_IS_SCALAR(j))
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
				 errmsg("toStringList(): list is expected but %s",
						JsonbToCString(NULL, &j->root, VARSIZE(j)))));

	pushJsonbValue(&jpstate, WJB_BEGIN_ARRAY, NULL);
	
	if (JB_ROOT_COUNT(j) > 1)
	{
		JsonbIterator 		*it;
		JsonbValue			jv;
		JsonbValue 			*jv_new;
		JsonbValue			sjv;
		JsonbIteratorToken 	tok;
		int32 				counter = 0;
		Datum				s;

		it = JsonbIteratorInit(&j->root);
		tok = JsonbIteratorNext(&it, &jv, false);
		sjv.type = jbvNumeric;

		while (tok != WJB_DONE)
		{
			if (tok == WJB_ELEM)
			{
				jv_new = getIthJsonbValueFromContainer(&j->root, counter++);
				if (jv_new->type == jbvString)
				{
					
					char 	res[64];/* long long can have 64 bits*/
					get_cstring_substr(jv_new->val.string.val, res, 0, jv_new->val.string.len);
					sjv.val.numeric = int64_to_numeric(string_to_int(res));
					jv = sjv;
				}
				else if (jv_new->type == jbvNumeric)
				{
					s = NumericGetDatum(jv_new->val.numeric);
					sjv.val.numeric = int64_to_numeric(datum_to_integer(s, NUMERICOID));
					jv = sjv;
				}
				else if (jv_new->type == jbvBool)
				{					

					if (jv_new->val.boolean)					
						sjv.val.numeric = int64_to_numeric(1);
					
					else				
						sjv.val.numeric = int64_to_numeric(0);

					jv = sjv;
				}
				pushJsonbValue(&jpstate, WJB_ELEM, &jv);
			}

			tok = JsonbIteratorNext(&it, &jv, true);
		}
	}

	ajv = pushJsonbValue(&jpstate, WJB_END_ARRAY, NULL);

	PG_RETURN_JSONB_P(JsonbValueToJsonb(ajv));
}


Datum
array_tointegerlist(PG_FUNCTION_ARGS)
{

	AnyArrayType 	*arr = PG_GETARG_ANY_ARRAY_P(0);
	int				ndims = AARR_NDIM(arr);
	int		   		*dims = AARR_DIMS(arr);
	int				nitems = ArrayGetNItems(ndims, dims);
	Oid				element_type = AARR_ELEMTYPE(arr);
	TypeCacheEntry 	*typentry;
	int				typlen;
	bool			typbyval;
	char			typalign;
	ArrayBuildState *astate = NULL;
	array_iter		it;
	Datum			rtnelt;
	bool			isnull;
	int				i;

	typentry = (TypeCacheEntry *) fcinfo->flinfo->fn_extra;

	if (typentry == NULL ||
		typentry->type_id != element_type)
	{
		typentry = lookup_type_cache(element_type,
									 TYPECACHE_CMP_PROC_FINFO);
		if (!OidIsValid(typentry->cmp_proc_finfo.fn_oid))
			ereport(ERROR,
					(errcode(ERRCODE_UNDEFINED_FUNCTION),
					 errmsg("could not identify a comparison function for type %s",
							format_type_be(element_type))));
		fcinfo->flinfo->fn_extra = (void *) typentry;
	}

	typlen = typentry->typlen;
	typbyval = typentry->typbyval;
	typalign = typentry->typalign;

	/* setup iterator for array */
	array_iter_setup(&it, arr);

	astate = initArrayResult(INT4OID, CurrentMemoryContext, false);

	/* iterate over the array */
	for (i = 0; i <nitems; i++)
	{
		/* get datum at index i */
		rtnelt = array_iter_next(&it, &isnull, i,
					typlen, typbyval, typalign);

		/* handle special case where converting null values to numeric type throws an error */
		if(!isnull)
			rtnelt = datum_to_integer(rtnelt, element_type);

		astate = accumArrayResult(astate, rtnelt, isnull,
							 INT4OID, CurrentMemoryContext);

	}

	PG_RETURN_ARRAYTYPE_P(makeArrayResult(astate, CurrentMemoryContext));
}


/* utility function for float functions */
Datum
datum_to_float(Datum d, Oid typeid)
{

	char* s;
	bool canConvert = true; /* a flag variable that reflects whether thee datum can be converted to cstring or not */
	switch (typeid){

		/* string types */
		case VARCHAROID:
		case BPCHAROID:
		case TEXTOID:
		{
			s = DatumGetCString(DirectFunctionCall1(textout, d));
			break;
		}				
		
		case CSTRINGOID:
		{
			s = DatumGetCString(DirectFunctionCall1(cstring_out, d));
			break;
		}

		case BOOLOID:
		{
			bool res = DatumGetBool(DirectFunctionCall1(boolout, d));
			PG_RETURN_FLOAT4((float4) res);
		}			

		/* integer, float and numeric types */
		case INT2OID:
		case INT4OID:
		case INT8OID:
		{
			s = DatumGetCString(DirectFunctionCall1(int4out, d));
			break;

		}			

		case NUMERICOID:
		{
			s = DatumGetCString(DirectFunctionCall1(numeric_out, d));
			break;
		}
		
		case FLOAT4OID:
		case FLOAT8OID:
		{
			s = DatumGetCString(DirectFunctionCall1(float8out, d));
			break;
		}

		/* unknown type */
		case UNKNOWNOID:
		{
			s = DatumGetCString(DirectFunctionCall1(unknownout, d));			
			break;

		}					

		default:
		{
			canConvert = false;
			break;
		}
			
	}	

	if(canConvert)
		PG_RETURN_FLOAT4(string_to_float(s));

	PG_RETURN_DATUM(0);	
}

/* utility function to covert cstring to float */
static float4
string_to_float(char* s)
{
	float 	res = 0.0;
	int64	len = strlen(s),
			i = 0;
	bool 	flag = false;

	/* iterate over the string and check for non numeric characters */
	for(i = 0; i < len ; ++i)
	{
		if(isalpha(s[i]))
		{
			flag = true;
			break;
		}
	}
	
	if(!flag)
	{
		res = atof(s);
		return res;
	}
	else
		return 0;
}

/*
 * tofloatlist:
 *		returns an array with the floatlist equivalent of all the elements
 *		example: tofloatlist(['1.4','2.5','true']) = [1.4, 2.5, 1.0]
 * 				 tofloatlist([null,'1289',34.6]) = [null,1289.0,34.6]
 */
Datum
array_tofloatlist(PG_FUNCTION_ARGS)
{

	AnyArrayType 	*arr = PG_GETARG_ANY_ARRAY_P(0);
	int				ndims = AARR_NDIM(arr);
	int		   		*dims = AARR_DIMS(arr);
	int				nitems = ArrayGetNItems(ndims, dims);
	Oid				element_type = AARR_ELEMTYPE(arr);
	TypeCacheEntry 	*typentry;
	int				typlen;
	bool			typbyval;
	char			typalign;
	ArrayBuildState *astate = NULL;
	array_iter		it;
	Datum			rtnelt;
	bool			isnull;
	int				i;

	typentry = (TypeCacheEntry *) fcinfo->flinfo->fn_extra;

	if (typentry == NULL ||
		typentry->type_id != element_type)
	{
		typentry = lookup_type_cache(element_type,
									 TYPECACHE_CMP_PROC_FINFO);
		if (!OidIsValid(typentry->cmp_proc_finfo.fn_oid))
			ereport(ERROR,
					(errcode(ERRCODE_UNDEFINED_FUNCTION),
					 errmsg("could not identify a comparison function for type %s",
							format_type_be(element_type))));
		fcinfo->flinfo->fn_extra = (void *) typentry;
	}

	typlen = typentry->typlen;
	typbyval = typentry->typbyval;
	typalign = typentry->typalign;

	/* setup iterator for array */
	array_iter_setup(&it, arr);

	astate = initArrayResult(FLOAT4OID, CurrentMemoryContext, false);

	/* iterate over the array */
	for (i = 0; i <nitems; i++)
	{
		/* get datum at index i */
		rtnelt = array_iter_next(&it, &isnull, i,
					typlen, typbyval, typalign);

		/* handle special case where converting null values to numeric type throws an error */
		if(!isnull)
			rtnelt = datum_to_float(rtnelt, element_type);

		astate = accumArrayResult(astate, rtnelt, isnull,
							 FLOAT4OID, CurrentMemoryContext);

	}

	PG_RETURN_ARRAYTYPE_P(makeArrayResult(astate, CurrentMemoryContext));
}