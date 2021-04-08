#include "log.hpp"

// not thread safe
time_t __g_log_time;
char __g_log_fmttime[64] = {0};
