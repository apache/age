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

#include "access/attnum.h"
#include "access/htup.h"
#include "access/htup_details.h"
#include "access/tupdesc.h"
#include "catalog/pg_collation.h"
#include "catalog/pg_type.h"
#include "funcapi.h"
#include "utils/builtins.h"
#include "utils/cypher_funcs.h"
#include "utils/jsonb.h"
#include "utils/memutils.h"
#include "utils/typcache.h"
#include <string.h>

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

	ArrayBuildState *astate = NULL;

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

	astate = initArrayResult(element_type, CurrentMemoryContext, false);

	for (i = 2; i <= nitems; i++)
	{

		rtnelt = array_get_element(PointerGetDatum(arr),
								   1,
								   &i,
								   -1,
								   typlen,
								   typbyval,
								   typalign,
								   &typbyval);
		astate =
			accumArrayResult(astate, rtnelt, false,
							 element_type, CurrentMemoryContext);

	}

	PG_RETURN_ARRAYTYPE_P(makeArrayResult(astate, CurrentMemoryContext));
}
