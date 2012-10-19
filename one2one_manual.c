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
#include <emmintrin.h>

#include "common.h"
#include "ssmp.h"
#include "measurements.h"
#include "pfd.h"
#include "papi.h"


#define NO_MSG 0
#define MSG 1
#define LOCKED 7
#define FOR_SENDER 1
#define FOR_RECVER 0
#define LOCK_GET(lock, who)			\
  while (!__sync_bool_compare_and_swap(lock, who, LOCKED));
#define LOCK_GIVE(lock, who)			\
  *(lock) = who;

#define MFENCE() asm volatile("mfence")

static inline void valset(volatile unsigned int *point, int ret) {
  __asm__ __volatile__("xchgl %1, %0"
                       : "=r"(ret)
                       : "m"(*point), "0"(ret)
                       : "memory");
}


typedef struct ticket_lock {
  unsigned int current;
  unsigned int ticket;
} ticket_lock_t;

__attribute__ ((always_inline)) void tlock(ticket_lock_t * lock) {
  unsigned int ticket = __sync_add_and_fetch (&lock->ticket, 1);
  while (ticket != lock->current);
}

__attribute__ ((always_inline)) void tunlock(ticket_lock_t * lock) {
  lock->current++;
}

int num_procs = 2;

static void
barrier_wait(volatile uint32_t* barrier)
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

  MFENCE();
}

long long int nm = 100000000;
int ID, wcycles = 0;
ticks getticks_correction;

int main(int argc, char **argv) {
  if (argc > 1) {
    num_procs = atoi(argv[1]);
  }
  if (argc > 2) {
    nm = atol(argv[2]);
  }

  if (argc > 5)
    {
      wcycles = atoi(argv[5]);
      //      PRINT("wcycles =  %d ", wcycles);
      PRINT("removing the line from caches <= L%d", wcycles);
    }

  ID = 0;
  printf("NUM of processes: %d\n", num_procs);
  printf("NUM of msgs: %lld\n", nm);
  printf("app guys sending to ID-1!\n");

  getticks_correction = getticks_correction_calc();

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
  if (argc > 3) {
    int on = atoi(argv[3 + ID]);
    P("placed on core %d", on);
    set_cpu(on);
  }
  else {
    set_cpu(ID);
  }
  ssmp_mem_init(ID, num_procs);



  volatile ssmp_msg_t* local;
  volatile ssmp_msg_t* remote;
  ssmp_barrier_wait(0);
  char keyF[100];
  sprintf(keyF,"/ssmp_to%02d_from%02d", ID, !ID);

  int size = (512 + (2 * 512 + 18 * 512) + ((2 * 512 + 18 * 512) + 1706 * 48)) * sizeof(ssmp_msg_t);

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

  local = (volatile ssmp_msg_t *) mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, ssmpfd, 0);
  if (local == NULL || (unsigned int) local == 0xFFFFFFFF) {
    perror("comb = NULL\n");
    exit(134);
  }

  ssmp_barrier_wait(0);

  sprintf(keyF,"/ssmp_to%02d_from%02d", !ID, ID);

  
  ssmpfd = shm_open(keyF, O_CREAT | O_EXCL | O_RDWR, S_IRWXU | S_IRWXG);
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

  remote = (volatile ssmp_msg_t *) mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, ssmpfd, 0);
  if (remote == NULL || (unsigned int) remote == 0xFFFFFFFF) {
    perror("comb = NULL\n");
    exit(134);
  }

  volatile ssmp_msg_t *msgp; 
  posix_memalign(&msgp, 64, 2 * 64); /* ********************* */
  //  msgp = (volatile ssmp_msg_t *) malloc(2 * sizeof(ssmp_msg_t));
  assert(msgp != NULL);

  uint32_t* msgpui = (uint32_t*) msgp;
  while ((uint64_t)msgpui % 64)
    {
      PRINT("aligning");
      msgpui++;
    }
  msgp = (ssmp_msg_t*) msgpui;

  msgp->w0 = 1;

  local[0].state = NO_MSG;
  local[1].state = NO_MSG;

  uint32_t* lock;
  if (ID)
    {
      lock = (uint32_t*) &remote[2].state;
    }
  else 
    {
      lock = (uint32_t*) &local[2].state;
    }

  *lock = 0;

  PF_MSG(1, "receiving");
  PF_MSG(2, "sending");
  PF_MSG(3, "wait while(!recv)");

  sprintf(keyF,"/finc");

  size = 16 * sizeof(ssmp_msg_t);

  ssmpfd = shm_open(keyF, O_CREAT | O_EXCL | O_RDWR, S_IRWXU | S_IRWXG);
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

  volatile uint32_t* finc = (volatile uint32_t*) mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, ssmpfd, 0);
  if (finc == NULL || (unsigned int) finc == 0xFFFFFFFF) {
    perror("comb = NULL\n");
    exit(134);
  }

  uint32_t f;
  for (f = 0; f < 16; f++) finc[f] = 0;

  pfd_store_init(10*nm);

  uint32_t* wted_det = (uint32_t*) calloc(nm, sizeof(uint32_t));
  assert(wted_det != NULL);

  PREFETCHW(local);

  srand(getticks() % 500009);

  /* PAPI ------------------------------------------------------------------------------- */

  int event_set = PAPI_NULL;

  int ret = PAPI_library_init(PAPI_VER_CURRENT);
  assert(ret == PAPI_VER_CURRENT);

  ret = PAPI_create_eventset(&event_set);
  assert(ret == PAPI_OK);

  ret = PAPI_add_event(event_set, PAPI_L1_DCM);
  assert(ret == PAPI_OK);

  ret = PAPI_add_event(event_set, PAPI_L1_DCH);
  assert(ret == PAPI_OK);

  ret = PAPI_add_event(event_set, PAPI_L1_DCA);
  assert(ret == PAPI_OK);


  ret = PAPI_reset(event_set);
  assert(ret == PAPI_OK);

  uint64_t l1_dc_s[3] = {0, 0, 0}, l1_dc_e[3];

  PAPI_start(event_set);

  ssmp_barrier_wait(0);

  /* /\********************************************************************************* */
  /*  *  main functionality */
  /*  *********************************************************************************\/ */

  ticks wtimes_snd = 0, wtimes_rcv = 0;
  uint32_t nm1 = nm;

  if (ID % 2 == 0) 
    {
      P("service core!");

      uint32_t nm1 = nm;

      for (nm1 = 0; nm1 < nm; nm1++)
	{
	  /* ssmp_barrier_wait(0); */
	  /* barrier_wait(finc); */


	  PREFETCHW(&event_set);
	  PREFETCHW(l1_dc_e);
	  
	  wait_cycles(100000);

	  PREFETCHW(&event_set);
	  PREFETCHW(l1_dc_e);

	  uint32_t wted = 0;
	  //	  PAPI_read(event_set, l1_dc_s);
	  PFDI(0);

	  PAPI_reset(event_set);
	  PREFETCHW(local);
	  while (local->state != MSG)
	    {
	      /* if (wted++) */
	      /* 	{ */
	      wted++;
	      PRINT("why??");
	      PREFETCHW(local);
	      //		  wait_cycles(wted * 6);
	      //		  PFDO(1, wtimes_rcv - 1);
	      /* } */
	      //	      wtimes_rcv++;
	      //	      PFDI(1);
	    }


	  /* if (wted) */
	  /*   { */
	  /*     PFDO(1, wtimes_rcv - 1); */
	  /*   } */
	  memcpy(msgp, local, 64);
	  local->state = NO_MSG;
	  PAPI_read(event_set, l1_dc_e);

	  PFDO(0, nm1);
	  PRINT("(%3d) M: %-6llu, H: %-6llu, A: %llu",
		nm1, l1_dc_e[0] - l1_dc_s[0], l1_dc_e[1] - l1_dc_s[1], l1_dc_e[2] - l1_dc_s[2]);

	  wted_det[nm1] = wted;
	  //	  printf("[%2d : %4d] ", nm1, wted);

	  /* } */

	  //	  wait_cycles(20000);
	  /* for (nm1 = 0; nm1 < nm; nm1++) */
	  /* 	{ */
	  /* ssmp_barrier_wait(3); */
	  /* barrier_wait(finc + 16); */
	  /* **************************************** send */
	  wted = 0;
	  PFDI(1);
	  PREFETCHW(remote);
	  while (remote->state != NO_MSG)
	    {
	      if (wted++)
	      	{
	      	  PREFETCHW(remote);
	      	  _mm_pause();
	      	}
	      wtimes_snd++;
	    }

	  msgp->state = MSG;
	  memcpy(remote, msgp, 64);
	  PFDO(1, nm1);

	  /* barrier_wait(finc + 32); */

	  /* ssmp_barrier_wait(4); */
	}
      
      PRINT(" ~~ waited before recv");
      for (nm1 = 0; nm1 < nm; nm1++)
	{
	  printf("[ %2d: %4d] ", nm1, wted_det[nm1]);
	}
      printf("\n");	
      barrier_wait(finc + 48);
    }
  else 			/* SENDER */
    {
      P("app core!");
	
      uint32_t* msgp_state = &msgp->state;

      uint32_t wc = atoi(argv[5]);
      for (nm1 = 0; nm1 < nm; nm1++)
	{
	  /* ssmp_barrier_wait(0); */
	  /* barrier_wait(finc); */

	  /* wait_cycles(wc); */

	  uint32_t wted = 0;
	  PFDI(1);

	  //	  PAPI_read(event_set, l1_dc_s);
	  PAPI_reset(event_set);
	  PREFETCHW(remote);
	  while (remote->state != NO_MSG)
	    {
	      PRINT("why??");
	      /* if (wted++) */
	      /* 	{ */
	      /* 	  PREFETCHW(remote); */
	      /* 	  _mm_pause(); */
	      /* 	} */
	      /* wtimes_snd++; */
	    }

	  msgp->state = MSG;
	  memcpy(remote, msgp, 64);
	  PAPI_read(event_set, l1_dc_e);

	  PFDO(1, nm1);
	  PRINT("\t\t\t\t\t\t\t\t(%3d) M: %-6llu, H: %-6llu, A: %llu",
		nm1, l1_dc_e[0] - l1_dc_s[0], l1_dc_e[1] - l1_dc_s[1], l1_dc_e[2] - l1_dc_s[2]);
	  /* } */


	  //	  wait_cycles(rand() % (1024 * 1024));
	  /* wait_cycles(wc); */
	  /* for (nm1 = 0; nm1 < nm; nm1++) */
	  /* 	{ */
	  
	  /* ssmp_barrier_wait(3); */
	  /* barrier_wait(finc + 16); */

	  /* **************************************** recv */

	  PFDI(0);
	  wted = 0;
	  PREFETCHW(local);
	  while (local->state != MSG)
	    {
	      wtimes_rcv++;
	      wted++;
	      PREFETCHW(local);
	    }
	  memcpy(msgp, local, 64);
	  local->state = NO_MSG;
	  PFDO(0, nm1);

	  wted_det[nm1] = wted;
	  /* barrier_wait(finc + 32); */
	  /* ssmp_barrier_wait(4); */
	}


      PAPI_stop(event_set, l1_dc_s);
      barrier_wait(finc + 48);
      PRINT(" ~~ waited before recv");
      for (nm1 = 0; nm1 < nm; nm1++)
	{
	  printf("[ %2d: %4d] ", nm1, wted_det[nm1]);
	}
      printf("\n");	

    }

  ssmp_barrier_wait(3);
  /* PRINT("recv = %6llu : avg recv = %5.1f || snd = %6llu : avg snd = %.1f",  */
  /* 	wtimes_rcv, wtimes_rcv / (double) nm,  wtimes_snd, wtimes_snd / (double) nm); */
  for (nm1 = 0; nm1 < num_procs; nm1++)
    {
      if (ID == nm1)
	{
	  PF_PRINT;
	  PFDPN(0, nm, 29);
	  PFDPN(1, nm, 29);
	}
      ssmp_barrier_wait(0);
    }
  ssmp_barrier_wait(0);


  shm_unlink(keyF);
  sprintf(keyF,"/ssmp_to%02d_from%02d", ID, !ID);
  shm_unlink(keyF);
  ssmp_term();
  return 0;
}

