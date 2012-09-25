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
#include "ssmp_recv.h"
#include "ssmp_send.h"

#define COLOR_BUF
#define USE_MEMCPY
//#define USE_SIGNAL
//#define BLOCKING

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
  set_cpu(ID);
  ssmp_mem_init(ID, num_procs);

  ssmp_color_buf_t *cbuf = NULL;
  if (ID % 2 == 0) {
    cbuf = (ssmp_color_buf_t *) malloc(sizeof(ssmp_color_buf_t));
    assert(cbuf != NULL);
    ssmp_color_buf_init(cbuf, color_app);
  }

  volatile ssmp_msg_t *msg;
  msg = (volatile ssmp_msg_t *) malloc(sizeof(ssmp_msg_t));
  assert(msg != NULL);

  uint32_t last_recv_from = 0;

  ssmp_barrier_wait(0);
  P("CLEARED barrier %d", 0);


  double _start = wtime();
  ticks _start_ticks = getticks();

  if (ID % 2 == 0) {
    while(1) {
      //ssmp_recv_color(cbuf, msg, 24);
      last_recv_from = ssmp_recv_color_start(cbuf, msg, last_recv_from + 1);

      if (msg->w0 < 0) {
	P("exiting ..");
	exit(0);
      }
      ssmp_send(msg->sender, msg, 8);
      //      ssmp_sendm(msg->sender, msg);
      //ssmp_send_inline(msg->sender, msg);
    }
  }
  else {
    unsigned int to = ID-1;
    long long int nm1 = nm;
    while (nm1--) {
      to = (to + 2) % num_procs;
      //to = 0;

      msg->w0 = nm1;
      ssmp_send(to, msg, 24);
      ssmp_recv_from(to, msg, 24);

      if (msg->w0 != nm1) {
	P("Ping-pong failed: sent %lld, recved %d", nm1, msg->w0);
      }
    }
  }

  

  ticks _end_ticks = getticks();
  double _end = wtime();

  double _time = _end - _start;
  ticks _ticks = _end_ticks - _start_ticks;
  ticks _ticksm =(ticks) ((double)_ticks / nm);
  double lat = (double) (1000*1000*1000*_time) / nm;


  uint32_t c;
  for (c = 0; c < ssmp_num_ues(); c++)
    {
      if (c == ssmp_id())
	{
	  printf("[%02d] sent %lld msgs\n\t"
		 "in %f secs\n\t"
		 "%.2f msgs/us\n\t"
		 "%f ns latency\n"
		 "in ticks:\n\t"
		 "in %lld ticks\n\t"
		 "%lld ticks/msg\n", 
		 c, nm, _time, ((double)nm/(1000*1000*_time)), lat,
		 _ticks, _ticksm);
	}
      ssmp_barrier_wait(1);
    }

  ssmp_barrier_wait(1);
  if (ssmp_id() == 1) {
    P("terminating --");
    int core; 
    for (core = 0; core < ssmp_num_ues(); core++) {
      if (core % 2 == 0) {
	ssmp_msg_t s; s.w0 = -1;
	ssmp_send(core, &s, 24);
      }
    }
  }

  ssmp_term();
  return 0;
}
