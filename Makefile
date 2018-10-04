CC = /usr/bin/clang
CPP = /usr/bin/clang++
GCC = /usr/bin/gcc
FAST = -march=native -Ofast -flto # -finline-functions
CFLAGS = -DLOGLEVEL=1 -DSTAT_CSV -Wall -Werror -luv
CPPFLAGS = -DLOGLEVEL=1 -DSTAT_CSV -Wall -Werror -luv


build:
	$(CC) $(CFLAGS) $(FAST) src/*.c -o dht_fast

build_gcc:
	$(CC) $(CFLAGS) $(FAST) -frename-registers src/*.c -o dht_fast_gcc

run: build
	./dht_fast

build_debug:
	$(GCC) $(CFLAGS) -g src/*.c -o dht_dbg

debug: build_debug
	gdb -ex run ./dht_dbg

trace_bd: build_tbd
	./dht_trace

build_tbd:
	$(GCC) $(CFLAGS) src/*.c -DBD_TRACE -o dht_trace
