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
     ARROW = 259,
     AS = 260,
     DESC = 261,
     LBRACKET = 262,
     RBRACKET = 263,
     LPAREN = 264,
     RPAREN = 265,
     COLON = 266,
     PIPE = 267,
     COMMA = 268,
     SEMICOLON = 269,
     LBRACE = 270,
     RBRACE = 271,
     MATCH = 272,
     WHERE = 273,
     WITH = 274,
     ORDER = 275,
     BY = 276,
     SKIP = 277,
     LIMIT = 278,
     RETURN = 279,
     INTEGER = 280,
     IDENTIFIER = 281,
     STRING = 282,
     UNKNOWN = 283
   };
#endif
/* Tokens.  */
#define ASC 258
#define ARROW 259
#define AS 260
#define DESC 261
#define LBRACKET 262
#define RBRACKET 263
#define LPAREN 264
#define RPAREN 265
#define COLON 266
#define PIPE 267
#define COMMA 268
#define SEMICOLON 269
#define LBRACE 270
#define RBRACE 271
#define MATCH 272
#define WHERE 273
#define WITH 274
#define ORDER 275
#define BY 276
#define SKIP 277
#define LIMIT 278
#define RETURN 279
#define INTEGER 280
#define IDENTIFIER 281
#define STRING 282
#define UNKNOWN 283




#if ! defined YYSTYPE && ! defined YYSTYPE_IS_DECLARED
typedef union YYSTYPE
#line 49 "cypher.y"
{
    char* str_val;
    int int_val;
}
/* Line 1529 of yacc.c.  */
#line 110 "cypher.tab.h"
	YYSTYPE;
# define yystype YYSTYPE /* obsolescent; will be withdrawn */
# define YYSTYPE_IS_DECLARED 1
# define YYSTYPE_IS_TRIVIAL 1
#endif

extern YYSTYPE yylval;

