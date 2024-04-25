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

#ifndef GRAPH_H
#define GRAPH_H

#include "postgres.h"

#include "fmgr.h"
#include "storage/itemptr.h"

typedef uint64 Graphid;
typedef uint16 Labid;

#define InvalidLabid			0

#define DatumGetGraphid(d)		((Graphid) (DatumGetUInt64(d)))
#define GraphidGetDatum(p)		UInt64GetDatum(p)
#define PG_GETARG_GRAPHID(x)	DatumGetGraphid(PG_GETARG_DATUM(x))
#define PG_RETURN_GRAPHID(x)	return UInt64GetDatum(x)

#define GraphidGetLabid(id)		((Labid) (((uint64) (id)) >> (32 + 16)))
#define GraphidGetLocid(id)		(((uint64) (id)) & 0x0000ffffffffffff)

#define GraphidSet(_id, _labid, _locid) \
	do { \
		AssertMacro(PointerIsValid(_id)); \
		*(_id) = (((uint64) (_labid)) << (32 + 16)) | \
				 (((uint64) (_locid)) & 0x0000ffffffffffff); \
	} while (0)

#define GRAPHID_LABID_MAX	PG_UINT16_MAX
#define GRAPHID_LOCID_MAX	((UINT64CONST(1) << (32 + 16)) - 1)

typedef struct {
	Oid			tableoid;
	ItemPointerData tid;
} Rowid;

#define RowidGetDatum(X)	PointerGetDatum(X)
#define PG_GETARG_ROWID(n)	((Rowid *) DatumGetPointer(PG_GETARG_DATUM(n)))
#define PG_RETURN_ROWID(x)	return RowidGetDatum(x)

/* graphid */
extern Datum graphid(PG_FUNCTION_ARGS);
extern Datum graphid_in(PG_FUNCTION_ARGS);
extern Datum graphid_out(PG_FUNCTION_ARGS);
extern Datum graphid_recv(PG_FUNCTION_ARGS);
extern Datum graphid_send(PG_FUNCTION_ARGS);
extern Datum graphid_labid(PG_FUNCTION_ARGS);
extern Datum graphid_locid(PG_FUNCTION_ARGS);
extern Datum graph_labid(PG_FUNCTION_ARGS);
/* graphid - comparison */
extern Datum graphid_eq(PG_FUNCTION_ARGS);
extern Datum graphid_ne(PG_FUNCTION_ARGS);
extern Datum graphid_lt(PG_FUNCTION_ARGS);
extern Datum graphid_gt(PG_FUNCTION_ARGS);
extern Datum graphid_le(PG_FUNCTION_ARGS);
extern Datum graphid_ge(PG_FUNCTION_ARGS);

/* rowid */
extern Datum rowid(PG_FUNCTION_ARGS);
extern Datum rowid_in(PG_FUNCTION_ARGS);
extern Datum rowid_out(PG_FUNCTION_ARGS);
extern Datum rowid_tableoid(PG_FUNCTION_ARGS);
extern Datum rowid_ctid(PG_FUNCTION_ARGS);
/* rowid -comparison */
extern Datum rowid_eq(PG_FUNCTION_ARGS);
extern Datum rowid_ne(PG_FUNCTION_ARGS);
extern Datum rowid_lt(PG_FUNCTION_ARGS);
extern Datum rowid_gt(PG_FUNCTION_ARGS);
extern Datum rowid_le(PG_FUNCTION_ARGS);
extern Datum rowid_ge(PG_FUNCTION_ARGS);
/* rowid - BTree */
extern Datum btrowidcmp(PG_FUNCTION_ARGS);

/* vertex */
extern Datum vertex_out(PG_FUNCTION_ARGS);
extern Datum _vertex_out(PG_FUNCTION_ARGS);
extern Datum vertex_label(PG_FUNCTION_ARGS);
extern Datum _vertex_length(PG_FUNCTION_ARGS);
extern Datum vtojb(PG_FUNCTION_ARGS);
/* vertex - comparison */
extern Datum vertex_eq(PG_FUNCTION_ARGS);
extern Datum vertex_ne(PG_FUNCTION_ARGS);
extern Datum vertex_lt(PG_FUNCTION_ARGS);
extern Datum vertex_gt(PG_FUNCTION_ARGS);
extern Datum vertex_le(PG_FUNCTION_ARGS);
extern Datum vertex_ge(PG_FUNCTION_ARGS);

/* edge */
extern Datum edge_out(PG_FUNCTION_ARGS);
extern Datum _edge_out(PG_FUNCTION_ARGS);
extern Datum edge_label(PG_FUNCTION_ARGS);
extern Datum _edge_length(PG_FUNCTION_ARGS);
extern Datum etojb(PG_FUNCTION_ARGS);
/* edge - comparison */
extern Datum edge_eq(PG_FUNCTION_ARGS);
extern Datum edge_ne(PG_FUNCTION_ARGS);
extern Datum edge_lt(PG_FUNCTION_ARGS);
extern Datum edge_gt(PG_FUNCTION_ARGS);
extern Datum edge_le(PG_FUNCTION_ARGS);
extern Datum edge_ge(PG_FUNCTION_ARGS);

/* graphpath */
extern Datum graphpath_out(PG_FUNCTION_ARGS);
extern Datum _graphpath_length(PG_FUNCTION_ARGS);
extern Datum graphpath_length(PG_FUNCTION_ARGS);
extern Datum graphpath_vertices(PG_FUNCTION_ARGS);
extern Datum graphpath_edges(PG_FUNCTION_ARGS);

/* additional function implementations */
extern Datum edge_start_vertex(PG_FUNCTION_ARGS);
extern Datum edge_end_vertex(PG_FUNCTION_ARGS);
extern Datum vertex_labels(PG_FUNCTION_ARGS);

/* support functions */
extern Datum getVertexIdDatum(Datum datum);
extern Datum getVertexPropDatum(Datum datum);
extern Datum getVertexTidDatum(Datum datum);
extern Datum getEdgeIdDatum(Datum datum);
extern Datum getEdgeStartDatum(Datum datum);
extern Datum getEdgeEndDatum(Datum datum);
extern Datum getEdgePropDatum(Datum datum);
extern Datum getEdgeTidDatum(Datum datum);
extern void getGraphpathArrays(Datum graphpath, Datum *vertices, Datum *edges);
extern Datum makeGraphpathDatum(Datum *vertices, int nvertices, Datum *edges,
								int nedges);
extern Datum makeGraphVertexDatum(Datum id, Datum prop_map, Datum tid);
extern Datum makeGraphEdgeDatum(Datum id, Datum start, Datum end,
								Datum prop_map, Datum tid);

/* index support - BTree */
extern Datum btgraphidcmp(PG_FUNCTION_ARGS);
/* index support - Hash */
extern Datum graphid_hash(PG_FUNCTION_ARGS);
/* index support - GIN (as BTree) */
extern Datum gin_extract_value_graphid(PG_FUNCTION_ARGS);
extern Datum gin_extract_query_graphid(PG_FUNCTION_ARGS);
extern Datum gin_consistent_graphid(PG_FUNCTION_ARGS);
extern Datum gin_compare_partial_graphid(PG_FUNCTION_ARGS);

/* graph meta */
extern Datum regather_graphmeta(PG_FUNCTION_ARGS);

#endif	/* GRAPH_H */
