#include "ssmp.h"
#include <pthread.h>


//#define SSMP_DEBUG


#if defined(OPTERON) || defined(TILERA) || defined(local)
uint8_t id_to_core[] =
  {
    0, 1, 2, 3, 4, 5,
    6, 7, 8, 9, 10, 11,
    12, 13, 14, 15, 16, 17,
    18, 19, 20, 21, 22, 23,
    24, 25, 26, 27, 28, 29,
    30, 31, 32, 33, 34, 35,
    36, 37, 38, 39, 40, 41,
    42, 43, 44, 45, 46, 47
  };
#elif defined(XEON)

uint8_t id_to_core[] =
  {
    00, 41, 42, 43, 44, 45, 46, 47, 48, 49, 
    50, 51, 52, 53, 54, 55, 56, 57, 58, 59, 
    60, 61, 62, 63, 64, 65, 66, 67, 68, 69, 
    70, 71, 72, 73, 74, 75, 76, 77, 78, 79,
    10, 01, 02, 03, 04, 05, 06, 07,  8,  9, 
    11, 12, 13, 14, 15, 16, 17, 18, 19, 20,
    21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 
    31, 32, 33, 34, 35, 36, 37, 38, 39, 40,
  };

uint8_t id_to_node[] =
  {
    4, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 
    2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 
    3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
    4, 4, 4, 4, 4, 4, 4, 4, 4, 
    5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 
    6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 
    7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 
  };

#elif defined(NIAGARA)
uint8_t id_to_core[] =
  {
    0, 8, 16, 24, 32, 40, 48, 56,
    9, 17, 25, 33, 41, 49, 57,
    10, 18, 26, 34, 42, 50, 58,
    11, 19, 27, 35, 43, 51, 59,
    12, 20, 28, 36, 44, 52, 60,
    13, 21, 29, 37, 45, 53, 61,
    14, 22, 30, 38, 46, 54, 62,
    15, 23, 31, 39, 47, 55, 63,
    1, 2, 3, 4, 5, 6, 7
  };
#endif

/* const uint8_t node_to_node_hops[8][8] = */
/*   { */
/*     /\* 0  1  2  3  4  5  6  7           *\/ */
/*     {0, 1, 2, 3, 2, 3, 2, 3},	/\* 0 *\/ */
/*     {1, 0, 3, 2, 3, 2, 3, 2},	/\* 1 *\/ */
/*     {2, 3, 0, 1, 2, 3, 2, 3},	/\* 2 *\/ */
/*     {3, 2, 1, 0, 3, 2, 3, 2},	/\* 3 *\/ */
/*     {2, 3, 2, 3, 0, 1, 2, 3},	/\* 4 *\/ */
/*     {3, 2, 3, 2, 1, 0, 3, 2},	/\* 5 *\/ */
/*     {2, 3, 2, 3, 2, 3, 0, 1},	/\* 6 *\/ */
/*     {3, 2, 3, 2, 3, 2, 1, 0},	/\* 7 *\/ */
/*   }; */

/* ------------------------------------------------------------------------------- */
/* library variables */
/* ------------------------------------------------------------------------------- */

int ssmp_num_ues_;
int ssmp_id_;
int last_recv_from;
ssmp_barrier_t *ssmp_barrier;
int *ues_initialized;
static __thread uint32_t ssmp_my_core;

#if defined(__tile__)
DynamicHeader *udn_header; //headers for messaging
cpu_set_t cpus;
#else
static ssmp_msg_t *ssmp_mem;
volatile ssmp_msg_t **ssmp_recv_buf;
volatile ssmp_msg_t **ssmp_send_buf;
static ssmp_chunk_t *ssmp_chunk_mem;
ssmp_chunk_t **ssmp_chunk_buf;
#endif


/* ------------------------------------------------------------------------------- */
/* init / term the MP system */
/* ------------------------------------------------------------------------------- */

#if defined(NIAGARA)
void ssmp_init(int num_procs)
{

  //create the shared space which will be managed by the allocator
  uint32_t sizem, sizeb, sizeckp, sizeui, sizecnk, size;;

  sizem = (num_procs * num_procs) * sizeof(ssmp_msg_t);
  sizeb = SSMP_NUM_BARRIERS * sizeof(ssmp_barrier_t);
  sizeckp = 0;
  sizeui = num_procs * sizeof(int);
  /* sizecnk = num_procs * sizeof(ssmp_chunk_t); */
  sizecnk = 0;
  size = sizem + sizeb + sizeckp + sizeui + sizecnk;

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
  ssmp_barrier = (ssmp_barrier_t *) (mem_just_int + sizem);
  /* ssmp_chk_t *chks = (ssmp_chk_t *) (mem_just_int + sizem + sizeb); */
  ues_initialized = (int *) (mem_just_int + sizem + sizeb + sizeckp);
  ssmp_chunk_mem = (ssmp_chunk_t *) (mem_just_int + sizem + sizeb + sizeckp + sizeui);

  /* int ue; */
  /* for (ue = 0; ue < SSMP_NUM_BARRIERS * num_procs; ue++) { */
  /*   chks[ue] = 0; */
  /*   _mm_sfence(); */
  /* } */

  int bar;
  for (bar = 0; bar < SSMP_NUM_BARRIERS; bar++) {
    /* ssmp_barrier[bar].checkpoints = (chks + (bar * num_procs)); */
    ssmp_barrier_init(bar, 0xFFFFFFFFFFFFFFFF, NULL);
  }
  ssmp_barrier_init(1, 0xFFFFFFFFFFFFFFFF, color_app);

  _mm_mfence();
}

void ssmp_mem_init(int id, int num_ues) 
{  
  ssmp_id_ = id;
  ssmp_num_ues_ = num_ues;
  last_recv_from = (id + 1) % num_ues;

  ssmp_recv_buf = (volatile ssmp_msg_t **) malloc(num_ues * sizeof(ssmp_msg_t *));
  ssmp_send_buf = (volatile ssmp_msg_t **) malloc(num_ues * sizeof(ssmp_msg_t *));
  /* ssmp_chunk_buf = (ssmp_chunk_t **) malloc(num_ues * sizeof(ssmp_chunk_t *)); */
  if (ssmp_recv_buf == NULL || ssmp_send_buf == NULL) /* || ssmp_chunk_buf == NULL) { */
    {
      perror("malloc@ ssmp_mem_init\n");
      exit(-1);
    }

  int core;
  for (core = 0; core < num_ues; core++) 
    {
      ssmp_recv_buf[core] = ssmp_mem + (id * num_ues) + core;
      ssmp_recv_buf[core]->state = 0;

      ssmp_send_buf[core] = ssmp_mem + (core * num_ues) + id;
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

void ssmp_term() 
{
  if (ssmp_id_ == 0 && shm_unlink(SSMP_MEM_NAME) < 0)
    {
      printf("Could not unlink ssmp_mem\n");
      fflush(stdout);
    }
}


#elif defined(TILERA)

void
ssmp_init(int num_procs)
{
  ssmp_num_ues_ = num_procs;

  //initialize shared memory
  tmc_cmem_init(0);

  // Reserve the UDN rectangle that surrounds our cpus.
  if (tmc_udn_init(&cpus) < 0)
    tmc_task_die("Failure in 'tmc_udn_init(0)'.");

  ssmp_barrier = (tmc_sync_barrier_t *) tmc_cmem_calloc(SSMP_NUM_BARRIERS, sizeof (tmc_sync_barrier_t));
  if (ssmp_barrier == NULL)
    {
      tmc_task_die("Failure in allocating mem for barriers");
    }

  uint32_t b;
  for (b = 0; b < num_procs; b++)
    {
      tmc_sync_barrier_init(ssmp_barrier + b, num_procs);
    }

  if (tmc_cpus_count(&cpus) < num_procs)
    {
      tmc_task_die("Insufficient cpus (%d < %d).", tmc_cpus_count(&cpus), num_procs);
    }

  tmc_task_watch_forked_children(1);

}

void ssmp_mem_init(int id, int num_ues) 
{  
  ssmp_id_ = id;
  ssmp_num_ues_ = num_ues;

  // Now that we're bound to a core, attach to our UDN rectangle.
  if (tmc_udn_activate() < 0)
    tmc_task_die("Failure in 'tmc_udn_activate()'.");

  udn_header = (DynamicHeader *) malloc(num_ues * sizeof (DynamicHeader));

  if (udn_header == NULL)
    {
      tmc_task_die("Failure in allocating dynamic headers");
    }

  int r;
  for (r = 0; r < num_ues; r++)
    {
      int _cpu = tmc_cpus_find_nth_cpu(&cpus, id_to_core[r]);
      DynamicHeader header = tmc_udn_header_from_cpu(_cpu);
      udn_header[r] = header;
    }

}

void ssmp_term() 
{
}

/* ---------------------------------------------------------------------------------------- */
#else  /* x86  ---------------------------------------------------------------------------- */
/* ---------------------------------------------------------------------------------------- */


void ssmp_init(int num_procs)
{
  //create the shared space which will be managed by the allocator
  unsigned int sizeb, sizeckp, sizeui, sizecnk, size;;

  sizeb = SSMP_NUM_BARRIERS * sizeof(ssmp_barrier_t);
  sizeckp = 0;
  sizeui = num_procs * sizeof(int);
  sizecnk = num_procs * sizeof(ssmp_chunk_t);
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
  ssmp_chunk_mem = (ssmp_chunk_t *) (mem_just_int + sizeb + sizeckp + sizeui);

  int bar;
  for (bar = 0; bar < SSMP_NUM_BARRIERS; bar++) 
    {
      ssmp_barrier_init(bar, 0xFFFFFFFFFFFFFFFF, NULL);
    }
  ssmp_barrier_init(1, 0xFFFFFFFFFFFFFFFF, color_app);

  _mm_mfence();
}

void ssmp_mem_init(int id, int num_ues) 
{  
  ssmp_id_ = id;
  ssmp_num_ues_ = num_ues;
  last_recv_from = (id + 1) % num_ues;

  ssmp_recv_buf = (volatile ssmp_msg_t **) malloc(num_ues * sizeof(ssmp_msg_t *));
  ssmp_send_buf = (volatile ssmp_msg_t **) malloc(num_ues * sizeof(ssmp_msg_t *));
  ssmp_chunk_buf = (ssmp_chunk_t **) malloc(num_ues * sizeof(ssmp_chunk_t *));
  if (ssmp_recv_buf == NULL || ssmp_send_buf == NULL || ssmp_chunk_buf == NULL) {
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
  
    ssmp_chunk_buf[core] = ssmp_chunk_mem + core;
    ssmp_chunk_buf[core]->state = 0;
  }

  /*********************************************************************************
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


void
ssmp_term() 
{
  if (ssmp_id_ == 0 && shm_unlink(SSMP_MEM_NAME) < 0)
    {
      shm_unlink("/ssmp_mem");
    }
  char keyF[100];
  sprintf(keyF, "/ssmp_core%03d", ssmp_id_);
  shm_unlink(keyF);
}
#endif 



/* ------------------------------------------------------------------------------- */
/* color-based initialization fucntions */
/* ------------------------------------------------------------------------------- */

void ssmp_color_buf_init(ssmp_color_buf_t *cbuf, int (*color)(int))
{
#if !defined(TILERA)
  if (cbuf == NULL) {
    cbuf = (ssmp_color_buf_t *) malloc(sizeof(ssmp_color_buf_t));
    if (cbuf == NULL) {
      perror("malloc @ ssmp_color_buf_init");
      exit(-1);
    }
  }

  uint32_t *participants = (uint32_t *) malloc(ssmp_num_ues_ * sizeof(uint32_t));
  if (participants == NULL) {
    perror("malloc @ ssmp_color_buf_init");
    exit(-1);
  }

  uint32_t ue, num_ues = 0;
  for (ue = 0; ue < ssmp_num_ues_; ue++) {
    if (ue == ssmp_id_) {
      participants[ue] = 0;
      continue;
    }

    participants[ue] = color(ue);
    if (participants[ue]) {
      num_ues++;
    }
  }

  cbuf->num_ues = num_ues;

  uint32_t size_buf = num_ues * sizeof(ssmp_msg_t *);
  uint32_t size_pad = 0;

  if (size_buf % SSMP_CACHE_LINE_SIZE)
    {
      size_pad = (SSMP_CACHE_LINE_SIZE - (size_buf % SSMP_CACHE_LINE_SIZE)) / sizeof(ssmp_msg_t *);
      size_buf += size_pad * sizeof(ssmp_msg_t *);
    }

  cbuf->buf = (volatile ssmp_msg_t **) malloc(size_buf);
  if (cbuf->buf == NULL)
    {
      perror("malloc @ ssmp_color_buf_init");
      exit(-1);
    }

#if defined(__sparc)
  cbuf->buf_state = (volatile uint8_t**) malloc(size_buf);
#else
  cbuf->buf_state = (volatile unsigned int **) malloc(size_buf);
#endif	/* __sparc__ */
  if (cbuf->buf_state == NULL) {
    perror("malloc @ ssmp_color_buf_init");
    exit(-1);
  }
  
#if defined(__sparc)
  uint32_t size_from = num_ues * sizeof(uint8_t);
#else
  uint32_t size_from = num_ues * sizeof(uint32_t);
#endif	/* __sparc__ */

  if (size_from % SSMP_CACHE_LINE_SIZE)
    {
#if defined(__sparc)
      size_pad = (SSMP_CACHE_LINE_SIZE - (size_from % SSMP_CACHE_LINE_SIZE)) / sizeof(uint8_t);
      size_from += size_pad * sizeof(uint8_t);
#else
      size_pad = (SSMP_CACHE_LINE_SIZE - (size_from % SSMP_CACHE_LINE_SIZE)) / sizeof(uint32_t);
      size_from += size_pad * sizeof(uint32_t);
#endif	/* __sparc__ */
    }


#if defined(__sparc)
  cbuf->from = (uint8_t*) memalign(SSMP_CACHE_LINE_SIZE, size_from);
#else
  cbuf->from = (uint32_t *) malloc(size_from);
#endif	/* __sparc__  */
  if (cbuf->from == NULL)
    {
      perror("malloc @ ssmp_color_buf_init");
      exit(-1);
    }
    
  uint32_t buf_num = 0;
  for (ue = 0; ue < ssmp_num_ues_; ue++)
    {
      if (participants[ue])
	{
	  cbuf->buf[buf_num] = ssmp_recv_buf[ue];
	  cbuf->buf_state[buf_num] = &ssmp_recv_buf[ue]->state;
	  cbuf->from[buf_num] = ue;
	  buf_num++;
	}
    }

  free(participants);
#endif	/* !tilera */
}


inline void ssmp_color_buf_free(ssmp_color_buf_t *cbuf) {
  free(cbuf->buf);
}



/* ------------------------------------------------------------------------------- */
/* barrier functions */
/* ------------------------------------------------------------------------------- */

int color_app(int id) {
  return ((id % 2) ? 1 : 0);
}

inline ssmp_barrier_t * ssmp_get_barrier(int barrier_num) {
  if (barrier_num < SSMP_NUM_BARRIERS) {
    return (ssmp_barrier + barrier_num);
  }
  else {
    return NULL;
  }
}

int color(int id) {
  return (10*id);
}

inline void ssmp_barrier_init(int barrier_num, long long int participants, int (*color)(int)) {
  if (barrier_num >= SSMP_NUM_BARRIERS) {
    return;
  }

#if defined(__tile__)
  uint32_t n, num_part = 0;
  for (n = 0; n < ssmp_num_ues_; n++)
    {
      if (color(n))
	{
	  num_part++;
	}
    }

    tmc_sync_barrier_init(ssmp_barrier + barrier_num, num_part);

#else
  ssmp_barrier[barrier_num].participants = 0xFFFFFFFFFFFFFFFF;
  ssmp_barrier[barrier_num].color = color;
  ssmp_barrier[barrier_num].ticket = 0;
  ssmp_barrier[barrier_num].cleared = 0;
  #endif
}

void 
ssmp_barrier_wait(int barrier_num) 
{
  if (barrier_num >= SSMP_NUM_BARRIERS)
    {
      return;
    }

#if defined(__tile__)
  tmc_sync_barrier_wait(ssmp_barrier + barrier_num);
#else
  _mm_sfence();
  _mm_lfence();
  ssmp_barrier_t *b = &ssmp_barrier[barrier_num];
  PD(">>Waiting barrier %d\t(v: %d)", barrier_num, version);

  int (*col)(int);
  col = b->color;

  uint64_t bpar = (uint64_t) b->participants;
  uint32_t num_part = 0;

  int from;
  for (from = 0; from < ssmp_num_ues_; from++) 
    {
      /* if there is a color function it has priority */
      if (col != NULL) 
	{
	  num_part += col(from);
	  if (from == ssmp_id_ && !col(from))
	    {
	      return;
	    }
	}
      else 
	{
	  uint32_t is_part = (uint32_t) (bpar & 0x0000000000000001);
	  num_part += is_part;
	  if (ssmp_id_ == from && !is_part)
	    {
	      return;
	    }
	  bpar >>= 1;
	}
    }
  
  
  _mm_lfence();
  uint32_t reps = 1;
  while (b->cleared == 1)
    {
      _mm_pause_rep(reps++);
      reps &= 255;
      _mm_lfence();
    }

  uint32_t my_ticket = IAF_U32(&b->ticket);
  if (my_ticket == num_part)
    {
      b->cleared = 1;
    }

  _mm_mfence();

  reps = 1;
  while (b->cleared == 0)
    {
      _mm_pause_rep(reps++);
      reps &= 255;
      _mm_lfence();
    }
  
  my_ticket = DAF_U32(&b->ticket);
  if (my_ticket == 0)
    {
      b->cleared = 0;
    }

  _mm_mfence();
  PD("<<Cleared barrier %d (v: %d)", barrier_num, version);
#endif
}

/* ------------------------------------------------------------------------------- */
/* help funcitons */
/* ------------------------------------------------------------------------------- */

inline double wtime(void)
{
  struct timeval t;
  gettimeofday(&t,NULL);
  return (double)t.tv_sec + ((double)t.tv_usec)/1000000.0;
}

inline void 
wait_cycles(uint64_t cycles)
{
  if (cycles < 256)
    {
      cycles /= 6;
      while (cycles--)
	{
	  _mm_pause();
	}
    }
  else
    {
      ticks _start_ticks = getticks();
      ticks _end_ticks = _start_ticks + cycles - 130;
      while (getticks() < _end_ticks);
    }
}

inline void
_mm_pause_rep(uint32_t num_reps)
{
  while (num_reps--)
    {
      _mm_pause();
    }
}

/* inline uint32_t  */
/* get_num_hops(uint32_t core1, uint32_t core2) */
/* { */
/*   uint32_t hops = node_to_node_hops[core1 / 6][core2 / 6]; */
/*   //  PRINT("%2d is %d hop", core2, hops); */
/*   return hops; */
/* } */

/**also work for the threads but the man page of
 * sched_setaffinity recommends pthread_setaffinity_np*/
void
set_cpu(int cpu) 
{
  ssmp_my_core = cpu;
#ifdef __sparc__
  processor_bind(P_LWPID,P_MYID, cpu, NULL);
#elif defined(__tile__)
    if (tmc_cpus_set_my_cpu(tmc_cpus_find_nth_cpu(&cpus, cpu)) < 0)
    {
      tmc_task_die("Failure in 'tmc_cpus_set_my_cpu()'.");
    }

  if (cpu != tmc_cpus_get_my_cpu())
    {
      PRINT("******* i am not CPU %d", tmc_cpus_get_my_cpu());
    }

#else
  cpu_set_t mask;
  CPU_ZERO(&mask);
  CPU_SET(cpu, &mask);
  if (sched_setaffinity(0, sizeof(cpu_set_t), &mask) != 0) {
    printf("Problem with setting processor affinity: %s\n",
	   strerror(errno));
    exit(3);
  }
#endif /* SPARC */

#ifdef OPTERON
  uint32_t numa_node = cpu/6;
  numa_set_preferred(numa_node);  
#endif
}

void
set_thread(int cpu) {
  ssmp_my_core = cpu;
  cpu_set_t cpuset;
  pthread_t thread = pthread_self();

  CPU_ZERO(&cpuset);
  CPU_SET(cpu, &cpuset);

  if (pthread_setaffinity_np(thread, sizeof(cpu_set_t), &cpuset) != 0) {
	  printf("Problem with setting thread affinity\n");
	  exit(3);
  }
}


inline uint32_t
get_cpu()
{
  return ssmp_my_core;
}

#if defined(__i386__)
inline ticks getticks(void) {
  ticks ret;

  __asm__ __volatile__("rdtsc" : "=A" (ret));
  return ret;
}
#elif defined(__x86_64__)
inline ticks getticks(void)
{
  unsigned hi, lo;
  __asm__ __volatile__ ("rdtsc" : "=a"(lo), "=d"(hi));
  return ( (unsigned long long)lo)|( ((unsigned long long)hi)<<32 );
}
#elif defined(__sparc__)
inline ticks getticks()
{
  ticks ret;
  __asm__ __volatile__ ("rd %%tick, %0" : "=r" (ret) : "0" (ret));
  return ret;
}
#elif defined(__tile__)
#include <arch/cycle.h>
inline ticks getticks()
{
  return get_cycle_count();
}
#endif


ticks getticks_correction;

ticks 
getticks_correction_calc() 
{
#define GETTICKS_CALC_REPS 2000000
  ticks t_dur = 0;
  uint32_t i;
  for (i = 0; i < GETTICKS_CALC_REPS; i++) {
    ticks t_start = getticks();
    ticks t_end = getticks();
    t_dur += t_end - t_start;
  }
  getticks_correction = (ticks)(t_dur / (double) GETTICKS_CALC_REPS);
  /* printf("corr in float %f -- in ticks %llu \n", (t_dur / (double) GETTICKS_CALC_REPS), */
  /* 	 (long long unsigned) getticks_correction); */
  return getticks_correction;
}

/* Round up to next higher power of 2 (return x if it's already a power */
/* of 2) for 32-bit numbers */
uint32_t 
pow2roundup (uint32_t x)
{
  if (x==0) return 1;
  --x;
  x |= x >> 1;
  x |= x >> 2;
  x |= x >> 4;
  x |= x >> 8;
  x |= x >> 16;
  return x+1;
}

inline uint32_t
ssmp_cores_on_same_socket(uint32_t core1, uint32_t core2)
{
#if defined(OPTERON)
  return (id_to_core[core1] / 6) == (id_to_core[core2] / 6);
#elif defined(XEON)
  return (id_to_node[id_to_core[core1]] == id_to_node[id_to_core[core2]]);
#else
  return 1;
#endif
}
inline int ssmp_id() 
{
  return ssmp_id_;
}

inline int ssmp_num_ues() 
{
  return ssmp_num_ues_;
}

