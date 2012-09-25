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

static inline void valset(volatile unsigned int *point, int ret) {
  __asm__ __volatile__("xchgl %1, %0"
                       : "=r"(ret)
                       : "m"(*point), "0"(ret)
                       : "memory");
}


typedef struct ticket_lock {
  unsigned int current;
  unsigned int ticket;
} ticket_lock_t;

__attribute__ ((always_inline)) void tlock(ticket_lock_t * lock) {
  unsigned int ticket = __sync_add_and_fetch (&lock->ticket, 1);
  while (ticket != lock->current);
}

__attribute__ ((always_inline)) void tunlock(ticket_lock_t * lock) {
  lock->current++;
}


int num_procs = 2;
long long int nm = 100000000;
int ID, wcycles = 30;
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
  printf("app guys sending to ID-1!\n");

  getticks_correction = getticks_correction_calc();

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



  volatile ssmp_msg_t* local;
  volatile ssmp_msg_t* remote;
  ssmp_barrier_wait(0);
  char keyF[100];
  sprintf(keyF,"/ssmp_to%02d_from%02d", ID, !ID);
  PRINT("opening my local buff (%s)", keyF);

  int size = 5 * sizeof(ssmp_msg_t);

  int ssmpfd = shm_open(keyF, O_CREAT | O_EXCL | O_RDWR, S_IRWXU | S_IRWXG);
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

  local = (volatile ssmp_msg_t *) mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, ssmpfd, 0);
  if (local == NULL || (unsigned int) local == 0xFFFFFFFF) {
    perror("comb = NULL\n");
    exit(134);
  }

  ssmp_barrier_wait(0);

  sprintf(keyF,"/ssmp_to%02d_from%02d", !ID, ID);
  PRINT("opening my remote buff (%s)", keyF);

  size = 5 * sizeof(ssmp_msg_t);

  
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

  remote = (volatile ssmp_msg_t *) mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, ssmpfd, 0);
  if (remote == NULL || (unsigned int) remote == 0xFFFFFFFF) {
    perror("comb = NULL\n");
    exit(134);
  }

  volatile ssmp_msg_t *msgp; 
  posix_memalign(&msgp, 64, 2 * 64); /* ********************* */
  //  msgp = (volatile ssmp_msg_t *) malloc(2 * sizeof(ssmp_msg_t));
  assert(msgp != NULL);

  uint32_t* msgpui = (uint32_t*) msgp;
  while ((uint64_t)msgpui % 64)
    {
      PRINT("aligning");
      msgpui++;
    }
  msgp = (ssmp_msg_t*) msgpui;

  msgp->w0 = 1;

  {
    uint32_t* combpui = (uint32_t*) local;
    uint32_t i = 0;
    printf("w %02d @ %p -- %llu\n", i, (combpui + i), ((uint64_t) (combpui + i) % 64));
  }


  uint8_t cur = 0;
  local[0].state = NO_MSG;
  local[1].state = NO_MSG;

  uint32_t* lock;
  if (ID)
    {
      lock = (uint32_t*) &remote[2].state;
    }
  else 
    {
      lock = (uint32_t*) &local[2].state;
    }

  *lock = 0;

  PF_MSG(1, "receiving");
  PF_MSG(2, "sending");
  PF_MSG(3, "wait while(!recv)");

  ssmp_barrier_wait(0);

  /* /\********************************************************************************* */
  /*  *  main functionality */
  /*  *********************************************************************************\/ */

  ticks wtimes = 0, wc;

  if (ID % 2 == 0) 
    {
      P("service core!");

      unsigned int old = nm - 1;

      wc = atoi(argv[5]);
      PRINT("wc = %d", (int) wc);

      while(1) 
	{
	  LOCK_GET(lock, ID);

	  //	  PREFETCH((local + cur));
	  /* PF_START(1); */
	  
	  /************************************************************* receive */
	  uint32_t* state = &local[cur].state;
	  LOCK_GIVE(lock, !ID);
	  LOCK_GET(lock, ID);
	  LOCK_GIVE(lock, !ID);
	  _mm_sfence();
	  wait_cycles(wc);


	  //	  PREFETCHW((&local[!cur].state));

	  PF_START(3);
	  //	  PREFETCHW(state);
	  while (*state != MSG)
	    {
	      wait_cycles(150);
	      //	      PREFETCHW(state);
	      wtimes++;
	      _mm_lfence();
	    }
	  //	  while (local[cur].state != MSG);

	  memcpy(msgp, local + cur, 64);
	  local[cur].state = NO_MSG;
	  //	  local[!cur].state = NO_MSG;
	  _mm_mfence();
	  PF_STOP(3);
	  /* PF_STOP(1); */

	  /************************************************************* end recv */
	  /************************************************************* send */
	  PF_START(2);
	  PREFETCHW((remote + cur));

	  msgp->state = MSG;
	  memcpy(remote + cur, msgp, 64);
	  _mm_mfence();
	  PF_STOP(2);

	  //	  cur = !cur;
	  /************************************************************* end send */

	  if (msgp->w0 == 0) {
	    PRINT("done");
	    break;
	  }

	  if (msgp->w0 != old) {
	    PRINT("w0 -- expected %d, got %d", old, msgp->w0);
	  }

	  old--;

	}
      PRINT("wtimes = %llu | avg = %f", wtimes, wtimes / (double) nm);
    }
  else 			/* SENDER */
    {
      P("app core!");
      long long int nm1 = nm;
      uint32_t wb = 1;

	
      while (nm1--)
	{

	  msgp->w0 = nm1;
	  msgp->w1 = nm1;
	  msgp->w2 = nm1;
	  msgp->w3 = nm1;
	  msgp->w4 = nm1;
	  msgp->w5 = nm1;
	  msgp->w6 = nm1;
	  msgp->w7 = nm1;
	  

	  /* while (remote[cur].state != NO_MSG) */
	  /*   { */
	  /*     wtimes++; */
	  /*   } */
	  LOCK_GET(lock, ID);
	  LOCK_GIVE(lock, !ID);
	  LOCK_GET(lock, ID);


	  /************************************************************* send */
	  PF_START(2);
	  msgp->state = MSG;
	  PREFETCHW((remote + cur));
	  //	  memcpy(remote + cur, msgp, 64);
	  remote[cur].w0 = msgp->w0;
	  remote[cur].state = MSG;
	  _mm_sfence();
	  PF_STOP(2);

	  LOCK_GIVE(lock, !ID);

	  //PRINT("%llu -- %d", nm1, cur);
	  /************************************************************* recv */
	  uint32_t w = 0;

	  PF_START(1);
	  //	  PREFETCHW(&local[!cur]);
	  PREFETCHW((local + cur));
	  /* PF_START(3); */
	  while (local[cur].state != MSG)
	    {
	      wtimes++;
	      PREFETCHW((local+cur));
	      _mm_pause();	      _mm_pause();
	    }
	  wb = w;
	  /* PF_STOP(3); */

	  memcpy(msgp, local + cur, 64);

	  //	  local[!cur].state = NO_MSG;
	  local[cur].state = NO_MSG;
	  PF_STOP(1);
	  //	  cur = !cur;
	}

      PRINT("wtimes = %llu | avg = %f", wtimes, wtimes / (double) nm);
    }


  uint64_t nm1;
  for (nm1 = 0; nm1 < num_procs; nm1++)
    {
      if (ID == nm1)
  	{
	  PF_PRINT;
	  //	  printf("[%02d] %lld ticks/msg\n", ID, (long long unsigned int) _ticksm);

	  /* printf("sent %lld msgs\n\t" */
	  /* 	 "in %f secs\n\t" */
	  /* 	 "%.2f msgs/us\n\t" */
	  /* 	 "%f ns latency\n" */
	  /* 	 "in ticks:\n\t" */
	  /* 	 "in %lld ticks\n\t" */
	  /* 	 "%lld ticks/msg\n", nm, _time, ((double)nm/(1000*1000*_time)), lat, */
	  /* 	 (long long unsigned int) _ticks, (long long unsigned int) _ticksm); */
    	}
      ssmp_barrier_wait(0);
    }
  ssmp_barrier_wait(0);

  shm_unlink(keyF);
  ssmp_term();
  return 0;
}

