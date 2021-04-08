#include "ctl.hpp"
#include "dht.hpp"
#include "gpmap.hpp"
#include "krpc.hpp"
#include "log.hpp"
#include "msg.hpp"
#include "rt.hpp"
#include "spamfilter.hpp"
#include "util.hpp"

#include <functional>
#include <vector>

#include <sys/random.h>
extern "C" {
#include <uv.h>
}

#define ASIO_STANDALONE
#include <asio.hpp>

#include <locale>

using namespace cht;
using bd::KRPC;
using rt::g_rt;

using SIN = struct sockaddr_in;

namespace cht {

#define CHECK(r, msg)                                                          \
    if ((r) < 0) {                                                             \
        fprintf(stderr, "%s: %s\n", msg, uv_strerror(r));                      \
        exit(1);                                                               \
    }

#define AS_SIN(x) ((const SIN *)(x))

constexpr u32 MSG_SEND_LEN = 256;

// UV HANDLES
static uv_loop_t *main_loop;
static uv_udp_t g_udp_server;
static uv_timer_t g_statgather_timer;
static uv_timer_t g_bootstrap_timer;

static std::vector<KRPC *> recv_buf_pool;
static std::vector<uv_udp_send_t *> send_req_pool;
static std::unordered_map<char *, KRPC *> krpc_map;

using msg_write_t = std::function<void(uv_buf_t &)>;

static void handle_msg(const KRPC &, const SIN &);

static inline void cb_alloc(uv_handle_t *client, size_t suggested_size,
                            uv_buf_t *buf) {

    if (suggested_size == 0) {
        buf->len = 0;
        return;
    }

    KRPC *krpc;

    if (!recv_buf_pool.empty()) {
        krpc = recv_buf_pool.back();
        krpc->clear();
    } else {
        krpc = new KRPC();
        st_inc(ST_ctl_n_recv_bufs);
    }

    buf->base = reinterpret_cast<char *>(const_cast<u8 *>(krpc->data));
    buf->len = bd::MAXLEN;

    krpc_map.insert({buf->base, krpc});
}

static void cb_recv_msg(uv_udp_t *handle, ssize_t nread, const uv_buf_t *buf,
                        const struct sockaddr *saddr, unsigned flags) {

    const auto &it = krpc_map.find(buf->base);
    assert(it != krpc_map.end());

    KRPC *const &krpc = it->second;

    if (saddr == nullptr || nread == 0) {
        goto cb_recv_msg_early_free;
    }

    if (nread < 0) {
        st_inc(ST_rx_err);
        DEBUG("%s", uv_strerror(nread))
        goto cb_recv_msg_early_free;
    }

    if (nread < MIN_MSG_LEN) {
        st_inc(ST_bm_too_short);
        goto cb_recv_msg_early_free;
    }

    // We probably clipped.
    if (nread == bd::MAXLEN) {
        st_inc(ST_bd_x_msg_too_long);
        goto cb_recv_msg_early_free;
    }

    if (!spam_check_rx(AS_SIN(saddr)->sin_addr.s_addr)) {
        goto cb_recv_msg_early_free;
    }

    // Where the magic happens. the data buffer that was written to krpc is
    // parsed.
    krpc->parse_msg(nread);

    if (krpc->status != ST_bd_a_no_error) {
        st_inc(krpc->status);
        return;
    }

    st_inc(ST_rx_tot);
    handle_msg(*krpc, *AS_SIN(saddr));

cb_recv_msg_early_free:
    recv_buf_pool.push_back(krpc);
}

static void cb_send_msg(uv_udp_send_t *req, int status) {

    if (status < 0) {
        st_inc(ST_tx_msg_drop_late_error);
    }

    send_req_pool.push_back(req);
}

static inline bool send_msg(const msg_write_t &write_fn, const SIN &dest,
                            stat_t acct) {

    if (!rt::validate_addr(dest.sin_addr.s_addr, dest.sin_port)) {
        st_inc(ST_tx_msg_drop_bad_addr);
        return false;
    }

    uv_udp_send_t *req;
    uv_buf_t(*bufarr)[1];

    if (send_req_pool.empty()) {
        bufarr = reinterpret_cast<uv_buf_t(*)[1]>(new uv_buf_t[1]);
        auto recvarr = new char[MSG_SEND_LEN];
        bufarr[0]->base = recvarr;
        req = new uv_udp_send_t{
            .data = reinterpret_cast<void *>(bufarr),
        };
        st_inc(ST_ctl_n_send_bufs);
    } else {
        req = send_req_pool.back();
        send_req_pool.pop_back();
        bufarr = reinterpret_cast<uv_buf_t(*)[1]>(req->data);
    }

    write_fn(*bufarr[0]);

    int status = uv_udp_send(req, &g_udp_server, *bufarr, 1,
                             (struct sockaddr *)&dest, &cb_send_msg);

    if (status >= 0) {
        st_inc(acct);
        st_inc(ST_tx_tot);
        return true;
    } else {
        st_inc(ST_tx_msg_drop_early_error);
        return false;
    }
}

static inline bool send_msg(const msg_write_t &write_fn, const PNode &pnode,
                            stat_t acct) {
    const SIN dest = {
        .sin_family = AF_INET,
        .sin_port = pnode.peerinfo.sin_port,
        .sin_addr.s_addr = pnode.peerinfo.in_addr,
    };
    return send_msg(write_fn, dest, acct);
}

void ping_sweep_nodes(const KRPC &krpc) {
    for (int ix = 0; ix < krpc.n_nodes; ix++) {

        if (!ctl_decide_ping(krpc.nodes[ix].nid)) {
            st_inc(ST_tx_ping_drop_ctl);
            continue;
        }
        if (!spam_check_tx_pg(krpc.nodes[ix].peerinfo.in_addr)) {
            continue;
        }

        msg_write_t write_fn = [nid =
                                    krpc.nodes[ix].nid](uv_buf_t &buf) -> void {
            buf.len = msg::q_pg(reinterpret_cast<u8 *>(buf.base), nid);
        };

        send_msg(write_fn, krpc.nodes[ix], ST_tx_q_pg);
    }
}

static void handle_msg(const KRPC &krpc, const SIN &saddr) {

    gpm::NextHop next_hop;

    switch (krpc.method) {
    case bd::Q_PG: {
        st_inc(ST_rx_q_pg);

        auto const &write_fn = [krp = bd::KReply(krpc)](auto &buf) {
            buf.len = msg::r_pg(reinterpret_cast<u8 *>(buf.base), krp);
        };

        send_msg(write_fn, saddr, ST_tx_r_pg);
        g_rt.insert_contact(krpc, saddr, 0);

        break;
    }

    case bd::Q_FN: {
        st_inc(ST_rx_q_fn);

        auto payload = g_rt.get_neighbor_contact(*krpc.target);

        auto const &write_fn = [krp = bd::KReply(krpc), payload](auto &buf) {
            buf.len = msg::r_fn(reinterpret_cast<u8 *>(buf.base), krp, payload);
        };

        send_msg(write_fn, saddr, ST_tx_r_fn);
        g_rt.insert_contact(krpc, saddr, 0);

        break;
    }

    case bd::Q_GP: {
        st_inc(ST_rx_q_gp);

        auto [pursue, tok] = gpm::decide_pursue_q_gp_ih(krpc);
        auto ih_neig = g_rt.get_neighbor_contact(*krpc.ih);

        if (pursue) {

            if (ih_neig.nid.checksum == krpc.nid->checksum) {
                ih_neig = g_rt.get_random_valid_node();
            }

            auto const &write_fn = [=, tok = tok, ih = *krpc.ih](auto &buf) {
                buf.len = msg::q_gp(reinterpret_cast<u8 *>(buf.base),
                                    ih_neig.nid, ih, tok);
            };

            send_msg(write_fn, ih_neig, ST_tx_q_gp);
            gpm::register_q_gp_ihash(ih_neig.nid, *krpc.ih, 0, tok);
        }

        // reply to the sender node

        auto const &write_fn = [=, krp = bd::KReply(krpc)](auto &buf) {
            buf.len = msg::r_gp(reinterpret_cast<u8 *>(buf.base), krp, ih_neig);
        };

        send_msg(write_fn, saddr, ST_tx_r_gp);
        g_rt.insert_contact(krpc, saddr, 1);

        break;
    }

    case bd::Q_AP: {
        st_inc(ST_rx_q_ap);

        DEBUG("got q_ap!")

        // u16 ap_port;
        // if (krpc->ap_implied_port) {
        //     ap_port = saddr->sin_port;
        // } else {
        //     ap_port = (u16)krpc->ap_port;
        // }

        if (krpc.ap_name_len > 0) {
            if (!is_valid_utf8((unsigned char *)(krpc.ap_name),
                               krpc.ap_name_len)) {
                st_inc(ST_bm_ap_bad_name);
                break;
            }
            // TODO handle AP with name
        }

        // TODO db_update_peers(ih, [compact_peerinfo_bytes(saddr[0],
        // ap_port)])

        const auto &write_fn = [krp = bd::KReply(krpc)](auto &buf) {
            buf.len = msg::r_pg(reinterpret_cast<u8 *>(buf.base), krp);
        };

        send_msg(write_fn, saddr, ST_tx_r_ap);
        break;
    }

    case bd::R_FN: {
        st_inc(ST_rx_r_fn);

        if (krpc.n_nodes == 0) {
            ERROR("Empty 'nodes' in R_FN")
            st_inc(ST_err_bd_empty_r_gp);
            break;
        }
        ping_sweep_nodes(krpc);
        // No add contact since we only q_fn the bootstrap node
        break;
    }

    case bd::R_GP: {
        st_inc(ST_rx_r_gp);

        if (krpc.n_peers > 0) {
            st_inc(ST_rx_r_gp_values);
#ifdef STAT_AUX
            int val;
            if ((val = gpm::get_tok_hops(krpc)) > 0) {
                st_click_gp_n_hops(val);
            }
#endif
            gpm::clear_tok(krpc);
            // TODO handle peer
            g_rt.insert_contact(krpc, saddr, 4);
        }

        if (krpc.n_nodes == 0) {
            break;
        }

        st_inc(ST_rx_r_gp_nodes);
        ping_sweep_nodes(krpc);

        if (!gpm::extract_tok(next_hop, krpc)) {
            break;
        }

        for (auto const &pn : next_hop.pnodes) {

            if (!spam_check_tx_q_gp(pn.peerinfo.in_addr, next_hop.ih)) {
                continue;
            }

            auto [ok, tok] = gpm::take_tok();
            if (!ok) {
                break;
            }

            const auto &write_fn = [ih = next_hop.ih, dest = pn.nid,
                                    tok = tok](auto &buf) {
                buf.len =
                    msg::q_gp(reinterpret_cast<u8 *>(buf.base), dest, ih, tok);
            };

            send_msg(write_fn, pn, ST_tx_q_gp);
            gpm::register_q_gp_ihash(pn.nid, next_hop.ih, next_hop.hop_ctr + 1,
                                     tok);
        }
        break;
    }

    case bd::R_PG: {
        st_inc(ST_rx_r_pg);
        g_rt.insert_contact(krpc, saddr, 2);
        break;
    }

    default:
        ERROR("Fell through in message handle! Not OK!")
        assert(0);
        break;
    }
} // namespace cht

void init_subsystems() {
    VERBOSE("Initializing ctl...")
    ctl_init();
    // VERBOSE("Initializing rt...")
    // rt::init();
    VERBOSE("Initializing st...")
    st_init();
    INFO("Initialized.")
    INFO("Rolling over stats every %d ms", STAT_ROLLOVER_FREQ_MS)
    INFO("Heartbeat every %d rollovers", STAT_HB_EVERY)
    INFO("Control parameters:")
#ifdef CTL_PPS_TARGET
    INFO("\ttarget ping rate: %.2f", CTL_PPS_TARGET);
#endif
    INFO("\tget peers timeout: %d ms", CTL_GPM_TIMEOUT_MS);
#ifdef MSG_CLOSE_SID
    INFO("Configured with MSG_CLOSE_SID: matching nids to 4 bytes.")
#endif
#ifdef RT_BIG
    INFO("Configured with RT_BIG: using depth-three routing table.")
#endif
#ifdef STAT_CSV
    INFO("Configured with STAT_CSV: saving counter stats to " STAT_CSV_FN)
    INFO("\tWriting to csv every %d rollovers", STAT_CSV_EVERY)
#ifdef STAT_AUX
    INFO("\tConfigrued with STAT_AUX: gathering auxiliary runtime statistics")
    INFO("\t\tWriting auxiliary stats to " STAT_CSV_FN)
#endif // STAT_AUX
#endif // STAT_CSV
}

void loop_statgather_cb(uv_timer_t *timer) {
    st_rollover();
    DEBUG("Rolled over stats.")
}

void loop_bootstrap_cb(uv_timer_t *timer) {
    Nih random_target;
    getrandom(random_target.raw, NIH_LEN, 0);

    auto const &write_fn = [dest = rt::RT::bootstrap_node.pnode.nid,
                            payload = random_target](auto &buf) {
        buf.len = msg::q_fn(reinterpret_cast<u8 *>(buf.base), dest, payload);
    };

    send_msg(write_fn, rt::RT::bootstrap_node.pnode, ST_tx_q_fn);

    // VERBOSE("Bootstrapped.")
}
}; // namespace cht

int main(int argc, char *argv[]) {
    init_subsystems();

    int status;
    SIN addr;

    main_loop = uv_default_loop();

    // INITIALIZE UDP SERVER
    status = uv_udp_init(main_loop, &g_udp_server);
    CHECK(status, "init");

    uv_ip4_addr(argc >= 2 ? argv[1] : "0.0.0.0", 6881, &addr);

    status = uv_udp_bind(&g_udp_server, (const struct sockaddr *)&addr, 0);
    CHECK(status, "bind");

    status = uv_udp_recv_start(&g_udp_server, cb_alloc, cb_recv_msg);
    CHECK(status, "recv");

    // INIT statgather
    status = uv_timer_init(main_loop, &g_statgather_timer);
    CHECK(status, "statgather timer init");
    //     status =
    uv_timer_start(&g_statgather_timer, &loop_statgather_cb,
                   STAT_ROLLOVER_FREQ_MS, STAT_ROLLOVER_FREQ_MS);
    CHECK(status, "statgather start")

    // INIT BOOTSTRAP
    status = uv_timer_init(main_loop, &g_bootstrap_timer);
    CHECK(status, "bootstrap timer init");
    status = uv_timer_start(&g_bootstrap_timer, &loop_bootstrap_cb, 100, 250);

    CHECK(status, "boostrap start")

    // RUN LOOP
    INFO("Starting loop.")
    uv_run(main_loop, UV_RUN_DEFAULT);

    return 0;
}
