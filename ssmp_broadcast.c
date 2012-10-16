#include "ssmp.h"

extern ssmp_msg_t *ssmp_mem;
extern volatile ssmp_msg_t **ssmp_recv_buf;
extern volatile ssmp_msg_t **ssmp_send_buf;
extern int ssmp_num_ues_;
extern int ssmp_id_;
extern int last_recv_from;
extern ssmp_barrier_t *ssmp_barrier;


/* ------------------------------------------------------------------------------- */
/* broadcasting functions */
/* ------------------------------------------------------------------------------- */

inline void ssmp_broadcast(ssmp_msg_t *msg, int length) {
  int core;
  for (core = 0; core < ssmp_num_ues_; core++) {
    if (core == ssmp_id_) {
      continue;
    }
    
    ssmp_send(core, msg, length);
  }
}

