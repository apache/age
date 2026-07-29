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

#include "common/hashfn.h"
#include "executor/executor.h"
#include "executor/nodeModifyTable.h"
#include "storage/bufmgr.h"
#include "utils/rls.h"

#include "catalog/ag_graph.h"
#include "catalog/ag_label.h"
#include "executor/cypher_executor.h"
#include "executor/cypher_utils.h"
#include "utils/age_global_graph.h"
#include "utils/agtype.h"

static void begin_cypher_set(CustomScanState *node, EState *estate,
                                int eflags);
static TupleTableSlot *exec_cypher_set(CustomScanState *node);
static void end_cypher_set(CustomScanState *node);
static void rescan_cypher_set(CustomScanState *node);

static void process_update_list(CustomScanState *node);
static HeapTuple update_entity_tuple(ResultRelInfo *resultRelInfo,
                                     TupleTableSlot *elemTupleSlot,
                                     EState *estate, HeapTuple old_tuple);

const CustomExecMethods cypher_set_exec_methods = {SET_SCAN_STATE_NAME,
                                                      begin_cypher_set,
                                                      exec_cypher_set,
                                                      end_cypher_set,
                                                      rescan_cypher_set,
                                                      NULL,
                                                      NULL,
                                                      NULL,
                                                      NULL,
                                                      NULL,
                                                      NULL,
                                                      NULL,
                                                      NULL};

static void begin_cypher_set(CustomScanState *node, EState *estate,
                             int eflags)
{
    cypher_set_custom_scan_state *css =
        (cypher_set_custom_scan_state *)node;
    Plan *subplan;

    Assert(list_length(css->cs->custom_plans) == 1);

    subplan = linitial(css->cs->custom_plans);
    node->ss.ps.lefttree = ExecInitNode(subplan, estate, eflags);

    ExecAssignExprContext(estate, &node->ss.ps);

    ExecInitScanTupleSlot(estate, &node->ss,
                          ExecGetResultType(node->ss.ps.lefttree),
                          &TTSOpsHeapTuple);

    if (!CYPHER_CLAUSE_IS_TERMINAL(css->flags))
    {
        TupleDesc tupdesc = node->ss.ss_ScanTupleSlot->tts_tupleDescriptor;

        ExecAssignProjectionInfo(&node->ss.ps, tupdesc);
    }

    /*
     * Postgres does not assign the es_output_cid in queries that do
     * not write to disk, ie: SELECT commands. We need the command id
     * for our clauses, and we may need to initialize it. We cannot use
     * GetCurrentCommandId because there may be other cypher clauses
     * that have modified the command id.
     */
    if (estate->es_output_cid == 0)
    {
        estate->es_output_cid = estate->es_snapshot->curcid;
    }

    /* Build the per-statement index and fetch-slot lookup cache. */
    css->entity_lookup_cache = create_entity_lookup_cache();

    Increment_Estate_CommandId(estate);
}

static HeapTuple update_entity_tuple(ResultRelInfo *resultRelInfo,
                                     TupleTableSlot *elemTupleSlot,
                                     EState *estate, HeapTuple old_tuple)
{
    HeapTuple tuple = NULL;
    LockTupleMode lockmode;
    TM_FailureData hufd;
    TM_Result lock_result;
    Buffer buffer;
    TU_UpdateIndexes update_indexes;
    TM_Result   result;
    CommandId cid = GetCurrentCommandId(true);
    ResultRelInfo **saved_resultRels = estate->es_result_relations;
    bool close_indices = false;

    estate->es_result_relations = &resultRelInfo;

    lockmode = ExecUpdateLockMode(estate, resultRelInfo);

    lock_result = heap_lock_tuple(resultRelInfo->ri_RelationDesc, old_tuple,
                                  GetCurrentCommandId(false), lockmode,
                                  LockWaitBlock, false, &buffer, &hufd);

    if (lock_result == TM_Ok)
    {
        /*
         * Open indices if not already open. The resultRelInfo may already
         * have indices opened by the caller (e.g., create_entity_result_rel_info),
         * so only open if needed and track that we did so for cleanup.
         */
        if (resultRelInfo->ri_IndexRelationDescs == NULL)
        {
            ExecOpenIndices(resultRelInfo, false);
            close_indices = true;
        }
        ExecStoreVirtualTuple(elemTupleSlot);

        /*
         * Recompute any stored generated columns before materializing the heap
         * tuple. The slot's tuple descriptor is the full relation descriptor,
         * which may include a GENERATED ALWAYS ... STORED column that the SET
         * path does not populate; leaving those slot entries uninitialized makes
         * heap_form_tuple() segfault on the garbage values (issue #2450).
         */
        if (resultRelInfo->ri_RelationDesc->rd_att->constr != NULL &&
            resultRelInfo->ri_RelationDesc->rd_att->constr->has_generated_stored)
        {
            /*
             * A generation expression may reference the tableoid system column,
             * so the slot must carry the relation's OID before we recompute the
             * stored generated columns (mirrors PostgreSQL's own ExecUpdate path).
             */
            elemTupleSlot->tts_tableOid =
                RelationGetRelid(resultRelInfo->ri_RelationDesc);
            ExecComputeStoredGenerated(resultRelInfo, estate, elemTupleSlot,
                                       CMD_UPDATE);
        }

        tuple = ExecFetchSlotHeapTuple(elemTupleSlot, true, NULL);
        tuple->t_self = old_tuple->t_self;

        /* Check the constraints of the tuple */
        tuple->t_tableOid = RelationGetRelid(resultRelInfo->ri_RelationDesc);
        if (resultRelInfo->ri_RelationDesc->rd_att->constr != NULL)
        {
            ExecConstraints(resultRelInfo, elemTupleSlot, estate);
        }

        /* Check RLS WITH CHECK policies if configured */
        if (resultRelInfo->ri_WithCheckOptions != NIL)
        {
            ExecWithCheckOptions(WCO_RLS_UPDATE_CHECK, resultRelInfo,
                                 elemTupleSlot, estate);
        }

        result = table_tuple_update(resultRelInfo->ri_RelationDesc,
                                    &tuple->t_self, elemTupleSlot,
                                    cid, estate->es_snapshot,
                                    estate->es_crosscheck_snapshot,
                                    true /* wait for commit */ ,
                                    &hufd, &lockmode, &update_indexes);

        if (result == TM_SelfModified)
        {
            if (hufd.cmax != cid)
            {
                ereport(ERROR,
                        (errcode(ERRCODE_TRIGGERED_DATA_CHANGE_VIOLATION),
                         errmsg("tuple to be updated was already modified")));
            }

            if (close_indices)
            {
                ExecCloseIndices(resultRelInfo);
            }
            estate->es_result_relations = saved_resultRels;

            return tuple;
        }

        if (result != TM_Ok)
        {
            ereport(ERROR, (errcode(ERRCODE_INTERNAL_ERROR),
                    errmsg("Entity failed to be updated: %i", result)));
        }

        /* Insert index entries for the tuple */
        if (resultRelInfo->ri_NumIndices > 0 && update_indexes != TU_None)
        {
          ExecInsertIndexTuples(resultRelInfo, elemTupleSlot, estate, false, false, NULL, NIL,
                                (update_indexes == TU_Summarizing));
        }

        if (close_indices)
        {
            ExecCloseIndices(resultRelInfo);
        }
    }
    else if (lock_result == TM_SelfModified)
    {
        if (hufd.cmax != cid)
        {
            ereport(ERROR,
                    (errcode(ERRCODE_TRIGGERED_DATA_CHANGE_VIOLATION),
                     errmsg("tuple to be updated was already modified")));
        }
    }
    else
    {
        ereport(ERROR, (errcode(ERRCODE_INTERNAL_ERROR),
                errmsg("Entity failed to be updated: %i", lock_result)));
    }

    ReleaseBuffer(buffer);

    estate->es_result_relations = saved_resultRels;

    return tuple;
}

/*
 * When SET or REMOVE is the last cypher clause, consume all input from the
 * previous clause(s) in the first call.
 */
static void process_all_tuples(CustomScanState *node)
{
    cypher_set_custom_scan_state *css = (cypher_set_custom_scan_state *)node;
    TupleTableSlot *slot;
    EState *estate = css->css.ss.ps.state;
    ExprContext *econtext = css->css.ss.ps.ps_ExprContext;

    do
    {
        /*
         * The left child owns the current scan tuple and its Datums. Resetting
         * this node's context only releases scratch from the previous update.
         */
        ResetExprContext(econtext);
        process_update_list(node);
        Decrement_Estate_CommandId(estate)
        slot = ExecProcNode(node->ss.ps.lefttree);
        Increment_Estate_CommandId(estate)
    } while (!TupIsNull(slot));
}

/*
 * Checks the path to see if the entities contained within
 * have the same graphid and the updated_id field. Returns
 * true if yes, false otherwise.
 */
static bool check_path(agtype_value *path, graphid updated_id)
{
    int i;

    for (i = 0; i < path->val.array.num_elems; i++)
    {
        agtype_value *elem = &path->val.array.elems[i];

        agtype_value *id = GET_AGTYPE_VALUE_OBJECT_VALUE(elem, "id");

        if (updated_id == id->val.int_value)
        {
            return true;
        }
    }

    return false;
}

/*
 * Construct a new agtype path with the entity with updated_id
 * replacing all of its instances in path with updated_entity
 */
static agtype_value *replace_entity_in_path(agtype_value *path,
                                            graphid updated_id,
                                            agtype *updated_entity)
{
    agtype_iterator *it;
    agtype_iterator_token tok = WAGT_DONE;
    agtype_parse_state *parse_state = NULL;
    agtype_value *r;
    agtype_value *parsed_agtype_value = NULL;
    agtype *prop_agtype;
    int i;

    r = palloc(sizeof(agtype_value));

    prop_agtype = agtype_value_to_agtype(path);
    it = agtype_iterator_init(&prop_agtype->root);
    tok = agtype_iterator_next(&it, r, true);

    parsed_agtype_value = push_agtype_value(&parse_state, tok,
                                            tok < WAGT_BEGIN_ARRAY ? r : NULL);

    /* Iterate through the path, replace entities as necessary. */
    for (i = 0; i < path->val.array.num_elems; i++)
    {
        agtype_value *id, *elem;

        elem = &path->val.array.elems[i];

        /* something unexpected happened, throw an error. */
        if (elem->type != AGTV_VERTEX && elem->type != AGTV_EDGE)
        {
            ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
                            errmsg("unsupported agtype found in a path")));
        }

        /* extract the id field */
        id = GET_AGTYPE_VALUE_OBJECT_VALUE(elem, "id");

        /*
         * Either replace or keep the entity in the new path, depending on the id
         * check.
         */
        if (updated_id == id->val.int_value)
        {
            parsed_agtype_value = push_agtype_value(&parse_state, WAGT_ELEM,
                get_ith_agtype_value_from_container(&updated_entity->root, 0));
        }
        else
        {
            parsed_agtype_value = push_agtype_value(&parse_state, WAGT_ELEM,
                                                    elem);
        }
    }

    parsed_agtype_value = push_agtype_value(&parse_state, WAGT_END_ARRAY, NULL);
    parsed_agtype_value->type = AGTV_PATH;

    return parsed_agtype_value;
}

/*
 * Keep every reference to an updated entity in the current row consistent.
 * An entity can appear directly under another variable or within a path.
 */
static void update_entity_references(TupleTableSlot *scanTupleSlot, graphid id,
                                     agtype *updated_entity)
{
    int i;

    for (i = 0; i < scanTupleSlot->tts_tupleDescriptor->natts; i++)
    {
        agtype *original_entity;
        agtype_value *original_entity_value;

        /* skip nulls */
        if (TupleDescAttr(scanTupleSlot->tts_tupleDescriptor, i)->atttypid != AGTYPEOID)
        {
            continue;
        }

        /* skip non agtype values */
        if (scanTupleSlot->tts_isnull[i])
        {
            continue;
        }

        original_entity = DATUM_GET_AGTYPE_P(scanTupleSlot->tts_values[i]);

        /* Vertices, edges, and paths are represented as scalar agtype values. */
        if (!AGTYPE_CONTAINER_IS_SCALAR(&original_entity->root))
        {
            continue;
        }

        original_entity_value = get_ith_agtype_value_from_container(&original_entity->root, 0);

        if (original_entity_value->type == AGTV_VERTEX ||
            original_entity_value->type == AGTV_EDGE)
        {
            agtype_value *original_id = GET_AGTYPE_VALUE_OBJECT_VALUE(
                original_entity_value, "id");

            if (original_id->val.int_value == id)
            {
                scanTupleSlot->tts_values[i] =
                    AGTYPE_P_GET_DATUM(updated_entity);
            }
        }
        else if (original_entity_value->type == AGTV_PATH)
        {
            /* check if the path contains the entity. */
            if (check_path(original_entity_value, id))
            {
                /* the path does contain the entity replace with the new entity. */
                agtype_value *new_path = replace_entity_in_path(original_entity_value, id, updated_entity);

                scanTupleSlot->tts_values[i] = AGTYPE_P_GET_DATUM(agtype_value_to_agtype(new_path));
            }
        }
    }
}

static agtype_value *get_tuple_properties(Relation rel, HeapTuple tuple,
                                          bool is_vertex)
{
    AttrNumber properties_attribute;
    agtype_iterator *iterator;
    agtype_iterator_token token;
    agtype_parse_state *parse_state = NULL;
    agtype_value value;
    agtype_value *properties = NULL;
    agtype *properties_agtype;
    Datum properties_datum;
    bool isnull;

    properties_attribute = is_vertex
        ? Anum_ag_label_vertex_table_properties
        : Anum_ag_label_edge_table_properties;
    properties_datum = heap_getattr(tuple, properties_attribute,
                                    RelationGetDescr(rel), &isnull);
    if (isnull)
    {
        return NULL;
    }

    properties_agtype = DATUM_GET_AGTYPE_P(properties_datum);

    /*
     * AGE has no general agtype-to-agtype_value conversion helper. Rebuild the
     * container through its iterator to obtain a mutable AGTV_OBJECT.
     */
    iterator = agtype_iterator_init(&properties_agtype->root);
    while ((token = agtype_iterator_next(&iterator, &value, true)) !=
           WAGT_DONE)
    {
        properties = push_agtype_value(
            &parse_state, token,
            token < WAGT_BEGIN_ARRAY ? &value : NULL);
    }

    if (properties == NULL || properties->type != AGTV_OBJECT)
    {
        ereport(ERROR,
                (errcode(ERRCODE_DATA_EXCEPTION),
                 errmsg("entity properties must be an agtype object")));
    }

    return properties;
}

/*
 * Core SET logic that can be called from any executor (SET, MERGE, etc.).
 * Takes the CustomScanState for expression context and a
 * cypher_update_information describing which properties to set.
 */
void apply_update_list(CustomScanState *node,
                       cypher_update_information *set_info)
{
    EntityLookupCache *entity_lookup_cache = NULL;
    ExprContext *econtext = node->ss.ps.ps_ExprContext;
    TupleTableSlot *scanTupleSlot = econtext->ecxt_scantuple;
    ListCell *lc;
    EState *estate = node->ss.ps.state;
    int *luindex = NULL;
    bool *seen_entity_positions = NULL;
    int lidx = 0;
    HTAB *qual_cache = NULL;
    HASHCTL hashctl;

    if (node->methods == &cypher_set_exec_methods)
    {
        cypher_set_custom_scan_state *css =
            (cypher_set_custom_scan_state *)node;

        entity_lookup_cache = css->entity_lookup_cache;
    }
    else if (node->methods == &cypher_merge_exec_methods)
    {
        cypher_merge_custom_scan_state *css =
            (cypher_merge_custom_scan_state *)node;

        entity_lookup_cache = css->entity_lookup_cache;
    }

    /* allocate an array to hold the last update index of each 'entity' */
    luindex = palloc0(sizeof(int) * scanTupleSlot->tts_nvalid);
    seen_entity_positions =
        palloc0(sizeof(bool) * scanTupleSlot->tts_nvalid);

    /* Hash table for caching compiled security quals per label */
    MemSet(&hashctl, 0, sizeof(hashctl));
    hashctl.keysize = sizeof(Oid);
    hashctl.entrysize = sizeof(RLSCacheEntry);
    hashctl.hcxt = CurrentMemoryContext;
    qual_cache = hash_create("update_qual_cache", 8, &hashctl,
                             HASH_ELEM | HASH_BLOBS | HASH_CONTEXT);

    /*
     * Iterate through the SET items list and store the loop index of each
     * 'entity' update. As there is only one entry for each entity, this will
     * have the effect of overwriting the previous loop index stored - if this
     * 'entity' is used more than once. This will create an array of the last
     * loop index for the update of that particular 'entity'. This will allow us
     * to correctly update an 'entity' after all other previous updates to that
     * 'entity' have been done.
     */
    foreach (lc, set_info->set_items)
    {
        cypher_update_item *update_item = NULL;

        update_item = (cypher_update_item *)lfirst(lc);
        luindex[update_item->entity_position - 1] = lidx;

        /* increment the loop index */
        lidx++;
    }

    /* reset loop index */
    lidx = 0;

    /* iterate through SET set items */
    foreach (lc, set_info->set_items)
    {
        agtype_value *altered_properties;
        agtype_value *original_entity_value;
        agtype_value *original_properties;
        agtype_value *id;
        agtype_value *label;
        agtype *original_entity;
        agtype *new_property_value = NULL;
        TupleTableSlot *slot;
        ResultRelInfo *resultRelInfo;
        bool remove_property;
        char *label_name;
        cypher_update_item *update_item;
        Datum new_entity;
        HeapTuple heap_tuple;
        HeapTuple found_tuple = NULL;
        char *clause_name = set_info->clause_name;
        int cid;
        Relation rel;
        Oid relid;
        bool first_update;
        bool last_update;

        update_item = (cypher_update_item *)lfirst(lc);

        /*
         * If the entity is null, we can skip this update. this will be
         * possible when the OPTIONAL MATCH clause is implemented.
         */
        if (scanTupleSlot->tts_isnull[update_item->entity_position - 1])
        {
            continue;
        }

        if (TupleDescAttr(scanTupleSlot->tts_tupleDescriptor, update_item->entity_position -1)->atttypid != AGTYPEOID)
        {
            ereport(ERROR,
                    (errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
                     errmsg("age %s clause can only update agtype",
                            clause_name)));
        }

        original_entity = DATUM_GET_AGTYPE_P(scanTupleSlot->tts_values[update_item->entity_position - 1]);
        original_entity_value = get_ith_agtype_value_from_container(&original_entity->root, 0);

        if (original_entity_value->type != AGTV_VERTEX &&
            original_entity_value->type != AGTV_EDGE)
        {
            ereport(ERROR,
                    (errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
                     errmsg("age %s clause can only update vertex and edges",
                            clause_name)));
        }

        /* get the id and label for later */
        id = GET_AGTYPE_VALUE_OBJECT_VALUE(original_entity_value, "id");
        label = GET_AGTYPE_VALUE_OBJECT_VALUE(original_entity_value, "label");

        label_name = pnstrdup(label->val.string.val, label->val.string.len);
        /* get the properties we need to update */
        original_properties = GET_AGTYPE_VALUE_OBJECT_VALUE(original_entity_value,
                                                            "properties");

        /*
         * Determine if the property should be removed. This will be because
         * this is a REMOVE clause or the variable references a variable that is
         * NULL. It will be possible for a variable to be NULL when OPTIONAL
         * MATCH is implemented.
         *
         * If prop_expr is set (used by MERGE ON CREATE/MATCH SET), evaluate
         * the expression directly rather than reading from the scan tuple.
         * The planner may have stripped the target entry at prop_position.
         */
        if (update_item->remove_item)
        {
            remove_property = true;
        }
        else if (update_item->prop_expr != NULL)
        {
            ExprState *expr_state;
            Datum val;
            bool isnull;

            /*
             * Use the pre-initialized ExprState if available (set during
             * plan init in begin_cypher_merge). Fall back to per-row init
             * for callers that haven't pre-initialized (e.g. plain SET).
             */
            if (update_item->prop_expr_state != NULL)
            {
                expr_state = update_item->prop_expr_state;
            }
            else
            {
                expr_state = ExecInitExpr((Expr *)update_item->prop_expr,
                                          (PlanState *)node);
            }
            val = ExecEvalExpr(expr_state, econtext, &isnull);
            remove_property = isnull;

            if (!isnull)
            {
                new_property_value = DATUM_GET_AGTYPE_P(val);
            }
        }
        else
        {
            remove_property = scanTupleSlot->tts_isnull[update_item->prop_position - 1];
        }

        /*
         * If we need to remove the property, set the value to NULL. Otherwise
         * fetch the evaluated expression from the tuple slot.
         */
        if (remove_property)
        {
            new_property_value = NULL;
        }
        else if (update_item->prop_expr == NULL)
        {
            new_property_value = DATUM_GET_AGTYPE_P(scanTupleSlot->tts_values[update_item->prop_position - 1]);
        }

        resultRelInfo = create_entity_result_rel_info(
            estate, set_info->graph_name, label_name);

        rel = resultRelInfo->ri_RelationDesc;
        relid = RelationGetRelid(rel);

        slot = ExecInitExtraTupleSlot(
            estate, RelationGetDescr(resultRelInfo->ri_RelationDesc),
            &TTSOpsHeapTuple);

        /* Setup RLS policies if RLS is enabled */
        if (check_enable_rls(resultRelInfo->ri_RelationDesc->rd_id,
                             InvalidOid, true) == RLS_ENABLED)
        {
            RLSCacheEntry *entry;
            bool found;

            /* Get cached RLS state for this label, or set it up */
            entry = hash_search(qual_cache, &relid, HASH_ENTER, &found);
            if (!found)
            {
                /* Setup WITH CHECK policies */
                setup_wcos(resultRelInfo, estate, node, CMD_UPDATE);
                entry->withCheckOptions = resultRelInfo->ri_WithCheckOptions;
                entry->withCheckOptionExprs = resultRelInfo->ri_WithCheckOptionExprs;

                /* Setup security quals */
                entry->qualExprs = setup_security_quals(resultRelInfo, estate,
                                                        node, CMD_UPDATE);
                entry->slot = ExecInitExtraTupleSlot(
                    estate, RelationGetDescr(resultRelInfo->ri_RelationDesc),
                    &TTSOpsHeapTuple);
            }
            else
            {
                /* Use cached WCOs */
                resultRelInfo->ri_WithCheckOptions = entry->withCheckOptions;
                resultRelInfo->ri_WithCheckOptionExprs = entry->withCheckOptionExprs;
            }
        }

        first_update =
            !seen_entity_positions[update_item->entity_position - 1];
        last_update =
            luindex[update_item->entity_position - 1] == lidx;

        cid = estate->es_snapshot->curcid;
        estate->es_snapshot->curcid = GetCurrentCommandId(false);

        /*
         * The entity value carried by an alias may predate an earlier SET
         * clause in the same query. Use the current heap tuple as the base for
         * the first update of each entity position. The last update also needs
         * the tuple for the physical write, so reuse that lookup below.
         */
        if (first_update || last_update)
        {
            found_tuple = find_entity_tuple(
                rel, estate->es_snapshot, id->val.int_value, scanTupleSlot,
                update_item->ctid_position, entity_lookup_cache);
        }
        if (first_update && HeapTupleIsValid(found_tuple))
        {
            ItemPointerData ctid_hint;

            /* Compare both CTID components to detect a moved tuple version. */
            if (!get_entity_ctid_hint(scanTupleSlot,
                                      update_item->ctid_position,
                                      &ctid_hint) ||
                ItemPointerGetBlockNumberNoCheck(&ctid_hint) !=
                    ItemPointerGetBlockNumberNoCheck(&found_tuple->t_self) ||
                ItemPointerGetOffsetNumberNoCheck(&ctid_hint) !=
                    ItemPointerGetOffsetNumberNoCheck(&found_tuple->t_self))
            {
                original_properties = get_tuple_properties(
                    rel, found_tuple,
                    original_entity_value->type == AGTV_VERTEX);
            }
        }
        seen_entity_positions[update_item->entity_position - 1] = true;

        /* Alter the properties Agtype value. */
        if (update_item->prop_name != NULL &&
            strcmp(update_item->prop_name, "") != 0)
        {
            altered_properties = alter_property_value(original_properties,
                                                      update_item->prop_name,
                                                      new_property_value,
                                                      remove_property);
        }
        else
        {
            altered_properties = alter_properties(
                update_item->is_add ? original_properties : NULL,
                new_property_value);

            /*
             * For SET clause with plus-equal operator, nulls are not removed
             * from the map during transformation because they are required in
             * the executor to alter (merge) properties correctly. Only after
             * that step, they can be removed.
             */
            if (update_item->is_add)
            {
                remove_null_from_agtype_object(altered_properties);
            }
        }

        /*
         *  Now that we have the updated properties, create a either a vertex or
         *  edge Datum for the in-memory update, and setup the tupleTableSlot
         *  for the on-disc update.
         */
        if (original_entity_value->type == AGTV_VERTEX)
        {
            new_entity = make_vertex(GRAPHID_GET_DATUM(id->val.int_value),
                                     string_to_agtype(label_name),
                                     AGTYPE_P_GET_DATUM(agtype_value_to_agtype(altered_properties)));

            slot = populate_vertex_tts(slot, id, altered_properties);
        }
        else if (original_entity_value->type == AGTV_EDGE)
        {
            agtype_value *startid = GET_AGTYPE_VALUE_OBJECT_VALUE(original_entity_value, "start_id");
            agtype_value *endid = GET_AGTYPE_VALUE_OBJECT_VALUE(original_entity_value, "end_id");

            new_entity = make_edge(GRAPHID_GET_DATUM(id->val.int_value),
                                   GRAPHID_GET_DATUM(startid->val.int_value),
                                   GRAPHID_GET_DATUM(endid->val.int_value),
                                   string_to_agtype(label_name),
                                   AGTYPE_P_GET_DATUM(agtype_value_to_agtype(altered_properties)));

            slot = populate_edge_tts(slot, id, startid, endid,
                                     altered_properties);
        }
        else
        {
            ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
                            errmsg("age %s clause can only update vertex and edges",
                                   clause_name)));
        }

        /* place the datum in its tuple table slot position. */
        scanTupleSlot->tts_values[update_item->entity_position - 1] = new_entity;

        /* Keep aliases and paths in the current row in sync. */
        update_entity_references(scanTupleSlot, id->val.int_value,
                                 DATUM_GET_AGTYPE_P(new_entity));

        /*
         * If the last update index for the entity is equal to the current loop
         * index, then update this tuple.
         */
        if (last_update)
        {
            if (HeapTupleIsValid(found_tuple))
            {
                bool should_update = true;

                if (check_enable_rls(relid, InvalidOid, true) == RLS_ENABLED)
                {
                    RLSCacheEntry *entry;

                    entry = hash_search(qual_cache, &relid, HASH_FIND, NULL);
                    if (entry == NULL)
                        elog(ERROR, "missing RLS cache entry for relation %u",
                             relid);
                    ExecStoreHeapTuple(found_tuple, entry->slot, false);
                    should_update = check_security_quals(entry->qualExprs,
                                                         entry->slot, econtext);
                }
                if (should_update)
                {
                    heap_tuple = update_entity_tuple(resultRelInfo, slot, estate,
                                                     found_tuple);
                    if (heap_tuple != NULL &&
                        update_item->ctid_position > 0 &&
                        update_item->ctid_position <=
                            scanTupleSlot->tts_tupleDescriptor->natts)
                    {
                        agtype *ctid_agtype;
                        agtype_value ctid_value;
                        MemoryContext old;

                        ctid_value.type = AGTV_INTEGER;
                        ctid_value.val.int_value = AGE_CTID_PACK(
                            ItemPointerGetBlockNumber(&heap_tuple->t_self),
                            ItemPointerGetOffsetNumber(&heap_tuple->t_self));
                        old = MemoryContextSwitchTo(
                            econtext->ecxt_per_tuple_memory);
                        ctid_agtype = agtype_value_to_agtype(&ctid_value);
                        MemoryContextSwitchTo(old);

                        /*
                         * Refresh this variable's hint only. Other aliases of
                         * the graphid may retain a stale hint, which is safe:
                         * find_entity_tuple() validates it and falls back to
                         * graphid lookup.
                         */
                        scanTupleSlot->tts_values[
                            update_item->ctid_position - 1] =
                            AGTYPE_P_GET_DATUM(ctid_agtype);
                        scanTupleSlot->tts_isnull[
                            update_item->ctid_position - 1] = false;
                    }
                }
            }
        }

        if (HeapTupleIsValid(found_tuple))
        {
            heap_freetuple(found_tuple);
        }
        estate->es_snapshot->curcid = cid;
        /* close relation */
        ExecCloseIndices(resultRelInfo);
        table_close(resultRelInfo->ri_RelationDesc, RowExclusiveLock);

        /* increment loop index */
        lidx++;
    }

    /* Clean up the cache */
    hash_destroy(qual_cache);

    /* free our lookup array */
    pfree_if_not_null(luindex);
    pfree_if_not_null(seen_entity_positions);
}

static void process_update_list(CustomScanState *node)
{
    cypher_set_custom_scan_state *css = (cypher_set_custom_scan_state *)node;

    apply_update_list(node, css->set_list);
}

static TupleTableSlot *exec_cypher_set(CustomScanState *node)
{
    cypher_set_custom_scan_state *css = (cypher_set_custom_scan_state *)node;
    ResultRelInfo **saved_resultRels;
    EState *estate = css->css.ss.ps.state;
    ExprContext *econtext = css->css.ss.ps.ps_ExprContext;
    TupleTableSlot *slot;

    saved_resultRels = estate->es_result_relations;

    /* Process the subtree first */
    Decrement_Estate_CommandId(estate);
    slot = ExecProcNode(node->ss.ps.lefttree);
    Increment_Estate_CommandId(estate);

    if (TupIsNull(slot))
    {
        return NULL;
    }

    econtext->ecxt_scantuple =
        node->ss.ps.lefttree->ps_ProjInfo->pi_exprContext->ecxt_scantuple;

    if (CYPHER_CLAUSE_IS_TERMINAL(css->flags))
    {
        estate->es_result_relations = saved_resultRels;

        process_all_tuples(node);

        /* increment the command counter to reflect the updates */
        CommandCounterIncrement();

        /* invalidate VLE cache — graph was mutated */
        increment_graph_version(get_graph_oid(css->set_list->graph_name));

        return NULL;
    }

    /*
     * The left child owns scanTupleSlot and its Datums. This reset only frees
     * per-row scratch allocated by this CustomScan on the preceding call.
     */
    ResetExprContext(econtext);
    process_update_list(node);

    /* increment the command counter to reflect the updates */
    CommandCounterIncrement();

    /* invalidate VLE cache — graph was mutated */
    increment_graph_version(get_graph_oid(css->set_list->graph_name));

    estate->es_result_relations = saved_resultRels;

    econtext->ecxt_scantuple = ExecProject(node->ss.ps.lefttree->ps_ProjInfo);

    return ExecProject(node->ss.ps.ps_ProjInfo);
}

static void end_cypher_set(CustomScanState *node)
{
    /* Release cached index handles and fetch slots. */
    destroy_entity_lookup_cache(
        ((cypher_set_custom_scan_state *)node)->entity_lookup_cache);

    ExecEndNode(node->ss.ps.lefttree);
}

static void rescan_cypher_set(CustomScanState *node)
{
    cypher_set_custom_scan_state *css = (cypher_set_custom_scan_state *)node;
    char *clause_name = css->set_list->clause_name;

     ereport(ERROR,
             (errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
                      errmsg("cypher %s clause cannot be rescanned",
                             clause_name),
                      errhint("its unsafe to use joins in a query with a Cypher %s clause", clause_name)));
}

Node *create_cypher_set_plan_state(CustomScan *cscan)
{
    cypher_set_custom_scan_state *cypher_css = palloc0(sizeof(cypher_set_custom_scan_state));
    cypher_update_information *set_list;
    char *serialized_data;
    Const *c;

    cypher_css->cs = cscan;

    /* get the serialized data structure from the Const and deserialize it. */
    c = linitial(cscan->custom_private);
    serialized_data = (char *)c->constvalue;
    set_list = stringToNode(serialized_data);

    Assert(is_ag_node(set_list, cypher_update_information));

    cypher_css->set_list = set_list;
    cypher_css->flags = set_list->flags;

    cypher_css->css.ss.ps.type = T_CustomScanState;
    cypher_css->css.methods = &cypher_set_exec_methods;

    return (Node *)cypher_css;
}
