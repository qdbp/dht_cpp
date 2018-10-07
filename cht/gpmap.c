#include "ctl.h"
#include "gpmap.h"
#include "log.h"
#include "stat.h"
#include "util.h"
#include <assert.h>
#include <time.h>

static struct timespec __NOW_MS_now;
static u64 now_ms;

#define UPDATE_NOW_MS()                                                        \
    clock_gettime(CLOCK_MONOTONIC_COARSE, &__NOW_MS_now);                      \
    now_ms = (__NOW_MS_now.tv_sec * 1000) + (__NOW_MS_now.tv_nsec / 1000000);

#define N_BINS (1 << 16)
#define MAX_USED (7 * (N_BINS >> 3))

#define IFL_BIN_SIZE 64

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

//
// INTERNAL FUNCTIONS
//

static inline void set_cell(gpm_ih_status_t *cell) {
    UPDATE_NOW_MS()
    cell->last_reponse_ms = now_ms;
    if (!cell->is_set) {
        cell->is_set = true;
        g_n_bins_used++;
        st_set(ST_ctl_n_gpm_bufs, g_n_bins_used);
    }
}

static inline void unset_cell(gpm_ih_status_t *cell) {
    if (cell->is_set) {
        cell->is_set = false;
        g_n_bins_used--;
        st_set(ST_ctl_n_gpm_bufs, g_n_bins_used);
    }
}

static inline void set_ih_status(u16 tok, const nih_t nid, const nih_t ih,
                                 u8 hop) {
    gpm_ih_status_t *cell = &g_ifl_buf[tok];

    cell->last_nid_checksum = nid.checksum;
    cell->ih = ih;
    cell->hop_ctr = hop;

    set_cell(cell);
}

inline static gpm_ih_status_t *gpm_lookup_tok(const parsed_msg *krpc) {
    assert(krpc->tok_len == 3);

    u16 tok = *(u16 *)(krpc->tok);
    gpm_ih_status_t *cell = &g_ifl_buf[tok];

    if (!(cell->is_set) || cell->last_nid_checksum != krpc->nid.checksum) {
        return NULL;
    }

    return cell;
}

// Writes up to MAX_GP_PNODES nodes to the next hop structure; could be less,
// writes the actual number written to the structure.
static inline void find_best_hops(gpm_next_hop_t *next_hop,
                                  const parsed_msg *krpc, const nih_t ih) {
    u8 n_pnodes =
        (MAX_GP_PNODES < krpc->n_nodes) ? MAX_GP_PNODES : krpc->n_nodes;

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

    for (u8 ix = 0; ix < krpc->n_nodes; ix += 1) {
        this_dkad = dkad(ih, krpc->nodes[ix].nid);
        this_ix = ix;
        for (u8 jx = 0; jx < MAX_GP_PNODES; jx++) {
            if (this_dkad < best_dkads[jx] && this_dkad > BULLSHIT_DKAD) {
                displace = true;
            }
            if (displace) {
                next_dkad = best_dkads[jx];
                next_ix = best_ixes[jx];
                best_dkads[jx] = this_dkad;
                best_ixes[jx] = this_ix;
                this_ix = next_ix;
                this_dkad = next_dkad;
            }
        }
        displace = false;
        st_click_dkad(this_dkad);
    }

    for (int dx = 0; dx < n_pnodes; dx++) {
        next_hop->pnodes[dx] = krpc->nodes[best_ixes[dx]];
    }

    next_hop->n_pnodes = n_pnodes;
}

//
// PUBLIC FUNCTIONS
//

bool get_vacant_tok(u16 *dst) {

    if (g_n_bins_used > MAX_USED) {
        return false;
    }

    UPDATE_NOW_MS()
    u16 ix = (u16)randint(0, N_BINS);

    gpm_ih_status_t *cell;

    for (int ctr = 0; ctr < N_BINS; ctr++) {
        cell = &g_ifl_buf[ix];

        if (cell->is_set &&
            (now_ms - cell->last_reponse_ms > CTL_GPM_TIMEOUT_MS)) {
            unset_cell(cell);
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

bool gpm_decide_pursue_q_gp_ih(u16 *restrict assign_tok,
                               const parsed_msg *rcvd) {
    // TODO connect to db and do actual business logic checks

    if (get_vacant_tok(assign_tok)) {
        return true;
    } else {
        st_inc(ST_gpm_ih_drop_buf_overflow);
        return false;
    }
}

void gpm_register_q_gp_ihash(nih_t nid, nih_t ih, u8 hop, u16 tok) {
    if (hop > GP_MAX_HOPS) {
        st_inc(ST_gpm_ih_drop_too_many_hops);
        return;
    }
    set_ih_status(tok, nid, ih, hop);
    st_inc(ST_gpm_ih_inserted);
};

bool gpm_extract_tok(gpm_next_hop_t *restrict next_hop,
                     const parsed_msg *krpc) {

    assert(krpc->n_nodes > 0);

    gpm_ih_status_t *cell = gpm_lookup_tok(krpc);

    if (!cell) {
        st_inc(ST_gpm_r_gp_lookup_failed);
        return false;
    }

    // u8 best_node = find_best_hop(krpc, extract->ih);
    next_hop->ih = cell->ih;
    find_best_hops(next_hop, krpc, cell->ih);
    next_hop->hop_ctr = cell->hop_ctr;

    // Important! This is how we clean up. There is no separate command or
    // sweep.
    unset_cell(cell);

    return true;
};

void gpm_clear_tok(const parsed_msg *krpc) {
    gpm_ih_status_t *cell = gpm_lookup_tok(krpc);
    if (cell) {
        unset_cell(cell);
    }
}

i32 gpm_get_tok_hops(const parsed_msg *krpc) {
    gpm_ih_status_t *cell = gpm_lookup_tok(krpc);
    if (cell) {
        return cell->hop_ctr;
    }
    return -1;
}
