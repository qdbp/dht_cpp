#include "ctl.hpp"
#include "dht.hpp"
#include "gpmap.hpp"
#include "log.hpp"
#include "spamfilter.hpp"
#include "stat.hpp"
#include <cassert>
#include <cerrno>
#include <ctime>

#include <chrono>

namespace chr = std::chrono;

using namespace cht;
namespace cht {

#define WITH_FILE(f, fn, mode)                                                 \
    FILE *f = fopen((fn), (mode));                                             \
    if (f) {

#define ENDWITH(f, errmsg)                                                     \
    fflush(f);                                                                 \
    fclose(f);                                                                 \
    }                                                                          \
    else {                                                                     \
        ERROR("%s: %s", (errmsg), strerror(errno));                            \
    }

// const char *stat_names[] = {FORSTAT(AS_STR)};

static chr::time_point<chr::steady_clock> st_time_now;
static chr::time_point<chr::steady_clock> st_time_old;

static u64 g_ctr[ST__ST_ENUM_END] = {0};
static u64 g_ctr_old[ST__ST_ENUM_END] = {0};

static inline void rollover_time() {
    st_time_old = st_time_now;
    st_time_now = chr::steady_clock::now();
}

#ifdef STAT_AUX
static u64 g_dkad_ctr[161] = {0};
static u64 g_n_hops_ctr[GP_MAX_HOPS + 1] = {0};
#endif

void st_init() {
    rollover_time();
#ifdef STAT_CSV
    WITH_FILE(csv, STAT_CSV_FN, "w") {

        fprintf(csv, "time,");
        for (int ix = 1; ix < ST__ST_ENUM_END - 1; ix++) {
            fprintf(csv, "%s,", stat_names[ix]);
        }
        fprintf(csv, "\n");
    }
    ENDWITH(csv, "Could not open CSV " STAT_CSV_FN " for writing")
#ifdef STAT_AUX
    WITH_FILE(csv_aux, STAT_AUX_FN, "w") {

        for (int ix = 0; ix <= 160; ix++) {
            fprintf(csv_aux, "dkad_%d,", ix);
        }
        for (int ix = 0; ix <= GP_MAX_HOPS; ix++) {
            fprintf(csv_aux, "gp_hops_%d,", ix);
        }
        fprintf(csv_aux, "\n");
    }
    ENDWITH(csv_aux, "Could not open aux CSV " STAT_AUX_FN " for writing")
#endif // STAT_AUX
#endif // STAT_CSV
}

void st_inc(stat_t stat) {
    g_ctr[stat]++;
}

void st_dec(stat_t stat) {
    g_ctr[stat] = g_ctr[stat] == 0 ? 0 : g_ctr[stat] - 1;
}

void st_set(stat_t stat, u64 val) {
    g_ctr[stat] = val;
}

void st_inc_debug(stat_t stat) {
    g_ctr[stat]++;
    DEBUG("%s -> %lu", stat_names[stat], g_ctr[stat]);
}

void st_add(stat_t stat, u32 val) {
    g_ctr[stat] += val;
}

void st_click_dkad(u8 dkad) {
#ifdef STAT_AUX
    assert(dkad <= 160);
    g_dkad_ctr[dkad]++;
#endif
};

void st_click_gp_n_hops(u8 n_hops) {
#ifdef STAT_AUX
    assert(n_hops < sizeof(g_n_hops_ctr));
    g_n_hops_ctr[n_hops]++;
#endif
}

u64 st_get(stat_t stat) {
    return g_ctr[stat];
}

u64 st_get_old(stat_t stat) {
    return g_ctr_old[stat];
}

void st_rollover() {
    static int write_csv = 0;
    static int next_heartbeat = 0;

    // TODO move to own uv loop
    spam_run_epoch();
    rollover_time();
    ctl_rollover_hook();

    for (int ix = 0; ix < ST__ST_ENUM_END; ix++) {
        g_ctr_old[ix] = g_ctr[ix];
    };

    if (next_heartbeat != STAT_HB_EVERY - 1) {
        next_heartbeat++;
    } else {
        INFO("Heartbeat: %010lu pkts sent, %010lu pkts rcvd", g_ctr[ST_tx_tot],
             g_ctr[ST_rx_tot]);
        next_heartbeat = 0;
    }

    if (write_csv != STAT_CSV_EVERY - 1) {
        write_csv++;
        return;
    }

    write_csv = 0;
    VERBOSE("Writing CSV")

#ifdef STAT_CSV
    WITH_FILE(csv, STAT_CSV_FN, "a") {

        fprintf(csv, "%lu,", (u64)time(0));
        for (int ix = 1; ix < ST__ST_ENUM_END - 1; ix++) {
            fprintf(csv, "%lu,", g_ctr[ix]);
        }
        fprintf(csv, "\n");
    }
    ENDWITH(csv, "Could not open " STAT_CSV_FN " for appending")

#ifdef STAT_AUX
    WITH_FILE(csv_aux, STAT_AUX_FN, "a") {
        // dkad statistics
        for (int ix = 0; ix <= 160; ix++) {
            fprintf(csv_aux, "%lu,", g_dkad_ctr[ix]);
        }
        // hop statistics
        for (int ix = 0; ix < GP_MAX_HOPS; ix++) {
            fprintf(csv_aux, "%lu,", g_n_hops_ctr[ix]);
        }
        fprintf(csv_aux, "\n");
    }
    ENDWITH(csv_aux, "Could not open CSV " STAT_AUX_FN " for writing")
#endif // STAT_AUX
#endif // STAT_CSV
}
} // namespace cht
