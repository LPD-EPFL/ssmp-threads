MEASUREMENTS=1
PERF_COUNTERS=0

UNAME := $(shell uname -n)

ifeq ($(UNAME), lpd48core)
PLATFORM=OPTERON
CC=gcc
PLATFORM_NUMA=1
endif

ifeq ($(UNAME), diassrv8)
PLATFORM=XEON
CC=gcc
PLATFORM_NUMA=1
endif

ifeq ($(UNAME), maglite)
PLATFORM=NIAGARA
CC=/opt/csw/bin/gcc -m64 -mcpu=v9 -mtune=v9
PLATFORM_NUMA=0
endif

ifeq ($(UNAME), parsasrv1.epfl.ch)
PLATFORM=TILERA
CC=tile-gcc
PERF_CLFAGS+=-ltmc
LINK=-ltmc
endif

ifeq ($(P),0) #give P=0 to compile with debug info
DEBUG_CFLAGS=-ggdb -Wall -g -fno-inline
PERF_CLFAGS+= 
ifeq ($(UNAME), SunOS)
CC=/opt/csw/bin/gcc -m32 -mcpu=v9 -mtune=v9 
endif
else
DEBUG_CFLAGS=-Wall
PERF_CLFAGS+=-O3 #-O0 -g
endif

ifeq ($(PLATFORM_NUMA),1) #give PLATFORM_NUMA=1 for NUMA
PERF_CLFAGS += -lnuma
endif 

ifeq ($(PERF_COUNTERS),1) #give PERF_COUNTERS=1 for compiling with the papi library included
PERF_CLFAGS += -lpapi
endif 

VER_FLAGS+=-D$(PLATFORM)

default: one2one main

main:	libssmp.a main.o common.h
	$(CC) $(VER_FLAGS) -o main main.o libssmp.a -lrt $(DEBUG_CFLAGS) $(PERF_CLFAGS)

main.o:	main.c
	$(CC) $(VER_FLAGS) -D_GNU_SOURCE -c main.c $(DEBUG_CFLAGS) $(PERF_CLFAGS)

bank:	libssmp.a bank.o common.h
	$(CC) $(VER_FLAGS) -o bank bank.o libssmp.a -lrt $(DEBUG_CFLAGS) $(PERF_CLFAGS)

bank.o:	bank.c
	$(CC) $(VER_FLAGS) -D_GNU_SOURCE -c bank.c $(DEBUG_CFLAGS) $(PERF_CLFAGS)

ssmp.o: ssmp.c
	$(CC) $(VER_FLAGS) -D_GNU_SOURCE -c ssmp.c $(DEBUG_CFLAGS) $(PERF_CLFAGS)

ssmp_send.o: ssmp_send.c
	$(CC) $(VER_FLAGS) -D_GNU_SOURCE -c ssmp_send.c $(DEBUG_CFLAGS) $(PERF_CLFAGS)

ssmp_recv.o: ssmp_send.c
	$(CC) $(VER_FLAGS) -D_GNU_SOURCE -c ssmp_recv.c $(DEBUG_CFLAGS) $(PERF_CLFAGS)

ssmp_broadcast.o: ssmp_broadcast.c
	$(CC) $(VER_FLAGS) -D_GNU_SOURCE -c ssmp_broadcast.c $(DEBUG_CFLAGS) $(PERF_CLFAGS)

ifeq ($(MEASUREMENTS),1)
PERF_CLFAGS += -DDO_TIMINGS
MEASUREMENTS_FILES += measurements.o pfd.o
endif

measurements.o: measurements.c
	$(CC) $(VER_FLAGS) -D_GNU_SOURCE -c measurements.c $(DEBUG_CFLAGS) $(PERF_CLFAGS)

pfd.o: pfd.c
	$(CC) $(VER_FLAGS) -D_GNU_SOURCE -c pfd.c $(DEBUG_CFLAGS) $(PERF_CLFAGS)	


libssmp.a: ssmp.o ssmp_send.o ssmp_recv.o ssmp_broadcast.o ssmp.h $(MEASUREMENTS_FILES)
	@echo Archive name = libssmp.a
	ar -r libssmp.a ssmp.o ssmp_send.o ssmp_recv.o ssmp_broadcast.o $(MEASUREMENTS_FILES)
	rm -f *.o	

one2one: libssmp.a one2one.o common.h measurements.o pfd.o
	$(CC) $(VER_FLAGS) -o one2one one2one.o libssmp.a -lrt $(DEBUG_CFLAGS) $(PERF_CLFAGS)

one2one.o:	one2one.c ssmp.c
		$(CC) $(VER_FLAGS) -D_GNU_SOURCE -c one2one.c $(DEBUG_CFLAGS) $(PERF_CLFAGS)

cas_stresh: libssmp.a cas_stresh.o common.h
	$(CC) $(VER_FLAGS) -o cas_stresh cas_stresh.o libssmp.a -lrt $(DEBUG_CFLAGS) $(PERF_CLFAGS)

cas_stresh.o:	cas_stresh.c ssmp.c
		$(CC) $(VER_FLAGS) -D_GNU_SOURCE -c cas_stresh.c $(DEBUG_CFLAGS) $(PERF_CLFAGS)

one2one_manual: libssmp.a one2one_manual.o common.h
	$(CC) $(VER_FLAGS) -o one2one_manual one2one_manual.o libssmp.a libpapi.a -lrt $(DEBUG_CFLAGS) $(PERF_CLFAGS) 

one2one_manual.o:	one2one_manual.c ssmp.c
		$(CC) $(VER_FLAGS) -D_GNU_SOURCE -c one2one_manual.c $(DEBUG_CFLAGS) $(PERF_CLFAGS) 

moesi: libssmp.a moesi.o common.h
	$(CC) $(VER_FLAGS) -o moesi moesi.o libssmp.a -lrt $(DEBUG_CFLAGS) $(PERF_CLFAGS) 

moesi.o:	moesi.c ssmp.c
		$(CC) $(VER_FLAGS) -D_GNU_SOURCE -c moesi.c $(DEBUG_CFLAGS) $(PERF_CLFAGS) 

l1_spil: libssmp.a l1_spil.o common.h
	$(CC) $(VER_FLAGS) -o l1_spil l1_spil.o libssmp.a -lrt $(DEBUG_CFLAGS) $(PERF_CLFAGS) 

l1_spil.o:	l1_spil.c ssmp.c
		$(CC) $(VER_FLAGS) -D_GNU_SOURCE -c l1_spil.c $(DEBUG_CFLAGS) $(PERF_CLFAGS) 

ticket_lock: libssmp.a ticket_lock.o common.h
	$(CC) $(VER_FLAGS) -o ticket_lock ticket_lock.o libssmp.a -lrt $(DEBUG_CFLAGS) $(PERF_CLFAGS)

ticket_lock.o:	ticket_lock.c ssmp.c
		$(CC) $(VER_FLAGS) -D_GNU_SOURCE -c ticket_lock.c $(DEBUG_CFLAGS) $(PERF_CLFAGS)


one2onep: libssmp.a one2onep.o common.h
	$(CC) $(VER_FLAGS) -o one2onep one2onep.o libssmp.a -lrt $(DEBUG_CFLAGS) $(PERF_CLFAGS) 

one2onep.o:	one2onep.c ssmp.c
		$(CC) $(VER_FLAGS) -D_GNU_SOURCE -c one2onep.c $(DEBUG_CFLAGS) $(PERF_CLFAGS)

wcycles: libssmp.a wcycles.o common.h
	$(CC) $(VER_FLAGS) -o wcycles wcycles.o libssmp.a -lrt $(DEBUG_CFLAGS) $(PERF_CLFAGS) 

wcycles.o:	wcycles.c ssmp.c
		$(CC) $(VER_FLAGS) -D_GNU_SOURCE -c wcycles.c $(DEBUG_CFLAGS) $(PERF_CLFAGS)


memstresh: libssmp.a memstresh.o common.h
	$(CC) $(VER_FLAGS) -o memstresh memstresh.o libssmp.a -lrt $(DEBUG_CFLAGS) $(PERF_CLFAGS)	

memstresh.o:	memstresh.c ssmp.c
		$(CC) $(VER_FLAGS) -D_GNU_SOURCE -c memstresh.c $(DEBUG_CFLAGS) $(PERF_CLFAGS)

memstreshmp: libssmp.a memstreshmp.o common.h
	$(CC) $(VER_FLAGS) -o memstreshmp memstreshmp.o libssmp.a -lrt $(DEBUG_CFLAGS) $(PERF_CLFAGS)	

memstreshmp.o:	memstreshmp.c ssmp.c
		$(CC) $(VER_FLAGS) -D_GNU_SOURCE -c memstreshmp.c $(DEBUG_CFLAGS) $(PERF_CLFAGS)

one2onebig: libssmp.a one2onebig.o common.h
	$(CC) $(VER_FLAGS) -o one2onebig one2onebig.o libssmp.a -lrt $(DEBUG_CFLAGS) $(PERF_CLFAGS)	

one2onebig.o:	one2onebig.c ssmp.c
		$(CC) $(VER_FLAGS) -D_GNU_SOURCE -c one2onebig.c $(DEBUG_CFLAGS) $(PERF_CLFAGS)

mainbig:	libssmp.a mainbig.o common.h
	$(CC) $(VER_FLAGS) -o mainbig mainbig.o libssmp.a -lrt $(DEBUG_CFLAGS) $(PERF_CLFAGS) 

mainbig.o:	mainbig.c
	$(CC) $(VER_FLAGS) -D_GNU_SOURCE -c mainbig.c $(DEBUG_CFLAGS) $(PERF_CLFAGS)


barrier: libssmp.a barrier.o common.h
	$(CC) $(VER_FLAGS) -o barrier barrier.o libssmp.a -lrt $(DEBUG_CFLAGS) $(PERF_CLFAGS)	

barrier.o:	barrier.c ssmp.c
		$(CC) $(VER_FLAGS) -D_GNU_SOURCE -c barrier.c $(DEBUG_CFLAGS) $(PERF_CLFAGS)

color_buf: libssmp.a color_buf.o common.h
	$(CC) $(VER_FLAGS) -o color_buf color_buf.o libssmp.a -lrt $(DEBUG_CFLAGS) $(PERF_CLFAGS)	

color_buf.o:	color_buf.c ssmp.c
		$(CC) $(VER_FLAGS) -D_GNU_SOURCE -c color_buf.c $(DEBUG_CFLAGS) $(PERF_CLFAGS)

clean:
	rm -f *.o *.a
