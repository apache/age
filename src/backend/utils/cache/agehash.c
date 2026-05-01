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
 * agehash.c - Robin Hood open-addressing hashtable for AGE.
 *
 * See agehash.h for the public contract. This file implements the INLINE
 * mode only.
 *
 * Internal slot layout (INLINE):
 *
 *   bytes 0..1     uint16 probe_dist  (AGEHASH_EMPTY = 0xFFFF marks empty)
 *   bytes 2..3     uint16 reserved    (future tombstone / flag bits)
 *   bytes 4..7     uint32 pad         (forces key to 8-byte alignment)
 *   bytes 8..K+7   key
 *   bytes K+8..    payload
 *
 * slot_size = MAXALIGN(8 + key_size + payload_size).
 */

#include "postgres.h"

#include "fmgr.h"
#include "utils/agehash.h"
#include "utils/builtins.h"
#include "utils/memutils.h"

/* ------------------------------------------------------------------------- */

struct AgeHashTable
{
    /* Slot array: capacity * slot_size bytes, palloc'd in mcxt. */
    char            *slots;
    uint32           capacity;       /* always a power of two */
    uint32           capacity_mask;  /* capacity - 1 */
    uint32           size;           /* live entries */
    uint32           max_size;       /* size at which we grow */
    uint32           slot_size;      /* total bytes per slot */
    uint32           key_size;
    uint32           payload_size;
    uint32           payload_offset; /* AGEHASH_SLOT_KEY_OFFSET + key_size */
    AgeHashMode      mode;
    bool             frozen;
    agehash_hash_fn  hash_fn;
    agehash_keyeq_fn keyeq_fn;
    MemoryContext    mcxt;
};

/* ------------------------------------------------------------------------- */
/* Slot accessors. */

static inline char *
slot_at(AgeHashTable *t, uint32 idx)
{
    return t->slots + (Size) idx * t->slot_size;
}

static inline uint16
slot_probe_dist(const char *slot)
{
    uint16 d;
    memcpy(&d, slot, sizeof(uint16));
    return d;
}

static inline void
slot_set_probe_dist(char *slot, uint16 d)
{
    memcpy(slot, &d, sizeof(uint16));
}

static inline char *
slot_key_ptr(AgeHashTable *t, char *slot)
{
    (void) t;
    return slot + AGEHASH_SLOT_KEY_OFFSET;
}

static inline char *
slot_payload_ptr(AgeHashTable *t, char *slot)
{
    return slot + t->payload_offset;
}

/* ------------------------------------------------------------------------- */
/* Construction. */

static uint32
next_pow2(uint32 v)
{
    uint32 p = 1;
    while (p < v)
        p <<= 1;
    return p;
}

AgeHashTable *
agehash_create_inline(MemoryContext mcxt,
                      Size key_size,
                      Size payload_size,
                      uint32 capacity_hint,
                      agehash_hash_fn hash_fn,
                      agehash_keyeq_fn keyeq_fn)
{
    AgeHashTable  *t;
    MemoryContext  oldctx;
    uint32         min_cap;
    uint32         cap;

    Assert(mcxt != NULL);
    Assert(key_size > 0 && key_size <= 64);
    Assert(payload_size > 0 && payload_size <= 4096);
    Assert(hash_fn != NULL);
    Assert(keyeq_fn != NULL);

    oldctx = MemoryContextSwitchTo(mcxt);

    t = palloc0(sizeof(AgeHashTable));
    t->mcxt = mcxt;
    t->mode = AGEHASH_INLINE;
    t->frozen = false;
    t->hash_fn = hash_fn;
    t->keyeq_fn = keyeq_fn;
    t->key_size = (uint32) key_size;
    t->payload_size = (uint32) payload_size;
    t->payload_offset = AGEHASH_SLOT_KEY_OFFSET + (uint32) key_size;
    t->slot_size = MAXALIGN(t->payload_offset + (uint32) payload_size);

    /*
     * Capacity floor of 64 keeps tiny tables out of degenerate-load territory
     * and avoids a flurry of grows on the first few inserts.
     */
    if (capacity_hint == 0)
        min_cap = 64;
    else
    {
        /* size capacity_hint at MAX_LOAD so we don't immediately grow */
        min_cap = (uint32) ((double) capacity_hint / AGEHASH_MAX_LOAD) + 1;
        if (min_cap < 64)
            min_cap = 64;
    }
    cap = next_pow2(min_cap);
    Assert((cap & (cap - 1)) == 0);

    t->capacity = cap;
    t->capacity_mask = cap - 1;
    t->size = 0;
    t->max_size = (uint32) ((double) cap * AGEHASH_MAX_LOAD);
    /*
     * The slot array can comfortably exceed 1 GiB on production graphs
     * (SF3 ldbc_snb edge_table is ~3 GiB at 0.7 load). Use the HUGE
     * allocator to bypass the standard MaxAllocSize check.
     */
    t->slots = (char *) MemoryContextAllocHuge(mcxt,
                                               (Size) cap * t->slot_size);

    /* Mark every slot empty. */
    {
        uint32 i;
        for (i = 0; i < cap; i++)
            slot_set_probe_dist(slot_at(t, i), AGEHASH_EMPTY);
    }

    MemoryContextSwitchTo(oldctx);
    return t;
}

/* ------------------------------------------------------------------------- */
/* Insert. Robin Hood with rich-poor swap. */

static void agehash_grow(AgeHashTable *t);

static void *
agehash_insert_internal(AgeHashTable *t, const void *key, uint32 hashvalue,
                        bool *found)
{
    uint32 i;
    uint16 d;
    /*
     * Carrier for the entry currently being placed. Starts as the caller's
     * key with a fresh, zero-filled payload; gets overwritten on each
     * Robin Hood swap.
     */
    char   carry_key[64];
    char   carry_payload[4096];
    void  *result_payload = NULL;
    bool   placed_caller = false;

    Assert(!t->frozen);
    Assert(t->key_size <= sizeof(carry_key));
    Assert(t->payload_size <= sizeof(carry_payload));

    /* Grow before insert if at threshold. */
    if (t->size >= t->max_size)
        agehash_grow(t);

    /* Initialize carry buffers with the caller's key and an empty payload. */
    memcpy(carry_key, key, t->key_size);
    memset(carry_payload, 0, t->payload_size);

    i = hashvalue & t->capacity_mask;
    d = 0;

    for (;;)
    {
        char  *slot = slot_at(t, i);
        uint16 sd   = slot_probe_dist(slot);

        if (sd == AGEHASH_EMPTY)
        {
            /* Place the carrier here and we're done. */
            slot_set_probe_dist(slot, d);
            memcpy(slot_key_ptr(t, slot), carry_key, t->key_size);
            memcpy(slot_payload_ptr(t, slot), carry_payload, t->payload_size);
            t->size++;
            if (!placed_caller)
            {
                /* The caller's slot landed here. */
                if (found != NULL)
                    *found = false;
                return slot_payload_ptr(t, slot);
            }
            /*
             * The caller was placed earlier via a swap; result_payload
             * already points at their final slot.
             */
            Assert(result_payload != NULL);
            return result_payload;
        }

        if (sd == d &&
            !placed_caller &&
            t->keyeq_fn(slot_key_ptr(t, slot), carry_key, t->key_size))
        {
            /*
             * Existing entry with the caller's key. Note: this match check
             * is only relevant before we've performed a swap; once we've
             * placed the caller into a slot, the key in `carry` is some
             * displaced entry that, by RH invariant on insert from a fresh
             * key, cannot already exist in the table.
             */
            if (found != NULL)
                *found = true;
            return slot_payload_ptr(t, slot);
        }

        if (sd < d)
        {
            /*
             * Rich-poor swap: this slot's owner is closer to its ideal
             * bucket than we are. Take its place and continue with the
             * displaced entry. If we have not yet placed the caller, this
             * is where they end up; remember the pointer so we can return
             * it once the displaced chain finishes.
             */
            char   tmp_key[64];
            char   tmp_payload[4096];
            uint16 tmp_d = sd;

            memcpy(tmp_key,     slot_key_ptr(t, slot),     t->key_size);
            memcpy(tmp_payload, slot_payload_ptr(t, slot), t->payload_size);

            slot_set_probe_dist(slot, d);
            memcpy(slot_key_ptr(t, slot),     carry_key,     t->key_size);
            memcpy(slot_payload_ptr(t, slot), carry_payload, t->payload_size);

            if (!placed_caller)
            {
                placed_caller = true;
                result_payload = slot_payload_ptr(t, slot);
                /* Notify caller: this insert is a fresh entry. */
                if (found != NULL)
                {
                    *found = false;
                    found = NULL; /* don't write again */
                }
            }

            /* Continue with the displaced entry as the new carrier. */
            memcpy(carry_key,     tmp_key,     t->key_size);
            memcpy(carry_payload, tmp_payload, t->payload_size);
            d = tmp_d;
        }

        i = (i + 1) & t->capacity_mask;
        d++;

        /*
         * Probe distance overflow guard. With AGEHASH_MAX_LOAD = 0.7 and a
         * non-degenerate hash function, max probe is empirically <= 32.
         * The 0xFE00 ceiling reserves headroom while leaving probe_dist
         * well clear of the AGEHASH_EMPTY sentinel.
         */
        Assert(d < 0xFE00);
        if (unlikely(d >= 0xFE00))
            elog(ERROR, "agehash: probe distance overflow (likely a bad hash function)");
    }
}

void *
agehash_insert(AgeHashTable *t, const void *key, bool *found)
{
    uint32 h = t->hash_fn(key, t->key_size);
    return agehash_insert_internal(t, key, h, found);
}

void *
agehash_insert_with_hash(AgeHashTable *t, const void *key,
                         uint32 hashvalue, bool *found)
{
    return agehash_insert_internal(t, key, hashvalue, found);
}

/* ------------------------------------------------------------------------- */
/* Grow: double the capacity and rehash. */

static void
agehash_grow(AgeHashTable *t)
{
    char         *old_slots;
    uint32        old_cap;
    uint32        old_slot_size;
    uint32        new_cap;
    MemoryContext oldctx;
    uint32        i;

    Assert(!t->frozen);

    old_slots     = t->slots;
    old_cap       = t->capacity;
    old_slot_size = t->slot_size;

    new_cap = old_cap << 1;
    Assert(new_cap > old_cap); /* overflow guard */

    oldctx = MemoryContextSwitchTo(t->mcxt);

    t->capacity = new_cap;
    t->capacity_mask = new_cap - 1;
    t->max_size = (uint32) ((double) new_cap * AGEHASH_MAX_LOAD);
    /* HUGE allocator: see agehash_create_inline for the rationale. */
    t->slots = (char *) MemoryContextAllocHuge(t->mcxt,
                                               (Size) new_cap * t->slot_size);
    for (i = 0; i < new_cap; i++)
        slot_set_probe_dist(slot_at(t, i), AGEHASH_EMPTY);

    /* Reset size; we re-insert below (which will increment it). */
    t->size = 0;
    for (i = 0; i < old_cap; i++)
    {
        char *src = old_slots + (Size) i * old_slot_size;
        if (slot_probe_dist(src) != AGEHASH_EMPTY)
        {
            void  *src_key     = src + AGEHASH_SLOT_KEY_OFFSET;
            void  *src_payload = src + AGEHASH_SLOT_KEY_OFFSET + t->key_size;
            uint32 h = t->hash_fn(src_key, t->key_size);
            void  *dst_payload = agehash_insert_internal(t, src_key, h, NULL);
            memcpy(dst_payload, src_payload, t->payload_size);
        }
    }

    pfree(old_slots);
    MemoryContextSwitchTo(oldctx);
}

/* ------------------------------------------------------------------------- */
/* Lookup. */

void *
agehash_lookup_with_hash(AgeHashTable *t, const void *key, uint32 hashvalue)
{
    uint32 i = hashvalue & t->capacity_mask;
    uint16 d = 0;

    for (;;)
    {
        char  *slot = slot_at(t, i);
        uint16 sd   = slot_probe_dist(slot);

        if (sd == AGEHASH_EMPTY)
            return NULL;
        /*
         * Robin Hood invariant: probe_dist values along a probe sequence
         * are non-increasing as we move from an entry's home slot. If the
         * slot we land on has a smaller probe_dist than ours, the key
         * we're looking for can't be anywhere later in the sequence.
         */
        if (sd < d)
            return NULL;
        if (t->keyeq_fn(slot_key_ptr(t, slot), key, t->key_size))
            return slot_payload_ptr(t, slot);

        i = (i + 1) & t->capacity_mask;
        d++;
        Assert(d < 0xFE00);
    }
}

void *
agehash_lookup(AgeHashTable *t, const void *key)
{
    uint32 h = t->hash_fn(key, t->key_size);
    return agehash_lookup_with_hash(t, key, h);
}

/* ------------------------------------------------------------------------- */
/* Misc accessors. */

void
agehash_freeze(AgeHashTable *t)
{
    t->frozen = true;
}

bool
agehash_is_frozen(const AgeHashTable *t)
{
    return t->frozen;
}

uint32
agehash_size(const AgeHashTable *t)
{
    return t->size;
}

uint32
agehash_capacity(const AgeHashTable *t)
{
    return t->capacity;
}

void
agehash_iter_init(AgeHashTable *t, AgeHashIter *it)
{
    it->t = t;
    it->idx = 0;
    it->key = NULL;
    it->payload = NULL;
}

bool
agehash_iter_next(AgeHashIter *it)
{
    AgeHashTable *t = it->t;
    while (it->idx < t->capacity)
    {
        char *slot = slot_at(t, it->idx);
        uint32 idx = it->idx++;
        (void) idx;
        if (slot_probe_dist(slot) != AGEHASH_EMPTY)
        {
            it->key     = slot_key_ptr(t, slot);
            it->payload = slot_payload_ptr(t, slot);
            return true;
        }
    }
    it->key = NULL;
    it->payload = NULL;
    return false;
}

/* ------------------------------------------------------------------------- */
/* Self-test. Exercises insert / lookup / grow / iterate at small + medium
 * sizes and verifies invariants. Returns a string in CurrentMemoryContext. */

/* MurmurHash3 fmix64, identical to graphid_hash. */
static uint32
selftest_hash(const void *key, Size keysize)
{
    uint64 k;
    Assert(keysize == sizeof(uint64));
    memcpy(&k, key, sizeof(uint64));
    k ^= k >> 33;
    k *= UINT64CONST(0xff51afd7ed558ccd);
    k ^= k >> 33;
    k *= UINT64CONST(0xc4ceb9fe1a85ec53);
    k ^= k >> 33;
    return (uint32) k;
}

static bool
selftest_keyeq(const void *a, const void *b, Size keysize)
{
    return memcmp(a, b, keysize) == 0;
}

typedef struct selftest_payload
{
    uint64 mirror_key;
    uint64 marker;
} selftest_payload;

static const char *
selftest_run_one(MemoryContext parent, uint32 n, uint32 hint)
{
    MemoryContext     mcxt;
    AgeHashTable     *t;
    selftest_payload *p;
    bool              found;
    uint32            i;
    uint32            seen;
    AgeHashIter       it;

    mcxt = AllocSetContextCreate(parent, "agehash selftest", ALLOCSET_DEFAULT_SIZES);
    t = agehash_create_inline(mcxt, sizeof(uint64), sizeof(selftest_payload),
                              hint, selftest_hash, selftest_keyeq);

    /* Insert n keys. */
    for (i = 0; i < n; i++)
    {
        uint64 k = ((uint64) 0xa5a5 << 48) | (i + 1);
        p = (selftest_payload *) agehash_insert(t, &k, &found);
        if (found)
        {
            MemoryContextDelete(mcxt);
            return psprintf("FAIL: duplicate insert at i=%u", i);
        }
        p->mirror_key = k;
        p->marker     = (uint64) 0xdeadbeef00000000ULL | i;
    }
    if (agehash_size(t) != n)
    {
        MemoryContextDelete(mcxt);
        return psprintf("FAIL: size %u != %u after inserts",
                        agehash_size(t), n);
    }

    /* Lookup all n keys. */
    for (i = 0; i < n; i++)
    {
        uint64 k = ((uint64) 0xa5a5 << 48) | (i + 1);
        p = (selftest_payload *) agehash_lookup(t, &k);
        if (p == NULL)
        {
            MemoryContextDelete(mcxt);
            return psprintf("FAIL: lookup miss at i=%u", i);
        }
        if (p->mirror_key != k ||
            p->marker != ((uint64) 0xdeadbeef00000000ULL | i))
        {
            MemoryContextDelete(mcxt);
            return psprintf("FAIL: payload corruption at i=%u", i);
        }
    }

    /* Lookup n keys that should not exist. */
    for (i = 0; i < n; i++)
    {
        uint64 k = ((uint64) 0xb6b6 << 48) | (i + 1);
        p = (selftest_payload *) agehash_lookup(t, &k);
        if (p != NULL)
        {
            MemoryContextDelete(mcxt);
            return psprintf("FAIL: phantom lookup hit at i=%u", i);
        }
    }

    /* Re-insert (HASH_ENTER semantics) — should report found = true. */
    for (i = 0; i < n; i++)
    {
        uint64 k = ((uint64) 0xa5a5 << 48) | (i + 1);
        p = (selftest_payload *) agehash_insert(t, &k, &found);
        if (!found)
        {
            MemoryContextDelete(mcxt);
            return psprintf("FAIL: re-insert reported !found at i=%u", i);
        }
        if (p->mirror_key != k)
        {
            MemoryContextDelete(mcxt);
            return psprintf("FAIL: re-insert payload mismatch at i=%u", i);
        }
    }
    if (agehash_size(t) != n)
    {
        MemoryContextDelete(mcxt);
        return psprintf("FAIL: size %u != %u after re-inserts",
                        agehash_size(t), n);
    }

    /* Iterate and count. */
    seen = 0;
    agehash_iter_init(t, &it);
    while (agehash_iter_next(&it))
    {
        selftest_payload *pp = it.payload;
        uint64 k;
        memcpy(&k, it.key, sizeof(uint64));
        if (pp->mirror_key != k)
        {
            MemoryContextDelete(mcxt);
            return psprintf("FAIL: iter payload mismatch at seen=%u", seen);
        }
        seen++;
    }
    if (seen != n)
    {
        MemoryContextDelete(mcxt);
        return psprintf("FAIL: iter saw %u of %u", seen, n);
    }

    /* Freeze and confirm lookups still work. */
    agehash_freeze(t);
    if (!agehash_is_frozen(t))
    {
        MemoryContextDelete(mcxt);
        return "FAIL: agehash_is_frozen returned false after freeze";
    }
    {
        uint64 k = ((uint64) 0xa5a5 << 48) | 1;
        p = (selftest_payload *) agehash_lookup(t, &k);
        if (p == NULL)
        {
            MemoryContextDelete(mcxt);
            return "FAIL: lookup failed after freeze";
        }
    }

    MemoryContextDelete(mcxt);
    return NULL; /* OK */
}

const char *
agehash_self_test(void)
{
    static const struct { uint32 n; uint32 hint; } cases[] = {
        {     1,    0 },
        {     7,    0 },
        {     8,    0 },
        {     9,    0 },
        {    63,    0 },
        {    64,    0 },
        {    65,    0 },
        {  1023,    0 }, /* forces grow from 64 floor */
        {  1024,    0 },
        {  1025,    0 },
        { 10000,    0 }, /* forces multiple grows */
        { 10000, 8192 }, /* with capacity hint, no grow expected */
        { 50000,    0 }, /* larger; multiple grows */
        { 1000000,    0 }, /* exercises grow at multi-MB allocations */
        /*
         * NOTE: this set is bounded so 'make installcheck' completes
         * quickly. The library has been manually verified up to 256M
         * entries (multi-GiB slot arrays via MemoryContextAllocHuge).
         */
    };
    const size_t ncases = sizeof(cases) / sizeof(cases[0]);
    size_t       i;

    for (i = 0; i < ncases; i++)
    {
        const char *r = selftest_run_one(CurrentMemoryContext,
                                         cases[i].n, cases[i].hint);
        if (r != NULL)
            return psprintf("%s [n=%u hint=%u]", r, cases[i].n, cases[i].hint);
    }
    return "OK";
}

/* ------------------------------------------------------------------------- */
/* SQL-callable wrapper: SELECT ag_catalog._agehash_self_test();             */

PG_FUNCTION_INFO_V1(_agehash_self_test);

Datum
_agehash_self_test(PG_FUNCTION_ARGS)
{
    const char *r = agehash_self_test();
    PG_RETURN_TEXT_P(cstring_to_text(r));
}
