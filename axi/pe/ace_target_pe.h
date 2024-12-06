/*
 * Copyright 2020-2022 Arteris IP
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

#ifndef SC_INCLUDE_DYNAMIC_PROCESSES
#define SC_INCLUDE_DYNAMIC_PROCESSES
#endif

#include <array>
#include <axi/fsm/base.h>
#include <functional>
#include <memory>
#include <scc/ordered_semaphore.h>
#include <scc/sc_attribute_randomized.h>
#include <scc/sc_variable.h>
#include <tlm/scc/pe/intor_if.h>
#include <tlm_utils/peq_with_cb_and_phase.h>
#include <unordered_set>

//! TLM2.0 components modeling AXI/ACE
namespace axi {
//! protocol engine implementations
namespace pe {
/**
 * the target protocol engine base class
 */
class ace_target_pe : public sc_core::sc_module,
                   protected axi::fsm::base,
                   public axi::axi_bw_transport_if<axi::axi_protocol_types>,
                   public axi::ace_fw_transport_if<axi::axi_protocol_types> {
    struct bw_intor_impl;
public:

    using payload_type = axi::axi_protocol_types::tlm_payload_type;
    using phase_type = axi::axi_protocol_types::tlm_phase_type;

    sc_core::sc_in<bool> clk_i{"clk_i"};

    // hongyu?? here first hardcoded
    axi::axi_initiator_socket<64> isckt_axi{"isckt_axi"};

    sc_core::sc_port<tlm::scc::pe::intor_fw_nb, 1, sc_core::SC_ZERO_OR_MORE_BOUND> fw_o{"fw_o"};

    sc_core::sc_export<tlm::scc::pe::intor_bw_nb> bw_i{"bw_i"};
    /**
     * @brief the latency between read request and response phase. Will be overwritten by the return of the callback function
     * (if registered) -> RIV
     */
    cci::cci_param<int> rd_resp_delay{"rd_resp_delay", 0};
    /**
     * @brief the latency between write request and response phase. Will be overwritten by the return of the callback function
     * (if registered) -> BV
     */
    cci::cci_param<int> wr_resp_delay{"wr_resp_delay", 0};

    void b_transport(payload_type& trans, sc_core::sc_time& t) override;

    tlm::tlm_sync_enum nb_transport_fw(payload_type& trans, phase_type& phase, sc_core::sc_time& t) override;

    bool get_direct_mem_ptr(payload_type& trans, tlm::tlm_dmi& dmi_data) override;

    unsigned int transport_dbg(payload_type& trans) override;
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
     * start the response from an operation callback if latency is not set by the callback
     *
     * @param trans
     * @param sync
     */
    void operation_resp(payload_type& trans, unsigned clk_delay = 0);
    /**
     * returns true if any transaction is still in flight
     *
     * @return
     */
    bool is_active() { return !active_fsm.empty(); }
    /**
     * get the event being notfied upon the finishing of a transaction
     *
     * @return reference to sc_event
     */
    const sc_core::sc_event& tx_finish_event() { return finish_evt; }

     /* overwrite function, defined in axi_bw_transport_if */
    tlm::tlm_sync_enum nb_transport_bw(payload_type& trans, phase_type& phase, sc_core::sc_time& t) override {
        SCCTRACE(SCMOD)  << " in nb_transport_bw () " ;
        return socket_bw->nb_transport_bw(trans, phase, t);
    }

    void invalidate_direct_mem_ptr(sc_dt::uint64 start_range, sc_dt::uint64 end_range) override {}


    virtual ~ace_target_pe();

    /**
     * the constructor. Protected as it should only be called by derived classes
     * @param port
     * @param transfer_width
     */
    explicit ace_target_pe(const sc_core::sc_module_name& nm, size_t transfer_width);

    void set_bw_interface(axi::axi_bw_transport_if<axi_protocol_types>* ifs) {socket_bw=ifs;}

    void snoop(payload_type& trans);

protected:
    ace_target_pe() = delete;

    ace_target_pe(ace_target_pe const&) = delete;

    ace_target_pe(ace_target_pe&&) = delete;

    ace_target_pe& operator=(ace_target_pe const&) = delete;

    ace_target_pe& operator=(ace_target_pe&&) = delete;

    void end_of_elaboration() override;

    void start_of_simulation() override;

    void fsm_clk_method() { process_fsm_clk_queue(); }
    /**
     * @see base::create_fsm_handle()
     */
    fsm::fsm_handle* create_fsm_handle() override;
    /**
     * @see base::setup_callbacks(fsm::fsm_handle*)
     */
    void setup_callbacks(fsm::fsm_handle*) override;

    unsigned operations_callback(payload_type& trans);

    axi::axi_bw_transport_if<axi_protocol_types>* socket_bw{nullptr};
    std::function<unsigned(payload_type& trans)> operation_cb;
    sc_core::sc_fifo<payload_type*> rd_resp_fifo{1}, wr_resp_fifo{1};

    sc_core::sc_fifo<std::tuple<fsm::fsm_handle*, axi::fsm::protocol_time_point_e>> wr_resp_beat_fifo{128},
    rd_resp_beat_fifo{128};
    scc::ordered_semaphore rd_resp{1}, wr_resp_ch{1}, rd_resp_ch{1};

    sc_core::sc_clock* clk_if{nullptr};
    std::unique_ptr<bw_intor_impl> bw_intor;
    std::array<unsigned, 3> outstanding_cnt{{0, 0, 0}}; // count for limiting

    void nb_fw(payload_type& trans, const phase_type& phase) {
        auto delay = sc_core::SC_ZERO_TIME;
        base::nb_fw(trans, phase, delay);
    }
    tlm_utils::peq_with_cb_and_phase<ace_target_pe> fw_peq{this, &ace_target_pe::nb_fw};
    std::unordered_set<unsigned> active_rdresp_id;

};

} // namespace pe
} // namespace axi
