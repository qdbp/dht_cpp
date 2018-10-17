#include "dht.h"
#include "log.h"
#include "spamfilter.h"
#include "stat.h"
#include "util.h"

#include <array>
#include <cassert>
#include <unordered_map>

using namespace cht;
namespace cht {

template <u8 MAX_TOKENS, stat_t ACCT>
class CTMap {
  private:
    std::unordered_map<u32, u8> map;

  public:
    template <u8 DELTA>
    bool withdraw(u32 key) {
        static_assert(MAX_TOKENS >= DELTA);

        const auto &it = map.find(key);

        if (it == map.end()) {
            map.insert({key, MAX_TOKENS - DELTA});
            return true;
        }
        if (it->second < DELTA) {
            return false;
        } else {
            it->second -= DELTA;
            return true;
        }
    }

    template <u8 DELTA>
    void replenish() {
        static_assert(u32(DELTA) + u32(MAX_TOKENS) <= UINT8_MAX);

        static u8 rehash = 0;

        auto it = map.begin();

        while (it != map.end()) {

            if (it->second + DELTA >= MAX_TOKENS) {
                it = map.erase(it);
            } else {
                it->second += DELTA;
                it++;
            }
        }

        st_set(ACCT, map.size());

        if ((rehash++) == UINT8_MAX) {
            map.rehash(map.bucket_count() / 2 + 1);
            VERBOSE("Rehashed map [acct = %s]", stat_names[ACCT]);
        }
    }
}; // namespace cht

static auto g_rx_spamtable = CTMap<12, ST_spam_size_rx>();
static auto g_pg_spamtable = CTMap<15, ST_spam_size_ping>();
static auto getpeers_spamtable = CTMap<15, ST_spam_size_q_gp>();

// PUBLIC FUNCTIONS

void spam_run_epoch(void) {
    g_rx_spamtable.replenish<4>();
    g_pg_spamtable.replenish<1>();
    getpeers_spamtable.replenish<1>();
}

bool spam_check_rx(u32 saddr_ip) {
    bool out = g_rx_spamtable.withdraw<1>(saddr_ip);
    if (!out) {
        st_inc(ST_rx_spam);
    }
    return out;
}

bool spam_check_tx_pg(u32 daddr_ip) {
    bool out = g_pg_spamtable.withdraw<15>(daddr_ip);
    if (!out) {
        st_inc(ST_tx_ping_drop_spam);
    }
    return out;
}

bool spam_check_tx_q_gp(u32 daddr_ip, const Nih &ih) {
    bool out = getpeers_spamtable.withdraw<15>(ih.checksum ^ daddr_ip);
    if (!out) {
        st_inc(ST_tx_q_gp_drop_spam);
    }
    return out;
}
} // namespace cht
