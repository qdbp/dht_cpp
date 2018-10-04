#ifndef DHT_RT_H
#define DHT_RT_H

#include "bdecode.h"
#include "dht.h"
#include <netinet/ip.h>
#include <stdbool.h>

#define RT_TOTAL_CONTACTS (256 * 256 * 256)
#define RT_Q_WIDTH 3
#define RT_MAX_Q                                                               \
    ((1 << RT_Q_WIDTH) - 1) // check quality bitwidth in rt_nodeinfo_t
#define CLIP_Q(qual) ((qual) > RT_MAX_Q ? RT_MAX_Q : (qual))

typedef struct rt_nodeinfo_t {
    _Alignas(32) char nid[20]; // 20
    u32 in_addr;               // 24, network order
    u16 sin_port;              // 26, network order
    u8 quality : RT_Q_WIDTH;   // 27
    // 5 bytes FREE
} rt_nodeinfo_t;

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

void write_nodeinfo(char *, const rt_nodeinfo_t *);

stat_t rt_random_replace_contact(const char *, u8);
stat_t rt_add_sender_as_contact(const parsed_msg *, const struct sockaddr_in *,
                                u8);

rt_nodeinfo_t *rt_get_valid_neighbor_contact(const char *);
rt_nodeinfo_t *rt_get_cell(const char *);
rt_nodeinfo_t *rt_get_cell_by_coords(u8, u8, u8);

void rt_adj_quality(const char *, i64);
bool rt_check_evict(u8, u8);

#endif // DHT_RT_H
