/* -*-pgsql-c-*- */
/*
 * $Header$
 *
 * Copyright (c) 2003-2012	PgPool Global Development Group
 *
 * Permission to use, copy, modify, and distribute this software and
 * its documentation for any purpose and without fee is hereby
 * granted, provided that the above copyright notice appear in all
 * copies and that both that copyright notice and this permission
 * notice appear in supporting documentation, and that the name of the
 * author not be used in advertising or publicity pertaining to
 * distribution of the software without specific, written prior
 * permission. The author makes no representations about the
 * suitability of this software for any purpose.  It is provided "as
 * is" without express or implied warranty.
 *
 * pgpool-regclass.c is similar to PostgreSQL builtin function
 * regclass but does not throw exceptions.
 * If something goes wrong, it returns InvalidOid.
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include "postgres.h"
#include "utils/builtins.h"
#include "utils/syscache.h"
#include "utils/elog.h"
#include "catalog/namespace.h"
#include "nodes/makefuncs.h"
#include "commands/dbcommands.h"
#include "fmgr.h"
#include "funcapi.h"

#include <stdlib.h>

#ifdef PG_MODULE_MAGIC
PG_MODULE_MAGIC;
#endif

PG_FUNCTION_INFO_V1(pgpool_regclass);

extern Datum pgpool_regclass(PG_FUNCTION_ARGS);

static List *MystringToQualifiedNameList(const char *string);

static RangeVar *MymakeRangeVarFromNameList(List *names);

extern Oid	MyDatabaseId;

#if !defined(PG_VERSION_NUM) || (PG_VERSION_NUM < 90100)
static Oid
			get_namespace_oid(const char *nspname, bool missing_ok);
#endif

Datum
pgpool_regclass(PG_FUNCTION_ARGS)
{
	char	   *pro_name_or_oid = PG_GETARG_CSTRING(0);
	Oid			result;
	List	   *names;
	RangeVar   *rel;

	names = MystringToQualifiedNameList(pro_name_or_oid);
	if (names == NIL)
		PG_RETURN_OID(InvalidOid);

	rel = MymakeRangeVarFromNameList(names);
	if (rel == NULL)
		PG_RETURN_OID(InvalidOid);

	/*
	 * Check to see if cross-database reference is used. This is to be done in
	 * RangeVarGetRelid(). The reason we do this here is, to avoid raising an
	 * error in RangeVarGetRelid().
	 */
	if (rel->catalogname)
	{
		if (strcmp(rel->catalogname, get_database_name(MyDatabaseId)) != 0)
			PG_RETURN_OID(InvalidOid);
	}

	/*
	 * Check to see if schema exists. This is to be done in
	 * RangeVarGetRelid(). The reason we do this here is, to avoid raising an
	 * error in RangeVarGetRelid().
	 */
	if (rel->schemaname)
	{
		if (get_namespace_oid(rel->schemaname, true) == InvalidOid)
			PG_RETURN_OID(InvalidOid);
	}
#if !defined(PG_VERSION_NUM) || (PG_VERSION_NUM < 90200)
	result = RangeVarGetRelid(rel, true);
#else

	/*
	 * RangeVarGetRelid() of PostgreSQL 9.2 or later, has third argument
	 * "missing_ok" which suppresses ERROR exception, but returns invalid_oid.
	 * See include/catalog/namespace.h
	 */
	result = RangeVarGetRelid(rel, true, true);
#endif
	PG_RETURN_OID(result);
}

/*
 * Given a C string, parse it into a qualified-name list.
 * Stolen from PostgreSQL, removing elog calls.
 */
static List *
MystringToQualifiedNameList(const char *string)
{
	char	   *rawname;
	List	   *result = NIL;
	List	   *namelist;
	ListCell   *l;

	/* We need a modifiable copy of the input string. */
	rawname = pstrdup(string);

	if (!SplitIdentifierString(rawname, '.', &namelist))
		return NIL;

	if (namelist == NIL)
		return NIL;

	foreach(l, namelist)
	{
		char	   *curname = (char *) lfirst(l);

		result = lappend(result, makeString(pstrdup(curname)));
	}

	pfree(rawname);
	list_free(namelist);

	return result;
}

/*
 * makeRangeVarFromNameList
 *		Utility routine to convert a qualified-name list into RangeVar form.
 *		Stolen from PostgreSQL, removing ereport calls.
 */
static RangeVar *
MymakeRangeVarFromNameList(List *names)
{
/*
 * Number of arguments of makeRangeVar() has been increased in 8.4 or
 * later.
 */
#if PG_VERSION_NUM >= 80400
	RangeVar   *rel = makeRangeVar(NULL, NULL, -1);
#else
	RangeVar   *rel = makeRangeVar(NULL, NULL);
#endif

	switch (list_length(names))
	{
		case 1:
			rel->relname = strVal(linitial(names));
			break;
		case 2:
			rel->schemaname = strVal(linitial(names));
			rel->relname = strVal(lsecond(names));
			break;
		case 3:
			rel->catalogname = strVal(linitial(names));
			rel->schemaname = strVal(lsecond(names));
			rel->relname = strVal(lthird(names));
			break;
		default:
			rel = NULL;
			break;
	}

	return rel;
}

#if !defined(PG_VERSION_NUM) || (PG_VERSION_NUM < 90100)
/*
 * get_namespace_oid - given a namespace name, look up the OID
 *
 * If missing_ok is false, throw an error if namespace name not found.  If
 * true, just return InvalidOid.
 *
 * This function was stolen from PostgreSQL 9.0 and modified to not
 * use GetSysCacheOid1. Since it is new in 9.0.
 */
static Oid
get_namespace_oid(const char *nspname, bool missing_ok)
{
	Oid			oid;

	oid = GetSysCacheOid(NAMESPACENAME, CStringGetDatum(nspname), 0, 0, 0);
	if (!OidIsValid(oid) && !missing_ok)
		ereport(ERROR,
				(errcode(ERRCODE_UNDEFINED_SCHEMA),
				 errmsg("schema \"%s\" does not exist", nspname)));

	return oid;
}
#endif
