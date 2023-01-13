/* -*-pgsql-c-*- */
/*
 * $Header$
 *
 * pgpool: a language independent connection pool server for PostgreSQL
 * written by Tatsuo Ishii
 *
 * Portions Copyright (c) 2003-2022, PgPool Global Development Group
 * Portions Copyright (c) 2003-2004, PostgreSQL Global Development Group
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
#include "pool.h"
#include "utils/elog.h"
#include <errno.h>
#include <string.h>
#include <sys/shm.h>
#include <unistd.h>

#include "utils/pool_ipc.h"


#ifdef SHM_SHARE_MMU			/* use intimate shared memory on Solaris */
#define PG_SHMAT_FLAGS			SHM_SHARE_MMU
#else
#define PG_SHMAT_FLAGS			0
#endif

static void* shared_mem_chunk = NULL;
static char* shared_mem_free_pos = NULL;
static size_t chunk_size = 0;

static void IpcMemoryDetach(int status, Datum shmaddr);
static void IpcMemoryDelete(int status, Datum shmId);

void
initialize_shared_memory_main_segment(size_t size)
{
	/* only main process is allowed to create the chunk */
	if (mypid != getpid())
	{
		/* should never happen */
		ereport(LOG, (errmsg("initialize_shared_memory_chunk called from invalid process")));
		return;
	}

	if (shared_mem_chunk)
		return;

	ereport(LOG,
			(errmsg("allocating shared memory segment of size: %zu ",size)));

	shared_mem_chunk = pool_shared_memory_create(size);
	shared_mem_free_pos = (char*)shared_mem_chunk;
	chunk_size = size;
	memset(shared_mem_chunk, 0, size);
}

void *
pool_shared_memory_segment_get_chunk(size_t size)
{
	void *ptr = NULL;
	if (mypid != getpid())
	{
		/* should never happen */
		ereport(ERROR,
				(errmsg("initialize_shared_memory_chunk called from invalid process")));
		return NULL;
	}
	/* check if we have enough space left in chunk */
	if ((shared_mem_free_pos - (char*)shared_mem_chunk) + MAXALIGN(size) > chunk_size)
	{
		ereport(ERROR,
				(errmsg("no space left in shared memory segment")));
		return NULL;

	}
	/*
	 * return the current shared_mem_free_pos pointer
	 * and advance it by size
	 */
	ptr = (void*)shared_mem_free_pos;
	shared_mem_free_pos += MAXALIGN(size);
	return ptr;
}

/*
 * Create a shared memory segment of the given size and initialize.  Also,
 * register an on_shmem_exit callback to release the storage.
 */
void *
pool_shared_memory_create(size_t size)
{
	int			shmid;
	void	   *memAddress;

	/* Try to create new segment */
	shmid = shmget(IPC_PRIVATE, size, IPC_CREAT | IPC_EXCL | IPCProtection);

	if (shmid < 0)
		ereport(FATAL,
				(errmsg("could not create shared memory for request size: %zu", size),
				 errdetail("shared memory creation failed with error \"%m\"")));

	/* Register on-exit routine to delete the new segment */
	on_shmem_exit(IpcMemoryDelete, shmid);

	/* OK, should be able to attach to the segment */
	memAddress = shmat(shmid, NULL, PG_SHMAT_FLAGS);

	if (memAddress == (void *) -1)
		ereport(FATAL,
				(errmsg("could not create shared memory for request size: %ld", size),
				 errdetail("attach to shared memory [id:%d] failed with reason: \"%m\"", shmid)));


	/* Register on-exit routine to detach new segment before deleting */
	on_shmem_exit(IpcMemoryDetach, (Datum) memAddress);

	return memAddress;
}

/*
 * Removes a shared memory segment from process' address spaceq (called as
 * an on_shmem_exit callback, hence funny argument list)
 */
static void
IpcMemoryDetach(int status, Datum shmaddr)
{
	if (shmdt((void *) shmaddr) < 0)
		ereport(LOG,
				(errmsg("removing shared memory segments"),
				 errdetail("shmdt(%p) failed: %m", (void *) shmaddr)));

}

/*
 * Deletes a shared memory segment (called as an on_shmem_exit callback,
 * hence funny argument list)
 */
static void
IpcMemoryDelete(int status, Datum shmId)
{
	struct shmid_ds shmStat;

	/*
	 * Is a previously-existing shmem segment still existing and in use?
	 */
	if (shmctl(shmId, IPC_STAT, &shmStat) < 0
		&& (errno == EINVAL || errno == EACCES))
		return;
#ifdef NOT_USED
	else if (shmStat.shm_nattch != 0)
		return;
#endif

	if (shmctl(shmId, IPC_RMID, NULL) < 0)
		ereport(LOG,
				(errmsg("deleting shared memory segments"),
				 errdetail("shmctl(%lu, %d, 0) failed with error \"%m\"",
						   shmId, IPC_RMID)));
}

void
pool_shmem_exit(int code)
{
	shmem_exit(code);
	/* Close syslog connection here as this function is always called on exit */
	closelog();
}
