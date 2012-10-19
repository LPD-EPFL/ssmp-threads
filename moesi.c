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
#include <float.h>

#include "common.h"
#include "ssmp.h"
#include "measurements.h"

#define NO_MSG 0
#define MSG 1
#define LOCKED 7
#define FOR_SENDER 1
#define FOR_RECVER 0
#define MFENCE() asm volatile("mfence")

static inline void
clflush(volatile void *p)
{
  asm volatile ("clflush (%0)" :: "r"(p));
}

typedef enum
  {
    STORE_ON_MODIFIED,
    STORE_ON_MODIFIED_NO_SYNC,
    STORE_ON_SINGLE_READER,
    STORE_ON_SHARED,
    STORE_ON_OWNED_MINE,
    LOAD_FROM_MODIFIED,
    STORE_ON_INVALID,
    LOAD_FROM_INVALID,
    LOAD_FROM_SHARED,
    LOAD_FROM_OWNED,
  } moesi_type_t;

const char* moesi_type_des[] =
  {
    "STORE_ON_MODIFIED",
    "STORE_ON_MODIFIED_NO_SYNC",
    "STORE_ON_SINGLE_READER",
    "STORE_ON_SHARED",
    "STORE_ON_OWNED_MINE",
    "LOAD_FROM_MODIFIED",
    "STORE_ON_INVALID",
    "LOAD_FROM_INVALID",
    "LOAD_FROM_SHARED",
    "LOAD_FROM_OWNED"
  };

#define B0 ssmp_barrier_wait(0);
#define B1 ssmp_barrier_wait(2);
#define B2 ssmp_barrier_wait(3);
#define B3 ssmp_barrier_wait(4);
#define B4 ssmp_barrier_wait(5);
#define B5 ssmp_barrier_wait(6);
#define B6 ssmp_barrier_wait(7);
#define B7 ssmp_barrier_wait(8);
#define B8 ssmp_barrier_wait(9);
#define B9 ssmp_barrier_wait(10);
#define B10 ssmp_barrier_wait(11);
#define B11 ssmp_barrier_wait(12);
#define B12 ssmp_barrier_wait(13);
#define B13 ssmp_barrier_wait(14);
#define B14 ssmp_barrier_wait(15);

#define FB0 barrier_wait(fbarrier + 0);
#define FB1 barrier_wait(fbarrier + 2);
#define FB2 barrier_wait(fbarrier + 3);
#define FB3 barrier_wait(fbarrier + 4);
#define FB4 barrier_wait(fbarrier + 5);
#define FB5 barrier_wait(fbarrier + 6);
#define FB6 barrier_wait(fbarrier + 7);
#define FB7 barrier_wait(fbarrier + 8);
#define FB8 barrier_wait(fbarrier + 9);
#define FB9 barrier_wait(fbarrier + 10);
#define FB10 barrier_wait(fbarrier + 11);
#define FB11 barrier_wait(fbarrier + 12);
#define FB12 barrier_wait(fbarrier + 13);
#define FB13 barrier_wait(fbarrier + 14);
#define FB14 barrier_wait(fbarrier + 15);


int num_procs = 2;
extern int64_t rands[];

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

  _mm_mfence();
}

static inline 
double absd(double x)
{
  if (x >= 0)
    {
      return x;
    }
  else 
    {
      return -x;
    }
}

uint32_t d = 0;

typedef struct abs_deviation
{
  uint64_t num_vals;
  double avg;
  double avg_10p;
  double avg_25p;
  double abs_dev;
  double min_dev;
  uint64_t min_dev_idx;
  double max_dev;
  uint64_t max_dev_idx;
  uint32_t num_dev_10p;
  uint32_t num_dev_25p;
  uint32_t num_dev_50p;
  uint32_t num_dev_75p;
  uint32_t num_dev_rst;
} abs_deviation_t;

static void print_abs_deviation(abs_deviation_t* abs_dev)
{
  PRINT("\n -- abs deviation stats:");
  PRINT("    num : %llu", abs_dev->num_vals);
  PRINT("    avg : %-10.1f (10%%: %-4.1f, 25%%: %-4.1f)", 
	abs_dev->avg, abs_dev->avg_10p, abs_dev->avg_25p);
  PRINT("    dev : %.1f", abs_dev->abs_dev);
  PRINT("    min : %-10.1f (elem: %llu)", abs_dev->min_dev, abs_dev->min_dev_idx);
  PRINT("    max : %-10.1f (elem: %llu)", abs_dev->max_dev, abs_dev->max_dev_idx);
  double v10p = 100 * 
    (1 - (abs_dev->num_vals - abs_dev->num_dev_10p) / (double) abs_dev->num_vals);
  PRINT("  0-10%% : %-10u (%3.1f)", abs_dev->num_dev_10p, v10p);
  double v25p = 100 
    * (1 - (abs_dev->num_vals - abs_dev->num_dev_25p) / (double) abs_dev->num_vals);
  PRINT(" 10-25%% : %-10u (%3.1f)", abs_dev->num_dev_25p, v25p);
  double v50p = 100 * 
    (1 - (abs_dev->num_vals - abs_dev->num_dev_50p) / (double) abs_dev->num_vals);
  PRINT(" 25-50%% : %-10u (%3.1f)", abs_dev->num_dev_50p, v50p);
  double v75p = 100 * 
    (1 - (abs_dev->num_vals - abs_dev->num_dev_75p) / (double) abs_dev->num_vals);
  PRINT(" 50-75%% : %-10u (%3.1f)", abs_dev->num_dev_75p, v75p);
  double vrest = 100 * 
    (1 - (abs_dev->num_vals - abs_dev->num_dev_rst) / (double) abs_dev->num_vals);
  PRINT("75-100%% : %-10u (%3.1f)", abs_dev->num_dev_rst, vrest);

}

static void
get_abs_deviation(ticks* vals, size_t num_vals, abs_deviation_t* abs_dev)
{
  abs_dev->num_vals = num_vals;
  ticks sum_vals = 0;
  uint32_t i;
  for (i = 0; i < num_vals; i++)
    {
      if ((int64_t) vals[i] < 0)
	{
	  vals[i] = 0;
	}
      sum_vals += vals[i];
    }
  double avg = sum_vals / (double) num_vals;
  abs_dev->avg = avg;
  double max_dev = 0;
  double min_dev = DBL_MAX;
  uint64_t max_dev_idx, min_dev_idx;
  uint32_t num_dev_10p = 0; ticks sum_vals_10p = 0; double dev_10p = 0.1 * avg;
  uint32_t num_dev_25p = 0; ticks sum_vals_25p = 0; double dev_25p = 0.25 * avg;
  uint32_t num_dev_50p = 0; double dev_50p = 0.5 * avg;
  uint32_t num_dev_75p = 0; double dev_75p = 0.75 * avg;
  uint32_t num_dev_rst = 0;

  double sum_adev = 0;
  for (i = 0; i < num_vals; i++)
    {
      double diff = vals[i] - avg;
      double ad = absd(diff);
      if (ad > max_dev)
	{
	  max_dev = ad;
	  max_dev_idx = i;
	}
      else if (ad < min_dev)
	{
	  min_dev = ad;
	  min_dev_idx = i;
	}

      if (ad <= dev_10p)
	{
	  num_dev_10p++;
	  sum_vals_10p += vals[i];
	}
      else if (ad <= dev_25p)
	{
	  num_dev_25p++;
	  sum_vals_25p += vals[i];
	}
      else if (ad <= dev_50p)
	{
	  num_dev_50p++;
	}
      else if (ad <= dev_75p)
	{
	  num_dev_75p++;
	}
      else
	{
	  num_dev_rst++;
	}

      sum_adev += ad;
    }
  abs_dev->min_dev = min_dev;
  abs_dev->min_dev_idx = min_dev_idx;
  abs_dev->max_dev = max_dev;
  abs_dev->max_dev_idx = max_dev_idx;
  abs_dev->num_dev_10p = num_dev_10p;
  abs_dev->num_dev_25p = num_dev_25p;
  abs_dev->num_dev_50p = num_dev_50p;
  abs_dev->num_dev_75p = num_dev_75p;
  abs_dev->num_dev_rst = num_dev_rst;

  abs_dev->avg_10p = sum_vals_10p / (double) num_dev_10p;
  abs_dev->avg_25p = sum_vals_25p / (double) num_dev_25p;

  double adev = sum_adev / num_vals;
  abs_dev->abs_dev = adev;
}


long long unsigned int nm = 1000000;
int ID;
ticks getticks_correction;
moesi_type_t moesi_type = STORE_ON_MODIFIED;
uint32_t inv_every_rep = 0;

int main(int argc, char **argv) {
  uint32_t ar;
  for (ar = 1; ar < argc; ar++)
    {
      if (strcmp(argv[ar], "h") == 0 ||
	  strcmp(argv[ar], "-h") == 0 ||
	  strcmp(argv[ar], "help") == 0 ||
	  strcmp(argv[ar], "-help") == 0 ||
	  strcmp(argv[ar], "--help") == 0
	  )
	{
	  PRINT("Usage:: ./%s NUM_CORES REPETITIONS CORE1 CORE2 [CORE3] MOESI_EVENT [INVALIDATE]", argv[0]);
	  PRINT("   where moesi event is one of the following:");
	  for (ar = 0; ar < 10; ar++)
	    {
	      PRINT("      %d - %s", ar, moesi_type_des[ar]);
	    }

	  return 0;
	}
    }


  if (argc > 1) {
    num_procs = atoi(argv[1]);
  }
  if (argc > 2) {
    nm = atol(argv[2]);
  }

  if (argc > 5 && num_procs == 2)
    {
      moesi_type = atoi(argv[5]);
    }
  else				/* num_procs = 3 */
    {
      moesi_type = atoi(argv[6]);
    }  



  PRINT(" -- moesi transitions under test: %d - %s", moesi_type, moesi_type_des[moesi_type]);

  if (argc > 6 || moesi_type == STORE_ON_SHARED && argc > 7)
    {
      PRINT(" -- flushing target cache line before each iteration");
      inv_every_rep = 1;
    }

  ID = 0;
  printf("NUM of processes  : %d\n", num_procs);
  printf("NUM of repetitions: %lld\n", nm);
  printf("testing moesi\n");


  ssmp_init(num_procs);

  volatile ssmp_msg_t* cache_line_anonym = (volatile ssmp_msg_t*) mmap(NULL, 4096, PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
  assert(cache_line_anonym != NULL);

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
  getticks_correction = getticks_correction_calc();


  volatile ssmp_msg_t* cache_line;
  ssmp_barrier_wait(0);
  char keyF[100];
  sprintf(keyF,"/cache_line");
  shm_unlink(keyF);
  //  PRINT("opening the cache line (%s)", keyF);

  int size = 20000 * sizeof(ssmp_msg_t);
  size_t num_cl = size / sizeof(ssmp_msg_t);;

  int ssmpfd = shm_open(keyF, O_CREAT | O_EXCL | O_RDWR, S_IRWXU | S_IRWXG);
  if (ssmpfd < 0) 
    {
      if (errno != EEXIST) 
	{
	  perror("In shm_open");
	  exit(1);
	}


      ssmpfd = shm_open(keyF, O_CREAT | O_RDWR, S_IRWXU | S_IRWXG);
      if (ssmpfd < 0) 
	{
	  perror("In shm_open");
	  exit(1);
	}
    }
  else {
    //    P("%s newly openned", keyF);
    if (ftruncate(ssmpfd, size) < 0) {
      perror("ftruncate failed\n");
      exit(1);
    }
  }

  cache_line = (volatile ssmp_msg_t *) mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, ssmpfd, 0);
  if (cache_line == NULL)
    {
      perror("cache_line = NULL\n");
      exit(134);
    }

  ssmp_barrier_wait(0);

  volatile ssmp_msg_t *msgp; 
  posix_memalign((void *) &msgp, 64, 2 * 64);
  assert(msgp != NULL);

  msgp->w0 = 1;
  cache_line->state = NO_MSG;



  sprintf(keyF,"/fbarrier");
  //  PRINT("opening my barrier buff (%s)", keyF);

  int size1 = sizeof(ssmp_msg_t);

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
    if (ftruncate(ssmpfd, size1) < 0) {
      perror("ftruncate failed\n");
      exit(1);
    }
  }

  volatile uint32_t* fbarrier = (volatile uint32_t*) mmap(NULL, size1, PROT_READ | PROT_WRITE, MAP_SHARED, ssmpfd, 0);
  if (fbarrier == NULL || (unsigned int) fbarrier == 0xFFFFFFFF) {
    perror("comb = NULL\n");
    exit(134);
  }

  uint32_t f;
  for (f = 0; f < 16; f++) 
    {
      fbarrier[f] = 0;
    }


  uint32_t c;
  for (c = 0; c < num_cl; c++)
    {
      _mm_clflush(cache_line + c);
      clflush(cache_line + c);
    }
  _mm_mfence();

  ssmp_barrier_wait(0);

  /* /\********************************************************************************* */
  /*  *  main functionality */
  /*  *********************************************************************************\/ */


#define PFI { ticks _s = getticks();
#define PFO(store)					\
  ticks _d = getticks() - _s - getticks_correction;	\
  (store) = _d; }
#define PFP(det, num_vals)					\
  {								\
    uint32_t _i; ticks _sum = 0;				\
    uint32_t p = (num_vals < 200) ? num_vals : 200;		\
    for (_i = 0; _i < p; _i++)					\
      {								\
	printf("[%3d : %3lld] ", _i, (int64_t) det[_i]);	\
      }								\
    abs_deviation_t ad;						\
    get_abs_deviation((det), nm, &ad);				\
    print_abs_deviation(&ad);					\
  }

  ticks* _ticks_det = (ticks*) malloc(2 * nm * sizeof(ticks));
  assert(_ticks_det != NULL);
  ticks* _ticks_det2 = _ticks_det + nm;

  uint64_t sum = 0;

  if (ID == 0) 
    {
      P("receiving core!");

      uint64_t reps;
      for (reps = 0; reps < nm; reps++)
	{
	  if (inv_every_rep)
	    {
	      clflush(cache_line);
	      _mm_mfence();
	      wait_cycles(10000);
	    }

	  B0;			/* BARRIER 0 */

	  switch (moesi_type)
	    {
	    case STORE_ON_MODIFIED:
	      {
		PFI;
		cache_line->w0 = reps;
		_mm_sfence();
		PFO(_ticks_det[reps]);
		B1;			/* BARRIER 1 */
		break;
	      }
	    case STORE_ON_MODIFIED_NO_SYNC:
	      {
		PFI;
		cache_line->w0 = reps;
		_mm_sfence();
		PFO(_ticks_det[reps]);
		break;
	      }
	    case STORE_ON_SINGLE_READER:
	      {
		int64_t cln = rands[reps];
		if (inv_every_rep)
		  {
		    cln = 0;
		  }

		asm("");
		PFI;
		sum += cache_line[cln].w0;
		_mm_lfence();
		PFO(_ticks_det[reps]);
		B1;			/* BARRIER 1 */
		break;
	      }
	    case STORE_ON_SHARED:
	      {
		int64_t cln = rands[reps];
		if (inv_every_rep)
		  {
		    cln = 0;
		  }
		ssmp_msg_t* cl = cache_line + cln;

		PFI;
		sum += cl->w0;
		_mm_lfence();
		PFO(_ticks_det[reps]);
		B1;			/* BARRIER 1 */
		B2;			/* BARRIER 2 */
		break;
	      }
	    case STORE_ON_OWNED_MINE:
	      {
		B1;			/* BARRIER 1 */
		PFI;
		sum += cache_line->w0;
		_mm_lfence();
		PFO(_ticks_det[reps]);
		B2;			/* BARRIER 2 */
		break;
	      }
	    case LOAD_FROM_MODIFIED:
	      {
		PFI;
		cache_line->w0 = reps;
		_mm_sfence();
		PFO(_ticks_det[reps]);
		B1;		
		break;
	      }
	    case STORE_ON_INVALID:
	      {
		B1;
		PFI;
		cache_line->w0 = reps;
		_mm_sfence();
		PFO(_ticks_det[reps]);		
		break;
	      }
	    case LOAD_FROM_INVALID:
	      {
		B1;			/* BARRIER 1 */
		int64_t cln = rands[reps];
		if (inv_every_rep)
		  {
		    cln = 0;
		  }

		asm("");
		PFI;
		sum += cache_line[cln].w0;
		_mm_lfence();
		PFO(_ticks_det[reps]);
		break;
	      }
	    case LOAD_FROM_SHARED:
	      {
		int64_t cln = rands[reps];
		if (inv_every_rep)
		  {
		    cln = 0;
		  }

		asm("");
		PFI;
		sum += cache_line[cln].w0;
		_mm_lfence();
		PFO(_ticks_det[reps]);
	      
		B1;			/* BARRIER 1 */
		B2;			/* BARRIER 2 */
		break;
	      }
	    case LOAD_FROM_OWNED:
	      {
		int64_t cln = rands[reps];
		if (inv_every_rep)
		  {
		    cln = 0;
		  }

		asm("");
		PFI;
		cache_line[cln].w0 = reps;
		_mm_sfence();
		PFO(_ticks_det[reps]);
	      
		B1;			/* BARRIER 1 */
		B2;			/* BARRIER 2 */
		break;
	      }
	    default:
	      break;
	    }

	  B3;			/* BARRIER 3 */
	}
    }
  else if (ID == 1) 			/* SENDER */
    {
      P("sending core!");

      uint64_t reps;
      for (reps = 0; reps < nm; reps++)
	{
	  if (inv_every_rep)
	    {
	      clflush(cache_line);
	      _mm_mfence();
	      wait_cycles(10000);
	    }

	  B0;			/* BARRIER 0 */

	  switch (moesi_type)
	    {
	    case STORE_ON_MODIFIED:
	      {
		B1;			/* BARRIER 1 */
		PFI;
		cache_line->w0 = reps;
		_mm_sfence();
		PFO(_ticks_det[reps]);
		break;
	      }
	    case STORE_ON_MODIFIED_NO_SYNC:
	      {
		PFI;
		cache_line->w0 = reps;
		_mm_sfence();
		PFO(_ticks_det[reps]);
		break;
	      }
	    case STORE_ON_SINGLE_READER:
	      {
		B1;			/* BARRIER 1 */
		int64_t cln = rands[reps];
		if (inv_every_rep)
		  {
		    cln = 0;
		  }

		asm("");
		PFI;
		cache_line[cln].w0 = reps;
		_mm_sfence();
		PFO(_ticks_det[reps]);
		break;
	      }
	    case STORE_ON_SHARED:
	      {
		B1;			/* BARRIER 1 */
		B2;			/* BARRIER 2 */

		int64_t cln = rands[reps];
		if (inv_every_rep)
		  {
		    cln = 0;
		  }
		ssmp_msg_t* cl = cache_line + cln;

		PFI;
		cl->w0 = reps;
		_mm_sfence();
		PFO(_ticks_det[reps]);
		break;
	      }
	    case STORE_ON_OWNED_MINE:
	      {
		PFI;
		cache_line->w0 = reps;
		_mm_sfence();
		PFO(_ticks_det[reps]);

		B1;			/* BARRIER 1 */
		B2;			/* BARRIER 2 */
		PFI;
		cache_line->w0 = reps;
		_mm_sfence();
		PFO(_ticks_det2[reps]);
		break;
	      case LOAD_FROM_MODIFIED:
		{
		  PFI;
		  sum += cache_line->w0;
		  _mm_lfence();
		  PFO(_ticks_det[reps]);
		  B1;		
		  break;
		}
	      }
	    case STORE_ON_INVALID:
	      {
		PFI;
		_mm_clflush(cache_line);
		_mm_mfence();
		PFO(_ticks_det[reps]);
		B1;
		break;
	      }
	    case LOAD_FROM_INVALID:
	      {
		int64_t cln = rands[reps];
		if (inv_every_rep)
		  {
		    cln = 0;
		  }

		PFI;
		_mm_clflush(cache_line + cln);
		_mm_mfence();
		PFO(_ticks_det[reps]);
		B1;
		break;
	      }
	    case LOAD_FROM_SHARED:
	      {
		B1;			/* BARRIER 1 */
		int64_t cln = rands[reps];
		if (inv_every_rep)
		  {
		    cln = 0;
		  }

		asm("");
		PFI;
		sum += cache_line[cln].w0;
		_mm_lfence();
		PFO(_ticks_det[reps]);
	      
		B2;			/* BARRIER 2 */
		break;
	      }
	    case LOAD_FROM_OWNED:
	      {
		B1;			/* BARRIER 1 */
		int64_t cln = rands[reps];
		if (inv_every_rep)
		  {
		    cln = 0;
		  }

		asm("");
		PFI;
		sum += cache_line[cln].w0;
		_mm_lfence();
		PFO(_ticks_det[reps]);
	      
		B2;			/* BARRIER 2 */
		break;
	      }
	    default:
	      break;
	    }

	  B3;			/* BARRIER 3 */
	}
    }
  else if (ID == 2)
    {
      P("sharing core!");

      uint64_t reps;
      for (reps = 0; reps < nm; reps++)
	{
	  if (inv_every_rep)
	    {
	      clflush(cache_line);
	      _mm_clflush(cache_line);
	      _mm_mfence();
	      wait_cycles(10000);
	    }

	  B0;			/* BARRIER 0 */

	  switch (moesi_type)
	    {
	    case STORE_ON_SHARED:
	      {
		int64_t cln = rands[reps];
		if (inv_every_rep)
		  {
		    cln = 0;
		  }
		ssmp_msg_t* cl = cache_line + cln;

		B1;			/* BARRIER 1 */
		PFI;
		sum += cl->w0;
		_mm_lfence();
		PFO(_ticks_det[reps]);
		B2;
		break;
	      }
	    case LOAD_FROM_SHARED:
	      {
		B1;			/* BARRIER 1 */
		B2;			/* BARRIER 2 */
		int64_t cln = rands[reps];
		if (inv_every_rep)
		  {
		    cln = 0;
		  }

		asm("");
		PFI;
		sum += cache_line[cln].w0;
		_mm_lfence();
		PFO(_ticks_det[reps]);
		break;
	      }
	    case LOAD_FROM_OWNED:
	      {
		B1;			/* BARRIER 1 */
		int64_t cln = rands[reps];
		if (inv_every_rep)
		  {
		    cln = 0;
		  }

		asm("");
		PFI;
		sum += cache_line[cln].w0;
		_mm_lfence();
		PFO(_ticks_det[reps]);
	      
		B2;			/* BARRIER 2 */
		break;
	      }
	    case STORE_ON_MODIFIED:
	    case STORE_ON_SINGLE_READER:
	    case STORE_ON_OWNED_MINE:
	    case LOAD_FROM_MODIFIED:
	    case STORE_ON_INVALID:
	    case LOAD_FROM_INVALID:
	    default:
	      break;
	    }

	  B3;			/* BARRIER 3 */      
	}
    }

  PRINT(" -- sum = %llu", sum);


  uint32_t id;
  for (id = 0; id < num_procs; id++)
    {
      if (ID == id)
	{
	  PRINT(" *** Core %2d *******************************************************", ID);
	  //	  PF_PRINT;
	  PFP(_ticks_det, nm);
	  if (ID && moesi_type == STORE_ON_OWNED_MINE)
	    {
	      PFP(_ticks_det2, nm);
	    }
	}
      ssmp_barrier_wait(0);
    }
  ssmp_barrier_wait(10);


  if (ID == 0)
    {
      switch (moesi_type)
	{
	case STORE_ON_MODIFIED:
	  {
	    if (inv_every_rep)
	      {
		PRINT(" ** Results from Core 0 : store on invalid");
		PRINT(" ** Results from Core 1 : store on modified");
	      }
	    else
	      {
		PRINT(" ** Results from Core 0 and 1 : store on modified");
	      }
	    break;
	  }
	case STORE_ON_MODIFIED_NO_SYNC:
	  {
	    if (inv_every_rep)
	      {
		PRINT(" ** Results do not make sense");
	      }
	    else
	      {
		PRINT(" ** Results from Core 0 and 1 : store on modified while another core is "
		      "also trying to do the same");
	      }
	    break;
	  }
	case STORE_ON_SINGLE_READER:
	  {
	    if (inv_every_rep)
	      {
		PRINT(" ** Results are NOT clear -- probably HW prefetching");
	      }
	    else
	      {
		PRINT(" ** Results from Core 0 : load from invalid");
		PRINT(" ** Results from Core 1 : store on exclusive");
	      }
	    break;
	  }
	case STORE_ON_SHARED:
	  {
	    PRINT(" ** Results from Core 0 & 2: load from invalid");
	    PRINT(" ** Results from Core 1 : store on shared");
	    break;
	  }
	case STORE_ON_OWNED_MINE:
	  {
	    PRINT(" ** Results from Core 0 : load from modified (makes it owned)");
	    if (inv_every_rep)
	      {
		PRINT(" ** Results 1 from Core 1 : store to invalid");
	      }
	    else
	      {
		PRINT(" ** Results 1 from Core 1 : store to modified mine");
	      }

	    PRINT(" ** Results 2 from Core 1 : store to owned mine");
	    break;
	  case LOAD_FROM_MODIFIED:
	    {
	      if (inv_every_rep)
		{
		  PRINT(" ** Results from Core 0 : store to invalid");
		}
	      else
		{
		  PRINT(" ** Results from Core 0 : store to owned mine");
		}

	      PRINT(" ** Results from Core 1 : load from modified (makes it owned)");

	      break;
	    }
	  }
	case STORE_ON_INVALID:
	  {
	    PRINT(" ** Results from Core 0 : store on invalid");
	    PRINT(" ** Results from Core 1 : cache line flush");
	    break;
	  }
	case LOAD_FROM_INVALID:
	  {
	    PRINT(" ** Results from Core 0 : load from invalid");
	    PRINT(" ** Results from Core 1 : cache line flush");
	    break;
	  }
	case LOAD_FROM_SHARED:
	  {
	    PRINT(" ** Results from Core 0 : ignore");
	    PRINT(" ** Results from Core 1 : read from shared");
	    if (num_procs == 3)
	      {
		PRINT(" ** Results from Core 2 : read from shared");
	      }
	    break;
	  }
	case LOAD_FROM_OWNED:
	  {
	    PRINT(" ** Results from Core 0 : ignore");
	    PRINT(" ** Results from Core 1 : read from modified");
	    if (num_procs == 3)
	      {
		PRINT(" ** Results from Core 2 : read from owned");
	      }
	    else
	      {
		PRINT(" ** Need 3 processes to achieve LOAD_FROM_SHARED");
	      }
	    break;
	  }
	default:
	  break;
	}
    }



  shm_unlink(keyF);
  sprintf(keyF,"/cache_line");
  shm_unlink(keyF);
  ssmp_term();
  return 0;
}

int64_t rands[] = {417, 915, 542, 78, 741, 699, 431, 499, 594, 674, 679, 138, 214, 718, 883, 25, 397, 598, 746, 145, 583, 881, 177, 439, 142, 512, 872, 392, 27, 596, 919, 444, 461, 875, 605, 513, 306, 104, 459, 332, 135, 597, 546, 205, 481, 923, 602, 669, 747, 366, 903, 277, 806, 45, 789, 30, 437, 816, 626, 356, 612, 490, 170, 839, 95, 35, 552, 494, 829, 687, 92, 375, 893, 925, 299, 847, 708, 320, 947, 74, 223, 224, 880, 268, 365, 262, 57, 533, 889, 766, 379, 288, 337, 827, 323, 482, 817, 312, 66, 261, 39, 311, 538, 338, 159, 246, 11, 106, 321, 234, 682, 553, 855, 47, 264, 580, 705, 77, 436, 670, 414, 263, 993, 249, 994, 811, 561, 61, 424, 952, 724, 963, 643, 209, 654, 341, 882, 240, 23, 70, 604, 360, 2, 309, 742, 80, 97, 413, 846, 713, 406, 707, 569, 8, 120, 346, 961, 845, 870, 610, 774, 753, 850, 797, 541, 298, 220, 658, 574, 752, 6, 517, 853, 968, 276, 948, 676, 197, 148, 543, 270, 204, 226, 426, 75, 188, 200, 180, 38, 349, 721, 336, 921, 218, 496, 24, 451, 502, 279, 355, 248, 244, 656, 442, 965, 985, 587, 770, 190, 165, 196, 617, 353, 748, 391, 19, 88, 778, 867, 464, 229, 369, 743, 198, 343, 794, 733, 619, 236, 50, 44, 637, 814, 764, 802, 362, 507, 110, 898, 207, 330, 578, 192, 445, 690, 167, 399, 329, 714, 623, 368, 924, 313, 522, 638, 46, 29, 928, 308, 32, 962, 238, 423, 684, 232, 465, 851, 984, 706, 447, 79, 93, 776, 793, 716, 904, 914, 227, 101, 171, 130, 906, 761, 579, 939, 723, 818, 515, 146, 854, 732, 504, 163, 763, 632, 956, 831, 491, 212, 745, 325, 916, 951, 301, 187, 857, 62, 119, 937, 664, 636, 791, 848, 704, 33, 319, 865, 884, 897, 981, 639, 68, 942, 800, 555, 300, 273, 844, 316, 910, 987, 432, 510, 280, 781, 891, 166, 383, 758, 199, 698, 7, 427, 773, 909, 969, 401, 957, 833, 34, 114, 740, 109, 333, 72, 69, 430, 767, 803, 456, 736, 98, 520, 1, 434, 149, 820, 998, 305, 738, 415, 673, 409, 463, 861, 618, 317, 859, 949, 926, 213, 172, 531, 476, 866, 81, 83, 216, 954, 505, 363, 345, 837, 160, 832, 678, 629, 126, 930, 52, 102, 454, 936, 825, 193, 389, 628, 997, 911, 335, 181, 359, 386, 43, 471, 285, 754, 739, 560, 572, 731, 750, 215, 472, 843, 429, 5, 245, 692, 508, 500, 419, 155, 838, 143, 286, 307, 826, 184, 751, 786, 600, 603, 466, 783, 526, 728, 868, 274, 58, 176, 457, 940, 247, 241, 757, 425, 116, 469, 780, 566, 169, 283, 582, 256, 550, 501, 810, 156, 920, 933, 13, 691, 478, 60, 259, 398, 339, 36, 76, 938, 709, 622, 792, 871, 137, 237, 616, 819, 295, 953, 252, 186, 467, 390, 477, 575, 988, 611, 563, 54, 73, 719, 805, 108, 111, 627, 297, 755, 715, 222, 527, 989, 435, 260, 443, 536, 686, 608, 498, 615, 607, 726, 689, 387, 493, 407, 864, 194, 144, 374, 788, 251, 403, 210, 105, 269, 410, 115, 235, 395, 642, 231, 694, 100, 462, 266, 680, 852, 514, 152, 310, 396, 9, 808, 18, 292, 485, 874, 168, 86, 862, 573, 768, 661, 470, 422, 950, 289, 37, 815, 55, 460, 540, 340, 899, 67, 211, 281, 48, 577, 10, 822, 999, 934, 625, 326, 556, 85, 448, 777, 992, 606, 784, 239, 438, 807, 16, 830, 63, 592, 599, 348, 849, 405, 946, 290, 996, 358, 122, 710, 324, 275, 408, 929, 372, 334, 660, 539, 318, 630, 528, 586, 128, 328, 208, 823, 449, 545, 284, 202, 970, 291, 421, 813, 765, 94, 547, 183, 633, 681, 671, 905, 557, 762, 394, 980, 973, 132, 381, 174, 103, 131, 195, 519, 4, 361, 503, 483, 900, 675, 41, 568, 124, 787, 468, 978, 433, 730, 966, 258, 653, 91, 858, 975, 96, 735, 912, 15, 141, 20, 760, 727, 89, 278, 28, 294, 357, 40, 567, 480, 565, 667, 644, 734, 677, 364, 479, 558, 631, 378, 377, 717, 913, 804, 635, 647, 982, 416, 666, 650, 257, 117, 703, 509, 548, 441, 440, 649, 164, 955, 530, 927, 624, 652, 327, 161, 314, 17, 458, 179, 303, 549, 446, 118, 876, 56, 107, 178, 254, 506, 489, 995, 385, 668, 150, 960, 711, 524, 370, 125, 271, 614, 564, 771, 121, 779, 796, 879, 65, 836, 601, 918, 890, 147, 412, 450, 487, 0, 646, 162, 640, 841, 979, 113, 695, 350, 302, 972, 665, 873, 388, 87, 672, 648, 782, 756, 651, 173, 21, 613, 974, 243, 863, 3, 175, 492, 571, 380, 191, 932, 544, 293, 894, 856, 991, 90, 185, 700, 217, 453, 484, 922, 12, 525, 702, 869, 420, 497, 411, 225, 452, 693, 529, 964, 221, 824, 537, 908, 404, 228, 812, 112, 31, 373, 400, 835, 151, 201, 944, 296, 384, 134, 182, 53, 986, 189, 304, 354, 562, 620, 139, 688, 347, 888, 282, 892, 895, 367, 322, 935, 376, 902, 907, 701, 860, 230, 886, 588, 685, 801, 49, 584, 418, 785, 267, 521, 129, 712, 799, 759, 722, 255, 219, 495, 581, 534, 585, 516, 657, 455, 473, 82, 26, 428, 14, 662, 609, 737, 877, 59, 645, 250, 790, 655, 351, 42, 659, 158, 136, 958, 917, 828, 523, 590, 959, 127, 840, 559, 725, 22, 591, 931, 621, 729, 64, 887, 663, 744, 331, 570, 896, 140, 589, 749, 943, 977, 242, 967, 474, 488, 99, 272, 983, 342, 795, 206, 945, 769, 203, 84, 576, 535, 382, 697, 641, 971, 402, 842, 518, 287, 821, 265, 315, 990, 772, 593, 720, 809, 153, 551, 901, 878, 532, 885, 157, 775, 475, 976, 123, 71, 393, 352, 941, 595, 696, 154, 798, 133, 634, 683, 371, 233, 253, 51, 511, 834, 486, 344, 554};
