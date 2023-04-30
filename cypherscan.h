/*
 * psql - the PostgreSQL interactive terminal
 *
 * Copyright (c) 2000-2022, PostgreSQL Global Development Group
 *
 * src/bin/psql/cypherscan.h
 */
#ifndef CYPHERSACN_H
#define CYPHERSCAN_H

#include "fe_utils/psqlscan.h"
void psql_scan_cypher_command(PsqlScanState state);

#endif   /* CYPHERSCAN_H */
