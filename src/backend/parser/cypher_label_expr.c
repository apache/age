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

#include "access/genam.h"
#include "access/heapam.h"
#include "catalog/indexing.h"
#include "utils/builtins.h"
#include "utils/inval.h"
#include "utils/lsyscache.h"

#include "catalog/ag_label.h"
#include "commands/label_commands.h"
#include "nodes/cypher_nodes.h"
#include "parser/cypher_label_expr.h"
#include "parser/cypher_transform_entity.h"
#include "utils/ag_cache.h"
#include "utils/array.h"
#include "utils/name_validation.h"

static void append_to_allrelations(Relation ag_label, char *label_name,
                                   char *intr_relname, Oid graphoid);

/*
 * Creates ag_label entries and relations required for the given `label_expr`
 * if does not exists. Returns the RangeVar of ag_label.relation.
 *
 * The label_expr may represent empty, single or multiple labels. For empty
 * (default labels) and single label, a relation is created and an ag_label
 * entry is added for that label. In the ag_label entry, the newly created
 * relation is pointed by the ag_label.relation column. This is the primary
 * relation for that label. All entities that has exactly that one label (not
 * any other labels), are stored in that label's ag_label.relation. This
 * concept is not new, but an additional concept is needed for multiple labels,
 * which is explained below.
 *
 * For multiple labels, only label expression type AND is supported in the
 * context of create and merge. For example, CREATE (:a:b). The expression :a:b
 * represents intersection of the label a and b. To avoid duplicating an entity
 * into both a and b's relation, it will be stored in an 'intersection
 * relation.' So, an intersection relation is created for :a:b and an ag_label
 * entry is added. Although an intersection relation is not a label, it is
 * treated like a label. The reason is explained later below. Next, for each
 * individual labels (i.e. a and b), their relations and ag_label entries are
 * created if does not exist already. Then, the intersection relation is
 * appended to each individual label's ag_label.allrelations column. The
 * allrelation column of a label represents all relations containing entities
 * that has at least that label among other labels. This column serves its
 * purpose for the MATCH clause. See the get_label_expr_relations function.
 *
 * Now, the reason for treating an intersection relation as a label is due to
 * the fact that an entity ID has its label ID encoded in it. So, entities with
 * multiple labels need to have multiple label ID encoded in the ID. The other
 * option is just encoding its intersection relation's label id (since it is
 * being treated as a label, it has a label ID). The latter option is easier to
 * implement.
 */
RangeVar *create_label_expr_relations(Oid graphoid, char *graphname,
                                      cypher_label_expr *label_expr,
                                      char label_expr_kind)
{
    List *parents;
    char *ag_label_relation;
    char rel_kind;
    cypher_label_expr_type label_expr_type;
    RangeVar *rv;
    ListCell *label_lc;

    label_expr_type = LABEL_EXPR_TYPE(label_expr);

    Assert(label_expr_type != LABEL_EXPR_TYPE_OR);

    /* set default labels as parent unless it is the default label itself */
    if (label_expr_type == LABEL_EXPR_TYPE_EMPTY)
    {
        parents = NIL;
        rel_kind = LABEL_REL_KIND_DEFAULT;
    }
    else
    {
        char *parent_label_name;
        RangeVar *parent_rv;

        rel_kind = label_expr_type == LABEL_EXPR_TYPE_SINGLE ?
                       LABEL_REL_KIND_SINGLE :
                       LABEL_REL_KIND_INTR;

        parent_label_name = label_expr_kind == LABEL_KIND_VERTEX ?
                                AG_DEFAULT_LABEL_VERTEX :
                                AG_DEFAULT_LABEL_EDGE;
        parent_rv = get_label_range_var(graphname, graphoid, parent_label_name);
        parents = list_make1(parent_rv);
    }

    /*
     * Content for the column: ag_label.relation. For empty and single labels,
     * this is a primary relation. For multiple labels (AND expr), this is the
     * intersection relation name. Note that, although intersection relations
     * are not labels, they have ag_label entries where the relation column
     * holds the intersection relation name.
     */
    ag_label_relation = label_expr_relname(label_expr, label_expr_kind);
    rv = makeRangeVar(graphname, ag_label_relation, -1);

    /*
     * If the relation exists, it can be assumed that all required ag_label
     * entries and relations also exists.
     */
    if (ag_relation_exists(ag_label_relation, graphoid))
    {
        return rv;
    }

    /* Verify if label names are valid */
    foreach (label_lc, label_expr->label_names)
    {
        char *label_name;

        label_name = strVal(lfirst(label_lc));

        if (!is_valid_label_name(label_name, label_expr_kind))
        {
            ereport(ERROR, (errcode(ERRCODE_UNDEFINED_SCHEMA),
                            errmsg("label name is invalid")));
        }
    }

    /*
     * Creates ag_label entry and relation.
     *
     * In case of LABEL_EXPR_TYPE_AND, ag_label_relation will be an invalid
     * label name due to the separator (see label_expr_relname()). For this
     * reason, check_valid_label parameter is set to false. Instead,
     * validity of individual label names are verified here.
     */
    create_label(graphname, ag_label_relation, label_expr_kind, rel_kind,
                 parents, false);

    /*
     * For multiple labels (AND expression), processes each individual labels
     * as described above.
     */
    if (label_expr_type == LABEL_EXPR_TYPE_AND)
    {
        ListCell *lc;
        Relation ag_label;

        Assert(list_length(label_expr->label_names) > 1);

        ag_label = table_open(ag_label_relation_id(), RowExclusiveLock);

        foreach (lc, label_expr->label_names)
        {
            char *label_name;

            label_name = strVal(lfirst(lc));

            if (!label_exists(label_name, graphoid))
            {
                create_label(graphname, label_name, label_expr_kind,
                             LABEL_REL_KIND_SINGLE, parents, false);
            }

            /*
             * appends intersection relation name the individual label's
             * allrelations column
             */
            append_to_allrelations(ag_label, label_name, ag_label_relation,
                                   graphoid);
        }
        table_close(ag_label, RowExclusiveLock);

        /* signals to rebuild the ag_label cache */
        CacheInvalidateRelcacheAll();
        CommandCounterIncrement();
    }

    return rv;
}

/*
 * Helper function for `create_label_expr_relations`. Appends intersection
 * relation name to label_name's ag_label.allrelations column.
 *
 * `ag_label` must an opened table.
 *
 * UPDATE ag_label
 * SET ag_label.allrelations = ag_label.allrelations || `intr_relname`
 * WHERE name = `label_name`;
 */
static void append_to_allrelations(Relation ag_label, char *label_name,
                                   char *intr_relname, Oid graphoid)
{
    SysScanDesc ag_label_scan;
    Name label_name_data;
    ScanKey skeys;
    TupleDesc ag_label_tupdesc;
    HeapTuple ag_label_tup;
    bool isNull;
    Datum intr_relid;
    Datum lhs_arraycat;
    Datum rhs_arraycat;
    Datum res_arraycat;
    HeapTuple new_ag_label_tup;
    int replCols;
    bool replIsNull;

    /* gets ag_label tuple for name=label_name */
    ag_label_tupdesc = RelationGetDescr(ag_label);
    label_name_data = (NameData *)palloc(sizeof(NameData));
    namestrcpy(label_name_data, label_name);

    skeys = (ScanKeyData *)palloc(sizeof(ScanKeyData) * 2);
    ScanKeyInit(&skeys[0], Anum_ag_label_name, BTEqualStrategyNumber, F_NAMEEQ,
                NameGetDatum(label_name_data));
    ScanKeyInit(&skeys[1], Anum_ag_label_graph, BTEqualStrategyNumber,
                F_INT4EQ, ObjectIdGetDatum(graphoid));

    ag_label_scan = systable_beginscan(
        ag_label, ag_label_name_graph_index_id(), true, NULL, 2, skeys);
    ag_label_tup = systable_getnext(ag_label_scan);
    Assert(HeapTupleIsValid(ag_label_tup));

    /* creates new datum for the allrelations column */
    lhs_arraycat = heap_getattr(ag_label_tup, Anum_ag_label_allrelations,
                                ag_label_tupdesc, &isNull);
    Assert(!isNull);
    intr_relid = ObjectIdGetDatum(get_relname_relid(intr_relname, graphoid));
    rhs_arraycat = PointerGetDatum(
        construct_array(&intr_relid, 1, REGCLASSOID, 4, true, TYPALIGN_INT));
    res_arraycat = DirectFunctionCall2(array_cat, lhs_arraycat, rhs_arraycat);

    /* update tuple */
    replCols = Anum_ag_label_allrelations;
    replIsNull = false;
    new_ag_label_tup =
        heap_modify_tuple_by_cols(ag_label_tup, ag_label_tupdesc, 1, &replCols,
                                  &res_arraycat, &replIsNull);
    CatalogTupleUpdate(ag_label, &ag_label_tup->t_data->t_ctid,
                       new_ag_label_tup);
    CommandCounterIncrement();
    systable_endscan(ag_label_scan);
}

/*
 * Returns a list of relations (as OIDs) that are MATCHed by the given
 * `label_expr`. It may return empty list (NIL) in case no relations to scan.
 *
 * MATCH's transformation function uses the returned list of relations to build
 * a chain of union-all subquery. The subquery contains the entities
 * represented by the given label_expr.
 *
 * The list of relations is built from ag_label.allrelations column (accessed
 * via cache). This column represents all relations that belong to a label. For
 * each label in the label_expr, the contents of allrelations column are
 * combined depending on the type of label_expr.
 */
List *get_label_expr_relations(cypher_label_expr *label_expr,
                               char label_expr_kind, Oid graph_oid)
{
    cypher_label_expr_type label_expr_type = LABEL_EXPR_TYPE(label_expr);
    char *label_name;
    label_cache_data *lcd;

    switch (label_expr_type)
    {
    case LABEL_EXPR_TYPE_EMPTY:
        label_name = label_expr_kind == LABEL_KIND_VERTEX ?
                               AG_DEFAULT_LABEL_VERTEX :
                               AG_DEFAULT_LABEL_EDGE;
        lcd = search_label_name_graph_cache(label_name, graph_oid);
        Assert(lcd);

        return list_copy(lcd->allrelations);

    case LABEL_EXPR_TYPE_SINGLE:
        label_name = strVal(linitial(label_expr->label_names));
        lcd = search_label_name_graph_cache(label_name, graph_oid);

        if (!lcd || lcd->kind != label_expr_kind)
        {
            return NIL;
        }

        return list_copy(lcd->allrelations);

    case LABEL_EXPR_TYPE_AND:
    case LABEL_EXPR_TYPE_OR:
    {
        List *reloids;
        List *(*merge_lists)(List *, const List *);
        ListCell *lc;

        reloids = NIL;

        /*
         * This function pointer describes how to combine two lists of relation
         * oids.
         *
         * For AND, uses intersection.
         *      MATCH (:A:B) -> intersection of allrelations of A and B.
         *      To scan only common relations of A and B.
         *
         * For OR, uses concat (similar to union).
         *      MATCH (:A|B) -> union of allrelations of A and B.
         *      To scan all relations of A and B.
         */
        merge_lists = label_expr_type == LABEL_EXPR_TYPE_AND ?
                          &list_intersection_oid :
                          &list_concat_unique_oid;

        foreach (lc, label_expr->label_names)
        {
            label_name = strVal(lfirst(lc));
            lcd = search_label_name_graph_cache(label_name, graph_oid);

            /* if some label_name does not exist in cache */
            if (!lcd || lcd->kind != label_expr_kind)
            {
                if (label_expr_type == LABEL_EXPR_TYPE_AND)
                {
                    /* for AND, no relation to scan */
                    return NIL;
                }
                else
                {
                    /* for OR, skip that label */
                    continue;
                }
            }

            /* if first iteration */
            if (list_length(reloids) == 0)
            {
                reloids = list_copy(lcd->allrelations);
                continue;
            }
            else
            {
                /*
                 * "Merges" lcd->allrelations into reloids.
                 *
                 * At the end of the loop, reloids is a result of all labels'
                 * allrelations merged together using the merge_list funciton.
                 * For example, for OR, reloids is a result of a chain of
                 * union.
                 */
                reloids = merge_lists(reloids, lcd->allrelations);
            }
        }
        return reloids;
    }
    default:
        elog(ERROR, "invalid cypher_label_expr type");
        return NIL;
    }
}

/*
 * Returns the first label in label_expr that exists but not the same kind as
 * `label_expr_kind`.
 *
 * This is a helper function to check if all labels in label_expr are valid in
 * kind during node\edge creation.
 */
char *find_first_invalid_label(cypher_label_expr *label_expr,
                               char label_expr_kind, Oid graph_oid)
{
    ListCell *lc;

    foreach (lc, label_expr->label_names)
    {
        char *label_name;
        label_cache_data *cache_data;

        label_name = strVal(lfirst(lc));
        cache_data = search_label_name_graph_cache(label_name, graph_oid);

        if (cache_data && cache_data->kind != label_expr_kind)
        {
            return label_name;
        }
    }

    return NULL;
}

/*
 * Generates relation name for label_expr. It does not check if the relation
 * exists for that name. The caller must do existence check before using the
 * table. This function is not applicable for LABEL_EXPR_TYPE_OR.
 */
char *label_expr_relname(cypher_label_expr *label_expr, char label_expr_kind)
{
    switch (label_expr->type)
    {
    case LABEL_EXPR_TYPE_EMPTY:
        /* returns default label names */
        return label_expr_kind == LABEL_KIND_VERTEX ? AG_DEFAULT_LABEL_VERTEX :
                                                      AG_DEFAULT_LABEL_EDGE;

    case LABEL_EXPR_TYPE_SINGLE:
        /* returns the first label's name */
        Assert(list_length(label_expr->label_names) == 1);
        return (char *)strVal(linitial(label_expr->label_names));

    case LABEL_EXPR_TYPE_AND:
    {
        /*
         * generates a name for intersection relation
         * i.e. for CREATE (:A:B:C), _agr_ABC
         */
        StringInfo relname_strinfo;
        ListCell *lc;
        char *relname;

        Assert(list_length(label_expr->label_names) > 1);

        relname_strinfo = makeStringInfo();
        appendStringInfoString(relname_strinfo, INTR_REL_PREFIX);

        foreach (lc, label_expr->label_names)
        {
            char *label_name = strVal(lfirst(lc));
            appendStringInfoString(relname_strinfo, label_name);

            if (lnext(label_expr->label_names, lc))
            {
                appendStringInfoChar(relname_strinfo, INTR_REL_SEPERATOR);
            }
        }

        relname = relname_strinfo->data;
        pfree(relname_strinfo);

        return relname;
    }

    case LABEL_EXPR_TYPE_OR:
        elog(ERROR, "label expression type OR cannot have a table");
        return NULL;

    default:
        elog(ERROR, "invalid cypher_label_expr type");
        return NULL;
    }
}

/*
 * Returns if two label expressions are equal.
 */
bool label_expr_are_equal(cypher_label_expr *le1, cypher_label_expr *le2)
{
    ListCell *lc1;
    ListCell *lc2;

    if (le1 == le2)
    {
        return true;
    }

    if ((le1 == NULL && le2 != NULL) || (le2 == NULL && le1 != NULL))
    {
        return false;
    }

    if (le1->type != le2->type)
    {
        return false;
    }

    if (LABEL_EXPR_LENGTH(le1) != LABEL_EXPR_LENGTH(le2))
    {
        return false;
    }

    /*
     * Assuming both lists are sorted and have same length.
     */
    forboth(lc1, le1->label_names, lc2, le2->label_names)
    {
        char *le1_label = strVal(lfirst(lc1));
        char *le2_label = strVal(lfirst(lc2));

        if (strcmp(le1_label, le2_label) != 0)
        {
            return false;
        }
    }

    return true;
}

/*
 * Returns label IDs of labels contained by the label expressoin in arbitrary
 * order. This is used by the MATCH clause to build filter quals.
 *
 * Note: Default labels are not considered labels by this function. So, for
 * empty label expression, empty list is returned. Also, for non-empty types,
 * empty list means the filter should not match any entities at all. For
 * example, when no labels are valid in the expression. The caller is
 * responsible for distinguishing between the two cases.
 */
List *label_expr_label_ids_for_filter(cypher_label_expr *label_expr,
                                      char label_expr_kind, Oid graph_oid)
{
    cypher_label_expr_type label_expr_type;
    List *result;
    ListCell *lc;

    label_expr_type = LABEL_EXPR_TYPE(label_expr);
    result = NIL;

    if (label_expr_type == LABEL_EXPR_TYPE_EMPTY)
    {
        return NIL;
    }

    foreach (lc, label_expr->label_names)
    {
        char *label_name;
        label_cache_data *lcd;

        label_name = strVal((String *)lfirst(lc));
        lcd = search_label_name_graph_cache(label_name, graph_oid);

        /* if a label_name does not exist in the cache */
        if (!lcd || lcd->kind != label_expr_kind)
        {
            if (label_expr_type == LABEL_EXPR_TYPE_AND)
            {
                /* for AND, nothing to match */
                return NIL;
            }
            else
            {
                /* for OR\Single, skip that label */
                continue;
            }
        }
        else
        {
            result = lappend_int(result, lcd->id);
        }
    }

    return result;
}

/*
 * Checks unsupported label expression type in CREATE\MERGE clause and reports
 * error.
 *
 * This is used in CREATE and MERGE's transformation.
 */
void check_label_expr_type_for_create(ParseState *pstate, Node *entity)
{
    cypher_label_expr *label_expr;
    int location;
    char label_expr_kind;
    char *variable_name;

    if (is_ag_node(entity, cypher_node))
    {
        cypher_node *node = (cypher_node *)entity;
        label_expr = node->label_expr;
        location = node->location;
        label_expr_kind = LABEL_KIND_VERTEX;
        variable_name = node->name;
    }
    else if (is_ag_node(entity, cypher_relationship))
    {
        cypher_relationship *rel = (cypher_relationship *)entity;
        label_expr = rel->label_expr;
        location = rel->location;
        label_expr_kind = LABEL_KIND_EDGE;
        variable_name = rel->name;
    }
    else
    {
        ereport(ERROR,
                (errmsg_internal("unrecognized node in create pattern")));
    }

    /*
     * If the entity has a variable which was declared in a previous clause,
     * then skip the check since the check has been done already.
     */
    if (variable_name &&
        find_variable((cypher_parsestate *)pstate, variable_name))
    {
        return;
    }

    /*
     * In a CREATE\MERGE clause, following types are allowed-
     *      for vertices: empty, single, and
     *      for edges   : single
     */
    if (LABEL_EXPR_TYPE(label_expr) == LABEL_EXPR_TYPE_OR)
    {
        ereport(
            ERROR,
            (errcode(ERRCODE_SYNTAX_ERROR),
             errmsg(
                 "label expression type OR is not allowed in a CREATE\\MERGE clause"),
             parser_errposition(pstate, location)));
    }

    if (label_expr_kind == LABEL_KIND_EDGE &&
        LABEL_EXPR_TYPE(label_expr) != LABEL_EXPR_TYPE_SINGLE)
    {
        ereport(
            ERROR,
            (errcode(ERRCODE_SYNTAX_ERROR),
             errmsg(
                 "relationships must have exactly one label in a CREATE\\MERGE clause"),
             parser_errposition(pstate, location)));
    }

    return;
}

/*
 * Reports an error if the given `label_name` or part of it is reserved.
 */
void check_reserved_label_name(char *label_name)
{
    /* prevents label names with same prefix as INTR_REL_PREFIX */
    if (strncmp(label_name, INTR_REL_PREFIX, INTR_REL_PREFIX_LEN) == 0)
    {
        ereport(
            ERROR,
            (errcode(ERRCODE_RESERVED_NAME),
             errmsg("label names cannot start with the reserved word: \"%s\"",
                    INTR_REL_PREFIX)));
    }

    return;
}

/*
 * This code is borrowed from Postgres' list_intersection for pointers since it
 * does not implement an intersection function for OIDs. Use of non-public
 * functions and macros are commented out.
 */
List *list_intersection_oid(List *list1, const List *list2)
{
    List *result;
    const ListCell *cell;

    if (list1 == NIL || list2 == NIL)
    {
        return NIL;
    }

    // Assert(IsOidList(list1));
    // Assert(IsOidList(list2));

    result = NIL;
    foreach (cell, list1)
    {
        if (list_member_oid(list2, lfirst_oid(cell)))
        {
            result = lappend_oid(result, lfirst_oid(cell));
        }
    }

    // check_list_invariants(result);
    return result;
}

/*
 * List comparator for String nodes. The ListCells a and b must
 * contain Node pointer of type T_String.
 */
int list_string_cmp(const ListCell *a, const ListCell *b)
{
    Node *na = lfirst(a);
    Node *nb = lfirst(b);

    Assert(IsA(na, String));
    Assert(IsA(nb, String));

    return strcmp(strVal(na), strVal(nb));
}
