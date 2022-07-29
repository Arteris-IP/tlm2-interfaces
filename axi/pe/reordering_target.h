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
class tx_reorderer: public sc_core::sc_module, tlm::scc::pe::intor_fw_nb {
public:
    sc_core::sc_in<bool> clk_i{"clk_i"};

    sc_core::sc_export<tlm::scc::pe::intor_fw_nb> fw_i{"fw_i"};

    sc_core::sc_port<tlm::scc::pe::intor_bw_nb, 1, sc_core::SC_ZERO_OR_MORE_BOUND> bw_o{"bw_o"};
    //! the minimum time a transaction will stay in the target
    sc_core::sc_attribute<unsigned> min_latency{"min_latency", 10};
    //! the maximum time a transaction is allwed to stay in the target
    sc_core::sc_attribute<unsigned> max_latency{"max_latency", 100};
    //! minimum number of transaction to start with reordering. If the total number is
    //! below the threshold transaction will stay for max_latency
    sc_core::sc_attribute<unsigned> window_size{"window_size", 2};
    //! use the waiting time as priority value - the longer a transaction waits the higher
    //! the probability becomes to be selected
    sc_core::sc_attribute<bool> prioritize_by_latency{"prioritize_by_latency", false};
    //! use the QoS field for selection.
    sc_core::sc_attribute<bool> prioritize_by_qos{"prioritize_by_qos", false};

    tx_reorderer(const sc_core::sc_module_name& nm);
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
    void clock_cb();
    struct que_entry {
        tlm::scc::tlm_gp_shared_ptr trans;
        unsigned age{0};
        que_entry(tlm::tlm_generic_payload& gp):trans(&gp){}
    };
    std::array<std::unordered_map<unsigned , std::deque<que_entry>>, 3> reorder_buffer;
};
/**
 * the AXI target which shuffles the responses from the order they arrived
 */
template <unsigned int BUSWIDTH = 32, typename TYPES = axi::axi_protocol_types, int N = 1,
          sc_core::sc_port_policy POL = sc_core::SC_ONE_OR_MORE_BOUND>
class reordering_target : public sc_core::sc_module {
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

    reordering_target(const sc_core::sc_module_name& nm)
    : sc_core::sc_module(nm)
    , pe("pe", BUSWIDTH) {
        sckt(pe);
        pe.clk_i(clk_i);
        pe.fw_o(reorder_buffer.fw_i);
        reorder_buffer.clk_i(clk_i);
        reorder_buffer.bw_o(pe.bw_i);
    }

    reordering_target() = delete;

    reordering_target(reordering_target const&) = delete;

    reordering_target(reordering_target&&) = delete;

    reordering_target& operator=(reordering_target const&) = delete;

    reordering_target& operator=(reordering_target&&) = delete;

protected:
    void end_of_elaboration(){
        auto* ifs = sckt.get_base_port().get_interface(0);
        sc_assert(ifs!=nullptr);
        pe.set_bw_interface(ifs);
    }
public:
    axi_target_pe pe;
    tx_reorderer reorder_buffer{"reorder_buffer"};
};
} // namespace pe
} // namespace axi
