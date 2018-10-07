// vi:ft=c
#ifndef DHT_GPM_H
#define DHT_GPM_H

// The GetPeersManager engine

// number of nodes to pursue from each get_peers nodes array
#define MAX_GP_PNODES 2
// dkads lower than this are overwhelmingly improbable, so we ignore any
// nodes like this.
#define BULLSHIT_DKAD 100
// maximum number of chained nodes to query when chasing down an ihash before
// giving up
#ifndef GP_MAX_HOPS
#define GP_MAX_HOPS 8
#endif

#include "bdecode.h"
#include <stdbool.h>

typedef struct gp_next_hop_s {
    nih_t ih;
    pnode_t pnodes[MAX_GP_PNODES];
    u8 n_pnodes; // the actual number of pnodes could be less than the max
    u8 hop_ctr;  // the hop number of the previous get peers call
} gpm_next_hop_t;

/// Get a vacant token. Does not reserve it.
bool get_vacant_tok(u16 *dst);

/// Check whether a q_gp_ih should be pursued.
/// If this function returns true, a gpm_register_q_gp_ihash call is expected.
/// With the address of the first hop node.
/// If the ih should be pursued, the token to use is written to the first
/// argument.
/// NOTE: This function does not reserve the tok in any way, or make any
/// guarantees that the tok slot it has found will not be clobbered before it is
/// actuall reserved using the register functions. Thus, this is not
/// concurrency-safe, even in a single threaded mode. Register must be called in
/// the same contiguous block of execution!
bool gpm_decide_pursue_q_gp_ih(u16 *restrict, const parsed_msg *);

/// Register the receipt of a q_gp infohash. Due to cache pressure or other
/// control parameters, this might silently drop the infohash - this is the
/// GPM's concern, and subsequent calls to gpm_extract_tok will return
/// nothing if this happens, ending the pursuit.
void gpm_register_q_gp_ihash(const nih_t nid, const nih_t ih, u8 hop, u16 tok);

/// Handles an r_gp message, finding the info hash it is associated with.
/// If the info hash should be pursued, returns true and fills out the passed
/// gpm_ih_status_t structure with the information of the next hop. Otherwise,
/// returns false.
/// As the name suggests, this function deletes the entry if one is found.
bool gpm_extract_tok(gpm_next_hop_t *restrict, const parsed_msg *krpc);

/// Simply clears any cells for the given tok if they are still associated with
/// the same nid.
void gpm_clear_tok(const parsed_msg *krpc);

i32 gpm_get_tok_hops(const parsed_msg *krpc);

#endif
