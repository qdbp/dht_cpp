#include "log.h"
#include "rt.h"
#include "stat.h"
#include "util.h"
#include <endian.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/ip.h>
#include <stdio.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>

static rt_nodeinfo_t *g_rt = NULL;
static int g_dkad_offsets[256] = {0};

inline bool is_pnode_empty(const pnode_t pnode) {
    return 0 == *(u64 *)pnode.nid.raw;
}

inline static bool eq_nodeinfo_nid(const rt_nodeinfo_t *node1,
                                   const nih_t other_nid) {
    return (0 == memcmp(node1->pnode.nid.raw, other_nid.raw, NIH_LEN));
}

inline bool validate_addr(u32 in_addr, u16 sin_port) {

    // in_addr and sin_port are big_endian
    char *addr_bytes = (char *)&in_addr;
    unsigned char a = addr_bytes[0];
    unsigned char b = addr_bytes[1];
    unsigned char c = addr_bytes[2];
    unsigned char d = addr_bytes[3];

    if (be16toh(sin_port) <= 1024) {
        return false;
    }

    if (((a & 0xf0) == 240) || (a == 0) || (a == 10) || (a == 127) ||
        (a == 100 && (b & 0xc0) == 64) || (a == 172 && (b & 0xf0) == 16) ||
        (a == 198 && (b & 0xfe) == 18) || (a == 169 && b == 254) ||
        (a == 192 && b == 168) || (a == 192 && b == 0 && c == 0) ||
        (a == 192 && b == 0 && c == 2) || (a == 192 && b == 31 && c == 196) ||
        (a == 192 && b == 51 && c == 100) ||
        (a == 192 && b == 52 && c == 193) ||
        (a == 192 && b == 175 && c == 48) ||
        (a == 198 && b == 51 && c == 100) || (a == 203 && b == 0 && c == 113) ||
        (a == 255 && b == 255 && c == 255 && d == 255)) {
        return false;
    }

    return true;
}

inline bool validate_pnode(const pnode_t pnode) {
    return (!is_pnode_empty(pnode)) &&
           validate_addr(pnode.peerinfo.in_addr, pnode.peerinfo.sin_port);
}

static void load_rt() {
    int fd = open(RT_FN, O_RDWR);
    if (fd == -1) {
        ERROR("Could not open rt file, bailing.");
        exit(-1);
    }

    struct stat info = {0};
    if (0 != fstat(fd, &info)) {
        ERROR("Could not stat rt file, bailing.")
        exit(-1);
    }

    u64 want_size = RT_TOTAL_CONTACTS * sizeof(rt_nodeinfo_t);

    if (info.st_size != want_size) {
        ERROR("Bad size (%ld) rt file found.", info.st_size);
        ERROR("If upgrading to a new rt format, you must migrate it manually")
        exit(-1);
    }

    void *addr =
        mmap(NULL, want_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);

    if (addr == (void *)-1) {
        ERROR("Failed to mmap rt file: %s.", strerror(errno));
        exit(-1);
    }
    g_rt = (rt_nodeinfo_t *)addr;
}

inline rt_nodeinfo_t *rt_get_cell(const nih_t nid) {
    /*
    Returns the index in the routing table corresponding to the given
    nid. Obviously not injective.

    ASSUMES 8 CONTACTS AT DEPTH 3
    */
    u8 a, b, c;
    a = nid.raw[0];
    b = nid.raw[1];
    c = nid.raw[2];

    return rt_get_cell_by_coords(a, b, c);
}

inline rt_nodeinfo_t *rt_get_cell_by_coords(u8 a, u8 b, u8 c) {
    return g_rt + ((u32)a << 16) + ((u32)b << 8) + c;
}

inline static void rt__set_from_addr(rt_nodeinfo_t *cell, const nih_t nid,
                                     const struct sockaddr_in *addr, u8 qual) {
    SET_NIH(cell->pnode.nid.raw, nid.raw);
    // These are all in network byte order
    cell->pnode.peerinfo.sin_port = addr->sin_port;
    cell->pnode.peerinfo.in_addr = addr->sin_addr.s_addr;
    cell->quality = CLIP_Q(qual);
}

// inline static void rt__set_from_pnode(rt_nodeinfo_t *cell, const char *pnode,
//                                       u8 qual) {
//     // Relies on no weird padding!
//     SET_PNODE(cell->nid, pnode);
//     cell->quality = CLIP_Q(qual);
// }

stat_t rt_add_sender_as_contact(const parsed_msg *krpc,
                                const struct sockaddr_in *addr, u8 base_qual) {

    if (!validate_addr(addr->sin_addr.s_addr, addr->sin_port)) {
        st_inc(ST_rt_replace_invalid);
        return ST_rt_replace_invalid;
    }

    rt_nodeinfo_t *node_spot = rt_get_cell(krpc->nid);

    if (!rt_check_evict(node_spot->quality, base_qual)) {
        return ST_rt_replace_reject;
    }

    rt__set_from_addr(node_spot, krpc->nid, addr, base_qual);
    return ST_rt_replace_accept;
}

stat_t rt_random_replace_contact(const pnode_t new_pnode, u8 base_qual) {
    /*
    Possibly randomly replaces the contact for `new_pnode` in the routing
    table.

    The old contact has a chance to be evicted inversely related to
    its quality given by the quality table.

    If no contact is evicted, the new_contact is simply ignored.
    */

    rt_nodeinfo_t *node_spot = rt_get_cell(new_pnode.nid);

    if (rt_check_evict(node_spot->quality, base_qual)) {
        // Assumes the relevant fields are contiguous in memory
        if (validate_pnode(new_pnode)) {
            node_spot->pnode = new_pnode;
            node_spot->quality = CLIP_Q(base_qual);
            return ST_rt_replace_accept;
        } else {
            return ST_rt_replace_invalid;
        }
    } else {
        return ST_rt_replace_reject;
    }
}

bool rt_check_evict(u8 cur_qual, u8 cand_qual) {
    /*
    Checks if a node with quality `cur_qual` should be replaced with
    one of quality `cand_qual`.

    If `cand_qual` > `cur_qual`, evicts certainly. Else, evicts with
    probability 1 / 2 ** (cur_qual - cand_qual)
    */

    if (cand_qual >= cur_qual) {
        return true;
    }

    return randint(0, 1 << (cur_qual - cand_qual)) == 0;
}

void rt_adj_quality(const nih_t nid, i64 delta) {
    /*
    Adjusts the quality of the routing contact "nid", if it
    can be found. Otherwise, does nothing.
    */

    rt_nodeinfo_t *cur_nodeinfo = rt_get_cell(nid);

    // the contact we're trying to adjust has been replaced!
    // just do nothing in this case
    if (!eq_nodeinfo_nid(cur_nodeinfo, nid)) {
        return;
    }

    u8 new_qual = cur_nodeinfo->quality + delta;
    cur_nodeinfo->quality = CLIP_Q(new_qual);
}

rt_nodeinfo_t *rt_get_valid_neighbor_contact(const nih_t target) {
    /*
    Returns a nid from the array of nids `narr` whose first two bytes
    match the target.
    */

    rt_nodeinfo_t *neighbor_cell = rt_get_cell(target);

    if (!is_pnode_empty(neighbor_cell->pnode)) {
        return neighbor_cell;
    }

    rt_nodeinfo_t *alt_cell;

    // try one neighbor cell, then fail
    if (g_dkad_offsets[(int)target.raw[2]] > 0) {
        alt_cell = neighbor_cell + 1;
    } else {
        alt_cell = neighbor_cell - 1;
    }

    if (!is_pnode_empty(alt_cell->pnode)) {
        return alt_cell;
    } else {
        st_inc(ST_rt_miss);
        return NULL;
    }
}

void rt_nuke_node(const nih_t target) {
    /*
    A node has been very naughty. It must be annihilated!
    */

    rt_nodeinfo_t *cell = rt_get_cell(target);
    // check the node hasn't been replaced in the interim
    if (0 == memcmp(cell, target.raw, NIH_LEN)) {
        memset(cell, 0, sizeof(rt_nodeinfo_t));
    }
}

rt_nodeinfo_t *rt_get_random_valid_node() {
    /*
        Returns a random non-zero, valid node from the current routing table.

        Is much slower when the table is empty. Returns None if it can
        find no node at all.
        */

    u32 start = randint(0, RT_TOTAL_CONTACTS);
    u32 end = RT_TOTAL_CONTACTS;

    while (start > 0) {
        for (int ix = start; ix < end; ix++) {
            u8 ax = ix >> 16;
            u8 bx = (ix >> 8) & 0xff;
            u8 cx = ix & 0xff;

            rt_nodeinfo_t *out = rt_get_cell_by_coords(ax, bx, cx);

            if (!is_pnode_empty(out->pnode)) {
                return out;
            }
        }
        end = start;
        start = 0;
    }

    st_inc(ST_err_rt_no_contacts);
    ERROR("Could not find any random valid contact. RT in trouble!")
    return NULL;
}

void rt_init(void) {
    load_rt();
    g_dkad_offsets[0] = 1;
    g_dkad_offsets[255] = -1;
    for (int i = 1; i < 255; i += 1) {
        int xm = (i - 1) ^ i;
        int xp = (i + 1) ^ i;
        g_dkad_offsets[i] = (xm < xp) ? -1 : 1;
    }
}
