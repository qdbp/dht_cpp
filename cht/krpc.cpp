#include "dht.h"
#include "krpc.h"
#include "log.h"
#include <assert.h>
#include <endian.h>

using namespace cht::bd;
namespace cht::bd {

const char *get_method_name(Method method) {
    switch (method) {
    case Q_AP:
        return "Q_AP";
    case Q_PG:
        return "Q_PG";
    case Q_FN:
        return "Q_FN";
    case Q_GP:
        return "Q_GP";
    case R_FN:
        return "R_FN";
    case R_GP:
        return "R_GP";
    case R_PG:
        return "R_PG";
    default:
        return "Mixed Method";
    }
}

stat_t KRPC::krpc_bdecode_atoi(u32 &dest, u32 &cur_pos, u32 data_len) {
    /*
    Decode strictly nonnegative, colon or 'e' terminated bencoded integers.

    buf[*cur_pos] must point to the first actual digit on the number.

    Advances the buffer index in-place.

    Advances the index an extra position before returning, thus censuming
    the termination symbol.

    Returns as host integer! This might need to be converted to be.
    */

    dest = 0;
    u8 val;

    while (cur_pos < data_len && dest < (UINT32_MAX >> 4u)) {

        val = data[cur_pos];
        cur_pos++;

        switch (val) {
        case '-':
            // hist->fail = ST_bd_z_negative_int;
            return ST_bd_z_negative_int;
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
            dest = 10 * dest + (val - 0x30);
            continue;
        case 'e':
        case ':':
            return ST_bd_a_no_error;
        default:
            return ST_bd_x_bad_char;
        }
    }
    // int overflows should be their own error, but it really doesn't matter
    // since they're not common... just as long as they don't kill us...
    return ST_bd_x_msg_too_long;
}

void KRPC::xdecode(u32 data_len) {
    // Invariant 0: this function is never fast enough.
    TRACE("BEGIN XDECODE: ");
#ifdef BD_TRACE
    PRINT_MSG(data, data_len);
#endif

    u32 cur_pos = 0;
    XDState xd_state = XD_START;

    u32 _int = 0;
    u32 start = 0;
    u32 &slen = _int;
    u32 &port = _int;

    // complete initialization of the state struct
    XDHist xd_hist = {
        .fail = ST_bd_a_no_error,
        .current_key = NOKEY,
        .seen_keys = NOKEY,
        // This is a bitflag
        .msg_kind = R_ANY | Q_ANY,
    };

    while (cur_pos < data_len) {
        switch ((int)data[cur_pos]) {
        case 'd':
            switch (xd_state) {
            case XD_START:
                TRACE("vvv enter message")
                xd_state = XD_OKEY;
                cur_pos++;
                continue;
            case XD_OVAL:
                TRACE("vvv enter payload")
                xd_state = XD_IKEY;
                cur_pos++;
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
                validate(xd_hist);
                return;
            case XD_IKEY:
                TRACE("^^^ exit payload")
                xd_state = XD_OKEY;
                cur_pos++;
                continue;
            case XD_IVLIST:
                TRACE("^^^ exit list")
                xd_state = XD_IKEY;
                cur_pos++;
                continue;
            case XD_START:
            case XD_OVAL:
            case XD_IVAL:
                XD_FAIL(ST_bd_x_bad_eom);
            }
        case 'i':
            switch (xd_state) {
            case XD_OVAL:
            case XD_IVAL:
                cur_pos += 1; // consume 'i'

                this->status = krpc_bdecode_atoi(port, cur_pos, data_len);

                if (this->status != ST_bd_a_no_error) {
                    XD_FAIL(this->status)
                }

                if (xd_state == XD_IVAL && xd_hist.current_key == IKEY_PORT) {
                    if (port > UINT16_MAX) {
                        XD_FAIL(ST_bd_y_port_overflow);
                    }
                    this->ap_port = u16(port);
                }
                xd_state = (xd_state == XD_OVAL) ? XD_OKEY : XD_IKEY;
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
                cur_pos++;
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

            this->status = krpc_bdecode_atoi(slen, cur_pos, data_len);
            if (this->status != ST_bd_a_no_error) {
                XD_FAIL(this->status);
            }

            start = cur_pos;

            if (data_len < start + slen) {
                XD_FAIL(ST_bd_x_bad_eom);
            }
            // Advance index ahead of time!!!
            cur_pos += slen;

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
                    if (xd_hist.seen_keys & OKEY_R) {
                        XD_FAIL(ST_bd_y_inconsistent_type)
                    }
                    TRACE(">>> matched okey 'a'; * -> Q")
                    // xd_hist.legal_kinds &= Q_ANY
                    xd_hist.msg_kind &= Q_ANY;
                    xd_hist.current_key = OKEY_A;
                    xd_hist.seen_keys |= OKEY_A;
                    continue;
                case 'r':
                    if (xd_hist.seen_keys & (OKEY_A | OKEY_Q)) {
                        XD_FAIL(ST_bd_y_inconsistent_type)
                    }
                    TRACE(">>> matched okey 'r'; * -> R");
                    xd_hist.msg_kind &= R_ANY;
                    xd_hist.current_key = OKEY_R;
                    xd_hist.seen_keys |= OKEY_R;
                    continue;
                case 't':
                    TRACE(">>> matched okey 't'")
                    xd_hist.current_key = OKEY_T;
                    xd_hist.seen_keys |= OKEY_T;
                    continue;
                case 'q':
                    if (xd_hist.seen_keys & OKEY_R) {
                        XD_FAIL(ST_bd_y_inconsistent_type)
                    }
                    TRACE(">>> matched okey 'q'; * -> Q")
                    xd_hist.msg_kind &= Q_ANY;
                    xd_hist.current_key = OKEY_Q;
                    xd_hist.seen_keys |= OKEY_Q;
                    continue;
                case 'y':
                    TRACE(">>> matched okey 'y'")
                    xd_hist.current_key = OKEY_Y;
                    xd_hist.seen_keys |= OKEY_Y;
                    continue;
                default:
                    TRACE("??? ignoring unknown okey '%.*s'", slen,
                          data + start)
                    xd_hist.current_key = NOKEY;
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
                        xd_hist.current_key = IKEY_NID;
                        xd_hist.seen_keys |= IKEY_NID;
                        continue;
                    }
                    goto xd_unknown_ikey;
                case 4:
                    if ((xd_hist.msg_kind & Q_AP) &&
                        XD_KEY_MATCH(data + start, NAME, slen)) {
                        TRACE(">>> matched ikey NAME; &Q_AP -> Q_AP")
                        xd_hist.msg_kind = Q_AP;
                        xd_hist.current_key = IKEY_AP_NAME;
                        xd_hist.seen_keys |= IKEY_AP_NAME;
                        continue;
                    }
                    if (XD_KEY_MATCH(data + start, PORT, slen)) {
                        TRACE(">>> matched ikey PORT")
                        // we do not the legal kinds, since other
                        // messages can have a port as extra data we ignore
                        xd_hist.seen_keys |= IKEY_PORT;
                        continue;
                    }
                    goto xd_unknown_ikey;
                case 5:
                    if (XD_KEY_MATCH(data + start, NODES, slen)) {
                        TRACE(">>> matched ikey NODES; * -> "
                              "R_FN|R_GP")
                        xd_hist.msg_kind &= (R_FN | R_GP);
                        xd_hist.current_key = IKEY_NODES;
                        xd_hist.seen_keys |= IKEY_NODES;
                        continue;
                    }
                    if (XD_KEY_MATCH(data + start, TOKEN, slen)) {
                        TRACE(">>> matched ikey TOKEN; X -> X & ~R_FN")
                        // NOTE many random queries include a token, we
                        // allow for it quite broadly
                        xd_hist.msg_kind &= (~Q_FN);
                        xd_hist.current_key = IKEY_TOKEN;
                        xd_hist.seen_keys |= IKEY_TOKEN;
                        continue;
                    }
                    goto xd_unknown_ikey;
                case 6:
                    if (XD_KEY_MATCH(data + start, VALUES, slen)) {
                        TRACE(">>> matched ikey VALUES; * -> R_GP")
                        xd_hist.msg_kind &= R_GP;
                        xd_hist.current_key = IKEY_VALUES;
                        xd_hist.seen_keys |= IKEY_VALUES;
                        continue;
                    }
                    if (XD_KEY_MATCH(data + start, TARGET, slen)) {
                        TRACE(">>> matched ikey TARGET; * -> Q_FN")
                        xd_hist.msg_kind &= Q_FN;
                        xd_hist.current_key = IKEY_TARGET;
                        xd_hist.seen_keys |= IKEY_TARGET;
                        continue;
                    }
                    goto xd_unknown_ikey;
                case 9:
                    if (XD_KEY_MATCH(data + start, INFO_HASH, slen)) {
                        TRACE(">>> matched ikey INFO_HASH; * -> "
                              "Q_AP|Q_GP")
                        xd_hist.msg_kind &= (Q_GP | Q_AP);
                        xd_hist.current_key = IKEY_IH;
                        xd_hist.seen_keys |= IKEY_IH;
                        continue;
                    }
                    goto xd_unknown_ikey;
                case 12:
                    if (XD_KEY_MATCH(data + start, IMPLIED_PORT, slen)) {
                        TRACE(">>> matched ikey IMPLIED_PORT; * -> Q_AP")
                        xd_hist.msg_kind &= Q_AP;
                        xd_hist.current_key = IKEY_IMPLPORT;
                        xd_hist.seen_keys |= IKEY_IMPLPORT;
                        continue;
                    }
                    goto xd_unknown_ikey;
                xd_unknown_ikey:
                    xd_hist.current_key = NOKEY;
                    TRACE("??? ignoring unknown ikey '%.*s'", slen,
                          data + start)
                default:
                    continue;
                }
                assert(0);

            case XD_OVAL:
                TRACE(">>> reading OVAL, OKEY is %d", xd_hist.current_key)
                xd_state = XD_OKEY;
                switch (xd_hist.current_key) { // OVAL
                // set the query type, if one is found...
                case OKEY_Q:
                    switch (slen) {
                    case 4:
                        if (!XD_VAL_MATCH(data + start, PG, slen)) {
                            goto xd_bad_q;
                        }
                        TRACE("!!! q is Q_PG")
                        xd_hist.msg_kind &= Q_PG;
                        continue;
                    case 9:
                        if (XD_VAL_MATCH(data + start, GP, slen)) {
                            TRACE("!!! q is Q_GP")
                            xd_hist.msg_kind &= Q_GP;
                            continue;
                        }
                        if (XD_VAL_MATCH(data + start, FN, slen)) {
                            TRACE("!!! q is Q_FN")
                            xd_hist.msg_kind &= Q_FN;
                            continue;
                        }
                        goto xd_bad_q;
                    case 13:
                        if (!XD_VAL_MATCH(data + start, AP, slen)) {
                            goto xd_bad_q;
                        }
                        TRACE("!!! q is Q_AP")
                        xd_hist.msg_kind &= Q_AP;
                    default:
                    xd_bad_q:
                        XD_FAIL(ST_bd_z_unknown_query);
                    }
                // set the tok
                case OKEY_T:
                    if (slen > MAXLEN_TOK) {
                        XD_FAIL(ST_bd_z_tok_too_long)
                    }
                    TRACE("!!! TOK[%u] = '%.*s'", slen, slen, data + start);
                    this->tok_len = slen;
                    this->tok = (data + start);
                    continue;
                // check response consistency
                case OKEY_Y:
                    if (slen == 1) {
                        switch (data[start]) {
                        case 'e':
                            XD_FAIL(ST_bd_z_error_type)
                            continue;
                        case 'r':
                            xd_hist.msg_kind &= R_ANY;
                            continue;
                        case 'q':
                            xd_hist.msg_kind &= Q_ANY;
                            continue;
                        default:
                            XD_FAIL(ST_bd_z_unknown_type)
                            continue;
                        }
                    } else {
                        XD_FAIL(ST_bd_z_unknown_type)
                    }
                default:
                    TRACE("??? ignoring oval")
                    continue;
                }
                assert(0);

            case XD_IVAL:
                TRACE(">>> reading IVAL")
                xd_state = XD_IKEY;
                switch (xd_hist.current_key) { // IVAL
                case IKEY_NODES:
                    if ((slen == 0) || ((slen % PNODE_LEN) != 0)) {
                        XD_FAIL(ST_bd_y_bad_length_nodes)
                    }
                    if (slen > (PNODE_LEN * MAX_NODES)) {
                        TRACE("*** truncating nodes list")
                        slen = PNODE_LEN * MAX_NODES;
                    }
                    TRACE("!!! NODES[%lu]", slen / PNODE_LEN);
                    this->n_nodes = slen / PNODE_LEN;
                    this->nodes = reinterpret_cast<const PNode *>(data + start);
                    continue;
                case IKEY_TOKEN:
                    if (slen > MAXLEN_TOKEN) {
                        XD_FAIL(ST_bd_z_token_too_long)
                    }
                    TRACE("!!! TOKEN[%u] = ...", slen);
                    this->token_len = slen;
                    this->token = data + start;
                    continue;
                case IKEY_TARGET:
                    if (slen != NIH_LEN) {
                        XD_FAIL(ST_bd_y_bad_length_target)
                    }
                    TRACE("!!! TARGET = ...")
                    this->target = reinterpret_cast<const Nih *>(data + start);
                    continue;
                case IKEY_NID:
                    if (slen != NIH_LEN) {
                        TRACE("slen = %u, bad nid msg: %.*s", slen, data_len,
                              data);
                        XD_FAIL(ST_bd_y_bad_length_nid)
                    }
                    TRACE("!!! NID")
                    this->nid = reinterpret_cast<const Nih *>(data + start);
                    continue;
                case IKEY_IH:
                    if (slen != NIH_LEN) {
                        XD_FAIL(ST_bd_y_bad_length_ih)
                    }
                    TRACE("!!! IH")
                    this->ih = reinterpret_cast<const Nih *>(data + start);
                    continue;
                case IKEY_AP_NAME:
                    if (slen > MAXLEN_AP_NAME) {
                        TRACE("*** name too long, ignoring.")
                    } else {
                        this->ap_name_len = slen;
                        this->ap_name = data + start;
                        TRACE("!!! NAME[%u] = [%.*s]...", slen, slen,
                              this->ap_name);
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
                if (xd_hist.current_key != IKEY_VALUES) {
                    XD_FAIL(ST_bd_z_unexpected_list);
                }
                // conservatively bail out if we get a bad length peer
                if (slen != PEERINFO_LEN) {
                    XD_FAIL(ST_bd_y_bad_length_peer)
                }
                if (this->n_peers < MAX_PEERS) {
                    this->n_peers++;
                    if (this->peers == nullptr) {
                        this->peers =
                            reinterpret_cast<const Peerinfo *>(data + start);
                    }
                }
                TRACE("!!! VALUES[%lu]", slen / PEERINFO_LEN)
                continue;

            default:
                // Unreacahble
                assert(0);
            }

        default:
            TRACE("Invalid char %c in state %d", data[cur_pos], xd_state)
            TRACE("data: %.*s", 20, data)
            TRACE("cur_pos: %d, rem_data: %.*s...", cur_pos, 10, data + cur_pos)
            XD_FAIL(ST_bd_x_bad_char);
        }
        assert(0);
    }
    ERROR("The following message reached end of loop! data_len = %d", data_len)
    PRINT_MSG(data, data_len);
    assert(0);
}

void KRPC::validate(const XDHist &hist) {

    TRACE("ENTER VALIDATE")

    // ALL messages need a NID...
    if (!(hist.seen_keys & IKEY_NID)) {
        XD_FAIL(ST_bd_y_no_nid);
    }

    //... and a TOK
    if (!(hist.seen_keys & OKEY_T)) {
        XD_FAIL(ST_bd_y_no_tok);
    }

    TRACE("??? DECIDING: [keys = %u] [methods = %u]", hist.seen_keys,
          hist.msg_kind);

    assert(!(hist.msg_kind & Q_ANY && hist.msg_kind & R_ANY));

    // exact APs and GPs need an info_hash only
    if (hist.msg_kind & Q_ANY) {
        TRACE("??? DECIDING as query")
        if (hist.msg_kind == Q_AP || hist.msg_kind == Q_GP) {
            if (!(hist.seen_keys & IKEY_IH)) {
                TRACE("=== REJECT (q_gp | q_ap) && ~ih")
                XD_FAIL(ST_bd_y_apgp_no_ih);
            } else if (hist.msg_kind == Q_AP &&
                       !(hist.seen_keys & (IKEY_PORT | IKEY_IMPLPORT))) {
                TRACE("=== REJECT q_ap && ~(port | impl_port)")
                XD_FAIL(ST_bd_y_ap_no_port)
            }
#ifndef NOFILTER_AP
            else if (hist.msg_kind == Q_AP &&
                     !(this->token_len != 1 && this->token[0] == OUR_TOKEN)) {
                TRACE("=== REJECT q_ap && unrecognized token")
                XD_FAIL(ST_bd_z_token_unrecognized)
            }
#endif
#ifdef TRACE
            if (hist.msg_kind == Q_AP) {
                TRACE("=== ACCEPT Q_AP")
            } else {
                TRACE("=== ACCEPT Q_GP")
            }
#endif
            this->method = hist.msg_kind;
        } else if (hist.msg_kind == Q_FN) {
            if (!(hist.seen_keys & IKEY_TARGET)) {
                TRACE("=== REJECT q_fn && ~target")
                XD_FAIL(ST_bd_y_fn_no_target)
            }
            TRACE("=== ACCEPT Q_FN")
            this->method = Q_FN;
        }
        // accept only simple pings
        else if (hist.msg_kind == Q_PG) {
            if (hist.seen_keys & IKEY_ANY_BODY) {
                TRACE("=== REJECT q_pg && body")
                XD_FAIL(ST_bd_z_ping_body)
            }
            TRACE("=== ACCEPT Q_PG")
            this->method = Q_PG;
        } else {
            TRACE("=== REJECT fallthrough q_any")
            XD_FAIL(ST_bd_z_unknown_query)
        }
    } else if (hist.msg_kind & R_ANY) {
        TRACE("??? DECIDING as reply")

        // TOKEN and (VALUES or NODES) <-> R_GP
        if ((hist.seen_keys & IKEY_TOKEN) &&
            hist.seen_keys & (IKEY_VALUES | IKEY_NODES)) {
            TRACE("??? DECIDING as R_GP")
            this->method = R_GP;

            if (this->tok_len != 3 || this->tok[2] != OUR_TOK_GP) {
                TRACE("=== REJECT r_gp && not our gp tok")
                XD_FAIL(ST_bd_z_bad_tok_gp)
            }

            if (this->n_nodes + this->n_peers == 0) {
                TRACE("=== REJECT r_gp && (n + v) == 0")
                XD_FAIL(ST_bd_y_empty_gp_response)
            }

            TRACE("=== ACCEPT token && (nodes | values) -> R_GP")
        }
        // VALUES and ~TOKEN <-> bad R_GP
        else if ((hist.seen_keys & IKEY_VALUES) &&
                 !(hist.seen_keys & IKEY_TOKEN)) {
            TRACE("=== REJECT values && ~token")
            XD_FAIL(ST_bd_y_vals_wo_token)
        }
        //~TOKEN and ~VALUES and NODES <->R_FN
        else if (hist.seen_keys & IKEY_NODES) {

            if (this->tok_len != 1 || this->tok[0] != OUR_TOK_FN) {
                TRACE("=== REJECT r_fn && not our fn tok")
                XD_FAIL(ST_bd_z_bad_tok_fn)
            }

            TRACE("=== ACCEPT ~token && ~values && nodes -> R_FN")
            this->method = R_FN;
        }
        //~NODES and ~VALUES <->R_PG
        else {
            if (hist.seen_keys & IKEY_ANY_NON_TOKEN_BODY) {
                TRACE("=== REJECT (body - tok) && ~(nodes || values) -> "
                      "bad r_pg")
                XD_FAIL(ST_bd_z_ping_body)
            }
            if (this->tok_len != 1 || this->tok[0] != OUR_TOK_PG) {
                TRACE("=== REJECT r_pg && not our pg tok")
                XD_FAIL(ST_bd_z_bad_tok_pg)
            }
            TRACE("=== ACCEPT ~(values | nodes) -> R_PG")
            this->method = R_PG;
        }
    } else {
        TRACE("=== REJECT ~[type & (q | r)] -> incongruous")
        XD_FAIL(ST_bd_z_incongruous_message)
    }

    this->status = ST_bd_a_no_error;
}

void KRPC::print() {
    printf("\tMETH = %s\n", get_method_name(method));

    printf("\tTOK[%u] = %.*s\n", tok_len, (int)tok_len, tok);
    printf("\tNID[20] = %.*s\n", NIH_LEN, nid->raw);

    if (method == Q_AP) {
        printf("\t\tQ_AP -> IH = %.*s\n", NIH_LEN, ih->raw);

        printf("\t\tQ_AP -> PORT = {%hu} (IP = {%i})\n", ap_port,
               ap_implied_port);

        printf("\t\tQ_AP -> TOKEN[%u] = %.*s\n", token_len, (int)token_len,
               token);

    } else if (method == Q_GP) {
        printf("\t\tQ_GP -> IH = %.*s\n", NIH_LEN, ih->raw);

    } else if (method == Q_FN) {
        printf("\t\tQ_FN -> TARGET = %.*s\n", NIH_LEN, target->raw);

    } else if (method == R_FN) {
        printf("\t\t R_FN -> NODES[%u] = ...\n", n_nodes);

    } else if (method == R_GP) {
        if (n_nodes > 0)
            printf("\t\tR_GP -> NODES[%u] = ...\n", n_nodes);
        if (n_peers > 0)
            printf("\t\tR_GP -> PEERS[%u] = ...\n", n_peers);

        printf("\t\tR_GP -> TOKEN[%u] = %.*s\n", token_len, (int)token_len,
               token);
    }
}

} // namespace cht::bd
