#ifndef _SSMP_H_
#define _SSMP_H_

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/time.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <sched.h>
#include <inttypes.h>
#include "atomic_ops.h"




/* ------------------------------------------------------------------------------- */
/* settings */
/* ------------------------------------------------------------------------------- */
extern uint8_t id_to_core[];
/* ------------------------------------------------------------------------------- */
/* defines */
/* ------------------------------------------------------------------------------- */
#define SSMP_CACHE_LINE_SIZE 64
#ifndef ALIGNED
#  if __GNUC__ && !SCC
#    define ALIGNED(N) __attribute__ ((aligned (N)))
#  else
#    define ALIGNED(N)
#  endif
#endif
/* ------------------------------------------------------------------------------- */
/* types */
/* ------------------------------------------------------------------------------- */

typedef struct ALIGNED(SSMP_CACHE_LINE_SIZE) ssmp_msg {
	int w0;
	int w1;
	int w2;
	int w3;
	int w4;
	int w5;
	int w6;
	int w7;
	int f[7];
	union {
		volatile uint32_t state;
		volatile uint32_t sender;
	};
} ssmp_msg_t;


/*type used for color-based function, i.e. functions that operate on a subset of the cores according to a color function*/
typedef struct ALIGNED(SSMP_CACHE_LINE_SIZE) ssmp_color_buf_struct {
	uint64_t num_ues;
	volatile uint32_t** buf_state;
	volatile ssmp_msg_t** buf;
	uint32_t* from;
} ssmp_color_buf_t;

/*barrier type*/
typedef struct {
	uint64_t participants;                  /*the participants of a barrier can be given either by this, as bits (0 -> no, 1 ->participate */
	int (*color)(int); /*or as a color function: if the function return 0 -> no participant, 1 -> participant. The color function has priority over the lluint participants*/
	uint32_t ticket;
	uint32_t cleared;
} ssmp_barrier_t;

volatile extern ssmp_msg_t **ssmp_recv_buf;
volatile extern ssmp_msg_t **ssmp_send_buf;


/* ------------------------------------------------------------------------------- */
/* init / term the MP system */
/* ------------------------------------------------------------------------------- */
void ssmpthread_init(int num_threads);
/* ------------------------------------------------------------------------------- */
/* sending functions : default is blocking */
/* ------------------------------------------------------------------------------- */

/* ------------------------------------------------------------------------------- */
/* help funcitons */
/* ------------------------------------------------------------------------------- */

extern void set_thread(int thread);
extern inline uint32_t get_thread();
extern inline int ssmpthread_id();
extern inline int ssmpthread_num_ues();

#endif
