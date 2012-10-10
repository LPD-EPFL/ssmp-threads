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

#include "measurements.h"

int num_procs = 2;
long long int nm = 100000000;
ticks getticks_correction;
int ID, on;


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

  char keyF[100];
  sprintf(keyF,"/finc");
  PRINT("opening my local buff (%s)", keyF);

  int size = sizeof(ssmp_msg_t);

  int ssmpfd = shm_open(keyF, O_CREAT | O_EXCL | O_RDWR, S_IRWXU | S_IRWXG);
  if (ssmpfd<0) {
    if (errno != EEXIST) {
      perror("In shm_open");
      exit(1);
    }
    else {
      ;
    }

    ssmpfd = shm_open(keyF, O_CREAT | O_RDWR, S_IRWXU | S_IRWXG);
    if (ssmpfd<0) {
      perror("In shm_open");
      exit(1);
    }
  }
  else {
    //P("%s newly openned", keyF);
    if (ftruncate(ssmpfd, size) < 0) {
      perror("ftruncate failed\n");
      exit(1);
    }
  }

  uint32_t* finc = (volatile uint32_t*) mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, ssmpfd, 0);
  if (finc == NULL || (unsigned int) finc == 0xFFFFFFFF) {
    perror("comb = NULL\n");
    exit(134);
  }


  getticks_correction = getticks_correction_calc();

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
  uint32_t on = ID;

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


  volatile  ssmp_msg_t *msgp;
  msgp = (volatile ssmp_msg_t *) malloc(sizeof(ssmp_msg_t));
  assert(msgp != NULL);
  
  PF_MSG(1, "receiving");
  PF_MSG(2, "sending");

  uint32_t f;
  for (f = 0; f < 16; f++) finc[f] = 0;
  ssmp_barrier_wait(0);

  /********************************************************************************* 
   * main part
   *********************************************************************************/
  if (ID % 2 == 0) {
    P("service core!");

    unsigned int from = ID+1;
    unsigned int old = nm-1;
    
    //    ssmp_barrier_wait(0);


    while(1) 
      {
	/* barrier_wait(finc); */

	PF_START(1);
	//	ssmp_recv_from(from, msgp, 64);
	ssmp_recv_from_socket(from, msgp);
	PF_STOP(1);

	/* barrier_wait(finc+1); */

	/* PF_START(2); */
	/* ssmp_send(from, msgp, 64); */
	/* PF_STOP(2); */

	if (msgp->w0 == 0) {
	  PRINT("done..");
	  break;
	}
	/* if (msgp->w0 != old) { */
	/*   PRINT("w0 -- expected %d, got %d", old, msgp->w0); */
	/* } */
	/* if (msgp->w5 != old) { */
	/*   PRINT("w5 -- expected %d, got %d", old, msgp->w5); */
	/* } */
	/* old--; */
	//	ssmp_barrier_wait(0);
	      
      }
  }
  else 
    {
      P("app core!");
      uint32_t wc = 0;
      if (argc > 5)
	{
	  wc = atoi(argv[5]);
	}
      PRINT("wc = %u", wc);
      unsigned int to = ID-1;
      long long int nm1 = nm;

      while (nm1--) {
	//	ssmp_barrier_wait(0);
	/* barrier_wait(finc); */

	msgp->w0 = nm1;
	/* msgp->w1 = nm1; */
	/* msgp->w2 = nm1; */
	/* msgp->w3 = nm1; */
	/* msgp->w4 = nm1; */
	/* msgp->w5 = nm1; */


	wait_cycles(wc);
	//    _mm_clflush((void*) ssmp_recv_buf[to]);
	/* PF_START(2);
	/* //	ssmp_send(to, msgp, 64); */
	/* ssmp_put(to, msgp); */
	/* PF_STOP(2); */

	/* barrier_wait(finc+1); */

	PF_START(1);
	//	ssmp_recv_from(to, msgp, 64);
	ssmp_send_socket(to, msgp);
	PF_STOP(1);

      }
    }


  /* double _end = wtime(); */
  /* double _time = _end - _start; */

  /* ticks _ticksm =(ticks) ((double)_ticks / nm); */
  /* double lat = (double) (1000*1000*1000*_time) / nm; */
  /* printf("[%2d] sent %lld msgs\n\t" */
  /* 	 "in %f secs\n\t" */
  /* 	 "%.2f msgs/us\n\t" */
  /* 	 "%f ns latency\n" */
  /* 	 "in ticks:\n\t" */
  /* 	 "in %lld ticks\n\t" */
  /* 	 "%lld ticks/msg\n", ID, nm, _time, ((double)nm/(1000*1000*_time)), lat, */
  /* 	 (long long unsigned int) _ticks, (long long unsigned int) _ticksm); */


  extern uint64_t waited, waited_send;

  uint32_t c;
  for (c = 0; c < ssmp_num_ues(); c++)
    {
      if (c == ssmp_id())
	{
	  PRINT(" - - waited: recv: %llu / %0.2f | send: %llu / %0.2f", 
		waited, waited / (double) nm,
		waited_send, waited_send / (double) nm);
	  PF_PRINT;
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

