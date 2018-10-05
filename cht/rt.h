#ifndef DHT_RT_H
#define DHT_RT_H

#include "bdecode.h"
#include "dht.h"
#include <netinet/ip.h>
#include <stdbool.h>

/// If this is set, the routing table is half a GiB, which is very
/// cache-unfriendly This can be considered experimental (even though it was the
/// first to be used).
#ifdef RT_BIG
#define RT_TOTAL_CONTACTS (256 * 256 * 256)
#define RT_FN "./data/rt3.dat"
#else
#define RT_TOTAL_CONTACTS (256 * 256)
#define RT_FN "./data/rt.dat"
#endif

#define RT_Q_WIDTH 3
#define RT_MAX_Q                                                               \
    ((1 << RT_Q_WIDTH) - 1) // check quality bitwidth in rt_nodeinfo_t
#define CLIP_Q(qual) ((qual) > RT_MAX_Q ? RT_MAX_Q : (qual))

typedef struct rt_nodeinfo_t {
    _Alignas(32) pnode_t pnode; // 20
    u8 quality : RT_Q_WIDTH;    // 27
    // 5 bytes FREE
} rt_nodeinfo_t;

_Static_assert(sizeof(rt_nodeinfo_t) == 32, "Bad nodeinfo size");

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

void rt_init(void);

bool validate_addr(u32, u16);
bool validate_nodeinfo(const rt_nodeinfo_t *);

void rt_insert_contact(const parsed_msg *, const struct sockaddr_in *, u8);
rt_nodeinfo_t *rt_get_neighbor_contact(const nih_t nid);
void rt_adj_quality(const nih_t, i64);

#endif // DHT_RT_H
