#include "ssmp.h"

extern volatile ssmp_msg_t **ssmp_recv_buf;
extern volatile ssmp_msg_t **ssmp_send_buf;
extern ssmp_chunk_t **ssmp_chunk_buf;
extern int ssmp_num_ues_;
extern int ssmp_id_;
extern int last_recv_from;
extern ssmp_barrier_t *ssmp_barrier;
#if defined(__tile__)
extern DynamicHeader *udn_header; //headers for messaging
#endif


/* ------------------------------------------------------------------------------- */
/* sending functions : default is blocking */
/* ------------------------------------------------------------------------------- */


inline 
void ssmp_send(uint32_t to, volatile ssmp_msg_t *msg) 
{
#if defined(OPTERON) /* --------------------------------------- opteron */
  volatile ssmp_msg_t *tmpm = ssmp_send_buf[to];
#  ifdef USE_ATOMIC
  while (!__sync_bool_compare_and_swap(&tmpm->state, BUF_EMPTY, BUF_LOCKD)) 
    {
      wait_cycles(WAIT_TIME);
    }
#  else 
  PREFETCHW(tmpm);
  while (tmpm->state != BUF_EMPTY)
    {
      _mm_pause();
      _mm_mfence();
      PREFETCHW(tmpm);
    }
#  endif

  msg->state = BUF_MESSG;
  memcpy((void*) tmpm, (const void*) msg, SSMP_CACHE_LINE_SIZE);

#elif defined(XEON) /* --------------------------------------- xeon */
  volatile ssmp_msg_t *tmpm = ssmp_send_buf[to];
  if (!ssmp_cores_on_same_socket(ssmp_id_, to))
    {
      while (!__sync_bool_compare_and_swap(&tmpm->state, BUF_EMPTY, BUF_LOCKD)) 
	{
	  wait_cycles(WAIT_TIME);
	}
    }
  else
    {
      while (tmpm->state != BUF_EMPTY)
	{
	  _mm_pause();
	  _mm_mfence();
	}
    }
  msg->state = BUF_MESSG;
  memcpy((void*) tmpm, (const void*) msg, SSMP_CACHE_LINE_SIZE);
  _mm_mfence();

#elif defined(NIAGARA) /* --------------------------------------- niagara */
  volatile ssmp_msg_t *tmpm = ssmp_send_buf[to];
  while (tmpm->state != BUF_EMPTY)
    {
      ;
    }

  memcpy64((volatile uint64_t*) tmpm, (const uint64_t*) msg, SSMP_CACHE_LINE_DW);
  tmpm->state = BUF_MESSG;
#elif defined(TILERA) /* --------------------------------------- niagara */
  msg->sender = ssmp_id_;
  tmc_udn_send_buffer(udn_header[to], UDN0_DEMUX_TAG, (void*) msg, SSMP_MSG_NUM_WORDS);
#endif
}


#if !defined(TILERA)
  inline 
    void ssmp_send_big(int to, void *data, size_t length) 
  {
    int last_chunk = length % SSMP_CHUNK_SIZE;
    int num_chunks = length / SSMP_CHUNK_SIZE;

    while(num_chunks--) {

      while(ssmp_chunk_buf[ssmp_id_]->state);

      memcpy(ssmp_chunk_buf[ssmp_id_], data, SSMP_CHUNK_SIZE);
      data = ((char *) data) + SSMP_CHUNK_SIZE;

      ssmp_chunk_buf[ssmp_id_]->state = 1;
    }

    if (!last_chunk) {
      return;
    }

    while(ssmp_chunk_buf[ssmp_id_]->state);

    memcpy(ssmp_chunk_buf[ssmp_id_], data, last_chunk);

    ssmp_chunk_buf[ssmp_id_]->state = 1;

    PD("sent to %d", to);
  }
#endif
