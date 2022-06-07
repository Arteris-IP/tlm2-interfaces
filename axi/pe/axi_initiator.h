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

#include <axi/axi_tlm.h>
#include <scc/ordered_semaphore.h>
#include <scc/peq.h>
#include <systemc>
#include <tlm_utils/peq_with_get.h>
#include <tuple>
#include <unordered_map>

namespace axi {
namespace pe {

class axi_initiator_b : public sc_core::sc_module, public axi::ace_bw_transport_if<axi::axi_protocol_types> {
public:
    enum class flavor_e { AXI, ACEL, ACE };
    using payload_type = axi::axi_protocol_types::tlm_payload_type;
    using phase_type = axi::axi_protocol_types::tlm_phase_type;

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

    axi_initiator_b(sc_core::sc_module_name nm, sc_core::sc_port_b<axi::axi_fw_transport_if<axi_protocol_types>>& port,
                    size_t transfer_width, flavor_e flavor);

    virtual ~axi_initiator_b();

    axi_initiator_b() = delete;

    axi_initiator_b(axi_initiator_b const&) = delete;

    axi_initiator_b(axi_initiator_b&&) = delete;

    axi_initiator_b& operator=(axi_initiator_b const&) = delete;

    axi_initiator_b& operator=(axi_initiator_b&&) = delete;

    // AXI4 does not allow write data interleaving, and ncore3 only supports AXI4.
    // However, AXI3 allows data interleaving and there may be support for AXI3 in Symphony, so keep it configurable in
    // the testbench.
    sc_core::sc_attribute<bool> data_interleaving{"data_interleaving", false};
    //! Read address valid to next read address valid
    sc_core::sc_attribute<unsigned> artv{"artv", 1};
    //! Write address valid to next write address valid
    sc_core::sc_attribute<unsigned> awtv{"awtv", 1};
    //! Write data handshake to next beat valid
    sc_core::sc_attribute<unsigned> wbv{"wbv", 1};
    //! Read data valid to same beat ready
    sc_core::sc_attribute<unsigned> rbr{"rbr", 0};
    //! Write response valid to ready
    sc_core::sc_attribute<unsigned> br{"br", 0};
    //! Read last data handshake to acknowledge
    sc_core::sc_attribute<unsigned> rla{"rla", 1};
    //! Write response handshake to acknowledge
    sc_core::sc_attribute<unsigned> ba{"ba", 1};
    //! Quirks enable
    sc_core::sc_attribute<bool> enable_id_serializing{"enable_id_serializing", false};
    //! number of snoops which can be handled
    sc_core::sc_attribute<unsigned> outstanding_snoops{"outstanding_snoops", 8};

protected:
    unsigned calculate_beats(payload_type& p) {
        sc_assert(p.get_data_length() > 0);
        return p.get_data_length() < transfer_width_in_bytes ? 1 : p.get_data_length() / transfer_width_in_bytes;
    }

    void snoop_thread();

    const size_t transfer_width_in_bytes;

    const flavor_e flavor;

    sc_core::sc_port_b<axi::axi_fw_transport_if<axi_protocol_types>>& socket_fw;

    std::function<unsigned(payload_type& trans)>* snoop_cb{nullptr};

    struct tx_state {
        payload_type* active_tx{nullptr};
        scc::peq<std::tuple<payload_type*, tlm::tlm_phase>> peq;
    };
    std::unordered_map<void*, tx_state*> tx_state_by_tx;
    std::unordered_map<unsigned, scc::ordered_semaphore*> id_mtx;

    tlm_utils::peq_with_get<payload_type> snp_peq{"snp_peq"};

    std::unordered_map<void*, tx_state*> snp_state_by_id;

    scc::ordered_semaphore rd_chnl{1};

    scc::ordered_semaphore wr_chnl{1};

    scc::ordered_semaphore sresp_chnl{1};

    sc_core::sc_event any_tx_finished;

    sc_core::sc_time clk_period{10, sc_core::SC_NS};

private:
    sc_core::sc_clock* clk_if{nullptr};
    void end_of_elaboration() override;
    void clk_counter() { m_clock_counter++; }
    unsigned get_clk_cnt() { return m_clock_counter; }

    tlm::tlm_phase send(payload_type& trans, axi::pe::axi_initiator_b::tx_state* txs, tlm::tlm_phase phase);

    unsigned m_clock_counter{0};
    unsigned m_prev_clk_cnt{0};
    unsigned snoops_in_flight{0};
};

/**
 * the axi initiator socket protocol engine adapted to a particular initiator socket configuration
 */
template <unsigned int BUSWIDTH = 32, typename TYPES = axi::axi_protocol_types, int N = 1,
          sc_core::sc_port_policy POL = sc_core::SC_ONE_OR_MORE_BOUND>
class axi_initiator : public axi_initiator_b {
public:
    using base = axi_initiator_b;

    using payload_type = base::payload_type;
    using phase_type = base::phase_type;
    /**
     * @brief the constructor
     * @param socket reference to the initiator socket used to send and receive transactions
     */
    axi_initiator(const sc_core::sc_module_name& nm, axi::axi_initiator_socket<BUSWIDTH, TYPES, N, POL>& socket_)
    : axi_initiator_b(nm, socket_.get_base_port(), BUSWIDTH, flavor_e::AXI)
    , socket(socket_) {
        socket(*this);
    }

    axi_initiator() = delete;

    axi_initiator(axi_initiator const&) = delete;

    axi_initiator(axi_initiator&&) = delete;

    axi_initiator& operator=(axi_initiator const&) = delete;

    axi_initiator& operator=(axi_initiator&&) = delete;

private:
    axi::axi_initiator_socket<BUSWIDTH, TYPES, N, POL>& socket;
};

/**
 * the axi initiator socket protocol engine adapted to a particular initiator socket configuration
 */
template <unsigned int BUSWIDTH = 32, typename TYPES = axi::axi_protocol_types, int N = 1,
          sc_core::sc_port_policy POL = sc_core::SC_ONE_OR_MORE_BOUND>
class ace_lite_initiator : public axi_initiator_b {
public:
    using base = axi_initiator_b;

    using payload_type = base::payload_type;
    using phase_type = base::phase_type;
    /**
     * @brief the constructor
     * @param socket reference to the initiator socket used to send and receive transactions
     */
    ace_lite_initiator(const sc_core::sc_module_name& nm, axi::axi_initiator_socket<BUSWIDTH, TYPES, N, POL>& socket_)
    : axi_initiator_b(nm, socket_.get_base_port(), BUSWIDTH, flavor_e::ACEL)
    , socket(socket_) {
        socket(*this);
    }

    ace_lite_initiator() = delete;

    ace_lite_initiator(ace_lite_initiator const&) = delete;

    ace_lite_initiator(ace_lite_initiator&&) = delete;

    ace_lite_initiator& operator=(ace_lite_initiator const&) = delete;

    ace_lite_initiator& operator=(ace_lite_initiator&&) = delete;

private:
    axi::axi_initiator_socket<BUSWIDTH, TYPES, N, POL>& socket;
};

/**
 * the axi initiator socket protocol engine adapted to a particular initiator socket configuration
 */
template <unsigned int BUSWIDTH = 32, typename TYPES = axi::axi_protocol_types, int N = 1,
          sc_core::sc_port_policy POL = sc_core::SC_ONE_OR_MORE_BOUND>
class ace_initiator : public axi_initiator_b {
public:
    using base = axi_initiator_b;

    using payload_type = base::payload_type;
    using phase_type = base::phase_type;
    /**
     * @brief the constructor
     * @param socket reference to the initiator socket used to send and receive transactions
     */
    ace_initiator(const sc_core::sc_module_name& nm, axi::ace_initiator_socket<BUSWIDTH, TYPES, N, POL>& socket_)
    : axi_initiator_b(nm, socket_.get_base_port(), BUSWIDTH, flavor_e::ACE)
    , socket(socket_) {
        socket(*this);
    }

    ace_initiator() = delete;

    ace_initiator(ace_initiator const&) = delete;

    ace_initiator(ace_initiator&&) = delete;

    ace_initiator& operator=(ace_initiator const&) = delete;

    ace_initiator& operator=(ace_initiator&&) = delete;

private:
    axi::ace_initiator_socket<BUSWIDTH, TYPES, N, POL>& socket;
};

} /* namespace pe */
} /* namespace axi */
