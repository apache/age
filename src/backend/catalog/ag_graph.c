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

#include "access/heapam.h"
#include "access/htup_details.h"
#include "access/parallel.h"
#include "access/xact.h"
#include "catalog/ag_graph.h"
#include "catalog/ag_graph_fn.h"
#include "catalog/dependency.h"
#include "catalog/indexing.h"
#include "catalog/pg_namespace.h"
#include "commands/schemacmds.h"
#include "utils/builtins.h"
#include "utils/guc.h"
#include "utils/lsyscache.h"
#include "utils/rel.h"
#include "utils/syscache.h"
#include "catalog/catalog.h"
#include "miscadmin.h"

/* a global variable for the GUC variable */
char	   *graph_path = NULL;
bool		cypher_allow_unsafe_dml = false;
bool		cypher_allow_unsafe_ddl = false;

/* Potentially set by pg_upgrade_support functions */
Oid			binary_upgrade_next_ag_graph_oid = InvalidOid;

/* check_hook: validate new graph_path value */
bool
check_graph_path(char **newval, void **extra, GucSource source)
{
	if (!cypher_allow_unsafe_ddl && IsTransactionState() &&
		!InitializingParallelWorker)
	{
		if (!OidIsValid(get_graphname_oid(*newval)))
		{
			GUC_check_errdetail("graph \"%s\" does not exist.", *newval);
			return false;
		}
	}

	return true;
}

char *
get_graph_path(bool lookup_cache)
{
	if (graph_path == NULL || strlen(graph_path) == 0)
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_SCHEMA_NAME),
				 errmsg("graph_path is NULL"),
				 errhint("Use SET graph_path")));

	if (lookup_cache && !OidIsValid(get_graphname_oid(graph_path)))
		ereport(ERROR,
				(errcode(ERRCODE_UNDEFINED_OBJECT),
				 errmsg("current graph_path \"%s\" is invalid", graph_path),
				 errhint("Use CREATE GRAPH")));

	return graph_path;
}

Oid
get_graph_path_oid(void)
{
	Oid			graphoid;

	if (graph_path == NULL || strlen(graph_path) == 0)
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_SCHEMA_NAME),
				 errmsg("graph_path is NULL"),
				 errhint("Use SET graph_path")));

	graphoid = get_graphname_oid(graph_path);
	if (!OidIsValid(graphoid))
		ereport(ERROR,
				(errcode(ERRCODE_UNDEFINED_OBJECT),
				 errmsg("current graph_path \"%s\" is invalid", graph_path),
				 errhint("Use SET graph_path")));

	return graphoid;
}

/* Create a graph (schema) with the name and owner OID. */
Oid
GraphCreate(CreateGraphStmt *stmt, const char *queryString,
			int stmt_location, int stmt_len)
{
	const char *graphName = stmt->graphname;
	CreateSchemaStmt *schemaStmt;
	Oid			schemaoid;
	Datum		values[Natts_ag_graph];
	bool		isnull[Natts_ag_graph] = {false,};
	NameData	gname;
	Relation	graphdesc;
	TupleDesc	tupDesc;
	HeapTuple	tup;
	Oid			graphoid;
	ObjectAddress graphobj;
	ObjectAddress schemaobj;

	AssertArg(graphName != NULL);

	if (OidIsValid(get_graphname_oid(graphName)))
	{
		if (stmt->if_not_exists)
		{
			ereport(NOTICE,
					(errcode(ERRCODE_DUPLICATE_SCHEMA),
					 errmsg("graph \"%s\" already exists, skipping",
							graphName)));

			return InvalidOid;
		}
		else
		{
			ereport(ERROR,
					(errcode(ERRCODE_DUPLICATE_SCHEMA),
					 errmsg("graph \"%s\" already exists", graphName)));
		}
	}

	/* create a schema as a graph */

	schemaStmt = makeNode(CreateSchemaStmt);
	schemaStmt->schemaname = stmt->graphname;
	schemaStmt->authrole = stmt->authrole;
	schemaStmt->if_not_exists = stmt->if_not_exists;
	schemaStmt->schemaElts = NIL;

	schemaoid = CreateSchemaCommand(schemaStmt, queryString,
									stmt_location, stmt_len);

	namestrcpy(&gname, graphName);

	graphdesc = table_open(GraphRelationId, RowExclusiveLock);
	tupDesc = graphdesc->rd_att;

	if (IsBinaryUpgrade)
	{
		if (!OidIsValid(binary_upgrade_next_ag_graph_oid))
			ereport(ERROR,
					(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
					 errmsg("ag_graph OID value not set when in binary upgrade mode")));
		graphoid = binary_upgrade_next_ag_graph_oid;
		binary_upgrade_next_ag_graph_oid = InvalidOid;
	}
	else
	{
		graphoid = GetNewOidWithIndex(graphdesc,
									  GraphOidIndexId,
									  Anum_ag_graph_oid);
	}

	values[Anum_ag_graph_oid - 1] = graphoid;
	values[Anum_ag_graph_graphname - 1] = NameGetDatum(&gname);
	values[Anum_ag_graph_nspid - 1] = ObjectIdGetDatum(schemaoid);

	tup = heap_form_tuple(tupDesc, values, isnull);

	CatalogTupleInsert(graphdesc, tup);
	Assert(OidIsValid(graphoid));

	table_close(graphdesc, RowExclusiveLock);

	graphobj.classId = GraphRelationId;
	graphobj.objectId = graphoid;
	graphobj.objectSubId = 0;
	schemaobj.classId = NamespaceRelationId;
	schemaobj.objectId = schemaoid;
	schemaobj.objectSubId = 0;

	/*
	 * Register dependency from the schema to the graph, so that the schema
	 * will be deleted if the graph is.
	 */
	recordDependencyOn(&schemaobj, &graphobj, DEPENDENCY_INTERNAL);

	return graphoid;
}
