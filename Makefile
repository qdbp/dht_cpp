CPP = /usr/bin/clang++
FAST = -march=native -Ofast -flto -finline-functions
FAST_CALLGRIND = -march=native -Ofast -flto -fno-inline-functions
DEBUG = $(FAST) -g
CPPFLAGS = -std=c++17 -Wall -Werror -fno-exceptions
LDFLAGS = -luv

CFG = \
	-DLOGLEVEL=LVL_INFO \
	-DSTAT_CSV \
	-DSTAT_AUX \
	-DMSG_CLOSE_SID \

CFG_DEBUG = \
	-DLOGLEVEL=LVL_DEBUG \
	-DMSG_CLOSE_SID \
	-DSTAT_AUX \
	-DSTAT_CSV

CFG_PROD =\
	-DLOGLEVEL=LVL_WARN\
	-DMSG_CLOSE_SID \

.PHONY: rtdump callgrind

build:
	$(CPP) $(CPPFLAGS) $(FAST) $(CFG) cht/*.cpp $(LDFLAGS) -o ./dht

run: build
	./dht

build_prod:
	$(CPP) $(CPPFLAGS) $(FAST) $(CFG_PROD) cht/*.cpp $(LDFLAGS) -o ./dht

prod: build_prod
	./dht

build_tbd:
	$(CPP) $(CPPFLAGS) $(FAST) -DBD_TRACE $(LDFLAGS) cht/*.cpp -DBD_TRACE -o dht_trace

trace_bd: build_tbd
	./dht_trace

build_debug:
	$(CPP) $(CPPFLAGS) $(DEBUG) $(CFG_DEBUG) cht/*.cpp $(LDFLAGS) -o dht_dbg

gdb: build_debug
	gdb -ex run ./dht_dbg

debug: build_debug
	./dht_dbg

rtdump: rtdump/main.c
	$(CC) $(CFLAGS) $(FAST) rtdump/main.c -o rtd

build_callgrind:
	$(CPP) $(CPPFLAGS) $(FAST_CALLGRIND) $(CFG_PROD) cht/*.cpp $(LDFLAGS) -o dht_callgrind

callgrind: build_callgrind
	valgrind --tool=callgrind --dump-instr=yes --collect-jumps=yes ./dht_callgrind

callgrind_prod: build_prod
	valgrind --tool=callgrind --dump-instr=yes --collect-jumps=yes ./dht
