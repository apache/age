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

#ifndef AG_LOAD_H
#define AG_LOAD_H

#include "access/heapam.h"
#include "commands/sequence.h"
#include "utils/builtins.h"
#include "utils/lsyscache.h"

#include "catalog/ag_graph.h"
#include "catalog/ag_label.h"
#include "commands/label_commands.h"
#include "commands/graph_commands.h"
#include "utils/ag_cache.h"

#define BATCH_SIZE 1000
#define MAX_BUFFERED_BYTES 65535  /* 64KB, same as pg COPY */

typedef struct batch_insert_state
{
    EState *estate;
    ResultRelInfo *resultRelInfo;
    TupleTableSlot **slots;
    int num_tuples;
    size_t buffered_bytes;
    BulkInsertState bistate;
} batch_insert_state;

agtype *create_empty_agtype(void);
agtype *create_agtype_from_list(char **header, char **fields,
                                size_t fields_len, int64 vertex_id,
                                bool load_as_agtype);
agtype *create_agtype_from_list_i(char **header, char **fields,
                                  size_t fields_len, size_t start_index,
                                  bool load_as_agtype);

void insert_vertex_simple(Oid graph_oid, char *label_name, graphid vertex_id,
                          agtype *vertex_properties);
void insert_edge_simple(Oid graph_oid, char *label_name, graphid edge_id,
                        graphid start_id, graphid end_id,
                        agtype *edge_properties);

void init_batch_insert(batch_insert_state **batch_state,
                       char *label_name, Oid graph_oid);
void insert_batch(batch_insert_state *batch_state);
void finish_batch_insert(batch_insert_state **batch_state);

char *trim_whitespace(const char *str);

#endif /* AG_LOAD_H */
