#include "ctl.h"
#include "log.h"
#include "stat.h"

using namespace cht;
namespace cht {

#define DIFF(x) (st_get(x) - st_get_old(x))
#define RATE(x) (1000 * DIFF(x) / (double)(st_now_ms - st_old_ms))

// PUBLIC FUNCTIONS

static u8 ctl_ping_window = 0;

void ctl_init(void) {
}

bool ctl_decide_ping(const Nih &nid) {
    return true;
    // return (nid.ctl_byte & 0xf0) == (ctl_ping_window & 0xf0);
}

void ctl_rollover_hook(void) {
    ctl_ping_window++;
}
} // namespace cht
