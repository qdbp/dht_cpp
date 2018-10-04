#ifndef DHT_UTIL_H
#define DHT_UTIL_H

#include "dht.h"
#include <stdbool.h>

u64 randint(u64, u64);
bool is_valid_utf8(const unsigned char[], u64);
u8 dkad(const char [20], const char[20]);

#endif //DHT_UTIL_H
