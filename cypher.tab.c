/* A Bison parser, made by GNU Bison 2.3.  */

/* Skeleton implementation for Bison's Yacc-like parsers in C

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

/* C LALR(1) parser skeleton written by Richard Stallman, by
   simplifying the original so-called "semantic" parser.  */

/* All symbols defined below should begin with yy or YY, to avoid
   infringing on user name space.  This should be done even for local
   variables, as they might otherwise be expanded by user macros.
   There are some unavoidable exceptions within include files to
   define necessary library symbols; they are noted "INFRINGES ON
   USER NAME SPACE" below.  */

/* Identify Bison output.  */
#define YYBISON 1

/* Bison version.  */
#define YYBISON_VERSION "2.3"

/* Skeleton name.  */
#define YYSKELETON_NAME "yacc.c"

/* Pure parsers.  */
#define YYPURE 0

/* Using locations.  */
#define YYLSP_NEEDED 0



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




/* Copy the first part of user declarations.  */
#line 1 "cypher.y"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "postgres_fe.h" 
#include "psqlscanslash.h"                                                      
#include "common/logging.h"                                                     
#include "fe_utils/conditional.h"                                               
#include "libpq-fe.h"                                                           
#include "cypherscan.h"

#include "cypher.tab.h"

void yyerror(char const *s);

typedef struct yy_buffer_state *YY_BUFFER_STATE;

typedef struct {
    char* str_val;
    int int_val;
} yyval;

int yylex(void);
int rel_direction = 0; // 1 for "->", -1 for "<-"
int order_clause_direction = 1; // 1 for ascending, -1 for descending

bool match = false;
bool where = false;
bool with = false;
bool rtn = false;

struct edge_pattern {
    int lower;
    bool dot;
    int upper;
};

struct map_pair {
    char* exp;
    char* idt;
};

static void reset_values();


/* Enabling traces.  */
#ifndef YYDEBUG
# define YYDEBUG 0
#endif

/* Enabling verbose error messages.  */
#ifdef YYERROR_VERBOSE
# undef YYERROR_VERBOSE
# define YYERROR_VERBOSE 1
#else
# define YYERROR_VERBOSE 0
#endif

/* Enabling the token table.  */
#ifndef YYTOKEN_TABLE
# define YYTOKEN_TABLE 0
#endif

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
/* Line 193 of yacc.c.  */
#line 228 "cypher.tab.c"
	YYSTYPE;
# define yystype YYSTYPE /* obsolescent; will be withdrawn */
# define YYSTYPE_IS_DECLARED 1
# define YYSTYPE_IS_TRIVIAL 1
#endif



/* Copy the second part of user declarations.  */


/* Line 216 of yacc.c.  */
#line 241 "cypher.tab.c"

#ifdef short
# undef short
#endif

#ifdef YYTYPE_UINT8
typedef YYTYPE_UINT8 yytype_uint8;
#else
typedef unsigned char yytype_uint8;
#endif

#ifdef YYTYPE_INT8
typedef YYTYPE_INT8 yytype_int8;
#elif (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
typedef signed char yytype_int8;
#else
typedef short int yytype_int8;
#endif

#ifdef YYTYPE_UINT16
typedef YYTYPE_UINT16 yytype_uint16;
#else
typedef unsigned short int yytype_uint16;
#endif

#ifdef YYTYPE_INT16
typedef YYTYPE_INT16 yytype_int16;
#else
typedef short int yytype_int16;
#endif

#ifndef YYSIZE_T
# ifdef __SIZE_TYPE__
#  define YYSIZE_T __SIZE_TYPE__
# elif defined size_t
#  define YYSIZE_T size_t
# elif ! defined YYSIZE_T && (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
#  include <stddef.h> /* INFRINGES ON USER NAME SPACE */
#  define YYSIZE_T size_t
# else
#  define YYSIZE_T unsigned int
# endif
#endif

#define YYSIZE_MAXIMUM ((YYSIZE_T) -1)

#ifndef YY_
# if defined YYENABLE_NLS && YYENABLE_NLS
#  if ENABLE_NLS
#   include <libintl.h> /* INFRINGES ON USER NAME SPACE */
#   define YY_(msgid) dgettext ("bison-runtime", msgid)
#  endif
# endif
# ifndef YY_
#  define YY_(msgid) msgid
# endif
#endif

/* Suppress unused-variable warnings by "using" E.  */
#if ! defined lint || defined __GNUC__
# define YYUSE(e) ((void) (e))
#else
# define YYUSE(e) /* empty */
#endif

/* Identity function, used to suppress warnings about constant conditions.  */
#ifndef lint
# define YYID(n) (n)
#else
#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
static int
YYID (int i)
#else
static int
YYID (i)
    int i;
#endif
{
  return i;
}
#endif

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
#    if ! defined _ALLOCA_H && ! defined _STDLIB_H && (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
#     include <stdlib.h> /* INFRINGES ON USER NAME SPACE */
#     ifndef _STDLIB_H
#      define _STDLIB_H 1
#     endif
#    endif
#   endif
#  endif
# endif

# ifdef YYSTACK_ALLOC
   /* Pacify GCC's `empty if-body' warning.  */
#  define YYSTACK_FREE(Ptr) do { /* empty */; } while (YYID (0))
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
#  if (defined __cplusplus && ! defined _STDLIB_H \
       && ! ((defined YYMALLOC || defined malloc) \
	     && (defined YYFREE || defined free)))
#   include <stdlib.h> /* INFRINGES ON USER NAME SPACE */
#   ifndef _STDLIB_H
#    define _STDLIB_H 1
#   endif
#  endif
#  ifndef YYMALLOC
#   define YYMALLOC malloc
#   if ! defined malloc && ! defined _STDLIB_H && (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
void *malloc (YYSIZE_T); /* INFRINGES ON USER NAME SPACE */
#   endif
#  endif
#  ifndef YYFREE
#   define YYFREE free
#   if ! defined free && ! defined _STDLIB_H && (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
void free (void *); /* INFRINGES ON USER NAME SPACE */
#   endif
#  endif
# endif
#endif /* ! defined yyoverflow || YYERROR_VERBOSE */


#if (! defined yyoverflow \
     && (! defined __cplusplus \
	 || (defined YYSTYPE_IS_TRIVIAL && YYSTYPE_IS_TRIVIAL)))

/* A type that is properly aligned for any stack member.  */
union yyalloc
{
  yytype_int16 yyss;
  YYSTYPE yyvs;
  };

/* The size of the maximum gap between one aligned stack and the next.  */
# define YYSTACK_GAP_MAXIMUM (sizeof (union yyalloc) - 1)

/* The size of an array large to enough to hold all stacks, each with
   N elements.  */
# define YYSTACK_BYTES(N) \
     ((N) * (sizeof (yytype_int16) + sizeof (YYSTYPE)) \
      + YYSTACK_GAP_MAXIMUM)

/* Copy COUNT objects from FROM to TO.  The source and destination do
   not overlap.  */
# ifndef YYCOPY
#  if defined __GNUC__ && 1 < __GNUC__
#   define YYCOPY(To, From, Count) \
      __builtin_memcpy (To, From, (Count) * sizeof (*(From)))
#  else
#   define YYCOPY(To, From, Count)		\
      do					\
	{					\
	  YYSIZE_T yyi;				\
	  for (yyi = 0; yyi < (Count); yyi++)	\
	    (To)[yyi] = (From)[yyi];		\
	}					\
      while (YYID (0))
#  endif
# endif

/* Relocate STACK from its old location to the new one.  The
   local variables YYSIZE and YYSTACKSIZE give the old and new number of
   elements in the stack, and YYPTR gives the new location of the
   stack.  Advance YYPTR to a properly aligned location for the next
   stack.  */
# define YYSTACK_RELOCATE(Stack)					\
    do									\
      {									\
	YYSIZE_T yynewbytes;						\
	YYCOPY (&yyptr->Stack, Stack, yysize);				\
	Stack = &yyptr->Stack;						\
	yynewbytes = yystacksize * sizeof (*Stack) + YYSTACK_GAP_MAXIMUM; \
	yyptr += yynewbytes / sizeof (*yyptr);				\
      }									\
    while (YYID (0))

#endif

/* YYFINAL -- State number of the termination state.  */
#define YYFINAL  16
/* YYLAST -- Last index in YYTABLE.  */
#define YYLAST   78

/* YYNTOKENS -- Number of terminals.  */
#define YYNTOKENS  40
/* YYNNTS -- Number of nonterminals.  */
#define YYNNTS  43
/* YYNRULES -- Number of rules.  */
#define YYNRULES  80
/* YYNRULES -- Number of states.  */
#define YYNSTATES  115

/* YYTRANSLATE(YYLEX) -- Bison symbol number corresponding to YYLEX.  */
#define YYUNDEFTOK  2
#define YYMAXUTOK   294

#define YYTRANSLATE(YYX)						\
  ((unsigned int) (YYX) <= YYMAXUTOK ? yytranslate[YYX] : YYUNDEFTOK)

/* YYTRANSLATE[YYLEX] -- Bison symbol number corresponding to YYLEX.  */
static const yytype_uint8 yytranslate[] =
{
       0,     2,     2,     2,     2,     2,     2,     2,     2,     2,
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
      35,    36,    37,    38,    39
};

#if YYDEBUG
/* YYPRHS[YYN] -- Index of the first RHS symbol of rule number YYN in
   YYRHS.  */
static const yytype_uint8 yyprhs[] =
{
       0,     0,     3,     5,     7,    10,    13,    15,    17,    19,
      21,    22,    27,    29,    33,    34,    41,    42,    44,    45,
      48,    49,    53,    57,    62,    67,    73,    74,    76,    77,
      80,    81,    84,    85,    89,    90,    93,    94,    96,    97,
      99,   101,   105,   109,   111,   113,   115,   116,   120,   125,
     132,   133,   135,   137,   139,   141,   143,   145,   147,   148,
     152,   153,   158,   163,   165,   169,   171,   175,   176,   180,
     182,   186,   189,   190,   192,   194,   195,   198,   199,   202,
     203
};

/* YYRHS -- A `-1'-separated list of the rules' RHS.  */
static const yytype_int8 yyrhs[] =
{
      41,     0,    -1,    42,    -1,    33,    -1,    41,    42,    -1,
      41,    33,    -1,    43,    -1,    63,    -1,    69,    -1,    71,
      -1,    -1,    20,    44,    45,    82,    -1,    46,    -1,    45,
      51,    46,    -1,    -1,    10,    48,    49,    47,    50,    11,
      -1,    -1,    35,    -1,    -1,    12,    35,    -1,    -1,    16,
      59,    17,    -1,     5,    52,     5,    -1,     6,     5,    52,
       5,    -1,     5,    52,     5,     7,    -1,     8,    53,    54,
      55,     9,    -1,    -1,    35,    -1,    -1,    12,    35,    -1,
      -1,    18,    56,    -1,    -1,    34,    57,    58,    -1,    -1,
      19,    19,    -1,    -1,    34,    -1,    -1,    60,    -1,    61,
      -1,    60,    14,    61,    -1,    35,    12,    62,    -1,    34,
      -1,    36,    -1,    35,    -1,    -1,    21,    64,    65,    -1,
      66,    62,    67,    62,    -1,    65,    68,    66,    62,    67,
      62,    -1,    -1,    32,    -1,    37,    -1,     6,    -1,     7,
      -1,    29,    -1,    30,    -1,    31,    -1,    -1,    22,    70,
      73,    -1,    -1,    27,    72,    73,    15,    -1,    74,    76,
      80,    81,    -1,    75,    -1,    74,    14,    75,    -1,    62,
      -1,    62,    28,    35,    -1,    -1,    23,    24,    77,    -1,
      78,    -1,    77,    14,    78,    -1,    62,    79,    -1,    -1,
       3,    -1,     4,    -1,    -1,    25,    34,    -1,    -1,    26,
      34,    -1,    -1,    15,    -1
};

/* YYRLINE[YYN] -- source line where rule number YYN was defined.  */
static const yytype_uint16 yyrline[] =
{
       0,    73,    73,    74,    75,    79,    83,    84,    85,    86,
      90,    90,    94,    95,   100,    99,   107,   108,   112,   113,
     117,   118,   122,   123,   124,   128,   138,   139,   143,   144,
     148,   149,   153,   154,   158,   159,   163,   164,   168,   169,
     173,   174,   178,   182,   183,   184,   188,   188,   192,   193,
     197,   198,   202,   203,   204,   208,   209,   210,   214,   214,
     218,   218,   222,   226,   227,   231,   232,   235,   237,   241,
     242,   246,   250,   251,   252,   255,   257,   260,   262,   265,
     267
};
#endif

#if YYDEBUG || YYERROR_VERBOSE || YYTOKEN_TABLE
/* YYTNAME[SYMBOL-NUM] -- String name of the symbol SYMBOL-NUM.
   First, the terminals, then, starting at YYNTOKENS, nonterminals.  */
static const char *const yytname[] =
{
  "$end", "error", "$undefined", "ASC", "DESC", "DASH", "LT", "GT",
  "LBRACKET", "RBRACKET", "LPAREN", "RPAREN", "COLON", "PIPE", "COMMA",
  "SEMICOLON", "LBRACE", "RBRACE", "ASTERISK", "DOT", "MATCH", "WHERE",
  "WITH", "ORDER", "BY", "SKIP", "LIMIT", "RETURN", "AS", "AND", "OR",
  "XOR", "NOT", "exit_command", "INTEGER", "IDENTIFIER", "STRING",
  "COMPARATOR", "UNKNOWN", "ARROW", "$accept", "statement", "query",
  "match_clause", "@1", "path_pattern", "node_pattern", "@2",
  "node_alias_opt", "node_labels_opt", "node_properties_opt",
  "edge_pattern", "rel_pattern", "rel_alias_opt", "rel_labels_opt",
  "variable_length_edges_opt", "edge_length_opt", "dot_opt",
  "upper_bound_opt", "map_literal", "nonempty_map_literal", "map_entry",
  "expression", "where_clause", "@3", "where_expression", "not_opt",
  "compare", "logic", "with_clause", "@4", "return_clause", "@5",
  "item_clause", "item_list", "item", "order_clause_opt", "sort_item_list",
  "sort_item", "sort_direction_opt", "skip_clause_opt", "limit_clause_opt",
  "semicolon_opt", 0
};
#endif

# ifdef YYPRINT
/* YYTOKNUM[YYLEX-NUM] -- Internal token number corresponding to
   token YYLEX-NUM.  */
static const yytype_uint16 yytoknum[] =
{
       0,   256,   257,   258,   259,   260,   261,   262,   263,   264,
     265,   266,   267,   268,   269,   270,   271,   272,   273,   274,
     275,   276,   277,   278,   279,   280,   281,   282,   283,   284,
     285,   286,   287,   288,   289,   290,   291,   292,   293,   294
};
# endif

/* YYR1[YYN] -- Symbol number of symbol that rule YYN derives.  */
static const yytype_uint8 yyr1[] =
{
       0,    40,    41,    41,    41,    41,    42,    42,    42,    42,
      44,    43,    45,    45,    47,    46,    48,    48,    49,    49,
      50,    50,    51,    51,    51,    52,    53,    53,    54,    54,
      55,    55,    56,    56,    57,    57,    58,    58,    59,    59,
      60,    60,    61,    62,    62,    62,    64,    63,    65,    65,
      66,    66,    67,    67,    67,    68,    68,    68,    70,    69,
      72,    71,    73,    74,    74,    75,    75,    76,    76,    77,
      77,    78,    79,    79,    79,    80,    80,    81,    81,    82,
      82
};

/* YYR2[YYN] -- Number of symbols composing right hand side of rule YYN.  */
static const yytype_uint8 yyr2[] =
{
       0,     2,     1,     1,     2,     2,     1,     1,     1,     1,
       0,     4,     1,     3,     0,     6,     0,     1,     0,     2,
       0,     3,     3,     4,     4,     5,     0,     1,     0,     2,
       0,     2,     0,     3,     0,     2,     0,     1,     0,     1,
       1,     3,     3,     1,     1,     1,     0,     3,     4,     6,
       0,     1,     1,     1,     1,     1,     1,     1,     0,     3,
       0,     4,     4,     1,     3,     1,     3,     0,     3,     1,
       3,     2,     0,     1,     1,     0,     2,     0,     2,     0,
       1
};

/* YYDEFACT[STATE-NAME] -- Default rule to reduce with in state
   STATE-NUM when YYTABLE doesn't specify something else to do.  Zero
   means the default is an error.  */
static const yytype_uint8 yydefact[] =
{
       0,    10,    46,    58,    60,     3,     0,     2,     6,     7,
       8,     9,     0,    50,     0,     0,     1,     5,     4,    16,
      79,    12,    51,    47,     0,    43,    45,    44,    65,    59,
      67,    63,     0,    17,    18,     0,     0,    80,     0,    11,
      55,    56,    57,    50,     0,     0,     0,     0,    75,    61,
       0,    14,    26,     0,     0,    13,     0,    53,    54,    52,
       0,    66,    64,     0,     0,    77,    19,    20,    27,    28,
      22,     0,     0,    48,    72,    68,    69,    76,     0,    62,
      38,     0,     0,    30,    24,    23,     0,    73,    74,    71,
       0,    78,     0,     0,    39,    40,    15,    29,    32,     0,
      49,    70,     0,    21,     0,    34,    31,    25,    42,    41,
       0,    36,    35,    37,    33
};

/* YYDEFGOTO[NTERM-NUM].  */
static const yytype_int8 yydefgoto[] =
{
      -1,     6,     7,     8,    12,    20,    21,    67,    34,    51,
      81,    38,    53,    69,    83,    99,   106,   111,   114,    93,
      94,    95,    28,     9,    13,    23,    24,    60,    43,    10,
      14,    11,    15,    29,    30,    31,    48,    75,    76,    89,
      65,    79,    39
};

/* YYPACT[STATE-NUM] -- Index in YYTABLE of the portion describing
   STATE-NUM.  */
#define YYPACT_NINF -37
static const yytype_int8 yypact[] =
{
     -16,   -37,   -37,   -37,   -37,   -37,     1,   -37,   -37,   -37,
     -37,   -37,     0,   -17,   -22,   -22,   -37,   -37,   -37,    -8,
       3,   -37,   -37,    -5,   -22,   -37,   -37,   -37,     2,   -37,
      -7,   -37,    14,   -37,    19,    27,    32,   -37,     0,   -37,
     -37,   -37,   -37,   -17,    -4,     5,   -22,    17,    13,   -37,
       7,   -37,     8,    39,    27,   -37,   -22,   -37,   -37,   -37,
     -22,   -37,   -37,   -22,    11,    20,   -37,    31,   -37,    36,
      42,    45,    -4,   -37,    16,    37,   -37,   -37,    18,   -37,
      21,    43,    22,    35,   -37,   -37,   -22,   -37,   -37,   -37,
     -22,   -37,    46,    38,    47,   -37,   -37,   -37,    25,    51,
     -37,   -37,   -22,   -37,    21,    44,   -37,   -37,   -37,   -37,
      48,    30,   -37,   -37,   -37
};

/* YYPGOTO[NTERM-NUM].  */
static const yytype_int8 yypgoto[] =
{
     -37,   -37,    59,   -37,   -37,   -37,    33,   -37,   -37,   -37,
     -37,   -37,    15,   -37,   -37,   -37,   -37,   -37,   -37,   -37,
     -37,   -36,   -24,   -37,   -37,   -37,    29,    -2,   -37,   -37,
     -37,   -37,   -37,    58,   -37,    28,   -37,   -37,   -15,   -37,
     -37,   -37,   -37
};

/* YYTABLE[YYPACT[STATE-NUM]].  What to do in state STATE-NUM.  If
   positive, shift that token.  If negative, reduce the rule which
   number is the opposite.  If zero, do what YYDEFACT says.
   If YYTABLE_NINF, syntax error.  */
#define YYTABLE_NINF -1
static const yytype_uint8 yytable[] =
{
      44,    16,    57,    58,     1,     2,     3,    46,    35,    36,
      19,     4,    25,    26,    27,    22,    47,     5,    37,    87,
      88,     1,     2,     3,    40,    41,    42,    33,     4,    49,
      45,    50,    72,    59,    17,    52,    73,    54,    64,    74,
      61,    63,    66,    68,    70,    77,    78,    80,    82,    84,
      85,    90,    91,    98,    96,   103,    92,    97,   102,   105,
     107,   104,   100,   110,   113,    18,    74,   112,   109,    71,
      86,    55,    56,    32,    62,   101,     0,     0,   108
};

static const yytype_int8 yycheck[] =
{
      24,     0,     6,     7,    20,    21,    22,    14,     5,     6,
      10,    27,    34,    35,    36,    32,    23,    33,    15,     3,
       4,    20,    21,    22,    29,    30,    31,    35,    27,    15,
      28,    12,    56,    37,    33,     8,    60,     5,    25,    63,
      35,    24,    35,    35,     5,    34,    26,    16,    12,     7,
       5,    14,    34,    18,    11,    17,    35,    35,    12,    34,
       9,    14,    86,    19,    34,     6,    90,    19,   104,    54,
      72,    38,    43,    15,    46,    90,    -1,    -1,   102
};

/* YYSTOS[STATE-NUM] -- The (internal number of the) accessing
   symbol of state STATE-NUM.  */
static const yytype_uint8 yystos[] =
{
       0,    20,    21,    22,    27,    33,    41,    42,    43,    63,
      69,    71,    44,    64,    70,    72,     0,    33,    42,    10,
      45,    46,    32,    65,    66,    34,    35,    36,    62,    73,
      74,    75,    73,    35,    48,     5,     6,    15,    51,    82,
      29,    30,    31,    68,    62,    28,    14,    23,    76,    15,
      12,    49,     8,    52,     5,    46,    66,     6,     7,    37,
      67,    35,    75,    24,    25,    80,    35,    47,    35,    53,
       5,    52,    62,    62,    62,    77,    78,    34,    26,    81,
      16,    50,    12,    54,     7,     5,    67,     3,     4,    79,
      14,    34,    35,    59,    60,    61,    11,    35,    18,    55,
      62,    78,    12,    17,    14,    34,    56,     9,    62,    61,
      19,    57,    19,    34,    58
};

#define yyerrok		(yyerrstatus = 0)
#define yyclearin	(yychar = YYEMPTY)
#define YYEMPTY		(-2)
#define YYEOF		0

#define YYACCEPT	goto yyacceptlab
#define YYABORT		goto yyabortlab
#define YYERROR		goto yyerrorlab


/* Like YYERROR except do call yyerror.  This remains here temporarily
   to ease the transition to the new meaning of YYERROR, for GCC.
   Once GCC version 2 has supplanted version 1, this can go.  */

#define YYFAIL		goto yyerrlab

#define YYRECOVERING()  (!!yyerrstatus)

#define YYBACKUP(Token, Value)					\
do								\
  if (yychar == YYEMPTY && yylen == 1)				\
    {								\
      yychar = (Token);						\
      yylval = (Value);						\
      yytoken = YYTRANSLATE (yychar);				\
      YYPOPSTACK (1);						\
      goto yybackup;						\
    }								\
  else								\
    {								\
      yyerror (YY_("syntax error: cannot back up")); \
      YYERROR;							\
    }								\
while (YYID (0))


#define YYTERROR	1
#define YYERRCODE	256


/* YYLLOC_DEFAULT -- Set CURRENT to span from RHS[1] to RHS[N].
   If N is 0, then set CURRENT to the empty location which ends
   the previous symbol: RHS[0] (always defined).  */

#define YYRHSLOC(Rhs, K) ((Rhs)[K])
#ifndef YYLLOC_DEFAULT
# define YYLLOC_DEFAULT(Current, Rhs, N)				\
    do									\
      if (YYID (N))                                                    \
	{								\
	  (Current).first_line   = YYRHSLOC (Rhs, 1).first_line;	\
	  (Current).first_column = YYRHSLOC (Rhs, 1).first_column;	\
	  (Current).last_line    = YYRHSLOC (Rhs, N).last_line;		\
	  (Current).last_column  = YYRHSLOC (Rhs, N).last_column;	\
	}								\
      else								\
	{								\
	  (Current).first_line   = (Current).last_line   =		\
	    YYRHSLOC (Rhs, 0).last_line;				\
	  (Current).first_column = (Current).last_column =		\
	    YYRHSLOC (Rhs, 0).last_column;				\
	}								\
    while (YYID (0))
#endif


/* YY_LOCATION_PRINT -- Print the location on the stream.
   This macro was not mandated originally: define only if we know
   we won't break user code: when these are the locations we know.  */

#ifndef YY_LOCATION_PRINT
# if defined YYLTYPE_IS_TRIVIAL && YYLTYPE_IS_TRIVIAL
#  define YY_LOCATION_PRINT(File, Loc)			\
     fprintf (File, "%d.%d-%d.%d",			\
	      (Loc).first_line, (Loc).first_column,	\
	      (Loc).last_line,  (Loc).last_column)
# else
#  define YY_LOCATION_PRINT(File, Loc) ((void) 0)
# endif
#endif


/* YYLEX -- calling `yylex' with the right arguments.  */

#ifdef YYLEX_PARAM
# define YYLEX yylex (YYLEX_PARAM)
#else
# define YYLEX yylex ()
#endif

/* Enable debugging if requested.  */
#if YYDEBUG

# ifndef YYFPRINTF
#  include <stdio.h> /* INFRINGES ON USER NAME SPACE */
#  define YYFPRINTF fprintf
# endif

# define YYDPRINTF(Args)			\
do {						\
  if (yydebug)					\
    YYFPRINTF Args;				\
} while (YYID (0))

# define YY_SYMBOL_PRINT(Title, Type, Value, Location)			  \
do {									  \
  if (yydebug)								  \
    {									  \
      YYFPRINTF (stderr, "%s ", Title);					  \
      yy_symbol_print (stderr,						  \
		  Type, Value); \
      YYFPRINTF (stderr, "\n");						  \
    }									  \
} while (YYID (0))


/*--------------------------------.
| Print this symbol on YYOUTPUT.  |
`--------------------------------*/

/*ARGSUSED*/
#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
static void
yy_symbol_value_print (FILE *yyoutput, int yytype, YYSTYPE const * const yyvaluep)
#else
static void
yy_symbol_value_print (yyoutput, yytype, yyvaluep)
    FILE *yyoutput;
    int yytype;
    YYSTYPE const * const yyvaluep;
#endif
{
  if (!yyvaluep)
    return;
# ifdef YYPRINT
  if (yytype < YYNTOKENS)
    YYPRINT (yyoutput, yytoknum[yytype], *yyvaluep);
# else
  YYUSE (yyoutput);
# endif
  switch (yytype)
    {
      default:
	break;
    }
}


/*--------------------------------.
| Print this symbol on YYOUTPUT.  |
`--------------------------------*/

#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
static void
yy_symbol_print (FILE *yyoutput, int yytype, YYSTYPE const * const yyvaluep)
#else
static void
yy_symbol_print (yyoutput, yytype, yyvaluep)
    FILE *yyoutput;
    int yytype;
    YYSTYPE const * const yyvaluep;
#endif
{
  if (yytype < YYNTOKENS)
    YYFPRINTF (yyoutput, "token %s (", yytname[yytype]);
  else
    YYFPRINTF (yyoutput, "nterm %s (", yytname[yytype]);

  yy_symbol_value_print (yyoutput, yytype, yyvaluep);
  YYFPRINTF (yyoutput, ")");
}

/*------------------------------------------------------------------.
| yy_stack_print -- Print the state stack from its BOTTOM up to its |
| TOP (included).                                                   |
`------------------------------------------------------------------*/

#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
static void
yy_stack_print (yytype_int16 *bottom, yytype_int16 *top)
#else
static void
yy_stack_print (bottom, top)
    yytype_int16 *bottom;
    yytype_int16 *top;
#endif
{
  YYFPRINTF (stderr, "Stack now");
  for (; bottom <= top; ++bottom)
    YYFPRINTF (stderr, " %d", *bottom);
  YYFPRINTF (stderr, "\n");
}

# define YY_STACK_PRINT(Bottom, Top)				\
do {								\
  if (yydebug)							\
    yy_stack_print ((Bottom), (Top));				\
} while (YYID (0))


/*------------------------------------------------.
| Report that the YYRULE is going to be reduced.  |
`------------------------------------------------*/

#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
static void
yy_reduce_print (YYSTYPE *yyvsp, int yyrule)
#else
static void
yy_reduce_print (yyvsp, yyrule)
    YYSTYPE *yyvsp;
    int yyrule;
#endif
{
  int yynrhs = yyr2[yyrule];
  int yyi;
  unsigned long int yylno = yyrline[yyrule];
  YYFPRINTF (stderr, "Reducing stack by rule %d (line %lu):\n",
	     yyrule - 1, yylno);
  /* The symbols being reduced.  */
  for (yyi = 0; yyi < yynrhs; yyi++)
    {
      fprintf (stderr, "   $%d = ", yyi + 1);
      yy_symbol_print (stderr, yyrhs[yyprhs[yyrule] + yyi],
		       &(yyvsp[(yyi + 1) - (yynrhs)])
		       		       );
      fprintf (stderr, "\n");
    }
}

# define YY_REDUCE_PRINT(Rule)		\
do {					\
  if (yydebug)				\
    yy_reduce_print (yyvsp, Rule); \
} while (YYID (0))

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
#ifndef	YYINITDEPTH
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
#   define yystrlen strlen
#  else
/* Return the length of YYSTR.  */
#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
static YYSIZE_T
yystrlen (const char *yystr)
#else
static YYSIZE_T
yystrlen (yystr)
    const char *yystr;
#endif
{
  YYSIZE_T yylen;
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
#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
static char *
yystpcpy (char *yydest, const char *yysrc)
#else
static char *
yystpcpy (yydest, yysrc)
    char *yydest;
    const char *yysrc;
#endif
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
static YYSIZE_T
yytnamerr (char *yyres, const char *yystr)
{
  if (*yystr == '"')
    {
      YYSIZE_T yyn = 0;
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
	    /* Fall through.  */
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

  if (! yyres)
    return yystrlen (yystr);

  return yystpcpy (yyres, yystr) - yyres;
}
# endif

/* Copy into YYRESULT an error message about the unexpected token
   YYCHAR while in state YYSTATE.  Return the number of bytes copied,
   including the terminating null byte.  If YYRESULT is null, do not
   copy anything; just return the number of bytes that would be
   copied.  As a special case, return 0 if an ordinary "syntax error"
   message will do.  Return YYSIZE_MAXIMUM if overflow occurs during
   size calculation.  */
static YYSIZE_T
yysyntax_error (char *yyresult, int yystate, int yychar)
{
  int yyn = yypact[yystate];

  if (! (YYPACT_NINF < yyn && yyn <= YYLAST))
    return 0;
  else
    {
      int yytype = YYTRANSLATE (yychar);
      YYSIZE_T yysize0 = yytnamerr (0, yytname[yytype]);
      YYSIZE_T yysize = yysize0;
      YYSIZE_T yysize1;
      int yysize_overflow = 0;
      enum { YYERROR_VERBOSE_ARGS_MAXIMUM = 5 };
      char const *yyarg[YYERROR_VERBOSE_ARGS_MAXIMUM];
      int yyx;

# if 0
      /* This is so xgettext sees the translatable formats that are
	 constructed on the fly.  */
      YY_("syntax error, unexpected %s");
      YY_("syntax error, unexpected %s, expecting %s");
      YY_("syntax error, unexpected %s, expecting %s or %s");
      YY_("syntax error, unexpected %s, expecting %s or %s or %s");
      YY_("syntax error, unexpected %s, expecting %s or %s or %s or %s");
# endif
      char *yyfmt;
      char const *yyf;
      static char const yyunexpected[] = "syntax error, unexpected %s";
      static char const yyexpecting[] = ", expecting %s";
      static char const yyor[] = " or %s";
      char yyformat[sizeof yyunexpected
		    + sizeof yyexpecting - 1
		    + ((YYERROR_VERBOSE_ARGS_MAXIMUM - 2)
		       * (sizeof yyor - 1))];
      char const *yyprefix = yyexpecting;

      /* Start YYX at -YYN if negative to avoid negative indexes in
	 YYCHECK.  */
      int yyxbegin = yyn < 0 ? -yyn : 0;

      /* Stay within bounds of both yycheck and yytname.  */
      int yychecklim = YYLAST - yyn + 1;
      int yyxend = yychecklim < YYNTOKENS ? yychecklim : YYNTOKENS;
      int yycount = 1;

      yyarg[0] = yytname[yytype];
      yyfmt = yystpcpy (yyformat, yyunexpected);

      for (yyx = yyxbegin; yyx < yyxend; ++yyx)
	if (yycheck[yyx + yyn] == yyx && yyx != YYTERROR)
	  {
	    if (yycount == YYERROR_VERBOSE_ARGS_MAXIMUM)
	      {
		yycount = 1;
		yysize = yysize0;
		yyformat[sizeof yyunexpected - 1] = '\0';
		break;
	      }
	    yyarg[yycount++] = yytname[yyx];
	    yysize1 = yysize + yytnamerr (0, yytname[yyx]);
	    yysize_overflow |= (yysize1 < yysize);
	    yysize = yysize1;
	    yyfmt = yystpcpy (yyfmt, yyprefix);
	    yyprefix = yyor;
	  }

      yyf = YY_(yyformat);
      yysize1 = yysize + yystrlen (yyf);
      yysize_overflow |= (yysize1 < yysize);
      yysize = yysize1;

      if (yysize_overflow)
	return YYSIZE_MAXIMUM;

      if (yyresult)
	{
	  /* Avoid sprintf, as that infringes on the user's name space.
	     Don't have undefined behavior even if the translation
	     produced a string with the wrong number of "%s"s.  */
	  char *yyp = yyresult;
	  int yyi = 0;
	  while ((*yyp = *yyf) != '\0')
	    {
	      if (*yyp == '%' && yyf[1] == 's' && yyi < yycount)
		{
		  yyp += yytnamerr (yyp, yyarg[yyi++]);
		  yyf += 2;
		}
	      else
		{
		  yyp++;
		  yyf++;
		}
	    }
	}
      return yysize;
    }
}
#endif /* YYERROR_VERBOSE */


/*-----------------------------------------------.
| Release the memory associated to this symbol.  |
`-----------------------------------------------*/

/*ARGSUSED*/
#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
static void
yydestruct (const char *yymsg, int yytype, YYSTYPE *yyvaluep)
#else
static void
yydestruct (yymsg, yytype, yyvaluep)
    const char *yymsg;
    int yytype;
    YYSTYPE *yyvaluep;
#endif
{
  YYUSE (yyvaluep);

  if (!yymsg)
    yymsg = "Deleting";
  YY_SYMBOL_PRINT (yymsg, yytype, yyvaluep, yylocationp);

  switch (yytype)
    {

      default:
	break;
    }
}


/* Prevent warnings from -Wmissing-prototypes.  */

#ifdef YYPARSE_PARAM
#if defined __STDC__ || defined __cplusplus
int yyparse (void *YYPARSE_PARAM);
#else
int yyparse ();
#endif
#else /* ! YYPARSE_PARAM */
#if defined __STDC__ || defined __cplusplus
int yyparse (void);
#else
int yyparse ();
#endif
#endif /* ! YYPARSE_PARAM */



/* The look-ahead symbol.  */
int yychar;

/* The semantic value of the look-ahead symbol.  */
YYSTYPE yylval;

/* Number of syntax errors so far.  */
int yynerrs;



/*----------.
| yyparse.  |
`----------*/

#ifdef YYPARSE_PARAM
#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
int
yyparse (void *YYPARSE_PARAM)
#else
int
yyparse (YYPARSE_PARAM)
    void *YYPARSE_PARAM;
#endif
#else /* ! YYPARSE_PARAM */
#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
int
yyparse (void)
#else
int
yyparse ()

#endif
#endif
{
  
  int yystate;
  int yyn;
  int yyresult;
  /* Number of tokens to shift before error messages enabled.  */
  int yyerrstatus;
  /* Look-ahead token as an internal (translated) token number.  */
  int yytoken = 0;
#if YYERROR_VERBOSE
  /* Buffer for error messages, and its allocated size.  */
  char yymsgbuf[128];
  char *yymsg = yymsgbuf;
  YYSIZE_T yymsg_alloc = sizeof yymsgbuf;
#endif

  /* Three stacks and their tools:
     `yyss': related to states,
     `yyvs': related to semantic values,
     `yyls': related to locations.

     Refer to the stacks thru separate pointers, to allow yyoverflow
     to reallocate them elsewhere.  */

  /* The state stack.  */
  yytype_int16 yyssa[YYINITDEPTH];
  yytype_int16 *yyss = yyssa;
  yytype_int16 *yyssp;

  /* The semantic value stack.  */
  YYSTYPE yyvsa[YYINITDEPTH];
  YYSTYPE *yyvs = yyvsa;
  YYSTYPE *yyvsp;



#define YYPOPSTACK(N)   (yyvsp -= (N), yyssp -= (N))

  YYSIZE_T yystacksize = YYINITDEPTH;

  /* The variables used to return semantic value and location from the
     action routines.  */
  YYSTYPE yyval;


  /* The number of symbols on the RHS of the reduced rule.
     Keep to zero when no symbol should be popped.  */
  int yylen = 0;

  YYDPRINTF ((stderr, "Starting parse\n"));

  yystate = 0;
  yyerrstatus = 0;
  yynerrs = 0;
  yychar = YYEMPTY;		/* Cause a token to be read.  */

  /* Initialize stack pointers.
     Waste one element of value and location stack
     so that they stay on the same level as the state stack.
     The wasted elements are never initialized.  */

  yyssp = yyss;
  yyvsp = yyvs;

  goto yysetstate;

/*------------------------------------------------------------.
| yynewstate -- Push a new state, which is found in yystate.  |
`------------------------------------------------------------*/
 yynewstate:
  /* In all cases, when you get here, the value and location stacks
     have just been pushed.  So pushing a state here evens the stacks.  */
  yyssp++;

 yysetstate:
  *yyssp = yystate;

  if (yyss + yystacksize - 1 <= yyssp)
    {
      /* Get the current used size of the three stacks, in elements.  */
      YYSIZE_T yysize = yyssp - yyss + 1;

#ifdef yyoverflow
      {
	/* Give user a chance to reallocate the stack.  Use copies of
	   these so that the &'s don't force the real ones into
	   memory.  */
	YYSTYPE *yyvs1 = yyvs;
	yytype_int16 *yyss1 = yyss;


	/* Each stack pointer address is followed by the size of the
	   data in use in that stack, in bytes.  This used to be a
	   conditional around just the two extra args, but that might
	   be undefined if yyoverflow is a macro.  */
	yyoverflow (YY_("memory exhausted"),
		    &yyss1, yysize * sizeof (*yyssp),
		    &yyvs1, yysize * sizeof (*yyvsp),

		    &yystacksize);

	yyss = yyss1;
	yyvs = yyvs1;
      }
#else /* no yyoverflow */
# ifndef YYSTACK_RELOCATE
      goto yyexhaustedlab;
# else
      /* Extend the stack our own way.  */
      if (YYMAXDEPTH <= yystacksize)
	goto yyexhaustedlab;
      yystacksize *= 2;
      if (YYMAXDEPTH < yystacksize)
	yystacksize = YYMAXDEPTH;

      {
	yytype_int16 *yyss1 = yyss;
	union yyalloc *yyptr =
	  (union yyalloc *) YYSTACK_ALLOC (YYSTACK_BYTES (yystacksize));
	if (! yyptr)
	  goto yyexhaustedlab;
	YYSTACK_RELOCATE (yyss);
	YYSTACK_RELOCATE (yyvs);

#  undef YYSTACK_RELOCATE
	if (yyss1 != yyssa)
	  YYSTACK_FREE (yyss1);
      }
# endif
#endif /* no yyoverflow */

      yyssp = yyss + yysize - 1;
      yyvsp = yyvs + yysize - 1;


      YYDPRINTF ((stderr, "Stack size increased to %lu\n",
		  (unsigned long int) yystacksize));

      if (yyss + yystacksize - 1 <= yyssp)
	YYABORT;
    }

  YYDPRINTF ((stderr, "Entering state %d\n", yystate));

  goto yybackup;

/*-----------.
| yybackup.  |
`-----------*/
yybackup:

  /* Do appropriate processing given the current state.  Read a
     look-ahead token if we need one and don't already have one.  */

  /* First try to decide what to do without reference to look-ahead token.  */
  yyn = yypact[yystate];
  if (yyn == YYPACT_NINF)
    goto yydefault;

  /* Not known => get a look-ahead token if don't already have one.  */

  /* YYCHAR is either YYEMPTY or YYEOF or a valid look-ahead symbol.  */
  if (yychar == YYEMPTY)
    {
      YYDPRINTF ((stderr, "Reading a token: "));
      yychar = YYLEX;
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
      if (yyn == 0 || yyn == YYTABLE_NINF)
	goto yyerrlab;
      yyn = -yyn;
      goto yyreduce;
    }

  if (yyn == YYFINAL)
    YYACCEPT;

  /* Count tokens shifted since error; after three, turn off error
     status.  */
  if (yyerrstatus)
    yyerrstatus--;

  /* Shift the look-ahead token.  */
  YY_SYMBOL_PRINT ("Shifting", yytoken, &yylval, &yylloc);

  /* Discard the shifted token unless it is eof.  */
  if (yychar != YYEOF)
    yychar = YYEMPTY;

  yystate = yyn;
  *++yyvsp = yylval;

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
| yyreduce -- Do a reduction.  |
`-----------------------------*/
yyreduce:
  /* yyn is the number of a rule to reduce with.  */
  yylen = yyr2[yyn];

  /* If YYLEN is nonzero, implement the default value of the action:
     `$$ = $1'.

     Otherwise, the following line sets YYVAL to garbage.
     This behavior is undocumented and Bison
     users should not rely upon it.  Assigning to YYVAL
     unconditionally makes the parser a bit smaller, and it avoids a
     GCC warning that YYVAL may be used uninitialized.  */
  yyval = yyvsp[1-yylen];


  YY_REDUCE_PRINT (yyn);
  switch (yyn)
    {
        case 3:
#line 74 "cypher.y"
    { printf("Exiting program...\n"); exit(EXIT_SUCCESS); ;}
    break;

  case 4:
#line 76 "cypher.y"
    {
          reset_values();
      ;}
    break;

  case 5:
#line 79 "cypher.y"
    { printf("Exiting program...\n"); exit(EXIT_SUCCESS); ;}
    break;

  case 10:
#line 90 "cypher.y"
    { match = true; if (match == false) {printf("\nmatch = %s\n", match ? "true" : "false");} ;}
    break;

  case 14:
#line 100 "cypher.y"
    {
        printf("node {\n\talias = %s\n\tlabels = %s\n", (yyvsp[(2) - (3)].str_val), (yyvsp[(3) - (3)].str_val));
    ;}
    break;

  case 15:
#line 103 "cypher.y"
    { printf("}\n"); ;}
    break;

  case 16:
#line 107 "cypher.y"
    { (yyval.str_val) = NULL; ;}
    break;

  case 17:
#line 108 "cypher.y"
    { (yyval.str_val) = (yyvsp[(1) - (1)].str_val); ;}
    break;

  case 18:
#line 112 "cypher.y"
    { (yyval.str_val) = NULL; ;}
    break;

  case 19:
#line 113 "cypher.y"
    { (yyval.str_val) = (yyvsp[(2) - (2)].str_val); ;}
    break;

  case 20:
#line 117 "cypher.y"
    { (yyval.pair) = NULL; ;}
    break;

  case 21:
#line 118 "cypher.y"
    { (yyval.pair) = (yyvsp[(2) - (3)].pair); ;}
    break;

  case 22:
#line 122 "cypher.y"
    { rel_direction = 0; printf("dir = %d\n}\n", rel_direction); ;}
    break;

  case 23:
#line 123 "cypher.y"
    { rel_direction = -1; printf("dir = %d\n}\n", rel_direction); ;}
    break;

  case 24:
#line 124 "cypher.y"
    { rel_direction = 1; printf("dir = %d\n}\n", rel_direction); ;}
    break;

  case 25:
#line 129 "cypher.y"
    { 
        printf("rel {\n\talias = %s\n\tlabels = %s\n\tedges = {", (yyvsp[(2) - (5)].str_val), (yyvsp[(3) - (5)].str_val));
        if ((yyvsp[(4) - (5)].pat) != NULL)
            printf("%d, %s, %d", (yyvsp[(4) - (5)].pat)->lower, (yyvsp[(4) - (5)].pat)->dot ? "true" : "false", (yyvsp[(4) - (5)].pat)->upper);
        printf("}\n\t");
    ;}
    break;

  case 26:
#line 138 "cypher.y"
    { (yyval.str_val) = NULL; ;}
    break;

  case 27:
#line 139 "cypher.y"
    { (yyval.str_val) = (yyvsp[(1) - (1)].str_val); ;}
    break;

  case 28:
#line 143 "cypher.y"
    { (yyval.str_val) = NULL; ;}
    break;

  case 29:
#line 144 "cypher.y"
    { (yyval.str_val) = (yyvsp[(2) - (2)].str_val); ;}
    break;

  case 30:
#line 148 "cypher.y"
    { (yyval.pat) = NULL; ;}
    break;

  case 31:
#line 149 "cypher.y"
    { (yyval.pat) = (yyvsp[(2) - (2)].pat); ;}
    break;

  case 32:
#line 153 "cypher.y"
    { (yyval.pat) = NULL; ;}
    break;

  case 33:
#line 154 "cypher.y"
    { (yyval.pat)->lower = (yyvsp[(1) - (3)].int_val); (yyval.pat)->dot = (yyvsp[(2) - (3)].bool_val); (yyval.pat)->upper = (yyvsp[(3) - (3)].int_val); ;}
    break;

  case 34:
#line 158 "cypher.y"
    { (yyval.bool_val) = false; ;}
    break;

  case 35:
#line 159 "cypher.y"
    { (yyval.bool_val) = true; ;}
    break;

  case 36:
#line 163 "cypher.y"
    { (yyval.int_val) = 0; ;}
    break;

  case 37:
#line 164 "cypher.y"
    { (yyval.int_val) = (yyvsp[(1) - (1)].int_val); ;}
    break;

  case 38:
#line 168 "cypher.y"
    { (yyval.pair) = NULL; ;}
    break;

  case 39:
#line 169 "cypher.y"
    { (yyval.pair) = (yyvsp[(1) - (1)].pair); ;}
    break;

  case 42:
#line 178 "cypher.y"
    { printf("\tproperty {\n\t\tidentifier = %s\n\t\texpression = %s\n\t}\n", (yyvsp[(1) - (3)].str_val), (yyvsp[(3) - (3)].str_val)); ;}
    break;

  case 43:
#line 182 "cypher.y"
    { sprintf((yyval.str_val), "%d", (yyvsp[(1) - (1)].int_val)); ;}
    break;

  case 44:
#line 183 "cypher.y"
    { (yyval.str_val) = (yyvsp[(1) - (1)].str_val); ;}
    break;

  case 45:
#line 184 "cypher.y"
    { (yyval.str_val) = (yyvsp[(1) - (1)].str_val); ;}
    break;

  case 46:
#line 188 "cypher.y"
    { where = true; printf("\nwhere = %s\n", where ? "true" : "false"); ;}
    break;

  case 48:
#line 192 "cypher.y"
    { printf("item {\n\tnot = %s\n\texpression = {%s, %s, %s}\n}\n", (yyvsp[(1) - (4)].bool_val) ? "true" : "false", (yyvsp[(2) - (4)].str_val), (yyvsp[(3) - (4)].str_val), (yyvsp[(4) - (4)].str_val)); ;}
    break;

  case 49:
#line 193 "cypher.y"
    { printf("item {\n\tlogic = %s\n\tnot = %s\n\texpression = %s %s %s\n}\n", (yyvsp[(2) - (6)].str_val), (yyvsp[(3) - (6)].bool_val) ? "true" : "false", (yyvsp[(4) - (6)].str_val), (yyvsp[(5) - (6)].str_val), (yyvsp[(6) - (6)].str_val)); ;}
    break;

  case 50:
#line 197 "cypher.y"
    { (yyval.bool_val) = 0; ;}
    break;

  case 51:
#line 198 "cypher.y"
    { (yyval.bool_val) = 1; ;}
    break;

  case 52:
#line 202 "cypher.y"
    { (yyval.str_val) = (yyvsp[(1) - (1)].str_val); ;}
    break;

  case 53:
#line 203 "cypher.y"
    { (yyval.str_val) = "<"; ;}
    break;

  case 54:
#line 204 "cypher.y"
    { (yyval.str_val) = ">"; ;}
    break;

  case 55:
#line 208 "cypher.y"
    { (yyval.str_val) = "AND"; ;}
    break;

  case 56:
#line 209 "cypher.y"
    { (yyval.str_val) = "OR"; ;}
    break;

  case 57:
#line 210 "cypher.y"
    { (yyval.str_val) = "XOR"; ;}
    break;

  case 58:
#line 214 "cypher.y"
    { with = true; printf("\nwith = %s\n", with ? "true" : "false"); ;}
    break;

  case 60:
#line 218 "cypher.y"
    { rtn = true; printf("\nreturn = %s\n", rtn ? "true" : "false"); ;}
    break;

  case 65:
#line 231 "cypher.y"
    { char* temp = NULL; printf("item {\n\texpression = %s\n\talias = %s\n}\n", (yyvsp[(1) - (1)].str_val), temp); ;}
    break;

  case 66:
#line 232 "cypher.y"
    { printf("item {\n\texpression = %s\n\talias = %s\n}\n", (yyvsp[(1) - (3)].str_val), (yyvsp[(3) - (3)].str_val)); ;}
    break;

  case 71:
#line 246 "cypher.y"
    { order_clause_direction = (yyvsp[(2) - (2)].int_val); printf("sort {\n\texpression = %s\n\tdirection = %d\n}\n", (yyvsp[(1) - (2)].str_val), order_clause_direction); ;}
    break;

  case 72:
#line 250 "cypher.y"
    { (yyval.int_val) = 1; ;}
    break;

  case 73:
#line 251 "cypher.y"
    { (yyval.int_val) = 1; ;}
    break;

  case 74:
#line 252 "cypher.y"
    { (yyval.int_val) = -1; ;}
    break;

  case 76:
#line 257 "cypher.y"
    { printf("skip {\n\tinteger = %d\n}\n", (yyvsp[(2) - (2)].int_val));  ;}
    break;

  case 78:
#line 262 "cypher.y"
    { printf("limit {\n\tinteger = %d\n}\n", (yyvsp[(2) - (2)].int_val)); ;}
    break;


/* Line 1267 of yacc.c.  */
#line 1832 "cypher.tab.c"
      default: break;
    }
  YY_SYMBOL_PRINT ("-> $$ =", yyr1[yyn], &yyval, &yyloc);

  YYPOPSTACK (yylen);
  yylen = 0;
  YY_STACK_PRINT (yyss, yyssp);

  *++yyvsp = yyval;


  /* Now `shift' the result of the reduction.  Determine what state
     that goes to, based on the state we popped back to and the rule
     number reduced by.  */

  yyn = yyr1[yyn];

  yystate = yypgoto[yyn - YYNTOKENS] + *yyssp;
  if (0 <= yystate && yystate <= YYLAST && yycheck[yystate] == *yyssp)
    yystate = yytable[yystate];
  else
    yystate = yydefgoto[yyn - YYNTOKENS];

  goto yynewstate;


/*------------------------------------.
| yyerrlab -- here on detecting error |
`------------------------------------*/
yyerrlab:
  /* If not already recovering from an error, report this error.  */
  if (!yyerrstatus)
    {
      ++yynerrs;
#if ! YYERROR_VERBOSE
      yyerror (YY_("syntax error"));
#else
      {
	YYSIZE_T yysize = yysyntax_error (0, yystate, yychar);
	if (yymsg_alloc < yysize && yymsg_alloc < YYSTACK_ALLOC_MAXIMUM)
	  {
	    YYSIZE_T yyalloc = 2 * yysize;
	    if (! (yysize <= yyalloc && yyalloc <= YYSTACK_ALLOC_MAXIMUM))
	      yyalloc = YYSTACK_ALLOC_MAXIMUM;
	    if (yymsg != yymsgbuf)
	      YYSTACK_FREE (yymsg);
	    yymsg = (char *) YYSTACK_ALLOC (yyalloc);
	    if (yymsg)
	      yymsg_alloc = yyalloc;
	    else
	      {
		yymsg = yymsgbuf;
		yymsg_alloc = sizeof yymsgbuf;
	      }
	  }

	if (0 < yysize && yysize <= yymsg_alloc)
	  {
	    (void) yysyntax_error (yymsg, yystate, yychar);
	    yyerror (yymsg);
	  }
	else
	  {
	    yyerror (YY_("syntax error"));
	    if (yysize != 0)
	      goto yyexhaustedlab;
	  }
      }
#endif
    }



  if (yyerrstatus == 3)
    {
      /* If just tried and failed to reuse look-ahead token after an
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
		      yytoken, &yylval);
	  yychar = YYEMPTY;
	}
    }

  /* Else will try to reuse look-ahead token after shifting the error
     token.  */
  goto yyerrlab1;


/*---------------------------------------------------.
| yyerrorlab -- error raised explicitly by YYERROR.  |
`---------------------------------------------------*/
yyerrorlab:

  /* Pacify compilers like GCC when the user code never invokes
     YYERROR and the label yyerrorlab therefore never appears in user
     code.  */
  if (/*CONSTCOND*/ 0)
     goto yyerrorlab;

  /* Do not reclaim the symbols of the rule which action triggered
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
  yyerrstatus = 3;	/* Each real token shifted decrements this.  */

  for (;;)
    {
      yyn = yypact[yystate];
      if (yyn != YYPACT_NINF)
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


      yydestruct ("Error: popping",
		  yystos[yystate], yyvsp);
      YYPOPSTACK (1);
      yystate = *yyssp;
      YY_STACK_PRINT (yyss, yyssp);
    }

  if (yyn == YYFINAL)
    YYACCEPT;

  *++yyvsp = yylval;


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

#ifndef yyoverflow
/*-------------------------------------------------.
| yyexhaustedlab -- memory exhaustion comes here.  |
`-------------------------------------------------*/
yyexhaustedlab:
  yyerror (YY_("memory exhausted"));
  yyresult = 2;
  /* Fall through.  */
#endif

yyreturn:
  if (yychar != YYEOF && yychar != YYEMPTY)
     yydestruct ("Cleanup: discarding lookahead",
		 yytoken, &yylval);
  /* Do not reclaim the symbols of the rule which action triggered
     this YYABORT or YYACCEPT.  */
  YYPOPSTACK (yylen);
  YY_STACK_PRINT (yyss, yyssp);
  while (yyssp != yyss)
    {
      yydestruct ("Cleanup: popping",
		  yystos[*yyssp], yyvsp);
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
  /* Make sure YYID is used.  */
  return YYID (yyresult);
}


#line 269 "cypher.y"


void yyerror(char const *s)
{
    printf("Parser error: %s\n", s);
}

void psql_scan_cypher_command(PsqlScanState state)
{
    yyparse();
}

void reset_values()
{
    match = false;
    where = false;
    with = false;
    rtn = false;
}

