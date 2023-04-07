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
#include "parser/extensible.h"
#include "age/cypher_nodes.h"


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
					List* funcname = fntree->funcname;

					bool isCypherCall = false;

					StringInfoData buff;
					initStringInfo(&buff);

					if (funcname->length == 2)
					{
						StringInfoData schemaName;
						initStringInfo(&schemaName);
						_outNode(&schemaName,linitial(funcname));

						if (!strcmp("\"ag_catalog\"",schemaName.data))
						{
							_outNode(&buff,lsecond(funcname));

							if (!strcmp("\"cypher\"",buff.data))
							{
								isCypherCall = true;
							}
						}
					}
					else if (funcname->length == 1)
					{
						_outNode(&buff,linitial(funcname));
						if (!strcmp("\"cypher\"",buff.data))
							{
								isCypherCall = true;
							}
					}
					
					if (isCypherCall){

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

List* 
/* Returns all the Cypher Nodes
   in the given parse tree.
*/
extractCypherFuncs(List *ParsedCyphertreeList)
{

	ListCell *resultfl;

	List *cypher_funcs_list= NIL;

	foreach (resultfl, ParsedCyphertreeList)
	{
		Node *newnode = (Node *)lfirst(resultfl);

		ExtensibleNode *extnode;

		if (IsA(newnode, ExtensibleNode)){

			extnode = (ExtensibleNode *)newnode;
			cypher_funcs_list = lappend(cypher_funcs_list, extnode->extnodename);

		}
	}
	
	return cypher_funcs_list;
}

bool 
/* Returns true if the given cypher node
   is a write query.
*/
IsWriteQuery(List *cypher_funcs_list){
	ListCell *cypherfl;
	foreach (cypherfl, cypher_funcs_list)
	{
		char *cypher_func = (char *)lfirst(cypherfl);


		// DETACH DELETE is covered under cypher_delete.
            // REMOVE command is covered under cypher_set.
        if (is_ag_node(cypher_func, cypher_create) || is_ag_node(cypher_func, cypher_set) ||
        is_ag_node(cypher_func, cypher_delete) || is_ag_node(cypher_func, cypher_merge))
        {
        	return true;
        }
	}

	return false;
}