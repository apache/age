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

#ifndef AG_LOAD_EDGES_H
#define AG_LOAD_EDGES_H

#include "utils/load/age_load.h"

/*
 * Load edges from a CSV file using pg's COPY infrastructure.
 *
 * CSV format: start_id, start_vertex_type, end_id, end_vertex_type, [properties...]
 *
 * Parameters:
 *   file_path       - Path to the CSV file (must be in /tmp/age/)
 *   graph_name      - Name of the graph
 *   graph_oid       - OID of the graph
 *   label_name      - Name of the edge label
 *   label_id        - ID of the label
 *   load_as_agtype  - If true, parse CSV values as agtype (JSON-like)
 *
 * Returns EXIT_SUCCESS on success.
 */
int create_edges_from_csv_file(char *file_path, char *graph_name, Oid graph_oid,
                               char *label_name, int label_id,
                               bool load_as_agtype);

#endif /* AG_LOAD_EDGES_H */
