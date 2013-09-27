#include "ssmp.h"
#include "ssmpthread.h"
#include <pthread.h>

//#define SSMP_DEBUG


/* ------------------------------------------------------------------------------- */
/* library variables : moveable to ssmp.h, wait for code update */
/* ------------------------------------------------------------------------------- */
static __thread int ssmp_id_;
ssmp_barrier_t *ssmp_barrier;
int *ues_initialized;
static uint32_t ssmp_my_core;

#if defined(__tile__)
DynamicHeader *udn_header; //headers for messaging
cpu_set_t cpus;
#else
static ssmp_msg_t *ssmp_mem;
volatile ssmp_msg_t **ssmp_recv_buf;
volatile ssmp_msg_t **ssmp_send_buf;
#endif

/* ------------------------------------------------------------------------------- */
/* init / term the MP system */
/* ------------------------------------------------------------------------------- */


/* ---------------------------------------------------------------------------------------- */
/* x86  ---------------------------------------------------------------------------- */
/* ---------------------------------------------------------------------------------------- */


void ssmpthread_init(int num_threads) {
  //create the shared space which will be managed by the allocator
  unsigned int sizeb, sizeckp, sizeui, sizecnk, size;;

  sizeb = SSMP_NUM_BARRIERS * sizeof(ssmp_barrier_t);
  sizeckp = 0;
  sizeui = num_threads * sizeof(int);
  sizecnk = 0;//don't need ssmp_chunk_buf
  size = sizeb + sizeckp + sizeui + sizecnk;

  char keyF[100];
  sprintf(keyF, SSMP_MEM_NAME);

  int ssmpfd = shm_open(keyF, O_CREAT | O_EXCL | O_RDWR, S_IRWXU | S_IRWXG);
  if (ssmpfd<0)
    {
      if (errno != EEXIST)
	{
	  perror("In shm_open");
	  exit(1);
	}

      //this time it is ok if it already exists
      ssmpfd = shm_open(keyF, O_CREAT | O_RDWR, S_IRWXU | S_IRWXG);
      if (ssmpfd<0)
	{
	  perror("In shm_open");
	  exit(1);
	}
    }
  else
    {
      if (ftruncate(ssmpfd, size) < 0) {
	perror("ftruncate failed\n");
	exit(1);
      }
    }

  ssmp_mem = (ssmp_msg_t *) mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, ssmpfd, 0);
  if (ssmp_mem == NULL)
    {
      perror("ssmp_mem = NULL\n");
      exit(134);
    }

  long long unsigned int mem_just_int = (long long unsigned int) ssmp_mem;
  ssmp_barrier = (ssmp_barrier_t *) (mem_just_int);
  ues_initialized = (int *) (mem_just_int + sizeb + sizeckp);

  int bar;
  for (bar = 0; bar < SSMP_NUM_BARRIERS; bar++) 
    {
      ssmp_barrier_init(bar, 0xFFFFFFFFFFFFFFFF, NULL);
    }
  ssmp_barrier_init(1, 0xFFFFFFFFFFFFFFFF, color_app);

  _mm_mfence();//maybe not usefull since we spawn threads afterwards
}


void ssmpthread_mem_init(int id, int num_ues) {
  ssmp_id_ = id;

  ssmp_recv_buf = (volatile ssmp_msg_t **) malloc(num_ues * sizeof(ssmp_msg_t *));
  ssmp_send_buf = (volatile ssmp_msg_t **) malloc(num_ues * sizeof(ssmp_msg_t *));
  if (ssmp_recv_buf == NULL || ssmp_send_buf == NULL) {
    perror("malloc@ ssmp_mem_init\n");
    exit(-1);
  }

  char keyF[100];
  unsigned int size = (num_ues - 1) * sizeof(ssmp_msg_t);
  unsigned int core;
  sprintf(keyF, "/ssmp_core%03d", id);
  
  if (num_ues == 1) return;

  
  int ssmpfd = shm_open(keyF, O_CREAT | O_EXCL | O_RDWR, S_IRWXU | S_IRWXG);
  if (ssmpfd < 0) {
    if (errno != EEXIST) {
      perror("In shm_open");
      exit(1);
    }

    ssmpfd = shm_open(keyF, O_CREAT | O_RDWR, S_IRWXU | S_IRWXG);
    if (ssmpfd<0) {
      perror("In shm_open");
      exit(1);
    }
  }
  else {
    if (ftruncate(ssmpfd, size) < 0) {
      perror("ftruncate failed\n");
      exit(1);
    }
  }

  ssmp_msg_t * tmp = (ssmp_msg_t *) mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, ssmpfd, 0);
  if (tmp == NULL)
    {
      perror("tmp = NULL\n");
      exit(134);
    }

  for (core = 0; core < num_ues; core++) {
    if (id == core) {
      continue;
    }

    ssmp_recv_buf[core] = tmp + ((core > id) ? (core - 1) : core);
    ssmp_recv_buf[core]->state = 0;
  }

 /********************************************************************************
    initialized own buffer
    ********************************************************************************
*/

  ssmp_barrier_wait(0);
  
  for (core = 0; core < num_ues; core++) {
    if (core == id) {
      continue;
    }

    sprintf(keyF, "/ssmp_core%03d", core);
  
    int ssmpfd = shm_open(keyF, O_CREAT | O_EXCL | O_RDWR, S_IRWXU | S_IRWXG);
    if (ssmpfd < 0) {
      if (errno != EEXIST) {
	perror("In shm_open");
	exit(1);
      }

      ssmpfd = shm_open(keyF, O_CREAT | O_RDWR, S_IRWXU | S_IRWXG);
      if (ssmpfd<0) {
	perror("In shm_open");
	exit(1);
      }
    }
    else {
      if (ftruncate(ssmpfd, size) < 0) {
	perror("ftruncate failed\n");
	exit(1);
      }
    }

    ssmp_msg_t * tmp = (ssmp_msg_t *) mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, ssmpfd, 0);
    if (tmp == NULL)
      {
	perror("tmp = NULL\n");
	exit(134);
      }

    ssmp_send_buf[core] = tmp + ((core < id) ? (id - 1) : id);
  }

  ues_initialized[id] = 1;

  //  SP("waiting for all to be initialized!");
  int ue;
  for (ue = 0; ue < num_ues; ue++) {
    while(!ues_initialized[ue]) 
      {
	_mm_pause();
	_mm_mfence();
      };
  }
  //  SP("\t\t\tall initialized!");
}

void set_thread(int cpu) {
  ssmp_my_core = cpu;
  cpu_set_t cpuset;
  pthread_t thread = pthread_self();

  CPU_ZERO(&cpuset);
  CPU_SET(cpu, &cpuset);

  if (pthread_setaffinity_np(thread, sizeof(cpu_set_t), &cpuset) != 0) {
	  printf("Problem with setting thread affinity\n");
	  exit(3);
  }
  pthread_getaffinity_np(thread, sizeof(cpuset), &cpuset);
  int j;
  for (j = 0; j < CPU_SETSIZE; j++) {
	  if (CPU_ISSET(j, &cpuset))
		  printf("    CPU %d\n", j);
  }
}


inline uint32_t get_thread() {
  return ssmp_my_core;
}

inline int ssmpthread_id() {
  return ssmp_id_;
}

/*inline int ssmpthread_num_ues() {
  return ssmp_num_ues_;
}*/

