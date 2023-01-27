/*
 * parse_cypher_utils.c
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
 *	  src/include/parser/parse_cypher_utils.c
 */





#include "postgres.h"

#include "access/relscan.h"
#include "catalog/ag_label.h"
#include "nodes/makefuncs.h"
#include "parser/parse_cypher_utils.h"
#include "parser/parse_relation.h"
#include "utils/lsyscache.h"
#include "utils/syscache.h"
#include "parser/parsetree.h"
#include "catalog/heap.h"
#include "ag_const.h"
#include "catalog/namespace.h"

static int	GetSubLevelsUpByNSItem(ParseState *pstate,
								   ParseNamespaceItem *nsitem);

Node *
makeJsonbFuncAccessor(ParseState *pstate, Node *expr, List *path)
{
	CypherAccessExpr *a = makeNode(CypherAccessExpr);

	a->arg = (Expr *) expr;
	a->path = path;

	return (Node *) a;
}

bool
IsJsonbAccessor(Node *expr)
{
	if (IsA(expr, CypherAccessExpr))
	{
		return true;
	}

	return false;
}

void
getAccessorArguments(Node *node, Node **expr, List **path)
{
	if (IsA(node, CypherAccessExpr))
	{
		CypherAccessExpr *a = (CypherAccessExpr *) node;

		*expr = (Node *) a->arg;
		*path = a->path;
	}
	else
	{
		elog(ERROR, "cannot extract elements from node");
	}
}

/*
 * When use PROPERTY INDEX, there is no way to specify original column
 * reference, so make vertex_id, edge_start, and edge_end in the form of
 * reserved words to create a way to specify B.
 */
bool
ConvertReservedColumnRefForIndex(Node *node, Oid relid)
{
	Form_ag_label labtup;
	Oid			laboid = get_relid_laboid(relid);
	HeapTuple	tuple = SearchSysCache1(LABELOID, ObjectIdGetDatum(laboid));
	bool		isVertex;

	if (!HeapTupleIsValid(tuple))
		elog(ERROR, "cache lookup failed for label (OID=%u)", laboid);

	labtup = (Form_ag_label) GETSTRUCT(tuple);
	isVertex = (labtup->labkind == LABEL_KIND_VERTEX);
	ReleaseSysCache(tuple);

	if (IsA(node, ColumnRef))
	{
		ColumnRef  *columnRef = (ColumnRef *) node;

		if (columnRef->fields->length == 1)
		{
			Node	   *field_name = linitial(columnRef->fields);

			if (IsA(field_name, String))
			{
				char	   *fieldStr = strVal(field_name);

				if (isVertex && (strcmp(fieldStr, "vertex_id") == 0))
				{
					columnRef->fields = list_make1(makeString("id"));
					return true;
				}
				else if (!isVertex && strcmp(fieldStr, "edge_start") == 0)
				{
					columnRef->fields = list_make1(makeString("start"));
					return true;
				}
				else if (!isVertex && strcmp(fieldStr, "edge_end") == 0)
				{
					columnRef->fields = list_make1(makeString("end"));
					return true;
				}
			}
		}
	}

	return false;
}


Alias *
makeAliasOptUnique(char *aliasname)
{
	aliasname = (aliasname == NULL ? genUniqueName() : pstrdup(aliasname));
	return makeAliasNoDup(aliasname, NIL);
}


Alias *
makeAliasNoDup(char *aliasname, List *colnames)
{
	Alias	   *alias;

	alias = makeNode(Alias);
	alias->aliasname = aliasname;
	alias->colnames = colnames;

	return alias;
}

/* generate unique name */
char *
genUniqueName(void)
{
	/* NOTE: safe unless there are more than 2^32 anonymous names at once */
	static uint32 seq = 0;

	char		data[NAMEDATALEN];

	snprintf(data, sizeof(data), "<%010u>", seq++);

	return pstrdup(data);
}

void
makeExtraFromNSItem(ParseNamespaceItem *nsitem, RangeTblRef **rtr,
					bool visible)
{
	if (rtr != NULL)
	{
		RangeTblRef *_rtr;

		_rtr = makeNode(RangeTblRef);
		_rtr->rtindex = nsitem->p_rtindex;

		*rtr = _rtr;
	}

	if (nsitem != NULL)
	{
		nsitem->p_rel_visible = visible;
		nsitem->p_cols_visible = visible;
		nsitem->p_lateral_only = false;
		nsitem->p_lateral_ok = true;
	}
}

void
addNSItemToJoinlist(ParseState *pstate, ParseNamespaceItem *nsitem,
					bool visible)
{
	ParseNamespaceItem *conflict_nsitem;
	RangeTblRef *rtr;

	/*
	 * There should be no namespace conflicts because we check a variable
	 * (which becomes an alias) is duplicated. This check remains to prevent
	 * future programming error.
	 */
	conflict_nsitem = scanNameSpaceForRefname(pstate,
											  nsitem->p_rte->eref->aliasname,
											  -1);
	if (conflict_nsitem != NULL)
	{
		RangeTblEntry *rte = nsitem->p_rte;
		RangeTblEntry *tmp = conflict_nsitem->p_rte;

		if (!(rte->rtekind == RTE_RELATION && rte->alias == NULL &&
			  tmp->rtekind == RTE_RELATION && tmp->alias == NULL &&
			  rte->relid != tmp->relid))
			ereport(ERROR,
					(errcode(ERRCODE_DUPLICATE_ALIAS),
					 errmsg("variable \"%s\" specified more than once",
							rte->eref->aliasname)));
	}

	makeExtraFromNSItem(nsitem, &rtr, visible);
	pstate->p_joinlist = lappend(pstate->p_joinlist, rtr);
	pstate->p_namespace = lappend(pstate->p_namespace, nsitem);
}


/*
 * make_var - Copy from a previous version of PG.
 *		Build a Var node for an attribute identified by RTE and attrno
 */
Var *
make_var(ParseState *pstate, ParseNamespaceItem *nsitem, AttrNumber attnum,
		 int location)
{
	Var		   *result;
	int			sublevels_up = GetSubLevelsUpByNSItem(pstate, nsitem);

	if (attnum > InvalidAttrNumber)
	{
		/* Get attribute data from the ParseNamespaceColumn array */
		ParseNamespaceColumn *nscol = &nsitem->p_nscolumns[attnum - 1];

		/* Complain if dropped column.  See notes in scanRTEForColumn. */
		if (nscol->p_varno == 0)
			ereport(ERROR,
					(errcode(ERRCODE_UNDEFINED_COLUMN),
					 errmsg("column of relation \"%s\" does not exist",
							nsitem->p_rte->eref->aliasname)));

		result = makeVar(nsitem->p_rtindex,
						 attnum,
						 nscol->p_vartype,
						 nscol->p_vartypmod,
						 nscol->p_varcollid,
						 sublevels_up);
	}
	else
	{
		/* System column, so use predetermined type data */
		const FormData_pg_attribute *sysatt;

		sysatt = SystemAttributeDefinition(attnum);
		result = makeVar(nsitem->p_rtindex,
						 attnum,
						 sysatt->atttypid,
						 sysatt->atttypmod,
						 sysatt->attcollation,
						 sublevels_up);
	}
	result->location = location;
	return result;
}

static int
GetSubLevelsUpByNSItem(ParseState *pstate, ParseNamespaceItem *nsitem)
{
	int			sublevels_up = 0;

	while (pstate != NULL)
	{
		int			rtable_length = list_length(pstate->p_rtable);

		if (rtable_length >= nsitem->p_rtindex)
		{
			if (nsitem->p_rte == rt_fetch(nsitem->p_rtindex, pstate->p_rtable))
				return sublevels_up;
		}

		pstate = pstate->parentParseState;
		sublevels_up++;
	}

	elog(ERROR, "RTE not found (internal error)");
	return 0;					/* keep compiler quiet */
}

FuncCall *
makeArrayAggFuncCall(List *args, int location)
{
	return makeFuncCall(list_make1(makeString("array_agg")),
						args, COERCE_EXPLICIT_CALL, location);
}


Node *
makeRowExprWithTypeCast(List *args, Oid typeOid, int location)
{
	RowExpr    *row;
	TypeCast   *cast;

	row = makeNode(RowExpr);
	row->args = args;
	row->row_typeid = typeOid;
	row->colnames = NIL;
	row->row_format = COERCE_EXPLICIT_CAST;
	row->location = location;

	cast = makeNode(TypeCast);
	cast->arg = (Node *) row;
	cast->typeName = makeTypeNameFromOid(typeOid, -1);
	cast->location = location;

	return (Node *) cast;
}

Node *
makeTypedRowExpr(List *args, Oid typoid, int location)
{
	RowExpr    *row = makeNode(RowExpr);

	row->args = args;
	row->row_typeid = typoid;
	row->row_format = COERCE_EXPLICIT_CAST;
	row->location = location;

	return (Node *) row;
}

Node *
makeAArrayExpr(List *elements, Oid typeOid)
{
	A_ArrayExpr *arr;
	TypeCast   *cast;

	arr = makeNode(A_ArrayExpr);
	arr->elements = elements;
	arr->location = -1;

	cast = makeNode(TypeCast);
	cast->arg = (Node *) arr;
	cast->typeName = makeTypeNameFromOid(typeOid, -1);
	cast->location = -1;

	return (Node *) cast;
}

Node *
makeArrayExpr(Oid typarray, Oid typoid, List *elems)
{
	ArrayExpr  *arr = makeNode(ArrayExpr);

	arr->array_typeid = typarray;
	arr->element_typeid = typoid;
	arr->elements = elems;
	arr->multidims = false;
	arr->location = -1;

	return (Node *) arr;
}

Node *
makeVertexExpr(ParseState *pstate, ParseNamespaceItem *nsitem, int location)
{
	Node	   *id;
	Node	   *prop_map;
	Node	   *tid;

	id = getColumnVar(pstate, nsitem, AG_ELEM_LOCAL_ID);
	prop_map = getColumnVar(pstate, nsitem, AG_ELEM_PROP_MAP);
	tid = getSysColumnVar(pstate, nsitem, SelfItemPointerAttributeNumber);

	return makeTypedRowExpr(list_make3(id, prop_map, tid), VERTEXOID, location);
}

Node *
makeEdgeExpr(ParseState *pstate, CypherRel *crel, ParseNamespaceItem *nsitem,
			 int location)
{
	Node	   *id;
	Node	   *start;
	Node	   *end;
	Node	   *prop_map;
	Node	   *tid;

	id = getColumnVar(pstate, nsitem, AG_ELEM_LOCAL_ID);
	start = getColumnVar(pstate, nsitem, AG_START_ID);
	end = getColumnVar(pstate, nsitem, AG_END_ID);
	prop_map = getColumnVar(pstate, nsitem, AG_ELEM_PROP_MAP);
	if (crel->direction == CYPHER_REL_DIR_NONE)
		tid = getColumnVar(pstate, nsitem, "ctid");
	else
		tid = getSysColumnVar(pstate, nsitem, SelfItemPointerAttributeNumber);

	return makeTypedRowExpr(list_make5(id, start, end, prop_map, tid),
							EDGEOID, location);
}

Node *
getColumnVar(ParseState *pstate, ParseNamespaceItem *nsitem, char *colname)
{
	ListCell   *lcn;
	AttrNumber	attrno;
	Var		   *var;
	RangeTblEntry *rte = nsitem->p_rte;

	attrno = 1;
	foreach(lcn, rte->eref->colnames)
	{
		const char *tmp = strVal(lfirst(lcn));

		if (strcmp(tmp, colname) == 0)
		{
			/*
			 * NOTE: no ambiguous reference check here since all column names
			 * in `rte` are unique
			 */

			var = make_var(pstate, nsitem, attrno, -1);

			/* require read access to the column */
			markVarForSelectPriv(pstate, var);

			return (Node *) var;
		}

		attrno++;
	}

	elog(ERROR, "column \"%s\" not found (internal error)", colname);
}

Node *
getSysColumnVar(ParseState *pstate, ParseNamespaceItem *nsitem,
				AttrNumber attnum)
{
	Var		   *var;

	AssertArg(attnum <= SelfItemPointerAttributeNumber &&
			  attnum >= FirstLowInvalidHeapAttributeNumber);

	var = make_var(pstate, nsitem, attnum, -1);

	markVarForSelectPriv(pstate, var);

	return (Node *) var;
}

Node *
makeColumnRef(List *fields)
{
	ColumnRef  *n = makeNode(ColumnRef);

	n->fields = fields;
	n->location = -1;
	return (Node *) n;
}

ResTarget *
makeSimpleResTarget(const char *field, const char *name)
{
	Node	   *cref;

	cref = makeColumnRef(list_make1(makeString(pstrdup(field))));

	if (name == NULL)
	{
		/* if name is null, filled from FigureColname */
		name = field;
	}

	return makeResTarget(cref, name);
}

ResTarget *
makeResTarget(Node *val, const char *name)
{
	ResTarget  *res = makeNode(ResTarget);

	if (name != NULL)
	{
		res->name = pstrdup(name);
	}
	else if (IsA(val, ColumnRef))
	{
		ColumnRef  *ref = (ColumnRef *) val;

		res->name = NameListToString(ref->fields);
	}
	else
	{
		res->name = NULL_COLUMN_NAME;
	}

	res->val = val;
	res->location = -1;

	return res;
}
