#include "ssmpthread.h"
#include <stdio.h>
#include <stdlib.h>
#include "common.h"

uint8_t ID;
uint8_t num_threads = 2;

void *mainthread(void *args) {
	set_thread(id_to_core[0]);
	/*ssmp_mem_init(ID, num_procs);*/
	pthread_exit(NULL);
}

int main(int argc, char **argv) {

	//ssmpthread_init(num_threads);
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

