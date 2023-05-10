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
[ \t\n] ;
"-" { return DASH; }
"<" { return LT; }
">" { return GT; }
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
"*" { return ASTERISK; }
"." { return DOT; }
"exit" { return exit_command; }

"MATCH" { return MATCH; }
"WHERE" { return WHERE; }
"WITH" { return WITH; }
"ORDER" { return ORDER; }
"BY" { return BY; }
"SKIP" { return SKIP; }
"LIMIT" { return LIMIT; }
"RETURN" { return RETURN; }
"ASC" { return ASC; }
"DESC" { return DESC; }
"AS" { return AS; }
"AND" { return AND; }
"OR" { return OR; }
"XOR" { return XOR; }
"NOT" { return NOT; }

[0-9]+ { yylval.int_val = atoi(yytext); return INTEGER; }
[a-zA-Z][a-zA-Z0-9_.]* { yylval.str_val = strdup(yytext); return IDENTIFIER; }
("\"")[^"]*("\"")|("\'")[^']*("\'") { yylval.str_val = strdup(yytext); return STRING; }
("=")|("!=")|("<=")|(">=")|("<")|(">") { yylval.str_val = strdup(yytext); return COMPARATOR; }
. { return UNKNOWN; }
%%

