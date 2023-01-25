/*
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
 */

#ifndef pool_params_h
#define pool_params_h

typedef struct
{
	int			num;			/* number of entries */
	char	  **names;			/* parameter names */
	char	  **values;			/* values */
}			ParamStatus;

extern int	pool_init_params(ParamStatus * params);
extern void pool_discard_params(ParamStatus * params);
extern char *pool_find_name(ParamStatus * params, char *name, int *pos);
extern int	pool_get_param(ParamStatus * params, int index, char **name, char **value);
extern int	pool_add_param(ParamStatus * params, char *name, char *value);
extern void pool_param_debug_print(ParamStatus * params);


#endif /* pool_params_h */
