#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <stdio.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <inttypes.h>
#include <unistd.h>
#include <time.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <sched.h>
#include <assert.h>

#include "common.h"
#include "ssmp.h"

int chunks_mb = 200 * 1024 * 1024;
int core = 0;
int ID;

int main(int argc, char **argv) {
  if (argc > 1) {
    chunks_mb = atoi(argv[1]);
  }
  if (argc > 2) {
    core = atoi(argv[2]);
  }

  ID = 0;
  printf("chunk in MB: %d\n", chunks_mb);
  printf("on core: %d\n", core);
  ssmp_init(1);

  set_cpu(core);

  ssmp_mem_init(ID, 1);
  P("Initialized child %u", ID);

  ssmp_barrier_wait(0);
  P("CLEARED barrier %d", 0);

  long long int sum=0;
  int c = 1; int alloc = chunks_mb/sizeof(long long int);
  while (1) {
    long long int *buf = (long long int *) malloc(alloc);
    if (buf == NULL) {
      P("*** not able to alloc");
    }
    else {
      P("%d MB", c++ * chunks_mb);
    }

    int i;
    for (i = 0; i < alloc; i+=1) {
      buf[i] = 1;
    }

    for (i = 0; i < alloc * (c-1); i+=1) {
      sum+=buf[i];
    }

  }

  ssmp_term();
  return 0;
}
