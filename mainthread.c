#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <stdio.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <inttypes.h>
#include <unistd.h>
#include <time.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <sched.h>
#include <assert.h>
#include <pthread.h>

#include "common.h"
#include "ssmp.h"
#include "measurements.h"
#include "pfd.h"

#define ROUNDTRIP
#define DEBUG_
#define NO_SYNC_SRV
//#define NO_SYNC_APP



uint32_t nm = 1000000;
uint8_t dsl_seq[80];
uint8_t num_threads = 2;
uint8_t ID;
uint8_t num_dsl = 0;
uint8_t num_app = 0;
uint8_t dsl_per_core = 2;
uint32_t delay_after = 0;
uint32_t delay_cs = 0;


void *mainthread(void *args) {
	set_cpu(id_to_core[ID]);
	/*ssmp_mem_init(ID, num_procs);*/
	printf("threads hell\n");
	pthread_exit(NULL);
}

int main(int argc, char **argv) {

	if (argc > 1)
	{
		num_threads = atoi(argv[1]);
	}
	if (argc > 2)
	{
		nm = atol(argv[2]);
	}

	if (argc > 3)
	{
		dsl_per_core = atoi(argv[3]);
	}

	if (argc > 4)
	{
		delay_after = atoi(argv[4]);
	}

	if (argc > 5)
	{
		delay_cs = atoi(argv[5]);
	}

	ssmp_init(num_threads);
	pthread_t threads[num_threads];
	int rank;
	for (rank = 0; rank < num_threads; rank++) {
		if(0 > pthread_create(threads + rank, NULL, mainthread, NULL)) {
			P("Failure in pthread():\n%s", strerror(errno));
			exit(3);
		}
	}

	pthread_exit(NULL);
}

