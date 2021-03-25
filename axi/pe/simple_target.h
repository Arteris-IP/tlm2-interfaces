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
#include <functional>
#include <scc/ordered_semaphore.h>
#include <unordered_set>
#include <tlm_utils/peq_with_cb_and_phase.h>


//! TLM2.0 components modeling AXI/ACE
namespace axi {
//! protocol engine implementations
namespace pe {
/**
 * the target protocol engine base class
 */
class axi_target_pe_b : public sc_core::sc_module, protected axi::fsm::base, public axi::axi_fw_transport_if<axi::axi_protocol_types> {
public:
    SC_HAS_PROCESS(axi_target_pe_b);

    using payload_type = axi::axi_protocol_types::tlm_payload_type;
    using phase_type = axi::axi_protocol_types::tlm_phase_type;

    sc_core::sc_in<bool> clk_i{"clk_i"};
    /**
     * @brief the number of supported outstanding transactions. If this limit is reached the target starts to do back-pressure
     */
    sc_core::sc_attribute<unsigned> max_outstanding_tx{"max_outstanding_tx", 0};
    /**
     * @brief enable data interleaving on read responses
     */
    sc_core::sc_attribute<bool> rd_data_interleaving{"rd_data_interleaving", true};
    /**
     * @brief the latency between between BEGIN(_PARTIAL)_REQ and END(_PARTIAL)_REQ (AWVALID to AWREADY and WVALID to WREADY) -> AWR, WBR
     */
    sc_core::sc_attribute<unsigned> wr_data_accept_delay{"wr_data_accept_delay", 0};
    /**
     * @brief the latency between between BEGIN_REQ and END_REQ (ARVALID to ARREADY) -> APR
     */
    sc_core::sc_attribute<unsigned> rd_addr_accept_delay{"rd_addr_accept_delay", 0};
    /**
     * @brief the latency between between END(_PARTIAL)_RESP and BEGIN(_PARTIAL)_RESP (RREADY to RVALID) -> RBV
     */
    sc_core::sc_attribute<unsigned> rd_data_beat_delay{"rd_data_beat_delay", 0};
    /**
     * @brief the latency between request and response phase. Will be overwritten by the return of the callback function (if registered) -> RIV
     */
    sc_core::sc_attribute<unsigned> rd_resp_delay{"rd_resp_delay", 0};
    /**
     * @brief the latency between request and response phase. Will be overwritten by the return of the callback function (if registered) -> BV
     */
    sc_core::sc_attribute<unsigned> wr_resp_delay{"wr_resp_delay", 0};

    /** @defgroup fw_if Initiator foreward interface
     *  @{
     */
    void b_transport(payload_type& trans, sc_core::sc_time& t) override;

    tlm::tlm_sync_enum nb_transport_fw(payload_type& trans, phase_type& phase, sc_core::sc_time& t) override;

    bool get_direct_mem_ptr(payload_type& trans, tlm::tlm_dmi& dmi_data) override;

    unsigned int transport_dbg(payload_type& trans) override;
    /** @} */ // end of fw_if
    /** @defgroup config Initiator configuration interface
     *  @{
     */
    /**
     * @brief Set the operation callback function
     *
     * This callback is invoked once a transaction arrives. This function is not allowed to block and returns the
     * latency of the operation i.e. the duration until the reponse phase starts
     * @todo refine API
     *
     * @param cb the callback function
     */

    void set_operation_cb(std::function<unsigned(payload_type& trans)> cb) { operation_cb = cb; }
    /**
     *
     * @param trans
     * @param sync
     */
    void operation_resp(payload_type& trans, unsigned clk_delay=0);
    /**
     * returns true if any transaction is still in flight
     *
     * @return
     */
    bool is_active() { return !active_fsm.empty(); }
    /**
     * get the event being notfied upon the finishing of an event
     *
     * @return reference to sc_event
     */
    const sc_core::sc_event& tx_finish_event() { return finish_evt; }

protected:
    /**
     * the constructor. Protected as it should only be called by derived classes
     * @param port
     * @param transfer_width
     */
    explicit axi_target_pe_b(const sc_core::sc_module_name& nm, sc_core::sc_port_b<axi::axi_bw_transport_if<axi_protocol_types>>& port,
                             size_t transfer_width);

    axi_target_pe_b() = delete;

    axi_target_pe_b(axi_target_pe_b const&) = delete;

    axi_target_pe_b(axi_target_pe_b&&) = delete;

    axi_target_pe_b& operator=(axi_target_pe_b const&) = delete;

    axi_target_pe_b& operator=(axi_target_pe_b&&) = delete;

    void fsm_clk_method() { process_fsm_clk_queue();}
    /**
     * @see base::create_fsm_handle()
     */
    fsm::fsm_handle* create_fsm_handle() override;
    /**
     * @see base::setup_callbacks(fsm::fsm_handle*)
     */
    void setup_callbacks(fsm::fsm_handle*) override;

    void send_wr_resp_thread();

    void send_rd_resp_thread();

    void rd_resp_thread();

    unsigned operations_callback(payload_type& trans);

    sc_core::sc_port_b<axi::axi_bw_transport_if<axi_protocol_types>>& socket_bw;
    sc_core::sc_semaphore sn_sem{1};
    sc_core::sc_mutex wr, rd, sn;
    std::function<unsigned(payload_type& trans)> operation_cb;
    sc_core::sc_fifo<std::tuple<fsm::fsm_handle*, axi::fsm::protocol_time_point_e>> send_wr_resp_fifo{128}, send_rd_resp_fifo{128};
    sc_core::sc_fifo<payload_type*> rd_resp_fifo{128};
    scc::ordered_semaphore rd_resp{1}, wr_resp_ch{1}, rd_resp_ch{1};
    scc::fifo_w_cb<std::tuple<payload_type*, unsigned>> rd_resp_queue;
    void process_rd_resp_queue();
    sc_core::sc_clock* clk_if{nullptr};
    void end_of_elaboration() override;
    void start_of_simulation() override;
    std::array<unsigned, 3> outstanding_cnt{{0,0,0}};
    std::array<unsigned, 3>  outstanding_tx{{0,0,0}};
    scc::sc_variable_t<unsigned> outstanding_rd_tx_v{"outstanding_rd_tx", outstanding_tx[tlm::TLM_READ_COMMAND]};
    scc::sc_variable_t<unsigned> outstanding_wr_tx_v{"outstanding_wr_tx", outstanding_tx[tlm::TLM_WRITE_COMMAND]};
    std::array<tlm::tlm_generic_payload*, 3> stalled_tx{nullptr,nullptr,nullptr};
    std::array<axi::fsm::protocol_time_point_e, 3> stalled_tp{{axi::fsm::CB_CNT,axi::fsm::CB_CNT,axi::fsm::CB_CNT}};
    void nb_fw(payload_type& trans, const phase_type& phase) {
        auto delay=sc_core::SC_ZERO_TIME;
        base::nb_fw(trans, phase, delay);
    }
    tlm_utils::peq_with_cb_and_phase<axi_target_pe_b> fw_peq{this, &axi_target_pe_b::nb_fw};
};

/**
 * the target socket protocol engine adapted to a particular target socket configuration
 */
template <unsigned int BUSWIDTH = 32, typename TYPES = axi::axi_protocol_types, int N = 1,
          sc_core::sc_port_policy POL = sc_core::SC_ONE_OR_MORE_BOUND>
class simple_target : public axi_target_pe_b {
public:
    using base = axi_target_pe_b;
    using payload_type = base::payload_type;
    using phase_type = base::phase_type;
    /**
     * @brief the constructor
     * @param socket reference to the initiator socket used to send and receive transactions
     */
    simple_target(axi::axi_target_socket<BUSWIDTH, TYPES, N, POL>& socket)
    : // @suppress("Class members should be properly initialized")
        simple_target(sc_core::sc_gen_unique_name("simple_target"), socket) {}

    simple_target(const sc_core::sc_module_name& nm, axi::axi_target_socket<BUSWIDTH, TYPES, N, POL>& socket)
    : axi_target_pe_b(nm, socket.get_base_port(), BUSWIDTH)
    , socket(socket) {
        socket(*this);
        this->instance_name = socket.name();
    }

    simple_target() = delete;

    simple_target(simple_target const&) = delete;

    simple_target(simple_target&&) = delete;

    simple_target& operator=(simple_target const&) = delete;

    simple_target& operator=(simple_target&&) = delete;

private:
    axi::axi_target_socket<BUSWIDTH, TYPES, N, POL>& socket;
};

} // namespace pe
} // namespace axi
