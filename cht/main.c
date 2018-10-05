#include "bdecode.h"
#include "dht.h"
#include "gpmap.h"
#include "log.h"
#include "msg.h"
#include "rt.h"
#include "util.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/random.h>
#include <uv.h>

#define CHECK(r, msg)                                                          \
    if (r < 0) {                                                               \
        fprintf(stderr, "%s: %s\n", msg, uv_strerror(r));                      \
        exit(1);                                                               \
    }

#define DEFAULT_QUAL 1

#define N_RECV 1024
#define N_SEND 1024
#define MSG_SEND_LEN 512
#define MAX_N_SEND (3 * (N_SEND >> 2))
#define MAX_N_RECV (3 * (N_RECV >> 2))

// RECEIVE BUFFER MANAGEMENT
static char g_recv_bufs[N_RECV][BD_MAXLEN + sizeof(u32)] = {{0}};
static bool g_recv_bufs_in_use[N_RECV] = {0};
static u32 g_recv_bufs_num_in_use = 0;

// SEND BUFFER MANAGEMENT
static uv_udp_send_t g_requests[N_SEND] = {{0}};
static uv_buf_t g_send_buf_objs[N_SEND][1] = {{{0}}};
static char g_send_bufs[N_SEND][MSG_SEND_LEN] = {{0}};
static bool g_send_buf_objs_in_use[N_SEND] = {0};
static u32 g_send_buf_objs_num_in_use = 0;

// UV HANDLES
static uv_loop_t *main_loop;
static uv_udp_t g_udp_server;
static uv_timer_t g_statgather_timer;
static uv_timer_t g_bootstrap_timer;

static const rt_nodeinfo_t g_bootstrap_node = {
    .pnode.nid = {.raw =
                      {
                          '2',  0xf5, 'N', 'i',  's',  'Q',  0xff,
                          'J',  0xec, ')', 0xcd, 0xba, 0xab, 0xf2,
                          0xfb, 0xe3, 'F', '|',  0xc2, 'g',
                      }},
    .pnode.peerinfo.in_addr = 183949123,
    // the "reverse" of 6881
    .pnode.peerinfo.sin_port = 57626,
};

static int find_unused(const bool flags[], u32 num) {

    u32 start = randint(0, num);
    for (u32 ctr = start; ctr < num; ctr++) {
        if (!flags[ctr]) {
            return ctr;
        }
    }
    for (u32 ctr = 0; ctr < start; ctr++) {
        if (!flags[ctr]) {
            return ctr;
        }
    }
    WARN("Exhausted flag search! This is painful!")
    return -1;
};

static void handle_msg(parsed_msg *, const struct sockaddr_in *);

static inline void cb_alloc(uv_handle_t *client, size_t suggested_size,
                            uv_buf_t *buf) {

    int buf_ix;

    if ((g_recv_bufs_num_in_use > MAX_N_SEND) ||
        (buf_ix = find_unused(g_recv_bufs_in_use, N_RECV)) < 0) {
        DEBUG("No free recv buffers!")
        buf->len = 0;
        return;
    }

    g_recv_bufs_num_in_use++;
    g_recv_bufs_in_use[buf_ix] = true;
    st_set(ST_ctl_n_recv_bufs, g_recv_bufs_num_in_use);

    buf->base = g_recv_bufs[buf_ix] + sizeof(u32);
    buf->len = BD_MAXLEN;

    *(u32 *)(g_recv_bufs[buf_ix]) = (u32)buf_ix;
}

static void cb_recv_msg(uv_udp_t *handle, ssize_t nread, const uv_buf_t *rcvbuf,
                        const struct sockaddr *saddr, unsigned flags) {

    u32 buf_ix = *(u32 *)((u64)rcvbuf->base - sizeof(u32));

    g_recv_bufs_num_in_use--;
    g_recv_bufs_in_use[buf_ix] = false;

    if (saddr == NULL) {
        return;
    }

    if (nread < MIN_MSG_LEN) {
        st_inc(ST_bm_too_short);
        return;
    }

    parsed_msg rcvd = {0};
    stat_t bd_status = xdecode(rcvbuf->base, nread, &rcvd);

    if (bd_status != ST_bd_a_no_error) {
        st_inc(bd_status);
        return;
    }

    st_inc(ST_rx_tot);
    handle_msg(&rcvd, (const struct sockaddr_in *)saddr);
}

static void cb_send_msg(uv_udp_send_t *req, int status) {

    u32 buf_ix = (u64)req->data;

    assert(buf_ix < N_SEND);

    g_send_buf_objs_num_in_use -= 1;
    g_send_buf_objs_in_use[buf_ix] = false;

    if (status < 0) {
        st_inc(ST_tx_msg_drop_late_error);
    }
}

static inline bool send_msg(char *msg, u32 len, const struct sockaddr_in *dest,
                            stat_t acct) {
    int buf_ix;

    if (!validate_addr(dest->sin_addr.s_addr, dest->sin_port)) {
        st_inc(ST_tx_msg_drop_bad_addr);
        return false;
    }

    if (g_send_buf_objs_num_in_use > MAX_N_SEND ||
        (buf_ix = find_unused(g_send_buf_objs_in_use, N_SEND)) < 0) {
        DEBUG("No free send buffers!")
        st_inc(ST_tx_msg_drop_overflow);
        return false;
    }

    g_send_buf_objs_in_use[buf_ix] = true;
    g_send_buf_objs_num_in_use += 1;
    st_set(ST_ctl_n_send_bufs, g_send_buf_objs_num_in_use);

    g_requests[buf_ix].data = (void *)(size_t)buf_ix;

    assert(len <= MSG_SEND_LEN);
    memcpy(g_send_bufs[buf_ix], msg, len);
    g_send_buf_objs[buf_ix]->base = g_send_bufs[buf_ix];
    g_send_buf_objs[buf_ix]->len = len;

    int status =
        uv_udp_send(&g_requests[buf_ix], &g_udp_server, g_send_buf_objs[buf_ix],
                    1, (struct sockaddr *)dest, &cb_send_msg);

    if (status >= 0) {
        st_inc(acct);
        st_inc(ST_tx_tot);
        return true;
    } else {
        DEBUG("send failed (early): %s", uv_strerror(status))
        st_inc(ST_tx_msg_drop_early_error);
        return false;
    }
}

static inline void send_to_pnode(char *msg, u32 len, const pnode_t pnode,
                                 stat_t acct) {
    const struct sockaddr_in dest = {
        .sin_family = AF_INET,
        .sin_port = pnode.peerinfo.sin_port,
        .sin_addr.s_addr = pnode.peerinfo.in_addr,
    };
    send_msg(msg, len, &dest, acct);
}

void ping_sweep_nodes(const parsed_msg *krpc_msg) {
    char ping[MSG_BUF_LEN];
    u32 len;

    for (int ix = 0; ix < krpc_msg->n_nodes; ix++) {
        len = msg_q_pg(ping, krpc_msg->nodes[ix].nid);
        send_to_pnode(ping, len, krpc_msg->nodes[ix], ST_tx_q_pg);
    }
}

static void handle_msg(parsed_msg *krpc_msg, const struct sockaddr_in *saddr) {
    char reply[MSG_BUF_LEN] = {0};
    u32 len = 0;

    rt_nodeinfo_t *node;
    gpm_next_hop_t next_node = {{{0}}};
    u16 gp_tok;

    switch (krpc_msg->method) {
    case MSG_Q_PG:
        st_inc(ST_rx_q_pg);
        len = msg_r_pg(reply, krpc_msg);
        send_msg(reply, len, saddr, ST_tx_r_pg);

        rt_insert_contact(krpc_msg, saddr, 0);
        break;

    case MSG_Q_FN:
        st_inc(ST_rx_q_fn);

        node = rt_get_neighbor_contact(krpc_msg->nid);
        if (node != NULL) {
            len = msg_r_fn(reply, krpc_msg, node->pnode);
            send_msg(reply, len, saddr, ST_tx_r_fn);
        }
        rt_insert_contact(krpc_msg, saddr, 0);
        break;

    case MSG_Q_GP:
        st_inc(ST_rx_q_gp);

        if (gpm_decide_pursue_q_gp_ih(&gp_tok, krpc_msg)) {
            node = rt_get_neighbor_contact(krpc_msg->ih);
            if (!node) {
                break;
            }
            len = msg_q_gp(reply, node->pnode.nid, krpc_msg->ih, gp_tok);
            send_to_pnode(reply, len, node->pnode, ST_tx_q_gp);
            gpm_register_q_gp_ihash(node->pnode.nid, krpc_msg->ih, 0, gp_tok);
        }

        // reply to the sender node
        node = rt_get_neighbor_contact(krpc_msg->nid);
        if (node != NULL) {
            len = msg_r_fn(reply, krpc_msg, node->pnode);
            send_msg(reply, len, saddr, ST_tx_r_gp);
        }
        rt_insert_contact(krpc_msg, saddr, 1);
        break;

    case MSG_Q_AP:
        st_inc(ST_rx_q_ap);

        DEBUG("got q_ap!")

        // u16 ap_port;
        // if (krpc_msg->ap_implied_port) {
        //     ap_port = saddr->sin_port;
        // } else {
        //     ap_port = (u16)krpc_msg->ap_port;
        // }

        if (krpc_msg->ap_name_len > 0) {
            if (!is_valid_utf8((unsigned char *)(krpc_msg->ap_name),
                               krpc_msg->ap_name_len)) {
                st_inc(ST_bm_ap_bad_name);
                break;
            }
            // TODO handle AP with name
        }

        // TODO db_update_peers(ih, [compact_peerinfo_bytes(saddr[0],
        // ap_port)])
        len = msg_r_pg(reply, krpc_msg);
        send_msg(reply, len, saddr, ST_tx_r_ap);

        rt_insert_contact(krpc_msg, saddr, 3);
        break;

    case MSG_R_FN:
        st_inc(ST_rx_r_fn);
        if (krpc_msg->n_nodes == 0) {
            ERROR("Empty 'nodes' in R_FN")
            st_inc(ST_err_bd_empty_r_gp);
            break;
        }
        ping_sweep_nodes(krpc_msg);
        break;

    case MSG_R_GP:
        st_inc(ST_rx_r_gp);

        if (krpc_msg->n_peers > 0) {
            INFO("got r_gp peers!")
            st_inc(ST_rx_r_gp_values);
            // TODO handle peer

            rt_insert_contact(krpc_msg, saddr, 4);
        }

        if (krpc_msg->n_nodes > 0) {
            st_inc(ST_rx_r_gp_nodes);
            // TODO handle gp nodes
            ping_sweep_nodes(krpc_msg);

            if (gpm_extract_r_gp_ih(&next_node, krpc_msg)) {
                for (int ix = 0; ix < next_node.n_pnodes; ix++) {
                    if (!get_vacant_tok(&gp_tok)) {
                        goto end_pnodes_iter;
                    }
                    len = msg_q_gp(reply, next_node.pnodes[ix].nid,
                                   next_node.ih, gp_tok);
                    send_to_pnode(reply, len, next_node.pnodes[ix], ST_tx_q_gp);
                    gpm_register_q_gp_ihash(next_node.pnodes[ix].nid,
                                            next_node.ih, next_node.hop_ctr + 1,
                                            gp_tok);
                }
            end_pnodes_iter:;
            }
        }
        break;

    case MSG_R_PG:
        st_inc(ST_rx_r_pg);
        rt_insert_contact(krpc_msg, saddr, 2);
        break;

    default:
        ERROR("Unhandled krpc method name %s (%d)",
              get_method_name(krpc_msg->method), krpc_msg->method)
        st_inc(ST_err_bd_handle_fallthrough);
        break;
    }
}

void init_subsystems(void) {
    VERBOSE("Initializing rt...")
    rt_init();
    VERBOSE("Initializing st...")
    st_init();
    INFO("Initialized.")
}

void loop_statgather_cb(uv_timer_t *timer) {
    st_rollover();
    VERBOSE("Rolled over stats.")
}

void loop_bootstrap_cb(uv_timer_t *timer) {
    char msg[MSG_BUF_LEN];
    nih_t random_target;

    getrandom(random_target.raw, NIH_LEN, 0);

    u32 len = msg_q_fn(msg, g_bootstrap_node.pnode, random_target);
    send_to_pnode(msg, len, g_bootstrap_node.pnode, ST_tx_q_fn);

    // VERBOSE("Bootstrapped.")
}

int main(int argc, char *argv[]) {
    init_subsystems();

    int status;
    struct sockaddr_in addr;

    main_loop = uv_default_loop();

    // INITIALIZE UDP SERVER
    status = uv_udp_init(main_loop, &g_udp_server);
    CHECK(status, "init");

    uv_ip4_addr("0.0.0.0", 6881, &addr);

    status = uv_udp_bind(&g_udp_server, (const struct sockaddr *)&addr, 0);
    CHECK(status, "bind");

    status = uv_udp_recv_start(&g_udp_server, cb_alloc, cb_recv_msg);
    CHECK(status, "recv");

    // INIT statgather
    status = uv_timer_init(main_loop, &g_statgather_timer);
    CHECK(status, "statgather timer init");
    //     status =
    uv_timer_start(&g_statgather_timer, &loop_statgather_cb, 6000, 6000);
    CHECK(status, "statgather start")

    // INIT BOOTSTRAP
    status = uv_timer_init(main_loop, &g_bootstrap_timer);
    CHECK(status, "bootstrap timer init");
    status = uv_timer_start(&g_bootstrap_timer, &loop_bootstrap_cb, 100, 50);

    CHECK(status, "boostrap start")

    // RUN LOOP
    INFO("Starting loop.")
    uv_run(main_loop, UV_RUN_DEFAULT);

    return 0;
}
