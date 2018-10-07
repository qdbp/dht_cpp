#include "dht.h"
#include "log.h"
#include "spamfilter.h"
#include "stat.h"
#include <stdlib.h>

#define SPAM_TABLE_SIZE 256
#define SPAM_MAX_TOKENS 5

_Static_assert(SPAM_MAX_TOKENS > 1, "bad max tokens");
_Static_assert(SPAM_MAX_TOKENS < UINT8_MAX - 1, "bad max tokens");

// TODO split into rx_ and tx_ tokens?
typedef struct rateinfo_s {
    u32 in_addr;
    u8 n_tokens;
    struct rateinfo_s *next;
} rateinfo_t;

// mapped by the lower two bytes (in network order) of the address
// SLL in each bucket
rateinfo_t g_rateinfo_map[SPAM_TABLE_SIZE] = {{0}};

static inline u8 hash_inaddr(u32 in_addr) {
    return in_addr & 0xff;
}

void spam_run_epoch(void) {
    DEBUG("EPOCH")
    for (int ix = 0; ix < SPAM_TABLE_SIZE; ix++) {
        rateinfo_t *this = &(g_rateinfo_map[ix]);
        rateinfo_t *temp;
        u8 new_tokens;
        while (this->next) {
            DEBUG("NEW TOKENS")
            new_tokens = this->next->n_tokens + 1;
            if (new_tokens > SPAM_MAX_TOKENS) {
                temp = this->next;
                this->next = this->next->next;
                DEBUG("FREE")
                free(temp);
                st_inc(ST_spam_free);
            } else {
                this->next->n_tokens = new_tokens;
                this = this->next;
            }
        }
    }
}

bool spam_check_rx(const struct sockaddr_in *saddr) {

    u8 bucket = hash_inaddr(saddr->sin_addr.s_addr);
    rateinfo_t *this = &(g_rateinfo_map[bucket]);
    rateinfo_t *last = this;

    while (this->next) {
        if (this->next->in_addr == saddr->sin_addr.s_addr) {
            if (this->next->n_tokens == 0) {
                DEBUG("SPAM!")
                return false;
            } else {
                this->next->n_tokens--;
                DEBUG("DEC TO %d", this->next->n_tokens)
                return true;
            }
        }
        last = this;
        this = this->next;
    }
    rateinfo_t *new_node = (rateinfo_t *)malloc(sizeof(rateinfo_t));

    new_node->next = NULL;
    new_node->in_addr = saddr->sin_addr.s_addr;
    new_node->n_tokens = SPAM_MAX_TOKENS - 1;

    last->next = new_node;
    return true;
}
