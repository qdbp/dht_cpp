#pragma once

#include "dht.hpp"
#include <chrono>

// namespace chr = std::chrono;

using namespace cht;
namespace cht {

#ifndef STAT_CSV_FN
#define STAT_CSV_FN "./data/stat.csv"
#endif

#ifndef STAT_AUX_FN
#define STAT_AUX_FN "./data/stat_aux.csv"
#endif

#ifndef STAT_ROLLOVER_FREQ_MS
#define STAT_ROLLOVER_FREQ_MS 1000
#endif

// Save the csv data every X rollovers
#ifndef STAT_CSV_EVERY
#define STAT_CSV_EVERY 5
#endif

// Print a heartbeat message ever X rollovers
#ifndef STAT_HB_EVERY
#define STAT_HB_EVERY 60
#endif

#define FORSTAT(X)                                                             \
    X(_ST_ENUM_START)                                                          \
    /* control variable */                                                     \
    X(ctl_n_recv_bufs)                                                         \
    X(ctl_n_send_bufs)                                                         \
    X(ctl_n_gpm_bufs)                                                          \
    X(ctl_ping_window)                                                         \
    /* spam stats */                                                           \
    X(spam_size_ping)                                                          \
    X(spam_ping_overflow)                                                      \
    X(spam_size_q_gp)                                                          \
    X(spam_q_gp_overflow)                                                      \
    X(spam_size_rx)                                                            \
    X(spam_rx_overflow)                                                        \
    /* received message statistics */                                          \
    X(rx_spam)                                                                 \
    X(rx_tot)                                                                  \
    X(rx_err)                                                                  \
    X(rx_q_ap)                                                                 \
    X(rx_q_fn)                                                                 \
    X(rx_q_pg)                                                                 \
    X(rx_q_gp)                                                                 \
    X(rx_r_ap)                                                                 \
    X(rx_r_fn)                                                                 \
    X(rx_r_gp)                                                                 \
    X(rx_r_gp_nodes)                                                           \
    X(rx_r_gp_values)                                                          \
    X(rx_r_pg)                                                                 \
    /* transmitted message statistics */                                       \
    X(tx_msg_drop_early_error)                                                 \
    X(tx_msg_drop_late_error)                                                  \
    X(tx_msg_drop_bad_addr)                                                    \
    X(tx_ping_drop_spam) /* we don't want to spam either! */                   \
    X(tx_ping_drop_ctl)  /* we don't want to spam either! */                   \
    X(tx_tot)                                                                  \
    X(tx_err)                                                                  \
    X(tx_q_fn)                                                                 \
    X(tx_q_pg)                                                                 \
    X(tx_q_gp)                                                                 \
    X(tx_q_gp_drop_spam)                                                       \
    X(tx_r_ap)                                                                 \
    X(tx_r_fn)                                                                 \
    X(tx_r_gp)                                                                 \
    X(tx_r_pg)                                                                 \
    /* routing table constant */                                               \
    X(rt_replace_accept)                                                       \
    X(rt_replace_reject)                                                       \
    X(rt_replace_invalid)                                                      \
    X(rt_newnode_invalid)                                                      \
    X(rt_miss)                                                                 \
    /* database interaction statistics */                                      \
    X(gpm_ih_drop_buf_overflow)                                                \
    X(gpm_ih_drop_too_many_hops)                                               \
    X(gpm_ih_inserted)                                                         \
    X(gpm_r_gp_lookup_failed)                                                  \
    X(db_update_peers)                                                         \
    X(db_rows_inserted)                                                        \
    /* infohash lookup cycle statistics... mind these well */                  \
    X(ih_pursue_accept) /* ignore a q_gp infohash */                           \
    X(ih_pursue_reject) /* pursue '' */                                        \
    /* A: the message is accepted */                                           \
    X(bd_a_no_error)                                                           \
    /* X: the bdecoding is ill-formed or we can't handle the message at all */ \
    X(bd_x_msg_too_short)                                                      \
    X(bd_x_msg_too_long)                                                       \
    X(bd_x_bad_eom)                                                            \
    X(bd_x_bad_char)                                                           \
    X(bd_x_dict_is_key)                                                        \
    X(bd_x_list_is_key)                                                        \
    /* Y: the message violates KRPC norms */                                   \
    X(bd_y_bad_length_peer)                                                    \
    X(bd_y_bad_length_nodes)                                                   \
    X(bd_y_bad_length_ih)                                                      \
    X(bd_y_bad_length_nid)                                                     \
    X(bd_y_bad_length_target)                                                  \
    X(bd_y_inconsistent_type)                                                  \
    X(bd_y_no_nid)                                                             \
    X(bd_y_no_tok)                                                             \
    X(bd_y_port_overflow)                                                      \
    X(bd_y_ap_no_port)                                                         \
    X(bd_y_apgp_no_ih)                                                         \
    X(bd_y_empty_gp_response)                                                  \
    X(bd_y_fn_no_target)                                                       \
    X(bd_y_vals_wo_token)                                                      \
    /* Z: the message is valid, but is suspicious or uninteresting to us */    \
    X(bd_z_tok_too_long)                                                       \
    X(bd_z_bad_tok_fn)                                                         \
    X(bd_z_bad_tok_gp)                                                         \
    X(bd_z_bad_tok_pg)                                                         \
    X(bd_z_token_too_long)                                                     \
    X(bd_z_token_unrecognized)                                                 \
    X(bd_z_naked_value)                                                        \
    X(bd_z_rogue_int)                                                          \
    X(bd_z_unexpected_list)                                                    \
    X(bd_z_unknown_query)                                                      \
    X(bd_z_unknown_type)                                                       \
    X(bd_z_incongruous_message)                                                \
    X(bd_z_dicts_too_deep)                                                     \
    X(bd_z_ping_body)                                                          \
    X(bd_z_error_type)                                                         \
    X(bd_z_negative_int)                                                       \
    /* bad messages that pass bdecode but that we reject in handle*/           \
    X(bd_z_ap_bad_name)                                                        \
    X(bd_z_ap_bad_token)                                                       \
    X(bd_z_nodes_invalid)                                                      \
    X(bd_z_peers_bad)                                                          \
    X(bd_z_bullshit_dkad)                                                      \
    X(bd_z_evil_source) /* for any blacklisted nodes */                        \
    /* E: there is a programming error */                                      \
    X(err_bd_handle_fallthrough)                                               \
    X(err_bd_empty_r_gp)                                                       \
    X(err_rt_no_contacts)                                                      \
    X(err_rt_pulled_bad_node)                                                  \
    X(err_rx_exc)                                                              \
    X(_ST_ENUM_END)

#define AS_ENUM(x) ST_##x,
#define AS_STR(x) #x,
#define AS_STRLEN(x) (strlen(#x) - 1),

// const unsigned long ST_strlen[] = {FORSTAT(AS_STRLEN)};

enum stat_t { FORSTAT(AS_ENUM) };

constexpr inline const char *stat_names[] = {FORSTAT(AS_STR)};
// extern const char *stat_names[];

void st_inc(stat_t);
void st_dec(stat_t);
void st_inc_debug(stat_t);
void st_add(stat_t, u32);
void st_set(stat_t, u64);

// aux stats
void st_click_dkad(u8);
void st_click_gp_n_hops(u8);

u64 st_get(stat_t);
u64 st_get_old(stat_t);

void st_init();
void st_rollover();

std::chrono::time_point<std::chrono::steady_clock> now();
std::chrono::time_point<std::chrono::steady_clock> old();

} // namespace cht
