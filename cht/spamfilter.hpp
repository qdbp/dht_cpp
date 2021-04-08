#pragma once

#include "dht.hpp"

using namespace cht;
namespace cht {

void spam_run_epoch();
bool spam_check_rx(u32 ip_addr);
bool spam_check_tx_pg(u32 ip_addr);
bool spam_check_tx_q_gp(u32 ip_addr, const Nih &ih);
} // namespace cht
