#include "dht.h"
#include "msg.h"
#include <uv.h>

static const char SID_MASK[20] = "\0\0\0SUBMITTOTHESCRAPE";

const char Q_FN_PROTO[] = "d1:ad2:id20:"
                          "\x0\x0\x0\x0\x0"
                          "\x0\x0\x0\x0\x0"
                          "\x0\x0\x0\x0\x0"
                          "\x0\x0\x0\x0\x0"
                          "6:target20:"
                          "\x0\x0\x0\x0\x0"
                          "\x0\x0\x0\x0\x0"
                          "\x0\x0\x0\x0\x0"
                          "\x0\x0\x0\x0\x0"
                          "e1:q9:find_node1:t1:" OUR_TOK "1:y1:qe";

#define Q_FN_LEN (sizeof(Q_FN_PROTO) - 1)
// #define Q_FN_LEN 12 + 20 + 11 + 20 + 28
#define Q_FN_SID_OFFSET 12
#define Q_FN_TARGET_OFFSET 43

const char Q_GP_PROTO[] = "d1:ad2:id20:"
                          "\x0\x0\x0\x0\x0"
                          "\x0\x0\x0\x0\x0"
                          "\x0\x0\x0\x0\x0"
                          "\x0\x0\x0\x0\x0"
                          "9:info_hash20:"
                          "\x0\x0\x0\x0\x0"
                          "\x0\x0\x0\x0\x0"
                          "\x0\x0\x0\x0\x0"
                          "\x0\x0\x0\x0\x0"
                          "e1:q9:get_peers1:t3:"
                          "\x0\x0" OUR_TOK "1:y1:qe";

#define Q_GP_LEN (sizeof(Q_GP_PROTO) - 1)
#define Q_GP_SID_OFFSET 12
#define Q_GP_IH_OFFSET 46
#define Q_GP_TOK_OFFSET 86

const char Q_PG_PROTO[] = "d1:ad2:id20:"
                          "\x0\x0\x0\x0\x0"
                          "\x0\x0\x0\x0\x0"
                          "\x0\x0\x0\x0\x0"
                          "\x0\x0\x0\x0\x0"
                          "e1:q4:ping1:t1:" OUR_TOK "1:y1:qe";

#define Q_PG_LEN (sizeof(Q_PG_PROTO) - 1)
#define Q_PG_SID_OFFSET 12

#define APPEND(buf, offset, strlit)                                            \
    memcpy(buf + offset, strlit, sizeof(strlit) - 1);                          \
    offset += sizeof(strlit) - 1;

inline void write_sid(char *restrict buf, const nih_t nid) {
    SET_NIH(buf, nid.raw);
    for (int ix = 0; ix < 20; ix++) {
        buf[ix] ^= SID_MASK[ix];
    }
}

u64 msg_q_gp(char *restrict buf, nih_t nid, nih_t infohash, u16 tok) {

    memcpy(buf, Q_GP_PROTO, Q_GP_LEN);
    write_sid(buf + Q_GP_SID_OFFSET, nid);
    SET_NIH(buf + Q_GP_IH_OFFSET, infohash.raw);
    *(u16 *)(buf + Q_GP_TOK_OFFSET) = tok;

    return Q_GP_LEN;
}

u64 msg_q_fn(char *restrict buf, const pnode_t dest, const nih_t target) {
    memcpy(buf, Q_FN_PROTO, Q_FN_LEN);
    SET_NIH(buf + Q_FN_TARGET_OFFSET, target.raw);
    write_sid(buf + Q_FN_SID_OFFSET, dest.nid);

    return Q_FN_LEN;
}

u64 msg_q_pg(char *restrict buf, const nih_t nid) {
    memcpy(buf, Q_PG_PROTO, Q_PG_LEN);
    write_sid(buf + Q_PG_SID_OFFSET, nid);

    return Q_PG_LEN;
}

u64 msg_r_fn(char *restrict buf, const parsed_msg *rcvd, pnode_t pnode) {
    u64 offset = 0;

    APPEND(buf, offset, "d1:rd2:id20:")

    write_sid(buf + offset, rcvd->nid);
    offset += NIH_LEN;

    APPEND(buf, offset, "5:nodes26:")

    SET_PNODE(buf + offset, pnode.raw);
    offset += PNODE_LEN;

    APPEND(buf, offset, "e1:t")

    memcpy(buf + offset, rcvd->tok, rcvd->tok_len);
    offset += rcvd->tok_len;

    APPEND(buf, offset, "1:y1:re")

    return offset;
}

u64 msg_r_gp(char *restrict buf, const parsed_msg *rcvd, const pnode_t pnode) {
    u64 offset = 0;

    APPEND(buf, offset, "d1:rdl:id20:")

    write_sid(buf, rcvd->nid);
    offset += NIH_LEN;

    APPEND(buf, offset, "5:token1:" OUR_TOKEN "5:nodes26:")

    SET_PNODE(buf + offset, pnode.raw);
    offset += PNODE_LEN;

    APPEND(buf, offset, "e1:t")

    memcpy(buf + offset, rcvd->tok, rcvd->tok_len);
    offset += rcvd->tok_len;

    APPEND(buf, offset, "1:y1:re")

    return offset;
}

u64 msg_r_pg(char *restrict buf, const parsed_msg *rcvd) {
    u64 offset = 0;

    APPEND(offset, buf, "d1:rd2:id20:")

    write_sid(buf + offset, rcvd->nid);
    offset += NIH_LEN;

    APPEND(offset, buf, "e1:t")

    memcpy(buf + offset, rcvd->tok, rcvd->tok_len);
    offset += rcvd->tok_len;

    APPEND(buf, offset, "1:y1:re");

    return offset;
}
