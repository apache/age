/*-------------------------------------------------------------------------
 *
 * parser.h
 *		Definitions for the "raw" parser (flex and bison phases only)
 *
 * This is the external API for the raw lexing/parsing functions.
 *
 * Portions Copyright (c) 2003-2022, PgPool Global Development Group
 * Portions Copyright (c) 1996-2022, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * src/include/parser/parser.h
 *
 *-------------------------------------------------------------------------
 */
#ifndef PARSER_H
#define PARSER_H

#include "parsenodes.h"


/*
 * RawParseMode determines the form of the string that raw_parser() accepts:
 *
 * RAW_PARSE_DEFAULT: parse a semicolon-separated list of SQL commands,
 * and return a List of RawStmt nodes.
 *
 * RAW_PARSE_TYPE_NAME: parse a type name, and return a one-element List
 * containing a TypeName node.
 *
 * RAW_PARSE_PLPGSQL_EXPR: parse a PL/pgSQL expression, and return
 * a one-element List containing a RawStmt node.
 *
 * RAW_PARSE_PLPGSQL_ASSIGNn: parse a PL/pgSQL assignment statement,
 * and return a one-element List containing a RawStmt node.  "n"
 * gives the number of dotted names comprising the target ColumnRef.
 */
typedef enum
{
	RAW_PARSE_DEFAULT = 0,
	RAW_PARSE_TYPE_NAME,
	RAW_PARSE_PLPGSQL_EXPR,
	RAW_PARSE_PLPGSQL_ASSIGN1,
	RAW_PARSE_PLPGSQL_ASSIGN2,
	RAW_PARSE_PLPGSQL_ASSIGN3
} RawParseMode;

/* Values for the backslash_quote GUC */
typedef enum
{
	BACKSLASH_QUOTE_OFF,
	BACKSLASH_QUOTE_ON,
	BACKSLASH_QUOTE_SAFE_ENCODING
}			BackslashQuoteType;

/* GUC variables in scan.l (every one of these is a bad idea :-() */
extern PGDLLIMPORT int backslash_quote;
extern PGDLLIMPORT bool escape_string_warning;
extern PGDLLIMPORT bool standard_conforming_strings;

/* Primary entry point for the raw parsing functions */
extern List *raw_parser(const char *str, RawParseMode mode, int len, bool *error, bool use_minimal);
extern Node *raw_parser2(List *parse_tree_list);

/* from src/backend/commands/define.c */
extern int32 defGetInt32(DefElem *def);

/* Utility functions exported by gram.y (perhaps these should be elsewhere) */
extern List *SystemFuncName(char *name);
extern TypeName *SystemTypeName(char *name);

extern void parser_set_param(const char *name, const char *value);

extern Node *makeTypeCast(Node *arg, TypeName *typename, int location);
extern Node *makeStringConstCast(char *str, int location, TypeName *typename);
extern Node *makeIntConst(int val, int location);

extern List *get_dummy_write_query_tree(void);
extern List *get_dummy_read_query_tree(void);
extern Node * get_dummy_insert_query_node(void);
#endif							/* PARSER_H */
