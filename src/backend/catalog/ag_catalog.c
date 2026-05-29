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

#include "catalog/dependency.h"
#include "catalog/namespace.h"
#include "catalog/objectaccess.h"
#include "catalog/pg_class_d.h"
#include "catalog/pg_namespace_d.h"
#include "commands/defrem.h"
#include "nodes/parsenodes.h"
#include "tcop/utility.h"
#include "utils/builtins.h"
#include "utils/catcache.h"
#include "utils/hsearch.h"
#include "utils/inval.h"
#include "utils/lsyscache.h"
#include "utils/memutils.h"

#include "catalog/ag_graph.h"
#include "catalog/ag_label.h"
#include "utils/ag_cache.h"
#include "utils/age_global_graph.h"

static object_access_hook_type prev_object_access_hook;
static ProcessUtility_hook_type prev_process_utility_hook;
static bool prev_object_hook_is_set;
static HTAB *ag_relation_id_cache = NULL;
static bool ag_relation_id_callback_registered = false;

typedef struct ag_relation_id_cache_entry
{
    NameData name;
    Oid oid;
} ag_relation_id_cache_entry;

static void object_access(ObjectAccessType access, Oid class_id, Oid object_id,
                          int sub_id, void *arg);
void ag_ProcessUtility_hook(PlannedStmt *pstmt, const char *queryString, bool readOnlyTree,
                            ProcessUtilityContext context, ParamListInfo params,
                            QueryEnvironment *queryEnv, DestReceiver *dest,
                            QueryCompletion *qc);

static bool is_age_drop(PlannedStmt *pstmt);
static void drop_age_extension(DropStmt *stmt);
static void initialize_ag_relation_id_cache(void);
static void invalidate_ag_relation_id_cache(Datum arg, Oid relid);
static int ag_relation_name_hash_compare(const void *key1, const void *key2,
                                         Size keysize);

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
 */
void ag_ProcessUtility_hook(PlannedStmt *pstmt, const char *queryString,
                            bool readOnlyTree, ProcessUtilityContext context,
                            ParamListInfo params, QueryEnvironment *queryEnv,
                            DestReceiver *dest, QueryCompletion *qc)
{
    if (is_age_drop(pstmt))
    {
        drop_age_extension((DropStmt *)pstmt->utilityStmt);
    }
    else
    {
        /*
         * Check for TRUNCATE on graph label tables. If any truncated
         * table is a graph label table, increment the version counter
         * for that graph to invalidate VLE caches. We do this before
         * the truncate executes so the cache is invalidated regardless.
         */
        if (IsA(pstmt->utilityStmt, TruncateStmt))
        {
            TruncateStmt *tstmt = (TruncateStmt *) pstmt->utilityStmt;
            ListCell *lc;

            foreach(lc, tstmt->relations)
            {
                RangeVar *rv = (RangeVar *) lfirst(lc);
                Oid rel_oid = RangeVarGetRelid(rv, AccessShareLock, true);

                if (OidIsValid(rel_oid))
                {
                    Oid graph_oid = get_graph_oid_for_table(rel_oid);

                    if (OidIsValid(graph_oid))
                    {
                        increment_graph_version(graph_oid);
                    }
                }
            }
        }

        if (prev_process_utility_hook)
        {
            (*prev_process_utility_hook) (pstmt, queryString, readOnlyTree,
                                          context, params, queryEnv, dest, qc);
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
}

static void drop_age_extension(DropStmt *stmt)
{
    /* Remove all graphs */
    drop_graphs(get_graphnames());

    /* Remove the object access hook */
    object_access_hook_fini();

    /*
     * Run Postgres' logic to perform the remaining work to drop the
     * extension.
     */
    RemoveObjects(stmt);

    /* reset global variables for OIDs */
    clear_global_Oids_AGTYPE();
    clear_global_Oids_GRAPHID();
}

/* Check to see if the Utility Command is to drop the AGE Extension. */
static bool is_age_drop(PlannedStmt *pstmt)
{
    ListCell *lc;
    DropStmt *drop_stmt;

    if (!IsA(pstmt->utilityStmt, DropStmt))
        return false;

    drop_stmt = (DropStmt *)pstmt->utilityStmt;

    foreach(lc, drop_stmt->objects)
    {
        Node *obj = lfirst(lc);

        if (IsA(obj, String))
        {
            String *val = (String *)obj;
            char *str = val->sval;

            if (!pg_strcasecmp(str, "age"))
                return true;
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

    /* We are interested in DROP SCHEMA and DROP TABLE commands. */
    if (access != OAT_DROP)
        return;

    /*
     * Age might be installed into shared_preload_libraries before extension is
     * created. In this case we must bail out from this hook.
     */
    if (!OidIsValid(get_namespace_oid("ag_catalog", true)))
        return;

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

        cache_data = search_graph_namespace_cache_cached(object_id);
        if (cache_data)
        {
            ereport(ERROR, (errcode(ERRCODE_DEPENDENT_OBJECTS_STILL_EXIST),
                            errmsg("schema \"%s\" is for graph \"%s\"",
                                   NameStr(cache_data->name),
                                   NameStr(cache_data->name))));
        }

        return;
    }

    if (class_id == RelationRelationId)
    {
        label_cache_data *cache_data;

        cache_data = search_label_relation_cache_cached(object_id);

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
            ereport(ERROR, (errcode(ERRCODE_DEPENDENT_OBJECTS_STILL_EXIST),
                            errmsg("table \"%s\" is for label \"%s\"",
                                   get_label_cache_relation_name(cache_data),
                                   NameStr(cache_data->name))));
        }
    }
}

Oid ag_relation_id(const char *name, const char *kind)
{
    Oid id;
    NameData name_key;
    ag_relation_id_cache_entry *entry;
    bool found;

    initialize_ag_relation_id_cache();

    namestrcpy(&name_key, name);
    entry = hash_search(ag_relation_id_cache, &name_key, HASH_FIND, NULL);
    if (entry != NULL)
    {
        return entry->oid;
    }

    id = get_relname_relid(name, ag_catalog_namespace_id());
    if (!OidIsValid(id))
    {
        ereport(ERROR, (errcode(ERRCODE_UNDEFINED_TABLE),
                        errmsg("%s \"%s\" does not exist", kind, name)));
    }

    if (ag_relation_id_cache == NULL)
    {
        initialize_ag_relation_id_cache();
    }

    entry = hash_search(ag_relation_id_cache, &name_key, HASH_ENTER, &found);
    if (found)
    {
        return entry->oid;
    }

    entry->oid = id;

    return id;
}

static void initialize_ag_relation_id_cache(void)
{
    HASHCTL hash_ctl;

    if (ag_relation_id_cache != NULL)
    {
        return;
    }

    if (!CacheMemoryContext)
    {
        CreateCacheMemoryContext();
    }

    MemSet(&hash_ctl, 0, sizeof(hash_ctl));
    hash_ctl.keysize = sizeof(NameData);
    hash_ctl.entrysize = sizeof(ag_relation_id_cache_entry);
    hash_ctl.match = ag_relation_name_hash_compare;
    hash_ctl.hcxt = CacheMemoryContext;

    ag_relation_id_cache = hash_create("ag_catalog relation oid cache", 8,
                                       &hash_ctl,
                                       HASH_ELEM | HASH_BLOBS | HASH_COMPARE |
                                       HASH_CONTEXT);

    if (!ag_relation_id_callback_registered)
    {
        CacheRegisterRelcacheCallback(invalidate_ag_relation_id_cache,
                                      (Datum)0);
        ag_relation_id_callback_registered = true;
    }
}

static void invalidate_ag_relation_id_cache(Datum arg, Oid relid)
{
    HASH_SEQ_STATUS hash_seq;
    ag_relation_id_cache_entry *entry;

    if (ag_relation_id_cache == NULL)
    {
        return;
    }

    if (!OidIsValid(relid))
    {
        hash_destroy(ag_relation_id_cache);
        ag_relation_id_cache = NULL;
        return;
    }

    hash_seq_init(&hash_seq, ag_relation_id_cache);
    while ((entry = hash_seq_search(&hash_seq)) != NULL)
    {
        if (entry->oid == relid)
        {
            void *removed;

            removed = hash_search(ag_relation_id_cache, &entry->name,
                                  HASH_REMOVE, NULL);
            hash_seq_term(&hash_seq);

            if (!removed)
            {
                ereport(ERROR,
                        (errmsg_internal("ag relation oid cache corrupted")));
            }

            break;
        }
    }
}

static int ag_relation_name_hash_compare(const void *key1, const void *key2,
                                         Size keysize)
{
    Name name1 = (Name)key1;
    Name name2 = (Name)key2;

    Assert(keysize == NAMEDATALEN);

    return strncmp(NameStr(*name1), NameStr(*name2), NAMEDATALEN);
}
