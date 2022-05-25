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

#include "axi_target_pe_b.h"

//! TLM2.0 components modeling AXI/ACE
namespace axi {
//! protocol engine implementations
namespace pe {
/**
 * the target socket protocol engine(s) adapted to a particular target socket configuration,
 * sends responses in the order they arrived
 */
template <unsigned int BUSWIDTH = 32, typename TYPES = axi::axi_protocol_types, int N = 1,
          sc_core::sc_port_policy POL = sc_core::SC_ONE_OR_MORE_BOUND>
class ordered_target : public axi_target_pe_b {
public:
    using base = axi_target_pe_b;
    using payload_type = base::payload_type;
    using phase_type = base::phase_type;
    /**
     * @brief the constructor
     * @param nm module instance name
     */

    ordered_target(const sc_core::sc_module_name& nm)
    : axi_target_pe_b(nm, BUSWIDTH) {
        socket(*this);
    }

    ordered_target() = delete;

    ordered_target(ordered_target const&) = delete;

    ordered_target(ordered_target&&) = delete;

    ordered_target& operator=(ordered_target const&) = delete;

    ordered_target& operator=(ordered_target&&) = delete;

    axi::axi_target_socket<BUSWIDTH, TYPES, N, POL> sckt{"sckt"};
protected:
    void end_of_elaboration(){
        set_bw_interface(sckt.get_base_port().operator -> ());
    }
};
} // namespace pe
} // namespace axi
