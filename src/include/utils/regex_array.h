/* -*-pgsql-c-*- */
/*
 *
 * pgpool: a language independent connection pool server for PostgreSQL
 * written by Tatsuo Ishii
 *
 * Copyright (c) 2003-2014	PgPool Global Development Group
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

#ifndef REGEX_ARRAY_H
#define REGEX_ARRAY_H

#include <regex.h>

#define AR_ALLOC_UNIT	16

/*
 * Regular expression array
 */
typedef struct
{
	int			size;			/* regex array size */
	int			pos;			/* next regex array index position */
	regex_t   **regex;			/* regular expression array */
}			RegArray;

RegArray   *create_regex_array(void);
int			add_regex_array(RegArray * ar, char *pattern);
int			regex_array_match(RegArray * ar, char *pattern);
void		destroy_regex_array(RegArray * ar);

/*
 * String left-right token type
 */
typedef struct
{
	char	   *left_token;
	char	   *right_token;
	double		weight_token;
}			Left_right_token;

typedef struct
{
	int			pos;
	int			size;
	Left_right_token *token;
}			Left_right_tokens;

Left_right_tokens *create_lrtoken_array(void);
void		extract_string_tokens2(char *str, char *delimi, char delimi2, Left_right_tokens * lrtokens);

#endif
