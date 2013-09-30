#include "ssmpthread.h"
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

/* ------------------------------------------------------------------------------- */
/* library variables															   */
/* ------------------------------------------------------------------------------- */
int ssmp_num_ues_;
__thread int ssmp_id_;
ssmp_barrier_t *ssmp_barrier;
int *ues_initialized;
__thread uint32_t ssmp_my_core;

#if defined(__tile__)
DynamicHeader *udn_header; //headers for messaging
cpu_set_t cpus;
#else
ssmp_msg_t *ssmp_mem;//volatile is needed since ssmp_mem is set once before forking
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
	if (ssmpfd<0) {
		if (errno != EEXIST) {
			perror("In shm_open");
			exit(1);
		}

		//this time it is ok if it already exists
		ssmpfd = shm_open(keyF, O_CREAT | O_RDWR, S_IRWXU | S_IRWXG);
		if (ssmpfd<0) {
			perror("In shm_open");
			exit(1);
		}
	} else {
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
		ssmpthread_barrier_init(bar, 0xFFFFFFFFFFFFFFFF, NULL);
	}
	ssmpthread_barrier_init(1, 0xFFFFFFFFFFFFFFFF, color_app);

	_mm_mfence();//maybe not usefull since we spawn threads afterwards
}


void ssmpthread_mem_init(int id, int num_ues) {
	ssmp_id_ = id;
	ssmp_num_ues_ = num_ues;
	ssmp_recv_buf = (volatile ssmp_msg_t **) malloc(num_ues * sizeof(ssmp_msg_t *));
	ssmp_send_buf = (volatile ssmp_msg_t **) malloc(num_ues * sizeof(ssmp_msg_t *));
	if (ssmp_recv_buf == NULL || ssmp_send_buf == NULL) {
		perror("malloc@ ssmp_mem_init\n");
		exit(-1);
	}

	char keyF[100];
	unsigned int size = (num_ues  - 1) * sizeof(ssmp_msg_t);
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
	} else {
		if (ftruncate(ssmpfd, size) < 0) {
			perror("ftruncate failed\n");
			exit(1);
		}
	}

	ssmp_msg_t * tmp = (ssmp_msg_t *) mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, ssmpfd, 0);
	if (tmp == NULL) {
		perror("tmp = NULL\n");
		exit(134);
	}

	for (core = 0; core < num_ues; core++) {
		if (id == core) {
			continue;
		}
		//TODO
		ssmp_recv_buf[core] = tmp + ((core > id) ? (core - 1) : core);
		ssmp_recv_buf[core]->state = 0;
		fprintf(stderr, "thread %d %p\n", ssmp_id_, ssmp_recv_buf);
		fprintf(stderr, "thread %d %p\n", ssmp_id_, ssmp_recv_buf[0]);
		fprintf(stderr, "thread %d %p\n", ssmp_id_, ssmp_recv_buf[1]);
	}

	/********************************************************************************
    initialized own buffer
	 ********************************************************************************
	 */

	ssmpthread_barrier_wait(0);

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


/* ------------------------------------------------------------------------------- */
/* barrier functions */
/* ------------------------------------------------------------------------------- */

int color_app(int id) {
	return ((id % 2) ? 1 : 0);
}

inline void ssmpthread_barrier_init(int barrier_num, long long int participants, int (*color)(int)) {
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
	ssmp_barrier[barrier_num].participants = participants;
	ssmp_barrier[barrier_num].color = color;
	ssmp_barrier[barrier_num].ticket = 0;
	ssmp_barrier[barrier_num].cleared = 0;
#endif
}

void
ssmpthread_barrier_wait(int barrier_num) {
	if (barrier_num >= SSMP_NUM_BARRIERS) {
		return;
	}

#if defined(__tile__)
	tmc_sync_barrier_wait(ssmp_barrier + barrier_num);
#else
	_mm_sfence();
	_mm_lfence();
	ssmp_barrier_t *b = &ssmp_barrier[barrier_num];

	int (*col)(int);
	col = b->color;

	uint64_t bpar = (uint64_t) b->participants;
	uint32_t num_part = 0;

	/**each node computes the number of participants according to the mask bpar*/
	int from;
	for (from = 0; from < ssmp_num_ues_; from++) {
		/* if there is a color function it has priority */
		if (col != NULL) {
			num_part += col(from);
			if (from == ssmp_id_ && !col(from)) {
				return;
			}
		} else {
			uint32_t is_part = (uint32_t) (bpar & 0x0000000000000001);
			num_part += is_part;
			if (ssmp_id_ == from && !is_part) {
				return; //doesn't have to wait
			}
			bpar >>= 1;
		}
	}

	_mm_lfence();
	uint32_t reps = 1;
	/*what for ?*/
	while (b->cleared == 1) {
		_mm_pause_rep(reps++);
		reps &= 255;
		_mm_lfence();
	}

	/* IAF_U32(a) __sync_add_and_fetch(a,1)*/
	uint32_t my_ticket = IAF_U32(&b->ticket);
	/*when all nodes have been there*/
	if (my_ticket == num_part) {
		b->cleared = 1;
	}

	_mm_mfence();

	reps = 1;
	/*all nodes but the last are loop in there until the
	 * last node set cleared to 1 */
	while (b->cleared == 0) {
		_mm_pause_rep(reps++);
		reps &= 255;
		_mm_lfence();
	}

	/*DAF_U32(a) __sync_sub_and_fetch(a,1)*/
	my_ticket = DAF_U32(&b->ticket);
	/*if the node is the last, clear the flag*/
	if (my_ticket == 0) {
		b->cleared = 0;
	}

	_mm_mfence();
#endif
}


/* ------------------------------------------------------------------------------- */
/* help funcitons */
/* ------------------------------------------------------------------------------- */

inline double wtime(void) {
	struct timeval t;
	gettimeofday(&t,NULL);
	return (double)t.tv_sec + ((double)t.tv_usec)/1000000.0;
}

inline void wait_cycles(uint64_t cycles) {
	if (cycles < 256) {
		cycles /= 6;
		while (cycles--) {
			_mm_pause();
		}
	} else {
		ticks _start_ticks = getticks();
		ticks _end_ticks = _start_ticks + cycles - 130;
		while (getticks() < _end_ticks);
	}
}

inline void _mm_pause_rep(uint32_t num_reps) {
	while (num_reps--) {
		_mm_pause();
	}
}

#if defined(__i386__)
inline ticks getticks(void) {
	ticks ret;
	__asm__ __volatile__("rdtsc" : "=A" (ret));
	return ret;
}
#elif defined(__x86_64__)
inline ticks getticks(void) {
	unsigned hi, lo;
	__asm__ __volatile__ ("rdtsc" : "=a"(lo), "=d"(hi));
	return ( (unsigned long long)lo)|( ((unsigned long long)hi)<<32 );
}
#elif defined(__sparc__)
inline ticks getticks() {
	ticks ret;
	__asm__ __volatile__ ("rd %%tick, %0" : "=r" (ret) : "0" (ret));
	return ret;
}
#elif defined(__tile__)
#include <arch/cycle.h>
inline ticks getticks() {
	return get_cycle_count();
}
#endif

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

inline int ssmpthread_num_ues() {
	return ssmp_num_ues_;
}
