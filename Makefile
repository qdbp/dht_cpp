CC = /usr/bin/clang
CPP = /usr/bin/clang++
GCC = /usr/bin/gcc
FAST = -march=native -Ofast -flto -finline-functions
CFLAGS = -std=gnu17 -Wall -Werror
CPPFLAGS = -std=gnu++17 -Wall -Werror -fno-exceptions
LDFLAGS = -luv -flto

CFG = \
	-DSTAT_CSV \
	-DLOGLEVEL=LVL_INFO \
	-DSTAT_AUX \
	-DMSG_CLOSE_SID \
#	-DBD_TRACE

.PHONY: rtdump callgrind

build:
	$(CC) $(CFLAGS) $(FAST) $(CFG) -luv cht/*.c -o dht_fast

build:
	rm -rf ./build_cpp
	mkdir build_cpp/
	$(CC) -c $(CFLAGS) $(FAST) $(CFG) cht/*.c
	$(CPP) -c $(CPPFLAGS) $(FAST) $(CFG) cht/*.cpp
	mv *.o build_cpp/
	$(CPP) $(LDFLAGS) -o dht_cpp build_cpp/*.o
	rm -rf ./build_cpp

run: build
	./dht_cpp

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
