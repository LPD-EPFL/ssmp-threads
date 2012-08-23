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
#include "ssmp_send.h"
#include "ssmp_recv.h"

int num_procs = 2;
long long int nm = 100000000;
int ID, on;

#define USE_INLINE_

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

#if defined(USE_INLINE)
  P("using INLINE");
#elif defined(USE_MACRO)
  P("using MACRO");
#else      
  P("using NORMAL");
#endif


  if (argc > 3) {
    on = atoi(argv[3 + ID]);
    P("placed on core %d", on);
    set_cpu(on);
  }

  ssmp_init(num_procs);

  int rank;
  for (rank = 1; rank < num_procs; rank++) {
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
    if (ID > 0) {
      on = atoi(argv[3 + ID]);
      P("placed on core %d", on);
      set_cpu(on);
    }
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

  volatile  ssmp_msg_t *msgp;
  msgp = (volatile ssmp_msg_t *) malloc(sizeof(ssmp_msg_t));
  assert(msgp != NULL);
  
  if (ID % 2 == 0) {
    P("service core!");

    unsigned int from = ID+1;
    unsigned int old = nm-1;
    
    while(1) {
#if defined(USE_INLINE)
      ssmp_recv_from_inline(from, msgp);
#elif defined(USE_MACRO)
      ssmp_recv_fromm(from, msgp);
#else      
      ssmp_recv_from(from, msgp, 24);
#endif

      _mm_mfence();

      if (msgp->w0 < 0) {
	P("exiting..");
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
    unsigned int to = ID-1;
    long long int nm1 = nm;
    
    while (nm1--) {
      msgp->w0 = nm1;
      msgp->w1 = nm1;
      msgp->w2 = nm1;
      msgp->w3 = nm1;
      msgp->w4 = nm1;
      msgp->w5 = nm1;

#if defined(USE_INLINE)
      ssmp_send_inline(to, msgp);
#elif defined(USE_MACRO)
      ssmp_sendm(to, msgp);
#else      
      ssmp_send(to, msgp, 24);
#endif
      _mm_mfence();
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
  if (ssmp_id() % 2) {
    int ex[16]; ex[0] = -1;
    ssmp_send(ssmp_id() - 1, (ssmp_msg_t *) &ex, sizeof(long long int));
  }

  ssmp_term();
  return 0;
}
