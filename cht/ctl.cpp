#include "ctl.hpp"
#include "log.hpp"
#include "stat.hpp"

using namespace cht;
namespace cht {

#define DIFF(x) (st_get(x) - st_get_old(x))
#define RATE(x) (1000 * DIFF(x) / (double)(st_now_ms - st_old_ms))

// PUBLIC FUNCTIONS

static u8 ctl_ping_window = 0;

void ctl_init() {
}

bool ctl_decide_ping(const Nih &nid) {
    return true;
    // return (nid.ctl_byte & 0xf0) == (ctl_ping_window & 0xf0);
}

void ctl_rollover_hook() {
    ctl_ping_window++;
}
} // namespace cht
