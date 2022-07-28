/*
 * Copyright 2021 Arteris IP
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once

#include <chi/chi_tlm.h>
#include <scc/ordered_semaphore.h>
#include <scc/peq.h>
#include <scc/sc_variable.h>
#include <systemc>
#include <tlm_utils/peq_with_get.h>
#include <tuple>
#include <unordered_map>

namespace chi {
namespace pe {

class chi_rn_initiator_b : public sc_core::sc_module, public chi::chi_bw_transport_if<chi::chi_protocol_types> {
public:
    using payload_type = chi::chi_protocol_types::tlm_payload_type;
    using phase_type = chi::chi_protocol_types::tlm_phase_type;

    sc_core::sc_in<bool> clk_i{"clk_i"};

    void b_snoop(payload_type& trans, sc_core::sc_time& t) override;

    tlm::tlm_sync_enum nb_transport_bw(payload_type& trans, phase_type& phase, sc_core::sc_time& t) override;

    void invalidate_direct_mem_ptr(sc_dt::uint64 start_range, sc_dt::uint64 end_range) override;

    size_t get_transferwith_in_bytes() const { return transfer_width_in_bytes; }
    /**
     * @brief The forward transport function. It behaves blocking and is re-entrant.
     *
     * This function initiates the forward transport either using b_transport() if blocking=true
     *  or the nb_transport_* interface.
     *
     * @param trans the transaction to send
     * @param blocking execute in using the blocking interface
     */
    void transport(payload_type& trans, bool blocking);
    /**
     * @brief Set the snoop callback function
     *
     * This callback is invoked once a snoop transaction arrives. This function shall return the latency
     * for the snoop response. If the response is std::numeric_limits<unsigned>::max() no snoop response will be
     * triggered. This needs to be done by a call to snoop_resp()
     *
     * @todo refine API
     * @todo handing in a pointer is a hack to work around a bug in gcc 4.8 not allowing to copy std::function objects
     * and should be fixed
     *
     * @param cb the callback function
     */
    void set_snoop_cb(std::function<unsigned(payload_type& trans)>* cb) { snoop_cb = cb; }
    /**
     * @brief triggers a non-blocking snoop response if the snoop callback does not do so.
     *
     * @param trans
     * @param sync when true send response with next rising clock edge otherwise send immediately
     */
    void snoop_resp(payload_type& trans, bool sync = false);

    chi_rn_initiator_b(sc_core::sc_module_name nm,
                       sc_core::sc_port_b<chi::chi_fw_transport_if<chi_protocol_types>>& port, size_t transfer_width);

    virtual ~chi_rn_initiator_b();

    chi_rn_initiator_b() = delete;

    chi_rn_initiator_b(chi_rn_initiator_b const&) = delete;

    chi_rn_initiator_b(chi_rn_initiator_b&&) = delete;

    chi_rn_initiator_b& operator=(chi_rn_initiator_b const&) = delete;

    chi_rn_initiator_b& operator=(chi_rn_initiator_b&&) = delete;

    sc_core::sc_attribute<unsigned> src_id{"src_id", 1};

    sc_core::sc_attribute<unsigned> home_node_id{"home_node_id",
                                                 0}; // home node id will be used as tgt_id in all transaction req

    sc_core::sc_attribute<bool> data_interleaving{"data_interleaving", true};

    sc_core::sc_attribute<bool> strict_income_order{"strict_income_order", true};

    sc_core::sc_attribute<bool> use_legacy_mapping{"use_legacy_mapping", false};

protected:
    void end_of_elaboration() override { clk_if = dynamic_cast<sc_core::sc_clock*>(clk_i.get_interface()); }

    unsigned calculate_beats(payload_type& p) {
        // sc_assert(p.get_data_length() > 0);
        return p.get_data_length() < transfer_width_in_bytes ? 1 : p.get_data_length() / transfer_width_in_bytes;
    }

    void snoop_dispatch();

    void snoop_handler(payload_type* trans);

    const size_t transfer_width_in_bytes;

    std::string instance_name;

    scc::ordered_semaphore req_credits{0U}; // L-credits provided by completer(HN)

    sc_core::sc_port_b<chi::chi_fw_transport_if<chi_protocol_types>>& socket_fw;

    std::function<unsigned(payload_type& trans)>* snoop_cb{nullptr};

    struct tx_state {
        scc::peq<std::tuple<payload_type*, tlm::tlm_phase>> peq;
        tx_state(std::string const& name)
        : peq(sc_core::sc_gen_unique_name(name.c_str())) {}
    };
    std::unordered_map<uintptr_t, tx_state*> tx_state_by_trans;

    std::unordered_map<unsigned, scc::ordered_semaphore> active_tx_by_id;

    scc::ordered_semaphore strict_order_sem{1};

    tlm_utils::peq_with_get<payload_type> snp_peq{"snp_peq"}, snp_dispatch_que{"snp_dispatch_que"};

    unsigned thread_avail{0}, thread_active{0};

    scc::ordered_semaphore req_chnl{1};

    scc::ordered_semaphore wdat_chnl{1};

    scc::ordered_semaphore sresp_chnl{1};

    sc_core::sc_event any_tx_finished;

    sc_core::sc_time clk_period{10, sc_core::SC_NS};

private:
    void send_wdata(payload_type& trans, chi::pe::chi_rn_initiator_b::tx_state* txs);
    void handle_snoop_response(payload_type& trans, chi::pe::chi_rn_initiator_b::tx_state* txs);
    void send_comp_ack(payload_type& trans, tx_state*& txs);
    void clk_counter() { m_clock_counter++; }
    unsigned get_clk_cnt() { return m_clock_counter; }

    void create_data_ext(payload_type& trans);
    void send_packet(tlm::tlm_phase phase, payload_type& trans, chi::pe::chi_rn_initiator_b::tx_state* txs);
    void exec_read_write_protocol(const unsigned int txn_id, payload_type& trans,
                                  chi::pe::chi_rn_initiator_b::tx_state*& txs);
    void exec_atomic_protocol(const unsigned int txn_id, payload_type& trans,
                              chi::pe::chi_rn_initiator_b::tx_state*& txs);
    void cresp_response(payload_type& trans);
    void update_data_extension(chi::chi_data_extension* data_ext, payload_type& trans);

    unsigned m_clock_counter{0};
    unsigned m_prev_clk_cnt{0};

    sc_core::sc_clock* clk_if{nullptr};
    uint64_t peq_cnt{0};

    scc::sc_variable<unsigned> tx_waiting{"TxWaiting", 0};
    scc::sc_variable<unsigned> tx_outstanding{"TxOutstanding", 0};
};

/**
 * the CHI initiator socket protocol engine adapted to a particular initiator socket configuration
 */
template <unsigned int BUSWIDTH = 32, typename TYPES = chi::chi_protocol_types, int N = 1,
          sc_core::sc_port_policy POL = sc_core::SC_ONE_OR_MORE_BOUND>
class chi_rn_initiator : public chi_rn_initiator_b {
public:
    using base = chi_rn_initiator_b;

    using payload_type = base::payload_type;
    using phase_type = base::phase_type;
    /**
     * @brief the constructor
     * @param socket reference to the initiator socket used to send and receive transactions
     */
    chi_rn_initiator(const sc_core::sc_module_name& nm, chi::chi_initiator_socket<BUSWIDTH, TYPES, N, POL>& socket_)
    : chi_rn_initiator_b(nm, socket_.get_base_port(), BUSWIDTH)
    , socket(socket_) {
        socket(*this);
        this->instance_name = socket.name();
    }

    chi_rn_initiator() = delete;

    chi_rn_initiator(chi_rn_initiator const&) = delete;

    chi_rn_initiator(chi_rn_initiator&&) = delete;

    chi_rn_initiator& operator=(chi_rn_initiator const&) = delete;

    chi_rn_initiator& operator=(chi_rn_initiator&&) = delete;

private:
    chi::chi_initiator_socket<BUSWIDTH, TYPES, N, POL>& socket;
};

} /* namespace pe */
} /* namespace chi */
