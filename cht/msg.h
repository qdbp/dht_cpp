// vi:ft=cpp
#pragma once

#include "dht.h"
#include "krpc.h"
#include "rt.h"
#include "stat.h"

using namespace cht;
namespace cht::msg {

#ifndef MSG_CLOSE_SID
constexpr u8 SID4 = 0;
#else
constexpr u8 SID4 = 0xff;
#endif

#define MSG_BUF_LEN 256

static_assert(100 + bd::MAXLEN_TOK < MSG_BUF_LEN);

u32 q_gp(u8 buf[], const Nih &nid, const Nih &ih, u16 tok);
u32 q_fn(u8 buf[], const Nih &nid, const Nih &target);
u32 q_pg(u8 buf[], const Nih &nid);
u32 r_fn(u8 buf[], const bd::KReply &, const PNode &);
u32 r_gp(u8 buf[], const bd::KReply &, const PNode &);
u32 r_pg(u8 buf[], const bd::KReply &);

void init_msg(void);

} // namespace cht::msg
