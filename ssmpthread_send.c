#include "ssmpthread.h"

extern volatile ssmp_msg_t **ssmp_recv_buf;
extern volatile ssmp_msg_t **ssmp_send_buf;
extern int ssmp_num_ues_;
extern __thread int ssmp_id_;
extern ssmp_barrier_t *ssmp_barrier;
#if defined(__tile__)
extern DynamicHeader *udn_header; //headers for messaging
#endif


/* ------------------------------------------------------------------------------- */
/* sending functions : default is blocking */
/* ------------------------------------------------------------------------------- */


inline 
void ssmpthread_send(uint32_t to, volatile ssmp_msg_t *msg)
{
#if defined(OPTERON)| defined(local) /* --------------------------------------- opteron */
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
  tmc_udn_send_buffer(udn_header[to], UDN0_DEMUX_TAG, (void*) msg, SSMP_CACHE_LINE_W);
#endif
}

inline 
void ssmpthread_send_no_sync(uint32_t to, volatile ssmp_msg_t *msg)
{
#if defined(OPTERON)|defined(local) /* --------------------------------------- opteron */
  volatile ssmp_msg_t *tmpm = ssmp_send_buf[to];
  msg->state = BUF_MESSG;
  memcpy((void*) tmpm, (const void*) msg, SSMP_CACHE_LINE_SIZE);

#elif defined(XEON) /* --------------------------------------- xeon */
  volatile ssmp_msg_t *tmpm = ssmp_send_buf[to];
  msg->state = BUF_MESSG;
  memcpy((void*) tmpm, (const void*) msg, SSMP_CACHE_LINE_SIZE);

#elif defined(NIAGARA) /* --------------------------------------- niagara */
  volatile ssmp_msg_t *tmpm = ssmp_send_buf[to];
  memcpy64((volatile uint64_t*) tmpm, (const uint64_t*) msg, SSMP_CACHE_LINE_DW);
  tmpm->state = BUF_MESSG;
#elif defined(TILERA) /* --------------------------------------- niagara */
  msg->sender = ssmp_id_;
  tmc_udn_send_buffer(udn_header[to], UDN0_DEMUX_TAG, (void*) msg, SSMP_CACHE_LINE_W);
#endif
}
