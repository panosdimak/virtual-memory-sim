#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h> 
#include <sys/types.h>
#include <signal.h>
#include <sys/wait.h>
#include <string.h>
#include "shared_defs.h"

int DEBUG_MODE = 0;

static pid_t child_pids[3];

/* Signal handler to terminate child processes on SIGINT/SIGTERM */
void handle_signal(int sig) {
	fprintf(stderr, "\nCaught signal %d, terminating children...\n", sig);
	for (int i = 0; i < 3; i++) {
		if (child_pids[i] > 0) {
			kill(child_pids[i], SIGTERM);
		}
	}
	while (wait(NULL) > 0) {};
	exit(EXIT_FAILURE);
}

void print_usage(const char *prog_name) {
	fprintf(stderr, 
			"Usage: %s k total_frames q trace1 trace2 (--debug)\n", prog_name);
}

int main(int argc, char *argv[]) {
	if (argc < 6) {
		print_usage(argv[0]);
		return EXIT_FAILURE;
	}

	for (int i = 1; i < argc; i++) {
		if (strcmp(argv[i], "--debug") == 0) {
			DEBUG_MODE = 1;
			break;
		}
	}

	/* Validate trace files before forking children */
	for (int i = 4; i <= 5; i++) {
		if (access(argv[i], R_OK) != 0) {
			fprintf(stderr, "Error: Cannot open trace file '%s'\n", argv[i]);
			exit(EXIT_FAILURE);
		}
	}

	/* Ensure graceful cleanup on Ctrl+C / kill */
	signal(SIGINT, handle_signal);
	signal(SIGTERM, handle_signal);

	/* Create and size shared memory segment */
	int shm_fd = shm_open(SHM_NAME, O_CREAT | O_RDWR, 0666);
	if (shm_fd < 0) {
		perror("shm_open");
		exit(1);
	}

	if (ftruncate(shm_fd, sizeof(shm_seg_t)) == -1) {
		perror("ftruncate");
		exit(1);
	}
	
	shm_seg_t *shm = mmap(0, sizeof(shm_seg_t),
						  PROT_READ | PROT_WRITE, 
						  MAP_SHARED, shm_fd, 0);
	if (shm == MAP_FAILED ) {
		perror("mmap");
		exit(1);
	}

	shm->in = shm->out = 0; /* initialize ring buffer */

	/* Cleanup stale semaphores from previous runs (if crashed) */
	sem_unlink(SEM_EMPTY);
	sem_unlink(SEM_FULL);
	sem_unlink(SEM_MUTEX);
	sem_unlink(SEM_PM1);
	sem_unlink(SEM_PM2);
	
	/* Initialize semaphores for circular buffer + PM alternation */
	sem_t *sem_empty = sem_open(SEM_EMPTY, O_CREAT, 0666, BUFFER_SIZE);
	sem_t *sem_full = sem_open(SEM_FULL, O_CREAT, 0666, 0);
	sem_t *sem_mutex = sem_open(SEM_MUTEX, O_CREAT, 0666, 1);
	sem_t *sem_pm1 = sem_open(SEM_PM1, O_CREAT, 0666, 1);   // PM1 starts
	sem_t *sem_pm2 = sem_open(SEM_PM2, O_CREAT, 0666, 0);   // PM2 waits

	/* Memory Manager (consumer) */
	child_pids[0] = fork();
	if (child_pids[0] < 0) {
		perror("Failed to fork memory manager");
		return EXIT_FAILURE;
	} else if (child_pids[0] == 0) {
		if (DEBUG_MODE)
			execl("./memory_manager", "memory_manager", 
				   argv[1], argv[2], "--debug", NULL);
		else
			execl("./memory_manager", "memory_manager", 
				   argv[1], argv[2], NULL);

		perror("Failed to exec memory manager");
		return EXIT_FAILURE;
	}

	/* Process Manager 1 (producer) */
	child_pids[1] = fork();
	if (child_pids[1] < 0) {
		perror("Failed to fork process manager 1");
		return EXIT_FAILURE;
	} else if (child_pids[1] == 0) {
		execl("./pm", "pm", "1", argv[4], argv[3], NULL);
		perror("Failed to exec process manager 1");
		return EXIT_FAILURE;
	}

	/* Process Manager 2 (producer) */
	child_pids[2] = fork();
	if (child_pids[2] < 0) {
		perror("Failed to fork process manager 2");
		return EXIT_FAILURE;
	} else if (child_pids[2] == 0) {
		execl("./pm", "pm", "2", argv[5], argv[3], NULL);
		perror("Failed to exec process manager 2");
		return EXIT_FAILURE;
	}
	
	/* Wait for memory manager to finish */
	int status;
	waitpid(child_pids[0], &status, 0);

	/* Cleanup resources */
	sem_close(sem_empty);
	sem_close(sem_full);
	sem_close(sem_mutex);
	sem_close(sem_pm1);
	sem_close(sem_pm2);
	
	sem_unlink(SEM_EMPTY);
	sem_unlink(SEM_FULL);
	sem_unlink(SEM_MUTEX);
	sem_unlink(SEM_PM1);
	sem_unlink(SEM_PM2);

	munmap(shm, sizeof(shm_seg_t));
	close(shm_fd);
	shm_unlink(SHM_NAME);

	return 0;
}