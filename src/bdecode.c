#include "bdecode.h"
#include "dht.h"
#include "log.h"
#include <assert.h>
#include <endian.h>

// IKEYS
const char keyname_ID[] = "id";
const char keyname_IMPLIED_PORT[] = "implied_port";
const char keyname_INFO_HASH[] = "info_hash";
const char keyname_NAME[] = "name";
const char keyname_NODES[] = "nodes";
const char keyname_PORT[] = "port";
const char keyname_TARGET[] = "target";
const char keyname_TOKEN[] = "token";
const char keyname_VALUES[] = "values";

// breakdown:
// i: id, implied_port, info_hash
// n: name, nodes
// p: port
// t: target, token
// v: values

// by length:
// 2: id
// 4: name, port
// 5: nodes, token
// 6: target, values
// 9: info_hash
// 12: implied_port

const char valname_AP[] = "announce_peer";
const char valname_GP[] = "get_peers";
const char valname_FN[] = "find_node";
const char valname_PG[] = "ping";

// breakdown:
// 4: ping
// 9: find_node, get_peers
// 13: announce_peer

#define XD_KEY_MATCH(ptr, keyname, slen)                                       \
    (0 == memcmp(ptr, keyname_##keyname, slen))
#define XD_VAL_MATCH(ptr, keyname, slen)                                       \
    (0 == memcmp(ptr, valname_##keyname, slen))

#ifdef BD_TRACE
#define TRACE(msg, ...) DEBUG(msg, ##__VA_ARGS__)
#else
#define TRACE(msg, ...) // nothing
#endif

#define XD_FAIL_MSG(code, msg)                                                 \
    TRACE("FAIL: %s %s", stat_names[(code)], msg);                             \
    return (code);
#define XD_FAIL(code) XD_FAIL_MSG((code), "")

typedef enum xdec_st_e {
    XD_START,
    XD_OKEY,
    XD_IKEY,
    XD_OVAL,
    XD_IVAL,
    XD_IVLIST,
} xdec_st;

static inline u32 krpc_bdecode_atoi(const char *buf, u32 *restrict ix,
                                    u32 maxlen, bd_state *restrict state) {
    /*
    Decode strictly nonnegative, colon or 'e' terminated decimal integers.
    Fast.

    buf[*ix] must point to the first actual digit on the number.

    Advances the buffer index in-place.

    Advances the index an extra position before returning, thus censuming the
    termination symbol.

    Returns as host integer! This might need to be converted to be.
    */

    u32 out = 0;
    u8 val;

    while (*ix < maxlen && out < (UINT32_MAX >> 4u)) {

        val = (u8)buf[*ix];
        (*ix)++;

        switch (val) {
        case '-':
            state->fail = ST_bd_z_negative_int;
            return -1;
        case '0':
        case '1':
        case '2':
        case '3':
        case '4':
        case '5':
        case '6':
        case '7':
        case '8':
        case '9':
            out = 10 * out + (val - 0x30);
            continue;
            // no break;
        case 'e':
        case ':':
            state->fail = ST_bd_a_no_error;
            return out;
        default:
            state->fail = ST_bd_x_bad_char;
            return -1;
        }
    }
    // Overflows should be their own error, but it really doesn't matter
    // since they're not common... just as long as they don't kill us...
    return ST_bd_x_msg_too_long;
}

inline stat_t xdecode(const char *data, u32 data_len,
                      parsed_msg *restrict out) {
    // Invariant 0: this function is never fast enough.
    if (data_len > BD_MAXLEN) {
        return ST_bd_x_msg_too_long;
    }

    TRACE("BEGIN XDECODE")

    u32 ix = 0;
    xdec_st xd_state = XD_START;

    u32 start;
    u32 dec_int;
    u32 slen;

    // complete initialization of the state struct
    bd_state bd_st = {
        .fail = ST_bd_a_no_error,
        .current_key = 0,
        .seen_keys = 0,
        // This is a bitflag
        .msg_kind = 0xffffffff,
        .save_ap_port = false,
    };

    while (ix < data_len) {
        switch ((int)data[ix]) {
        case 'd':
            switch (xd_state) {
            case XD_START:
                TRACE("vvv enter message")
                xd_state = XD_OKEY;
                ix++;
                continue;
            case XD_OVAL:
                TRACE("vvv enter payload")
                xd_state = XD_IKEY;
                ix++;
                continue;
            case XD_IKEY:
            case XD_OKEY:
                XD_FAIL(ST_bd_x_dict_is_key);
            case XD_IVLIST:
            case XD_IVAL:
                XD_FAIL(ST_bd_z_dicts_too_deep);
            }
        case 'e':
            switch (xd_state) {
            // This is the only clean way out!
            case XD_OKEY:
                TRACE("^^^ exit message")
                goto xd_validate;
            case XD_IKEY:
                TRACE("^^^ exit payload")
                xd_state = XD_OKEY;
                ix++;
                continue;
            case XD_IVLIST:
                TRACE("^^^ exit list")
                xd_state = XD_IKEY;
                ix++;
                continue;
            case XD_START:
            case XD_OVAL:
            case XD_IVAL:
                DEBUG("EOM at %u (%c) in %.*s", ix, data[ix], data_len, data);
                XD_FAIL(ST_bd_x_bad_eom);
            }
        case 'i':
            switch (xd_state) {
            case XD_OVAL:
            case XD_IVAL:
                ix += 1; // consume 'i'
                dec_int = krpc_bdecode_atoi(data, &ix, data_len, &bd_st);
                if (bd_st.fail != ST_bd_a_no_error) {
                    XD_FAIL(bd_st.fail);
                }
                if (xd_state == XD_IVAL && bd_st.current_key == BD_IKEY_PORT) {
                    if (dec_int > UINT16_MAX) {
                        XD_FAIL(ST_bd_y_port_overflow);
                    }
                    out->ap_port = (u16)dec_int;
                }
                if (xd_state == XD_OVAL) {
                    xd_state = XD_OKEY;
                } else {
                    xd_state = XD_IKEY;
                }
                continue;
            case XD_START:
                XD_FAIL(ST_bd_z_naked_value);
            case XD_IVLIST:
            case XD_IKEY:
            case XD_OKEY:
                XD_FAIL(ST_bd_z_rogue_int);
            }
        case 'l':
            switch (xd_state) {
            case XD_IVAL:
                TRACE("vvv enter list")
                xd_state = XD_IVLIST;
                ix++;
                continue;
            // We don't have an OVLIST to keep things simple
            default:
                XD_FAIL(ST_bd_z_unexpected_list);
            }
        // The infamous "case s"
        case '0':
        case '1':
        case '2':
        case '3':
        case '4':
        case '5':
        case '6':
        case '7':
        case '8':
        case '9':
            if (xd_state == XD_START) {
                XD_FAIL(ST_bd_z_naked_value);
            }

            slen = krpc_bdecode_atoi(data, &ix, data_len, &bd_st);
            // if overflow, unwind instantly
            if (bd_st.fail != ST_bd_a_no_error) {
                XD_FAIL(bd_st.fail);
            }
            start = ix;

            if (data_len < start + slen) {
                XD_FAIL(ST_bd_x_bad_eom);
            }
            // Advance index ahead of time!!!
            ix += slen;

            switch (xd_state) {
            case XD_OKEY:
                TRACE(">>> reading OKEY")
                // all outer keys have length 1, can check it off the bat
                xd_state = XD_OVAL;

                if (slen != 1) {
                    TRACE("??? unknown okey '%.*s' [slen %u != 1], ignoring",
                          slen, data + start, slen)
                    continue;
                }

                switch ((int)data[start]) { // OKEY
                case 'a':
                    if (bd_st.seen_keys & BD_OKEY_R) {
                        XD_FAIL(ST_bd_y_inconsistent_type)
                    }
                    TRACE(">>> matched okey 'a'; * -> MSG_Q")
                    // bd_st.legal_kinds &= Q_ANY
                    bd_st.msg_kind &= Q_ANY;
                    bd_st.current_key = BD_OKEY_A;
                    bd_st.seen_keys |= BD_OKEY_A;
                    continue;
                case 'r':
                    if (bd_st.seen_keys & (BD_OKEY_A | BD_OKEY_Q)) {
                        XD_FAIL(ST_bd_y_inconsistent_type)
                    }
                    TRACE(">>> matched okey 'r'; * -> MSG_R");
                    bd_st.msg_kind &= R_ANY;
                    bd_st.current_key = BD_OKEY_R;
                    bd_st.seen_keys |= BD_OKEY_R;
                    continue;
                case 't':
                    TRACE(">>> matched okey 't'")
                    bd_st.current_key = BD_OKEY_T;
                    bd_st.seen_keys |= BD_OKEY_T;
                    continue;
                case 'q':
                    if (bd_st.seen_keys & BD_OKEY_R) {
                        XD_FAIL(ST_bd_y_inconsistent_type)
                    }
                    TRACE(">>> matched okey 'q'; * -> MSG_Q")
                    bd_st.msg_kind &= Q_ANY;
                    bd_st.current_key = BD_OKEY_Q;
                    bd_st.seen_keys |= BD_OKEY_Q;
                    continue;
                case 'y':
                    TRACE(">>> matched okey 'y'")
                    bd_st.current_key = BD_OKEY_Y;
                    bd_st.seen_keys |= BD_OKEY_Y;
                    continue;
                default:
                    TRACE("??? ignoring unknown okey '%.*s'", slen,
                          data + start)
                    bd_st.current_key = BD_NOKEY;
                    continue;
                }
                assert(0);

            case XD_IKEY:
                TRACE(">>> reading IKEY")
                xd_state = XD_IVAL;
                switch (slen) { // IKEY
                case 2:
                    if (XD_KEY_MATCH(data + start, ID, slen)) {
                        TRACE(">>> matched ikey ID; * -> *")
                        bd_st.current_key = BD_IKEY_NID;
                        bd_st.seen_keys |= BD_IKEY_NID;
                        continue;
                    }
                    goto xd_unknown_ikey;
                case 4:
                    if ((bd_st.msg_kind & MSG_Q_AP) &&
                        XD_KEY_MATCH(data + start, NAME, slen)) {
                        TRACE(">>> matched ikey NAME; &MSG_Q_AP -> MSG_Q_AP")
                        bd_st.msg_kind = MSG_Q_AP;
                        bd_st.current_key = BD_IKEY_AP_NAME;
                        bd_st.seen_keys |= BD_IKEY_AP_NAME;
                        continue;
                    }
                    if (XD_KEY_MATCH(data + start, PORT, slen)) {
                        TRACE(">>> matched ikey PORT")
                        // we do not restrict the legal kinds, since other
                        // messages can have a port as extra data we ignore
                        bd_st.seen_keys |= BD_IKEY_PORT;
                        bd_st.save_ap_port = 1;
                        continue;
                    }
                    goto xd_unknown_ikey;
                case 5:
                    if (XD_KEY_MATCH(data + start, NODES, slen)) {
                        TRACE(">>> matched ikey NODES; * -> MSG_R_FN|MSG_R_GP")
                        bd_st.msg_kind &= (MSG_R_FN | MSG_R_GP);
                        bd_st.current_key = BD_IKEY_NODES;
                        bd_st.seen_keys |= BD_IKEY_NODES;
                        continue;
                    }
                    if (XD_KEY_MATCH(data + start, TOKEN, slen)) {
                        TRACE(">>> matched ikey TOKEN; X -> X & ~MSG_R_FN")
                        // NOTE many random queries include a token, we allow
                        // for it quite broadly
                        bd_st.msg_kind &= (~MSG_Q_FN);
                        bd_st.current_key = BD_IKEY_TOKEN;
                        bd_st.seen_keys |= BD_IKEY_TOKEN;
                        continue;
                    }
                    goto xd_unknown_ikey;
                case 6:
                    if (XD_KEY_MATCH(data + start, VALUES, slen)) {
                        TRACE(">>> matched ikey VALUES; * -> MSG_R_GP")
                        bd_st.msg_kind &= MSG_R_GP;
                        bd_st.current_key = BD_IKEY_VALUES;
                        bd_st.seen_keys |= BD_IKEY_VALUES;
                        continue;
                    }
                    if (XD_KEY_MATCH(data + start, TARGET, slen)) {
                        TRACE(">>> matched ikey TARGET; * -> MSG_Q_FN")
                        bd_st.msg_kind &= MSG_Q_FN;
                        bd_st.current_key = BD_IKEY_TARGET;
                        bd_st.seen_keys |= BD_IKEY_TARGET;
                        continue;
                    }
                    goto xd_unknown_ikey;
                case 9:
                    if (XD_KEY_MATCH(data + start, INFO_HASH, slen)) {
                        TRACE(">>> matched ikey INFO_HASH; * -> "
                              "MSG_Q_AP|MSG_Q_GP")
                        bd_st.msg_kind &= (MSG_Q_GP | MSG_Q_AP);
                        bd_st.current_key = BD_IKEY_IH;
                        bd_st.seen_keys |= BD_IKEY_IH;
                        continue;
                    }
                    goto xd_unknown_ikey;
                case 12:
                    if (XD_KEY_MATCH(data + start, IMPLIED_PORT, slen)) {
                        TRACE(">>> matched ikey IMPLIED_PORT; * -> MSG_Q_AP")
                        bd_st.msg_kind &= MSG_Q_AP;
                        bd_st.current_key = BD_IKEY_IMPLPORT;
                        bd_st.seen_keys |= BD_IKEY_IMPLPORT;
                        continue;
                    }
                    goto xd_unknown_ikey;
                xd_unknown_ikey:
                    bd_st.current_key = BD_NOKEY;
                    TRACE("??? ignoring unknown ikey '%.*s'", slen,
                          data + start)
                default:
                    continue;
                }
                assert(0);

            case XD_OVAL:
                TRACE(">>> reading OVAL")
                xd_state = XD_OKEY;
                switch (bd_st.current_key) { // OVAL
                // set the query type, if one is found...
                case BD_OKEY_Q:
                    switch (slen) {
                    case 4:
                        if (!XD_VAL_MATCH(data + start, PG, slen)) {
                            goto xd_bad_q;
                        }
                        TRACE("!!! q is MSG_Q_PG")
                        bd_st.msg_kind &= MSG_Q_PG;
                        continue;
                    case 9:
                        if (XD_VAL_MATCH(data + start, GP, slen)) {
                            TRACE("!!! q is MSG_Q_GP")
                            bd_st.msg_kind &= MSG_Q_GP;
                            continue;
                        }
                        if (XD_VAL_MATCH(data + start, FN, slen)) {
                            TRACE("!!! q is MSG_Q_FN")
                            bd_st.msg_kind &= MSG_Q_FN;
                            continue;
                        }
                        goto xd_bad_q;
                    case 13:
                        if (!XD_VAL_MATCH(data + start, AP, slen)) {
                            goto xd_bad_q;
                        }
                        TRACE("!!! q is MSG_Q_AP")
                        bd_st.msg_kind &= MSG_Q_AP;
                    default:
                    xd_bad_q:
                        XD_FAIL(ST_bd_z_unknown_query);
                    }
                // set the token
                case BD_OKEY_T:
                    if (slen > BD_MAXLEN_TOK) {
                        XD_FAIL(ST_bd_z_tok_too_long)
                    }
                    TRACE("!!! TOK[%u] = '%.*s'", slen, slen, data + start);
                    out->tok_len = slen;
                    memcpy(out->tok, data + start, slen);
                    continue;
                // check response consistency
                case BD_OKEY_Y:
                    if (slen == 1) {
                        switch (data[start]) {
                        case 'e':
                            XD_FAIL(ST_bd_z_error_type)
                            continue;
                        case 'r':
                            bd_st.msg_kind &= R_ANY;
                            continue;
                        case 'q':
                            bd_st.msg_kind &= Q_ANY;
                            continue;
                        default:
                            XD_FAIL(ST_bd_z_unknown_type)
                            continue;
                        }
                    } else {
                        XD_FAIL(ST_bd_z_unknown_type)
                    }
                // ignore other cases
                // TODO add better logic?
                default:
                    TRACE("??? ignoring oval")
                    continue;
                }
                assert(0);

            case XD_IVAL:
                TRACE(">>> reading IVAL")
                xd_state = XD_IKEY;
                switch (bd_st.current_key) { // IVAL
                case BD_IKEY_NODES:
                    if ((slen == 0) || ((slen % PNODE_LEN) != 0)) {
                        XD_FAIL(ST_bd_y_bad_length_nodes)
                    }
                    if (slen > (PNODE_LEN * BD_MAX_NODES)) {
                        TRACE("*** truncating nodes list")
                        slen = PNODE_LEN * BD_MAX_NODES;
                    }
                    TRACE("!!! NODES[%lu]", slen / PNODE_LEN);
                    out->n_nodes = slen / PNODE_LEN;
                    memcpy(out->nodes, data + start, slen);
                    continue;
                case BD_IKEY_TOKEN:
                    if (slen > BD_MAXLEN_TOKEN) {
                        XD_FAIL(ST_bd_z_token_too_long)
                    }
                    TRACE("!!! TOKEN[%u] = ...", slen);
                    memcpy(out->token, data + start, slen);
                    out->token_len = slen;
                    continue;
                case BD_IKEY_TARGET:
                    if (slen != NIH_LEN) {
                        XD_FAIL(ST_bd_y_bad_length_target)
                    }
                    TRACE("!!! TARGET = ...")
                    memcpy(out->target, data + start, slen);
                    continue;
                case BD_IKEY_NID:
                    if (slen != NIH_LEN) {
                        TRACE("slen = %u, bad nid msg: %.*s", slen, data_len,
                              data);
                        XD_FAIL(ST_bd_y_bad_length_nid)
                    }
                    TRACE("!!! NID")
                    SET_NIH(out->nid, data + start)
                    continue;
                case BD_IKEY_IH:
                    if (slen != NIH_LEN) {
                        XD_FAIL(ST_bd_y_bad_length_ih)
                    }
                    TRACE("!!! IH")
                    memcpy(out->ih, data + start, slen);
                    continue;
                case BD_IKEY_AP_NAME:
                    if (slen > BD_MAXLEN_AP_NAME) {
                        TRACE("*** name too long, ignoring.")
                    } else {
                        memcpy(out->ap_name, data + start, slen);
                        out->ap_name_len = slen;
                        TRACE("!!! NAME[%u] = [%.*s]...", slen, slen,
                              out->ap_name);
                    }
                    continue;
                // ignore other keys
                default:
                    TRACE("??? ignoring ival")
                    continue;
                }
                assert(0);

            case XD_IVLIST:
                TRACE(">>> reading IVLIST")
                if (bd_st.current_key != BD_IKEY_VALUES) {
                    XD_FAIL(ST_bd_z_unexpected_list);
                }
                // we are in a values list, but we read a weird
                // string NOTE we assume the entire message is
                // corrupted and bail out, parsing very
                // conservatively is the key to sanity
                if (slen != PEERINFO_LEN) {
                    XD_FAIL(ST_bd_y_bad_length_peer)
                }
                if (out->n_peers < BD_MAX_PEERS) {
                    memcpy(out->peers[out->n_peers], data + start,
                           PEERINFO_LEN);
                }
                out->n_peers += 1;

                TRACE("!!! VALUES[%lu]", slen / PEERINFO_LEN)
                continue;

            default:
                // Unreacahble
                assert(0);
            }

        default:
            TRACE("Invalid char %c in state %d", data[ix], xd_state)
            TRACE("data: %.*s", 20, data)
            TRACE("ix: %d, rem_data: %.*s...", ix, 10, data + ix)
            XD_FAIL(ST_bd_x_bad_char);
        }
        assert(0);
    }

xd_validate:

    // MESSAGE SANITY FILTERING
    // ALL messages need a NID...

    if (!(bd_st.seen_keys & BD_IKEY_NID)) {
        return ST_bd_y_no_nid;
    }

    //... and a TOK
    if (!(bd_st.seen_keys & BD_OKEY_T)) {
        return ST_bd_y_no_tok;
    }

    TRACE("??? DECIDING: [keys = %u] [methods = %u]", bd_st.seen_keys,
          bd_st.msg_kind);

    // METHOD RESOLUTION
    // exact APs and GPs need an info_hash only
    if (bd_st.msg_kind & Q_ANY) {
        TRACE("??? DECIDING as query")
        if (bd_st.msg_kind == MSG_Q_AP || bd_st.msg_kind == MSG_Q_GP) {
            if (!(bd_st.seen_keys & BD_IKEY_IH)) {
                TRACE("=== REJECT (q_gp | q_ap) && ~ih")
                return ST_bd_y_apgp_no_ih;
            } else if (bd_st.msg_kind == MSG_Q_AP &&
                       !(bd_st.seen_keys & (BD_IKEY_PORT | BD_IKEY_IMPLPORT))) {
                TRACE("=== REJECT q_ap && ~(port | impl_port)")
                return ST_bd_y_ap_no_port;
            }
#ifndef BD_NOFILTER_AP
            else if (bd_st.msg_kind == MSG_Q_AP &&
                     !(out->tok_len != 1 && out->token[0] == OUR_TOKEN[0])) {
                TRACE("=== REJECT q_ap && unrecognized token")
                return ST_bd_z_token_unrecognized;
            }
#endif
#ifdef BD_TRACE
            if (bd_st.msg_kind == MSG_Q_AP) {
                TRACE("=== ACCEPT MSG_Q_AP")
            } else {
                TRACE("=== ACCEPT MSG_Q_GP")
            }
#endif
            out->method = bd_st.msg_kind;
        } else if (bd_st.msg_kind == MSG_Q_FN) {
            if (!(bd_st.seen_keys & BD_IKEY_TARGET)) {
                TRACE("=== REJECT q_fn && ~target")
                return ST_bd_y_fn_no_target;
            }
            TRACE("=== ACCEPT MSG_Q_FN")
            out->method = MSG_Q_FN;
        }
        // accept only simple pings
        else if (bd_st.msg_kind == MSG_Q_PG) {
            if (bd_st.seen_keys & BD_IKEY_ANY_BODY) {
                TRACE("=== REJECT q_pg && body")
                return ST_bd_z_ping_body;
            }
            TRACE("=== ACCEPT MSG_Q_PG")
            out->method = MSG_Q_PG;
        } else {
            TRACE("=== REJECT fallthrough q_any")
            return ST_bd_z_unknown_query;
        }
    } else if (bd_st.msg_kind & R_ANY) {
        TRACE("??? DECIDING as reply")
        // This check could be made tighter... probably not worth it...
        if (!((out->tok_len == 1 && out->tok[0] == OUR_TOK[0]) ||
              (out->tok_len == 3 && out->tok[2] == OUR_TOK[0]))) {
            TRACE("=== REJECT reply && not our tok")
            return ST_bd_z_tok_unrecognized;
        }
        // TOKEN and (VALUES or NODES) <-> R_GP
        if ((bd_st.seen_keys & BD_IKEY_TOKEN) &&
            bd_st.seen_keys & (BD_IKEY_VALUES | BD_IKEY_NODES)) {
            TRACE("??? DECIDING as R_GP")
            out->method = MSG_R_GP;

            if (out->n_nodes + out->n_peers == 0) {
                TRACE("=== REJECT r_gp && (n + v) == 0")
                return ST_bd_y_empty_gp_response;
            }
            TRACE("=== ACCEPT token && (nodes | values) -> MSG_R_GP")
        }
        // VALUES and ~TOKEN <-> bad R_GP
        else if ((bd_st.seen_keys & BD_IKEY_VALUES) &&
                 !(bd_st.seen_keys & BD_IKEY_TOKEN)) {
            TRACE("=== REJECT values && ~token")
            return ST_bd_y_vals_wo_token;
        }
        //~TOKEN and ~VALUES and NODES <->R_FN
        else if (bd_st.seen_keys & BD_IKEY_NODES) {
            TRACE("=== ACCEPT ~token && ~values && nodes -> MSG_R_FN")
            out->method = MSG_R_FN;
        }
        //~NODES and ~VALUES <->R_PG
        else {
            if (bd_st.seen_keys & BD_IKEY_ANY_NON_TOKEN_BODY) {
                TRACE("=== REJECT (body - tok) && ~(nodes || values) -> "
                      "bad r_pg")
                return ST_bd_z_ping_body;
            }
            TRACE("=== ACCEPT ~(values | nodes) -> MSG_R_PG")
            out->method = MSG_R_PG;
        }
    } else {
        TRACE("=== REJECT ~[type & (q | r)] -> incongruous")
        return ST_bd_z_incongruous_message;
    }

    return ST_bd_a_no_error;
}

inline const char *get_method_name(bd_meth_t method) {
    switch (method) {
    case MSG_R_GP:
        return "MSG_R_GP";
    case MSG_R_PG:
        return "MSG_R_PG";
    case MSG_R_FN:
        return "MSG_R_FN";
    case MSG_Q_AP:
        return "MSG_Q_AP";
    case MSG_Q_GP:
        return "MSG_Q_GP";
    case MSG_Q_FN:
        return "MSG_Q_FN";
    case MSG_Q_PG:
        return "MSG_Q_PG";
    default:
        return "No such method!";
    }
}

void print_parsed_msg(parsed_msg *out) {
    printf("\tMETH = %s\n", get_method_name(out->method));

    printf("\tTOK[%u] = %.*s\n", out->tok_len, (int)out->tok_len, out->tok);
    printf("\tNID[20] = %.*s\n", NIH_LEN, out->nid);

    if (out->method == MSG_Q_AP) {
        printf("\t\tMSG_Q_AP -> IH = %.*s\n", NIH_LEN, out->ih);

        printf("\t\tMSG_Q_AP -> PORT = {%hu} (IP = {%i})\n", out->ap_port,
               out->ap_implied_port);

        printf("\t\tMSG_Q_AP -> TOKEN[%u] = %.*s\n", out->token_len,
               (int)out->token_len, out->token);

    } else if (out->method == MSG_Q_GP) {
        printf("\t\tMSG_Q_GP -> IH = %.*s\n", NIH_LEN, out->ih);

    } else if (out->method == MSG_Q_FN) {
        printf("\t\tMSG_Q_FN -> TARGET = %.*s\n", NIH_LEN, out->target);

    } else if (out->method == MSG_R_FN) {
        printf("\t\t MSG_R_FN -> NODES[%u] = ...\n", out->n_nodes);

    } else if (out->method == MSG_R_GP) {
        if (out->n_nodes > 0)
            printf("\t\tMSG_R_GP -> NODES[%u] = ...\n", out->n_nodes);
        if (out->n_peers > 0)
            printf("\t\tMSG_R_GP -> PEERS[%u] = ...\n", out->n_peers);

        printf("\t\tMSG_R_GP -> TOKEN[%u] = %.*s\n", out->token_len,
               (int)out->token_len, out->token);
    }
}
