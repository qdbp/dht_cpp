#pragma once

#include <locale>
#include <stdint.h>

namespace cht {

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
constexpr u8 OUR_TOK_PG = 0x77;
constexpr u8 OUR_TOK_GP = 0x78;
constexpr u8 OUR_TOK_FN = 0x79;
constexpr u8 OUR_TOKEN = 0x88;

#define DB_FN "./data/dht.db"

union Nih {
    u8 raw[NIH_LEN];
    struct __attribute__((packed)) {
        u8 a;
        u8 b;
        u8 c;
        u8 _[7];
        u8 ctl_byte;
        u8 __[5];
        u32 checksum;
    };
};

bool operator==(const Nih &x, const Nih &y);
bool operator!=(const Nih &x, const Nih &y);

static_assert(sizeof(Nih) == NIH_LEN, "Messed up Nih layout!");

union Peerinfo {
    char packed[PEERINFO_LEN];
    struct __attribute__((packed)) {
        u32 in_addr;
        u16 sin_port;
    };
};

static_assert(sizeof(Peerinfo) == PEERINFO_LEN, "Messed up Peerinfo layout!");

typedef union pnode_u {
    char raw[PNODE_LEN];
    struct __attribute__((packed)) {
        Nih nid;
        Peerinfo peerinfo;
    };
} PNode;

static_assert(sizeof(PNode) == PNODE_LEN, "Messed up Peerinfo layout!");

#define SET_NIH(dst, src) memcpy((dst), (src), NIH_LEN);
#define SET_PNODE(dst, src) memcpy((dst), (src), PNODE_LEN);

#define PRINT_MSG(buf, len)                                                    \
    for (auto ix = 0; ix < len; ix++) {                                        \
        printf("%c", isprint(buf[ix]) ? buf[ix] : '.');                        \
    }                                                                          \
    printf("\n");                                                              \
    fflush(stdout);

} // namespace cht
