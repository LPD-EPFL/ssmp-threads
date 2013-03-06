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

int num_procs = 2;
long long int nm = 10000000;
ticks getticks_correction;
uint8_t ID, on = 0;


static inline void
barrier_wait(uint32_t* barrier)
{
  uint32_t num_passed = __sync_add_and_fetch(barrier, 1);
  if (num_passed < num_procs)
    {
      while (*barrier < num_procs)
	{
	  _mm_lfence();
	}

      *barrier = 0;
      _mm_sfence();
    }
  else
    {
      while (*barrier > 0)
	{
	  _mm_lfence();
	}
    }
}

int
main(int argc, char **argv) 
{
  /* before doing any allocations */
#if defined(__tile__)
  if (tmc_cpus_get_my_affinity(&cpus) != 0)
    {
      tmc_task_die("Failure in 'tmc_cpus_get_my_affinity()'.");
    }
#endif

  on = id_to_core[0];
  if (argc > 3) 
    {
      uint32_t c;
      for (c = 3; c < argc; c++)
	{
	  id_to_core[c - 3] = atoi(argv[c]);
	}
      on = atoi(argv[3 + ID]);
      P("placed on core %d", on);
    }
  set_cpu(on);

  if (argc > 1) 
    {
      num_procs = atoi(argv[1]);
    }
  if (argc > 2) 
    {
      nm = atol(argv[2]);
    }

  ID = 0;
  printf("processes: %-10d / msgs: %10lld\n", num_procs, nm);
#if defined(ROUNDTRIP)
  PRINT("ROUNDTRIP");
#else
  PRINT("ONEWAY");
#endif  /* ROUNDTRIP */


  getticks_correction = getticks_correction_calc();

  ssmp_init(num_procs);

  int rank;
  for (rank = 1; rank < num_procs; rank++)
    {
      pid_t child = fork();
      if (child < 0) 
	{
	  P("Failure in fork():\n%s", strerror(errno));
	} 
      else if (child == 0) 
	{
	  goto fork_done;
	}
    }
  rank = 0;

 fork_done:
  ID = rank;
  uint32_t on = id_to_core[ID];

#if defined(TILERA)
  tmc_cmem_init(0);		/*   initialize shared memory */
#endif  /* TILERA */

  if (argc > 3)
    {
      if (ID > 0) 
	{
	  on = atoi(argv[3 + ID]);
	  P("placed on core %d", on);
	  set_cpu(on);
	}
    }
  else
    {
      set_cpu(on);
    }
  id_to_core[ID] = on;

  ssmp_mem_init(ID, num_procs);
  /* P("Initialized child %u", rank); */

  ssmp_barrier_wait(0);
  /* P("CLEARED barrier %d", 0); */


  volatile  ssmp_msg_t *msgp;
  msgp = (volatile ssmp_msg_t *) malloc(sizeof(ssmp_msg_t));
  assert(msgp != NULL);
  
  PF_MSG(0, "receiving");
  PF_MSG(1, "sending");

  /* PFDINIT(nm); */

  ssmp_barrier_wait(0);

  /********************************************************************************* 
   * main part
   *********************************************************************************/
  if (ID % 2 == 0) {

    uint32_t from = ID+1;
    uint32_t out = nm-1;
    uint32_t idx = 0; 
    uint32_t expected = 0;


    while(1) 
      {
	ssmp_recv_from(from, msgp);

#if defined(ROUNDTRIP)
	ssmp_send(from, msgp);
/* #  if !defined(NIAGARA) && !defined(TILERA) */
/* 	wait_cycles(128); */
/* #  endif */
#endif

	if (msgp->w0 == out) 
	  {
	    break;
	  }

	if (msgp->w0 != expected++)
	  {
	    PRINT(" *** expected %5d, got %5d", expected, msgp->w0);
	  }
	idx++;
      }
  }
  else 
    {
      uint32_t to = ID-1;
      long long int nm1 = nm;

      for (nm1 = 0; nm1 < nm; nm1++)
	{
	  msgp->w0 = nm1;

	  PF_START(1);
	  ssmp_send(to, msgp);
#if !defined(ROUNDTRIP)
	  PF_STOP(1);
#endif

	/* wait_cycles(128); */

#if defined(ROUNDTRIP)
	  ssmp_recv_from(to, msgp);
	  PF_STOP(1);

	  if (msgp->w0 != nm1)
	    {
	      PRINT(" *** expected %5d, got %5d", nm1, msgp->w0);
	    }
#else
#if defined(__x86_64__)
	wait_cycles(256);
#endif
#endif
	}
    }


  uint32_t c;
  for (c = 0; c < ssmp_num_ues(); c++)
    {
      if (c == ssmp_id())
	{
	  /* PRINT(" - - waited: recv: %llu / %0.2f | send: %llu / %0.2f",  */
	  /* 	waited, waited / (double) nm, */
	  /* 	waited_send, waited_send / (double) nm); */
	  PF_PRINT;
	  /* PFDPN(0, nm, wc); */
	  /* PFDPN(1, nm, wc); */
	}
      ssmp_barrier_wait(0);
    }


  /* ssmp_barrier_wait(1); */
  /* if (ssmp_id() % 2) { */
  /*   int ex[16]; ex[0] = -1; */
  /*   ssmp_send(ssmp_id() - 1, (ssmp_msg_t *) &ex, 64); */
  /* } */

  ssmp_term();
  return 0;
}

