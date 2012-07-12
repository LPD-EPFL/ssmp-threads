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


#include "common.h"
#include "ssmp.h"

#define MFENCE() asm volatile("mfence")

static inline void valset(volatile unsigned int *point, int ret) {
  __asm__ __volatile__("xchgl %1, %0"
                       : "=r"(ret)
                       : "m"(*point), "0"(ret)
                       : "memory");
}

/*static inline void valget(volatile unsigned int *point, int ret) {
  __asm__ __volatile__("xchgl %0, %1"
                       : "=r"(ret)
                       : "0"(ret), "m"(*point)
                       : "memory");
		       }*/

#define valget(a,b) asm( "xchg %0, %1" : "=r" (a) , "=mr" (b) : "0" (a),  "1" (b) );


int num_procs = 2;
long long int nm = 100000000;
int ID;

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

  ssmp_init(num_procs);

  ssmp_msg_t *comb = (ssmp_msg_t *) mmap(0,2 * sizeof(ssmp_msg_t),PROT_READ|PROT_WRITE,MAP_SHARED|MAP_ANONYMOUS,-1,0);
  assert(comb != NULL);
  comb[0].state = 0;
  comb[1].state = 0;

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
  P("Initializing child %u", rank);
  if (argc > 3) {
    int on = atoi(argv[3 + ID]);
    P("placed on core %d", on);
    set_cpu(on);
  }
  else {
    set_cpu(ID);
  }
  ssmp_mem_init(ID, num_procs);
  P("Initialized child %u", rank);

  ssmp_barrier_wait(0);
  P("CLEARED barrier %d", 0);

  double _start = wtime();
  ticks _start_ticks = getticks();

  ssmp_msg_t msg;
  unsigned int cur = 0;
  unsigned int p = 0;

#define NO_MSG 0
#define MSG 1
#define WRT 2
#define RD 3


  if (ID % 2 == 0) {
    P("service core!");

    while(1) {
      while (!__sync_bool_compare_and_swap(&comb[cur].state, MSG, MSG));
      //      __sync_bool_compare_and_swap(&comb[cur].state, RD, NO_MSG);
      msg.state = NO_MSG;
      memcpy(&msg, &comb[cur], 64);
      comb[cur].state = NO_MSG;
      if (msg.w0 == 0) {
	break;
      }
      //      while (!__sync_fetch_and_or(&comb->state, 0));
      //      //while (!(comb[cur].state));
      //	__sync_synchronize();
      //      valset(&comb[cur].state, 0);
      //      valset(&comb[cur].state, NO_MSG);
      
      //      cur = cur ? 0 : 1;
      //      __sync_synchronize();
      //__sync_fetch_and_and(&comb->state, 0);
    }
  }
  else {
    P("app core!");
    int to = ID-1;
    long long int nm1 = nm;
    
    while (nm1--) {
      while (!__sync_bool_compare_and_swap(&comb[cur].state, NO_MSG, NO_MSG));
      msg.w0 = nm1;
      msg.state = MSG;
      memcpy(&comb[cur], &msg, 64);

      //      __sync_bool_compare_and_swap(&comb[cur].state, WRT, MSG);
      //      while (__sync_fetch_and_or(&comb[cur].state, 0));
      //      while (comb[cur].state);
      //	__sync_synchronize();
      //      _mm_stream_si32(&comb->state, 1);
      
      //      valset(&comb[cur].state, MSG);
      //cur = cur ? 0 : 1;
      //      __sync_synchronize();
      //      __sync_fetch_and_or(&comb->state, 1);
    }
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



  ssmp_barrier_wait(1);

  ssmp_term();
  return 0;
}
