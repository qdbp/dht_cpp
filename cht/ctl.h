// vi:ft=c
#ifndef DHT_CTL_H
#define DHT_CTL_H

#ifndef CTL_GPM_TIMEOUT
#define CTL_GPM_TIMEOUT_MS 175
#endif

#include "bdecode.h"
#include "dht.h"
#include "rt.h"
#include <stdbool.h>

bool ctl_decide_ping(const nih_t nid);

// Function to be called on stat rollover, to compute control parameters
// from the value of the counters and their values at the previous rollover.
void ctl_rollover_hook(void);

void ctl_init(void);

#endif
