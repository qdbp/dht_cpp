#include "dht.h"
#include "log.h"
#include "stat.h"
#include <errno.h>
#include <time.h>

const char *stat_names[] = {FORSTAT(AS_STR)};

static u64 g_ctr[ST__ST_ENUM_END] = {0};
static u64 g_ctr_old[ST__ST_ENUM_END] = {0};
static u64 g_ctr_diff[ST__ST_ENUM_END] = {0};

#ifdef STAT_DKAD
static u64 g_dkad_ctr[161] = {0};
#endif

static u64 time_old = 0;
static u64 time_now = 0;

void st_init() {
    time_now = (u64)time(0);
    time_old = (u64)time(0);
#ifdef STAT_CSV
    FILE *csv = fopen(STAT_CSV_FN, "w");
    if (csv != NULL) {
        fprintf(csv, "time,");
        for (int ix = 1; ix < ST__ST_ENUM_END - 1; ix++) {
            fprintf(csv, "%s,", stat_names[ix]);
        }
        fprintf(csv, "\n");
        fflush(csv);
    } else {
        WARN("Could not open CSV " STAT_CSV_FN " for writing: %s",
             strerror(errno));
    }
#endif
}

inline void st_inc(stat_t stat) {
    g_ctr[stat]++;
}

inline void st_inc_debug(stat_t stat) {
    g_ctr[stat]++;
    DEBUG("%s -> %lu", stat_names[stat], g_ctr[stat]);
}

inline void st_add(stat_t stat, u32 val) {
    g_ctr[stat] += val;
}

inline void st_click_dkad(u8 dkad) {
#ifdef STAT_DKAD
    assert(dkad <= 160);
    g_dkat_ctr[dkad]++;
#endif
};

void st_rollover(void) {
    for (int i = 0; i < ST__ST_ENUM_END; i++) {
        g_ctr_diff[i] = g_ctr[i] - g_ctr_old[i];
        g_ctr_old[i] = g_ctr[i];
    }
    time_old = time_now;
    time_now = (u64)time(0);
#ifdef STAT_CSV
    FILE *csv = fopen(STAT_CSV_FN, "a");
    if (csv != NULL) {
        fprintf(csv, "%lu,", (unsigned long)time(0));
        for (int ix = 1; ix < ST__ST_ENUM_END - 1; ix++) {
            fprintf(csv, "%lu,", g_ctr_diff[ix]);
        }
        fprintf(csv, "\n");
        fflush(csv);
    } else {
        WARN("Could not open CSV " STAT_CSV_FN " for appending: %s",
             strerror(errno));
    }
#ifdef STAT_DKAD
    csv = fopen(STAT_DKAD_FN, "w");
    if (csv != NULL) {
        for (int ix = 0; ix <= 160; ix++) {
            fprintf(csv, "%d,", ix);
        }
        fprintf(csv, "\n");
        for (int ix = 0; ix <= 160; ix++) {
            fprintf(csv, "%d,", g_dkad_ctr[ix]);
        }
        fflush(csv);
    } else {
        WARN("Could not open CSV " STAT_CSV_FN " for appending: %s",
             strerror(errno));
    }
#endif // STAT_DKAD
#endif // STAT_CSV
}
