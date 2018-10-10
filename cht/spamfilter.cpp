extern "C" {
#include "dht.h"
#include "log.h"
#include "spamfilter.h"
#include "stat.h"
#include "util.h"
}
#include <cassert>

template <typename K>
struct rateinfo_s {
    rateinfo_s<K> *next;
    K key;
    u8 n_tokens;
};

/// Generate the optimal N-bit hash of (uniformly distributed...) u32s
template <unsigned int N>
static inline u32 hash_key(u32 key) {
    static_assert(N < 32);
    return key >> (32 - N);
}

template <typename K, u8 N, u8 M, stat_t account>
class Spamtable {
  private:
    // TODO replace with std::forward_list;
    rateinfo_s<K> headarr[1 << N];

  public:
    const u8 keywidth = N;
    const u8 max_tokens = M;
    const stat_t acct = account;

    /// Walks the linked list hash table address `table`, looking for the
    /// address entry `saddr`. If it is found, add `delta` tokens to that
    /// entry's token value. If the result would be less than or equal to zero,
    /// return false. Else replace the token value with the sum and return true.
    /// If the entry is not found, create a new entry with initial token value
    /// `max_tokens` + delta. Returns -1 if there are not enough tokens, else
    /// the remaining number of tokens.
    i32 transact(K key, i8 delta) {

        // Withdrawing more than the allowed max would always fail, we don't
        // need a node.
        static bool has_warned = false;
        if (M + delta < 0) {
            return -1;
            if (!has_warned) {
                WARN("Called with a widrawal %d on spam account %s with "
                     "max_tokens "
                     "%d",
                     delta, stat_names[acct], max_tokens)
                has_warned = true;
            }
        }

        u8 bucket = hash_key<N>(key);

        auto *const head = &(headarr[bucket]);
        auto *last = head;
        auto *curr = last->next;

        while (curr) {
            if (curr->key == key) {
                assert(curr->n_tokens <= max_tokens);
                // We're out of tokens! Block the operation.
                if (curr->n_tokens + delta < 0) {
                    return -1;
                }
                // We have >= max tokens! Remove curr node and return the max.
                if (curr->n_tokens + delta >= max_tokens) {
                    last->next = curr->next;
                    delete curr;
                    st_dec(acct);
                    return max_tokens;
                }
                // We are "in range", do the adjustment, return the remaining
                // tokens.
                curr->n_tokens += delta;
                // Rotate the node to the front
                if (head != last) {
                    // H->[x*]->L->T->[y*]
                    last->next = curr->next;
                    // H->[x*]->L--->[y*] ; T->[y*]
                    curr->next = head->next;
                    // H->[x*]->L->[y*] ; T->[x*]
                    head->next = curr;
                    // H->T->[x*]->L->[y*]
                }
                return curr->n_tokens;
            }
            last = curr;
            curr = curr->next;
        }

        // We didn't find a node

        // New nodes have max_tokens implicitly - if we would have more than
        // that, the node would be poppable immediately; so we just do nothing.
        if (delta >= 0) {
            return max_tokens;
        }

        // Otherwise we insert a new node and subtract the delta to start.
        auto new_node = new rateinfo_s<K>;

        st_inc(acct);

        new_node->key = key;
        new_node->n_tokens = max_tokens + delta;
        // insert the node at the front
        new_node->next = head->next;
        head->next = new_node;

        return max_tokens + delta;
    }

    void sweep(u8 delta) {

        for (int ix = 0; ix < (1 << keywidth); ix++) {

            rateinfo_s<K> *last = &(headarr[ix]);
            rateinfo_s<K> *curr = last->next;

            while (curr) {
                if (curr->n_tokens + delta >= max_tokens) {
                    last->next = curr->next;
                    delete curr;
                    st_dec(acct);
                    /* last = last */
                } else {
                    curr->n_tokens += delta;
                    last = curr;
                }
                curr = last->next;
            }
        }
    }
};

// The tablesize values are empirically determined
static auto g_rx_spamtable = Spamtable<u32, 8, 5, ST_spam_size_rx>();
static auto g_ping_spamtable = Spamtable<u32, 10, 20, ST_spam_size_ping>();
static auto g_gp_spamtable = Spamtable<u32, 12, 20, ST_spam_size_q_gp>();

// PUBLIC FUNCTIONS

extern "C" {

void spam_run_epoch(void) {
    g_rx_spamtable.sweep(1);
    g_ping_spamtable.sweep(1);
    g_gp_spamtable.sweep(1);
}

bool spam_check_rx(u32 saddr_ip) {
    auto val = g_rx_spamtable.transact(saddr_ip, -1);
    if (val < 0) {
        return false;
    }
    return true;
}

bool spam_check_tx_ping(u32 saddr_ip) {
    auto val = g_ping_spamtable.transact(saddr_ip, -20);
    if (val < 0) {
        return false;
    }
    return true;
}

bool spam_check_tx_q_gp(u32 saddr_ip, nih_t ih) {
    auto key = saddr_ip ^ ih.checksum;
    auto val = g_gp_spamtable.transact(key, -20);
    if (val < 0) {
        return false;
    }
    return true;
}
}
