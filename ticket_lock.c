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

inline void tlock(ticket_lock_t * lock) {
  unsigned int ticket = __sync_add_and_fetch(&lock->ticket, 1);
  //  P("got ticket %d", ticket);
  while (ticket != lock->current);
}

inline void tunlock(ticket_lock_t * lock) {
  lock->current++;
    //__sync_add_and_fetch(&lock->current, 1);
  //  P("releasing %d", lock->current - 1);
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
  if (argc > 3) {
    wcycles = atoi(argv[3]);
  }
  P("wait cycles = %d", wcycles);

  ID = 0;
  printf("NUM of processes: %d\n", num_procs);
  printf("NUM of msgs: %lld\n", nm);
  printf("app guys trying to lock a single lock!\n");

  ticket_lock_t *lock = (ticket_lock_t *) mmap(0,sizeof(ticket_lock_t),PROT_READ|PROT_WRITE,MAP_SHARED|MAP_ANONYMOUS,-1,0);
  assert(lock != NULL);

  lock->current = 1;
  lock->ticket = 0;


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
  if (argc > 4) {
    int on = atoi(argv[4 + ID]);
    P("placed on core %d", on);
    set_cpu(on);
  }
  else {
    set_cpu(ID);
  }
  ssmp_mem_init(ID, num_procs);

  ssmp_barrier_wait(0);

  long long int nm1 = nm;

  double _start = wtime();
  ticks _start_ticks = getticks();


  while (nm1--) {
    tlock(lock);
    wait_cycles(wcycles);
    tunlock(lock);

  }
  

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


  ssmp_barrier_wait(0);


  ssmp_term();
  return 0;
}
