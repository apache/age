/* -*-pgsql-c-*- */
/*
 * $Header$
 *
 * pgpool: a language independent connection pool server for PostgreSQL
 * written by Tatsuo Ishii
 *
 * Copyright (c) 2003-2020	PgPool Global Development Group
 *
 * Permission to use, copy, modify, and distribute this software and
 * its documentation for any purpose and without fee is hereby
 * granted, provided that the above copyright notice appear in all
 * copies and that both that copyright notice and this permission
 * notice appear in supporting documentation, and that the name of the
 * author not be used in advertising or publicity pertaining to
 * distribution of the software without specific, written prior
 * permission. The author makes no representations about the
 * suitability of this software for any purpose.  It is provided "as
 * is" without express or implied warranty.
 *
 * params.c: Parameter Status handling routines
 *
 */
#include "config.h"

#include <stdlib.h>
#include <string.h>
#include "utils/elog.h"
#include "utils/pool_params.h"

#include "pool.h"
#include "parser/parser.h"
#include "utils/palloc.h"
#include "utils/memutils.h"

#define MAX_PARAM_ITEMS 128

/*
 * initialize parameter structure
 */
int
pool_init_params(ParamStatus * params)
{
	MemoryContext oldContext = MemoryContextSwitchTo(TopMemoryContext);

	params->num = 0;
	params->names = palloc(MAX_PARAM_ITEMS * sizeof(char *));
	params->values = palloc(MAX_PARAM_ITEMS * sizeof(char *));

	MemoryContextSwitchTo(oldContext);

	return 0;
}

/*
 * discard parameter structure
 */
void
pool_discard_params(ParamStatus * params)
{
	int			i;

	for (i = 0; i < params->num; i++)
	{
		pfree(params->names[i]);
		pfree(params->values[i]);
	}
	if (params->names)
		pfree(params->names);
	if (params->values)
		pfree(params->values);
	params->num = 0;
	params->names = NULL;
	params->values = NULL;

}

/*
 * find param value by name. if found, its value is returned
 * also, pos is set
 * if not found, NULL is returned
 */
char *
pool_find_name(ParamStatus * params, char *name, int *pos)
{
	int			i;

	for (i = 0; i < params->num; i++)
	{
		if (!strcmp(name, params->names[i]))
		{
			*pos = i;
			return params->values[i];
		}
	}
	return NULL;
}

/*
 * return name and value by index.
 */
int
pool_get_param(ParamStatus * params, int index, char **name, char **value)
{
	if (index < 0 || index >= params->num)
		return -1;

	*name = params->names[index];
	*value = params->values[index];

	return 0;
}

/*
 * add or replace name/value pair
 */
int
pool_add_param(ParamStatus * params, char *name, char *value)
{
	int			pos;
	MemoryContext oldContext = MemoryContextSwitchTo(TopMemoryContext);

	if (pool_find_name(params, name, &pos))
	{
		/* name already exists */
		if (strlen(params->values[pos]) < strlen(value))
		{
			params->values[pos] = repalloc(params->values[pos], strlen(value) + 1);
		}
		strcpy(params->values[pos], value);
	}
	else
	{
		int			num;

		/* add name/value pair */
		if (params->num >= MAX_PARAM_ITEMS)
		{
			ereport(ERROR,
					(errmsg("add parameter failed"),
					 errdetail("no more room for num")));
		}
		num = params->num;
		params->names[num] = pstrdup(name);
		params->values[num] = pstrdup(value);
		params->num++;
	}
	parser_set_param(name, value);
	MemoryContextSwitchTo(oldContext);

	return 0;
}

void
pool_param_debug_print(ParamStatus * params)
{
	int			i;

	for (i = 0; i < params->num; i++)
	{
		ereport(DEBUG2,
				(errmsg("No.%d: name: %s value: %s", i, params->names[i], params->values[i])));
	}
}
