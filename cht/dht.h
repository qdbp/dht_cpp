#ifndef DHT_DHT_H
#define DHT_DHT_H

#include <stdint.h>
#include <string.h>

#define u8 uint8_t
#define u16 uint16_t
#define u32 uint32_t
#define u64 uint64_t

#define i8 int8_t
#define i16 int16_t
#define i32 int32_t
#define i64 int64_t

#define MIN_MSG_LEN                                                            \
    (sizeof("d1:rd2:id20:mnopqrstuvwxyz123456e1:t0:1:y1:re") - 1)

// KPRC DEFINITIONS
#define NIH_LEN 20
#define IP_LEN sizeof(u32)
#define PORT_LEN sizeof(u16)
#define PEERINFO_LEN (IP_LEN + PORT_LEN)
#define PNODE_LEN (NIH_LEN + PEERINFO_LEN)

// our key values
#define OUR_TOK "\x77"
#define OUR_TOKEN "\x88"

#define DB_FN "./data/dht.db"
#define RT_FN "./data/rt.dat"
#define RT_QUAL_FN "./data/rt_qual.dat"
#define INFO_FILE "./live_info.txt"

typedef struct nih_s {
    char raw[NIH_LEN];
} nih_t;

typedef union peerinfo_u {
    char packed[PEERINFO_LEN];
    struct {
        u32 in_addr;
        u16 sin_port;
    };
} peerinfo_t;

typedef union pnode_u {
    char raw[PNODE_LEN];
    struct {
        nih_t nid;
        peerinfo_t peerinfo;
    };
} pnode_t;

#define SET_NIH(dst, src) memcpy((dst), (src), NIH_LEN);
#define SET_PNODE(dst, src) memcpy((dst), (src), PNODE_LEN);

#endif // DHT_DHT_H
