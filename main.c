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
#include "measurements.h"


#define COLOR_BUF
#define USE_MEMCPY
//#define USE_SIGNAL
//#define BLOCKING

int num_procs = 2;
long long int nm = 1000000;
int ID;
uint32_t num_dsl = 0;
uint32_t num_app = 0;
uint32_t dsl_seq[48];

uint32_t wcycles = 2;

int color_dsl(int id)
{
  return (id % wcycles == 0);
}

uint32_t nth_dsl(int id)
{
  int i, id_seq = 0;
  for (i = 0; i < ssmp_num_ues(); i++)
    {
      if (i == id)
	{
	  return id_seq;
	}
      id_seq++;
    }
}

int color_app1(int id)
{
  return !(color_dsl(id));
}

int main(int argc, char **argv) {
  if (argc > 1) {
    num_procs = atoi(argv[1]);
  }
  if (argc > 2) {
    nm = atol(argv[2]);
  }

  if (argc > 3)
    {
      wcycles = atoi(argv[3]);
    }

  ID = 0;
  printf("NUM of processes: %d\n", num_procs);
  printf("NUM of msgs: %lld\n", nm);


  uint32_t i, dsl_seq_idx = 0;;
  for (i = 0; i < num_procs; i++)
    {
      if (color_dsl(i))
	{
	  printf("%2d -> DSL\n", i);
	  num_dsl++;
	  dsl_seq[dsl_seq_idx++] = i;
	}
      else
	{
	  printf("%2d -> APP\n", i);
	  num_app++;
	}
    }

  PRINT("dsl: %2d | app: %d", num_dsl, num_app);

  ssmp_init(num_procs);

  ssmp_barrier_init(2, 0, color_dsl);
  ssmp_barrier_init(1, 0, color_app1);

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
  if (color_dsl(ID)) {
    cbuf = (ssmp_color_buf_t *) malloc(sizeof(ssmp_color_buf_t));
    assert(cbuf != NULL);
    ssmp_color_buf_init(cbuf, color_app1);
  }

  volatile ssmp_msg_t *msg;
  msg = (volatile ssmp_msg_t *) malloc(sizeof(ssmp_msg_t));
  assert(msg != NULL);

  uint32_t last_recv_from = 0;

  PF_MSG(0, "recv");
  PF_MSG(1, "send");
  PF_MSG(2, "roundtrip");


  uint32_t num_zeros = num_app;
  uint32_t lim_zeros = num_dsl;
  ssmp_barrier_wait(0);

  if (color_dsl(ID)) 
    {
      PRINT("dsl core");

      while(1) 
	{
	  /* wait_cycles(wcycles); */

	  PF_START(0);
	  last_recv_from = ssmp_recv_color_start(cbuf, msg, last_recv_from + 1);
	  PF_STOP(0);

	  PF_START(1);
	  ssmp_send(msg->sender, msg);
	  PF_STOP(1);

	  if (msg->w0 < lim_zeros) {
	    if (--num_zeros == 0)
	      {
		P("done ..");
		break;
	      }
	  }

	}
    }
  else {
    unsigned int to = 0, to_idx = 0;
    long long int nm1 = nm;

    ssmp_barrier_wait(1);
    while (nm1--)
      {
	PF_START(2);
	to = dsl_seq[to_idx++];
	if (to_idx == num_dsl)
	  {
	    to_idx = 0;
	  }
	//	PREFETCHW(ssmp_send_buf[dsl_seq[to_idx]]);

	msg->w0 = nm1;
	PF_START(1);
	ssmp_send(to, msg);
	PF_STOP(1);

	PF_START(0);
	ssmp_recv_from(to, msg);
	PF_STOP(0);
	PF_STOP(2);

	if (msg->w0 != nm1) {
	  P("Ping-pong failed: sent %lld, recved %d", nm1, msg->w0);
	}
      }
  }

  
  uint32_t c;
  for (c = 0; c < ssmp_num_ues(); c++)
    {
      if (c == ssmp_id())
  	{
	  PF_PRINT;
  	}
      ssmp_barrier_wait(0);
    }

  ssmp_barrier_wait(3);
  ssmp_term();
  return 0;
}
