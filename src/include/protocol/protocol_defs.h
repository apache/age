/*
 * Copyright (c) 2003-2018	PgPool Global Development Group
 *
 * Permission to use, copy, modify, and distribute this software and
 * its documentation for any purpose and without fee is hereby
 * granted, provided that the above copyright notice appear in all
 * copies and that both that copyright notice and this permission
 * notice appear in supporting documentation, and that the name of the
 * author not be used in advertising or publicity pertaining to
 * distribution of the software without specific, written prior
 * permission. The author makes no representations about the
 * suitability of this software for any purpose.  It is provided "as
 * is" without express or implied warranty.
 *
*/

#ifndef pgpool_protocol_defs_h
#define pgpool_protocol_defs_h

#define SEQUENCETABLEQUERY (Pgversion(backend)->major >= 73 ? \
	"SELECT pg_get_expr(d.adbin, d.adrelid) FROM pg_catalog.pg_class c, pg_catalog.pg_attribute a LEFT JOIN pg_catalog.pg_attrdef d ON (a.attrelid = d.adrelid AND a.attnum = d.adnum) WHERE c.oid = a.attrelid AND a.attnum >= 1 AND a.attisdropped = 'f' AND c.relname = '%s' AND d.pg_get_expr(d.adbin, d.adrelid) ~ 'nextval'" : \
	"SELECT d.adsrc FROM pg_catalog.pg_class c, pg_catalog.pg_attribute a LEFT JOIN pg_catalog.pg_attrdef d ON (a.attrelid = d.adrelid AND a.attnum = d.adnum) WHERE c.oid = a.attrelid AND a.attnum >= 1 AND a.attisdropped = 'f' AND c.relname = '%s' AND d.adsrc ~ 'nextval'")

#define SEQUENCETABLEQUERY2 (Pgversion(backend)->major >= 73 ? \
	"SELECT pg_get_expr(d.adbin, d.adrelid) FROM pg_catalog.pg_class c, pg_catalog.pg_attribute a LEFT JOIN pg_catalog.pg_attrdef d ON (a.attrelid = d.adrelid AND a.attnum = d.adnum) WHERE c.oid = a.attrelid AND a.attnum >= 1 AND a.attisdropped = 'f' AND c.oid = pgpool_regclass('%s') AND pg_get_expr(d.adbin, d.adrelid) ~ 'nextval'" : \
	"SELECT d.adsrc FROM pg_catalog.pg_class c, pg_catalog.pg_attribute a LEFT JOIN pg_catalog.pg_attrdef d ON (a.attrelid = d.adrelid AND a.attnum = d.adnum) WHERE c.oid = a.attrelid AND a.attnum >= 1 AND a.attisdropped = 'f' AND c.oid = pgpool_regclass('%s') AND d.adsrc ~ 'nextval'")

#define SEQUENCETABLEQUERY3 (Pgversion(backend)->major >= 73 ? \
	"SELECT pg_get_expr(d.adbin, d.adrelid) FROM pg_catalog.pg_class c, pg_catalog.pg_attribute a LEFT JOIN pg_catalog.pg_attrdef d ON (a.attrelid = d.adrelid AND a.attnum = d.adnum) WHERE c.oid = a.attrelid AND a.attnum >= 1 AND a.attisdropped = 'f' AND c.oid = pg_catalog.to_regclass('%s') AND pg_get_expr(d.adbin, d.adrelid) ~ 'nextval'" : \
	"SELECT d.adsrc FROM pg_catalog.pg_class c, pg_catalog.pg_attribute a LEFT JOIN pg_catalog.pg_attrdef d ON (a.attrelid = d.adrelid AND a.attnum = d.adnum) WHERE c.oid = a.attrelid AND a.attnum >= 1 AND a.attisdropped = 'f' AND c.oid = pg_catalog.to_regclass('%s') AND d.adsrc ~ 'nextval'")

/* query to lock a row by only the specified table name without regard to the schema */
#define ROWLOCKQUERY "SELECT 1 FROM pgpool_catalog.insert_lock WHERE reloid = (SELECT oid FROM pg_catalog.pg_class WHERE relname = '%s' ORDER BY oid LIMIT 1) FOR UPDATE"

#define ROWLOCKQUERY2 "SELECT 1 FROM pgpool_catalog.insert_lock WHERE reloid = pgpool_regclass('%s') FOR UPDATE"

#define ROWLOCKQUERY3 "SELECT 1 FROM pgpool_catalog.insert_lock WHERE reloid = pg_catalog.to_regclass('%s') FOR UPDATE"

#define MAX_NAME_LEN 128


#define HASINSERT_LOCKQUERY "SELECT count(*) FROM pg_catalog.pg_class c JOIN pg_catalog.pg_namespace n ON (c.relnamespace = n.oid) WHERE nspname = 'pgpool_catalog' AND relname = '%s'"


#endif
