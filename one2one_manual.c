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

#define NO_MSG 0
#define MSG 1
#define LOCKED 7

#define MFENCE() asm volatile("mfence")

static inline void valset(volatile unsigned int *point, int ret) {
  __asm__ __volatile__("xchgl %1, %0"
                       : "=r"(ret)
                       : "m"(*point), "0"(ret)
                       : "memory");
}

static inline void wait_cycles(unsigned int cycles) {
  cycles /= 6;
  while (cycles--) {
      _mm_pause();
  }
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

int main(int argc, char **argv) {
  if (argc > 1) {
    num_procs = atoi(argv[1]);
  }
  if (argc > 2) {
    nm = atol(argv[2]);
  }

  wcycles = atoi(argv[5]);
  P("wait cycles = %d", wcycles);

  ID = 0;
  printf("NUM of processes: %d\n", num_procs);
  printf("NUM of msgs: %lld\n", nm);
  printf("app guys sending to ID-1!\n");

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

  if (ssmp_id() > 0) {
    ssmp_barrier_wait(0);
  }

  int size = 2 * sizeof(ssmp_msg_t);

  P("opening after forking");
  char keyF[100];
  sprintf(keyF,"/ssmp_to00_from01");
  int ssmpfd = shm_open(keyF, O_CREAT | O_EXCL | O_RDWR, S_IRWXU | S_IRWXG);
  if (ssmpfd<0) {
    if (errno != EEXIST) {
      perror("In shm_open");
      exit(1);
    }
    else {
      P("%s was open ;)", keyF);
    }

    ssmpfd = shm_open(keyF, O_CREAT | O_RDWR, S_IRWXU | S_IRWXG);
    if (ssmpfd<0) {
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

  volatile ssmp_msg_t * comb = (volatile ssmp_msg_t *) mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, ssmpfd, 0);
  if (comb == NULL || (unsigned int) comb == 0xFFFFFFFF) {
    perror("comb = NULL\n");
    exit(134);
  }

  ticket_lock_t *lock = (ticket_lock_t *) mmap(NULL, sizeof(ticket_lock_t), PROT_READ | PROT_WRITE, MAP_SHARED, ssmpfd, 0);
  assert(lock != NULL);

  if (ssmp_id() == 0) {
    ssmp_barrier_wait(0);
  }
  ssmp_barrier_wait(0);

  volatile ssmp_msg_t *msgp; comb[0].w0 = 1;
  msgp = (volatile ssmp_msg_t *) malloc(sizeof(ssmp_msg_t));
  assert(msgp != NULL);
  unsigned int cur = 0;
  msgp->w0 = 1;
  

  unsigned long long int times = 0, ttimes = 0;
  long long int nm1 = nm;

  double _start = wtime();
  ticks _start_ticks = getticks();


  while (nm1--) {
    tlock(lock);
    P("locked it");
    tunlock(lock);

  }
  

  /*
  if (ID % 2 == 0) {
    P("service core!");

    unsigned int old = nm-1;

    while(1) {
      
      while (!__sync_bool_compare_and_swap(&comb->state, MSG, LOCKED)) {
	//	times++;
	wait_cycles(wcycles);
      }


      msgp->w0 = comb->w0;				
      msgp->w1 = comb->w1;				
      msgp->w2 = comb->w2;				
      msgp->w3 = comb->w3;				
      msgp->w4 = comb->w4;				
      msgp->w5 = comb->w5;				
      comb->state = NO_MSG;

      //      memcpy(msgp, comb, 24);

      if (msgp->w0 == 0) {
     	exit(0);
      }

      if (msgp->w0 != old) {
	PRINT("w0 -- expected %d, got %d", old, msgp->w0);
      }
      if (msgp->w5 != old) {
	PRINT("w5 -- expected %d, got %d", old, msgp->w5);
      }

      old--;

    }
  }
  else {
    P("app core!");
    long long int nm1 = nm;
    
    while (nm1--) {

      while (!__sync_bool_compare_and_swap(&comb->state, NO_MSG, LOCKED)) {
	//	times++;
	wait_cycles(wcycles);
      }

      msgp->w0 = nm1;
      msgp->w1 = nm1;
      msgp->w2 = nm1;
      msgp->w3 = nm1;
      msgp->w4 = nm1;
      msgp->w5 = nm1;
      msgp->state = MSG;
      
	comb->w0 = msgp->w0;				
	comb->w1 = msgp->w1;				
	comb->w2 = msgp->w2;				
	comb->w3 = msgp->w3;				
	comb->w4 = msgp->w4;				
	comb->w5 = msgp->w5;				
      
	//      memcpy(comb, msgp, 24);
      comb->state = MSG;
      //      ttimes += times;  
      //      times = 0;
    }
  }

  */
  ticks _end_ticks = getticks();
  double _end = wtime();

  double _time = _end - _start;
  ticks _ticks = _end_ticks - _start_ticks;
  ticks _ticksm =(ticks) ((double)_ticks / nm);
  double lat = (double) (1000*1000*1000*_time) / nm;
  printf("sent %lld msgs\n\t"
	 "in %f secs\n\t"
	 "%.2f msgs/us\n\t"
	 "%f ns latency\n"
	 "in ticks:\n\t"
	 "in %lld ticks\n\t"
	 "%lld ticks/msg\n", nm, _time, ((double)nm/(1000*1000*_time)), lat,
	 _ticks, _ticksm);





  ssmp_barrier_wait(1);

  P("avg times\t= %0.3f", ((double)ttimes)/nm);
  shm_unlink(keyF);
  ssmp_term();
  return 0;
}
