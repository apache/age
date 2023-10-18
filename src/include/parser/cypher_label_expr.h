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

#ifndef AG_CYPHER_LABEL_EXPR_H
#define AG_CYPHER_LABEL_EXPR_H

#include "nodes/cypher_nodes.h"
#include "utils/ag_cache.h"
#include "catalog/ag_label.h"
#include "commands/label_commands.h"

#define FIRST_LABEL_NAME(label_expr) \
    (list_length((label_expr)->label_names) == 0) \
    ? NULL : linitial((label_expr)->label_names)

// TODO: more readable name LABEL_EXPR_HAS_LABEL returning the oppposite?
#define LABEL_EXPR_LENGTH(label_expr) (list_length((label_expr)->label_names))
#define LABEL_EXPR_IS_EMPTY(label_expr) (LABEL_EXPR_LENGTH((label_expr)) == 0)
#define LABEL_EXPR_LABEL_NAMES(label_expr) (list_copy_deep((label_expr)->label_names))
//((label_expr)->label_names)

// TODO: move to C
// TODO: expensive and redundant to call multiple times
/**
 * Returns if each label_name in label_expr->label_names is of kind
 * `label_kind`, if it exists.
 *
 * If label_expr is empty, returns true.
 */
bool validate_label_expr_kind(cypher_label_expr *label_expr, uint32 graph_oid, char label_kind)
{
    ListCell *lc;

    foreach (lc, label_expr->label_names)
    {
        char *label_name;
        label_cache_data *cache_data;

        label_name = strVal(lfirst(lc));
        cache_data = search_label_name_graph_cache(label_name, graph_oid);

        if (cache_data && cache_data->kind != label_kind)
        {
            return false;
        }
    }

    return true;
}

// TODO: can be moved to utils?
int string_list_comparator(const ListCell *a, const ListCell *b)
{
    char *str_a = lfirst(a);
    char *str_b = lfirst(b);
    return strcmp(str_a, str_b);
}

char *get_cluster_name(cypher_label_expr *label_expr)
{
    ListCell *lc;
    bool is_first;

    // TODO: or, defalt, single, and can be a switch?

    /* or */
    Assert(label_expr->type != LABEL_EXPR_TYPE_OR);

    /* default */
    if (LABEL_EXPR_IS_EMPTY(label_expr))
    {
        return label_expr->kind == LABEL_KIND_VERTEX ?
            AG_DEFAULT_LABEL_VERTEX : AG_DEFAULT_LABEL_EDGE;
    }

    /* single */
    if (LABEL_EXPR_LENGTH(label_expr) == 1)
    {
        // TODO: when multi-label implemented, it will be different
        return (char *)strVal(linitial(label_expr->label_names));
    }

    /* and */
    // TODO: implement

    // TODO: can be done during early parsing?
    // list_sort(label_expr->label_names, &string_list_comparator);

    return NULL;
}

#endif
