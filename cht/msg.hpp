// vi:ft=cpp
#pragma once

#include "dht.hpp"
#include "krpc.hpp"
#include "rt.hpp"
#include "stat.hpp"

using namespace cht;
namespace cht::msg {

#ifndef MSG_CLOSE_SID
constexpr inline u8 SID4 = 0;
#else
constexpr inline u8 SID4 = 0xff;
#endif

#define MSG_BUF_LEN 256

static_assert(100 + bd::MAXLEN_TOK < MSG_BUF_LEN);

i32 q_gp(u8 buf[], const Nih &nid, const Nih &ih, u16 tok);
i32 q_fn(u8 buf[], const Nih &nid, const Nih &target);
i32 q_pg(u8 buf[], const Nih &nid);
i32 r_fn(u8 buf[], const bd::KReply &, const PNode &);
i32 r_gp(u8 buf[], const bd::KReply &, const PNode &);
i32 r_pg(u8 buf[], const bd::KReply &);

void init_msg();

} // namespace cht::msg
