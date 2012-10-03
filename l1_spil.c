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

static double
get_abs_deviation(ticks* vals, size_t num_vals, double mean)
{
  double adev = 0;
  uint32_t i;
  if (d)
    {
      PRINT("get_abs_deviation(.., %d, %f)", num_vals, mean);
    }


  for (i = 0; i < num_vals; i++)
    {
      double diff = vals[i] - mean;
      double ad = absd(diff);
      if (d)
	{
	  printf("index %2d, value %3llu, diff %3.1f, abs diff %3.1f\n", i, vals[i], diff, ad);
	}
      adev += ad;
    }

  if (d)
    {
      PRINT("\ntotal: %f | avg: %f", adev, adev / num_vals);
      d = 0;
    }

  return (adev / num_vals);
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
  PRINT("opening my local buff (%s)", keyF);

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
  PRINT("opening my remote buff (%s)", keyF);

  
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

  {
    uint32_t* combpui = (uint32_t*) local;
    uint32_t i = 0;
    printf("w %02d @ %p -- %llu\n", i, (combpui + i), ((uint64_t) (combpui + i) % 64));
  }


  uint8_t cur = 0;
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
  PRINT("opening my lock buff (%s)", keyF);

  size = sizeof(ssmp_msg_t);

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

  ssmp_barrier_wait(0);

  /* /\********************************************************************************* */
  /*  *  main functionality */
  /*  *********************************************************************************\/ */

  ticks wtimes = 0, wc = 0, access_lat = 0;

  if (ID % 2 == 0) 
    {
      P("service core!");

      unsigned int old = nm;
      _mm_clflush(remote);
      _mm_sfence();

      while(old) 
	{

	  /* PFIN(1); */
	  /* //	  PREFETCHW((remote + cur)); */
	  /* remote[cur].state = NO_MSG; */
	  /* PFOUT(1); */

	  ssmp_barrier_wait(2);
	  barrier_wait(finc);
	  /************************************************************* send */

	  //	  PREFETCHW((remote + cur));
	  ssmp_barrier_wait(3);
	  barrier_wait(finc+1);
	  wait_cycles(wcycles);

	  /* PF_START(2); */
	  /* msgp->state = MSG; */
	  /* //	  PREFETCHW((remote + cur)); */
	  /* memcpy(remote + cur, msgp, 64); */
	  /* PF_STOP(2); */

	  /* volatile uint32_t* st1 =  &remote[512].w0; */
	  /* volatile uint32_t* st2 =  &remote[1024].w0; */
	  /* ticks _s = getticks(); */
	  /* PREFETCHW(st1); */
	  /* *st1 = old; */

	  /* _mm_sfence(); */
	  /* ticks _e = getticks(); */
	  /* ticks _ev1 = _e - _s - getticks_correction; */

	  /* _s = getticks(); */
	  /* PREFETCHW(st2); */
	  /* *st2 = old; */
	  /* _mm_sfence(); */
	  /* _e = getticks(); */
	  /* ticks _ev2 = _e - _s - getticks_correction; */


	  /* wc += *st1; */
	  /* wc += *st2; */

	  /* 
	     0..511 -- messaging
	     512..3*512-1 -- L1
	     3*512..20*512-1
	     20*512-1..end
	  */


	  ticks _ev1 = 0, _ev2 = 0; ticks _s, _e;

	  if (wcycles >= 1)	/* clear L1 */
	    {
	      uint32_t i;
	      for (i = 512; i < 3*512; i++)
	      	{
	      	  volatile uint32_t* st = (volatile uint32_t*) &remote[i].w0;
	      	  wc += *st;
	      	  _mm_mfence();
	      	}
	    }

	  _mm_mfence();

	  if (wcycles >= 2) 	/* clear L2 */
	    {
	      uint32_t i;
	      for (i = 3*512; i < 20*512; i++)
	      	{
	      	  volatile uint32_t* st = (volatile uint32_t*) &remote[i].w0;
	      	  //		  PREFETCHW(st);
		  wc += *st;
	      	  _mm_mfence();
	      	}
	    }

	  _mm_mfence();
	  
	  if (wcycles >= 3)	/* clear L2 */
	    {
	      uint32_t i;
	      for (i = 20*512; i < (1706 * 48 + 23 * 512); i++)
	      	{
	      	  volatile uint32_t* st = (volatile uint32_t*) &remote[i].w0;
	      	  //		  PREFETCHW(st);
		  wc += *st;
	      	  _mm_mfence();
	      	}
	    }


	  _mm_mfence();





	  uint32_t reps = 100;
	  ticks* _lat_details = (ticks*) malloc(reps * sizeof(ticks));
	  assert(_lat_details != NULL);


	  PRINT("--- trying with no interference");
	  int32_t retries = -1;
	no_interference1:
	  retries++;
	  int64_t _lat_total = 0;
	  uint32_t invalid = 0, r;
	  for (r = 0; r < reps; r++)
	    {
	      _s = getticks();
	      uint32_t ss = remote[0].w0;
	      _mm_lfence();
	      _e = getticks();
	      int64_t _lat = _e - _s - getticks_correction;
	      if (_lat < 0 || _lat > 100) 
		{
		  invalid++;
		  _lat = 0;
		  r--;
		}
	      else
		{
		  _lat_details[r] = _lat;
		}
	      _lat_total += _lat;
	      wc += ss;
	      wait_cycles(256);
	    }

	  uint32_t i;
	  double _lat_avg = _lat_total / (double) reps;

	  double max_abs_dev_perc = 0.3;
	  if (_lat_avg <= 6)
	    {
	      double abs_dev =  get_abs_deviation(_lat_details, reps, _lat_avg);
	      double abs_dev_perc = 1 - ((_lat_avg - abs_dev) / _lat_avg);
	      if (abs_dev_perc < max_abs_dev_perc)
		{
		  PRINT("passed (retries %d)", retries);
		}
	      else
		{
		  goto no_interference1;
		}

	    }

	  wait_cycles(1234);

	  uint32_t num_cl = 4*1024;
	  uint32_t j, k, found = 0, attempts = 10;
	  while (!found && attempts--)
	    {
	      for (j = 0; j < num_cl; j++)
		{
		  if (j % 1024 == 0)
		    {
		      PRINT("j = %d", j);
		    }

		  volatile uint32_t* st1 = (volatile uint32_t*) &remote[j].w0;

		  int32_t success_ev = 0;

		  for (k = j + 512; k < num_cl; k += 512)
		    {
		      uint32_t* st2 = (volatile uint32_t*) &remote[k].w0;

		      int32_t abs_dev_retries = 0;
		    abs_dev_retry:
		      ;
		      uint32_t r, invalid = 0;
		      int64_t _lat_total = 0;

		      for (r = 0; r < reps; r++)
			{
			  wc += *st1;
			  wc += *st2;
			  _mm_lfence();

			  _s = getticks();
			  uint32_t ss = remote[0].w0;
			  _mm_lfence();
			  _e = getticks();
			  int64_t _lat = _e - _s - getticks_correction;
			  if (_lat < 0 || _lat > 100) 
			    {
			      invalid++;
			      _lat = 0;
			      r--;
			    }
			  else
			    {
			      _lat_details[r] = _lat;
			    }
			  _lat_total += _lat;
			  wc += ss;

			  wait_cycles(60);
			}

		      double _lat_avg = _lat_total / (double) reps;

		      uint32_t max_avg_lat = 20;
		      uint32_t min_avg_lat = 10;
		      double max_abs_dev_perc = 0.3;

		      if (_lat_avg > min_avg_lat && _lat_avg < max_avg_lat)
			{
			  double abs_dev =  get_abs_deviation(_lat_details, reps, _lat_avg);
			  double abs_dev_perc = 1 - ((_lat_avg - abs_dev) / _lat_avg);
			  if (abs_dev_perc < max_abs_dev_perc)
			    {

			      PRINT(" -- %4d & %4d (%4d = %d*512 + %d) (%.2f ticks) (invalid: %u) "\
				    "(abs_dev: %.2f | abs_dev_perc: %.2f) ------------------------",
				    j, k, k - j, 
				    (k - j) / 512, (k - j) % 512,  _lat_avg, invalid, abs_dev, abs_dev_perc);
			      uint32_t i;
			      for (i = 0; i < reps; i++)
				{
				  printf("%2d,", _lat_details[i]);
				}
			      printf("\n");

			      if ((k - j) % 512 == 0)
				{
				  success_ev++;
				  wait_cycles(512);
				}
			    }
			  else 
			    {
			      PRINT("*** rejected %4d & %4d (%4d) due abs_dev: avg: %5.2f, abs_dev: %5.2f, abs_dev_perc: %.2f",
				    j, k, k - j, _lat_avg, abs_dev, abs_dev_perc);
			      if (abs_dev_retries++ < 3)
				{
				  goto abs_dev_retry;
				}
			      success_ev--;
			    }
			}
		    }
		  if (success_ev > 2)
		    {
		      PRINT("****************************************");
		      PRINT("****************************************");
		      PRINT("*** accepting %d (%p)", j, st1);
		      PRINT("****************************************");
		      PRINT("****************************************");
		      found = 1;
		      break;
		    }
		}
	    }
	  

	  if (!found)
	    {
	      PRINT("****************************************");
	      PRINT("****************************************");
	      PRINT("*** NOT FOUND :( ***********************");
	      PRINT("****************************************");
	      PRINT("****************************************");
	      break;
	    }

	  ssmp_msg_t** remote_cl_conflict = (ssmp_msg_t**) malloc(18 * sizeof(void *));
	  assert(remote_cl_conflict != NULL);

	  uint32_t c;
	  for (c = 0; c < 18; c++)
	    {
	      remote_cl_conflict[c] = remote + j + (c * 512);
	      //	      PRINT("cl %2d is %4d (%p)", c, j + (c * 512), remote_cl_conflict[c]);
	    }



	  PRINT("testing ()()()()()()()()()()()()()()()()()()()()()()()()()()()()()()()()");
	  PRINT(" --- without interference");
	  
	  retries = -1;
	no_interference:
	  retries++;
	  _lat_total = 0;
	  invalid = 0;
	  for (r = 0; r < reps; r++)
	    {
	      _s = getticks();
	      uint32_t ss = remote[0].w0;
	      _mm_lfence();
	      _e = getticks();
	      int64_t _lat = _e - _s - getticks_correction;
	      wc += ss;
	      wait_cycles(1024);
	      if (_lat < 0 || _lat > 100) 
		{
		  invalid++;
		  _lat = 0;
		  r--;
		}
	      else
		{
		  _lat_details[r] = _lat;
		}
	      _lat_total += _lat;

	    }
	  _lat_total /= reps;
	  
	  if (_lat_total <= 10)
	    {
	      PRINT(" --  (%llu ticks) (invalid: %u) (retries: %d) ---------------------------------", 
		    _lat_total, invalid, retries);

	      for (i = 0; i < reps; i++)
		{
		  printf("%2d,", _lat_details[i]);
		}
	      printf("\n");
	    }
	  else
	    {
	      goto no_interference;
	    }

	  wait_cycles(1234);


	  PRINT(" --- with interference in L1");
	  for (j = 0; j < 18; j++)
	    {
	      for (k = j + 1; k < 18; k++)
		{
		 printf(" --- %2d & %2d :: ", j, k);
		  int32_t retries = -1;
		l1_inteference:
		  retries++;
		  wait_cycles(512);

		  _lat_total = 0;
		  invalid = 0;
		  for (r = 0; r < reps; r++)
		    {
		      wc += remote_cl_conflict[j]->w0;
		      wc += remote_cl_conflict[k]->w0;
		      _mm_lfence();

		      _s = getticks();
		      uint32_t ss = remote[0].w0;
		      _mm_lfence();
		      _e = getticks();
		      wc += ss;
		      int64_t _lat = _e - _s - getticks_correction;
		      if (_lat <= 0 || _lat > 100) 
			{
			  invalid++;
			  _lat = 0;
			  r--;
			}
		      else
			{
			  _lat_details[r] = _lat;
			}
		      _lat_total += _lat;
		      wait_cycles(256);
		    }

		  double _lat_avg = _lat_total / (double) reps;
		  double abs_dev =  get_abs_deviation(_lat_details, reps, _lat_avg);
		  double abs_dev_perc = 1 - ((_lat_avg - abs_dev) / _lat_avg);

		  if (_lat_avg >= 10)
		    {
		      printf(" -- (%.2f ticks) (invalid: %u) (retries: %d) " \
			    "(abs_dev: %.2f | abs_dev_perc: %.2f) ----------\n",
			    _lat_avg, invalid, retries, abs_dev, abs_dev_perc);
		      /* for (i = 0; i < reps; i++) */
		      /* 	{ */
		      /* 	  printf("%2d,", _lat_details[i]); */
		      /* 	} */
		      /* printf("\n"); */
		    }
		  else 
		    {
		      goto l1_inteference;
		    }

		}
	    }
	  
	  PRINT(" --- with interference in L2");
	  j = 0; k = 1;
	  uint32_t l2_times = 10;
	  while (l2_times--)
	    {
	      int32_t retries = -1;
	    l2_inteference:
	      retries++;
	      wait_cycles(512);

	      _lat_total = 0;
	      invalid = 0;
	      for (r = 0; r < reps; r++)
		{
		  wc += remote_cl_conflict[0]->w0;
		  wc += remote_cl_conflict[1]->w0;

		  wc += remote_cl_conflict[2]->w0;
		  wc += remote_cl_conflict[3]->w0;
		  wc += remote_cl_conflict[4]->w0;
		  wc += remote_cl_conflict[5]->w0;
		  wc += remote_cl_conflict[6]->w0;
		  wc += remote_cl_conflict[7]->w0;
		  wc += remote_cl_conflict[8]->w0;
		  wc += remote_cl_conflict[9]->w0;
		  wc += remote_cl_conflict[10]->w0;
		  wc += remote_cl_conflict[11]->w0;
		  wc += remote_cl_conflict[12]->w0;
		  wc += remote_cl_conflict[13]->w0;
		  wc += remote_cl_conflict[14]->w0;
		  wc += remote_cl_conflict[15]->w0;
		  wc += remote_cl_conflict[16]->w0;
		  wc += remote_cl_conflict[17]->w0;

		  _mm_lfence();

		  _s = getticks();
		  uint32_t ss = remote[0].w0;
		  _mm_lfence();
		  _e = getticks();
		  wc += ss;
		  int64_t _lat = _e - _s - getticks_correction;
		  if (_lat <= 0 || _lat > 100) 
		    {
		      invalid++;
		      _lat = 0;
		      r--;
		    }
		  else
		    {
		      _lat_details[r] = _lat;
		    }
		  _lat_total += _lat;
		  wait_cycles(256);
		}

	      double _lat_avg = _lat_total / (double) reps;
	      double abs_dev =  get_abs_deviation(_lat_details, reps, _lat_avg);
	      double abs_dev_perc = 1 - ((_lat_avg - abs_dev) / _lat_avg);

	      if (_lat_avg >= 10)
		{
		  PRINT(" -- (%.2f ticks) (invalid: %u) (retries: %d) " \
			"(abs_dev: %.2f | abs_dev_perc: %.2f) ------------------------",
			_lat_avg, invalid, retries, abs_dev, abs_dev_perc);
		  /* for (i = 0; i < reps; i++) */
		  /* 	{ */
		  /* 	  printf("%2d,", _lat_details[i]); */
		  /* 	} */
		  /* printf("\n"); */
		}
	      else 
		{
		  goto l2_inteference;
		}

	      /*   } */
	      /* else */
	      /*   { */
	      /*     PRINT("again.."); */
	      /*     goto l2_interference; */
	      /*   } */
	    }

	  /* PRINT(" --- try L2 manually"); */
	  /* for (j = 0; j = num_ */



	  break; 
	  _s = getticks();
	  uint32_t ss = remote[0].state;
	  _mm_lfence();
	  _e = getticks();
	  if (nm < 50)
	    {
	      ticks _lat = _e - _s - getticks_correction;
	      if (old < nm)
		{	
		  access_lat += _lat;;
		}
	      PRINT("accessed (%d) in %-20llu evicted in %5llu and %llu",
		    ss, _lat, _ev1, _ev2);

	    }
	  else
	    {
	      if (old < nm)
		{
		  access_lat += _e - _s - getticks_correction;
		}
	    }
	  wc += ss;


	  /* wait_cycles(500); */
	  /* PF_START(13); */
	  /* msgp->state = MSG; */
	  /* //	  PREFETCHW((remote + cur)); */
	  /* memcpy(remote + cur, msgp, 64); */
	  /* PF_STOP(13); */

	  ssmp_barrier_wait(15);
	  /* PFIN(14); */
	  /* //	  PREFETCHW((remote + cur)); */
	  /* remote[cur].state = NO_MSG; */
	  /* PFOUT(14); */
	  /************************************************************* end send */

	  /* if (msgp->w0 == 0) { */
	  /*   PRINT("done"); */
	  /*   break; */
	  /* } */

	  /* if (msgp->w0 != old) { */
	  /*   PRINT("w0 -- expected %d, got %d", old, msgp->w0); */
	  /* } */

	  old--;

	}
      PRINT("~ ~ ~ ~ ~ ~ ~ ~ ~ access avg %0.2f \t\t\t\t\twc=%d", 
	    access_lat / (double) (nm - 1), wc);
      PRINT("wtimes = %llu | avg = %f", wtimes, wtimes / (double) nm);
    }
  else 			/* SENDER */
    {
      P("app core!");
      long long int nm1 = nm;
      uint32_t wb = 1;

	
      while (nm1--)
	{

	  msgp->w0 = nm1;
	  msgp->w1 = nm1;
	  msgp->w2 = nm1;
	  msgp->w3 = nm1;
	  msgp->w4 = nm1;
	  msgp->w5 = nm1;
	  msgp->w6 = nm1;
	  msgp->w7 = nm1;
	  
	  ssmp_barrier_wait(2);
	  barrier_wait(finc);
	  ssmp_barrier_wait(3);
	  //	  msgp->state = local[cur].state;
	  //	  PREFETCH((local+cur));
	  barrier_wait(finc+1);
	  /************************************************************* recv */
	  /*   wait_cycles(10000); */
	  /*   PF_START(1); */
	  /* again: */

	  /*   //	  memcpy(msgp, local + cur, 64); */
	  /*   /\* msgp->state = local[cur].state; *\/ */
	  /*   /\* _mm_lfence(); *\/ */
	  /*   /\* if (msgp->state != MSG) *\/ */
	  /*   /\*   { *\/ */
	  /*   /\*     wtimes++; *\/ */
	  /*   /\*     //	      wait_cycles(150); *\/ */
	  /*   /\*     goto again; *\/ */
	  /*   /\*   } *\/ */
	  
	  /*   PF_STOP(1); */

	  break;
	  ssmp_barrier_wait(15);
	  /* local[!cur].state = NO_MSG; */
	  //	  cur = !cur;
	}

      PRINT("wtimes = %llu | avg = %f", wtimes, wtimes / (double) nm);
    }


  uint64_t nm1;
  for (nm1 = 0; nm1 < num_procs; nm1++)
    {
      if (ID == nm1)
	{
	  PF_PRINT;
	  //	  printf("[%02d] %lld ticks/msg\n", ID, (long long unsigned int) _ticksm);

	  /* printf("sent %lld msgs\n\t" */
	  /* 	 "in %f secs\n\t" */
	  /* 	 "%.2f msgs/us\n\t" */
	  /* 	 "%f ns latency\n" */
	  /* 	 "in ticks:\n\t" */
	  /* 	 "in %lld ticks\n\t" */
	  /* 	 "%lld ticks/msg\n", nm, _time, ((double)nm/(1000*1000*_time)), lat, */
	  /* 	 (long long unsigned int) _ticks, (long long unsigned int) _ticksm); */
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

