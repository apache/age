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

#include "executor/executor.h"
#include "storage/bufmgr.h"
#include "common/hashfn.h"
#include "miscadmin.h"
#include "utils/acl.h"
#include "utils/rls.h"

#include "catalog/ag_label.h"
#include "executor/cypher_executor.h"
#include "utils/age_global_graph.h"
#include "utils/ag_cache.h"
#include "executor/cypher_utils.h"

static void begin_cypher_delete(CustomScanState *node, EState *estate,
                                int eflags);
static TupleTableSlot *exec_cypher_delete(CustomScanState *node);
static void end_cypher_delete(CustomScanState *node);
static void rescan_cypher_delete(CustomScanState *node);

static void process_delete_list(CustomScanState *node);

static void check_for_connected_edges(CustomScanState *node);
static void ensure_detach_delete_rls(CustomScanState *node,
                                     ResultRelInfo *resultRelInfo,
                                     Oid relid, bool *rls_checked,
                                     bool *rls_enabled, List **qualExprs,
                                     ExprContext **econtext);
static agtype_value *extract_entity(CustomScanState *node,
                                    TupleTableSlot *scanTupleSlot,
                                    int entity_position);
static void delete_entity(EState *estate, ResultRelInfo *resultRelInfo,
                          HeapTuple tuple);
static void init_delete_caches(cypher_delete_custom_scan_state *css);

const CustomExecMethods cypher_delete_exec_methods = {DELETE_SCAN_STATE_NAME,
                                                      begin_cypher_delete,
                                                      exec_cypher_delete,
                                                      end_cypher_delete,
                                                      rescan_cypher_delete,
                                                      NULL,
                                                      NULL,
                                                      NULL,
                                                      NULL,
                                                      NULL,
                                                      NULL,
                                                      NULL,
                                                      NULL};

/*
 * Initialization at the beginning of execution. Setup the child node,
 * setup its scan tuple slot and projection info, expression context,
 * collect metadata about visible edges, and alter the commandid for
 * the transaction.
 */
static void begin_cypher_delete(CustomScanState *node, EState *estate,
                                int eflags)
{
    cypher_delete_custom_scan_state *css =
        (cypher_delete_custom_scan_state *)node;
    Plan *subplan;
    HASHCTL hashctl;

    Assert(list_length(css->cs->custom_plans) == 1);

    /* setup child */
    subplan = linitial(css->cs->custom_plans);
    node->ss.ps.lefttree = ExecInitNode(subplan, estate, eflags);

    /* setup expr context */
    ExecAssignExprContext(estate, &node->ss.ps);

    /* setup scan tuple slot and projection info */
    ExecInitScanTupleSlot(estate, &node->ss,
                          ExecGetResultType(node->ss.ps.lefttree),
                          &TTSOpsHeapTuple);

    if (!CYPHER_CLAUSE_IS_TERMINAL(css->flags))
    {
        TupleDesc tupdesc = node->ss.ss_ScanTupleSlot->tts_tupleDescriptor;

        ExecAssignProjectionInfo(&node->ss.ps, tupdesc);
    }

    /* init vertex_id_htab */
    MemSet(&hashctl, 0, sizeof(hashctl));
    hashctl.keysize = sizeof(graphid);
    hashctl.entrysize =
        sizeof(graphid); /* entries are not used, but entrysize must >= keysize */
    hashctl.hash = tag_hash;
    css->vertex_id_htab = hash_create(DELETE_VERTEX_HTAB_NAME,
                                      DELETE_VERTEX_HTAB_SIZE, &hashctl,
                                      HASH_ELEM | HASH_FUNCTION);
    init_delete_caches(css);

    /*
     * Postgres does not assign the es_output_cid in queries that do
     * not write to disk, ie: SELECT commands. We need the command id
     * for our clauses, and we may need to initialize it. We cannot use
     * GetCurrentCommandId because there may be other cypher clauses
     * that have modified the command id.
     */
    if (estate->es_output_cid == 0)
        estate->es_output_cid = estate->es_snapshot->curcid;

    Increment_Estate_CommandId(estate);
}

/*
 * Called once per tuple. If this is a terminal DELETE clause
 * process everyone of its child tuple, otherwise process the
 * next tuple.
 */
static TupleTableSlot *exec_cypher_delete(CustomScanState *node)
{
    cypher_delete_custom_scan_state *css =
        (cypher_delete_custom_scan_state *)node;
    EState *estate = css->css.ss.ps.state;
    ExprContext *econtext = css->css.ss.ps.ps_ExprContext;
    TupleTableSlot *slot;

    if (CYPHER_CLAUSE_IS_TERMINAL(css->flags))
    {
        /*
         * If the DELETE clause was the final cypher clause written
         * then we aren't returning anything from this result node.
         * So the exec_cypher_delete function will only be called once.
         * Therefore we will process all tuples from the subtree at once.
         */
        while(true)
        {
            /* Process the subtree first */
            Decrement_Estate_CommandId(estate)
            slot = ExecProcNode(node->ss.ps.lefttree);
            Increment_Estate_CommandId(estate)

            if (TupIsNull(slot))
                break;

            /* setup the scantuple that the process_delete_list needs */
            econtext->ecxt_scantuple =
                node->ss.ps.lefttree->ps_ProjInfo->pi_exprContext->ecxt_scantuple;

            process_delete_list(node);
        }

        return NULL;
    }
    else
    {
        /* Process the subtree first */
        Decrement_Estate_CommandId(estate)
        slot = ExecProcNode(node->ss.ps.lefttree);
        Increment_Estate_CommandId(estate)

        if (TupIsNull(slot))
            return NULL;

        /* setup the scantuple that the process_delete_list needs */
        econtext->ecxt_scantuple =
            node->ss.ps.lefttree->ps_ProjInfo->pi_exprContext->ecxt_scantuple;

        process_delete_list(node);

        econtext->ecxt_scantuple =
            ExecProject(node->ss.ps.lefttree->ps_ProjInfo);

        return ExecProject(node->ss.ps.ps_ProjInfo);
    }
}

/*
 * Called at the end of execution. Tell its child to
 * end its execution.
 */
static void end_cypher_delete(CustomScanState *node)
{
    cypher_delete_custom_scan_state *css =
        (cypher_delete_custom_scan_state *)node;

    check_for_connected_edges(node);

    /* invalidate VLE cache — graph was mutated */
    increment_graph_version(css->delete_data->graph_oid);

    if (css->qual_cache != NULL)
    {
        hash_destroy(css->qual_cache);
        css->qual_cache = NULL;
    }

    if (css->index_cache != NULL)
    {
        hash_destroy(css->index_cache);
        css->index_cache = NULL;
    }

    if (css->result_rel_info_cache != NULL)
    {
        destroy_entity_result_rel_info_cache(css->result_rel_info_cache);
        css->result_rel_info_cache = NULL;
    }

    hash_destroy(((cypher_delete_custom_scan_state *)node)->vertex_id_htab);

    ExecEndNode(node->ss.ps.lefttree);
}

static void init_delete_caches(cypher_delete_custom_scan_state *css)
{
    HASHCTL hashctl;
    HASHCTL idx_hashctl;

    MemSet(&hashctl, 0, sizeof(hashctl));
    hashctl.keysize = sizeof(Oid);
    hashctl.entrysize = sizeof(RLSCacheEntry);
    hashctl.hcxt = CurrentMemoryContext;
    css->qual_cache = hash_create("delete_qual_cache", 8, &hashctl,
                                  HASH_ELEM | HASH_BLOBS | HASH_CONTEXT);

    MemSet(&idx_hashctl, 0, sizeof(idx_hashctl));
    idx_hashctl.keysize = sizeof(Oid);
    idx_hashctl.entrysize = sizeof(IndexCacheEntry);
    idx_hashctl.hcxt = CurrentMemoryContext;
    css->index_cache = hash_create("delete_index_cache", 8, &idx_hashctl,
                                   HASH_ELEM | HASH_BLOBS | HASH_CONTEXT);

    css->result_rel_info_cache =
        create_entity_result_rel_info_cache("delete_result_rel_info_cache");
}

/*
 * Used for rewinding the scan state and reprocessing the results.
 *
 * XXX: This is not currently supported. We need to find out
 * when this function will be called and determine a process
 * for allowing the Delete clause to run multiple times without
 * redundant edits to the database.
 */
static void rescan_cypher_delete(CustomScanState *node)
{
     ereport(ERROR,
             (errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
                      errmsg("cypher DELETE clause cannot be rescanned"),
                      errhint("its unsafe to use joins in a query with a Cypher DELETE clause")));
}

/*
 * Create the CustomScanState from the CustomScan and pass
 * necessary metadata.
 */
Node *create_cypher_delete_plan_state(CustomScan *cscan)
{
    cypher_delete_custom_scan_state *cypher_css =
        palloc0(sizeof(cypher_delete_custom_scan_state));
    cypher_delete_information *delete_data;
    char *serialized_data;
    Const *c;

    cypher_css->cs = cscan;

    /* get the serialized data structure from the Const and deserialize it. */
    c = linitial(cscan->custom_private);
    serialized_data = (char *)c->constvalue;
    delete_data = stringToNode(serialized_data);

    Assert(is_ag_node(delete_data, cypher_delete_information));

    cypher_css->delete_data = delete_data;
    cypher_css->flags = delete_data->flags;

    cypher_css->css.ss.ps.type = T_CustomScanState;
    cypher_css->css.methods = &cypher_delete_exec_methods;

    return (Node *)cypher_css;
}

/*
 * Extract the vertex or edge to be deleted, perform some type checking to
 * validate datum is an agtype vertex or edge.
 */
static agtype_value *extract_entity(CustomScanState *node,
                                    TupleTableSlot *scanTupleSlot,
                                    int entity_position)
{
    agtype_value *original_entity_value;
    agtype *original_entity;
    TupleDesc tupleDescriptor;

    tupleDescriptor = scanTupleSlot->tts_tupleDescriptor;

    /* type checking, make sure the entity is an agtype vertex or edge */
    if (TupleDescAttr(tupleDescriptor, entity_position -1)->atttypid != AGTYPEOID)
        ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
                errmsg("DELETE clause can only delete agtype")));

    original_entity = DATUM_GET_AGTYPE_P(scanTupleSlot->tts_values[entity_position - 1]);
    original_entity_value = get_ith_agtype_value_from_container(&original_entity->root, 0);

    if (original_entity_value->type != AGTV_VERTEX && original_entity_value->type != AGTV_EDGE)
        ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
                errmsg("DELETE clause can only delete vertices and edges")));

    return original_entity_value;
}

/*
 * Try and delete the entity that is describe by the HeapTuple in the table
 * described by the resultRelInfo.
 */
static void delete_entity(EState *estate, ResultRelInfo *resultRelInfo,
                          HeapTuple tuple)
{
    ResultRelInfo **saved_resultRels;
    LockTupleMode lockmode;
    TM_FailureData hufd;
    TM_Result lock_result;
    TM_Result delete_result;
    Buffer buffer;

    /* Find the physical tuple, this variable is coming from */
    saved_resultRels = estate->es_result_relations;
    estate->es_result_relations = &resultRelInfo;

    lockmode = ExecUpdateLockMode(estate, resultRelInfo);

    lock_result = heap_lock_tuple(resultRelInfo->ri_RelationDesc, tuple,
                                  GetCurrentCommandId(false), lockmode,
                                  LockWaitBlock, false, &buffer, &hufd);

    /*
     * It is possible the entity may have already been deleted. If the tuple
     * can be deleted, the lock result will be HeapTupleMayBeUpdated. If the
     * tuple was already deleted by this DELETE clause, the result would be
     * TM_SelfModified, if the result was deleted by a previous delete
     * clause, the result will TM_Invisible. Throw an error if any
     * other result was returned.
     */
    if (lock_result == TM_Ok)
    {
        delete_result = heap_delete(resultRelInfo->ri_RelationDesc,
                                    &tuple->t_self, GetCurrentCommandId(true),
                                    estate->es_crosscheck_snapshot, true, &hufd,
                                    false);

        /*
         * Unlike locking, the heap_delete either succeeded
         * HeapTupleMayBeUpdate, or it failed and returned any other result.
         */
        switch (delete_result)
        {
        case TM_Ok:
            break;
        case TM_SelfModified:
            ereport(
                ERROR,
                (errcode(ERRCODE_INTERNAL_ERROR),
                 errmsg(
                     "deleting the same entity more than once cannot happen")));
            /* ereport never gets here */
            break;
        case TM_Updated:
            ereport(
                ERROR,
                (errcode(ERRCODE_T_R_SERIALIZATION_FAILURE),
                 errmsg("could not serialize access due to concurrent update")));
            /* ereport never gets here */
            break;
        default:
            elog(ERROR, "Entity failed to be update");
            /* elog never gets here */
            break;
        }
        /* increment the command counter */
        CommandCounterIncrement();

        /* Update command id in estate */
        estate->es_snapshot->curcid = GetCurrentCommandId(false);
        estate->es_output_cid = GetCurrentCommandId(false);
    }
    else if (lock_result != TM_Invisible && lock_result != TM_SelfModified)
    {
        ereport(ERROR,
                (errcode(ERRCODE_INTERNAL_ERROR),
                 errmsg("Entity could not be locked for updating")));

    }

    ReleaseBuffer(buffer);

    estate->es_result_relations = saved_resultRels;
}

/*
 * After the delete's subtress has been processed, we then go through the list
 * of variables to be deleted.
 */
static void process_delete_list(CustomScanState *node)
{
    cypher_delete_custom_scan_state *css =
        (cypher_delete_custom_scan_state *)node;
    ListCell *lc;
    ExprContext *econtext = css->css.ss.ps.ps_ExprContext;
    TupleTableSlot *scanTupleSlot = econtext->ecxt_scantuple;
    EState *estate = node->ss.ps.state;
    HTAB *qual_cache = css->qual_cache;
    HTAB *index_cache = css->index_cache;

    foreach(lc, css->delete_data->delete_items)
    {
        cypher_delete_item *item;
        agtype_value *original_entity_value, *id;
        ScanKeyData scan_keys[1];
        TableScanDesc scan_desc = NULL;
        ResultRelInfo *resultRelInfo;
        HeapTuple heap_tuple = NULL;
        label_cache_data *label_cache;
        Integer *pos;
        int entity_position;
        Oid relid;
        Relation rel;
        int id_attr_num;
        Oid index_oid = InvalidOid;
        TupleTableSlot *slot = NULL;
        Relation index_rel = NULL;
        IndexScanDesc index_scan_desc = NULL;
        bool shouldFree = false;
        IndexCacheEntry *idx_entry;
        bool found_idx_entry;
        RLSCacheEntry *rls_entry;
        bool found_rls_entry;

        item = lfirst(lc);

        pos = item->entity_position;
        entity_position = pos->ival;

        /* skip if the entity is null */
        if (scanTupleSlot->tts_isnull[entity_position - 1])
            continue;

        original_entity_value = extract_entity(node, scanTupleSlot,
                                               entity_position);

        id = GET_AGTYPE_VALUE_OBJECT_VALUE(original_entity_value, "id");
        label_cache = search_label_graph_oid_cache_cached(
            css->delete_data->graph_oid, GET_LABEL_ID(id->val.int_value));
        if (label_cache == NULL)
        {
            ereport(ERROR,
                    (errcode(ERRCODE_UNDEFINED_TABLE),
                     errmsg("label id %lu does not exist",
                            GET_LABEL_ID(id->val.int_value))));
        }

        resultRelInfo = get_entity_result_rel_info(estate,
                                                   css->result_rel_info_cache,
                                                   label_cache->relation);
        rel = resultRelInfo->ri_RelationDesc;
        relid = RelationGetRelid(rel);

        /*
         * Setup the scan key to require the id field on-disc to match the
         * entity's graphid.
         */
        if (original_entity_value->type == AGTV_VERTEX)
        {
            id_attr_num = Anum_ag_label_vertex_table_id;
            ScanKeyInit(&scan_keys[0], Anum_ag_label_vertex_table_id,
                        BTEqualStrategyNumber, F_GRAPHIDEQ,
                        GRAPHID_GET_DATUM(id->val.int_value));
        }
        else if (original_entity_value->type == AGTV_EDGE)
        {
            id_attr_num = Anum_ag_label_edge_table_id;
            ScanKeyInit(&scan_keys[0], Anum_ag_label_edge_table_id,
                        BTEqualStrategyNumber, F_GRAPHIDEQ,
                        GRAPHID_GET_DATUM(id->val.int_value));
        }
        else
        {
            ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
                    errmsg("DELETE clause can only delete vertices and edges")));
        }

        idx_entry = hash_search(index_cache, &relid, HASH_ENTER,
                                &found_idx_entry);
        if (!found_idx_entry)
        {
            init_index_cache_entry(idx_entry);
        }

        if (!idx_entry->index_oid_cached)
        {
            idx_entry->index_oid = find_usable_btree_index_for_attr(rel, id_attr_num);
            idx_entry->index_oid_cached = true;
        }

        index_oid = idx_entry->index_oid;

        rls_entry = hash_search(qual_cache, &relid, HASH_ENTER,
                                &found_rls_entry);
        if (!found_rls_entry)
        {
            rls_entry->rls_enabled =
                check_enable_rls(relid, InvalidOid, true) == RLS_ENABLED;

            if (rls_entry->rls_enabled)
            {
                rls_entry->qualExprs = setup_security_quals(resultRelInfo,
                                                            estate, node,
                                                            CMD_DELETE);
                rls_entry->slot = ExecInitExtraTupleSlot(
                    estate, RelationGetDescr(rel), &TTSOpsHeapTuple);
            }
        }

        /*
         * Setup the scan description, with the correct snapshot and scan keys.
         */
        estate->es_snapshot->curcid = GetCurrentCommandId(false);
        estate->es_output_cid = GetCurrentCommandId(false);

        if (OidIsValid(index_oid))
        {
            slot = table_slot_create(rel, NULL);

            index_rel = index_open(index_oid, RowExclusiveLock);
            index_scan_desc = index_beginscan(rel, index_rel, estate->es_snapshot, NULL, 1, 0);
            index_rescan(index_scan_desc, scan_keys, 1, NULL, 0);

            if (index_getnext_slot(index_scan_desc, ForwardScanDirection, slot))
            {
                heap_tuple = ExecFetchSlotHeapTuple(slot, true, &shouldFree);
            }
        }
        else
        {
            scan_desc = table_beginscan(rel, estate->es_snapshot, 1, scan_keys);
            /* Retrieve the tuple. */
            heap_tuple = heap_getnext(scan_desc, ForwardScanDirection);
        }

        if (HeapTupleIsValid(heap_tuple))
        {
            bool passed_rls = true;

            /* Check RLS security quals (USING policy) before delete */
            if (rls_entry->rls_enabled)
            {
                ExecStoreHeapTuple(heap_tuple, rls_entry->slot, false);

                if (!check_security_quals(rls_entry->qualExprs,
                                          rls_entry->slot, econtext))
                {
                    passed_rls = false;
                }
            }

            if (passed_rls)
            {
                /*
                 * For vertices, we insert the vertex ID in the hashtable
                 * vertex_id_htab. This hashtable is used later to process
                 * connected edges.
                 */
                if (original_entity_value->type == AGTV_VERTEX)
                {
                    bool found;
                    hash_search(css->vertex_id_htab, (void *)&(id->val.int_value),
                                HASH_ENTER, &found);
                }

                /* At this point, we are ready to delete the node/vertex. */
                delete_entity(estate, resultRelInfo, heap_tuple);
            }

            if (shouldFree)
            {
                heap_freetuple(heap_tuple);
            }
        }

        if (OidIsValid(index_oid))
        {
            ExecDropSingleTupleTableSlot(slot);
            index_endscan(index_scan_desc);
            index_close(index_rel, RowExclusiveLock);
        }
        else
        {
            table_endscan(scan_desc);
        }

    }

}

/*
 * Helper function to scan an edge table using a specific index (start_id or end_id)
 * and delete the connected edges if the vertex is being deleted.
 */
static void process_edges_by_index(Oid index_oid,
                                   Relation rel,
                                   EState *estate,
                                   cypher_delete_custom_scan_state *css,
                                   ResultRelInfo *resultRelInfo,
                                   Oid relid,
                                   char *label_name,
                                   bool *rls_checked,
                                   bool *rls_enabled,
                                   List **qualExprs,
                                   ExprContext **econtext,
                                   bool is_pass_two,
                                   bool *delete_acl_checked)
{
    HASH_SEQ_STATUS hash_status;
    graphid *vid;
    Relation index_rel;
    IndexScanDesc scan;
    ScanKeyData key;
    TupleTableSlot *slot;

    slot = table_slot_create(rel, NULL);

    index_rel = index_open(index_oid, RowExclusiveLock);
    scan = index_beginscan(rel, index_rel, estate->es_snapshot, NULL, 1, 0);

    /* Initialize ScanKey with a dummy argument (0), updated in the loop */
    ScanKeyInit(&key, 1, BTEqualStrategyNumber, F_GRAPHIDEQ, 0);
    hash_seq_init(&hash_status, css->vertex_id_htab);         

    while ((vid = (graphid *) hash_seq_search(&hash_status)) != NULL)
    {
        /* Update search key with the current ID of the vertex being deleted */
        key.sk_argument = GRAPHID_GET_DATUM(*vid);
        index_rescan(scan, &key, 1, NULL, 0);

        while (index_getnext_slot(scan, ForwardScanDirection, slot))
        {
            if (is_pass_two)
            {
                bool is_null;
                graphid startid;
                bool found_startid = false;

                startid = GRAPHID_GET_DATUM(slot_getattr(slot, Anum_ag_label_edge_table_start_id, &is_null));

                hash_search(css->vertex_id_htab, (void *)&startid, HASH_FIND, &found_startid);

                if (found_startid)
                {
                    ExecClearTuple(slot);
                    continue;
                }
            }

            /* If edge found - delete it (or error if not DETACH) */
            if (css->delete_data->detach)
            {
                bool shouldFree;
                HeapTuple tuple;

                if (!*delete_acl_checked)
                {
                    AclResult aclresult;

                    aclresult = pg_class_aclcheck(relid, GetUserId(),
                                                  ACL_DELETE);
                    if (aclresult != ACLCHECK_OK)
                    {
                        aclcheck_error(aclresult, OBJECT_TABLE, label_name);
                    }

                    *delete_acl_checked = true;
                }

                ensure_detach_delete_rls(&css->css, resultRelInfo, relid,
                                         rls_checked, rls_enabled, qualExprs,
                                         econtext);

                /* Check RLS security quals (USING policy) before delete */
                if (*rls_enabled)
                {
                    if (!check_security_quals(*qualExprs, slot, *econtext))
                    {
                        ereport(ERROR,
                                (errcode(ERRCODE_INSUFFICIENT_PRIVILEGE),
                                    errmsg("cannot delete edge due to row-level security policy on \"%s\"",
                                        label_name),
                                    errhint("DETACH DELETE requires permission to delete all connected edges.")));
                    }
                }
                
                tuple = ExecFetchSlotHeapTuple(slot, true, &shouldFree);
                delete_entity(estate, resultRelInfo, tuple);
                
                if (shouldFree) 
                {
                    heap_freetuple(tuple);
                }
            }
            else
            {
                ereport(ERROR,
                    (errcode(ERRCODE_INTERNAL_ERROR),
                        errmsg("Cannot delete a vertex that has edge(s). "
                            "Delete the edge(s) first, or try DETACH DELETE.")));
            }
            ExecClearTuple(slot);
        }
    }
    index_endscan(scan);
    index_close(index_rel, RowExclusiveLock);
    ExecDropSingleTupleTableSlot(slot);
}

static void ensure_detach_delete_rls(CustomScanState *node,
                                     ResultRelInfo *resultRelInfo,
                                     Oid relid, bool *rls_checked,
                                     bool *rls_enabled, List **qualExprs,
                                     ExprContext **econtext)
{
    if (*rls_checked)
    {
        return;
    }

    *rls_checked = true;

    if (check_enable_rls(relid, InvalidOid, true) == RLS_ENABLED)
    {
        *rls_enabled = true;
        *econtext = node->ss.ps.ps_ExprContext;
        *qualExprs = setup_security_quals(resultRelInfo, node->ss.ps.state,
                                          node, CMD_DELETE);
    }
}

/*
 * Scans the edge tables and checks if the deleted vertices are connected to
 * any edge(s). For DETACH DELETE, the connected edges are deleted. Otherwise,
 * an error is thrown.
 */
static void check_for_connected_edges(CustomScanState *node)
{
    ListCell *lc;
    cypher_delete_custom_scan_state *css =
        (cypher_delete_custom_scan_state *)node;
    EState *estate = css->css.ss.ps.state;

    if (hash_get_num_entries(css->vertex_id_htab) == 0)
    {
        return;
    }

    /*
     * Edge labels are only needed after at least one vertex was deleted.
     * Edge-only DELETEs and empty inputs can skip the catalog lookup entirely.
     */
    if (css->edge_labels == NIL)
    {
        css->edge_labels = get_all_edge_labels_per_graph(estate->es_snapshot,
                                                         css->delete_data->graph_oid);
    }

    /* scans each label from css->edge_labels */
    foreach (lc, css->edge_labels)
    {
        ag_label_info *label_info = lfirst(lc);
        char *label_name = label_info->name;
        ResultRelInfo *resultRelInfo;
        TableScanDesc scan_desc;
        HeapTuple tuple;
        TupleTableSlot *slot;
        Oid relid;
        bool rls_checked = false;
        bool rls_enabled = false;
        bool delete_acl_checked = false;
        List *qualExprs = NIL;
        ExprContext *econtext = NULL;
        Oid start_index_oid;
        Oid end_index_oid;
        Relation rel;
        IndexCacheEntry *idx_entry;
        bool found_idx_entry;

        resultRelInfo = get_entity_result_rel_info(estate,
                                                   css->result_rel_info_cache,
                                                   label_info->relation);
        rel = resultRelInfo->ri_RelationDesc;
        relid = RelationGetRelid(rel);
        estate->es_snapshot->curcid = GetCurrentCommandId(false);
        estate->es_output_cid = GetCurrentCommandId(false);

        idx_entry = hash_search(css->index_cache, &relid, HASH_ENTER,
                                &found_idx_entry);
        if (!found_idx_entry)
        {
            init_index_cache_entry(idx_entry);
        }
        if (!idx_entry->start_index_oid_cached)
        {
            idx_entry->start_index_oid = find_usable_btree_index_for_attr(
                rel, Anum_ag_label_edge_table_start_id);
            idx_entry->start_index_oid_cached = true;
        }
        if (!idx_entry->end_index_oid_cached)
        {
            idx_entry->end_index_oid = find_usable_btree_index_for_attr(
                rel, Anum_ag_label_edge_table_end_id);
            idx_entry->end_index_oid_cached = true;
        }

        start_index_oid = idx_entry->start_index_oid;
        end_index_oid = idx_entry->end_index_oid;

        if (OidIsValid(start_index_oid) && OidIsValid(end_index_oid))
        {
            /* PASS 1: Find edges where the deleted vertex is the START_ID. */
            process_edges_by_index(start_index_oid, rel, estate, css, resultRelInfo,
                                   relid, label_name, &rls_checked,
                                   &rls_enabled, &qualExprs, &econtext, false,
                                   &delete_acl_checked);
               
            /* PASS 2: Find edges where the deleted vertex is the END_ID. */
            process_edges_by_index(end_index_oid, rel, estate, css, resultRelInfo,
                                   relid, label_name, &rls_checked,
                                   &rls_enabled, &qualExprs, &econtext, true,
                                   &delete_acl_checked);
        }
        else
        {
            scan_desc = table_beginscan(rel, estate->es_snapshot, 0, NULL);
            slot = ExecInitExtraTupleSlot(
                estate, RelationGetDescr(rel),
                &TTSOpsHeapTuple);

            /* for each row */
            while (true)
            {
                graphid startid;
                graphid endid;
                bool isNull;
                bool found_startid = false;
                bool found_endid = false;

                tuple = heap_getnext(scan_desc, ForwardScanDirection);

                /* no more tuples to process, break and scan the next label. */
                if (!HeapTupleIsValid(tuple))
                {
                    break;
                }

                ExecStoreHeapTuple(tuple, slot, false);

                startid = GRAPHID_GET_DATUM(slot_getattr(
                    slot, Anum_ag_label_edge_table_start_id, &isNull));
                endid = GRAPHID_GET_DATUM(
                    slot_getattr(slot, Anum_ag_label_edge_table_end_id, &isNull));

                hash_search(css->vertex_id_htab, (void *)&startid, HASH_FIND,
                            &found_startid);

                if (!found_startid)
                {
                    hash_search(css->vertex_id_htab, (void *)&endid, HASH_FIND,
                                &found_endid);
                }

                if (found_startid || found_endid)
                {
                    if (css->delete_data->detach)
                    {
                        if (!delete_acl_checked)
                        {
                            AclResult aclresult;

                            aclresult = pg_class_aclcheck(relid, GetUserId(),
                                                          ACL_DELETE);
                            if (aclresult != ACLCHECK_OK)
                            {
                                aclcheck_error(aclresult, OBJECT_TABLE,
                                               label_name);
                            }

                            delete_acl_checked = true;
                        }

                        ensure_detach_delete_rls(node, resultRelInfo, relid,
                                                 &rls_checked, &rls_enabled,
                                                 &qualExprs, &econtext);

                        /* Check RLS security quals (USING policy) before delete */
                        if (rls_enabled)
                        {
                            /*
                             * For DETACH DELETE, error out if edge RLS check fails.
                             * Unlike normal DELETE which silently skips, we cannot
                             * silently skip edges here as it would leave dangling
                             * edges pointing to deleted vertices.
                             */
                            if (!check_security_quals(qualExprs, slot, econtext))
                            {
                                ereport(ERROR,
                                        (errcode(ERRCODE_INSUFFICIENT_PRIVILEGE),
                                         errmsg("cannot delete edge due to row-level security policy on \"%s\"",
                                                label_name),
                                         errhint("DETACH DELETE requires permission to delete all connected edges.")));
                            }
                        }

                        delete_entity(estate, resultRelInfo, tuple);
                    }
                    else
                    {
                        ereport(
                            ERROR,
                            (errcode(ERRCODE_INTERNAL_ERROR),
                             errmsg(
                                 "Cannot delete a vertex that has edge(s). "
                                 "Delete the edge(s) first, or try DETACH DELETE.")));
                    }
                }
            }

            table_endscan(scan_desc);
        }

    }
}
