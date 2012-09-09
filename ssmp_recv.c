#include "ssmp.h"

extern volatile ssmp_msg_t **ssmp_recv_buf;
extern volatile ssmp_msg_t **ssmp_send_buf;
extern ssmp_chunk_t **ssmp_chunk_buf;
extern int ssmp_num_ues_;
extern int ssmp_id_;
extern int last_recv_from;
extern ssmp_barrier_t *ssmp_barrier;

/* ------------------------------------------------------------------------------- */
/* receiving functions : default is blocking */
/* ------------------------------------------------------------------------------- */

inline void ssmp_recv_from(int from, ssmp_msg_t *msg, int length) {
  volatile ssmp_msg_t *tmpm = ssmp_recv_buf[from];
#ifdef USE_ATOMIC  
  while (!__sync_bool_compare_and_swap(&tmpm->state, BUF_MESSG, BUF_LOCKD)) {
    wait_cycles(WAIT_TIME);
  }
#else
  while(tmpm->state == BUF_EMPTY);
#endif
  msg->w0 = tmpm->w0;				
  msg->w1 = tmpm->w1;				
  msg->w2 = tmpm->w2;				
  msg->w3 = tmpm->w3;				
  msg->w4 = tmpm->w4;				
  msg->w5 = tmpm->w5;				
  //  CPY_LLINTS(msg, tmpm, length);
  tmpm->state = BUF_EMPTY;

}

inline volatile ssmp_msg_t * ssmp_recv_fromp(int from) {
  volatile ssmp_msg_t *tmpm = ssmp_recv_buf[from];
  PD("recv from %d\n", from);
  while(!tmpm->state);

  return tmpm;
}

inline void ssmp_recv_rls(int from) {
  ssmp_recv_buf[from]->state = 0;
}

inline void ssmp_recv_from_sig(int from) {
  volatile ssmp_msg_t *tmpm = ssmp_recv_buf[from];
  while(!tmpm->state);
  tmpm->state = 0;

  PD("recved from %d\n", from);
}

inline void ssmp_recv_from_big(int from, void *data, int length) {
  int last_chunk = length % SSMP_CHUNK_SIZE;
  int num_chunks = length / SSMP_CHUNK_SIZE;

  while(num_chunks--) {

    while(!ssmp_chunk_buf[from]->state);

    memcpy(data, ssmp_chunk_buf[from], SSMP_CHUNK_SIZE);
    data = ((char *) data) + SSMP_CHUNK_SIZE;

    ssmp_chunk_buf[from]->state = 0;
  }

  if (!last_chunk) {
    return;
  }

  while(!ssmp_chunk_buf[from]->state);

  memcpy(data, ssmp_chunk_buf[from], last_chunk);

  ssmp_chunk_buf[from]->state = 0;

  PD("recved from %d\n", from);
}

inline int ssmp_recv_from_try(int from, ssmp_msg_t *msg, int length) {
  PD("recv from %d\n", from);
  if (ssmp_recv_buf[from]->state) {

    CPY_LLINTS(msg, ssmp_recv_buf[from], length);

    msg->sender = from;
    ssmp_recv_buf[from]->state = 0;
    return 1;
  }
  return 0;
}

inline uint32_t
ssmp_recv_from_test(uint32_t from) 
{
  volatile ssmp_msg_t *tmpm = ssmp_recv_buf[from];
  return (tmpm->state == BUF_MESSG);
}

inline void ssmp_recv(ssmp_msg_t *msg, int length) {
  while(1) {
    last_recv_from = (last_recv_from + 1) % ssmp_num_ues_;
    if (last_recv_from == ssmp_id_) {
      continue;
    } 

    if (ssmp_recv_from_try(last_recv_from, msg, length)) {
      return;
    }
  }
}

inline int ssmp_recv_try(ssmp_msg_t *msg, int length) {
  int i;
  for (i = 0; i < ssmp_num_ues_; i++) {
    last_recv_from = (last_recv_from + 1) % ssmp_num_ues_;
    if (last_recv_from == ssmp_id_) {
      continue;
    } 

    if (ssmp_recv_from_try(last_recv_from, msg, length)) {
      return 1;
    }
  }

  return 0;
}

inline void 
ssmp_recv_color(ssmp_color_buf_t *cbuf, ssmp_msg_t *msg, int length)
{
  unsigned int from;
  unsigned int num_ues = cbuf->num_ues;
  volatile unsigned int **cbuf_state = cbuf->buf_state;
  while(1)
    {
      //XXX: maybe have a last_recv_from field
      for (from = 0; from < num_ues; from++) 
	{

#ifdef USE_ATOMIC
	  if(__sync_bool_compare_and_swap(cbuf_state[from], BUF_MESSG, BUF_LOCKD))
#else
	    if (cbuf->buf[from]->state == BUF_MESSG)
#endif
	      {
		//CPY_LLINTS(msg, cbuf->buf[from], length);
		volatile ssmp_msg_t *tmpm = cbuf->buf[from];
		msg->w0 = tmpm->w0;				
		msg->w1 = tmpm->w1;				
		msg->w2 = tmpm->w2;				
		msg->w3 = tmpm->w3;				
		msg->w4 = tmpm->w4;				
		msg->w5 = tmpm->w5;				
		msg->w6 = tmpm->w6;
		msg->w7 = tmpm->w7;
		msg->sender = cbuf->from[from];

		tmpm->state = BUF_EMPTY;
		return;
	      }
	}
    }
}


inline uint32_t
ssmp_recv_color_start(ssmp_color_buf_t *cbuf, ssmp_msg_t *msg, uint32_t start_from)
{
  uint32_t from = start_from;
  uint32_t num_ues = cbuf->num_ues;
  //  printf("starting recv from %d (num_ues is %d)\n", from, num_ues);
  volatile unsigned int **cbuf_state = cbuf->buf_state;
  while(1) {
    for (; from < num_ues; from++)
      {

#ifdef USE_ATOMIC
	if(__sync_bool_compare_and_swap(cbuf_state[from], BUF_MESSG, BUF_LOCKD))
#else
	  if (cbuf->buf[from]->state == BUF_MESSG)
#endif
	    {
	      //CPY_LLINTS(msg, cbuf->buf[from], length);
	      volatile ssmp_msg_t *tmpm = cbuf->buf[from];
	      msg->w0 = tmpm->w0;				
	      msg->w1 = tmpm->w1;				
	      msg->w2 = tmpm->w2;				
	      msg->w3 = tmpm->w3;				
	      msg->w4 = tmpm->w4;				
	      msg->w5 = tmpm->w5;				
	      msg->w6 = tmpm->w6;
	      msg->w7 = tmpm->w7;
	      msg->sender = cbuf->from[from];

	      tmpm->state = BUF_EMPTY;
	      return from;
	    }
      }
    from = 0;
  }
}



inline void ssmp_recv_from4(int from, ssmp_msg_t *msg) {
  volatile ssmp_msg_t *m = ssmp_recv_buf[from];
  PD("recv from %d\n", from);
  while(!m->state);

  msg->sender = from;
  msg->w0 = m->w0;
  msg->w1 = m->w1;
  msg->w2 = m->w2;
  msg->w3 = m->w3;

  m->state = 0;
  PD("recved from %d\n", from);
}

inline void ssmp_recv_from6(int from, ssmp_msg_t *msg) {
  volatile ssmp_msg_t *m = ssmp_recv_buf[from];
  PD("recv from %d\n", from);
  while(!m->state);


  msg->w0 = m->w0;
  msg->w1 = m->w1;
  msg->w2 = m->w2;
  msg->w3 = m->w3;
  msg->w4 = m->w4;
  msg->w5 = m->w5;
  msg->sender = from;

  m->state = 0;
  PD("recved from %d\n", from);
}



inline int ssmp_recv_from_try1(int from, ssmp_msg_t *msg) {
  volatile ssmp_msg_t *m = ssmp_recv_buf[from];
  PD("recv from %d\n", from);
  if (m->state) {

    msg->sender = from;
    msg->w0 = m->w0;

    m->state = 0;
    PD("recved from %d\n", from);
    return 1;
  }
  return 0;
}

inline int ssmp_recv_from_try4(int from, ssmp_msg_t *msg) {
  volatile ssmp_msg_t *m = ssmp_recv_buf[from];
  PD("recv from %d\n", from);
  if (m->state) {

    msg->sender = from;
    msg->w0 = m->w0;
    msg->w1 = m->w1;
    msg->w2 = m->w2;
    msg->w3 = m->w3;

    m->state = 0;
    PD("recved from %d\n", from);
    return 1;
  }
  return 0;
}


inline int ssmp_recv_from_try6(int from, ssmp_msg_t *msg) {
  volatile ssmp_msg_t *m = ssmp_recv_buf[from];
  PD("recv from %d\n", from);
  if (m->state) {

    msg->sender = from;
    msg->w0 = m->w0;
    msg->w1 = m->w1;
    msg->w2 = m->w2;
    msg->w3 = m->w3;
    msg->w4 = m->w4;
    msg->w5 = m->w5;

    m->state = 0;
    PD("recved from %d\n", from);
    return 1;
  }
  return 0;
}



inline void ssmp_recv1(ssmp_msg_t *msg) {
  while(1) {
    last_recv_from = (last_recv_from + 1) % ssmp_num_ues_;
    if (last_recv_from == ssmp_id_) {
      continue;
    } 

    if (ssmp_recv_from_try1(last_recv_from, msg)) {
      return;
    }
  }
}


inline void ssmp_recv4(ssmp_msg_t *msg) {
  while(1) {
    last_recv_from = (last_recv_from + 1) % ssmp_num_ues_;
    if (last_recv_from == ssmp_id_) {
      continue;
    } 

    if (ssmp_recv_from_try4(last_recv_from, msg)) {
      return;
    }
  }
}


inline void ssmp_recv6(ssmp_msg_t *msg) {
  while(1) {
    last_recv_from = (last_recv_from + 1) % ssmp_num_ues_;
    if (last_recv_from == ssmp_id_) {
      continue;
    } 

    if (ssmp_recv_from_try6(last_recv_from, msg)) {
      return;
    }
  }
}


inline int ssmp_recv_try4(ssmp_msg_t *msg) {
  int i;
  for (i = 0; i < ssmp_num_ues_; i++) {
    last_recv_from = (last_recv_from + 1) % ssmp_num_ues_;
    if (last_recv_from == ssmp_id_) {
      continue;
    } 

    if (ssmp_recv_from_try4(last_recv_from, msg)) {
      return 1;
    }
  }

  return 0;
}

inline int ssmp_recv_try6(ssmp_msg_t *msg) {
  int i;
  for (i = 0; i < ssmp_num_ues_; i++) {
    last_recv_from = (last_recv_from + 1) % ssmp_num_ues_;
    if (last_recv_from == ssmp_id_) {
      continue;
    } 

    if (ssmp_recv_from_try6(last_recv_from, msg)) {
      return 1;
    }
  }

  return 0;
}

inline void ssmp_recv_color4(ssmp_color_buf_t *cbuf, ssmp_msg_t *msg) {
  int from;
  while(1) {
    //XXX: maybe have a last_recv_from field
    for (from = 0; from < cbuf->num_ues; from++) {

      if (cbuf->buf[from]->state) {
	msg->w0 = cbuf->buf[from]->w0;
	msg->w1 = cbuf->buf[from]->w1;
	msg->w2 = cbuf->buf[from]->w2;
	msg->w3 = cbuf->buf[from]->w3;

	msg->sender = cbuf->from[from];
	cbuf->buf[from]->state = 0;
	return;
      }
    }
  }
}

inline void ssmp_recv_color6(ssmp_color_buf_t *cbuf, ssmp_msg_t *msg) {
  int from;
  while(1) {
    //XXX: maybe have a last_recv_from field
    for (from = 0; from < cbuf->num_ues; from++) {

      if (cbuf->buf[from]->state) {
	msg->w0 = cbuf->buf[from]->w0;
	msg->w1 = cbuf->buf[from]->w1;
	msg->w2 = cbuf->buf[from]->w2;
	msg->w3 = cbuf->buf[from]->w3;
	msg->w4 = cbuf->buf[from]->w4;
	msg->w5 = cbuf->buf[from]->w5;

	msg->sender = cbuf->from[from];
	cbuf->buf[from]->state = 0;
	return;
      }
    }
  }
}

