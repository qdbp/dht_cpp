#include "gpmap.h"
#include "log.h"
#include "stat.h"
#include "util.h"
#include <assert.h>
#include <time.h>

static struct timespec __NOW_MS_now;
static u64 now_ms;

#define UPDATE_NOW_MS()                                                        \
    clock_gettime(CLOCK_MONOTONIC, &__NOW_MS_now);                             \
    now_ms = __NOW_MS_now.tv_nsec >> 20; /* ms ...ish */

#define N_BINS (1 << 16)
#define MAX_USED (7 * (N_BINS >> 3))

#define IFL_BIN_SIZE 64
#define IFL_EXPIRE_MS 200
#ifndef IFL_MAX_HOPS
#define IFL_MAX_HOPS 6
#endif

typedef struct gpm_ih_status {
    nih_t ih;              // The infohash being tracked
    u32 last_nid_checksum; // The last peer we contacted
    u64 last_reponse_ms;   // Last time we got a r_gp for this infohash
    bool is_set;           // whether the entry is valid
    u8 hop_ctr;
} gpm_ih_status_t;

// Random cells will be assigned from this array
static gpm_ih_status_t g_ifl_buf[N_BINS] = {{{{0}}}};
// The number of used cells. If this number equals or exceeds MAX_USED, new
// cells will be denied. The maximum should be low enough to minimize the
// probability of pathological iteration, on the same principles as a
// linear-probed hashmap.
static u16 g_n_bins_used = 0;

#define SET_CELL(cell)                                                         \
    if (!((cell)->is_set)) {                                                   \
        (cell)->is_set = true;                                                 \
        g_n_bins_used++;                                                       \
        st_set(ST_ctl_n_gpm_bufs, g_n_bins_used);                              \
    }

#define UNSET_CELL(cell)                                                       \
    if ((cell)->is_set) {                                                      \
        (cell)->is_set = false;                                                \
        g_n_bins_used--;                                                       \
        st_set(ST_ctl_n_gpm_bufs, g_n_bins_used);                              \
    }

//
// INTERNAL FUNCTIONS
//

static inline u32 nih_checksum(const nih_t nih) {
    return *(u32 *)(nih.raw + 16);
}

inline bool get_vacant_tok(u16 *dst) {

    if (g_n_bins_used > MAX_USED) {
        DEBUG("too many bins used")
        return false;
    }

    UPDATE_NOW_MS()
    u16 ix = (u16)randint(0, N_BINS);

    gpm_ih_status_t *cell;

    for (int ctr = 0; ctr < N_BINS; ctr++) {
        cell = &g_ifl_buf[ix];

        if (cell->is_set && (now_ms - cell->last_reponse_ms > IFL_EXPIRE_MS)) {
            UNSET_CELL(cell)
            *dst = ix;
            return true;
        } else if (!(cell->is_set)) {
            *dst = ix;
            return true;
        }
        // overflow as needed. Could also be *= (prime != 2), if that's
        // faster...
        ix++;
    }
    WARN("Iterated every cell! Did you set MAX_USED correctly?");
    return false;
}

static inline void set_ih_status(u16 tok, const nih_t nid, const nih_t ih,
                                 u8 hop) {

    gpm_ih_status_t *cell = &g_ifl_buf[tok];

    cell->last_nid_checksum = nih_checksum(nid);
    cell->ih = ih;

    UPDATE_NOW_MS()
    cell->last_reponse_ms = now_ms;
    cell->hop_ctr = hop;

    SET_CELL(cell)
}

// Writes up to MAX_GP_PNODES nodes to the next hop structure; could be less,
// writes the actual number written to the structure.
static inline void find_best_hops(gpm_next_hop_t *next_hop,
                                  const parsed_msg *krpc_msg, const nih_t ih) {
    u8 n_pnodes =
        (MAX_GP_PNODES < krpc_msg->n_nodes) ? MAX_GP_PNODES : krpc_msg->n_nodes;

    u8 this_dkad;
    u8 this_ix;
    u8 next_dkad;
    u8 next_ix;
    u8 best_dkads[MAX_GP_PNODES] = {0};
    u8 best_ixes[MAX_GP_PNODES] = {0};

    for (u8 ix = 0; ix < MAX_GP_PNODES; ix++) {
        best_dkads[ix] = 160;
    }

    bool displace = false;

    for (u8 ix = 0; ix < krpc_msg->n_nodes; ix += 1) {
        this_dkad = dkad(ih, krpc_msg->nodes[ix].nid);
        this_ix = ix;
        // DEBUG("Got new dkad %d at %d", this_dkad, this_ix)
        for (u8 jx = 0; jx < MAX_GP_PNODES; jx++) {
            if (this_dkad < best_dkads[jx] && this_dkad > BULLSHIT_DKAD) {
                displace = true;
                // DEBUG("Found better ix %d at jx %d in list", this_ix, jx);
            }
            if (displace) {
                next_dkad = best_dkads[jx];
                next_ix = best_ixes[jx];
                best_dkads[jx] = this_dkad;
                best_ixes[jx] = this_ix;
                // DEBUG("[jx %d] moved ix %d (dkad %d) to where %d (dkad %d)
                // was",
                //       jx, this_ix, this_dkad, next_ix, next_dkad);
                this_ix = next_ix;
                this_dkad = next_dkad;
            }
        }
        displace = false;
        st_click_dkad(this_dkad);
    }

    for (int dx = 0; dx < n_pnodes; dx++) {
        // DEBUG("Writing ix %d (dkad %d)", best_ixes[dx], best_dkads[dx]);
        next_hop->pnodes[dx] = krpc_msg->nodes[best_ixes[dx]];
    }

    next_hop->n_pnodes = n_pnodes;
}

//
// PUBLIC FUNCTIONS
//

bool gpm_decide_pursue_q_gp_ih(u16 *assign_tok, const parsed_msg *rcvd) {
    // TODO connect to db and do actual business logic checks

    if (get_vacant_tok(assign_tok)) {
        return true;
    } else {
        st_inc_debug(ST_gpm_ih_drop_buf_overflow);
        return false;
    }
}

void gpm_register_q_gp_ihash(nih_t nid, nih_t ih, u8 hop, u16 tok) {

    if (hop > IFL_MAX_HOPS) {
        st_inc(ST_gpm_ih_drop_too_many_hops);
        return;
    }

    set_ih_status(tok, nid, ih, hop);
    st_inc(ST_gpm_ih_inserted);
};

bool gpm_extract_r_gp_ih(gpm_next_hop_t *restrict next_hop,
                         const parsed_msg *krpc_msg) {

    assert(krpc_msg->n_nodes > 0);
    assert(krpc_msg->tok_len == 3);

    u16 tok = *(u16 *)(krpc_msg->tok);
    gpm_ih_status_t *cell = &g_ifl_buf[tok];

    if (!cell->is_set) {
        st_inc(ST_gpm_r_gp_lookup_empty);
        return false;
    } else if (cell->last_nid_checksum != nih_checksum(krpc_msg->nid)) {
        st_inc(ST_gpm_r_gp_bad_checksum);
        return false;
    }

    // u8 best_node = find_best_hop(krpc_msg, extract->ih);
    next_hop->ih = cell->ih;
    find_best_hops(next_hop, krpc_msg, cell->ih);
    next_hop->hop_ctr = cell->hop_ctr;

    // Important! This is how we clean up. There is no separate command or
    // sweep.
    UNSET_CELL(cell)

    return true;
};
