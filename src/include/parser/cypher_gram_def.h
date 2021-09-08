/* A Bison parser, made by GNU Bison 3.5.1.  */

/* Bison interface for Yacc-like parsers in C

   Copyright (C) 1984, 1989-1990, 2000-2015, 2018-2020 Free Software Foundation,
   Inc.

   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.  */

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

/* Undocumented macros, especially those whose name start with YY_,
   are private implementation details.  Do not rely on them.  */

#ifndef YY_CYPHER_YY_SRC_INCLUDE_PARSER_CYPHER_GRAM_DEF_H_INCLUDED
# define YY_CYPHER_YY_SRC_INCLUDE_PARSER_CYPHER_GRAM_DEF_H_INCLUDED
/* Debug traces.  */
#ifndef YYDEBUG
# define YYDEBUG 0
#endif
#if YYDEBUG
extern int cypher_yydebug;
#endif

/* Token type.  */
#ifndef YYTOKENTYPE
# define YYTOKENTYPE
  enum yytokentype
  {
    INTEGER = 258,
    DECIMAL = 259,
    STRING = 260,
    IDENTIFIER = 261,
    PARAMETER = 262,
    NOT_EQ = 263,
    LT_EQ = 264,
    GT_EQ = 265,
    DOT_DOT = 266,
    TYPECAST = 267,
    PLUS_EQ = 268,
    EQ_TILDE = 269,
    ANALYZE = 270,
    AND = 271,
    AS = 272,
    ASC = 273,
    ASCENDING = 274,
    BY = 275,
    CASE = 276,
    COALESCE = 277,
    CONTAINS = 278,
    CREATE = 279,
    DELETE = 280,
    DESC = 281,
    DESCENDING = 282,
    DETACH = 283,
    DISTINCT = 284,
    ELSE = 285,
    END_P = 286,
    ENDS = 287,
    EXISTS = 288,
    EXPLAIN = 289,
    FALSE_P = 290,
    IN = 291,
    IS = 292,
    LIMIT = 293,
    MATCH = 294,
    NOT = 295,
    NULL_P = 296,
    OR = 297,
    ORDER = 298,
    REMOVE = 299,
    RETURN = 300,
    SET = 301,
    SKIP = 302,
    STARTS = 303,
    THEN = 304,
    TRUE_P = 305,
    VERBOSE = 306,
    WHEN = 307,
    WHERE = 308,
    WITH = 309,
    UNARY_MINUS = 310
  };
#endif

/* Value type.  */
#if ! defined YYSTYPE && ! defined YYSTYPE_IS_DECLARED
union YYSTYPE
{
#line 58 "src/backend/parser/cypher_gram.y"

    /* types in cypher_yylex() */
    int integer;
    char *string;
    const char *keyword;

    /* extra types */
    bool boolean;
    Node *node;
    List *list;

#line 125 "src/include/parser/cypher_gram_def.h"

};
typedef union YYSTYPE YYSTYPE;
# define YYSTYPE_IS_TRIVIAL 1
# define YYSTYPE_IS_DECLARED 1
#endif

/* Location type.  */
#if ! defined YYLTYPE && ! defined YYLTYPE_IS_DECLARED
typedef struct YYLTYPE YYLTYPE;
struct YYLTYPE
{
  int first_line;
  int first_column;
  int last_line;
  int last_column;
};
# define YYLTYPE_IS_DECLARED 1
# define YYLTYPE_IS_TRIVIAL 1
#endif



int cypher_yyparse (ag_scanner_t scanner, cypher_yy_extra *extra);

#endif /* !YY_CYPHER_YY_SRC_INCLUDE_PARSER_CYPHER_GRAM_DEF_H_INCLUDED  */
