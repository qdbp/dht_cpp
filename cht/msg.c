#include "dht.h"
#include "log.h"
#include "msg.h"
#include <assert.h>
#include <uv.h>

static const char SID_MASK[20] = "\0\0\0" SID4 "YIELDTOTHESCRAPE";

static const char Q_FN_PROTO[] = "d1:ad2:id20:"
                                 "\x0\x0\x0\x0\x0"
                                 "\x0\x0\x0\x0\x0"
                                 "\x0\x0\x0\x0\x0"
                                 "\x0\x0\x0\x0\x0"
                                 "6:target20:"
                                 "\x0\x0\x0\x0\x0"
                                 "\x0\x0\x0\x0\x0"
                                 "\x0\x0\x0\x0\x0"
                                 "\x0\x0\x0\x0\x0"
                                 "e1:q9:find_node1:t1:" OUR_TOK_FN "1:y1:qe";

#define Q_FN_LEN (sizeof(Q_FN_PROTO) - 1)
#define Q_FN_SID_OFFSET 12
#define Q_FN_TARGET_OFFSET 43

static const char Q_GP_PROTO[] = "d1:ad2:id20:"
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
                                 "\x0\x0" OUR_TOK_GP "1:y1:qe";

#define Q_GP_LEN (sizeof(Q_GP_PROTO) - 1)
#define Q_GP_SID_OFFSET 12
#define Q_GP_IH_OFFSET 46
#define Q_GP_TOK_OFFSET 86

static const char Q_PG_PROTO[] = "d1:ad2:id20:"
                                 "\x0\x0\x0\x0\x0"
                                 "\x0\x0\x0\x0\x0"
                                 "\x0\x0\x0\x0\x0"
                                 "\x0\x0\x0\x0\x0"
                                 "e1:q4:ping1:t1:" OUR_TOK_PG "1:y1:qe";

#define Q_PG_LEN (sizeof(Q_PG_PROTO) - 1)
#define Q_PG_SID_OFFSET 12

#define R_SID_OFFSET 12
static const char R_PG_PROTO_BASE[] = "d1:rd2:id20:"
                                      "\x0\x0\x0\x0\x0"
                                      "\x0\x0\x0\x0\x0"
                                      "\x0\x0\x0\x0\x0"
                                      "\x0\x0\x0\x0\x0"
                                      "e1:t";
#define R_PG_BASE_LEN (sizeof(R_PG_PROTO_BASE) - 1)

static const char R_FN_PROTO_BASE[] = "d1:rd2:id20:" /* 12 */
                                      "\x0\x0\x0\x0\x0"
                                      "\x0\x0\x0\x0\x0"
                                      "\x0\x0\x0\x0\x0"
                                      "\x0\x0\x0\x0\x0" /* 32 */
                                      "5:nodes26:"      /* 42 */
                                      "\x0\x0\x0\x0\x0\x0\x0\x0\x0\x0\x0\x0\x0"
                                      "\x0\x0\x0\x0\x0\x0\x0\x0\x0\x0\x0\x0\x0"
                                      "e1:t";

#define R_FN_BASE_LEN (sizeof(R_FN_PROTO_BASE) - 1)
#define R_FN_PNODE_OFFSET 42

static const char R_GP_PROTO_BASE[] = "d1:rd2:id20:"
                                      "\x0\x0\x0\x0\x0"
                                      "\x0\x0\x0\x0\x0"
                                      "\x0\x0\x0\x0\x0"
                                      "\x0\x0\x0\x0\x0"
                                      "5:nodes26:"
                                      "\x0\x0\x0\x0\x0\x0\x0\x0\x0\x0\x0\x0\x0"
                                      "\x0\x0\x0\x0\x0\x0\x0\x0\x0\x0\x0\x0\x0"
                                      "5:token1:" OUR_TOKEN "e1:t";
#define R_GP_BASE_LEN (sizeof(R_GP_PROTO_BASE) - 1)
#define R_GP_PNODE_OFFSET 42

static const char R_EPILOGUE[] = "1:y1:re";
#define R_EPILOGUE_LEN (sizeof(R_EPILOGUE) - 1)

static inline u32 write_tok(char *restrict buf, const parsed_msg *krpc) {
    u32 offset = sprintf(buf, "%u:", krpc->tok_len);
    // Can't use sprintf because of possible null bytes
    memcpy(buf + offset, krpc->tok, krpc->tok_len);
    return offset + krpc->tok_len;
}

static inline void write_sid_raw(char *restrict buf) {
    for (int ix = 0; ix < NIH_LEN; ix++) {
        buf[ix] ^= SID_MASK[ix];
    }
}

static inline void write_sid(char *restrict buf, const nih_t nid) {
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

u32 msg_r_fn(char *restrict buf, const parsed_msg *krpc, const pnode_t pnode) {
    memcpy(buf, R_FN_PROTO_BASE, R_FN_BASE_LEN);
    write_sid(buf + R_SID_OFFSET, krpc->nid);
    memcpy(buf + R_FN_PNODE_OFFSET, pnode.raw, PNODE_LEN);

    u32 offset = R_FN_BASE_LEN;
    offset += write_tok(buf + offset, krpc);
    memcpy(buf + offset, R_EPILOGUE, R_EPILOGUE_LEN);
    offset += R_EPILOGUE_LEN;
    return offset;
}

u32 msg_r_gp(char *restrict buf, const parsed_msg *krpc, const pnode_t pnode) {
    memcpy(buf, R_GP_PROTO_BASE, R_GP_BASE_LEN);
    write_sid(buf + R_SID_OFFSET, krpc->nid);
    memcpy(buf + R_GP_PNODE_OFFSET, pnode.raw, PNODE_LEN);

    u32 offset = R_GP_BASE_LEN;
    offset += write_tok(buf + offset, krpc);
    memcpy(buf + offset, R_EPILOGUE, R_EPILOGUE_LEN);
    offset += R_EPILOGUE_LEN;
    return offset;
}

u32 msg_r_pg(char *restrict buf, const parsed_msg *krpc) {
    memcpy(buf, R_PG_PROTO_BASE, R_PG_BASE_LEN);
    write_sid(buf + R_SID_OFFSET, krpc->nid);

    u32 offset = R_PG_BASE_LEN;
    offset += write_tok(buf + offset, krpc);
    memcpy(buf + offset, R_EPILOGUE, R_EPILOGUE_LEN);
    offset += R_EPILOGUE_LEN;
    return offset;
}
