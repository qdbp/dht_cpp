#include "msg.hpp"
#include <cassert>
#include <uv.h>

#include <charconv>

using namespace cht;
namespace cht::msg {

static constexpr u8 SID_MASK[20] = {0,   0,   0,   SID4, 'Y', 'I', 'E',
                                    'L', 'D', 'T', 'O',  'T', 'H', 'E',
                                    'S', 'C', 'R', 'A',  'P', 'E'};

static constexpr u8 Q_FN_PROTO[] = {
    'd', '1', ':',        'a', 'd', '2', ':', 'i', 'd', '2', '0', ':', // 12
    0,   0,   0,          0,   0,   0,   0,   0,   0,   0,             // 22
    0,   0,   0,          0,   0,   0,   0,   0,   0,   0,             // 32
    '6', ':', 't',        'a', 'r', 'g', 'e', 't', '2', '0', ':',      // 43
    0,   0,   0,          0,   0,   0,   0,   0,   0,   0,             // 53
    0,   0,   0,          0,   0,   0,   0,   0,   0,   0,             // 63
    'e', '1', ':',        'q', '9', ':', 'f',                          // 70
    'i', 'n', 'd',        '_', 'n', 'o', 'd', 'e', '1', ':', 't',      // 80
    '1', ':', OUR_TOK_FN, '1', ':', 'y', '1', ':', 'q', 'e',           // 90
};
static constexpr i32 Q_FN_SID_OFFSET = 12;
static constexpr i32 Q_FN_TARGET_OFFSET = 43;

static constexpr u8 Q_GP_PROTO[97] = {
    'd', '1', ':', 'a', 'd', '2', ':', 'i', 'd', '2', '0',        ':', // 12
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,                    // 22
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,                    // 32
    '9', ':', 'i', 'n', 'f', 'o', '_', 'h', 'a', 's', 'h',        '2',
    '0', ':', // 46
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,          0,
    0,   0,   0,   0,   0,   0,   0,   0,                              // 66
    'e', '1', ':', 'q', '9', ':', 'g', 'e', 't', '_', 'p',        'e', // 78
    'e', 'r', 's', '1', ':', 't', '3', ':', 0,   0,   OUR_TOK_GP, '1', // 90
    ':', 'y', '1', ':', 'q', 'e',                                      // 96
};
static constexpr i32 Q_GP_SID_OFFSET = 12;
static constexpr i32 Q_GP_IH_OFFSET = 46;
static constexpr i32 Q_GP_TOK_OFFSET = 86;

static constexpr u8 Q_PG_PROTO[] = {
    'd', '1', ':', 'a',        'd', '2', ':', 'i', 'd', '2', '0', ':', // 12
    0,   0,   0,   0,          0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,          0,   0,   0,   0, // 32
    'e', '1', ':', 'q',        '4', ':', 'p', 'i', 'n', 'g', '1', ':',
    't', '1', ':', OUR_TOK_PG, '1', ':', 'y', '1', ':', 'q', 'e',
};
static constexpr u16 Q_PG_SID_OFFSET = 12;

// REPLIES, offsets from the end of the respective block
static constexpr u8 R_BASE[38] = {
    'd', '1', ':', 'y', '1', ':', 'r', '1', ':', 'r',
    'd', '2', ':', 'i', 'd', '2', '0', ':',         // 18
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0, // 28
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0, // 38
};
static constexpr i16 R_SID_OFFSET = -20;

static constexpr u8 R_NODES[36] = {
    '5', ':', 'n', 'o', 'd', 'e', 's', '2', '6', ':', // 10
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0, 0, 0,
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0, 0, 0,
};
static constexpr i16 R_NODES_OFFSET = -26;

static constexpr u8 R_TOKEN[10] = {
    '5', ':', 't', 'o', 'k', 'e', 'n', '1', ':', OUR_TOKEN,
};

template <size_t N>
static inline void append(i32 &offset, u8 *dst, const u8 src[N]) {
    memcpy(dst + offset, src, N);
    offset += N;
}

static inline void close_r_with_tok(i32 &offset, u8 *buf,
                                    const bd::KReply &krpc) {
    // close inner dict
    buf[offset++] = 'e';

    offset +=
        sprintf(reinterpret_cast<char *>(buf + offset), "1:t%u:", krpc.tok_len);
    // Can't use sprintf because
    // of possible null bytes
    memcpy(buf + offset, krpc.tok, krpc.tok_len);
    offset += krpc.tok_len;

    // close outer dict
    buf[offset++] = 'e';
}

static inline void write_sid_raw(u8 *buf) {
    for (int ix = 0; ix < NIH_LEN; ix++) {
        buf[ix] ^= SID_MASK[ix];
    }
}

static inline void write_sid(u8 *buf, const Nih &nid) {
    set_nih(buf, nid.raw._raw);
    write_sid_raw(buf);
}

i32 q_gp(u8 *buf, const Nih &nid, const Nih &infohash, u16 tok) {

    memcpy(buf, Q_GP_PROTO, sizeof(Q_GP_PROTO));

    write_sid(buf + Q_GP_SID_OFFSET, nid);
    set_nih(buf + Q_GP_IH_OFFSET, infohash.raw._raw);
    *reinterpret_cast<u16 *>(buf + Q_GP_TOK_OFFSET) = tok;

    return sizeof(Q_GP_PROTO);
}

i32 q_fn(u8 *buf, const Nih &nid, const Nih &target) {
    memcpy(buf, Q_FN_PROTO, sizeof(Q_FN_PROTO));

    set_nih(buf + Q_FN_TARGET_OFFSET, target.raw._raw);
    write_sid(buf + Q_FN_SID_OFFSET, nid);

    return sizeof(Q_FN_PROTO);
}

i32 q_pg(u8 *buf, const Nih &nid) {
    memcpy(buf, Q_PG_PROTO, sizeof(Q_PG_PROTO));
    write_sid(buf + Q_PG_SID_OFFSET, nid);

    return sizeof(Q_PG_PROTO);
}

i32 r_fn(u8 *buf, const bd::KReply &krpc, const PNode &pnode) {

    i32 offset = 0;

    append<sizeof(R_BASE)>(offset, buf, R_BASE);
    write_sid(buf + offset + R_SID_OFFSET, krpc.nid);

    append<sizeof(R_NODES)>(offset, buf, R_NODES);
    set_pnode(buf + offset + R_NODES_OFFSET, pnode.raw);

    close_r_with_tok(offset, buf, krpc);

    return offset;
}

i32 r_gp(u8 *buf, const bd::KReply &krpc, const PNode &payload) {

    i32 offset = 0;

    append<sizeof(R_BASE)>(offset, buf, R_BASE);
    write_sid(buf + offset + R_SID_OFFSET, krpc.nid);

    append<sizeof(R_NODES)>(offset, buf, R_NODES);
    set_pnode(buf + offset + R_NODES_OFFSET, payload.raw);

    append<sizeof(R_TOKEN)>(offset, buf, R_TOKEN);

    close_r_with_tok(offset, buf, krpc);

    return offset;
}

i32 r_pg(u8 *buf, const bd::KReply &krpc) {

    i32 offset = 0;

    append<sizeof(R_BASE)>(offset, buf, R_BASE);
    write_sid(buf + offset + R_SID_OFFSET, krpc.nid);

    close_r_with_tok(offset, buf, krpc);

    return offset;
}

} // namespace cht::msg
