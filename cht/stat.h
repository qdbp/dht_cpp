// vi:ft=c
#ifndef DHT_STAT_C
#define DHT_STAT_C

#include "dht.h"
#include <time.h>

#ifndef STAT_CSV_FN
#define STAT_CSV_FN "./data/stat.csv"
#endif

#ifndef STAT_CSV_FN
#define STAT_DKAD_FN "./data/dkad.csv"
#endif

#ifndef STAT_ROLLOVER_FREQ_MS
#define STAT_ROLLOVER_FREQ_MS 1000
#endif

#ifndef STAT_CSV_EVERY
#define STAT_CSV_EVERY 5
#endif

#define FORSTAT(X)                                                             \
    X(_ST_ENUM_START)                                                          \
    /* control variable */                                                     \
    X(ctl_n_recv_bufs)                                                         \
    X(ctl_n_send_bufs)                                                         \
    X(ctl_n_gpm_bufs)                                                          \
    X(ctl_ping_thresh)                                                         \
    /* received message statistics */                                          \
    X(rx_tot)                                                                  \
    X(rx_oserr)                                                                \
    X(rx_err_received)                                                         \
    X(rx_q_ap)                                                                 \
    X(rx_q_fn)                                                                 \
    X(rx_q_pg)                                                                 \
    X(rx_q_gp)                                                                 \
    X(rx_r_ap)                                                                 \
    X(rx_r_fn)                                                                 \
    X(rx_r_gp)                                                                 \
    X(rx_r_pg)                                                                 \
    X(rx_r_gp_nodes)                                                           \
    X(rx_r_gp_values)                                                          \
    /* transmitted message statistics */                                       \
    X(tx_tot)                                                                  \
    X(tx_exc)                                                                  \
    X(tx_msg_drop_overflow)                                                    \
    X(tx_msg_drop_early_error)                                                 \
    X(tx_msg_drop_late_error)                                                  \
    X(tx_msg_drop_bad_addr)                                                    \
    X(tx_q_ap)                                                                 \
    X(tx_q_fn)                                                                 \
    X(tx_q_pg)                                                                 \
    X(tx_q_gp)                                                                 \
    X(tx_r_ap)                                                                 \
    X(tx_r_fn)                                                                 \
    X(tx_r_gp)                                                                 \
    X(tx_r_pg)                                                                 \
    /* bad messages that pass bdecode but that we reject */                    \
    X(bm_too_short)                                                            \
    X(bm_ap_bad_name)                                                          \
    X(bm_ap_bad_token)                                                         \
    X(bm_nodes_invalid)                                                        \
    X(bm_peers_bad)                                                            \
    X(bm_bullshit_dkad)                                                        \
    X(bm_evil_source)                                                          \
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
    X(gpm_r_gp_lookup_empty)                                                   \
    X(gpm_r_gp_bad_checksum)                                                   \
    X(db_update_peers)                                                         \
    X(db_rows_inserted)                                                        \
    /* infohash lookup cycle statistics... mind these well */                  \
    X(ih_pursue_accept) /* ignore a q_gp infohash */                           \
    X(ih_pursue_reject) /* pursue '' */                                        \
    /* A: the message is accepted */                                           \
    X(bd_a_no_error)                                                           \
    /* X: the bdecoding is ill-formed or we can't handle the message at all */ \
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
    X(bd_z_tok_unrecognized)                                                   \
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

typedef enum stat_t { FORSTAT(AS_ENUM) } stat_t;

extern const char *stat_names[];

void st_inc(stat_t);
void st_inc_debug(stat_t);
void st_add(stat_t, u32);
void st_set(stat_t, u64);
void st_click_dkad(u8);

u64 st_get(stat_t);
u64 st_get_old(stat_t);

void st_init(void);
void st_rollover(void);

extern u64 st_now_ms;
extern u64 st_old_ms;

#endif // DHT_STAT_C
