#include "dht.h"
#include "log.h"
#include "msg.h"
#include <assert.h>
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

#define R_SID_OFFSET 12

#define APPEND(buf, offset, strlit)                                            \
    memcpy(buf + offset, strlit, sizeof(strlit) - 1);                          \
    offset += sizeof(strlit) - 1;

inline static void write_sid_raw(char *restrict buf) {
    for (int ix = 0; ix < NIH_LEN; ix++) {
        buf[ix] ^= SID_MASK[ix];
    }
}

inline void write_sid(char *restrict buf, const nih_t nid) {
    SET_NIH(buf, nid.raw);
    write_sid_raw(buf);
}

u32 msg_q_gp(char *restrict buf, nih_t nid, nih_t infohash, u16 tok) {

    memcpy(buf, Q_GP_PROTO, Q_GP_LEN);
    write_sid(buf + Q_GP_SID_OFFSET, nid);
    SET_NIH(buf + Q_GP_IH_OFFSET, infohash.raw);
    *(u16 *)(buf + Q_GP_TOK_OFFSET) = tok;

    return Q_GP_LEN;
}

u32 msg_q_fn(char *restrict buf, const pnode_t dest, const nih_t target) {
    memcpy(buf, Q_FN_PROTO, Q_FN_LEN);
    SET_NIH(buf + Q_FN_TARGET_OFFSET, target.raw);
    write_sid(buf + Q_FN_SID_OFFSET, dest.nid);

    return Q_FN_LEN;
}

u32 msg_q_pg(char *restrict buf, const nih_t nid) {
    memcpy(buf, Q_PG_PROTO, Q_PG_LEN);
    write_sid(buf + Q_PG_SID_OFFSET, nid);

    return Q_PG_LEN;
}

u32 msg_r_fn(char *restrict buf, const parsed_msg *rcvd, pnode_t pnode) {
    u32 len = sprintf(buf, "d1:rd2:id20:%.*s5:nodes26:%.*se1:t%u:%.*s1:y1:re",
                      20, rcvd->nid.raw, 26, pnode.raw, rcvd->tok_len,
                      rcvd->tok_len, rcvd->tok);

    write_sid_raw(buf + R_SID_OFFSET);
    return len;
}

u32 msg_r_gp(char *restrict buf, const parsed_msg *rcvd, const pnode_t pnode) {
    u32 len = sprintf(buf,
                      "d1:rd2:id20:%.*s5:token1:" OUR_TOKEN
                      "5:nodes26:%.*se1:t%u:%.*s1:y1:re",
                      20, rcvd->nid.raw, 26, pnode.raw, rcvd->tok_len,
                      rcvd->tok_len, rcvd->tok);

    write_sid_raw(buf + R_SID_OFFSET);
    return len;
}

u32 msg_r_pg(char *restrict buf, const parsed_msg *rcvd) {
    u32 len = sprintf(buf, "d1:rd2:id20:%.*se1:t%u:%.*s1:y1:re", 20,
                      rcvd->nid.raw, rcvd->tok_len, rcvd->tok_len, rcvd->tok);

    write_sid_raw(buf + R_SID_OFFSET);

    return len;
}
