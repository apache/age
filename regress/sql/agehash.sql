/*
 * agehash internal selftest.
 *
 * Exercises the Robin Hood open-addressing hashtable used by AGE's hot-path
 * caches at boundary sizes (1, 7, 8, 9, 63, 64, 65, 1024, ...) and across
 * grow thresholds. A success returns the literal "OK"; any failure returns
 * "FAIL: <reason>" describing the offending case.
 */
SELECT ag_catalog._agehash_self_test();
