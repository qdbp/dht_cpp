#pragma once

#include "dht.h"
#include "krpc.h"
#include "rt.h"
#include <stdbool.h>

using namespace cht;
namespace cht {

#ifndef CTL_GPM_TIMEOUT
#define CTL_GPM_TIMEOUT_MS 200
#endif

bool ctl_decide_ping(const Nih &nid);

// Function to be called on stat rollover, to compute control parameters
// from the value of the counters and their values at the previous rollover.
void ctl_rollover_hook(void);

void ctl_init(void);

} // namespace cht
