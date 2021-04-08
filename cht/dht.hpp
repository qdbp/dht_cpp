#pragma once

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <locale>

namespace cht {

using u8 = uint8_t;
using u16 = uint16_t;
using u32 = uint32_t;
using u64 = uint64_t;

using i8 = int8_t;
using i16 = int16_t;
using i32 = int32_t;
using i64 = int64_t;
// #define u64 uint64_t

constexpr inline auto MIN_MSG_LEN =
    (sizeof("d1:rd2:id20:12345123451234512345e1:t0:1:y1:re") - 1);

// KPRC DEFINITIONS
constexpr inline auto NIH_LEN = 20;
constexpr inline auto IP_LEN = sizeof(u32);
constexpr inline auto PORT_LEN = sizeof(u16);
constexpr inline auto PEERINFO_LEN = (IP_LEN + PORT_LEN);
constexpr inline auto PNODE_LEN = (NIH_LEN + PEERINFO_LEN);

// our key values
constexpr inline u8 OUR_TOK_PG = 0x77;
constexpr inline u8 OUR_TOK_GP = 0x78;
constexpr inline u8 OUR_TOK_FN = 0x79;
constexpr inline u8 OUR_TOKEN = 0x88;

constexpr inline auto DB_FN = "./data/dht.db";

struct Nih_h {
    u8 high[2];
};

union Nih_l {
    u8 low[NIH_LEN - 2];
    struct __attribute__((packed)) {
        u8 _[NIH_LEN - 2 - sizeof(u32)];
        u32 checksum;
    };
    friend inline bool operator==(const Nih_l &x, const Nih_l &y) {
        return x.checksum == y.checksum;
    }
    friend inline bool operator!=(const Nih_l &x, const Nih_l &y) {
        return x.checksum != y.checksum;
    }
};

class Nih {

  private:
    static constexpr u32 CSUM_OFFSET = NIH_LEN - sizeof(u32);

  public:
    std::array<u8, NIH_LEN> raw;

    u8 &a() {
        return raw[0];
    }
    u8 &b() {
        return raw[1];
    }
    u32 checksum() const {
        return *reinterpret_cast<u32 *>(raw[CSUM_OFFSET]);
    }
    void clear() {
        std::fill(std::next(raw.begin() + CSUM_OFFSET), raw.end(), 0);
    };

  public:
    friend inline bool operator==(const Nih &x, const Nih &y) {
        return x.checksum() == y.checksum();
    }
    friend inline bool operator!=(const Nih &x, const Nih &y) {
        return x.checksum() != y.checksum();
    }
};

static_assert(sizeof(Nih) == 20, "Messed up Nih layout!");

union Peerinfo {
    u8 packed[PEERINFO_LEN];
    struct __attribute__((packed)) {
        u32 in_addr;
        u16 sin_port;
    };
};

static_assert(sizeof(Peerinfo) == 6, "Messed up Peerinfo layout!");

union PNode {
    u8 raw[PNODE_LEN];
    struct {
        Nih nid;
        Peerinfo peerinfo;
    };
};

static_assert(sizeof(PNode) == 26, "Messed up Pnode layout!");

inline void set_nih(u8 *dst, const u8 *src) {
    memcpy(dst, src, sizeof(Nih));
}
inline void set_pnode(u8 *dst, const u8 *src) {
    memcpy(dst, src, sizeof(PNode));
}

inline void print_msg(const u8 *buf, int len) {
    for (auto ix = 0; ix < len; ix++) {
        printf("%c", isprint(buf[ix]) ? buf[ix] : '.');
    }
    printf("\n");
    fflush(stdout);
}

} // namespace cht
