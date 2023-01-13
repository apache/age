/* -*-pgsql-c-*- */
/*
 *
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
 * pool_config.l: read configuration file
 *
 */

%{
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "pool.h"
#include "pool_config.h"
#include "pool_config_variables.h"
#include "utils/regex_array.h"
#include "utils/pool_path.h"
#ifndef POOL_PRIVATE
#include "utils/elog.h"
#else
#include "utils/fe_ports.h"
#endif

/* to shut off compiler warnings */
int yylex(void);

POOL_CONFIG g_pool_config;	/* configuration values */
POOL_CONFIG *pool_config = &g_pool_config;	/* for legacy reason pointer to the above struct */
static unsigned Lineno;
char   *config_file_dir = NULL; /* directory path of config file pgpool.conf */

typedef enum {
  POOL_KEY = 1,
  POOL_INTEGER,
  POOL_REAL,
  POOL_STRING,
  POOL_UNQUOTED_STRING,
  POOL_EQUALS,
  POOL_EOL,
  POOL_PARSE_ERROR
} POOL_TOKEN;

static char *extract_string(char *value, POOL_TOKEN token);
static void FreeConfigVariable(ConfigVariable *item);
static bool ParseConfigFile(const char *config_file, const char *calling_file,
				int depth, int elevel, ConfigVariable **head_p, ConfigVariable **tail_p);

%}

%option 8bit
%option never-interactive
%option nounput
%option noyywrap
%option noinput

SIGN            ("-"|"+")
DIGIT           [0-9]
HEXDIGIT        [0-9a-fA-F]

UNIT_LETTER     [a-zA-Z]
INTEGER         {SIGN}?({DIGIT}+|0x{HEXDIGIT}+){UNIT_LETTER}*

EXPONENT        [Ee]{SIGN}?{DIGIT}+
REAL            {SIGN}?{DIGIT}*"."{DIGIT}*{EXPONENT}?

LETTER          [A-Za-z_\200-\377]
LETTER_OR_DIGIT [A-Za-z_0-9\200-\377]

KEY              {LETTER}{LETTER_OR_DIGIT}*

UNQUOTED_STRING {LETTER}({LETTER_OR_DIGIT}|[-._:/])*
STRING          \'([^'\n]|\\.)*\'

%%

\n              Lineno++; return POOL_EOL;
[ \t\r]+        /* eat whitespace */
#.*$            /* eat comment */

{KEY}           return POOL_KEY;
{STRING}        return POOL_STRING;
{UNQUOTED_STRING} return POOL_UNQUOTED_STRING;
{INTEGER}       return POOL_INTEGER;
{REAL}          return POOL_REAL;
=               return POOL_EQUALS;

.               return POOL_PARSE_ERROR;

%%

int pool_init_config(void)
{
	memset(pool_config, 0, sizeof(POOL_CONFIG));

	g_pool_config.backend_desc = palloc0(sizeof(BackendDesc));
	g_pool_config.health_check_params = palloc0(MAX_NUM_BACKENDS*sizeof(HealthCheckParams));

	InitializeConfigOptions();

	return 0;
}

/*
 * Add regex expression to patterns array
 * The supported type are: write_function_list and read_only_function_list
 * Return 0 on error, 1 on success
 */
int add_regex_pattern(const char *type, char *s)
{
	int regex_flags = REG_NOSUB;
	RegPattern currItem;
	int	pattern_len;

	/* force case insensitive pattern matching */
	regex_flags |= REG_ICASE;
	/* Add extended regex search */
	regex_flags |= REG_EXTENDED;
	/* Fill the pattern type */
	if (strcmp(type, "write_function_list") == 0 ||
		strcmp(type, "primary_routing_query_pattern_list") == 0 ||
		strcmp(type, "cache_unsafe_memqcache_table_list") == 0)
	{
		currItem.type = WRITELIST;
	}
	else if (strcmp(type, "read_only_function_list") == 0 ||
	         strcmp(type, "cache_safe_memqcache_table_list") == 0)
	{
		currItem.type = READONLYLIST;
	}
	else
	{
		ereport(WARNING,
			(errmsg("unable to add regex pattern, bad pattern type %s", type)));
		return 0;
	}
	/* Fill the pattern flag */
	currItem.flag = regex_flags;

	/* Fill pattern array */
	pattern_len = sizeof(char)*(strlen(s)+3);
	currItem.pattern = palloc(pattern_len);

	/* Force exact matching of function name with ^ and $ on the regex
	   if required to prevent partial matching. It also allow backward
	   compatibility.
	 */
	if (strncmp(s, "^", 1) != 0) {
		strncpy(currItem.pattern, "^", 2);
		strncat(currItem.pattern, s, pattern_len - 2);
	} else {
		strncpy(currItem.pattern, s, pattern_len);
	}
	if (s[strlen(s)-1] != '$') {
		strncat(currItem.pattern, "$", 2);
	}
	ereport(DEBUG1,
		(errmsg("initializing pool configuration"),
			errdetail("adding regex pattern for \"%s\" pattern: %s",type, currItem.pattern)));

	/* compile our regex */
	if (regcomp(&currItem.regexv, currItem.pattern, currItem.flag) != 0)
	{
		ereport(WARNING,
			(errmsg("unable to add regex pattern for \"%s\", invalid pattern: \"%s\"", type,currItem.pattern)));
	}
    else if ((strcmp(type, "read_only_function_list") == 0 ||
	          strcmp(type, "write_function_list") == 0) &&
             growFunctionPatternArray(currItem) < 0)
    {
		ereport(WARNING,
			(errmsg("unable to add regex pattern for \"%s\", unable to allocate new pattern", type)));
        return 0;
    }
    else if ((strcmp(type, "cache_safe_memqcache_table_list") == 0 ||
	          strcmp(type, "cache_unsafe_memqcache_table_list") == 0) &&
             growMemqcacheTablePatternArray(currItem) < 0)
    {
		ereport(WARNING,
			(errmsg("unable to add regex pattern for \"%s\", unable to allocate new pattern", type)));
        return 0;
    }
	else if (strcmp(type, "primary_routing_query_pattern_list") == 0 &&
			growQueryPatternArray(currItem) < 0)
	{
		ereport(WARNING,
			(errmsg("unable to add regex pattern for \"%s\", unable to allocate new pattern", type)));
		return 0;
	}

	return 1;
}

/*
 * Dynamically grow the regex pattern array
 * The array start with PATTERN_ARR_SIZE storage place, if required
 * it will grow of PATTERN_ARR_SIZE more each time.
 */
int growFunctionPatternArray(RegPattern item)
{
	void *_tmp = NULL;
	if (pool_config->pattc == pool_config->current_pattern_size)
	{
		pool_config->current_pattern_size += PATTERN_ARR_SIZE;
		_tmp = repalloc(pool_config->lists_patterns,
		               (pool_config->current_pattern_size * sizeof(RegPattern)));
		if (!_tmp)
		{
			return(-1);
		}

		pool_config->lists_patterns = (RegPattern*)_tmp;
	}
	pool_config->lists_patterns[pool_config->pattc] = item;
	pool_config->pattc++;

	return(pool_config->pattc);
}

int growMemqcacheTablePatternArray(RegPattern item)
{
	void *_tmp = NULL;
	if (pool_config->memqcache_table_pattc == pool_config->current_memqcache_table_pattern_size)
	{
		pool_config->current_memqcache_table_pattern_size += PATTERN_ARR_SIZE;
		_tmp = repalloc(pool_config->lists_memqcache_table_patterns,
		               (pool_config->current_memqcache_table_pattern_size * sizeof(RegPattern)));
		if (!_tmp)
		{
			return(-1);
		}

		pool_config->lists_memqcache_table_patterns = (RegPattern*)_tmp;
	}
	pool_config->lists_memqcache_table_patterns[pool_config->memqcache_table_pattc] = item;
	pool_config->memqcache_table_pattc++;

	return(pool_config->memqcache_table_pattc);
}

int growQueryPatternArray(RegPattern item)
{
	void *_tmp = NULL;
	if (pool_config->query_pattc == pool_config->current_query_pattern_size)
	{
		pool_config->current_query_pattern_size += PATTERN_ARR_SIZE;
		_tmp = repalloc(pool_config->lists_query_patterns,
		               (pool_config->current_query_pattern_size * sizeof(RegPattern)));
		if (!_tmp)
		{
			return(-1);
		}

		pool_config->lists_query_patterns = (RegPattern*)_tmp;
	}
	pool_config->lists_query_patterns[pool_config->query_pattc] = item;
	pool_config->query_pattc++;

	return(pool_config->query_pattc);
}

/*
 * Free a single ConfigVariable
 */
static void
FreeConfigVariable(ConfigVariable *item)
{
	if (item->name)
		pfree(item->name);
	if (item->value)
		pfree(item->value);
	pfree(item);
}

/*
 * Free a list of ConfigVariables, including the names and the values
 */
static void
FreeConfigVariables(ConfigVariable *list)
{
	ConfigVariable *item;
	
	item = list;
	while (item)
	{
		ConfigVariable *next = item->next;
		FreeConfigVariable(item);
		item = next;
	}
}

/*
 * Read and parse a single configuration file.
 *
 * Input parameters:
 *  config_file: absolute or relative path name of the configuration file
 *  calling_file: absolute path of file containing the "include" directive,
 *    or NULL at outer level (config_file must be absolute at outer level)
 *  depth: recursion depth (used only to prevent infinite recursion)
 *  elevel: error logging level to use
 * Input/Output parameters:
 *  head_p, tail_p: head and tail of linked list of name/value pairs
 *
 * *head_p and *tail_p must be initialized, either to NULL or valid pointers
 * to a ConfigVariable list, before calling the outer recursion level.  Any
 * name-value pairs read from the input file(s) will be appended to the list.
 *
 * Note: if elevel >= ERROR then an error will not return control to the
 * caller, so there is no need to check the return value in that case.
 *
 */
static bool
ParseConfigFile(const char *config_file, const char *calling_file,
				int depth, int elevel, ConfigVariable **head_p,
				ConfigVariable **tail_p)
{

	FILE *fd;
	YY_BUFFER_STATE lex_buffer;
	int token;
	char *key;
	char *val;
	ConfigVariable *item;
	char buf[POOLMAXPATHLEN + 1];
	char *config_filepath;

	/*
	 * Reject too-deep include nesting depth.
	 */
	if (depth > 10)
		ereport(elevel,
				(errmsg("could not open configuration file \"%s\": maximum nesting depth exceeded",
								config_file)));

	if (depth == 0)
	{
		/* get directory path of config file pgpool.conf */
		strlcpy(buf, config_file, sizeof(buf));
		get_parent_directory(buf);
		config_file_dir = buf;
	}


	if (calling_file == NULL || is_absolute_path(config_file))
	{
		/* absolute path is taken as-is */
		config_filepath = pstrdup(config_file);
	}
	else
	{
		/* relative path is relative to dir of calling file */
		config_filepath = (char *) palloc(strlen(config_file) + 1 +
									   strlen(calling_file) + 1);
		strcpy(config_filepath, calling_file);
		get_parent_directory(config_filepath);
		join_path_components(config_filepath, config_filepath, config_file);
		canonicalize_path(config_filepath);
	}

	/* open config file */
	fd = fopen(config_filepath, "r");
	if (!fd)
	{
		ereport(WARNING,
			(errmsg("could not open configuration file: \"%s\"",config_filepath),
				errdetail("using default configuration parameter values")));
		return false;
	}

	lex_buffer = yy_create_buffer(fd, YY_BUF_SIZE);
	yy_switch_to_buffer(lex_buffer);

	yyin = fd;
	Lineno = 1;

	for(;;)
	{
		token = yylex();

		if (token == 0)
			break;

		if (token == POOL_EOL)
			continue;

		if (token == POOL_PARSE_ERROR)
			goto parse_error;

		if (token != POOL_KEY)
			goto parse_error;

		key = pstrdup(yytext);

		token = yylex();

		/* next we have an optional equal sign; discard if present */
		if (token == POOL_EQUALS)
			token = yylex();

		val = extract_string(yytext,token);

		ereport(DEBUG5,
			(errmsg("key: \"%s\" value: \"%s\" kind: %d",key, val, token)));

		if (strcmp(key, "include") == 0)
		{
			/*
			 * An include directive isn't a variable and should be processed
			 * immediately.
			 */
			unsigned save_Lineno = Lineno;

			if (!ParseConfigFile(val, config_filepath, depth + 1, elevel, head_p, tail_p))
			{
				goto parse_error;
			}
			yy_switch_to_buffer(lex_buffer);
			Lineno = save_Lineno;
		}
		else
		{
			/* Add this to the list */
			item = palloc(sizeof(ConfigVariable));
			item->name = key;
			item->value = val;
			item->sourceline = Lineno;
			item->next = NULL;

			if (*head_p == NULL)
				*head_p = item;
			else
				(*tail_p)->next = item;
			*tail_p = item;
		}
	}

	fclose(fd);
	return true;

parse_error:

	fclose(fd);
	FreeConfigVariables(*head_p);
	*head_p = NULL;
	*tail_p = NULL;
	ereport(elevel,
		(errmsg("syntax error in configuration file \"%s\"", config_filepath),
			errdetail("parse error at line %d '%s' token = %d", Lineno, yytext,token)));

	return false;
}

/*
 * Read the configuration file and load the values of all parameters
 */
bool pool_get_config(const char *config_file, ConfigContext context)
{
	ConfigVariable *head_p = NULL;
	ConfigVariable *tail_p = NULL;
	bool res;
	int elevel = (context == CFGCXT_INIT)?FATAL:WARNING;

	res = ParseConfigFile(config_file, NULL, 0, elevel, &head_p, &tail_p);
	if (res == false || head_p == NULL)
		return false;

	res = set_config_options(head_p, context, PGC_S_FILE, elevel);
	FreeConfigVariables(head_p);
	return res;
}


static char *extract_string(char *value, POOL_TOKEN token)
{
	char *ret = NULL;

	if (token == POOL_STRING)
	{
		ret = pstrdup(value+1);
		ret[strlen(ret)-1] = '\0';
	}
	else
		ret = pstrdup(value);

	return ret;
}

/*
 * Try to interpret value as boolean value.  Valid values are: true,
 * false, yes, no, on, off, 1, 0; as well as unique prefixes thereof.
 * If the string parses okay, return true, else false.
 * If okay and result is not NULL, return the value in *result.
 * This function copied from PostgreSQL source code.
 */
static bool parse_bool_with_len(const char *value, size_t len, bool *result)
{
	switch (*value)
	{
		case 't':
		case 'T':
			if (strncasecmp(value, "true", len) == 0)
			{
				if (result)
					*result = true;
				return true;
			}
			break;
		case 'f':
		case 'F':
			if (strncasecmp(value, "false", len) == 0)
			{
				if (result)
					*result = false;
				return true;
			}
			break;
		case 'y':
		case 'Y':
			if (strncasecmp(value, "yes", len) == 0)
			{
				if (result)
					*result = true;
				return true;
			}
			break;
		case 'n':
		case 'N':
			if (strncasecmp(value, "no", len) == 0)
			{
				if (result)
					*result = false;
				return true;
			}
			break;
		case 'o':
		case 'O':
			/* 'o' is not unique enough */
			if (strncasecmp(value, "on", (len > 2 ? len : 2)) == 0)
			{
				if (result)
					*result = true;
				return true;
			}
			else if (strncasecmp(value, "off", (len > 2 ? len : 2)) == 0)
			{
				if (result)
					*result = false;
				return true;
			}
			break;
		case '1':
			if (len == 1)
			{
				if (result)
					*result = true;
				return true;
			}
			break;
		case '0':
			if (len == 1)
			{
				if (result)
					*result = false;
				return true;
			}
			break;
		default:
			break;
	}

	if (result)
		*result = false;		/* suppress compiler warning */
	return false;
}

int eval_logical(const char *str)
{
	bool result;

	if (!parse_bool_with_len(str, strlen(str), &result))
		return -1;

	return (result ? 1 : 0);
}


#ifdef DEBUG
static void print_host_entry(int slot)
{
	ereport(DEBUG1,
		(errmsg("initializing pool configuration"),
			errdetail("slot: %d host: %s port: %d status: %d weight: %f",
					slot,
					pool_config->server_hostnames[slot],
					pool_config->server_ports[slot],
					pool_config->server_status[slot],
					pool_config->server_weights[slot])));
}
#endif


/*
 * Translate binary form of backend flag to string.
 * The returned data is in static buffer, and it will be destroyed
 * at the next call to this function.
 */
char *pool_flag_to_str(unsigned short flag)
{
	static char buf[1024];		/* should be large enough */

	*buf = '\0';

	if (POOL_ALLOW_TO_FAILOVER(flag))
		snprintf(buf, sizeof(buf), "ALLOW_TO_FAILOVER");
	else if (POOL_DISALLOW_TO_FAILOVER(flag))
		snprintf(buf, sizeof(buf), "DISALLOW_TO_FAILOVER");

	if (POOL_ALWAYS_PRIMARY & flag)
	{
		if (*buf == '\0')
			snprintf(buf, sizeof(buf), "ALWAYS_PRIMARY");
		else
			snprintf(buf+strlen(buf), sizeof(buf), "|ALWAYS_PRIMARY");
	}

	return buf;
}

/*
 * Translate the BACKEND_STATUS enum value to string.
 * the function returns the constant string so should not be freed
 */
char* backend_status_to_str(BackendInfo *bi)
{
	char *statusName;

	switch (bi->backend_status) {

		case CON_UNUSED:
		statusName = BACKEND_STATUS_CON_UNUSED;
		break;

		case CON_CONNECT_WAIT:
		statusName = BACKEND_STATUS_CON_CONNECT_WAIT;
		break;

		case CON_UP:
		statusName = BACKEND_STATUS_CON_UP;
		break;

		case CON_DOWN:
		{
			if (bi->quarantine)
				statusName = BACKEND_STATUS_QUARANTINE;
			else
				statusName = BACKEND_STATUS_CON_DOWN;
		}
		break;

		default:
		statusName = "unknown";
		break;
	}
	return statusName;
}
