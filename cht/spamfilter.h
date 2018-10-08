// vi:ft=c
#ifndef DHT_SPAMFILTER_H
#define DHT_SPAMFILTER_H

#include "dht.h"
#include <netinet/in.h>
#include <stdbool.h>

void spam_run_epoch(void);
bool spam_check_rx(u32 ip_addr);
bool spam_check_tx_ping(u32 ip_addr);

#endif
