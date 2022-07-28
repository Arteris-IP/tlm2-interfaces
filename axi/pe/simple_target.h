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

#include <axi/pe/axi_target_pe.h>

//! TLM2.0 components modeling AXI/ACE
namespace axi {
//! protocol engine implementations
namespace pe {
/**
 * the target socket protocol engine(s) adapted to a particular target socket configuration
 *
 * @deprecated Use ordered_/reordering_target instead
 */
template <unsigned int BUSWIDTH = 32, typename TYPES = axi::axi_protocol_types, int N = 1,
          sc_core::sc_port_policy POL = sc_core::SC_ONE_OR_MORE_BOUND>
class simple_target : public axi_target_pe {
public:
    using base = axi_target_pe;
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
    : axi_target_pe(nm, BUSWIDTH)
    , socket(socket) {
        socket(*this);
        this->instance_name = name();
    }

    simple_target() = delete;

    simple_target(simple_target const&) = delete;

    simple_target(simple_target&&) = delete;

    simple_target& operator=(simple_target const&) = delete;

    simple_target& operator=(simple_target&&) = delete;

protected:
    axi::axi_target_socket<BUSWIDTH, TYPES, N, POL>& socket;

    void end_of_elaboration(){
        set_bw_interface(socket.get_base_port().operator -> ());
    }
};

} // namespace pe
} // namespace axi
