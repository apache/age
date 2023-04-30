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

int order_clause_direction = 1; // 1 for ascending, -1 for descending
%}

%union {
    char* str_val;
    int int_val;
}

%token ASC ARROW AS DESC LBRACKET RBRACKET LPAREN RPAREN COLON PIPE COMMA SEMICOLON LBRACE RBRACE MATCH WHERE WITH ORDER BY SKIP LIMIT RETURN
%token <int_val> INTEGER
%token <str_val> IDENTIFIER STRING
%token UNKNOWN

%left PIPE
%left ARROW

%start statement

%%

statement:
    { printf("Query parsed successfully.\n"); }
    ;

query:
    match_clause
    //where_clause_opt
    //with_clause_opt
    //return_clause
    ;

match_clause:
    MATCH path_pattern { printf("Match clause parsed successfully.\n"); }
	;    

path_pattern:
    node_pattern
    | node_pattern ARROW rel_pattern node_pattern { printf("Path pattern parsed successfully.\n"); }
    ;

node_pattern:
    LPAREN node_labels_opt node_properties_opt RPAREN { printf("Node pattern parsed successfully.\n"); }
    ;

node_labels_opt:
    /* empty */ { printf("No node labels.\n"); }
    | COLON IDENTIFIER { printf("Node label parsed: %s.\n", $2); }
    | node_labels_opt COLON IDENTIFIER { printf("Node label parsed: %s.\n", $3); }
    ;

node_properties_opt:
    /* empty */ { printf("No node properties.\n"); }
    | LBRACE map_literal RBRACE { printf("Node properties parsed successfully.\n"); }
    ;

rel_pattern:
    rel_type rel_direction rel_type { printf("Rel pattern parsed successfully.\n"); }
    ;

rel_type:
    { printf("Rel type parsed: \n"); }
    ;

//rel_type:
//    COLON str_val { printf("Rel type parsed: %s.\n", $2); }
//     | LBRACKET str_val RBRACKET { printf("Rel type parsed: %s.\n", $2); }
//    ;


rel_direction:
    {printf("Rel direction parsed: ->.\n"); }
    ;

//rel_direction:
//    ARROW { printf("Rel direction parsed: ->.\n"); }
//    | ARROW rel_type_name ARROW { printf("Rel direction parsed: ->%s->.\n", $2); }
//    ;

map_literal:
    /* empty */ { printf("Empty map literal.\n"); }
    | nonempty_map_literal { printf("Nonempty map literal.\n"); }
    ;

nonempty_map_literal:
    map_entry { printf("Map literal entry parsed successfully.\n"); }
    | nonempty_map_literal COMMA map_entry { printf("Map literal entry parsed successfully.\n"); }
    ;

map_entry:
    IDENTIFIER COLON expression { printf("Map entry parsed successfully.\n"); }
    ;

expression:
    INTEGER { printf("Integer expression parsed: %d.\n", $1); }
    | STRING { printf("String expression parsed: %s.\n", $1); }
    | IDENTIFIER { printf("Identifier expression parsed: %s.\n", $1); }
    ;

where_clause_opt:
    /* empty */ { printf("No WHERE clause.\n"); }
    | WHERE expression { printf("WHERE clause parsed successfully.\n"); }
    ;

with_clause_opt:
    /* empty */ { printf("No WITH clause.\n"); }
    | WITH expression_list return_clause { printf("WITH clause parsed successfully.\n"); }

expression_list:
	expression { printf("Expression parsed successfully.\n"); }
	| expression_list COMMA expression { printf("Expression parsed successfully.\n"); }
	;

return_clause:
	RETURN return_item_list order_clause_opt skip_clause_opt limit_clause_opt { printf("Return clause parsed successfully.\n"); }
;

return_item_list:
	return_item { printf("Return item parsed successfully.\n"); }
	| return_item_list COMMA return_item { printf("Return item parsed successfully.\n"); }
	;

return_item:
	expression { printf("Return item parsed successfully.\n"); }
	| expression AS IDENTIFIER { printf("Return item with alias parsed successfully.\n"); }
	;

order_clause_opt:
	/* empty */ { printf("No ORDER BY clause.\n"); }
	| ORDER BY sort_item_list { printf("ORDER BY clause parsed successfully.\n"); }
	;

sort_item_list:
sort_item { printf("Sort item parsed successfully.\n"); }
| sort_item_list COMMA sort_item { printf("Sort item parsed successfully.\n"); }
;

sort_item:
expression sort_direction_opt { printf("Sort item parsed successfully.\n"); }
;

sort_direction_opt:
	/* empty */ { printf("Sort direction not specified; defaulting to ASC.\n"); order_clause_direction = 1; }
	| ASC { printf("Sort direction specified: ASC.\n"); order_clause_direction = 1; }
	| DESC { printf("Sort direction specified: DESC.\n"); order_clause_direction = -1; }
	;

skip_clause_opt:
	/* empty */ { printf("No SKIP clause.\n"); }
	| SKIP INTEGER { printf("SKIP clause parsed: %d.\n", $2); }
	;

limit_clause_opt:
	/* empty */ { printf("No LIMIT clause.\n"); }
	| LIMIT INTEGER { printf("LIMIT clause parsed: %d.\n", $2); }
	;

%%

void yyerror(char const *s)
{
	printf("Parser error: %s\n", s);
}

void
psql_scan_cypher_command(PsqlScanState state)
{

}
