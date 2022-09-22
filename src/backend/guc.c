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

#include "guc.h"

bool age_guc_enable_auto_column_conversion = false;

void guc_init(void)
{
    DefineCustomBoolVariable("age.enable_auto_column_conversion",
                             "Enable AGE auto column conversion",
                             NULL,
                             &age_guc_enable_auto_column_conversion,
                             false,
                             PGC_USERSET,
                             0,
                             NULL,
                             NULL,
                             NULL);
}