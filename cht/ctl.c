#include "ctl.h"
#include "log.h"
#include "stat.h"

#define DIFF(x) (st_get(x) - st_get_old(x))
#define RATE(x) (1000 * DIFF(x) / (double)(st_now_ms - st_old_ms))

// PUBLIC FUNCTIONS

void ctl_init(void) {
    st_set(ST_ctl_ping_thresh, 0);
}

inline bool ctl_decide_ping(nih_t nid) {
    return nid.ctl_byte >= st_get(ST_ctl_ping_thresh);
}

void ctl_rollover_hook(void) {
    // Calibrate ping rate
#ifdef CTL_PPS_TARGET
    double ping_rate = RATE(ST_tx_q_pg);
    u64 cur_thresh = st_get(ST_ctl_ping_thresh);

    u8 delta = cur_thresh > 230 ? 1 : 5;
    if (ping_rate < CTL_PPS_TARGET) {
        cur_thresh = delta <= cur_thresh ? cur_thresh - delta : 0;
    } else {
        cur_thresh += delta;
    }
    st_set(ST_ctl_ping_thresh, cur_thresh);
#endif
}
