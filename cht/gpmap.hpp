#pragma once

#include "ctl.hpp"
#include "dht.hpp"
#include "krpc.hpp"
#include "log.hpp"
#include "stat.hpp"
#include "util.hpp"

#include <vector>

using namespace cht;
namespace cht::gpm {

// The GetPeersManager engine

// number of nodes to pursue from each get_peers nodes array
#ifndef MAX_GP_PNODES
#define MAX_GP_PNODES 8
#endif
// dkads lower than this are overwhelmingly improbable, so we ignore any
// nodes like this.
#define BULLSHIT_DKAD 100
// maximum number of chained nodes to query when chasing down an ihash before
// giving up
#ifndef GP_MAX_HOPS
#define GP_MAX_HOPS 8
#endif

struct NextHop {
    Nih ih;
    std::vector<PNode> pnodes;
    u8 hop_ctr = 0; // the hop number of the previous get peers call

    NextHop() {
        pnodes.reserve(MAX_GP_PNODES);
    };
};

// Reserves a vacant token.
std::pair<bool, u16> take_tok();

// Check whether a q_gp_ih should be pursued.
// If this function returns true, a gpm_register_q_gp_ihash call is expected.
// With the address of the first hop node.
// If the ih should be pursued, the token to use is written to the first
// argument.
// NOTE: This function does not reserve the tok in any way, or make any
// guarantees that the tok slot it has found will not be clobbered before it is
// actuall reserved using the register functions. Thus, this is not
// concurrency-safe, even in a single threaded mode. Register must be called in
// the same contiguous block of execution!
std::pair<bool, u16> decide_pursue_q_gp_ih(const bd::KRPC &);

// Register the receipt of a q_gp infohash. Due to cache pressure or other
// control parameters, this might silently drop the infohash - this is the
// GPM's concern. Subsequent calls to gpm_extract_tok will return / nothing if
// this happens.
void register_q_gp_ihash(const Nih &nid, const Nih &ih, u8 hop, u16 tok);

// Handles an r_gp message, finding the info hash it is associated with.
// If the info hash should be pursued, returns true and fills out the passed
// gpm_ih_status_t structure with the information of the next hop. Otherwise,
// returns false.
// This function deletes the entry if one is found.
bool extract_tok(NextHop &, const bd::KRPC &);

// Simply clears any cells for the given tok if they are still associated with
// the same nid.
void clear_tok(const bd::KRPC &);

i32 get_tok_hops(const bd::KRPC &);

} // namespace cht::gpm
