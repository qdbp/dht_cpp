// vi:ft=cpp
#ifndef DHT_SPAMFILTER_H
#define DHT_SPAMFILTER_H

#include "cppcompat.h"
#include "dht.h"
#include <netinet/in.h>
#include <stdbool.h>

EXTERN_C(void spam_run_epoch(void))
EXTERN_C(bool spam_check_rx(u32 ip_addr))
EXTERN_C(bool spam_check_tx_ping(u32 ip_addr))
EXTERN_C(bool spam_check_tx_q_gp(u32 ip_addr, nih_t ih))

#endif
