#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/shm.h>
#include <string.h>
#include "shared_defs.h"
#include "memory_manager.h"
#include "hash_table.h"

int DEBUG_MODE = 0;

/* Sum of all expected trace lines (PM1 + PM2), used for progress bar */
long total_expected = 0;

/* Check if a page is present in the process's hash table.
 * Returns frame index if found, -1 if not found.
 */
int find_page(proc_state_t *p, unsigned long page) {
	return hash_lookup(p->ht, page);
}

/* Flush all frames for a process, writing dirty pages to disk */
void flush_frames(proc_state_t *p, int pm_id) {
	int cleared_frames = 0, written_pages = 0;
	
	for (int i = 0; i < p->max_frames; i++) {
		if (p->frames[i].valid) {
			cleared_frames++;
			if (p->frames[i].dirty) {
				p->disk_writes++;
				written_pages++;
			}
		}
		p->frames[i].valid = 0;
		p->frames[i].dirty = 0;
	}
	hash_clear(p->ht);
	p->used_frames = 0;
	p->pf_count = 0;

	if (DEBUG_MODE) {
		printf("[DEBUG] Flush: PM%d cleared %d frames, wrote %d dirty pages\n",
			pm_id, cleared_frames, written_pages);
	}
}

/* Simple text-based progress bar */
void print_progress(const char *label, int current, int total) {
	const int bar_width = 40;
	float progress = (float)current / total;
	int pos = (int)(bar_width * progress);

	fprintf(stderr, "\r%s [", label);
	for (int i = 0; i < bar_width; ++i) {
		if (i < pos) fprintf(stderr, "=");
		else if (i == pos) fprintf(stderr, ">");
		else fprintf(stderr, " ");
	}
	fprintf(stderr, "] %3d%%", (int)(progress * 100));
	fflush(stderr);
}

int main(int argc, char *argv[]) {
	if (argc < 3) {
		fprintf(stderr, "Incorrect number of arguments\n");
		exit(EXIT_FAILURE);
	}

	for (int i = 1; i < argc; i++) {
		if (strcmp(argv[i], "--debug") == 0) {
			DEBUG_MODE = 1;
		}
	}

	int k = atoi(argv[1]);
	int total_frames = atoi(argv[2]);

	proc_state_t pm[2];

	/* Initialize per-process state (PM1 + PM2) */
	for (int i = 0; i < 2; i++) {
		pm[i].max_frames = total_frames / 2;
		pm[i].ht = hash_init(pm[i].max_frames);
		pm[i].used_frames = 0;
		pm[i].peak_frames = 0;
		pm[i].pf_count = 0;
		pm[i].total_pf = 0;
		pm[i].frames = calloc(pm[i].max_frames, sizeof(frame_t));
		pm[i].disk_reads = 0;
		pm[i].disk_writes = 0;
		pm[i].trace_count = 0;
		pm[i].flush_count = 0;
	}

	/* Attach to shared memory and semaphores created by main() */
	int shm_fd = shm_open(SHM_NAME, O_RDWR, 0666);
	if (shm_fd < 0) {
		perror("shm_open");
		exit(1);
	}

	shm_seg_t *shm = mmap(0, sizeof(shm_seg_t),
						  PROT_READ | PROT_WRITE,
						  MAP_SHARED, shm_fd, 0);
	if (shm == MAP_FAILED) {
		perror("mmap");
		exit(1);
	}
	
	sem_t *sem_empty = sem_open(SEM_EMPTY, 0);
	sem_t *sem_full = sem_open(SEM_FULL, 0);
	sem_t *sem_mutex = sem_open(SEM_MUTEX, 0);

	printf("[MM] Ready, waiting for requests...\n");

	int term_count = 0;
	while (1) {
		sem_wait(sem_full);
		sem_wait(sem_mutex);

		request_t req = shm->buffer[shm->out];
		shm->out = (shm->out + 1) % BUFFER_SIZE;    

		sem_post(sem_mutex);
		sem_post(sem_empty);

		int id = req.pm_id - 1;
		proc_state_t *proc = &pm[id];

		/* PM sends initial "count" message with expected trace size */
		if (req.op == 'C') {
			pm[id].trace_count = 0;
			pm[id].total_expected = req.page;
			total_expected = pm[0].total_expected + pm[1].total_expected;
			continue;
		}		

		/* Termination signal from PM */
		if (req.op == 'T') {
			term_count++;
			if (term_count == 2) {
				fprintf(stderr, "\n");
				printf("[MM] Received termination signals from both PMs.\n\n");
				break;
			}
			continue;
		}
		
		proc->trace_count++;

		/* Show progress (only when not in debug mode) */
		if (!DEBUG_MODE) {	
			int total_processed = pm[0].trace_count + pm[1].trace_count;
			if (total_expected > 0 && total_processed % 20000 == 0) {
				print_progress("Processing", total_processed, total_expected);
			}
		}	

		int page_id = find_page(proc, req.page);
		if (page_id >= 0) {
			/* HIT: just mark dirty if write */
			if (req.op == 'W') proc->frames[page_id].dirty = 1;  
		} else {
			/* MISS: page fault */
			proc->total_pf++;
			proc->pf_count++;

			/* FWF flush if k faults in this block */
			if (proc->pf_count > k) {
				flush_frames(proc, req.pm_id);
				proc->flush_count++;
			}

			/* Ensure space is available */
			if (proc->used_frames >= proc->max_frames) {
				flush_frames(proc, req.pm_id);
				proc->flush_count++;
			}

			/* Load page into first free frame */
			for (int i = 0; i < proc->max_frames; i++) {
				if (!proc->frames[i].valid) {
					proc->frames[i].valid = 1;
					proc->frames[i].page = req.page;
					proc->frames[i].dirty = (req.op == 'W');
					proc->used_frames++;
					if (proc->used_frames > proc->peak_frames) {
						proc->peak_frames = proc->used_frames;
					}
					proc->disk_reads++;
					hash_insert(proc->ht, req.page, i);
					break;
				}
			}
		}
	}

	/* Print final stats */
	for (int i = 0; i < 2; i++) {
		printf("=== Stats for PM%d ===\n", i+1);
		printf("Page faults : %d\n", pm[i].total_pf);
		printf("Disk reads  : %d\n", pm[i].disk_reads);
		printf("Disk writes : %d\n", pm[i].disk_writes);
		printf("Peak frames : %d\n", pm[i].peak_frames);
		printf("Frames@exit : %d/%d\n", pm[i].used_frames, pm[i].max_frames);
		printf("Entries     : %d\n", pm[i].trace_count);
		printf("Hits        : %d\n", pm[i].trace_count - pm[i].total_pf);
		printf("Hit rate    : %.2f%%\n", 
			  (pm[i].trace_count - pm[i].total_pf) * 100.0 / pm[i].trace_count);
		printf("Flushes     : %d\n", pm[i].flush_count);
		printf("\n");

		hash_destroy(pm[i].ht);
		free(pm[i].frames);
	}

	/* Cleanup resources */
	sem_close(sem_empty);
	sem_close(sem_full);
	sem_close(sem_mutex);
	munmap(shm, sizeof(shm_seg_t));
	close(shm_fd);

	return 0;
}
