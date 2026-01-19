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

#include "utils/load/ag_load_labels.h"

/*
 * Process a single vertex row from COPY's raw fields.
 * Vertex CSV format: [id,] [properties...]
 */
static void process_vertex_row(char **fields, int nfields,
                               char **header, int header_count,
                               int label_id, Oid label_seq_relid,
                               bool id_field_exists, bool load_as_agtype,
                               int64 *curr_seq_num,
                               batch_insert_state *batch_state)
{
    graphid vertex_id;
    int64 entry_id;
    TupleTableSlot *slot;
    agtype *vertex_properties;

    /* Generate or use provided entry_id */
    if (id_field_exists)
    {
        entry_id = strtol(fields[0], NULL, 10);
        if (entry_id > *curr_seq_num)
        {
            /* This is needed to ensure the sequence is up-to-date */
            DirectFunctionCall2(setval_oid,
                                ObjectIdGetDatum(label_seq_relid),
                                Int64GetDatum(entry_id));
            *curr_seq_num = entry_id;
        }
    }
    else
    {
        entry_id = nextval_internal(label_seq_relid, true);
    }

    vertex_id = make_graphid(label_id, entry_id);

    /* Get the appropriate slot from the batch state */
    slot = batch_state->slots[batch_state->num_tuples];

    /* Clear the slots contents */
    ExecClearTuple(slot);

    /* Build the agtype properties */
    vertex_properties = create_agtype_from_list(header, fields,
                                                nfields, entry_id,
                                                load_as_agtype);

    /* Fill the values in the slot */
    slot->tts_values[0] = GRAPHID_GET_DATUM(vertex_id);
    slot->tts_values[1] = AGTYPE_P_GET_DATUM(vertex_properties);
    slot->tts_isnull[0] = false;
    slot->tts_isnull[1] = false;

    /* Make the slot as containing virtual tuple */
    ExecStoreVirtualTuple(slot);

    batch_state->buffered_bytes += VARSIZE(vertex_properties);
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

/*
 * Create COPY options for csv parsing.
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
 * Load vertex labels from csv file using pg's COPY infrastructure.
 */
int create_labels_from_csv_file(char *file_path,
                                char *graph_name,
                                Oid graph_oid,
                                char *label_name,
                                int label_id,
                                bool id_field_exists,
                                bool load_as_agtype)
{
    Relation        label_rel;
    Oid             label_relid;
    CopyFromState   cstate;
    List           *copy_options;
    ParseState     *pstate;
    char          **fields;
    int             nfields;
    char          **header = NULL;
    int             header_count = 0;
    bool            is_first_row = true;
    char           *label_seq_name;
    Oid             label_seq_relid;
    int64           curr_seq_num = 0;
    batch_insert_state *batch_state = NULL;
    MemoryContext   batch_context;
    MemoryContext   old_context;

    /* Create a memory context for batch processing - reset after each batch */
    batch_context = AllocSetContextCreate(CurrentMemoryContext,
                                          "AGE CSV Load Batch Context",
                                          ALLOCSET_DEFAULT_SIZES);

    /* Get the label relation */
    label_relid = get_label_relation(label_name, graph_oid);
    label_rel = table_open(label_relid, RowExclusiveLock);

    /* Get sequence info */
    label_seq_name = get_label_seq_relation_name(label_name);
    label_seq_relid = get_relname_relid(label_seq_name, graph_oid);

    if (id_field_exists)
    {
        /*
         * Set the curr_seq_num since we will need it to compare with
         * incoming entry_id.
         */
        curr_seq_num = nextval_internal(label_seq_relid, true);
    }

    /* Initialize the batch insert state */
    init_batch_insert(&batch_state, label_name, graph_oid);

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
                               NIL,            /* attnamelist - NULL means all columns */
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
                process_vertex_row(fields, nfields,
                                   header, header_count,
                                   label_id, label_seq_relid,
                                   id_field_exists, load_as_agtype,
                                   &curr_seq_num,
                                   batch_state);

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

        /* Delete batch context */
        MemoryContextDelete(batch_context);

        /* Free parse state */
        free_parsestate(pstate);
    }
    PG_END_TRY();

    return EXIT_SUCCESS;
}
