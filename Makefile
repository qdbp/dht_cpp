C = /usr/bin/clang
CPP = /usr/bin/clang++
GCC = /usr/bin/gcc
FAST = -march=native -Ofast -flto -finline-functions
CFLAGS = -DLOGLEVEL=1 -DSTAT_CSV -Wall -Werror -luv
CPPFLAGS = -DLOGLEVEL=1 -DSTAT_CSV -Wall -Werror -luv

.PHONY: rtdump valgrind

build:
	$(CC) $(CFLAGS) $(FAST) cht/*.c -o dht_fast -DCTL_PPS_TARGET=1000.0

build_gcc:
	$(GCC) $(CFLAGS) $(FAST) -frename-registers cht/*.c -o dht_fast_gcc

run: build
	./dht_fast

build_debug:
	$(GCC) $(CFLAGS) -g cht/*.c -o dht_dbg

debug: build_debug
	gdb -ex run ./dht_dbg

trace_bd: build_tbd
	./dht_trace

build_tbd:
	$(GCC) $(CFLAGS) cht/*.c -DBD_TRACE -o dht_trace

rtdump: rtdump/main.c
	$(GCC) $(CFLAGS) $(FAST) rtdump/main.c -o rtd

valgrind: build
	valgrind --tool=callgrind ./dht_fast
