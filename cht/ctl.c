#include "ctl.h"
#include "log.h"
#include "stat.h"

#define DIFF(x) (st_get(x) - st_get_old(x))
#define RATE(x) (1000 * DIFF(x) / (double)(st_now_ms - st_old_ms))

i32 g_ctl_ping_thresh = -1;

// PUBLIC FUNCTIONS

inline bool ctl_decide_ping(nih_t nid) {
    return nid.ctl_byte >= g_ctl_ping_thresh;
}

void ctl_rollover_hook(void) {
    // Calibrate ping rate
    double ping_rate = RATE(ST_tx_q_pg);
    DEBUG("ping rate = %f", ping_rate)
    DEBUG("ping diff = %lu", DIFF(ST_tx_q_pg))
    if (ping_rate < CTL_PPS_TARGET) {
        g_ctl_ping_thresh = g_ctl_ping_thresh < 0 ? -1 : g_ctl_ping_thresh - 5;
    } else {
        g_ctl_ping_thresh += g_ctl_ping_thresh > 220 ? 1 : 5;
        DEBUG("Set ping thresh to %d", g_ctl_ping_thresh);
    }
}
