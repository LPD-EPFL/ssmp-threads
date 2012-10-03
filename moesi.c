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
  printf("NUM of processes: %d\n", num_procs);
  printf("NUM of msgs: %lld\n", nm);
  printf("testing moesi\n");


  ssmp_init(num_procs);

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

  int size =  sizeof(ssmp_msg_t);

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


  ssmp_barrier_wait(0);

  /* /\********************************************************************************* */
  /*  *  main functionality */
  /*  *********************************************************************************\/ */


  if (ID % 2 == 0) 
    {
      P("receiving core!");

      uint64_t reps;
      for (reps = 0; reps < nm; reps++)
	{

	}

    }
  else 			/* SENDER */
    {
      P("sending core!");

      uint64_t reps;
      for (reps = 0; reps < nm; reps++)
	{

	}

    }
    

  uint32_t id;
  for (id = 0; id < num_procs; id++)
    {
      if (ID == id)
	{
	  PF_PRINT;
	}
      ssmp_barrier_wait(0);
    }
  ssmp_barrier_wait(0);


  shm_unlink(keyF);
  sprintf(keyF,"/cache_line");
  shm_unlink(keyF);
  ssmp_term();
  return 0;
}

