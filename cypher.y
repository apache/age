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

typedef struct
{
    char* str_val;
    int int_val;
} yyval;

int yylex(void);

int order_clause_direction = 1; // 1 for ascending, -1 for descending

int match = 0;
char* label1 = "NULL";
char* label2 = "NULL";
char* property = "NULL";
int expression_int = -1;
char* expression_str = "NULL";
char* expression_id = "NULL";
int directed = 0;
char* direction = "NULL";
int where = 0;
int with = 0;
int return_ = 0;
int with_alias = 0;
int order_by = 0;
int skip_value = 0;
int limit_value = 0;
%}

%union
{
    char* str_val;
    int int_val;
}

%token ASC ARROW AS DESC LBRACKET RBRACKET LPAREN RPAREN COLON PIPE COMMA SEMICOLON LBRACE RBRACE MATCH WHERE WITH ORDER BY SKIP LIMIT RETURN
%token <int_val> INTEGER
%token <str_val> IDENTIFIER STRING
%token UNKNOWN

%type <str_val> str_val

%left PIPE
%left ARROW

%start statement

%%

statement:
    query
    ;

query:
    match_clause
    where_clause_opt
    with_clause_opt
    return_clause
    ;

match_clause:
    MATCH path_pattern 
        {
            match = 1;
        }
    ;    

path_pattern:
    node_pattern
    | node_pattern ARROW rel_pattern node_pattern
        {
            /*printf("Path pattern parsed successfully.\n");*/
        }
    ;

node_pattern:
    LPAREN node_labels_opt node_properties_opt RPAREN 
        {
            /*printf("Node pattern parsed successfully.\n");*/
        }
    ;

node_labels_opt:
    /* empty */ 
        { 
            label1 = "NULL"; 
            label2 = "NULL";
        }
    |
      IDENTIFIER
        {
            label1 = $1;
        }
    | COLON IDENTIFIER 
        {
            label1 = $2;
        }
    | node_labels_opt COLON IDENTIFIER 
        {
            label2 = $3;
        }
    ;

node_properties_opt:
    /* empty */ 
        { 
            property = "NULL"; 
        }
    | LBRACE map_literal RBRACE 
        {
            /*printf("Node properties parsed successfully.\n");*/
        }
    ;

str_val:
    IDENTIFIER 
        {
            $$ = $1;
        }
    | STRING 
        {
            $$ = $1;
        }
    ;

rel_pattern:
    rel_type rel_direction rel_type 
        {
            directed = 1;
        }
    ;

rel_type:
    COLON str_val
        {
            /*printf("Rel type parsed: %s.\n", $2);*/
        }
    | LBRACKET str_val RBRACKET 
        {
            /*printf("Rel type parsed: %s.\n", $2);*/
        }
    ;

rel_direction:
    ARROW 
        {
            direction = "->";
        }
    | ARROW str_val ARROW 
        {
            /*printf("Rel direction parsed: ->%s->.\n", $2);*/
        }
    ;

map_literal:
    /* empty */ 
        {
            /*printf("Empty map literal.\n");*/
        }
    | nonempty_map_literal 
        {
            /*printf("Nonempty map literal.\n");*/
        }
    ;

nonempty_map_literal:
    map_entry 
        {
            /*printf("Map literal entry parsed successfully.\n");*/
        }
    | nonempty_map_literal COMMA map_entry 
        {
            /*printf("Map literal entry parsed successfully.\n");*/
        }
    ;

map_entry:
    IDENTIFIER COLON expression 
        {
            property = $1;
        }
    ;

expression:
    INTEGER 
        {
            expression_int = $1;
        }
    | STRING 
        {
            expression_str = $1;
        }
    | IDENTIFIER 
        {
            expression_id = $1;
        }
    ;

where_clause_opt:
    /* empty */ 
        {
            where = 0;
        }
    | WHERE expression 
        { 
            where = 1;
        }
    ;

with_clause_opt:
    /* empty */ 
        {
            with = 0;
        }
    | WITH expression_list return_clause 
        {
            with = 1;
        }
        ;

expression_list:
	expression 
            {
                /*printf("Expression parsed successfully.\n");*/
            }
	| expression_list COMMA expression 
            {
                /*printf("Expression parsed successfully.\n");*/
            }
	;

return_clause:
	RETURN return_item_list order_clause_opt skip_clause_opt limit_clause_opt 
            {
                return_ = 1;
            }
        ;

return_item_list:
	return_item 
            {
                /*printf("Return item parsed successfully.\n");*/
            }
	| return_item_list COMMA return_item 
            {
                /*printf("Return item parsed successfully.\n");*/
            }
	;

return_item:
	expression 
            {
                /*printf("Return item parsed successfully.\n");*/
            }
	| expression AS IDENTIFIER 
            {
                with_alias = 1;
            }
	;

order_clause_opt:
	/* empty */ 
            {
                order_by = 0;
            }
	| ORDER BY sort_item_list 
            {
                order_by = 1;
            }
	;

sort_item_list:
    sort_item 
        {
            /*printf("Sort item parsed successfully.\n");*/
        }
    | sort_item_list COMMA sort_item 
        {
            /*printf("Sort item parsed successfully.\n");*/
        }
    ;

sort_item:
    expression sort_direction_opt 
        {
            /*printf("Sort item parsed successfully.\n");*/
        }
    ;

sort_direction_opt:
    /* empty */ 
        {
            printf("Sort direction not specified; defaulting to ASC.\n");
            order_clause_direction = 1;
        }
    | ASC 
        {
            printf("Sort direction specified: ASC.\n"); 
            order_clause_direction = 1;
        }
    | DESC 
        {
            printf("Sort direction specified: DESC.\n"); 
            order_clause_direction = -1;
        }
    ;

skip_clause_opt:
    /* empty */ {/* printf("No SKIP clause.\n"); */}
    | SKIP INTEGER 
        {
            skip_value = $2;
        }
    ;

limit_clause_opt:
    /* empty */ 
        {
            /* printf("No LIMIT clause.\n"); */
        }
    | LIMIT INTEGER
        {
            limit_value = $2;
        }
    ;

%%

void yyerror(char const *s)
{
	printf("Parser error: %s\n", s);
}

void
psql_scan_cypher_command(PsqlScanState state)
{
    yyparse();
    printf("match = : %d\n", match);
    printf("label1 = : %s\n", label1);
    printf("label2 = : %s\n", label2);
    printf("property = : %s\n", property);
    printf("expression_int = : %d\n", expression_int);
    printf("expression_str = : %s\n", expression_str);
    printf("expression_id = : %s\n", expression_id);
    printf("str_val = : %s\n", yylval.str_val);
    printf("directed = : %d\n", directed);
    printf("direction = : %s\n", direction);
    printf("where = : %d\n", where);
    printf("with = : %d\n", with);
    printf("return = : %d\n", return_);
    printf("with_alias = : %d\n", with_alias);
    printf("order_by = : %d\n", order_by);
    printf("order_clause_direction = : %d\n", order_clause_direction);
    printf("skip_value = : %d\n", skip_value);
    printf("limit_value = : %d\n", limit_value);
}
