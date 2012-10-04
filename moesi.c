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
#include <emmintrin.h>

#include "common.h"
#include "ssmp.h"
#include "measurements.h"

#define NO_MSG 0
#define MSG 1
#define LOCKED 7
#define FOR_SENDER 1
#define FOR_RECVER 0
#define LOCK_GET(lock, who)			\
  while (!__sync_bool_compare_and_swap(lock, who, LOCKED));
#define LOCK_GIVE(lock, who)			\
  *(lock) = who;

#define MFENCE() asm volatile("mfence")

#define B0 ssmp_barrier_wait(0);
#define B1 ssmp_barrier_wait(2);
#define B2 ssmp_barrier_wait(3);
#define B3 ssmp_barrier_wait(4);
#define B4 ssmp_barrier_wait(5);
#define B5 ssmp_barrier_wait(6);
#define B6 ssmp_barrier_wait(7);
#define B7 ssmp_barrier_wait(8);
#define B8 ssmp_barrier_wait(9);
#define B9 ssmp_barrier_wait(10);
#define B10 ssmp_barrier_wait(11);
#define B11 ssmp_barrier_wait(12);
#define B12 ssmp_barrier_wait(13);
#define B13 ssmp_barrier_wait(14);
#define B14 ssmp_barrier_wait(15);

#define FB0 barrier_wait(fbarrier + 0);
#define FB1 barrier_wait(fbarrier + 2);
#define FB2 barrier_wait(fbarrier + 3);
#define FB3 barrier_wait(fbarrier + 4);
#define FB4 barrier_wait(fbarrier + 5);
#define FB5 barrier_wait(fbarrier + 6);
#define FB6 barrier_wait(fbarrier + 7);
#define FB7 barrier_wait(fbarrier + 8);
#define FB8 barrier_wait(fbarrier + 9);
#define FB9 barrier_wait(fbarrier + 10);
#define FB10 barrier_wait(fbarrier + 11);
#define FB11 barrier_wait(fbarrier + 12);
#define FB12 barrier_wait(fbarrier + 13);
#define FB13 barrier_wait(fbarrier + 14);
#define FB14 barrier_wait(fbarrier + 15);


int num_procs = 2;

static void
barrier_wait(volatile uint32_t* barrier)
{
  uint32_t num_passed = __sync_add_and_fetch(barrier, 1);
  if (num_passed < num_procs)
    {
      while (*barrier < num_procs)
	{
	  _mm_lfence();
	}

      *barrier = 0;
      _mm_sfence();
    }
  else
    {
      while (*barrier > 0)
	{
	  _mm_lfence();
	}
    }

  _mm_mfence();
}

long long unsigned int nm = 1000000;
int ID;
ticks getticks_correction;

int main(int argc, char **argv) {
  if (argc > 1) {
    num_procs = atoi(argv[1]);
  }
  if (argc > 2) {
    nm = atol(argv[2]);
  }

  ID = 0;
  printf("NUM of processes  : %d\n", num_procs);
  printf("NUM of repetitions: %lld\n", nm);
  printf("testing moesi\n");


  ssmp_init(num_procs);

  volatile ssmp_msg_t* cache_line_anonym = (volatile ssmp_msg_t*) mmap(NULL, 4096, PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
  assert(cache_line_anonym != NULL);

  int rank;
  for (rank = 1; rank < num_procs; rank++) {
    P("Forking child %d", rank);
    pid_t child = fork();
    if (child < 0) {
      P("Failure in fork():\n%s", strerror(errno));
    } else if (child == 0) {
      goto fork_done;
    }
  }
  rank = 0;

 fork_done:
  ID = rank;
  if (argc > 3) {
    int on = atoi(argv[3 + ID]);
    P("placed on core %d", on);
    set_cpu(on);
  }
  else {
    set_cpu(ID);
  }
  ssmp_mem_init(ID, num_procs);
  getticks_correction = getticks_correction_calc();




  volatile ssmp_msg_t* cache_line;
  ssmp_barrier_wait(0);
  char keyF[100];
  sprintf(keyF,"/cache_line");
  shm_unlink(keyF);
  PRINT("opening the cache line (%s)", keyF);

  int size = 4096;// sizeof(ssmp_msg_t);

  int ssmpfd = shm_open(keyF, O_CREAT | O_EXCL | O_RDWR, S_IRWXU | S_IRWXG);
  if (ssmpfd < 0) 
    {
      if (errno != EEXIST) 
	{
	  perror("In shm_open");
	  exit(1);
	}


      ssmpfd = shm_open(keyF, O_CREAT | O_RDWR, S_IRWXU | S_IRWXG);
      if (ssmpfd < 0) 
	{
	  perror("In shm_open");
	  exit(1);
	}
    }
  else {
    P("%s newly openned", keyF);
    if (ftruncate(ssmpfd, size) < 0) {
      perror("ftruncate failed\n");
      exit(1);
    }
  }

  cache_line = (volatile ssmp_msg_t *) mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, ssmpfd, 0);
  if (cache_line == NULL)
    {
      perror("cache_line = NULL\n");
      exit(134);
    }

  ssmp_barrier_wait(0);

  volatile ssmp_msg_t *msgp; 
  posix_memalign((void *) &msgp, 64, 2 * 64);
  assert(msgp != NULL);

  msgp->w0 = 1;
  cache_line->state = NO_MSG;



  sprintf(keyF,"/fbarrier");
  PRINT("opening my lock buff (%s)", keyF);

  size = sizeof(ssmp_msg_t);

  ssmpfd = shm_open(keyF, O_CREAT | O_EXCL | O_RDWR, S_IRWXU | S_IRWXG);
  if (ssmpfd<0) {
    if (errno != EEXIST) {
      perror("In shm_open");
      exit(1);
    }
    else {
      ;
    }

    ssmpfd = shm_open(keyF, O_CREAT | O_RDWR, S_IRWXU | S_IRWXG);
    if (ssmpfd<0) {
      perror("In shm_open");
      exit(1);
    }
  }
  else {
    //P("%s newly openned", keyF);
    if (ftruncate(ssmpfd, size) < 0) {
      perror("ftruncate failed\n");
      exit(1);
    }
  }

  volatile uint32_t* fbarrier = (volatile uint32_t*) mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, ssmpfd, 0);
  if (fbarrier == NULL || (unsigned int) fbarrier == 0xFFFFFFFF) {
    perror("comb = NULL\n");
    exit(134);
  }

  uint32_t f;
  for (f = 0; f < 16; f++) 
    {
      fbarrier[f] = 0;
    }


  fbarrier[11] = 111;

  ssmp_barrier_wait(0);

  /* /\********************************************************************************* */
  /*  *  main functionality */
  /*  *********************************************************************************\/ */


#define PFI { ticks _s = getticks();
#define PFO(store)					\
  ticks _d = getticks() - _s - getticks_correction;	\
  (store) = _d; }
#define PFP(det, num_vals)					\
  {								\
    uint32_t _i; ticks _sum = 0;				\
    for (_i = 0; _i < (num_vals); _i++)				\
      {								\
	_sum += (det)[_i];					\
	printf("[%3d : %3lld] ", _i, (int64_t) det[_i]);	\
      }								\
    printf("\n");						\
    printf(" -- sum: %10llu | avg: %.1f\n",			\
	   _sum, _sum / (double) num_vals);			\
  }

  ticks* _ticks_det = (ticks*) malloc(2 * nm * sizeof(ticks));
  assert(_ticks_det != NULL);
  ticks* _ticks_det2 = _ticks_det + nm;

  uint64_t sum = 0;

  if (ID % 2 == 0) 
    {
      P("receiving core!");

      uint64_t reps;
      for (reps = 0; reps < nm; reps++)
	{
	  /* if (reps % 10 == 0) */
	  /*   { */
	  /* _mm_clflush(cache_line); */
	  /* _mm_mfence(); */
	  /* wait_cycles(5000); */
	  /*   } */

	  B0;			/* BARRIER 0 */
	  PFI;
	  cache_line->w0 = reps;
	  _mm_sfence();
	  /* sum += cache_line->w0; */
	  /* _mm_lfence(); */
	  PFO(_ticks_det[reps]);
	  B1;			/* BARRIER 1 */
	  B2;			/* BARRIER 2 */
	}
    }
  else 			/* SENDER */
    {
      P("sending core!");

      uint64_t reps;
      for (reps = 0; reps < nm; reps++)
	{
	  B0;			/* BARRIER 0 */
	  B1;			/* BARRIER 1 */
	  PFI;
	  sum += cache_line->w0;
	  _mm_lfence();
	  PFO(_ticks_det[reps]);

	  PFI;
	  cache_line->w0 = reps;
	  _mm_sfence();
	  PFO(_ticks_det2[reps]);
	  B2;			/* BARRIER 2 */
	}
    }

  PRINT(" -- sum = %llu", sum);
    

  uint32_t id;
  for (id = 0; id < num_procs; id++)
    {
      if (ID == id)
	{
	  //	  PF_PRINT;
	  PFP(_ticks_det, nm);
	  if (ID)
	    {
	      PFP(_ticks_det2, nm);
	    }
	}
      ssmp_barrier_wait(0);
    }
  ssmp_barrier_wait(10);


  shm_unlink(keyF);
  sprintf(keyF,"/cache_line");
  shm_unlink(keyF);
  ssmp_term();
  return 0;
}

