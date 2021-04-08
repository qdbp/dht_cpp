// vi:ft=cpp
#pragma once
#include "dht.hpp"
#include "krpc.hpp"
#include "util.hpp"
#include <netinet/ip.h>

using namespace cht;
using bd::KRPC;
using SIN = struct sockaddr_in;

namespace cht::rt {

/// If this is set, the routing table is half a GiB, which is very
/// cache-unfriendly This can be considered experimental (even though it was the
/// first to be used).
#define RT_FN "./data/rt.dat"

#define RT_Q_WIDTH 3
#define RT_MAX_Q ((1 << RT_Q_WIDTH) - 1) // check quality bitwidth in Nodeinfo
#define CLIP_Q(qual) ((qual) > RT_MAX_Q ? RT_MAX_Q : (qual))

#define AS_SOCKADDR_IN(node_ptr)                                               \
    {                                                                          \
        .sin_family = AF_INET, .sin_port = (node_ptr)->sin_port,               \
        .sin_addr.s_addr = (node_ptr)->in_addr                                 \
    }

#define PNODE_AS_SOCKADDR_IN(pnode_ptr)                                        \
    {                                                                          \
        .sin_family = AF_INET,                                                 \
        .sin_port = *(u16 *)((pnode_ptr) + NIH_LEN + IP_LEN),                  \
        .sin_addr.s_addr = *(u32 *)((pnode_ptr) + NIH_LEN)                     \
    }

void init();

bool validate_addr(u32, u16);

class RT {
  private:
    struct Nodeinfo {
        Nih_l nih_l;
        Peerinfo peerinfo;

      public:
        bool is_empty() {
            return nih_l.checksum == 0;
        }
    };
    static_assert(sizeof(Nodeinfo) == 24, "Bad nodeinfo size");

    Nodeinfo *__rt;
    static constexpr u32 RT_SIZE = sizeof(Nodeinfo) * 256 * 256;

    RT() : __rt(load_rt()) {
    }

    static inline bool check_evict(u8 cur_qual, u8 cand_qual) {
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

    Nodeinfo *load_rt();
    Nodeinfo *get_cell(const Nih &nid) const;
    Nodeinfo *get_cell(u8 a, u8 b) const;
    void set_cell(const Nih &nid, const SIN &addr, u8 qual);
    void set_cell(const Nih &nid, u32 in_addr, u16 sin_port, u8 qual);

  public:
    static constexpr PNode bootstrap_node = {
        .nid = {.raw =
                    {
                        '2',  0xf5, 'N', 'i',  's',  'Q',  0xff,
                        'J',  0xec, ')', 0xcd, 0xba, 0xab, 0xf2,
                        0xfb, 0xe3, 'F', '|',  0xc2, 'g',
                    }},
        .peerinfo.in_addr = 183949123,
        // the "reverse" of 6881
        .peerinfo.sin_port = 57626,
    };

  public:
    static RT &getinstance() {
        static RT rt;
        return rt;
    }

    RT(RT const &) = delete;
    RT &operator=(RT const &) = delete;

    void insert_contact(const KRPC &krpc, const SIN &addr, u8 base_qual);
    void insert_contact(const KRPC &krpc, u32 in_addr, u16 sin_port,
                        u8 base_qual);
    void adj_quality(const Nih &nid, i64 delta);
    void delete_node(const Nih &target);

    const PNode get_neighbor_contact(const Nih &target) const;
    const PNode get_random_valid_node() const;
};

bool validate_addr(u32 in_addr, u16 sin_port);
extern RT &g_rt;

} // namespace cht::rt
