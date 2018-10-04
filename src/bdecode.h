#ifndef DHT_BDECODE_H
#define DHT_BDECODE_H

#include "dht.h"
#include "stat.h"
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// BDECODE SIZES
#define BD_MAXLEN 1024
#define BD_MAXLEN_AP_NAME 256
#define BD_MAXLEN_TOK 32
#define BD_MAXLEN_TOKEN 32
#define BD_MAX_PEERS 36
#define BD_MAX_NODES 8

// bdecode message types
typedef enum bd_meth_t {
    MSG_Q_AP = 1u,
    MSG_Q_FN = 1u << 1u,
    MSG_Q_GP = 1u << 2u,
    MSG_Q_PG = 1u << 3u,

    MSG_R_FN = 1u << 5u,
    MSG_R_GP = 1u << 6u,
    MSG_R_PG = 1u << 7u,
} bd_meth_t;

#define BD_NOKEY 0
#define BD_IKEY_VALUES 1u
#define BD_IKEY_NODES 1u << 1u
#define BD_IKEY_TOKEN 1u << 2u
#define BD_IKEY_IH 1u << 3u
#define BD_IKEY_NID 1u << 4u
#define BD_IKEY_TARGET 1u << 5u
#define BD_IKEY_IMPLPORT 1u << 6u
#define BD_IKEY_PORT 1u << 7u
#define BD_IKEY_AP_NAME 1u << 8u
#define BD_OKEY_A 1u << 9u
#define BD_OKEY_T 1u << 10u
#define BD_OKEY_Q 1u << 11u
#define BD_OKEY_R 1u << 12u
#define BD_OKEY_Y 1u << 13u

#define BD_IKEY_ANY_BODY                                                       \
    (BD_IKEY_NODES | BD_IKEY_VALUES | BD_IKEY_IH | BD_IKEY_TARGET |            \
     BD_IKEY_TOKEN)

#define BD_IKEY_ANY_NON_TOKEN_BODY                                             \
    (BD_IKEY_NODES | BD_IKEY_VALUES | BD_IKEY_IH | BD_IKEY_TARGET)

#define R_ANY (MSG_R_FN | MSG_R_GP | MSG_R_PG)
#define Q_ANY (MSG_Q_AP | MSG_Q_FN | MSG_Q_GP | MSG_Q_PG)

typedef struct parsed_msg {
    bd_meth_t method;
    nih_t nid;
    nih_t ih;
    char tok[BD_MAXLEN_TOK];
    u32 tok_len;
    nih_t target;
    pnode_t nodes[BD_MAX_NODES];
    u32 n_nodes;
    peerinfo_t peers[BD_MAX_PEERS];
    u32 n_peers;
    char token[BD_MAXLEN_TOKEN];
    u32 token_len;
    u16 ap_port; // nbo
    bool ap_implied_port;
    u32 ap_name_len;
    char ap_name[BD_MAXLEN_AP_NAME];
} parsed_msg;

typedef struct bd_state {
    u32 fail;
    // set when we find a key, expecting a particular value
    // set during the reading_dict_key phase
    u32 current_key;
    u32 seen_keys;
    // u32 legal_kinds
    bd_meth_t msg_kind;
    bool save_ap_port;
} bd_state;

stat_t xdecode(const char *, u32, parsed_msg *);

void print_parsed_msg(parsed_msg *);
const char *get_method_name(bd_meth_t);

#endif // DHT_BDECODE_H
