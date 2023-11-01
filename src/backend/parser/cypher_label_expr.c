#include "catalog/ag_label.h"
#include "commands/label_commands.h"
#include "nodes/cypher_nodes.h"
#include "utils/ag_cache.h"

#include "parser/cypher_label_expr.h"

/*
 * Returns the first label in label_expr that exists in the cache and is not
 * of type label_kind.
 *
 * This is a helper function to check if all labels in label_expr are valid in
 * kind, during node\edge creation.
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

// TODO: can be moved to utils?
/*
 * List comparator for String nodes. The ListCells a and b must
 * contain node pointer of type T_String.
 */
int list_string_cmp(const ListCell *a, const ListCell *b)
{
    Node *na = lfirst(a);
    Node *nb = lfirst(b);

    Assert(IsA(na, String));
    Assert(IsA(nb, String));

    return strcmp(strVal(na), strVal(nb));
}

/*
 * Generates table name for label_expr. It does not check if a table exists
 * for that name. The caller must do existence check before using the table.
 * This function is not applicable for LABEL_EXPR_TYPE_OR.
 */
char *label_expr_table_name(cypher_label_expr *label_expr,
                            char label_expr_kind)
{
    switch (label_expr->type)
    {
    case LABEL_EXPR_TYPE_EMPTY:
        return label_expr_kind == LABEL_KIND_VERTEX ? AG_DEFAULT_LABEL_VERTEX :
                                                      AG_DEFAULT_LABEL_EDGE;

    case LABEL_EXPR_TYPE_SINGLE:
        return (char *)strVal(linitial(label_expr->label_names));

    case LABEL_EXPR_TYPE_AND:
        // TODO: implement
        ereport(ERROR,
                (errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
                 errmsg("label expression type AND is not implemented")));
        return NULL;

    case LABEL_EXPR_TYPE_OR:
        // TODO: implement
        elog(ERROR, "label expression type OR cannot have a table");
        return NULL;

    default:
        elog(ERROR, "invalid cypher_label_expr type");
        return NULL;
    }
}

// TODO: whenever a new field is added, update this one.
bool label_expr_are_equal(cypher_label_expr *le1, cypher_label_expr *le2)
{
    ListCell *lc1;
    ListCell *lc2;

    if (le1 == le2)
    {
        return true;
    }

    /**
     * If exactly one is null, return false.
     * TODO: should null be allowed?
     */
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
 * Returns true if there is at least one table to be scanned for this
 * label_expr.
 *
 * This helper function is used in the MATCH clause transformation.
 */
bool label_expr_has_tables(cypher_label_expr *label_expr, char label_expr_kind,
                           Oid graph_oid)
{
    char *table_name;
    label_cache_data *lcd;
    ListCell *lc;

    switch (label_expr->type)
    {
    case LABEL_EXPR_TYPE_EMPTY:
        return true;

    case LABEL_EXPR_TYPE_SINGLE:
    case LABEL_EXPR_TYPE_AND:
        table_name = label_expr_table_name(label_expr, label_expr_kind);
        lcd = search_label_name_graph_cache(table_name, graph_oid);
        return (lcd != NULL && lcd->kind == label_expr_kind);

    case LABEL_EXPR_TYPE_OR:
        foreach (lc, label_expr->label_names)
        {
            table_name = strVal(lfirst(lc));
            lcd = search_label_name_graph_cache(table_name, graph_oid);

            if  (lcd != NULL && lcd->kind == label_expr_kind)
            {
                return true;
            }
        }
        return false;

    default:
        elog(ERROR, "invalid cypher_label_expr type");
        return false;
    }
}
