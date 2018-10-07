C = /usr/bin/clang
CPP = /usr/bin/clang++
GCC = /usr/bin/gcc
FAST = -march=native -Ofast -flto -finline-functions
CFLAGS = -DSTAT_CSV -Wall -Werror -luv
CPPFLAGS = -DSTAT_CSV -Wall -Werror -luv

CFG = \
	-DLOGLEVEL=LVL_INFO \
	-DMSG_CLOSE_SID \
	-DSTAT_AUX \
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
