C = /usr/bin/clang
CPP = /usr/bin/clang++
GCC = /usr/bin/gcc
FAST = -march=native -Ofast -flto -finline-functions
CFLAGS = -Wall -Werror -luv
CPPFLAGS = -Wall -Werror -luv

CFG = \
	-DSTAT_CSV \
	-DLOGLEVEL=LVL_INFO \
	-DSTAT_AUX \
	-DMSG_CLOSE_SID \
#	-DBD_TRACE

.PHONY: rtdump callgrind

build:
	$(CC) $(CFLAGS) $(FAST) $(CFG) cht/*.c -o dht_fast

run: build
	./dht_fast

build_debug:
	$(GCC) $(CFLAGS) $(CFG) -g cht/*.c -o dht_dbg

debug: build_debug
	gdb -ex run ./dht_dbg

trace_bd: build_tbd
	./dht_trace

build_tbd:
	$(CC) $(CFLAGS) $(CFG) cht/*.c -DBD_TRACE -o dht_trace

rtdump: rtdump/main.c
	$(CC) $(CFLAGS) $(FAST) rtdump/main.c -o rtd

callgrind: build
	valgrind --tool=callgrind ./dht_fast
