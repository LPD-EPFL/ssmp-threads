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

int num_procs = 2;
long long int nm = 100000000;
int ID;

#define MSIZE (1024 * 1024)

int main(int argc, char **argv) {
  if (argc > 1) {
    num_procs = atoi(argv[1]);
  }
  if (argc > 2) {
    nm = atol(argv[2]);
  }

  ID = 0;
  printf("RUNNING ON MEM NODE: %d\n", num_procs);
  printf("NUM of msgs: %lld\n", nm);

  numa_set_preferred(num_procs);

  ssmp_init(num_procs);

  if (argc > 3) {
    int on = atoi(argv[3 + ID]);
    P("placed on core %d", on);
    set_cpu(on);
  }
  else {
    set_cpu(ID);
  }
  //  ssmp_mem_init(ID, num_procs);

  ssmp_barrier_wait(0);

  double _start = wtime();
  ticks _start_ticks = getticks();



  if (argc < 4) {
    P("testing numa access latencies --- ---");
    int *memory = (int *) malloc(MSIZE * sizeof(int));
    assert(memory != NULL);
    long long int w = nm;
    long long int s = 0;

    while (w > 0) {
      unsigned int i;
      for (i = 0; i < MSIZE; i+=16) {
	_mm_stream_si32(memory + i, w - i);
      }
      /*    for (i = 0; i < MSIZE; i+=16) {
	    s += memory[i];
	    }
      */
      w -= (MSIZE/16);
    }

    printf("sum = %lld\n", s);
  }
  else {
    P("testing CAS  access latencies --- ---");

    ssmp_msg_t *comb = (ssmp_msg_t *) mmap(0,2 * sizeof(ssmp_msg_t),PROT_READ|PROT_WRITE,MAP_SHARED|MAP_ANONYMOUS,-1,0);
    assert(comb != NULL);
    comb[0].state = 0;
    comb[1].state = 0;

    long long int w = nm;
    unsigned int prev = 0;
    unsigned int val;
    srand(time(NULL));

    while (w--) {
      val = rand();
      if(!__sync_bool_compare_and_swap(&comb->state, prev, val)) {
	P("no no no cas failed");
      }
      prev = val;
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



  ssmp_term();
  return 0;
}
