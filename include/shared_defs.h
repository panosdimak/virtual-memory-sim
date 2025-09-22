#ifndef SHARED_DEFS_H
#define SHARED_DEFS_H

#include <semaphore.h>

#define SHM_NAME "/mm_shm"

/* Named semaphores for producer/consumer synchronization */
#define SEM_EMPTY "/sem_empty" /* counts empty slots in buffer */
#define SEM_FULL "/sem_full" /* counts filled slots in buffer */
#define SEM_MUTEX "/sem_mutex" /* mutual exclusion on buffer access */

/* Semaphores to enfore PM1/PM2 alternation */
#define SEM_PM1 "/sem_pm1" 
#define SEM_PM2 "/sem_pm2"

#define BUFFER_SIZE 16

extern int DEBUG_MODE;

/**
 * struct request_t - represents a single memory access request
 * @pm_id: ID of requesting process (1 = PM1, 2 = PM2)
 * @page:  page number (addr >> 12) or -1 for termination
 * @op:    operation type: 'R' (read), 'W' (write), 'C' (count init), 'T' (terminate)
 */
typedef struct {
	int pm_id;
	unsigned long page;
	char op;
} request_t;

/**
 * struct shm_seg_t - shared memory segment layout
 * @buffer: ring buffer storing memory access requests
 * @in:     producer index (next slot to write)
 * @out:    consumer index (next slot to read)
 */
typedef struct {
	request_t buffer[BUFFER_SIZE];
	int in;
	int out;
} shm_seg_t;

#endif /* SHARED_DEFS_H */