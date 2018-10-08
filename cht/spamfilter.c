#include "dht.h"
#include "log.h"
#include "spamfilter.h"
#include "stat.h"
#include <assert.h>
#include <stdlib.h>

#define SPAM_TABLE_SIZE 256
#define SPAM_RX_MAX_TOKENS 10
#define SPAM_PING_MAX_TOKENS 15

// TODO split into rx_ and tx_ tokens?
typedef struct rateinfo_s {
    struct rateinfo_s *next;
    u32 key;
    u8 n_tokens;
} rateinfo_t;

rateinfo_t g_spam_rx_map[SPAM_TABLE_SIZE] = {{0}};
rateinfo_t g_spam_ping_map[SPAM_TABLE_SIZE] = {{0}};

static inline u8 hash_key(u32 in_addr) {
    return in_addr >> 24;
}

/// Walks the linked list hash table address `table`, looking for the address
/// entry `saddr`. If it is found, add `delta` tokens to that entry's token
/// value. If the result would be less than or equal to zero, return false. Else
/// replace the token value with the sum and return true. If the entry is not
/// found, create a new entry with initial token value `max_tokens` + delta.
/// Returns -1 if there are not enough tokens, else the remaining number of
/// tokens.
static inline i32 spam__transact(u32 key, rateinfo_t table[SPAM_TABLE_SIZE],
                                 i8 delta, u8 max_tokens, stat_t acct) {

    // Withdrawing more than the allowed max would always fail, we don't
    // need a node.
    static bool has_warned = false;
    if (max_tokens + delta < 0) {
        return -1;
        if (!has_warned) {
            WARN("Called with a widrawal %d on spam account %s with max_tokens "
                 "%d",
                 delta, stat_names[acct], max_tokens)
            has_warned = true;
        }
    }

    u8 bucket = hash_key(key);

    rateinfo_t *const head = &(table[bucket]);
    rateinfo_t *last = head;
    rateinfo_t *this = last->next;

    DEBUG("this %p; last %p; head %p; delta %d", this, last, head, delta)

    while (this) {
        if (this->key == key) {
            assert(this->n_tokens <= max_tokens);
            // We're out of tokens! Block the operation.
            if (this->n_tokens + delta < 0) {
                return -1;
            }
            // We have >= max tokens! Remove this node and return the max.
            else if (this->n_tokens + delta >= max_tokens) {
                last->next = this->next;
                free(this);
                st_dec(acct);
                return max_tokens;
            }
            // We are "in range", do the adjustment, return the remaining
            // tokens.
            else {
                this->n_tokens += delta;
                // Rotate the node to the front
                if (head != last) {
                    // H->[x*]->L->T->[y*]
                    last->next = this->next;
                    // H->[x*]->L--->[y*] ; T->[y*]
                    this->next = head->next;
                    // H->[x*]->L->[y*] ; T->[x*]
                    head->next = this;
                    // H->T->[x*]->L->[y*]
                }
                return this->n_tokens;
            }
        }
        last = this;
        this = this->next;
    }

    // We didn't find a node

    // New nodes have max_tokens implicitly - if we would have more than that,
    // the node would be poppable immediately; so we just do nothing.
    if (delta >= 0) {
        return max_tokens;
    }

    // Otherwise we insert a new node and subtract the delta to start.
    rateinfo_t *new_node = (rateinfo_t *)malloc(sizeof(rateinfo_t));
    st_inc(acct);

    new_node->key = key;
    new_node->n_tokens = max_tokens + delta;
    // insert the node at the front
    new_node->next = head->next;
    head->next = new_node;

    return max_tokens + delta;
}

static void spam__sweep(rateinfo_t table[SPAM_TABLE_SIZE], u8 delta,
                        u8 max_tokens, stat_t acct) {

    for (int ix = 0; ix < SPAM_TABLE_SIZE; ix++) {
        rateinfo_t *last = &(table[ix]);
        rateinfo_t *this = last->next;

        while (this) {
            if (this->n_tokens + delta >= max_tokens) {
                last->next = this->next;
                free(this);
                st_dec(acct);
                /* last = last */
            } else {
                this->n_tokens += delta;
                last = this;
            }
            this = last->next;
        }
    }
}

// PUBLIC FUNCTIONS

void spam_run_epoch(void) {
    spam__sweep(g_spam_rx_map, 1, SPAM_RX_MAX_TOKENS, ST_spam_size_rx);
    spam__sweep(g_spam_ping_map, 1, SPAM_PING_MAX_TOKENS, ST_spam_size_ping);
}

bool spam_check_rx(u32 saddr_ip) {
    i32 val = spam__transact(saddr_ip, g_spam_rx_map, -1, SPAM_RX_MAX_TOKENS,
                             ST_spam_size_rx);
    if (val < 0) {
        return false;
    }
    return true;
}

bool spam_check_tx_ping(u32 saddr_ip) {
    i32 val = spam__transact(saddr_ip, g_spam_ping_map, -15,
                             SPAM_PING_MAX_TOKENS, ST_spam_size_ping);
    if (val < 0) {
        return false;
    }
    return true;
}
