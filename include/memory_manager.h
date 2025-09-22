#ifndef MEMORY_MANAGER_H
#define MEMORY_MANAGER_H

#include "hash_table.h"

#define MAX_FRAMES 1024

/**
 * struct frame_t - represents a physical frame entry
 * @page:  page number currently stored in this frame
 * @dirty: non-zero if page has been written and must be flushed to disk
 * @valid: non-zero if this frame contains a valid page
 */
typedef struct {
	unsigned long page;
	int dirty;
	int valid;
} frame_t;

/**
 * struct proc_state_t - per-process memory management state
 * @frames:       array of frame descriptors owned by this process
 * @ht:           hash table for fast page lookups
 * @max_frames:   total number of frames allocated to this process
 * @used_frames:  number of currently allocated frames in use
 * @peak_frames:  peak number of frames used by this process
 * @trace_count:  number of trace entries processed
 * @pf_count:     page faults since last flush (used by FWF algorithm)
 * @total_pf:     total page faults since start
 *
 * @disk_reads:   number of simulated disk reads
 * @disk_writes:  number of simulated disk writes
 * @flush_count:  number of flush operations performed (FWF)
 * @total_expected: number of trace entries expected (sent by PM at startup)
 */
typedef struct {
	frame_t *frames;
	hash_table_t *ht;

	int max_frames;
	int used_frames;
	int peak_frames;

	int trace_count;
	int pf_count;
	int total_pf;

	int disk_reads;
	int disk_writes;
	int flush_count;
	
	long total_expected;
} proc_state_t;

#endif /* MEMORY_MANAGER_H */