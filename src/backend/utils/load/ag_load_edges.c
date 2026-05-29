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

#include "access/heapam.h"
#include "access/table.h"
#include "catalog/namespace.h"
#include "commands/copy.h"
#include "executor/executor.h"
#include "nodes/makefuncs.h"
#include "parser/parse_node.h"
#include "utils/memutils.h"
#include "utils/rel.h"

#include "utils/load/ag_load_edges.h"
#include "utils/ag_cache.h"

typedef struct vertex_label_id_cache_entry
{
    NameData name;
    int32 id;
} vertex_label_id_cache_entry;

typedef struct vertex_label_id_cache
{
    HTAB *entries;
    NameData last_name;
    int32 last_id;
    bool last_valid;
} vertex_label_id_cache;

static vertex_label_id_cache *create_vertex_label_id_cache(void);
static void destroy_vertex_label_id_cache(vertex_label_id_cache *cache);
static int32 get_cached_vertex_label_id(vertex_label_id_cache *cache,
                                        const char *label_name,
                                        Oid graph_oid);
static void trim_field_to_name(Name name, const char *str);

/*
 * Process a single edge row from COPY's raw fields.
 * Edge CSV format: start_id, start_vertex_type, end_id, end_vertex_type, [properties...]
 */
static void process_edge_row(char **fields, int nfields,
                             char **header, int header_count,
                             int label_id, Oid label_seq_relid,
                             Oid graph_oid, bool load_as_agtype,
                             batch_insert_state *batch_state,
                             vertex_label_id_cache *vertex_label_id_cache)
{
    int64 start_id_int;
    graphid start_vertex_graph_id;
    int start_vertex_type_id;

    int64 end_id_int;
    graphid end_vertex_graph_id;
    int end_vertex_type_id;

    graphid edge_id;
    int64 entry_id;
    TupleTableSlot *slot;

    NameData start_vertex_type;
    NameData end_vertex_type;
    agtype *edge_properties;

    /* Generate edge ID */
    entry_id = nextval_internal(label_seq_relid, true);
    edge_id = make_graphid(label_id, entry_id);

    /* Trim whitespace from vertex type names */
    trim_field_to_name(&start_vertex_type, fields[1]);
    trim_field_to_name(&end_vertex_type, fields[3]);

    /* Parse start vertex info */
    start_id_int = strtol(fields[0], NULL, 10);
    start_vertex_type_id = get_cached_vertex_label_id(vertex_label_id_cache,
                                                      NameStr(start_vertex_type),
                                                      graph_oid);

    /* Parse end vertex info */
    end_id_int = strtol(fields[2], NULL, 10);
    end_vertex_type_id = get_cached_vertex_label_id(vertex_label_id_cache,
                                                    NameStr(end_vertex_type),
                                                    graph_oid);

    /* Create graphids for start and end vertices */
    start_vertex_graph_id = make_graphid(start_vertex_type_id, start_id_int);
    end_vertex_graph_id = make_graphid(end_vertex_type_id, end_id_int);

    /* Get the appropriate slot from the batch state */
    slot = batch_state->slots[batch_state->num_tuples];

    /* Clear the slots contents */
    ExecClearTuple(slot);

    /* Build the agtype properties */
    edge_properties = create_agtype_from_list_i(header, fields,
                                                nfields, 4, load_as_agtype);

    /* Fill the values in the slot */
    slot->tts_values[0] = GRAPHID_GET_DATUM(edge_id);
    slot->tts_values[1] = GRAPHID_GET_DATUM(start_vertex_graph_id);
    slot->tts_values[2] = GRAPHID_GET_DATUM(end_vertex_graph_id);
    slot->tts_values[3] = AGTYPE_P_GET_DATUM(edge_properties);
    slot->tts_isnull[0] = false;
    slot->tts_isnull[1] = false;
    slot->tts_isnull[2] = false;
    slot->tts_isnull[3] = false;

    /* Make the slot as containing virtual tuple */
    ExecStoreVirtualTuple(slot);

    batch_state->buffered_bytes += VARSIZE(edge_properties);
    batch_state->num_tuples++;

    /* Insert the batch when tuple count OR byte threshold is reached */
    if (batch_state->num_tuples >= BATCH_SIZE ||
        batch_state->buffered_bytes >= MAX_BUFFERED_BYTES)
    {
        insert_batch(batch_state);
        batch_state->num_tuples = 0;
        batch_state->buffered_bytes = 0;
    }
}

static void trim_field_to_name(Name name, const char *str)
{
    const char *start;
    const char *p;
    const char *last_non_space = NULL;
    size_t len;

    if (str == NULL)
    {
        NameStr(*name)[0] = '\0';
        return;
    }

    start = str;
    while (*start == ' ' || *start == '\t' ||
           *start == '\n' || *start == '\r')
    {
        start++;
    }

    if (*start == '\0')
    {
        NameStr(*name)[0] = '\0';
        return;
    }

    for (p = start; *p != '\0'; p++)
    {
        if (*p != ' ' && *p != '\t' &&
            *p != '\n' && *p != '\r')
        {
            last_non_space = p;
        }
    }

    if (last_non_space == NULL)
    {
        NameStr(*name)[0] = '\0';
        return;
    }

    len = last_non_space - start + 1;
    if (len >= NAMEDATALEN)
    {
        len = NAMEDATALEN - 1;
    }

    memcpy(NameStr(*name), start, len);
    NameStr(*name)[len] = '\0';
}

static vertex_label_id_cache *create_vertex_label_id_cache(void)
{
    HASHCTL hashctl;
    vertex_label_id_cache *cache;

    MemSet(&hashctl, 0, sizeof(hashctl));
    hashctl.keysize = sizeof(NameData);
    hashctl.entrysize = sizeof(vertex_label_id_cache_entry);

    cache = palloc(sizeof(vertex_label_id_cache));
    cache->entries = hash_create("age edge load vertex label id cache", 16,
                                 &hashctl, HASH_ELEM | HASH_BLOBS);
    cache->last_valid = false;
    cache->last_id = -1;

    return cache;
}

static void destroy_vertex_label_id_cache(vertex_label_id_cache *cache)
{
    hash_destroy(cache->entries);
    pfree(cache);
}

static int32 get_cached_vertex_label_id(vertex_label_id_cache *cache,
                                        const char *label_name,
                                        Oid graph_oid)
{
    NameData key;
    vertex_label_id_cache_entry *entry;
    bool found;

    if (cache->last_valid &&
        namestrcmp(&cache->last_name, label_name) == 0)
    {
        return cache->last_id;
    }

    namestrcpy(&key, label_name);
    entry = hash_search(cache->entries, &key, HASH_ENTER, &found);
    if (!found)
    {
        label_cache_data *label_cache;

        label_cache = search_label_name_graph_cache_cached(label_name,
                                                           graph_oid);
        entry->id = label_cache != NULL ? label_cache->id : -1;
    }

    cache->last_name = key;
    cache->last_id = entry->id;
    cache->last_valid = true;

    return entry->id;
}

/*
 * Create COPY options for CSV parsing.
 * Returns a List of DefElem nodes.
 */
static List *create_copy_options(void)
{
    List *options = NIL;

    /* FORMAT csv */
    options = lappend(options,
                      makeDefElem("format",
                                  (Node *) makeString("csv"),
                                  -1));

    /* HEADER false - we'll read the header ourselves */
    options = lappend(options,
                      makeDefElem("header",
                                  (Node *) makeBoolean(false),
                                  -1));

    return options;
}

/*
 * Load edges from CSV file using pg's COPY infrastructure.
 */
int create_edges_from_csv_file(char *file_path,
                               char *graph_name,
                               Oid graph_oid,
                               char *label_name,
                               Oid label_relid,
                               Oid label_seq_relid,
                               int label_id,
                               bool load_as_agtype)
{
    Relation        label_rel;
    CopyFromState   cstate;
    List           *copy_options;
    ParseState     *pstate;
    char          **fields;
    int             nfields;
    char          **header = NULL;
    int             header_count = 0;
    bool            is_first_row = true;
    batch_insert_state *batch_state = NULL;
    MemoryContext   batch_context;
    MemoryContext   old_context;
    vertex_label_id_cache *vertex_label_id_cache = NULL;

    /* Create a memory context for batch processing - reset after each batch */
    batch_context = AllocSetContextCreate(CurrentMemoryContext,
                                          "AGE CSV Edge Load Batch Context",
                                          ALLOCSET_DEFAULT_SIZES);

    label_rel = table_open(label_relid, RowExclusiveLock);

    /* Initialize the batch insert state */
    init_batch_insert(&batch_state, label_relid);
    vertex_label_id_cache = create_vertex_label_id_cache();

    /* Create COPY options for CSV parsing */
    copy_options = create_copy_options();

    /* Create a minimal ParseState for BeginCopyFrom */
    pstate = make_parsestate(NULL);

    PG_TRY();
    {
        /*
         * Initialize COPY FROM state.
         * We pass the label relation but will only use NextCopyFromRawFields
         * which returns raw parsed strings without type conversion.
         */
        cstate = BeginCopyFrom(pstate,
                               label_rel,
                               NULL,           /* whereClause */
                               file_path,
                               false,          /* is_program */
                               NULL,           /* data_source_cb */
                               NIL,            /* attnamelist */
                               copy_options);

        /*
         * Process rows using COPY's csv parsing.
         * NextCopyFromRawFields uses 64KB buffers internally.
         */
        while (NextCopyFromRawFields(cstate, &fields, &nfields))
        {
            if (is_first_row)
            {
                int i;

                /* First row is the header - save column names (in main context) */
                header_count = nfields;
                header = (char **) palloc(sizeof(char *) * nfields);

                for (i = 0; i < nfields; i++)
                {
                    /* Trim whitespace from header fields */
                    header[i] = trim_whitespace(fields[i]);
                }

                is_first_row = false;
            }
            else
            {
                /* Switch to batch context for row processing */
                old_context = MemoryContextSwitchTo(batch_context);

                /* Data row - process it */
                process_edge_row(fields, nfields,
                                 header, header_count,
                                 label_id, label_seq_relid,
                                 graph_oid, load_as_agtype,
                                 batch_state, vertex_label_id_cache);

                /* Switch back to main context */
                MemoryContextSwitchTo(old_context);

                /* Reset batch context after each batch to free memory */
                if (batch_state->num_tuples == 0)
                {
                    MemoryContextReset(batch_context);
                }
            }
        }

        /* Finish any remaining batch inserts */
        finish_batch_insert(&batch_state);
        MemoryContextReset(batch_context);
        destroy_vertex_label_id_cache(vertex_label_id_cache);
        vertex_label_id_cache = NULL;

        /* Clean up COPY state */
        EndCopyFrom(cstate);
    }
    PG_FINALLY();
    {
        /* Free header if allocated */
        if (header != NULL)
        {
            int i;
            for (i = 0; i < header_count; i++)
            {
                pfree(header[i]);
            }
            pfree(header);
        }

        /* Close the relation */
        table_close(label_rel, RowExclusiveLock);

        if (vertex_label_id_cache != NULL)
        {
            destroy_vertex_label_id_cache(vertex_label_id_cache);
        }

        /* Delete batch context */
        MemoryContextDelete(batch_context);

        /* Free parse state */
        free_parsestate(pstate);
    }
    PG_END_TRY();

    return EXIT_SUCCESS;
}
