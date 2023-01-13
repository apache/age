/* -*-pgsql-c-*- */
/*
 * $Header$
 *
 * pgpool: a language independent connection pool server for PostgreSQL
 * written by Tatsuo Ishii
 *
 * Copyright (c) 2003-2019	PgPool Global Development Group
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
 *---------------------------------------------------------------------
 * pool_proto2.c: modules corresponding to protocol 2.0.
 * used by pool_process_query()
 *---------------------------------------------------------------------
 */
#include <stdlib.h>
#include <string.h>
#include <netinet/in.h>

#include "pool.h"
#include "pool_config.h"
#include "protocol/pool_proto_modules.h"
#include "utils/pool_stream.h"
#include "utils/elog.h"

POOL_STATUS
AsciiRow(POOL_CONNECTION * frontend,
		 POOL_CONNECTION_POOL * backend,
		 short num_fields)
{
	static char nullmap[8192],
				nullmap1[8192];
	int			nbytes;
	int			i,
				j;
	unsigned char mask;
	int			size,
				size1 = 0;
	char	   *buf = NULL,
			   *sendbuf = NULL;
	char		msgbuf[1024];

	pool_write(frontend, "D", 1);

	nbytes = (num_fields + 7) / 8;

	if (nbytes <= 0)
		return POOL_CONTINUE;

	/* NULL map */
	pool_read(MAIN(backend), nullmap, nbytes);
	memcpy(nullmap1, nullmap, nbytes);
	for (i = 0; i < NUM_BACKENDS; i++)
	{
		if (VALID_BACKEND(i) && !IS_MAIN_NODE_ID(i))
		{
			pool_read(CONNECTION(backend, i), nullmap, nbytes);
			if (memcmp(nullmap, nullmap1, nbytes))
			{
				/*
				 * XXX: NULLMAP maybe different among backends. If we were a
				 * paranoid, we have to treat this as a fatal error. However
				 * in the real world we'd better to adapt this situation. Just
				 * throw a log...
				 */
				ereport(DEBUG1,
						(errmsg("processing ASCII row"),
						 errdetail("NULLMAP is different between main and backend no %d", i)));
			}
		}
	}

	if (pool_write(frontend, nullmap1, nbytes) < 0)
		return POOL_END;

	mask = 0;

	for (i = 0; i < num_fields; i++)
	{
		if (mask == 0)
			mask = 0x80;

		/* NOT NULL? */
		if (mask & nullmap[i / 8])
		{
			/* field size */
			if (pool_read(MAIN(backend), &size, sizeof(int)) < 0)
				return POOL_END;

			size1 = ntohl(size) - 4;

			/* read and send actual data only when size > 0 */
			if (size1 > 0)
			{
				sendbuf = pool_read2(MAIN(backend), size1);
				if (sendbuf == NULL)
					return POOL_END;
			}

			/* forward to frontend */
			pool_write(frontend, &size, sizeof(int));
			if (size1 > 0)
				pool_write(frontend, sendbuf, size1);
			snprintf(msgbuf, Min(sizeof(msgbuf), size1 + 1), "%s", sendbuf);
			ereport(DEBUG1,
					(errmsg("processing ASCII row"),
					 errdetail("len: %d data: %s", size1, msgbuf)));


			for (j = 0; j < NUM_BACKENDS; j++)
			{
				if (VALID_BACKEND(j) && !IS_MAIN_NODE_ID(j))
				{
					/* field size */
					if (pool_read(CONNECTION(backend, j), &size, sizeof(int)) < 0)
						return POOL_END;

					buf = NULL;
					size = ntohl(size) - 4;

					/*
					 * XXX: field size maybe different among backends. If we
					 * were a paranoid, we have to treat this as a fatal
					 * error. However in the real world we'd better to adapt
					 * this situation. Just throw a log...
					 */
					if (size != size1)
						ereport(DEBUG1,
								(errmsg("processing ASCII row"),
								 errdetail("size of field no %d does not match between main [size:%d] and backend no %d [size:%d]",
										   i, ntohl(size), j, ntohl(size1))));

					/* read and send actual data only when size > 0 */
					if (size > 0)
					{
						buf = pool_read2(CONNECTION(backend, j), size);
						if (buf == NULL)
							return POOL_END;
					}
				}
			}
		}

		mask >>= 1;
	}

	if (pool_flush(frontend))
		return POOL_END;

	return POOL_CONTINUE;
}

POOL_STATUS
BinaryRow(POOL_CONNECTION * frontend,
		  POOL_CONNECTION_POOL * backend,
		  short num_fields)
{
	static char nullmap[8192],
				nullmap1[8192];
	int			nbytes;
	int			i,
				j;
	unsigned char mask;
	int			size,
				size1 = 0;
	char	   *buf = NULL;

	pool_write(frontend, "B", 1);

	nbytes = (num_fields + 7) / 8;

	if (nbytes <= 0)
		return POOL_CONTINUE;

	/* NULL map */
	pool_read(MAIN(backend), nullmap, nbytes);
	if (pool_write(frontend, nullmap, nbytes) < 0)
		return POOL_END;
	memcpy(nullmap1, nullmap, nbytes);
	for (i = 0; i < NUM_BACKENDS; i++)
	{
		if (VALID_BACKEND(i) && !IS_MAIN_NODE_ID(i))
		{
			pool_read(CONNECTION(backend, i), nullmap, nbytes);
			if (memcmp(nullmap, nullmap1, nbytes))
			{
				/*
				 * XXX: NULLMAP maybe different among backends. If we were a
				 * paranoid, we have to treat this as a fatal error. However
				 * in the real world we'd better to adapt this situation. Just
				 * throw a log...
				 */
				ereport(DEBUG1,
						(errmsg("processing binary row"),
						 errdetail("NULLMAP is different between main and backend no %d", i)));
			}
		}
	}

	mask = 0;

	for (i = 0; i < num_fields; i++)
	{
		if (mask == 0)
			mask = 0x80;

		/* NOT NULL? */
		if (mask & nullmap[i / 8])
		{
			/* field size */
			if (pool_read(MAIN(backend), &size, sizeof(int)) < 0)
				return POOL_END;
			for (j = 0; j < NUM_BACKENDS; j++)
			{
				if (VALID_BACKEND(j) && !IS_MAIN_NODE_ID(j))
				{
					/* field size */
					if (pool_read(CONNECTION(backend, i), &size, sizeof(int)) < 0)
						return POOL_END;

					/*
					 * XXX: field size maybe different among backends. If we
					 * were a paranoid, we have to treat this as a fatal
					 * error. However in the real world we'd better to adapt
					 * this situation. Just throw a log...
					 */
					if (size != size1)
						ereport(DEBUG1,
								(errmsg("processing binary row"),
								 errdetail("size of field no %d does not match between main [size:%d] and backend no %d [size:%d]",
										   i, ntohl(size), j, ntohl(size1))));
				}

				buf = NULL;

				/* forward to frontend */
				if (IS_MAIN_NODE_ID(j))
					pool_write(frontend, &size, sizeof(int));
				size = ntohl(size) - 4;

				/* read and send actual data only when size > 0 */
				if (size > 0)
				{
					buf = pool_read2(CONNECTION(backend, j), size);
					if (buf == NULL)
						return POOL_END;

					if (IS_MAIN_NODE_ID(j))
					{
						pool_write(frontend, buf, size);
					}
				}
			}

			mask >>= 1;
		}
	}

	if (pool_flush(frontend))
		return POOL_END;

	return POOL_CONTINUE;
}

POOL_STATUS
CompletedResponse(POOL_CONNECTION * frontend,
				  POOL_CONNECTION_POOL * backend)
{
	int			i;
	char	   *string = NULL;
	char	   *string1 = NULL;
	int			len,
				len1 = 0;

	/* read command tag */
	string = pool_read_string(MAIN(backend), &len, 0);
	if (string == NULL)
		return POOL_END;
	else if (!strncmp(string, "BEGIN", 5))
		TSTATE(backend, MAIN_NODE_ID) = 'T';
	else if (!strncmp(string, "COMMIT", 6) || !strncmp(string, "ROLLBACK", 8))
		TSTATE(backend, MAIN_NODE_ID) = 'I';

	len1 = len;
	string1 = pstrdup(string);

	for (i = 0; i < NUM_BACKENDS; i++)
	{
		if (!VALID_BACKEND(i) || IS_MAIN_NODE_ID(i))
			continue;

		/* read command tag */
		string = pool_read_string(CONNECTION(backend, i), &len, 0);
		if (string == NULL)
			ereport(FATAL,
					(return_code(2),
					 errmsg("unable to process completed response"),
					 errdetail("read from backend node %d failed", i)));

		else if (!strncmp(string, "BEGIN", 5))
			TSTATE(backend, i) = 'T';
		else if (!strncmp(string, "COMMIT", 6) || !strncmp(string, "ROLLBACK", 8))
			TSTATE(backend, i) = 'I';

		if (len != len1)
		{
			ereport(DEBUG1,
					(errmsg("processing completed response"),
					 errdetail("message length does not match between main(%d \"%s\",) and %d th server (%d \"%s\",)",
							   len, string, i, len1, string1)));

			/* we except INSERT, because INSERT response has OID */
			if (strncmp(string1, "INSERT", 6))
			{
				pfree(string1);
				return POOL_END;
			}
		}
	}
	/* forward to the frontend */
	pool_write(frontend, "C", 1);
	ereport(DEBUG2,
			(errmsg("processing completed response"),
			 errdetail("string: \"%s\"", string1)));
	pool_write(frontend, string1, len1);

	pfree(string1);
	return pool_flush(frontend);
}

POOL_STATUS
CursorResponse(POOL_CONNECTION * frontend,
			   POOL_CONNECTION_POOL * backend)
{
	char	   *string = NULL;
	char	   *string1 = NULL;
	int			len,
				len1 = 0;
	int			i;

	/* read cursor name */
	string = pool_read_string(MAIN(backend), &len, 0);
	if (string == NULL)
		return POOL_END;
	len1 = len;
	string1 = pstrdup(string);

	for (i = 0; i < NUM_BACKENDS; i++)
	{
		if (VALID_BACKEND(i) && !IS_MAIN_NODE_ID(i))
		{
			/* read cursor name */
			string = pool_read_string(CONNECTION(backend, i), &len, 0);
			if (string == NULL)
				ereport(FATAL,
						(return_code(2),
						 errmsg("unable to process cursor response"),
						 errdetail("read failed on backend node %d", i)));
			if (len != len1)
			{
				ereport(FATAL,
						(return_code(2),
						 errmsg("unable to process cursor response"),
						 errdetail("length does not match between main(%d) and %d th backend(%d)", len, i, len1),
						 errhint("main(%s) %d th backend(%s)", string1, i, string)));
			}
		}
	}

	/* forward to the frontend */
	pool_write(frontend, "P", 1);
	pool_write(frontend, string1, len1);
	pool_flush(frontend);
	pfree(string1);

	return POOL_CONTINUE;
}

void
EmptyQueryResponse(POOL_CONNECTION * frontend,
				   POOL_CONNECTION_POOL * backend)
{
	char		c;
	int			i;

	for (i = 0; i < NUM_BACKENDS; i++)
	{
		if (VALID_BACKEND(i))
		{
			pool_read(CONNECTION(backend, i), &c, sizeof(c));
		}
	}

	pool_write(frontend, "I", 1);
	pool_write_and_flush(frontend, "", 1);
}

POOL_STATUS
ErrorResponse(POOL_CONNECTION * frontend,
			  POOL_CONNECTION_POOL * backend)
{
	char	   *string = "";
	int			len = 0;
	int			i;

	for (i = 0; i < NUM_BACKENDS; i++)
	{
		if (VALID_BACKEND(i))
		{
			/* read error message */
			string = pool_read_string(CONNECTION(backend, i), &len, 0);
			if (string == NULL)
				return POOL_END;
		}
	}
	/* forward to the frontend */
	pool_write(frontend, "E", 1);
	pool_write_and_flush(frontend, string, len);

	/*
	 * check session context, because this function is called by pool_do_auth
	 * too.
	 */
	if (pool_get_session_context(true))
		raise_intentional_error_if_need(backend);

	/* change transaction state */
	for (i = 0; i < NUM_BACKENDS; i++)
	{
		if (VALID_BACKEND(i))
		{
			if (TSTATE(backend, i) == 'T')
				TSTATE(backend, i) = 'E';
		}
	}

	return POOL_CONTINUE;
}

POOL_STATUS
FunctionResultResponse(POOL_CONNECTION * frontend,
					   POOL_CONNECTION_POOL * backend)
{
	char		dummy;
	int			len;
	char	   *result = NULL;
	int			i;

	pool_write(frontend, "V", 1);

	for (i = 0; i < NUM_BACKENDS; i++)
	{
		if (VALID_BACKEND(i))
		{
			pool_read(CONNECTION(backend, i), &dummy, 1);
		}
	}
	pool_write(frontend, &dummy, 1);

	/* non empty result? */
	if (dummy == 'G')
	{
		for (i = 0; i < NUM_BACKENDS; i++)
		{
			if (VALID_BACKEND(i))
			{
				/* length of result in bytes */
				pool_read(CONNECTION(backend, i), &len, sizeof(len));
			}
		}
		pool_write(frontend, &len, sizeof(len));

		len = ntohl(len);

		for (i = 0; i < NUM_BACKENDS; i++)
		{
			if (VALID_BACKEND(i))
			{
				/* result value itself */
				if ((result = pool_read2(MAIN(backend), len)) == NULL)
					ereport(FATAL,
							(return_code(2),
							 errmsg("unable to process function result response"),
							 errdetail("reading from backend node %d failed", i)));
			}
		}
		if (result)
			pool_write(frontend, result, len);
		else
			ereport(FATAL,
					(return_code(2),
					 errmsg("unable to process function result response"),
					 errdetail("reading from backend node failed")));
	}

	for (i = 0; i < NUM_BACKENDS; i++)
	{
		if (VALID_BACKEND(i))
		{
			/* unused ('0') */
			pool_read(MAIN(backend), &dummy, 1);
		}
	}
	pool_write(frontend, "0", 1);

	return pool_flush(frontend);
}

void
NoticeResponse(POOL_CONNECTION * frontend,
			   POOL_CONNECTION_POOL * backend)
{
	char	   *string = NULL;
	int			len = 0;
	int			i;

	for (i = 0; i < NUM_BACKENDS; i++)
	{
		if (VALID_BACKEND(i))
		{
			/* read notice message */
			string = pool_read_string(CONNECTION(backend, i), &len, 0);
			if (string == NULL)
				ereport(FATAL,
						(return_code(2),
						 errmsg("unable to process Notice response"),
						 errdetail("reading from backend node %d failed", i)));
		}
	}

	/* forward to the frontend */
	pool_write(frontend, "N", 1);
	if (string == NULL)
		ereport(FATAL,
				(return_code(2),
				 errmsg("unable to process Notice response"),
				 errdetail("reading from backend node failed")));
	else
		pool_write_and_flush(frontend, string, len);
}

POOL_STATUS
NotificationResponse(POOL_CONNECTION * frontend,
					 POOL_CONNECTION_POOL * backend)
{
	int			pid,
				pid1;
	char	   *condition,
			   *condition1 = NULL;
	int			len,
				len1 = 0;
	int			i;

	pool_write(frontend, "A", 1);

	for (i = 0; i < NUM_BACKENDS; i++)
	{
		if (VALID_BACKEND(i))
		{
			pool_read(CONNECTION(backend, i), &pid, sizeof(pid));
			condition = pool_read_string(CONNECTION(backend, i), &len, 0);
			if (condition == NULL)
				ereport(FATAL,
						(return_code(2),
						 errmsg("unable to process Notification response"),
						 errdetail("reading from backend node %d failed", i)));

			if (IS_MAIN_NODE_ID(i))
			{
				pid1 = pid;
				len1 = len;
				if (condition1)
					pfree(condition1);
				condition1 = pstrdup(condition);
			}
		}
	}

	pool_write(frontend, &pid1, sizeof(pid1));
	pool_write_and_flush(frontend, condition1, len1);
	pfree(condition1);
	return POOL_CONTINUE;
}

int
RowDescription(POOL_CONNECTION * frontend,
			   POOL_CONNECTION_POOL * backend,
			   short *result)
{
	short		num_fields,
				num_fields1 = 0;
	int			oid,
				mod;
	int			oid1,
				mod1;
	short		size,
				size1;
	char	   *string;
	int			len,
				len1;
	int			i;

	pool_read(MAIN(backend), &num_fields, sizeof(short));
	num_fields1 = num_fields;
	for (i = 0; i < NUM_BACKENDS; i++)
	{
		if (VALID_BACKEND(i) && !IS_MAIN_NODE_ID(i))
		{
			/* # of fields (could be 0) */
			pool_read(CONNECTION(backend, i), &num_fields, sizeof(short));
			if (num_fields != num_fields1)
			{
				ereport(FATAL,
						(return_code(2),
						 errmsg("unable to process row description"),
						 errdetail("num_fields does not match between backends main(%d) and %d th backend(%d)",
								   num_fields, i, num_fields1)));
			}
		}
	}

	/* forward it to the frontend */
	pool_write(frontend, "T", 1);
	pool_write(frontend, &num_fields, sizeof(short));
	num_fields = ntohs(num_fields);
	for (i = 0; i < num_fields; i++)
	{
		int			j;

		/* field name */
		string = pool_read_string(MAIN(backend), &len, 0);
		if (string == NULL)
			return POOL_END;
		len1 = len;
		pool_write(frontend, string, len);

		for (j = 0; j < NUM_BACKENDS; j++)
		{
			if (VALID_BACKEND(j) && !IS_MAIN_NODE_ID(j))
			{
				string = pool_read_string(CONNECTION(backend, j), &len, 0);
				if (string == NULL)
					ereport(FATAL,
							(return_code(2),
							 errmsg("unable to process row description"),
							 errdetail("read failed on backend node %d", j)));

				if (len != len1)
				{
					ereport(FATAL,
							(return_code(2),
							 errmsg("unable to process row description"),
							 errdetail("field length does not match between backends main(%d) and %d th backend(%d)",
									   ntohl(len), j, ntohl(len1))));
				}
			}
		}

		/* type oid */
		pool_read(MAIN(backend), &oid, sizeof(int));
		oid1 = oid;
		ereport(DEBUG1,
				(errmsg("processing ROW DESCRIPTION"),
				 errdetail("type oid: %d", ntohl(oid))));
		for (j = 0; j < NUM_BACKENDS; j++)
		{
			if (VALID_BACKEND(j) && !IS_MAIN_NODE_ID(j))
			{
				pool_read(CONNECTION(backend, j), &oid, sizeof(int));

				/* we do not regard oid mismatch as fatal */
				if (oid != oid1)
				{
					ereport(DEBUG1,
							(errmsg("processing ROW DESCRIPTION"),
							 errdetail("field oid does not match between backends main(%d) and %d th backend(%d)",
									   ntohl(oid), j, ntohl(oid1))));
				}
			}
		}
		pool_write(frontend, &oid1, sizeof(int));

		/* size */
		pool_read(MAIN(backend), &size, sizeof(short));
		size1 = size;
		for (j = 0; j < NUM_BACKENDS; j++)
		{
			if (VALID_BACKEND(j) && !IS_MAIN_NODE_ID(j))
			{
				pool_read(CONNECTION(backend, j), &size, sizeof(short));
				if (size1 != size)
				{
					ereport(FATAL,
							(errmsg("data among backends are different"),
							 errdetail("field size does not match between backends main(%d) and %d th backend(%d", ntohs(size), j, ntohs(size1))));

				}
			}
		}
		ereport(DEBUG1,
				(errmsg("processing ROW DESCRIPTION"),
				 errdetail("field size: %d", ntohs(size))));
		pool_write(frontend, &size1, sizeof(short));

		/* modifier */
		pool_read(MAIN(backend), &mod, sizeof(int));
		ereport(DEBUG1,
				(errmsg("processing ROW DESCRIPTION"),
				 errdetail("modifier: %d", ntohs(mod))));
		mod1 = mod;
		for (j = 0; j < NUM_BACKENDS; j++)
		{
			if (VALID_BACKEND(j) && !IS_MAIN_NODE_ID(j))
			{
				pool_read(CONNECTION(backend, j), &mod, sizeof(int));
				if (mod != mod1)
				{
					ereport(DEBUG1,
							(errmsg("processing ROW DESCRIPTION"),
							 errdetail("modifier does not match between backends main(%d) and %d th backend(%d)",
									   ntohl(mod), j, ntohl(mod1))));
				}
			}
		}
		pool_write(frontend, &mod1, sizeof(int));
	}

	*result = num_fields;

	return pool_flush(frontend);
}
