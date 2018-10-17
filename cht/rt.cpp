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
#include <unistd.h>

#include <array>

// STATIC FUNCTIONS

using namespace cht;
using bd::KRPC;
namespace cht::rt {

RT::Nodeinfo *RT::load_rt(void) {

    int fd = open(RT_FN, O_RDWR | O_CREAT);

    if (fd == -1) {
        ERROR("Could not open rt file, bailing.");
        exit(-1);
    }

    struct stat info = {0};
    if (fstat(fd, &info)) {
        ERROR("Could not stat rt file: %s, bailing.", strerror(errno))
        exit(-1);
    }

    if (info.st_size != RT_SIZE) {
        WARN("Bad size (%ld) rt file found.", info.st_size)
        WARN("Will be truncating this file!")
        if (ftruncate(fd, RT_SIZE)) {
            ERROR("Could not truncate file: %s, bailing", strerror(errno))
            exit(-1);
        }
    }

    void *addr = mmap(NULL, RT_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);

    if (addr == (void *)-1) {
        ERROR("Failed to mmap rt file: %s.", strerror(errno));
        exit(-1);
    }
    close(fd);
    INFO("Memmaped rt of size %lu * %lu", sizeof(Nodeinfo),
         RT_SIZE / sizeof(Nodeinfo))
    return static_cast<Nodeinfo *>(addr);
}

inline RT::Nodeinfo *RT::get_cell(const Nih &nid) const {
    return get_cell(nid.a, nid.b);
}

inline RT::Nodeinfo *RT::get_cell(u8 a, u8 b) const {
    return __rt + 256 * a + b;
}

inline void RT::set_cell(const Nih &nid, const SIN &addr, u8 qual) {

    Nodeinfo &cell = *get_cell(nid);

    cell.pnode.nid = nid;
    // These are all in network byte order
    cell.pnode.peerinfo.sin_port = addr.sin_port;
    cell.pnode.peerinfo.in_addr = addr.sin_addr.s_addr;
    cell.quality = CLIP_Q(qual);
}

void RT::insert_contact(const KRPC &krpc, const SIN &addr, u8 base_qual) {

    if (!validate_addr(addr.sin_addr.s_addr, addr.sin_port)) {
        st_inc(ST_rt_replace_invalid);
    }

    Nodeinfo &node_spot = *get_cell(*krpc.nid);

    if (!RT::check_evict(node_spot.quality, base_qual)) {
        st_inc(ST_rt_replace_reject);
    }

    set_cell(*krpc.nid, addr, base_qual);
    st_inc(ST_rt_replace_accept);
}

void RT::adj_quality(const Nih &nid, i64 delta) {
    /*
    Adjusts the quality of the routing contact "nid", if it
    can be found. Otherwise, does nothing.
    */

    Nodeinfo &cell = *get_cell(nid);

    // the contact we're trying to adjust has been replaced!
    // just do nothing in this case
    if (cell.pnode.nid != nid) {
        return;
    }

    cell.quality = CLIP_Q(cell.quality + delta);
}

void RT::delete_node(const Nih &target) {
    /*
    A node has been very naughty. It must be annihilated!
    */

    Nodeinfo &cell = *get_cell(target);
    // check the node hasn't been replaced in the interim
    if (cell.pnode.nid == target) {
        cell.pnode.nid.checksum = 0;
    }
}

const PNode &RT::get_neighbor_contact(const Nih &target) const {
    /*
    Returns a nid from the array of nids `narr` whose first two bytes
    match the target.
    */

    Nodeinfo &out_cell = *get_cell(target);

    if (out_cell.is_empty()) {
        st_inc(ST_rt_miss);
        return get_random_valid_node();
    }

    return out_cell.pnode;
}

const PNode &RT::get_random_valid_node(void) const {
    /*
        Returns a random non-zero, valid node from the current routing
       table.

        Is much slower when the table is sparse. Returns None if it can
        find no node at all.
        */

    u32 start = randint(0, RT_SIZE);
    u32 end = RT_SIZE;

    while (start > 0) {
        for (int ix = start; ix < end; ix++) {
            u8 ax = ix >> 8;
            u8 bx = ix & 0xff;

            Nodeinfo &out = *get_cell(ax, bx);

            if (!out.is_empty()) {
                return out.pnode;
            }
        }
        end = start;
        start = 0;
    }

    st_inc(ST_err_rt_no_contacts);
    ERROR("Could not find any random valid contact. RT in trouble!")
    return get_cell(0, 0)->pnode;
}

bool validate_addr(u32 in_addr, u16 sin_port) {

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

RT &g_rt = RT::getinstance();

} // namespace cht::rt
