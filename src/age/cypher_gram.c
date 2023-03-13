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
#line 1 "src/age/cypher_gram.y"

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

#include "pool.h"
#include "utils/elog.h"
#include "parser/makefuncs.h"
#include "parser/nodes.h"
#include "parser/parsenodes.h"
#include "parser/pg_list.h"
#include "parser/primnodes.h"
#include "parser/value.h"
#include "parser/parser.h"

#include "age/ag_nodes.h"
#include "age/cypher_nodes.h"
#include "age/ag_scanner.h"
#include "age/cypher_gram.h"

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

#line 124 "src/age/cypher_gram.c"

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

#line 252 "src/age/cypher_gram.c"

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
#line 184 "src/age/cypher_gram.y"

//
// unique name generation
#define UNIQUE_NAME_NULL_PREFIX "_unique_null_prefix"
static char *create_unique_name(char *prefix_name);
static unsigned long get_a_unique_number(void);

// logical operators
static Node *make_or_expr(Node *lexpr, Node *rexpr, int location);
static Node *make_and_expr(Node *lexpr, Node *rexpr, int location);
static Node *make_xor_expr(Node *lexpr, Node *rexpr, int location);
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

// setops
static Node *make_set_op(SetOperation op, bool all_or_distinct, List *larg,
                         List *rarg);

// VLE
static cypher_relationship *build_VLE_relation(List *left_arg,
                                               cypher_relationship *cr,
                                               Node *right_arg,
                                               int left_arg_location,
                                               int cr_location);
// comparison
static bool is_A_Expr_a_comparison_operation(A_Expr *a);
static Node *build_comparison_expression(Node *left_grammar_node,
                                         Node *right_grammar_node,
                                         char *opr_name, int location);

#line 331 "src/age/cypher_gram.c"


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
#define YYFINAL  9
/* YYLAST -- Last index in YYTABLE.  */
#define YYLAST   1770

/* YYNTOKENS -- Number of terminals.  */
#define YYNTOKENS  83
/* YYNNTS -- Number of nonterminals.  */
#define YYNNTS  84
/* YYNRULES -- Number of rules.  */
#define YYNRULES  242
/* YYNSTATES -- Number of states.  */
#define YYNSTATES  381

#define YYUNDEFTOK  2
#define YYMAXUTOK   318


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
       2,     2,     2,     2,     2,     2,     2,    70,     2,     2,
      75,    76,    68,    66,    78,    67,    77,    69,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,    80,    79,
      64,    63,    65,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,    73,     2,    74,    71,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,    81,     2,    82,     2,     2,     2,     2,
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
      55,    56,    57,    58,    59,    60,    61,    62,    72
};

#if YYDEBUG
  /* YYRLINE[YYN] -- Source line where rule number YYN was defined.  */
static const yytype_int16 yyrline[] =
{
       0,   240,   240,   261,   275,   289,   303,   321,   325,   332,
     339,   349,   353,   360,   364,   370,   372,   376,   380,   385,
     396,   404,   407,   414,   418,   426,   429,   436,   437,   442,
     445,   449,   453,   460,   461,   462,   463,   464,   465,   469,
     489,   495,   504,   514,   520,   527,   528,   531,   538,   551,
     568,   572,   579,   591,   603,   624,   627,   634,   638,   645,
     662,   665,   669,   673,   677,   685,   688,   696,   699,   706,
     737,   776,   790,   795,   802,   822,   838,   852,   856,   863,
     875,   890,   904,   908,   915,   933,   947,   952,   961,   978,
     981,   993,   997,  1005,  1006,  1019,  1033,  1034,  1041,  1045,
    1070,  1085,  1094,  1103,  1115,  1129,  1144,  1147,  1155,  1158,
    1159,  1177,  1181,  1185,  1189,  1193,  1197,  1201,  1205,  1209,
    1213,  1217,  1221,  1225,  1229,  1233,  1237,  1241,  1245,  1256,
    1267,  1271,  1283,  1295,  1307,  1312,  1323,  1341,  1419,  1423,
    1428,  1431,  1435,  1439,  1447,  1450,  1454,  1455,  1459,  1463,
    1468,  1484,  1494,  1503,  1521,  1529,  1536,  1537,  1547,  1551,
    1552,  1553,  1557,  1561,  1565,  1569,  1573,  1577,  1581,  1582,
    1586,  1599,  1602,  1606,  1610,  1617,  1629,  1641,  1655,  1659,
    1666,  1679,  1684,  1690,  1706,  1716,  1723,  1727,  1732,  1735,
    1739,  1743,  1747,  1748,  1756,  1757,  1766,  1767,  1768,  1769,
    1770,  1771,  1772,  1773,  1774,  1775,  1776,  1777,  1778,  1779,
    1780,  1781,  1782,  1783,  1784,  1785,  1786,  1787,  1788,  1789,
    1790,  1791,  1792,  1793,  1794,  1795,  1796,  1797,  1798,  1799,
    1800,  1801,  1802,  1803,  1804,  1805,  1806,  1807,  1808,  1812,
    1813,  1814,  1815
};
#endif

#if YYDEBUG || YYERROR_VERBOSE || 0
/* YYTNAME[SYMBOL-NUM] -- String name of the symbol SYMBOL-NUM.
   First, the terminals, then, starting at YYNTOKENS, nonterminals.  */
static const char *const yytname[] =
{
  "$end", "error", "$undefined", "INTEGER", "DECIMAL", "STRING",
  "IDENTIFIER", "PARAMETER", "NOT_EQ", "LT_EQ", "GT_EQ", "DOT_DOT",
  "TYPECAST", "PLUS_EQ", "EQ_TILDE", "ALL", "ANALYZE", "AND", "AS", "ASC",
  "ASCENDING", "BY", "CALL", "CASE", "COALESCE", "CONTAINS", "CREATE",
  "DELETE", "DESC", "DESCENDING", "DETACH", "DISTINCT", "ELSE", "END_P",
  "ENDS", "EXISTS", "EXPLAIN", "FALSE_P", "IN", "IS", "LIMIT", "MATCH",
  "MERGE", "NOT", "NULL_P", "OPTIONAL", "OR", "ORDER", "REMOVE", "RETURN",
  "SET", "SKIP", "STARTS", "THEN", "TRUE_P", "UNION", "UNWIND", "VERBOSE",
  "WHEN", "WHERE", "WITH", "XOR", "YIELD", "'='", "'<'", "'>'", "'+'",
  "'-'", "'*'", "'/'", "'%'", "'^'", "UNARY_MINUS", "'['", "']'", "'('",
  "')'", "'.'", "','", "';'", "':'", "'{'", "'}'", "$accept", "stmt",
  "cypher_stmt", "call_stmt", "yield_item_list", "yield_item",
  "semicolon_opt", "all_or_distinct", "single_query", "query_part_init",
  "query_part_last", "reading_clause_list", "reading_clause",
  "updating_clause_list_0", "updating_clause_list_1", "updating_clause",
  "cypher_varlen_opt", "cypher_range_opt", "cypher_range_idx",
  "cypher_range_idx_opt", "Iconst", "return", "return_item_list",
  "return_item", "order_by_opt", "sort_item_list", "sort_item",
  "order_opt", "skip_opt", "limit_opt", "with", "match", "optional_opt",
  "unwind", "create", "set", "set_item_list", "set_item", "remove",
  "remove_item_list", "remove_item", "delete", "detach_opt", "merge",
  "where_opt", "pattern", "path", "anonymous_path",
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
     305,   306,   307,   308,   309,   310,   311,   312,   313,   314,
     315,   316,   317,    61,    60,    62,    43,    45,    42,    47,
      37,    94,   318,    91,    93,    40,    41,    46,    44,    59,
      58,   123,   125
};
# endif

#define YYPACT_NINF (-299)

#define yypact_value_is_default(Yyn) \
  ((Yyn) == YYPACT_NINF)

#define YYTABLE_NINF (-185)

#define yytable_value_is_error(Yyn) \
  ((Yyn) == YYTABLE_NINF)

  /* YYPACT[STATE-NUM] -- Index in YYTABLE of the portion describing
     STATE-NUM.  */
static const yytype_int16 yypact[] =
{
      -4,     1,    39,   -33,  -299,  -299,    17,  -299,   -33,  -299,
      16,  -299,  -299,  -299,   973,  -299,   -33,   -33,  -299,  -299,
    -299,  -299,  1708,     7,  -299,     7,  -299,   692,   692,   692,
    -299,  -299,   -25,   169,  -299,  -299,    60,  -299,  -299,  -299,
    -299,  -299,    53,  -299,   -33,  -299,  -299,  -299,  -299,  -299,
    -299,  -299,  -299,  -299,  -299,  -299,  -299,  -299,  -299,  -299,
    -299,  -299,  -299,  -299,  -299,  -299,  -299,  -299,  -299,  -299,
    -299,  -299,  -299,  -299,  -299,  -299,  -299,  -299,  -299,  -299,
    -299,  -299,  -299,  -299,  -299,  -299,  -299,  -299,  -299,  -299,
    -299,  -299,    46,    34,  -299,    37,     9,    33,  -299,  -299,
    -299,   -30,  -299,    58,  -299,  -299,  -299,  -299,  -299,  -299,
     765,    47,    48,  -299,   692,  -299,  -299,   692,   692,   692,
    1658,    49,  -299,  1437,  -299,  -299,  -299,  -299,  -299,  -299,
    -299,  -299,  -299,  -299,    51,    50,  -299,   835,   901,   473,
     546,  -299,  -299,  -299,     7,   692,  -299,   692,   394,   118,
     118,    43,  -299,    57,     7,    62,    65,    66,    67,   692,
     967,   -13,  -299,   692,     7,  1569,   301,  1437,    52,    69,
    1033,  -299,  -299,  -299,  -299,    63,    68,    64,  -299,  -299,
    -299,  -299,  -299,   692,   692,   692,   692,   118,   692,   692,
     692,    80,   692,    61,   692,    87,   692,   692,   692,   692,
     692,   692,   692,   692,   692,   692,   692,   692,   692,   692,
     692,   118,   619,  -299,   -29,  -299,  1103,   619,   -29,   -36,
      52,   -34,  -299,  1169,   692,    72,  -299,   -26,  -299,  -299,
    1658,     4,  -299,    65,   118,    83,  -299,  -299,  1235,   692,
     -13,   692,  -299,   119,    -7,    75,    77,    78,   692,  -299,
    -299,  -299,  1658,   692,  -299,  1592,  1592,  1592,  -299,   909,
    1503,   909,   692,   234,   110,  -299,   224,   692,  1569,  1592,
    1592,  1592,    26,    26,    29,    29,    29,   816,  1301,   149,
     152,  -299,  1437,  1371,  -299,   -29,   145,   619,   117,   118,
     -29,   117,   692,  -299,   692,  -299,   118,    -3,  -299,  -299,
    -299,  -299,  -299,   108,  -299,   120,    57,   121,   692,   155,
    1437,  -299,  -299,  -299,  -299,  1658,  1437,   113,  1437,   909,
    -299,   909,  -299,   692,   117,   692,  -299,   692,   154,  -299,
     117,   154,  1437,  -299,  -299,  -299,  -299,  -299,   129,  -299,
    1437,  -299,  -299,   692,  1437,   124,   154,   122,  -299,   300,
    1437,   692,  -299,   154,   142,    25,     4,  1437,  -299,  -299,
     692,  -299,  -299,  -299,  -299,  -299,  1437,   142,  -299,  -299,
    -299,   192,   193,  -299,   131,  -299,  -299,   203,  -299,  -299,
    -299
};

  /* YYDEFACT[STATE-NUM] -- Default reduction number in state STATE-NUM.
     Performed when YYTABLE does not specify something else to do.  Zero
     means the default is an error.  */
static const yytype_uint8 yydefact[] =
{
      21,    21,     0,    15,     7,    25,    21,    21,    15,     1,
      19,    16,     2,    20,    29,    21,    15,    15,     3,    17,
      18,    21,     0,     0,    86,     0,    72,     0,     0,     0,
      38,    26,     0,    23,    31,    27,     0,    28,    33,    34,
      35,    36,     0,    37,    15,     5,     4,     8,   191,   196,
     197,   198,   199,   200,   201,   202,   203,   204,   205,   206,
     207,   208,   209,   210,   211,   212,   213,   214,   215,   216,
     217,   218,   219,   220,   221,   222,   223,   224,   225,   226,
     227,   228,   229,   230,   231,   232,   234,   233,   235,   236,
     237,   238,     9,     0,   184,     0,   188,    75,    91,    93,
      95,    96,    98,     0,   187,    88,   162,   163,   164,   157,
     204,   205,   215,   166,   222,   167,   165,     0,   144,     0,
     171,    81,    82,    84,   161,   146,   147,   139,   156,   168,
     169,   159,   160,   183,   187,    76,    77,     0,     0,     0,
       0,    24,    22,    32,     0,     0,     6,     0,     0,     0,
     188,     0,   189,   106,     0,     0,   105,     0,     0,   233,
       0,   182,   178,     0,     0,   114,   130,   142,   145,     0,
       0,   239,   240,   241,   242,     0,   172,     0,   192,   186,
     193,   194,   195,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,   140,     0,     0,     0,
       0,     0,   212,    54,    55,    50,    53,   212,    55,    89,
      85,    89,    11,    14,   212,     0,   148,     0,   185,    97,
       0,   108,    92,   105,   188,     0,    99,    94,     0,     0,
     182,     0,   179,     0,     0,     0,     0,     0,     0,   175,
     158,   170,     0,     0,    83,   116,   118,   120,   138,   134,
     112,   133,     0,   127,     0,   128,   111,     0,   113,   115,
     117,   119,   121,   122,   123,   124,   125,   126,   141,     0,
     137,    78,    80,    79,    74,    55,     0,     0,    65,     0,
      55,    65,     0,    71,     0,    10,     0,     0,   150,   149,
     107,   190,   110,     0,   109,     0,   106,   101,     0,     0,
     181,   177,   152,   153,   154,     0,   143,     0,   173,   132,
     129,   131,   135,   140,    65,     0,    51,     0,    67,    52,
      65,    67,    90,    12,    13,   151,   100,   103,    40,   102,
     180,   176,   155,     0,   141,     0,    67,    56,    57,    60,
      66,     0,    49,    67,    89,    43,   108,   174,   136,    48,
       0,    61,    62,    63,    64,    59,    68,    89,    70,    47,
      39,    41,     0,    44,     0,    58,    69,    46,   104,    45,
      42
};

  /* YYPGOTO[NTERM-NUM].  */
static const yytype_int16 yypgoto[] =
{
    -299,  -299,    55,  -299,  -299,   -87,    13,  -299,  -299,  -299,
    -299,  -299,  -299,  -299,  -299,   175,  -299,  -299,  -168,  -167,
    -299,  -299,  -126,   -71,  -192,  -299,  -140,  -299,  -271,  -298,
    -299,  -299,  -299,  -299,  -299,  -299,  -299,    14,  -299,  -299,
      40,  -299,  -299,  -299,  -218,    81,   -20,   -92,  -299,   128,
      70,  -299,    -5,   -76,  -125,   -27,   -84,  -136,  -299,  -299,
     222,  -299,  -299,  -299,  -299,  -223,  -299,  -299,  -299,  -299,
      85,  -151,    10,    88,  -299,  -236,   -19,    19,  -299,    93,
      21,  -299,  -113,  -299
};

  /* YYDEFGOTO[NTERM-NUM].  */
static const yytype_int16 yydefgoto[] =
{
      -1,     2,     3,    30,   221,   222,    12,    21,     4,     5,
      13,    14,    31,    32,    33,    34,   356,   370,   371,   372,
     373,   141,   214,   215,   288,   347,   348,   365,   328,   352,
     142,    35,    36,    37,    38,    39,   135,   136,    40,   121,
     122,    41,    42,    43,   293,    97,    98,    99,   100,   101,
     102,   157,   235,   231,   303,   167,   279,   168,   169,   124,
     125,   126,   246,   127,   128,   129,   175,   176,   130,   131,
     161,   162,   243,   132,    93,   177,   133,   153,   300,   134,
     179,   180,    95,   182
};

  /* YYTABLE[YYPACT[STATE-NUM]] -- What to do in state STATE-NUM.  If
     positive, shift that token.  If negative, reduce the rule whose
     number is the opposite.  If YYTABLE_NINF, syntax error.  */
static const yytype_int16 yytable[] =
{
     123,   137,   138,   295,   103,   105,   103,   181,   304,   220,
     242,   302,   227,    48,   218,    48,   317,     6,   286,   241,
     331,    18,    10,   292,   139,   292,   291,   244,   369,    45,
      46,    19,     1,   354,   155,   140,   -46,   156,   187,     9,
     188,   187,   154,   188,   294,   239,    11,    20,   359,   287,
     299,   190,   248,   346,   190,   367,     8,   146,     7,   353,
     191,    16,    17,   191,   192,   193,   237,   192,   193,   312,
      44,   248,   245,   335,    15,   248,    47,   152,   195,   342,
     145,   195,    96,   160,   150,   120,   285,   165,   297,   242,
     166,   290,   170,   324,   202,   203,   204,   205,   330,   206,
     205,   144,   206,   207,   264,   265,   207,   155,   147,   148,
     156,   154,   216,   216,   149,    94,   104,   181,   104,   229,
     223,   158,   163,   164,    48,   103,  -184,   183,   208,   233,
     248,   152,   238,   304,   232,   103,   368,   230,   234,   181,
     262,   150,    96,   249,   253,   251,   252,   267,   298,   376,
     307,   313,   311,   314,   320,   315,   123,   255,   256,   257,
     323,   259,   260,   261,   187,   263,   325,   266,   327,   268,
     269,   270,   271,   272,   273,   274,   275,   276,   277,   278,
     280,   137,   282,   283,   336,   216,   339,   337,   341,   104,
     216,    22,   284,   343,   351,    23,   -87,   355,   358,    24,
     360,   292,   181,   -45,   377,   378,   369,   333,   143,   379,
     380,    25,   238,   178,   310,   152,   326,    27,   -30,    28,
     375,   316,   281,   254,   151,   219,   318,   236,   305,   -30,
     338,   374,   184,   185,   186,   319,   187,   104,   188,   345,
     321,   189,   228,   104,    92,   240,   187,   104,   188,   190,
     309,   301,   247,   306,     0,     0,     0,   104,   191,   190,
     216,     0,   192,   193,     0,   332,     0,   223,   191,     0,
     329,     0,  -185,  -185,     0,     0,   195,   334,     0,     0,
     258,   340,     0,     0,     0,   196,   195,   197,   198,   199,
     200,   201,   202,   203,   204,   205,   344,   206,   349,     0,
     350,   207,     0,     0,   104,     0,     0,   206,   184,   185,
     186,   207,   187,   187,   188,   188,   357,   189,     0,   361,
     362,     0,     0,   178,   366,   190,   190,   104,   363,   364,
       0,     0,     0,   349,   191,   191,     0,     0,   192,   193,
       0,     0,     0,     0,     0,   178,   194,     0,     0,     0,
       0,     0,   195,   195,     0,     0,     0,     0,     0,     0,
       0,   196,     0,   197,   198,   199,   200,   201,   202,   203,
     204,   205,     0,   206,   206,     0,     0,   207,   207,     0,
       0,     0,   104,     0,     0,     0,     0,     0,     0,   104,
       0,     0,     0,     0,     0,     0,     0,   106,   107,   108,
      48,   109,     0,     0,     0,     0,     0,     0,   178,    49,
      50,    51,    52,    53,    54,    55,    56,   110,   111,    59,
      60,    61,    62,    63,    64,   224,    66,     0,    67,   112,
      69,   113,    70,    71,    72,    73,    74,   114,   115,    76,
      77,    78,    79,    80,    81,    82,    83,    84,   116,    85,
       0,    86,    87,    88,    89,    90,    91,     0,     0,     0,
       0,   117,   225,     0,     0,     0,     0,   118,     0,   119,
     226,     0,     0,     0,     0,   120,   106,   107,   108,    48,
     109,     0,     0,     0,     0,     0,     0,     0,    49,    50,
      51,    52,    53,    54,    55,    56,   110,   111,    59,    60,
      61,    62,    63,    64,   212,    66,     0,    67,   112,    69,
     113,    70,    71,    72,    73,    74,   114,   115,    76,    77,
      78,    79,    80,    81,    82,    83,    84,   116,    85,     0,
      86,    87,    88,    89,    90,    91,     0,     0,     0,     0,
     117,   213,     0,     0,     0,     0,   118,     0,   119,   106,
     107,   108,    48,   109,   120,     0,     0,     0,     0,     0,
       0,    49,    50,    51,    52,    53,    54,    55,    56,   110,
     111,    59,    60,    61,    62,    63,    64,   217,    66,     0,
      67,   112,    69,   113,    70,    71,    72,    73,    74,   114,
     115,    76,    77,    78,    79,    80,    81,    82,    83,    84,
     116,    85,     0,    86,    87,    88,    89,    90,    91,     0,
       0,     0,     0,   117,   213,     0,     0,     0,     0,   118,
       0,   119,   106,   107,   108,    48,   109,   120,     0,     0,
       0,     0,     0,     0,    49,    50,    51,    52,    53,    54,
      55,    56,   110,   111,    59,    60,    61,    62,    63,    64,
      65,    66,     0,    67,   112,    69,   113,    70,    71,    72,
      73,    74,   114,   115,    76,    77,    78,    79,    80,    81,
      82,    83,    84,   116,    85,     0,    86,    87,    88,    89,
      90,    91,     0,     0,     0,     0,   117,   213,     0,     0,
       0,     0,   118,     0,   119,   106,   107,   108,    48,   109,
     120,     0,     0,     0,     0,     0,     0,    49,    50,    51,
      52,    53,    54,    55,    56,   110,   111,    59,    60,    61,
      62,    63,    64,    65,    66,     0,    67,   112,    69,   113,
      70,    71,    72,    73,    74,   114,   115,    76,    77,    78,
      79,    80,    81,    82,    83,    84,   116,    85,     0,    86,
      87,    88,    89,    90,    91,     0,     0,     0,     0,   117,
       0,     0,     0,     0,     0,   118,     0,   119,   106,   107,
     108,    48,   109,   120,     0,     0,     0,     0,     0,     0,
      49,    50,    51,    52,    53,    54,    55,    56,   110,   111,
      59,    60,    61,    62,    63,    64,    65,    66,     0,    67,
     112,    69,   113,    70,    71,    72,    73,    74,   114,   115,
      76,    77,    78,    79,    80,    81,    82,    83,    84,   116,
      85,     0,    86,   159,    88,    89,    90,    91,   187,     0,
     188,     0,   117,     0,     0,     0,     0,     0,   118,     0,
     119,   190,     0,   184,   185,   186,   120,   187,   209,   188,
     191,     0,   189,     0,   192,   193,     0,     0,     0,     0,
     190,     0,     0,     0,     0,     0,     0,     0,   195,   191,
       0,     0,     0,   192,   193,     0,     0,     0,     0,     0,
       0,   194,     0,     0,     0,     0,     0,   195,     0,   206,
       0,     0,     0,   207,     0,     0,   196,     0,   210,   198,
     199,   200,   201,   202,   203,   204,   205,     0,   206,   184,
     185,   186,   207,   187,     0,   188,     0,     0,   189,   211,
       0,   187,     0,  -185,     0,     0,   190,     0,     0,     0,
       0,     0,     0,     0,  -185,   191,     0,     0,     0,   192,
     193,     0,     0,  -185,     0,     0,     0,   194,     0,     0,
       0,     0,     0,   195,     0,     0,     0,     0,     0,     0,
       0,  -185,   196,     0,   197,   198,   199,   200,   201,   202,
     203,   204,   205,     0,   206,   184,   185,   186,   207,   187,
       0,   188,   206,     0,   189,     0,   207,     0,     0,     0,
       0,     0,   190,     0,     0,    22,     0,     0,     0,    23,
     -87,   191,     0,    24,     0,   192,   193,     0,     0,     0,
       0,     0,     0,   194,   -73,    25,     0,     0,    26,   195,
       0,    27,     0,    28,     0,   239,     0,     0,   196,    29,
     197,   198,   199,   200,   201,   202,   203,   204,   205,     0,
     206,   184,   185,   186,   207,   187,     0,   188,     0,     0,
     189,     0,     0,     0,     0,     0,     0,     0,   190,     0,
       0,     0,     0,     0,     0,     0,     0,   191,     0,     0,
       0,   192,   193,     0,     0,     0,     0,     0,     0,   194,
       0,     0,     0,     0,     0,   195,     0,     0,     0,     0,
       0,     0,     0,     0,   196,     0,   197,   198,   199,   200,
     201,   202,   203,   204,   205,     0,   206,     0,     0,   250,
     207,   184,   185,   186,     0,   187,     0,   188,     0,     0,
     189,   289,     0,     0,     0,     0,     0,     0,   190,     0,
       0,     0,     0,     0,     0,     0,     0,   191,     0,     0,
       0,   192,   193,     0,     0,     0,     0,     0,     0,   194,
       0,     0,     0,     0,     0,   195,     0,     0,     0,     0,
       0,     0,     0,     0,   196,     0,   197,   198,   199,   200,
     201,   202,   203,   204,   205,     0,   206,   184,   185,   186,
     207,   187,     0,   188,     0,     0,   189,   296,     0,     0,
       0,     0,     0,     0,   190,     0,     0,     0,     0,     0,
       0,     0,     0,   191,     0,     0,     0,   192,   193,     0,
       0,     0,     0,     0,     0,   194,     0,     0,     0,     0,
       0,   195,     0,     0,     0,     0,     0,     0,     0,     0,
     196,     0,   197,   198,   199,   200,   201,   202,   203,   204,
     205,     0,   206,   184,   185,   186,   207,   187,     0,   188,
       0,     0,   189,     0,     0,     0,     0,     0,     0,     0,
     190,     0,     0,     0,     0,     0,     0,     0,     0,   191,
       0,     0,     0,   192,   193,     0,     0,     0,     0,     0,
       0,   194,     0,     0,     0,     0,     0,   195,   308,     0,
       0,     0,     0,     0,     0,     0,   196,     0,   197,   198,
     199,   200,   201,   202,   203,   204,   205,     0,   206,   184,
     185,   186,   207,   187,     0,   188,     0,     0,   189,     0,
       0,     0,     0,     0,     0,     0,   190,     0,     0,     0,
       0,     0,     0,     0,     0,   191,     0,     0,     0,   192,
     193,     0,     0,     0,     0,     0,     0,   194,     0,     0,
       0,     0,     0,   195,     0,     0,     0,     0,     0,     0,
       0,     0,   196,     0,   197,   198,   199,   200,   201,   202,
     203,   204,   205,     0,   206,   322,     0,     0,   207,  -115,
    -115,  -115,     0,   187,  -115,   188,     0,     0,  -115,     0,
       0,     0,     0,     0,     0,     0,   190,     0,     0,     0,
       0,     0,     0,     0,     0,   191,     0,     0,     0,   192,
     193,     0,     0,     0,     0,     0,     0,  -115,     0,     0,
       0,     0,     0,   195,     0,     0,     0,     0,     0,     0,
       0,     0,  -115,     0,  -115,  -115,  -115,   200,   201,   202,
     203,   204,   205,     0,   206,   184,   185,   186,   207,   187,
       0,   188,     0,     0,   189,     0,     0,     0,     0,     0,
       0,     0,   190,     0,     0,     0,     0,     0,     0,     0,
       0,   191,     0,     0,     0,   192,   193,     0,     0,     0,
       0,     0,     0,   194,     0,     0,     0,     0,     0,   195,
       0,     0,     0,     0,     0,     0,     0,     0,   196,     0,
     197,   198,   199,   200,   201,   202,   203,   204,   205,     0,
     206,   184,   185,   186,   207,   187,     0,   188,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,   190,     0,
       0,     0,     0,     0,     0,     0,     0,   191,     0,     0,
       0,   192,   193,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,   195,     0,     0,     0,     0,
       0,     0,     0,     0,   196,     0,   197,   198,   199,   200,
     201,   202,   203,   204,   205,     0,   206,   184,   185,   186,
     207,   187,     0,   188,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,   190,     0,     0,     0,     0,     0,
       0,     0,     0,   191,   187,     0,   188,   192,   193,     0,
       0,     0,     0,     0,     0,     0,     0,   190,     0,     0,
       0,   195,     0,     0,     0,     0,   191,     0,     0,     0,
     192,   193,   197,   198,   199,   200,   201,   202,   203,   204,
     205,     0,   206,     0,   195,     0,   207,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,   200,   201,
     202,   203,   204,   205,    48,   206,     0,     0,     0,   207,
       0,     0,     0,    49,    50,    51,    52,    53,    54,    55,
      56,    57,    58,    59,    60,    61,    62,    63,    64,    65,
      66,   171,    67,    68,    69,   172,    70,    71,    72,    73,
      74,    75,   173,    76,    77,    78,    79,    80,    81,    82,
      83,    84,   174,    85,    48,    86,    87,    88,    89,    90,
      91,     0,     0,    49,    50,    51,    52,    53,    54,    55,
      56,    57,    58,    59,    60,    61,    62,    63,    64,    65,
      66,     0,    67,    68,    69,     0,    70,    71,    72,    73,
      74,    75,     0,    76,    77,    78,    79,    80,    81,    82,
      83,    84,     0,    85,     0,    86,    87,    88,    89,    90,
      91
};

static const yytype_int16 yycheck[] =
{
      27,    28,    29,   221,    23,    25,    25,   120,   231,   145,
     161,     7,   148,     6,   140,     6,   252,    16,    47,    32,
     291,     8,    55,    59,    49,    59,   218,   163,     3,    16,
      17,    15,    36,   331,    64,    60,    11,    67,    12,     0,
      14,    12,    78,    14,    78,    58,    79,    31,   346,    78,
      76,    25,    78,   324,    25,   353,     1,    44,    57,   330,
      34,     6,     7,    34,    38,    39,   158,    38,    39,    76,
      15,    78,   164,    76,    57,    78,    21,    96,    52,   315,
      27,    52,    75,   110,    75,    81,   212,   114,   224,   240,
     117,   217,   119,   285,    68,    69,    70,    71,   290,    73,
      71,    41,    73,    77,    43,    44,    77,    64,    62,    75,
      67,    78,   139,   140,    77,    22,    23,   230,    25,    76,
     147,    63,    75,    75,     6,   144,    75,    78,    78,    67,
      78,   150,   159,   356,   154,   154,   354,    80,    73,   252,
      60,    75,    75,    74,    80,    82,    78,    60,    76,   367,
      67,    76,    33,    76,    44,    77,   183,   184,   185,   186,
      11,   188,   189,   190,    12,   192,    21,   194,    51,   196,
     197,   198,   199,   200,   201,   202,   203,   204,   205,   206,
     207,   208,   209,   210,    76,   212,    65,    67,    33,    96,
     217,    22,   211,    80,    40,    26,    27,    68,    74,    30,
      78,    59,   315,    11,    11,    74,     3,   294,    33,   377,
     377,    42,   239,   120,   241,   234,   287,    48,    49,    50,
     360,   248,   208,   183,    96,   144,   253,   157,   233,    60,
     306,   356,     8,     9,    10,   262,    12,   144,    14,   323,
     267,    17,   149,   150,    22,   160,    12,   154,    14,    25,
     240,   230,   164,   234,    -1,    -1,    -1,   164,    34,    25,
     287,    -1,    38,    39,    -1,   292,    -1,   294,    34,    -1,
     289,    -1,    38,    39,    -1,    -1,    52,   296,    -1,    -1,
     187,   308,    -1,    -1,    -1,    61,    52,    63,    64,    65,
      66,    67,    68,    69,    70,    71,   323,    73,   325,    -1,
     327,    77,    -1,    -1,   211,    -1,    -1,    73,     8,     9,
      10,    77,    12,    12,    14,    14,   343,    17,    -1,    19,
      20,    -1,    -1,   230,   351,    25,    25,   234,    28,    29,
      -1,    -1,    -1,   360,    34,    34,    -1,    -1,    38,    39,
      -1,    -1,    -1,    -1,    -1,   252,    46,    -1,    -1,    -1,
      -1,    -1,    52,    52,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    61,    -1,    63,    64,    65,    66,    67,    68,    69,
      70,    71,    -1,    73,    73,    -1,    -1,    77,    77,    -1,
      -1,    -1,   289,    -1,    -1,    -1,    -1,    -1,    -1,   296,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,     3,     4,     5,
       6,     7,    -1,    -1,    -1,    -1,    -1,    -1,   315,    15,
      16,    17,    18,    19,    20,    21,    22,    23,    24,    25,
      26,    27,    28,    29,    30,    31,    32,    -1,    34,    35,
      36,    37,    38,    39,    40,    41,    42,    43,    44,    45,
      46,    47,    48,    49,    50,    51,    52,    53,    54,    55,
      -1,    57,    58,    59,    60,    61,    62,    -1,    -1,    -1,
      -1,    67,    68,    -1,    -1,    -1,    -1,    73,    -1,    75,
      76,    -1,    -1,    -1,    -1,    81,     3,     4,     5,     6,
       7,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    15,    16,
      17,    18,    19,    20,    21,    22,    23,    24,    25,    26,
      27,    28,    29,    30,    31,    32,    -1,    34,    35,    36,
      37,    38,    39,    40,    41,    42,    43,    44,    45,    46,
      47,    48,    49,    50,    51,    52,    53,    54,    55,    -1,
      57,    58,    59,    60,    61,    62,    -1,    -1,    -1,    -1,
      67,    68,    -1,    -1,    -1,    -1,    73,    -1,    75,     3,
       4,     5,     6,     7,    81,    -1,    -1,    -1,    -1,    -1,
      -1,    15,    16,    17,    18,    19,    20,    21,    22,    23,
      24,    25,    26,    27,    28,    29,    30,    31,    32,    -1,
      34,    35,    36,    37,    38,    39,    40,    41,    42,    43,
      44,    45,    46,    47,    48,    49,    50,    51,    52,    53,
      54,    55,    -1,    57,    58,    59,    60,    61,    62,    -1,
      -1,    -1,    -1,    67,    68,    -1,    -1,    -1,    -1,    73,
      -1,    75,     3,     4,     5,     6,     7,    81,    -1,    -1,
      -1,    -1,    -1,    -1,    15,    16,    17,    18,    19,    20,
      21,    22,    23,    24,    25,    26,    27,    28,    29,    30,
      31,    32,    -1,    34,    35,    36,    37,    38,    39,    40,
      41,    42,    43,    44,    45,    46,    47,    48,    49,    50,
      51,    52,    53,    54,    55,    -1,    57,    58,    59,    60,
      61,    62,    -1,    -1,    -1,    -1,    67,    68,    -1,    -1,
      -1,    -1,    73,    -1,    75,     3,     4,     5,     6,     7,
      81,    -1,    -1,    -1,    -1,    -1,    -1,    15,    16,    17,
      18,    19,    20,    21,    22,    23,    24,    25,    26,    27,
      28,    29,    30,    31,    32,    -1,    34,    35,    36,    37,
      38,    39,    40,    41,    42,    43,    44,    45,    46,    47,
      48,    49,    50,    51,    52,    53,    54,    55,    -1,    57,
      58,    59,    60,    61,    62,    -1,    -1,    -1,    -1,    67,
      -1,    -1,    -1,    -1,    -1,    73,    -1,    75,     3,     4,
       5,     6,     7,    81,    -1,    -1,    -1,    -1,    -1,    -1,
      15,    16,    17,    18,    19,    20,    21,    22,    23,    24,
      25,    26,    27,    28,    29,    30,    31,    32,    -1,    34,
      35,    36,    37,    38,    39,    40,    41,    42,    43,    44,
      45,    46,    47,    48,    49,    50,    51,    52,    53,    54,
      55,    -1,    57,    58,    59,    60,    61,    62,    12,    -1,
      14,    -1,    67,    -1,    -1,    -1,    -1,    -1,    73,    -1,
      75,    25,    -1,     8,     9,    10,    81,    12,    13,    14,
      34,    -1,    17,    -1,    38,    39,    -1,    -1,    -1,    -1,
      25,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    52,    34,
      -1,    -1,    -1,    38,    39,    -1,    -1,    -1,    -1,    -1,
      -1,    46,    -1,    -1,    -1,    -1,    -1,    52,    -1,    73,
      -1,    -1,    -1,    77,    -1,    -1,    61,    -1,    63,    64,
      65,    66,    67,    68,    69,    70,    71,    -1,    73,     8,
       9,    10,    77,    12,    -1,    14,    -1,    -1,    17,    18,
      -1,    12,    -1,    14,    -1,    -1,    25,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    25,    34,    -1,    -1,    -1,    38,
      39,    -1,    -1,    34,    -1,    -1,    -1,    46,    -1,    -1,
      -1,    -1,    -1,    52,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    52,    61,    -1,    63,    64,    65,    66,    67,    68,
      69,    70,    71,    -1,    73,     8,     9,    10,    77,    12,
      -1,    14,    73,    -1,    17,    -1,    77,    -1,    -1,    -1,
      -1,    -1,    25,    -1,    -1,    22,    -1,    -1,    -1,    26,
      27,    34,    -1,    30,    -1,    38,    39,    -1,    -1,    -1,
      -1,    -1,    -1,    46,    41,    42,    -1,    -1,    45,    52,
      -1,    48,    -1,    50,    -1,    58,    -1,    -1,    61,    56,
      63,    64,    65,    66,    67,    68,    69,    70,    71,    -1,
      73,     8,     9,    10,    77,    12,    -1,    14,    -1,    -1,
      17,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    25,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    34,    -1,    -1,
      -1,    38,    39,    -1,    -1,    -1,    -1,    -1,    -1,    46,
      -1,    -1,    -1,    -1,    -1,    52,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    61,    -1,    63,    64,    65,    66,
      67,    68,    69,    70,    71,    -1,    73,    -1,    -1,    76,
      77,     8,     9,    10,    -1,    12,    -1,    14,    -1,    -1,
      17,    18,    -1,    -1,    -1,    -1,    -1,    -1,    25,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    34,    -1,    -1,
      -1,    38,    39,    -1,    -1,    -1,    -1,    -1,    -1,    46,
      -1,    -1,    -1,    -1,    -1,    52,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    61,    -1,    63,    64,    65,    66,
      67,    68,    69,    70,    71,    -1,    73,     8,     9,    10,
      77,    12,    -1,    14,    -1,    -1,    17,    18,    -1,    -1,
      -1,    -1,    -1,    -1,    25,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    34,    -1,    -1,    -1,    38,    39,    -1,
      -1,    -1,    -1,    -1,    -1,    46,    -1,    -1,    -1,    -1,
      -1,    52,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      61,    -1,    63,    64,    65,    66,    67,    68,    69,    70,
      71,    -1,    73,     8,     9,    10,    77,    12,    -1,    14,
      -1,    -1,    17,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      25,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    34,
      -1,    -1,    -1,    38,    39,    -1,    -1,    -1,    -1,    -1,
      -1,    46,    -1,    -1,    -1,    -1,    -1,    52,    53,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    61,    -1,    63,    64,
      65,    66,    67,    68,    69,    70,    71,    -1,    73,     8,
       9,    10,    77,    12,    -1,    14,    -1,    -1,    17,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    25,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    34,    -1,    -1,    -1,    38,
      39,    -1,    -1,    -1,    -1,    -1,    -1,    46,    -1,    -1,
      -1,    -1,    -1,    52,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    61,    -1,    63,    64,    65,    66,    67,    68,
      69,    70,    71,    -1,    73,    74,    -1,    -1,    77,     8,
       9,    10,    -1,    12,    13,    14,    -1,    -1,    17,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    25,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    34,    -1,    -1,    -1,    38,
      39,    -1,    -1,    -1,    -1,    -1,    -1,    46,    -1,    -1,
      -1,    -1,    -1,    52,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    61,    -1,    63,    64,    65,    66,    67,    68,
      69,    70,    71,    -1,    73,     8,     9,    10,    77,    12,
      -1,    14,    -1,    -1,    17,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    25,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    34,    -1,    -1,    -1,    38,    39,    -1,    -1,    -1,
      -1,    -1,    -1,    46,    -1,    -1,    -1,    -1,    -1,    52,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    61,    -1,
      63,    64,    65,    66,    67,    68,    69,    70,    71,    -1,
      73,     8,     9,    10,    77,    12,    -1,    14,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    25,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    34,    -1,    -1,
      -1,    38,    39,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    52,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    61,    -1,    63,    64,    65,    66,
      67,    68,    69,    70,    71,    -1,    73,     8,     9,    10,
      77,    12,    -1,    14,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    25,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    34,    12,    -1,    14,    38,    39,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    25,    -1,    -1,
      -1,    52,    -1,    -1,    -1,    -1,    34,    -1,    -1,    -1,
      38,    39,    63,    64,    65,    66,    67,    68,    69,    70,
      71,    -1,    73,    -1,    52,    -1,    77,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    66,    67,
      68,    69,    70,    71,     6,    73,    -1,    -1,    -1,    77,
      -1,    -1,    -1,    15,    16,    17,    18,    19,    20,    21,
      22,    23,    24,    25,    26,    27,    28,    29,    30,    31,
      32,    33,    34,    35,    36,    37,    38,    39,    40,    41,
      42,    43,    44,    45,    46,    47,    48,    49,    50,    51,
      52,    53,    54,    55,     6,    57,    58,    59,    60,    61,
      62,    -1,    -1,    15,    16,    17,    18,    19,    20,    21,
      22,    23,    24,    25,    26,    27,    28,    29,    30,    31,
      32,    -1,    34,    35,    36,    -1,    38,    39,    40,    41,
      42,    43,    -1,    45,    46,    47,    48,    49,    50,    51,
      52,    53,    -1,    55,    -1,    57,    58,    59,    60,    61,
      62
};

  /* YYSTOS[STATE-NUM] -- The (internal number of the) accessing
     symbol of state STATE-NUM.  */
static const yytype_uint8 yystos[] =
{
       0,    36,    84,    85,    91,    92,    16,    57,    85,     0,
      55,    79,    89,    93,    94,    57,    85,    85,    89,    15,
      31,    90,    22,    26,    30,    42,    45,    48,    50,    56,
      86,    95,    96,    97,    98,   114,   115,   116,   117,   118,
     121,   124,   125,   126,    85,    89,    89,    85,     6,    15,
      16,    17,    18,    19,    20,    21,    22,    23,    24,    25,
      26,    27,    28,    29,    30,    31,    32,    34,    35,    36,
      38,    39,    40,    41,    42,    43,    45,    46,    47,    48,
      49,    50,    51,    52,    53,    55,    57,    58,    59,    60,
      61,    62,   143,   157,   162,   165,    75,   128,   129,   130,
     131,   132,   133,   159,   162,   129,     3,     4,     5,     7,
      23,    24,    35,    37,    43,    44,    54,    67,    73,    75,
      81,   122,   123,   138,   142,   143,   144,   146,   147,   148,
     151,   152,   156,   159,   162,   119,   120,   138,   138,    49,
      60,   104,   113,    98,    41,    27,    89,    62,    75,    77,
      75,   132,   159,   160,    78,    64,    67,   134,    63,    58,
     138,   153,   154,    75,    75,   138,   138,   138,   140,   141,
     138,    33,    37,    44,    54,   149,   150,   158,   162,   163,
     164,   165,   166,    78,     8,     9,    10,    12,    14,    17,
      25,    34,    38,    39,    46,    52,    61,    63,    64,    65,
      66,    67,    68,    69,    70,    71,    73,    77,    78,    13,
      63,    18,    31,    68,   105,   106,   138,    31,   105,   128,
     140,    87,    88,   138,    31,    68,    76,   140,   162,    76,
      80,   136,   129,    67,    73,   135,   133,   130,   138,    58,
     153,    32,   154,   155,   140,   130,   145,   156,    78,    74,
      76,    82,    78,    80,   123,   138,   138,   138,   162,   138,
     138,   138,    60,   138,    43,    44,   138,    60,   138,   138,
     138,   138,   138,   138,   138,   138,   138,   138,   138,   139,
     138,   120,   138,   138,   159,   105,    47,    78,   107,    18,
     105,   107,    59,   127,    78,   127,    18,   140,    76,    76,
     161,   163,     7,   137,   148,   135,   160,    67,    53,   155,
     138,    33,    76,    76,    76,    77,   138,   158,   138,   138,
      44,   138,    74,    11,   107,    21,   106,    51,   111,   159,
     107,   111,   138,    88,   159,    76,    76,    67,   136,    65,
     138,    33,   158,    80,   138,   139,   111,   108,   109,   138,
     138,    40,   112,   111,   112,    68,    99,   138,    74,   112,
      78,    19,    20,    28,    29,   110,   138,   112,   127,     3,
     100,   101,   102,   103,   137,   109,   127,    11,    74,   101,
     102
};

  /* YYR1[YYN] -- Symbol number of symbol that rule YYN derives.  */
static const yytype_uint8 yyr1[] =
{
       0,    83,    84,    84,    84,    84,    84,    85,    85,    86,
      86,    87,    87,    88,    88,    89,    89,    90,    90,    90,
      91,    92,    92,    93,    93,    94,    94,    95,    95,    96,
      96,    97,    97,    98,    98,    98,    98,    98,    98,    99,
      99,   100,   100,   100,   101,   102,   102,   103,   104,   104,
     105,   105,   106,   106,   106,   107,   107,   108,   108,   109,
     110,   110,   110,   110,   110,   111,   111,   112,   112,   113,
     113,   114,   115,   115,   116,   117,   118,   119,   119,   120,
     120,   121,   122,   122,   123,   124,   125,   125,   126,   127,
     127,   128,   128,   129,   129,   130,   131,   131,   132,   132,
     133,   134,   134,   134,   135,   135,   136,   136,   137,   137,
     137,   138,   138,   138,   138,   138,   138,   138,   138,   138,
     138,   138,   138,   138,   138,   138,   138,   138,   138,   138,
     138,   138,   138,   138,   138,   138,   138,   138,   138,   138,
     139,   139,   140,   140,   141,   141,   142,   142,   143,   143,
     143,   143,   144,   144,   144,   145,   146,   146,   146,   146,
     146,   146,   147,   147,   147,   147,   147,   147,   147,   147,
     148,   149,   149,   150,   150,   151,   152,   152,   153,   153,
     154,   155,   155,   156,   157,   157,   158,   159,   160,   160,
     161,   162,   163,   163,   164,   164,   165,   165,   165,   165,
     165,   165,   165,   165,   165,   165,   165,   165,   165,   165,
     165,   165,   165,   165,   165,   165,   165,   165,   165,   165,
     165,   165,   165,   165,   165,   165,   165,   165,   165,   165,
     165,   165,   165,   165,   165,   165,   165,   165,   165,   166,
     166,   166,   166
};

  /* YYR2[YYN] -- Number of symbols on the right hand side of rule YYN.  */
static const yytype_int8 yyr2[] =
{
       0,     2,     2,     3,     4,     4,     5,     1,     4,     2,
       5,     1,     3,     3,     1,     0,     1,     1,     1,     0,
       2,     0,     4,     2,     3,     0,     2,     1,     1,     0,
       1,     1,     2,     1,     1,     1,     1,     1,     1,     2,
       0,     1,     3,     0,     1,     1,     0,     1,     6,     5,
       1,     3,     3,     1,     1,     0,     3,     1,     3,     2,
       0,     1,     1,     1,     1,     0,     2,     0,     2,     7,
       6,     4,     1,     0,     4,     2,     2,     1,     3,     3,
       3,     2,     1,     3,     1,     3,     1,     0,     2,     0,
       2,     1,     3,     1,     3,     1,     1,     3,     1,     3,
       5,     3,     4,     4,     6,     0,     0,     2,     0,     1,
       1,     3,     3,     3,     2,     3,     3,     3,     3,     3,
       3,     3,     3,     3,     3,     3,     3,     3,     3,     4,
       2,     4,     4,     3,     3,     4,     6,     3,     3,     1,
       0,     1,     1,     3,     0,     1,     1,     1,     3,     4,
       4,     5,     4,     4,     4,     3,     1,     1,     3,     1,
       1,     1,     1,     1,     1,     1,     1,     1,     1,     1,
       3,     0,     1,     3,     5,     3,     5,     4,     1,     2,
       4,     2,     0,     1,     1,     3,     1,     1,     0,     1,
       1,     1,     1,     1,     1,     1,     1,     1,     1,     1,
       1,     1,     1,     1,     1,     1,     1,     1,     1,     1,
       1,     1,     1,     1,     1,     1,     1,     1,     1,     1,
       1,     1,     1,     1,     1,     1,     1,     1,     1,     1,
       1,     1,     1,     1,     1,     1,     1,     1,     1,     1,
       1,     1,     1
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
#line 241 "src/age/cypher_gram.y"
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
#line 2206 "src/age/cypher_gram.c"
    break;

  case 3:
#line 262 "src/age/cypher_gram.y"
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
#line 2224 "src/age/cypher_gram.c"
    break;

  case 4:
#line 276 "src/age/cypher_gram.y"
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
#line 2242 "src/age/cypher_gram.c"
    break;

  case 5:
#line 290 "src/age/cypher_gram.y"
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
#line 2260 "src/age/cypher_gram.c"
    break;

  case 6:
#line 304 "src/age/cypher_gram.y"
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
#line 2279 "src/age/cypher_gram.c"
    break;

  case 7:
#line 322 "src/age/cypher_gram.y"
        {
            (yyval.list) = (yyvsp[0].list);
        }
#line 2287 "src/age/cypher_gram.c"
    break;

  case 8:
#line 326 "src/age/cypher_gram.y"
        {
            (yyval.list) = list_make1(make_set_op(SETOP_UNION, (yyvsp[-1].boolean), (yyvsp[-3].list), (yyvsp[0].list)));
        }
#line 2295 "src/age/cypher_gram.c"
    break;

  case 9:
#line 333 "src/age/cypher_gram.y"
        {
            ereport(ERROR,
                    (errcode(ERRCODE_SYNTAX_ERROR),
                     errmsg("CALL not supported yet"),
                     ag_scanner_errposition((yylsp[-1]), scanner)));
        }
#line 2306 "src/age/cypher_gram.c"
    break;

  case 10:
#line 340 "src/age/cypher_gram.y"
        {
            ereport(ERROR,
                    (errcode(ERRCODE_SYNTAX_ERROR),
                     errmsg("CALL... [YIELD] not supported yet"),
                     ag_scanner_errposition((yylsp[-4]), scanner)));
        }
#line 2317 "src/age/cypher_gram.c"
    break;

  case 11:
#line 350 "src/age/cypher_gram.y"
        {
            (yyval.list) = list_make1((yyvsp[0].node));
        }
#line 2325 "src/age/cypher_gram.c"
    break;

  case 12:
#line 354 "src/age/cypher_gram.y"
        {
            (yyval.list) = lappend((yyvsp[-2].list), (yyvsp[0].node));
        }
#line 2333 "src/age/cypher_gram.c"
    break;

  case 13:
#line 361 "src/age/cypher_gram.y"
        {

        }
#line 2341 "src/age/cypher_gram.c"
    break;

  case 14:
#line 365 "src/age/cypher_gram.y"
        {

        }
#line 2349 "src/age/cypher_gram.c"
    break;

  case 17:
#line 377 "src/age/cypher_gram.y"
    {
        (yyval.boolean) = true;
    }
#line 2357 "src/age/cypher_gram.c"
    break;

  case 18:
#line 381 "src/age/cypher_gram.y"
    {
        (yyval.boolean) = false;
    }
#line 2365 "src/age/cypher_gram.c"
    break;

  case 19:
#line 385 "src/age/cypher_gram.y"
    {
        (yyval.boolean) = false;
    }
#line 2373 "src/age/cypher_gram.c"
    break;

  case 20:
#line 397 "src/age/cypher_gram.y"
        {
            (yyval.list) = list_concat((yyvsp[-1].list), (yyvsp[0].list));
        }
#line 2381 "src/age/cypher_gram.c"
    break;

  case 21:
#line 404 "src/age/cypher_gram.y"
        {
            (yyval.list) = NIL;
        }
#line 2389 "src/age/cypher_gram.c"
    break;

  case 22:
#line 408 "src/age/cypher_gram.y"
        {
            (yyval.list) = lappend(list_concat(list_concat((yyvsp[-3].list), (yyvsp[-2].list)), (yyvsp[-1].list)), (yyvsp[0].node));
        }
#line 2397 "src/age/cypher_gram.c"
    break;

  case 23:
#line 415 "src/age/cypher_gram.y"
        {
            (yyval.list) = list_concat((yyvsp[-1].list), (yyvsp[0].list));
        }
#line 2405 "src/age/cypher_gram.c"
    break;

  case 24:
#line 419 "src/age/cypher_gram.y"
        {
            (yyval.list) = lappend(list_concat((yyvsp[-2].list), (yyvsp[-1].list)), (yyvsp[0].node));
        }
#line 2413 "src/age/cypher_gram.c"
    break;

  case 25:
#line 426 "src/age/cypher_gram.y"
        {
            (yyval.list) = NIL;
        }
#line 2421 "src/age/cypher_gram.c"
    break;

  case 26:
#line 430 "src/age/cypher_gram.y"
        {
            (yyval.list) = lappend((yyvsp[-1].list), (yyvsp[0].node));
        }
#line 2429 "src/age/cypher_gram.c"
    break;

  case 29:
#line 442 "src/age/cypher_gram.y"
        {
            (yyval.list) = NIL;
        }
#line 2437 "src/age/cypher_gram.c"
    break;

  case 31:
#line 450 "src/age/cypher_gram.y"
        {
            (yyval.list) = list_make1((yyvsp[0].node));
        }
#line 2445 "src/age/cypher_gram.c"
    break;

  case 32:
#line 454 "src/age/cypher_gram.y"
        {
            (yyval.list) = lappend((yyvsp[-1].list), (yyvsp[0].node));
        }
#line 2453 "src/age/cypher_gram.c"
    break;

  case 39:
#line 470 "src/age/cypher_gram.y"
        {
            A_Indices *n = (A_Indices *) (yyvsp[0].node);

            if (n->lidx == NULL)
                n->lidx = make_int_const(1, (yylsp[0]));

            if (n->uidx != NULL)
            {
                A_Const    *lidx = (A_Const *) n->lidx;
                A_Const    *uidx = (A_Const *) n->uidx;

                if (lidx->val_age.val.ival > uidx->val_age.val.ival)
                    ereport(ERROR, (errcode(ERRCODE_SYNTAX_ERROR),
                                    errmsg("invalid range"),
                                    ag_scanner_errposition((yylsp[0]), scanner)));
            }
            (yyval.node) = (Node *) n;
        }
#line 2476 "src/age/cypher_gram.c"
    break;

  case 40:
#line 489 "src/age/cypher_gram.y"
        {
            (yyval.node) = NULL;
        }
#line 2484 "src/age/cypher_gram.c"
    break;

  case 41:
#line 496 "src/age/cypher_gram.y"
        {
            A_Indices  *n;

            n = makeNode(A_Indices);
            n->lidx = copyObject((yyvsp[0].node));
            n->uidx = (yyvsp[0].node);
            (yyval.node) = (Node *) n;
        }
#line 2497 "src/age/cypher_gram.c"
    break;

  case 42:
#line 505 "src/age/cypher_gram.y"
        {
            A_Indices  *n;

            n = makeNode(A_Indices);
            n->lidx = (yyvsp[-2].node);
            n->uidx = (yyvsp[0].node);
            (yyval.node) = (Node *) n;
        }
#line 2510 "src/age/cypher_gram.c"
    break;

  case 43:
#line 514 "src/age/cypher_gram.y"
        {
            (yyval.node) = (Node *) makeNode(A_Indices);
        }
#line 2518 "src/age/cypher_gram.c"
    break;

  case 44:
#line 521 "src/age/cypher_gram.y"
        {
            (yyval.node) = make_int_const((yyvsp[0].integer), (yylsp[0]));
        }
#line 2526 "src/age/cypher_gram.c"
    break;

  case 46:
#line 528 "src/age/cypher_gram.y"
                                                        { (yyval.node) = NULL; }
#line 2532 "src/age/cypher_gram.c"
    break;

  case 48:
#line 539 "src/age/cypher_gram.y"
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
#line 2549 "src/age/cypher_gram.c"
    break;

  case 49:
#line 552 "src/age/cypher_gram.y"
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
#line 2566 "src/age/cypher_gram.c"
    break;

  case 50:
#line 569 "src/age/cypher_gram.y"
        {
            (yyval.list) = list_make1((yyvsp[0].node));
        }
#line 2574 "src/age/cypher_gram.c"
    break;

  case 51:
#line 573 "src/age/cypher_gram.y"
        {
            (yyval.list) = lappend((yyvsp[-2].list), (yyvsp[0].node));
        }
#line 2582 "src/age/cypher_gram.c"
    break;

  case 52:
#line 580 "src/age/cypher_gram.y"
        {
            ResTarget *n;

            n = makeNode(ResTarget);
            n->name = (yyvsp[0].string);
            n->indirection = NIL;
            n->val = (yyvsp[-2].node);
            n->location = (yylsp[-2]);

            (yyval.node) = (Node *)n;
        }
#line 2598 "src/age/cypher_gram.c"
    break;

  case 53:
#line 592 "src/age/cypher_gram.y"
        {
            ResTarget *n;

            n = makeNode(ResTarget);
            n->name = NULL;
            n->indirection = NIL;
            n->val = (yyvsp[0].node);
            n->location = (yylsp[0]);

            (yyval.node) = (Node *)n;
        }
#line 2614 "src/age/cypher_gram.c"
    break;

  case 54:
#line 604 "src/age/cypher_gram.y"
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
#line 2635 "src/age/cypher_gram.c"
    break;

  case 55:
#line 624 "src/age/cypher_gram.y"
        {
            (yyval.list) = NIL;
        }
#line 2643 "src/age/cypher_gram.c"
    break;

  case 56:
#line 628 "src/age/cypher_gram.y"
        {
            (yyval.list) = (yyvsp[0].list);
        }
#line 2651 "src/age/cypher_gram.c"
    break;

  case 57:
#line 635 "src/age/cypher_gram.y"
        {
            (yyval.list) = list_make1((yyvsp[0].node));
        }
#line 2659 "src/age/cypher_gram.c"
    break;

  case 58:
#line 639 "src/age/cypher_gram.y"
        {
            (yyval.list) = lappend((yyvsp[-2].list), (yyvsp[0].node));
        }
#line 2667 "src/age/cypher_gram.c"
    break;

  case 59:
#line 646 "src/age/cypher_gram.y"
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
#line 2684 "src/age/cypher_gram.c"
    break;

  case 60:
#line 662 "src/age/cypher_gram.y"
        {
            (yyval.integer) = SORTBY_DEFAULT; // is the same with SORTBY_ASC
        }
#line 2692 "src/age/cypher_gram.c"
    break;

  case 61:
#line 666 "src/age/cypher_gram.y"
        {
            (yyval.integer) = SORTBY_ASC;
        }
#line 2700 "src/age/cypher_gram.c"
    break;

  case 62:
#line 670 "src/age/cypher_gram.y"
        {
            (yyval.integer) = SORTBY_ASC;
        }
#line 2708 "src/age/cypher_gram.c"
    break;

  case 63:
#line 674 "src/age/cypher_gram.y"
        {
            (yyval.integer) = SORTBY_DESC;
        }
#line 2716 "src/age/cypher_gram.c"
    break;

  case 64:
#line 678 "src/age/cypher_gram.y"
        {
            (yyval.integer) = SORTBY_DESC;
        }
#line 2724 "src/age/cypher_gram.c"
    break;

  case 65:
#line 685 "src/age/cypher_gram.y"
        {
            (yyval.node) = NULL;
        }
#line 2732 "src/age/cypher_gram.c"
    break;

  case 66:
#line 689 "src/age/cypher_gram.y"
        {
            (yyval.node) = (yyvsp[0].node);
        }
#line 2740 "src/age/cypher_gram.c"
    break;

  case 67:
#line 696 "src/age/cypher_gram.y"
        {
            (yyval.node) = NULL;
        }
#line 2748 "src/age/cypher_gram.c"
    break;

  case 68:
#line 700 "src/age/cypher_gram.y"
        {
            (yyval.node) = (yyvsp[0].node);
        }
#line 2756 "src/age/cypher_gram.c"
    break;

  case 69:
#line 707 "src/age/cypher_gram.y"
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
#line 2791 "src/age/cypher_gram.c"
    break;

  case 70:
#line 739 "src/age/cypher_gram.y"
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
#line 2826 "src/age/cypher_gram.c"
    break;

  case 71:
#line 777 "src/age/cypher_gram.y"
        {
            cypher_match *n;

            n = make_ag_node(cypher_match);
            n->optional = (yyvsp[-3].boolean);
            n->pattern = (yyvsp[-1].list);
            n->where = (yyvsp[0].node);

            (yyval.node) = (Node *)n;
        }
#line 2841 "src/age/cypher_gram.c"
    break;

  case 72:
#line 791 "src/age/cypher_gram.y"
        {
            (yyval.boolean) = true;
        }
#line 2849 "src/age/cypher_gram.c"
    break;

  case 73:
#line 795 "src/age/cypher_gram.y"
        {
            (yyval.boolean) = false;
        }
#line 2857 "src/age/cypher_gram.c"
    break;

  case 74:
#line 803 "src/age/cypher_gram.y"
        {
            ResTarget  *res;
            cypher_unwind *n;

            res = makeNode(ResTarget);
            res->name = (yyvsp[0].string);
            res->val = (Node *) (yyvsp[-2].node);
            res->location = (yylsp[-2]);

            n = make_ag_node(cypher_unwind);
            n->target = res;
            (yyval.node) = (Node *) n;
        }
#line 2875 "src/age/cypher_gram.c"
    break;

  case 75:
#line 823 "src/age/cypher_gram.y"
        {
            cypher_create *n;

            n = make_ag_node(cypher_create);
            n->pattern = (yyvsp[0].list);

            (yyval.node) = (Node *)n;
        }
#line 2888 "src/age/cypher_gram.c"
    break;

  case 76:
#line 839 "src/age/cypher_gram.y"
        {
            cypher_set *n;

            n = make_ag_node(cypher_set);
            n->items = (yyvsp[0].list);
            n->is_remove = false;
            n->location = (yylsp[-1]);

            (yyval.node) = (Node *)n;
        }
#line 2903 "src/age/cypher_gram.c"
    break;

  case 77:
#line 853 "src/age/cypher_gram.y"
        {
            (yyval.list) = list_make1((yyvsp[0].node));
        }
#line 2911 "src/age/cypher_gram.c"
    break;

  case 78:
#line 857 "src/age/cypher_gram.y"
        {
            (yyval.list) = lappend((yyvsp[-2].list), (yyvsp[0].node));
        }
#line 2919 "src/age/cypher_gram.c"
    break;

  case 79:
#line 864 "src/age/cypher_gram.y"
        {
            cypher_set_item *n;

            n = make_ag_node(cypher_set_item);
            n->prop = (yyvsp[-2].node);
            n->expr = (yyvsp[0].node);
            n->is_add = false;
            n->location = (yylsp[-2]);

            (yyval.node) = (Node *)n;
        }
#line 2935 "src/age/cypher_gram.c"
    break;

  case 80:
#line 876 "src/age/cypher_gram.y"
        {
            cypher_set_item *n;

            n = make_ag_node(cypher_set_item);
            n->prop = (yyvsp[-2].node);
            n->expr = (yyvsp[0].node);
            n->is_add = true;
            n->location = (yylsp[-2]);

            (yyval.node) = (Node *)n;
        }
#line 2951 "src/age/cypher_gram.c"
    break;

  case 81:
#line 891 "src/age/cypher_gram.y"
        {
            cypher_set *n;

            n = make_ag_node(cypher_set);
            n->items = (yyvsp[0].list);
            n->is_remove = true;
             n->location = (yylsp[-1]);

            (yyval.node) = (Node *)n;
        }
#line 2966 "src/age/cypher_gram.c"
    break;

  case 82:
#line 905 "src/age/cypher_gram.y"
        {
            (yyval.list) = list_make1((yyvsp[0].node));
        }
#line 2974 "src/age/cypher_gram.c"
    break;

  case 83:
#line 909 "src/age/cypher_gram.y"
        {
            (yyval.list) = lappend((yyvsp[-2].list), (yyvsp[0].node));
        }
#line 2982 "src/age/cypher_gram.c"
    break;

  case 84:
#line 916 "src/age/cypher_gram.y"
        {
            cypher_set_item *n;

            n = make_ag_node(cypher_set_item);
            n->prop = (yyvsp[0].node);
            n->expr = make_null_const(-1);
            n->is_add = false;

            (yyval.node) = (Node *)n;
        }
#line 2997 "src/age/cypher_gram.c"
    break;

  case 85:
#line 934 "src/age/cypher_gram.y"
        {
            cypher_delete *n;

            n = make_ag_node(cypher_delete);
            n->detach = (yyvsp[-2].boolean);
            n->exprs = (yyvsp[0].list);
            n->location = (yylsp[-2]);

            (yyval.node) = (Node *)n;
        }
#line 3012 "src/age/cypher_gram.c"
    break;

  case 86:
#line 948 "src/age/cypher_gram.y"
        {
            (yyval.boolean) = true;
        }
#line 3020 "src/age/cypher_gram.c"
    break;

  case 87:
#line 952 "src/age/cypher_gram.y"
        {
            (yyval.boolean) = false;
        }
#line 3028 "src/age/cypher_gram.c"
    break;

  case 88:
#line 962 "src/age/cypher_gram.y"
        {
            cypher_merge *n;

            n = make_ag_node(cypher_merge);
            n->path = (yyvsp[0].node);

            (yyval.node) = (Node *)n;
        }
#line 3041 "src/age/cypher_gram.c"
    break;

  case 89:
#line 978 "src/age/cypher_gram.y"
        {
            (yyval.node) = NULL;
        }
#line 3049 "src/age/cypher_gram.c"
    break;

  case 90:
#line 982 "src/age/cypher_gram.y"
        {
            (yyval.node) = (yyvsp[0].node);
        }
#line 3057 "src/age/cypher_gram.c"
    break;

  case 91:
#line 994 "src/age/cypher_gram.y"
        {
            (yyval.list) = list_make1((yyvsp[0].node));
        }
#line 3065 "src/age/cypher_gram.c"
    break;

  case 92:
#line 998 "src/age/cypher_gram.y"
        {
            (yyval.list) = lappend((yyvsp[-2].list), (yyvsp[0].node));
        }
#line 3073 "src/age/cypher_gram.c"
    break;

  case 94:
#line 1007 "src/age/cypher_gram.y"
        {
            cypher_path *p;

            p = (cypher_path *)(yyvsp[0].node);
            p->var_name = (yyvsp[-2].string);

            (yyval.node) = (Node *)p;
        }
#line 3086 "src/age/cypher_gram.c"
    break;

  case 95:
#line 1020 "src/age/cypher_gram.y"
        {
            cypher_path *n;

            n = make_ag_node(cypher_path);
            n->path = (yyvsp[0].list);
            n->var_name = NULL;
            n->location = (yylsp[0]);

            (yyval.node) = (Node *)n;
        }
#line 3101 "src/age/cypher_gram.c"
    break;

  case 97:
#line 1035 "src/age/cypher_gram.y"
        {
            (yyval.list) = (yyvsp[-1].list);
        }
#line 3109 "src/age/cypher_gram.c"
    break;

  case 98:
#line 1042 "src/age/cypher_gram.y"
        {
            (yyval.list) = list_make1((yyvsp[0].node));
        }
#line 3117 "src/age/cypher_gram.c"
    break;

  case 99:
#line 1046 "src/age/cypher_gram.y"
        {
            cypher_relationship *cr = NULL;

            /* get the relationship */
            cr = (cypher_relationship *)(yyvsp[-1].node);

            /* if this is a VLE relation node */
            if (cr->varlen != NULL)
            {
                /* build the VLE relation */
                cr = build_VLE_relation((yyvsp[-2].list), cr, (yyvsp[0].node), (yylsp[-2]), (yylsp[-1]));

                /* return the VLE relation in the path */
                (yyval.list) = lappend(lappend((yyvsp[-2].list), cr), (yyvsp[0].node));
            }
            /* otherwise, it is a regular relationship node */
            else
            {
                (yyval.list) = lappend(lappend((yyvsp[-2].list), (yyvsp[-1].node)), (yyvsp[0].node));
            }
        }
#line 3143 "src/age/cypher_gram.c"
    break;

  case 100:
#line 1071 "src/age/cypher_gram.y"
        {
            cypher_node *n;

            n = make_ag_node(cypher_node);
            n->name = (yyvsp[-3].string);
            n->label = (yyvsp[-2].string);
            n->props = (yyvsp[-1].node);
            n->location = (yylsp[-4]);

            (yyval.node) = (Node *)n;
        }
#line 3159 "src/age/cypher_gram.c"
    break;

  case 101:
#line 1086 "src/age/cypher_gram.y"
        {
            cypher_relationship *n = (cypher_relationship *)(yyvsp[-1].node);

            n->dir = CYPHER_REL_DIR_NONE;
            n->location = (yylsp[-1]);

            (yyval.node) = (yyvsp[-1].node);
        }
#line 3172 "src/age/cypher_gram.c"
    break;

  case 102:
#line 1095 "src/age/cypher_gram.y"
        {
            cypher_relationship *n = (cypher_relationship *)(yyvsp[-2].node);

            n->dir = CYPHER_REL_DIR_RIGHT;
            n->location = (yylsp[-2]);

            (yyval.node) = (yyvsp[-2].node);
        }
#line 3185 "src/age/cypher_gram.c"
    break;

  case 103:
#line 1104 "src/age/cypher_gram.y"
        {
            cypher_relationship *n = (cypher_relationship *)(yyvsp[-1].node);

            n->dir = CYPHER_REL_DIR_LEFT;
            n->location = (yylsp[-1]);

            (yyval.node) = (yyvsp[-1].node);
        }
#line 3198 "src/age/cypher_gram.c"
    break;

  case 104:
#line 1116 "src/age/cypher_gram.y"
        {
            cypher_relationship *n;

            n = make_ag_node(cypher_relationship);
            n->name = (yyvsp[-4].string);
            n->label = (yyvsp[-3].string);
            n->varlen = (yyvsp[-2].node);
            n->props = (yyvsp[-1].node);

            (yyval.node) = (Node *)n;
        }
#line 3214 "src/age/cypher_gram.c"
    break;

  case 105:
#line 1129 "src/age/cypher_gram.y"
        {
            cypher_relationship *n;

            n = make_ag_node(cypher_relationship);
            n->name = NULL;
            n->label = NULL;
            n->varlen = NULL;
            n->props = NULL;

            (yyval.node) = (Node *)n;
        }
#line 3230 "src/age/cypher_gram.c"
    break;

  case 106:
#line 1144 "src/age/cypher_gram.y"
        {
            (yyval.string) = NULL;
        }
#line 3238 "src/age/cypher_gram.c"
    break;

  case 107:
#line 1148 "src/age/cypher_gram.y"
        {
            (yyval.string) = (yyvsp[0].string);
        }
#line 3246 "src/age/cypher_gram.c"
    break;

  case 108:
#line 1155 "src/age/cypher_gram.y"
        {
            (yyval.node) = NULL;
        }
#line 3254 "src/age/cypher_gram.c"
    break;

  case 110:
#line 1160 "src/age/cypher_gram.y"
        {
            cypher_param *n;

            n = make_ag_node(cypher_param);
            n->name = (yyvsp[0].string);
            n->location = (yylsp[0]);

            (yyval.node) = (Node *)n;
        }
#line 3268 "src/age/cypher_gram.c"
    break;

  case 111:
#line 1178 "src/age/cypher_gram.y"
        {
            (yyval.node) = make_or_expr((yyvsp[-2].node), (yyvsp[0].node), (yylsp[-1]));
        }
#line 3276 "src/age/cypher_gram.c"
    break;

  case 112:
#line 1182 "src/age/cypher_gram.y"
        {
            (yyval.node) = make_and_expr((yyvsp[-2].node), (yyvsp[0].node), (yylsp[-1]));
        }
#line 3284 "src/age/cypher_gram.c"
    break;

  case 113:
#line 1186 "src/age/cypher_gram.y"
        {
            (yyval.node) = make_xor_expr((yyvsp[-2].node), (yyvsp[0].node), (yylsp[-1]));
        }
#line 3292 "src/age/cypher_gram.c"
    break;

  case 114:
#line 1190 "src/age/cypher_gram.y"
        {
            (yyval.node) = make_not_expr((yyvsp[0].node), (yylsp[-1]));
        }
#line 3300 "src/age/cypher_gram.c"
    break;

  case 115:
#line 1194 "src/age/cypher_gram.y"
        {
            (yyval.node) = build_comparison_expression((yyvsp[-2].node), (yyvsp[0].node), "=", (yylsp[-1]));
        }
#line 3308 "src/age/cypher_gram.c"
    break;

  case 116:
#line 1198 "src/age/cypher_gram.y"
        {
            (yyval.node) = build_comparison_expression((yyvsp[-2].node), (yyvsp[0].node), "<>", (yylsp[-1]));
        }
#line 3316 "src/age/cypher_gram.c"
    break;

  case 117:
#line 1202 "src/age/cypher_gram.y"
        {
            (yyval.node) = build_comparison_expression((yyvsp[-2].node), (yyvsp[0].node), "<", (yylsp[-1]));
        }
#line 3324 "src/age/cypher_gram.c"
    break;

  case 118:
#line 1206 "src/age/cypher_gram.y"
        {
            (yyval.node) = build_comparison_expression((yyvsp[-2].node), (yyvsp[0].node), "<=", (yylsp[-1]));
        }
#line 3332 "src/age/cypher_gram.c"
    break;

  case 119:
#line 1210 "src/age/cypher_gram.y"
        {
            (yyval.node) = build_comparison_expression((yyvsp[-2].node), (yyvsp[0].node), ">", (yylsp[-1]));
        }
#line 3340 "src/age/cypher_gram.c"
    break;

  case 120:
#line 1214 "src/age/cypher_gram.y"
        {
            (yyval.node) = build_comparison_expression((yyvsp[-2].node), (yyvsp[0].node), ">=", (yylsp[-1]));
        }
#line 3348 "src/age/cypher_gram.c"
    break;

  case 121:
#line 1218 "src/age/cypher_gram.y"
        {
            (yyval.node) = (Node *)makeSimpleA_Expr(AEXPR_OP, "+", (yyvsp[-2].node), (yyvsp[0].node), (yylsp[-1]));
        }
#line 3356 "src/age/cypher_gram.c"
    break;

  case 122:
#line 1222 "src/age/cypher_gram.y"
        {
            (yyval.node) = (Node *)makeSimpleA_Expr(AEXPR_OP, "-", (yyvsp[-2].node), (yyvsp[0].node), (yylsp[-1]));
        }
#line 3364 "src/age/cypher_gram.c"
    break;

  case 123:
#line 1226 "src/age/cypher_gram.y"
        {
            (yyval.node) = (Node *)makeSimpleA_Expr(AEXPR_OP, "*", (yyvsp[-2].node), (yyvsp[0].node), (yylsp[-1]));
        }
#line 3372 "src/age/cypher_gram.c"
    break;

  case 124:
#line 1230 "src/age/cypher_gram.y"
        {
            (yyval.node) = (Node *)makeSimpleA_Expr(AEXPR_OP, "/", (yyvsp[-2].node), (yyvsp[0].node), (yylsp[-1]));
        }
#line 3380 "src/age/cypher_gram.c"
    break;

  case 125:
#line 1234 "src/age/cypher_gram.y"
        {
            (yyval.node) = (Node *)makeSimpleA_Expr(AEXPR_OP, "%", (yyvsp[-2].node), (yyvsp[0].node), (yylsp[-1]));
        }
#line 3388 "src/age/cypher_gram.c"
    break;

  case 126:
#line 1238 "src/age/cypher_gram.y"
        {
            (yyval.node) = (Node *)makeSimpleA_Expr(AEXPR_OP, "^", (yyvsp[-2].node), (yyvsp[0].node), (yylsp[-1]));
        }
#line 3396 "src/age/cypher_gram.c"
    break;

  case 127:
#line 1242 "src/age/cypher_gram.y"
        {
            (yyval.node) = (Node *)makeSimpleA_Expr(AEXPR_IN, "=", (yyvsp[-2].node), (yyvsp[0].node), (yylsp[-1]));
        }
#line 3404 "src/age/cypher_gram.c"
    break;

  case 128:
#line 1246 "src/age/cypher_gram.y"
        {
            NullTest *n;

            n = makeNode(NullTest);
            n->arg = (Expr *)(yyvsp[-2].node);
            n->nulltesttype = IS_NULL;
            n->location = (yylsp[-1]);

            (yyval.node) = (Node *)n;
        }
#line 3419 "src/age/cypher_gram.c"
    break;

  case 129:
#line 1257 "src/age/cypher_gram.y"
        {
            NullTest *n;

            n = makeNode(NullTest);
            n->arg = (Expr *)(yyvsp[-3].node);
            n->nulltesttype = IS_NOT_NULL;
            n->location = (yylsp[-2]);

            (yyval.node) = (Node *)n;
        }
#line 3434 "src/age/cypher_gram.c"
    break;

  case 130:
#line 1268 "src/age/cypher_gram.y"
        {
            (yyval.node) = do_negate((yyvsp[0].node), (yylsp[-1]));
        }
#line 3442 "src/age/cypher_gram.c"
    break;

  case 131:
#line 1272 "src/age/cypher_gram.y"
        {
            cypher_string_match *n;

            n = make_ag_node(cypher_string_match);
            n->operation = CSMO_STARTS_WITH;
            n->lhs = (yyvsp[-3].node);
            n->rhs = (yyvsp[0].node);
            n->location = (yylsp[-2]);

            (yyval.node) = (Node *)n;
        }
#line 3458 "src/age/cypher_gram.c"
    break;

  case 132:
#line 1284 "src/age/cypher_gram.y"
        {
            cypher_string_match *n;

            n = make_ag_node(cypher_string_match);
            n->operation = CSMO_ENDS_WITH;
            n->lhs = (yyvsp[-3].node);
            n->rhs = (yyvsp[0].node);
            n->location = (yylsp[-2]);

            (yyval.node) = (Node *)n;
        }
#line 3474 "src/age/cypher_gram.c"
    break;

  case 133:
#line 1296 "src/age/cypher_gram.y"
        {
            cypher_string_match *n;

            n = make_ag_node(cypher_string_match);
            n->operation = CSMO_CONTAINS;
            n->lhs = (yyvsp[-2].node);
            n->rhs = (yyvsp[0].node);
            n->location = (yylsp[-1]);

            (yyval.node) = (Node *)n;
        }
#line 3490 "src/age/cypher_gram.c"
    break;

  case 134:
#line 1308 "src/age/cypher_gram.y"
        {
            (yyval.node) = make_function_expr(list_make1(makeString("eq_tilde")),
                                    list_make2((yyvsp[-2].node), (yyvsp[0].node)), (yylsp[-1]));
        }
#line 3499 "src/age/cypher_gram.c"
    break;

  case 135:
#line 1313 "src/age/cypher_gram.y"
        {
            A_Indices *i;

            i = makeNode(A_Indices);
            i->is_slice = false;
            i->lidx = NULL;
            i->uidx = (yyvsp[-1].node);

            (yyval.node) = append_indirection((yyvsp[-3].node), (Node *)i);
        }
#line 3514 "src/age/cypher_gram.c"
    break;

  case 136:
#line 1324 "src/age/cypher_gram.y"
        {
            A_Indices *i;

            i = makeNode(A_Indices);
            i->is_slice = true;
            i->lidx = (yyvsp[-3].node);
            i->uidx = (yyvsp[-1].node);

            (yyval.node) = append_indirection((yyvsp[-5].node), (Node *)i);
        }
#line 3529 "src/age/cypher_gram.c"
    break;

  case 137:
#line 1342 "src/age/cypher_gram.y"
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
            /* allow a function to be used as a parent of an indirection */
            else if (IsA((yyvsp[-2].node), FuncCall) && IsA((yyvsp[0].node), ColumnRef))
            {
                ColumnRef *cr = (ColumnRef*)(yyvsp[0].node);
                List *fields = cr->fields;
                Value *string = linitial(fields);

                (yyval.node) = append_indirection((yyvsp[-2].node), (Node*)string);
            }
            else if (IsA((yyvsp[-2].node), FuncCall) && IsA((yyvsp[0].node), A_Indirection))
            {
                ereport(ERROR,
                            (errcode(ERRCODE_SYNTAX_ERROR),
                             errmsg("not supported A_Indirection indirection"),
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
#line 3611 "src/age/cypher_gram.c"
    break;

  case 138:
#line 1420 "src/age/cypher_gram.y"
        {
            (yyval.node) = make_typecast_expr((yyvsp[-2].node), (yyvsp[0].string), (yylsp[-1]));
        }
#line 3619 "src/age/cypher_gram.c"
    break;

  case 140:
#line 1428 "src/age/cypher_gram.y"
        {
            (yyval.node) = NULL;
        }
#line 3627 "src/age/cypher_gram.c"
    break;

  case 142:
#line 1436 "src/age/cypher_gram.y"
        {
            (yyval.list) = list_make1((yyvsp[0].node));
        }
#line 3635 "src/age/cypher_gram.c"
    break;

  case 143:
#line 1440 "src/age/cypher_gram.y"
        {
            (yyval.list) = lappend((yyvsp[-2].list), (yyvsp[0].node));
        }
#line 3643 "src/age/cypher_gram.c"
    break;

  case 144:
#line 1447 "src/age/cypher_gram.y"
        {
            (yyval.list) = NIL;
        }
#line 3651 "src/age/cypher_gram.c"
    break;

  case 148:
#line 1460 "src/age/cypher_gram.y"
        {
            (yyval.node) = make_function_expr((yyvsp[-2].list), NIL, (yylsp[-2]));
        }
#line 3659 "src/age/cypher_gram.c"
    break;

  case 149:
#line 1464 "src/age/cypher_gram.y"
        {
            (yyval.node) = make_function_expr((yyvsp[-3].list), (yyvsp[-1].list), (yylsp[-2]));
        }
#line 3667 "src/age/cypher_gram.c"
    break;

  case 150:
#line 1469 "src/age/cypher_gram.y"
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
#line 3687 "src/age/cypher_gram.c"
    break;

  case 151:
#line 1485 "src/age/cypher_gram.y"
        {
            FuncCall *n = (FuncCall *)make_function_expr((yyvsp[-4].list), (yyvsp[-1].list), (yylsp[-4]));
            n->agg_order = NIL;
            n->agg_distinct = true;
            (yyval.node) = (Node *)n;
        }
#line 3698 "src/age/cypher_gram.c"
    break;

  case 152:
#line 1495 "src/age/cypher_gram.y"
        {
            CoalesceExpr *c;

            c = makeNode(CoalesceExpr);
            c->args = (yyvsp[-1].list);
            c->location = (yylsp[-3]);
            (yyval.node) = (Node *) c;
        }
#line 3711 "src/age/cypher_gram.c"
    break;

  case 153:
#line 1504 "src/age/cypher_gram.y"
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
#line 3733 "src/age/cypher_gram.c"
    break;

  case 154:
#line 1522 "src/age/cypher_gram.y"
        {
            (yyval.node) = make_function_expr(list_make1(makeString("exists")),
                                    list_make1((yyvsp[-1].node)), (yylsp[-2]));
        }
#line 3742 "src/age/cypher_gram.c"
    break;

  case 155:
#line 1530 "src/age/cypher_gram.y"
        {
            (yyval.node) = append_indirection((yyvsp[-2].node), (Node *)makeString((yyvsp[0].string)));
        }
#line 3750 "src/age/cypher_gram.c"
    break;

  case 157:
#line 1538 "src/age/cypher_gram.y"
        {
            cypher_param *n;

            n = make_ag_node(cypher_param);
            n->name = (yyvsp[0].string);
            n->location = (yylsp[0]);

            (yyval.node) = (Node *)n;
        }
#line 3764 "src/age/cypher_gram.c"
    break;

  case 158:
#line 1548 "src/age/cypher_gram.y"
        {
            (yyval.node) = (yyvsp[-1].node);
        }
#line 3772 "src/age/cypher_gram.c"
    break;

  case 162:
#line 1558 "src/age/cypher_gram.y"
        {
            (yyval.node) = make_int_const((yyvsp[0].integer), (yylsp[0]));
        }
#line 3780 "src/age/cypher_gram.c"
    break;

  case 163:
#line 1562 "src/age/cypher_gram.y"
        {
            (yyval.node) = make_float_const((yyvsp[0].string), (yylsp[0]));
        }
#line 3788 "src/age/cypher_gram.c"
    break;

  case 164:
#line 1566 "src/age/cypher_gram.y"
        {
            (yyval.node) = make_string_const((yyvsp[0].string), (yylsp[0]));
        }
#line 3796 "src/age/cypher_gram.c"
    break;

  case 165:
#line 1570 "src/age/cypher_gram.y"
        {
            (yyval.node) = make_bool_const(true, (yylsp[0]));
        }
#line 3804 "src/age/cypher_gram.c"
    break;

  case 166:
#line 1574 "src/age/cypher_gram.y"
        {
            (yyval.node) = make_bool_const(false, (yylsp[0]));
        }
#line 3812 "src/age/cypher_gram.c"
    break;

  case 167:
#line 1578 "src/age/cypher_gram.y"
        {
            (yyval.node) = make_null_const((yylsp[0]));
        }
#line 3820 "src/age/cypher_gram.c"
    break;

  case 170:
#line 1587 "src/age/cypher_gram.y"
        {
            cypher_map *n;

            n = make_ag_node(cypher_map);
            n->keyvals = (yyvsp[-1].list);

            (yyval.node) = (Node *)n;
        }
#line 3833 "src/age/cypher_gram.c"
    break;

  case 171:
#line 1599 "src/age/cypher_gram.y"
        {
            (yyval.list) = NIL;
        }
#line 3841 "src/age/cypher_gram.c"
    break;

  case 173:
#line 1607 "src/age/cypher_gram.y"
        {
            (yyval.list) = list_make2(makeString((yyvsp[-2].string)), (yyvsp[0].node));
        }
#line 3849 "src/age/cypher_gram.c"
    break;

  case 174:
#line 1611 "src/age/cypher_gram.y"
        {
            (yyval.list) = lappend(lappend((yyvsp[-4].list), makeString((yyvsp[-2].string))), (yyvsp[0].node));
        }
#line 3857 "src/age/cypher_gram.c"
    break;

  case 175:
#line 1618 "src/age/cypher_gram.y"
        {
            cypher_list *n;

            n = make_ag_node(cypher_list);
            n->elems = (yyvsp[-1].list);

            (yyval.node) = (Node *)n;
        }
#line 3870 "src/age/cypher_gram.c"
    break;

  case 176:
#line 1630 "src/age/cypher_gram.y"
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
#line 3886 "src/age/cypher_gram.c"
    break;

  case 177:
#line 1642 "src/age/cypher_gram.y"
        {
            CaseExpr *n;

            n = makeNode(CaseExpr);
            n->casetype = InvalidOid;
            n->args = (yyvsp[-2].list);
            n->defresult = (Expr *) (yyvsp[-1].node);
            n->location = (yylsp[-3]);
            (yyval.node) = (Node *) n;
        }
#line 3901 "src/age/cypher_gram.c"
    break;

  case 178:
#line 1656 "src/age/cypher_gram.y"
        {
            (yyval.list) = list_make1((yyvsp[0].node));
        }
#line 3909 "src/age/cypher_gram.c"
    break;

  case 179:
#line 1660 "src/age/cypher_gram.y"
        {
            (yyval.list) = lappend((yyvsp[-1].list), (yyvsp[0].node));
        }
#line 3917 "src/age/cypher_gram.c"
    break;

  case 180:
#line 1667 "src/age/cypher_gram.y"
        {
            CaseWhen   *n;

            n = makeNode(CaseWhen);
            n->expr = (Expr *) (yyvsp[-2].node);
            n->result = (Expr *) (yyvsp[0].node);
            n->location = (yylsp[-3]);
            (yyval.node) = (Node *) n;
        }
#line 3931 "src/age/cypher_gram.c"
    break;

  case 181:
#line 1680 "src/age/cypher_gram.y"
        {
            (yyval.node) = (yyvsp[0].node);
        }
#line 3939 "src/age/cypher_gram.c"
    break;

  case 182:
#line 1684 "src/age/cypher_gram.y"
        {
            (yyval.node) = NULL;
        }
#line 3947 "src/age/cypher_gram.c"
    break;

  case 183:
#line 1691 "src/age/cypher_gram.y"
        {
            ColumnRef *n;

            n = makeNode(ColumnRef);
            n->fields = list_make1(makeString((yyvsp[0].string)));
            n->location = (yylsp[0]);

            (yyval.node) = (Node *)n;
        }
#line 3961 "src/age/cypher_gram.c"
    break;

  case 184:
#line 1707 "src/age/cypher_gram.y"
        {
            (yyval.list) = list_make1(makeString((yyvsp[0].string)));
        }
#line 3969 "src/age/cypher_gram.c"
    break;

  case 185:
#line 1717 "src/age/cypher_gram.y"
        {
            (yyval.list) = list_make2(makeString((char *)(yyvsp[-2].keyword)), makeString((yyvsp[0].string)));
        }
#line 3977 "src/age/cypher_gram.c"
    break;

  case 188:
#line 1732 "src/age/cypher_gram.y"
        {
            (yyval.string) = NULL;
        }
#line 3985 "src/age/cypher_gram.c"
    break;

  case 193:
#line 1749 "src/age/cypher_gram.y"
        {
            /* we don't need to copy it, as it already has been */
            (yyval.string) = (char *) (yyvsp[0].keyword);
        }
#line 3994 "src/age/cypher_gram.c"
    break;

  case 196:
#line 1766 "src/age/cypher_gram.y"
                 { (yyval.keyword) = pnstrdup((yyvsp[0].keyword), 3); }
#line 4000 "src/age/cypher_gram.c"
    break;

  case 197:
#line 1767 "src/age/cypher_gram.y"
                 { (yyval.keyword) = pnstrdup((yyvsp[0].keyword), 7); }
#line 4006 "src/age/cypher_gram.c"
    break;

  case 198:
#line 1768 "src/age/cypher_gram.y"
                 { (yyval.keyword) = pnstrdup((yyvsp[0].keyword), 3); }
#line 4012 "src/age/cypher_gram.c"
    break;

  case 199:
#line 1769 "src/age/cypher_gram.y"
                 { (yyval.keyword) = pnstrdup((yyvsp[0].keyword), 2); }
#line 4018 "src/age/cypher_gram.c"
    break;

  case 200:
#line 1770 "src/age/cypher_gram.y"
                 { (yyval.keyword) = pnstrdup((yyvsp[0].keyword), 3); }
#line 4024 "src/age/cypher_gram.c"
    break;

  case 201:
#line 1771 "src/age/cypher_gram.y"
                 { (yyval.keyword) = pnstrdup((yyvsp[0].keyword), 9); }
#line 4030 "src/age/cypher_gram.c"
    break;

  case 202:
#line 1772 "src/age/cypher_gram.y"
                 { (yyval.keyword) = pnstrdup((yyvsp[0].keyword), 2); }
#line 4036 "src/age/cypher_gram.c"
    break;

  case 203:
#line 1773 "src/age/cypher_gram.y"
                 { (yyval.keyword) = pnstrdup((yyvsp[0].keyword), 4); }
#line 4042 "src/age/cypher_gram.c"
    break;

  case 204:
#line 1774 "src/age/cypher_gram.y"
                 { (yyval.keyword) = pnstrdup((yyvsp[0].keyword), 4); }
#line 4048 "src/age/cypher_gram.c"
    break;

  case 205:
#line 1775 "src/age/cypher_gram.y"
                 { (yyval.keyword) = pnstrdup((yyvsp[0].keyword), 8); }
#line 4054 "src/age/cypher_gram.c"
    break;

  case 206:
#line 1776 "src/age/cypher_gram.y"
                 { (yyval.keyword) = pnstrdup((yyvsp[0].keyword), 8); }
#line 4060 "src/age/cypher_gram.c"
    break;

  case 207:
#line 1777 "src/age/cypher_gram.y"
                 { (yyval.keyword) = pnstrdup((yyvsp[0].keyword), 6); }
#line 4066 "src/age/cypher_gram.c"
    break;

  case 208:
#line 1778 "src/age/cypher_gram.y"
                 { (yyval.keyword) = pnstrdup((yyvsp[0].keyword), 6); }
#line 4072 "src/age/cypher_gram.c"
    break;

  case 209:
#line 1779 "src/age/cypher_gram.y"
                 { (yyval.keyword) = pnstrdup((yyvsp[0].keyword), 4); }
#line 4078 "src/age/cypher_gram.c"
    break;

  case 210:
#line 1780 "src/age/cypher_gram.y"
                 { (yyval.keyword) = pnstrdup((yyvsp[0].keyword), 10); }
#line 4084 "src/age/cypher_gram.c"
    break;

  case 211:
#line 1781 "src/age/cypher_gram.y"
                 { (yyval.keyword) = pnstrdup((yyvsp[0].keyword), 6); }
#line 4090 "src/age/cypher_gram.c"
    break;

  case 212:
#line 1782 "src/age/cypher_gram.y"
                 { (yyval.keyword) = pnstrdup((yyvsp[0].keyword), 8); }
#line 4096 "src/age/cypher_gram.c"
    break;

  case 213:
#line 1783 "src/age/cypher_gram.y"
                 { (yyval.keyword) = pnstrdup((yyvsp[0].keyword), 4); }
#line 4102 "src/age/cypher_gram.c"
    break;

  case 214:
#line 1784 "src/age/cypher_gram.y"
                 { (yyval.keyword) = pnstrdup((yyvsp[0].keyword), 4); }
#line 4108 "src/age/cypher_gram.c"
    break;

  case 215:
#line 1785 "src/age/cypher_gram.y"
                 { (yyval.keyword) = pnstrdup((yyvsp[0].keyword), 6); }
#line 4114 "src/age/cypher_gram.c"
    break;

  case 216:
#line 1786 "src/age/cypher_gram.y"
                 { (yyval.keyword) = pnstrdup((yyvsp[0].keyword), 7); }
#line 4120 "src/age/cypher_gram.c"
    break;

  case 217:
#line 1787 "src/age/cypher_gram.y"
                 { (yyval.keyword) = pnstrdup((yyvsp[0].keyword), 2); }
#line 4126 "src/age/cypher_gram.c"
    break;

  case 218:
#line 1788 "src/age/cypher_gram.y"
                 { (yyval.keyword) = pnstrdup((yyvsp[0].keyword), 2); }
#line 4132 "src/age/cypher_gram.c"
    break;

  case 219:
#line 1789 "src/age/cypher_gram.y"
                 { (yyval.keyword) = pnstrdup((yyvsp[0].keyword), 6); }
#line 4138 "src/age/cypher_gram.c"
    break;

  case 220:
#line 1790 "src/age/cypher_gram.y"
                 { (yyval.keyword) = pnstrdup((yyvsp[0].keyword), 6); }
#line 4144 "src/age/cypher_gram.c"
    break;

  case 221:
#line 1791 "src/age/cypher_gram.y"
                 { (yyval.keyword) = pnstrdup((yyvsp[0].keyword), 6); }
#line 4150 "src/age/cypher_gram.c"
    break;

  case 222:
#line 1792 "src/age/cypher_gram.y"
                 { (yyval.keyword) = pnstrdup((yyvsp[0].keyword), 3); }
#line 4156 "src/age/cypher_gram.c"
    break;

  case 223:
#line 1793 "src/age/cypher_gram.y"
                 { (yyval.keyword) = pnstrdup((yyvsp[0].keyword), 8); }
#line 4162 "src/age/cypher_gram.c"
    break;

  case 224:
#line 1794 "src/age/cypher_gram.y"
                 { (yyval.keyword) = pnstrdup((yyvsp[0].keyword), 2); }
#line 4168 "src/age/cypher_gram.c"
    break;

  case 225:
#line 1795 "src/age/cypher_gram.y"
                 { (yyval.keyword) = pnstrdup((yyvsp[0].keyword), 5); }
#line 4174 "src/age/cypher_gram.c"
    break;

  case 226:
#line 1796 "src/age/cypher_gram.y"
                 { (yyval.keyword) = pnstrdup((yyvsp[0].keyword), 6); }
#line 4180 "src/age/cypher_gram.c"
    break;

  case 227:
#line 1797 "src/age/cypher_gram.y"
                 { (yyval.keyword) = pnstrdup((yyvsp[0].keyword), 6); }
#line 4186 "src/age/cypher_gram.c"
    break;

  case 228:
#line 1798 "src/age/cypher_gram.y"
                 { (yyval.keyword) = pnstrdup((yyvsp[0].keyword), 3); }
#line 4192 "src/age/cypher_gram.c"
    break;

  case 229:
#line 1799 "src/age/cypher_gram.y"
                 { (yyval.keyword) = pnstrdup((yyvsp[0].keyword), 4); }
#line 4198 "src/age/cypher_gram.c"
    break;

  case 230:
#line 1800 "src/age/cypher_gram.y"
                 { (yyval.keyword) = pnstrdup((yyvsp[0].keyword), 6); }
#line 4204 "src/age/cypher_gram.c"
    break;

  case 231:
#line 1801 "src/age/cypher_gram.y"
                 { (yyval.keyword) = pnstrdup((yyvsp[0].keyword), 4); }
#line 4210 "src/age/cypher_gram.c"
    break;

  case 232:
#line 1802 "src/age/cypher_gram.y"
                 { (yyval.keyword) = pnstrdup((yyvsp[0].keyword), 5); }
#line 4216 "src/age/cypher_gram.c"
    break;

  case 233:
#line 1803 "src/age/cypher_gram.y"
                 { (yyval.keyword) = pnstrdup((yyvsp[0].keyword), 4); }
#line 4222 "src/age/cypher_gram.c"
    break;

  case 234:
#line 1804 "src/age/cypher_gram.y"
                 { (yyval.keyword) = pnstrdup((yyvsp[0].keyword), 7); }
#line 4228 "src/age/cypher_gram.c"
    break;

  case 235:
#line 1805 "src/age/cypher_gram.y"
                 { (yyval.keyword) = pnstrdup((yyvsp[0].keyword), 5); }
#line 4234 "src/age/cypher_gram.c"
    break;

  case 236:
#line 1806 "src/age/cypher_gram.y"
                 { (yyval.keyword) = pnstrdup((yyvsp[0].keyword), 4); }
#line 4240 "src/age/cypher_gram.c"
    break;

  case 237:
#line 1807 "src/age/cypher_gram.y"
                 { (yyval.keyword) = pnstrdup((yyvsp[0].keyword), 3); }
#line 4246 "src/age/cypher_gram.c"
    break;

  case 238:
#line 1808 "src/age/cypher_gram.y"
                 { (yyval.keyword) = pnstrdup((yyvsp[0].keyword), 5); }
#line 4252 "src/age/cypher_gram.c"
    break;

  case 239:
#line 1812 "src/age/cypher_gram.y"
              { (yyval.keyword) = pnstrdup((yyvsp[0].keyword), 5); }
#line 4258 "src/age/cypher_gram.c"
    break;

  case 240:
#line 1813 "src/age/cypher_gram.y"
              { (yyval.keyword) = pnstrdup((yyvsp[0].keyword), 7); }
#line 4264 "src/age/cypher_gram.c"
    break;

  case 241:
#line 1814 "src/age/cypher_gram.y"
              { (yyval.keyword) = pnstrdup((yyvsp[0].keyword), 6); }
#line 4270 "src/age/cypher_gram.c"
    break;

  case 242:
#line 1815 "src/age/cypher_gram.y"
              { (yyval.keyword) = pnstrdup((yyvsp[0].keyword), 6); }
#line 4276 "src/age/cypher_gram.c"
    break;


#line 4280 "src/age/cypher_gram.c"

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
#line 1818 "src/age/cypher_gram.y"


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

static Node *make_xor_expr(Node *lexpr, Node *rexpr, int location)
{
    Expr *aorb;
    Expr *notaandb;

    // XOR is (A OR B) AND (NOT (A AND B))
    aorb = makeBoolExpr(OR_EXPR, list_make2(lexpr, rexpr), location);

    notaandb = makeBoolExpr(AND_EXPR, list_make2(lexpr, rexpr), location);
    notaandb = makeBoolExpr(NOT_EXPR, list_make1(notaandb), location);

    return (Node *)makeBoolExpr(AND_EXPR, list_make2(aorb, notaandb), location);
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

        if (c->val_age.type == T_Integer)
        {
            c->val_age.val.ival = -c->val_age.val.ival;
            return n;
        }
        else if (c->val_age.type == T_Float)
        {
            do_negate_float(&c->val_age);
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
    n->val_age.type = T_Integer;
    n->val_age.val.ival = i;
    n->location = location;

    return (Node *)n;
}

static Node *make_float_const(char *s, int location)
{
    A_Const *n;

    n = makeNode(A_Const);
    n->val_age.type = T_Float;
    n->val_age.val.str = s;
    n->location = location;

    return (Node *)n;
}

static Node *make_string_const(char *s, int location)
{
    A_Const *n;

    n = makeNode(A_Const);
    n->val_age.type = T_String;
    n->val_age.val.str = s;
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
    n->val_age.type = T_Null;
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
        if (strcasecmp(name, "rand") == 0)
            funcname = SystemFuncName("random");
        else if (strcasecmp(name, "pi") == 0)
            funcname = SystemFuncName("pi");
        else if (strcasecmp(name, "count") == 0)
            funcname = SystemFuncName("count");
        else
            /*
             * We don't qualify AGE functions here. This is done in the
             * transform layer and allows us to know which functions are ours.
             */
            funcname = func_name;

        /* build the function call */
        fnode = makeFuncCall(funcname, exprs, COERCE_SQL_SYNTAX, location);
    }
    /* all other functions are passed as is */
    else
        fnode = makeFuncCall(func_name, exprs, COERCE_SQL_SYNTAX, location);

    /* return the node */
    return (Node *)fnode;
}

/* function to create a unique name given a prefix */
static char *create_unique_name(char *prefix_name)
{
    char *name = NULL;
    char *prefix = NULL;
    uint nlen = 0;
    unsigned long unique_number = 0;

    /* get a unique number */
    unique_number = get_a_unique_number();

    /* was a valid prefix supplied */
    if (prefix_name == NULL || strlen(prefix_name) <= 0)
    {
        prefix = pnstrdup(UNIQUE_NAME_NULL_PREFIX,
                          strlen(UNIQUE_NAME_NULL_PREFIX));
    }
    else
    {
        prefix = prefix_name;
    }

    /* get the length of the combinded string */
    nlen = snprintf(NULL, 0, "%s_%lu", prefix, unique_number);

    /* allocate the space */
    name = palloc0(nlen + 1);

    /* create the name */
    snprintf(name, nlen + 1, "%s_%lu", prefix, unique_number);

    /* if we created the prefix, we need to free it */
    if (prefix_name == NULL || strlen(prefix_name) <= 0)
    {
        pfree(prefix);
    }

    return name;
}

/* function to return a unique unsigned long number */
static unsigned long get_a_unique_number(void)
{
    /* STATIC VARIABLE unique_counter for number uniqueness */
    static unsigned long unique_counter = 0;

    return unique_counter++;
}

/*set operation function node to make a set op node*/
static Node *make_set_op(SetOperation op, bool all_or_distinct, List *larg,
                         List *rarg)
{
    cypher_return *n = make_ag_node(cypher_return);

    n->op = op;
    n->all_or_distinct = all_or_distinct;
    n->larg = (List *) larg;
    n->rarg = (List *) rarg;
    return (Node *) n;
}

/* check if A_Expr is a comparison expression */
static bool is_A_Expr_a_comparison_operation(A_Expr *a)
{
    Value *v = NULL;
    char *opr_name = NULL;

    /* we don't support qualified comparison operators */
    if (list_length(a->name) != 1)
    {
        ereport(ERROR,
                (errcode(ERRCODE_SYNTAX_ERROR),
                 errmsg("qualified comparison operator names are not permitted")));
    }

    /* get the value and verify that it is a string */
    v = linitial(a->name);
    Assert(v->type == T_String);

    /* get the string value */
    opr_name = v->val.str;

    /* verify it is a comparison operation */
    if (strcmp(opr_name, "<") == 0)
    {
        return true;
    }
    if (strcmp(opr_name, ">") == 0)
    {
        return true;
    }
    if (strcmp(opr_name, "<=") == 0)
    {
        return true;
    }
    if (strcmp(opr_name, "=>") == 0)
    {
        return true;
    }
    if (strcmp(opr_name, "=") == 0)
    {
        return true;
    }
    if (strcmp(opr_name, "<>") == 0)
    {
        return true;
    }

    return false;
}

/*
 * Helper function to build the comparison operator expression. It will also
 * build a chained comparison operator expression if it detects a chained
 * comparison.
 */
static Node *build_comparison_expression(Node *left_grammar_node,
                                         Node *right_grammar_node,
                                         char *opr_name, int location)
{
    Node *result_expr = NULL;

    Assert(left_grammar_node != NULL);
    Assert(right_grammar_node != NULL);
    Assert(opr_name != NULL);

    /*
     * Case 1:
     *    If the previous expression is an A_Expr and it is also a
     *    comparison, then this is part of a chained comparison. In this
     *    specific case, the second chained element.
     */
    if (IsA(left_grammar_node, A_Expr) &&
        is_A_Expr_a_comparison_operation((A_Expr *)left_grammar_node))
    {
        A_Expr *aexpr = NULL;
        Node *lexpr = NULL;
        Node *n = NULL;

        /* get the A_Expr on the left side */
        aexpr = (A_Expr *) left_grammar_node;
        /* get its rexpr which will be our lexpr */
        lexpr = aexpr->rexpr;
        /* build our comparison operator */
        n = (Node *)makeSimpleA_Expr(AEXPR_OP, opr_name, lexpr,
                                     right_grammar_node, location);

        /* now add it (AND) to the other comparison */
        result_expr = make_and_expr(left_grammar_node, n, location);
    }

    /*
     * Case 2:
     *    If the previous expression is a boolean AND and its right most
     *    expression is an A_Expr and a comparison, then this is part of
     *    a chained comparison. In this specific case, the third and
     *    beyond chained element.
     */
    if (IsA(left_grammar_node, BoolExpr) &&
        ((BoolExpr*)left_grammar_node)->boolop == AND_EXPR)
    {
        BoolExpr *bexpr = NULL;
        Node *last = NULL;

        /* cast the left to a boolean */
        bexpr = (BoolExpr *)left_grammar_node;
        /* extract the last node - ANDs are chained in a flat list */
        last = llast(bexpr->args);

        /* is the last node an A_Expr and a comparison operator */
        if (IsA(last, A_Expr) &&
            is_A_Expr_a_comparison_operation((A_Expr *)last))
        {
            A_Expr *aexpr = NULL;
            Node *lexpr = NULL;
            Node *n = NULL;

            /* get the last expressions right expression */
            aexpr = (A_Expr *) last;
            lexpr = aexpr->rexpr;
            /* make our comparison operator */
            n = (Node *)makeSimpleA_Expr(AEXPR_OP, opr_name, lexpr,
                                         right_grammar_node, location);

            /* now add it (AND) to the other comparisons */
            result_expr = make_and_expr(left_grammar_node, n, location);
        }
    }

    /*
     * Case 3:
     *    The previous expression isn't a chained comparison. So, treat
     *    it as a regular comparison expression.
     */
    if (result_expr == NULL)
    {
        result_expr = (Node *)makeSimpleA_Expr(AEXPR_OP, opr_name,
                                               left_grammar_node,
                                               right_grammar_node, location);
    }

    return result_expr;
}

static cypher_relationship *build_VLE_relation(List *left_arg,
                                               cypher_relationship *cr,
                                               Node *right_arg,
                                               int left_arg_location,
                                               int cr_location)
{
    ColumnRef *cref = NULL;
    A_Indices *ai = NULL;
    List *args = NIL;
    List *eargs = NIL;
    List *fname = NIL;
    cypher_node *cnl = NULL;
    cypher_node *cnr = NULL;
    Node *node = NULL;
    int length = 0;
    unsigned long unique_number = 0;
    int location = 0;

    /* get a unique number to identify this VLE node */
    unique_number = get_a_unique_number();

    /* get the location */
    location = cr->location;

    /* get the left and right cypher_nodes */
    cnl = (cypher_node*)llast(left_arg);
    cnr = (cypher_node*)right_arg;

    /* get the length of the left path */
    length = list_length(left_arg);

    /*
     * If the left name is NULL and the left path is greater than 1
     * If the left name is NULL and the left label is not NULL
     * If the left name is NULL and the left props is not NULL
     * If the left name is NULL and the right name is not NULL
     * If the left name is NULL and the right label is not NULL
     * If the left name is NULL and the right props is not NULL
     * we need to create a variable name for the left node.
     */
    if ((cnl->name == NULL && length > 1) ||
        (cnl->name == NULL && cnl->label != NULL) ||
        (cnl->name == NULL && cnl->props != NULL) ||
        (cnl->name == NULL && cnr->name != NULL) ||
        (cnl->name == NULL && cnr->label != NULL) ||
        (cnl->name == NULL && cnr->props != NULL))
    {
        cnl->name = create_unique_name("_vle_function_start_var");
    }

    /* add in the start vertex as a ColumnRef if necessary */
    if (cnl->name != NULL)
    {
        cref = makeNode(ColumnRef);
        cref->fields = list_make2(makeString(cnl->name), makeString("id"));
        cref->location = left_arg_location;
        args = lappend(args, cref);
    }
    /*
     * If there aren't any variables in the VLE path, we can use
     * the FROM_ALL algorithm.
     */
    else
    {
        args = lappend(args, make_null_const(-1));
    }

    /*
     * Create a variable name for the end vertex if we have a label
     * name or props but we don't have a variable name.
     *
     * For example: ()-[*]-(:label) or ()-[*]-({name: "John"})
     *
     * We need this so the addition of match_vle_terminal_edge is
     * done in the transform phase.
     */
    if (cnr->name == NULL &&
        (cnr->label != NULL || cnr->props != NULL))
    {
        cnr->name = create_unique_name("_vle_function_end_var");
    }
    /*
     * We need a NULL for the target vertex in the VLE match to
     * force the dfs_find_a_path_from algorithm. However, for now,
     * the default will be to only do that when a target isn't
     * supplied.
     *
     * TODO: We will likely want to force it to use
     * dfs_find_a_path_from.
     */
    if (cnl->name == NULL && cnr->name != NULL)
    {
        cref = makeNode(ColumnRef);
        cref->fields = list_make2(makeString(cnr->name), makeString("id"));
        cref->location = left_arg_location;
        args = lappend(args, cref);
    }
    else
    {
        args = lappend(args, make_null_const(-1));
    }

    /* build the required edge arguments */
    if (cr->label == NULL)
    {
        eargs = lappend(eargs, make_null_const(location));
    }
    else
    {
        eargs = lappend(eargs, make_string_const(cr->label, location));
    }
    if (cr->props == NULL)
    {
        eargs = lappend(eargs, make_null_const(location));
    }
    else
    {
        eargs = lappend(eargs, cr->props);
    }
    /* build the edge function name (schema.funcname) */
    fname = list_make2(makeString("ag_catalog"),
                       makeString("age_build_vle_match_edge"));
    /* build the edge function node */
    node = make_function_expr(fname, eargs, location);
    /* add in the edge*/
    args = lappend(args, node);
    /* add in the lidx and uidx range as Const */
    ai = (A_Indices*)cr->varlen;
    if (ai == NULL || ai->lidx == NULL)
    {
        args = lappend(args, make_null_const(location));
    }
    else
    {
        args = lappend(args, ai->lidx);
    }
    if (ai == NULL || ai->uidx == NULL)
    {
        args = lappend(args, make_null_const(location));
    }
    else
    {
        args = lappend(args, ai->uidx);
    }
    /* add in the direction as Const */
    args = lappend(args, make_int_const(cr->dir, cr_location));

    /* add in the unique number used to identify this VLE node */
    args = lappend(args, make_int_const(unique_number, -1));

    /* build the VLE function node */
    cr->varlen = make_function_expr(list_make1(makeString("vle")), args,
                                    cr_location);
    /* return the VLE relation node */
    return cr;
}
