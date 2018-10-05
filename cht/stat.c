#include "dht.h"
#include "log.h"
#include "stat.h"
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
        fprintf(stderr, "%s: %s", (errmsg), strerror(errno));                  \
    }

static u64 g_ctr[ST__ST_ENUM_END] = {0};

#ifdef STAT_DKAD
static u64 g_dkad_ctr[161] = {0};
#endif

void st_init() {
#ifdef STAT_CSV
    WITH_FILE(csv, STAT_CSV_FN, "w") {

        fprintf(csv, "time,");
        for (int ix = 1; ix < ST__ST_ENUM_END - 1; ix++) {
            fprintf(csv, "%s,", stat_names[ix]);
        }
        fprintf(csv, "\n");
    }
    ENDWITH(csv, "Could not open CSV " STAT_CSV_FN " for writing")
#endif
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
#ifdef STAT_DKAD
    assert(dkad <= 160);
    g_dkat_ctr[dkad]++;
#endif
};

void st_rollover(void) {
#ifdef STAT_CSV
    WITH_FILE(csv, STAT_CSV_FN, "a") {

        fprintf(csv, "%lu,", (u64)time(0));
        for (int ix = 1; ix < ST__ST_ENUM_END - 1; ix++) {
            fprintf(csv, "%lu,", g_ctr[ix]);
        }
        fprintf(csv, "\n");
    }
    ENDWITH(csv, "Could not open " STAT_CSV_FN " for appending")
#ifdef STAT_DKAD
    WITH_FILE(csv, STAT_DKAD_FN, "w") {

        for (int ix = 0; ix <= 160; ix++) {
            fprintf(csv, "%d,", ix);
        }
        fprintf(csv, "\n");
        for (int ix = 0; ix <= 160; ix++) {
            fprintf(csv, "%d,", g_dkad_ctr[ix]);
        }
    }
    ENDWITH(csv, "Could not open CSV " STAT_DKAD_FN " for writing")
#endif // STAT_DKAD
#endif // STAT_CSV
}
