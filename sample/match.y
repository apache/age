%{
#include <stdio.h>
#include <stdlib.h>

int yylex(void);
void yyerror(const char*);

char u;
%}

%token MATCH RETURN DASH LPAREN RPAREN LBRACKET RBRACKET VARIABLE

%%

statement : match_clause RETURN_VARIABLE { printf("Match statement parsed successfully.\n"); }
          ;

match_clause : MATCH pattern { printf("Match clause parsed successfully.\n"); }
             ;

pattern : LPAREN VARIABLE RPAREN DASH LBRACKET VARIABLE RBRACKET DASH LPAREN VARIABLE RPAREN
        { printf("Pattern parsed successfully.\n"); }
        ;

RETURN_VARIABLE : VARIABLE { u = $1; }
                ;

%%


void yyerror(const char *s) {
    fprintf(stderr, "Parse error: %s\n", s);
    exit(1);
}

int main(void) {
    yyparse();
    printf("Parsed variable: %c\n", u);
    return 0;
}

