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


#ifndef health_check_h
#define health_check_h

/*
 * Health check statistics per node
*/
typedef struct {
	uint64	total_count;	/* total count of health check */
	uint64	success_count;	/* total count of successful health check */
	uint64	fail_count;		/* total count of failed health check */
	uint64	skip_count;		/* total count of skipped health check */
	uint64	retry_count;	/* total count of health check retries */
	uint32	max_retry_count;	/* max retry count in a health check session */
	uint64	total_health_check_duration;	/* sum of health check duration */
	int32	max_health_check_duration;	/* maximum duration spent for a health check session in milli seconds */
	int32	min_health_check_duration;	/* minimum duration spent for a health check session in milli seconds */
	time_t	last_health_check;	/* last health check timestamp */
	time_t	last_successful_health_check;	/* last successful health check timestamp */
	time_t	last_skip_health_check;			/* last skipped health check timestamp */
	time_t	last_failed_health_check;		/* last failed health check timestamp */
} POOL_HEALTH_CHECK_STATISTICS;

extern volatile POOL_HEALTH_CHECK_STATISTICS	*health_check_stats;	/* health check stats area in shared memory */

extern void do_health_check_child(int *node_id);
extern size_t	health_check_stats_shared_memory_size(void);
extern void		health_check_stats_init(POOL_HEALTH_CHECK_STATISTICS *addr);

#endif /* health_check_h */
