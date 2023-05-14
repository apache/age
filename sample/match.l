%{
#include "match.tab.h"
%}

%%
[ \t\n]          /* ignore whitespace */
"MATCH"          { return MATCH; }
"RETURN"         { return RETURN; }
"-"              { return DASH; }
"("              { return LPAREN; }
")"              { return RPAREN; }
"["              { return LBRACKET; }
"]"              { return RBRACKET; }
[uv]             { return VARIABLE; }
%%
int yywrap(void) {
    return 1;
}


