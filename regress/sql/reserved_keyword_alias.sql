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

/*
 * Regression coverage for issue #2355:
 *   Cypher reserved keywords from the `safe_keywords` set must be accepted
 *   in alias positions (RETURN/WITH/YIELD ... AS <name>, UNWIND ... AS <name>).
 *   Conflicting tokens (END / NULL / TRUE / FALSE) must remain rejected.
 *   Pattern variable bindings are intentionally still restricted to plain
 *   identifiers.
 */

LOAD 'age';
SET search_path TO ag_catalog;

SELECT * FROM create_graph('issue_2355');

-- The exact reproducer from the issue (previously failed with
-- "syntax error at or near "count"").
SELECT * FROM cypher('issue_2355', $$ RETURN 1 AS count $$) AS (a agtype);

-- Representative coverage across the safe_keywords set as RETURN aliases.
SELECT * FROM cypher('issue_2355', $$ RETURN 1 AS exists      $$) AS (a agtype);
SELECT * FROM cypher('issue_2355', $$ RETURN 1 AS coalesce    $$) AS (a agtype);
SELECT * FROM cypher('issue_2355', $$ RETURN 1 AS match       $$) AS (a agtype);
SELECT * FROM cypher('issue_2355', $$ RETURN 1 AS return      $$) AS (a agtype);
SELECT * FROM cypher('issue_2355', $$ RETURN 1 AS where       $$) AS (a agtype);
SELECT * FROM cypher('issue_2355', $$ RETURN 1 AS order       $$) AS (a agtype);
SELECT * FROM cypher('issue_2355', $$ RETURN 1 AS limit       $$) AS (a agtype);
SELECT * FROM cypher('issue_2355', $$ RETURN 1 AS distinct    $$) AS (a agtype);
SELECT * FROM cypher('issue_2355', $$ RETURN 1 AS optional    $$) AS (a agtype);
SELECT * FROM cypher('issue_2355', $$ RETURN 1 AS detach      $$) AS (a agtype);
SELECT * FROM cypher('issue_2355', $$ RETURN 1 AS contains    $$) AS (a agtype);
SELECT * FROM cypher('issue_2355', $$ RETURN 1 AS starts      $$) AS (a agtype);
SELECT * FROM cypher('issue_2355', $$ RETURN 1 AS ends        $$) AS (a agtype);
SELECT * FROM cypher('issue_2355', $$ RETURN 1 AS in          $$) AS (a agtype);
SELECT * FROM cypher('issue_2355', $$ RETURN 1 AS is          $$) AS (a agtype);
SELECT * FROM cypher('issue_2355', $$ RETURN 1 AS not         $$) AS (a agtype);
SELECT * FROM cypher('issue_2355', $$ RETURN 1 AS yield       $$) AS (a agtype);
SELECT * FROM cypher('issue_2355', $$ RETURN 1 AS call        $$) AS (a agtype);

-- Multiple keyword aliases in one projection.
SELECT * FROM cypher('issue_2355',
    $$ RETURN 1 AS count, 2 AS exists, 3 AS where $$
) AS (count agtype, ex agtype, w agtype);

-- WITH ... AS <safe_keyword>: alias binding works.
SELECT * FROM cypher('issue_2355',
    $$ WITH 1 AS count RETURN 1 AS x $$
) AS (a agtype);

-- UNWIND ... AS <safe_keyword>: alias binding works.
SELECT * FROM cypher('issue_2355',
    $$ UNWIND [1, 2, 3] AS row RETURN 1 AS x $$
) AS (a agtype);

-- conflicted_keywords (END, NULL, TRUE, FALSE) MUST still be rejected
-- because they introduce real grammar ambiguity with literal/expression
-- productions.
SELECT * FROM cypher('issue_2355', $$ RETURN 1 AS null  $$) AS (a agtype);
SELECT * FROM cypher('issue_2355', $$ RETURN 1 AS true  $$) AS (a agtype);
SELECT * FROM cypher('issue_2355', $$ RETURN 1 AS false $$) AS (a agtype);
SELECT * FROM cypher('issue_2355', $$ RETURN 1 AS end   $$) AS (a agtype);

-- Pattern-variable positions are intentionally NOT broadened (would
-- create shift/reduce conflicts). Confirm they still error.
SELECT * FROM cypher('issue_2355',
    $$ MATCH (count) RETURN 1 AS x $$
) AS (a agtype);

-- Plain identifiers naturally remain unaffected.
SELECT * FROM cypher('issue_2355', $$ RETURN 1 AS my_alias $$) AS (a agtype);

-- Backtick-quoted alias positive case: forces the IDENTIFIER token
-- path, so future grammar refactors don't accidentally regress quoted
-- identifiers when the unquoted form is also a keyword.
SELECT * FROM cypher('issue_2355', $$ RETURN 1 AS `count` $$) AS (a agtype);
SELECT * FROM cypher('issue_2355', $$ WITH 1 AS `count` RETURN `count` $$) AS (a agtype);

-- Known limitation: reading a keyword-named alias back fails because
-- expr_var reads through var_name (which is unchanged here). Tracked
-- in issue #2416. Captured in the expected output so the next
-- contributor who fixes expr_var has a precise file to update.
SELECT * FROM cypher('issue_2355', $$ WITH 1 AS count RETURN count $$) AS (a agtype);

SELECT * FROM drop_graph('issue_2355', true);
