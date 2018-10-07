// vi:ft=c
#ifndef DHT_SPAMFILTER_H
#define DHT_SPAMFILTER_H

#include <netinet/in.h>
#include <stdbool.h>

void spam_run_epoch(void);
bool spam_check_rx(const struct sockaddr_in *src);

#endif
