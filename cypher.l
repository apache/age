%option noyywrap
%{
#include "postgres_fe.h"                                                        
                                                                                
#include "psqlscanslash.h"                                                      
#include "common/logging.h"                                                     
#include "fe_utils/conditional.h"                                               
                                                                                
#include "libpq-fe.h"  
#include "cypherscan.h"
#include "cypher.tab.h"
%}

%%

"->" { return ARROW; }
"[" { return LBRACKET; }
"]" { return RBRACKET; }
"(" { return LPAREN; }
")" { return RPAREN; }
":" { return COLON; }
"|" { return PIPE; }
"," { return COMMA; }
";" { return SEMICOLON; }
"{" { return LBRACE; }
"}" { return RBRACE; }

"MATCH" { return MATCH; }
"WHERE" { return WHERE; }
"WITH" { return WITH; }
"ORDER" { return ORDER; }
"BY" { return BY; }
"SKIP" { return SKIP; }
"LIMIT" { return LIMIT; }
"RETURN" { return RETURN; }
"AS" { return AS; }

[0-9]+ { yylval.int_val = atoi(yytext); return INTEGER; }
[a-zA-Z][a-zA-Z0-9_]* { yylval.str_val = strdup(yytext); return IDENTIFIER; }
"([^\"]|\.)*" { yylval.str_val = strdup(yytext); return STRING; }
. { return UNKNOWN; }

%%

