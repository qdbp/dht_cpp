// vi:ft=cpp
#pragma once

#include "dht.hpp"
#include "log.hpp"
#include "stat.hpp"
#include <cassert>
#include <cstdio>
#include <cstring>

#include <locale>

using namespace cht;
namespace cht::bd {

#ifdef BD_TRACE
#define TRACE(msg, ...) DEBUG(msg, ##__VA_ARGS__)
#else
#define TRACE(msg, ...) // nothing
#endif

#define XD_KEY_MATCH(ptr, keyname, slen)                                       \
    (0 == memcmp(ptr, keyname_##keyname, slen))
#define XD_VAL_MATCH(ptr, keyname, slen)                                       \
    (0 == memcmp(ptr, valname_##keyname, slen))

#define XD_FAIL_MSG(code, msg)                                                 \
    TRACE("FAIL: %s %s", stat_names[(code)], msg);                             \
    this->status = (code);                                                     \
    return;
#define XD_FAIL(code) XD_FAIL_MSG((code), "")

// IKEYS
constexpr inline char keyname_ID[] = "id";
constexpr inline char keyname_IMPLIED_PORT[] = "implied_port";
constexpr inline char keyname_INFO_HASH[] = "info_hash";
constexpr inline char keyname_NAME[] = "name";
constexpr inline char keyname_NODES[] = "nodes";
constexpr inline char keyname_PORT[] = "port";
constexpr inline char keyname_TARGET[] = "target";
constexpr inline char keyname_TOKEN[] = "token";
constexpr inline char keyname_VALUES[] = "values";

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

constexpr inline char valname_AP[] = "announce_peer";
constexpr inline char valname_GP[] = "get_peers";
constexpr inline char valname_FN[] = "find_node";
constexpr inline char valname_PG[] = "ping";

// BDECODE SIZES
constexpr inline u16 MAXLEN = 1024;
constexpr inline u16 MAXLEN_AP_NAME = 256;
constexpr inline u8 MAXLEN_TOK = 32;
constexpr inline u8 MAXLEN_TOKEN = 32;
constexpr inline u8 MAX_PEERS = 36;
constexpr inline u8 MAX_NODES = 8;

#define MK_BIT_OPERATORS(type)                                                 \
    inline type operator|(const type &x, const type &y) {                      \
        return static_cast<type>(u64(x) | u64(y));                             \
    }                                                                          \
    inline type operator&(const type &x, const type &y) {                      \
        return static_cast<type>(u64(x) & u64(y));                             \
    }                                                                          \
    inline type &operator|=(type &x, const type &y) {                          \
        x = static_cast<type>(x | y);                                          \
        return x;                                                              \
    }                                                                          \
    inline type &operator&=(type &x, const type &y) {                          \
        x = static_cast<type>(x & y);                                          \
        return x;                                                              \
    }                                                                          \
    inline type operator~(type x) {                                            \
        return static_cast<type>(~u64(x));                                     \
    }

// bdecode message types
enum Method {
    Q_AP = 1u,
    Q_FN = 1u << 1u,
    Q_GP = 1u << 2u,
    Q_PG = 1u << 3u,

    R_FN = 1u << 5u,
    R_GP = 1u << 6u,
    R_PG = 1u << 7u,

    R_ANY = (R_FN | R_GP | R_PG),
    Q_ANY = (Q_AP | Q_FN | Q_GP | Q_PG),
    ANY = R_ANY | Q_ANY,

};

MK_BIT_OPERATORS(Method)

enum Key {
    NOKEY = 0,
    IKEY_VALUES = 1u,
    IKEY_NODES = 1u << 1u,
    IKEY_TOKEN = 1u << 2u,
    IKEY_IH = 1u << 3u,
    IKEY_NID = 1u << 4u,
    IKEY_TARGET = 1u << 5u,
    IKEY_IMPLPORT = 1u << 6u,
    IKEY_PORT = 1u << 7u,
    IKEY_AP_NAME = 1u << 8u,
    OKEY_A = 1u << 9u,
    OKEY_T = 1u << 10u,
    OKEY_Q = 1u << 11u,
    OKEY_R = 1u << 12u,
    OKEY_Y = 1u << 13u,

    IKEY_ANY_BODY =
        (IKEY_NODES | IKEY_VALUES | IKEY_IH | IKEY_TARGET | IKEY_TOKEN),

    IKEY_ANY_NON_TOKEN_BODY =
        (IKEY_NODES | IKEY_VALUES | IKEY_IH | IKEY_TARGET),
};

MK_BIT_OPERATORS(Key)

class KRPC {
  public:
    std::array<u8, MAXLEN> data;
    // const u8 data[MAXLEN] = {0};

    stat_t status = ST__ST_ENUM_START;
    Method method = ANY;

    const Nih *nid = nullptr;
    const Nih *ih = nullptr;
    const Nih *target = nullptr;

    u32 tok_len = 0;
    const u8 *tok = nullptr;

    u32 n_nodes = 0;
    const PNode *nodes = nullptr;

    u32 n_peers = 0;
    const Peerinfo *peers = nullptr;

    u32 token_len = 0;
    const u8 *token = nullptr;

    u16 ap_port = 0; // nbo
    u32 ap_name_len = 0;
    const u8 *ap_name = nullptr;
    bool ap_implied_port = false;

  private:
    enum XDState {
        XD_START,
        XD_OKEY,
        XD_IKEY,
        XD_OVAL,
        XD_IVAL,
        XD_IVLIST,
    };

    struct XDHist {
        stat_t fail;
        // set when we find a key, expecting a particular value
        // set during the reading_dict_key phase
        Key current_key;
        Key seen_keys;
        // u32 legal_kinds
        Method msg_kind;
    };

  private:
    stat_t krpc_bdecode_atoi(u32 &dest, u32 &cur_pos, u32 data_len);

    void validate(const XDHist &);
    void xdecode(u32 data_len);

  public:
    void print();
    void clear() {
        status = ST__ST_ENUM_START;
        method = ANY;

        nid = nullptr;
        ih = nullptr;
        target = nullptr;

        // patiently awaiting std::span
        tok_len = 0;
        tok = nullptr;

        n_nodes = 0;
        nodes = nullptr;

        n_peers = 0;
        peers = nullptr;

        token_len = 0;
        token = nullptr;

        ap_port = 0; // nbo
        ap_name_len = 0;
        ap_name = nullptr;
        ap_implied_port = false;
    }
    void parse_msg(u32 nread) {
        // NO CLEAR
        status = ST_bd_a_no_error;
        xdecode(nread);
    }
};

class KReply {
  public:
    Nih nid;
    u8 tok_len;
    u8 tok[MAXLEN_TOK];

    KReply(const KRPC &krpc) : nid(*krpc.nid), tok_len(krpc.tok_len) {
        // If this trips we didn't initialize our KRPC properly, it's always
        // a programming error.
        assert(krpc.status != ST__ST_ENUM_START);
        memcpy(tok, krpc.tok, tok_len);
    }
};

} // namespace cht::bd
