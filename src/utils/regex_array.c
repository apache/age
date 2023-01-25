/* -*-pgsql-c-*- */
/*
 * $Header$
 *
 * pgpool: a language independent connection pool server for PostgreSQL
 * written by Tatsuo Ishii
 *
 * Copyright (c) 2003-2016	PgPool Global Development Group
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
 */

/*
 * This module handles regular expression array.
 */
#include <string.h>

#include "pool.h"
#include "pool_config.h"
#include "utils/regex_array.h"
#ifndef POOL_PRIVATE
#include "utils/elog.h"
#include "utils/palloc.h"
#else
#include "utils/fe_ports.h"
#endif

/*
 * Create RegArray object
 */
RegArray *
create_regex_array(void)
{
	RegArray   *ar;

	ar = palloc(sizeof(RegArray));
	ar->pos = 0;
	ar->size = AR_ALLOC_UNIT;
	ar->regex = (regex_t **) palloc(sizeof(regex_t *) * ar->size);

	return ar;
}

/*
 * Add an regular expression pattern
 */
int
add_regex_array(RegArray * ar, char *pattern)
{
	int			regex_flags;
	regex_t    *regex;
	char	   *pat;
	int			len;

	if (ar == NULL)
	{
		ereport(WARNING,
				(errmsg("failed to add regex pattern, regex array is NULL")));

		return -1;
	}

	if (pattern == NULL)
	{
		ereport(WARNING,
				(errmsg("failed to add regex pattern, regex pattern is NULL")));
		return -1;
	}

	len = strlen(pattern);

	/* Force case insensitive pattern matching */
	regex_flags = REG_NOSUB;
	regex_flags |= REG_ICASE;

	/* Add extended regex search */
	regex_flags |= REG_EXTENDED;

	pat = palloc(sizeof(char) * (len + 3));
	if (strncmp(pattern, "^", 1) != 0)
	{
		strncpy(pat, "^", 2);
		strncat(pat, pattern, len + 1);
	}
	else
	{
		strncpy(pat, pattern, len + 1);
	}

	if (len == 0 || (len > 0 && pattern[len - 1] != '$'))
	{
		strncat(pat, "$", 2);
	}

	/* Compile our regex */
	regex = palloc(sizeof(regex_t));
	if (regcomp(regex, pat, regex_flags) != 0)
	{
		ereport(WARNING,
				(errmsg("failed to add regex pattern, invalid regex pattern: \"%s\" (%s)", pattern, pat)));
		pfree(regex);
		pfree(pat);
		return -1;
	}
	pfree(pat);

	if (ar->pos == ar->size)
	{
		ar->size += AR_ALLOC_UNIT;
		ar->regex = repalloc(ar->regex, sizeof(regex_t *) * ar->size);
	}
	ar->regex[ar->pos] = regex;
	ar->pos++;

	return 0;
}

/*
 * Execute regex matching. Returns matched array index.
 */
int
regex_array_match(RegArray * ar, char *pattern)
{
	int			i;

	if (ar == NULL)
	{
		ereport(WARNING,
				(errmsg("failed to execute regex matching, regex array is NULL")));
		return -1;
	}

	if (pattern == NULL)
	{
		ereport(WARNING,
				(errmsg("failed to execute regex matching, pattern is NULL")));
		return -1;
	}

	for (i = 0; i < ar->pos; i++)
	{
		if (regexec(ar->regex[i], pattern, 0, 0, 0) == 0)
			return i;
	}
	return -1;
}

/*
 * Destroy RegArray object
 */
void
destroy_regex_array(RegArray * ar)
{
	pfree(ar->regex);
	pfree(ar);
}

/*
 * Create L-R token array
 */
Left_right_tokens *
create_lrtoken_array(void)
{
	Left_right_tokens *ar;

	ar = palloc(sizeof(Left_right_tokens));
	ar->pos = 0;
	ar->size = AR_ALLOC_UNIT;
	ar->token = (Left_right_token *) palloc(sizeof(Left_right_token) * ar->size);

	return ar;
}

/*
 * Extract tokens separated by delimi from str. Each token is separated by delim2,
 * and token lists are returned to left_tokens and right_tokens respectively.
 * Nnumber of tokens is set to *n.
 */
void
extract_string_tokens2(char *str, char *delimi, char delimi2, Left_right_tokens * lrtokens)
{
	char	   *token;
	char	   *mystr;

	mystr = pstrdup(str);

	for (token = strtok(mystr, delimi); token != NULL; token = strtok(NULL, delimi))
	{
		int			len;
		char	   *left_token;
		char	   *right_token;
		char	   *weight_token = NULL;
		int			i,
					j;

		len = strlen(token) + 1;
		left_token = palloc(len);
		right_token = palloc(len);
		weight_token = palloc(len);

		for (i = 0; token[i] && token[i] != delimi2; i++)
			left_token[i] = token[i];

		left_token[i] = '\0';

		if (i == len - 1)
		{
			pfree(left_token);
			pfree(right_token);
			pfree(mystr);
			pfree(weight_token);
			return;
		}

		/* Skip delimiter 2 */
		i++;
		j = 0;

		for (; token[i] && token[i] != '('; i++)
			right_token[j++] = token[i];

		right_token[j] = '\0';

		/* delimiter 3 */
		int			k = 0;

		if (token[i] == '(')
		{
			i++;
			for (; token[i] && token[i] != ')'; i++)
				weight_token[k++] = token[i];
		}
		weight_token[k] = '\0';

		if (lrtokens->pos == lrtokens->size)
		{
			lrtokens->size += AR_ALLOC_UNIT;
			lrtokens->token = repalloc(lrtokens->token, sizeof(Left_right_token) * lrtokens->size);
		}
		lrtokens->token[lrtokens->pos].left_token = left_token;
		lrtokens->token[lrtokens->pos].right_token = right_token;
		if (weight_token[0] != '\0')
			lrtokens->token[lrtokens->pos].weight_token = atof(weight_token);
		else
			lrtokens->token[lrtokens->pos].weight_token = 1.0;

		lrtokens->pos++;
		pfree(weight_token);
	}
	pfree(mystr);
}
