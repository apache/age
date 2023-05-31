/*
 * For PostgreSQL Database Management System:
 * (formerly known as Postgres, then as Postgres95)
 *
 * Portions Copyright (c) 1996-2010, The PostgreSQL Global Development Group
 *
 * Portions Copyright (c) 1994, The Regents of the University of California
 *
 * Permission to use, copy, modify, and distribute this software and its documentation for any purpose,
 * without fee, and without a written agreement is hereby granted, provided that the above copyright notice
 * and this paragraph and the following two paragraphs appear in all copies.
 *
 * IN NO EVENT SHALL THE UNIVERSITY OF CALIFORNIA BE LIABLE TO ANY PARTY FOR DIRECT,
 * INDIRECT, SPECIAL, INCIDENTAL, OR CONSEQUENTIAL DAMAGES, INCLUDING LOST PROFITS,
 * ARISING OUT OF THE USE OF THIS SOFTWARE AND ITS DOCUMENTATION, EVEN IF THE UNIVERSITY
 * OF CALIFORNIA HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * THE UNIVERSITY OF CALIFORNIA SPECIFICALLY DISCLAIMS ANY WARRANTIES, INCLUDING,
 * BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 *
 * THE SOFTWARE PROVIDED HEREUNDER IS ON AN "AS IS" BASIS, AND THE UNIVERSITY OF CALIFORNIA
 * HAS NO OBLIGATIONS TO PROVIDE MAINTENANCE, SUPPORT, UPDATES, ENHANCEMENTS, OR MODIFICATIONS.
 */

#ifndef MY_HEADER_H
#define MY_HEADER_H

#include "postgres.h"

#include "mb/pg_wchar.h"
#include "nodes/primnodes.h"
#include "parser/parse_node.h"
#include "parser/cypher_parse_node.h"

typedef struct cypher_parsestate
{
    ParseState parsestate;
    int default_alias_num;
    char *graph_name;
    Oid graph_oid;
    /* other fields */
} cypher_parsestate;

typedef struct errpos_ecb_state
{
    ErrorContextCallback ecb;
    ParseState *pstate;
    int query_loc;
    /* other fields */
} errpos_ecb_state;

cypher_parsestate *make_cypher_parsestate(cypher_parsestate *parent_cpstate);
void free_cypher_parsestate(cypher_parsestate *cpstate);
void setup_errpos_ecb(errpos_ecb_state *ecb_state, ParseState *pstate, int query_loc);
void cancel_errpos_ecb(errpos_ecb_state *ecb_state);
RangeTblEntry *find_rte(cypher_parsestate *cpstate, char *varname);
char *get_next_default_alias(cypher_parsestate *cpstate);

#endif  // MY_HEADER_H

#include "my_header.h"

static void errpos_ecb(void *arg);

cypher_parsestate *make_cypher_parsestate(cypher_parsestate *parent_cpstate)
{
    /* Create a new cypher_parsestate and initialize the ParseState */
    cypher_parsestate *cpstate = palloc0(sizeof(*cpstate));
    ParseState *pstate = &cpstate->parsestate;

    /* Fill in fields that don't start at null/false/zero */
    pstate->parentParseState = (ParseState *)parent_cpstate;
    pstate->p_next_resno = 1;
    pstate->p_resolve_unknowns = true;

    if (parent_cpstate)
    {
        /* Inherit certain fields from the parent parse state */
        pstate->p_sourcetext = parent_cpstate->parsestate.p_sourcetext;
        pstate->p_queryEnv = parent_cpstate->parsestate.p_queryEnv;
        pstate->p_pre_columnref_hook = parent_cpstate->parsestate.p_pre_columnref_hook;
        pstate->p_post_columnref_hook = parent_cpstate->parsestate.p_post_columnref_hook;
        pstate->p_paramref_hook = parent_cpstate->parsestate.p_paramref_hook;
        pstate->p_coerce_param_hook = parent_cpstate->parsestate.p_coerce_param_hook;
        pstate->p_ref_hook_state = parent_cpstate->parsestate.p_ref_hook_state;

        /* Inherit graph-related fields */
        cpstate->graph_name = parent_cpstate->graph_name;
        cpstate->graph_oid = parent_cpstate->graph_oid;
        cpstate->params = parent_cpstate->params;
    }

    return cpstate;
}

void free_cypher_parsestate(cypher_parsestate *cpstate)
{
    /* Free the cypher_parsestate */
    pfree(cpstate);
}

void setup_errpos_ecb(errpos_ecb_state *ecb_state, ParseState *pstate, int query_loc)
{
    /* Set up error context callback */
    ecb_state->ecb.previous = error_context_stack;
    ecb_state->ecb.callback = errpos_ecb;
    ecb_state->ecb.arg = ecb_state;
    ecb_state->pstate = pstate;
    ecb_state->query_loc = query_loc;

    error_context_stack = &ecb_state->ecb;
}

void cancel_errpos_ecb(errpos_ecb_state *ecb_state)
{
    /* Cancel error context callback */
    error_context_stack = ecb_state->ecb.previous;
}

static void errpos_ecb(void *arg)
{
    /* Adjust the error position based on the query location */
    errpos_ecb_state *ecb_state = arg;
    int query_pos;

    if (geterrcode() == ERRCODE_QUERY_CANCELED)
        return;

    Assert(ecb_state->query_loc > -1);
    query_pos = pg_mbstrlen_with_len(ecb_state->pstate->p_sourcetext,
                                     ecb_state->query_loc);
    errposition(query_pos + geterrposition());
}

RangeTblEntry *find_rte(cypher_parsestate *cpstate, char *varname)
{
    ParseState *pstate = &cpstate->parsestate;
    ListCell *lc;

    foreach (lc, pstate->p_rtable)
    {
        RangeTblEntry *rte = (RangeTblEntry *)lfirst(lc);
        Alias *alias = rte->alias;
        if (!alias)
            continue;

        if (!strcmp(alias->aliasname, varname))
            return rte;
    }

    return NULL;
}

char *get_next_default_alias(cypher_parsestate *cpstate)
{
    char alias_num_str[32];
    snprintf(alias_num_str, sizeof(alias_num_str), "%d", cpstate->default_alias_num);
    cpstate->default_alias_num++;

    return pg_strdup(AGE_DEFAULT_ALIAS_PREFIX, alias_num_str);
}
