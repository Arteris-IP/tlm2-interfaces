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

#include <axi/pe/axi_target_pe.h>

//! TLM2.0 components modeling AXI/ACE
namespace axi {
//! protocol engine implementations
namespace pe {
class rate_limiting_buffer: public sc_core::sc_module, tlm::scc::pe::intor_fw_nb {
public:
    sc_core::sc_in<bool> clk_i{"clk_i"};

    sc_core::sc_export<tlm::scc::pe::intor_fw_nb> fw_i{"fw_i"};

    sc_core::sc_port<tlm::scc::pe::intor_bw_nb, 1, sc_core::SC_ZERO_OR_MORE_BOUND> bw_o{"bw_o"};

    /**
     * @brief the latency between request and response phase. Will be overwritten by the return of the callback function
     * (if registered) -> RIV
     */
    scc::sc_attribute_randomized<int> rd_resp_delay{"rd_resp_delay", 0};
    /**
     * @brief the latency between request and response phase. Will be overwritten by the return of the callback function
     * (if registered) -> BV
     */
    scc::sc_attribute_randomized<int> wr_resp_delay{"wr_resp_delay", 0};
    /**
     * @brief the bandwidth limit for read accesses
     */
    sc_core::sc_attribute<double> rd_bw_limit_byte_per_sec{"rd_bw_limit_byte_per_sec", -1.0};
    /**
     * @brief the bandwidth limit for write accesses
     */
    sc_core::sc_attribute<double> wr_bw_limit_byte_per_sec{"wr_bw_limit_byte_per_sec", -1.0};

    rate_limiting_buffer(const sc_core::sc_module_name& nm);
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
protected:
    sc_core::sc_clock* clk_if{nullptr};
    sc_core::sc_time time_per_byte_rd, time_per_byte_wr;
    //! queues realizing the min latency
    scc::fifo_w_cb<std::tuple<tlm::tlm_generic_payload*, unsigned>> rd_req2resp_fifo{"rd_req2resp_fifo"};
    scc::fifo_w_cb<std::tuple<tlm::tlm_generic_payload*, unsigned>> wr_req2resp_fifo{"wr_req2resp_fifo"};
    //! queues to handle bandwidth limit
    scc::fifo_w_cb<tlm::tlm_generic_payload*> rd_resp_fifo{"rd_resp_fifo"};
    scc::fifo_w_cb<tlm::tlm_generic_payload*> wr_resp_fifo{"wr_resp_fifo"};

    void end_of_elaboration() override;

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
class ordered_target : public sc_core::sc_module {
public:
    using base = axi_target_pe;
    using payload_type = base::payload_type;
    using phase_type = base::phase_type;
    sc_core::sc_in<bool> clk_i{"clk_i"};

    axi::axi_target_socket<BUSWIDTH, TYPES, N, POL> sckt{"sckt"};

    /**
     * @brief the constructor
     * @param nm module instance name
     */
    ordered_target(const sc_core::sc_module_name& nm)
    : sc_core::sc_module(nm)
    , pe("pe", BUSWIDTH) {
        sckt(pe);
        pe.clk_i(clk_i);
        rate_limit_buffer.clk_i(clk_i);
        pe.fw_o(rate_limit_buffer.fw_i);
        rate_limit_buffer.bw_o(pe.bw_i);
    }

    ordered_target() = delete;

    ordered_target(ordered_target const&) = delete;

    ordered_target(ordered_target&&) = delete;

    ordered_target& operator=(ordered_target const&) = delete;

    ordered_target& operator=(ordered_target&&) = delete;

protected:
    void end_of_elaboration(){
        auto* ifs = sckt.get_base_port().get_interface(0);
        sc_assert(ifs!=nullptr);
        pe.set_bw_interface(ifs);
    }
public:
    axi_target_pe pe;
    rate_limiting_buffer rate_limit_buffer{"rate_limit_buffer"};
};
} // namespace pe
} // namespace axi
