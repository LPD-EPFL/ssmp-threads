/*
 *  File: atomic_ops.h
 *  Author: Tudor David
 *
 *  Created on: December 06, 2012
 *
 *  Description: 
 *      Cross-platform atomic operations
 */
#ifndef _ATOMIC_OPS_H_INCLUDED_
#define _ATOMIC_OPS_H_INCLUDED_

#include <inttypes.h>

#ifdef __sparc__
/*
 *  sparc code
 */

#  include <atomic.h>

//test-and-set uint8_t
static inline uint8_t tas_uint8(volatile uint8_t *addr) {
uint8_t oldval;
  __asm__ __volatile__("ldstub %1,%0"
        : "=r"(oldval), "=m"(*addr)
        : "m"(*addr) : "memory");
    return oldval;
}

#  define CAS_PTR(a,b,c) atomic_cas_ptr(a,b,c)
#  define CAS_U8(a,b,c) atomic_cas_8(a,b,c)
#  define CAS_U16(a,b,c) atomic_cas_16(a,b,c)
#  define CAS_U32(a,b,c) atomic_cas_32(a,b,c)
#  define CAS_U64(a,b,c) atomic_cas_64(a,b,c)
#  define SWAP_PTR(a,b) atomic_swap_ptr(a,b)
#  define SWAP_U8(a,b) atomic_swap_8(a,b)
#  define SWAP_U16(a,b) atomic_swap_16(a,b)
#  define SWAP_U32(a,b) atomic_swap_32(a,b)
#  define SWAP_U64(a,b) atomic_swap_64(a,b)
#  define FAI_U8(a) atomic_inc_8_nv(a)-1
#  define FAI_U16(a) atomic_inc_16_nv(a)-1
#  define FAI_U32(a) atomic_inc_32_nv(a)-1
#  define FAI_U64(a) atomic_inc_64_nv(a)-1
#  define FAD_U8(a) atomic_dec_8_nv(a,)+1
#  define FAD_U16(a) atomic_dec_16_nv(a)+1
#  define FAD_U32(a) atomic_dec_32_nv(a)+1
#  define FAD_U64(a) atomic_dec_64_nv(a)+1
#  define IAF_U8(a) atomic_inc_8_nv(a)
#  define IAF_U16(a) atomic_inc_16_nv(a)
#  define IAF_U32(a) atomic_inc_32_nv(a)
#  define IAF_U64(a) atomic_inc_64_nv(a)
#  define DAF_U8(a) atomic_dec_8_nv(a)
#  define DAF_U16(a) atomic_dec_16_nv(a)
#  define DAF_U32(a) atomic_dec_32_nv(a)
#  define DAF_U64(a) atomic_dec_64_nv(a)
#  define TAS_U8(a) tas_uint8(a)
#  define MEM_BARRIER 
/* #  define PAUSE asm volatile ("rd %%ccr %%g0") */
#  define PAUSE    asm volatile("rd    %%ccr, %%g0\n\t" \
                    ::: "memory")
 
//#define PAUSE

//end of sparc code
#else
/*
 *  x86 code
 */

#  include <xmmintrin.h>

//Swap pointers
static inline void* swap_pointer(volatile void* ptr, void *x) {
#  ifdef __i386__
   __asm__ __volatile__("xchgl %0,%1"
        :"=r" ((unsigned) x)
        :"m" (*(volatile unsigned *)ptr), "0" (x)
        :"memory");

  return x;
#  elif defined(__x86_64__)
  __asm__ __volatile__("xchgq %0,%1"
        :"=r" ((unsigned long long) x)
        :"m" (*(volatile long long *)ptr), "0" ((unsigned long long) x)
        :"memory");

  return x;
#  endif
}

//Swap uint64_t
static inline uint64_t swap_uint64(volatile uint64_t* target,  uint64_t x) {
  __asm__ __volatile__("xchgq %0,%1"
        :"=r" ((uint64_t) x)
        :"m" (*(volatile uint64_t *)target), "0" ((uint64_t) x)
        :"memory");

  return x;
}

//Swap uint32_t
static inline uint32_t swap_uint32(volatile uint32_t* target,  uint32_t x) {
  __asm__ __volatile__("xchgl %0,%1"
        :"=r" ((uint32_t) x)
        :"m" (*(volatile uint32_t *)target), "0" ((uint32_t) x)
        :"memory");

  return x;
}

//Swap uint16_t
static inline uint16_t swap_uint16(volatile uint16_t* target,  uint16_t x) {
  __asm__ __volatile__("xchgw %0,%1"
        :"=r" ((uint16_t) x)
        :"m" (*(volatile uint16_t *)target), "0" ((uint16_t) x)
        :"memory");

  return x;
}

//Swap uint8_t
static inline uint8_t swap_uint8(volatile uint8_t* target,  uint8_t x) {
  __asm__ __volatile__("xchgb %0,%1"
        :"=r" ((uint8_t) x)
        :"m" (*(volatile uint8_t *)target), "0" ((uint8_t) x)
        :"memory");

  return x;
}

//test-and-set uint8_t
static inline uint8_t tas_uint8(volatile uint8_t *addr) {
uint8_t oldval;
  __asm__ __volatile__("xchgb %0,%1"
        : "=q"(oldval), "=m"(*addr)
        : "0"((unsigned char) 0xff), "m"(*addr) : "memory");
    return (uint8_t) oldval;
}

//atomic operations interface
#  define CAS_PTR(a,b,c) __sync_val_compare_and_swap(a,b,c)
#  define CAS_U8(a,b,c) __sync_val_compare_and_swap(a,b,c)
#  define CAS_U16(a,b,c) __sync_val_compare_and_swap(a,b,c)
#  define CAS_U32(a,b,c) __sync_val_compare_and_swap(a,b,c)
#  define CAS_U64(a,b,c) __sync_val_compare_and_swap(a,b,c)
#  define SWAP_PTR(a,b) swap_pointer(a,b)
#  define SWAP_U8(a,b) swap_uint8(a,b)
#  define SWAP_U16(a,b) swap_uint16(a,b)
#  define SWAP_U32(a,b) swap_uint32(a,b)
#  define SWAP_U64(a,b) swap_uint64(a,b)
#  define FAI_U8(a) __sync_fetch_and_add(a,1)
#  define FAI_U16(a) __sync_fetch_and_add(a,1)
#  define FAI_U32(a) __sync_fetch_and_add(a,1)
#  define FAI_U64(a) __sync_fetch_and_add(a,1)
#  define FAD_U8(a) __sync_fetch_and_sub(a,1)
#  define FAD_U16(a) __sync_fetch_and_sub(a,1)
#  define FAD_U32(a) __sync_fetch_and_sub(a,1)
#  define FAD_U64(a) __sync_fetch_and_sub(a,1)
#  define IAF_U8(a) __sync_add_and_fetch(a,1)
#  define IAF_U16(a) __sync_add_and_fetch(a,1)
#  define IAF_U32(a) __sync_add_and_fetch(a,1)
#  define IAF_U64(a) __sync_add_and_fetch(a,1)
#  define DAF_U8(a) __sync_sub_and_fetch(a,1)
#  define DAF_U16(a) __sync_sub_and_fetch(a,1)
#  define DAF_U32(a) __sync_sub_and_fetch(a,1)
#  define DAF_U64(a) __sync_sub_and_fetch(a,1)
#  define TAS_U8(a) tas_uint8(a)
#  define MEM_BARRIER __sync_synchronize()
#  define PAUSE _mm_pause()

/*End of x86 code*/
#endif

  static inline void
  pause_rep(uint32_t num_reps)
  {
    uint32_t i;
    for (i = 0; i < num_reps; i++)
      {
	PAUSE;
	/* PAUSE; */
	/* asm volatile ("NOP"); */
      }
  }

#endif



