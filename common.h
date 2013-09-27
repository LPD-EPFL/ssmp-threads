#ifndef _COMMON_H_
#define _COMMON_H_

#include <inttypes.h>

#define P(args...) printf("[%02d] ", ID); printf(args); printf("\n"); fflush(stdout)
#define PRINT P

#ifndef __ID
extern uint8_t ID;
#endif
#endif
