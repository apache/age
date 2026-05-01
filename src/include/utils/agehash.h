/*
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 */

/*
 * agehash.h - Robin Hood open-addressing hashtable for AGE's hot-path caches.
 *
 * This is an internal utility used by the global graph cache (vertex and edge
 * tables) to replace dynahash on the lookup-dominated read-only-after-build
 * path. Each lookup traverses a single contiguous slot array with no chain
 * pointer-chasing, which on AGE workloads roughly halves lookup latency
 * relative to dynahash (see Stage 5 microbench).
 *
 * Mode: only AGEHASH_INLINE is supported by this initial revision. INLINE
 * stores the payload directly in the slot, suitable for tables that are
 * never mutated after the build phase. A future revision will add an
 * AGEHASH_INDIRECT mode for tables that support insert and pointer-stable
 * payloads during queries.
 *
 * Memory: every allocation lives in a caller-supplied MemoryContext. Free is
 * a single MemoryContextDelete by the caller; agehash itself never frees
 * piecewise. This makes leak-on-elog impossible: PG unwinds the context.
 *
 * Capacity: always a power of two; grown by doubling when size exceeds
 * AGEHASH_MAX_LOAD * capacity. After agehash_freeze() is called, all
 * insert paths are forbidden (asserted in DEBUG builds), which guarantees
 * that lookups can never observe a partially-rehashed structure.
 */

#ifndef AG_AGEHASH_H
#define AG_AGEHASH_H

#include "postgres.h"
#include "utils/memutils.h"
#include "utils/palloc.h"

/* Sentinel probe distance marking an empty slot. */
#define AGEHASH_EMPTY 0xFFFFu

/*
 * Load factor above which we grow.
 *
 * 0.85 balances three goals on AGE's hot tables:
 *   - Memory: the edge_table on SF3 is ~52M entries; at 0.85 the slot array
 *     is ~67M slots which is roughly the same total bytes as the dynahash
 *     bucket array + per-entry HASHELEMENT chain headers it replaces.
 *   - Probe distance: Robin Hood at 0.85 still keeps average probes near 1
 *     and max probes well below the 0xFE00 overflow guard.
 *   - Grow cadence: a higher threshold means fewer doublings during the
 *     edge cache build (each doubling rehashes the entire table).
 *
 * If you change this, re-run the rh_microbench harness on the VM and the
 * SF3 paired benchmark; both are sensitive to the load factor.
 */
#define AGEHASH_MAX_LOAD 0.85

/*
 * Caller-supplied hash callback. keysize is constant for a given table; we
 * still pass it so callers can reuse one function across multiple tables
 * with different key types if desired.
 */
typedef uint32 (*agehash_hash_fn)(const void *key, Size keysize);

/* Caller-supplied key-equality callback. Returns true iff a == b. */
typedef bool   (*agehash_keyeq_fn)(const void *a, const void *b, Size keysize);

/*
 * Layout mode. Only INLINE is implemented in this revision; INDIRECT is
 * declared so the public enum values stay stable when it lands.
 */
typedef enum AgeHashMode
{
    AGEHASH_INLINE = 0,
    AGEHASH_INDIRECT = 1
} AgeHashMode;

/* Opaque table handle. */
typedef struct AgeHashTable AgeHashTable;

/*
 * Slot layout (INLINE mode), packed:
 *
 *   offset 0 : uint16 probe_dist        (AGEHASH_EMPTY == empty)
 *   offset 2 : uint16 _reserved         (future flags / tombstones)
 *   offset 4 : uint32 _pad              (force key to 8-byte alignment)
 *   offset 8 : key   (key_size bytes)
 *   offset 8+key_size : payload (payload_size bytes)
 *
 * The header is 8 bytes; total slot bytes = 8 + key_size + payload_size,
 * rounded up to MAXIMUM_ALIGNOF.
 */

#define AGEHASH_SLOT_HDR_BYTES 8
#define AGEHASH_SLOT_KEY_OFFSET AGEHASH_SLOT_HDR_BYTES

/*
 * Recover a key pointer from a payload pointer. INLINE-mode tables store
 * the key immediately before the payload, so this is pure pointer
 * arithmetic and does not need the table handle. The caller must know the
 * key size at this site; this is the case for every AGE caller (each table
 * has a single fixed key type).
 */
#define agehash_key_from_payload(payload, key_size) \
    ((const void *) ((const char *) (payload) - (Size) (key_size)))

/*
 * Construction. capacity_hint is a number of entries; the actual capacity
 * will be the next power of two >= capacity_hint / AGEHASH_MAX_LOAD, with a
 * floor of 64 slots. Pass 0 to let the table start at the floor.
 */
extern AgeHashTable *agehash_create_inline(MemoryContext mcxt,
                                           Size key_size,
                                           Size payload_size,
                                           uint32 capacity_hint,
                                           agehash_hash_fn hash_fn,
                                           agehash_keyeq_fn keyeq_fn);

/*
 * Reserve / find. If the key is not present, allocates a fresh slot
 * (rebalancing via Robin Hood swaps), zero-fills the payload, sets
 * *found = false, and returns a pointer to the payload region. The caller
 * fills it in. If the key is present, sets *found = true and returns the
 * existing payload pointer.
 *
 * The returned payload pointer is *not* stable across subsequent
 * agehash_insert calls in INLINE mode (a later insert may swap this slot).
 * Callers requiring stable pointers must use INDIRECT mode (future).
 *
 * Asserts that the table has not been frozen (DEBUG builds).
 */
extern void *agehash_insert(AgeHashTable *t, const void *key, bool *found);

/* Variant that accepts a precomputed hash value, skipping the hash callback. */
extern void *agehash_insert_with_hash(AgeHashTable *t, const void *key,
                                      uint32 hashvalue, bool *found);

/*
 * Lookup. Returns a pointer to the payload region, or NULL if absent.
 * The pointer is stable as long as no further insert touches the table.
 */
extern void *agehash_lookup(AgeHashTable *t, const void *key);

/* Variant accepting a precomputed hash value. */
extern void *agehash_lookup_with_hash(AgeHashTable *t, const void *key,
                                      uint32 hashvalue);

/*
 * Freeze the table: subsequent insert/grow attempts are an Assert failure
 * in DEBUG and an elog(ERROR) in production. This is the contract that
 * lets read-only-after-build callers hand out long-lived payload pointers.
 */
extern void agehash_freeze(AgeHashTable *t);

/* True after agehash_freeze(); useful for caller-side asserts. */
extern bool agehash_is_frozen(const AgeHashTable *t);

/* Live entry count. */
extern uint32 agehash_size(const AgeHashTable *t);

/* Allocated slot count (capacity). */
extern uint32 agehash_capacity(const AgeHashTable *t);

/*
 * Iteration. Usage:
 *
 *     AgeHashIter it;
 *     for (agehash_iter_init(t, &it); agehash_iter_next(&it); )
 *     {
 *         graphid k = *(graphid *) it.key;
 *         my_payload *p = it.payload;
 *         ...
 *     }
 *
 * Iteration order is unspecified. Modifying the table during iteration is
 * undefined behaviour.
 */
typedef struct AgeHashIter
{
    AgeHashTable *t;
    uint32        idx;
    void         *key;
    void         *payload;
} AgeHashIter;

extern void agehash_iter_init(AgeHashTable *t, AgeHashIter *it);
extern bool agehash_iter_next(AgeHashIter *it);

/*
 * Internal self-test. Returns a NUL-terminated diagnostic string allocated
 * in CurrentMemoryContext: "OK" on success, "FAIL: <reason>" on failure.
 * Used by the agehash regression test.
 */
extern const char *agehash_self_test(void);

#endif /* AG_AGEHASH_H */
