PLATFORM_NUMA=1
MEASUREMENTS=1

ifeq ($(P),0) #give P=0 to compile with debug info
DEBUG_CFLAGS=-ggdb -Wall -g  -fno-inline #-pg
PERF_CLFAGS= 
else
DEBUG_CFLAGS=-Wall
PERF_CLFAGS= -O3 #-O0 -g
endif

ifeq ($(PLATFORM_NUMA),1) #give PLATFORM_NUMA=1 for NUMA
PERF_CLFAGS += -lnuma
VER_FLAGS = -DPLATFORM_NUMA
endif 


default: main

main:	libssmp.a main.o common.h
	gcc $(VER_FLAGS) -o main main.o libssmp.a -lrt $(DEBUG_CFLAGS) $(PERF_CLFAGS)

main.o:	main.c
	gcc $(VER_FLAGS) -D_GNU_SOURCE -c main.c $(DEBUG_CFLAGS) $(PERF_CLFAGS)

ssmp.o: ssmp.c
	gcc $(VER_FLAGS) -D_GNU_SOURCE -c ssmp.c $(DEBUG_CFLAGS) $(PERF_CLFAGS)

ssmp_send.o: ssmp_send.c
	gcc $(VER_FLAGS) -D_GNU_SOURCE -c ssmp_send.c $(DEBUG_CFLAGS) $(PERF_CLFAGS)

ssmp_recv.o: ssmp_send.c
	gcc $(VER_FLAGS) -D_GNU_SOURCE -c ssmp_recv.c $(DEBUG_CFLAGS) $(PERF_CLFAGS)

ssmp_broadcast.o: ssmp_broadcast.c
	gcc $(VER_FLAGS) -D_GNU_SOURCE -c ssmp_broadcast.c $(DEBUG_CFLAGS) $(PERF_CLFAGS)

ifeq ($(MEASUREMENTS),1)
MEASUREMENTS_FILES += measurements.o pfd.o
endif

measurements.o: measurements.c
	gcc $(VER_FLAGS) -D_GNU_SOURCE -c measurements.c $(DEBUG_CFLAGS) $(PERF_CLFAGS)	

pfd.o: pfd.c
	gcc $(VER_FLAGS) -D_GNU_SOURCE -c pfd.c $(DEBUG_CFLAGS) $(PERF_CLFAGS)	


libssmp.a: ssmp.o ssmp_send.o ssmp_recv.o ssmp_broadcast.o ssmp.h $(MEASUREMENTS_FILES)
	@echo Archive name = libssmp.a
	ar -r libssmp.a ssmp.o ssmp_send.o ssmp_recv.o ssmp_broadcast.o $(MEASUREMENTS_FILES)
	rm -f *.o	

one2one: libssmp.a one2one.o common.h
	gcc $(VER_FLAGS) -o one2one one2one.o libssmp.a -lrt $(DEBUG_CFLAGS) $(PERF_CLFAGS)

one2one.o:	one2one.c ssmp.c
		gcc $(VER_FLAGS) -D_GNU_SOURCE -c one2one.c $(DEBUG_CFLAGS) $(PERF_CLFAGS)

cas_stresh: libssmp.a cas_stresh.o common.h
	gcc $(VER_FLAGS) -o cas_stresh cas_stresh.o libssmp.a -lrt $(DEBUG_CFLAGS) $(PERF_CLFAGS)

cas_stresh.o:	cas_stresh.c ssmp.c
		gcc $(VER_FLAGS) -D_GNU_SOURCE -c cas_stresh.c $(DEBUG_CFLAGS) $(PERF_CLFAGS)

one2one_manual: libssmp.a one2one_manual.o common.h
	gcc $(VER_FLAGS) -o one2one_manual one2one_manual.o libssmp.a -lrt $(DEBUG_CFLAGS) $(PERF_CLFAGS) 

one2one_manual.o:	one2one_manual.c ssmp.c
		gcc $(VER_FLAGS) -D_GNU_SOURCE -c one2one_manual.c $(DEBUG_CFLAGS) $(PERF_CLFAGS) 

moesi: libssmp.a moesi.o common.h
	gcc $(VER_FLAGS) -o moesi moesi.o libssmp.a -lrt $(DEBUG_CFLAGS) $(PERF_CLFAGS) 

moesi.o:	moesi.c ssmp.c
		gcc $(VER_FLAGS) -D_GNU_SOURCE -c moesi.c $(DEBUG_CFLAGS) $(PERF_CLFAGS) 

l1_spil: libssmp.a l1_spil.o common.h
	gcc $(VER_FLAGS) -o l1_spil l1_spil.o libssmp.a -lrt $(DEBUG_CFLAGS) $(PERF_CLFAGS) 

l1_spil.o:	l1_spil.c ssmp.c
		gcc $(VER_FLAGS) -D_GNU_SOURCE -c l1_spil.c $(DEBUG_CFLAGS) $(PERF_CLFAGS) 

ticket_lock: libssmp.a ticket_lock.o common.h
	gcc $(VER_FLAGS) -o ticket_lock ticket_lock.o libssmp.a -lrt $(DEBUG_CFLAGS) $(PERF_CLFAGS)

ticket_lock.o:	ticket_lock.c ssmp.c
		gcc $(VER_FLAGS) -D_GNU_SOURCE -c ticket_lock.c $(DEBUG_CFLAGS) $(PERF_CLFAGS)


one2onep: libssmp.a one2onep.o common.h
	gcc $(VER_FLAGS) -o one2onep one2onep.o libssmp.a -lrt $(DEBUG_CFLAGS) $(PERF_CLFAGS) 

one2onep.o:	one2onep.c ssmp.c
		gcc $(VER_FLAGS) -D_GNU_SOURCE -c one2onep.c $(DEBUG_CFLAGS) $(PERF_CLFAGS)

wcycles: libssmp.a wcycles.o common.h
	gcc $(VER_FLAGS) -o wcycles wcycles.o libssmp.a -lrt $(DEBUG_CFLAGS) $(PERF_CLFAGS) 

wcycles.o:	wcycles.c ssmp.c
		gcc $(VER_FLAGS) -D_GNU_SOURCE -c wcycles.c $(DEBUG_CFLAGS) $(PERF_CLFAGS)


memstresh: libssmp.a memstresh.o common.h
	gcc $(VER_FLAGS) -o memstresh memstresh.o libssmp.a -lrt $(DEBUG_CFLAGS) $(PERF_CLFAGS)	

memstresh.o:	memstresh.c ssmp.c
		gcc $(VER_FLAGS) -D_GNU_SOURCE -c memstresh.c $(DEBUG_CFLAGS) $(PERF_CLFAGS)

memstreshmp: libssmp.a memstreshmp.o common.h
	gcc $(VER_FLAGS) -o memstreshmp memstreshmp.o libssmp.a -lrt $(DEBUG_CFLAGS) $(PERF_CLFAGS)	

memstreshmp.o:	memstreshmp.c ssmp.c
		gcc $(VER_FLAGS) -D_GNU_SOURCE -c memstreshmp.c $(DEBUG_CFLAGS) $(PERF_CLFAGS)

one2onebig: libssmp.a one2onebig.o common.h
	gcc $(VER_FLAGS) -o one2onebig one2onebig.o libssmp.a -lrt $(DEBUG_CFLAGS) $(PERF_CLFAGS)	

one2onebig.o:	one2onebig.c ssmp.c
		gcc $(VER_FLAGS) -D_GNU_SOURCE -c one2onebig.c $(DEBUG_CFLAGS) $(PERF_CLFAGS)

mainbig:	libssmp.a mainbig.o common.h
	gcc $(VER_FLAGS) -o mainbig mainbig.o libssmp.a -lrt $(DEBUG_CFLAGS) $(PERF_CLFAGS) 

mainbig.o:	mainbig.c
	gcc $(VER_FLAGS) -D_GNU_SOURCE -c mainbig.c $(DEBUG_CFLAGS) $(PERF_CLFAGS)


barrier: libssmp.a barrier.o common.h
	gcc $(VER_FLAGS) -o barrier barrier.o libssmp.a -lrt $(DEBUG_CFLAGS) $(PERF_CLFAGS)	

barrier.o:	barrier.c ssmp.c
		gcc $(VER_FLAGS) -D_GNU_SOURCE -c barrier.c $(DEBUG_CFLAGS) $(PERF_CLFAGS)

color_buf: libssmp.a color_buf.o common.h
	gcc $(VER_FLAGS) -o color_buf color_buf.o libssmp.a -lrt $(DEBUG_CFLAGS) $(PERF_CLFAGS)	

color_buf.o:	color_buf.c ssmp.c
		gcc $(VER_FLAGS) -D_GNU_SOURCE -c color_buf.c $(DEBUG_CFLAGS) $(PERF_CLFAGS)



clean:
	rm -f *.o *.a