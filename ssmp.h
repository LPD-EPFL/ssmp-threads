#ifndef _SSMP_H_
#define _SSMP_H_

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <sched.h>
#include <inttypes.h>
#include <emmintrin.h>
#ifdef PLATFORM_NUMA
#include <numa.h>
#endif /* PLATFORM_NUMA */

/* ------------------------------------------------------------------------------- */
/* defines */
/* ------------------------------------------------------------------------------- */
#define SSMP_NUM_BARRIERS 16 /*number of available barriers*/
#define SSMP_CHUNK_SIZE 1020
#define SSMP_CACHE_LINE_SIZE 64

#define BUF_EMPTY 0
#define BUF_MESSG 1
#define BUF_LOCKD 2

#define USE_ATOMIC


#define SP(args...) printf("[%d] ", ssmp_id_); printf(args); printf("\n"); fflush(stdout)
#ifdef SSMP_DEBUG
#define PD(args...) printf("[%d] ", ssmp_id_); printf(args); printf("\n"); fflush(stdout)
#else
#define PD(args...) 
#endif

#ifndef ALIGNED
#  if __GNUC__ && !SCC
#    define ALIGNED(N) __attribute__ ((aligned (N)))
#  else
#    define ALIGNED(N)
#  endif
#endif

#define USE_MEMCPY

#ifdef USE_MEMCPY
#define CPY_LLINTS(to, from, length)		\
  memcpy(to, from, length)
#else
#define CPY_LLINTS(to, from, num)		\
  switch (num/sizeof(int)) {			\
  case 1:					\
    to->w0 = from->w0;				\
    break;					\
  case 2:					\
    to->w0 = from->w0;				\
    to->w1 = from->w1;				\
    break;					\
  case 3:					\
    to->w0 = from->w0;				\
    to->w1 = from->w1;				\
    to->w2 = from->w2;				\
    break;					\
  case 4:					\
    to->w0 = from->w0;				\
    to->w1 = from->w1;				\
    to->w2 = from->w2;				\
    to->w3 = from->w3;				\
    break;					\
  case 5:					\
    to->w0 = from->w0;				\
    to->w1 = from->w1;				\
    to->w2 = from->w2;				\
    to->w3 = from->w3;				\
    to->w4 = from->w4;				\
    break;					\
  case 6:					\
    to->w0 = from->w0;				\
    to->w1 = from->w1;				\
    to->w2 = from->w2;				\
    to->w3 = from->w3;				\
    to->w4 = from->w4;				\
    to->w5 = from->w5;				\
    break;					\
  default:					\
    memcpy(to, from, sizeof(int)*num);		\
  }
#endif /* USE_MEMCPY */

#define WAIT_TIME 66
#define ssmp_recv_fromm(from, msg)					\
  volatile ssmp_msg_t *tmpmr = ssmp_recv_buf[from];			\
  while (!__sync_bool_compare_and_swap(&tmpmr->state, BUF_MESSG, BUF_LOCKD)) { \
    wait_cycles(WAIT_TIME);						\
  }									\
  msg->w0 = tmpmr->w0;							\
  msg->w1 = tmpmr->w1;							\
  msg->w2 = tmpmr->w2;							\
  msg->w3 = tmpmr->w3;							\
  msg->w4 = tmpmr->w4;							\
  msg->w5 = tmpmr->w5;							\
  tmpmr->state = BUF_EMPTY;


#define ssmp_sendm(to, msg)						\
  volatile ssmp_msg_t *tmpm = tmpm = ssmp_send_buf[to];				\
  while (!__sync_bool_compare_and_swap(&tmpm->state, BUF_EMPTY, BUF_LOCKD)) { \
    wait_cycles(WAIT_TIME);						\
  }									\
  tmpm->w0 = msg->w0;							\
  tmpm->w1 = msg->w1;							\
  tmpm->w2 = msg->w2;							\
  tmpm->w3 = msg->w3;							\
  tmpm->w4 = msg->w4;							\
  tmpm->w5 = msg->w5;							\
  tmpm->state = BUF_MESSG;


/* ------------------------------------------------------------------------------- */
/* types */
/* ------------------------------------------------------------------------------- */
typedef int ssmp_chk_t; /*used for the checkpoints*/

/*msg type: contains 15 words of data and 1 word flag*/
typedef struct ALIGNED(64) ssmp_msg {
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

  //  unsigned int sender;
  //  volatile unsigned int state;
} ssmp_msg_t;

typedef struct {
  unsigned char data[SSMP_CHUNK_SIZE];
  int state;
} ssmp_chunk_t;

/*type used for color-based function, i.e. functions that operate on a subset of the cores according to a color function*/
typedef ALIGNED(64) struct {
  uint64_t num_ues;
  volatile uint32_t** buf_state;
  volatile ssmp_msg_t** buf;
  uint32_t* from;
  /* int32_t pad[8]; */
} ssmp_color_buf_t;


/*barrier type*/
typedef struct {
  uint64_t participants;                  /*the participants of a barrier can be given either by this, as bits (0 -> no, 1 ->participate */
  int (*color)(int); /*or as a color function: if the function return 0 -> no participant, 1 -> participant. The color function has priority over the lluint participants*/
  ssmp_chk_t * checkpoints; /*the checkpoints array used for sync*/
  uint32_t version; /*the current version of the barrier, used to make a barrier reusable*/
} ssmp_barrier_t;

volatile extern ssmp_msg_t **ssmp_recv_buf;
volatile extern ssmp_msg_t **ssmp_send_buf;

#define PREFETCHW(x) asm volatile("prefetchw %0" :: "m" (*(unsigned long *)x))
#define PREFETCH(x) asm volatile("prefetch %0" :: "m" (*(unsigned long *)x))
#define PREFETCHNTA(x) asm volatile("prefetchnta %0" :: "m" (*(unsigned long *)x))

/* ------------------------------------------------------------------------------- */
/* init / term the MP system */
/* ------------------------------------------------------------------------------- */

/* initialize the system: called before forking */
extern void ssmp_init(int num_procs);
/* initilize the memory structures of the system: called after forking by every proc */
extern void ssmp_mem_init(int id, int num_ues);
/* terminate the system */
extern void ssmp_term(void);

/* ------------------------------------------------------------------------------- */
/* sending functions : default is blocking */
/* ------------------------------------------------------------------------------- */

/* blocking send length words to to */
/* blocking in the sense that the data are copied to the receiver's buffer */
extern inline void ssmp_send(uint32_t to, volatile ssmp_msg_t *msg, uint32_t length);
extern inline void ssmp_send_sig(int to);
extern inline void ssmp_send_big(int to, void *data, int length);

/* blocking until the receivers reads the data */
extern inline void ssmp_sendb(int to, ssmp_msg_t *msg, int length);
/* blocking send from 1 to 6 words to to */
extern inline void ssmp_send1(int to, int w0);
extern inline void ssmp_send2(int to, int w0, int w1);
extern inline void ssmp_send3(int to, int w0, int w1, int w2);
extern inline void ssmp_send4(int to, int w0, int w1, int w2, int w3);
extern inline void ssmp_send5(int to, int w0, int w1, int w2, int w3, int w4);
extern inline void ssmp_send6(int to, int w0, int w1, int w2, int w3, int w4, int w5);

/* non-blocking send length words
   returns 1 if successful, 0 if not */
extern inline int ssmp_send_try(int to, ssmp_msg_t *msg, int length);
/* non-blocking send from 1 to 6 words */
extern inline int ssmp_send_try1(int to, int w0);
extern inline int ssmp_send_try2(int to, int w0, int w1);
extern inline int ssmp_send_try3(int to, int w0, int w1, int w2);
extern inline int ssmp_send_try4(int to, int w0, int w1, int w2, int w3);
extern inline int ssmp_send_try5(int to, int w0, int w1, int w2, int w3, int w4);
extern inline int ssmp_send_try6(int to, int w0, int w1, int w2, int w3, int w4, int w5);


/* ------------------------------------------------------------------------------- */
/* broadcasting functions */
/* ------------------------------------------------------------------------------- */

/* broadcast length bytes using blocking sends */
extern inline void ssmp_broadcast(ssmp_msg_t *msg, int length);
/* broadcast 1 to 7 words using blocking sends */
extern inline void ssmp_broadcast1(int w0);
extern inline void ssmp_broadcast2(int w0, int w1);
extern inline void ssmp_broadcast3(int w0, int w1, int w2);
extern inline void ssmp_broadcast4(int w0, int w1, int w2, int w3);
extern inline void ssmp_broadcast5(int w0, int w1, int w2, int w3, int w4);
extern inline void ssmp_broadcast6(int w0, int w1, int w2, int w3, int w4, int w5);
extern inline void ssmp_broadcast7(int w0, int w1, int w2, int w3, int w4, int w5, int w6);
/* broadcast 1 to 7 words using non-blocking sends (~parallelize sends) */
extern inline void ssmp_broadcast_par(int w0, int w1, int w2, int w3); //XXX: fix perf

/* ------------------------------------------------------------------------------- */
/* receiving functions : default is blocking */
/* ------------------------------------------------------------------------------- */

/* blocking receive from process from length bytes */
extern inline void ssmp_recv_from(uint32_t from, volatile ssmp_msg_t *msg, uint32_t length);
extern inline volatile ssmp_msg_t * ssmp_recv_fromp(int from);
extern inline void ssmp_recv_rls(int from);
extern inline void ssmp_recv_from_sig(int from);
extern inline void ssmp_recv_from_big(int from, void *data, int length);

/* blocking receive from process from 6 bytes */
extern inline void ssmp_recv_from6(int from, ssmp_msg_t *msg);
/* non-blocking receive from process from
   returns 1 if recved msg, else 0 */
extern inline int ssmp_recv_from_try(int from, ssmp_msg_t *msg, int length);
extern inline uint32_t ssmp_recv_from_test(uint32_t from);
extern inline int ssmp_recv_from_try1(int from, ssmp_msg_t *msg);
extern inline int ssmp_recv_from_try6(int from, ssmp_msg_t *msg);
/* blocking receive from any proc. 
   Sender at msg->sender */
extern inline void ssmp_recv(ssmp_msg_t *msg, int length);
extern inline void ssmp_recv1(ssmp_msg_t *msg);
extern inline void ssmp_recv6(ssmp_msg_t *msg);
/* blocking receive from any proc. 
   returns 1 if recved msg, else 0
   Sender at msg->sender */
extern inline int ssmp_recv_try(ssmp_msg_t *msg, int length);
extern inline int ssmp_recv_try6(ssmp_msg_t *msg);


/* ------------------------------------------------------------------------------- */
/* color-based recv fucntions */
/* ------------------------------------------------------------------------------- */

/* initialize the color buf data structure to be used with consequent ssmp_recv_color calls. A node is considered a participant if the call to color(ID) returns 1 */
extern void ssmp_color_buf_init(ssmp_color_buf_t *cbuf, int (*color)(int));
extern void ssmp_color_buf_free(ssmp_color_buf_t *cbuf);

/* blocking receive from any of the participants according to the color function */
extern inline void ssmp_recv_color(ssmp_color_buf_t *cbuf, ssmp_msg_t *msg, int length);
extern inline uint32_t ssmp_recv_color_start(ssmp_color_buf_t *cbuf, ssmp_msg_t *msg, uint32_t start_from);
extern inline void ssmp_recv_color4(ssmp_color_buf_t *cbuf, ssmp_msg_t *msg);
extern inline void ssmp_recv_color6(ssmp_color_buf_t *cbuf, ssmp_msg_t *msg);

/* ------------------------------------------------------------------------------- */
/* barrier functions */
/* ------------------------------------------------------------------------------- */
extern int color_app(int id);

extern inline ssmp_barrier_t * ssmp_get_barrier(int barrier_num);
extern inline void ssmp_barrier_init(int barrier_num, long long int participants, int (*color)(int));

extern inline void ssmp_barrier_wait(int barrier_num);


/* ------------------------------------------------------------------------------- */
/* help funcitons */
/* ------------------------------------------------------------------------------- */
extern inline double wtime(void);
extern inline void wait_cycles(unsigned int cycles);
extern void set_cpu(int cpu);

typedef uint64_t ticks;
extern inline ticks getticks(void);


extern inline int ssmp_id();
extern inline int ssmp_num_ues();

#endif
