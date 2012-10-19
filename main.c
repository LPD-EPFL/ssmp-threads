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
#include "pfd.h"


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

uint32_t dsl_per_core = 2;
uint32_t wcycles[4];

int color_dsl(int id)
{
  return (id % dsl_per_core == 0);
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


static uint8_t node_to_node_hops[8][8] =
  {
  /* 0  1  2  3  4  5  6  7           */
    {0, 1, 2, 3, 2, 3, 2, 3},	/* 0 */
    {1, 0, 3, 2, 3, 2, 3, 2},	/* 1 */
    {2, 3, 0, 1, 2, 3, 2, 3},	/* 2 */
    {3, 2, 1, 0, 3, 2, 3, 2},	/* 3 */
    {2, 3, 2, 3, 0, 1, 2, 3},	/* 4 */
    {3, 2, 3, 2, 1, 0, 3, 2},	/* 5 */
    {2, 3, 2, 3, 2, 3, 0, 1},	/* 6 */
    {3, 2, 3, 2, 3, 2, 1, 0},	/* 7 */
  };

static inline uint32_t 
get_num_hops(uint32_t core1, uint32_t core2)
{
  uint32_t hops = node_to_node_hops[core1 / 6][core2 / 6];
  //  PRINT("%2d is %d hop", core2, hops);
  return hops;
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
      dsl_per_core = atoi(argv[3]);
    }

  if (argc < 7)
    {
      PRINT("need 6 params");
      return -1;
    }

  wcycles[0] = atoi(argv[4]);
  wcycles[1] = atoi(argv[5]);
  wcycles[2] = atoi(argv[6]);
  wcycles[3] = atoi(argv[7]);

  PRINT("****************************** %5d - %5d - %5d - %5d", wcycles[0], wcycles[1], wcycles[2], wcycles[3]);

  ID = 0;
  printf("NUM of processes: %d\n", num_procs);
  printf("NUM of msgs: %lld\n", nm);


  uint32_t i, dsl_seq_idx = 0;;
  for (i = 0; i < num_procs; i++)
    {
      if (color_dsl(i))
	{
	  num_dsl++;
	  dsl_seq[dsl_seq_idx++] = i;
	}
      else
	{
	  num_app++;
	}
    }

  PRINT("dsl: %2d | app: %d", num_dsl, num_app);

  ssmp_init(num_procs);

  ssmp_barrier_init(2, 0, color_dsl);
  ssmp_barrier_init(1, 0, color_app1);

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
  set_cpu(id_to_core[ID]);
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

  PF_MSG(0, "recv");
  PF_MSG(1, "send");
  PF_MSG(2, "roundtrip");


  uint32_t num_zeros = num_app;
  uint32_t lim_zeros = num_dsl;

  getticks_correction_calc();

  PFDINIT((num_app/num_dsl)*nm + 96);

  ssmp_barrier_wait(0);

  uint32_t cur = 0;

  /* ********************************************************************************
     main functionality
   *********************************************************************************/

  if (color_dsl(ID)) 
    {

      PRINT("dsl core");

      PF_START(3);
      while(1) 
	{
	  /* wait_cycles(wcycles); */

	  /* PF_START(0); */
	  ssmp_recv_color_start(cbuf, msg);
	  /* PF_STOP(0); */

	  /* uint32_t s = msg->sender; */
	  /* PREFETCHW(ssmp_send_buf[s]); */

	  _mm_pause_rep(30);
	  //	  PF_START(1);
	  PFDI(0);
	  ssmp_send(msg->sender, msg);
	  PFDO(0, cur++);

	  //	  PF_STOP(1);
	  

	  if (msg->w0 < lim_zeros) {
	    if (--num_zeros == 0)
	      {
		P("done ..");
		break;
	      }
	  }
	}
      PF_STOP(3);
    }
  else {
    unsigned int to = 0, to_idx = 0;
    long long int nm1 = nm;

    ssmp_barrier_wait(1);
    PF_START(3);
    while (nm1--)
      {
	PF_START(2);
	msg->w0 = nm1;
	/* PF_START(1); */
	ssmp_send(to, msg);
	/* PF_STOP(1); */
	
	uint32_t hops = get_num_hops(get_cpu(), to);
	wait_cycles(wcycles[hops]);
	/* switch (hops) */
	/*   { */
	/*   case 0: */
	/*     wait_cycles(820); */
	/*     break; */
	/*   case 1: */
	/*     wait_cycles(wcycles1); */
	/*     break; */
	/*   case 2: */
	/*     wait_cycles(wcycles2); */
	/*     break; */
	/*   case 3: */
	/*     wait_cycles(wcycles3); */
	/*   default: */
	/*     ; */
	/*   } */

	/* PF_START(0); */
	ssmp_recv_from(to, msg);
	/* PF_STOP(0); */

	to = dsl_seq[to_idx++];
	if (to_idx == num_dsl)
	  {
	    to_idx = 0;
	  }
	/* PREFETCHW(ssmp_send_buf[dsl_seq[to_idx]]); */

	PF_STOP(2);

	if (msg->w0 != nm1) {
	  P("Ping-pong failed: sent %lld, recved %d", nm1, msg->w0);
	}
      }
    /* PF_STOP(3); */
  }

  
  uint32_t c;
  for (c = 0; c < ssmp_num_ues(); c++)
    {
      if (c == ssmp_id())
  	{
	  PF_PRINT;
	  if (color_dsl(c))
	    {
	      PFDPN(0, cur, 10);
	    }
  	}
      ssmp_barrier_wait(0);
    }

  ssmp_barrier_wait(3);
  ssmp_term();
  return 0;
}
