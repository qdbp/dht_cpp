#include "ctl.h"
#include "log.h"
#include "stat.h"

#define DIFF(x) (st_get(x) - st_get_old(x))
#define RATE(x) (1000 * DIFF(x) / (double)(st_now_ms - st_old_ms))

// PUBLIC FUNCTIONS

inline bool ctl_decide_ping(nih_t nid) {
    return nid.ctl_byte >= st_get(ST_ctl_ping_thresh);
}

void ctl_rollover_hook(void) {
    // Calibrate ping rate
    double ping_rate = RATE(ST_tx_q_pg);
    u64 cur_thresh = st_get(ST_ctl_ping_thresh);

    DEBUG("ping rate = %f", ping_rate)
    DEBUG("ping diff = %lu", DIFF(ST_tx_q_pg))

    u8 delta = cur_thresh > 230 ? 1 : 5;
    if (ping_rate < CTL_PPS_TARGET) {
        cur_thresh -= delta;
    } else {
        cur_thresh += delta;
        DEBUG("Set ping thresh to %d", cur_thresh);
    }
    st_set(ST_ctl_ping_thresh, cur_thresh < 0 ? 0 : cur_thresh);
}
