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
int ID, clines = 1;

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

  ssmp_msg_t *cls = (ssmp_msg_t *) malloc(8 * sizeof(ssmp_msg_t));
  if (argc > 3) {
    clines = atoi(argv[3]);
  }
  P("updating %d cache lines", clines);

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
  P("Initializing child %u", rank);
  ssmp_mem_init(ID, num_procs);
  P("Initialized child %u", rank);

  srand(ID * (int)(1000000*wtime()));

  ssmp_barrier_wait(0);
  long long int nm1 = nm;
  P("CLEARED barrier %d", 0);

  double _start = wtime();
  ticks _start_ticks = getticks();

  int i, s;  
  while(nm1--) {
    switch(clines) {
    case 8:
      cls[7].w0 = nm1;
      cls[0].w1 = nm1;
      cls[3].w3 = nm1;
      cls[2].w2 = nm1;
    case 4:
      cls[1].w3 = nm1;
      cls[4].w1 = nm1;
    case 2:
      cls[5].w6 = nm1;
    case 1:
      cls[6].w3 = nm1;
    }
  }


  

  ticks _end_ticks = getticks();
  double _end = wtime();

  double _time = _end - _start;
  P("%d", s);
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
