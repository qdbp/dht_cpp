#ifndef DHT_MSG_H
#define DHT_MSG_H

#include "bdecode.h"
#include "dht.h"
#include "rt.h"

#define MSG_BUF_LEN 512

void write_sid(char *, const nih_t);

u64 msg_q_gp(char *restrict buf, const nih_t nid, const nih_t ih, u16 tok);
u64 msg_q_fn(char *restrict buf, const pnode_t dest, const nih_t target);
u64 msg_q_pg(char *restrict buf, const nih_t nid);
u64 msg_r_fn(char *restrict buf, const parsed_msg *, const pnode_t);
u64 msg_r_gp(char *restrict buf, const parsed_msg *, const pnode_t);
u64 msg_r_pg(char *restrict buf, const parsed_msg *);

#endif // DHT_MSG_H
