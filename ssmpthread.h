#ifndef _SSMP_H_
#define _SSMP_H_

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

#ifdef __sparc__
#  define SPARC_SMALL_MSG
#  include <sys/types.h>
#  include <sys/processor.h>
#  include <sys/procset.h>
#elif defined(__tile__)
#  define TILE_SMALL_MSG
#  include <tmc/alloc.h>
#  include <tmc/cpus.h>
#  include <tmc/task.h>
#  include <tmc/udn.h>
#  include <tmc/spin.h>
#  include <tmc/sync.h>
#  include <tmc/cmem.h>
#  include <arch/cycle.h> 
#endif /* __sparc */

#ifdef PLATFORM_NUMA
#  include <emmintrin.h>
#  include <numa.h>
#endif /* PLATFORM_NUMA */

#include "measurements.h"

/* ------------------------------------------------------------------------------- */
/* settings */
/* ------------------------------------------------------------------------------- */
#define SSMP_MEM_NAME "/ssmp_mem2"

#define USE_ATOMIC_
#define WAIT_TIME 66

extern uint8_t id_to_core[];
extern const uint8_t node_to_node_hops[8][8];
/* ------------------------------------------------------------------------------- */
/* defines */
/* ------------------------------------------------------------------------------- */
#define SSMP_NUM_BARRIERS 16 /*number of available barriers*/
#define SSMP_CHUNK_SIZE 1020

#define SSMP_CACHE_LINE_SIZE 64

#if defined(__sparc__)
#  if defined(SPARC_SMALL_MSG)
#    define SSMP_CACHE_LINE_DW   2
#  else
#    define SSMP_CACHE_LINE_DW   7
#  endif
#elif defined(__tilepro__)
#  if defined(TILE_SMALL_MSG)
#    define SSMP_CACHE_LINE_W   4
#  else
#    define SSMP_CACHE_LINE_SIZE 64
#    define SSMP_CACHE_LINE_W    16
#  endif
#endif


#define BUF_EMPTY 0
#define BUF_MESSG 1
#define BUF_LOCKD 2


#if defined(__sparc__)
#  define PREFETCHW(x) 
#  define PREFETCH(x) 
#  define PREFETCHNTA(x) 
#  define PREFETCHT0(x) 
#  define PREFETCHT1(x) 
#  define PREFETCHT2(x) 

#  define _mm_pause() PAUSE
#  define _mm_mfence() __asm__ __volatile__("membar #LoadLoad | #LoadStore | #StoreLoad | #StoreStore");
#  define _mm_lfence() __asm__ __volatile__("membar #LoadLoad | #LoadStore");
#  define _mm_sfence() __asm__ __volatile__("membar #StoreLoad | #StoreStore");
#elif defined(__tile__)
#  define PREFETCHW(x) 
#  define PREFETCH(x) 
#  define PREFETCHNTA(x) 
#  define PREFETCHT0(x) 
#  define PREFETCHT1(x) 
#  define PREFETCHT2(x) 
#else  /* !__sparc__ */
#  define PREFETCHW(x) asm volatile("prefetchw %0" :: "m" (*(unsigned long *)x)) /* write */
#  define PREFETCH(x) asm volatile("prefetch %0" :: "m" (*(unsigned long *)x)) /* read */
#  define PREFETCHNTA(x) asm volatile("prefetchnta %0" :: "m" (*(unsigned long *)x)) /* non-temporal */
#  define PREFETCHT0(x) asm volatile("prefetcht0 %0" :: "m" (*(unsigned long *)x)) /* all levels */
#  define PREFETCHT1(x) asm volatile("prefetcht1 %0" :: "m" (*(unsigned long *)x)) /* all but L1 */
#  define PREFETCHT2(x) asm volatile("prefetcht2 %0" :: "m" (*(unsigned long *)x)) /* all but L1 & L2 */
#endif /* !__sparc__ */
#define SP(args...) printf("[%d] ", ssmp_id_); printf(args); printf("\n"); fflush(stdout)
#ifdef SSMP_DEBUG
#  define PD(args...) printf("[%d] ", ssmp_id_); printf(args); printf("\n"); fflush(stdout)
#else
#  define PD(args...) 
#endif

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
typedef uint32_t ssmp_chk_t; /*used for the checkpoints*/

/*msg type: contains 15 words of data and 1 word flag*/
#if defined(SPARC_SMALL_MSG)
typedef struct ALIGNED(64) ssmp_msg 
{
  int32_t w0;
  int32_t w1;
  int32_t w2;
  int32_t w3;
  uint8_t pad[3];
  union 
  {
    volatile uint8_t state;
    volatile uint8_t sender;
  };
#elif defined(TILE_SMALL_MSG)
typedef struct ALIGNED(64) ssmp_msg 
{
  int32_t w0;
  int32_t w1;
  int32_t w2;
  uint8_t pad[3];
  union 
  {
    volatile uint8_t state;
    volatile uint8_t sender;
  };
#else
typedef struct ALIGNED(SSMP_CACHE_LINE_SIZE) ssmp_msg 
{
  int w0;
  int w1;
  int w2;
  int w3;
  int w4;
  int w5;
  int w6;
  int w7;
  int f[7];
  union 
  {
#  if defined(__sparc__)
    volatile uint8_t state;
    volatile uint8_t sender;
#  else
    volatile uint32_t state;
    volatile uint32_t sender;
#  endif
  };
#endif	/* __sparc__ */
} ssmp_msg_t;

typedef struct 
{
  unsigned char data[SSMP_CHUNK_SIZE];
  int state;
} ssmp_chunk_t;

/*type used for color-based function, i.e. functions that operate on a subset of the cores according to a color function*/
typedef struct ALIGNED(SSMP_CACHE_LINE_SIZE) ssmp_color_buf_struct
{
#if defined(__sparc__)
  uint64_t num_ues;
  volatile uint8_t** buf_state;
  volatile ssmp_msg_t** buf;
  uint8_t* from;
#else
  uint64_t num_ues;
  volatile uint32_t** buf_state;
  volatile ssmp_msg_t** buf;
  uint32_t* from;
#endif	/* __sparc__ */
} ssmp_color_buf_t;


#if defined(__tile__)
typedef tmc_sync_barrier_t ssmp_barrier_t;
#else
/*barrier type*/
typedef struct 
{
  uint64_t participants;                  /*the participants of a barrier can be given either by this, as bits (0 -> no, 1 ->participate */
  int (*color)(int); /*or as a color function: if the function return 0 -> no participant, 1 -> participant. The color function has priority over the lluint participants*/
  uint32_t ticket;
  uint32_t cleared;
} ssmp_barrier_t;
#endif

volatile extern ssmp_msg_t **ssmp_recv_buf;
volatile extern ssmp_msg_t **ssmp_send_buf;

#if defined(TILERA)
extern cpu_set_t cpus;
#  if defined(__tilepro__)
#    define SSMP_MSG_NUM_WORDS 16
#  else
#    define SSMP_MSG_NUM_WORDS 8
#  endif
#endif

/* ------------------------------------------------------------------------------- */
/* init / term the MP system */
/* ------------------------------------------------------------------------------- */

/* initialize the system: called before forking */
extern void ssmpthread_init(int num_procs);
/* initilize the memory structures of the system: called after forking by every proc */
extern void ssmpthread_mem_init(int id, int num_ues);
/* terminate the system */
extern void ssmp_term(void);

/* ------------------------------------------------------------------------------- */
/* sending functions : default is blocking */
/* ------------------------------------------------------------------------------- */

/* blocking send length words to to */
/* blocking in the sense that the data are copied to the receiver's buffer */
extern inline void ssmpthread_send(uint32_t to, volatile ssmp_msg_t *msg);
extern inline void ssmpthread_send_no_sync(uint32_t to, volatile ssmp_msg_t *msg);

/* ------------------------------------------------------------------------------- */
/* broadcasting functions */
/* ------------------------------------------------------------------------------- */

/* broadcast length bytes using blocking sends */
extern inline void ssmpthread_broadcast(ssmp_msg_t *msg);

/* ------------------------------------------------------------------------------- */
/* receiving functions : default is blocking */
/* ------------------------------------------------------------------------------- */

/* blocking receive from process from length bytes */
extern inline void ssmpthread_recv_from(uint32_t from, volatile ssmp_msg_t *msg);

/* blocking receive from any proc. 
   Sender at msg->sender */
extern inline void ssmpthread_recv(ssmp_msg_t *msg, int length);

/* ------------------------------------------------------------------------------- */
/* color-based recv fucntions */
/* ------------------------------------------------------------------------------- */

/* initialize the color buf data structure to be used with consequent ssmp_recv_color calls. A node is considered a participant if the call to color(ID) returns 1 */
extern void ssmp_color_buf_init(ssmp_color_buf_t *cbuf, int (*color)(int));
extern void ssmp_color_buf_free(ssmp_color_buf_t *cbuf);

/* blocking receive from any of the participants according to the color function */
extern inline void ssmpthread_recv_color(ssmp_color_buf_t *cbuf, ssmp_msg_t *msg);
extern inline void ssmpthread_recv_color_start(ssmp_color_buf_t *cbuf, ssmp_msg_t *msg);

/* ------------------------------------------------------------------------------- */
/* barrier functions */
/* ------------------------------------------------------------------------------- */
extern int color_app(int id);

extern inline ssmp_barrier_t * ssmpthread_get_barrier(int barrier_num);
extern inline void ssmpthread_barrier_init(int barrier_num, long long int participants, int (*color)(int));

extern inline void ssmpthread_barrier_wait(int barrier_num);


/* ------------------------------------------------------------------------------- */
/* help funcitons */
/* ------------------------------------------------------------------------------- */
extern inline double wtime(void);
extern inline void wait_cycles(uint64_t cycles);
extern inline void _mm_pause_rep(uint32_t num_reps);
extern inline uint32_t get_num_hops(uint32_t core1, uint32_t core2);

extern void set_thread(int cpu);
extern inline uint32_t get_thread();

typedef uint64_t ticks;
extern inline ticks getticks(void);
extern ticks getticks_correction;
extern ticks getticks_correction_calc();

/// Round up to next higher power of 2 (return x if it's already a power
/// of 2) for 32-bit numbers
extern inline uint32_t pow2roundup (uint32_t x);

#define my_random xorshf96

/* 
 * Returns a pseudo-random value in [1;range).
 * Depending on the symbolic constant RAND_MAX>=32767 defined in stdlib.h,
 * the granularity of rand() could be lower-bounded by the 32767^th which might 
 * be too high for given values of range and initial.
 */

//Marsaglia's xorshf generator
static inline unsigned long
xorshf96(unsigned long* x, unsigned long* y, unsigned long* z) {          //period 2^96-1
  unsigned long t;
  (*x) ^= (*x) << 16;
  (*x) ^= (*x) >> 5;
  (*x) ^= (*x) << 1;

  t = *x;
  (*x) = *y;
  (*y) = *z;
  (*z) = t ^ (*x) ^ (*y);

  return *z;
}


extern inline uint32_t ssmp_cores_on_same_socket(uint32_t core1, uint32_t core2);
extern inline int ssmpthreead_id();
extern inline int ssmp_num_ues();

#if defined(__sparc__)
static inline void
memcpy64(volatile uint64_t* dst, const uint64_t* src, const size_t dwords)
{
#  if defined(SPARC_SMALL_MSG)

  /* uint32_t* dst32 = (uint32_t*) (dst); */
  /* uint32_t* src32 = (uint32_t*) (src); */
  /* uint8_t* dst8 = (uint8_t*) (dst); */
  /* uint8_t* src8 = (uint8_t*) (dst); */

  dst[0] = src[0];
  dst[1] = src[1];
  /* dst32[2] = src32[2]; */
  /* dst8[12] = src8[12]; */
#  else
  uint32_t w;
  for (w = 0; w < dwords; w++)
    {
      *dst++ = *src++;
    }
  _mm_mfence();
#  endif
}

#endif


#endif