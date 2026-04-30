# Licensed to the Apache Software Foundation (ASF) under one
# or more contributor license agreements.  See the NOTICE file
# distributed with this work for additional information
# regarding copyright ownership.  The ASF licenses this file
# to you under the Apache License, Version 2.0 (the
# "License"); you may not use this file except in compliance
# with the License.  You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing,
# software distributed under the License is distributed on an
# "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
# KIND, either express or implied.  See the License for the
# specific language governing permissions and limitations
# under the License.

MODULE_big = age

age_sql = age--1.7.0.sql

# --- Extension upgrade regression test support ---
#
# Validates the upgrade template (age--<VER>--y.y.y.sql) by simulating an
# extension version upgrade entirely within "make installcheck". The test:
#
#   1. Builds the default install SQL (age--<CURR>.sql) from current HEAD's
#      sql/sql_files. This is what CREATE EXTENSION age installs.
#   2. Builds a synthetic "initial" version install SQL from the version-bump
#      commit in git history. This represents the pre-upgrade state.
#   3. Stamps the upgrade template to upgrade from the initial version to the
#      current version.
#   4. Temporarily installs the synthetic files into the PG extension directory
#      so that CREATE EXTENSION age VERSION '<INIT>' and ALTER EXTENSION
#      age UPDATE TO '<CURR>' can find them.
#   5. The age_upgrade regression test snapshots the ag_catalog schema from
#      a fresh install, then installs at INIT, upgrades to CURR, and compares
#      the catalog across seven system catalogs to detect missing, extra,
#      or changed objects.
#   6. The test SQL cleans up the synthetic files via a generated shell script.
#
# This forces developers to keep the upgrade template in sync: any SQL object
# added after the version-bump commit must also appear in the template, or the
# upgrade test will fail (the object will be missing after ALTER EXTENSION UPDATE).
#
# Because the default install SQL comes from HEAD, all non-upgrade tests
# run with every SQL function registered — no functions are missing.
#
# Graceful degradation — the upgrade test is silently skipped when:
#   - No git history (tarball build): AGE_VER_COMMIT is empty.
#   - No upgrade template: age--<CURR>--y.y.y.sql does not exist.
#   - A real (git-tracked) upgrade script from <CURR> already exists
#     (e.g., age--1.7.0--1.8.0.sql is committed): the synthetic test is
#     redundant because the real script ships with the extension.
# Current version from age.control (e.g., "1.7.0")
AGE_CURR_VER := $(shell awk -F"'" '/default_version/ {print $$2}' age.control 2>/dev/null)
# Git commit that last changed age.control — the "initial release" commit
AGE_VER_COMMIT := $(shell git log -1 --format=%H -- age.control 2>/dev/null)
# Synthetic initial version: current version with _initial suffix
AGE_INIT_VER := $(AGE_CURR_VER)_initial
# The upgrade template file (e.g., age--1.7.0--y.y.y.sql); empty if not present
AGE_UPGRADE_TEMPLATE := $(wildcard age--$(AGE_CURR_VER)--y.y.y.sql)

# Synthetic filenames — these are NOT installed permanently; they are temporarily
# placed in $(SHAREDIR)/extension/ during installcheck and removed after.
age_init_sql = $(if $(AGE_INIT_VER),age--$(AGE_INIT_VER).sql)
age_upgrade_test_sql = $(if $(AGE_INIT_VER),age--$(AGE_INIT_VER)--$(AGE_CURR_VER).sql)

# Real (git-tracked, non-template) upgrade scripts FROM the current version.
# If any exist (e.g., age--1.7.0--1.8.0.sql is committed), the synthetic
# upgrade test is redundant because a real upgrade path already ships.
# Uses git ls-files so untracked synthetic files are NOT matched.
AGE_REAL_UPGRADE := $(shell git ls-files 'age--$(AGE_CURR_VER)--*.sql' 2>/dev/null | grep -v 'y\.y\.y')

# Non-empty when ALL of these hold:
#   1. Git history is available (AGE_VER_COMMIT non-empty)
#   2. The upgrade template exists (AGE_UPGRADE_TEMPLATE non-empty)
#   3. No real upgrade script from current version exists (AGE_REAL_UPGRADE empty)
# When a real upgrade script ships, the test is skipped — the real script
# supersedes the synthetic one and has its own validation path.
AGE_HAS_UPGRADE_TEST = $(and $(AGE_VER_COMMIT),$(AGE_UPGRADE_TEMPLATE),$(if $(AGE_REAL_UPGRADE),,yes))

OBJS = src/backend/age.o \
       src/backend/catalog/ag_catalog.o \
       src/backend/catalog/ag_graph.o \
       src/backend/catalog/ag_label.o \
       src/backend/catalog/ag_namespace.o \
       src/backend/commands/graph_commands.o \
       src/backend/commands/label_commands.o \
       src/backend/executor/cypher_create.o \
       src/backend/executor/cypher_merge.o \
       src/backend/executor/cypher_set.o \
       src/backend/executor/cypher_utils.o \
       src/backend/nodes/ag_nodes.o \
       src/backend/nodes/cypher_copyfuncs.o \
       src/backend/nodes/cypher_outfuncs.o \
       src/backend/nodes/cypher_readfuncs.o \
       src/backend/optimizer/cypher_createplan.o \
       src/backend/optimizer/cypher_pathnode.o \
       src/backend/optimizer/cypher_paths.o \
       src/backend/parser/ag_scanner.o \
       src/backend/parser/cypher_analyze.o \
       src/backend/parser/cypher_clause.o \
       src/backend/executor/cypher_delete.o \
       src/backend/parser/cypher_expr.o \
       src/backend/parser/cypher_gram.o \
       src/backend/parser/cypher_item.o \
       src/backend/parser/cypher_keywords.o \
       src/backend/parser/cypher_parse_agg.o \
       src/backend/parser/cypher_parse_node.o \
       src/backend/parser/cypher_parser.o \
       src/backend/parser/cypher_transform_entity.o \
       src/backend/utils/adt/age_graphid_ds.o \
       src/backend/utils/adt/agtype.o \
       src/backend/utils/adt/agtype_ext.o \
       src/backend/utils/adt/agtype_gin.o \
       src/backend/utils/adt/agtype_ops.o \
       src/backend/utils/adt/agtype_parser.o \
       src/backend/utils/adt/agtype_util.o \
       src/backend/utils/adt/agtype_raw.o \
       src/backend/utils/adt/age_global_graph.o \
       src/backend/utils/adt/age_session_info.o \
       src/backend/utils/adt/age_vle.o \
       src/backend/utils/adt/cypher_funcs.o \
       src/backend/utils/adt/ag_float8_supp.o \
       src/backend/utils/adt/graphid.o \
       src/backend/utils/ag_func.o \
       src/backend/utils/graph_generation.o \
       src/backend/utils/cache/ag_cache.o \
       src/backend/utils/load/ag_load_labels.o \
       src/backend/utils/load/ag_load_edges.o \
       src/backend/utils/load/age_load.o \
       src/backend/utils/name_validation.o \
       src/backend/utils/ag_guc.o

EXTENSION = age

# to allow cleaning of previous (old) age--.sql files
all_age_sql = $(shell find . -maxdepth 1 -type f -regex './age--[0-9]+\.[0-9]+\.[0-9]+\.sql')

SQLS := $(shell cat sql/sql_files)
SQLS := $(addprefix sql/,$(SQLS))
SQLS := $(addsuffix .sql,$(SQLS))

DATA_built = $(age_sql)

# Git-tracked upgrade scripts shipped with the extension (e.g., age--1.6.0--1.7.0.sql).
# Excludes the upgrade template (y.y.y) and the synthetic stamped test file.
DATA = $(filter-out age--%-y.y.y.sql $(age_upgrade_test_sql),$(wildcard age--*--*.sql))

# sorted in dependency order
REGRESS = scan \
          graphid \
          agtype \
          agtype_hash_cmp \
          catalog \
          cypher \
          expr \
          cypher_create \
          cypher_match \
          cypher_unwind \
          cypher_set \
          cypher_remove \
          cypher_delete \
          cypher_with \
          cypher_vle \
          cypher_union \
          cypher_call \
          cypher_merge \
          cypher_subquery \
          age_global_graph \
          age_load \
          index \
          analyze \
          graph_generation \
          name_validation \
          jsonb_operators \
          list_comprehension \
          predicate_functions \
          map_projection \
          direct_field_access \
          security \
          reserved_keyword_alias

ifneq ($(EXTRA_TESTS),)
  REGRESS += $(EXTRA_TESTS)
endif

# Extension upgrade test — included when git history is available, the upgrade
# template exists, and no real upgrade script from the current version is
# committed. Runs between "security" and "drop" in test order.
ifneq ($(AGE_HAS_UPGRADE_TEST),)
  REGRESS += age_upgrade
endif

REGRESS += drop

srcdir=`pwd`

ag_regress_dir = $(srcdir)/regress
REGRESS_OPTS = --load-extension=age --inputdir=$(ag_regress_dir) --outputdir=$(ag_regress_dir) --temp-instance=$(ag_regress_dir)/instance --port=61958 --encoding=UTF-8 --temp-config $(ag_regress_dir)/age_regression.conf

ag_regress_out = instance/ log/ results/ regression.*
EXTRA_CLEAN = $(addprefix $(ag_regress_dir)/, $(ag_regress_out)) src/backend/parser/cypher_gram.c src/include/parser/cypher_gram_def.h src/include/parser/cypher_kwlist_d.h $(all_age_sql) $(age_init_sql) $(age_upgrade_test_sql) $(ag_regress_dir)/age_upgrade_cleanup.sh

GEN_KEYWORDLIST = $(PERL) -I ./tools/ ./tools/gen_keywordlist.pl
GEN_KEYWORDLIST_DEPS = ./tools/gen_keywordlist.pl tools/PerfectHash.pm

ag_include_dir = $(srcdir)/src/include
PG_CPPFLAGS = -I$(ag_include_dir) -I$(ag_include_dir)/parser

PG_CONFIG ?= pg_config
PGXS := $(shell $(PG_CONFIG) --pgxs)
include $(PGXS)

# 32-bit platform support: pass SIZEOF_DATUM=4 to enable (e.g., make SIZEOF_DATUM=4)
# When SIZEOF_DATUM=4, PASSEDBYVALUE is stripped from graphid type for pass-by-reference.
# If not specified, normal 64-bit behavior is used (PASSEDBYVALUE preserved).

src/backend/parser/cypher_keywords.o: src/include/parser/cypher_kwlist_d.h

src/include/parser/cypher_kwlist_d.h: src/include/parser/cypher_kwlist.h $(GEN_KEYWORDLIST_DEPS)
	$(GEN_KEYWORDLIST) --extern --varname CypherKeyword --output src/include/parser $<

src/include/parser/cypher_gram_def.h: src/backend/parser/cypher_gram.c

src/backend/parser/cypher_gram.c: BISONFLAGS += --defines=src/include/parser/cypher_gram_def.h -Werror

src/backend/parser/cypher_parser.o: src/backend/parser/cypher_gram.c src/include/parser/cypher_gram_def.h
src/backend/parser/cypher_parser.bc: src/backend/parser/cypher_gram.c src/include/parser/cypher_gram_def.h
src/backend/parser/cypher_keywords.o: src/backend/parser/cypher_gram.c src/include/parser/cypher_gram_def.h
src/backend/parser/cypher_keywords.bc: src/backend/parser/cypher_gram.c src/include/parser/cypher_gram_def.h

# Build the default install SQL (age--<VER>.sql) from current HEAD's sql/sql_files.
# This is what CREATE EXTENSION age installs — it contains ALL current functions.
# All 31 non-upgrade regression tests run against this complete SQL.
$(age_sql): $(SQLS)
	@echo "Building install SQL: $@ from HEAD"
	@cat $(SQLS) > $@
ifeq ($(SIZEOF_DATUM),4)
	@echo "32-bit build: removing PASSEDBYVALUE from graphid type"
	@sed 's/^  PASSEDBYVALUE,$$/  -- PASSEDBYVALUE removed for 32-bit (see Makefile)/' $@ > $@.tmp && mv $@.tmp $@
endif

# Build synthetic "initial" version install SQL from the version-bump commit.
# This represents the pre-upgrade state — the SQL at the time the version was
# bumped in age.control. Used only by the upgrade test.
ifneq ($(AGE_HAS_UPGRADE_TEST),)
$(age_init_sql): age.control
	@echo "Building synthetic initial version install SQL: $@ from commit $(AGE_VER_COMMIT)"
	@for f in $$(git show $(AGE_VER_COMMIT):sql/sql_files 2>/dev/null); do \
		git show $(AGE_VER_COMMIT):sql/$${f}.sql 2>/dev/null; \
	done > $@
ifeq ($(SIZEOF_DATUM),4)
	@sed 's/^  PASSEDBYVALUE,$$/  -- PASSEDBYVALUE removed for 32-bit (see Makefile)/' $@ > $@.tmp && mv $@.tmp $@
endif

# Stamp upgrade template as upgrade from initial to current version
$(age_upgrade_test_sql): $(AGE_UPGRADE_TEMPLATE)
	@echo "Stamping upgrade template: $< -> $@ ($(AGE_INIT_VER) -> $(AGE_CURR_VER))"
	@sed -e "s/1\.X\.0/$(AGE_CURR_VER)/g" -e "s/y\.y\.y/$(AGE_CURR_VER)/g" $< > $@
endif

src/backend/parser/ag_scanner.c: FLEX_NO_BACKUP=yes

# --- Upgrade test file lifecycle during installcheck ---
#
# Problem: The upgrade test needs age--<INIT>.sql and age--<INIT>--<CURR>.sql
# in the PG extension directory for CREATE EXTENSION VERSION and ALTER
# EXTENSION UPDATE to find them, but we must not leave them installed
# permanently (they would confuse users).
#
# Solution: A Make prerequisite installs them before pg_regress runs, and the
# test SQL removes them at the end via \! (shell escape in psql). A generated
# cleanup script (regress/age_upgrade_cleanup.sh) contains the exact absolute
# paths so the removal works regardless of the working directory. EXTRA_CLEAN
# also removes them on "make clean" as a safety net.
#
# This adds a prerequisite to "installcheck" but does NOT override the PGXS
# recipe, so there are no Makefile warnings.
SHAREDIR = $(shell $(PG_CONFIG) --sharedir)

installcheck: export LC_COLLATE=C
ifneq ($(AGE_HAS_UPGRADE_TEST),)
.PHONY: _install_upgrade_test_files
_install_upgrade_test_files: $(age_init_sql) $(age_upgrade_test_sql)  ## Build, install synthetic files, generate cleanup script
	@echo "Installing upgrade test files to $(SHAREDIR)/extension/"
	@$(INSTALL_DATA) $(age_init_sql) $(age_upgrade_test_sql) '$(SHAREDIR)/extension/'
	@printf '#!/bin/sh\nrm -f "$(SHAREDIR)/extension/$(age_init_sql)" "$(SHAREDIR)/extension/$(age_upgrade_test_sql)"\nrm -f "$(age_init_sql)" "$(age_upgrade_test_sql)" "$(ag_regress_dir)/age_upgrade_cleanup.sh"\n' > $(ag_regress_dir)/age_upgrade_cleanup.sh
	@chmod +x $(ag_regress_dir)/age_upgrade_cleanup.sh

installcheck: _install_upgrade_test_files
endif
