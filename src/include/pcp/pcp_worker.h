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
*
*/

#ifndef pcp_worker_h
#define pcp_worker_h

extern int	send_to_pcp_frontend(char *data, int len, bool flush);
extern int	pcp_frontend_exists(void);
extern void pcp_worker_main(int port);
extern void pcp_mark_recovery_finished(void);
extern bool pcp_mark_recovery_in_progress(void);


#endif /* pcp_worker_h */
