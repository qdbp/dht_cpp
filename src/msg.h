#ifndef DHT_MSG_H
#define DHT_MSG_H

#include "bdecode.h"
#include "dht.h"
#include "rt.h"

#define MSG_BUF_LEN 512

void write_sid(char *, const char *);

u64 msg_q_gp(char *restrict buf, char nid[20], char ih[20], u16 tok);
u64 msg_q_fn(char *restrict buf, const rt_nodeinfo_t *dest, const char *);
u64 msg_q_pg(char *restrict buf, const char *nid);
u64 msg_r_fn(char *restrict buf, const parsed_msg *, const rt_nodeinfo_t *);
u64 msg_r_gp(char *restrict buf, const parsed_msg *, const rt_nodeinfo_t *);
u64 msg_r_pg(char *restrict buf, const parsed_msg *);

#endif // DHT_MSG_H
