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
    ALL = 270,
    ANALYZE = 271,
    AND = 272,
    AS = 273,
    ASC = 274,
    ASCENDING = 275,
    BY = 276,
    CALL = 277,
    CASE = 278,
    COALESCE = 279,
    CONTAINS = 280,
    CREATE = 281,
    DELETE = 282,
    DESC = 283,
    DESCENDING = 284,
    DETACH = 285,
    DISTINCT = 286,
    ELSE = 287,
    END_P = 288,
    ENDS = 289,
    EXISTS = 290,
    EXPLAIN = 291,
    FALSE_P = 292,
    IN = 293,
    IS = 294,
    LIMIT = 295,
    MATCH = 296,
    MERGE = 297,
    NOT = 298,
    NULL_P = 299,
    OPTIONAL = 300,
    OR = 301,
    ORDER = 302,
    REMOVE = 303,
    RETURN = 304,
    SET = 305,
    SKIP = 306,
    STARTS = 307,
    THEN = 308,
    TRUE_P = 309,
    UNION = 310,
    UNWIND = 311,
    VERBOSE = 312,
    WHEN = 313,
    WHERE = 314,
    WITH = 315,
    XOR = 316,
    YIELD = 317,
    UNARY_MINUS = 318
  };
#endif

/* Value type.  */
#if ! defined YYSTYPE && ! defined YYSTYPE_IS_DECLARED
union YYSTYPE
{
#line 58 "src/age/cypher_gram.y"

    /* types in cypher_yylex() */
    int integer;
    char *string;
    const char *keyword;

    /* extra types */
    bool boolean;
    Node *node;
    List *list;

#line 133 "src/include/age/cypher_gram_def.h"

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
