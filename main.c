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
#include "measurements.h"
#include "pfd.h"

#define ROUNDTRIP_
#define DEBUG_

uint32_t nm = 1000000;
uint8_t dsl_seq[80];
uint8_t num_procs = 2;
uint8_t ID;
uint8_t num_dsl = 0;
uint8_t num_app = 0;
uint8_t dsl_per_core = 2;
uint32_t delay_after = 0;

int 
color_all(int id)
{
  return 1;
}

int 
color_dsl(int id)
{
  return (id % dsl_per_core == 0);
}

uint8_t 
nth_dsl(uint8_t id)
{
  uint8_t i, id_seq = 0;
  for (i = 0; i < ssmp_num_ues(); i++)
    {
      if (i == id)
	{
	  return id_seq;
	}
      id_seq++;
    }
  return id_seq;
}

int 
color_app1(int id)
{
  return !(color_dsl(id));
}


int 
main(int argc, char **argv) 
{
  if (argc > 1) 
    {
      num_procs = atoi(argv[1]);
    }
  if (argc > 2) 
    {
      nm = atol(argv[2]);
    }

  if (argc > 3)
    {
      dsl_per_core = atoi(argv[3]);
    }

  if (argc > 4)
    {
      delay_after = atoi(argv[4]);
    }

  ID = 0;

#if defined(ROUNDTRIP)
  PRINT("ROUNTRIP");
#else
  PRINT("ONEWAY");
#endif  /* ROUNDTRIP */
  printf("NUM of processes: %d\n", num_procs);
  printf("NUM of msgs: %u\n", nm);
  printf("Delay after each message: %u\n", delay_after);


  uint8_t i, dsl_seq_idx = 0;;
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
#if defined(XEON)
  ssmp_barrier_init(0, 0, color_all);
  ssmp_barrier_init(5, 0, color_all);
#endif

  uint8_t rank;
  for (rank = 1; rank < num_procs; rank++) 
    {
      pid_t child = fork();
      if (child < 0) {
	P("Failure in fork():\n%s", strerror(errno));
      } else if (child == 0) 
	{
	  goto fork_done;
	}
    }
  rank = 0;

 fork_done:
  ID = rank;

  set_cpu(id_to_core[ID]);
  ssmp_mem_init(ID, num_procs);

  ssmp_color_buf_t *cbuf = NULL;
  if (color_dsl(ID)) 
    {
      cbuf = (ssmp_color_buf_t *) malloc(sizeof(ssmp_color_buf_t));
      assert(cbuf != NULL);
      ssmp_color_buf_init(cbuf, color_app1);
    }

  ssmp_msg_t *msg;
  msg = (ssmp_msg_t *) malloc(sizeof(ssmp_msg_t));
  assert(msg != NULL);

  PF_MSG(0, "recv");
  PF_MSG(1, "send");
  PF_MSG(2, "roundtrip");


  uint32_t num_zeros = num_app;
  uint32_t lim_zeros = num_dsl;

  /* PFDINIT((num_app/num_dsl)*nm + 96); */

  ticks t_start = 0, t_end = 0;

  ssmp_barrier_wait(0);
  /* ********************************************************************************
     main functionality
  *********************************************************************************/

  if (color_dsl(ID)) 
    {
      while(1) 
	{
	  ssmp_recv_color_start(cbuf, msg);
#if defined(ROUNDTRIP)
	  ssmp_send(msg->sender, msg);
#endif  /* ROUNDTRIP */
	  //	  PF_STOP(1);
	  
	  if (msg->w0 < lim_zeros) {
	    if (--num_zeros == 0)
	      {
		break;
	      }
	  }
	}
    }
  else 
    {
      unsigned int to = 0, to_idx = 0;
      long long int nm1 = nm;

      ssmp_barrier_wait(1);

      t_start = getticks();

#if defined(NIAGARA)
      if (ID == 1)
	{
#endif
	  while (nm1--)
	    {
	      msg->w0 = nm1;
	      PF_START(1);
	      ssmp_send(to, msg);
#if !defined(ROUNDTRIP)
	      PF_STOP(1);
#endif 
#if defined(ROUNDTRIP)
	      ssmp_recv_from(to, msg);
	      PF_STOP(1);	
#endif  /* ROUNDTRIP */

	      to = dsl_seq[to_idx++];
	      if (to_idx == num_dsl)
		{
		  to_idx = 0;
		}

	      if (msg->w0 != nm1) 
		{
		  P("Ping-pong failed: sent %lld, recved %d", nm1, msg->w0);
		}

	      if (delay_after > 0)
		{
		  wait_cycles(delay_after);
		}
	    }
#if defined(NIAGARA)
	}
      else
	{
	  while (nm1--)
	    {
	      msg->w0 = nm1;
	      ssmp_send(to, msg);

#if defined(ROUNDTRIP)
	      ssmp_recv_from(to, msg);
#endif  /* ROUNDTRIP */

	      to = dsl_seq[to_idx++];
	      if (to_idx == num_dsl)
		{
		  to_idx = 0;
		}

	      if (msg->w0 != nm1) 
		{
		  P("Ping-pong failed: sent %lld, recved %d", nm1, msg->w0);
		}

	      if (delay_after > 0)
		{
		  wait_cycles(delay_after);
		}
	    }
	}      
#endif
      t_end = getticks();
    }

  
  double total_throughput = 0;
  uint32_t c;
  for (c = 0; c < ssmp_num_ues(); c++)
    {
      if (c == ssmp_id())
  	{
	  if (c <= 1)
	    {
	      PF_PRINT;
	    }
	  if (color_dsl(c))
	    {
	      /* PFDPN(0, cur, 10); */
	    }
	  else
	    {
 	      ticks t_dur = t_end - t_start - getticks_correction;
	      double ticks_per_sec = REF_SPEED_GHZ * 1e9;
	      double dur = t_dur / ticks_per_sec;
	      double througput = nm / dur;

#if defined(DEBUG)
	      PRINT("Completed in %10f secs | Througput: %f", dur, througput);
#endif

	      memcpy(msg, &througput, sizeof(double));
	      ssmp_send(0, msg);
	    }
  	}
      else if (ssmp_id() == 0 && color_app1(c))
	{
	  ssmp_recv_from(c, msg);
	  double througput = *(double*) msg;
	  /* PRINT("recved th from %02d : %f", c, througput); */
	  total_throughput += througput;
	}
      ssmp_barrier_wait(5);
    }

  ssmp_barrier_wait(0);
  if (ssmp_id() == 0)
    {
      PRINT("Total throughput: %f", total_throughput);
    }
  ssmp_term();
  return 0;
}
