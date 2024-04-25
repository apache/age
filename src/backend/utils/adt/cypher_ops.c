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

#include "utils/builtins.h"
#include "utils/cypher_ops.h"
#include "utils/graph.h"
#include "utils/jsonb.h"
#include "utils/memutils.h"
#include "utils/numeric.h"

static Jsonb *jnumber_op(PGFunction f, Jsonb *l, Jsonb *r);
static Jsonb *numeric_to_jnumber(Numeric n);
static void ereport_op(PGFunction f, Jsonb *l, Jsonb *r);
static void ereport_op_str(const char *op, Jsonb *l, Jsonb *r);

Datum
jsonb_add(PG_FUNCTION_ARGS)
{
	Jsonb	   *l = PG_GETARG_JSONB_P(0);
	Jsonb	   *r = PG_GETARG_JSONB_P(1);
	JsonbValue *ljv;
	JsonbValue *rjv;
	JsonbValue	jv;
	Size		len;
	char	   *buf;
	Datum		n;
	char	   *nstr;
	Size		nlen;

	if (!(JB_ROOT_IS_SCALAR(l) && JB_ROOT_IS_SCALAR(r)))
	{
		Datum		j;

		if ((JB_ROOT_IS_SCALAR(l) && JB_ROOT_IS_OBJECT(r)) ||
			(JB_ROOT_IS_OBJECT(l) && JB_ROOT_IS_SCALAR(r)) ||
			(JB_ROOT_IS_OBJECT(l) && JB_ROOT_IS_OBJECT(r)))
			ereport_op_str("+", l, r);

		j = DirectFunctionCall2(jsonb_concat,
								JsonbPGetDatum(l), JsonbPGetDatum(r));

		PG_RETURN_DATUM(j);
	}

	ljv = getIthJsonbValueFromContainer(&l->root, 0);
	rjv = getIthJsonbValueFromContainer(&r->root, 0);

	if (ljv->type == jbvString && rjv->type == jbvString)
	{
		len = ljv->val.string.len + rjv->val.string.len;
		buf = palloc(len);

		strncpy(buf, ljv->val.string.val, ljv->val.string.len);
		strncpy(buf + ljv->val.string.len,
				rjv->val.string.val, rjv->val.string.len);

		jv.type = jbvString;
		jv.val.string.len = (int) len;
		jv.val.string.val = buf;

		PG_RETURN_JSONB_P(JsonbValueToJsonb(&jv));
	}
	else if (ljv->type == jbvString && rjv->type == jbvNumeric)
	{
		n = DirectFunctionCall1(numeric_out,
								NumericGetDatum(rjv->val.numeric));
		nstr = DatumGetCString(n);
		nlen = strlen(nstr);

		len = ljv->val.string.len + nlen;
		buf = palloc(len);

		strncpy(buf, ljv->val.string.val, ljv->val.string.len);
		strncpy(buf + ljv->val.string.len, nstr, nlen);

		jv.type = jbvString;
		jv.val.string.len = (int) len;
		jv.val.string.val = buf;

		PG_RETURN_JSONB_P(JsonbValueToJsonb(&jv));
	}
	else if (ljv->type == jbvNumeric && rjv->type == jbvString)
	{
		n = DirectFunctionCall1(numeric_out,
								NumericGetDatum(ljv->val.numeric));
		nstr = DatumGetCString(n);
		nlen = strlen(nstr);

		len = nlen + rjv->val.string.len;
		buf = palloc(len);

		strncpy(buf, nstr, nlen);
		strncpy(buf + nlen, rjv->val.string.val, rjv->val.string.len);

		jv.type = jbvString;
		jv.val.string.len = (int) len;
		jv.val.string.val = buf;

		PG_RETURN_JSONB_P(JsonbValueToJsonb(&jv));
	}
	else if (ljv->type == jbvNumeric && rjv->type == jbvNumeric)
	{
		n = DirectFunctionCall2(numeric_add,
								NumericGetDatum(ljv->val.numeric),
								NumericGetDatum(rjv->val.numeric));

		PG_RETURN_JSONB_P(numeric_to_jnumber(DatumGetNumeric(n)));
	}
	else
	{
		ereport_op_str("+", l, r);
	}

	PG_RETURN_NULL();
}

Datum
jsonb_sub(PG_FUNCTION_ARGS)
{
	PG_RETURN_JSONB_P(jnumber_op(numeric_sub,
								 PG_GETARG_JSONB_P(0), PG_GETARG_JSONB_P(1)));
}

Datum
jsonb_mul(PG_FUNCTION_ARGS)
{
	PG_RETURN_JSONB_P(jnumber_op(numeric_mul,
								 PG_GETARG_JSONB_P(0), PG_GETARG_JSONB_P(1)));
}

Datum
jsonb_div(PG_FUNCTION_ARGS)
{
	PG_RETURN_JSONB_P(jnumber_op(numeric_div,
								 PG_GETARG_JSONB_P(0), PG_GETARG_JSONB_P(1)));
}

Datum
jsonb_mod(PG_FUNCTION_ARGS)
{
	PG_RETURN_JSONB_P(jnumber_op(numeric_mod,
								 PG_GETARG_JSONB_P(0), PG_GETARG_JSONB_P(1)));
}

Datum
jsonb_pow(PG_FUNCTION_ARGS)
{
	PG_RETURN_JSONB_P(jnumber_op(numeric_power,
								 PG_GETARG_JSONB_P(0), PG_GETARG_JSONB_P(1)));
}

Datum
jsonb_uplus(PG_FUNCTION_ARGS)
{
	PG_RETURN_JSONB_P(jnumber_op(numeric_uplus, NULL, PG_GETARG_JSONB_P(0)));
}

Datum
jsonb_uminus(PG_FUNCTION_ARGS)
{
	PG_RETURN_JSONB_P(jnumber_op(numeric_uminus, NULL, PG_GETARG_JSONB_P(0)));
}

static Jsonb *
jnumber_op(PGFunction f, Jsonb *l, Jsonb *r)
{
	FunctionCallInfo fcinfo;
	JsonbValue	*jv;
	Datum		n;
	Jsonb		*op_result = NULL;

	AssertArg(r != NULL);

	if (!((l == NULL || JB_ROOT_IS_SCALAR(l)) && JB_ROOT_IS_SCALAR(r)))
		ereport_op(f, l, r);

	fcinfo = palloc(SizeForFunctionCallInfo(2));
	InitFunctionCallInfoData(*fcinfo, NULL, 0, InvalidOid, NULL, NULL);

	if (l != NULL)
	{
		jv = getIthJsonbValueFromContainer(&l->root, 0);
		if (jv->type != jbvNumeric)
			ereport_op(f, l, r);

		fcinfo->args[fcinfo->nargs].value = NumericGetDatum(jv->val.numeric);
		fcinfo->args[fcinfo->nargs].isnull = false;
		fcinfo->nargs++;
	}

	jv = getIthJsonbValueFromContainer(&r->root, 0);
	if (jv->type != jbvNumeric)
		ereport_op(f, l, r);

	fcinfo->args[fcinfo->nargs].value = NumericGetDatum(jv->val.numeric);
	fcinfo->args[fcinfo->nargs].isnull = false;
	fcinfo->nargs++;

	n = (*f) (fcinfo);
	if (fcinfo->isnull)
		elog(ERROR, "function %p returned NULL", (void *) f);

	if (f == numeric_power || f == numeric_div)
	{
		int			s;

		s = DatumGetInt32(DirectFunctionCall1(numeric_scale, fcinfo->args[0].value)) +
			DatumGetInt32(DirectFunctionCall1(numeric_scale, fcinfo->args[1].value));
		if (s == 0)
			n = DirectFunctionCall2(numeric_trunc, n, 0);
	}

	op_result = numeric_to_jnumber(DatumGetNumeric(n));
	pfree(fcinfo);
	return op_result;
}

static Jsonb *
numeric_to_jnumber(Numeric n)
{
	JsonbValue	jv;

	jv.type = jbvNumeric;
	jv.val.numeric = n;

	return JsonbValueToJsonb(&jv);
}

static void
ereport_op(PGFunction f, Jsonb *l, Jsonb *r)
{
	const char *op;

	if (f == numeric_add || f == numeric_uplus)
	{
		op = "+";
	}
	else if (f == numeric_sub || f == numeric_uminus)
	{
		op = "-";
	}
	else if (f == numeric_mul)
	{
		op = "*";
	}
	else if (f == numeric_div)
	{
		op = "/";
	}
	else if (f == numeric_mod)
	{
		op = "%";
	}
	else if (f == numeric_power)
	{
		op = "^";
	}
	else
	{
		elog(ERROR, "invalid number operator");
	}

	ereport_op_str(op, l, r);
}

static void
ereport_op_str(const char *op, Jsonb *l, Jsonb *r)
{
	const char *msgfmt;
	const char *lstr;
	const char *rstr;

	AssertArg(r != NULL);

	if (l == NULL)
	{
		msgfmt = "invalid expression: %s%s%s";
		lstr = "";
	}
	else
	{
		msgfmt = "invalid expression: %s %s %s";
		lstr = JsonbToCString(NULL, &l->root, VARSIZE(l));
	}
	rstr = JsonbToCString(NULL, &r->root, VARSIZE(r));

	ereport(ERROR,
			(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
			 errmsg(msgfmt, lstr, op, rstr)));
}

Datum
numeric_graphid(PG_FUNCTION_ARGS)
{
	Datum		n = PG_GETARG_DATUM(0);
	Datum		d;

	d = DirectFunctionCall1(numeric_out, n);

	PG_RETURN_DATUM(DirectFunctionCall1(graphid_in, d));
}
