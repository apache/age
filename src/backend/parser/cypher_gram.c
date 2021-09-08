/* A Bison parser, made by GNU Bison 3.5.1.  */

/* Bison implementation for Yacc-like parsers in C

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

/* C LALR(1) parser skeleton written by Richard Stallman, by
   simplifying the original so-called "semantic" parser.  */

/* All symbols defined below should begin with yy or YY, to avoid
   infringing on user name space.  This should be done even for local
   variables, as they might otherwise be expanded by user macros.
   There are some unavoidable exceptions within include files to
   define necessary library symbols; they are noted "INFRINGES ON
   USER NAME SPACE" below.  */

/* Undocumented macros, especially those whose name start with YY_,
   are private implementation details.  Do not rely on them.  */

/* Identify Bison output.  */
#define YYBISON 1

/* Bison version.  */
#define YYBISON_VERSION "3.5.1"

/* Skeleton name.  */
#define YYSKELETON_NAME "yacc.c"

/* Pure parsers.  */
#define YYPURE 1

/* Push parsers.  */
#define YYPUSH 0

/* Pull parsers.  */
#define YYPULL 1


/* Substitute the variable and function names.  */
#define yyparse         cypher_yyparse
#define yylex           cypher_yylex
#define yyerror         cypher_yyerror
#define yydebug         cypher_yydebug
#define yynerrs         cypher_yynerrs

/* First part of user prologue.  */
#line 1 "src/backend/parser/cypher_gram.y"

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

#include "postgres.h"

#include "nodes/makefuncs.h"
#include "nodes/nodes.h"
#include "nodes/parsenodes.h"
#include "nodes/pg_list.h"
#include "nodes/primnodes.h"
#include "nodes/value.h"
#include "parser/parser.h"

#include "nodes/ag_nodes.h"
#include "nodes/cypher_nodes.h"
#include "parser/ag_scanner.h"
#include "parser/cypher_gram.h"

// override the default action for locations
#define YYLLOC_DEFAULT(current, rhs, n) \
    do \
    { \
        if ((n) > 0) \
            current = (rhs)[1]; \
        else \
            current = -1; \
    } while (0)

#define YYMALLOC palloc
#define YYFREE pfree

#line 124 "src/backend/parser/cypher_gram.c"

# ifndef YY_CAST
#  ifdef __cplusplus
#   define YY_CAST(Type, Val) static_cast<Type> (Val)
#   define YY_REINTERPRET_CAST(Type, Val) reinterpret_cast<Type> (Val)
#  else
#   define YY_CAST(Type, Val) ((Type) (Val))
#   define YY_REINTERPRET_CAST(Type, Val) ((Type) (Val))
#  endif
# endif
# ifndef YY_NULLPTR
#  if defined __cplusplus
#   if 201103L <= __cplusplus
#    define YY_NULLPTR nullptr
#   else
#    define YY_NULLPTR 0
#   endif
#  else
#   define YY_NULLPTR ((void*)0)
#  endif
# endif

/* Enabling verbose error messages.  */
#ifdef YYERROR_VERBOSE
# undef YYERROR_VERBOSE
# define YYERROR_VERBOSE 1
#else
# define YYERROR_VERBOSE 0
#endif

/* Use api.header.include to #include this header
   instead of duplicating it here.  */
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

#line 244 "src/backend/parser/cypher_gram.c"

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

/* Second part of user prologue.  */
#line 163 "src/backend/parser/cypher_gram.y"

// logical operators
static Node *make_or_expr(Node *lexpr, Node *rexpr, int location);
static Node *make_and_expr(Node *lexpr, Node *rexpr, int location);
static Node *make_not_expr(Node *expr, int location);

// arithmetic operators
static Node *do_negate(Node *n, int location);
static void do_negate_float(Value *v);

// indirection
static Node *append_indirection(Node *expr, Node *selector);

// literals
static Node *make_int_const(int i, int location);
static Node *make_float_const(char *s, int location);
static Node *make_string_const(char *s, int location);
static Node *make_bool_const(bool b, int location);
static Node *make_null_const(int location);

// typecast
static Node *make_typecast_expr(Node *expr, char *typecast, int location);

// functions
static Node *make_function_expr(List *func_name, List *exprs, int location);

#line 300 "src/backend/parser/cypher_gram.c"


#ifdef short
# undef short
#endif

/* On compilers that do not define __PTRDIFF_MAX__ etc., make sure
   <limits.h> and (if available) <stdint.h> are included
   so that the code can choose integer types of a good width.  */

#ifndef __PTRDIFF_MAX__
# include <limits.h> /* INFRINGES ON USER NAME SPACE */
# if defined __STDC_VERSION__ && 199901 <= __STDC_VERSION__
#  include <stdint.h> /* INFRINGES ON USER NAME SPACE */
#  define YY_STDINT_H
# endif
#endif

/* Narrow types that promote to a signed type and that can represent a
   signed or unsigned integer of at least N bits.  In tables they can
   save space and decrease cache pressure.  Promoting to a signed type
   helps avoid bugs in integer arithmetic.  */

#ifdef __INT_LEAST8_MAX__
typedef __INT_LEAST8_TYPE__ yytype_int8;
#elif defined YY_STDINT_H
typedef int_least8_t yytype_int8;
#else
typedef signed char yytype_int8;
#endif

#ifdef __INT_LEAST16_MAX__
typedef __INT_LEAST16_TYPE__ yytype_int16;
#elif defined YY_STDINT_H
typedef int_least16_t yytype_int16;
#else
typedef short yytype_int16;
#endif

#if defined __UINT_LEAST8_MAX__ && __UINT_LEAST8_MAX__ <= __INT_MAX__
typedef __UINT_LEAST8_TYPE__ yytype_uint8;
#elif (!defined __UINT_LEAST8_MAX__ && defined YY_STDINT_H \
       && UINT_LEAST8_MAX <= INT_MAX)
typedef uint_least8_t yytype_uint8;
#elif !defined __UINT_LEAST8_MAX__ && UCHAR_MAX <= INT_MAX
typedef unsigned char yytype_uint8;
#else
typedef short yytype_uint8;
#endif

#if defined __UINT_LEAST16_MAX__ && __UINT_LEAST16_MAX__ <= __INT_MAX__
typedef __UINT_LEAST16_TYPE__ yytype_uint16;
#elif (!defined __UINT_LEAST16_MAX__ && defined YY_STDINT_H \
       && UINT_LEAST16_MAX <= INT_MAX)
typedef uint_least16_t yytype_uint16;
#elif !defined __UINT_LEAST16_MAX__ && USHRT_MAX <= INT_MAX
typedef unsigned short yytype_uint16;
#else
typedef int yytype_uint16;
#endif

#ifndef YYPTRDIFF_T
# if defined __PTRDIFF_TYPE__ && defined __PTRDIFF_MAX__
#  define YYPTRDIFF_T __PTRDIFF_TYPE__
#  define YYPTRDIFF_MAXIMUM __PTRDIFF_MAX__
# elif defined PTRDIFF_MAX
#  ifndef ptrdiff_t
#   include <stddef.h> /* INFRINGES ON USER NAME SPACE */
#  endif
#  define YYPTRDIFF_T ptrdiff_t
#  define YYPTRDIFF_MAXIMUM PTRDIFF_MAX
# else
#  define YYPTRDIFF_T long
#  define YYPTRDIFF_MAXIMUM LONG_MAX
# endif
#endif

#ifndef YYSIZE_T
# ifdef __SIZE_TYPE__
#  define YYSIZE_T __SIZE_TYPE__
# elif defined size_t
#  define YYSIZE_T size_t
# elif defined __STDC_VERSION__ && 199901 <= __STDC_VERSION__
#  include <stddef.h> /* INFRINGES ON USER NAME SPACE */
#  define YYSIZE_T size_t
# else
#  define YYSIZE_T unsigned
# endif
#endif

#define YYSIZE_MAXIMUM                                  \
  YY_CAST (YYPTRDIFF_T,                                 \
           (YYPTRDIFF_MAXIMUM < YY_CAST (YYSIZE_T, -1)  \
            ? YYPTRDIFF_MAXIMUM                         \
            : YY_CAST (YYSIZE_T, -1)))

#define YYSIZEOF(X) YY_CAST (YYPTRDIFF_T, sizeof (X))

/* Stored state numbers (used for stacks). */
typedef yytype_int16 yy_state_t;

/* State numbers in computations.  */
typedef int yy_state_fast_t;

#ifndef YY_
# if defined YYENABLE_NLS && YYENABLE_NLS
#  if ENABLE_NLS
#   include <libintl.h> /* INFRINGES ON USER NAME SPACE */
#   define YY_(Msgid) dgettext ("bison-runtime", Msgid)
#  endif
# endif
# ifndef YY_
#  define YY_(Msgid) Msgid
# endif
#endif

#ifndef YY_ATTRIBUTE_PURE
# if defined __GNUC__ && 2 < __GNUC__ + (96 <= __GNUC_MINOR__)
#  define YY_ATTRIBUTE_PURE __attribute__ ((__pure__))
# else
#  define YY_ATTRIBUTE_PURE
# endif
#endif

#ifndef YY_ATTRIBUTE_UNUSED
# if defined __GNUC__ && 2 < __GNUC__ + (7 <= __GNUC_MINOR__)
#  define YY_ATTRIBUTE_UNUSED __attribute__ ((__unused__))
# else
#  define YY_ATTRIBUTE_UNUSED
# endif
#endif

/* Suppress unused-variable warnings by "using" E.  */
#if ! defined lint || defined __GNUC__
# define YYUSE(E) ((void) (E))
#else
# define YYUSE(E) /* empty */
#endif

#if defined __GNUC__ && ! defined __ICC && 407 <= __GNUC__ * 100 + __GNUC_MINOR__
/* Suppress an incorrect diagnostic about yylval being uninitialized.  */
# define YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN                            \
    _Pragma ("GCC diagnostic push")                                     \
    _Pragma ("GCC diagnostic ignored \"-Wuninitialized\"")              \
    _Pragma ("GCC diagnostic ignored \"-Wmaybe-uninitialized\"")
# define YY_IGNORE_MAYBE_UNINITIALIZED_END      \
    _Pragma ("GCC diagnostic pop")
#else
# define YY_INITIAL_VALUE(Value) Value
#endif
#ifndef YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
# define YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
# define YY_IGNORE_MAYBE_UNINITIALIZED_END
#endif
#ifndef YY_INITIAL_VALUE
# define YY_INITIAL_VALUE(Value) /* Nothing. */
#endif

#if defined __cplusplus && defined __GNUC__ && ! defined __ICC && 6 <= __GNUC__
# define YY_IGNORE_USELESS_CAST_BEGIN                          \
    _Pragma ("GCC diagnostic push")                            \
    _Pragma ("GCC diagnostic ignored \"-Wuseless-cast\"")
# define YY_IGNORE_USELESS_CAST_END            \
    _Pragma ("GCC diagnostic pop")
#endif
#ifndef YY_IGNORE_USELESS_CAST_BEGIN
# define YY_IGNORE_USELESS_CAST_BEGIN
# define YY_IGNORE_USELESS_CAST_END
#endif


#define YY_ASSERT(E) ((void) (0 && (E)))

#if ! defined yyoverflow || YYERROR_VERBOSE

/* The parser invokes alloca or malloc; define the necessary symbols.  */

# ifdef YYSTACK_USE_ALLOCA
#  if YYSTACK_USE_ALLOCA
#   ifdef __GNUC__
#    define YYSTACK_ALLOC __builtin_alloca
#   elif defined __BUILTIN_VA_ARG_INCR
#    include <alloca.h> /* INFRINGES ON USER NAME SPACE */
#   elif defined _AIX
#    define YYSTACK_ALLOC __alloca
#   elif defined _MSC_VER
#    include <malloc.h> /* INFRINGES ON USER NAME SPACE */
#    define alloca _alloca
#   else
#    define YYSTACK_ALLOC alloca
#    if ! defined _ALLOCA_H && ! defined EXIT_SUCCESS
#     include <stdlib.h> /* INFRINGES ON USER NAME SPACE */
      /* Use EXIT_SUCCESS as a witness for stdlib.h.  */
#     ifndef EXIT_SUCCESS
#      define EXIT_SUCCESS 0
#     endif
#    endif
#   endif
#  endif
# endif

# ifdef YYSTACK_ALLOC
   /* Pacify GCC's 'empty if-body' warning.  */
#  define YYSTACK_FREE(Ptr) do { /* empty */; } while (0)
#  ifndef YYSTACK_ALLOC_MAXIMUM
    /* The OS might guarantee only one guard page at the bottom of the stack,
       and a page size can be as small as 4096 bytes.  So we cannot safely
       invoke alloca (N) if N exceeds 4096.  Use a slightly smaller number
       to allow for a few compiler-allocated temporary stack slots.  */
#   define YYSTACK_ALLOC_MAXIMUM 4032 /* reasonable circa 2006 */
#  endif
# else
#  define YYSTACK_ALLOC YYMALLOC
#  define YYSTACK_FREE YYFREE
#  ifndef YYSTACK_ALLOC_MAXIMUM
#   define YYSTACK_ALLOC_MAXIMUM YYSIZE_MAXIMUM
#  endif
#  if (defined __cplusplus && ! defined EXIT_SUCCESS \
       && ! ((defined YYMALLOC || defined malloc) \
             && (defined YYFREE || defined free)))
#   include <stdlib.h> /* INFRINGES ON USER NAME SPACE */
#   ifndef EXIT_SUCCESS
#    define EXIT_SUCCESS 0
#   endif
#  endif
#  ifndef YYMALLOC
#   define YYMALLOC malloc
#   if ! defined malloc && ! defined EXIT_SUCCESS
void *malloc (YYSIZE_T); /* INFRINGES ON USER NAME SPACE */
#   endif
#  endif
#  ifndef YYFREE
#   define YYFREE free
#   if ! defined free && ! defined EXIT_SUCCESS
void free (void *); /* INFRINGES ON USER NAME SPACE */
#   endif
#  endif
# endif
#endif /* ! defined yyoverflow || YYERROR_VERBOSE */


#if (! defined yyoverflow \
     && (! defined __cplusplus \
         || (defined YYLTYPE_IS_TRIVIAL && YYLTYPE_IS_TRIVIAL \
             && defined YYSTYPE_IS_TRIVIAL && YYSTYPE_IS_TRIVIAL)))

/* A type that is properly aligned for any stack member.  */
union yyalloc
{
  yy_state_t yyss_alloc;
  YYSTYPE yyvs_alloc;
  YYLTYPE yyls_alloc;
};

/* The size of the maximum gap between one aligned stack and the next.  */
# define YYSTACK_GAP_MAXIMUM (YYSIZEOF (union yyalloc) - 1)

/* The size of an array large to enough to hold all stacks, each with
   N elements.  */
# define YYSTACK_BYTES(N) \
     ((N) * (YYSIZEOF (yy_state_t) + YYSIZEOF (YYSTYPE) \
             + YYSIZEOF (YYLTYPE)) \
      + 2 * YYSTACK_GAP_MAXIMUM)

# define YYCOPY_NEEDED 1

/* Relocate STACK from its old location to the new one.  The
   local variables YYSIZE and YYSTACKSIZE give the old and new number of
   elements in the stack, and YYPTR gives the new location of the
   stack.  Advance YYPTR to a properly aligned location for the next
   stack.  */
# define YYSTACK_RELOCATE(Stack_alloc, Stack)                           \
    do                                                                  \
      {                                                                 \
        YYPTRDIFF_T yynewbytes;                                         \
        YYCOPY (&yyptr->Stack_alloc, Stack, yysize);                    \
        Stack = &yyptr->Stack_alloc;                                    \
        yynewbytes = yystacksize * YYSIZEOF (*Stack) + YYSTACK_GAP_MAXIMUM; \
        yyptr += yynewbytes / YYSIZEOF (*yyptr);                        \
      }                                                                 \
    while (0)

#endif

#if defined YYCOPY_NEEDED && YYCOPY_NEEDED
/* Copy COUNT objects from SRC to DST.  The source and destination do
   not overlap.  */
# ifndef YYCOPY
#  if defined __GNUC__ && 1 < __GNUC__
#   define YYCOPY(Dst, Src, Count) \
      __builtin_memcpy (Dst, Src, YY_CAST (YYSIZE_T, (Count)) * sizeof (*(Src)))
#  else
#   define YYCOPY(Dst, Src, Count)              \
      do                                        \
        {                                       \
          YYPTRDIFF_T yyi;                      \
          for (yyi = 0; yyi < (Count); yyi++)   \
            (Dst)[yyi] = (Src)[yyi];            \
        }                                       \
      while (0)
#  endif
# endif
#endif /* !YYCOPY_NEEDED */

/* YYFINAL -- State number of the termination state.  */
#define YYFINAL  8
/* YYLAST -- Last index in YYTABLE.  */
#define YYLAST   1396

/* YYNTOKENS -- Number of terminals.  */
#define YYNTOKENS  75
/* YYNNTS -- Number of nonterminals.  */
#define YYNNTS  76
/* YYNRULES -- Number of rules.  */
#define YYNRULES  215
/* YYNSTATES -- Number of states.  */
#define YYNSTATES  341

#define YYUNDEFTOK  2
#define YYMAXUTOK   310


/* YYTRANSLATE(TOKEN-NUM) -- Symbol number corresponding to TOKEN-NUM
   as returned by yylex, with out-of-bounds checking.  */
#define YYTRANSLATE(YYX)                                                \
  (0 <= (YYX) && (YYX) <= YYMAXUTOK ? yytranslate[YYX] : YYUNDEFTOK)

/* YYTRANSLATE[TOKEN-NUM] -- Symbol number corresponding to TOKEN-NUM
   as returned by yylex.  */
static const yytype_int8 yytranslate[] =
{
       0,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,    62,     2,     2,
      67,    68,    60,    58,    71,    59,    69,    61,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,    72,    70,
      56,    55,    57,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,    65,     2,    66,    63,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,    73,     2,    74,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     1,     2,     3,     4,
       5,     6,     7,     8,     9,    10,    11,    12,    13,    14,
      15,    16,    17,    18,    19,    20,    21,    22,    23,    24,
      25,    26,    27,    28,    29,    30,    31,    32,    33,    34,
      35,    36,    37,    38,    39,    40,    41,    42,    43,    44,
      45,    46,    47,    48,    49,    50,    51,    52,    53,    54,
      64
};

#if YYDEBUG
  /* YYRLINE[YYN] -- Source line where rule number YYN was defined.  */
static const yytype_int16 yyrline[] =
{
       0,   197,   197,   218,   232,   246,   260,   277,   279,   289,
     297,   300,   307,   311,   319,   322,   329,   334,   337,   341,
     345,   352,   353,   354,   355,   359,   379,   385,   394,   404,
     410,   417,   418,   421,   428,   441,   458,   462,   469,   481,
     493,   514,   517,   524,   528,   535,   552,   555,   559,   563,
     567,   575,   578,   586,   589,   596,   627,   666,   683,   699,
     713,   717,   724,   736,   751,   765,   769,   776,   794,   808,
     813,   824,   827,   839,   843,   851,   852,   865,   879,   880,
     887,   891,   898,   913,   922,   931,   943,   957,   972,   975,
     983,   986,   987,  1005,  1009,  1013,  1017,  1021,  1025,  1029,
    1033,  1037,  1041,  1045,  1049,  1053,  1057,  1061,  1065,  1069,
    1080,  1091,  1095,  1107,  1119,  1131,  1142,  1160,  1222,  1226,
    1231,  1234,  1238,  1242,  1250,  1253,  1257,  1258,  1262,  1266,
    1271,  1287,  1297,  1306,  1324,  1332,  1339,  1340,  1350,  1354,
    1355,  1356,  1360,  1364,  1368,  1372,  1376,  1380,  1384,  1385,
    1389,  1402,  1405,  1409,  1413,  1420,  1432,  1444,  1458,  1462,
    1469,  1482,  1487,  1493,  1509,  1519,  1526,  1530,  1535,  1538,
    1542,  1546,  1550,  1551,  1559,  1560,  1569,  1570,  1571,  1572,
    1573,  1574,  1575,  1576,  1577,  1578,  1579,  1580,  1581,  1582,
    1583,  1584,  1585,  1586,  1587,  1588,  1589,  1590,  1591,  1592,
    1593,  1594,  1595,  1596,  1597,  1598,  1599,  1600,  1601,  1602,
    1603,  1604,  1608,  1609,  1610,  1611
};
#endif

#if YYDEBUG || YYERROR_VERBOSE || 0
/* YYTNAME[SYMBOL-NUM] -- String name of the symbol SYMBOL-NUM.
   First, the terminals, then, starting at YYNTOKENS, nonterminals.  */
static const char *const yytname[] =
{
  "$end", "error", "$undefined", "INTEGER", "DECIMAL", "STRING",
  "IDENTIFIER", "PARAMETER", "NOT_EQ", "LT_EQ", "GT_EQ", "DOT_DOT",
  "TYPECAST", "PLUS_EQ", "EQ_TILDE", "ANALYZE", "AND", "AS", "ASC",
  "ASCENDING", "BY", "CASE", "COALESCE", "CONTAINS", "CREATE", "DELETE",
  "DESC", "DESCENDING", "DETACH", "DISTINCT", "ELSE", "END_P", "ENDS",
  "EXISTS", "EXPLAIN", "FALSE_P", "IN", "IS", "LIMIT", "MATCH", "NOT",
  "NULL_P", "OR", "ORDER", "REMOVE", "RETURN", "SET", "SKIP", "STARTS",
  "THEN", "TRUE_P", "VERBOSE", "WHEN", "WHERE", "WITH", "'='", "'<'",
  "'>'", "'+'", "'-'", "'*'", "'/'", "'%'", "'^'", "UNARY_MINUS", "'['",
  "']'", "'('", "')'", "'.'", "';'", "','", "':'", "'{'", "'}'", "$accept",
  "stmt", "semicolon_opt", "single_query", "query_part_init",
  "query_part_last", "reading_clause_list", "reading_clause",
  "updating_clause_list_0", "updating_clause_list_1", "updating_clause",
  "cypher_varlen_opt", "cypher_range_opt", "cypher_range_idx",
  "cypher_range_idx_opt", "Iconst", "return", "return_item_list",
  "return_item", "order_by_opt", "sort_item_list", "sort_item",
  "order_opt", "skip_opt", "limit_opt", "with", "match", "create", "set",
  "set_item_list", "set_item", "remove", "remove_item_list", "remove_item",
  "delete", "detach_opt", "where_opt", "pattern", "path", "anonymous_path",
  "simple_path_opt_parens", "simple_path", "path_node",
  "path_relationship", "path_relationship_body", "label_opt",
  "properties_opt", "expr", "expr_opt", "expr_list", "expr_list_opt",
  "expr_func", "expr_func_norm", "expr_func_subexpr", "property_value",
  "expr_atom", "expr_literal", "map", "map_keyval_list_opt",
  "map_keyval_list", "list", "expr_case", "expr_case_when_list",
  "expr_case_when", "expr_case_default", "expr_var", "func_name",
  "property_key_name", "var_name", "var_name_opt", "label_name",
  "symbolic_name", "schema_name", "reserved_keyword", "safe_keywords",
  "conflicted_keywords", YY_NULLPTR
};
#endif

# ifdef YYPRINT
/* YYTOKNUM[NUM] -- (External) token number corresponding to the
   (internal) symbol number NUM (which must be that of a token).  */
static const yytype_int16 yytoknum[] =
{
       0,   256,   257,   258,   259,   260,   261,   262,   263,   264,
     265,   266,   267,   268,   269,   270,   271,   272,   273,   274,
     275,   276,   277,   278,   279,   280,   281,   282,   283,   284,
     285,   286,   287,   288,   289,   290,   291,   292,   293,   294,
     295,   296,   297,   298,   299,   300,   301,   302,   303,   304,
     305,   306,   307,   308,   309,    61,    60,    62,    43,    45,
      42,    47,    37,    94,   310,    91,    93,    40,    41,    46,
      59,    44,    58,   123,   125
};
# endif

#define YYPACT_NINF (-291)

#define yypact_value_is_default(Yyn) \
  ((Yyn) == YYPACT_NINF)

#define YYTABLE_NINF (-165)

#define yytable_value_is_error(Yyn) \
  ((Yyn) == YYTABLE_NINF)

  /* YYPACT[STATE-NUM] -- Index in YYTABLE of the portion describing
     STATE-NUM.  */
static const yytype_int16 yypact[] =
{
      31,    -4,    14,   -40,  -291,    37,  -291,   -40,  -291,  -291,
    -291,  -291,   103,  -291,   -40,   -40,  -291,     6,  -291,     6,
     555,   555,  -291,     5,   152,  -291,  -291,  -291,  -291,  -291,
    -291,    56,   -40,  -291,  -291,  -291,     7,    16,  -291,  -291,
    -291,   -28,  -291,    39,  -291,   -32,  -291,  -291,  -291,  -291,
    -291,  -291,  -291,  -291,  -291,  -291,   620,    33,  -291,  -291,
    -291,  -291,  -291,  -291,  -291,  -291,  -291,    38,  -291,  -291,
    -291,  -291,  -291,  -291,   555,  -291,  -291,  -291,  -291,  -291,
    -291,  -291,  -291,  -291,  -291,  -291,  -291,  -291,  -291,   555,
     555,   555,  1342,    35,  -291,  1144,  -291,  -291,  -291,  -291,
    -291,  -291,  -291,  -291,  -291,    47,  -291,    49,    48,    50,
    -291,   730,   360,   425,  -291,  -291,  -291,   555,  -291,   112,
       2,  -291,    52,     6,    60,    55,    58,    59,   555,  -291,
     555,   788,   -14,  -291,   555,     6,  1243,    30,  1144,    51,
      66,   846,  -291,  -291,  -291,  -291,  -291,  -291,  -291,  -291,
      61,    65,    89,  -291,  -291,  -291,  -291,  -291,   555,   555,
     555,   555,   112,   555,   555,    83,   555,    -6,   555,   108,
     555,   555,   555,   555,   555,   555,   555,   555,   555,   555,
     555,   289,   112,   555,   555,   555,   490,  -291,   -23,  -291,
     908,   490,   -23,    51,  -291,  1342,     3,  -291,    55,   112,
     109,  -291,  -291,  1144,   966,   555,   -14,   555,  -291,   136,
      -8,   101,   102,   104,   555,  -291,  -291,  -291,  1342,   555,
    -291,  1284,  1284,  1284,  -291,  1243,    32,   555,   188,   133,
    -291,  1202,   555,  1284,  1284,  1284,   181,   181,    20,    20,
      20,  1208,  1024,   164,   166,   555,   111,  -291,    36,  -291,
    -291,  1144,  1086,   -23,   161,   490,   135,   112,   -23,   135,
    -291,  -291,  -291,   115,  -291,   125,    52,   129,   555,   157,
    1144,  -291,  -291,  -291,  -291,  1342,  1144,   118,  1144,    32,
    -291,    32,  -291,   555,    44,  -291,  -291,   135,   555,  -291,
     555,   163,  -291,   135,   163,  -291,  -291,   142,  -291,  1144,
    -291,  -291,   555,  1144,   137,  -291,   163,   124,  -291,   672,
    1144,   555,  -291,   163,   155,    22,     3,  1144,  -291,  -291,
     555,  -291,  -291,  -291,  -291,  -291,  1144,   155,  -291,  -291,
    -291,   199,   203,  -291,   149,  -291,  -291,   213,  -291,  -291,
    -291
};

  /* YYDEFACT[STATE-NUM] -- Default reduction number in state STATE-NUM.
     Performed when YYTABLE does not specify something else to do.  Zero
     means the default is an error.  */
static const yytype_uint8 yydefact[] =
{
      10,    10,     0,     7,    14,    10,    10,     7,     1,     8,
       2,     9,    17,    10,     7,     7,     3,     0,    69,     0,
       0,     0,    15,     0,    12,    19,    16,    21,    22,    23,
      24,     0,     7,     5,     4,   171,   168,    58,    73,    75,
      77,    78,    80,     0,   167,    71,   142,   143,   144,   137,
     177,   176,   178,   179,   180,   181,   182,   183,   184,   185,
     186,   187,   188,   189,   190,   191,   192,   193,   194,   146,
     195,   196,   197,   198,   199,   147,   200,   201,   202,   203,
     204,   205,   206,   207,   145,   209,   208,   210,   211,     0,
     124,     0,   151,    64,    65,    67,   141,   126,   127,   119,
     136,   148,   149,   139,   140,     0,   163,   167,     0,    59,
      60,     0,     0,     0,    13,    11,    20,     0,     6,   168,
       0,   169,    88,     0,     0,    87,     0,     0,     0,    57,
     208,     0,   162,   158,     0,     0,    95,   111,   122,   125,
       0,     0,   182,   183,   212,   193,   213,   199,   214,   215,
       0,   152,     0,   172,   166,   173,   174,   175,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,   120,
       0,     0,     0,     0,     0,     0,   190,    40,    41,    36,
      39,   190,    41,    68,    79,     0,    90,    74,    87,   168,
       0,    81,    76,    72,     0,     0,   162,     0,   159,     0,
       0,     0,     0,     0,     0,   155,   138,   150,     0,     0,
      66,    97,    99,   101,   118,    94,   114,     0,   108,     0,
     109,    93,     0,    96,    98,   100,   102,   103,   104,   105,
     106,   107,   121,     0,   117,   190,     0,   128,     0,   165,
      61,    63,    62,    41,     0,     0,    51,     0,    41,    51,
      89,   170,    92,     0,    91,     0,    88,    83,     0,     0,
     161,   157,   132,   133,   134,     0,   123,     0,   153,   113,
     110,   112,   115,   120,     0,   130,   129,    51,     0,    37,
       0,    53,    38,    51,    53,    82,    85,    26,    84,   160,
     156,   135,     0,   121,     0,   131,    53,    42,    43,    46,
      52,     0,    35,    53,    71,    29,    90,   154,   116,    34,
       0,    47,    48,    49,    50,    45,    54,    71,    56,    33,
      25,    27,     0,    30,     0,    44,    55,    32,    86,    31,
      28
};

  /* YYPGOTO[NTERM-NUM].  */
static const yytype_int16 yypgoto[] =
{
    -291,  -291,     8,    85,  -291,  -291,  -291,  -291,  -291,  -291,
     195,  -291,  -291,  -116,  -114,  -291,  -291,  -109,   -33,  -186,
    -291,   -94,  -291,  -242,  -210,  -291,  -291,  -291,  -291,  -291,
      54,  -291,  -291,    69,  -291,  -291,  -290,   211,   110,   -86,
    -291,   196,   105,  -291,    40,   -27,   -82,   -20,   -43,  -115,
    -291,  -291,  -291,  -291,  -291,  -291,  -291,  -193,  -291,  -291,
    -291,  -291,   114,  -127,    43,   116,  -291,  -200,   -10,    53,
    -291,    10,    63,  -291,   -84,  -291
};

  /* YYDEFGOTO[NTERM-NUM].  */
static const yytype_int16 yydefgoto[] =
{
      -1,     2,    10,     3,     4,    11,    12,    22,    23,    24,
      25,   316,   330,   331,   332,   333,   114,   188,   189,   256,
     307,   308,   325,   291,   312,   115,    26,    27,    28,   109,
     110,    29,    93,    94,    30,    31,   129,    37,    38,    39,
      40,    41,    42,   126,   200,   196,   263,   138,   243,   139,
     140,    96,    97,    98,   212,    99,   100,   101,   150,   151,
     102,   103,   132,   133,   209,   104,   105,   152,   106,   122,
     260,   107,   154,   155,   108,   157
};

  /* YYTABLE[YYPACT[STATE-NUM]] -- What to do in state STATE-NUM.  If
     positive, shift that token.  If negative, reduce the rule whose
     number is the opposite.  If YYTABLE_NINF, syntax error.  */
static const yytype_int16 yytable[] =
{
      95,   111,   193,   264,   192,   208,   259,    43,   156,    43,
     262,     5,    35,    35,     8,    16,   207,   294,   277,   210,
     254,   128,    33,    34,   328,   329,   121,    44,   124,    44,
       9,   125,   162,   -32,   229,   230,   131,   336,   205,   123,
     118,   202,   162,   164,   162,   306,    44,     6,   255,   211,
     112,   313,   165,   164,   136,  -165,   166,   167,   124,   113,
     272,   125,   165,   214,  -165,     1,   248,   287,   169,   137,
     194,   141,   293,    36,   119,   301,    92,   253,   169,   208,
    -165,   117,   258,   178,   314,   179,     7,   123,    13,   180,
      14,    15,   190,   190,   127,   179,   319,   179,    32,   180,
     134,   180,   153,   327,   286,   135,   158,   214,   203,   121,
     204,   156,   305,    43,   181,   214,  -164,   182,    35,   198,
     199,   183,   214,   264,   195,   119,    36,    17,   -70,    44,
     284,    18,   215,    44,   156,   217,   218,   227,    95,   221,
     222,   223,    19,   225,   226,    44,   228,    20,   231,    21,
     233,   234,   235,   236,   237,   238,   239,   240,   241,   242,
     244,   219,   232,   111,   251,   252,   190,   271,   267,   273,
     274,   190,   224,   275,   280,   283,    17,   -70,   162,   285,
      18,   288,   290,   295,   296,   204,   298,   270,   300,   121,
     302,   156,   249,   162,   276,   320,    20,   -18,    21,   278,
     162,   311,   315,   318,   164,   153,   -18,   279,   128,    44,
     -31,   164,   281,   165,   337,   338,   329,   166,   167,   116,
     165,   339,   289,   340,  -165,  -165,   335,   220,   153,   169,
      45,   201,   120,   197,   334,   190,   169,   250,   265,   297,
     304,   175,   176,   177,   178,   206,   179,   292,   299,   269,
     180,   213,   266,   179,     0,     0,     0,   180,   261,     0,
       0,     0,     0,   303,     0,     0,     0,    44,   309,     0,
     310,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,   317,     0,     0,   153,     0,     0,     0,     0,
       0,   326,    46,    47,    48,    35,    49,     0,     0,     0,
     309,     0,     0,     0,    50,    51,    52,    53,    54,    55,
      56,    57,    58,    59,    60,    61,    62,    63,   245,    65,
       0,    66,    67,    68,    69,    70,    71,    72,    73,    74,
      75,    76,    77,    78,    79,    80,    81,    82,    83,    84,
      85,    86,    87,    88,     0,     0,     0,     0,    89,   246,
       0,     0,     0,     0,    90,     0,    91,   247,     0,     0,
       0,     0,    92,    46,    47,    48,    35,    49,     0,     0,
       0,     0,     0,     0,     0,    50,    51,    52,    53,    54,
      55,    56,    57,    58,    59,    60,    61,    62,    63,   186,
      65,     0,    66,    67,    68,    69,    70,    71,    72,    73,
      74,    75,    76,    77,    78,    79,    80,    81,    82,    83,
      84,    85,    86,    87,    88,     0,     0,     0,     0,    89,
     187,     0,     0,     0,     0,    90,     0,    91,    46,    47,
      48,    35,    49,    92,     0,     0,     0,     0,     0,     0,
      50,    51,    52,    53,    54,    55,    56,    57,    58,    59,
      60,    61,    62,    63,   191,    65,     0,    66,    67,    68,
      69,    70,    71,    72,    73,    74,    75,    76,    77,    78,
      79,    80,    81,    82,    83,    84,    85,    86,    87,    88,
       0,     0,     0,     0,    89,   187,     0,     0,     0,     0,
      90,     0,    91,    46,    47,    48,    35,    49,    92,     0,
       0,     0,     0,     0,     0,    50,    51,    52,    53,    54,
      55,    56,    57,    58,    59,    60,    61,    62,    63,    64,
      65,     0,    66,    67,    68,    69,    70,    71,    72,    73,
      74,    75,    76,    77,    78,    79,    80,    81,    82,    83,
      84,    85,    86,    87,    88,     0,     0,     0,     0,    89,
     187,     0,     0,     0,     0,    90,     0,    91,    46,    47,
      48,    35,    49,    92,     0,     0,     0,     0,     0,     0,
      50,    51,    52,    53,    54,    55,    56,    57,    58,    59,
      60,    61,    62,    63,    64,    65,     0,    66,    67,    68,
      69,    70,    71,    72,    73,    74,    75,    76,    77,    78,
      79,    80,    81,    82,    83,    84,    85,    86,    87,    88,
       0,     0,     0,     0,    89,     0,     0,     0,     0,     0,
      90,     0,    91,    46,    47,    48,    35,    49,    92,     0,
       0,     0,     0,     0,     0,    50,    51,    52,    53,    54,
      55,    56,    57,    58,    59,    60,    61,    62,    63,    64,
      65,     0,    66,    67,    68,    69,    70,    71,    72,    73,
      74,    75,    76,    77,    78,    79,    80,    81,    82,    83,
      84,    85,   130,    87,    88,     0,     0,     0,     0,    89,
     159,   160,   161,     0,   162,    90,     0,    91,   163,     0,
     321,   322,     0,    92,     0,   164,     0,     0,   323,   324,
       0,     0,     0,     0,   165,     0,     0,     0,   166,   167,
       0,     0,     0,     0,   168,     0,     0,     0,     0,     0,
     169,     0,     0,     0,     0,     0,     0,   170,   171,   172,
     173,   174,   175,   176,   177,   178,     0,   179,   159,   160,
     161,   180,   162,   184,     0,     0,   163,     0,     0,     0,
       0,     0,     0,   164,     0,     0,     0,     0,     0,     0,
       0,     0,   165,     0,     0,     0,   166,   167,     0,     0,
       0,     0,   168,     0,     0,     0,     0,     0,   169,     0,
       0,     0,     0,     0,     0,   185,   171,   172,   173,   174,
     175,   176,   177,   178,     0,   179,   159,   160,   161,   180,
     162,     0,     0,     0,   163,     0,     0,     0,     0,     0,
       0,   164,     0,     0,     0,     0,     0,     0,     0,     0,
     165,     0,     0,     0,   166,   167,     0,     0,     0,     0,
     168,     0,     0,     0,     0,     0,   169,     0,     0,     0,
     205,     0,     0,   170,   171,   172,   173,   174,   175,   176,
     177,   178,     0,   179,   159,   160,   161,   180,   162,     0,
       0,     0,   163,     0,     0,     0,     0,     0,     0,   164,
       0,     0,     0,     0,     0,     0,     0,     0,   165,     0,
       0,     0,   166,   167,     0,     0,     0,     0,   168,     0,
       0,     0,     0,     0,   169,     0,     0,     0,     0,     0,
       0,   170,   171,   172,   173,   174,   175,   176,   177,   178,
       0,   179,     0,     0,   216,   180,   159,   160,   161,     0,
     162,     0,     0,     0,   163,   257,     0,     0,     0,     0,
       0,   164,     0,     0,     0,     0,     0,     0,     0,     0,
     165,     0,     0,     0,   166,   167,     0,     0,     0,     0,
     168,     0,     0,     0,     0,     0,   169,     0,     0,     0,
       0,     0,     0,   170,   171,   172,   173,   174,   175,   176,
     177,   178,     0,   179,   159,   160,   161,   180,   162,     0,
       0,     0,   163,     0,     0,     0,     0,     0,     0,   164,
       0,     0,     0,     0,     0,     0,     0,     0,   165,     0,
       0,     0,   166,   167,     0,     0,     0,     0,   168,     0,
       0,     0,     0,     0,   169,   268,     0,     0,     0,     0,
       0,   170,   171,   172,   173,   174,   175,   176,   177,   178,
       0,   179,   159,   160,   161,   180,   162,     0,     0,     0,
     163,     0,     0,     0,     0,     0,     0,   164,     0,     0,
       0,     0,     0,     0,     0,     0,   165,     0,     0,     0,
     166,   167,     0,     0,     0,     0,   168,     0,     0,     0,
       0,     0,   169,     0,     0,     0,     0,     0,     0,   170,
     171,   172,   173,   174,   175,   176,   177,   178,     0,   179,
     282,     0,     0,   180,  -165,  -165,  -165,     0,   162,   -96,
       0,     0,   -96,     0,     0,     0,     0,     0,     0,   164,
       0,     0,     0,     0,     0,     0,     0,     0,   165,     0,
       0,     0,   166,   167,     0,     0,     0,     0,   -96,     0,
       0,     0,     0,     0,   169,     0,     0,     0,     0,     0,
       0,  -165,  -165,  -165,   173,   174,   175,   176,   177,   178,
       0,   179,   159,   160,   161,   180,   162,     0,     0,     0,
     163,     0,     0,     0,     0,     0,     0,   164,     0,     0,
       0,     0,     0,     0,     0,     0,   165,     0,     0,     0,
     166,   167,     0,     0,     0,     0,   168,     0,     0,     0,
       0,     0,   169,     0,     0,     0,     0,     0,     0,   170,
     171,   172,   173,   174,   175,   176,   177,   178,     0,   179,
     159,   160,   161,   180,   162,     0,     0,     0,   163,     0,
     162,     0,     0,     0,     0,   164,     0,     0,     0,     0,
       0,   164,     0,     0,   165,     0,     0,     0,   166,   167,
     165,     0,     0,     0,   166,   167,     0,     0,     0,     0,
     169,   159,   160,   161,     0,   162,   169,   170,   171,   172,
     173,   174,   175,   176,   177,   178,   164,   179,     0,     0,
       0,   180,     0,   179,     0,   165,     0,   180,     0,   166,
     167,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,   169,  -165,  -165,  -165,     0,   162,     0,   170,   171,
     172,   173,   174,   175,   176,   177,   178,   164,   179,     0,
       0,     0,   180,     0,     0,     0,   165,     0,     0,     0,
     166,   167,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,   169,     0,     0,     0,     0,     0,     0,  -165,
    -165,  -165,   173,   174,   175,   176,   177,   178,    35,   179,
       0,     0,     0,   180,     0,     0,     0,    50,    51,    52,
      53,    54,    55,   142,   143,    58,    59,    60,    61,    62,
      63,    64,    65,   144,    66,   145,    68,   146,    70,    71,
      72,    73,   147,   148,    76,    77,    78,    79,    80,    81,
      82,    83,   149,    85,    86,    87,    88
};

static const yytype_int16 yycheck[] =
{
      20,    21,   117,   196,   113,   132,   192,    17,    92,    19,
       7,    15,     6,     6,     0,     7,    30,   259,   218,   134,
      43,    53,    14,    15,   314,     3,    36,    17,    56,    19,
      70,    59,    12,    11,    40,    41,    56,   327,    52,    71,
      32,   127,    12,    23,    12,   287,    36,    51,    71,   135,
      45,   293,    32,    23,    74,    23,    36,    37,    56,    54,
      68,    59,    32,    71,    32,    34,   181,   253,    48,    89,
      68,    91,   258,    67,    67,   275,    73,   186,    48,   206,
      48,    25,   191,    63,   294,    65,     1,    71,    51,    69,
       5,     6,   112,   113,    55,    65,   306,    65,    13,    69,
      67,    69,    92,   313,    68,    67,    71,    71,   128,   119,
     130,   195,    68,   123,    67,    71,    67,    69,     6,    59,
      65,    71,    71,   316,    72,    67,    67,    24,    25,   119,
     245,    28,    66,   123,   218,    74,    71,    54,   158,   159,
     160,   161,    39,   163,   164,   135,   166,    44,   168,    46,
     170,   171,   172,   173,   174,   175,   176,   177,   178,   179,
     180,    72,    54,   183,   184,   185,   186,    31,    59,    68,
      68,   191,   162,    69,    41,    11,    24,    25,    12,    68,
      28,    20,    47,    68,    59,   205,    57,   207,    31,   199,
      72,   275,   182,    12,   214,    71,    44,    45,    46,   219,
      12,    38,    60,    66,    23,   195,    54,   227,    53,   199,
      11,    23,   232,    32,    11,    66,     3,    36,    37,    24,
      32,   337,   255,   337,    36,    37,   320,   158,   218,    48,
      19,   126,    36,   123,   316,   255,    48,   183,   198,   266,
     283,    60,    61,    62,    63,   131,    65,   257,   268,   206,
      69,   135,   199,    65,    -1,    -1,    -1,    69,   195,    -1,
      -1,    -1,    -1,   283,    -1,    -1,    -1,   257,   288,    -1,
     290,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,   302,    -1,    -1,   275,    -1,    -1,    -1,    -1,
      -1,   311,     3,     4,     5,     6,     7,    -1,    -1,    -1,
     320,    -1,    -1,    -1,    15,    16,    17,    18,    19,    20,
      21,    22,    23,    24,    25,    26,    27,    28,    29,    30,
      -1,    32,    33,    34,    35,    36,    37,    38,    39,    40,
      41,    42,    43,    44,    45,    46,    47,    48,    49,    50,
      51,    52,    53,    54,    -1,    -1,    -1,    -1,    59,    60,
      -1,    -1,    -1,    -1,    65,    -1,    67,    68,    -1,    -1,
      -1,    -1,    73,     3,     4,     5,     6,     7,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    15,    16,    17,    18,    19,
      20,    21,    22,    23,    24,    25,    26,    27,    28,    29,
      30,    -1,    32,    33,    34,    35,    36,    37,    38,    39,
      40,    41,    42,    43,    44,    45,    46,    47,    48,    49,
      50,    51,    52,    53,    54,    -1,    -1,    -1,    -1,    59,
      60,    -1,    -1,    -1,    -1,    65,    -1,    67,     3,     4,
       5,     6,     7,    73,    -1,    -1,    -1,    -1,    -1,    -1,
      15,    16,    17,    18,    19,    20,    21,    22,    23,    24,
      25,    26,    27,    28,    29,    30,    -1,    32,    33,    34,
      35,    36,    37,    38,    39,    40,    41,    42,    43,    44,
      45,    46,    47,    48,    49,    50,    51,    52,    53,    54,
      -1,    -1,    -1,    -1,    59,    60,    -1,    -1,    -1,    -1,
      65,    -1,    67,     3,     4,     5,     6,     7,    73,    -1,
      -1,    -1,    -1,    -1,    -1,    15,    16,    17,    18,    19,
      20,    21,    22,    23,    24,    25,    26,    27,    28,    29,
      30,    -1,    32,    33,    34,    35,    36,    37,    38,    39,
      40,    41,    42,    43,    44,    45,    46,    47,    48,    49,
      50,    51,    52,    53,    54,    -1,    -1,    -1,    -1,    59,
      60,    -1,    -1,    -1,    -1,    65,    -1,    67,     3,     4,
       5,     6,     7,    73,    -1,    -1,    -1,    -1,    -1,    -1,
      15,    16,    17,    18,    19,    20,    21,    22,    23,    24,
      25,    26,    27,    28,    29,    30,    -1,    32,    33,    34,
      35,    36,    37,    38,    39,    40,    41,    42,    43,    44,
      45,    46,    47,    48,    49,    50,    51,    52,    53,    54,
      -1,    -1,    -1,    -1,    59,    -1,    -1,    -1,    -1,    -1,
      65,    -1,    67,     3,     4,     5,     6,     7,    73,    -1,
      -1,    -1,    -1,    -1,    -1,    15,    16,    17,    18,    19,
      20,    21,    22,    23,    24,    25,    26,    27,    28,    29,
      30,    -1,    32,    33,    34,    35,    36,    37,    38,    39,
      40,    41,    42,    43,    44,    45,    46,    47,    48,    49,
      50,    51,    52,    53,    54,    -1,    -1,    -1,    -1,    59,
       8,     9,    10,    -1,    12,    65,    -1,    67,    16,    -1,
      18,    19,    -1,    73,    -1,    23,    -1,    -1,    26,    27,
      -1,    -1,    -1,    -1,    32,    -1,    -1,    -1,    36,    37,
      -1,    -1,    -1,    -1,    42,    -1,    -1,    -1,    -1,    -1,
      48,    -1,    -1,    -1,    -1,    -1,    -1,    55,    56,    57,
      58,    59,    60,    61,    62,    63,    -1,    65,     8,     9,
      10,    69,    12,    13,    -1,    -1,    16,    -1,    -1,    -1,
      -1,    -1,    -1,    23,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    32,    -1,    -1,    -1,    36,    37,    -1,    -1,
      -1,    -1,    42,    -1,    -1,    -1,    -1,    -1,    48,    -1,
      -1,    -1,    -1,    -1,    -1,    55,    56,    57,    58,    59,
      60,    61,    62,    63,    -1,    65,     8,     9,    10,    69,
      12,    -1,    -1,    -1,    16,    -1,    -1,    -1,    -1,    -1,
      -1,    23,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      32,    -1,    -1,    -1,    36,    37,    -1,    -1,    -1,    -1,
      42,    -1,    -1,    -1,    -1,    -1,    48,    -1,    -1,    -1,
      52,    -1,    -1,    55,    56,    57,    58,    59,    60,    61,
      62,    63,    -1,    65,     8,     9,    10,    69,    12,    -1,
      -1,    -1,    16,    -1,    -1,    -1,    -1,    -1,    -1,    23,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    32,    -1,
      -1,    -1,    36,    37,    -1,    -1,    -1,    -1,    42,    -1,
      -1,    -1,    -1,    -1,    48,    -1,    -1,    -1,    -1,    -1,
      -1,    55,    56,    57,    58,    59,    60,    61,    62,    63,
      -1,    65,    -1,    -1,    68,    69,     8,     9,    10,    -1,
      12,    -1,    -1,    -1,    16,    17,    -1,    -1,    -1,    -1,
      -1,    23,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      32,    -1,    -1,    -1,    36,    37,    -1,    -1,    -1,    -1,
      42,    -1,    -1,    -1,    -1,    -1,    48,    -1,    -1,    -1,
      -1,    -1,    -1,    55,    56,    57,    58,    59,    60,    61,
      62,    63,    -1,    65,     8,     9,    10,    69,    12,    -1,
      -1,    -1,    16,    -1,    -1,    -1,    -1,    -1,    -1,    23,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    32,    -1,
      -1,    -1,    36,    37,    -1,    -1,    -1,    -1,    42,    -1,
      -1,    -1,    -1,    -1,    48,    49,    -1,    -1,    -1,    -1,
      -1,    55,    56,    57,    58,    59,    60,    61,    62,    63,
      -1,    65,     8,     9,    10,    69,    12,    -1,    -1,    -1,
      16,    -1,    -1,    -1,    -1,    -1,    -1,    23,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    32,    -1,    -1,    -1,
      36,    37,    -1,    -1,    -1,    -1,    42,    -1,    -1,    -1,
      -1,    -1,    48,    -1,    -1,    -1,    -1,    -1,    -1,    55,
      56,    57,    58,    59,    60,    61,    62,    63,    -1,    65,
      66,    -1,    -1,    69,     8,     9,    10,    -1,    12,    13,
      -1,    -1,    16,    -1,    -1,    -1,    -1,    -1,    -1,    23,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    32,    -1,
      -1,    -1,    36,    37,    -1,    -1,    -1,    -1,    42,    -1,
      -1,    -1,    -1,    -1,    48,    -1,    -1,    -1,    -1,    -1,
      -1,    55,    56,    57,    58,    59,    60,    61,    62,    63,
      -1,    65,     8,     9,    10,    69,    12,    -1,    -1,    -1,
      16,    -1,    -1,    -1,    -1,    -1,    -1,    23,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    32,    -1,    -1,    -1,
      36,    37,    -1,    -1,    -1,    -1,    42,    -1,    -1,    -1,
      -1,    -1,    48,    -1,    -1,    -1,    -1,    -1,    -1,    55,
      56,    57,    58,    59,    60,    61,    62,    63,    -1,    65,
       8,     9,    10,    69,    12,    -1,    -1,    -1,    16,    -1,
      12,    -1,    -1,    -1,    -1,    23,    -1,    -1,    -1,    -1,
      -1,    23,    -1,    -1,    32,    -1,    -1,    -1,    36,    37,
      32,    -1,    -1,    -1,    36,    37,    -1,    -1,    -1,    -1,
      48,     8,     9,    10,    -1,    12,    48,    55,    56,    57,
      58,    59,    60,    61,    62,    63,    23,    65,    -1,    -1,
      -1,    69,    -1,    65,    -1,    32,    -1,    69,    -1,    36,
      37,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    48,     8,     9,    10,    -1,    12,    -1,    55,    56,
      57,    58,    59,    60,    61,    62,    63,    23,    65,    -1,
      -1,    -1,    69,    -1,    -1,    -1,    32,    -1,    -1,    -1,
      36,    37,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    48,    -1,    -1,    -1,    -1,    -1,    -1,    55,
      56,    57,    58,    59,    60,    61,    62,    63,     6,    65,
      -1,    -1,    -1,    69,    -1,    -1,    -1,    15,    16,    17,
      18,    19,    20,    21,    22,    23,    24,    25,    26,    27,
      28,    29,    30,    31,    32,    33,    34,    35,    36,    37,
      38,    39,    40,    41,    42,    43,    44,    45,    46,    47,
      48,    49,    50,    51,    52,    53,    54
};

  /* YYSTOS[STATE-NUM] -- The (internal number of the) accessing
     symbol of state STATE-NUM.  */
static const yytype_uint8 yystos[] =
{
       0,    34,    76,    78,    79,    15,    51,    78,     0,    70,
      77,    80,    81,    51,    78,    78,    77,    24,    28,    39,
      44,    46,    82,    83,    84,    85,   101,   102,   103,   106,
     109,   110,    78,    77,    77,     6,    67,   112,   113,   114,
     115,   116,   117,   143,   146,   112,     3,     4,     5,     7,
      15,    16,    17,    18,    19,    20,    21,    22,    23,    24,
      25,    26,    27,    28,    29,    30,    32,    33,    34,    35,
      36,    37,    38,    39,    40,    41,    42,    43,    44,    45,
      46,    47,    48,    49,    50,    51,    52,    53,    54,    59,
      65,    67,    73,   107,   108,   122,   126,   127,   128,   130,
     131,   132,   135,   136,   140,   141,   143,   146,   149,   104,
     105,   122,    45,    54,    91,   100,    85,    25,    77,    67,
     116,   143,   144,    71,    56,    59,   118,    55,    53,   111,
      52,   122,   137,   138,    67,    67,   122,   122,   122,   124,
     125,   122,    21,    22,    31,    33,    35,    40,    41,    50,
     133,   134,   142,   146,   147,   148,   149,   150,    71,     8,
       9,    10,    12,    16,    23,    32,    36,    37,    42,    48,
      55,    56,    57,    58,    59,    60,    61,    62,    63,    65,
      69,    67,    69,    71,    13,    55,    29,    60,    92,    93,
     122,    29,    92,   124,    68,    72,   120,   113,    59,    65,
     119,   117,   114,   122,   122,    52,   137,    30,   138,   139,
     124,   114,   129,   140,    71,    66,    68,    74,    71,    72,
     108,   122,   122,   122,   146,   122,   122,    54,   122,    40,
      41,   122,    54,   122,   122,   122,   122,   122,   122,   122,
     122,   122,   122,   123,   122,    29,    60,    68,   124,   146,
     105,   122,   122,    92,    43,    71,    94,    17,    92,    94,
     145,   147,     7,   121,   132,   119,   144,    59,    49,   139,
     122,    31,    68,    68,    68,    69,   122,   142,   122,   122,
      41,   122,    66,    11,   124,    68,    68,    94,    20,    93,
      47,    98,   143,    94,    98,    68,    59,   120,    57,   122,
      31,   142,    72,   122,   123,    68,    98,    95,    96,   122,
     122,    38,    99,    98,    99,    60,    86,   122,    66,    99,
      71,    18,    19,    26,    27,    97,   122,    99,   111,     3,
      87,    88,    89,    90,   121,    96,   111,    11,    66,    88,
      89
};

  /* YYR1[YYN] -- Symbol number of symbol that rule YYN derives.  */
static const yytype_uint8 yyr1[] =
{
       0,    75,    76,    76,    76,    76,    76,    77,    77,    78,
      79,    79,    80,    80,    81,    81,    82,    83,    83,    84,
      84,    85,    85,    85,    85,    86,    86,    87,    87,    87,
      88,    89,    89,    90,    91,    91,    92,    92,    93,    93,
      93,    94,    94,    95,    95,    96,    97,    97,    97,    97,
      97,    98,    98,    99,    99,   100,   100,   101,   102,   103,
     104,   104,   105,   105,   106,   107,   107,   108,   109,   110,
     110,   111,   111,   112,   112,   113,   113,   114,   115,   115,
     116,   116,   117,   118,   118,   118,   119,   119,   120,   120,
     121,   121,   121,   122,   122,   122,   122,   122,   122,   122,
     122,   122,   122,   122,   122,   122,   122,   122,   122,   122,
     122,   122,   122,   122,   122,   122,   122,   122,   122,   122,
     123,   123,   124,   124,   125,   125,   126,   126,   127,   127,
     127,   127,   128,   128,   128,   129,   130,   130,   130,   130,
     130,   130,   131,   131,   131,   131,   131,   131,   131,   131,
     132,   133,   133,   134,   134,   135,   136,   136,   137,   137,
     138,   139,   139,   140,   141,   141,   142,   143,   144,   144,
     145,   146,   147,   147,   148,   148,   149,   149,   149,   149,
     149,   149,   149,   149,   149,   149,   149,   149,   149,   149,
     149,   149,   149,   149,   149,   149,   149,   149,   149,   149,
     149,   149,   149,   149,   149,   149,   149,   149,   149,   149,
     149,   149,   150,   150,   150,   150
};

  /* YYR2[YYN] -- Number of symbols on the right hand side of rule YYN.  */
static const yytype_int8 yyr2[] =
{
       0,     2,     2,     3,     4,     4,     5,     0,     1,     2,
       0,     4,     2,     3,     0,     2,     1,     0,     1,     1,
       2,     1,     1,     1,     1,     2,     0,     1,     3,     0,
       1,     1,     0,     1,     6,     5,     1,     3,     3,     1,
       1,     0,     3,     1,     3,     2,     0,     1,     1,     1,
       1,     0,     2,     0,     2,     7,     6,     3,     2,     2,
       1,     3,     3,     3,     2,     1,     3,     1,     3,     1,
       0,     0,     2,     1,     3,     1,     3,     1,     1,     3,
       1,     3,     5,     3,     4,     4,     6,     0,     0,     2,
       0,     1,     1,     3,     3,     2,     3,     3,     3,     3,
       3,     3,     3,     3,     3,     3,     3,     3,     3,     3,
       4,     2,     4,     4,     3,     4,     6,     3,     3,     1,
       0,     1,     1,     3,     0,     1,     1,     1,     3,     4,
       4,     5,     4,     4,     4,     3,     1,     1,     3,     1,
       1,     1,     1,     1,     1,     1,     1,     1,     1,     1,
       3,     0,     1,     3,     5,     3,     5,     4,     1,     2,
       4,     2,     0,     1,     1,     3,     1,     1,     0,     1,
       1,     1,     1,     1,     1,     1,     1,     1,     1,     1,
       1,     1,     1,     1,     1,     1,     1,     1,     1,     1,
       1,     1,     1,     1,     1,     1,     1,     1,     1,     1,
       1,     1,     1,     1,     1,     1,     1,     1,     1,     1,
       1,     1,     1,     1,     1,     1
};


#define yyerrok         (yyerrstatus = 0)
#define yyclearin       (yychar = YYEMPTY)
#define YYEMPTY         (-2)
#define YYEOF           0

#define YYACCEPT        goto yyacceptlab
#define YYABORT         goto yyabortlab
#define YYERROR         goto yyerrorlab


#define YYRECOVERING()  (!!yyerrstatus)

#define YYBACKUP(Token, Value)                                    \
  do                                                              \
    if (yychar == YYEMPTY)                                        \
      {                                                           \
        yychar = (Token);                                         \
        yylval = (Value);                                         \
        YYPOPSTACK (yylen);                                       \
        yystate = *yyssp;                                         \
        goto yybackup;                                            \
      }                                                           \
    else                                                          \
      {                                                           \
        yyerror (&yylloc, scanner, extra, YY_("syntax error: cannot back up")); \
        YYERROR;                                                  \
      }                                                           \
  while (0)

/* Error token number */
#define YYTERROR        1
#define YYERRCODE       256


/* YYLLOC_DEFAULT -- Set CURRENT to span from RHS[1] to RHS[N].
   If N is 0, then set CURRENT to the empty location which ends
   the previous symbol: RHS[0] (always defined).  */

#ifndef YYLLOC_DEFAULT
# define YYLLOC_DEFAULT(Current, Rhs, N)                                \
    do                                                                  \
      if (N)                                                            \
        {                                                               \
          (Current).first_line   = YYRHSLOC (Rhs, 1).first_line;        \
          (Current).first_column = YYRHSLOC (Rhs, 1).first_column;      \
          (Current).last_line    = YYRHSLOC (Rhs, N).last_line;         \
          (Current).last_column  = YYRHSLOC (Rhs, N).last_column;       \
        }                                                               \
      else                                                              \
        {                                                               \
          (Current).first_line   = (Current).last_line   =              \
            YYRHSLOC (Rhs, 0).last_line;                                \
          (Current).first_column = (Current).last_column =              \
            YYRHSLOC (Rhs, 0).last_column;                              \
        }                                                               \
    while (0)
#endif

#define YYRHSLOC(Rhs, K) ((Rhs)[K])


/* Enable debugging if requested.  */
#if YYDEBUG

# ifndef YYFPRINTF
#  include <stdio.h> /* INFRINGES ON USER NAME SPACE */
#  define YYFPRINTF fprintf
# endif

# define YYDPRINTF(Args)                        \
do {                                            \
  if (yydebug)                                  \
    YYFPRINTF Args;                             \
} while (0)


/* YY_LOCATION_PRINT -- Print the location on the stream.
   This macro was not mandated originally: define only if we know
   we won't break user code: when these are the locations we know.  */

#ifndef YY_LOCATION_PRINT
# if defined YYLTYPE_IS_TRIVIAL && YYLTYPE_IS_TRIVIAL

/* Print *YYLOCP on YYO.  Private, do not rely on its existence. */

YY_ATTRIBUTE_UNUSED
static int
yy_location_print_ (FILE *yyo, YYLTYPE const * const yylocp)
{
  int res = 0;
  int end_col = 0 != yylocp->last_column ? yylocp->last_column - 1 : 0;
  if (0 <= yylocp->first_line)
    {
      res += YYFPRINTF (yyo, "%d", yylocp->first_line);
      if (0 <= yylocp->first_column)
        res += YYFPRINTF (yyo, ".%d", yylocp->first_column);
    }
  if (0 <= yylocp->last_line)
    {
      if (yylocp->first_line < yylocp->last_line)
        {
          res += YYFPRINTF (yyo, "-%d", yylocp->last_line);
          if (0 <= end_col)
            res += YYFPRINTF (yyo, ".%d", end_col);
        }
      else if (0 <= end_col && yylocp->first_column < end_col)
        res += YYFPRINTF (yyo, "-%d", end_col);
    }
  return res;
 }

#  define YY_LOCATION_PRINT(File, Loc)          \
  yy_location_print_ (File, &(Loc))

# else
#  define YY_LOCATION_PRINT(File, Loc) ((void) 0)
# endif
#endif


# define YY_SYMBOL_PRINT(Title, Type, Value, Location)                    \
do {                                                                      \
  if (yydebug)                                                            \
    {                                                                     \
      YYFPRINTF (stderr, "%s ", Title);                                   \
      yy_symbol_print (stderr,                                            \
                  Type, Value, Location, scanner, extra); \
      YYFPRINTF (stderr, "\n");                                           \
    }                                                                     \
} while (0)


/*-----------------------------------.
| Print this symbol's value on YYO.  |
`-----------------------------------*/

static void
yy_symbol_value_print (FILE *yyo, int yytype, YYSTYPE const * const yyvaluep, YYLTYPE const * const yylocationp, ag_scanner_t scanner, cypher_yy_extra *extra)
{
  FILE *yyoutput = yyo;
  YYUSE (yyoutput);
  YYUSE (yylocationp);
  YYUSE (scanner);
  YYUSE (extra);
  if (!yyvaluep)
    return;
# ifdef YYPRINT
  if (yytype < YYNTOKENS)
    YYPRINT (yyo, yytoknum[yytype], *yyvaluep);
# endif
  YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
  YYUSE (yytype);
  YY_IGNORE_MAYBE_UNINITIALIZED_END
}


/*---------------------------.
| Print this symbol on YYO.  |
`---------------------------*/

static void
yy_symbol_print (FILE *yyo, int yytype, YYSTYPE const * const yyvaluep, YYLTYPE const * const yylocationp, ag_scanner_t scanner, cypher_yy_extra *extra)
{
  YYFPRINTF (yyo, "%s %s (",
             yytype < YYNTOKENS ? "token" : "nterm", yytname[yytype]);

  YY_LOCATION_PRINT (yyo, *yylocationp);
  YYFPRINTF (yyo, ": ");
  yy_symbol_value_print (yyo, yytype, yyvaluep, yylocationp, scanner, extra);
  YYFPRINTF (yyo, ")");
}

/*------------------------------------------------------------------.
| yy_stack_print -- Print the state stack from its BOTTOM up to its |
| TOP (included).                                                   |
`------------------------------------------------------------------*/

static void
yy_stack_print (yy_state_t *yybottom, yy_state_t *yytop)
{
  YYFPRINTF (stderr, "Stack now");
  for (; yybottom <= yytop; yybottom++)
    {
      int yybot = *yybottom;
      YYFPRINTF (stderr, " %d", yybot);
    }
  YYFPRINTF (stderr, "\n");
}

# define YY_STACK_PRINT(Bottom, Top)                            \
do {                                                            \
  if (yydebug)                                                  \
    yy_stack_print ((Bottom), (Top));                           \
} while (0)


/*------------------------------------------------.
| Report that the YYRULE is going to be reduced.  |
`------------------------------------------------*/

static void
yy_reduce_print (yy_state_t *yyssp, YYSTYPE *yyvsp, YYLTYPE *yylsp, int yyrule, ag_scanner_t scanner, cypher_yy_extra *extra)
{
  int yylno = yyrline[yyrule];
  int yynrhs = yyr2[yyrule];
  int yyi;
  YYFPRINTF (stderr, "Reducing stack by rule %d (line %d):\n",
             yyrule - 1, yylno);
  /* The symbols being reduced.  */
  for (yyi = 0; yyi < yynrhs; yyi++)
    {
      YYFPRINTF (stderr, "   $%d = ", yyi + 1);
      yy_symbol_print (stderr,
                       yystos[+yyssp[yyi + 1 - yynrhs]],
                       &yyvsp[(yyi + 1) - (yynrhs)]
                       , &(yylsp[(yyi + 1) - (yynrhs)])                       , scanner, extra);
      YYFPRINTF (stderr, "\n");
    }
}

# define YY_REDUCE_PRINT(Rule)          \
do {                                    \
  if (yydebug)                          \
    yy_reduce_print (yyssp, yyvsp, yylsp, Rule, scanner, extra); \
} while (0)

/* Nonzero means print parse trace.  It is left uninitialized so that
   multiple parsers can coexist.  */
int yydebug;
#else /* !YYDEBUG */
# define YYDPRINTF(Args)
# define YY_SYMBOL_PRINT(Title, Type, Value, Location)
# define YY_STACK_PRINT(Bottom, Top)
# define YY_REDUCE_PRINT(Rule)
#endif /* !YYDEBUG */


/* YYINITDEPTH -- initial size of the parser's stacks.  */
#ifndef YYINITDEPTH
# define YYINITDEPTH 200
#endif

/* YYMAXDEPTH -- maximum size the stacks can grow to (effective only
   if the built-in stack extension method is used).

   Do not make this value too large; the results are undefined if
   YYSTACK_ALLOC_MAXIMUM < YYSTACK_BYTES (YYMAXDEPTH)
   evaluated with infinite-precision integer arithmetic.  */

#ifndef YYMAXDEPTH
# define YYMAXDEPTH 10000
#endif


#if YYERROR_VERBOSE

# ifndef yystrlen
#  if defined __GLIBC__ && defined _STRING_H
#   define yystrlen(S) (YY_CAST (YYPTRDIFF_T, strlen (S)))
#  else
/* Return the length of YYSTR.  */
static YYPTRDIFF_T
yystrlen (const char *yystr)
{
  YYPTRDIFF_T yylen;
  for (yylen = 0; yystr[yylen]; yylen++)
    continue;
  return yylen;
}
#  endif
# endif

# ifndef yystpcpy
#  if defined __GLIBC__ && defined _STRING_H && defined _GNU_SOURCE
#   define yystpcpy stpcpy
#  else
/* Copy YYSRC to YYDEST, returning the address of the terminating '\0' in
   YYDEST.  */
static char *
yystpcpy (char *yydest, const char *yysrc)
{
  char *yyd = yydest;
  const char *yys = yysrc;

  while ((*yyd++ = *yys++) != '\0')
    continue;

  return yyd - 1;
}
#  endif
# endif

# ifndef yytnamerr
/* Copy to YYRES the contents of YYSTR after stripping away unnecessary
   quotes and backslashes, so that it's suitable for yyerror.  The
   heuristic is that double-quoting is unnecessary unless the string
   contains an apostrophe, a comma, or backslash (other than
   backslash-backslash).  YYSTR is taken from yytname.  If YYRES is
   null, do not copy; instead, return the length of what the result
   would have been.  */
static YYPTRDIFF_T
yytnamerr (char *yyres, const char *yystr)
{
  if (*yystr == '"')
    {
      YYPTRDIFF_T yyn = 0;
      char const *yyp = yystr;

      for (;;)
        switch (*++yyp)
          {
          case '\'':
          case ',':
            goto do_not_strip_quotes;

          case '\\':
            if (*++yyp != '\\')
              goto do_not_strip_quotes;
            else
              goto append;

          append:
          default:
            if (yyres)
              yyres[yyn] = *yyp;
            yyn++;
            break;

          case '"':
            if (yyres)
              yyres[yyn] = '\0';
            return yyn;
          }
    do_not_strip_quotes: ;
    }

  if (yyres)
    return yystpcpy (yyres, yystr) - yyres;
  else
    return yystrlen (yystr);
}
# endif

/* Copy into *YYMSG, which is of size *YYMSG_ALLOC, an error message
   about the unexpected token YYTOKEN for the state stack whose top is
   YYSSP.

   Return 0 if *YYMSG was successfully written.  Return 1 if *YYMSG is
   not large enough to hold the message.  In that case, also set
   *YYMSG_ALLOC to the required number of bytes.  Return 2 if the
   required number of bytes is too large to store.  */
static int
yysyntax_error (YYPTRDIFF_T *yymsg_alloc, char **yymsg,
                yy_state_t *yyssp, int yytoken)
{
  enum { YYERROR_VERBOSE_ARGS_MAXIMUM = 5 };
  /* Internationalized format string. */
  const char *yyformat = YY_NULLPTR;
  /* Arguments of yyformat: reported tokens (one for the "unexpected",
     one per "expected"). */
  char const *yyarg[YYERROR_VERBOSE_ARGS_MAXIMUM];
  /* Actual size of YYARG. */
  int yycount = 0;
  /* Cumulated lengths of YYARG.  */
  YYPTRDIFF_T yysize = 0;

  /* There are many possibilities here to consider:
     - If this state is a consistent state with a default action, then
       the only way this function was invoked is if the default action
       is an error action.  In that case, don't check for expected
       tokens because there are none.
     - The only way there can be no lookahead present (in yychar) is if
       this state is a consistent state with a default action.  Thus,
       detecting the absence of a lookahead is sufficient to determine
       that there is no unexpected or expected token to report.  In that
       case, just report a simple "syntax error".
     - Don't assume there isn't a lookahead just because this state is a
       consistent state with a default action.  There might have been a
       previous inconsistent state, consistent state with a non-default
       action, or user semantic action that manipulated yychar.
     - Of course, the expected token list depends on states to have
       correct lookahead information, and it depends on the parser not
       to perform extra reductions after fetching a lookahead from the
       scanner and before detecting a syntax error.  Thus, state merging
       (from LALR or IELR) and default reductions corrupt the expected
       token list.  However, the list is correct for canonical LR with
       one exception: it will still contain any token that will not be
       accepted due to an error action in a later state.
  */
  if (yytoken != YYEMPTY)
    {
      int yyn = yypact[+*yyssp];
      YYPTRDIFF_T yysize0 = yytnamerr (YY_NULLPTR, yytname[yytoken]);
      yysize = yysize0;
      yyarg[yycount++] = yytname[yytoken];
      if (!yypact_value_is_default (yyn))
        {
          /* Start YYX at -YYN if negative to avoid negative indexes in
             YYCHECK.  In other words, skip the first -YYN actions for
             this state because they are default actions.  */
          int yyxbegin = yyn < 0 ? -yyn : 0;
          /* Stay within bounds of both yycheck and yytname.  */
          int yychecklim = YYLAST - yyn + 1;
          int yyxend = yychecklim < YYNTOKENS ? yychecklim : YYNTOKENS;
          int yyx;

          for (yyx = yyxbegin; yyx < yyxend; ++yyx)
            if (yycheck[yyx + yyn] == yyx && yyx != YYTERROR
                && !yytable_value_is_error (yytable[yyx + yyn]))
              {
                if (yycount == YYERROR_VERBOSE_ARGS_MAXIMUM)
                  {
                    yycount = 1;
                    yysize = yysize0;
                    break;
                  }
                yyarg[yycount++] = yytname[yyx];
                {
                  YYPTRDIFF_T yysize1
                    = yysize + yytnamerr (YY_NULLPTR, yytname[yyx]);
                  if (yysize <= yysize1 && yysize1 <= YYSTACK_ALLOC_MAXIMUM)
                    yysize = yysize1;
                  else
                    return 2;
                }
              }
        }
    }

  switch (yycount)
    {
# define YYCASE_(N, S)                      \
      case N:                               \
        yyformat = S;                       \
      break
    default: /* Avoid compiler warnings. */
      YYCASE_(0, YY_("syntax error"));
      YYCASE_(1, YY_("syntax error, unexpected %s"));
      YYCASE_(2, YY_("syntax error, unexpected %s, expecting %s"));
      YYCASE_(3, YY_("syntax error, unexpected %s, expecting %s or %s"));
      YYCASE_(4, YY_("syntax error, unexpected %s, expecting %s or %s or %s"));
      YYCASE_(5, YY_("syntax error, unexpected %s, expecting %s or %s or %s or %s"));
# undef YYCASE_
    }

  {
    /* Don't count the "%s"s in the final size, but reserve room for
       the terminator.  */
    YYPTRDIFF_T yysize1 = yysize + (yystrlen (yyformat) - 2 * yycount) + 1;
    if (yysize <= yysize1 && yysize1 <= YYSTACK_ALLOC_MAXIMUM)
      yysize = yysize1;
    else
      return 2;
  }

  if (*yymsg_alloc < yysize)
    {
      *yymsg_alloc = 2 * yysize;
      if (! (yysize <= *yymsg_alloc
             && *yymsg_alloc <= YYSTACK_ALLOC_MAXIMUM))
        *yymsg_alloc = YYSTACK_ALLOC_MAXIMUM;
      return 1;
    }

  /* Avoid sprintf, as that infringes on the user's name space.
     Don't have undefined behavior even if the translation
     produced a string with the wrong number of "%s"s.  */
  {
    char *yyp = *yymsg;
    int yyi = 0;
    while ((*yyp = *yyformat) != '\0')
      if (*yyp == '%' && yyformat[1] == 's' && yyi < yycount)
        {
          yyp += yytnamerr (yyp, yyarg[yyi++]);
          yyformat += 2;
        }
      else
        {
          ++yyp;
          ++yyformat;
        }
  }
  return 0;
}
#endif /* YYERROR_VERBOSE */

/*-----------------------------------------------.
| Release the memory associated to this symbol.  |
`-----------------------------------------------*/

static void
yydestruct (const char *yymsg, int yytype, YYSTYPE *yyvaluep, YYLTYPE *yylocationp, ag_scanner_t scanner, cypher_yy_extra *extra)
{
  YYUSE (yyvaluep);
  YYUSE (yylocationp);
  YYUSE (scanner);
  YYUSE (extra);
  if (!yymsg)
    yymsg = "Deleting";
  YY_SYMBOL_PRINT (yymsg, yytype, yyvaluep, yylocationp);

  YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
  YYUSE (yytype);
  YY_IGNORE_MAYBE_UNINITIALIZED_END
}




/*----------.
| yyparse.  |
`----------*/

int
yyparse (ag_scanner_t scanner, cypher_yy_extra *extra)
{
/* The lookahead symbol.  */
int yychar;


/* The semantic value of the lookahead symbol.  */
/* Default value used for initialization, for pacifying older GCCs
   or non-GCC compilers.  */
YY_INITIAL_VALUE (static YYSTYPE yyval_default;)
YYSTYPE yylval YY_INITIAL_VALUE (= yyval_default);

/* Location data for the lookahead symbol.  */
static YYLTYPE yyloc_default
# if defined YYLTYPE_IS_TRIVIAL && YYLTYPE_IS_TRIVIAL
  = { 1, 1, 1, 1 }
# endif
;
YYLTYPE yylloc = yyloc_default;

    /* Number of syntax errors so far.  */
    int yynerrs;

    yy_state_fast_t yystate;
    /* Number of tokens to shift before error messages enabled.  */
    int yyerrstatus;

    /* The stacks and their tools:
       'yyss': related to states.
       'yyvs': related to semantic values.
       'yyls': related to locations.

       Refer to the stacks through separate pointers, to allow yyoverflow
       to reallocate them elsewhere.  */

    /* The state stack.  */
    yy_state_t yyssa[YYINITDEPTH];
    yy_state_t *yyss;
    yy_state_t *yyssp;

    /* The semantic value stack.  */
    YYSTYPE yyvsa[YYINITDEPTH];
    YYSTYPE *yyvs;
    YYSTYPE *yyvsp;

    /* The location stack.  */
    YYLTYPE yylsa[YYINITDEPTH];
    YYLTYPE *yyls;
    YYLTYPE *yylsp;

    /* The locations where the error started and ended.  */
    YYLTYPE yyerror_range[3];

    YYPTRDIFF_T yystacksize;

  int yyn;
  int yyresult;
  /* Lookahead token as an internal (translated) token number.  */
  int yytoken = 0;
  /* The variables used to return semantic value and location from the
     action routines.  */
  YYSTYPE yyval;
  YYLTYPE yyloc;

#if YYERROR_VERBOSE
  /* Buffer for error messages, and its allocated size.  */
  char yymsgbuf[128];
  char *yymsg = yymsgbuf;
  YYPTRDIFF_T yymsg_alloc = sizeof yymsgbuf;
#endif

#define YYPOPSTACK(N)   (yyvsp -= (N), yyssp -= (N), yylsp -= (N))

  /* The number of symbols on the RHS of the reduced rule.
     Keep to zero when no symbol should be popped.  */
  int yylen = 0;

  yyssp = yyss = yyssa;
  yyvsp = yyvs = yyvsa;
  yylsp = yyls = yylsa;
  yystacksize = YYINITDEPTH;

  YYDPRINTF ((stderr, "Starting parse\n"));

  yystate = 0;
  yyerrstatus = 0;
  yynerrs = 0;
  yychar = YYEMPTY; /* Cause a token to be read.  */
  yylsp[0] = yylloc;
  goto yysetstate;


/*------------------------------------------------------------.
| yynewstate -- push a new state, which is found in yystate.  |
`------------------------------------------------------------*/
yynewstate:
  /* In all cases, when you get here, the value and location stacks
     have just been pushed.  So pushing a state here evens the stacks.  */
  yyssp++;


/*--------------------------------------------------------------------.
| yysetstate -- set current state (the top of the stack) to yystate.  |
`--------------------------------------------------------------------*/
yysetstate:
  YYDPRINTF ((stderr, "Entering state %d\n", yystate));
  YY_ASSERT (0 <= yystate && yystate < YYNSTATES);
  YY_IGNORE_USELESS_CAST_BEGIN
  *yyssp = YY_CAST (yy_state_t, yystate);
  YY_IGNORE_USELESS_CAST_END

  if (yyss + yystacksize - 1 <= yyssp)
#if !defined yyoverflow && !defined YYSTACK_RELOCATE
    goto yyexhaustedlab;
#else
    {
      /* Get the current used size of the three stacks, in elements.  */
      YYPTRDIFF_T yysize = yyssp - yyss + 1;

# if defined yyoverflow
      {
        /* Give user a chance to reallocate the stack.  Use copies of
           these so that the &'s don't force the real ones into
           memory.  */
        yy_state_t *yyss1 = yyss;
        YYSTYPE *yyvs1 = yyvs;
        YYLTYPE *yyls1 = yyls;

        /* Each stack pointer address is followed by the size of the
           data in use in that stack, in bytes.  This used to be a
           conditional around just the two extra args, but that might
           be undefined if yyoverflow is a macro.  */
        yyoverflow (YY_("memory exhausted"),
                    &yyss1, yysize * YYSIZEOF (*yyssp),
                    &yyvs1, yysize * YYSIZEOF (*yyvsp),
                    &yyls1, yysize * YYSIZEOF (*yylsp),
                    &yystacksize);
        yyss = yyss1;
        yyvs = yyvs1;
        yyls = yyls1;
      }
# else /* defined YYSTACK_RELOCATE */
      /* Extend the stack our own way.  */
      if (YYMAXDEPTH <= yystacksize)
        goto yyexhaustedlab;
      yystacksize *= 2;
      if (YYMAXDEPTH < yystacksize)
        yystacksize = YYMAXDEPTH;

      {
        yy_state_t *yyss1 = yyss;
        union yyalloc *yyptr =
          YY_CAST (union yyalloc *,
                   YYSTACK_ALLOC (YY_CAST (YYSIZE_T, YYSTACK_BYTES (yystacksize))));
        if (! yyptr)
          goto yyexhaustedlab;
        YYSTACK_RELOCATE (yyss_alloc, yyss);
        YYSTACK_RELOCATE (yyvs_alloc, yyvs);
        YYSTACK_RELOCATE (yyls_alloc, yyls);
# undef YYSTACK_RELOCATE
        if (yyss1 != yyssa)
          YYSTACK_FREE (yyss1);
      }
# endif

      yyssp = yyss + yysize - 1;
      yyvsp = yyvs + yysize - 1;
      yylsp = yyls + yysize - 1;

      YY_IGNORE_USELESS_CAST_BEGIN
      YYDPRINTF ((stderr, "Stack size increased to %ld\n",
                  YY_CAST (long, yystacksize)));
      YY_IGNORE_USELESS_CAST_END

      if (yyss + yystacksize - 1 <= yyssp)
        YYABORT;
    }
#endif /* !defined yyoverflow && !defined YYSTACK_RELOCATE */

  if (yystate == YYFINAL)
    YYACCEPT;

  goto yybackup;


/*-----------.
| yybackup.  |
`-----------*/
yybackup:
  /* Do appropriate processing given the current state.  Read a
     lookahead token if we need one and don't already have one.  */

  /* First try to decide what to do without reference to lookahead token.  */
  yyn = yypact[yystate];
  if (yypact_value_is_default (yyn))
    goto yydefault;

  /* Not known => get a lookahead token if don't already have one.  */

  /* YYCHAR is either YYEMPTY or YYEOF or a valid lookahead symbol.  */
  if (yychar == YYEMPTY)
    {
      YYDPRINTF ((stderr, "Reading a token: "));
      yychar = yylex (&yylval, &yylloc, scanner);
    }

  if (yychar <= YYEOF)
    {
      yychar = yytoken = YYEOF;
      YYDPRINTF ((stderr, "Now at end of input.\n"));
    }
  else
    {
      yytoken = YYTRANSLATE (yychar);
      YY_SYMBOL_PRINT ("Next token is", yytoken, &yylval, &yylloc);
    }

  /* If the proper action on seeing token YYTOKEN is to reduce or to
     detect an error, take that action.  */
  yyn += yytoken;
  if (yyn < 0 || YYLAST < yyn || yycheck[yyn] != yytoken)
    goto yydefault;
  yyn = yytable[yyn];
  if (yyn <= 0)
    {
      if (yytable_value_is_error (yyn))
        goto yyerrlab;
      yyn = -yyn;
      goto yyreduce;
    }

  /* Count tokens shifted since error; after three, turn off error
     status.  */
  if (yyerrstatus)
    yyerrstatus--;

  /* Shift the lookahead token.  */
  YY_SYMBOL_PRINT ("Shifting", yytoken, &yylval, &yylloc);
  yystate = yyn;
  YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
  *++yyvsp = yylval;
  YY_IGNORE_MAYBE_UNINITIALIZED_END
  *++yylsp = yylloc;

  /* Discard the shifted token.  */
  yychar = YYEMPTY;
  goto yynewstate;


/*-----------------------------------------------------------.
| yydefault -- do the default action for the current state.  |
`-----------------------------------------------------------*/
yydefault:
  yyn = yydefact[yystate];
  if (yyn == 0)
    goto yyerrlab;
  goto yyreduce;


/*-----------------------------.
| yyreduce -- do a reduction.  |
`-----------------------------*/
yyreduce:
  /* yyn is the number of a rule to reduce with.  */
  yylen = yyr2[yyn];

  /* If YYLEN is nonzero, implement the default value of the action:
     '$$ = $1'.

     Otherwise, the following line sets YYVAL to garbage.
     This behavior is undocumented and Bison
     users should not rely upon it.  Assigning to YYVAL
     unconditionally makes the parser a bit smaller, and it avoids a
     GCC warning that YYVAL may be used uninitialized.  */
  yyval = yyvsp[1-yylen];

  /* Default location. */
  YYLLOC_DEFAULT (yyloc, (yylsp - yylen), yylen);
  yyerror_range[1] = yyloc;
  YY_REDUCE_PRINT (yyn);
  switch (yyn)
    {
  case 2:
#line 198 "src/backend/parser/cypher_gram.y"
        {
            /*
             * If there is no transition for the lookahead token and the
             * clauses can be reduced to single_query, the parsing is
             * considered successful although it actually isn't.
             *
             * For example, when `MATCH ... CREATE ... MATCH ... ;` query is
             * being parsed, there is no transition for the second `MATCH ...`
             * because the query is wrong but `MATCH .. CREATE ...` is correct
             * so it will be reduced to query_part_last anyway even if there
             * are more tokens to read.
             *
             * Throw syntax error in this case.
             */
            if (yychar != YYEOF)
                yyerror(&yylloc, scanner, extra, "syntax error");

            extra->result = (yyvsp[-1].list);
            extra->extra = NULL;
        }
#line 2072 "src/backend/parser/cypher_gram.c"
    break;

  case 3:
#line 219 "src/backend/parser/cypher_gram.y"
        {
            ExplainStmt *estmt = NULL;

            if (yychar != YYEOF)
                yyerror(&yylloc, scanner, extra, "syntax error");

            extra->result = (yyvsp[-1].list);

            estmt = makeNode(ExplainStmt);
            estmt->query = NULL;
            estmt->options = NIL;
            extra->extra = (Node *)estmt;
        }
#line 2090 "src/backend/parser/cypher_gram.c"
    break;

  case 4:
#line 233 "src/backend/parser/cypher_gram.y"
        {
            ExplainStmt *estmt = NULL;

            if (yychar != YYEOF)
                yyerror(&yylloc, scanner, extra, "syntax error");

            extra->result = (yyvsp[-1].list);

            estmt = makeNode(ExplainStmt);
            estmt->query = NULL;
            estmt->options = list_make1(makeDefElem("verbose", NULL, (yylsp[-2])));;
            extra->extra = (Node *)estmt;
        }
#line 2108 "src/backend/parser/cypher_gram.c"
    break;

  case 5:
#line 247 "src/backend/parser/cypher_gram.y"
        {
            ExplainStmt *estmt = NULL;

            if (yychar != YYEOF)
                yyerror(&yylloc, scanner, extra, "syntax error");

            extra->result = (yyvsp[-1].list);

            estmt = makeNode(ExplainStmt);
            estmt->query = NULL;
            estmt->options = list_make1(makeDefElem("analyze", NULL, (yylsp[-2])));;
            extra->extra = (Node *)estmt;
        }
#line 2126 "src/backend/parser/cypher_gram.c"
    break;

  case 6:
#line 261 "src/backend/parser/cypher_gram.y"
        {
            ExplainStmt *estmt = NULL;

            if (yychar != YYEOF)
                yyerror(&yylloc, scanner, extra, "syntax error");

            extra->result = (yyvsp[-1].list);

            estmt = makeNode(ExplainStmt);
            estmt->query = NULL;
            estmt->options = list_make2(makeDefElem("analyze", NULL, (yylsp[-3])),
                                        makeDefElem("verbose", NULL, (yylsp[-2])));;
            extra->extra = (Node *)estmt;
        }
#line 2145 "src/backend/parser/cypher_gram.c"
    break;

  case 9:
#line 290 "src/backend/parser/cypher_gram.y"
        {
            (yyval.list) = list_concat((yyvsp[-1].list), (yyvsp[0].list));
        }
#line 2153 "src/backend/parser/cypher_gram.c"
    break;

  case 10:
#line 297 "src/backend/parser/cypher_gram.y"
        {
            (yyval.list) = NIL;
        }
#line 2161 "src/backend/parser/cypher_gram.c"
    break;

  case 11:
#line 301 "src/backend/parser/cypher_gram.y"
        {
            (yyval.list) = lappend(list_concat(list_concat((yyvsp[-3].list), (yyvsp[-2].list)), (yyvsp[-1].list)), (yyvsp[0].node));
        }
#line 2169 "src/backend/parser/cypher_gram.c"
    break;

  case 12:
#line 308 "src/backend/parser/cypher_gram.y"
        {
            (yyval.list) = list_concat((yyvsp[-1].list), (yyvsp[0].list));
        }
#line 2177 "src/backend/parser/cypher_gram.c"
    break;

  case 13:
#line 312 "src/backend/parser/cypher_gram.y"
        {
            (yyval.list) = lappend(list_concat((yyvsp[-2].list), (yyvsp[-1].list)), (yyvsp[0].node));
        }
#line 2185 "src/backend/parser/cypher_gram.c"
    break;

  case 14:
#line 319 "src/backend/parser/cypher_gram.y"
        {
            (yyval.list) = NIL;
        }
#line 2193 "src/backend/parser/cypher_gram.c"
    break;

  case 15:
#line 323 "src/backend/parser/cypher_gram.y"
        {
            (yyval.list) = lappend((yyvsp[-1].list), (yyvsp[0].node));
        }
#line 2201 "src/backend/parser/cypher_gram.c"
    break;

  case 17:
#line 334 "src/backend/parser/cypher_gram.y"
        {
            (yyval.list) = NIL;
        }
#line 2209 "src/backend/parser/cypher_gram.c"
    break;

  case 19:
#line 342 "src/backend/parser/cypher_gram.y"
        {
            (yyval.list) = list_make1((yyvsp[0].node));
        }
#line 2217 "src/backend/parser/cypher_gram.c"
    break;

  case 20:
#line 346 "src/backend/parser/cypher_gram.y"
        {
            (yyval.list) = lappend((yyvsp[-1].list), (yyvsp[0].node));
        }
#line 2225 "src/backend/parser/cypher_gram.c"
    break;

  case 25:
#line 360 "src/backend/parser/cypher_gram.y"
        {
            A_Indices *n = (A_Indices *) (yyvsp[0].node);

            if (n->lidx == NULL)
                n->lidx = make_int_const(1, (yylsp[0]));

            if (n->uidx != NULL)
            {
                A_Const    *lidx = (A_Const *) n->lidx;
                A_Const    *uidx = (A_Const *) n->uidx;

                if (lidx->val.val.ival > uidx->val.val.ival)
                    ereport(ERROR, (errcode(ERRCODE_SYNTAX_ERROR),
                                    errmsg("invalid range"),
                                    ag_scanner_errposition((yylsp[0]), scanner)));
            }
            (yyval.node) = (Node *) n;
        }
#line 2248 "src/backend/parser/cypher_gram.c"
    break;

  case 26:
#line 379 "src/backend/parser/cypher_gram.y"
        {
            (yyval.node) = NULL;
        }
#line 2256 "src/backend/parser/cypher_gram.c"
    break;

  case 27:
#line 386 "src/backend/parser/cypher_gram.y"
        {
            A_Indices  *n;

            n = makeNode(A_Indices);
            n->lidx = copyObject((yyvsp[0].node));
            n->uidx = (yyvsp[0].node);
            (yyval.node) = (Node *) n;
        }
#line 2269 "src/backend/parser/cypher_gram.c"
    break;

  case 28:
#line 395 "src/backend/parser/cypher_gram.y"
        {
            A_Indices  *n;

            n = makeNode(A_Indices);
            n->lidx = (yyvsp[-2].node);
            n->uidx = (yyvsp[0].node);
            (yyval.node) = (Node *) n;
        }
#line 2282 "src/backend/parser/cypher_gram.c"
    break;

  case 29:
#line 404 "src/backend/parser/cypher_gram.y"
        {
            (yyval.node) = (Node *) makeNode(A_Indices);
        }
#line 2290 "src/backend/parser/cypher_gram.c"
    break;

  case 30:
#line 411 "src/backend/parser/cypher_gram.y"
        {
            (yyval.node) = make_int_const((yyvsp[0].integer), (yylsp[0]));
        }
#line 2298 "src/backend/parser/cypher_gram.c"
    break;

  case 32:
#line 418 "src/backend/parser/cypher_gram.y"
                                                        { (yyval.node) = NULL; }
#line 2304 "src/backend/parser/cypher_gram.c"
    break;

  case 34:
#line 429 "src/backend/parser/cypher_gram.y"
        {
            cypher_return *n;

            n = make_ag_node(cypher_return);
            n->distinct = true;
            n->items = (yyvsp[-3].list);
            n->order_by = (yyvsp[-2].list);
            n->skip = (yyvsp[-1].node);
            n->limit = (yyvsp[0].node);

            (yyval.node) = (Node *)n;
        }
#line 2321 "src/backend/parser/cypher_gram.c"
    break;

  case 35:
#line 442 "src/backend/parser/cypher_gram.y"
        {
            cypher_return *n;

            n = make_ag_node(cypher_return);
            n->distinct = false;
            n->items = (yyvsp[-3].list);
            n->order_by = (yyvsp[-2].list);
            n->skip = (yyvsp[-1].node);
            n->limit = (yyvsp[0].node);

            (yyval.node) = (Node *)n;
        }
#line 2338 "src/backend/parser/cypher_gram.c"
    break;

  case 36:
#line 459 "src/backend/parser/cypher_gram.y"
        {
            (yyval.list) = list_make1((yyvsp[0].node));
        }
#line 2346 "src/backend/parser/cypher_gram.c"
    break;

  case 37:
#line 463 "src/backend/parser/cypher_gram.y"
        {
            (yyval.list) = lappend((yyvsp[-2].list), (yyvsp[0].node));
        }
#line 2354 "src/backend/parser/cypher_gram.c"
    break;

  case 38:
#line 470 "src/backend/parser/cypher_gram.y"
        {
            ResTarget *n;

            n = makeNode(ResTarget);
            n->name = (yyvsp[0].string);
            n->indirection = NIL;
            n->val = (yyvsp[-2].node);
            n->location = (yylsp[-2]);

            (yyval.node) = (Node *)n;
        }
#line 2370 "src/backend/parser/cypher_gram.c"
    break;

  case 39:
#line 482 "src/backend/parser/cypher_gram.y"
        {
            ResTarget *n;

            n = makeNode(ResTarget);
            n->name = NULL;
            n->indirection = NIL;
            n->val = (yyvsp[0].node);
            n->location = (yylsp[0]);

            (yyval.node) = (Node *)n;
        }
#line 2386 "src/backend/parser/cypher_gram.c"
    break;

  case 40:
#line 494 "src/backend/parser/cypher_gram.y"
        {
            ColumnRef *cr;
            ResTarget *rt;

            cr = makeNode(ColumnRef);
            cr->fields = list_make1(makeNode(A_Star));
            cr->location = (yylsp[0]);

            rt = makeNode(ResTarget);
            rt->name = NULL;
            rt->indirection = NIL;
            rt->val = (Node *)cr;
            rt->location = (yylsp[0]);

            (yyval.node) = (Node *)rt;
        }
#line 2407 "src/backend/parser/cypher_gram.c"
    break;

  case 41:
#line 514 "src/backend/parser/cypher_gram.y"
        {
            (yyval.list) = NIL;
        }
#line 2415 "src/backend/parser/cypher_gram.c"
    break;

  case 42:
#line 518 "src/backend/parser/cypher_gram.y"
        {
            (yyval.list) = (yyvsp[0].list);
        }
#line 2423 "src/backend/parser/cypher_gram.c"
    break;

  case 43:
#line 525 "src/backend/parser/cypher_gram.y"
        {
            (yyval.list) = list_make1((yyvsp[0].node));
        }
#line 2431 "src/backend/parser/cypher_gram.c"
    break;

  case 44:
#line 529 "src/backend/parser/cypher_gram.y"
        {
            (yyval.list) = lappend((yyvsp[-2].list), (yyvsp[0].node));
        }
#line 2439 "src/backend/parser/cypher_gram.c"
    break;

  case 45:
#line 536 "src/backend/parser/cypher_gram.y"
        {
            SortBy *n;

            n = makeNode(SortBy);
            n->node = (yyvsp[-1].node);
            n->sortby_dir = (yyvsp[0].integer);
            n->sortby_nulls = SORTBY_NULLS_DEFAULT;
            n->useOp = NIL;
            n->location = -1; // no operator

            (yyval.node) = (Node *)n;
        }
#line 2456 "src/backend/parser/cypher_gram.c"
    break;

  case 46:
#line 552 "src/backend/parser/cypher_gram.y"
        {
            (yyval.integer) = SORTBY_DEFAULT; // is the same with SORTBY_ASC
        }
#line 2464 "src/backend/parser/cypher_gram.c"
    break;

  case 47:
#line 556 "src/backend/parser/cypher_gram.y"
        {
            (yyval.integer) = SORTBY_ASC;
        }
#line 2472 "src/backend/parser/cypher_gram.c"
    break;

  case 48:
#line 560 "src/backend/parser/cypher_gram.y"
        {
            (yyval.integer) = SORTBY_ASC;
        }
#line 2480 "src/backend/parser/cypher_gram.c"
    break;

  case 49:
#line 564 "src/backend/parser/cypher_gram.y"
        {
            (yyval.integer) = SORTBY_DESC;
        }
#line 2488 "src/backend/parser/cypher_gram.c"
    break;

  case 50:
#line 568 "src/backend/parser/cypher_gram.y"
        {
            (yyval.integer) = SORTBY_DESC;
        }
#line 2496 "src/backend/parser/cypher_gram.c"
    break;

  case 51:
#line 575 "src/backend/parser/cypher_gram.y"
        {
            (yyval.node) = NULL;
        }
#line 2504 "src/backend/parser/cypher_gram.c"
    break;

  case 52:
#line 579 "src/backend/parser/cypher_gram.y"
        {
            (yyval.node) = (yyvsp[0].node);
        }
#line 2512 "src/backend/parser/cypher_gram.c"
    break;

  case 53:
#line 586 "src/backend/parser/cypher_gram.y"
        {
            (yyval.node) = NULL;
        }
#line 2520 "src/backend/parser/cypher_gram.c"
    break;

  case 54:
#line 590 "src/backend/parser/cypher_gram.y"
        {
            (yyval.node) = (yyvsp[0].node);
        }
#line 2528 "src/backend/parser/cypher_gram.c"
    break;

  case 55:
#line 597 "src/backend/parser/cypher_gram.y"
        {
            ListCell *li;
            cypher_with *n;

            // check expressions are aliased
            foreach (li, (yyvsp[-4].list))
            {
                ResTarget *item = lfirst(li);

                // variable does not have to be aliased
                if (IsA(item->val, ColumnRef) || item->name)
                    continue;

                ereport(ERROR,
                        (errcode(ERRCODE_SYNTAX_ERROR),
                         errmsg("expression item must be aliased"),
                         errhint("Items can be aliased by using AS."),
                         ag_scanner_errposition(item->location, scanner)));
            }

            n = make_ag_node(cypher_with);
            n->distinct = true;
            n->items = (yyvsp[-4].list);
            n->order_by = (yyvsp[-3].list);
            n->skip = (yyvsp[-2].node);
            n->limit = (yyvsp[-1].node);
            n->where = (yyvsp[0].node);

            (yyval.node) = (Node *)n;
        }
#line 2563 "src/backend/parser/cypher_gram.c"
    break;

  case 56:
#line 629 "src/backend/parser/cypher_gram.y"
        {
            ListCell *li;
            cypher_with *n;

            // check expressions are aliased
            foreach (li, (yyvsp[-4].list))
            {
                ResTarget *item = lfirst(li);

                // variable does not have to be aliased
                if (IsA(item->val, ColumnRef) || item->name)
                    continue;

                ereport(ERROR,
                        (errcode(ERRCODE_SYNTAX_ERROR),
                         errmsg("expression item must be aliased"),
                         errhint("Items can be aliased by using AS."),
                         ag_scanner_errposition(item->location, scanner)));
            }

            n = make_ag_node(cypher_with);
            n->distinct = false;
            n->items = (yyvsp[-4].list);
            n->order_by = (yyvsp[-3].list);
            n->skip = (yyvsp[-2].node);
            n->limit = (yyvsp[-1].node);
            n->where = (yyvsp[0].node);

            (yyval.node) = (Node *)n;
        }
#line 2598 "src/backend/parser/cypher_gram.c"
    break;

  case 57:
#line 667 "src/backend/parser/cypher_gram.y"
        {
            cypher_match *n;

            n = make_ag_node(cypher_match);
            n->pattern = (yyvsp[-1].list);
            n->where = (yyvsp[0].node);

            (yyval.node) = (Node *)n;
        }
#line 2612 "src/backend/parser/cypher_gram.c"
    break;

  case 58:
#line 684 "src/backend/parser/cypher_gram.y"
        {
            cypher_create *n;

            n = make_ag_node(cypher_create);
            n->pattern = (yyvsp[0].list);

            (yyval.node) = (Node *)n;
        }
#line 2625 "src/backend/parser/cypher_gram.c"
    break;

  case 59:
#line 700 "src/backend/parser/cypher_gram.y"
        {
            cypher_set *n;

            n = make_ag_node(cypher_set);
            n->items = (yyvsp[0].list);
            n->is_remove = false;
            n->location = (yylsp[-1]);

            (yyval.node) = (Node *)n;
        }
#line 2640 "src/backend/parser/cypher_gram.c"
    break;

  case 60:
#line 714 "src/backend/parser/cypher_gram.y"
        {
            (yyval.list) = list_make1((yyvsp[0].node));
        }
#line 2648 "src/backend/parser/cypher_gram.c"
    break;

  case 61:
#line 718 "src/backend/parser/cypher_gram.y"
        {
            (yyval.list) = lappend((yyvsp[-2].list), (yyvsp[0].node));
        }
#line 2656 "src/backend/parser/cypher_gram.c"
    break;

  case 62:
#line 725 "src/backend/parser/cypher_gram.y"
        {
            cypher_set_item *n;

            n = make_ag_node(cypher_set_item);
            n->prop = (yyvsp[-2].node);
            n->expr = (yyvsp[0].node);
            n->is_add = false;
            n->location = (yylsp[-2]);

            (yyval.node) = (Node *)n;
        }
#line 2672 "src/backend/parser/cypher_gram.c"
    break;

  case 63:
#line 737 "src/backend/parser/cypher_gram.y"
        {
            cypher_set_item *n;

            n = make_ag_node(cypher_set_item);
            n->prop = (yyvsp[-2].node);
            n->expr = (yyvsp[0].node);
            n->is_add = true;
            n->location = (yylsp[-2]);

            (yyval.node) = (Node *)n;
        }
#line 2688 "src/backend/parser/cypher_gram.c"
    break;

  case 64:
#line 752 "src/backend/parser/cypher_gram.y"
        {
            cypher_set *n;

            n = make_ag_node(cypher_set);
            n->items = (yyvsp[0].list);
            n->is_remove = true;
             n->location = (yylsp[-1]);

            (yyval.node) = (Node *)n;
        }
#line 2703 "src/backend/parser/cypher_gram.c"
    break;

  case 65:
#line 766 "src/backend/parser/cypher_gram.y"
        {
            (yyval.list) = list_make1((yyvsp[0].node));
        }
#line 2711 "src/backend/parser/cypher_gram.c"
    break;

  case 66:
#line 770 "src/backend/parser/cypher_gram.y"
        {
            (yyval.list) = lappend((yyvsp[-2].list), (yyvsp[0].node));
        }
#line 2719 "src/backend/parser/cypher_gram.c"
    break;

  case 67:
#line 777 "src/backend/parser/cypher_gram.y"
        {
            cypher_set_item *n;

            n = make_ag_node(cypher_set_item);
            n->prop = (yyvsp[0].node);
            n->expr = make_null_const(-1);
            n->is_add = false;

            (yyval.node) = (Node *)n;
        }
#line 2734 "src/backend/parser/cypher_gram.c"
    break;

  case 68:
#line 795 "src/backend/parser/cypher_gram.y"
        {
            cypher_delete *n;

            n = make_ag_node(cypher_delete);
            n->detach = (yyvsp[-2].boolean);
            n->exprs = (yyvsp[0].list);
            n->location = (yylsp[-2]);

            (yyval.node) = (Node *)n;
        }
#line 2749 "src/backend/parser/cypher_gram.c"
    break;

  case 69:
#line 809 "src/backend/parser/cypher_gram.y"
        {
            (yyval.boolean) = true;
        }
#line 2757 "src/backend/parser/cypher_gram.c"
    break;

  case 70:
#line 813 "src/backend/parser/cypher_gram.y"
        {
            (yyval.boolean) = false;
        }
#line 2765 "src/backend/parser/cypher_gram.c"
    break;

  case 71:
#line 824 "src/backend/parser/cypher_gram.y"
        {
            (yyval.node) = NULL;
        }
#line 2773 "src/backend/parser/cypher_gram.c"
    break;

  case 72:
#line 828 "src/backend/parser/cypher_gram.y"
        {
            (yyval.node) = (yyvsp[0].node);
        }
#line 2781 "src/backend/parser/cypher_gram.c"
    break;

  case 73:
#line 840 "src/backend/parser/cypher_gram.y"
        {
            (yyval.list) = list_make1((yyvsp[0].node));
        }
#line 2789 "src/backend/parser/cypher_gram.c"
    break;

  case 74:
#line 844 "src/backend/parser/cypher_gram.y"
        {
            (yyval.list) = lappend((yyvsp[-2].list), (yyvsp[0].node));
        }
#line 2797 "src/backend/parser/cypher_gram.c"
    break;

  case 76:
#line 853 "src/backend/parser/cypher_gram.y"
        {
            cypher_path *p;

            p = (cypher_path *)(yyvsp[0].node);
            p->var_name = (yyvsp[-2].string);

            (yyval.node) = (Node *)p;
        }
#line 2810 "src/backend/parser/cypher_gram.c"
    break;

  case 77:
#line 866 "src/backend/parser/cypher_gram.y"
        {
            cypher_path *n;

            n = make_ag_node(cypher_path);
            n->path = (yyvsp[0].list);
            n->var_name = NULL;
            n->location = (yylsp[0]);

            (yyval.node) = (Node *)n;
        }
#line 2825 "src/backend/parser/cypher_gram.c"
    break;

  case 79:
#line 881 "src/backend/parser/cypher_gram.y"
        {
            (yyval.list) = (yyvsp[-1].list);
        }
#line 2833 "src/backend/parser/cypher_gram.c"
    break;

  case 80:
#line 888 "src/backend/parser/cypher_gram.y"
        {
            (yyval.list) = list_make1((yyvsp[0].node));
        }
#line 2841 "src/backend/parser/cypher_gram.c"
    break;

  case 81:
#line 892 "src/backend/parser/cypher_gram.y"
        {
            (yyval.list) = lappend(lappend((yyvsp[-2].list), (yyvsp[-1].node)), (yyvsp[0].node));
        }
#line 2849 "src/backend/parser/cypher_gram.c"
    break;

  case 82:
#line 899 "src/backend/parser/cypher_gram.y"
        {
            cypher_node *n;

            n = make_ag_node(cypher_node);
            n->name = (yyvsp[-3].string);
            n->label = (yyvsp[-2].string);
            n->props = (yyvsp[-1].node);
            n->location = (yylsp[-4]);

            (yyval.node) = (Node *)n;
        }
#line 2865 "src/backend/parser/cypher_gram.c"
    break;

  case 83:
#line 914 "src/backend/parser/cypher_gram.y"
        {
            cypher_relationship *n = (cypher_relationship *)(yyvsp[-1].node);

            n->dir = CYPHER_REL_DIR_NONE;
            n->location = (yylsp[-1]);

            (yyval.node) = (yyvsp[-1].node);
        }
#line 2878 "src/backend/parser/cypher_gram.c"
    break;

  case 84:
#line 923 "src/backend/parser/cypher_gram.y"
        {
            cypher_relationship *n = (cypher_relationship *)(yyvsp[-2].node);

            n->dir = CYPHER_REL_DIR_RIGHT;
            n->location = (yylsp[-2]);

            (yyval.node) = (yyvsp[-2].node);
        }
#line 2891 "src/backend/parser/cypher_gram.c"
    break;

  case 85:
#line 932 "src/backend/parser/cypher_gram.y"
        {
            cypher_relationship *n = (cypher_relationship *)(yyvsp[-1].node);

            n->dir = CYPHER_REL_DIR_LEFT;
            n->location = (yylsp[-1]);

            (yyval.node) = (yyvsp[-1].node);
        }
#line 2904 "src/backend/parser/cypher_gram.c"
    break;

  case 86:
#line 944 "src/backend/parser/cypher_gram.y"
        {
            cypher_relationship *n;

            n = make_ag_node(cypher_relationship);
            n->name = (yyvsp[-4].string);
            n->label = (yyvsp[-3].string);
            n->varlen = (yyvsp[-2].node);
            n->props = (yyvsp[-1].node);

            (yyval.node) = (Node *)n;
        }
#line 2920 "src/backend/parser/cypher_gram.c"
    break;

  case 87:
#line 957 "src/backend/parser/cypher_gram.y"
        {
            cypher_relationship *n;

            n = make_ag_node(cypher_relationship);
            n->name = NULL;
            n->label = NULL;
            n->varlen = NULL;
            n->props = NULL;

            (yyval.node) = (Node *)n;
        }
#line 2936 "src/backend/parser/cypher_gram.c"
    break;

  case 88:
#line 972 "src/backend/parser/cypher_gram.y"
        {
            (yyval.string) = NULL;
        }
#line 2944 "src/backend/parser/cypher_gram.c"
    break;

  case 89:
#line 976 "src/backend/parser/cypher_gram.y"
        {
            (yyval.string) = (yyvsp[0].string);
        }
#line 2952 "src/backend/parser/cypher_gram.c"
    break;

  case 90:
#line 983 "src/backend/parser/cypher_gram.y"
        {
            (yyval.node) = NULL;
        }
#line 2960 "src/backend/parser/cypher_gram.c"
    break;

  case 92:
#line 988 "src/backend/parser/cypher_gram.y"
        {
            cypher_param *n;

            n = make_ag_node(cypher_param);
            n->name = (yyvsp[0].string);
            n->location = (yylsp[0]);

            (yyval.node) = (Node *)n;
        }
#line 2974 "src/backend/parser/cypher_gram.c"
    break;

  case 93:
#line 1006 "src/backend/parser/cypher_gram.y"
        {
            (yyval.node) = make_or_expr((yyvsp[-2].node), (yyvsp[0].node), (yylsp[-1]));
        }
#line 2982 "src/backend/parser/cypher_gram.c"
    break;

  case 94:
#line 1010 "src/backend/parser/cypher_gram.y"
        {
            (yyval.node) = make_and_expr((yyvsp[-2].node), (yyvsp[0].node), (yylsp[-1]));
        }
#line 2990 "src/backend/parser/cypher_gram.c"
    break;

  case 95:
#line 1014 "src/backend/parser/cypher_gram.y"
        {
            (yyval.node) = make_not_expr((yyvsp[0].node), (yylsp[-1]));
        }
#line 2998 "src/backend/parser/cypher_gram.c"
    break;

  case 96:
#line 1018 "src/backend/parser/cypher_gram.y"
        {
            (yyval.node) = (Node *)makeSimpleA_Expr(AEXPR_OP, "=", (yyvsp[-2].node), (yyvsp[0].node), (yylsp[-1]));
        }
#line 3006 "src/backend/parser/cypher_gram.c"
    break;

  case 97:
#line 1022 "src/backend/parser/cypher_gram.y"
        {
            (yyval.node) = (Node *)makeSimpleA_Expr(AEXPR_OP, "<>", (yyvsp[-2].node), (yyvsp[0].node), (yylsp[-1]));
        }
#line 3014 "src/backend/parser/cypher_gram.c"
    break;

  case 98:
#line 1026 "src/backend/parser/cypher_gram.y"
        {
            (yyval.node) = (Node *)makeSimpleA_Expr(AEXPR_OP, "<", (yyvsp[-2].node), (yyvsp[0].node), (yylsp[-1]));
        }
#line 3022 "src/backend/parser/cypher_gram.c"
    break;

  case 99:
#line 1030 "src/backend/parser/cypher_gram.y"
        {
            (yyval.node) = (Node *)makeSimpleA_Expr(AEXPR_OP, "<=", (yyvsp[-2].node), (yyvsp[0].node), (yylsp[-1]));
        }
#line 3030 "src/backend/parser/cypher_gram.c"
    break;

  case 100:
#line 1034 "src/backend/parser/cypher_gram.y"
        {
            (yyval.node) = (Node *)makeSimpleA_Expr(AEXPR_OP, ">", (yyvsp[-2].node), (yyvsp[0].node), (yylsp[-1]));
        }
#line 3038 "src/backend/parser/cypher_gram.c"
    break;

  case 101:
#line 1038 "src/backend/parser/cypher_gram.y"
        {
            (yyval.node) = (Node *)makeSimpleA_Expr(AEXPR_OP, ">=", (yyvsp[-2].node), (yyvsp[0].node), (yylsp[-1]));
        }
#line 3046 "src/backend/parser/cypher_gram.c"
    break;

  case 102:
#line 1042 "src/backend/parser/cypher_gram.y"
        {
            (yyval.node) = (Node *)makeSimpleA_Expr(AEXPR_OP, "+", (yyvsp[-2].node), (yyvsp[0].node), (yylsp[-1]));
        }
#line 3054 "src/backend/parser/cypher_gram.c"
    break;

  case 103:
#line 1046 "src/backend/parser/cypher_gram.y"
        {
            (yyval.node) = (Node *)makeSimpleA_Expr(AEXPR_OP, "-", (yyvsp[-2].node), (yyvsp[0].node), (yylsp[-1]));
        }
#line 3062 "src/backend/parser/cypher_gram.c"
    break;

  case 104:
#line 1050 "src/backend/parser/cypher_gram.y"
        {
            (yyval.node) = (Node *)makeSimpleA_Expr(AEXPR_OP, "*", (yyvsp[-2].node), (yyvsp[0].node), (yylsp[-1]));
        }
#line 3070 "src/backend/parser/cypher_gram.c"
    break;

  case 105:
#line 1054 "src/backend/parser/cypher_gram.y"
        {
            (yyval.node) = (Node *)makeSimpleA_Expr(AEXPR_OP, "/", (yyvsp[-2].node), (yyvsp[0].node), (yylsp[-1]));
        }
#line 3078 "src/backend/parser/cypher_gram.c"
    break;

  case 106:
#line 1058 "src/backend/parser/cypher_gram.y"
        {
            (yyval.node) = (Node *)makeSimpleA_Expr(AEXPR_OP, "%", (yyvsp[-2].node), (yyvsp[0].node), (yylsp[-1]));
        }
#line 3086 "src/backend/parser/cypher_gram.c"
    break;

  case 107:
#line 1062 "src/backend/parser/cypher_gram.y"
        {
            (yyval.node) = (Node *)makeSimpleA_Expr(AEXPR_OP, "^", (yyvsp[-2].node), (yyvsp[0].node), (yylsp[-1]));
        }
#line 3094 "src/backend/parser/cypher_gram.c"
    break;

  case 108:
#line 1066 "src/backend/parser/cypher_gram.y"
        {
            (yyval.node) = (Node *)makeSimpleA_Expr(AEXPR_IN, "=", (yyvsp[-2].node), (yyvsp[0].node), (yylsp[-1]));
        }
#line 3102 "src/backend/parser/cypher_gram.c"
    break;

  case 109:
#line 1070 "src/backend/parser/cypher_gram.y"
        {
            NullTest *n;

            n = makeNode(NullTest);
            n->arg = (Expr *)(yyvsp[-2].node);
            n->nulltesttype = IS_NULL;
            n->location = (yylsp[-1]);

            (yyval.node) = (Node *)n;
        }
#line 3117 "src/backend/parser/cypher_gram.c"
    break;

  case 110:
#line 1081 "src/backend/parser/cypher_gram.y"
        {
            NullTest *n;

            n = makeNode(NullTest);
            n->arg = (Expr *)(yyvsp[-3].node);
            n->nulltesttype = IS_NOT_NULL;
            n->location = (yylsp[-2]);

            (yyval.node) = (Node *)n;
        }
#line 3132 "src/backend/parser/cypher_gram.c"
    break;

  case 111:
#line 1092 "src/backend/parser/cypher_gram.y"
        {
            (yyval.node) = do_negate((yyvsp[0].node), (yylsp[-1]));
        }
#line 3140 "src/backend/parser/cypher_gram.c"
    break;

  case 112:
#line 1096 "src/backend/parser/cypher_gram.y"
        {
            cypher_string_match *n;

            n = make_ag_node(cypher_string_match);
            n->operation = CSMO_STARTS_WITH;
            n->lhs = (yyvsp[-3].node);
            n->rhs = (yyvsp[0].node);
            n->location = (yylsp[-2]);

            (yyval.node) = (Node *)n;
        }
#line 3156 "src/backend/parser/cypher_gram.c"
    break;

  case 113:
#line 1108 "src/backend/parser/cypher_gram.y"
        {
            cypher_string_match *n;

            n = make_ag_node(cypher_string_match);
            n->operation = CSMO_ENDS_WITH;
            n->lhs = (yyvsp[-3].node);
            n->rhs = (yyvsp[0].node);
            n->location = (yylsp[-2]);

            (yyval.node) = (Node *)n;
        }
#line 3172 "src/backend/parser/cypher_gram.c"
    break;

  case 114:
#line 1120 "src/backend/parser/cypher_gram.y"
        {
            cypher_string_match *n;

            n = make_ag_node(cypher_string_match);
            n->operation = CSMO_CONTAINS;
            n->lhs = (yyvsp[-2].node);
            n->rhs = (yyvsp[0].node);
            n->location = (yylsp[-1]);

            (yyval.node) = (Node *)n;
        }
#line 3188 "src/backend/parser/cypher_gram.c"
    break;

  case 115:
#line 1132 "src/backend/parser/cypher_gram.y"
        {
            A_Indices *i;

            i = makeNode(A_Indices);
            i->is_slice = false;
            i->lidx = NULL;
            i->uidx = (yyvsp[-1].node);

            (yyval.node) = append_indirection((yyvsp[-3].node), (Node *)i);
        }
#line 3203 "src/backend/parser/cypher_gram.c"
    break;

  case 116:
#line 1143 "src/backend/parser/cypher_gram.y"
        {
            A_Indices *i;

            i = makeNode(A_Indices);
            i->is_slice = true;
            i->lidx = (yyvsp[-3].node);
            i->uidx = (yyvsp[-1].node);

            (yyval.node) = append_indirection((yyvsp[-5].node), (Node *)i);
        }
#line 3218 "src/backend/parser/cypher_gram.c"
    break;

  case 117:
#line 1161 "src/backend/parser/cypher_gram.y"
        {
            /*
             * This checks for the grammar rule -
             *     expr '.' property_key_name
             * where the expr can be anything.
             * Note: A property_key_name ends up as a ColumnRef.
             * Note: We restrict some of what the expr can be, for now. More may
             *       need to be added later to loosen the restrictions. Or, it
             *       may need to be removed.
             */
            if (IsA((yyvsp[0].node), ColumnRef) &&
                (IsA((yyvsp[-2].node), ExtensibleNode) ||
                 IsA((yyvsp[-2].node), ColumnRef) ||
                 IsA((yyvsp[-2].node), A_Indirection)))
            {
                ColumnRef *cr = (ColumnRef*)(yyvsp[0].node);
                List *fields = cr->fields;
                Value *string = linitial(fields);

                (yyval.node) = append_indirection((yyvsp[-2].node), (Node*)string);
            }
            /*
             * This checks for the grammar rule -
             *    symbolic_name '.' expr
             * Where expr is a function call.
             * Note: symbolic_name ends up as a ColumnRef
             */
            else if (IsA((yyvsp[0].node), FuncCall) && IsA((yyvsp[-2].node), ColumnRef))
            {
                FuncCall *fc = (FuncCall*)(yyvsp[0].node);
                ColumnRef *cr = (ColumnRef*)(yyvsp[-2].node);
                List *fields = cr->fields;
                Value *string = linitial(fields);

                /*
                 * A function can only be qualified with a single schema. So, we
                 * check to see that the function isn't already qualified. There
                 * may be unforeseen cases where we might need to remove this in
                 * the future.
                 */
                if (list_length(fc->funcname) == 1)
                {
                    fc->funcname = lcons(string, fc->funcname);
                    (yyval.node) = (Node*)fc;
                }
                else
                    ereport(ERROR,
                            (errcode(ERRCODE_SYNTAX_ERROR),
                             errmsg("function already qualified"),
                             ag_scanner_errposition((yylsp[-2]), scanner)));
            }
            /*
             * All other types of expression indirections are currently not
             * supported
             */
            else
                ereport(ERROR,
                        (errcode(ERRCODE_SYNTAX_ERROR),
                         errmsg("invalid indirection syntax"),
                         ag_scanner_errposition((yylsp[-2]), scanner)));
        }
#line 3284 "src/backend/parser/cypher_gram.c"
    break;

  case 118:
#line 1223 "src/backend/parser/cypher_gram.y"
        {
            (yyval.node) = make_typecast_expr((yyvsp[-2].node), (yyvsp[0].string), (yylsp[-1]));
        }
#line 3292 "src/backend/parser/cypher_gram.c"
    break;

  case 120:
#line 1231 "src/backend/parser/cypher_gram.y"
        {
            (yyval.node) = NULL;
        }
#line 3300 "src/backend/parser/cypher_gram.c"
    break;

  case 122:
#line 1239 "src/backend/parser/cypher_gram.y"
        {
            (yyval.list) = list_make1((yyvsp[0].node));
        }
#line 3308 "src/backend/parser/cypher_gram.c"
    break;

  case 123:
#line 1243 "src/backend/parser/cypher_gram.y"
        {
            (yyval.list) = lappend((yyvsp[-2].list), (yyvsp[0].node));
        }
#line 3316 "src/backend/parser/cypher_gram.c"
    break;

  case 124:
#line 1250 "src/backend/parser/cypher_gram.y"
        {
            (yyval.list) = NIL;
        }
#line 3324 "src/backend/parser/cypher_gram.c"
    break;

  case 128:
#line 1263 "src/backend/parser/cypher_gram.y"
        {
            (yyval.node) = make_function_expr((yyvsp[-2].list), NIL, (yylsp[-2]));
        }
#line 3332 "src/backend/parser/cypher_gram.c"
    break;

  case 129:
#line 1267 "src/backend/parser/cypher_gram.y"
        {
            (yyval.node) = make_function_expr((yyvsp[-3].list), (yyvsp[-1].list), (yylsp[-2]));
        }
#line 3340 "src/backend/parser/cypher_gram.c"
    break;

  case 130:
#line 1272 "src/backend/parser/cypher_gram.y"
        {
            /*
             * We consider AGGREGATE(*) to invoke a parameterless
             * aggregate.  This does the right thing for COUNT(*),
             * and there are no other aggregates in SQL that accept
             * '*' as parameter.
             *
             * The FuncCall node is also marked agg_star = true,
             * so that later processing can detect what the argument
             * really was.
             */
             FuncCall *n = (FuncCall *)make_function_expr((yyvsp[-3].list), NIL, (yylsp[-3]));
             n->agg_star = true;
             (yyval.node) = (Node *)n;
         }
#line 3360 "src/backend/parser/cypher_gram.c"
    break;

  case 131:
#line 1288 "src/backend/parser/cypher_gram.y"
        {
            FuncCall *n = (FuncCall *)make_function_expr((yyvsp[-4].list), (yyvsp[-1].list), (yylsp[-4]));
            n->agg_order = NIL;
            n->agg_distinct = true;
            (yyval.node) = (Node *)n;
        }
#line 3371 "src/backend/parser/cypher_gram.c"
    break;

  case 132:
#line 1298 "src/backend/parser/cypher_gram.y"
        {
            CoalesceExpr *c;

            c = makeNode(CoalesceExpr);
            c->args = (yyvsp[-1].list);
            c->location = (yylsp[-3]);
            (yyval.node) = (Node *) c;
        }
#line 3384 "src/backend/parser/cypher_gram.c"
    break;

  case 133:
#line 1307 "src/backend/parser/cypher_gram.y"
        {
            cypher_sub_pattern *sub;
            SubLink    *n;

            sub = make_ag_node(cypher_sub_pattern);
            sub->kind = CSP_EXISTS;
            sub->pattern = list_make1((yyvsp[-1].node));

            n = makeNode(SubLink);
            n->subLinkType = EXISTS_SUBLINK;
            n->subLinkId = 0;
            n->testexpr = NULL;
            n->operName = NIL;
            n->subselect = (Node *) sub;
            n->location = (yylsp[-3]);
            (yyval.node) = (Node *) n;
        }
#line 3406 "src/backend/parser/cypher_gram.c"
    break;

  case 134:
#line 1325 "src/backend/parser/cypher_gram.y"
        {
            (yyval.node) = make_function_expr(list_make1(makeString("exists")),
                                    list_make1((yyvsp[-1].node)), (yylsp[-2]));
        }
#line 3415 "src/backend/parser/cypher_gram.c"
    break;

  case 135:
#line 1333 "src/backend/parser/cypher_gram.y"
        {
            (yyval.node) = append_indirection((yyvsp[-2].node), (Node *)makeString((yyvsp[0].string)));
        }
#line 3423 "src/backend/parser/cypher_gram.c"
    break;

  case 137:
#line 1341 "src/backend/parser/cypher_gram.y"
        {
            cypher_param *n;

            n = make_ag_node(cypher_param);
            n->name = (yyvsp[0].string);
            n->location = (yylsp[0]);

            (yyval.node) = (Node *)n;
        }
#line 3437 "src/backend/parser/cypher_gram.c"
    break;

  case 138:
#line 1351 "src/backend/parser/cypher_gram.y"
        {
            (yyval.node) = (yyvsp[-1].node);
        }
#line 3445 "src/backend/parser/cypher_gram.c"
    break;

  case 142:
#line 1361 "src/backend/parser/cypher_gram.y"
        {
            (yyval.node) = make_int_const((yyvsp[0].integer), (yylsp[0]));
        }
#line 3453 "src/backend/parser/cypher_gram.c"
    break;

  case 143:
#line 1365 "src/backend/parser/cypher_gram.y"
        {
            (yyval.node) = make_float_const((yyvsp[0].string), (yylsp[0]));
        }
#line 3461 "src/backend/parser/cypher_gram.c"
    break;

  case 144:
#line 1369 "src/backend/parser/cypher_gram.y"
        {
            (yyval.node) = make_string_const((yyvsp[0].string), (yylsp[0]));
        }
#line 3469 "src/backend/parser/cypher_gram.c"
    break;

  case 145:
#line 1373 "src/backend/parser/cypher_gram.y"
        {
            (yyval.node) = make_bool_const(true, (yylsp[0]));
        }
#line 3477 "src/backend/parser/cypher_gram.c"
    break;

  case 146:
#line 1377 "src/backend/parser/cypher_gram.y"
        {
            (yyval.node) = make_bool_const(false, (yylsp[0]));
        }
#line 3485 "src/backend/parser/cypher_gram.c"
    break;

  case 147:
#line 1381 "src/backend/parser/cypher_gram.y"
        {
            (yyval.node) = make_null_const((yylsp[0]));
        }
#line 3493 "src/backend/parser/cypher_gram.c"
    break;

  case 150:
#line 1390 "src/backend/parser/cypher_gram.y"
        {
            cypher_map *n;

            n = make_ag_node(cypher_map);
            n->keyvals = (yyvsp[-1].list);

            (yyval.node) = (Node *)n;
        }
#line 3506 "src/backend/parser/cypher_gram.c"
    break;

  case 151:
#line 1402 "src/backend/parser/cypher_gram.y"
        {
            (yyval.list) = NIL;
        }
#line 3514 "src/backend/parser/cypher_gram.c"
    break;

  case 153:
#line 1410 "src/backend/parser/cypher_gram.y"
        {
            (yyval.list) = list_make2(makeString((yyvsp[-2].string)), (yyvsp[0].node));
        }
#line 3522 "src/backend/parser/cypher_gram.c"
    break;

  case 154:
#line 1414 "src/backend/parser/cypher_gram.y"
        {
            (yyval.list) = lappend(lappend((yyvsp[-4].list), makeString((yyvsp[-2].string))), (yyvsp[0].node));
        }
#line 3530 "src/backend/parser/cypher_gram.c"
    break;

  case 155:
#line 1421 "src/backend/parser/cypher_gram.y"
        {
            cypher_list *n;

            n = make_ag_node(cypher_list);
            n->elems = (yyvsp[-1].list);

            (yyval.node) = (Node *)n;
        }
#line 3543 "src/backend/parser/cypher_gram.c"
    break;

  case 156:
#line 1433 "src/backend/parser/cypher_gram.y"
        {
            CaseExpr *n;

            n = makeNode(CaseExpr);
            n->casetype = InvalidOid;
            n->arg = (Expr *) (yyvsp[-3].node);
            n->args = (yyvsp[-2].list);
            n->defresult = (Expr *) (yyvsp[-1].node);
            n->location = (yylsp[-4]);
            (yyval.node) = (Node *) n;
        }
#line 3559 "src/backend/parser/cypher_gram.c"
    break;

  case 157:
#line 1445 "src/backend/parser/cypher_gram.y"
        {
            CaseExpr *n;

            n = makeNode(CaseExpr);
            n->casetype = InvalidOid;
            n->args = (yyvsp[-2].list);
            n->defresult = (Expr *) (yyvsp[-1].node);
            n->location = (yylsp[-3]);
            (yyval.node) = (Node *) n;
        }
#line 3574 "src/backend/parser/cypher_gram.c"
    break;

  case 158:
#line 1459 "src/backend/parser/cypher_gram.y"
        {
            (yyval.list) = list_make1((yyvsp[0].node));
        }
#line 3582 "src/backend/parser/cypher_gram.c"
    break;

  case 159:
#line 1463 "src/backend/parser/cypher_gram.y"
        {
            (yyval.list) = lappend((yyvsp[-1].list), (yyvsp[0].node));
        }
#line 3590 "src/backend/parser/cypher_gram.c"
    break;

  case 160:
#line 1470 "src/backend/parser/cypher_gram.y"
        {
            CaseWhen   *n;

            n = makeNode(CaseWhen);
            n->expr = (Expr *) (yyvsp[-2].node);
            n->result = (Expr *) (yyvsp[0].node);
            n->location = (yylsp[-3]);
            (yyval.node) = (Node *) n;
        }
#line 3604 "src/backend/parser/cypher_gram.c"
    break;

  case 161:
#line 1483 "src/backend/parser/cypher_gram.y"
        {
            (yyval.node) = (yyvsp[0].node);
        }
#line 3612 "src/backend/parser/cypher_gram.c"
    break;

  case 162:
#line 1487 "src/backend/parser/cypher_gram.y"
        {
            (yyval.node) = NULL;
        }
#line 3620 "src/backend/parser/cypher_gram.c"
    break;

  case 163:
#line 1494 "src/backend/parser/cypher_gram.y"
        {
            ColumnRef *n;

            n = makeNode(ColumnRef);
            n->fields = list_make1(makeString((yyvsp[0].string)));
            n->location = (yylsp[0]);

            (yyval.node) = (Node *)n;
        }
#line 3634 "src/backend/parser/cypher_gram.c"
    break;

  case 164:
#line 1510 "src/backend/parser/cypher_gram.y"
        {
            (yyval.list) = list_make1(makeString((yyvsp[0].string)));
        }
#line 3642 "src/backend/parser/cypher_gram.c"
    break;

  case 165:
#line 1520 "src/backend/parser/cypher_gram.y"
        {
            (yyval.list) = list_make2(makeString((char *)(yyvsp[-2].keyword)), makeString((yyvsp[0].string)));
        }
#line 3650 "src/backend/parser/cypher_gram.c"
    break;

  case 168:
#line 1535 "src/backend/parser/cypher_gram.y"
        {
            (yyval.string) = NULL;
        }
#line 3658 "src/backend/parser/cypher_gram.c"
    break;

  case 173:
#line 1552 "src/backend/parser/cypher_gram.y"
        {
            /* we don't need to copy it, as it already has been */
            (yyval.string) = (char *) (yyvsp[0].keyword);
        }
#line 3667 "src/backend/parser/cypher_gram.c"
    break;

  case 176:
#line 1569 "src/backend/parser/cypher_gram.y"
                 { (yyval.keyword) = pnstrdup((yyvsp[0].keyword), 3); }
#line 3673 "src/backend/parser/cypher_gram.c"
    break;

  case 177:
#line 1570 "src/backend/parser/cypher_gram.y"
                 { (yyval.keyword) = pnstrdup((yyvsp[0].keyword), 7); }
#line 3679 "src/backend/parser/cypher_gram.c"
    break;

  case 178:
#line 1571 "src/backend/parser/cypher_gram.y"
                 { (yyval.keyword) = pnstrdup((yyvsp[0].keyword), 2); }
#line 3685 "src/backend/parser/cypher_gram.c"
    break;

  case 179:
#line 1572 "src/backend/parser/cypher_gram.y"
                 { (yyval.keyword) = pnstrdup((yyvsp[0].keyword), 3); }
#line 3691 "src/backend/parser/cypher_gram.c"
    break;

  case 180:
#line 1573 "src/backend/parser/cypher_gram.y"
                 { (yyval.keyword) = pnstrdup((yyvsp[0].keyword), 9); }
#line 3697 "src/backend/parser/cypher_gram.c"
    break;

  case 181:
#line 1574 "src/backend/parser/cypher_gram.y"
                 { (yyval.keyword) = pnstrdup((yyvsp[0].keyword), 2); }
#line 3703 "src/backend/parser/cypher_gram.c"
    break;

  case 182:
#line 1575 "src/backend/parser/cypher_gram.y"
                 { (yyval.keyword) = pnstrdup((yyvsp[0].keyword), 4); }
#line 3709 "src/backend/parser/cypher_gram.c"
    break;

  case 183:
#line 1576 "src/backend/parser/cypher_gram.y"
                 { (yyval.keyword) = pnstrdup((yyvsp[0].keyword), 8); }
#line 3715 "src/backend/parser/cypher_gram.c"
    break;

  case 184:
#line 1577 "src/backend/parser/cypher_gram.y"
                 { (yyval.keyword) = pnstrdup((yyvsp[0].keyword), 8); }
#line 3721 "src/backend/parser/cypher_gram.c"
    break;

  case 185:
#line 1578 "src/backend/parser/cypher_gram.y"
                 { (yyval.keyword) = pnstrdup((yyvsp[0].keyword), 6); }
#line 3727 "src/backend/parser/cypher_gram.c"
    break;

  case 186:
#line 1579 "src/backend/parser/cypher_gram.y"
                 { (yyval.keyword) = pnstrdup((yyvsp[0].keyword), 6); }
#line 3733 "src/backend/parser/cypher_gram.c"
    break;

  case 187:
#line 1580 "src/backend/parser/cypher_gram.y"
                 { (yyval.keyword) = pnstrdup((yyvsp[0].keyword), 4); }
#line 3739 "src/backend/parser/cypher_gram.c"
    break;

  case 188:
#line 1581 "src/backend/parser/cypher_gram.y"
                 { (yyval.keyword) = pnstrdup((yyvsp[0].keyword), 10); }
#line 3745 "src/backend/parser/cypher_gram.c"
    break;

  case 189:
#line 1582 "src/backend/parser/cypher_gram.y"
                 { (yyval.keyword) = pnstrdup((yyvsp[0].keyword), 6); }
#line 3751 "src/backend/parser/cypher_gram.c"
    break;

  case 190:
#line 1583 "src/backend/parser/cypher_gram.y"
                 { (yyval.keyword) = pnstrdup((yyvsp[0].keyword), 8); }
#line 3757 "src/backend/parser/cypher_gram.c"
    break;

  case 191:
#line 1584 "src/backend/parser/cypher_gram.y"
                 { (yyval.keyword) = pnstrdup((yyvsp[0].keyword), 4); }
#line 3763 "src/backend/parser/cypher_gram.c"
    break;

  case 192:
#line 1585 "src/backend/parser/cypher_gram.y"
                 { (yyval.keyword) = pnstrdup((yyvsp[0].keyword), 4); }
#line 3769 "src/backend/parser/cypher_gram.c"
    break;

  case 193:
#line 1586 "src/backend/parser/cypher_gram.y"
                 { (yyval.keyword) = pnstrdup((yyvsp[0].keyword), 6); }
#line 3775 "src/backend/parser/cypher_gram.c"
    break;

  case 194:
#line 1587 "src/backend/parser/cypher_gram.y"
                 { (yyval.keyword) = pnstrdup((yyvsp[0].keyword), 7); }
#line 3781 "src/backend/parser/cypher_gram.c"
    break;

  case 195:
#line 1588 "src/backend/parser/cypher_gram.y"
                 { (yyval.keyword) = pnstrdup((yyvsp[0].keyword), 2); }
#line 3787 "src/backend/parser/cypher_gram.c"
    break;

  case 196:
#line 1589 "src/backend/parser/cypher_gram.y"
                 { (yyval.keyword) = pnstrdup((yyvsp[0].keyword), 2); }
#line 3793 "src/backend/parser/cypher_gram.c"
    break;

  case 197:
#line 1590 "src/backend/parser/cypher_gram.y"
                 { (yyval.keyword) = pnstrdup((yyvsp[0].keyword), 6); }
#line 3799 "src/backend/parser/cypher_gram.c"
    break;

  case 198:
#line 1591 "src/backend/parser/cypher_gram.y"
                 { (yyval.keyword) = pnstrdup((yyvsp[0].keyword), 6); }
#line 3805 "src/backend/parser/cypher_gram.c"
    break;

  case 199:
#line 1592 "src/backend/parser/cypher_gram.y"
                 { (yyval.keyword) = pnstrdup((yyvsp[0].keyword), 3); }
#line 3811 "src/backend/parser/cypher_gram.c"
    break;

  case 200:
#line 1593 "src/backend/parser/cypher_gram.y"
                 { (yyval.keyword) = pnstrdup((yyvsp[0].keyword), 2); }
#line 3817 "src/backend/parser/cypher_gram.c"
    break;

  case 201:
#line 1594 "src/backend/parser/cypher_gram.y"
                 { (yyval.keyword) = pnstrdup((yyvsp[0].keyword), 5); }
#line 3823 "src/backend/parser/cypher_gram.c"
    break;

  case 202:
#line 1595 "src/backend/parser/cypher_gram.y"
                 { (yyval.keyword) = pnstrdup((yyvsp[0].keyword), 6); }
#line 3829 "src/backend/parser/cypher_gram.c"
    break;

  case 203:
#line 1596 "src/backend/parser/cypher_gram.y"
                 { (yyval.keyword) = pnstrdup((yyvsp[0].keyword), 6); }
#line 3835 "src/backend/parser/cypher_gram.c"
    break;

  case 204:
#line 1597 "src/backend/parser/cypher_gram.y"
                 { (yyval.keyword) = pnstrdup((yyvsp[0].keyword), 3); }
#line 3841 "src/backend/parser/cypher_gram.c"
    break;

  case 205:
#line 1598 "src/backend/parser/cypher_gram.y"
                 { (yyval.keyword) = pnstrdup((yyvsp[0].keyword), 4); }
#line 3847 "src/backend/parser/cypher_gram.c"
    break;

  case 206:
#line 1599 "src/backend/parser/cypher_gram.y"
                 { (yyval.keyword) = pnstrdup((yyvsp[0].keyword), 6); }
#line 3853 "src/backend/parser/cypher_gram.c"
    break;

  case 207:
#line 1600 "src/backend/parser/cypher_gram.y"
                 { (yyval.keyword) = pnstrdup((yyvsp[0].keyword), 4); }
#line 3859 "src/backend/parser/cypher_gram.c"
    break;

  case 208:
#line 1601 "src/backend/parser/cypher_gram.y"
                 { (yyval.keyword) = pnstrdup((yyvsp[0].keyword), 4); }
#line 3865 "src/backend/parser/cypher_gram.c"
    break;

  case 209:
#line 1602 "src/backend/parser/cypher_gram.y"
                 { (yyval.keyword) = pnstrdup((yyvsp[0].keyword), 7); }
#line 3871 "src/backend/parser/cypher_gram.c"
    break;

  case 210:
#line 1603 "src/backend/parser/cypher_gram.y"
                 { (yyval.keyword) = pnstrdup((yyvsp[0].keyword), 5); }
#line 3877 "src/backend/parser/cypher_gram.c"
    break;

  case 211:
#line 1604 "src/backend/parser/cypher_gram.y"
                 { (yyval.keyword) = pnstrdup((yyvsp[0].keyword), 4); }
#line 3883 "src/backend/parser/cypher_gram.c"
    break;

  case 212:
#line 1608 "src/backend/parser/cypher_gram.y"
              { (yyval.keyword) = pnstrdup((yyvsp[0].keyword), 5); }
#line 3889 "src/backend/parser/cypher_gram.c"
    break;

  case 213:
#line 1609 "src/backend/parser/cypher_gram.y"
              { (yyval.keyword) = pnstrdup((yyvsp[0].keyword), 7); }
#line 3895 "src/backend/parser/cypher_gram.c"
    break;

  case 214:
#line 1610 "src/backend/parser/cypher_gram.y"
              { (yyval.keyword) = pnstrdup((yyvsp[0].keyword), 6); }
#line 3901 "src/backend/parser/cypher_gram.c"
    break;

  case 215:
#line 1611 "src/backend/parser/cypher_gram.y"
              { (yyval.keyword) = pnstrdup((yyvsp[0].keyword), 6); }
#line 3907 "src/backend/parser/cypher_gram.c"
    break;


#line 3911 "src/backend/parser/cypher_gram.c"

      default: break;
    }
  /* User semantic actions sometimes alter yychar, and that requires
     that yytoken be updated with the new translation.  We take the
     approach of translating immediately before every use of yytoken.
     One alternative is translating here after every semantic action,
     but that translation would be missed if the semantic action invokes
     YYABORT, YYACCEPT, or YYERROR immediately after altering yychar or
     if it invokes YYBACKUP.  In the case of YYABORT or YYACCEPT, an
     incorrect destructor might then be invoked immediately.  In the
     case of YYERROR or YYBACKUP, subsequent parser actions might lead
     to an incorrect destructor call or verbose syntax error message
     before the lookahead is translated.  */
  YY_SYMBOL_PRINT ("-> $$ =", yyr1[yyn], &yyval, &yyloc);

  YYPOPSTACK (yylen);
  yylen = 0;
  YY_STACK_PRINT (yyss, yyssp);

  *++yyvsp = yyval;
  *++yylsp = yyloc;

  /* Now 'shift' the result of the reduction.  Determine what state
     that goes to, based on the state we popped back to and the rule
     number reduced by.  */
  {
    const int yylhs = yyr1[yyn] - YYNTOKENS;
    const int yyi = yypgoto[yylhs] + *yyssp;
    yystate = (0 <= yyi && yyi <= YYLAST && yycheck[yyi] == *yyssp
               ? yytable[yyi]
               : yydefgoto[yylhs]);
  }

  goto yynewstate;


/*--------------------------------------.
| yyerrlab -- here on detecting error.  |
`--------------------------------------*/
yyerrlab:
  /* Make sure we have latest lookahead translation.  See comments at
     user semantic actions for why this is necessary.  */
  yytoken = yychar == YYEMPTY ? YYEMPTY : YYTRANSLATE (yychar);

  /* If not already recovering from an error, report this error.  */
  if (!yyerrstatus)
    {
      ++yynerrs;
#if ! YYERROR_VERBOSE
      yyerror (&yylloc, scanner, extra, YY_("syntax error"));
#else
# define YYSYNTAX_ERROR yysyntax_error (&yymsg_alloc, &yymsg, \
                                        yyssp, yytoken)
      {
        char const *yymsgp = YY_("syntax error");
        int yysyntax_error_status;
        yysyntax_error_status = YYSYNTAX_ERROR;
        if (yysyntax_error_status == 0)
          yymsgp = yymsg;
        else if (yysyntax_error_status == 1)
          {
            if (yymsg != yymsgbuf)
              YYSTACK_FREE (yymsg);
            yymsg = YY_CAST (char *, YYSTACK_ALLOC (YY_CAST (YYSIZE_T, yymsg_alloc)));
            if (!yymsg)
              {
                yymsg = yymsgbuf;
                yymsg_alloc = sizeof yymsgbuf;
                yysyntax_error_status = 2;
              }
            else
              {
                yysyntax_error_status = YYSYNTAX_ERROR;
                yymsgp = yymsg;
              }
          }
        yyerror (&yylloc, scanner, extra, yymsgp);
        if (yysyntax_error_status == 2)
          goto yyexhaustedlab;
      }
# undef YYSYNTAX_ERROR
#endif
    }

  yyerror_range[1] = yylloc;

  if (yyerrstatus == 3)
    {
      /* If just tried and failed to reuse lookahead token after an
         error, discard it.  */

      if (yychar <= YYEOF)
        {
          /* Return failure if at end of input.  */
          if (yychar == YYEOF)
            YYABORT;
        }
      else
        {
          yydestruct ("Error: discarding",
                      yytoken, &yylval, &yylloc, scanner, extra);
          yychar = YYEMPTY;
        }
    }

  /* Else will try to reuse lookahead token after shifting the error
     token.  */
  goto yyerrlab1;


/*---------------------------------------------------.
| yyerrorlab -- error raised explicitly by YYERROR.  |
`---------------------------------------------------*/
yyerrorlab:
  /* Pacify compilers when the user code never invokes YYERROR and the
     label yyerrorlab therefore never appears in user code.  */
  if (0)
    YYERROR;

  /* Do not reclaim the symbols of the rule whose action triggered
     this YYERROR.  */
  YYPOPSTACK (yylen);
  yylen = 0;
  YY_STACK_PRINT (yyss, yyssp);
  yystate = *yyssp;
  goto yyerrlab1;


/*-------------------------------------------------------------.
| yyerrlab1 -- common code for both syntax error and YYERROR.  |
`-------------------------------------------------------------*/
yyerrlab1:
  yyerrstatus = 3;      /* Each real token shifted decrements this.  */

  for (;;)
    {
      yyn = yypact[yystate];
      if (!yypact_value_is_default (yyn))
        {
          yyn += YYTERROR;
          if (0 <= yyn && yyn <= YYLAST && yycheck[yyn] == YYTERROR)
            {
              yyn = yytable[yyn];
              if (0 < yyn)
                break;
            }
        }

      /* Pop the current state because it cannot handle the error token.  */
      if (yyssp == yyss)
        YYABORT;

      yyerror_range[1] = *yylsp;
      yydestruct ("Error: popping",
                  yystos[yystate], yyvsp, yylsp, scanner, extra);
      YYPOPSTACK (1);
      yystate = *yyssp;
      YY_STACK_PRINT (yyss, yyssp);
    }

  YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
  *++yyvsp = yylval;
  YY_IGNORE_MAYBE_UNINITIALIZED_END

  yyerror_range[2] = yylloc;
  /* Using YYLLOC is tempting, but would change the location of
     the lookahead.  YYLOC is available though.  */
  YYLLOC_DEFAULT (yyloc, yyerror_range, 2);
  *++yylsp = yyloc;

  /* Shift the error token.  */
  YY_SYMBOL_PRINT ("Shifting", yystos[yyn], yyvsp, yylsp);

  yystate = yyn;
  goto yynewstate;


/*-------------------------------------.
| yyacceptlab -- YYACCEPT comes here.  |
`-------------------------------------*/
yyacceptlab:
  yyresult = 0;
  goto yyreturn;


/*-----------------------------------.
| yyabortlab -- YYABORT comes here.  |
`-----------------------------------*/
yyabortlab:
  yyresult = 1;
  goto yyreturn;


#if !defined yyoverflow || YYERROR_VERBOSE
/*-------------------------------------------------.
| yyexhaustedlab -- memory exhaustion comes here.  |
`-------------------------------------------------*/
yyexhaustedlab:
  yyerror (&yylloc, scanner, extra, YY_("memory exhausted"));
  yyresult = 2;
  /* Fall through.  */
#endif


/*-----------------------------------------------------.
| yyreturn -- parsing is finished, return the result.  |
`-----------------------------------------------------*/
yyreturn:
  if (yychar != YYEMPTY)
    {
      /* Make sure we have latest lookahead translation.  See comments at
         user semantic actions for why this is necessary.  */
      yytoken = YYTRANSLATE (yychar);
      yydestruct ("Cleanup: discarding lookahead",
                  yytoken, &yylval, &yylloc, scanner, extra);
    }
  /* Do not reclaim the symbols of the rule whose action triggered
     this YYABORT or YYACCEPT.  */
  YYPOPSTACK (yylen);
  YY_STACK_PRINT (yyss, yyssp);
  while (yyssp != yyss)
    {
      yydestruct ("Cleanup: popping",
                  yystos[+*yyssp], yyvsp, yylsp, scanner, extra);
      YYPOPSTACK (1);
    }
#ifndef yyoverflow
  if (yyss != yyssa)
    YYSTACK_FREE (yyss);
#endif
#if YYERROR_VERBOSE
  if (yymsg != yymsgbuf)
    YYSTACK_FREE (yymsg);
#endif
  return yyresult;
}
#line 1614 "src/backend/parser/cypher_gram.y"


/*
 * logical operators
 */

static Node *make_or_expr(Node *lexpr, Node *rexpr, int location)
{
    // flatten "a OR b OR c ..." to a single BoolExpr on sight
    if (IsA(lexpr, BoolExpr))
    {
        BoolExpr *bexpr = (BoolExpr *)lexpr;

        if (bexpr->boolop == OR_EXPR)
        {
            bexpr->args = lappend(bexpr->args, rexpr);

            return (Node *)bexpr;
        }
    }

    return (Node *)makeBoolExpr(OR_EXPR, list_make2(lexpr, rexpr), location);
}

static Node *make_and_expr(Node *lexpr, Node *rexpr, int location)
{
    // flatten "a AND b AND c ..." to a single BoolExpr on sight
    if (IsA(lexpr, BoolExpr))
    {
        BoolExpr *bexpr = (BoolExpr *)lexpr;

        if (bexpr->boolop == AND_EXPR)
        {
            bexpr->args = lappend(bexpr->args, rexpr);

            return (Node *)bexpr;
        }
    }

    return (Node *)makeBoolExpr(AND_EXPR, list_make2(lexpr, rexpr), location);
}

static Node *make_not_expr(Node *expr, int location)
{
    return (Node *)makeBoolExpr(NOT_EXPR, list_make1(expr), location);
}

/*
 * arithmetic operators
 */

static Node *do_negate(Node *n, int location)
{
    if (IsA(n, A_Const))
    {
        A_Const *c = (A_Const *)n;

        // report the constant's location as that of the '-' sign
        c->location = location;

        if (c->val.type == T_Integer)
        {
            c->val.val.ival = -c->val.val.ival;
            return n;
        }
        else if (c->val.type == T_Float)
        {
            do_negate_float(&c->val);
            return n;
        }
    }

    return (Node *)makeSimpleA_Expr(AEXPR_OP, "-", NULL, n, location);
}

static void do_negate_float(Value *v)
{
    Assert(IsA(v, Float));

    if (v->val.str[0] == '-')
        v->val.str = v->val.str + 1; // just strip the '-'
    else
        v->val.str = psprintf("-%s", v->val.str);
}

/*
 * indirection
 */

static Node *append_indirection(Node *expr, Node *selector)
{
    A_Indirection *indir;

    if (IsA(expr, A_Indirection))
    {
        indir = (A_Indirection *)expr;
        indir->indirection = lappend(indir->indirection, selector);

        return expr;
    }
    else
    {
        indir = makeNode(A_Indirection);
        indir->arg = expr;
        indir->indirection = list_make1(selector);

        return (Node *)indir;
    }
}

/*
 * literals
 */

static Node *make_int_const(int i, int location)
{
    A_Const *n;

    n = makeNode(A_Const);
    n->val.type = T_Integer;
    n->val.val.ival = i;
    n->location = location;

    return (Node *)n;
}

static Node *make_float_const(char *s, int location)
{
    A_Const *n;

    n = makeNode(A_Const);
    n->val.type = T_Float;
    n->val.val.str = s;
    n->location = location;

    return (Node *)n;
}

static Node *make_string_const(char *s, int location)
{
    A_Const *n;

    n = makeNode(A_Const);
    n->val.type = T_String;
    n->val.val.str = s;
    n->location = location;

    return (Node *)n;
}

static Node *make_bool_const(bool b, int location)
{
    cypher_bool_const *n;

    n = make_ag_node(cypher_bool_const);
    n->boolean = b;
    n->location = location;

    return (Node *)n;
}

static Node *make_null_const(int location)
{
    A_Const *n;

    n = makeNode(A_Const);
    n->val.type = T_Null;
    n->location = location;

    return (Node *)n;
}

/*
 * typecast
 */
static Node *make_typecast_expr(Node *expr, char *typecast, int location)
{
    cypher_typecast *node;

    node = make_ag_node(cypher_typecast);
    node->expr = expr;
    node->typecast = typecast;
    node->location = location;

    return (Node *)node;
}

/*
 * functions
 */
static Node *make_function_expr(List *func_name, List *exprs, int location)
{
    FuncCall *fnode;

    /* AGE function names are unqualified. So, their list size = 1 */
    if (list_length(func_name) == 1)
    {
        List *funcname;
        char *name;

        /* get the name of the function */
        name = ((Value*)linitial(func_name))->val.str;

        /*
         * Check for openCypher functions that are directly mapped to PG
         * functions. We may want to find a better way to do this, as there
         * could be many.
         */
        if (pg_strcasecmp(name, "rand") == 0)
            funcname = SystemFuncName("random");
        else if (pg_strcasecmp(name, "pi") == 0)
            funcname = SystemFuncName("pi");
        else if (pg_strcasecmp(name, "count") == 0)
            funcname = SystemFuncName("count");
        else
            /*
             * We don't qualify AGE functions here. This is done in the
             * transform layer and allows us to know which functions are ours.
             */
            funcname = func_name;

        /* build the function call */
        fnode = makeFuncCall(funcname, exprs, location);
    }
    /* all other functions are passed as is */
    else
        fnode = makeFuncCall(func_name, exprs, location);

    /* return the node */
    return (Node *)fnode;
}
