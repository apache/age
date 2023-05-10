/* A Bison parser, made by GNU Bison 2.3.  */

/* Skeleton interface for Bison's Yacc-like parsers in C

   Copyright (C) 1984, 1989, 1990, 2000, 2001, 2002, 2003, 2004, 2005, 2006
   Free Software Foundation, Inc.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor,
   Boston, MA 02110-1301, USA.  */

/* As a special exception, you may create a larger work that contains
   part or all of the Bison parser skeleton and distribute that work
   under terms of your choice, so long as that work isn't itself a
   parser generator using the skeleton or a modified version thereof
   as a parser skeleton.  Alternatively, if you modify or redistribute
   the parser skeleton itself, you may (at your option) remove this
   special exception, which will cause the skeleton and the resulting
   Bison output files to be licensed under the GNU General Public
   License without this special exception.

   This special exception was added by the Free Software Foundation in
   version 2.2 of Bison.  */

/* Tokens.  */
#ifndef YYTOKENTYPE
# define YYTOKENTYPE
   /* Put the tokens into the symbol table, so that GDB and other debuggers
      know about them.  */
   enum yytokentype {
     ASC = 258,
     DESC = 259,
     DASH = 260,
     LT = 261,
     GT = 262,
     LBRACKET = 263,
     RBRACKET = 264,
     LPAREN = 265,
     RPAREN = 266,
     COLON = 267,
     PIPE = 268,
     COMMA = 269,
     SEMICOLON = 270,
     LBRACE = 271,
     RBRACE = 272,
     ASTERISK = 273,
     DOT = 274,
     MATCH = 275,
     WHERE = 276,
     WITH = 277,
     ORDER = 278,
     BY = 279,
     SKIP = 280,
     LIMIT = 281,
     RETURN = 282,
     AS = 283,
     AND = 284,
     OR = 285,
     XOR = 286,
     NOT = 287,
     exit_command = 288,
     INTEGER = 289,
     IDENTIFIER = 290,
     STRING = 291,
     COMPARATOR = 292,
     UNKNOWN = 293,
     ARROW = 294
   };
#endif
/* Tokens.  */
#define ASC 258
#define DESC 259
#define DASH 260
#define LT 261
#define GT 262
#define LBRACKET 263
#define RBRACKET 264
#define LPAREN 265
#define RPAREN 266
#define COLON 267
#define PIPE 268
#define COMMA 269
#define SEMICOLON 270
#define LBRACE 271
#define RBRACE 272
#define ASTERISK 273
#define DOT 274
#define MATCH 275
#define WHERE 276
#define WITH 277
#define ORDER 278
#define BY 279
#define SKIP 280
#define LIMIT 281
#define RETURN 282
#define AS 283
#define AND 284
#define OR 285
#define XOR 286
#define NOT 287
#define exit_command 288
#define INTEGER 289
#define IDENTIFIER 290
#define STRING 291
#define COMPARATOR 292
#define UNKNOWN 293
#define ARROW 294




#if ! defined YYSTYPE && ! defined YYSTYPE_IS_DECLARED
typedef union YYSTYPE
#line 47 "cypher.y"
{
    char* str_val;
    int int_val;
    bool bool_val;
    struct edge_pattern* pat;
    struct map_pair* pair;
}
/* Line 1529 of yacc.c.  */
#line 135 "cypher.tab.h"
	YYSTYPE;
# define yystype YYSTYPE /* obsolescent; will be withdrawn */
# define YYSTYPE_IS_DECLARED 1
# define YYSTYPE_IS_TRIVIAL 1
#endif

extern YYSTYPE yylval;

