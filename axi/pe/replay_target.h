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

#include "target_info_if.h"
#include <axi/pe/axi_target_pe.h>
#include <cci_configuration>

//! TLM2.0 components modeling AXI/ACE
namespace axi {
//! protocol engine implementations
namespace pe {
class replay_buffer : public sc_core::sc_module, tlm::scc::pe::intor_fw_nb {
public:
    sc_core::sc_in<bool> clk_i{"clk_i"};

    sc_core::sc_export<tlm::scc::pe::intor_fw_nb> fw_i{"fw_i"};

    sc_core::sc_port<tlm::scc::pe::intor_bw_nb, 1, sc_core::SC_ZERO_OR_MORE_BOUND> bw_o{"bw_o"};

    cci::cci_param<std::string> replay_file_name{"replay_file_name", ""};

    replay_buffer(const sc_core::sc_module_name& nm);
    /**
     * execute the transport of the payload. Independent of the underlying layer this function is blocking
     *
     * @param payload object with (optional) extensions
     * @param lt_transport use b_transport instead of nb_transport*
     */
    void transport(tlm::tlm_generic_payload& payload, bool lt_transport = false) override;
    /**
     * send a response to a backward transaction if not immediately answered
     *
     * @param payload object with (optional) extensions
     * @param sync if true send with next rising clock edge of the pe otherwise send it immediately
     */
    void snoop_resp(tlm::tlm_generic_payload& payload, bool sync = false) override {}

    void end_of_reset() { reset_end_cycle = sc_core::sc_time_stamp() / clk_if->period(); }

protected:
    sc_core::sc_clock* clk_if{nullptr};
    uint64_t reset_end_cycle{0};
    using entry_t = std::tuple<uint64_t, unsigned>;
    std::vector<std::vector<entry_t>> rd_sequence, wr_sequence;
    sc_core::sc_time time_per_byte_rd, time_per_byte_wr, time_per_byte_total;
    //! queues realizing the latencies
    scc::fifo_w_cb<std::tuple<tlm::tlm_generic_payload*, unsigned>> rd_req2resp_fifo{"rd_req2resp_fifo"};
    scc::fifo_w_cb<std::tuple<tlm::tlm_generic_payload*, unsigned>> wr_req2resp_fifo{"wr_req2resp_fifo"};
    //! queues to handle bandwidth limit
    scc::fifo_w_cb<tlm::tlm_generic_payload*> rd_resp_fifo{"rd_resp_fifo"};
    scc::fifo_w_cb<tlm::tlm_generic_payload*> wr_resp_fifo{"wr_resp_fifo"};
    scc::ordered_semaphore total_arb{1};
    double total_residual_clocks{0.0};
    void end_of_elaboration() override;
    void start_of_simulation() override;
    void process_req2resp_fifos();
    void start_rd_resp_thread();
    void start_wr_resp_thread();
};
/**
 * the target socket protocol engine(s) adapted to a particular target socket configuration,
 * sends responses in the order they arrived
 */
template <unsigned int BUSWIDTH = 32, typename TYPES = axi::axi_protocol_types, int N = 1,
          sc_core::sc_port_policy POL = sc_core::SC_ONE_OR_MORE_BOUND>
class replay_target : public sc_core::sc_module, public target_info_if {
public:
    using base = axi_target_pe;
    using payload_type = base::payload_type;
    using phase_type = base::phase_type;

    sc_core::sc_in<bool> clk_i{"clk_i"};

    sc_core::sc_in<bool> rst_i{"rst_i"};

    axi::axi_target_socket<BUSWIDTH, TYPES, N, POL> sckt{"sckt"};

    /**
     * @brief the constructor
     * @param nm module instance name
     */
    replay_target(const sc_core::sc_module_name& nm)
    : sc_core::sc_module(nm)
    , pe("pe", BUSWIDTH)
    , repl_buffer("repl_buffer") {
        sckt(pe);
        pe.clk_i(clk_i);
        repl_buffer.clk_i(clk_i);
        pe.fw_o(repl_buffer.fw_i);
        repl_buffer.bw_o(pe.bw_i);
#if SYSTEMC_VERSION < 20250221
        SC_HAS_PROCESS(replay_target);
#endif
        SC_METHOD(end_of_reset);
        sensitive << rst_i.neg();
    }

    replay_target() = delete;

    replay_target(replay_target const&) = delete;

    replay_target(replay_target&&) = delete;

    replay_target& operator=(replay_target const&) = delete;

    replay_target& operator=(replay_target&&) = delete;

    size_t get_outstanding_tx_count() override { return pe.getAllOutStandingTx(); }

protected:
    void end_of_reset() { repl_buffer.end_of_reset(); }
    void end_of_elaboration() {
        auto* ifs = sckt.get_base_port().get_interface(0);
        sc_assert(ifs != nullptr);
        pe.set_bw_interface(ifs);
    }

public:
    axi_target_pe pe;
    replay_buffer repl_buffer;
};
} // namespace pe
} // namespace axi
