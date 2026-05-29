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

#include "catalog/namespace.h"
#include "utils/inval.h"
#include "utils/syscache.h"

#include "catalog/ag_namespace.h"

static Oid cached_ag_catalog_namespace = InvalidOid;
static Oid cached_pg_catalog_namespace = InvalidOid;
static bool namespace_cache_callback_registered = false;

static void invalidate_namespace_oid_cache(Datum arg, int cache_id,
                                           uint32 hash_value);
static void initialize_namespace_oid_cache(void);

Oid ag_catalog_namespace_id(void)
{
    initialize_namespace_oid_cache();

    if (!OidIsValid(cached_ag_catalog_namespace))
    {
        cached_ag_catalog_namespace = get_namespace_oid("ag_catalog", false);
    }

    return cached_ag_catalog_namespace;
}

Oid pg_catalog_namespace_id(void)
{
    initialize_namespace_oid_cache();

    if (!OidIsValid(cached_pg_catalog_namespace))
    {
        cached_pg_catalog_namespace = get_namespace_oid("pg_catalog", false);
    }

    return cached_pg_catalog_namespace;
}

static void initialize_namespace_oid_cache(void)
{
    if (namespace_cache_callback_registered)
    {
        return;
    }

    CacheRegisterSyscacheCallback(NAMESPACEOID, invalidate_namespace_oid_cache,
                                  (Datum)0);
    namespace_cache_callback_registered = true;
}

static void invalidate_namespace_oid_cache(Datum arg, int cache_id,
                                           uint32 hash_value)
{
    cached_ag_catalog_namespace = InvalidOid;
    cached_pg_catalog_namespace = InvalidOid;
}
