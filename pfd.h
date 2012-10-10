#ifndef _PFD_H_
#define _PFD_H_

#include <inttypes.h>
#include <float.h>
#include <assert.h>
#include "measurements.h"

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


#define PFD_NUM_STORES 2
#define PFD_PRINT_MAX 200

extern ticks* pfd_store[PFD_NUM_STORES];



#define PFDI()					\
  {						\
  ticks _pfd_s = getticks();

#define PFDO(store, entry)					\
  ticks _d = getticks() - _pfd_s - getticks_correction;		\
  pfd_store[store][entry] = _d;					\
  }
#define PFDP(store, num_vals)					\
  {								\
    uint32_t _i; ticks _sum = 0;				\
    uint32_t p = (num_vals < PFD_PRINT_MAX)			\
      ? num_vals : PFD_PRINT_MAX;				\
    for (_i = 0; _i < p; _i++)					\
      {								\
	printf("[%3d: %4lld] ", _i, (int64_t) pfd_store[store][_i]);	\
      }								\
    abs_deviation_t ad;						\
    get_abs_deviation(pfd_store[store], nm, &ad);		\
    print_abs_deviation(&ad);					\
  }


void pfd_store_init(uint32_t num_entries);
void get_abs_deviation(ticks* vals, size_t num_vals, abs_deviation_t* abs_dev);
void print_abs_deviation(abs_deviation_t* abs_dev);


#endif	/* _PFD_H_ */
