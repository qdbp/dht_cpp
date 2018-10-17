#pragma once

#include "dht.h"
#include "stat.h"
#include <stdbool.h>
#include <unordered_set>
#include <vector>

using namespace cht;
namespace cht {

u64 randint(u64, u64);
bool is_valid_utf8(const unsigned char[], u64);
u8 dkad(const Nih &, const Nih &);

/// Manages N "tickets", meant to be indices into some resource array
template <u64 N, stat_t ACCT, stat_t OFLOW_ACCT = ST__ST_ENUM_END>
class Ticketer {
  private:
    constexpr static auto get_type() {
        if constexpr (N <= UINT8_MAX) {
            u8 x = 0;
            return x;
        } else if constexpr (N <= UINT16_MAX) {
            u16 x = 0;
            return x;
        } else if constexpr (N <= UINT32_MAX) {
            u32 x = 0;
            return x;
        } else {
            u64 x = 0;
            return x;
        }
    }

    using UX_T = decltype(get_type());

    std::unordered_set<UX_T> _free;
    std::unordered_set<UX_T> _taken;

    // std::vector<UX_T> _free = std::vector<UX_T>(N);

  public:
    Ticketer() {
        for (UX_T ix = 0; ix < N; ix++) {
            _free.insert(ix);
        }
    }

    std::pair<bool, UX_T> take() {
        if (!_free.empty()) {
            UX_T val = *_free.begin();
            _free.erase(val);
            _taken.insert(val);
            st_inc(ACCT);
            return {true, val};
        } else {
            if constexpr (OFLOW_ACCT != ST__ST_ENUM_END) {
                st_inc(OFLOW_ACCT);
            }
            return {false, 0};
        }
    }
    void give_back(UX_T val) {
        // All that two set hullabaloo just for this one line
        assert(_taken.count(val) == 1);
        _taken.erase(val);
        _free.insert(val);
        st_dec(ACCT);
    };
};

} // namespace cht
