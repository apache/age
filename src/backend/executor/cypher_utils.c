/*
 * For PostgreSQL Database Management System:
 * (formerly known as Postgres, then as Postgres95)
 *
 * Portions Copyright (c) 1996-2010, The PostgreSQL Global Development Group
 *
 * Portions Copyright (c) 1994, The Regents of the University of California
 *
 * Permission to use, copy, modify, and distribute this software and its documentation for any purpose,
 * without fee, and without a written agreement is hereby granted, provided that the above copyright notice
 * and this paragraph and the following two paragraphs appear in all copies.
 *
 * IN NO EVENT SHALL THE UNIVERSITY OF CALIFORNIA BE LIABLE TO ANY PARTY FOR DIRECT,
 * INDIRECT, SPECIAL, INCIDENTAL, OR CONSEQUENTIAL DAMAGES, INCLUDING LOST PROFITS,
 * ARISING OUT OF THE USE OF THIS SOFTWARE AND ITS DOCUMENTATION, EVEN IF THE UNIVERSITY
 * OF CALIFORNIA HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * THE UNIVERSITY OF CALIFORNIA SPECIFICALLY DISCLAIMS ANY WARRANTIES, INCLUDING,
 * BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 *
 * THE SOFTWARE PROVIDED HEREUNDER IS ON AN "AS IS" BASIS, AND THE UNIVERSITY OF CALIFORNIA
 * HAS NO OBLIGATIONS TO PROVIDE MAINTENANCE, SUPPORT, UPDATES, ENHANCEMENTS, OR MODIFICATIONS.
 */

#include "postgres.h"

#include "executor/executor.h"
#include "miscadmin.h"
#include "nodes/makefuncs.h"
#include "parser/parse_relation.h"
#include "rewrite/rewriteManip.h"
#include "rewrite/rowsecurity.h"
#include "utils/acl.h"
#include "utils/rls.h"

#include "catalog/ag_label.h"
#include "commands/label_commands.h"
#include "executor/cypher_utils.h"
#include "utils/ag_cache.h"

/* RLS helper function declarations */
static void get_policies_for_relation(Relation relation, CmdType cmd,
                                      Oid user_id, List **permissive_policies,
                                      List **restrictive_policies);
static void add_with_check_options(Relation rel, int rt_index, WCOKind kind,
                                   List *permissive_policies,
                                   List *restrictive_policies,
                                   List **withCheckOptions, bool *hasSubLinks,
                                   bool force_using);
static void add_security_quals(int rt_index, List *permissive_policies,
                               List *restrictive_policies,
                               List **securityQuals, bool *hasSubLinks);
static void sort_policies_by_name(List *policies);
static int row_security_policy_cmp(const ListCell *a, const ListCell *b);
static bool check_role_for_policy(ArrayType *policy_roles, Oid user_id);

/*
 * Given the graph name and the label name, create a ResultRelInfo for the table
 * those two variables represent. Open the Indices too.
 */
ResultRelInfo *create_entity_result_rel_info(EState *estate, char *graph_name,
                                             char *label_name)
{
    RangeVar *rv = NULL;
    Relation label_relation = NULL;
    ResultRelInfo *resultRelInfo = NULL;
    ParseState *pstate = NULL;
    RangeTblEntry *rte = NULL;
    int pii = 0;

    /* create a new parse state for this operation */
    pstate = make_parsestate(NULL);

    resultRelInfo = palloc(sizeof(ResultRelInfo));

    if (strlen(label_name) == 0)
    {
        rv = makeRangeVar(graph_name, AG_DEFAULT_LABEL_VERTEX, -1);
    }
    else
    {
        rv = makeRangeVar(graph_name, label_name, -1);
    }

    label_relation = parserOpenTable(pstate, rv, RowExclusiveLock);

    /*
     * Get the rte to determine the correct perminfoindex value. Some rtes
     * may have it set up, some created here (executor) may not.
     *
     * Note: The RTEPermissionInfo structure was added in PostgreSQL version 16.
     *
     * Note: We use the list_length because exec_rt_fetch starts at 1, not 0.
     *       Doing this gives us the last rte in the es_range_table list, which
     *       is the rte in question.
     *
     *       If the rte is created here and doesn't have a perminfoindex, we
     *       need to pass on a 0. Otherwise, later on GetResultRTEPermissionInfo
     *       will attempt to get the rte's RTEPermissionInfo data, which doesn't
     *       exist.
     *
     * TODO: Ideally, we should consider creating the RTEPermissionInfo data,
     *       but as this is just a read of the label relation, it is likely
     *       unnecessary.
     */
    rte = exec_rt_fetch(list_length(estate->es_range_table), estate);
    pii = (rte->perminfoindex == 0) ? 0 : list_length(estate->es_range_table);

    /* initialize the resultRelInfo */
    InitResultRelInfo(resultRelInfo, label_relation, pii, NULL,
                      estate->es_instrument);

    /* open the indices */
    ExecOpenIndices(resultRelInfo, false);

    free_parsestate(pstate);

    return resultRelInfo;
}

/* close the result_rel_info and close all the indices */
void destroy_entity_result_rel_info(ResultRelInfo *result_rel_info)
{
    /* close the indices */
    ExecCloseIndices(result_rel_info);

    /* close the rel */
    table_close(result_rel_info->ri_RelationDesc, RowExclusiveLock);
}

TupleTableSlot *populate_vertex_tts(
    TupleTableSlot *elemTupleSlot, agtype_value *id, agtype_value *properties)
{
    bool properties_isnull;

    if (id == NULL)
    {
        ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
                        errmsg("vertex id field cannot be NULL")));
    }

    properties_isnull = properties == NULL;

    elemTupleSlot->tts_values[vertex_tuple_id] = GRAPHID_GET_DATUM(id->val.int_value);
    elemTupleSlot->tts_isnull[vertex_tuple_id] = false;

    elemTupleSlot->tts_values[vertex_tuple_properties] =
        AGTYPE_P_GET_DATUM(agtype_value_to_agtype(properties));
    elemTupleSlot->tts_isnull[vertex_tuple_properties] = properties_isnull;

    return elemTupleSlot;
}

TupleTableSlot *populate_edge_tts(
    TupleTableSlot *elemTupleSlot, agtype_value *id, agtype_value *startid,
    agtype_value *endid, agtype_value *properties)
{
    bool properties_isnull;

    if (id == NULL)
    {
        ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
                        errmsg("edge id field cannot be NULL")));
    }
    if (startid == NULL)
    {
        ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
                        errmsg("edge start_id field cannot be NULL")));
    }

    if (endid == NULL)
    {
        ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
                        errmsg("edge end_id field cannot be NULL")));
    }

    properties_isnull = properties == NULL;

    elemTupleSlot->tts_values[edge_tuple_id] =
        GRAPHID_GET_DATUM(id->val.int_value);
    elemTupleSlot->tts_isnull[edge_tuple_id] = false;

    elemTupleSlot->tts_values[edge_tuple_start_id] =
        GRAPHID_GET_DATUM(startid->val.int_value);
    elemTupleSlot->tts_isnull[edge_tuple_start_id] = false;

    elemTupleSlot->tts_values[edge_tuple_end_id] =
        GRAPHID_GET_DATUM(endid->val.int_value);
    elemTupleSlot->tts_isnull[edge_tuple_end_id] = false;

    elemTupleSlot->tts_values[edge_tuple_properties] =
        AGTYPE_P_GET_DATUM(agtype_value_to_agtype(properties));
    elemTupleSlot->tts_isnull[edge_tuple_properties] = properties_isnull;

    return elemTupleSlot;
}


/*
 * Find out if the entity still exists. This is for 'implicit' deletion
 * of an entity.
 */
bool entity_exists(EState *estate, Oid graph_oid, graphid id)
{
    label_cache_data *label;
    ScanKeyData scan_keys[1];
    TableScanDesc scan_desc;
    HeapTuple tuple;
    Relation rel;
    bool result = true;

    /*
     * Extract the label id from the graph id and get the table name
     * the entity is part of.
     */
    label = search_label_graph_oid_cache(graph_oid, GET_LABEL_ID(id));

    /* Setup the scan key to be the graphid */
    ScanKeyInit(&scan_keys[0], 1, BTEqualStrategyNumber,
                F_GRAPHIDEQ, GRAPHID_GET_DATUM(id));

    rel = table_open(label->relation, RowExclusiveLock);
    scan_desc = table_beginscan(rel, estate->es_snapshot, 1, scan_keys);

    tuple = heap_getnext(scan_desc, ForwardScanDirection);

    /*
     * If a single tuple was returned, the tuple is still valid, otherwise'
     * set to false.
     */
    if (!HeapTupleIsValid(tuple))
    {
        result = false;
    }

    table_endscan(scan_desc);
    table_close(rel, RowExclusiveLock);

    return result;
}

/*
 * Insert the edge/vertex tuple into the table and indices. Check that the
 * table's constraints have not been violated.
 *
 * This function defaults to, and flags for update, the currentCommandId. If
 * you need to pass a specific cid and avoid using the currentCommandId, use
 * insert_entity_tuple_cid instead.
 */
HeapTuple insert_entity_tuple(ResultRelInfo *resultRelInfo,
                              TupleTableSlot *elemTupleSlot,
                              EState *estate)
{
    return insert_entity_tuple_cid(resultRelInfo, elemTupleSlot, estate,
                                   GetCurrentCommandId(true));
}

/*
 * Insert the edge/vertex tuple into the table and indices. Check that the
 * table's constraints have not been violated.
 *
 * This function uses the passed cid for updates.
 */
HeapTuple insert_entity_tuple_cid(ResultRelInfo *resultRelInfo,
                                  TupleTableSlot *elemTupleSlot,
                                  EState *estate, CommandId cid)
{
    HeapTuple tuple = NULL;

    ExecStoreVirtualTuple(elemTupleSlot);
    tuple = ExecFetchSlotHeapTuple(elemTupleSlot, true, NULL);

    /* Check the constraints of the tuple */
    tuple->t_tableOid = RelationGetRelid(resultRelInfo->ri_RelationDesc);
    if (resultRelInfo->ri_RelationDesc->rd_att->constr != NULL)
    {
        ExecConstraints(resultRelInfo, elemTupleSlot, estate);
    }

    /* Check RLS WITH CHECK policies if configured */
    if (resultRelInfo->ri_WithCheckOptions != NIL)
    {
        ExecWithCheckOptions(WCO_RLS_INSERT_CHECK, resultRelInfo,
                             elemTupleSlot, estate);
    }

    /* Insert the tuple normally */
    table_tuple_insert(resultRelInfo->ri_RelationDesc, elemTupleSlot, cid, 0,
                       NULL);

    /* Insert index entries for the tuple */
    if (resultRelInfo->ri_NumIndices > 0)
    {
        ExecInsertIndexTuples(resultRelInfo, elemTupleSlot, estate,
                              false, false, NULL, NIL, false);
    }

    return tuple;
}

/*
 * setup_wcos
 *
 * WithCheckOptions are added during the rewrite phase, but since AGE uses
 * CMD_SELECT for all queries, WCOs don't get added for CREATE/SET/MERGE
 * operations. This function compensates by adding WCOs at execution time.
 *
 * Based on PostgreSQL's row security implementation in rowsecurity.c
 */
void setup_wcos(ResultRelInfo *resultRelInfo, EState *estate,
                CustomScanState *node, CmdType cmd)
{
    List *permissive_policies;
    List *restrictive_policies;
    List *withCheckOptions = NIL;
    List *wcoExprs = NIL;
    ListCell *lc;
    Relation rel;
    Oid user_id;
    int rt_index;
    WCOKind wco_kind;
    bool hasSubLinks = false;

    /* Determine the WCO kind based on command type */
    if (cmd == CMD_INSERT)
    {
        wco_kind = WCO_RLS_INSERT_CHECK;
    }
    else if (cmd == CMD_UPDATE)
    {
        wco_kind = WCO_RLS_UPDATE_CHECK;
    }
    else
    {
        ereport(ERROR,
                (errcode(ERRCODE_INTERNAL_ERROR),
                 errmsg_internal("unexpected command type for setup_wcos")));
    }

    rel = resultRelInfo->ri_RelationDesc;

    /*
     * Use rt_index=1 since we're evaluating policies against a single relation.
     * Policy quals are stored with varno=1, and we set ecxt_scantuple to the
     * tuple we want to check, so keeping varno=1 is correct.
     */
    rt_index = 1;
    user_id = GetUserId();

    /* Get the policies for the specified command type */
    get_policies_for_relation(rel, cmd, user_id,
                              &permissive_policies,
                              &restrictive_policies);

    /* Build WithCheckOptions from the policies */
    add_with_check_options(rel, rt_index, wco_kind,
                           permissive_policies,
                           restrictive_policies,
                           &withCheckOptions,
                           &hasSubLinks,
                           false);

    /* Compile the WCO expressions */
    foreach(lc, withCheckOptions)
    {
        WithCheckOption *wco = lfirst_node(WithCheckOption, lc);
        ExprState *wcoExpr;

        /* Ensure qual is a List for ExecInitQual */
        if (!IsA(wco->qual, List))
        {
            wco->qual = (Node *) list_make1(wco->qual);
        }

        wcoExpr = ExecInitQual((List *) wco->qual, (PlanState *) node);
        wcoExprs = lappend(wcoExprs, wcoExpr);
    }

    /* Set up the ResultRelInfo with WCOs */
    resultRelInfo->ri_WithCheckOptions = withCheckOptions;
    resultRelInfo->ri_WithCheckOptionExprs = wcoExprs;
}

/*
 * get_policies_for_relation
 *
 * Returns lists of permissive and restrictive policies to be applied to the
 * specified relation, based on the command type and role.
 *
 * This includes any policies added by extensions.
 *
 * Copied from PostgreSQL's src/backend/rewrite/rowsecurity.c
 */
static void
get_policies_for_relation(Relation relation, CmdType cmd, Oid user_id,
                          List **permissive_policies,
                          List **restrictive_policies)
{
    ListCell *item;

    *permissive_policies = NIL;
    *restrictive_policies = NIL;

    /* No policies if RLS descriptor is not present */
    if (relation->rd_rsdesc == NULL)
    {
        return;
    }

    /* First find all internal policies for the relation. */
    foreach(item, relation->rd_rsdesc->policies)
    {
        bool cmd_matches = false;
        RowSecurityPolicy *policy = (RowSecurityPolicy *) lfirst(item);

        /* Always add ALL policies, if they exist. */
        if (policy->polcmd == '*')
        {
            cmd_matches = true;
        }
        else
        {
            /* Check whether the policy applies to the specified command type */
            switch (cmd)
            {
                case CMD_SELECT:
                    if (policy->polcmd == ACL_SELECT_CHR)
                    {
                        cmd_matches = true;
                    }
                    break;
                case CMD_INSERT:
                    if (policy->polcmd == ACL_INSERT_CHR)
                    {
                        cmd_matches = true;
                    }
                    break;
                case CMD_UPDATE:
                    if (policy->polcmd == ACL_UPDATE_CHR)
                    {
                        cmd_matches = true;
                    }
                    break;
                case CMD_DELETE:
                    if (policy->polcmd == ACL_DELETE_CHR)
                    {
                        cmd_matches = true;
                    }
                    break;
                case CMD_MERGE:
                    /*
                     * We do not support a separate policy for MERGE command.
                     * Instead it derives from the policies defined for other
                     * commands.
                     */
                    break;
                default:
                    elog(ERROR, "unrecognized policy command type %d",
                         (int) cmd);
                    break;
            }
        }

        /*
         * Add this policy to the relevant list of policies if it applies to
         * the specified role.
         */
        if (cmd_matches && check_role_for_policy(policy->roles, user_id))
        {
            if (policy->permissive)
            {
                *permissive_policies = lappend(*permissive_policies, policy);
            }
            else
            {
                *restrictive_policies = lappend(*restrictive_policies, policy);
            }
        }
    }

    /*
     * We sort restrictive policies by name so that any WCOs they generate are
     * checked in a well-defined order.
     */
    sort_policies_by_name(*restrictive_policies);

    /*
     * Then add any permissive or restrictive policies defined by extensions.
     * These are simply appended to the lists of internal policies, if they
     * apply to the specified role.
     */
    if (row_security_policy_hook_restrictive)
    {
        List *hook_policies =
            (*row_security_policy_hook_restrictive) (cmd, relation);

        /*
         * As with built-in restrictive policies, we sort any hook-provided
         * restrictive policies by name also.  Note that we also intentionally
         * always check all built-in restrictive policies, in name order,
         * before checking restrictive policies added by hooks, in name order.
         */
        sort_policies_by_name(hook_policies);

        foreach(item, hook_policies)
        {
            RowSecurityPolicy *policy = (RowSecurityPolicy *) lfirst(item);

            if (check_role_for_policy(policy->roles, user_id))
            {
                *restrictive_policies = lappend(*restrictive_policies, policy);
            }
        }
    }

    if (row_security_policy_hook_permissive)
    {
        List *hook_policies =
            (*row_security_policy_hook_permissive) (cmd, relation);

        foreach(item, hook_policies)
        {
            RowSecurityPolicy *policy = (RowSecurityPolicy *) lfirst(item);

            if (check_role_for_policy(policy->roles, user_id))
            {
                *permissive_policies = lappend(*permissive_policies, policy);
            }
        }
    }
}

/*
 * add_with_check_options
 *
 * Add WithCheckOptions of the specified kind to check that new records
 * added by an INSERT or UPDATE are consistent with the specified RLS
 * policies.  Normally new data must satisfy the WITH CHECK clauses from the
 * policies.  If a policy has no explicit WITH CHECK clause, its USING clause
 * is used instead.  In the special case of an UPDATE arising from an
 * INSERT ... ON CONFLICT DO UPDATE, existing records are first checked using
 * a WCO_RLS_CONFLICT_CHECK WithCheckOption, which always uses the USING
 * clauses from RLS policies.
 *
 * New WCOs are added to withCheckOptions, and hasSubLinks is set to true if
 * any of the check clauses added contain sublink subqueries.
 * 
 * Copied from PostgreSQL's src/backend/rewrite/rowsecurity.c
 */
static void
add_with_check_options(Relation rel,
                       int rt_index,
                       WCOKind kind,
                       List *permissive_policies,
                       List *restrictive_policies,
                       List **withCheckOptions,
                       bool *hasSubLinks,
                       bool force_using)
{
    ListCell *item;
    List *permissive_quals = NIL;

#define QUAL_FOR_WCO(policy) \
    ( !force_using && \
      (policy)->with_check_qual != NULL ? \
      (policy)->with_check_qual : (policy)->qual )

    /*
     * First collect up the permissive policy clauses, similar to
     * add_security_quals.
     */
    foreach(item, permissive_policies)
    {
        RowSecurityPolicy *policy = (RowSecurityPolicy *) lfirst(item);
        Expr *qual = QUAL_FOR_WCO(policy);

        if (qual != NULL)
        {
            permissive_quals = lappend(permissive_quals, copyObject(qual));
            *hasSubLinks |= policy->hassublinks;
        }
    }

    /*
     * There must be at least one permissive qual found or no rows are allowed
     * to be added.  This is the same as in add_security_quals.
     *
     * If there are no permissive_quals then we fall through and return a
     * single 'false' WCO, preventing all new rows.
     */
    if (permissive_quals != NIL)
    {
        /*
         * Add a single WithCheckOption for all the permissive policy clauses,
         * combining them together using OR.  This check has no policy name,
         * since if the check fails it means that no policy granted permission
         * to perform the update, rather than any particular policy being
         * violated.
         */
        WithCheckOption *wco;

        wco = makeNode(WithCheckOption);
        wco->kind = kind;
        wco->relname = pstrdup(RelationGetRelationName(rel));
        wco->polname = NULL;
        wco->cascaded = false;

        if (list_length(permissive_quals) == 1)
        {
            wco->qual = (Node *) linitial(permissive_quals);
        }
        else
        {
            wco->qual = (Node *) makeBoolExpr(OR_EXPR, permissive_quals, -1);
        }

        ChangeVarNodes(wco->qual, 1, rt_index, 0);

        *withCheckOptions = list_append_unique(*withCheckOptions, wco);

        /*
         * Now add WithCheckOptions for each of the restrictive policy clauses
         * (which will be combined together using AND).  We use a separate
         * WithCheckOption for each restrictive policy to allow the policy
         * name to be included in error reports if the policy is violated.
         */
        foreach(item, restrictive_policies)
        {
            RowSecurityPolicy *policy = (RowSecurityPolicy *) lfirst(item);
            Expr *qual = QUAL_FOR_WCO(policy);

            if (qual != NULL)
            {
                qual = copyObject(qual);
                ChangeVarNodes((Node *) qual, 1, rt_index, 0);

                wco = makeNode(WithCheckOption);
                wco->kind = kind;
                wco->relname = pstrdup(RelationGetRelationName(rel));
                wco->polname = pstrdup(policy->policy_name);
                wco->qual = (Node *) qual;
                wco->cascaded = false;

                *withCheckOptions = list_append_unique(*withCheckOptions, wco);
                *hasSubLinks |= policy->hassublinks;
            }
        }
    }
    else
    {
        /*
         * If there were no policy clauses to check new data, add a single
         * always-false WCO (a default-deny policy).
         */
        WithCheckOption *wco;

        wco = makeNode(WithCheckOption);
        wco->kind = kind;
        wco->relname = pstrdup(RelationGetRelationName(rel));
        wco->polname = NULL;
        wco->qual = (Node *) makeConst(BOOLOID, -1, InvalidOid,
                                       sizeof(bool), BoolGetDatum(false),
                                       false, true);
        wco->cascaded = false;

        *withCheckOptions = lappend(*withCheckOptions, wco);
    }
}

/*
 * sort_policies_by_name
 *
 * This is only used for restrictive policies, ensuring that any
 * WithCheckOptions they generate are applied in a well-defined order.
 * This is not necessary for permissive policies, since they are all combined
 * together using OR into a single WithCheckOption check.
 * 
 * Copied from PostgreSQL's src/backend/rewrite/rowsecurity.c
 */
static void
sort_policies_by_name(List *policies)
{
    list_sort(policies, row_security_policy_cmp);
}

/*
 * list_sort comparator to sort RowSecurityPolicy entries by name
 *
 * Copied from PostgreSQL's src/backend/rewrite/rowsecurity.c
 */
static int
row_security_policy_cmp(const ListCell *a, const ListCell *b)
{
    const RowSecurityPolicy *pa = (const RowSecurityPolicy *) lfirst(a);
    const RowSecurityPolicy *pb = (const RowSecurityPolicy *) lfirst(b);

    /* Guard against NULL policy names from extensions */
    if (pa->policy_name == NULL)
    {
        return pb->policy_name == NULL ? 0 : 1;
    }
    if (pb->policy_name == NULL)
    {
        return -1;
    }

    return strcmp(pa->policy_name, pb->policy_name);
}

/*
 * check_role_for_policy -
 *   determines if the policy should be applied for the current role
 *
 * Copied from PostgreSQL's src/backend/rewrite/rowsecurity.c
 */
static bool
check_role_for_policy(ArrayType *policy_roles, Oid user_id)
{
    int i;
    Oid *roles = (Oid *) ARR_DATA_PTR(policy_roles);

    /* Quick fall-thru for policies applied to all roles */
    if (roles[0] == ACL_ID_PUBLIC)
    {
        return true;
    }

    for (i = 0; i < ARR_DIMS(policy_roles)[0]; i++)
    {
        if (has_privs_of_role(user_id, roles[i]))
        {
            return true;
        }
    }

    return false;
}

/*
 * add_security_quals
 *
 * Add security quals to enforce the specified RLS policies, restricting
 * access to existing data in a table.  If there are no policies controlling
 * access to the table, then all access is prohibited --- i.e., an implicit
 * default-deny policy is used.
 *
 * New security quals are added to securityQuals, and hasSubLinks is set to
 * true if any of the quals added contain sublink subqueries.
 *
 * Copied from PostgreSQL's src/backend/rewrite/rowsecurity.c
 */
static void
add_security_quals(int rt_index,
                   List *permissive_policies,
                   List *restrictive_policies,
                   List **securityQuals,
                   bool *hasSubLinks)
{
    ListCell *item;
    List *permissive_quals = NIL;
    Expr *rowsec_expr;

    /*
     * First collect up the permissive quals.  If we do not find any
     * permissive policies then no rows are visible (this is handled below).
     */
    foreach(item, permissive_policies)
    {
        RowSecurityPolicy *policy = (RowSecurityPolicy *) lfirst(item);

        if (policy->qual != NULL)
        {
            permissive_quals = lappend(permissive_quals,
                                       copyObject(policy->qual));
            *hasSubLinks |= policy->hassublinks;
        }
    }

    /*
     * We must have permissive quals, always, or no rows are visible.
     *
     * If we do not, then we simply return a single 'false' qual which results
     * in no rows being visible.
     */
    if (permissive_quals != NIL)
    {
        /*
         * We now know that permissive policies exist, so we can now add
         * security quals based on the USING clauses from the restrictive
         * policies.  Since these need to be combined together using AND, we
         * can just add them one at a time.
         */
        foreach(item, restrictive_policies)
        {
            RowSecurityPolicy *policy = (RowSecurityPolicy *) lfirst(item);
            Expr *qual;

            if (policy->qual != NULL)
            {
                qual = copyObject(policy->qual);
                ChangeVarNodes((Node *) qual, 1, rt_index, 0);

                *securityQuals = list_append_unique(*securityQuals, qual);
                *hasSubLinks |= policy->hassublinks;
            }
        }

        /*
         * Then add a single security qual combining together the USING
         * clauses from all the permissive policies using OR.
         */
        if (list_length(permissive_quals) == 1)
        {
            rowsec_expr = (Expr *) linitial(permissive_quals);
        }
        else
        {
            rowsec_expr = makeBoolExpr(OR_EXPR, permissive_quals, -1);
        }

        ChangeVarNodes((Node *) rowsec_expr, 1, rt_index, 0);
        *securityQuals = list_append_unique(*securityQuals, rowsec_expr);
    }
    else
    {
        /*
         * A permissive policy must exist for rows to be visible at all.
         * Therefore, if there were no permissive policies found, return a
         * single always-false clause.
         */
        *securityQuals = lappend(*securityQuals,
                                 makeConst(BOOLOID, -1, InvalidOid,
                                           sizeof(bool), BoolGetDatum(false),
                                           false, true));
    }
}

/*
 * setup_security_quals
 *
 * Security quals (USING policies) are added during the rewrite phase, but
 * since AGE uses CMD_SELECT for all queries, they don't get added for
 * UPDATE/DELETE operations. This function sets up security quals at
 * execution time to be evaluated against each tuple before modification.
 *
 * Returns a list of compiled ExprState for the security quals.
 */
List *
setup_security_quals(ResultRelInfo *resultRelInfo, EState *estate,
                     CustomScanState *node, CmdType cmd)
{
    List *permissive_policies;
    List *restrictive_policies;
    List *securityQuals = NIL;
    List *qualExprs = NIL;
    ListCell *lc;
    Relation rel;
    Oid user_id;
    int rt_index;
    bool hasSubLinks = false;

    /* Only UPDATE and DELETE have security quals */
    if (cmd != CMD_UPDATE && cmd != CMD_DELETE)
    {
        ereport(ERROR,
                (errcode(ERRCODE_INTERNAL_ERROR),
                 errmsg_internal("unexpected command type for setup_security_quals")));
    }

    rel = resultRelInfo->ri_RelationDesc;

    /* If no RLS policies exist, return empty list */
    if (rel->rd_rsdesc == NULL)
    {
        return NIL;
    }

    /*
     * Use rt_index=1 since we're evaluating policies against a single relation.
     * Policy quals are stored with varno=1, and we set ecxt_scantuple to the
     * tuple we want to check, so keeping varno=1 is correct.
     */
    rt_index = 1;
    user_id = GetUserId();

    /* Get the policies for the specified command type */
    get_policies_for_relation(rel, cmd, user_id,
                              &permissive_policies,
                              &restrictive_policies);

    /* Build security quals from the policies */
    add_security_quals(rt_index, permissive_policies, restrictive_policies,
                       &securityQuals, &hasSubLinks);

    /* Compile the security qual expressions */
    foreach(lc, securityQuals)
    {
        Expr *qual = (Expr *) lfirst(lc);
        ExprState *qualExpr;

        /* Ensure qual is a List for ExecInitQual */
        if (!IsA(qual, List))
        {
            qual = (Expr *) list_make1(qual);
        }

        qualExpr = ExecInitQual((List *) qual, (PlanState *) node);
        qualExprs = lappend(qualExprs, qualExpr);
    }

    return qualExprs;
}

/*
 * check_security_quals
 *
 * Evaluate security quals against a tuple. Returns true if all quals pass
 * (row can be modified), false if any qual fails (row should be silently
 * skipped).
 *
 * This matches PostgreSQL's behavior where USING expressions for UPDATE/DELETE
 * silently filter rows rather than raising errors.
 */
bool
check_security_quals(List *qualExprs, TupleTableSlot *slot,
                     ExprContext *econtext)
{
    ListCell *lc;
    TupleTableSlot *saved_scantuple;
    bool result = true;

    if (qualExprs == NIL)
    {
        return true;
    }

    /* Save and set up the scan tuple for expression evaluation */
    saved_scantuple = econtext->ecxt_scantuple;
    econtext->ecxt_scantuple = slot;

    foreach(lc, qualExprs)
    {
        ExprState *qualExpr = (ExprState *) lfirst(lc);

        if (!ExecQual(qualExpr, econtext))
        {
            result = false;
            break;
        }
    }

    econtext->ecxt_scantuple = saved_scantuple;
    return result;
}

/*
 * check_rls_for_tuple
 *
 * Check RLS policies for a tuple without needing full executor context.
 * Used by standalone functions like startNode()/endNode() that access
 * tables directly.
 *
 * Returns true if the tuple passes RLS checks (or if RLS is not enabled),
 * false if the tuple should be filtered out.
 */
bool
check_rls_for_tuple(Relation rel, HeapTuple tuple, CmdType cmd)
{
    List *permissive_policies;
    List *restrictive_policies;
    List *securityQuals = NIL;
    ListCell *lc;
    Oid user_id;
    bool hasSubLinks = false;
    bool result = true;
    EState *estate;
    ExprContext *econtext;
    TupleTableSlot *slot;

    /* If RLS is not enabled, tuple passes */
    if (check_enable_rls(RelationGetRelid(rel), InvalidOid, true) != RLS_ENABLED)
    {
        return true;
    }

    /* If no RLS policies exist on the relation, tuple passes */
    if (rel->rd_rsdesc == NULL)
    {
        return true;
    }

    /* Get the policies for the specified command type */
    user_id = GetUserId();
    get_policies_for_relation(rel, cmd, user_id,
                              &permissive_policies,
                              &restrictive_policies);

    /* Build security quals from the policies (use rt_index=1) */
    add_security_quals(1, permissive_policies, restrictive_policies,
                       &securityQuals, &hasSubLinks);

    /* If no quals, tuple passes */
    if (securityQuals == NIL)
    {
        return true;
    }

    /* Create minimal execution environment */
    estate = CreateExecutorState();
    econtext = CreateExprContext(estate);

    /* Create tuple slot and store the tuple */
    slot = MakeSingleTupleTableSlot(RelationGetDescr(rel), &TTSOpsHeapTuple);
    ExecStoreHeapTuple(tuple, slot, false);
    econtext->ecxt_scantuple = slot;

    /* Compile and evaluate each qual */
    foreach(lc, securityQuals)
    {
        Expr *qual = (Expr *) lfirst(lc);
        ExprState *qualExpr;
        List *qualList;

        /* ExecPrepareQual expects a List */
        if (!IsA(qual, List))
        {
            qualList = list_make1(qual);
        }
        else
        {
            qualList = (List *) qual;
        }

        /* Use ExecPrepareQual for standalone expression evaluation */
        qualExpr = ExecPrepareQual(qualList, estate);

        if (!ExecQual(qualExpr, econtext))
        {
            result = false;
            break;
        }
    }

    /* Clean up */
    ExecDropSingleTupleTableSlot(slot);
    FreeExprContext(econtext, true);
    FreeExecutorState(estate);

    return result;
}
