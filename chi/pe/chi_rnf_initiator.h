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
#include "cache-chi.h"
#include <scc/ordered_semaphore.h>
#include <scc/peq.h>
#include <scc/sc_variable.h>
#include <systemc>
#include <tlm_utils/peq_with_get.h>
#include <tuple>
#include <unordered_map>

namespace chi {
namespace pe {

class chi_rnf_initiator_b : public sc_core::sc_module, public chi::chi_bw_transport_if<chi::chi_protocol_types> {
public:
    using payload_type = chi::chi_protocol_types::tlm_payload_type;
    using phase_type = chi::chi_protocol_types::tlm_phase_type;
    using port_type = sc_core::sc_port_b<chi::chi_fw_transport_if<chi_protocol_types>>;

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

    void set_snoop_cb(std::function<unsigned(payload_type& trans)>* cb) {  }

    void snoop_resp(payload_type& trans, bool sync = false){}

    chi_rnf_initiator_b(sc_core::sc_module_name nm, port_type& port, size_t transfer_width);

    virtual ~chi_rnf_initiator_b();

    chi_rnf_initiator_b() = delete;

    chi_rnf_initiator_b(chi_rnf_initiator_b const&) = delete;

    chi_rnf_initiator_b(chi_rnf_initiator_b&&) = delete;

    chi_rnf_initiator_b& operator=(chi_rnf_initiator_b const&) = delete;

    chi_rnf_initiator_b& operator=(chi_rnf_initiator_b&&) = delete;

    sc_core::sc_attribute<unsigned> src_id{"src_id", 1};

    sc_core::sc_attribute<unsigned> home_node_id{"home_node_id", 0}; // home node id will be used as tgt_id in all transaction req

    sc_core::sc_attribute<bool> data_interleaving{"data_interleaving", true};

    sc_core::sc_attribute<bool> strict_income_order{"strict_income_order", true};

    sc_core::sc_attribute<bool> use_legacy_mapping{"use_legacy_mapping", false};

protected:
    void end_of_elaboration() override { clk_if = dynamic_cast<sc_core::sc_clock*>(clk_i.get_interface()); }

    void send_flit(payload_type& gp, tlm::tlm_phase const& beg_ph, tlm::tlm_phase const& end_ph);

    const size_t transfer_width_in_bytes;

    scc::ordered_semaphore req_credits{0U}; // L-credits provided by completer(HN)

    sc_core::sc_port_b<chi::chi_fw_transport_if<chi_protocol_types>>& socket_fw;

    std::deque<std::tuple<payload_type*, tlm::tlm_phase, sc_core::sc_time>> bw_flits;

    sc_core::sc_event bw_flits_evt;

    scc::ordered_semaphore strict_order_sem{1};

    scc::ordered_semaphore req_order{1};

    cache_chi<1, 16384, 2> cache;

    sc_core::sc_event any_tx_finished;

    sc_core::sc_time clk_period{10, sc_core::SC_NS};

    std::string socket_name;

private:
    void clk_counter() { m_clock_counter++; }
    unsigned get_clk_cnt() { return m_clock_counter; }

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
class chi_rnf_initiator : public chi_rnf_initiator_b {
public:
    using base = chi_rnf_initiator_b;

    using payload_type = base::payload_type;
    using phase_type = base::phase_type;
    /**
     * @brief the constructor
     * @param socket reference to the initiator socket used to send and receive transactions
     */
    chi_rnf_initiator(const sc_core::sc_module_name& nm, chi::chi_initiator_socket<BUSWIDTH, TYPES, N, POL>& socket_)
    : chi_rnf_initiator_b(nm, socket_.get_base_port(), BUSWIDTH)
    , socket(socket_) {
        socket(*this);
        this->socket_name = socket.name();
    }

    chi_rnf_initiator() = delete;

    chi_rnf_initiator(chi_rnf_initiator const&) = delete;

    chi_rnf_initiator(chi_rnf_initiator&&) = delete;

    chi_rnf_initiator& operator=(chi_rnf_initiator const&) = delete;

    chi_rnf_initiator& operator=(chi_rnf_initiator&&) = delete;

private:
    chi::chi_initiator_socket<BUSWIDTH, TYPES, N, POL>& socket;
};

} /* namespace pe */
} /* namespace chi */
