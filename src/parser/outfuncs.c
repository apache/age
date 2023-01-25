/*-------------------------------------------------------------------------
 *
 * outfuncs.c
 *	  Output functions for Postgres tree nodes.
 *
 * Portions Copyright (c) 2003-2022, PgPool Global Development Group
 * Portions Copyright (c) 1996-2022, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *
 * IDENTIFICATION
 *	  src/backend/nodes/outfuncs.c
 *
 * NOTES
 *	  Every node type that can appear in stored rules' parsetrees *must*
 *	  have an output function defined here (as well as an input function
 *	  in readfuncs.c).	For use in debugging, we also provide output
 *	  functions for nodes that appear in raw parsetrees, path, and plan trees.
 *	  These nodes however need not have input functions.
 *
 *-------------------------------------------------------------------------
 */
#include <string.h>
#include <limits.h>
#include <ctype.h>
#include "pool_type.h"
#include "utils/palloc.h"
#include "utils/elog.h"
#include "parser.h"
#include "extensible.h"
#include "pg_list.h"
#include "parsenodes.h"
#include "pg_class.h"
#include "pg_trigger.h"
#include "value.h"

#define booltostr(x)  ((x) ? "true" : "false")

void		_outNode(StringInfo str, void *obj);

static void _outList(StringInfo str, List *node);
static void _outIdList(StringInfo str, List *node);
static void _outAlias(StringInfo str, Alias *node);
static void _outRangeVar(StringInfo str, RangeVar *node);
static void _outVar(StringInfo str, Var *node);
static void _outConst(StringInfo str, Const *node);
static void _outParam(StringInfo str, Param *node);
static void _outAggref(StringInfo str, Aggref *node);
static void _outGroupingFunc(StringInfo str, GroupingFunc *node);
static void _outSubscriptingRef(StringInfo str, SubscriptingRef *node);
static void _outFuncExpr(StringInfo str, FuncExpr *node);
static void _outNamedArgExpr(StringInfo str, NamedArgExpr *node);
static void _outOpExpr(StringInfo str, OpExpr *node);
static void _outDistinctExpr(StringInfo str, DistinctExpr *node);
static void _outScalarArrayOpExpr(StringInfo str, ScalarArrayOpExpr *node);
static void _outBoolExpr(StringInfo str, BoolExpr *node);
static void _outSubLink(StringInfo str, SubLink *node);
static void _outSubPlan(StringInfo str, SubPlan *node);
static void _outFieldSelect(StringInfo str, FieldSelect *node);
static void _outFieldStore(StringInfo str, FieldStore *node);
static void _outRelabelType(StringInfo str, RelabelType *node);
static void _outConvertRowtypeExpr(StringInfo str, ConvertRowtypeExpr *node);
static void _outCaseExpr(StringInfo str, CaseExpr *node);
static void _outCaseWhen(StringInfo str, CaseWhen *node);
static void _outCaseTestExpr(StringInfo str, CaseTestExpr *node);
static void _outArrayExpr(StringInfo str, ArrayExpr *node);
static void _outRowExpr(StringInfo str, RowExpr *node);
static void _outCoalesceExpr(StringInfo str, CoalesceExpr *node);
static void _outMinMaxExpr(StringInfo str, MinMaxExpr *node);
static void _outNullIfExpr(StringInfo str, NullIfExpr *node);
static void _outNullTest(StringInfo str, NullTest *node);
static void _outBooleanTest(StringInfo str, BooleanTest *node);
static void _outCoerceToDomain(StringInfo str, CoerceToDomain *node);
static void _outCoerceToDomainValue(StringInfo str, CoerceToDomainValue *node);
static void _outSetToDefault(StringInfo str, SetToDefault *node);
static void _outCurrentOfExpr(StringInfo str, CurrentOfExpr *node);
static void _outInferenceElem(StringInfo str, InferenceElem *node);
static void _outTargetEntry(StringInfo str, TargetEntry *node);
static void _outRangeTblRef(StringInfo str, RangeTblRef *node);
static void _outJoinExpr(StringInfo str, JoinExpr *node);
static void _outFromExpr(StringInfo str, FromExpr *node);
static void _outCreateStmt(StringInfo str, CreateStmt *node);
static void _outCreateTableAsStmt(StringInfo str, CreateTableAsStmt *node);
static void _outCreateForeignTableStmt(StringInfo str, CreateForeignTableStmt *node);
static void _outImportForeignSchemaStmt(StringInfo str, ImportForeignSchemaStmt *node);
static void _outIndexStmt(StringInfo str, IndexStmt *node);
static void _outNotifyStmt(StringInfo str, NotifyStmt *node);
static void _outDeclareCursorStmt(StringInfo str, DeclareCursorStmt *node);
static void _outSelectStmt(StringInfo str, SelectStmt *node);
static void _outFuncCall(StringInfo str, FuncCall *node);
static void _outDefElem(StringInfo str, DefElem *node);
static void _outLockingClause(StringInfo str, LockingClause *node);
static void _outReturnStmt(StringInfo str, ReturnStmt *node);
static void _outPLAssignStmt(StringInfo str, PLAssignStmt *node);
static void _outColumnDef(StringInfo str, ColumnDef *node);
static void _outTypeName(StringInfo str, TypeName *node);
static void _outTypeCast(StringInfo str, TypeCast *node);
static void _outIndexElem(StringInfo str, IndexElem *node);
static void _outGroupingSet(StringInfo str, GroupingSet *node);
static void _outWithClause(StringInfo str, WithClause *node);
static void _outCTESearchClause(StringInfo str, CTESearchClause *node);
static void _outCTECycleClause(StringInfo str, CTECycleClause *node);
static void _outCommonTableExpr(StringInfo str, CommonTableExpr *node);
static void _outMergeWhenClauses(StringInfo str, List *node);
static void _outMergeAction(StringInfo str, MergeAction *node);
static void _outSetOperationStmt(StringInfo str, SetOperationStmt *node);
static void _outTableSampleClause(StringInfo str, TableSampleClause *node);
static void _outA_Expr(StringInfo str, A_Expr *node);
static void _outColumnRef(StringInfo str, ColumnRef *node);
static void _outParamRef(StringInfo str, ParamRef *node);
static void _outA_Const(StringInfo str, A_Const *node);
static void _outA_Indices(StringInfo str, A_Indices *node);
static void _outA_Indirection(StringInfo str, A_Indirection *node);
static void _outResTarget(StringInfo str, ResTarget *node);
static void _outMultiAssignRef(StringInfo str, MultiAssignRef *node);
static void _outA_ArrayExpr(StringInfo str, A_ArrayExpr *node);
static void _outWindowDef(StringInfo str, WindowDef *node);
static void _outConstraint(StringInfo str, Constraint *node);

static void _outSortBy(StringInfo str, SortBy *node);
static void _outInsertStmt(StringInfo str, InsertStmt *node);
static void _outSetClause(StringInfo str, List *node);
static void _outUpdateStmt(StringInfo str, UpdateStmt *node);
static void _outDeleteStmt(StringInfo str, DeleteStmt *node);
static void _outMergeStmt(StringInfo str, MergeStmt *node);
static void _outTransactionStmt(StringInfo str, TransactionStmt *node);
static void _outTruncateStmt(StringInfo str, TruncateStmt *node);
static void _outVacuumStmt(StringInfo str, VacuumStmt *node);
static void _outExplainStmt(StringInfo str, ExplainStmt *node);
static void _outClusterStmt(StringInfo str, ClusterStmt *node);
static void _outCheckPointStmt(StringInfo str, CheckPointStmt *node);
static void _outClosePortalStmt(StringInfo str, ClosePortalStmt *node);
static void _outListenStmt(StringInfo str, ListenStmt *node);
static void _outUnlistenStmt(StringInfo str, UnlistenStmt *node);
static void _outLoadStmt(StringInfo str, LoadStmt *node);
static void _outCopyStmt(StringInfo str, CopyStmt *node);
static void _outDeallocateStmt(StringInfo str, DeallocateStmt *node);
static void _outRenameStmt(StringInfo str, RenameStmt *node);
static void _outCreateRoleStmt(StringInfo str, CreateRoleStmt *node);
static void _outAlterRoleStmt(StringInfo str, AlterRoleStmt *node);
static void _outDropRoleStmt(StringInfo str, DropRoleStmt *node);
static void _outCreateSchemaStmt(StringInfo str, CreateSchemaStmt *node);
static void _outVariableSetStmt(StringInfo str, VariableSetStmt *node);
static void _outVariableShowStmt(StringInfo str, VariableShowStmt *node);
static void _outConstraintsSetStmt(StringInfo str, ConstraintsSetStmt *node);
static void _outAlterTableStmt(StringInfo str, AlterTableStmt *node);
static void _outCreateSeqStmt(StringInfo str, CreateSeqStmt *node);
static void _outAlterSeqStmt(StringInfo str, AlterSeqStmt *node);
static void _outCreatePLangStmt(StringInfo str, CreatePLangStmt *node);
static void _outCreateTableSpaceStmt(StringInfo str, CreateTableSpaceStmt *node);
static void _outDropTableSpaceStmt(StringInfo str, DropTableSpaceStmt *node);
static void _outCreateTrigStmt(StringInfo str, CreateTrigStmt *node);
static void _outDefineStmt(StringInfo str, DefineStmt *node);
static void _outCreateOpClassStmt(StringInfo str, CreateOpClassStmt *node);
static void _outDropStmt(StringInfo str, DropStmt *node);
static void _outFetchStmt(StringInfo str, FetchStmt *node);
static void _outGrantStmt(StringInfo str, GrantStmt *node);
static void _outGrantRoleStmt(StringInfo str, GrantRoleStmt *node);
static void _outCreateFunctionStmt(StringInfo str, CreateFunctionStmt *node);
static void _outAlterFunctionStmt(StringInfo str, AlterFunctionStmt *node);
static void _outCreateCastStmt(StringInfo str, CreateCastStmt *node);
static void _outReindexStmt(StringInfo str, ReindexStmt *node);
static void _outRuleStmt(StringInfo str, RuleStmt *node);
static void _outViewStmt(StringInfo str, ViewStmt *node);
static void _outCreatedbStmt(StringInfo str, CreatedbStmt *node);
static void _outAlterDatabaseStmt(StringInfo str, AlterDatabaseStmt *node);
static void _outAlterDatabaseSetStmt(StringInfo str, AlterDatabaseSetStmt *node);
static void _outDropdbStmt(StringInfo str, DropdbStmt *node);
static void _outCreateDomainStmt(StringInfo str, CreateDomainStmt *node);
static void _outAlterDomainStmt(StringInfo str, AlterDomainStmt *node);
static void _outCreateConversionStmt(StringInfo str, CreateConversionStmt *node);
static void _outPrepareStmt(StringInfo str, PrepareStmt *node);
static void _outExecuteStmt(StringInfo str, ExecuteStmt *node);
static void _outLockStmt(StringInfo str, LockStmt *node);
static void _outCommentStmt(StringInfo str, CommentStmt *node);
static void _outDiscardStmt(StringInfo str, DiscardStmt *node);
static void _outCreateOpFamilyStmt(StringInfo str, CreateOpFamilyStmt *node);
static void _outAlterOpFamilyStmt(StringInfo str, AlterOpFamilyStmt *node);
static void _outCreateEnumStmt(StringInfo str, CreateEnumStmt *node);
static void _outDropOwnedStmt(StringInfo str, DropOwnedStmt *node);
static void _outReassignOwnedStmt(StringInfo str, ReassignOwnedStmt *node);
static void _outAlterTSDictionaryStmt(StringInfo str, AlterTSDictionaryStmt *node);
static void _outAlterTSConfigurationStmt(StringInfo str, AlterTSConfigurationStmt *node);
static void _outXmlExpr(StringInfo str, XmlExpr *node);
static void _outXmlSerialize(StringInfo str, XmlSerialize *node);

static void _outFuncName(StringInfo str, List *func_name);
static void _outSetRest(StringInfo str, VariableSetStmt *node);
static void _outSetTransactionModeList(StringInfo str, List *list);
static void _outAlterTableCmd(StringInfo str, AlterTableCmd *node);
static void _outOptSeqList(StringInfo str, List *options);
static void _outObjectWithArgs(StringInfo str, ObjectWithArgs *node);
static void _outFunctionParameter(StringInfo str, FunctionParameter *node);
static void _outPrivilegeList(StringInfo str, List *list);
static void _outFuncOptList(StringInfo str, List *list);
static void _outCreatedbOptList(StringInfo str, List *options);
static void _outOperatorArgTypes(StringInfo str, List *args);
static void _outRangeFunction(StringInfo str, RangeFunction *node);
static void _outRangeTableSample(StringInfo str, RangeTableSample *node);
static void _outWithDefinition(StringInfo str, List *def_list);
static void _outOnConflictClause(StringInfo str, OnConflictClause *node);


/*
 * Borrowed from backend/catalog/namespace.c
 * NameListToString
 *      Utility routine to convert a qualified-name list into a string.
 *
 * This is used primarily to form error messages, and so we do not quote
 * the list elements, for the sake of legibility.
 *
 * In most scenarios the list elements should always be Value strings,
 * but we also allow A_Star for the convenience of ColumnRef processing.
 */
char *
NameListToString(List *names)
{
	StringInfoData string;
	ListCell   *l;

	initStringInfo(&string);

	foreach(l, names)
	{
		Node	   *name = (Node *) lfirst(l);

		if (l != list_head(names))
			appendStringInfoChar(&string, '.');

		if (IsA(name, String))
			appendStringInfoString(&string, strVal(name));
		else if (IsA(name, A_Star))
			appendStringInfoString(&string, "*");
		else
			elog(ERROR, "unexpected node type in name list: %d",
				 (int) nodeTag(name));
	}

	return string.data;
}


static char *
escape_string(char *str)
{
	int			len = strlen(str),
				i,
				j;
	char	   *es = palloc0(len * 2 + 1);

	if (es == NULL)
	{
		return NULL;
	}

	for (i = 0, j = 0; i < len; i++, j++)
	{
		if (str[i] == '\'')
		{
			es[j++] = '\'';
		}
		else if (str[i] == '\\')
		{
			if (!standard_conforming_strings)
				es[j++] = '\\';
		}
		es[j] = str[i];
	}

	return es;
}

/* Assuming that node is a list of String */
static void
_outIdList(StringInfo str, List *node)
{
	ListCell   *lc;
	char		first = 0;

	foreach(lc, node)
	{
		if (first == 0)
			first = 1;
		else
			appendStringInfoString(str, ", ");

		appendStringInfoString(str, "\"");
		appendStringInfoString(str, strVal(lfirst(lc)));
		appendStringInfoString(str, "\"");
	}
}

static void
_outListWith(StringInfo str, List *node, char *op)
{
	ListCell   *lc;
	char		first = 0;

	foreach(lc, node)
	{
		if (first == 0)
			first = 1;
		else
		{
			if (lfirst(lc) != NIL && !IsA(lfirst(lc), A_Indices))
				appendStringInfoString(str, op);
		}
		_outNode(str, lfirst(lc));
	}
}

static void
_outList(StringInfo str, List *node)
{
	_outListWith(str, node, ",");
}



/*****************************************************************************
 *
 *	Stuff from primnodes.h.
 *
 *****************************************************************************/

/* for use by extensions which define extensible nodes */
void
outToken(StringInfo str, const char *s)
{
	if (s == NULL || *s == '\0')
	{
		appendStringInfoString(str, "<>");
		return;
	}

	/*
	 * Look for characters or patterns that are treated specially by read.c
	 * (either in pg_strtok() or in nodeRead()), and therefore need a
	 * protective backslash.
	 */
	/* These characters only need to be quoted at the start of the string */
	if (*s == '<' ||
		*s == '"' ||
		isdigit((unsigned char) *s) ||
		((*s == '+' || *s == '-') &&
		 (isdigit((unsigned char) s[1]) || s[1] == '.')))
		appendStringInfoChar(str, '\\');
	while (*s)
	{
		/* These chars must be backslashed anywhere in the string */
		if (*s == ' ' || *s == '\n' || *s == '\t' ||
			*s == '(' || *s == ')' || *s == '{' || *s == '}' ||
			*s == '\\')
			appendStringInfoChar(str, '\\');
		appendStringInfoChar(str, *s++);
	}
}

static void
_outAlias(StringInfo str, Alias *node)
{
	appendStringInfoString(str, " AS \"");
	appendStringInfoString(str, node->aliasname);
	appendStringInfoString(str, "\"");

	if (node->colnames)
	{
		appendStringInfoString(str, "(");
		_outNode(str, node->colnames);
		appendStringInfoString(str, ")");
	}
}

static void
_outRangeVar(StringInfo str, RangeVar *node)
{
	if (node->catalogname)
	{
		appendStringInfoString(str, "\"");
		appendStringInfoString(str, node->catalogname);
		appendStringInfoString(str, "\".");
	}

	if (node->schemaname)
	{
		appendStringInfoString(str, "\"");
		appendStringInfoString(str, node->schemaname);
		appendStringInfoString(str, "\".");
	}

	appendStringInfoString(str, "\"");
	appendStringInfoString(str, node->relname);
	appendStringInfoString(str, "\"");

	if (node->alias)
		_outNode(str, node->alias);
}

static void
_outVar(StringInfo str, Var *node)
{

}

static void
_outConst(StringInfo str, Const *node)
{

}

static void
_outParam(StringInfo str, Param *node)
{

}

static void
_outAggref(StringInfo str, Aggref *node)
{

}

static void
_outGroupingFunc(StringInfo str, GroupingFunc *node)
{
	appendStringInfoString(str, "GROUPING (");
	_outNode(str, node->args);
	appendStringInfoString(str, ")");

}

static void
_outSubscriptingRef(StringInfo str, SubscriptingRef *node)
{

}

static void
_outFuncExpr(StringInfo str, FuncExpr *node)
{

}

static void
_outNamedArgExpr(StringInfo str, NamedArgExpr *node)
{
	appendStringInfoString(str, node->name);
	appendStringInfoString(str, " := ");
	_outNode(str, node->arg);
}

static void
_outOpExpr(StringInfo str, OpExpr *node)
{

}

static void
_outDistinctExpr(StringInfo str, DistinctExpr *node)
{

}

static void
_outScalarArrayOpExpr(StringInfo str, ScalarArrayOpExpr *node)
{

}

static void
_outBoolExpr(StringInfo str, BoolExpr *node)
{
	switch (node->boolop)
	{
		case AND_EXPR:
			appendStringInfoString(str, " (");
			_outListWith(str, node->args, " AND ");
			appendStringInfoString(str, ")");
			break;

		case OR_EXPR:
			appendStringInfoString(str, " (");
			_outListWith(str, node->args, " OR ");
			appendStringInfoString(str, ")");
			break;

		case NOT_EXPR:
			appendStringInfoString(str, " (NOT ");
			_outList(str, node->args);
			appendStringInfoString(str, ")");
			break;
	}
}

static void
_outSubLink(StringInfo str, SubLink *node)
{
	_outNode(str, node->testexpr);

	/*
	 * If the source was "x IN (select)", convert to "x = ANY (select)".
	 */
	if (node->subLinkType == ANY_SUBLINK && node->operName == NIL)
		node->operName = list_make1(makeString("="));

	if (node->operName != NIL)
	{
		String	   *v = linitial(node->operName);

		if (strcmp(strVal(v), "=") == 0)
			appendStringInfoString(str, " IN ");
		else
		{
			appendStringInfoString(str, strVal(v));
		}
	}

	switch (node->subLinkType)
	{
		case EXISTS_SUBLINK:
			appendStringInfoString(str, " EXISTS ");
			break;

		case ARRAY_SUBLINK:
			appendStringInfoString(str, " ARRAY ");
			break;

		case ANY_SUBLINK:
			if (node->operName != NIL)
			{
				String	   *v = linitial(node->operName);

				if (strcmp(strVal(v), "=") != 0)
				{
					appendStringInfoString(str, strVal(v));
					appendStringInfoString(str, " ANY ");
				}
			}
			break;

		case ALL_SUBLINK:
			appendStringInfoString(str, " ALL ");
			break;

		default:
			break;
	}


	if (node->subselect)
	{
		appendStringInfoString(str, "(");
		_outNode(str, node->subselect);
		appendStringInfoString(str, ")");
	}
}

static void
_outSubPlan(StringInfo str, SubPlan *node)
{

}

static void
_outFieldSelect(StringInfo str, FieldSelect *node)
{

}

static void
_outFieldStore(StringInfo str, FieldStore *node)
{

}

static void
_outRelabelType(StringInfo str, RelabelType *node)
{

}

static void
_outConvertRowtypeExpr(StringInfo str, ConvertRowtypeExpr *node)
{

}

static void
_outCaseExpr(StringInfo str, CaseExpr *node)
{
	ListCell   *lc;

	appendStringInfoString(str, "CASE ");
	if (node->arg)
		_outNode(str, node->arg);

	foreach(lc, node->args)
	{
		_outNode(str, lfirst(lc));
	}

	if (node->defresult)
	{
		appendStringInfoString(str, " ELSE ");
		_outNode(str, node->defresult);
	}

	appendStringInfoString(str, " END");
}

static void
_outCaseWhen(StringInfo str, CaseWhen *node)
{
	appendStringInfoString(str, " WHEN ");
	_outNode(str, node->expr);
	appendStringInfoString(str, " THEN ");
	_outNode(str, node->result);
}

static void
_outCaseTestExpr(StringInfo str, CaseTestExpr *node)
{

}

static void
_outArrayExpr(StringInfo str, ArrayExpr *node)
{
	appendStringInfoString(str, "[");
	_outNode(str, node->elements);
	appendStringInfoString(str, "]");
}

static void
_outRowExpr(StringInfo str, RowExpr *node)
{
	if (node->row_format == COERCE_EXPLICIT_CAST)
		appendStringInfoString(str, "ROW (");
	else
		appendStringInfoString(str, "(");

	if (node->args != NIL)
		_outNode(str, node->args);

	appendStringInfoString(str, ")");
}

static void
_outCoalesceExpr(StringInfo str, CoalesceExpr *node)
{
	appendStringInfoString(str, "COALESCE (");
	_outNode(str, node->args);
	appendStringInfoString(str, ")");
}

static void
_outMinMaxExpr(StringInfo str, MinMaxExpr *node)
{
	if (node->op == IS_GREATEST)
	{
		appendStringInfoString(str, "GREATEST (");
		_outNode(str, node->args);
		appendStringInfoString(str, ")");
	}
	else if (node->op == IS_LEAST)
	{
		appendStringInfoString(str, "LEAST (");
		_outNode(str, node->args);
		appendStringInfoString(str, ")");
	}
}

static void
_outNullIfExpr(StringInfo str, NullIfExpr *node)
{

}

static void
_outNullTest(StringInfo str, NullTest *node)
{
	_outNode(str, node->arg);
	if (node->nulltesttype == IS_NOT_NULL)
		appendStringInfoString(str, " IS NOT NULL");
	else
		appendStringInfoString(str, " IS NULL");
}

static void
_outBooleanTest(StringInfo str, BooleanTest *node)
{
	_outNode(str, node->arg);

	switch (node->booltesttype)
	{
		case IS_TRUE:
			appendStringInfoString(str, " IS TRUE ");
			break;

		case IS_NOT_TRUE:
			appendStringInfoString(str, " IS NOT TRUE ");
			break;

		case IS_FALSE:
			appendStringInfoString(str, " IS FALSE ");
			break;

		case IS_NOT_FALSE:
			appendStringInfoString(str, " IS NOT FALSE ");
			break;

		case IS_UNKNOWN:
			appendStringInfoString(str, " IS UNKNOWN ");
			break;

		case IS_NOT_UNKNOWN:
			appendStringInfoString(str, " IS NOT UNKNOWN ");
			break;
	}
}

static void
_outCoerceToDomain(StringInfo str, CoerceToDomain *node)
{

}

static void
_outCoerceToDomainValue(StringInfo str, CoerceToDomainValue *node)
{

}

static void
_outSetToDefault(StringInfo str, SetToDefault *node)
{
	appendStringInfoString(str, "DEFAULT");
}

static void
_outCurrentOfExpr(StringInfo str, CurrentOfExpr *node)
{
	appendStringInfoString(str, "CURRENT OF ");
	if (node->cursor_name == NULL)
	{
		char		n[10];

		snprintf(n, sizeof(n), "$%d", node->cursor_param);
		appendStringInfoString(str, n);
	}
	else
		appendStringInfoString(str, node->cursor_name);
}

static void
_outInferenceElem(StringInfo str, InferenceElem *node)
{

}

static void
_outTargetEntry(StringInfo str, TargetEntry *node)
{

}

static void
_outRangeTblRef(StringInfo str, RangeTblRef *node)
{

}

static void
_outJoinExpr(StringInfo str, JoinExpr *node)
{
	_outNode(str, node->larg);

	if (node->isNatural == TRUE)
		appendStringInfoString(str, " NATURAL");

	if (node->jointype == JOIN_INNER)
	{
		if (node->usingClause == NIL && node->quals == NULL && !node->isNatural)
			appendStringInfoString(str, " CROSS JOIN ");
		else
			appendStringInfoString(str, " JOIN ");
	}
	else if (node->jointype == JOIN_INNER)
		appendStringInfoString(str, " JOIN ");
	else if (node->jointype == JOIN_LEFT)
		appendStringInfoString(str, " LEFT OUTER JOIN ");
	else if (node->jointype == JOIN_FULL)
		appendStringInfoString(str, " FULL OUTER JOIN ");
	else if (node->jointype == JOIN_RIGHT)
		appendStringInfoString(str, " RIGHT OUTER JOIN ");

	_outNode(str, node->rarg);

	if (node->usingClause != NIL && IsA(node->usingClause, List))
	{
		ListCell   *lc;
		char		comma = 0;

		appendStringInfoString(str, " USING(");

		foreach(lc, node->usingClause)
		{
			if (comma == 0)
				comma = 1;
			else
				appendStringInfoString(str, ",");

			appendStringInfoString(str, "\"");
			appendStringInfoString(str, strVal(lfirst(lc)));
			appendStringInfoString(str, "\"");
		}

		appendStringInfoString(str, ")");

		if (node->join_using_alias)
			_outAlias(str, node->join_using_alias);

	}

	if (node->quals)
	{
		appendStringInfoString(str, " ON ");
		_outNode(str, node->quals);
	}
}

static void
_outFromExpr(StringInfo str, FromExpr *node)
{

}

static void
_outOnConflictExpr(StringInfo str, const OnConflictExpr *node)
{

}

#ifdef NOT_USED
/*****************************************************************************
 *
 *	Stuff from extensible.h
 *
 *****************************************************************************/

static void
_outExtensibleNode(StringInfo str, const ExtensibleNode *node)
{
	const ExtensibleNodeMethods *methods;

	methods = GetExtensibleNodeMethods(node->extnodename, false);

	WRITE_NODE_TYPE("EXTENSIBLENODE");

	WRITE_STRING_FIELD(extnodename);

	/* serialize the private fields */
	methods->nodeOut(str, node);
}
#endif

/*****************************************************************************
 *
 *	Stuff from parsenodes.h.
 *
 *****************************************************************************/

static void
_outCreateStmt(StringInfo str, CreateStmt *node)
{
	appendStringInfoString(str, "CREATE ");
	if (node->relation->relpersistence == RELPERSISTENCE_TEMP)
		appendStringInfoString(str, "TEMP ");
	appendStringInfoString(str, "TABLE ");
	_outNode(str, node->relation);
	appendStringInfoString(str, " (");
	_outNode(str, node->tableElts);
	appendStringInfoString(str, ") ");

	if (node->inhRelations != NIL)
	{
		appendStringInfoString(str, "INHERITS (");
		_outNode(str, node->inhRelations);
		appendStringInfoString(str, ")");
	}

	if (node->options)
		_outWithDefinition(str, node->options);

	switch (node->oncommit)
	{
		case ONCOMMIT_DROP:
			appendStringInfoString(str, " ON COMMIT DROP");
			break;

		case ONCOMMIT_DELETE_ROWS:
			appendStringInfoString(str, " ON COMMIT DELETE ROWS");
			break;

		case ONCOMMIT_PRESERVE_ROWS:
			appendStringInfoString(str, " ON COMMIT PRESERVE ROWS");
			break;

		default:
			break;
	}

	if (node->tablespacename)
	{
		appendStringInfoString(str, " TABLESPACE \"");
		appendStringInfoString(str, node->tablespacename);
		appendStringInfoString(str, "\"");
	}
}

static void
_outCreateTableAsStmt(StringInfo str, CreateTableAsStmt *node)
{
	appendStringInfoString(str, "CREATE ");
	if (node->into->rel->relpersistence == RELPERSISTENCE_TEMP)
		appendStringInfoString(str, "TEMP ");
	appendStringInfoString(str, "TABLE ");
	_outNode(str, node->into->rel);

	if (node->into->colNames)
	{
		appendStringInfoString(str, " (");
		_outIdList(str, node->into->colNames);
		appendStringInfoString(str, ") ");
	}

	if (node->into->options)
		_outWithDefinition(str, node->into->options);

	switch (node->into->onCommit)
	{
		case ONCOMMIT_DROP:
			appendStringInfoString(str, " ON COMMIT DROP");
			break;

		case ONCOMMIT_DELETE_ROWS:
			appendStringInfoString(str, " ON COMMIT DELETE ROWS");
			break;

		case ONCOMMIT_PRESERVE_ROWS:
			appendStringInfoString(str, " ON COMMIT PRESERVE ROWS");
			break;

		default:
			break;
	}

	if (node->into->tableSpaceName)
	{
		appendStringInfoString(str, " TABLESPACE \"");
		appendStringInfoString(str, node->into->tableSpaceName);
		appendStringInfoString(str, "\"");
	}

	if (node->query)
	{
		appendStringInfoString(str, " AS");
		_outSelectStmt(str, (SelectStmt *) node->query);
	}
}

static void
_outCreateForeignTableStmt(StringInfo str, CreateForeignTableStmt *node)
{

}

static void
_outImportForeignSchemaStmt(StringInfo str, ImportForeignSchemaStmt *node)
{

}

static void
_outIndexStmt(StringInfo str, IndexStmt *node)
{
	appendStringInfoString(str, "CREATE ");

	if (node->unique == TRUE)
		appendStringInfoString(str, "UNIQUE ");

	if (node->concurrent == true)
		appendStringInfoString(str, "INDEX CONCURRENTLY ");
	else
		appendStringInfoString(str, "INDEX ");
	if (node->idxname)
	{
		appendStringInfoString(str, "\"");
		appendStringInfoString(str, node->idxname);
		appendStringInfoString(str, "\" ");
	}
	appendStringInfoString(str, "ON ");
	_outNode(str, node->relation);

	if (strcmp(node->accessMethod, DEFAULT_INDEX_TYPE))
	{
		appendStringInfoString(str, " USING ");
		appendStringInfoString(str, node->accessMethod);
	}

	appendStringInfoString(str, "(");
	_outNode(str, node->indexParams);
	appendStringInfoString(str, ")");

	if (node->tableSpace)
	{
		appendStringInfoString(str, " TABLESPACE \"");
		appendStringInfoString(str, node->tableSpace);
		appendStringInfoString(str, "\"");
	}

	if (node->whereClause)
	{
		appendStringInfoString(str, " WHERE ");
		_outNode(str, node->whereClause);
	}
}

static void
_outCreateStatsStmt(StringInfo str, CreateStatsStmt *node)
{
}

static void
_outAlterStatsStmt(StringInfo str, const AlterStatsStmt *node)
{
}

static void
_outNotifyStmt(StringInfo str, NotifyStmt *node)
{
	appendStringInfoString(str, "NOTIFY ");
	appendStringInfoString(str, "\"");
	appendStringInfoString(str, node->conditionname);
	appendStringInfoString(str, "\"");
}

static void
_outDeclareCursorStmt(StringInfo str, DeclareCursorStmt *node)
{
	appendStringInfoString(str, "DECLARE \"");
	appendStringInfoString(str, node->portalname);
	appendStringInfoString(str, "\" ");

	if (node->options & CURSOR_OPT_SCROLL)
		appendStringInfoString(str, "SCROLL ");
	if (node->options & CURSOR_OPT_BINARY)
		appendStringInfoString(str, "BINARY ");
	if (node->options & CURSOR_OPT_INSENSITIVE)
		appendStringInfoString(str, "INSENSITIVE ");

	appendStringInfoString(str, "CURSOR ");
	if (node->options & CURSOR_OPT_HOLD)
		appendStringInfoString(str, "WITH HOLD ");
	appendStringInfoString(str, "FOR");
	_outNode(str, node->query);
}

static void
_outSelectStmt(StringInfo str, SelectStmt *node)
{
	if (node->larg)				/* SETOP */
	{
		appendStringInfoString(str, "(");
		_outNode(str, node->larg);
		appendStringInfoString(str, ") ");

		switch (node->op)
		{
			case SETOP_UNION:
				appendStringInfoString(str, " UNION ");
				break;

			case SETOP_INTERSECT:
				appendStringInfoString(str, " INTERSECT ");
				break;

			case SETOP_EXCEPT:
				appendStringInfoString(str, " EXCEPT ");

			default:
				break;
		}

		if (node->all)
			appendStringInfoString(str, "ALL ");

		if (node->rarg)
		{
			appendStringInfoString(str, "(");
			_outNode(str, node->rarg);
			appendStringInfoString(str, ") ");
		}
	}
	else if (node->valuesLists) /* VALUES ... */
	{
		ListCell   *lc;
		int			comma = 0;

		appendStringInfoString(str, " VALUES");
		foreach(lc, node->valuesLists)
		{
			if (comma == 0)
				comma = 1;
			else
				appendStringInfoString(str, ",");

			appendStringInfoString(str, " (");
			_outNode(str, lfirst(lc));
			appendStringInfoString(str, ")");
		}
	}
	else
	{
		if (node->intoClause)
		{
			IntoClause *into = (IntoClause *) node->intoClause;
			RangeVar   *rel = (RangeVar *) into->rel;

			appendStringInfoString(str, "CREATE ");
			if (rel->relpersistence == RELPERSISTENCE_TEMP)
				appendStringInfoString(str, "TEMP ");
			appendStringInfoString(str, "TABLE ");
			_outNode(str, into->rel);

			if (into->colNames)
			{
				appendStringInfoString(str, " (");
				_outNode(str, into->colNames);
				appendStringInfoString(str, ") ");
			}

			if (into->options)
				_outWithDefinition(str, into->options);

			switch (into->onCommit)
			{
				case ONCOMMIT_DROP:
					appendStringInfoString(str, " ON COMMIT DROP");
					break;

				case ONCOMMIT_DELETE_ROWS:
					appendStringInfoString(str, " ON COMMIT DELETE ROWS");
					break;

				case ONCOMMIT_PRESERVE_ROWS:
					appendStringInfoString(str, " ON COMMIT PRESERVE ROWS");
					break;

				default:
					break;
			}

			appendStringInfoString(str, " AS");
		}

		if (node->withClause)
			_outWithClause(str, node->withClause);

		appendStringInfoString(str, " SELECT ");

		if (node->distinctClause)
		{
			appendStringInfoString(str, "DISTINCT ");
			if (lfirst(list_head(node->distinctClause)) != NIL)
			{
				appendStringInfoString(str, "ON (");
				_outNode(str, node->distinctClause);
				appendStringInfoString(str, " ) ");
			}
		}

		_outNode(str, node->targetList);

		if (node->fromClause)
		{
			appendStringInfoString(str, " FROM ");
			_outNode(str, node->fromClause);
		}

		if (node->whereClause)
		{
			appendStringInfoString(str, " WHERE ");
			_outNode(str, node->whereClause);
		}

		if (node->groupClause)
		{
			appendStringInfoString(str, " GROUP BY ");

			if (node->groupDistinct)
				appendStringInfoString(str, "DISTINCT ");

			_outNode(str, node->groupClause);
		}

		if (node->havingClause)
		{
			appendStringInfoString(str, " HAVING ");
			_outNode(str, node->havingClause);
		}

		if (node->windowClause)
		{
			appendStringInfoString(str, " WINDOW ");
			_outNode(str, node->windowClause);
		}
	}

	if (node->sortClause)
	{
		appendStringInfoString(str, " ORDER BY ");
		_outNode(str, node->sortClause);
	}

	if (node->limitOffset)
	{
		appendStringInfoString(str, " OFFSET ");
		_outNode(str, node->limitOffset);
	}

	if (node->limitCount)
	{
		appendStringInfoString(str, " LIMIT ");
		if (IsA(node->limitCount, A_Const) &&
			((A_Const *) node->limitCount)->isnull)
		{
			appendStringInfoString(str, "ALL ");
		}
		else
		{
			_outNode(str, node->limitCount);
		}
	}

	_outNode(str, node->lockingClause);
}

static void
_outFuncCall(StringInfo str, FuncCall *node)
{
	char	   *funcname;

	_outFuncName(str, node->funcname);

	funcname = strVal(lfirst(list_head(node->funcname)));

	if (strcmp(funcname, "user") == 0 ||
		strcmp(funcname, "current_user") == 0 ||
		strcmp(funcname, "session_user") == 0 ||
		strcmp(funcname, "current_role") == 0)
		return;

	appendStringInfoString(str, "(");

	if (node->func_variadic == TRUE)
		appendStringInfoString(str, "VARIADIC ");

	if (node->agg_distinct == TRUE)
		appendStringInfoString(str, "DISTINCT ");

	if (node->agg_star == TRUE)
		appendStringInfoString(str, "*");
	else
		_outNode(str, node->args);

	if (node->agg_order != NIL)
	{
		appendStringInfoString(str, " ORDER BY ");
		_outNode(str, node->agg_order);
	}

	appendStringInfoString(str, ")");

	if (node->over)
	{
		appendStringInfoString(str, " OVER ");
		if (node->over->name)
		{
			appendStringInfoString(str, "\"");
			appendStringInfoString(str, node->over->name);
			appendStringInfoString(str, "\"");
		}
		else
			_outWindowDef(str, node->over);
	}
}

static void
_outDefElem(StringInfo str, DefElem *node)
{

}

static void
_outLockingClause(StringInfo str, LockingClause *node)
{
	if (node == NULL)
		return;

	switch (node->strength)
	{
		case LCS_FORKEYSHARE:
			appendStringInfoString(str, " FOR KEY SHARE");
			break;
		case LCS_FORSHARE:
			appendStringInfoString(str, " FOR SHARE");
			break;
		case LCS_FORNOKEYUPDATE:
			appendStringInfoString(str, " FOR NO KEY UPDATE");
			break;
		case LCS_FORUPDATE:
			appendStringInfoString(str, " FOR UPDATE");
			break;
		case LCS_NONE:
			break;
	}

	if (node->lockedRels)
	{
		appendStringInfoString(str, " OF ");
		_outNode(str, node->lockedRels);
	}

	switch (node->waitPolicy)
	{
		case LockWaitError:
			appendStringInfoString(str, " NOWAIT");
			break;
		case LockWaitSkip:
			appendStringInfoString(str, " SKIP LOCKED");
			break;
		case LockWaitBlock:
			break;
	}
}

static void
_outTriggerTransition(StringInfo str, TriggerTransition *node)
{
}

static void
_outReturnStmt(StringInfo str, ReturnStmt *node)
{
}

static void
_outPLAssignStmt(StringInfo str, PLAssignStmt *node)
{
}

static void
_outColumnDef(StringInfo str, ColumnDef *node)
{
	appendStringInfoString(str, "\"");
	appendStringInfoString(str, node->colname);
	appendStringInfoString(str, "\" ");
	_outNode(str, node->typeName);
	_outNode(str, node->constraints);
}

static void
_outTypeName(StringInfo str, TypeName *node)
{

	/*
	 * don't quote SystemType name, because 1. char is not "char". 2. in 8.4,
	 * interval with fields cause error. =# SELECT '1'::"interval" year;
	 * ERROR:  syntax error at or near "year" LINE 1: SELECT '1'::"interval"
	 * year;
	 */
	if (list_length(node->names) == 2 &&
		strcmp("pg_catalog", strVal(linitial(node->names))) == 0)
	{
		appendStringInfoString(str, strVal(lsecond(node->names)));

		if (strcmp("interval", strVal(lsecond(node->names))) == 0)
		{
			if (node->typmods != NIL)
			{
				A_Const    *v = (A_Const *) linitial(node->typmods);
				int			mask = v->val.ival.ival;

				/*
				 * precision for SECOND field. backward compatibility. use
				 * `'1.2 second'::interval(0) second' not `'1.2
				 * second'::interval second(0)'(standard for 8.4).
				 */
				if ((INTERVAL_MASK(SECOND) & mask) &&
					list_length(node->typmods) == 2)
				{
					appendStringInfoString(str, "(");
					_outA_Const(str, lsecond(node->typmods));
					appendStringInfoString(str, ")");
				}

				/* optional fields */
				if (mask == INTERVAL_MASK(YEAR))
					appendStringInfoString(str, " YEAR");
				else if (mask == INTERVAL_MASK(MONTH))
					appendStringInfoString(str, " MONTH");
				else if (mask == INTERVAL_MASK(DAY))
					appendStringInfoString(str, " DAY");
				else if (mask == INTERVAL_MASK(HOUR))
					appendStringInfoString(str, " HOUR");
				else if (mask == INTERVAL_MASK(MINUTE))
					appendStringInfoString(str, " MINUTE");
				else if (mask == INTERVAL_MASK(SECOND))
					appendStringInfoString(str, " SECOND");
				else if (mask == (INTERVAL_MASK(YEAR) | INTERVAL_MASK(MONTH)))
					appendStringInfoString(str, " YEAR TO MONTH");
				else if (mask == (INTERVAL_MASK(DAY) | INTERVAL_MASK(HOUR)))
					appendStringInfoString(str, " DAY TO HOUR");
				else if (mask == (INTERVAL_MASK(DAY) | INTERVAL_MASK(HOUR) |
								  INTERVAL_MASK(MINUTE)))
					appendStringInfoString(str, " DAY TO MINUTE");
				else if (mask == (INTERVAL_MASK(DAY) | INTERVAL_MASK(HOUR) |
								  INTERVAL_MASK(MINUTE) | INTERVAL_MASK(SECOND)))
					appendStringInfoString(str, " DAY TO SECOND");
				else if (mask == (INTERVAL_MASK(HOUR) | INTERVAL_MASK(MINUTE)))
					appendStringInfoString(str, " HOUR TO MINUTE");
				else if (mask == (INTERVAL_MASK(HOUR) | INTERVAL_MASK(MINUTE) |
								  INTERVAL_MASK(SECOND)))
					appendStringInfoString(str, " HOUR TO SECOND");
				else if (mask == (INTERVAL_MASK(MINUTE) | INTERVAL_MASK(SECOND)))
					appendStringInfoString(str, " MINUTE TO SECOND");
			}

			return;
		}
	}
	else
	{
		ListCell   *lc;
		char		dot = 0;

		foreach(lc, node->names)
		{
			String	   *s = (String *) lfirst(lc);
			char	   *typename = s->sval;

			if (dot == 0)
				dot = 1;
			else
				appendStringInfoString(str, ".");
			if (node->typemod < 0)
			{
				appendStringInfoString(str, "\"");
				appendStringInfoString(str, typename);
				appendStringInfoString(str, "\"");
			}
			else
				appendStringInfoString(str, typename);
		}
	}

	/* precisions */
	if (node->typmods)
	{
		appendStringInfoString(str, "(");
		_outList(str, node->typmods);
		appendStringInfoString(str, ")");
	}

	if (node->arrayBounds != NIL)
	{
		ListCell   *lc;

		foreach(lc, node->arrayBounds)
		{
			if (intVal(lfirst(lc)) == -1)
				appendStringInfoString(str, "[]");
			else
			{
				appendStringInfoString(str, "[");
				_outNode(str, lfirst(lc));
				appendStringInfoString(str, "]");
			}
		}
	}
}

static void
_outTypeCast(StringInfo str, TypeCast *node)
{
	_outNode(str, node->arg);
	appendStringInfoString(str, "::");
	_outNode(str, node->typeName);

}

static void
_outCollateClause(StringInfo str, const CollateClause *node)
{
	if (node->arg)
		_outNode(str, node->arg);
	appendStringInfoString(str, " COLLATE ");
	appendStringInfoString(str, "\"");
	/* XXX collation name can be schema qualified */
	appendStringInfoString(str, strVal(linitial(node->collname)));
	appendStringInfoString(str, "\"");
}

static void
_outIndexElem(StringInfo str, IndexElem *node)
{
	if (node->name)
	{
		appendStringInfoString(str, "\"");
		appendStringInfoString(str, node->name);
		appendStringInfoString(str, "\"");
		if (node->opclass != NIL)
			_outNode(str, node->opclass);
	}
	else
	{
		appendStringInfoString(str, "(");
		_outNode(str, node->expr);
		appendStringInfoString(str, ")");
		if (node->opclass != NIL)
			_outNode(str, node->opclass);
	}
}

static void
_outGroupingSet(StringInfo str, GroupingSet *node)
{
	switch (node->kind)
	{
		case GROUPING_SET_EMPTY:
			appendStringInfoString(str, " (");
			break;
		case GROUPING_SET_ROLLUP:
			appendStringInfoString(str, " ROLLUP (");
			_outNode(str, node->content);
			break;
		case GROUPING_SET_CUBE:
			appendStringInfoString(str, " CUBE (");
			_outNode(str, node->content);
			break;
		case GROUPING_SET_SETS:
			appendStringInfoString(str, " GROUPING SETS (");
			_outNode(str, node->content);
			break;
		default:
			break;
	}

	appendStringInfoString(str, " )");

}

static void
_outWithClause(StringInfo str, WithClause *node)
{
	appendStringInfoString(str, " WITH ");
	if (node->recursive)
		appendStringInfoString(str, "RECURSIVE ");

	_outList(str, node->ctes);
}

static void
_outCTESearchClause(StringInfo str, CTESearchClause *node)
{
}

static void
_outCTECycleClause(StringInfo str, CTECycleClause *node)
{
}

static void
_outCommonTableExpr(StringInfo str, CommonTableExpr *node)
{
	appendStringInfoString(str, "\"");
	appendStringInfoString(str, node->ctename);
	appendStringInfoString(str, "\" ");

	if (node->aliascolnames)
	{
		appendStringInfoString(str, "(");
		_outIdList(str, node->aliascolnames);
		appendStringInfoString(str, ") ");
	}

	appendStringInfoString(str, "AS ");

	if (node->ctematerialized & CTEMaterializeAlways)
		appendStringInfoString(str, "MATERIALIZED ");
	else if (node->ctematerialized & CTEMaterializeNever)
		appendStringInfoString(str, "NOT MATERIALIZED ");

	appendStringInfoString(str, "(");
	_outNode(str, node->ctequery);
	appendStringInfoString(str, ")");
}

static void
_outMergeWhenClauses(StringInfo str, List *node)
{
	ListCell   *temp;

	foreach(temp, node)
	{
		MergeWhenClause *m = (MergeWhenClause *) lfirst(temp);

		if (m->matched)
			appendStringInfoString(str, " WHEN MATCHED ");
		else
			appendStringInfoString(str, " WHEN NOT MATCHED ");

		if (m->condition)
		{
			appendStringInfoString(str, "AND ");
			_outNode(str, m->condition);
		}

		appendStringInfoString(str, "THEN ");

		switch (m->commandType)
		{
			ListCell *s;

			case CMD_UPDATE:
				foreach(s, m->targetList)
				{
					ResTarget *r = (ResTarget *) lfirst(s);
					appendStringInfo(str, "UPDATE SET %s = ", r->name);
					_outNode(str, r->val);
				}
				break;
			default:
				break;
		}
	}
}

static void
_outMergeAction(StringInfo str, MergeAction *node)
{
}

static void
_outSetOperationStmt(StringInfo str, SetOperationStmt *node)
{

}


static void
_outTableSampleClause(StringInfo str, TableSampleClause *node)
{

}

static void
_outA_Expr(StringInfo str, A_Expr *node)
{
	String	   *s;

	switch (node->kind)
	{
		case AEXPR_OP:
			if (list_length(node->name) == 1)
			{
				/*
				 * From PostgreSQL 15, node->name is type of String, rather
				 * than A_Const because "Value" struct has gone.
				 * https://git.postgresql.org/gitweb/?p=postgresql.git;a=commit;h=639a86e36aaecb84faaf941dcd0b183ba0aba9e9
				 */

				String	   *op = (String *) lfirst(list_head(node->name));

				appendStringInfoString(str, " (");
				_outNode(str, node->lexpr);
				appendStringInfoString(str, " ");
				appendStringInfoString(str, strVal(op));
				appendStringInfoString(str, " ");
				_outNode(str, node->rexpr);
				appendStringInfoString(str, " )");
			}
			break;

		case AEXPR_OP_ANY:
			_outNode(str, node->lexpr);
			s = linitial(node->name);
			appendStringInfoString(str, strVal(s));
			appendStringInfoString(str, "ANY(");
			_outNode(str, node->rexpr);
			appendStringInfoString(str, ")");
			break;

		case AEXPR_OP_ALL:
			_outNode(str, node->lexpr);
			s = linitial(node->name);
			appendStringInfoString(str, strVal(s));
			appendStringInfoString(str, "ALL(");
			_outNode(str, node->rexpr);
			appendStringInfoString(str, ")");
			break;

		case AEXPR_DISTINCT:
			appendStringInfoString(str, " (");
			_outNode(str, node->lexpr);
			appendStringInfoString(str, " IS DISTINCT FROM ");
			_outNode(str, node->rexpr);
			appendStringInfoString(str, ")");
			break;

		case AEXPR_NOT_DISTINCT:
			appendStringInfoString(str, " (");
			_outNode(str, node->lexpr);
			appendStringInfoString(str, " IS NOT DISTINCT FROM ");
			_outNode(str, node->rexpr);
			appendStringInfoString(str, ")");
			break;

		case AEXPR_NULLIF:
			appendStringInfoString(str, " NULLIF(");
			_outNode(str, node->lexpr);
			appendStringInfoString(str, ", ");
			_outNode(str, node->rexpr);
			appendStringInfoString(str, ")");
			break;

		case AEXPR_IN:
			_outNode(str, node->lexpr);
			s = (String *) lfirst(list_head(node->name));
			if (*strVal(s) == '=')
				appendStringInfoString(str, " IN (");
			else
				appendStringInfoString(str, " NOT IN (");
			_outNode(str, node->rexpr);
			appendStringInfoString(str, ")");
			break;

		case AEXPR_LIKE:
			_outNode(str, node->lexpr);
			s = lfirst(list_head(node->name));
			if (!strcmp(strVal(s), "~~"))
				appendStringInfoString(str, " LIKE ");
			else
				appendStringInfoString(str, " NOT LIKE ");
			if (IsA(node->rexpr, FuncCall))
			{
				FuncCall   *rexpr = (FuncCall *) node->rexpr;

				_outNode(str, linitial(rexpr->args));
				appendStringInfoString(str, " ESCAPE ");
				_outNode(str, lsecond(rexpr->args));
			}
			else
				_outNode(str, node->rexpr);
			break;

		case AEXPR_ILIKE:
			_outNode(str, node->lexpr);
			s = lfirst(list_head(node->name));
			if (!strcmp(strVal(s), "~~*"))
				appendStringInfoString(str, " ILIKE ");
			else
				appendStringInfoString(str, " NOT ILIKE ");
			if (IsA(node->rexpr, FuncCall))
			{
				FuncCall   *rexpr = (FuncCall *) node->rexpr;

				_outNode(str, linitial(rexpr->args));
				appendStringInfoString(str, " ESCAPE ");
				_outNode(str, lsecond(rexpr->args));
			}
			else
				_outNode(str, node->rexpr);
			break;

		case AEXPR_SIMILAR:
			_outNode(str, node->lexpr);
			s = lfirst(list_head(node->name));
			if (!strcmp(strVal(s), "~"))
				appendStringInfoString(str, " SIMILAR TO ");
			else
				appendStringInfoString(str, " NOT SIMILAR TO ");
			if (IsA(node->rexpr, FuncCall))
			{
				FuncCall   *rexpr = (FuncCall *) node->rexpr;

				_outNode(str, linitial(rexpr->args));
				appendStringInfoString(str, " ESCAPE ");
				_outNode(str, lsecond(rexpr->args));
			}
			else
				_outNode(str, node->rexpr);
			break;

		case AEXPR_BETWEEN:
			_outNode(str, node->lexpr);
			appendStringInfoString(str, " BETWEEN ");
			_outNode(str, linitial((List *) node->rexpr));
			appendStringInfoString(str, " AND ");
			_outNode(str, lsecond((List *) node->rexpr));
			break;

		case AEXPR_NOT_BETWEEN:
			_outNode(str, node->lexpr);
			appendStringInfoString(str, " NOT BETWEEN ");
			_outNode(str, linitial((List *) node->rexpr));
			appendStringInfoString(str, " AND ");
			_outNode(str, lsecond((List *) node->rexpr));
			break;

		case AEXPR_BETWEEN_SYM:
			_outNode(str, node->lexpr);
			appendStringInfoString(str, " BETWEEN SYMMETRIC ");
			_outNode(str, linitial((List *) node->rexpr));
			appendStringInfoString(str, " AND ");
			_outNode(str, lsecond((List *) node->rexpr));
			break;

		case AEXPR_NOT_BETWEEN_SYM:
			_outNode(str, node->lexpr);
			appendStringInfoString(str, " NOT BETWEEN SYMMETRIC ");
			_outNode(str, linitial((List *) node->rexpr));
			appendStringInfoString(str, " AND ");
			_outNode(str, lsecond((List *) node->rexpr));
			break;

		default:
			break;
	}
}

static void
_outInteger(StringInfo str, const Integer *node)
{
    appendStringInfo(str, "%d", node->ival);
}

static void
_outFloat(StringInfo str, const Float *node)
{
    /*
 *      * We assume the value is a valid numeric literal and so does not need
 *           * quoting.
 *                */
    appendStringInfoString(str, node->fval);
}

static void
_outBoolean(StringInfo str, const Boolean *node)
{
    appendStringInfoString(str, node->boolval ? "true" : "false");
}

static void
_outString(StringInfo str, const String *node)
{
	/*
	 * We use outToken to provide escaping of the string's content, but we
	 * don't want it to do anything with an empty string.
	 */
    appendStringInfoChar(str, '"');
    if (node->sval[0] != '\0')
        outToken(str, node->sval);
    appendStringInfoChar(str, '"');
}

static void
_outBitString(StringInfo str, const BitString *node)
{
    /* internal representation already has leading 'b' */
    appendStringInfoString(str, node->bsval);
}

/*
 * Node types found in raw parse trees (supported for debug purposes)
 */

static void
_outRawStmt(StringInfo str, const RawStmt *node)
{
}

static void
_outColumnRef(StringInfo str, ColumnRef *node)
{
	ListCell   *c;
	char		first = 0;

	foreach(c, node->fields)
	{
		Node	   *n = (Node *) lfirst(c);

		if (IsA(n, String))
		{
			String	   *v = (String *) lfirst(c);

			if (first == 0)
				first = 1;
			else
				appendStringInfoString(str, ".");

			appendStringInfoString(str, "\"");
			appendStringInfoString(str, strVal(v));
			appendStringInfoString(str, "\"");
		}
		else if (IsA(n, A_Star))
		{
			if (first == 0)
				first = 1;
			else
				appendStringInfoString(str, ".");

			appendStringInfoString(str, "*");
		}
	}
}

static void
_outParamRef(StringInfo str, ParamRef *node)
{
	char		buf[16];

	snprintf(buf, 16, "%d", node->number);
	appendStringInfoString(str, "$");
	appendStringInfoString(str, buf);
}

static void
_outA_Const(StringInfo str, A_Const *node)
{
	char		*p;

	if (node->isnull)
	{
		appendStringInfoString(str, " NULL");
		return;
	}

	switch (nodeTag(&node->val))
	{
		case T_Integer:
			_outInteger(str, &node->val.ival);
			break;

		case T_Float:
			_outFloat(str, &node->val.fval);
			break;

		case T_String:
			appendStringInfoString(str, "'");
			p = escape_string(node->val.sval.sval);
			appendStringInfoString(str, p);
			pfree(p);
			appendStringInfoString(str, "'");
			break;

		case T_BitString:
			_outBitString(str, &node->val.bsval);
			break;

		case T_Boolean:
			_outBoolean(str, &node->val.boolval);
			break;

		default:
			elog(ERROR, "unrecognized A_Const kind: %d", nodeTag(&node->val));
			break;
	}
}

static void
_outA_Indices(StringInfo str, A_Indices *node)
{
	appendStringInfoString(str, "[");
	if (node->lidx)
	{
		_outNode(str, node->lidx);
		appendStringInfoString(str, ":");
	}
	_outNode(str, node->uidx);
	appendStringInfoString(str, "]");
}

static void
_outA_Indirection(StringInfo str, A_Indirection *node)
{
	ListCell   *lc;

	if (node->indirection != NIL)
	{
		if (IsA(node->arg, ParamRef))
			/* "$1[1]" OR "$1.foo" */
			_outParamRef(str, (ParamRef *) node->arg);
		else
		{
			/* "(ARRAY[1])[1]" */
			appendStringInfoString(str, "(");
			_outNode(str, node->arg);
			appendStringInfoString(str, ")");
		}

		foreach(lc, node->indirection)
		{
			Node	   *ind = lfirst(lc);

			if (IsA(ind, A_Star))
				/* foo.* */
				appendStringInfoString(str, ".*");
			else if (IsA(ind, String))
			{
				/* foo.bar */
				appendStringInfoString(str, ".\"");
				appendStringInfoString(str, strVal(ind));
				appendStringInfoString(str, "\"");
			}
			else
				/* foo[1] (A_Indices) */
				_outNode(str, ind);
		}
	}
}

static void
_outA_ArrayExpr(StringInfo str, A_ArrayExpr *node)
{
	appendStringInfoString(str, "ARRAY [");
	_outNode(str, node->elements);
	appendStringInfoString(str, "]");
}

static void
_outResTarget(StringInfo str, ResTarget *node)
{
	if (node->indirection != NIL)
	{
		appendStringInfoString(str, "\"");
		appendStringInfoString(str, node->name);
		appendStringInfoString(str, "\"=");
		_outNode(str, node->val);
	}
	else
	{
		_outNode(str, node->val);

		if (node->name)
		{
			appendStringInfoString(str, " AS ");
			appendStringInfoString(str, "\"");
			appendStringInfoString(str, node->name);
			appendStringInfoString(str, "\" ");
		}
	}
}


static void
_outMultiAssignRef(StringInfo str, MultiAssignRef *node)
{
	_outNode(str, node->source);
}

static void
_outWindowDef(StringInfo str, WindowDef *node)
{
	if (node->name)
	{
		appendStringInfoString(str, "\"");
		appendStringInfoString(str, node->name);
		appendStringInfoString(str, "\" AS ");
	}
	appendStringInfoString(str, "(");

	if (node->refname)
	{
		appendStringInfoString(str, "\"");
		appendStringInfoString(str, node->refname);
		appendStringInfoString(str, "\" ");
	}

	if (node->partitionClause)
	{
		appendStringInfoString(str, " PARTITION BY ");
		_outNode(str, node->partitionClause);
	}

	if (node->orderClause)
	{
		appendStringInfoString(str, " ORDER BY ");
		_outNode(str, node->orderClause);
	}

	if (node->frameOptions != FRAMEOPTION_DEFAULTS)
	{
		if (node->frameOptions & FRAMEOPTION_RANGE)
			appendStringInfoString(str, " RANGE");
		else if (node->frameOptions & FRAMEOPTION_ROWS)
			appendStringInfoString(str, " ROWS");
		else if (node->frameOptions & FRAMEOPTION_GROUPS)
			appendStringInfoString(str, " GROUPS");

		if (node->frameOptions & FRAMEOPTION_BETWEEN)
			appendStringInfoString(str, " BETWEEN");

		if (node->frameOptions & FRAMEOPTION_START_UNBOUNDED_PRECEDING)
			appendStringInfoString(str, " UNBOUNDED PRECEDING");
		else if (node->frameOptions & FRAMEOPTION_START_UNBOUNDED_FOLLOWING)
			appendStringInfoString(str, " UNBOUNDED FOLLOWING");
		else if (node->frameOptions & FRAMEOPTION_START_CURRENT_ROW)
			appendStringInfoString(str, " UNBOUNDED CURRENT ROW");
		else if (node->frameOptions & FRAMEOPTION_START_OFFSET_PRECEDING)
		{
			appendStringInfoString(str, " ");
			_outNode(str, node->startOffset);
			appendStringInfoString(str, " PRECEDING");
		}
		else if (node->frameOptions & FRAMEOPTION_START_OFFSET_FOLLOWING)
		{
			appendStringInfoString(str, " ");
			_outNode(str, node->startOffset);
			appendStringInfoString(str, " FOLLOWING");
		}

		if (node->frameOptions & FRAMEOPTION_BETWEEN)
		{
			appendStringInfoString(str, " AND");
			if (node->frameOptions & FRAMEOPTION_END_UNBOUNDED_PRECEDING)
				appendStringInfoString(str, " UNBOUNDED PRECEDING");
			else if (node->frameOptions & FRAMEOPTION_END_UNBOUNDED_FOLLOWING)
				appendStringInfoString(str, " UNBOUNDED FOLLOWING");
			else if (node->frameOptions & FRAMEOPTION_END_CURRENT_ROW)
				appendStringInfoString(str, " UNBOUNDED CURRENT ROW");
			else if (node->frameOptions & FRAMEOPTION_END_OFFSET_PRECEDING)
			{
				appendStringInfoString(str, " ");
				_outNode(str, node->endOffset);
				appendStringInfoString(str, " PRECEDING");
			}
			else if (node->frameOptions & FRAMEOPTION_END_OFFSET_FOLLOWING)
			{
				appendStringInfoString(str, " ");
				_outNode(str, node->endOffset);
				appendStringInfoString(str, " FOLLOWING");
			}
		}

		if (node->frameOptions & FRAMEOPTION_EXCLUDE_CURRENT_ROW)
		{
			appendStringInfoString(str, " EXCLUDE CURRENT ROW");
		}
		else if (node->frameOptions & FRAMEOPTION_EXCLUDE_GROUP)
		{
			appendStringInfoString(str, " EXCLUDE GROUP");
		}
		else if (node->frameOptions & FRAMEOPTION_EXCLUDE_TIES)
		{
			appendStringInfoString(str, " EXCLUDE TIES");
		}
	}
	appendStringInfoString(str, ")");
}

static void
_outConstraint(StringInfo str, Constraint *node)
{
	if (node->conname)
	{
		appendStringInfoString(str, "CONSTRAINT \"");
		appendStringInfoString(str, node->conname);
		appendStringInfoString(str, "\"");
	}

	switch (node->contype)
	{
		case CONSTR_CHECK:
			appendStringInfoString(str, " CHECK (");
			_outNode(str, node->raw_expr);
			appendStringInfoString(str, ")");
			break;

		case CONSTR_UNIQUE:
			appendStringInfoString(str, " UNIQUE");
			if (node->keys)
			{
				appendStringInfoString(str, "(");
				_outIdList(str, node->keys);
				appendStringInfoString(str, ")");
			}

			if (node->options)
			{
				_outWithDefinition(str, node->options);
			}

			if (node->indexspace)
			{
				appendStringInfoString(str, " USING INDEX TABLESPACE \"");
				appendStringInfoString(str, node->indexspace);
				appendStringInfoString(str, "\"");
			}
			break;

		case CONSTR_PRIMARY:
			appendStringInfoString(str, " PRIMARY KEY");
			if (node->keys)
			{
				appendStringInfoString(str, "(");
				_outIdList(str, node->keys);
				appendStringInfoString(str, ")");
			}
			if (node->options)
				;

			if (node->indexspace)
			{
				appendStringInfoString(str, " USING INDEX TABLESPACE \"");
				appendStringInfoString(str, node->indexspace);
				appendStringInfoString(str, "\"");
			}
			break;

		case CONSTR_FOREIGN:
			if (node->fk_attrs != NIL)
			{
				appendStringInfoString(str, " FOREIGN KEY(");
				_outIdList(str, node->fk_attrs);
				appendStringInfoString(str, ")");
			}

			appendStringInfoString(str, " REFERENCES ");
			_outNode(str, node->pktable);

			if (node->pk_attrs != NIL)
			{
				appendStringInfoString(str, "(");
				_outIdList(str, node->pk_attrs);
				appendStringInfoString(str, ")");
			}

			switch (node->fk_matchtype)
			{
				case FKCONSTR_MATCH_FULL:
					appendStringInfoString(str, " MATCH FULL");
					break;

				case FKCONSTR_MATCH_PARTIAL:
					appendStringInfoString(str, " MATCH PARTIAL");
					break;

				default:
					break;
			}

			switch (node->fk_upd_action)
			{
				case FKCONSTR_ACTION_RESTRICT:
					appendStringInfoString(str, " ON UPDATE RESTRICT");
					break;

				case FKCONSTR_ACTION_CASCADE:
					appendStringInfoString(str, " ON UPDATE CASCADE");
					break;

				case FKCONSTR_ACTION_SETNULL:
					appendStringInfoString(str, " ON UPDATE SET NULL");
					break;

				case FKCONSTR_ACTION_SETDEFAULT:
					appendStringInfoString(str, " ON UPDATE SET DEFAULT");
					break;

				default:
					break;
			}

			switch (node->fk_del_action)
			{
				case FKCONSTR_ACTION_RESTRICT:
					appendStringInfoString(str, " ON DELETE RESTRICT");
					break;

				case FKCONSTR_ACTION_CASCADE:
					appendStringInfoString(str, " ON DELETE CASCADE");
					break;

				case FKCONSTR_ACTION_SETNULL:
					appendStringInfoString(str, " ON DELETE SET NULL");
					break;

				case FKCONSTR_ACTION_SETDEFAULT:
					appendStringInfoString(str, " ON DELETE SET DEFAULT");
					break;

				default:
					break;
			}

			if (node->deferrable)
				appendStringInfoString(str, " DEFERRABLE");

			if (node->initdeferred)
				appendStringInfoString(str, " INITIALLY DEFERRED");
			break;

		case CONSTR_NOTNULL:
			appendStringInfoString(str, " NOT NULL");
			break;

		case CONSTR_NULL:
			appendStringInfoString(str, " NULL");
			break;

		case CONSTR_DEFAULT:
			appendStringInfoString(str, "DEFAULT ");
			_outNode(str, node->raw_expr);
			break;

		default:
			break;
	}
}


static void
_outSortBy(StringInfo str, SortBy *node)
{
	_outNode(str, node->node);

	if (node->sortby_dir == SORTBY_USING)
	{
		appendStringInfoString(str, " USING ");
		_outNode(str, node->useOp);
	}
	else if (node->sortby_dir == SORTBY_DESC)
		appendStringInfoString(str, " DESC ");

	if (node->sortby_nulls == SORTBY_NULLS_FIRST)
		appendStringInfoString(str, " NULLS FIRST ");
	else if (node->sortby_nulls == SORTBY_NULLS_LAST)
		appendStringInfoString(str, " NULLS LAST ");
}

static void
_outInsertStmt(StringInfo str, InsertStmt *node)
{
	if (node->withClause)
		_outWithClause(str, node->withClause);

	appendStringInfoString(str, "INSERT INTO ");
	_outNode(str, node->relation);

	if (node->cols == NIL && node->selectStmt == NULL)
		appendStringInfoString(str, " DEFAULT VALUES");

	if (node->cols)
	{
		char		comma = 0;
		ListCell   *lc;

		appendStringInfoString(str, "(");

		foreach(lc, node->cols)
		{
			ResTarget  *node = lfirst(lc);
			ListCell   *lc_ind;

			if (comma == 0)
				comma = 1;
			else
				appendStringInfoString(str, ", ");

			appendStringInfoString(str, "\"");
			appendStringInfoString(str, node->name);
			appendStringInfoString(str, "\"");

			foreach(lc_ind, node->indirection)
			{
				Node	   *ind = lfirst(lc_ind);

				if (IsA(ind, String))
				{
					/* foo.bar */
					appendStringInfoString(str, ".\"");
					appendStringInfoString(str, strVal(ind));
					appendStringInfoString(str, "\"");
				}
				else
					/* foo[1] (A_Indices) */
					_outNode(str, ind);
			}
		}
		appendStringInfoString(str, ")");
	}

	if (node->selectStmt)
	{
		_outNode(str, node->selectStmt);
	}

	if (node->onConflictClause)
	{
		_outOnConflictClause(str, node->onConflictClause);
	}

	if (node->returningList)
	{
		appendStringInfoString(str, " RETURNING ");
		_outNode(str, node->returningList);
	}
}

static void
_outSetClause(StringInfo str, List *node)
{
	ListCell   *lc;
	char		comma = 0;

	ResTarget  *first = linitial(node);

	appendStringInfoString(str, " SET ");

	if (IsA(first->val, MultiAssignRef))
	{
		appendStringInfoString(str, "(");
		foreach(lc, node)
		{
			ResTarget  *node = lfirst(lc);
			ListCell   *lc_ind;

			if (comma == 0)
				comma = 1;
			else
				appendStringInfoString(str, ", ");

			appendStringInfoString(str, "\"");
			appendStringInfoString(str, node->name);
			appendStringInfoString(str, "\"");

			foreach(lc_ind, node->indirection)
			{
				Node	   *ind = lfirst(lc_ind);

				if (IsA(ind, String))
				{
					appendStringInfoString(str, ".\"");
					appendStringInfoString(str, strVal(ind));
					appendStringInfoString(str, "\"");
				}
				else
					/* foo[1] (A_Indices) */
					_outNode(str, ind);
			}
		}
		appendStringInfoString(str, ") = ");

		_outNode(str, first->val);

	}
	else
	{
		foreach(lc, node)
		{
			ResTarget  *node = lfirst(lc);
			ListCell   *lc_ind;

			if (comma == 0)
				comma = 1;
			else
				appendStringInfoString(str, ", ");

			appendStringInfoString(str, "\"");
			appendStringInfoString(str, node->name);
			appendStringInfoString(str, "\"");

			foreach(lc_ind, node->indirection)
			{
				Node	   *ind = lfirst(lc_ind);

				if (IsA(ind, String))
				{
					appendStringInfoString(str, ".\"");
					appendStringInfoString(str, strVal(ind));
					appendStringInfoString(str, "\"");
				}
				else
					/* foo[1] (A_Indices) */
					_outNode(str, ind);
			}

			appendStringInfoString(str, " = ");
			_outNode(str, node->val);
		}
	}
}

static void
_outUpdateStmt(StringInfo str, UpdateStmt *node)
{
	if (node->withClause)
		_outWithClause(str, node->withClause);

	appendStringInfoString(str, "UPDATE ");

	_outNode(str, node->relation);

	_outSetClause(str, node->targetList);

	if (node->fromClause)
	{
		appendStringInfoString(str, " FROM ");
		_outNode(str, node->fromClause);
	}

	if (node->whereClause)
	{
		appendStringInfoString(str, " WHERE ");
		_outNode(str, node->whereClause);
	}

	if (node->returningList)
	{
		appendStringInfoString(str, " RETURNING ");
		_outNode(str, node->returningList);
	}
}

static void
_outDeleteStmt(StringInfo str, DeleteStmt *node)
{
	if (node->withClause)
		_outWithClause(str, node->withClause);

	appendStringInfoString(str, "DELETE FROM ");

	_outNode(str, node->relation);

	if (node->usingClause)
	{
		appendStringInfoString(str, " USING ");
		_outNode(str, node->usingClause);
	}

	if (node->whereClause)
	{
		appendStringInfoString(str, " WHERE ");
		_outNode(str, node->whereClause);
	}

	if (node->returningList)
	{
		appendStringInfoString(str, " RETURNING ");
		_outNode(str, node->returningList);
	}
}

static void
_outMergeStmt(StringInfo str, MergeStmt *node)
{
	appendStringInfoString(str, "MERGE INTO ");
	_outRangeVar(str, node->relation);

	if (node->sourceRelation)
	{
		appendStringInfoString(str, " USING ");
		_outNode(str, node->sourceRelation);
	}

	if (node->joinCondition)
	{
		appendStringInfoString(str, " ON ");
		_outNode(str, node->joinCondition);
	}

	if (node->mergeWhenClauses != NIL)
	{
		_outMergeWhenClauses(str, node->mergeWhenClauses);
	}
}

static void
_outTransactionStmt(StringInfo str, TransactionStmt *node)
{
	switch (node->kind)
	{
		case TRANS_STMT_BEGIN:
			appendStringInfoString(str, "BEGIN ");
			break;

		case TRANS_STMT_START:
			appendStringInfoString(str, "START TRANSACTION ");
			break;

		case TRANS_STMT_COMMIT:
			appendStringInfoString(str, "COMMIT ");
			break;

		case TRANS_STMT_ROLLBACK:
			appendStringInfoString(str, "ABORT ");
			break;

		case TRANS_STMT_SAVEPOINT:
			appendStringInfoString(str, "SAVEPOINT ");
			break;

		case TRANS_STMT_RELEASE:
			appendStringInfoString(str, "RELEASE ");
			break;

		case TRANS_STMT_ROLLBACK_TO:
			appendStringInfoString(str, "ROLLBACK TO ");
			break;

		case TRANS_STMT_PREPARE:
			appendStringInfoString(str, "PREPARE TRANSACTION ");
			break;

		case TRANS_STMT_COMMIT_PREPARED:
			appendStringInfoString(str, "COMMIT PREPARED ");
			break;

		case TRANS_STMT_ROLLBACK_PREPARED:
			appendStringInfoString(str, "ROLLBACK PREPARED ");
			break;

		default:
			break;
	}

	if (node->options)
		_outSetTransactionModeList(str, node->options);

	if (node->gid)
		appendStringInfoString(str, node->gid);
}


static void
_outTruncateStmt(StringInfo str, TruncateStmt *node)
{
	appendStringInfoString(str, "TRUNCATE ");
	_outNode(str, node->relations);
}

static void
_outVacuumStmt(StringInfo str, VacuumStmt *node)
{

	VacuumParams params;
	params.options = node->is_vacuumcmd ? VACOPT_VACUUM : VACOPT_ANALYZE;

	if (params.options & VACOPT_VACUUM)
		appendStringInfoString(str, "VACUUM ");
	else
		appendStringInfoString(str, "ANALYZE ");

	if (params.options & VACOPT_FULL)
		appendStringInfoString(str, "FULL ");

	if (params.options & VACOPT_FREEZE)
		appendStringInfoString(str, "FREEZE ");

	if (params.options & VACOPT_VERBOSE)
		appendStringInfoString(str, "VERBOSE ");

	if (params.options & VACOPT_VACUUM && params.options & VACOPT_ANALYZE)
		appendStringInfoString(str, "ANALYZE ");

	if (params.options & VACOPT_DISABLE_PAGE_SKIPPING)
		appendStringInfoString(str, "DISABLE_PAGE_SKIPPING ");

	if (params.options & VACOPT_SKIP_LOCKED)
		appendStringInfoString(str, "SKIP_LOCKED ");

	ListCell   *lc;
	foreach(lc, node->rels)
	{
		VacuumRelation *vrel = lfirst_node(VacuumRelation, lc);

		_outNode(str, vrel->relation);
		if (vrel->va_cols)
		{
			appendStringInfoString(str, "(");
			_outIdList(str, vrel->va_cols);
			appendStringInfoString(str, ") ");
		}
	}
}

static void
_outExplainStmt(StringInfo str, ExplainStmt *node)
{
	ListCell   *lc;

	appendStringInfoString(str, "EXPLAIN ");

	if (server_version_num < 90000)
	{
		foreach(lc, node->options)
		{
			DefElem    *opt = (DefElem *) lfirst(lc);

			if (strcmp(opt->defname, "analyze") == 0)
				appendStringInfoString(str, "ANALYZE ");
			else if (strcmp(opt->defname, "verbose") == 0)
				appendStringInfoString(str, "VERBOSE ");
		}
	}
	else
	{
		if (node->options)
		{
			appendStringInfoString(str, "(");
			foreach(lc, node->options)
			{
				DefElem    *opt = (DefElem *) lfirst(lc);

				if (list_head(node->options) != lc)
					appendStringInfoString(str, ", ");

				appendStringInfoString(str, opt->defname);
				appendStringInfoString(str, " ");
				if (opt->arg)
					_outNode(str, opt->arg);
			}
			appendStringInfoString(str, ")");
		}
	}

	_outNode(str, node->query);
}

static void
_outClusterStmt(StringInfo str, ClusterStmt *node)
{
	appendStringInfoString(str, "CLUSTER ");

	if (node->indexname)
	{
		appendStringInfoString(str, "\"");
		appendStringInfoString(str, node->indexname);
		appendStringInfoString(str, "\" ON ");
	}
	if (node->relation)
		_outNode(str, node->relation);
}

static void
_outCheckPointStmt(StringInfo str, CheckPointStmt *node)
{
	appendStringInfoString(str, "CHECKPOINT");
}

static void
_outClosePortalStmt(StringInfo str, ClosePortalStmt *node)
{
	appendStringInfoString(str, "CLOSE ");
	appendStringInfoString(str, "\"");
	appendStringInfoString(str, node->portalname);
	appendStringInfoString(str, "\"");
}

static void
_outListenStmt(StringInfo str, ListenStmt *node)
{
	appendStringInfoString(str, "LISTEN ");
	appendStringInfoString(str, "\"");
	appendStringInfoString(str, node->conditionname);
	appendStringInfoString(str, "\"");
}

static void
_outUnlistenStmt(StringInfo str, UnlistenStmt *node)
{
	appendStringInfoString(str, "UNLISTEN ");
	if (node->conditionname == NULL)
		appendStringInfoString(str, "*");
	else
	{
		appendStringInfoString(str, "\"");
		appendStringInfoString(str, node->conditionname);
		appendStringInfoString(str, "\"");
	}
}

static void
_outLoadStmt(StringInfo str, LoadStmt *node)
{
	appendStringInfoString(str, "LOAD '");
	appendStringInfoString(str, node->filename);
	appendStringInfoString(str, "'");
}

static void
_outCopyStmt(StringInfo str, CopyStmt *node)
{
	ListCell   *lc;

	appendStringInfoString(str, "COPY ");

	if (node->query)
	{
		appendStringInfoString(str, "(");
		_outNode(str, node->query);
		appendStringInfoString(str, ")");
	}

	_outNode(str, node->relation);

	if (node->attlist)
	{
		appendStringInfoString(str, "(");
		_outIdList(str, node->attlist);
		appendStringInfoString(str, ")");
	}

	if (node->is_from == TRUE)
		appendStringInfoString(str, " FROM ");
	else
		appendStringInfoString(str, " TO ");

	if (node->filename)
	{
		appendStringInfoString(str, "'");
		appendStringInfoString(str, node->filename);
		appendStringInfoString(str, "' ");
	}
	else
		appendStringInfoString(str, node->is_from == TRUE ? "STDIN " : "STDOUT ");

	if (server_version_num < 90000)
	{
		foreach(lc, node->options)
		{
			DefElem    *e = lfirst(lc);

			if (strcmp(e->defname, "format") == 0)
			{
				char	   *fmt = strVal(e->arg);

				if (strcmp(fmt, "text") == 0)
					;
				else if (strcmp(fmt, "binary") == 0)
					appendStringInfoString(str, "BINARY ");
				else if (strcmp(fmt, "csv") == 0)
					appendStringInfoString(str, "CSV ");
			}
			else if (strcmp(e->defname, "oids") == 0)
				appendStringInfoString(str, "OIDS ");
			else if (strcmp(e->defname, "delimiter") == 0)
			{
				appendStringInfoString(str, "DELIMITERS ");
				_outNode(str, e->arg);
				appendStringInfoString(str, " ");
			}
			else if (strcmp(e->defname, "null") == 0)
			{
				appendStringInfoString(str, "NULL ");
				_outNode(str, e->arg);
				appendStringInfoString(str, " ");
			}
			else if (strcmp(e->defname, "header") == 0)
				appendStringInfoString(str, "HEADER ");
			else if (strcmp(e->defname, "quote") == 0)
			{
				appendStringInfoString(str, "QUOTE ");
				_outNode(str, e->arg);
				appendStringInfoString(str, " ");
			}
			else if (strcmp(e->defname, "escape") == 0)
			{
				appendStringInfoString(str, "ESCAPE ");
				_outNode(str, e->arg);
				appendStringInfoString(str, " ");
			}
			else if (strcmp(e->defname, "force_quote") == 0)
			{
				appendStringInfoString(str, "FORCE QUOTE ");
				_outIdList(str, (List *) e->arg);
			}
			else if (strcmp(e->defname, "force_not_null") == 0)
			{
				appendStringInfoString(str, "FORCE NOT NULL ");
				_outIdList(str, (List *) e->arg);
			}
		}
	}
	else
	{
		/* version_num >= 90000 */
		if (node->options)
		{
			appendStringInfoString(str, "(");

			foreach(lc, node->options)
			{
				DefElem    *e = lfirst(lc);

				if (list_head(node->options) != lc)
					appendStringInfoString(str, ", ");

				appendStringInfoString(str, e->defname);
				appendStringInfoString(str, " ");

				if (strcmp(e->defname, "format") == 0
					|| strcmp(e->defname, "oids") == 0
					|| strcmp(e->defname, "delimiter") == 0
					|| strcmp(e->defname, "null") == 0
					|| strcmp(e->defname, "header") == 0
					|| strcmp(e->defname, "quote") == 0
					|| strcmp(e->defname, "escape") == 0)
					_outNode(str, e->arg);
				else if (strcmp(e->defname, "force_not_null") == 0)
				{
					appendStringInfoString(str, "(");
					_outIdList(str, (List *) e->arg);
					appendStringInfoString(str, ")");
				}
				else if (strcmp(e->defname, "force_quote") == 0)
				{
					if (IsA(e->arg, A_Star))
						appendStringInfoString(str, "*");
					else if (IsA(e->arg, List))
					{
						appendStringInfoString(str, "(");
						_outIdList(str, (List *) e->arg);
						appendStringInfoString(str, ")");
					}
				}
			}
			appendStringInfoString(str, ")");
		}
	}

	if (node->whereClause)
	{
		appendStringInfoString(str, " WHERE ");
		_outNode(str, node->whereClause);
	}
}

static void
_outDeallocateStmt(StringInfo str, DeallocateStmt *node)
{
	appendStringInfoString(str, "DEALLOCATE \"");
	appendStringInfoString(str, node->name);
	appendStringInfoString(str, "\"");
}

static void
_outRenameStmt(StringInfo str, RenameStmt *node)
{
	ListCell   *lc;
	char		comma = 0;

	appendStringInfoString(str, "ALTER ");

	switch (node->renameType)
	{
		case OBJECT_AGGREGATE:
			appendStringInfoString(str, "AGGREGATE ");
			_outNode(str, node->object);
			appendStringInfoString(str, " (");
			appendStringInfoString(str, ") RENAME TO \"");
			appendStringInfoString(str, node->newname);
			appendStringInfoString(str, "\"");
			break;

		case OBJECT_CONVERSION:
			appendStringInfoString(str, "CONVERSION ");
			_outNode(str, node->object);
			appendStringInfoString(str, " RENAME TO \"");
			appendStringInfoString(str, node->newname);
			appendStringInfoString(str, "\"");
			break;

		case OBJECT_DATABASE:
			appendStringInfoString(str, "DATABASE \"");
			appendStringInfoString(str, node->subname);
			appendStringInfoString(str, "\" RENAME TO \"");
			appendStringInfoString(str, node->newname);
			appendStringInfoString(str, "\"");
			break;

		case OBJECT_FUNCTION:
			appendStringInfoString(str, "FUNCTION ");

			foreach(lc, castNode(List, node->object))
			{
				Node	   *n = lfirst(lc);

				if (IsA(n, String))
				{
					String	   *value = (String *) n;

					if (comma == 0)
						comma = 1;
					else
						appendStringInfoString(str, ".");
					appendStringInfoString(str, "\"");
					appendStringInfoString(str, value->sval);
					appendStringInfoString(str, "\"");
				}
				else
					_outNode(str, n);
			}

			appendStringInfoString(str, "(");
			appendStringInfoString(str, ")");
			appendStringInfoString(str, " RENAME TO \"");
			appendStringInfoString(str, node->newname);
			appendStringInfoString(str, "\"");
			break;

		case OBJECT_ROLE:
			appendStringInfoString(str, "ROLE \"");
			appendStringInfoString(str, node->subname);
			appendStringInfoString(str, "\" RENAME TO \"");
			appendStringInfoString(str, node->newname);
			appendStringInfoString(str, "\"");
			break;

		case OBJECT_LANGUAGE:
			appendStringInfoString(str, "LANGUAGE \"");
			appendStringInfoString(str, node->subname);
			appendStringInfoString(str, "\" RENAME TO \"");
			appendStringInfoString(str, node->newname);
			appendStringInfoString(str, "\"");
			break;

		case OBJECT_OPCLASS:
			appendStringInfoString(str, "OPERATOR CLASS ");
			_outNode(str, node->object);
			appendStringInfoString(str, " USING ");
			appendStringInfoString(str, node->subname);
			appendStringInfoString(str, " RENAME TO \"");
			appendStringInfoString(str, node->newname);
			appendStringInfoString(str, "\"");
			break;

		case OBJECT_SCHEMA:
			appendStringInfoString(str, "SCHEMA \"");
			appendStringInfoString(str, node->subname);
			appendStringInfoString(str, "\" RENAME TO \"");
			appendStringInfoString(str, node->newname);
			appendStringInfoString(str, "\"");
			break;

		case OBJECT_TABLE:
			appendStringInfoString(str, "TABLE ");
			_outNode(str, node->relation);
			appendStringInfoString(str, " RENAME TO \"");
			appendStringInfoString(str, node->newname);
			appendStringInfoString(str, "\"");
			break;

		case OBJECT_INDEX:
			appendStringInfoString(str, "INDEX ");
			_outNode(str, node->relation);
			appendStringInfoString(str, " RENAME TO \"");
			appendStringInfoString(str, node->newname);
			appendStringInfoString(str, "\"");
			break;

		case OBJECT_COLUMN:
			appendStringInfoString(str, "TABLE ");
			_outNode(str, node->relation);
			appendStringInfoString(str, " RENAME \"");
			appendStringInfoString(str, node->subname);
			appendStringInfoString(str, "\" TO \"");
			appendStringInfoString(str, node->newname);
			appendStringInfoString(str, "\"");
			break;

		case OBJECT_TRIGGER:
			appendStringInfoString(str, "TRIGGER \"");
			appendStringInfoString(str, node->subname);
			appendStringInfoString(str, "\" ON ");
			_outNode(str, node->relation);
			appendStringInfoString(str, " RENAME TO \"");
			appendStringInfoString(str, node->newname);
			appendStringInfoString(str, "\"");
			break;

		case OBJECT_TABLESPACE:
			appendStringInfoString(str, "TABLESPACE \"");
			appendStringInfoString(str, node->subname);
			appendStringInfoString(str, "\" RENAME TO \"");
			appendStringInfoString(str, node->newname);
			appendStringInfoString(str, "\"");
			break;

		default:
			break;
	}
}

static void
_outOptRoleList(StringInfo str, List *options)
{
	ListCell   *lc;

	foreach(lc, options)
	{
		DefElem    *elem = lfirst(lc);
		A_Const	   *value = (A_Const *) elem->arg;

		if (strcmp(elem->defname, "password") == 0)
		{
			if (value == NULL)
				appendStringInfoString(str, " PASSWORD NULL");
			else
			{
				appendStringInfoString(str, " PASSWORD '");
				appendStringInfoString(str, value->val.sval.sval);
				appendStringInfoString(str, "'");
			}
		}
		else if (strcmp(elem->defname, "encryptedPassword") == 0)
		{
			appendStringInfoString(str, " ENCRYPTED PASSWORD '");
			appendStringInfoString(str, value->val.sval.sval);
			appendStringInfoString(str, "'");
		}
		else if (strcmp(elem->defname, "unencryptedPassword") == 0)
		{
			appendStringInfoString(str, " UNENCRYPTED PASSWORD '");
			appendStringInfoString(str, value->val.sval.sval);
			appendStringInfoString(str, "'");
		}
		else if (strcmp(elem->defname, "superuser") == 0)
		{
			if (value->val.boolval.boolval)
				appendStringInfoString(str, " SUPERUSER");
			else
				appendStringInfoString(str, " NOSUPERUSER");
		}
		else if (strcmp(elem->defname, "inherit") == 0)
		{
			if (value->val.boolval.boolval)
				appendStringInfoString(str, " INHERIT");
			else
				appendStringInfoString(str, " NOINHERIT");
		}
		else if (strcmp(elem->defname, "createdb") == 0)
		{
			if (value->val.boolval.boolval)
				appendStringInfoString(str, " CREATEDB");
			else
				appendStringInfoString(str, " NOCREATEDB");
		}
		else if (strcmp(elem->defname, "createrole") == 0)
		{
			if (value->val.boolval.boolval)
				appendStringInfoString(str, " CREATEROLE");
			else
				appendStringInfoString(str, " NOCREATEROLE");
		}
		else if (strcmp(elem->defname, "canlogin") == 0)
		{
			if (value->val.boolval.boolval)
				appendStringInfoString(str, " LOGIN");
			else
				appendStringInfoString(str, " NOLOGIN");
		}
		else if (strcmp(elem->defname, "connectionlimit") == 0)
		{
			char		buf[16];

			appendStringInfoString(str, " CONNECTION LIMIT ");
			snprintf(buf, 16, "%d", value->val.ival.ival);
			appendStringInfoString(str, buf);
		}
		else if (strcmp(elem->defname, "validUntil") == 0)
		{
			appendStringInfoString(str, " VALID UNTIL '");
			appendStringInfoString(str, value->val.sval.sval);
			appendStringInfoString(str, "'");
		}
		else if (strcmp(elem->defname, "rolemembers") == 0)
		{
			appendStringInfoString(str, " ROLE ");
			_outIdList(str, (List *) elem->arg);
		}
		else if (strcmp(elem->defname, "sysid") == 0)
		{
			char		buf[16];

			appendStringInfoString(str, " SYSID ");
			snprintf(buf, 16, "%d", value->val.ival.ival);
			appendStringInfoString(str, buf);
		}
		else if (strcmp(elem->defname, "adminmembers") == 0)
		{
			appendStringInfoString(str, " ADMIN ");
			_outIdList(str, (List *) elem->arg);
		}
		else if (strcmp(elem->defname, "addroleto") == 0)
		{
			appendStringInfoString(str, " IN ROLE ");
			_outIdList(str, (List *) elem->arg);
		}
	}
}

static void
_outCreateRoleStmt(StringInfo str, CreateRoleStmt *node)
{
	appendStringInfoString(str, "CREATE ");
	switch (node->stmt_type)
	{
		case ROLESTMT_ROLE:
			appendStringInfoString(str, "ROLE \"");
			break;

		case ROLESTMT_USER:
			appendStringInfoString(str, "USER \"");
			break;

		case ROLESTMT_GROUP:
			appendStringInfoString(str, "GROUP \"");
			break;
	}
	_outNode(str, node->role);
	appendStringInfoString(str, "\"");

	_outOptRoleList(str, node->options);
}

static void
_outAlterRoleStmt(StringInfo str, AlterRoleStmt *node)
{
	appendStringInfoString(str, "ALTER ROLE \"");
	_outNode(str, node->role);
	appendStringInfoString(str, "\"");

	if (node->options)
		_outOptRoleList(str, node->options);
}

static void
_outAlterRoleSetStmt(StringInfo str, AlterRoleSetStmt *node)
{
	appendStringInfoString(str, "ALTER ROLE \"");
	_outNode(str, node->role);
	appendStringInfoString(str, "\" ");

	if (node->setstmt)
	{
		_outNode(str, node->setstmt);
	}
}
static void
_outRoleSpec(StringInfo str, RoleSpec *node)
{
	appendStringInfoString(str, node->rolename);
}

static void
_outSetTransactionModeList(StringInfo str, List *list)
{
	ListCell   *lc;
	char		comma = 0;

	foreach(lc, list)
	{
		DefElem    *elem = lfirst(lc);

		if (comma == 0)
			comma = 1;
		else
			appendStringInfoString(str, ",");

		if (strcmp(elem->defname, "transaction_isolation") == 0)
		{
			A_Const    *v = (A_Const *) elem->arg;

			appendStringInfoString(str, " ISOLATION LEVEL ");
			appendStringInfoString(str, v->val.sval.sval);
		}
		else if (strcmp(elem->defname, "transaction_read_only") == 0)
		{
			A_Const    *n = (A_Const *) elem->arg;

			if (n->val.boolval.boolval)
				appendStringInfoString(str, "READ ONLY ");
			else
				appendStringInfoString(str, "READ WRITE ");
		}
	}
}


static void
_outSetRest(StringInfo str, VariableSetStmt *node)
{
	if (strcmp(node->name, "timezone") == 0)
	{
		appendStringInfoString(str, "TIME ZONE ");
		if (node->kind != VAR_RESET)
			_outNode(str, node->args);
	}
	else if (strcmp(node->name, "TRANSACTION") == 0)
	{
		appendStringInfoString(str, "TRANSACTION ");
		_outSetTransactionModeList(str, node->args);
	}
	else if (strcmp(node->name, "SESSION CHARACTERISTICS") == 0)
	{
		appendStringInfoString(str, "SESSION CHARACTERISTICS AS TRANSACTION ");
		_outSetTransactionModeList(str, node->args);
	}
	else if (strcmp(node->name, "role") == 0)
	{
		appendStringInfoString(str, "ROLE ");
		if (node->kind != VAR_RESET)
			_outNode(str, node->args);
	}
	else if (strcmp(node->name, "session_authorization") == 0)
	{
		appendStringInfoString(str, "SESSION AUTHORIZATION ");
		if (node->args == NIL && node->kind != VAR_RESET)
			appendStringInfoString(str, "DEFAULT");
		else
			_outNode(str, node->args);
	}
	else if (strcmp(node->name, "transaction_isolation") == 0)
	{
		appendStringInfoString(str, "TRANSACTION ISOLATION LEVEL");
		if (node->kind != VAR_RESET)
			_outSetTransactionModeList(str, node->args);
	}
	else if (strcmp(node->name, "xmloption") == 0)
	{
		A_Const    *v = linitial(node->args);

		appendStringInfoString(str, "XML OPTION ");
		appendStringInfoString(str, v->val.sval.sval);
	}
	else
	{
		appendStringInfoString(str, node->name);
		if (node->kind != VAR_RESET)
		{
			if (node->kind == VAR_SET_CURRENT)
			{
				appendStringInfoString(str, " FROM CURRENT");
			}
			else
			{
				appendStringInfoString(str, " TO ");
				if (node->args == NULL)
				{
					appendStringInfoString(str, "DEFAULT");
				}
				else
					_outNode(str, node->args);
			}
		}
	}
}

static void
_outDropRoleStmt(StringInfo str, DropRoleStmt *node)
{
	appendStringInfoString(str, "DROP ROLE ");
	if (node->missing_ok == TRUE)
		appendStringInfoString(str, "IF EXISTS ");
	_outIdList(str, node->roles);
}

static void
_outCreateSchemaStmt(StringInfo str, CreateSchemaStmt *node)
{
	appendStringInfoString(str, "CREATE SCHEMA \"");
	appendStringInfoString(str, node->schemaname);
	appendStringInfoString(str, "\"");
	if (node->authrole)
	{
		appendStringInfoString(str, "AUTHORIZATION \"");
		_outNode(str, node->authrole);
		appendStringInfoString(str, "\" ");
	}
	_outNode(str, node->schemaElts);
}

static void
_outVariableSetStmt(StringInfo str, VariableSetStmt *node)
{
	if (node->kind == VAR_RESET_ALL)
	{
		appendStringInfoString(str, "RESET ALL");
		return;
	}

	if (node->kind == VAR_RESET)
		appendStringInfoString(str, "RESET ");
	else
		appendStringInfoString(str, "SET ");

	if (node->is_local)
		appendStringInfoString(str, "LOCAL ");

	_outSetRest(str, node);
}

static void
_outVariableShowStmt(StringInfo str, VariableShowStmt *node)
{
	if (strcmp(node->name, "timezone") == 0)
		appendStringInfoString(str, "SHOW TIME ZONE");
	else if (strcmp(node->name, "transaction_isolation") == 0)
		appendStringInfoString(str, "SHOW TRANSACTION ISOLATION LEVEL");
	else if (strcmp(node->name, "session_authorization") == 0)
		appendStringInfoString(str, "SHOW SESSION AUTHORIZATION");
	else if (strcmp(node->name, "all") == 0)
		appendStringInfoString(str, "SHOW ALL");
	else
	{
		appendStringInfoString(str, "SHOW ");
		appendStringInfoString(str, node->name);
	}
}

static void
_outConstraintsSetStmt(StringInfo str, ConstraintsSetStmt *node)
{
	appendStringInfoString(str, "SET CONSTRAINTS ");

	if (node->constraints == NIL)
		appendStringInfoString(str, "ALL");
	else
		_outNode(str, node->constraints);

	appendStringInfoString(str, node->deferred == TRUE ? " DEFERRED" : " IMMEDIATE");
}

static void
_outAlterTableCmd(StringInfo str, AlterTableCmd *node)
{
	char		buf[16];

	switch (node->subtype)
	{
		case AT_AddColumn:
			appendStringInfoString(str, "ADD ");
			_outNode(str, node->def);
			break;

		case AT_ColumnDefault:
			appendStringInfoString(str, "ALTER \"");
			appendStringInfoString(str, node->name);
			appendStringInfoString(str, "\" ");
			if (node->def == NULL)
				appendStringInfoString(str, "DROP DEFAULT");
			else
			{
				appendStringInfoString(str, "SET DEFAULT ");
				_outNode(str, node->def);
			}
			break;

		case AT_DropNotNull:
			appendStringInfoString(str, "ALTER \"");
			appendStringInfoString(str, node->name);
			appendStringInfoString(str, "\" DROP NOT NULL");
			break;

		case AT_SetNotNull:
			appendStringInfoString(str, "ALTER \"");
			appendStringInfoString(str, node->name);
			appendStringInfoString(str, "\" SET NOT NULL");
			break;

		case AT_SetStatistics:
			appendStringInfoString(str, "ALTER \"");
			appendStringInfoString(str, node->name);
			appendStringInfoString(str, "\" SET STATISTICS ");
			snprintf(buf, 16, "%d", ((A_Const *) node->def)->val.ival.ival);
			appendStringInfoString(str, buf);
			break;

		case AT_SetStorage:
			appendStringInfoString(str, "ALTER \"");
			appendStringInfoString(str, node->name);
			appendStringInfoString(str, "\" SET STORAGE ");
			appendStringInfoString(str, ((A_Const *) node->def)->val.sval.sval);
			break;

		case AT_DropColumn:
			appendStringInfoString(str, "DROP \"");
			appendStringInfoString(str, node->name);
			appendStringInfoString(str, "\" ");
			if (node->behavior == DROP_CASCADE)
				appendStringInfoString(str, "CASCADE");
			break;

		case AT_AlterColumnType:
			appendStringInfoString(str, "ALTER \"");
			appendStringInfoString(str, node->name);
			appendStringInfoString(str, "\" TYPE ");
			_outNode(str, node->def);
			break;

		case AT_AddConstraint:
			appendStringInfoString(str, "ADD ");
			_outNode(str, node->def);
			break;

		case AT_DropConstraint:
			appendStringInfoString(str, "DROP CONSTRAINT \"");
			appendStringInfoString(str, node->name);
			appendStringInfoString(str, "\"");
			if (node->behavior == DROP_CASCADE)
				appendStringInfoString(str, " CASCADE");
			break;

		case AT_DropOids:
			appendStringInfoString(str, "SET WITHOUT OIDS");
			break;

		case AT_SetAccessMethod:
			appendStringInfoString(str, "SET ACCESS METHOD");
			break;

		case AT_ClusterOn:
			appendStringInfoString(str, "CLUSTER ON \"");
			appendStringInfoString(str, node->name);
			appendStringInfoString(str, "\"");
			break;

		case AT_DropCluster:
			appendStringInfoString(str, "SET WITHOUT CLUSTER");
			break;

		case AT_EnableTrig:
			appendStringInfoString(str, "ENABLE TRIGGER \"");
			appendStringInfoString(str, node->name);
			appendStringInfoString(str, "\"");
			break;

		case AT_EnableAlwaysTrig:
			/* not implemented */
			break;

		case AT_EnableReplicaTrig:
			/* not implemented */
			break;

		case AT_EnableTrigAll:
			appendStringInfoString(str, "ENABLE TRIGGER ALL");
			break;

		case AT_EnableTrigUser:
			appendStringInfoString(str, "ENABLE TRIGGER USER");
			break;

		case AT_DisableTrig:
			appendStringInfoString(str, "DISABLE TRIGGER \"");
			appendStringInfoString(str, node->name);
			appendStringInfoString(str, "\"");
			break;

		case AT_DisableTrigAll:
			appendStringInfoString(str, "DISABLE TRIGGER ALL");
			break;

		case AT_DisableTrigUser:
			appendStringInfoString(str, "DISABLE TRIGGER USER");
			break;

		case AT_EnableRule:
			/* not implemented */
			break;

		case AT_EnableReplicaRule:
			/* not implemented */
			break;

		case AT_EnableAlwaysRule:
			/* not implemented */
			break;

		case AT_DisableRule:
			/* not implemented */
			break;

		case AT_AddInherit:
			/* not implemented */
			break;

		case AT_ChangeOwner:
			appendStringInfoString(str, "OWNER TO \"");
			appendStringInfoString(str, node->name);
			appendStringInfoString(str, "\"");
			break;

		case AT_SetTableSpace:
			appendStringInfoString(str, "SET TABLESPACE \"");
			appendStringInfoString(str, node->name);
			appendStringInfoString(str, "\"");
			break;

		case AT_SetRelOptions:
			/* not implemented */
			break;

		case AT_ResetRelOptions:
			/* not implemented */
			break;

		default:
			break;
	}
}

static void
_outAlterTableStmt(StringInfo str, AlterTableStmt *node)
{
	if (node->objtype == OBJECT_TABLE)
		appendStringInfoString(str, "ALTER TABLE ");
	else
		appendStringInfoString(str, "ALTER INDEX ");

	_outNode(str, node->relation);
	appendStringInfoString(str, " ");
	_outNode(str, node->cmds);
}

static void
_outOptSeqList(StringInfo str, List *options)
{
	ListCell   *lc;

	foreach(lc, options)
	{
		DefElem    *e = lfirst(lc);
		A_Const	   *v = (A_Const *) e->arg;
		char		buf[16];

		if (strcmp(e->defname, "cycle") == 0)
		{
			if (v->val.boolval.boolval)
				appendStringInfoString(str, " CYCLE");
			else
				appendStringInfoString(str, " NO CYCLE");
		}
		else if (strcmp(e->defname, "minvalue") == 0 && !v)
			appendStringInfoString(str, " NO MINVALUE");
		else if (strcmp(e->defname, "maxvalue") == 0 && !v)
			appendStringInfoString(str, " NO MAXVALUE");
		else if (strcmp(e->defname, "owned_by") == 0)
		{
			appendStringInfoString(str, " OWNED BY ");
			_outIdList(str, (List *) e->arg);
		}
		else
		{
			if (strcmp(e->defname, "cache") == 0)
				appendStringInfoString(str, " CACHE ");
			else if (strcmp(e->defname, "increment") == 0)
				appendStringInfoString(str, " INCREMENT ");
			else if (strcmp(e->defname, "maxvalue") == 0 && v)
				appendStringInfoString(str, " MAXVALUE ");
			else if (strcmp(e->defname, "minvalue") == 0 && v)
				appendStringInfoString(str, " MINVALUE ");
			else if (strcmp(e->defname, "start") == 0)
				appendStringInfoString(str, " START ");
			else if (strcmp(e->defname, "restart") == 0)
				appendStringInfoString(str, " RESTART ");

			if (IsA(e->arg, String))
				appendStringInfoString(str, v->val.sval.sval);
			else if (IsA(e->arg, Integer))
			{
				snprintf(buf, 16, "%d", v->val.ival.ival);
				appendStringInfoString(str, buf);
			}
		}
	}
}

static void
_outCreateSeqStmt(StringInfo str, CreateSeqStmt *node)
{
	appendStringInfoString(str, "CREATE ");
	if (node->sequence->relpersistence == RELPERSISTENCE_TEMP)
		appendStringInfoString(str, "TEMP ");
	appendStringInfoString(str, "SEQUENCE ");
	_outNode(str, node->sequence);

	_outOptSeqList(str, node->options);
}

static void
_outAlterSeqStmt(StringInfo str, AlterSeqStmt *node)
{
	appendStringInfoString(str, "ALTER SEQUENCE ");
	_outNode(str, node->sequence);
	_outOptSeqList(str, node->options);
}

static void
_outCreatePLangStmt(StringInfo str, CreatePLangStmt *node)
{
	appendStringInfoString(str, "CREATE ");
	if (node->pltrusted == true)
		appendStringInfoString(str, "TRUSTED ");
	appendStringInfoString(str, "LANGUAGE \"");
	appendStringInfoString(str, node->plname);
	appendStringInfoString(str, "\"");

	if (node->plhandler != NIL)
	{
		ListCell   *lc;
		char		dot = 0;

		appendStringInfoString(str, " HANDLER ");
		foreach(lc, node->plhandler)
		{
			A_Const    *v = (A_Const *) lfirst(lc);

			if (dot == 0)
				dot = 1;
			else
				appendStringInfoString(str, ".");

			appendStringInfoString(str, "\"");
			appendStringInfoString(str, v->val.sval.sval);
			appendStringInfoString(str, "\"");
		}
	}

	if (node->plvalidator != NIL)
	{
		ListCell   *lc;
		char		dot = 0;

		appendStringInfoString(str, " VALIDATOR ");
		foreach(lc, node->plvalidator)
		{
			A_Const    *v = (A_Const *) lfirst(lc);

			if (dot == 0)
				dot = 1;
			else
				appendStringInfoString(str, ".");

			appendStringInfoString(str, "\"");
			appendStringInfoString(str, v->val.sval.sval);
			appendStringInfoString(str, "\"");
		}
	}
}


static void
_outCreateTableSpaceStmt(StringInfo str, CreateTableSpaceStmt *node)
{
	appendStringInfoString(str, "CREATE TABLESPACE \"");
	appendStringInfoString(str, node->tablespacename);
	appendStringInfoString(str, "\" ");

	if (node->owner)
	{
		appendStringInfoString(str, "OWNER \"");
		_outNode(str, node->owner);
		appendStringInfoString(str, "\" ");
	}

	appendStringInfoString(str, "LOCATION '");
	appendStringInfoString(str, node->location);
	appendStringInfoString(str, "'");
}

static void
_outDropTableSpaceStmt(StringInfo str, DropTableSpaceStmt *node)
{
	appendStringInfoString(str, "DROP TABLESPACE \"");
	appendStringInfoString(str, node->tablespacename);
	appendStringInfoString(str, "\"");
}

static void
_outFuncName(StringInfo str, List *func_name)
{
	ListCell   *lc;
	char		dot = 0;

	if (func_name == NULL)
		return;

	foreach(lc, func_name)
	{
		A_Const *v = (A_Const *) lfirst(lc);

		if (dot == 0)
			dot = 1;
		else
			appendStringInfoString(str, ".");

		if (IsA(&v->val, String))
		{
			appendStringInfoString(str, "\"");
			appendStringInfoString(str, v->val.sval.sval);
			appendStringInfoString(str, "\"");
		}
		else
		{
			_outNode(str, v);
		}
	}
}

static void
_outCreateTrigStmt(StringInfo str, CreateTrigStmt *node)
{
	bool		has_events = false;

	if (node->isconstraint == TRUE)
		appendStringInfoString(str, "CREATE CONSTRAINT TRIGGER \"");
	else
		appendStringInfoString(str, "CREATE TRIGGER \"");
	appendStringInfoString(str, node->trigname);
	appendStringInfoString(str, "\" ");

	if (TRIGGER_FOR_BEFORE(node->timing))
		appendStringInfoString(str, "BEFORE ");
	if (TRIGGER_FOR_AFTER(node->timing))
		appendStringInfoString(str, "AFTER ");

	if (node->events & TRIGGER_TYPE_INSERT)
	{
		appendStringInfoString(str, "INSERT ");
		has_events = true;
	}
	if (node->events & TRIGGER_TYPE_DELETE)
	{
		if (has_events)
			appendStringInfoString(str, "OR ");
		appendStringInfoString(str, "DELETE ");
		has_events = true;
	}
	if (node->events & TRIGGER_TYPE_UPDATE)
	{
		if (has_events)
			appendStringInfoString(str, "OR ");
		appendStringInfoString(str, "UPDATE ");
		has_events = true;
	}
	if (node->events & TRIGGER_TYPE_TRUNCATE)
	{
		if (has_events)
			appendStringInfoString(str, "OR ");
		appendStringInfoString(str, "TRUNCATE ");
		has_events = true;
	}

	appendStringInfoString(str, "ON ");
	_outNode(str, node->relation);

	if (node->constrrel)
	{
		appendStringInfoString(str, " FROM ");
		_outNode(str, node->constrrel);
	}

	if (node->deferrable)
		appendStringInfoString(str, " DEFERRABLE");
	if (node->initdeferred)
		appendStringInfoString(str, " INITIALLY DEFERRED");

	if (node->row == TRUE)
		appendStringInfoString(str, " FOR EACH ROW ");
	else
		appendStringInfoString(str, " FOR EACH STATEMENT ");

	appendStringInfoString(str, "EXECUTE PROCEDURE ");

	_outFuncName(str, node->funcname);
	appendStringInfoString(str, "(");
	_outNode(str, node->args);
	appendStringInfoString(str, ")");
}

static void
_outDefinition(StringInfo str, List *definition)
{
	ListCell   *lc;
	char		comma = 0;

	if (definition == NIL)
		return;

	appendStringInfoString(str, "(");
	foreach(lc, definition)
	{
		DefElem    *e = lfirst(lc);

		if (comma == 0)
			comma = 1;
		else
			appendStringInfoString(str, ", ");

		appendStringInfoString(str, "\"");
		appendStringInfoString(str, e->defname);
		appendStringInfoString(str, "\"");

		if (e->arg)
		{
			appendStringInfoString(str, "=");
			_outNode(str, e->arg);
		}
	}
	appendStringInfoString(str, ")");
}

static void
_outDefineStmt(StringInfo str, DefineStmt *node)
{
	ListCell   *lc;
	char		dot = 0;

	switch (node->kind)
	{
		case OBJECT_AGGREGATE:
			appendStringInfoString(str, "CREATE AGGREGATE ");
			_outFuncName(str, node->defnames);
			appendStringInfoString(str, " ");
			_outDefinition(str, node->definition);
			break;

		case OBJECT_OPERATOR:
			appendStringInfoString(str, "CREATE OPERATOR ");

			foreach(lc, node->defnames)
			{
				A_Const	   *v = (A_Const *) lfirst(lc);

				if (dot == 0)
					dot = 1;
				else
					appendStringInfoString(str, ".");

				appendStringInfoString(str, v->val.sval.sval);
			}

			appendStringInfoString(str, " ");
			_outDefinition(str, node->definition);
			break;

		case OBJECT_TYPE:
			appendStringInfoString(str, "CREATE TYPE");
			_outFuncName(str, node->defnames);
			appendStringInfoString(str, " ");
			_outDefinition(str, node->definition);
			break;

		case OBJECT_TSPARSER:
			appendStringInfoString(str, "CREATE TEXT SEARCH PARSER ");
			_outIdList(str, node->defnames);
			_outDefinition(str, node->definition);
			break;

		case OBJECT_TSDICTIONARY:
			appendStringInfoString(str, "CREATE TEXT SEARCH DICTIONARY ");
			_outIdList(str, node->defnames);
			_outDefinition(str, node->definition);
			break;

		case OBJECT_TSTEMPLATE:
			appendStringInfoString(str, "CREATE TEXT SEARCH TEMPLATE ");
			_outIdList(str, node->defnames);
			_outDefinition(str, node->definition);
			break;

		case OBJECT_TSCONFIGURATION:
			appendStringInfoString(str, "CREATE TEXT SEARCH CONFIGURATION ");
			_outIdList(str, node->defnames);
			_outDefinition(str, node->definition);
			break;

		default:
			break;
	}
}

static void
_outOperatorName(StringInfo str, List *list)
{
	char		dot = 0;
	ListCell   *lc;

	foreach(lc, list)
	{
		A_Const	   *v = (A_Const *) lfirst(lc);

		if (dot == 0)
			dot = 1;
		else
			appendStringInfoString(str, ".");

		appendStringInfoString(str, v->val.sval.sval);
	}
}

static void
_outCreateOpClassItem(StringInfo str, CreateOpClassItem *node)
{
	char		buf[16];

	switch (node->itemtype)
	{
		case OPCLASS_ITEM_OPERATOR:
			appendStringInfoString(str, "OPERATOR ");
			snprintf(buf, 16, "%d", node->number);
			appendStringInfoString(str, buf);
			appendStringInfoString(str, " ");
			_outOperatorName(str, node->name->objname);

			if (node->name->objname != NIL)
			{
				appendStringInfoString(str, "(");
				_outNode(str, node->name->objname);
				appendStringInfoString(str, ")");
			}

			/*
			 * if (node->recheck == TRUE) appendStringInfoString(str, " RECHECK");
			 */
			break;

		case OPCLASS_ITEM_FUNCTION:
			appendStringInfoString(str, "FUNCTION ");
			snprintf(buf, 16, "%d", node->number);
			appendStringInfoString(str, buf);
			appendStringInfoString(str, " ");
			_outFuncName(str, node->name->objname);
			appendStringInfoString(str, "(");
			_outNode(str, node->name->objname);
			appendStringInfoString(str, ")");
			break;

		case OPCLASS_ITEM_STORAGETYPE:
			appendStringInfoString(str, "STORAGE ");
			_outNode(str, node->storedtype);
			break;

		default:
			break;
	}

}

static void
_outCreateOpClassStmt(StringInfo str, CreateOpClassStmt *node)
{
	appendStringInfoString(str, "CREATE OPERATOR CLASS ");
	_outFuncName(str, node->opclassname);

	if (node->isDefault == TRUE)
		appendStringInfoString(str, " DEFAULT");

	appendStringInfoString(str, " FOR TYPE ");
	_outNode(str, node->datatype);
	appendStringInfoString(str, " USING ");
	appendStringInfoString(str, node->amname);
	appendStringInfoString(str, " AS ");
	_outNode(str, node->items);
}

/*
 * Handle drop statements. As of pgpool-II 3.3(derived parser from
 * PostgreSQL 9.2), following types are supported:
 *
 * DROP TABLE, DROP SEQUENCE, DROP VIEW, DROP INDEX, DROP FOREIGN
 * TABLE, DROP TYPE, DROP DOMAIN, DROP COLLATION, DROP CONVERSION,
 * DROP SCHEMA, DROP TEXT SEARCH PARSER, DROP TEXT SEARCH DICTIONARY,
 * DROP TEXT SEARCH CONFIGURATION, DROP LANGUAGE, DROP RULE, DROP
 * OPERATOR, DROP OPERATOR CLASS
 */

static void
add_function_like_objs(StringInfo str, DropStmt *node)
{
	ListCell   *lc;
	char		comma = 0;

	if (node->concurrent)
		appendStringInfoString(str, "CONCURRENTLY ");
	if (node->missing_ok)
		appendStringInfoString(str, "IF EXISTS ");

	foreach(lc, node->objects)
	{
		if (comma == 0)
			comma = 1;
		else
			appendStringInfoString(str, ", ");
		_outFuncName(str, lfirst(lc));
	}
}

static void
_outDropStmt(StringInfo str, DropStmt *node)
{
	List	   *objname;
	char		*p;
	char		*p1;
	List		*l;

	appendStringInfoString(str, "DROP ");
	switch (node->removeType)
	{
		case OBJECT_TABLE:
			appendStringInfoString(str, "TABLE ");
			add_function_like_objs(str, node);
			break;

		case OBJECT_SEQUENCE:
			appendStringInfoString(str, "SEQUENCE ");
			add_function_like_objs(str, node);
			break;

		case OBJECT_VIEW:
			appendStringInfoString(str, "VIEW ");
			add_function_like_objs(str, node);
			break;

		case OBJECT_INDEX:
			appendStringInfoString(str, "INDEX ");
			add_function_like_objs(str, node);
			break;

		case OBJECT_FOREIGN_TABLE:
			appendStringInfoString(str, "FOREIGN TABLE ");
			add_function_like_objs(str, node);
			break;

		case OBJECT_FDW:
			appendStringInfoString(str, "FOREIGN DATA WRAPPER ");
			add_function_like_objs(str, node);
			break;

		case OBJECT_TYPE:
			appendStringInfoString(str, "TYPE ");
			add_function_like_objs(str, node);
			break;

		case OBJECT_DOMAIN:
			appendStringInfoString(str, "DOMAIN ");
			add_function_like_objs(str, node);
			break;

		case OBJECT_COLLATION:
			appendStringInfoString(str, "COLLATION ");
			add_function_like_objs(str, node);
			break;

		case OBJECT_CONVERSION:
			appendStringInfoString(str, "CONVERSION ");
			add_function_like_objs(str, node);
			break;

		case OBJECT_SCHEMA:
			appendStringInfoString(str, "SCHEMA ");
			add_function_like_objs(str, node);
			break;

		case OBJECT_EXTENSION:
			appendStringInfoString(str, "EXTENSION ");
			add_function_like_objs(str, node);
			break;

		case OBJECT_TSPARSER:
			appendStringInfoString(str, "TEXT SEARCH PARSER ");
			add_function_like_objs(str, node);
			break;

		case OBJECT_TSDICTIONARY:
			appendStringInfoString(str, "TEXT SEARCH DICTIONARY ");
			add_function_like_objs(str, node);
			break;

		case OBJECT_TSTEMPLATE:
			appendStringInfoString(str, "TEXT SEARCH TEMPLATE ");
			add_function_like_objs(str, node);
			break;

		case OBJECT_TSCONFIGURATION:
			appendStringInfoString(str, "TEXT SEARCH CONFIGURATION ");
			add_function_like_objs(str, node);
			break;

		case OBJECT_LANGUAGE:
			appendStringInfoString(str, "PROCEDURAL LANGUAGE ");
			add_function_like_objs(str, node);
			break;

		case OBJECT_RULE:
			appendStringInfoString(str, "DROP RULE ");
			if (node->missing_ok)
				appendStringInfoString(str, "IF EXISTS ");
			objname = lfirst(list_head(node->objects));
			appendStringInfoString(str, strVal(llast(objname)));
			appendStringInfoString(str, " ON ");
			l = list_truncate(list_copy(objname),
							  list_length(objname) - 1);
			p = NameListToString(l);
			appendStringInfoString(str, p);
			pfree(p);
			list_free(l);
			break;

		case OBJECT_OPERATOR:
			appendStringInfoString(str, "OPERATOR ");
			add_function_like_objs(str, node);
			break;

		case OBJECT_OPCLASS:
			appendStringInfoString(str, "DROP OPERATOR CLASS ");
			if (node->missing_ok)
				appendStringInfoString(str, "IF EXISTS ");
			objname = lfirst(list_head(node->objects));
			appendStringInfoString(str, strVal(llast(objname)));
			appendStringInfoString(str, " USING ");
			appendStringInfoString(str, "'");
			l = list_truncate(list_copy(objname),
							  list_length(objname) - 1);
			p = NameListToString(l);
			p1 = escape_string(p);
			appendStringInfoString(str, p1);
			pfree(p1);
			pfree(p);
			list_free(l);
			appendStringInfoString(str, "'");
			break;

		case OBJECT_CAST:
			appendStringInfoString(str, "DROP CAST ");
			if (node->missing_ok)
				appendStringInfoString(str, "IF EXISTS ");
			appendStringInfoString(str, "(");
			objname = linitial(node->objects);
			_outNode(str, linitial(objname));
			appendStringInfoString(str, " AS ");
			_outNode(str, linitial(objname));
			appendStringInfoString(str, ")");
			break;

		case OBJECT_OPFAMILY:
			appendStringInfoString(str, "OPERATOR FAMILY ");
			if (node->missing_ok)
				appendStringInfoString(str, "IF EXISTS ");
			objname = linitial(node->objects);
			_outIdList(str, objname);
			appendStringInfoString(str, " USING ");
			_outIdList(str, objname);
			break;

		default:
			break;
	}

	if (node->behavior == DROP_CASCADE)
		appendStringInfoString(str, " CASCADE");
}

static void
_outFetchStmt(StringInfo str, FetchStmt *node)
{
	char		buf[16];

	snprintf(buf, 16, "%ld", node->howMany);

	if (node->ismove == TRUE)
		appendStringInfoString(str, "MOVE ");
	else
		appendStringInfoString(str, "FETCH ");

	switch (node->direction)
	{
		case FETCH_FORWARD:
			appendStringInfoString(str, "FORWARD ");
			if (node->howMany == FETCH_ALL)
				appendStringInfoString(str, "ALL ");
			else
			{
				appendStringInfoString(str, buf);
				appendStringInfoString(str, " ");
			}
			break;

		case FETCH_BACKWARD:
			appendStringInfoString(str, "BACKWARD ");
			if (node->howMany == FETCH_ALL)
				appendStringInfoString(str, "ALL ");
			else
			{
				appendStringInfoString(str, buf);
				appendStringInfoString(str, " ");
			}
			break;

		case FETCH_ABSOLUTE:
			if (node->howMany == 1)
				appendStringInfoString(str, "FIRST ");
			else if (node->howMany == -1)
				appendStringInfoString(str, "LAST ");
			else
			{
				appendStringInfoString(str, "ABSOLUTE ");
				appendStringInfoString(str, buf);
				appendStringInfoString(str, " ");
			}
			break;

		case FETCH_RELATIVE:
			appendStringInfoString(str, "RELATIVE ");
			appendStringInfoString(str, buf);
			appendStringInfoString(str, " ");
			break;
	}

	appendStringInfoString(str, "IN \"");
	appendStringInfoString(str, node->portalname);
	appendStringInfoString(str, "\"");
}

static void
_outPrivilegeList(StringInfo str, List *list)
{
	ListCell   *lc;
	char		comma = 0;

	if (list == NIL)
		appendStringInfoString(str, "ALL");
	else
	{
		foreach(lc, list)
		{
			A_Const	   *v = (A_Const *) lfirst(lc);

			if (comma == 0)
				comma = 1;
			else
				appendStringInfoString(str, ", ");

			appendStringInfoString(str, v->val.sval.sval);
		}
	}
}

static void
_outFunctionParameter(StringInfo str, FunctionParameter *node)
{
	switch (node->mode)
	{
		case FUNC_PARAM_OUT:
			appendStringInfoString(str, "OUT ");
			break;

		case FUNC_PARAM_INOUT:
			appendStringInfoString(str, "INOUT ");
			break;

		default:
			break;
	}

	/* function name */
	if (node->name)
	{
		appendStringInfoString(str, "\"");
		appendStringInfoString(str, node->name);
		appendStringInfoString(str, "\" ");
	}

	_outNode(str, node->argType);
}

static void
_outObjectWithArgs(StringInfo str, ObjectWithArgs *node)
{
	_outFuncName(str, node->objname);
	appendStringInfoString(str, "(");
	_outNode(str, node->objargs);
	appendStringInfoString(str, ")");
}

static void
_outGrantStmt(StringInfo str, GrantStmt *node)
{
	if (node->is_grant == true)
		appendStringInfoString(str, "GRANT ");
	else
	{
		appendStringInfoString(str, "REVOKE ");
		if (node->grant_option == true)
			appendStringInfoString(str, "GRANT OPTION FOR ");
	}

	_outPrivilegeList(str, node->privileges);

	appendStringInfoString(str, " ON ");

	switch (node->objtype)
	{
		case OBJECT_TABLE:
			_outNode(str, node->objects);
			break;

		case OBJECT_SEQUENCE:
			appendStringInfoString(str, "SEQUENCE ");
			_outNode(str, node->objects);
			break;

		case OBJECT_FUNCTION:
			appendStringInfoString(str, "FUNCTION ");
			_outNode(str, node->objects);
			break;

		case OBJECT_DATABASE:
			appendStringInfoString(str, "DATABASE ");
			_outIdList(str, node->objects);
			break;

		case OBJECT_LANGUAGE:
			appendStringInfoString(str, "LANGUAGE ");
			_outIdList(str, node->objects);
			break;

		case OBJECT_SCHEMA:
			appendStringInfoString(str, "SCHEMA ");
			_outIdList(str, node->objects);
			break;

		case OBJECT_TABLESPACE:
			appendStringInfoString(str, "TABLESPACE ");
			_outIdList(str, node->objects);
			break;

		case OBJECT_FDW:
			appendStringInfoString(str, "FOREIGN DATA WRAPPER ");
			_outIdList(str, node->objects);
			break;

		case OBJECT_FOREIGN_SERVER:
			appendStringInfoString(str, "FOREIGN SERVER ");
			_outIdList(str, node->objects);
			break;

		case OBJECT_COLUMN:
		case OBJECT_DOMAIN:
		case OBJECT_LARGEOBJECT:
		case OBJECT_TYPE:
		case OBJECT_ACCESS_METHOD:
		case OBJECT_AGGREGATE:
		case OBJECT_AMOP:
		case OBJECT_AMPROC:
		case OBJECT_ATTRIBUTE:
		case OBJECT_CAST:
		case OBJECT_COLLATION:
		case OBJECT_CONVERSION:
		case OBJECT_DEFAULT:
		case OBJECT_DEFACL:
		case OBJECT_DOMCONSTRAINT:
		case OBJECT_EVENT_TRIGGER:
		case OBJECT_EXTENSION:
		case OBJECT_FOREIGN_TABLE:
		case OBJECT_INDEX:
		case OBJECT_MATVIEW:
		case OBJECT_OPCLASS:
		case OBJECT_OPERATOR:
		case OBJECT_OPFAMILY:
		case OBJECT_PARAMETER_ACL:
		case OBJECT_POLICY:
		case OBJECT_PROCEDURE:
		case OBJECT_PUBLICATION:
		case OBJECT_PUBLICATION_NAMESPACE:
		case OBJECT_PUBLICATION_REL:
		case OBJECT_ROLE:
		case OBJECT_ROUTINE:
		case OBJECT_RULE:
		case OBJECT_SUBSCRIPTION:
		case OBJECT_STATISTIC_EXT:
		case OBJECT_TABCONSTRAINT:
		case OBJECT_TRANSFORM:
		case OBJECT_TRIGGER:
		case OBJECT_TSCONFIGURATION:
		case OBJECT_TSDICTIONARY:
		case OBJECT_TSPARSER:
		case OBJECT_TSTEMPLATE:
		case OBJECT_USER_MAPPING:
		case OBJECT_VIEW:
			break;
	}

	if (node->is_grant == true)
		appendStringInfoString(str, " TO ");
	else
		appendStringInfoString(str, " FROM ");
	_outNode(str, node->grantees);

	if (node->is_grant == true && node->grant_option == TRUE)
		appendStringInfoString(str, " WITH GRANT OPTION");

	if (node->behavior == DROP_CASCADE)
		appendStringInfoString(str, " CASCADE");
}

static void
_outGrantRoleStmt(StringInfo str, GrantRoleStmt *node)
{
	if (node->is_grant == true)
		appendStringInfoString(str, "GRANT ");
	else
	{
		appendStringInfoString(str, "REVOKE ");
		if (node->admin_opt == true)
			appendStringInfoString(str, "ADMIN OPTION FOR ");
	}

	_outIdList(str, node->granted_roles);

	appendStringInfoString(str, node->is_grant == true ? " TO " : " FROM ");

	_outIdList(str, node->grantee_roles);

	if (node->admin_opt == true && node->is_grant == true)
		appendStringInfoString(str, "  WITH ADMIN OPTION");

	if (node->grantor != NULL)
	{
		appendStringInfoString(str, " GRANTED BY \"");
		_outNode(str, node->grantor);
		appendStringInfoString(str, "\"");
	}

	if (node->behavior == DROP_CASCADE)
		appendStringInfoString(str, " CASCADE");
}

static void
_outFuncOptList(StringInfo str, List *list)
{
	ListCell   *lc;

	foreach(lc, list)
	{
		DefElem    *e = lfirst(lc);
		A_Const	   *v = (A_Const *) e->arg;

		if (strcmp(e->defname, "strict") == 0)
		{
			if (v->val.boolval.boolval)
				appendStringInfoString(str, " STRICT");
			else
				appendStringInfoString(str, " CALLED ON NULL INPUT");
		}
		else if (strcmp(e->defname, "volatility") == 0)
		{
			char	   *s = v->val.sval.sval;

			if (strcmp(s, "immutable") == 0)
				appendStringInfoString(str, " IMMUTABLE");
			else if (strcmp(s, "stable") == 0)
				appendStringInfoString(str, " STABLE");
			else if (strcmp(s, "volatile") == 0)
				appendStringInfoString(str, " VOLATILE");
		}
		else if (strcmp(e->defname, "security") == 0)
		{
			if (v->val.boolval.boolval)
				appendStringInfoString(str, " SECURITY DEFINER");
			else
				appendStringInfoString(str, " SECURITY INVOKER");
		}
		else if (strcmp(e->defname, "as") == 0)
		{
			appendStringInfoString(str, " AS ");
			_outNode(str, e->arg);
		}
		else if (strcmp(e->defname, "language") == 0)
		{
			appendStringInfoString(str, " LANGUAGE '");
			appendStringInfoString(str, v->val.sval.sval);
			appendStringInfoString(str, "'");
		}
	}
}

static void
_outCreateFunctionStmt(StringInfo str, CreateFunctionStmt *node)
{
	appendStringInfoString(str, "CREATE ");
	if (node->replace == true)
		appendStringInfoString(str, "OR REPLACE ");
	appendStringInfoString(str, "FUNCTION ");

	_outFuncName(str, node->funcname);

	appendStringInfoString(str, " (");
	_outNode(str, node->parameters);
	appendStringInfoString(str, ")");

	if (node->returnType)
	{
		appendStringInfoString(str, " RETURNS ");
		_outNode(str, node->returnType);
	}

	_outFuncOptList(str, node->options);
}

static void
_outAlterFunctionStmt(StringInfo str, AlterFunctionStmt *node)
{
	appendStringInfoString(str, "ALTER FUNCTION ");
	_outNode(str, node->func);
	_outFuncOptList(str, node->actions);
}


static void
_outCreateCastStmt(StringInfo str, CreateCastStmt *node)
{
	appendStringInfoString(str, "CREATE CAST (");
	_outNode(str, node->sourcetype);
	appendStringInfoString(str, " AS ");
	_outNode(str, node->targettype);
	appendStringInfoString(str, ") WITH FUNCTION ");
	_outNode(str, node->func);

	switch (node->context)
	{
		case COERCION_IMPLICIT:
			appendStringInfoString(str, " AS IMPLICIT");
			break;

		case COERCION_ASSIGNMENT:
			appendStringInfoString(str, " AS ASSIGNMENT");
			break;

		default:
			break;
	}
}

static void
_outReindexStmt(StringInfo str, ReindexStmt *node)
{
	appendStringInfoString(str, "REINDEX ");

	switch (node->kind)
	{
		case REINDEX_OBJECT_SYSTEM:
			appendStringInfoString(str, "SYSTEM ");
			break;

		case REINDEX_OBJECT_DATABASE:
			appendStringInfoString(str, "DATABASE ");
			break;

		case REINDEX_OBJECT_INDEX:
			appendStringInfoString(str, "INDEX ");
			break;

		case REINDEX_OBJECT_TABLE:
			appendStringInfoString(str, "TABLE ");
			break;

		default:
			break;
	}

	if (node->relation)
		_outNode(str, node->relation);

	if (node->name)
	{
		appendStringInfoString(str, "\"");
		appendStringInfoString(str, (char *) node->name);
		appendStringInfoString(str, "\"");
	}
}

static void
_outAlterObjectSchemaStmt(StringInfo str, AlterObjectSchemaStmt *node)
{
	appendStringInfoString(str, "ALTER ");

	ObjectWithArgs *owa = castNode(ObjectWithArgs, node);

	switch (node->objectType)
	{
		case OBJECT_AGGREGATE:
			appendStringInfoString(str, "AGGREGATE ");
			_outFuncName(str, owa->objname);
			appendStringInfoString(str, "(");
			if (lfirst(list_head(owa->objargs)) == NULL)
				appendStringInfoString(str, "*");
			else
				_outNode(str, lfirst(list_head(owa->objargs)));
			appendStringInfoString(str, ") SET SCHEME \"");
			appendStringInfoString(str, node->newschema);
			appendStringInfoString(str, "\"");
			break;

		case OBJECT_DOMAIN:
			appendStringInfoString(str, "DOMAIN ");
			_outFuncName(str, owa->objname);
			appendStringInfoString(str, " SET SCHEMA \"");
			appendStringInfoString(str, node->newschema);
			appendStringInfoString(str, "\"");
			break;

		case OBJECT_FUNCTION:
			appendStringInfoString(str, "FUNCTION ");
			_outFuncName(str, owa->objname);
			appendStringInfoString(str, "(");
			appendStringInfoString(str, ") SET SCHEMA \"");
			appendStringInfoString(str, node->newschema);
			appendStringInfoString(str, "\"");
			break;

		case OBJECT_SEQUENCE:
			appendStringInfoString(str, "SEQUENCE ");
			_outNode(str, node->relation);
			appendStringInfoString(str, " SET SCHEMA \"");
			appendStringInfoString(str, node->newschema);
			appendStringInfoString(str, "\"");
			break;

		case OBJECT_TABLE:
			appendStringInfoString(str, "TABLE ");
			_outNode(str, node->relation);
			appendStringInfoString(str, " SET SCHEMA \"");
			appendStringInfoString(str, node->newschema);
			appendStringInfoString(str, "\"");
			break;

		case OBJECT_TYPE:
			appendStringInfoString(str, "TYPE ");
			_outFuncName(str, owa->objname);
			appendStringInfoString(str, " SET SCHEMA \"");
			appendStringInfoString(str, node->newschema);
			appendStringInfoString(str, "\"");
			break;

		default:
			break;
	}
}

static void
_outAlterOwnerStmt(StringInfo str, AlterOwnerStmt *node)
{
	appendStringInfoString(str, "ALTER ");

	ObjectWithArgs *owa = castNode(ObjectWithArgs, node);

	switch (node->objectType)
	{
		case OBJECT_AGGREGATE:
			appendStringInfoString(str, "AGGREGATE ");
			_outFuncName(str, owa->objname);
			appendStringInfoString(str, "(");
			if (lfirst(list_head(owa->objargs)) == NULL)
				appendStringInfoString(str, "*");
			else
				_outNode(str, lfirst(list_head(owa->objargs)));
			appendStringInfoString(str, ") OWNER TO \"");
			_outNode(str, node->newowner);
			appendStringInfoString(str, "\"");
			break;

		case OBJECT_CONVERSION:
			appendStringInfoString(str, "CONVERSION \"");
			appendStringInfoString(str, strVal(linitial(owa->objname)));
			appendStringInfoString(str, "\" OWNER TO \"");
			_outNode(str, node->newowner);
			appendStringInfoString(str, "\"");
			break;

		case OBJECT_DATABASE:
			appendStringInfoString(str, "DATABASE \"");
			appendStringInfoString(str, strVal(linitial(owa->objname)));
			appendStringInfoString(str, "\" OWNER TO \"");
			_outNode(str, node->newowner);
			appendStringInfoString(str, "\"");
			break;

		case OBJECT_DOMAIN:
			appendStringInfoString(str, "DOMAIN \"");
			appendStringInfoString(str, strVal(linitial(owa->objname)));
			appendStringInfoString(str, "\" OWNER TO \"");
			_outNode(str, node->newowner);
			appendStringInfoString(str, "\"");
			break;

		case OBJECT_FUNCTION:
			appendStringInfoString(str, "FUNCTION ");
			_outFuncName(str, owa->objname);
			appendStringInfoString(str, "(");
			_outNode(str, owa->objargs);
			appendStringInfoString(str, ") OWNER TO \"");
			_outNode(str, node->newowner);
			appendStringInfoString(str, "\"");
			break;

		case OBJECT_LANGUAGE:
			appendStringInfoString(str, "LANGUAGE \"");
			appendStringInfoString(str, strVal(linitial(owa->objname)));
			appendStringInfoString(str, "\" OWNER TO \"");
			_outNode(str, node->newowner);
			appendStringInfoString(str, "\"");
			break;

		case OBJECT_OPERATOR:
			appendStringInfoString(str, "OPERATOR ");
			_outOperatorName(str, owa->objname);
			appendStringInfoString(str, "(");
			appendStringInfoString(str, ") OWNER TO \"");
			_outNode(str, node->newowner);
			appendStringInfoString(str, "\"");
			break;

		case OBJECT_OPCLASS:
			appendStringInfoString(str, "OPERATOR CLASS ");
			_outFuncName(str, owa->objname);
			appendStringInfoString(str, " USING ");
			appendStringInfoString(str, strVal(linitial(owa->objargs)));
			appendStringInfoString(str, " OWNER TO \"");
			_outNode(str, node->newowner);
			appendStringInfoString(str, "\"");
			break;

		case OBJECT_OPFAMILY:
			appendStringInfoString(str, "OPERATOR FAMILY ");
			_outFuncName(str, owa->objname);
			appendStringInfoString(str, " USING ");
			appendStringInfoString(str, strVal(linitial(owa->objargs)));
			appendStringInfoString(str, " OWNER TO \"");
			_outNode(str, node->newowner);
			appendStringInfoString(str, "\"");
			break;

		case OBJECT_SCHEMA:
			appendStringInfoString(str, "SCHEMA \"");
			appendStringInfoString(str, strVal(linitial(owa->objname)));
			appendStringInfoString(str, "\" OWNER TO \"");
			_outNode(str, node->newowner);
			appendStringInfoString(str, "\"");
			break;

		case OBJECT_TYPE:
			appendStringInfoString(str, "TYPE \"");
			appendStringInfoString(str, strVal(linitial(owa->objname)));
			appendStringInfoString(str, "\" OWNER TO \"");
			_outNode(str, node->newowner);
			appendStringInfoString(str, "\"");
			break;

		case OBJECT_TABLESPACE:
			appendStringInfoString(str, "TABLESPACE \"");
			appendStringInfoString(str, strVal(linitial(owa->objname)));
			appendStringInfoString(str, "\" OWNER TO \"");
			_outNode(str, node->newowner);
			appendStringInfoString(str, "\"");
			break;

		case OBJECT_TSDICTIONARY:
			appendStringInfoString(str, "TEXT SEARCH DICTIONARY \"");
			appendStringInfoString(str, strVal(linitial(owa->objname)));
			appendStringInfoString(str, "\" OWNER TO \"");
			_outNode(str, node->newowner);
			appendStringInfoString(str, "\"");
			break;

		case OBJECT_TSCONFIGURATION:
			appendStringInfoString(str, "TEXT SEARCH CONFIGURATION \"");
			appendStringInfoString(str, strVal(linitial(owa->objname)));
			appendStringInfoString(str, "\" OWNER TO \"");
			_outNode(str, node->newowner);
			appendStringInfoString(str, "\"");
			break;

		case OBJECT_FDW:
			appendStringInfoString(str, "FOREIGN DATA WRAPPER \"");
			appendStringInfoString(str, strVal(linitial(owa->objname)));
			appendStringInfoString(str, "\" OWNER TO \"");
			_outNode(str, node->newowner);
			appendStringInfoString(str, "\"");
			break;

		case OBJECT_FOREIGN_SERVER:
			appendStringInfoString(str, "SERVER \"");
			appendStringInfoString(str, strVal(linitial(owa->objname)));
			appendStringInfoString(str, "\" OWNER TO \"");
			_outNode(str, node->newowner);
			appendStringInfoString(str, "\"");
			break;

		default:
			break;
	}
}

static void
_outRuleStmt(StringInfo str, RuleStmt *node)
{
	appendStringInfoString(str, "CREATE ");
	if (node->replace)
		appendStringInfoString(str, "OR REPLACE ");
	appendStringInfoString(str, "RULE \"");
	appendStringInfoString(str, node->rulename);
	appendStringInfoString(str, "\" AS ON ");

	switch (node->event)
	{
		case CMD_SELECT:
			appendStringInfoString(str, "SELECT");
			break;

		case CMD_UPDATE:
			appendStringInfoString(str, "UPDATE");
			break;

		case CMD_DELETE:
			appendStringInfoString(str, "DELETE");
			break;

		case CMD_INSERT:
			appendStringInfoString(str, "INSERT");
			break;

		default:
			break;
	}

	appendStringInfoString(str, " TO ");
	_outNode(str, node->relation);

	if (node->whereClause)
	{
		appendStringInfoString(str, " WHERE ");
		_outNode(str, node->whereClause);
	}

	appendStringInfoString(str, " DO ");

	if (node->instead)
		appendStringInfoString(str, "INSTEAD ");

	if (node->actions == NIL)
		appendStringInfoString(str, "NOTHING");
	else if (list_length(node->actions) == 1)
		_outNode(str, linitial(node->actions));
	else
	{
		ListCell   *lc;
		char		semi = 0;

		appendStringInfoString(str, "(");

		foreach(lc, node->actions)
		{
			if (semi == 0)
				semi = 1;
			else
				appendStringInfoString(str, ";");

			_outNode(str, lfirst(lc));
		}

		appendStringInfoString(str, ")");
	}
}

static void
_outViewStmt(StringInfo str, ViewStmt *node)
{
	if (node->replace)
		appendStringInfoString(str, "CREATE OR REPLACE ");
	else
		appendStringInfoString(str, "CREATE ");

	if (node->view->relpersistence == RELPERSISTENCE_TEMP)
		appendStringInfoString(str, "TEMP ");

	appendStringInfoString(str, "VIEW ");
	_outNode(str, node->view);

	if (node->aliases)
	{
		appendStringInfoString(str, "(");
		_outIdList(str, node->aliases);
		appendStringInfoString(str, ")");
	}

	appendStringInfoString(str, " AS");
	_outNode(str, node->query);
}

static void
_outCreatedbOptList(StringInfo str, List *options)
{
	ListCell   *lc;

	foreach(lc, options)
	{
		DefElem    *e = lfirst(lc);
		A_Const	   *v = (A_Const *) e->arg;

		/* keyword */
		if (strcmp(e->defname, "template") == 0)
			appendStringInfoString(str, " TEMPLATE ");
		else if (strcmp(e->defname, "location") == 0)
		{
			appendStringInfoString(str, " LOCATION ");
		}
		else if (strcmp(e->defname, "tablespace") == 0)
			appendStringInfoString(str, " TABLESPACE ");
		else if (strcmp(e->defname, "encoding") == 0)
		{
			appendStringInfoString(str, " ENCODING ");
		}
		else if (strcmp(e->defname, "owner") == 0)
			appendStringInfoString(str, " OWNER ");
		else if (strcmp(e->defname, "connectionlimit") == 0)
			appendStringInfoString(str, " CONNECTION LIMIT ");

		/* value */
		if (v == NULL)
			appendStringInfoString(str, "DEFAULT");
		else if (IsA(&v->val, String))
		{
			appendStringInfoString(str, "'");
			appendStringInfoString(str, v->val.sval.sval);
			appendStringInfoString(str, "'");
		}
		else if (IsA(&v->val, Integer))
		{
			char		buf[16];

			snprintf(buf, sizeof(buf), "%d", v->val.ival.ival);
			appendStringInfoString(str, buf);
		}
	}
}

static void
_outCreatedbStmt(StringInfo str, CreatedbStmt *node)
{
	appendStringInfoString(str, "CREATE DATABASE \"");
	appendStringInfoString(str, node->dbname);
	appendStringInfoString(str, "\"");

	_outCreatedbOptList(str, node->options);
}

static void
_outAlterDatabaseStmt(StringInfo str, AlterDatabaseStmt *node)
{
	appendStringInfoString(str, "ALTER DATABASE \"");
	appendStringInfoString(str, node->dbname);
	appendStringInfoString(str, "\" ");

	_outCreatedbOptList(str, node->options);
}

static void
_outAlterDatabaseSetStmt(StringInfo str, AlterDatabaseSetStmt *node)
{
	appendStringInfoString(str, "ALTER DATABASE \"");
	appendStringInfoString(str, node->dbname);
	appendStringInfoString(str, "\" ");

	_outNode(str, node->setstmt);
}

static void
_outDropdbStmt(StringInfo str, DropdbStmt *node)
{
	appendStringInfoString(str, "DROP DATABASE \"");
	appendStringInfoString(str, node->dbname);
	appendStringInfoString(str, "\"");
}

static void
_outCreateDomainStmt(StringInfo str, CreateDomainStmt *node)
{
	ListCell   *lc;

	appendStringInfoString(str, "CREATE DOMAIN ");
	_outFuncName(str, node->domainname);
	appendStringInfoString(str, " ");
	_outNode(str, node->typeName);


	foreach(lc, node->constraints)
	{
		appendStringInfoString(str, " ");
		_outNode(str, lfirst(lc));
	}
}

static void
_outAlterDomainStmt(StringInfo str, AlterDomainStmt *node)
{
	appendStringInfoString(str, "ALTER DOMAIN ");
	_outFuncName(str, node->typeName);

	switch (node->subtype)
	{
		case 'T':
			if (node->def)
			{
				appendStringInfoString(str, " SET DEFAULT ");
				_outNode(str, node->def);
			}
			else
				appendStringInfoString(str, " DROP DEFAULT");
			break;

		case 'N':
			appendStringInfoString(str, " DROP NOT NULL");
			break;

		case 'O':
			appendStringInfoString(str, " SET NOT NULL");
			break;

		case 'C':
			appendStringInfoString(str, " ADD ");
			_outNode(str, node->def);
			break;

		case 'X':
			appendStringInfoString(str, " DROP CONSTRAINT \"");
			appendStringInfoString(str, node->name);
			appendStringInfoString(str, "\"");
			if (node->behavior == DROP_CASCADE)
				appendStringInfoString(str, " CASCADE");
			break;
	}
}

static void
_outCreateConversionStmt(StringInfo str, CreateConversionStmt *node)
{
	appendStringInfoString(str, "CREATE ");

	if (node->def == TRUE)
		appendStringInfoString(str, "DEFAULT ");

	appendStringInfoString(str, "CONVERSION ");

	_outFuncName(str, node->conversion_name);

	appendStringInfoString(str, " FOR '");
	appendStringInfoString(str, node->for_encoding_name);
	appendStringInfoString(str, "' TO '");
	appendStringInfoString(str, node->to_encoding_name);
	appendStringInfoString(str, " FROM ");
	_outFuncName(str, node->func_name);
}

static void
_outPrepareStmt(StringInfo str, PrepareStmt *node)
{
	appendStringInfoString(str, "PREPARE \"");
	appendStringInfoString(str, node->name);
	appendStringInfoString(str, "\" ");

	if (node->argtypes != NIL)
	{
		appendStringInfoString(str, "(");
		_outNode(str, node->argtypes);
		appendStringInfoString(str, ") ");
	}

	appendStringInfoString(str, "AS ");
	_outNode(str, node->query);
}

static void
_outExecuteStmt(StringInfo str, ExecuteStmt *node)
{
	appendStringInfoString(str, "EXECUTE \"");
	appendStringInfoString(str, node->name);
	appendStringInfoString(str, "\" ");

	if (node->params != NIL)
	{
		appendStringInfoString(str, "(");
		_outNode(str, node->params);
		appendStringInfoString(str, ")");
	}
}

static void
_outLockStmt(StringInfo str, LockStmt *node)
{
	appendStringInfoString(str, "LOCK TABLE ");
	_outNode(str, node->relations);

	appendStringInfoString(str, " IN ");
	switch (node->mode)
	{
		case AccessShareLock:
			appendStringInfoString(str, "ACCESS SHARE ");
			break;

		case RowShareLock:
			appendStringInfoString(str, "ROW SHARE ");
			break;

		case RowExclusiveLock:
			appendStringInfoString(str, "ROW EXCLUSIVE ");
			break;

		case ShareUpdateExclusiveLock:
			appendStringInfoString(str, "SHARE UPDATE EXCLUSIVE ");
			break;

		case ShareLock:
			appendStringInfoString(str, "SHARE ");
			break;

		case ShareRowExclusiveLock:
			appendStringInfoString(str, "SHARE ROW EXCLUSIVE ");
			break;

		case ExclusiveLock:
			appendStringInfoString(str, "EXCLUSIVE ");
			break;

		case AccessExclusiveLock:
			appendStringInfoString(str, "ACCESS EXCLUSIVE ");
			break;
	}
	appendStringInfoString(str, "MODE");

	if (node->nowait == TRUE)
		appendStringInfoString(str, " NOWAIT");
}

static void
_outOperatorArgTypes(StringInfo str, List *args)
{
	TypeName   *left,
			   *right;

	left = linitial(args);
	right = lsecond(args);

	if (left)
		_outNode(str, left);
	else
		appendStringInfoString(str, "NONE");
	appendStringInfoString(str, ", ");
	if (right)
		_outNode(str, right);
	else
		appendStringInfoString(str, "NONE");
}

static void
_outCommentStmt(StringInfo str, CommentStmt *node)
{
	TypeName   *t;
	A_Const	   *v;
	char		buf[16];

	appendStringInfoString(str, "COMMENT ON ");

	ObjectWithArgs *owa = castNode(ObjectWithArgs, node);

	switch (node->objtype)
	{
		case OBJECT_AGGREGATE:
			appendStringInfoString(str, "AGGREGATE ");
			_outFuncName(str, owa->objname);
			appendStringInfoString(str, "(");

			t = linitial(owa->objargs);
			if (t)
				_outNode(str, t);
			else
				appendStringInfoString(str, "*");
			appendStringInfoString(str, ")");
			break;

		case OBJECT_FUNCTION:
			appendStringInfoString(str, "FUNCTION ");
			_outFuncName(str, owa->objname);
			appendStringInfoString(str, "(");
			_outNode(str, owa->objargs);
			appendStringInfoString(str, ")");
			break;

		case OBJECT_OPERATOR:
			appendStringInfoString(str, "OPERATOR ");
			_outOperatorName(str, owa->objname);
			appendStringInfoString(str, "(");
			_outOperatorArgTypes(str, owa->objargs);
			appendStringInfoString(str, ")");
			break;

		case OBJECT_TABCONSTRAINT:
		case OBJECT_DOMCONSTRAINT:
			appendStringInfoString(str, "CONSTRAINT \"");
			v = lsecond(owa->objname);
			appendStringInfoString(str, v->val.sval.sval);
			appendStringInfoString(str, "\" ON ");
			_outFuncName(str, linitial(owa->objargs));
			break;

		case OBJECT_RULE:
			appendStringInfoString(str, "RULE \"");
			v = lsecond(owa->objname);
			appendStringInfoString(str, v->val.sval.sval);
			appendStringInfoString(str, "\" ON ");
			_outFuncName(str, linitial(owa->objargs));
			break;

		case OBJECT_TRIGGER:
			appendStringInfoString(str, "TRIGGER \"");
			v = lsecond(owa->objname);
			appendStringInfoString(str, v->val.sval.sval);
			appendStringInfoString(str, "\" ON ");
			_outFuncName(str, linitial(owa->objargs));
			break;

		case OBJECT_OPCLASS:
			appendStringInfoString(str, "OPERATOR CLASS ");
			_outFuncName(str, owa->objname);
			appendStringInfoString(str, " USING ");
			v = linitial(owa->objargs);
			appendStringInfoString(str, v->val.sval.sval);
			break;

		case OBJECT_LARGEOBJECT:
			appendStringInfoString(str, "LARGE OBJECT ");
			v = (A_Const *) linitial(owa->objname);
			if (IsA(&v->val, String))
				appendStringInfoString(str, v->val.sval.sval);
			else if (IsA(&v->val, Integer))
			{
				snprintf(buf, 16, "%d", v->val.ival.ival);
				appendStringInfoString(str, buf);
			}
			break;

		case OBJECT_CAST:
			appendStringInfoString(str, "CAST (");
			_outNode(str, linitial(owa->objname));
			appendStringInfoString(str, " AS ");
			_outNode(str, linitial(owa->objargs));
			appendStringInfoString(str, ")");
			break;

		case OBJECT_LANGUAGE:
			appendStringInfoString(str, "LANGUAGE ");
			_outFuncName(str, owa->objname);
			break;

		default:
			switch (node->objtype)
			{
				case OBJECT_COLUMN:
					appendStringInfoString(str, "COLUMN ");
					break;
				case OBJECT_DATABASE:
					appendStringInfoString(str, "DATABASE ");
					break;
				case OBJECT_SCHEMA:
					appendStringInfoString(str, "SCHEMA ");
					break;
				case OBJECT_INDEX:
					appendStringInfoString(str, "INDEX ");
					break;
				case OBJECT_SEQUENCE:
					appendStringInfoString(str, "SEQUENCE ");
					break;
				case OBJECT_TABLE:
					appendStringInfoString(str, "TABLE ");
					break;
				case OBJECT_DOMAIN:
					appendStringInfoString(str, "DOMAIN ");
					break;
				case OBJECT_TYPE:
					appendStringInfoString(str, "TYPE ");
					break;
				case OBJECT_VIEW:
					appendStringInfoString(str, "VIEW ");
					break;
				default:
					break;
			}
			_outFuncName(str, owa->objname);
			break;
	}

	appendStringInfoString(str, " IS ");
	if (node->comment)
	{
		appendStringInfoString(str, "'");
		appendStringInfoString(str, node->comment);
		appendStringInfoString(str, "'");
	}
	else
		appendStringInfoString(str, "NULL");
}

static void
_outRangeSubselect(StringInfo str, RangeSubselect *node)
{
	appendStringInfoString(str, "(");
	_outNode(str, node->subquery);
	appendStringInfoString(str, ")");

	_outNode(str, node->alias);
}

/*TODO*/
static void
_outRangeFunction(StringInfo str, RangeFunction *node)
{
	_outNode(str, node->functions);
	//TODO
		if (node->alias)
	{
		_outNode(str, node->alias);
	}

	if (node->coldeflist)
	{
		appendStringInfoString(str, " (");
		_outNode(str, node->coldeflist);
		appendStringInfoString(str, ")");
	}
}

static void
_outRangeTableSample(StringInfo str, RangeTableSample *node)
{
	_outNode(str, node->relation);
	appendStringInfoString(str, " TABLESAMPLE ");
	_outFuncName(str, node->method);
	appendStringInfoString(str, " (");
	_outNode(str, node->args);
	appendStringInfoString(str, ")");

	if (node->repeatable)
	{
		appendStringInfoString(str, " REPEATABLE (");
		_outNode(str, node->repeatable);
		appendStringInfoString(str, ")");
	}
}

static void
_outRangeTableFunc(StringInfo str, RangeTableFunc *node)
{
}

static void
_outRangeTableFuncCol(StringInfo str, RangeTableFuncCol *node)
{
}

static void
_outDiscardStmt(StringInfo str, DiscardStmt *node)
{
	switch (node->target)
	{
		case DISCARD_ALL:
			appendStringInfoString(str, "DISCARD ALL");
			break;

		case DISCARD_TEMP:
			appendStringInfoString(str, "DISCARD TEMP");
			break;

		case DISCARD_PLANS:
			appendStringInfoString(str, "DISCARD PLANS");
			break;

		default:
			break;
	}
}

static void
_outCreateOpFamilyStmt(StringInfo str, CreateOpFamilyStmt *node)
{
	appendStringInfoString(str, "CREATE OPERATOR FAMILY ");
	_outIdList(str, node->opfamilyname);
	appendStringInfoString(str, " USING \"");
	appendStringInfoString(str, node->amname);
	appendStringInfoString(str, "\"");
}

static void
_outAlterOpFamilyStmt(StringInfo str, AlterOpFamilyStmt *node)
{
}

static void
_outCreateEnumStmt(StringInfo str, CreateEnumStmt *node)
{
	appendStringInfoString(str, "CREATE TYPE ");
	_outIdList(str, node->typeName);
	appendStringInfoString(str, " AS ENUM (");
	_outNode(str, node->vals);
	appendStringInfoString(str, ")");
}

static void
_outDropOwnedStmt(StringInfo str, DropOwnedStmt *node)
{
	appendStringInfoString(str, "DROP OWNED BY ");
	_outIdList(str, node->roles);
	if (node->behavior == DROP_CASCADE)
		appendStringInfoString(str, " CASCADE");
}

static void
_outReassignOwnedStmt(StringInfo str, ReassignOwnedStmt *node)
{
	appendStringInfoString(str, "REASSIGN OWNED BY ");
	_outIdList(str, node->roles);
	appendStringInfoString(str, " TO \"");
	_outNode(str, node->newrole);
	appendStringInfoString(str, "\"");
}

static void
_outAlterTSDictionaryStmt(StringInfo str, AlterTSDictionaryStmt *node)
{
	appendStringInfoString(str, "ALTER TEXT SEARCH DICTIONARY ");
	_outIdList(str, node->dictname);
	appendStringInfoString(str, "(");
	_outNode(str, node->options);
	appendStringInfoString(str, ")");
}

static void
_outAlterTSConfigurationStmt(StringInfo str, AlterTSConfigurationStmt *node)
{
	appendStringInfoString(str, "ALTER TEXT SEARCH CONFIGURATION ");
	_outIdList(str, node->cfgname);
	if (node->override == false && node->replace == false)
	{
		appendStringInfoString(str, "ADD MAPPING FOR ");
		_outIdList(str, node->tokentype);
		appendStringInfoString(str, " WITH ");
		_outIdList(str, node->dicts);
	}
	else if (node->override == true && node->replace == false)
	{
		appendStringInfoString(str, "ALTER MAPPING FOR ");
		_outIdList(str, node->tokentype);
		appendStringInfoString(str, " WITH ");
		_outIdList(str, node->dicts);
	}
	else if (node->override == false && node->replace == true)
	{
		if (node->tokentype == NIL)
			appendStringInfoString(str, "ALTER MAPPING ");
		else
		{
			appendStringInfoString(str, "ALTER MAPPING FOR ");
			_outIdList(str, node->tokentype);
		}
		appendStringInfoString(str, "REPLACE ");
		_outNode(str, linitial(node->dicts));
		appendStringInfoString(str, " WITH ");
		_outNode(str, lsecond(node->dicts));
	}
	else if (node->missing_ok == false)
	{
		appendStringInfoString(str, " DROP MAPPING FOR ");
		_outIdList(str, node->tokentype);
	}
	else if (node->missing_ok == true)
	{
		appendStringInfoString(str, " DROP MAPPING IF EXISTS FOR ");
		_outIdList(str, node->tokentype);
	}
}

static void
_outXmlExpr(StringInfo str, XmlExpr *node)
{
	A_Const    *n;

	switch (node->op)
	{
		case IS_DOCUMENT:
			_outNode(str, node->args);
			appendStringInfoString(str, " IS DOCUMENT");
			break;

		case IS_XMLCONCAT:
			appendStringInfoString(str, "XMLCONCAT (");
			_outNode(str, node->args);
			appendStringInfoString(str, ")");
			break;

		case IS_XMLELEMENT:
			appendStringInfoString(str, "XMLELEMENT (");
			if (node->name)
			{
				appendStringInfoString(str, "NAME \"");
				appendStringInfoString(str, node->name);
				appendStringInfoString(str, "\"");
				if (node->named_args != NIL)
				{
					appendStringInfoString(str, ",");
					_outIdList(str, node->named_args);
				}
			}
			if (node->args != NIL)
			{
				appendStringInfoString(str, ",");
				_outNode(str, node->args);
			}
			appendStringInfoString(str, ")");
			break;

		case IS_XMLFOREST:
			appendStringInfoString(str, "XMLFOREST (");
			_outNode(str, node->named_args);
			appendStringInfoString(str, ")");
			break;

		case IS_XMLPARSE:
			appendStringInfoString(str, "XMLPARSE (");
			if (node->xmloption == XMLOPTION_DOCUMENT)
				appendStringInfoString(str, "DOCUMENT ");
			else
				appendStringInfoString(str, "CONTENT ");

			_outNode(str, linitial(node->args));
			n = lsecond(node->args);
			{
				Node	   *arg = ((TypeCast *) n)->arg;

				if (((A_Const *) arg)->val.sval.sval[0] == 't')
					appendStringInfoString(str, " PRESERVE WHITESPACE");
			}

			appendStringInfoString(str, ")");
			break;


		default:
			break;
	}
}

static void
_outXmlSerialize(StringInfo str, XmlSerialize *node)
{

}

static void
_outWithDefinition(StringInfo str, List *def_list)
{
	int			oid = 0;

	if (list_length(def_list) == 1)
	{
		DefElem    *elem;

		elem = linitial(def_list);
		if (strcmp(elem->defname, "oids") == 0)
		{
			Integer	   *v = (Integer *) elem->arg;

			if (v->ival == 1)
				appendStringInfoString(str, " WITH OIDS ");
			else
				appendStringInfoString(str, " WITHOUT OIDS ");
			oid = 1;
		}
	}

	if (oid == 1)
		return;

	appendStringInfoString(str, " WITH ");
	_outDefinition(str, def_list);
}

static void
_outOnConflictClause(StringInfo str, OnConflictClause *node)
{
	appendStringInfoString(str, " ON CONFLICT ");

	if (node->infer)
	{
		if (node->infer->indexElems != NIL)
		{
			appendStringInfoString(str, " ( ");
			_outList(str, node->infer->indexElems);
			appendStringInfoString(str, " ) ");

			if (node->infer->whereClause)
			{
				appendStringInfoString(str, " WHERE ");
				_outNode(str, node->infer->whereClause);
			}
		}
		else
		{
			appendStringInfoString(str, " ON CONSTRAINT ");
			appendStringInfoString(str, node->infer->conname);
		}
	}

	switch (node->action)
	{
		case ONCONFLICT_UPDATE:
			appendStringInfoString(str, " DO UPDATE ");
			break;

		case ONCONFLICT_NOTHING:
			appendStringInfoString(str, " DO NOTHING ");
			break;

		default:
			break;
	}

	if (node->targetList)
	{
		_outSetClause(str, node->targetList);
	}

	if (node->whereClause)
	{
		appendStringInfoString(str, " WHERE ");
		_outNode(str, node->whereClause);
	}
}

static void
_outPartitionElem(StringInfo str, PartitionElem *node)
{
}

static void
_outPartitionSpec(StringInfo str, PartitionSpec *node)
{
}

static void
_outPartitionBoundSpec(StringInfo str, PartitionBoundSpec *node)
{
}

static void
_outPartitionRangeDatum(StringInfo str, PartitionRangeDatum *node)
{
}

/*
 * _outNode -
 *	  converts a Node into ascii string and append it to 'str'
 */
void
_outNode(StringInfo str, void *obj)
{
	/* Guard against stack overflow due to overly complex expressions */
	/*
	 * check_stack_depth();
	 */

	if (obj == NULL)
		return;
	else if (IsA(obj, List) ||IsA(obj, IntList) || IsA(obj, OidList))
		_outList(str, obj);
	/* nodeRead does not want to see { } around these! */
	else if (IsA(obj, Integer))
		_outInteger(str, (Integer *) obj);
	else if (IsA(obj, Float))
		_outFloat(str, (Float *) obj);
	else if (IsA(obj, Boolean))
		_outBoolean(str, (Boolean *) obj);
	else if (IsA(obj, String))
		_outString(str, (String *) obj);
	else if (IsA(obj, BitString))
		_outBitString(str, (BitString *) obj);
	else
	{
		switch (nodeTag(obj))
		{
			case T_Alias:
				_outAlias(str, obj);
				break;
			case T_RangeVar:
				_outRangeVar(str, obj);
				break;

				/*
				 * case T_IntoClause: _outIntoClause(str, obj); break;
				 */
			case T_Var:
				_outVar(str, obj);
				break;
			case T_Const:
				_outConst(str, obj);
				break;
			case T_Param:
				_outParam(str, obj);
				break;
			case T_Aggref:
				_outAggref(str, obj);
				break;
			case T_GroupingFunc:
				_outGroupingFunc(str, obj);
				break;

				/*
				 * case T_WindowFunc: _outWindowFunc(str, obj); break;
				 */
			case T_SubscriptingRef:
				_outSubscriptingRef(str, obj);
				break;
			case T_FuncExpr:
				_outFuncExpr(str, obj);
				break;
			case T_NamedArgExpr:
				_outNamedArgExpr(str, obj);
				break;
			case T_OpExpr:
				_outOpExpr(str, obj);
				break;
			case T_DistinctExpr:
				_outDistinctExpr(str, obj);
				break;
			case T_NullIfExpr:
				_outNullIfExpr(str, obj);
				break;
			case T_ScalarArrayOpExpr:
				_outScalarArrayOpExpr(str, obj);
				break;
			case T_BoolExpr:
				_outBoolExpr(str, obj);
				break;
			case T_SubLink:
				_outSubLink(str, obj);
				break;
			case T_SubPlan:
				_outSubPlan(str, obj);
				break;

				/*
				 * case T_AlternativeSubPlan: _outAlternativeSubPlan(str,
				 * obj); break;
				 */
			case T_FieldSelect:
				_outFieldSelect(str, obj);
				break;
			case T_FieldStore:
				_outFieldStore(str, obj);
				break;
			case T_RelabelType:
				_outRelabelType(str, obj);
				break;

				/*
				 * case T_CoerceViaIO: _outCoerceViaIO(str, obj); break; case
				 * T_ArrayCoerceExpr: _outArrayCoerceExpr(str, obj); break;
				 */
			case T_ConvertRowtypeExpr:
				_outConvertRowtypeExpr(str, obj);
				break;

				/*
				 * case T_CollateExpr: _outCollateExpr(str, obj); break;
				 */
			case T_CaseExpr:
				_outCaseExpr(str, obj);
				break;
			case T_CaseWhen:
				_outCaseWhen(str, obj);
				break;
			case T_CaseTestExpr:
				_outCaseTestExpr(str, obj);
				break;
			case T_ArrayExpr:
				_outArrayExpr(str, obj);
				break;
			case T_RowExpr:
				_outRowExpr(str, obj);
				break;

				/*
				 * case T_RowCompareExpr: _outRowCompareExpr(str, obj); break;
				 */
			case T_CoalesceExpr:
				_outCoalesceExpr(str, obj);
				break;
			case T_MinMaxExpr:
				_outMinMaxExpr(str, obj);
				break;

				/*
				 * case T_SQLValueFunction: _outSQLValueFunction(str, obj);
				 */
				break;
			case T_XmlExpr:
				_outXmlExpr(str, obj);
				break;
			case T_NullTest:
				_outNullTest(str, obj);
				break;
			case T_BooleanTest:
				_outBooleanTest(str, obj);
				break;
			case T_CoerceToDomain:
				_outCoerceToDomain(str, obj);
				break;
			case T_CoerceToDomainValue:
				_outCoerceToDomainValue(str, obj);
				break;
			case T_SetToDefault:
				_outSetToDefault(str, obj);
				break;
			case T_CurrentOfExpr:
				_outCurrentOfExpr(str, obj);
				break;

				/*
				 * case T_NextValueExpr: _outNextValueExpr(str, obj); break;
				 */
			case T_InferenceElem:
				_outInferenceElem(str, obj);
				break;
			case T_TargetEntry:
				_outTargetEntry(str, obj);
				break;
			case T_RangeTblRef:
				_outRangeTblRef(str, obj);
				break;
			case T_JoinExpr:
				_outJoinExpr(str, obj);
				break;
			case T_FromExpr:
				_outFromExpr(str, obj);
				break;
			case T_OnConflictExpr:
				_outOnConflictExpr(str, obj);
				break;
#ifdef NOT_USED
			case T_ExtensibleNode:
				_outExtensibleNode(str, obj);
				break;
#endif
			case T_CreateStmt:
				_outCreateStmt(str, obj);
				break;
			case T_CreateTableAsStmt:
				_outCreateTableAsStmt(str, obj);
				break;
			case T_CreateForeignTableStmt:
				_outCreateForeignTableStmt(str, obj);
				break;
			case T_ImportForeignSchemaStmt:
				_outImportForeignSchemaStmt(str, obj);
				break;
			case T_IndexStmt:
				_outIndexStmt(str, obj);
				break;
			case T_CreateStatsStmt:
				_outCreateStatsStmt(str, obj);
				break;
			case T_AlterStatsStmt:
				_outAlterStatsStmt(str, obj);
				break;
			case T_NotifyStmt:
				_outNotifyStmt(str, obj);
				break;
			case T_DeclareCursorStmt:
				_outDeclareCursorStmt(str, obj);
				break;
			case T_SelectStmt:
				_outSelectStmt(str, obj);
				break;
			case T_ReturnStmt:
				_outReturnStmt(str, obj);
				break;
			case T_PLAssignStmt:
				_outPLAssignStmt(str, obj);
				break;
			case T_ColumnDef:
				_outColumnDef(str, obj);
				break;
			case T_TypeName:
				_outTypeName(str, obj);
				break;
			case T_TypeCast:
				_outTypeCast(str, obj);
				break;
			case T_CollateClause:
				_outCollateClause(str, obj);
				break;
			case T_IndexElem:
				_outIndexElem(str, obj);
				break;

				/*
				 * case T_Query: _outQuery(str, obj); break; case
				 * T_WithCheckOption: _outWithCheckOption(str, obj); break;
				 * case T_SortGroupClause: _outSortGroupClause(str, obj);
				 * break;
				 */
			case T_GroupingSet:
				_outGroupingSet(str, obj);
				break;

				/*
				 * case T_WindowClause: _outWindowClause(str, obj); break;
				 * case T_RowMarkClause: _outRowMarkClause(str, obj); break;
				 */
			case T_WithClause:
				_outWithClause(str, obj);
				break;
			case T_CTESearchClause:
				_outCTESearchClause(str, obj);
				break;
			case T_CTECycleClause:
				_outCTECycleClause(str, obj);
				break;
			case T_CommonTableExpr:
				_outCommonTableExpr(str, obj);
				break;
			case T_MergeWhenClause:
				_outMergeWhenClauses(str, obj);
				break;
			case T_MergeAction:
				_outMergeAction(str, obj);
				break;
			case T_SetOperationStmt:
				_outSetOperationStmt(str, obj);
				break;

				/*
				 * case T_RangeTblEntry: _outRangeTblEntry(str, obj); break;
				 */
			case T_TableSampleClause:
				_outTableSampleClause(str, obj);
				break;
			case T_A_Expr:
				_outA_Expr(str, obj);
				break;
			case T_ColumnRef:
				_outColumnRef(str, obj);
				break;
			case T_ParamRef:
				_outParamRef(str, obj);
				break;
			case T_RawStmt:
				_outRawStmt(str, obj);
				break;
			case T_A_Const:
				_outA_Const(str, obj);
				break;

				/*
				 * case T_A_Star: _outA_Star(str, obj); break;
				 */
			case T_A_Indices:
				_outA_Indices(str, obj);
				break;
			case T_A_Indirection:
				_outA_Indirection(str, obj);
				break;
			case T_A_ArrayExpr:
				_outA_ArrayExpr(str, obj);
				break;
			case T_ResTarget:
				_outResTarget(str, obj);
				break;
			case T_MultiAssignRef:
				_outMultiAssignRef(str, obj);
				break;
			case T_SortBy:
				_outSortBy(str, obj);
				break;
			case T_WindowDef:
				_outWindowDef(str, obj);
				break;
			case T_RangeSubselect:
				_outRangeSubselect(str, obj);
				break;
			case T_RangeFunction:
				_outRangeFunction(str, obj);
				break;
			case T_RangeTableSample:
				_outRangeTableSample(str, obj);
				break;
			case T_RangeTableFunc:
				_outRangeTableFunc(str, obj);
				break;
			case T_RangeTableFuncCol:
				_outRangeTableFuncCol(str, obj);
				break;
			case T_Constraint:
				_outConstraint(str, obj);
				break;
			case T_FuncCall:
				_outFuncCall(str, obj);
				break;
			case T_DefElem:
				_outDefElem(str, obj);
				break;

				/*
				 * case T_TableLikeClause: _outTableLikeClause(str, obj);
				 * break;
				 */
			case T_LockingClause:
				_outLockingClause(str, obj);
				break;
			case T_XmlSerialize:
				_outXmlSerialize(str, obj);
				break;
			case T_TriggerTransition:
				_outTriggerTransition(str, obj);
				break;
			case T_PartitionElem:
				_outPartitionElem(str, obj);
				break;
			case T_PartitionSpec:
				_outPartitionSpec(str, obj);
				break;
			case T_PartitionBoundSpec:
				_outPartitionBoundSpec(str, obj);
				break;
			case T_PartitionRangeDatum:
				_outPartitionRangeDatum(str, obj);
				break;

			case T_InsertStmt:
				_outInsertStmt(str, obj);
				break;

			case T_DeleteStmt:
				_outDeleteStmt(str, obj);
				break;

			case T_UpdateStmt:
				_outUpdateStmt(str, obj);
				break;

			case T_MergeStmt:
				_outMergeStmt(str, obj);
				break;

			case T_TransactionStmt:
				_outTransactionStmt(str, obj);
				break;

			case T_TruncateStmt:
				_outTruncateStmt(str, obj);
				break;

			case T_VacuumStmt:
				_outVacuumStmt(str, obj);
				break;

			case T_ExplainStmt:
				_outExplainStmt(str, obj);
				break;

			case T_ClusterStmt:
				_outClusterStmt(str, obj);
				break;

			case T_CheckPointStmt:
				_outCheckPointStmt(str, obj);
				break;

			case T_ClosePortalStmt:
				_outClosePortalStmt(str, obj);
				break;

			case T_ListenStmt:
				_outListenStmt(str, obj);
				break;

			case T_UnlistenStmt:
				_outUnlistenStmt(str, obj);
				break;

			case T_LoadStmt:
				_outLoadStmt(str, obj);
				break;

			case T_CopyStmt:
				_outCopyStmt(str, obj);
				break;

			case T_DeallocateStmt:
				_outDeallocateStmt(str, obj);
				break;

			case T_RenameStmt:
				_outRenameStmt(str, obj);
				break;

			case T_CreateRoleStmt:
				_outCreateRoleStmt(str, obj);
				break;

			case T_AlterRoleStmt:
				_outAlterRoleStmt(str, obj);
				break;

			case T_AlterRoleSetStmt:
				_outAlterRoleSetStmt(str, obj);
				break;

			case T_RoleSpec:
				_outRoleSpec(str, obj);
				break;

			case T_DropRoleStmt:
				_outDropRoleStmt(str, obj);
				break;

			case T_CreateSchemaStmt:
				_outCreateSchemaStmt(str, obj);
				break;

			case T_VariableSetStmt:
				_outVariableSetStmt(str, obj);
				break;

			case T_VariableShowStmt:
				_outVariableShowStmt(str, obj);
				break;

			case T_ConstraintsSetStmt:
				_outConstraintsSetStmt(str, obj);
				break;

			case T_AlterTableStmt:
				_outAlterTableStmt(str, obj);
				break;

			case T_AlterTableCmd:
				_outAlterTableCmd(str, obj);
				break;

			case T_CreateSeqStmt:
				_outCreateSeqStmt(str, obj);
				break;

			case T_AlterSeqStmt:
				_outAlterSeqStmt(str, obj);
				break;

			case T_CreatePLangStmt:
				_outCreatePLangStmt(str, obj);
				break;

			case T_CreateTableSpaceStmt:
				_outCreateTableSpaceStmt(str, obj);
				break;

			case T_DropTableSpaceStmt:
				_outDropTableSpaceStmt(str, obj);
				break;

			case T_CreateTrigStmt:
				_outCreateTrigStmt(str, obj);
				break;

			case T_DefineStmt:
				_outDefineStmt(str, obj);
				break;

			case T_CreateOpClassStmt:
				_outCreateOpClassStmt(str, obj);
				break;

			case T_CreateOpClassItem:
				_outCreateOpClassItem(str, obj);
				break;

			case T_DropStmt:
				_outDropStmt(str, obj);
				break;

			case T_FetchStmt:
				_outFetchStmt(str, obj);
				break;

			case T_GrantStmt:
				_outGrantStmt(str, obj);
				break;

			case T_ObjectWithArgs:
				_outObjectWithArgs(str, obj);
				break;

			case T_FunctionParameter:
				_outFunctionParameter(str, obj);
				break;

			case T_GrantRoleStmt:
				_outGrantRoleStmt(str, obj);
				break;

			case T_CreateFunctionStmt:
				_outCreateFunctionStmt(str, obj);
				break;

			case T_AlterFunctionStmt:
				_outAlterFunctionStmt(str, obj);
				break;

			case T_CreateCastStmt:
				_outCreateCastStmt(str, obj);
				break;

			case T_ReindexStmt:
				_outReindexStmt(str, obj);
				break;

			case T_AlterObjectSchemaStmt:
				_outAlterObjectSchemaStmt(str, obj);
				break;

			case T_AlterOwnerStmt:
				_outAlterOwnerStmt(str, obj);
				break;

			case T_RuleStmt:
				_outRuleStmt(str, obj);
				break;

			case T_ViewStmt:
				_outViewStmt(str, obj);
				break;

			case T_CreatedbStmt:
				_outCreatedbStmt(str, obj);
				break;

			case T_AlterDatabaseStmt:
				_outAlterDatabaseStmt(str, obj);
				break;

			case T_AlterDatabaseSetStmt:
				_outAlterDatabaseSetStmt(str, obj);
				break;

			case T_DropdbStmt:
				_outDropdbStmt(str, obj);
				break;

			case T_CreateDomainStmt:
				_outCreateDomainStmt(str, obj);
				break;

			case T_AlterDomainStmt:
				_outAlterDomainStmt(str, obj);
				break;

			case T_CreateConversionStmt:
				_outCreateConversionStmt(str, obj);
				break;

			case T_PrepareStmt:
				_outPrepareStmt(str, obj);
				break;

			case T_ExecuteStmt:
				_outExecuteStmt(str, obj);
				break;

			case T_LockStmt:
				_outLockStmt(str, obj);
				break;

			case T_CommentStmt:
				_outCommentStmt(str, obj);
				break;

			case T_DiscardStmt:
				_outDiscardStmt(str, obj);
				break;

			case T_CreateOpFamilyStmt:
				_outCreateOpFamilyStmt(str, obj);
				break;

			case T_AlterOpFamilyStmt:
				_outAlterOpFamilyStmt(str, obj);
				break;

			case T_CreateEnumStmt:
				_outCreateEnumStmt(str, obj);
				break;

			case T_DropOwnedStmt:
				_outDropOwnedStmt(str, obj);
				break;

			case T_ReassignOwnedStmt:
				_outReassignOwnedStmt(str, obj);
				break;

			case T_AlterTSDictionaryStmt:
				_outAlterTSDictionaryStmt(str, obj);
				break;

			case T_AlterTSConfigurationStmt:
				_outAlterTSConfigurationStmt(str, obj);
				break;

			case T_OnConflictClause:
				_outOnConflictClause(str, obj);
				break;

			default:
				break;
		}
	}
}

/*
 * nodeToString -
 *	   returns the ascii representation of the Node as a palloc'd string
 */
char *
nodeToString(const void *obj)
{
	StringInfoData	   str;

	initStringInfo(&str);
	_outNode(&str, (void *) obj);
	return str.data;
}
