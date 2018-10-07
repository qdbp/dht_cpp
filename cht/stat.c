#include "ctl.h"
#include "dht.h"
#include "gpmap.h"
#include "log.h"
#include "stat.h"
#include <assert.h>
#include <errno.h>
#include <time.h>

const char *stat_names[] = {FORSTAT(AS_STR)};

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

static u64 g_ctr[ST__ST_ENUM_END] = {0};
static u64 g_ctr_old[ST__ST_ENUM_END] = {0};
u64 st_now_ms = 0;
u64 st_old_ms = 0;

struct timespec __st_now = {0};
#define ROLLOVER_TIME()                                                        \
    clock_gettime(CLOCK_MONOTONIC_COARSE, &__st_now);                          \
    st_old_ms = st_now_ms;                                                     \
    st_now_ms = (__st_now.tv_sec * 1000) + (__st_now.tv_nsec / 1000000);

#ifdef STAT_AUX
static u64 g_dkad_ctr[161] = {0};
static u64 g_n_hops_ctr[GP_MAX_HOPS + 1] = {0};
#endif

void st_init() {
    ROLLOVER_TIME()
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

inline void st_inc(stat_t stat) {
    g_ctr[stat]++;
}

inline void st_set(stat_t stat, u64 val) {
    g_ctr[stat] = val;
}

inline void st_inc_debug(stat_t stat) {
    g_ctr[stat]++;
    DEBUG("%s -> %lu", stat_names[stat], g_ctr[stat]);
}

inline void st_add(stat_t stat, u32 val) {
    g_ctr[stat] += val;
}

inline void st_click_dkad(u8 dkad) {
#ifdef STAT_AUX
    assert(dkad <= 160);
    g_dkad_ctr[dkad]++;
#endif
};

inline void st_click_gp_n_hops(u8 n_hops) {
#ifdef STAT_AUX
    assert(n_hops < sizeof(g_n_hops_ctr));
    g_n_hops_ctr[n_hops]++;
#endif
}

inline u64 st_get(stat_t stat) {
    return g_ctr[stat];
}

inline u64 st_get_old(stat_t stat) {
    return g_ctr_old[stat];
}

void st_rollover(void) {
    static int write_csv = 0;
    static int next_heartbeat = 0;

    ROLLOVER_TIME()
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
