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

#include "postgres.h"
#include "nodes/cypher_nodes.h"

/*
 * Functions and macros associated with the cypher node: cypher_label_expr
 * defined in cypher_nodes.h.
 *
 * Most functions that takes cypher_label_expr as an argument also takes a
 * char type argument `label_expr_kind`. It tells whether the label_expr
 * belongs to a cypher_node or a cypher_relationship. It does not say anything
 * about the actual type of any label in the ag_label catalog.
 */

/* Prefix for intersection relations. */
#define INTR_REL_PREFIX "_agr_"
#define INTR_REL_PREFIX_LEN 5
#define INTR_REL_SEPERATOR '-'

#define LABEL_EXPR_LENGTH(label_expr) (list_length((label_expr)->label_names))

#define LABEL_EXPR_TYPE(label_expr) \
    ((label_expr) ? (label_expr)->type : LABEL_EXPR_TYPE_EMPTY)

#define LABEL_EXPR_IS_EMPTY(label_expr) \
    (LABEL_EXPR_TYPE((label_expr)) == LABEL_EXPR_TYPE_EMPTY)

#define LABEL_EXPR_HAS_RELATIONS(label_expr, label_expr_kind, graph_oid) \
    (list_length(get_label_expr_relations((label_expr), (label_expr_kind), \
                                          (graph_oid))) > 0)

RangeVar *create_label_expr_relations(Oid graphoid, char *graphname,
                                      cypher_label_expr *label_expr,
                                      char label_expr_kind);
List *get_label_expr_relations(cypher_label_expr *label_expr,
                               char label_expr_kind, Oid graph_oid);

char *find_first_invalid_label(cypher_label_expr *label_expr,
                               char label_expr_kind, Oid graph_oid);
char *label_expr_relname(cypher_label_expr *label_expr, char label_expr_kind);
bool label_expr_are_equal(cypher_label_expr *le1, cypher_label_expr *le2);
List *label_expr_label_ids_for_filter(cypher_label_expr *label_expr,
                                      char label_expr_kind, Oid graph_oid);
void check_label_expr_type_for_create(ParseState *pstate, Node *entity);
void check_reserved_label_name(char *label_name);

List *list_intersection_oid(List *list1, const List *list2);
int list_string_cmp(const ListCell *a, const ListCell *b);

#endif
