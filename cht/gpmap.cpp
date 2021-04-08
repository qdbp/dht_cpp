#include "gpmap.hpp"
#include <cassert>

using namespace cht;
namespace cht::gpm {

static struct timespec __NOW_MS_now;
static u64 now_ms;

#define UPDATE_NOW_MS()                                                        \
    clock_gettime(CLOCK_MONOTONIC_COARSE, &__NOW_MS_now);                      \
    now_ms = (__NOW_MS_now.tv_sec * 1000) + (__NOW_MS_now.tv_nsec / 1000000);

constexpr u32 N_BINS = 1 << 16;
// Random cells will be assigned from this array

struct __attribute__((packed)) GPMStatus {
  public:
    Nih ih;                // The infohash being tracked
    u32 last_nid_checksum; // The last peer we contacted
    u64 hop_ctr : 7;
    u64 last_reponse_ms : 56; // Last time we got a r_gp for this infohash
    u64 is_set : 1;
};
static_assert(sizeof(GPMStatus) == 32);

static std::array<GPMStatus, N_BINS> g_ifl_buf = {{{{{0}}}}};

//
// INTERNAL FUNCTIONS
//

static inline GPMStatus &set_cell(u16 tok) {
    UPDATE_NOW_MS()
    auto &cell = g_ifl_buf[tok];
    cell.last_reponse_ms = now_ms;
    if (!cell.is_set) {
        cell.is_set = true;
    }
    return cell;
}

static inline void unset_cell(u16 tok) {
    auto &cell = g_ifl_buf[tok];
    if (!cell.is_set) {
        cell.is_set = false;
    }
}

static inline void unset_cell(const KRPC &krpc) {
    auto const &tok = *(u16 *)krpc.tok;
    if (g_ifl_buf[tok].last_nid_checksum == krpc.nid->checksum) {
        unset_cell(tok);
    }
}

static inline void set_ih_status(u16 tok, const Nih &nid, const Nih &ih,
                                 u8 hop) {

    GPMStatus &cell = set_cell(tok);

    cell.last_nid_checksum = nid.checksum;
    cell.ih = ih;
    cell.hop_ctr = hop;
}

inline static std::pair<bool, GPMStatus &> lookup_tok(const bd::KRPC &krpc) {
    assert(krpc.tok_len == 3);

    u16 tok = *(u16 *)(krpc.tok);
    GPMStatus &cell = g_ifl_buf[tok];

    if (!(cell.is_set) || cell.last_nid_checksum != krpc.nid->checksum) {
        return {false, g_ifl_buf[0]};
    }

    return {true, cell};
}

// Writes up to MAX_GP_PNODES nodes to the next hop structure; could be
// less, writes the actual number written to the structure.
static inline void find_best_hops(NextHop &next_hop, const bd::KRPC &krpc,
                                  const Nih &ih) {
    u8 n_pnodes = (MAX_GP_PNODES < krpc.n_nodes) ? MAX_GP_PNODES : krpc.n_nodes;

    u8 this_dkad;
    u8 this_ix;
    u8 next_dkad;
    u8 next_ix;
    u8 best_ixes[MAX_GP_PNODES] = {0};

    u8 best_dkads[MAX_GP_PNODES];
    std::fill(std::begin(best_dkads), std::end(best_dkads), 160);

    bool displace = false;

    for (u8 ix = 0; ix < krpc.n_nodes; ix += 1) {
        this_dkad = dkad(ih, krpc.nodes[ix].nid);
        st_click_dkad(this_dkad);

        this_ix = ix;
        for (u8 jx = 0; jx < MAX_GP_PNODES; jx++) {
            if ((this_dkad < best_dkads[jx]) && (this_dkad > BULLSHIT_DKAD)) {
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
    }

    for (int dx = 0; dx < n_pnodes; dx++) {
        next_hop.pnodes.push_back(krpc.nodes[best_ixes[dx]]);
    }
}

//
// PUBLIC FUNCTIONS
//

std::pair<bool, u16> take_tok() {

    UPDATE_NOW_MS()

    u16 tok = randint(0, 1 << 16);

    for (auto &cell : g_ifl_buf) {

        if (!cell.is_set) {
            return {true, tok};
        }

        if (cell.is_set && now_ms - cell.last_reponse_ms > CTL_GPM_TIMEOUT_MS) {
            unset_cell(tok);
            return {true, tok};
        }

        tok++;
    }

    st_inc(ST_gpm_ih_drop_buf_overflow);
    return {false, 0};
}

std::pair<bool, u16> decide_pursue_q_gp_ih(const bd::KRPC &rcvd) {
    // TODO connect to db and do actual business logic checks

    auto [ok, tok] = take_tok();
    if (ok) {
        return {true, tok};
    } else {
        return {false, 0};
    }
}

void register_q_gp_ihash(const Nih &nid, const Nih &ih, u8 hop, u16 tok) {
    if (hop > GP_MAX_HOPS) {
        st_inc(ST_gpm_ih_drop_too_many_hops);
        return;
    }
    set_ih_status(tok, nid, ih, hop);
    st_inc(ST_gpm_ih_inserted);
};

bool extract_tok(NextHop &next_hop, const bd::KRPC &krpc) {

    assert(krpc.n_nodes > 0);

    auto [ok, cell] = lookup_tok(krpc);

    if (!ok) {
        st_inc(ST_gpm_r_gp_lookup_failed);
        return false;
    }

    next_hop.ih = cell.ih;
    find_best_hops(next_hop, krpc, cell.ih);
    next_hop.hop_ctr = cell.hop_ctr;

    unset_cell(krpc);
    return true;
};

void clear_tok(const bd::KRPC &krpc) {
    unset_cell(krpc);
}

i32 get_tok_hops(const bd::KRPC &krpc) {
    auto [ok, cell] = lookup_tok(krpc);
    if (ok) {
        return cell.hop_ctr;
    }
    return -1;
}
} // namespace cht::gpm
