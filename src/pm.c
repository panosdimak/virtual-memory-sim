#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include "shared_defs.h"

int main(int argc, char *argv[]) {
	if (argc != 4) {
		fprintf(stderr, "Incorrect number of arguments\n");
		exit(EXIT_FAILURE);
	}
	
	int pm_id = atoi(argv[1]);
	const char *trace_file = argv[2];
	int q = atoi(argv[3]);
	
	/* Attach to shared memory created by main() */
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
	sem_t *sem_pm1 = sem_open(SEM_PM1, 0);
	sem_t *sem_pm2 = sem_open(SEM_PM2, 0);

	/* Count total lines in trace file so we can send it to MM first.
	 * Used for progress bar display and stats.
	 */
	long total_lines = 0;
	char buf[256];
	
	FILE *fp = fopen(trace_file, "r");
	if (!fp) {
		perror("fopen");
		exit(EXIT_FAILURE);
	}
	
	while (fgets(buf, sizeof(buf), fp)) {
		total_lines++;
	}

	/* Send a "C" (count) request to MM with number of lines.
	 * Lets MM know the total number of references it will process
	 * for progress tracking and final reporting.
	 */
	request_t req;
	req.pm_id = pm_id;
	req.page = total_lines;
	req.op = 'C';

	sem_wait(sem_empty);
	sem_wait(sem_mutex);
	shm->buffer[shm->in] = req;
	shm->in = (shm->in + 1) % BUFFER_SIZE;
	sem_post(sem_mutex);
	sem_post(sem_full);

	/* Reset file position for actual trace processing */
	fseek(fp, 0, SEEK_SET);

	char line[64];
	unsigned long addr;
	char op;

	int count = 0;
	/* Choose turn semaphores based on PM identity.
	 * Ensures PM1 and PM2 alternate after q requests each.
	 */
	sem_t *sem_current = (pm_id == 1) ? sem_pm1 : sem_pm2;
	sem_t *sem_other = (pm_id == 1) ? sem_pm2: sem_pm1;


	while (fgets(line, sizeof(line), fp)) {
		if (sscanf(line, "%lx %c", &addr, &op) == 2) {
			if (count == 0) {
				sem_wait(sem_current);
			}

			/* Prepare memory request */
			request_t req;
			req.pm_id = pm_id;
			req.page = addr / 4096; /* Translate address to page number */
			req.op = op; 

			/* Producer side of circular buffer */
			sem_wait(sem_empty);
			sem_wait(sem_mutex);

			shm->buffer[shm->in] = req;
			shm->in = (shm->in + 1) % BUFFER_SIZE;

			sem_post(sem_mutex);
			sem_post(sem_full);

			/* After sending q requests, hand turn to other PM */
			count++;
			if (count == q) {
				sem_post(sem_other);
				count = 0;
			}
		}
	}

	/* Send termination signal (page = -1) to MM when done */
	req.pm_id = pm_id;
	req.page = -1;
	req.op = 'T';

	sem_wait(sem_empty);
	sem_wait(sem_mutex);

	shm->buffer[shm->in] = req;
	shm->in = (shm->in + 1) % BUFFER_SIZE;

	sem_post(sem_mutex);
	sem_post(sem_full);

	fclose(fp);

	/* Cleanup resources */
	sem_close(sem_empty);
	sem_close(sem_full);
	sem_close(sem_mutex);
	sem_close(sem_pm1);
	sem_close(sem_pm2);
	munmap(shm, sizeof(shm_seg_t));
	close(shm_fd);
	
	return 0;
}