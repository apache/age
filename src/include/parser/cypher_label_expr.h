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

#define FIRST_LABEL_NAME(label_expr) \
    (list_length((label_expr)->label_names) == 0) ? \
        NULL : \
        linitial((label_expr)->label_names)

// TODO: more readable name LABEL_EXPR_HAS_LABEL returning the oppposite?
#define LABEL_EXPR_LENGTH(label_expr) (list_length((label_expr)->label_names))
#define LABEL_EXPR_IS_EMPTY(label_expr) \
    (LABEL_EXPR_TYPE((label_expr)) == LABEL_EXPR_TYPE_EMPTY) // TODO: maybe redundant to LABEL_EXPR_TYPE
#define LABEL_EXPR_TYPE(label_expr) \
    ((label_expr) ? (label_expr)->type : LABEL_EXPR_TYPE_EMPTY)

char *find_first_invalid_label(cypher_label_expr *label_expr,
                               char label_expr_kind, Oid graph_oid);
int list_string_cmp(const ListCell *a, const ListCell *b);
char *label_expr_table_name(cypher_label_expr *label_expr, char label_expr_kind);
bool label_expr_are_equal(cypher_label_expr *le1, cypher_label_expr *le2);
bool label_expr_has_tables(cypher_label_expr *label_expr, char label_expr_kind,
                           Oid graph_oid);

#endif
