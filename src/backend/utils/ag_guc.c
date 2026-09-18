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

#include "utils/guc.h"
#include "utils/ag_guc.h"

bool age_enable_containment = true;
bool age_enforce_rls_in_traversal = true;

/*
 * Defines AGE's custom configuration parameters.
 *
 * The name of the parameter must be `age.*`. This name is used for setting
 * value to the parameter. For example, `SET age.enable_containment = on;`.
 */
void define_config_params(void)
{
    DefineCustomBoolVariable("age.enable_containment",
                             "Use @> operator to transform MATCH's filter. Otherwise, use -> operator.",
                             NULL,
                             &age_enable_containment,
                             true,
                             PGC_SUSET,
                             0,
                             NULL,
                             NULL,
                             NULL);
    DefineCustomBoolVariable("age.enforce_rls_in_traversal",
                             "Enforce row-level security during VLE / global-graph traversal. When off, the traversal cache is loaded with a direct scan that bypasses RLS.",
                             NULL,
                             &age_enforce_rls_in_traversal,
                             true,
                             PGC_SUSET,
                             0,
                             NULL,
                             NULL,
                             NULL);
    EmitWarningsOnPlaceholders("age");
}
