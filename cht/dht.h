// vi:ft=c
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
    (sizeof("d1:rd2:id20:12345123451234512345e1:t0:1:y1:re") - 1)

// KPRC DEFINITIONS
#define NIH_LEN 20
#define IP_LEN sizeof(u32)
#define PORT_LEN sizeof(u16)
#define PEERINFO_LEN (IP_LEN + PORT_LEN)
#define PNODE_LEN (NIH_LEN + PEERINFO_LEN)

// our key values
#define OUR_TOK_PG "\x77"
#define OUR_TOK_GP "\x78"
#define OUR_TOK_FN "\x79"
#define OUR_TOKEN "\x88"

#define DB_FN "./data/dht.db"

typedef union nih_u {
    char raw[NIH_LEN];
    struct {
        u8 a;
        u8 b;
        u8 c;
        char _[7];
        u8 ctl_byte;
        char __[5];
        u32 checksum;
    };
} nih_t;

_Static_assert(sizeof(nih_t) == NIH_LEN, "Messed up nih_t layout!");

typedef union peerinfo_u {
    char packed[PEERINFO_LEN];
    struct __attribute__((packed)) {
        u32 in_addr;
        u16 sin_port;
    };
} peerinfo_t;

_Static_assert(sizeof(peerinfo_t) == PEERINFO_LEN,
               "Messed up peerinfo_t layout!");

typedef union pnode_u {
    char raw[PNODE_LEN];
    struct __attribute__((packed)) {
        nih_t nid;
        peerinfo_t peerinfo;
    };
} pnode_t;

_Static_assert(sizeof(pnode_t) == PNODE_LEN, "Messed up peerinfo_t layout!");

#define SET_NIH(dst, src) memcpy((dst), (src), NIH_LEN);
#define SET_PNODE(dst, src) memcpy((dst), (src), PNODE_LEN);

#endif // DHT_DHT_H
