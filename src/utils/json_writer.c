/*
 *
 * json_writer.c
 *
 * pgpool: a language independent connection pool server for PostgreSQL
 * written by Tatsuo Ishii
 *
 * Copyright (c) 2003-2015	PgPool Global Development Group
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


#include <stdio.h>
#include "parser/stringinfo.h"
#include "utils/palloc.h"
#include "utils/json_writer.h"

static inline int jw_get_current_element_count(JsonNode * jNode);
static inline void jw_inc_current_element_count(JsonNode * jNode);
static inline JWElementType jw_get_current_element_type(JsonNode * jNode);


JsonNode *
jw_create(JWElementType rootElement, bool pretty_output)
{
	JsonNode   *jNode = palloc(sizeof(JsonNode));

	jNode->buf = makeStringInfo();
	jNode->stack_ptr = 0;
	jNode->pretty = pretty_output;
	jw_start_element(jNode, rootElement, NULL);
	return jNode;
}

JsonNode *
jw_create_with_array(bool pretty_output)
{
	return jw_create(JWARRAY, pretty_output);
}

JsonNode *
jw_create_with_object(bool pretty_output)
{
	return jw_create(JWOBJECT, pretty_output);
}

bool
jw_put_string(JsonNode * jNode, char *key, char *value)
{
	if (key == NULL || value == NULL)
		return false;
	if (jw_get_current_element_count(jNode) < 0)
		return false;
	if (jw_get_current_element_type(jNode) != JWOBJECT)
		return false;

	if (jw_get_current_element_count(jNode) > 0)
		appendStringInfoChar(jNode->buf, ',');
	appendStringInfo(jNode->buf, "\"%s\":\"%s\"", key, value);
	jw_inc_current_element_count(jNode);
	return true;
}

/* for compatibility reasons we pack bool in int*/
bool
jw_put_bool(JsonNode * jNode, char *key, bool value)
{
	return jw_put_int(jNode, key, value ? 1 : 0);
}

bool
jw_put_int(JsonNode * jNode, char *key, int value)
{
	if (key == NULL)
		return false;
	if (jw_get_current_element_count(jNode) < 0)
		return false;
	if (jw_get_current_element_type(jNode) != JWOBJECT)
		return false;

	if (jw_get_current_element_count(jNode) > 0)
		appendStringInfoChar(jNode->buf, ',');
	appendStringInfo(jNode->buf, "\"%s\":%d", key, value);
	jw_inc_current_element_count(jNode);
	return true;
}

bool
jw_put_long(JsonNode * jNode, char *key, long value)
{
	if (key == NULL)
		return false;
	if (jw_get_current_element_count(jNode) < 0)
		return false;
	if (jw_get_current_element_type(jNode) != JWOBJECT)
		return false;

	if (jw_get_current_element_count(jNode) > 0)
		appendStringInfoChar(jNode->buf, ',');
	appendStringInfo(jNode->buf, "\"%s\":%ld", key, value);
	jw_inc_current_element_count(jNode);
	return true;
}

bool
jw_put_null(JsonNode * jNode, char *key)
{
	if (key == NULL)
		return false;
	if (jw_get_current_element_count(jNode) < 0)
		return false;
	if (jw_get_current_element_type(jNode) != JWOBJECT)
		return false;

	if (jw_get_current_element_count(jNode) > 0)
		appendStringInfoChar(jNode->buf, ',');
	appendStringInfo(jNode->buf, "\"%s\":null", key);
	jw_inc_current_element_count(jNode);
	return true;
}

bool
jw_put_string_value(JsonNode * jNode, char *value)
{
	if (value == NULL)
		return false;
	if (jw_get_current_element_count(jNode) < 0)
		return false;
	if (jw_get_current_element_type(jNode) != JWARRAY)
		return false;

	if (jw_get_current_element_count(jNode) > 0)
		appendStringInfoChar(jNode->buf, ',');
	appendStringInfo(jNode->buf, "\"%s\"", value);
	jw_inc_current_element_count(jNode);
	return true;
}

bool
jw_put_bool_value(JsonNode * jNode, bool value)
{
	return jw_put_int_value(jNode, value ? 1 : 0);
}

bool
jw_put_int_value(JsonNode * jNode, int value)
{
	if (jw_get_current_element_count(jNode) < 0)
		return false;
	if (jw_get_current_element_type(jNode) != JWARRAY)
		return false;

	if (jw_get_current_element_count(jNode) > 0)
		appendStringInfoChar(jNode->buf, ',');
	appendStringInfo(jNode->buf, "%d", value);
	jw_inc_current_element_count(jNode);
	return true;
}

bool
jw_put_long_value(JsonNode * jNode, long value)
{
	if (jw_get_current_element_count(jNode) < 0)
		return false;
	if (jw_get_current_element_type(jNode) != JWARRAY)
		return false;

	if (jw_get_current_element_count(jNode) > 0)
		appendStringInfoChar(jNode->buf, ',');
	appendStringInfo(jNode->buf, "%ld", value);
	jw_inc_current_element_count(jNode);
	return true;
}

bool
jw_put_null_value(JsonNode * jNode)
{
	if (jw_get_current_element_count(jNode) < 0)
		return false;
	if (jw_get_current_element_type(jNode) != JWARRAY)
		return false;

	if (jw_get_current_element_count(jNode) > 0)
		appendStringInfoChar(jNode->buf, ',');
	appendStringInfoString(jNode->buf, "null");
	jw_inc_current_element_count(jNode);
	return true;
}

bool
jw_start_element(JsonNode * jNode, JWElementType element, char *key)
{
	char		ch;

	if (jNode->stack_ptr >= MAX_STACK_DEPTH)
		return false;

	if (jw_get_current_element_count(jNode) > 0)
		appendStringInfoChar(jNode->buf, ',');

	if (element == JWOBJECT)
	{
		ch = '{';
	}
	else if (element == JWARRAY)
	{
		ch = '[';
	}
	else
		return false;

	if (key == NULL)
		appendStringInfoChar(jNode->buf, ch);
	else
		appendStringInfo(jNode->buf, "\"%s\": %c", key, ch);

	jw_inc_current_element_count(jNode);

	jNode->stack[jNode->stack_ptr].elementCount = 0;
	jNode->stack[jNode->stack_ptr++].elementType = element;
	return true;
}

bool
jw_start_array(JsonNode * jNode, char *key)
{
	if (jw_get_current_element_type(jNode) == JWARRAY)
		return jw_start_element(jNode, JWARRAY, NULL);
	return jw_start_element(jNode, JWARRAY, key);
}

bool
jw_start_object(JsonNode * jNode, char *key)
{
	if (jw_get_current_element_type(jNode) == JWARRAY)
		return jw_start_element(jNode, JWOBJECT, NULL);
	return jw_start_element(jNode, JWOBJECT, key);
}

bool
jw_end_element(JsonNode * jNode)
{
	char		ch;
	JWElementType element;

	if (jNode->stack_ptr <= 0)
		return false;
	/* pop the element from stack */
	element = jNode->stack[--jNode->stack_ptr].elementType;
	if (element == JWOBJECT)
		ch = '}';
	else if (element == JWARRAY)
		ch = ']';
	else
		return false;
	appendStringInfoChar(jNode->buf, ch);
	return true;
}

bool
jw_finish_document(JsonNode * jNode)
{
	while (jNode->stack_ptr > 0)
	{
		if (!jw_end_element(jNode))
			return false;
	}
	return true;
}

char *
jw_get_json_string(JsonNode * jNode)
{
	return jNode->buf->data;
}

int
jw_get_json_length(JsonNode * jNode)
{
	return jNode->buf->len;
}

void
jw_destroy(JsonNode * jNode)
{
	pfree(jNode->buf->data);
	pfree(jNode->buf);
	pfree(jNode);
}

static inline int
jw_get_current_element_count(JsonNode * jNode)
{
	if (jNode->stack_ptr <= 0)
		return -1;
	return jNode->stack[jNode->stack_ptr - 1].elementCount;
}

static inline void
jw_inc_current_element_count(JsonNode * jNode)
{
	if (jNode->stack_ptr <= 0)
		return;
	jNode->stack[jNode->stack_ptr - 1].elementCount++;
}

static inline JWElementType jw_get_current_element_type(JsonNode * jNode)
{
	if (jNode->stack_ptr <= 0)
		return -1;
	return jNode->stack[jNode->stack_ptr - 1].elementType;
}
