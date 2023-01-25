/* -*-pgsql-c-*- */
/*
 *
 * $Header$
 *
 * pgpool: a language independent connection pool server for PostgreSQL
 * written by Tatsuo Ishii
 *
 * Copyright (c) 2003-2020	PgPool Global Development Group
 *
 */
/*--------------------------------------------------------------------
 * statistics.c
 *
 * Various statistics related functions.
 *--------------------------------------------------------------------
 */

#include <unistd.h>
#include <string.h>

#include "pool.h"
#include "utils/statistics.h"
#include "parser/nodes.h"

/*
 * Per backend node stat area in shared memory
 */
typedef struct
{
	uint64		select_cnt;		/* number of read SELECT queries issued */
	uint64		insert_cnt;		/* number of INSERT queries issued */
	uint64		update_cnt;		/* number of UPDATE queries issued */
	uint64		delete_cnt;		/* number of DELETE queries issued */
	uint64		ddl_cnt;		/* number of DDL queries issued */
	uint64		other_cnt;		/* number of any other queries issued */
	uint64		panic_cnt;		/* number of PANIC messages */
	uint64		fatal_cnt;		/* number of FATAL messages */
	uint64		error_cnt;		/* number of ERROR messages */
}			PER_NODE_STAT;

static volatile PER_NODE_STAT *per_node_stat;

/*
 * Return shared memory size necessary for this module
 */
size_t
stat_shared_memory_size(void)
{
	size_t		size;

	/* query counter area */
	size = MAXALIGN(MAX_NUM_BACKENDS * sizeof(PER_NODE_STAT));

	return size;
}

/*
 * Set PER_NODE_STAT address in the shared memory area to global variable.
 * This should be called from pgpool main process upon startup.
 */
void
stat_set_stat_area(void *address)
{
	per_node_stat = (PER_NODE_STAT *) address;
}

/*
 * Initialize shared memory stat area
 */
void
stat_init_stat_area(void)
{
	memset((void *) per_node_stat, 0, stat_shared_memory_size());
}

/*
 * Update stat counter
 */
void
stat_count_up(int backend_node_id, Node *parse_tree)
{
	if (parse_tree == NULL)
	{
		/*
		 * No parse tree. does not worth to gather statistics (internal
		 * queries).  If we want to gather statistics for queries without
		 * parse tree, we could call the parser but I don't think it's worth
		 * the trouble.
		 */
		return;
	}

	if (IsA(parse_tree, SelectStmt))
	{
		per_node_stat[backend_node_id].select_cnt++;
	}

	else if (IsA(parse_tree, InsertStmt))
	{
		per_node_stat[backend_node_id].insert_cnt++;
	}

	else if (IsA(parse_tree, UpdateStmt))
	{
		per_node_stat[backend_node_id].update_cnt++;
	}

	else if (IsA(parse_tree, DeleteStmt))
	{
		per_node_stat[backend_node_id].delete_cnt++;
	}

	else
	{
		switch(nodeTag(parse_tree))
		{
			case(T_CheckPointStmt):
			case(T_DeallocateStmt):
			case(T_DiscardStmt):
			case(T_ExecuteStmt):
			case(T_ExplainStmt):
			case(T_ListenStmt):
			case(T_LoadStmt):
			case(T_LockStmt):
			case(T_NotifyStmt):
			case(T_PrepareStmt):
			case(T_TransactionStmt):
			case(T_UnlistenStmt):
			case(T_VacuumStmt):
			case(T_VariableSetStmt):
			case(T_VariableShowStmt):
				per_node_stat[backend_node_id].other_cnt++;
				break;

			default:
				per_node_stat[backend_node_id].ddl_cnt++;
		}
	}
}

/*
 * Update error stat counter
 */
void
error_stat_count_up(int backend_node_id, char *str)
{
	if (strcasecmp(str, "PANIC") == 0)
		per_node_stat[backend_node_id].panic_cnt++;
	else if (strcasecmp(str, "FATAL") == 0)
		per_node_stat[backend_node_id].fatal_cnt++;
	else if (strcasecmp(str, "ERROR") == 0)
		per_node_stat[backend_node_id].error_cnt++;
}

/*
 * Stat counter read functions
 */
uint64
stat_get_select_count(int backend_node_id)
{
	return per_node_stat[backend_node_id].select_cnt;
}

uint64
stat_get_insert_count(int backend_node_id)
{
	return per_node_stat[backend_node_id].insert_cnt;
}

uint64
stat_get_update_count(int backend_node_id)
{
	return per_node_stat[backend_node_id].update_cnt;
}

uint64
stat_get_delete_count(int backend_node_id)
{
	return per_node_stat[backend_node_id].delete_cnt;
}

uint64
stat_get_ddl_count(int backend_node_id)
{
	return per_node_stat[backend_node_id].ddl_cnt;
}

uint64
stat_get_other_count(int backend_node_id)
{
	return per_node_stat[backend_node_id].other_cnt;
}

uint64
stat_get_panic_count(int backend_node_id)
{
	return per_node_stat[backend_node_id].panic_cnt;
}

uint64
stat_get_fatal_count(int backend_node_id)
{
	return per_node_stat[backend_node_id].fatal_cnt;
}

uint64
stat_get_error_count(int backend_node_id)
{
	return per_node_stat[backend_node_id].error_cnt;
}
