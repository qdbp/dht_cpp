#pragma once

#include "dht.hpp"
#include "gpmap.hpp"
#include "krpc.hpp"
#include "msg.hpp"
#include "rt.hpp"
#include "spamfilter.hpp"
#include "stat.hpp"
#include <chrono>
#include <sys/random.h>

#define ASIO_STANDALONE
#include <asio.hpp>

using namespace std::literals;
using namespace asio;
using namespace ip;
using bd::KRPC;
using rt::g_rt;

using msg_write_t = std::function<u32(u8 *)>;

namespace cht {

static constexpr u16 PORT = 6881;
static constexpr u16 SEND_LEN = 256;

static inline const udp::endpoint as_endpoint(const Peerinfo &pinfo) {
    return udp::endpoint(make_address_v4(be32toh(pinfo.in_addr)),
                         be16toh(pinfo.sin_port));
}

static inline const Peerinfo as_peerinfo(const udp::endpoint &ep) {
    return {.in_addr = htobe32(ep.address().to_v4().to_uint()),
            .sin_port = htobe16(ep.port())};
}

static inline u32 in_addr(const udp::endpoint &ep) {
    return htobe32(ep.address().to_v4().to_uint());
}

class KRPCServer {

  private:
    static std::vector<u8 *> send_bufs;

    u8 *take_send_buf() {
        if (send_bufs.empty()) {
            st_inc(ST_ctl_n_send_bufs);
            return new u8[SEND_LEN];
        } else {
            auto out = send_bufs.back();
            send_bufs.pop_back();
            return out;
        }
    }

    void release_send_buf(u8 *buf) {
        send_bufs.push_back(buf);
    }

    io_service srv;

    steady_timer bst_timer;

    udp::socket sock;
    udp::endpoint sender;

    KRPC krpc = KRPC();
    mutable_buffer rcv_buf = buffer(krpc.data);

    void handle_recv(const error_code &ec, size_t nread) {

        if (ec) {
            DEBUG("Receive error: %s", ec.message().c_str());
            return;
        }

        if (nread == 0) {
            return;
        }

        if (nread < MIN_MSG_LEN) {
            st_inc(ST_bd_x_msg_too_short);
            return;
        }
        // We probably clipped. Yes, GEQ
        if (nread >= bd::MAXLEN) {
            st_inc(ST_bd_x_msg_too_long);
            return;
        }

        if (!spam_check_rx(sender.address().to_v4().to_uint())) {
            return;
        }

        // Where the magic happens. the data buffer that was written to krpc is
        // parsed.
        krpc.clear();
        krpc.parse_msg(nread);

        if (krpc.status != ST_bd_a_no_error) {
            st_inc(krpc.status);
            return;
        }

        st_inc(ST_rx_tot);

        gpm::NextHop next_hop;

        switch (krpc.method) {
        case bd::Q_PG: {
            st_inc(ST_rx_q_pg);

            auto const &write_fn = [krp = bd::KReply(krpc)](auto buf) {
                return msg::r_pg(buf, krp);
            };

            send_msg(write_fn, sender, ST_tx_r_pg);
            g_rt.insert_contact(krpc, in_addr(sender), htobe16(sender.port()),
                                0);

            break;
        }

        case bd::Q_FN: {
            st_inc(ST_rx_q_fn);

            auto payload = g_rt.get_neighbor_contact(*krpc.target);

            auto const &write_fn = [krp = bd::KReply(krpc), payload](auto buf) {
                return msg::r_fn(buf, krp, payload);
            };

            send_msg(write_fn, sender, ST_tx_r_fn);
            g_rt.insert_contact(krpc, in_addr(sender), htobe16(sender.port()),
                                0);

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

                auto const &write_fn = [=, tok = tok, ih = *krpc.ih](auto buf) {
                    return msg::q_gp(buf, ih_neig.nid, ih, tok);
                };

                send_msg(write_fn, as_endpoint(ih_neig.peerinfo), ST_tx_q_gp);
                gpm::register_q_gp_ihash(ih_neig.nid, *krpc.ih, 0, tok);
            }

            // reply to the sender node

            auto const &write_fn = [=, krp = bd::KReply(krpc)](auto buf) {
                return msg::r_gp(buf, krp, ih_neig);
            };

            send_msg(write_fn, sender, ST_tx_r_gp);
            g_rt.insert_contact(krpc, in_addr(sender), htobe16(sender.port()),
                                1);

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
                    st_inc(ST_bd_z_ap_bad_name);
                    break;
                }
                // TODO handle AP with name
            }

            // TODO db_update_peers(ih, [compact_peerinfo_bytes(saddr[0],
            // ap_port)])

            const auto &write_fn = [krp = bd::KReply(krpc)](auto buf) {
                return msg::r_pg(buf, krp);
            };

            send_msg(write_fn, sender, ST_tx_r_ap);
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
                g_rt.insert_contact(krpc, in_addr(sender),
                                    htobe16(sender.port()), 4);
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
                                        tok = tok](auto buf) {
                    return msg::q_gp(buf, dest, ih, tok);
                };

                send_msg(write_fn, as_endpoint(pn.peerinfo), ST_tx_q_gp);
                gpm::register_q_gp_ihash(pn.nid, next_hop.ih,
                                         next_hop.hop_ctr + 1, tok);
            }
            break;
        }

        case bd::R_PG: {
            st_inc(ST_rx_r_pg);
            g_rt.insert_contact(krpc, in_addr(sender), sender.port(), 2);
            break;
        }

        default:
            ERROR("Fell through in message handle! Not OK!")
            assert(0);
            break;
        }
    } // namespace cht

    inline void send_msg(const msg_write_t &write_fn, const udp::endpoint &dest,
                         stat_t acct) {

        auto buf = take_send_buf();
        auto len = write_fn(buf);

        sock.async_send_to(buffer(buf, len), dest,
                           [buf, this](const error_code &ec, size_t nsent) {
                               release_send_buf(buf);
                               if (ec) {
                                   DEBUG("Send error: %s.",
                                         ec.message().c_str());
                               }
                           });
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

            const msg_write_t &write_fn = [nid = krpc.nodes[ix].nid](auto buf) {
                return msg::q_pg(buf, nid);
            };

            send_msg(write_fn, as_endpoint(krpc.nodes[ix].peerinfo),
                     ST_tx_q_pg);
        }
    }

    inline void loop_bootstrap_cb() {
        Nih random_target;
        getrandom(random_target.raw, NIH_LEN, 0);

        auto const &write_fn = [dest = rt::RT::bootstrap_node.pnode.nid,
                                payload = random_target](auto buf) {
            return msg::q_fn(buf, dest, payload);
        };

        send_msg(write_fn, as_endpoint(rt::RT::bootstrap_node.pnode.peerinfo),
                 ST_tx_q_fn);
    }

  public:
    void run() {
        sock = udp::socket(srv, udp::endpoint(udp::v4(), PORT));
        bst_timer = steady_timer(srv, 200ms);
        srv.run();
    }
}; // namespace cht
} // namespace cht
