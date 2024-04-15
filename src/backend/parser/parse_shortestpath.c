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

#include "ag_const.h"
#include "catalog/ag_graph_fn.h"
#include "catalog/pg_type.h"
#include "nodes/makefuncs.h"
#include "nodes/nodeFuncs.h"
#include "nodes/pg_list.h"
#include "parser/analyze.h"
#include "parser/parse_agg.h"
#include "parser/parse_collate.h"
#include "parser/parse_cypher_expr.h"
#include "parser/parse_expr.h"
#include "parser/parse_func.h"
#include "parser/parse_relation.h"
#include "parser/parse_shortestpath.h"
#include "parser/parse_target.h"
#include "utils/builtins.h"
#include "parser/parse_cypher_utils.h"

#define SP_COLNAME_VIDS		"vids"
#define SP_COLNAME_EIDS		"eids"

#define SP_ALIASNAME_START_LEFT  "lstart"
#define SP_ALIASNAME_START_RIGHT "rstart"
#define SP_ALIASNAME_END_LEFT    "lend"
#define SP_ALIASNAME_END_RIGHT   "rend"
#define SP_ALIASNAME_OID_LEFT    "loid"
#define SP_ALIASNAME_OID_RIGHT   "roid"
#define SP_ALIASNAME_TID_LEFT    "ltid"
#define SP_ALIASNAME_TID_RIGHT   "rtid"

/* semantic checks */
static void checkNodeForRefForDijkstra(ParseState *pstate, CypherNode *cnode);
static void checkNodeReferableForDijkstra(ParseState *pstate, CypherNode *cnode);
static void checkNodeForRef(ParseState *pstate, CypherNode *cnode);
static void checkNodeReferable(ParseState *pstate, CypherNode *cnode);
static void checkRelFormat(ParseState *pstate, CypherRel *rel, bool is_match);
static void checkRelFormatForDijkstra(ParseState *pstate, CypherRel *crel);

/* shortest path */
static Query *makeShortestPathQuery(ParseState *pstate, CypherPath *cpath,
									bool isexpr);
static ParseNamespaceItem *makeShortestPathFrom(ParseState *parentParseState,
												CypherPath *cpath);
static ParseNamespaceItem *makeShortestpathEdgeLeftQuery(ParseState *pstate,
														 CypherPath *cpath);
static ParseNamespaceItem *makeShortestpathEdgeRightQuery(ParseState *pstate,
														  CypherPath *cpath);
static Node *makeShortestpathEdgeUnionLeft(char *elabel_name, CypherRel *crel);
static Node *makeShortestpathEdgeUnionRight(char *elabel_name, CypherRel *crel);
static Node *makeShortestpathEdgeLeft(char *elabel_name, CypherRel *crel);
static Node *makeShortestpathEdgeRight(char *elabel_name, CypherRel *crel);
static Node *makeVerticesSubLink(void);
static Node *makeEdgesSubLink(CypherPath *cpath, bool is_dijkstra);
static void getCypherRelType(CypherRel *crel, char **typname, int *typloc);
static Node *makeVertexIdExpr(Node *vertex);

/* dijkstra */
static Query *makeDijkstraQuery(ParseState *pstate, CypherPath *cpath,
								bool is_expr);
static ParseNamespaceItem *makeDijkstraFrom(ParseState *parentParseState,
											CypherPath *cpath);
static ParseNamespaceItem *makeDijkstraEdgeQuery(ParseState *pstate,
												 CypherPath *cpath);
static Node *makeDijkstraEdgeUnion(char *elabel_name, char *row_name);
static Node *makeDijkstraEdge(char *elabel_name, char *row_name);

/* parse node */
static Node *makeColumnRef1(char *colname);
static Node *makeColumnRef(List *fields);
static ResTarget *makeSimpleResTarget(char *field, char *name);
static ResTarget *makeResTarget(Node *val, char *name);
static Node *makeAArrayExpr(List *elements, char *typeName);
static Node *makeRowExpr(List *args, char *typeName);
static Node *makeSubLink(SelectStmt *sel);

Query *
transformShortestPath(ParseState *pstate, CypherPath *cpath)
{
	Assert(list_length(cpath->chain) == 3);

	checkNodeForRef(pstate, linitial(cpath->chain));
	checkRelFormat(pstate, lsecond(cpath->chain), false);
	checkNodeForRef(pstate, llast(cpath->chain));

	return makeShortestPathQuery(pstate, cpath, true);
}

Query *
transformShortestPathInMatch(ParseState *parentParseState, CypherPath *cpath)
{
	ParseState *pstate = make_parsestate(parentParseState);
	Query	   *qry;

	Assert(list_length(cpath->chain) == 3);

	checkNodeReferable(pstate, linitial(cpath->chain));
	checkRelFormat(pstate, lsecond(cpath->chain), true);
	checkNodeReferable(pstate, llast(cpath->chain));

	qry = makeShortestPathQuery(pstate, cpath, false);

	free_parsestate(pstate);

	return qry;
}

static void
checkNodeForRefForDijkstra(ParseState *pstate, CypherNode *cnode)
{
	checkNodeReferable(pstate, cnode);

	if (getCypherName(cnode->label) != NULL)
		ereport(ERROR,
				(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
						errmsg("label is not supported"),
						parser_errposition(pstate, getCypherNameLoc(cnode->label))));

	if (cnode->prop_map != NULL)
		ereport(ERROR,
				(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
						errmsg("property constraint is not supported"),
						parser_errposition(pstate,
										   getCypherNameLoc(cnode->variable))));
}

static void
checkNodeReferableForDijkstra(ParseState *pstate, CypherNode *cnode)
{
	char	   *varname = getCypherName(cnode->variable);
	int			varloc = getCypherNameLoc(cnode->variable);
	Node	   *col;

	if (varname == NULL)
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_COLUMN_REFERENCE),
						errmsg("node must be a reference to a specific node")));

	col = colNameToVar(pstate, varname, false, varloc);
	if (col == NULL)
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_COLUMN_REFERENCE),
						errmsg("variable \"%s\" does not exist", varname),
						parser_errposition(pstate, varloc)));
	if (exprType(col) != VERTEXOID)
		ereport(ERROR,
				(errcode(ERRCODE_DATATYPE_MISMATCH),
						errmsg("variable \"%s\" is not a vertex", varname),
						parser_errposition(pstate, varloc)));

	if (cnode->prop_map != NULL)
		ereport(ERROR,
				(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
						errmsg("property constraint is not supported"),
						parser_errposition(pstate, varloc)));
}

static void
checkNodeForRef(ParseState *pstate, CypherNode *cnode)
{
	checkNodeReferable(pstate, cnode);

	if (getCypherName(cnode->label) != NULL)
		ereport(ERROR,
				(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
						errmsg("label is not supported"),
						parser_errposition(pstate, getCypherNameLoc(cnode->label))));

	if (cnode->prop_map != NULL)
		ereport(ERROR,
				(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
						errmsg("property constraint is not supported"),
						parser_errposition(pstate,
										   getCypherNameLoc(cnode->variable))));
}

static void
checkNodeReferable(ParseState *pstate, CypherNode *cnode)
{
	char	   *varname = getCypherName(cnode->variable);
	int			varloc = getCypherNameLoc(cnode->variable);
	Node	   *col;

	if (varname == NULL)
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_COLUMN_REFERENCE),
						errmsg("node must be a reference to a specific node")));

	col = colNameToVar(pstate, varname, false, varloc);
	if (col == NULL)
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_COLUMN_REFERENCE),
						errmsg("variable \"%s\" does not exist", varname),
						parser_errposition(pstate, varloc)));
	if (exprType(col) != VERTEXOID)
		ereport(ERROR,
				(errcode(ERRCODE_DATATYPE_MISMATCH),
						errmsg("variable \"%s\" is not a vertex", varname),
						parser_errposition(pstate, varloc)));
}

static void
checkRelFormat(ParseState *pstate, CypherRel *crel, bool is_match)
{
	char *varname;
	int   varloc;

	if (crel->variable != NULL)
	{
		if (is_match)
		{
			varname = getCypherName(crel->variable);
			varloc = getCypherNameLoc(crel->variable);
			if (varname != NULL)
			{
				if (colNameToVar(pstate, varname, false, varloc) != NULL)
					ereport(ERROR,
							(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
									errmsg("bound variable to edge not allowed in shortestpath"),
									parser_errposition(pstate, getCypherNameLoc(crel->variable))));
			}
		}
		else
		{
			ereport(ERROR,
					(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
							errmsg("variable is not supported"),
							parser_errposition(pstate, getCypherNameLoc(crel->variable))));
		}
	}

	if (crel->varlen != NULL)
	{
		A_Indices  *indices = (A_Indices *) crel->varlen;
		A_Const	   *lidx = (A_Const *) indices->lidx;

		if (lidx->val.val.ival > 1)
			ereport(ERROR,
					(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
							errmsg("only 0 or 1 is allowed for minimal length"),
							parser_errposition(pstate, lidx->location)));
	}

	if (crel->prop_map != NULL)
		ereport(ERROR,
				(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
						errmsg("property constraint is not supported")));
}

/*
 * SELECT (
 *     (
 *       SELECT array_agg(
 *           (
 *             SELECT (id, properties)::vertex
 *             FROM `get_graph_path()`.ag_vertex
 *             WHERE id = vid
 *           )
 *         )
 *       FROM unnest(vids) AS vid
 *     ),
 *     (
 *       SELECT array_agg(
 *           (
 *             SELECT (id, start, "end", properties)::edge
 *             FROM `get_graph_path()`.`typname`
 *             WHERE tableoid = rowid_tableoid(eid) AND ctid = rowid_ctid(eid)
 *           )
 *         )
 *       FROM unnest(eids) AS eid
 *     )
 *   )::graphpath AS `pathname`,
 *   (
 *     SELECT array_agg(
 *         (
 *           SELECT (id, start, "end", properties)::edge
 *           FROM `get_graph_path()`.`typname`
 *           WHERE tableoid = rowid_tableoid(eid) AND ctid = rowid_ctid(eid)
 *         )
 *       )
 *     FROM unnest(eids) AS eid
 *   ) AS `relname`
 * FROM
 * (
 *   SELECT shortestpath_graphids() as vids,
 *          shortestpath_rowids() as eids
 *   FROM `graph_path`.edge_label
 *   WHERE start = id(source)
 *     AND "end" = id(target)
 *
 *   SHORTESTPATH( id(source), id(target) )
 * )
 */
static Query *
makeShortestPathQuery(ParseState *pstate, CypherPath *cpath, bool isexpr)
{
	Query 	   *qry;
	ParseNamespaceItem *nsitem;
	Node	   *vertices;
	Node	   *edges;
	Node	   *empty_edges;
	CoalesceExpr *coalesced;
	Node	   *path;
	Node	   *expr;
	CypherRel  *crel;
	char	   *pathname;
	char	   *edgename;
	TargetEntry *te;

	qry = makeNode(Query);
	qry->commandType = CMD_SELECT;

	nsitem = makeShortestPathFrom(pstate, cpath);
	addNSItemToJoinlist(pstate, nsitem, true);

	pathname = getCypherName(cpath->variable);
	if ( pathname != NULL || isexpr)
	{
		vertices = makeVerticesSubLink();
		edges = makeEdgesSubLink(cpath, false);
		empty_edges = makeAArrayExpr(NIL, "_edge");
		coalesced = makeNode(CoalesceExpr);
		coalesced->args = list_make2(edges, empty_edges);
		coalesced->location = -1;
		path = makeRowExpr(list_make2(vertices, coalesced), "graphpath");
		if (cpath->kind == CPATH_SHORTEST_ALL && isexpr)
		{
			FuncCall *arragg;

			arragg = makeFuncCall(list_make1(makeString("array_agg")),
								  list_make1(path), -1);
			path = (Node *) arragg;
		}
		expr = transformExpr(pstate, path, EXPR_KIND_SELECT_TARGET);
		te = makeTargetEntry((Expr *) expr,
							 (AttrNumber) pstate->p_next_resno++,
							 pathname, false);
		qry->targetList = lappend(qry->targetList, te);
	}

	crel = lsecond(cpath->chain);
	if (crel->variable != NULL)
	{
		edges = makeEdgesSubLink(cpath, false);
		expr = transformExpr(pstate, edges, EXPR_KIND_SELECT_TARGET);
		edgename = getCypherName(crel->variable);
		te = makeTargetEntry((Expr *) expr,
							 (AttrNumber) pstate->p_next_resno++,
							 edgename, false);
		qry->targetList = lappend(qry->targetList, te);
	}

	markTargetListOrigins(pstate, qry->targetList);

	qry->rtable = pstate->p_rtable;
	qry->jointree = makeFromExpr(pstate->p_joinlist, NULL);

	qry->hasSubLinks = pstate->p_hasSubLinks;
	qry->hasAggs = pstate->p_hasAggs;
	if (qry->hasAggs)
		parseCheckAggregates(pstate, qry);

	assign_query_collations(pstate, qry);

	return qry;
}

/*
 * SELECT shortestpath_vids() as vids,
 *        shortestpath_eids() as eids
 * FROM `graph_path`.edge_label
 * WHERE start = id(source)
 *   AND "end" = id(target)
 *
 * SHORTESTPATH( id(source), id(target) )
 */
static ParseNamespaceItem *
makeShortestPathFrom(ParseState *parentParseState, CypherPath *cpath)
{
	Alias	      *alias;
	ParseState    *pstate;
	Query	      *qry;
	Node	      *target;
	TargetEntry   *te;
	FuncCall      *fc;
	CypherRel     *crel;
	Node  	      *start;
	Node  	      *end;
	CypherNode    *vertex;
	Node	      *param;
	Node	      *vertex_id;
	List	      *where = NIL;
	Node	      *qual;
	ParseNamespaceItem *nsitem;

	Assert(parentParseState->p_expr_kind == EXPR_KIND_NONE);
	parentParseState->p_expr_kind = EXPR_KIND_FROM_SUBSELECT;

	alias = makeAlias("_d", NIL);

	pstate = make_parsestate(parentParseState);
	pstate->p_locked_from_parent = isLockedRefname(pstate, alias->aliasname);

	qry = makeNode(Query);
	qry->commandType = CMD_SELECT;

	nsitem = makeShortestpathEdgeLeftQuery(pstate, cpath);
	addNSItemToJoinlist(pstate, nsitem, true);
	nsitem = makeShortestpathEdgeRightQuery(pstate, cpath);
	addNSItemToJoinlist(pstate, nsitem, true);

	/* vids */
	fc = makeFuncCall(list_make1(makeString("shortestpath_graphids")), NIL, -1);
	target = ParseFuncOrColumn(pstate, fc->funcname, NIL, pstate->p_last_srf,
							   fc, false, -1);
	te = makeTargetEntry((Expr *) target,
						 (AttrNumber) pstate->p_next_resno++,
						 "vids", false);
	qry->targetList = list_make1(te);

	/* eids */
	fc = makeFuncCall(list_make1(makeString("shortestpath_rowids")), NIL, -1);
	target = ParseFuncOrColumn(pstate, fc->funcname, NIL, pstate->p_last_srf,
							   fc, false, -1);
	te = makeTargetEntry((Expr *) target,
						 (AttrNumber) pstate->p_next_resno++,
						 "eids", false);
	qry->targetList = lappend(qry->targetList, te);

	/* end ID */
	crel = lsecond(cpath->chain);
	if (crel->direction == CYPHER_REL_DIR_LEFT)
		target = transformExpr(pstate, makeColumnRef1(SP_ALIASNAME_START_LEFT),
							   EXPR_KIND_SELECT_TARGET);
	else
		target = transformExpr(pstate, makeColumnRef1(SP_ALIASNAME_END_LEFT),
							   EXPR_KIND_SELECT_TARGET);
	qry->shortestpathEndIdLeft = target;
	if (crel->direction == CYPHER_REL_DIR_LEFT)
		target = transformExpr(pstate, makeColumnRef1(SP_ALIASNAME_END_RIGHT),
							   EXPR_KIND_SELECT_TARGET);
	else
		target = transformExpr(pstate, makeColumnRef1(SP_ALIASNAME_START_RIGHT),
							   EXPR_KIND_SELECT_TARGET);
	qry->shortestpathEndIdRight = target;

	/* minhop & maxhop */
	qry->shortestpathMinhops = 1;
	qry->shortestpathMaxhops = 1;
	if (crel->varlen != NULL)
	{
		A_Indices     *indices;
		A_Const       *idx;
		qry->shortestpathMaxhops = LONG_MAX;
		indices = (A_Indices *) crel->varlen;
		if( indices->lidx != NULL)
		{
			idx = (A_Const *)indices->lidx;
			qry->shortestpathMinhops = idx->val.val.ival;
		}
		if( indices->uidx != NULL)
		{
			idx = (A_Const *)indices->uidx;
			qry->shortestpathMaxhops = idx->val.val.ival;
		}
	}

	/* limit */
	qry->shortestpathLimit = 1;
	if (cpath->kind == CPATH_SHORTEST_ALL)
		qry->shortestpathLimit = LONG_MAX;

	/* edge ID */
	target = transformExpr(pstate, makeColumnRef1(SP_ALIASNAME_OID_LEFT),
						   EXPR_KIND_SELECT_TARGET);
	qry->shortestpathTableOidLeft = target;
	target = transformExpr(pstate, makeColumnRef1(SP_ALIASNAME_OID_RIGHT),
						   EXPR_KIND_SELECT_TARGET);
	qry->shortestpathTableOidRight = target;
	target = transformExpr(pstate, makeColumnRef1(SP_ALIASNAME_TID_LEFT),
						   EXPR_KIND_SELECT_TARGET);
	qry->shortestpathCtidLeft = target;
	target = transformExpr(pstate, makeColumnRef1(SP_ALIASNAME_TID_RIGHT),
						   EXPR_KIND_SELECT_TARGET);
	qry->shortestpathCtidRight = target;

	markTargetListOrigins(pstate, qry->targetList);

	/* WHERE */
	if (crel->direction == CYPHER_REL_DIR_LEFT)
	{
		start = makeColumnRef1(SP_ALIASNAME_END_LEFT);
		end   = makeColumnRef1(SP_ALIASNAME_START_RIGHT);
	}
	else
	{
		start = makeColumnRef1(SP_ALIASNAME_START_LEFT);
		end   = makeColumnRef1(SP_ALIASNAME_END_RIGHT);
	}

	vertex = linitial(cpath->chain);
	param = makeColumnRef1(getCypherName(vertex->variable));
	vertex_id = makeVertexIdExpr(param);

	where = list_make1(makeSimpleA_Expr(AEXPR_OP, "=", start, vertex_id, -1));

	vertex = llast(cpath->chain);
	param = makeColumnRef1(getCypherName(vertex->variable));
	vertex_id = makeVertexIdExpr(param);

	where = lappend(where, makeSimpleA_Expr(AEXPR_OP, "=", end, vertex_id, -1));

	/* qual */
	/* Add Property Constraint
	 * if (cpath->qual != NULL)
	 *    where = lappend(where, cpath->qual);
	 */

	qual = transformCypherExpr(pstate,
							   (Node *) makeBoolExpr(AND_EXPR, where, -1),
							   EXPR_KIND_WHERE);

	/* Shortestpath source */
	vertex = linitial(cpath->chain);
	param = makeColumnRef1(getCypherName(vertex->variable));
	vertex_id = makeVertexIdExpr(param);
	qry->shortestpathSource = transformExpr(pstate,
											(Node *) copyObject(vertex_id),
											EXPR_KIND_SELECT_TARGET);

	/* Shortestpath target */
	vertex = llast(cpath->chain);
	param = makeColumnRef1(getCypherName(vertex->variable));
	vertex_id = makeVertexIdExpr(param);
	qry->shortestpathTarget = transformExpr(pstate,
											vertex_id,
											EXPR_KIND_SELECT_TARGET);

	qry->rtable = pstate->p_rtable;
	qry->jointree = makeFromExpr(pstate->p_joinlist, qual);

	qry->hasSubLinks = pstate->p_hasSubLinks;
	qry->hasAggs = pstate->p_hasAggs;
	if (qry->hasAggs)
		parseCheckAggregates(pstate, qry);

	assign_query_collations(pstate, qry);

	parentParseState->p_expr_kind = EXPR_KIND_NONE;

	return addRangeTableEntryForSubquery(parentParseState, qry, alias, false,
										 true);
}

static ParseNamespaceItem *
makeShortestpathEdgeLeftQuery(ParseState *pstate, CypherPath *cpath)
{
	CypherRel 	   *crel;
	char 		   *elabel_name;
	Alias		   *alias;
	Node		   *sub;
	Query		   *qry;

	Assert(pstate->p_expr_kind == EXPR_KIND_NONE);
	pstate->p_expr_kind = EXPR_KIND_FROM_SUBSELECT;

	crel = lsecond(cpath->chain);
	getCypherRelType(crel, &elabel_name, NULL);

	if (crel->direction == CYPHER_REL_DIR_NONE)
		sub = makeShortestpathEdgeUnionLeft(elabel_name, crel);
	else
		sub = makeShortestpathEdgeLeft(elabel_name, crel);

	alias = makeAliasOptUnique(NULL);
	qry = parse_sub_analyze((Node *) sub, pstate, NULL,
							isLockedRefname(pstate, alias->aliasname), true);
	pstate->p_expr_kind = EXPR_KIND_NONE;

	return addRangeTableEntryForSubquery(pstate, qry, alias, false, true);
}

static ParseNamespaceItem *
makeShortestpathEdgeRightQuery(ParseState *pstate, CypherPath *cpath)
{
	CypherRel 	   *crel;
	char 		   *elabel_name;
	Alias		   *alias;
	Node		   *sub;
	Query		   *qry;

	Assert(pstate->p_expr_kind == EXPR_KIND_NONE);
	pstate->p_expr_kind = EXPR_KIND_FROM_SUBSELECT;

	crel = lsecond(cpath->chain);
	getCypherRelType(crel, &elabel_name, NULL);

	if (crel->direction == CYPHER_REL_DIR_NONE)
		sub = makeShortestpathEdgeUnionRight(elabel_name, crel);
	else
		sub = makeShortestpathEdgeRight(elabel_name, crel);

	alias = makeAliasOptUnique(NULL);
	qry = parse_sub_analyze((Node *) sub, pstate, NULL,
							isLockedRefname(pstate, alias->aliasname), true);
	pstate->p_expr_kind = EXPR_KIND_NONE;

	return addRangeTableEntryForSubquery(pstate, qry, alias, false, true);
}

/*
 * SELECT start lstart, "end" lend, ltableoid loid, lctid ltid
 * FROM (
 *   SELECT start, "end", tableoid ltableoid, ctid lctid FROM `get_graph_path()`.`elabel_name`
 *   UNION
 *   SELECT "end", start, tableoid rtableoid, ctid rctid FROM `get_graph_path()`.`elabel_name`
 * )
 */
static Node *
makeShortestpathEdgeUnionLeft(char *elabel_name, CypherRel *crel)
{
	RangeVar   *r;
	SelectStmt *lsel;
	SelectStmt *rsel;
	SelectStmt *u;
	RangeSubselect *sub_sel;
	SelectStmt *sel;

	r = makeRangeVar(get_graph_path(true), elabel_name, -1);
	r->inh = (crel->only) ? false : true;

	lsel = makeNode(SelectStmt);
	lsel->fromClause = list_make1(r);

	rsel = copyObject(lsel);

	lsel->targetList = lappend(lsel->targetList,
							   makeSimpleResTarget(AG_START_ID, NULL));
	lsel->targetList = lappend(lsel->targetList,
							   makeSimpleResTarget(AG_END_ID, NULL));
	lsel->targetList = lappend(lsel->targetList,
							   makeSimpleResTarget("tableoid", "ltableoid"));
	lsel->targetList = lappend(lsel->targetList,
							   makeSimpleResTarget("ctid", "lctid"));

	rsel->targetList = lappend(rsel->targetList,
							   makeSimpleResTarget(AG_END_ID, NULL));
	rsel->targetList = lappend(rsel->targetList,
							   makeSimpleResTarget(AG_START_ID, NULL));
	rsel->targetList = lappend(rsel->targetList,
							   makeSimpleResTarget("tableoid", "rtableoid"));
	rsel->targetList = lappend(rsel->targetList,
							   makeSimpleResTarget("ctid", "rctid"));

	u = makeNode(SelectStmt);
	u->op = SETOP_UNION;
	u->all = false;
	u->larg = lsel;
	u->rarg = rsel;

	sub_sel = makeNode(RangeSubselect);
	sub_sel->subquery = (Node *) u;
	sub_sel->alias = makeAliasOptUnique(NULL);

	sel = makeNode(SelectStmt);
	sel->fromClause = list_make1(sub_sel);

	sel->targetList = list_make4(makeSimpleResTarget(AG_START_ID, SP_ALIASNAME_START_LEFT),
								 makeSimpleResTarget(AG_END_ID, SP_ALIASNAME_END_LEFT),
								 makeSimpleResTarget("ltableoid", SP_ALIASNAME_OID_LEFT),
								 makeSimpleResTarget("lctid", SP_ALIASNAME_TID_LEFT));

	return (Node *) sel;
}

/*
 * SELECT start rstart, "end" rend, ltableoid roid, lctid rtid
 * FROM (
 *   SELECT start, "end", tableoid ltableoid, ctid lctid FROM `get_graph_path()`.`elabel_name`
 *   UNION
 *   SELECT "end", start, tableoid rtableoid, ctid rctid FROM `get_graph_path()`.`elabel_name`
 * )
 */
static Node *
makeShortestpathEdgeUnionRight(char *elabel_name, CypherRel *crel)
{
	RangeVar   *r;
	SelectStmt *lsel;
	SelectStmt *rsel;
	SelectStmt *u;
	RangeSubselect *sub_sel;
	SelectStmt *sel;

	r = makeRangeVar(get_graph_path(true), elabel_name, -1);
	r->inh = (crel->only) ? false : true;

	lsel = makeNode(SelectStmt);
	lsel->fromClause = list_make1(r);

	rsel = copyObject(lsel);

	lsel->targetList = lappend(lsel->targetList,
							   makeSimpleResTarget(AG_START_ID, NULL));
	lsel->targetList = lappend(lsel->targetList,
							   makeSimpleResTarget(AG_END_ID, NULL));
	lsel->targetList = lappend(lsel->targetList,
							   makeSimpleResTarget("tableoid", "ltableoid"));
	lsel->targetList = lappend(lsel->targetList,
							   makeSimpleResTarget("ctid", "lctid"));

	rsel->targetList = lappend(rsel->targetList,
							   makeSimpleResTarget(AG_END_ID, NULL));
	rsel->targetList = lappend(rsel->targetList,
							   makeSimpleResTarget(AG_START_ID, NULL));
	rsel->targetList = lappend(rsel->targetList,
							   makeSimpleResTarget("tableoid", "rtableoid"));
	rsel->targetList = lappend(rsel->targetList,
							   makeSimpleResTarget("ctid", "rctid"));

	u = makeNode(SelectStmt);
	u->op = SETOP_UNION;
	u->all = false;
	u->larg = lsel;
	u->rarg = rsel;

	sub_sel = makeNode(RangeSubselect);
	sub_sel->subquery = (Node *) u;
	sub_sel->alias = makeAliasOptUnique(NULL);

	sel = makeNode(SelectStmt);
	sel->fromClause = list_make1(sub_sel);

	sel->targetList = list_make4(makeSimpleResTarget(AG_START_ID, SP_ALIASNAME_START_RIGHT),
								 makeSimpleResTarget(AG_END_ID, SP_ALIASNAME_END_RIGHT),
								 makeSimpleResTarget("ltableoid", SP_ALIASNAME_OID_RIGHT),
								 makeSimpleResTarget("lctid", SP_ALIASNAME_TID_RIGHT));

	return (Node *) sel;
}

/*
 * SELECT start lstart, "end" lend, tableoid loid, ctid ltid FROM
 * ( SELECT start, "end", tableoid, ctid FROM `get_graph_path()`.`elabel_name` )
 */
static Node *
makeShortestpathEdgeLeft(char *elabel_name, CypherRel *crel)
{
	SelectStmt *sel;
	RangeVar   *r;

	sel = makeNode(SelectStmt);

	r = makeRangeVar(get_graph_path(true), elabel_name, -1);
	r->inh = (crel->only) ? false : true;
	sel->fromClause = list_make1(r);

	sel->targetList = list_make4(makeSimpleResTarget(AG_START_ID, SP_ALIASNAME_START_LEFT),
								 makeSimpleResTarget(AG_END_ID, SP_ALIASNAME_END_LEFT),
								 makeSimpleResTarget("tableoid", SP_ALIASNAME_OID_LEFT),
								 makeSimpleResTarget("ctid", SP_ALIASNAME_TID_LEFT));

	return (Node *) sel;
}

/*
 * SELECT start rstart, "end" rend, tableoid roid, ctid rtid FROM
 * ( SELECT start, "end", tableoid, ctid FROM `get_graph_path()`.`elabel_name` )
 */
static Node *
makeShortestpathEdgeRight(char *elabel_name, CypherRel *crel)
{
	SelectStmt *sel;
	RangeVar   *r;

	sel = makeNode(SelectStmt);

	r = makeRangeVar(get_graph_path(true), elabel_name, -1);
	r->inh = (crel->only) ? false : true;
	sel->fromClause = list_make1(r);

	sel->targetList = list_make4(makeSimpleResTarget(AG_START_ID, SP_ALIASNAME_START_RIGHT),
								 makeSimpleResTarget(AG_END_ID, SP_ALIASNAME_END_RIGHT),
								 makeSimpleResTarget("tableoid", SP_ALIASNAME_OID_RIGHT),
								 makeSimpleResTarget("ctid", SP_ALIASNAME_TID_RIGHT));

	return (Node *) sel;
}

/*
 * SELECT array_agg(
 *     (
 *       SELECT (id, properties)::vertex
 *       FROM `get_graph_path()`.ag_vertex
 *       WHERE id = vid
 *     )
 *   )
 * FROM unnest(vids) AS vid
 */
static Node *
makeVerticesSubLink(void)
{
	Node	   *id;
	SelectStmt *selsub;
	Node	   *vertex;
	RangeVar   *ag_vertex;
	A_Expr	   *qual;
	Node	   *vertices;
	SelectStmt *sel;
	FuncCall   *arragg;
	FuncCall   *unnest;
	RangeFunction *vid;

	/*
	 * SubLink for `array_agg()`
	 */

	id = makeColumnRef1(AG_ELEM_LOCAL_ID);

	selsub = makeNode(SelectStmt);

	vertex = makeRowExpr(list_make3(id,
									makeColumnRef1(AG_ELEM_PROP_MAP),
									makeColumnRef1("ctid")),
						 "vertex");
	selsub->targetList = list_make1(makeResTarget(vertex, NULL));

	ag_vertex = makeRangeVar(get_graph_path(true), AG_VERTEX, -1);
	ag_vertex->inh = true;
	selsub->fromClause = list_make1(ag_vertex);

	qual = makeSimpleA_Expr(AEXPR_OP, "=", id, makeColumnRef1("vid"), -1);
	selsub->whereClause = (Node *) makeBoolExpr(AND_EXPR, list_make1(qual), -1);

	vertices = makeSubLink(selsub);

	/*
	 * SELECT which gets array of vertices from `vids`
	 */

	sel = makeNode(SelectStmt);

	arragg = makeFuncCall(list_make1(makeString("array_agg")),
						  list_make1(vertices), -1);
	sel->targetList = list_make1(makeResTarget((Node *) arragg, NULL));

	unnest = makeFuncCall(list_make1(makeString("unnest")),
						  list_make1(makeColumnRef1(SP_COLNAME_VIDS)), -1);
	vid = makeNode(RangeFunction);
	vid->lateral = false;
	vid->ordinality = false;
	vid->is_rowsfrom = false;
	vid->functions = list_make1(list_make2(unnest, NIL));
	vid->alias = makeAliasNoDup("vid", NIL);
	vid->coldeflist = NIL;
	sel->fromClause = list_make1(vid);

	return makeSubLink(sel);
}

/*
 * SELECT array_agg(
 *     (
 *       SELECT (id, start, "end", properties)::edge
 *       FROM `get_graph_path()`.`typname`
 *
 *       # shortestpath()
 *       WHERE tableoid = rowid_tableoid(eid) AND ctid = rowid_ctid(eid)
 *
 *       # dijkstra()
 *       WHERE id = eid
 *     )
 *   )
 * FROM unnest(eids) AS eid
 */
static Node *
makeEdgesSubLink(CypherPath *cpath, bool is_dijkstra)
{
	Node	   *id;
	SelectStmt *selsub;
	Node	   *edge;
	CypherRel  *crel;
	char	   *typname;
	RangeVar   *e;
	A_Expr	   *qual;
	List	   *where_args = NIL;
	Node	   *edges;
	SelectStmt *sel;
	FuncCall   *arragg;
	FuncCall   *unnest;
	RangeFunction *eid;

	/*
	 * SubLink for `array_agg()`
	 */

	id = makeColumnRef1(AG_ELEM_LOCAL_ID);

	selsub = makeNode(SelectStmt);

	edge = makeRowExpr(list_make5(id,
								  makeColumnRef1(AG_START_ID),
								  makeColumnRef1(AG_END_ID),
								  makeColumnRef1(AG_ELEM_PROP_MAP),
								  makeColumnRef1("ctid")),
					   "edge");
	selsub->targetList = list_make1(makeResTarget(edge, NULL));

	crel = lsecond(cpath->chain);
	getCypherRelType(crel, &typname, NULL);
	e = makeRangeVar(get_graph_path(true), typname, -1);
	e->inh = true;
	selsub->fromClause = list_make1(e);

	if (is_dijkstra)
	{
		qual = makeSimpleA_Expr(AEXPR_OP, "=", copyObject(id),
								makeColumnRef1("eid"), -1);
		where_args = list_make1(qual);
	}
	else
	{
		Node	   *tableoid;
		Node	   *ctid;
		FuncCall   *getid;

		tableoid = makeColumnRef1("tableoid");
		getid = makeFuncCall(list_make1(makeString("rowid_tableoid")),
							 list_make1(makeColumnRef1("eid")), -1);
		qual = makeSimpleA_Expr(AEXPR_OP, "=", tableoid, (Node *) getid, -1);
		where_args = list_make1(qual);

		ctid = makeColumnRef1("ctid");
		getid = makeFuncCall(list_make1(makeString("rowid_ctid")),
							 list_make1(makeColumnRef1("eid")), -1);
		qual = makeSimpleA_Expr(AEXPR_OP, "=", ctid, (Node *) getid, -1);
		where_args = lappend(where_args, qual);
	}

	selsub->whereClause = (Node *) makeBoolExpr(AND_EXPR, where_args, -1);

	edges = makeSubLink(selsub);

	/*
	 * SELECT which gets array of edges from `eids`
	 */

	sel = makeNode(SelectStmt);

	arragg = makeFuncCall(list_make1(makeString("array_agg")),
						  list_make1(edges), -1);
	sel->targetList = list_make1(makeResTarget((Node *) arragg, NULL));

	unnest = makeFuncCall(list_make1(makeString("unnest")),
						  list_make1(makeColumnRef1(SP_COLNAME_EIDS)), -1);
	eid = makeNode(RangeFunction);
	eid->lateral = false;
	eid->ordinality = false;
	eid->is_rowsfrom = false;
	eid->functions = list_make1(list_make2(unnest, NIL));
	eid->alias = makeAliasNoDup("eid", NIL);
	eid->coldeflist = NIL;
	sel->fromClause = list_make1(eid);

	return makeSubLink(sel);
}

static void
getCypherRelType(CypherRel *crel, char **typname, int *typloc)
{
	if (crel->types == NIL)
	{
		*typname = AG_EDGE;
		if (typloc != NULL)
			*typloc = -1;
	}
	else
	{
		Node *type;

		if (list_length(crel->types) > 1)
			ereport(ERROR,
					(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
							errmsg("multiple types for relationship not supported")));

		type = linitial(crel->types);

		*typname = getCypherName(type);
		if (typloc != NULL)
			*typloc = getCypherNameLoc(type);
	}
}

static Node *
makeVertexIdExpr(Node *vertex)
{
	return (Node *) makeFuncCall(list_make1(makeString(AG_ELEM_ID)),
								 list_make1(vertex),
								 -1);
}

static Node *
makeColumnRef1(char *colname)
{
	List *fields;

	fields = list_make1(makeString(pstrdup(colname)));

	return makeColumnRef(fields);
}

static Node *
makeColumnRef(List *fields)
{
	ColumnRef *cref;

	cref = makeNode(ColumnRef);
	cref->fields = fields;
	cref->location = -1;

	return (Node *) cref;
}

static ResTarget *
makeSimpleResTarget(char *field, char *name)
{
	Node *cref;

	cref = makeColumnRef(list_make1(makeString(pstrdup(field))));

	return makeResTarget(cref, name);
}

static ResTarget *
makeResTarget(Node *val, char *name)
{
	ResTarget *res;

	res = makeNode(ResTarget);
	if (name != NULL)
		res->name = pstrdup(name);
	res->val = val;
	res->location = -1;

	return res;
}

static Node *
makeAArrayExpr(List *elements, char *typeName)
{
	A_ArrayExpr *arr;
	TypeCast   *cast;

	arr = makeNode(A_ArrayExpr);
	arr->elements = elements;
	arr->location = -1;

	cast = makeNode(TypeCast);
	cast->arg = (Node *) arr;
	cast->typeName = makeTypeName(typeName);
	cast->location = -1;

	return (Node *) cast;
}

static Node *
makeRowExpr(List *args, char *typeName)
{
	RowExpr	   *row;
	TypeCast   *cast;

	row = makeNode(RowExpr);
	row->args = args;
	row->row_typeid = InvalidOid;
	row->colnames = NIL;
	row->row_format = COERCE_EXPLICIT_CAST;
	row->location = -1;

	cast = makeNode(TypeCast);
	cast->arg = (Node *) row;
	cast->typeName = makeTypeName(typeName);
	cast->location = -1;

	return (Node *) cast;
}

static Node *
makeSubLink(SelectStmt *sel)
{
	SubLink *sublink;

	sublink = makeNode(SubLink);
	sublink->subLinkType = EXPR_SUBLINK;
	sublink->subLinkId = 0;
	sublink->testexpr = NULL;
	sublink->operName = NIL;
	sublink->subselect = (Node *) sel;
	sublink->location = -1;

	return (Node *) sublink;
}

Query *
transformDijkstra(ParseState *pstate, CypherPath *cpath)
{
	Assert(list_length(cpath->chain) == 3);

	checkNodeForRefForDijkstra(pstate, linitial(cpath->chain));
	checkRelFormatForDijkstra(pstate, lsecond(cpath->chain));
	checkNodeForRefForDijkstra(pstate, llast(cpath->chain));

	return makeDijkstraQuery(pstate, cpath, true);
}

Query *
transformDijkstraInMatch(ParseState *parentParseState, CypherPath *cpath)
{
	ParseState *pstate = make_parsestate(parentParseState);
	Query	   *qry;

	Assert(list_length(cpath->chain) == 3);

	checkNodeReferableForDijkstra(pstate, linitial(cpath->chain));
	checkRelFormatForDijkstra(pstate, lsecond(cpath->chain));
	checkNodeReferableForDijkstra(pstate, llast(cpath->chain));

	qry = makeDijkstraQuery(pstate, cpath, false);

	free_parsestate(pstate);

	return qry;
}

static void
checkRelFormatForDijkstra(ParseState *pstate, CypherRel *crel)
{
	if (crel->varlen != NULL)
	{
		A_Indices  *indices = (A_Indices *) crel->varlen;
		A_Const	   *lidx = (A_Const *) indices->lidx;

		ereport(ERROR,
				(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
						errmsg("variable length relationship is not supported"),
						parser_errposition(pstate, lidx->location)));
	}

	if (crel->prop_map != NULL)
		ereport(ERROR,
				(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
						errmsg("property constraint is not supported")));
}

/*
 * path = DIJKSTRA((source)-[:edge_label]->(target), weight, qual, LIMIT n)
 *
 * |
 * v
 *
 * SELECT (
 *     (
 *       SELECT array_agg(
 *           (
 *             SELECT (id, properties)::vertex
 *             FROM `get_graph_path()`.ag_vertex
 *             WHERE id = vid
 *           )
 *         )
 *       FROM unnest(vids) AS vid
 *     ),
 *     (
 *       SELECT array_agg(
 *           (
 *             SELECT (id, start, "end", properties)::edge
 *             FROM `get_graph_path()`.`typname`
 *             WHERE id = eid
 *           )
 *         )
 *       FROM unnest(eids) AS eid
 *     )
 *   )::graphpath AS `pathname`,
 *   weight
 * FROM
 * (
 *   SELECT dijkstra_vids() as vids,
 *          dijkstra_eids() as eids,
 *          weight
 *   FROM `graph_path`.edge_label
 *   WHERE start = id(source) AND `qual`
 *
 *   DIJKSTRA (id(source), id(target), LIMIT n, "end", id)
 * )
 */
static Query *
makeDijkstraQuery(ParseState *pstate, CypherPath *cpath, bool is_expr)
{
	Query 	   *qry;
	ParseNamespaceItem *nsitem;
	Node	   *vertices;
	Node	   *edges;
	Node	   *empty_edges;
	CoalesceExpr *coalesced;
	Node	   *path;
	Node	   *expr;
	char	   *pathname;
	TargetEntry *te;

	qry = makeNode(Query);
	qry->commandType = CMD_SELECT;

	nsitem = makeDijkstraFrom(pstate, cpath);
	addNSItemToJoinlist(pstate, nsitem, true);

	vertices = makeVerticesSubLink();
	edges = makeEdgesSubLink(cpath, true);
	empty_edges = makeAArrayExpr(NIL, "_edge");
	coalesced = makeNode(CoalesceExpr);
	coalesced->args = list_make2(edges, empty_edges);
	coalesced->location = -1;
	path = makeRowExpr(list_make2(vertices, coalesced), "graphpath");
	if (is_expr)
	{
		FuncCall *arragg;

		arragg = makeFuncCall(list_make1(makeString("array_agg")),
							  list_make1(path), -1);
		path = (Node *) arragg;
	}
	expr = transformExpr(pstate, path, EXPR_KIND_SELECT_TARGET);
	pathname = getCypherName(cpath->variable);
	te = makeTargetEntry((Expr *) expr,
						 (AttrNumber) pstate->p_next_resno++,
						 pathname, false);
	qry->targetList = list_make1(te);

	if (cpath->weight_var)
	{
		char *weight_varname = getCypherName(cpath->weight_var);

		expr = transformExpr(pstate, makeColumnRef1("weight"),
							 EXPR_KIND_SELECT_TARGET);
		te = makeTargetEntry((Expr *) expr,
							 (AttrNumber) pstate->p_next_resno++,
							 weight_varname, false);
		qry->targetList = lappend(qry->targetList, te);
	}

	markTargetListOrigins(pstate, qry->targetList);

	qry->rtable = pstate->p_rtable;
	qry->jointree = makeFromExpr(pstate->p_joinlist, NULL);

	qry->hasSubLinks = pstate->p_hasSubLinks;
	qry->hasAggs = pstate->p_hasAggs;
	if (qry->hasAggs)
		parseCheckAggregates(pstate, qry);

	assign_query_collations(pstate, qry);

	return qry;
}

/*
 * SELECT dijkstra_vids() as vids,
 *        dijkstra_eids() as eids,
 *        weight
 * FROM `graph_path`.edge_label
 * WHERE start = id(source) AND `qual`
 *
 * DIJKSTRA (id(source), id(target), LIMIT n, "end", id)
 */
static ParseNamespaceItem *
makeDijkstraFrom(ParseState *parentParseState, CypherPath *cpath)
{
	Alias	   *alias;
	ParseState *pstate;
	Query	   *qry;
	Node	   *target;
	TargetEntry *te;
	FuncCall   *fc;
	CypherRel  *crel;
	Oid			wtype;
	Node  	   *start;
	CypherNode *vertex;
	Node	   *param;
	Node	   *vertex_id;
	List	   *where = NIL;
	Node	   *qual;
	ParseNamespaceItem *nsitem;

	Assert(parentParseState->p_expr_kind == EXPR_KIND_NONE);
	parentParseState->p_expr_kind = EXPR_KIND_FROM_SUBSELECT;

	alias = makeAlias("_d", NIL);

	pstate = make_parsestate(parentParseState);
	pstate->p_locked_from_parent = isLockedRefname(pstate, alias->aliasname);

	qry = makeNode(Query);
	qry->commandType = CMD_SELECT;

	nsitem = makeDijkstraEdgeQuery(pstate, cpath);
	addNSItemToJoinlist(pstate, nsitem, true);

	/* vids */
	fc = makeFuncCall(list_make1(makeString("dijkstra_vids")), NIL, -1);
	target = ParseFuncOrColumn(pstate, fc->funcname, NIL, pstate->p_last_srf,
							   fc, false, -1);
	te = makeTargetEntry((Expr *) target,
						 (AttrNumber) pstate->p_next_resno++,
						 "vids", false);
	qry->targetList = list_make1(te);

	/* eids */
	fc = makeFuncCall(list_make1(makeString("dijkstra_eids")), NIL, -1);
	target = ParseFuncOrColumn(pstate, fc->funcname, NIL, pstate->p_last_srf,
							   fc, false, -1);
	te = makeTargetEntry((Expr *) target,
						 (AttrNumber) pstate->p_next_resno++,
						 "eids", false);
	qry->targetList = lappend(qry->targetList, te);

	/* weight */
	target = transformCypherExpr(pstate, cpath->weight,
								 EXPR_KIND_SELECT_TARGET);
	wtype = exprType(target);
	if (wtype != FLOAT8OID)
	{
		Node	   *weight;

		weight = coerce_expr(pstate, target, wtype, FLOAT8OID, -1,
							 COERCION_EXPLICIT, COERCE_EXPLICIT_CAST, -1);
		if (weight == NULL)
			ereport(ERROR,
					(errcode(ERRCODE_DATATYPE_MISMATCH),
							errmsg("weight must be type %s, not type %s",
								   format_type_be(FLOAT8OID),
								   format_type_be(wtype)),
							parser_errposition(pstate, exprLocation(target))));

		target = weight;
	}
	if (expression_returns_set(target))
		ereport(ERROR,
				(errcode(ERRCODE_DATATYPE_MISMATCH),
						errmsg("weight must not return a set"),
						parser_errposition(pstate, exprLocation(target))));

	te = makeTargetEntry((Expr *) target,
						 (AttrNumber) pstate->p_next_resno++,
						 "weight", cpath->weight_var == NULL);
	qry->targetList = lappend(qry->targetList, te);

	qry->dijkstraWeight = pstate->p_next_resno;
	qry->dijkstraWeightOut = (cpath->weight_var != NULL);

	/* end ID */
	crel = lsecond(cpath->chain);
	if (crel->direction == CYPHER_REL_DIR_LEFT)
		target = transformExpr(pstate, makeColumnRef1(AG_START_ID),
							   EXPR_KIND_SELECT_TARGET);
	else
		target = transformExpr(pstate, makeColumnRef1(AG_END_ID),
							   EXPR_KIND_SELECT_TARGET);
	qry->dijkstraEndId = target;

	/* edge ID */
	target = transformExpr(pstate, makeColumnRef1(AG_ELEM_ID),
						   EXPR_KIND_SELECT_TARGET);
	qry->dijkstraEdgeId = target;

	markTargetListOrigins(pstate, qry->targetList);

	/* WHERE */
	if (crel->direction == CYPHER_REL_DIR_LEFT)
		start = makeColumnRef1(AG_END_ID);
	else
		start = makeColumnRef1(AG_START_ID);

	vertex = linitial(cpath->chain);
	param = makeColumnRef1(getCypherName(vertex->variable));
	vertex_id = makeVertexIdExpr(param);

	where = list_make1(makeSimpleA_Expr(AEXPR_OP, "=", start, vertex_id, -1));

	/* qual */
	if (cpath->qual != NULL)
		where = lappend(where, cpath->qual);

	qual = transformCypherExpr(pstate,
							   (Node *) makeBoolExpr(AND_EXPR, where, -1),
							   EXPR_KIND_WHERE);

	/* Dijkstra source */
	qry->shortestpathSource = transformExpr(pstate,
											(Node *) copyObject(vertex_id),
											EXPR_KIND_SELECT_TARGET);

	/* Dijkstra target */
	vertex = llast(cpath->chain);
	param = makeColumnRef1(getCypherName(vertex->variable));
	vertex_id = makeVertexIdExpr(param);
	qry->shortestpathTarget = transformExpr(pstate,
											vertex_id,
											EXPR_KIND_SELECT_TARGET);

	/* Dijkstra LIMIT */
	qry->dijkstraLimit = transformCypherLimit(pstate, cpath->limit,
											  EXPR_KIND_LIMIT, "LIMIT");

	qry->rtable = pstate->p_rtable;
	qry->jointree = makeFromExpr(pstate->p_joinlist, qual);

	qry->hasSubLinks = pstate->p_hasSubLinks;
	qry->hasAggs = pstate->p_hasAggs;
	if (qry->hasAggs)
		parseCheckAggregates(pstate, qry);

	assign_query_collations(pstate, qry);

	parentParseState->p_expr_kind = EXPR_KIND_NONE;

	return addRangeTableEntryForSubquery(parentParseState, qry, alias, false,
										 true);
}

static ParseNamespaceItem *
makeDijkstraEdgeQuery(ParseState *pstate, CypherPath *cpath)
{
	CypherRel 	   *crel;
	char 		   *elabel_name;
	char		   *row_name;
	Alias		   *alias;
	Node		   *sub;
	Query		   *qry;

	Assert(pstate->p_expr_kind == EXPR_KIND_NONE);
	pstate->p_expr_kind = EXPR_KIND_FROM_SUBSELECT;

	crel = lsecond(cpath->chain);
	getCypherRelType(crel, &elabel_name, NULL);
	row_name = getCypherName(crel->variable);

	if (crel->direction == CYPHER_REL_DIR_NONE)
		sub = makeDijkstraEdgeUnion(elabel_name, row_name);
	else
		sub = makeDijkstraEdge(elabel_name, row_name);

	alias = makeAliasOptUnique(NULL);
	qry = parse_sub_analyze((Node *) sub, pstate, NULL,
							isLockedRefname(pstate, alias->aliasname), true);
	pstate->p_expr_kind = EXPR_KIND_NONE;

	return addRangeTableEntryForSubquery(pstate, qry, alias, false, true);
}

/*
 * SELECT start, "end", id, (id, _start, _end, properties)::edge AS row_name
 * FROM (
 *   SELECT start AS _start, "end" AS _end, start, "end", id, properties
 *   FROM `get_graph_path()`.`elabel_name`
 *   UNION
 *   SELECT start AS _start, "end" AS _end, end, start, id, properties
 *   FROM `get_graph_path()`.`elabel_name`
 * )
 */
static Node *
makeDijkstraEdgeUnion(char *elabel_name, char *row_name)
{
	RangeVar   *r;
	SelectStmt *lsel;
	Node	   *row;
	SelectStmt *rsel;
	SelectStmt *u;
	RangeSubselect *sub_sel;
	SelectStmt *sel;

	r = makeRangeVar(get_graph_path(true), elabel_name, -1);
	r->inh = true;

	lsel = makeNode(SelectStmt);
	lsel->targetList = lappend(lsel->targetList,
							   makeSimpleResTarget(AG_START_ID, "_start"));
	lsel->targetList = lappend(lsel->targetList,
							   makeSimpleResTarget(AG_END_ID, "_end"));
	lsel->targetList = lappend(lsel->targetList,
							   makeSimpleResTarget("ctid", NULL));
	lsel->fromClause = list_make1(r);

	rsel = copyObject(lsel);

	lsel->targetList = lappend(lsel->targetList,
							   makeSimpleResTarget(AG_START_ID, NULL));
	lsel->targetList = lappend(lsel->targetList,
							   makeSimpleResTarget(AG_END_ID, NULL));
	lsel->targetList = lappend(lsel->targetList,
							   makeSimpleResTarget(AG_ELEM_LOCAL_ID, NULL));
	if (row_name != NULL)
		lsel->targetList = lappend(lsel->targetList,
								   makeSimpleResTarget(AG_ELEM_PROP_MAP, NULL));

	rsel->targetList = lappend(rsel->targetList,
							   makeSimpleResTarget(AG_END_ID, NULL));
	rsel->targetList = lappend(rsel->targetList,
							   makeSimpleResTarget(AG_START_ID, NULL));
	rsel->targetList = lappend(rsel->targetList,
							   makeSimpleResTarget(AG_ELEM_LOCAL_ID, NULL));
	if (row_name != NULL)
		rsel->targetList = lappend(rsel->targetList,
								   makeSimpleResTarget(AG_ELEM_PROP_MAP, NULL));

	u = makeNode(SelectStmt);
	u->op = SETOP_UNION;
	u->all = true;
	u->larg = lsel;
	u->rarg = rsel;

	sub_sel = makeNode(RangeSubselect);
	sub_sel->subquery = (Node *) u;
	sub_sel->alias = makeAliasOptUnique(NULL);

	sel = makeNode(SelectStmt);
	sel->fromClause = list_make1(sub_sel);

	sel->targetList = list_make4(makeSimpleResTarget(AG_START_ID, NULL),
								 makeSimpleResTarget(AG_END_ID, NULL),
								 makeSimpleResTarget(AG_ELEM_LOCAL_ID, NULL),
								 makeSimpleResTarget("ctid", NULL));
	if (row_name != NULL)
	{
		row = makeRowExpr(list_make5(makeColumnRef1(AG_ELEM_LOCAL_ID),
									 makeColumnRef1("_start"),
									 makeColumnRef1("_end"),
									 makeColumnRef1(AG_ELEM_PROP_MAP),
									 makeColumnRef1("ctid")),
						  "edge");
		sel->targetList = lappend(sel->targetList,
								  makeResTarget(row, row_name));
	}

	return (Node *) sel;
}

/*
 * SELECT start, "end", id, (id, start, end, properties)::edge AS row_name
 * FROM `get_graph_path()`.`elabel_name`
 */
static Node *
makeDijkstraEdge(char *elabel_name, char *row_name)
{
	SelectStmt *sel;
	RangeVar   *r;

	sel = makeNode(SelectStmt);

	r = makeRangeVar(get_graph_path(true), elabel_name, -1);
	r->inh = true;
	sel->fromClause = list_make1(r);

	sel->targetList = list_make4(makeSimpleResTarget(AG_START_ID, NULL),
								 makeSimpleResTarget(AG_END_ID, NULL),
								 makeSimpleResTarget(AG_ELEM_LOCAL_ID, NULL),
								 makeSimpleResTarget("ctid", NULL));
	if (row_name != NULL)
	{
		Node	   *row;

		row = makeRowExpr(list_make5(makeColumnRef1(AG_ELEM_LOCAL_ID),
									 makeColumnRef1(AG_START_ID),
									 makeColumnRef1(AG_END_ID),
									 makeColumnRef1(AG_ELEM_PROP_MAP),
									 makeColumnRef1("ctid")),
						  "edge");
		sel->targetList = lappend(sel->targetList,
								  makeResTarget(row, row_name));
	}

	return (Node *) sel;
}