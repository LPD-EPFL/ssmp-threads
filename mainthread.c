#include "ssmpthread.h"
#include <stdio.h>
#include <pthread.h>
#include <stdlib.h>

uint8_t num_threads = 2;
__thread uint8_t THREAD_ID;

void *mainthread(void *args) {

	THREAD_ID = *((int*)args);
	set_thread(id_to_core[THREAD_ID]);
	ssmpthread_mem_init(THREAD_ID, num_threads);

	if (THREAD_ID == 0) {
		volatile  ssmp_msg_t *msgp;
		msgp = (volatile ssmp_msg_t *) malloc(sizeof(ssmp_msg_t));
		msgp->w0 = 5555;
		fprintf(stderr, "send msgp->w0 %d\n", msgp->w0);
		ssmpthread_send(1, msgp);
		fprintf(stderr,"message sent\n");
	} else if (THREAD_ID == 1) {
		volatile  ssmp_msg_t *msgp;
		msgp = (volatile ssmp_msg_t *) malloc(sizeof(ssmp_msg_t));
		fprintf(stderr, "waiting for message\n");
		ssmpthread_recv_from(0, msgp);
		fprintf(stderr,"recv msgp->w0 %d\n", msgp->w0);
	}
	ssmpthread_barrier_wait(0);
	free(args);
	pthread_exit(NULL);
}

int main(int argc, char **argv) {

	ssmpthread_init(num_threads);
	pthread_t threads[num_threads];
	int rank;
	for (rank = 0; rank < num_threads; rank++) {
		void * args = malloc(sizeof(int));
		*((int*)args) = rank;
		if(0 > pthread_create(threads + rank, NULL, mainthread, args)) {
			exit(3);
		}
	}

	pthread_exit(NULL);
}

