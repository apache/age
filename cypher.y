%{
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
%}

%union {
    char* str_val;
    int int_val;
    bool bool_val;
    struct edge_pattern* pat;
    struct map_pair* pair;
}

%type <str_val> node_alias_opt node_labels_opt node_pattern expression rel_alias_opt rel_labels_opt compare logic
%type <int_val> upper_bound_opt sort_direction_opt
%type <bool_val> dot_opt not_opt
%type <pat> rel_pattern variable_length_edges_opt edge_length_opt
%type <pair> node_properties_opt map_literal nonempty_map_literal map_entry item_list item

%token ASC DESC DASH LT GT LBRACKET RBRACKET LPAREN RPAREN COLON PIPE COMMA SEMICOLON LBRACE RBRACE ASTERISK DOT MATCH WHERE WITH ORDER BY SKIP LIMIT RETURN AS AND OR XOR NOT exit_command
%token <int_val> INTEGER
%token <str_val> IDENTIFIER STRING COMPARATOR
%token UNKNOWN

%left PIPE
%left ARROW

%start statement

%%
statement:
    query
    | exit_command { printf("Exiting program...\n"); exit(EXIT_SUCCESS); }
    | statement query
      {
          reset_values();
      }
    | statement exit_command { printf("Exiting program...\n"); exit(EXIT_SUCCESS); }
    ;

query:
    match_clause
    | where_clause
    | with_clause
    | return_clause
    ;

match_clause:
    MATCH { match = true; if (match == false) {printf("\nmatch = %s\n", match ? "true" : "false");} } path_pattern semicolon_opt
    ;    

path_pattern:
    node_pattern
    | path_pattern edge_pattern node_pattern
    ;

node_pattern:
    LPAREN node_alias_opt node_labels_opt
    {
        printf("node {\n\talias = %s\n\tlabels = %s\n", $2, $3);
    }
    node_properties_opt RPAREN { printf("}\n"); }
    ;

node_alias_opt:
    /* empty */ { $$ = NULL; }
    | IDENTIFIER { $$ = $1; }
    ;

node_labels_opt:
    /* empty */ { $$ = NULL; }
    | COLON IDENTIFIER { $$ = $2; }
    ;

node_properties_opt:
    /* empty */ { $$ = NULL; }
    | LBRACE map_literal RBRACE { $$ = $2; }
    ;

edge_pattern:
    DASH rel_pattern DASH { rel_direction = 0; printf("dir = %d\n}\n", rel_direction); }
    | LT DASH rel_pattern DASH { rel_direction = -1; printf("dir = %d\n}\n", rel_direction); }
    | DASH rel_pattern DASH GT { rel_direction = 1; printf("dir = %d\n}\n", rel_direction); }
    ;

rel_pattern:
    LBRACKET rel_alias_opt rel_labels_opt variable_length_edges_opt RBRACKET
    { 
        printf("rel {\n\talias = %s\n\tlabels = %s\n\tedges = {", $2, $3);
        if ($4 != NULL)
            printf("%d, %s, %d", $4->lower, $4->dot ? "true" : "false", $4->upper);
        printf("}\n\t");
    }
    ;

rel_alias_opt:
    /* empty */ { $$ = NULL; }
    | IDENTIFIER { $$ = $1; }
    ;

rel_labels_opt:
    /* empty */ { $$ = NULL; }
    | COLON IDENTIFIER { $$ = $2; }
    ;

variable_length_edges_opt:
    /* empty */ { $$ = NULL; }
    | ASTERISK edge_length_opt { $$ = $2; }
    ;

edge_length_opt:
    /* empty */ { $$ = NULL; }
    | INTEGER dot_opt upper_bound_opt { $$->lower = $1; $$->dot = $2; $$->upper = $3; }
    ;

dot_opt:
    /* empty */ { $$ = false; } 
    | DOT DOT { $$ = true; }
    ;

upper_bound_opt:
    /* empty */ { $$ = 0; }
    | INTEGER { $$ = $1; }
    ;

map_literal:
    /* empty */ { $$ = NULL; }
    | nonempty_map_literal { $$ = $1; }
    ;

nonempty_map_literal:
    map_entry
    | nonempty_map_literal COMMA map_entry
    ;

map_entry:
    IDENTIFIER COLON expression { printf("\tproperty {\n\t\tidentifier = %s\n\t\texpression = %s\n\t}\n", $1, $3); }
    ;

expression:
    INTEGER { sprintf($$, "%d", $1); }
    | STRING { $$ = $1; }
    | IDENTIFIER { $$ = $1; }
    ;

where_clause:
    WHERE { where = true; printf("\nwhere = %s\n", where ? "true" : "false"); } where_expression
    ;

where_expression:
    not_opt expression compare expression { printf("item {\n\tnot = %s\n\texpression = {%s, %s, %s}\n}\n", $1 ? "true" : "false", $2, $3, $4); }
    | where_expression logic not_opt expression compare expression { printf("item {\n\tlogic = %s\n\tnot = %s\n\texpression = {%s, %s, %s}\n}\n", $2, $3 ? "true" : "false", $4, $5, $6); }
    ;

not_opt:
    /* empty */ { $$ = 0; }
    | NOT { $$ = 1; }
    ;

compare:
    COMPARATOR { $$ = $1; }
    | LT { $$ = "<"; }
    | GT { $$ = ">"; }
    ;

logic:
    AND { $$ = "AND"; }
    | OR { $$ = "OR"; }
    | XOR { $$ = "XOR"; }
    ;

with_clause:
    WITH { with = true; printf("\nwith = %s\n", with ? "true" : "false"); } item_clause
    ;

return_clause:
    RETURN { rtn = true; printf("\nreturn = %s\n", rtn ? "true" : "false"); } item_clause SEMICOLON
    ;

item_clause:
    item_list order_clause_opt skip_clause_opt limit_clause_opt
    ;

item_list:
    item
    | item_list COMMA item
    ;

item:
    expression { char* temp = NULL; printf("item {\n\texpression = %s\n\talias = %s\n}\n", $1, temp); }
    | expression AS IDENTIFIER { printf("item {\n\texpression = %s\n\talias = %s\n}\n", $1, $3); }
    ;

order_clause_opt:
    /* empty */
    | ORDER BY sort_item_list
    ;

sort_item_list:
    sort_item
    | sort_item_list COMMA sort_item
    ;

sort_item:
    expression sort_direction_opt { order_clause_direction = $2; printf("sort {\n\texpression = %s\n\tdirection = %d\n}\n", $1, order_clause_direction); }
    ;

sort_direction_opt:
    /* empty */ { $$ = 1; }
    | ASC { $$ = 1; }
    | DESC { $$ = -1; }
    ;

skip_clause_opt:
    /* empty */
    | SKIP INTEGER { printf("skip {\n\tinteger = %d\n}\n", $2);  }
    ;

limit_clause_opt:
    /* empty */
    | LIMIT INTEGER { printf("limit {\n\tinteger = %d\n}\n", $2); }
    ;

semicolon_opt:
    /* empty */
    | SEMICOLON
    ;
%%

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
