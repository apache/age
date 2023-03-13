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

#include "pool.h"

#include "parser/pg_list.h"
#include "parser/nodes.h"
#include "parser/parsenodes.h"
#include "utils/elog.h"



void
 /*
 * outNode replaces single quote marks (e.g. 'name')
 * with two single quote marks (''name'') . This function
 * replaces opening quotes with a space followed by a " ' "
 * and closing quotes with " ' " followed by a space. Simple
 * replacement is done instead of creating a new string to 
 * preserve memory allocated by outNode. 
 * 
 * Fixing quotes is necessary before sending the
 * string to the cypher parser.
 */
preprocess_cypherstr(char *target)
{
	//cypher parser needs first and last strings to not be single quotes
	target[0] = ' ';
	target[strlen(target)-1] = ' ';

	char *p = strstr(target, "''");
	bool OPEN_QUOTE = true;
	while (p){
		if (OPEN_QUOTE){
			p[0] = ' ';
			p[1] = '\'';
			OPEN_QUOTE = false;
		}
		else{
			p[0] = '\'';
			p[1] = ' ';
			OPEN_QUOTE = true;
		}

		p = strstr(target, "''");
	}
}



bool
/* If the given raw parse tree root node 
*  contains a cypher call, the cypher query
*  inside is stored within str.
*/
isCypherQuery (Node* node, char** retstr)
{
	if (IsA(node, SelectStmt))
	{
		SelectStmt *stmt = (SelectStmt *) node;
		List* fromClause = stmt->fromClause;

		ListCell   *fl;

		//Match the first cypher function call in the FROM clause. Could be multiple tables
		// e.g. FROM table1, table2, cypher(),table3....
		foreach(fl, fromClause)
		{
			Node	   *n = lfirst(fl);
			if (IsA(n, RangeFunction))
			{
				RangeFunction *rf = (RangeFunction*) n;
				List* functions = rf->functions; 

				if (functions->length == 1)
				{
					List *sublist = (List *) lfirst(list_head(functions)); 
					FuncCall *fntree = (FuncCall *) lfirst(list_head(sublist));
					StringInfoData str;
					initStringInfo(&str);
					_outNode(&str,fntree->funcname);
					
					if (!strcmp("\"cypher\"",str.data)){

						StringInfoData cypher_str;
						initStringInfo(&cypher_str);

						_outNode(&cypher_str,lsecond(fntree->args));

						preprocess_cypherstr(cypher_str.data);

						*retstr = cypher_str.data;

						return true;
					}
				}
			}
		}
	}

	return false; 
}