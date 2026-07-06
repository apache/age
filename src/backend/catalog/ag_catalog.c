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

#include "access/xact.h"
#include "catalog/dependency.h"
#include "catalog/namespace.h"
#include "catalog/objectaccess.h"
#include "catalog/pg_class_d.h"
#include "catalog/pg_extension_d.h"
#include "catalog/pg_namespace_d.h"
#include "commands/defrem.h"
#include "commands/extension.h"
#include "nodes/parsenodes.h"
#include "tcop/utility.h"
#include "utils/inval.h"
#include "utils/lsyscache.h"

#include "catalog/ag_graph.h"
#include "catalog/ag_label.h"
#include "utils/ag_cache.h"
#include "utils/age_global_graph.h"

static bool extension_cache_is_valid = false;
static bool age_extension_exists = false;
static object_access_hook_type prev_object_access_hook;
static ProcessUtility_hook_type prev_process_utility_hook;
static bool prev_object_hook_is_set;

static void object_access(ObjectAccessType access, Oid class_id, Oid object_id,
                          int sub_id, void *arg);
void ag_ProcessUtility_hook(PlannedStmt *pstmt, const char *queryString, bool readOnlyTree,
                            ProcessUtilityContext context, ParamListInfo params,
                            QueryEnvironment *queryEnv, DestReceiver *dest,
                            QueryCompletion *qc);

static bool is_age_drop(DropStmt *drop_stmt);

static void
invalidate_extension_cache_callback(Datum argument, Oid relationId)
{
    if (!OidIsValid(relationId) || relationId == ExtensionRelationId)
    {
        extension_cache_is_valid = false;
    }
}

/*
 * We don't want most of hooks to do anything if the "age" extension isn't
 * created. However, scanning pg_extension is a costly operation, therefore we
 * implement a caching mechanism and reset it with the help of the relcache
 * callback mechanism.
 *
 * Please also see ag_ProcessUtility_hook() function for more details.
 */
bool
is_age_extension_exists(void)
{
    static bool callback_registered = false;

    if (extension_cache_is_valid)
        return age_extension_exists;

    if (!callback_registered)
    {
        CacheRegisterRelcacheCallback(invalidate_extension_cache_callback,
                                      (Datum) 0);
        callback_registered = true;
    }

    age_extension_exists = OidIsValid(get_extension_oid("age", true));

    extension_cache_is_valid = true;

    return age_extension_exists;
}

void object_access_hook_init(void)
{
    prev_object_access_hook = object_access_hook;
    object_access_hook = object_access;
    prev_object_hook_is_set = true;
}

void object_access_hook_fini(void)
{
    if (prev_object_hook_is_set)
    {
        object_access_hook = prev_object_access_hook;
        prev_object_access_hook = NULL;
        prev_object_hook_is_set = false;
    }

}

void process_utility_hook_init(void)
{
    prev_process_utility_hook = ProcessUtility_hook;
    ProcessUtility_hook = ag_ProcessUtility_hook;
}

void process_utility_hook_fini(void)
{
    ProcessUtility_hook = prev_process_utility_hook;
}

/*
 * When Postgres tries to drop AGE using the standard logic, two issues occur:
 *
 * 1. The schema that graphs in stored in are not dropped.
 *
 * 2. While dropping ag_catalog, the object hook is run. Which uses the
 * information in the indexes and tables being dropped. To prevent an error
 * from being thrown, we need to disable the object_access_hook before dropping
 * the extension.
 *
 * Besides that, we want to notify other backends about the fact that "age"
 * extension was probably created/dropped so that they can enable/disable
 * hooks.
 */
void ag_ProcessUtility_hook(PlannedStmt *pstmt, const char *queryString,
                            bool readOnlyTree, ProcessUtilityContext context,
                            ParamListInfo params, QueryEnvironment *queryEnv,
                            DestReceiver *dest, QueryCompletion *qc)
{
    bool creating_age = false;
    bool dropping_age = false;

    if (!IsAbortedTransactionBlockState())
    {
        Node *parsetree = pstmt->utilityStmt;

        switch (nodeTag(parsetree))
        {
            case T_CreateExtensionStmt:
                {
                    CreateExtensionStmt *stmt =
                        (CreateExtensionStmt *) parsetree;
                    creating_age = strcmp(stmt->extname, "age") == 0;
                }
                break;
            case T_DropStmt:
                {
                    DropStmt *stmt = (DropStmt *) parsetree;

                    if (stmt->removeType != OBJECT_EXTENSION)
                        break;

                    if (!is_age_drop(stmt))
                        break;

                    dropping_age = true;
                }
                break;
            case T_TruncateStmt:
                {
                    /*
                     * Check for TRUNCATE on graph label tables. If any
                     * truncated table is a graph label table, increment the
                     * version counter for that graph to invalidate VLE caches.
                     * We do this before the truncate executes so the cache is
                     * invalidated regardless.
                     */
                    TruncateStmt *tstmt = (TruncateStmt *) parsetree;
                    ListCell *lc;

                    foreach(lc, tstmt->relations)
                    {
                        RangeVar *rv = (RangeVar *) lfirst(lc);
                        Oid rel_oid = RangeVarGetRelid(rv, AccessShareLock,
                                                       true);

                        if (OidIsValid(rel_oid))
                        {
                            Oid graph_oid =
                                get_graph_oid_for_table(rel_oid);

                            if (OidIsValid(graph_oid))
                            {
                                increment_graph_version(graph_oid);
                            }
                        }
                    }
                }
                break;
            default:
                break;
        }
    }

    if (dropping_age)
    {
        /* Remove all graphs */
        drop_graphs(get_graphnames());

        /* Remove the object access hook */
        object_access_hook_fini();
    }

    PG_TRY();
    {
        if (prev_process_utility_hook)
        {
            (*prev_process_utility_hook) (pstmt, queryString, readOnlyTree,
                                          context, params, queryEnv, dest,
                                          qc);
        }
        else
        {
            Assert(IsA(pstmt, PlannedStmt));
            Assert(pstmt->commandType == CMD_UTILITY);
            Assert(queryString != NULL);
            Assert(qc == NULL || qc->commandTag == CMDTAG_UNKNOWN);
            standard_ProcessUtility(pstmt, queryString, readOnlyTree, context,
                                    params, queryEnv, dest, qc);
        }
    }
    PG_CATCH();
    {
        if (dropping_age)
        {
            /*
             * We have to restore the disabled object_access_hook if
             * DROP EXTENSION age failed.
             */
            object_access_hook_init();
        }
        PG_RE_THROW();
    }
    PG_END_TRY();

    if (dropping_age)
    {
        /* reset global variables for OIDs */
        clear_global_Oids_AGTYPE();
        clear_global_Oids_GRAPHID();
        clear_global_Oids_VERTEX_EDGE();

        /* Restore the object access hook */
        object_access_hook_init();
    }

    if (creating_age || dropping_age)
    {
        /* Notify all backends that pg_extension was modified. */
        CacheInvalidateRelcacheByRelid(ExtensionRelationId);
    }
}

/* Check to see if the Utility Command is to drop the AGE Extension. */
static bool is_age_drop(DropStmt *drop_stmt)
{
    ListCell *lc;

    if (!is_age_extension_exists())
    {
        return false;
    }

    foreach(lc, drop_stmt->objects)
    {
        Node *obj = lfirst(lc);

        if (IsA(obj, String))
        {
            String *val = (String *)obj;
            char *str = val->sval;

            if (strcmp(str, "age") == 0)
            {
                return true;
            }
        }
    }

    return false;
}

/*
 * object_access_hook is called before actual deletion. So, looking up ag_cache
 * is still valid at this point. For labels, once a backed table is deleted,
 * its corresponding ag_label cache entry will be removed by cache
 * invalidation.
 */
static void object_access(ObjectAccessType access, Oid class_id, Oid object_id,
                          int sub_id, void *arg)
{
    ObjectAccessDrop *drop_arg;

    if (prev_object_access_hook)
        prev_object_access_hook(access, class_id, object_id, sub_id, arg);

    if (!is_age_extension_exists())
    {
        return;
    }

    /* We are interested in DROP SCHEMA and DROP TABLE commands. */
    if (access != OAT_DROP)
    {
        return;
    }

    drop_arg = arg;

    /*
     * PERFORM_DELETION_INTERNAL flag will be set when remove_schema() calls
     * performDeletion(). However, if PostgreSQL does performDeletion() with
     * PERFORM_DELETION_INTERNAL flag over backed schemas of graphs due to
     * side effects of other commands run by user, it is impossible to
     * distinguish between this and drop_graph().
     *
     * The above applies to DROP TABLE command too.
     */

    if (class_id == NamespaceRelationId)
    {
        graph_cache_data *cache_data;

        if (drop_arg->dropflags & PERFORM_DELETION_INTERNAL)
            return;

        cache_data = search_graph_namespace_cache(object_id);
        if (cache_data)
        {
            char *nspname = get_namespace_name(object_id);

            ereport(ERROR, (errcode(ERRCODE_DEPENDENT_OBJECTS_STILL_EXIST),
                            errmsg("schema \"%s\" is for graph \"%s\"",
                                   nspname, NameStr(cache_data->name))));
        }

        return;
    }

    if (class_id == RelationRelationId)
    {
        label_cache_data *cache_data;

        cache_data = search_label_relation_cache(object_id);

        /* We are interested in only tables that are labels. */
        if (!cache_data)
            return;

        if (drop_arg->dropflags & PERFORM_DELETION_INTERNAL)
        {
            /*
             * Remove the corresponding ag_label entry here first. We don't
             * know whether this operation is drop_label() or a part of
             * drop_graph().
             */
            delete_label(object_id);
        }
        else
        {
            char *relname = get_rel_name(object_id);

            ereport(ERROR, (errcode(ERRCODE_DEPENDENT_OBJECTS_STILL_EXIST),
                            errmsg("table \"%s\" is for label \"%s\"",
                                   relname, NameStr(cache_data->name))));
        }
    }
}

Oid ag_relation_id(const char *name, const char *kind)
{
    Oid id;

    id = get_relname_relid(name, ag_catalog_namespace_id());
    if (!OidIsValid(id))
    {
        ereport(ERROR, (errcode(ERRCODE_UNDEFINED_TABLE),
                        errmsg("%s \"%s\" does not exist", kind, name)));
    }

    return id;
}
