// vi:ft=cpp
#pragma once

#include "dht.h"
#include <netinet/in.h>
#include <stdbool.h>

using namespace cht;
namespace cht {

void spam_run_epoch(void);
bool spam_check_rx(u32 ip_addr);
bool spam_check_tx_pg(u32 ip_addr);
bool spam_check_tx_q_gp(u32 ip_addr, const Nih &ih);
} // namespace cht
