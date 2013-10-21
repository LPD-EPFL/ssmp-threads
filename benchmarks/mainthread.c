#include "ssmpthread.h"
#include <stdio.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>

uint8_t num_threads = 3;
__thread uint8_t THREAD_ID;

void *mainthread(void *args) {

	THREAD_ID = *((int*)args);
	free(args);
	set_cpu(id_to_core[THREAD_ID]);
	ssmp_mem_init(THREAD_ID, num_threads);

/**test ssmp_recv a message from 2 senders*/
	ssmp_barrier_wait(0);
	if (THREAD_ID == 2 || THREAD_ID == 0) {
		ssmp_msg_t *msgp;
		msgp = (ssmp_msg_t *) malloc(sizeof(ssmp_msg_t));
		msgp->w0 = THREAD_ID;
		fprintf(stderr, "send msgp->w0 %d\n", msgp->w0);
		ssmp_send(1, msgp);
		fprintf(stderr,"message sent\n");
		free(msgp);
	} else if (THREAD_ID == 1) {
		ssmp_msg_t *msgp;
		msgp = (ssmp_msg_t *) malloc(sizeof(ssmp_msg_t));
		fprintf(stderr, "waiting for message\n");
		ssmp_recv(msgp);
		fprintf(stderr,"recv msgp->w0 %d\n", msgp->w0);
		ssmp_recv(msgp);
		fprintf(stderr,"recv msgp->w0 %d\n", msgp->w0);
		free(msgp);
	}

	ssmp_barrier_wait(0);
	fprintf(stderr, "---------------send big-------------------------\n");
/**test send big message*/
	if (THREAD_ID == 2) {
		char *big_msg = malloc(100*sizeof(char));
		strcpy(big_msg, "greetings from thread 2");

		ssmp_send_big(0, (void*) big_msg, 100*sizeof(char));
		free(big_msg);
	} else if (THREAD_ID == 0) {
		char *big_msg = malloc(100*sizeof(char));

		ssmp_recv_from_big(2, (void*) big_msg, 100*sizeof(char));
		fprintf(stderr, "received %s\n", big_msg);
		free(big_msg);
	}

	ssmp_barrier_wait(0);
	fprintf(stderr, "---------------send no sync-------------------------\n");
	if (THREAD_ID == 0) {
		ssmp_msg_t *msgp;
		msgp = (ssmp_msg_t *) malloc(sizeof(ssmp_msg_t));
		msgp->w0 = 8888;
		ssmp_send_no_sync(1, msgp);
		free(msgp);
	} else if (THREAD_ID == 1) {
		ssmp_msg_t *msgp;
		msgp = (ssmp_msg_t *) malloc(sizeof(ssmp_msg_t));
		fprintf(stderr, "waiting for no sync message\n");
		ssmp_recv(msgp);
		fprintf(stderr,"recv msgp->w0 %d\n", msgp->w0);
	}


	pthread_exit(NULL);
}

int main(int argc, char **argv) {

	ssmp_init(num_threads);
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

