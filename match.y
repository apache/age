%{
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


void yyerror(const char *s) 
{
    fprintf(stderr, "Parse error: %s\n", s);
    exit(1);
}

int main(void) 
{
    yyparse();
    printf("Parsed variable: %c\n", u);
    return 0;
}

