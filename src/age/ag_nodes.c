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

#include "pool.h"

#include "parser/extensible.h"
#include "utils/palloc.h"

#include "age/ag_nodes.h"
#include "age/cypher_nodes.h"

static bool equal_ag_node(const ExtensibleNode *a, const ExtensibleNode *b);

// This list must match ag_node_tag.
const char *node_names[] = {
    "ag_node_invalid",
    "cypher_return",
    "cypher_with",
    "cypher_match",
    "cypher_create",
    "cypher_set",
    "cypher_set_item",
    "cypher_delete",
    "cypher_unwind",
    "cypher_merge",
    "cypher_path",
    "cypher_node",
    "cypher_relationship",
    "cypher_bool_const",
    "cypher_param",
    "cypher_map",
    "cypher_list",
    "cypher_string_match",
    "cypher_typecast",
    "cypher_integer_const",
    "cypher_sub_pattern",
    "cypher_create_target_nodes",
    "cypher_create_path",
    "cypher_target_node",
    "cypher_update_information",
    "cypher_update_item",
    "cypher_delete_information",
    "cypher_delete_item",
    "cypher_merge_information"
};


ExtensibleNode *_new_ag_node(Size size, ag_node_tag tag)
{
    ExtensibleNode *n;

    n = (ExtensibleNode *)palloc0fast(size);
    n->type = T_ExtensibleNode;
    n->extnodename = node_names[tag];

    return n;
}
