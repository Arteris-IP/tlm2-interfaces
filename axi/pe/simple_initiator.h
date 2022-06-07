/*
 * Copyright 2020 Arteris IP
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
 * limitations under the License.axi_util.cpp
 */

#pragma once

#define SC_INCLUDE_DYNAMIC_PROCESSES

#include <array>
#include <axi/fsm/base.h>
#include <deque>
#include <scc/ordered_semaphore.h>
#include <sysc/kernel/sc_attribute.h>
#include <tlm/scc/pe/intor_if.h>
#include <unordered_map>
#include <unordered_set>

//! TLM2.0 components modeling AXI/ACE
namespace axi {
//! protocol engine implementations
namespace pe {
/**
 * the initiator protocol engine base class
 */
class simple_initiator_b : public sc_core::sc_module, public tlm::scc::pe::intor_fw_b, protected axi::fsm::base {
public:
    using payload_type = axi::axi_protocol_types::tlm_payload_type;
    using phase_type = axi::axi_protocol_types::tlm_phase_type;

    sc_core::sc_in<bool> clk_i{"clk_i"};

    sc_core::sc_export<tlm::scc::pe::intor_fw_b> drv_i{"drv_i"};

    sc_core::sc_port<tlm::scc::pe::intor_bw_b, 1, sc_core::SC_ZERO_OR_MORE_BOUND> drv_o{"drv_o"};
    /**
     * @brief the latency between between END(_PARTIAL)_REQ and BEGIN(_PARTIAL)_REQ (AWREADY to AWVALID and WREADY to
     * WVALID)
     */
    sc_core::sc_attribute<unsigned> wr_data_beat_delay{"wr_data_beat_delay", 0};
    /**
     * @brief the latency between between BEGIN(_PARTIAL)_RESP and END(_PARTIAL)_RESP (RVALID to RREADY)
     */
    sc_core::sc_attribute<unsigned> rd_data_accept_delay{"rd_data_accept_delay", 0};
    /**
     * @brief the latency between between BEGIN_RESP and END_RESP (BVALID to BREADY)
     */
    sc_core::sc_attribute<unsigned> wr_resp_accept_delay{"wr_resp_accept_delay", 0};
    /**
     * @brief the latency between between BEGIN_RESP and END_RESP (BVALID to BREADY)
     */
    sc_core::sc_attribute<unsigned> ack_resp_delay{"ack_resp_delay", 0};

    void b_snoop(payload_type& trans, sc_core::sc_time& t);

    tlm::tlm_sync_enum nb_transport_bw(payload_type& trans, phase_type& phase, sc_core::sc_time& t);

    void invalidate_direct_mem_ptr(sc_dt::uint64 start_range, sc_dt::uint64 end_range);

    void set_clock_period(sc_core::sc_time clk_period) { this->clk_period = clk_period; }

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
    void transport(payload_type& trans, bool blocking) override;
    /**
     * @brief Set the snoop callback function
     *
     * This callback is invoked once a snoop transaction arrives. This function shall return the latency
     * for the snoop response. If the response is std::numeric_limits<unsigned>::max() not snoop response will be
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
    void snoop_resp(payload_type& trans, bool sync = false) override;
    /**
     * @brief the default snoop latency between request and response phase. Will be overwritten by the
     * return of the callback function (if registered)
     * @todo this is a hack and should be fixed
     */
    unsigned snoop_latency{1};
    /**
     * @brief register a callback for a certain time point
     *
     * This function allows to register a callback for certain time points of a transaction
     * (see #axi::fsm::protocol_time_point_e). The callback will be invoked after the FSM-actions are executed.
     *
     * @param e the timepoint
     * @param cb the callback taking a reference to the transaction and a bool indicating a snoop if true
     */
    void add_protocol_cb(axi::fsm::protocol_time_point_e e, std::function<void(payload_type&, bool)> cb) {
        assert(e < axi::fsm::CB_CNT);
        protocol_cb[e] = cb;
    }

protected:
    /**
     * the constructor. Protected as it should only be called by derived classes
     * @param port
     * @param transfer_width
     */
    explicit simple_initiator_b(const sc_core::sc_module_name& nm,
                                sc_core::sc_port_b<axi::axi_fw_transport_if<axi_protocol_types>>& port,
                                size_t transfer_width, bool coherent = false);

    simple_initiator_b() = delete;

    simple_initiator_b(simple_initiator_b const&) = delete;

    simple_initiator_b(simple_initiator_b&&) = delete;

    simple_initiator_b& operator=(simple_initiator_b const&) = delete;

    simple_initiator_b& operator=(simple_initiator_b&&) = delete;

    void fsm_clk_method() { process_fsm_clk_queue(); }
    /**
     * @see base::create_fsm_handle()
     */
    axi::fsm::fsm_handle* create_fsm_handle() override;
    /**
     * @see base::setup_callbacks(axi::fsm::fsm_handle*)
     */
    void setup_callbacks(axi::fsm::fsm_handle*) override;

    void process_snoop_resp();

    sc_core::sc_port_b<axi::axi_fw_transport_if<axi_protocol_types>>& socket_fw;
    std::deque<fsm::fsm_handle*> idle_proc;
    ::scc::ordered_semaphore rd{1}, wr{1}, snp{1};
    ::scc::fifo_w_cb<std::tuple<axi::fsm::protocol_time_point_e, payload_type*, unsigned>> snp_resp_queue;
    sc_core::sc_process_handle snp_resp_queue_hndl;
    // TODO: remove hard coded value
    sc_core::sc_time clk_period{1, sc_core::SC_NS};
    std::function<unsigned(payload_type& trans)>* snoop_cb{nullptr};
    std::array<std::function<void(payload_type&, bool)>, axi::fsm::CB_CNT> protocol_cb;
    sc_core::sc_clock* clk_if{nullptr};
    void end_of_elaboration() override;
};
/**
 * the AXI initiator socket protocol engine adapted to a particular initiator socket configuration
 */
template <unsigned int BUSWIDTH = 32, typename TYPES = axi::axi_protocol_types, int N = 1,
          sc_core::sc_port_policy POL = sc_core::SC_ONE_OR_MORE_BOUND>
class simple_axi_initiator : public simple_initiator_b, public axi::axi_bw_transport_if<axi::axi_protocol_types> {
public:
    using base = simple_initiator_b;

    using payload_type = base::payload_type;
    using phase_type = base::phase_type;
    /**
     * @brief the constructor
     * @param socket reference to the initiator socket used to send and receive transactions
     */
    simple_axi_initiator(const sc_core::sc_module_name& nm, axi::axi_initiator_socket<BUSWIDTH, TYPES, N, POL>& socket_)
    : simple_initiator_b(nm, socket_.get_base_port(), BUSWIDTH)
    , socket(socket_) {
        socket(*this);
        this->instance_name = name();
    }

    simple_axi_initiator() = delete;

    simple_axi_initiator(simple_axi_initiator const&) = delete;

    simple_axi_initiator(simple_axi_initiator&&) = delete;

    simple_axi_initiator& operator=(simple_axi_initiator const&) = delete;

    simple_axi_initiator& operator=(simple_axi_initiator&&) = delete;

    /**
     * @brief forwarding function to the base class (due to inheritance)
     * @param trans
     * @param phase
     * @param t
     * @return
     */
    tlm::tlm_sync_enum nb_transport_bw(payload_type& trans, phase_type& phase, sc_core::sc_time& t) override {
        return base::nb_transport_bw(trans, phase, t);
    }
    /**
     * @brief forwarding function to the base class (due to inheritance)
     * @param start_range
     * @param end_range
     */
    void invalidate_direct_mem_ptr(sc_dt::uint64 start_range, sc_dt::uint64 end_range) override {
        return base::invalidate_direct_mem_ptr(start_range, end_range);
    }

private:
    axi::axi_initiator_socket<BUSWIDTH, TYPES, N, POL>& socket;
};
/**
 * the ACE initiator socket protocol engine adapted to a particular initiator socket configuration
 */
template <unsigned int BUSWIDTH = 32, typename TYPES = axi::axi_protocol_types, int N = 1,
          sc_core::sc_port_policy POL = sc_core::SC_ONE_OR_MORE_BOUND>
class simple_ace_initiator : public simple_initiator_b, public axi::ace_bw_transport_if<axi::axi_protocol_types> {
public:
    using base = simple_initiator_b;

    using payload_type = base::payload_type;
    using phase_type = base::phase_type;
    /**
     * @brief the constructor
     * @param socket reference to the initiator socket used to send and receive transactions
     */

    simple_ace_initiator(const sc_core::sc_module_name& nm, axi::ace_initiator_socket<BUSWIDTH, TYPES, N, POL>& socket)
    : simple_initiator_b(nm, socket.get_base_port(), BUSWIDTH, true)
    , socket(socket) {
        socket(*this);
        this->instance_name = name();
    }

    simple_ace_initiator() = delete;

    simple_ace_initiator(simple_ace_initiator const&) = delete;

    simple_ace_initiator(simple_ace_initiator&&) = delete;

    simple_ace_initiator& operator=(simple_ace_initiator const&) = delete;

    simple_ace_initiator& operator=(simple_ace_initiator&&) = delete;
    /**
     * @brief forwarding function to the base class (due to inheritance)
     * @param trans
     * @param t
     */
    void b_snoop(payload_type& trans, sc_core::sc_time& t) override { base::b_snoop(trans, t); }
    /**
     * @brief forwarding function to the base class (due to inheritance)
     * @param trans
     * @param phase
     * @param t
     * @return
     */
    tlm::tlm_sync_enum nb_transport_bw(payload_type& trans, phase_type& phase, sc_core::sc_time& t) override {
        return base::nb_transport_bw(trans, phase, t);
    }
    /**
     * @brief forwarding function to the base class (due to inheritance)
     * @param start_range
     * @param end_range
     */
    void invalidate_direct_mem_ptr(sc_dt::uint64 start_range, sc_dt::uint64 end_range) override {
        return base::invalidate_direct_mem_ptr(start_range, end_range);
    }

private:
    axi::ace_initiator_socket<BUSWIDTH, TYPES, N, POL>& socket;
};

} // namespace pe
} // namespace axi
