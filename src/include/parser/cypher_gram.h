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

/*
 * NOTE: this file is only meant to be included in the Cypher parsing files,
 *       i.e., cypher_gram.y, cypher_keywords.c, and cypher_parser.c.
 *       Definitions that are needed outside the Cypher parser should be in
 *       cypher_parser.h.
 */

#ifndef AG_CYPHER_GRAM_H
#define AG_CYPHER_GRAM_H

#include "nodes/pg_list.h"

#include "parser/ag_scanner.h"

// override the default data type of locations (yylloc)
#define YYLTYPE int

/*
 * Extra data that is passed to the generated parser.
 * The resulting abstract syntax tree is stored in result field.
 */
typedef struct cypher_yy_extra
{
    List *result;
    /*
     * This node currently holds the EXPLAIN ExplainStmt node. It is generic in
     * the event we need to allow more than just EXPLAIN to be passed up.
     */
    Node *extra;
} cypher_yy_extra;

/*
 * cypher_gram_def.h is generated by Bison.
 *
 * nodes/nodes.h and nodes/pg_list.h must be included before this due to the
 * use of Node and List in YYSTYPE.
 *
 * ag_scanner.h must be included before this because ag_scanner_t is one of the
 * parameters of cypher_yyparse() which is generated by Bison.
 *
 * YYLTYPE must be defined before this. (see above)
 *
 * cypher_yy_extra must be defined before this because it is another parameter
 * of cypher_yyparse().
 */
#include "parser/cypher_gram_def.h"

// cypher_parser.c
int cypher_yylex(YYSTYPE *lvalp, YYLTYPE *llocp, ag_scanner_t scanner);
void cypher_yyerror(YYLTYPE *llocp, ag_scanner_t scanner,
                    cypher_yy_extra *extra, const char *msg);

#endif
