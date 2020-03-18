/*******************************************************************************
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
 * limitations under the License.
 *******************************************************************************/

#pragma once

#include "chi_tlm.h"
#include <memory>
#include <tlm/tlm2_pv_av.h>

namespace chi {

template <unsigned int BUSWIDTH = 32, typename TYPES = chi_protocol_types, int N = 1,
          sc_core::sc_port_policy POL = sc_core::SC_ONE_OR_MORE_BOUND,
          typename TSOCKET_TYPE = chi::chi_target_socket<BUSWIDTH, TYPES, N, POL>,
          typename ISOCKET_TYPE = chi::chi_initiator_socket<BUSWIDTH, TYPES, N, POL>>
using chi_pv_av_target_adapter = typename tlm::tlm2_pv_av_target_adapter<BUSWIDTH, TYPES, N, POL, TSOCKET_TYPE, ISOCKET_TYPE>;

template <unsigned int BUSWIDTH = 32, typename TYPES = chi_protocol_types, int N = 1,
          sc_core::sc_port_policy POL = sc_core::SC_ONE_OR_MORE_BOUND,
          typename TSOCKET_TYPE = chi::chi_target_socket<BUSWIDTH, TYPES, N, POL>,
          typename ISOCKET_TYPE = chi::chi_initiator_socket<BUSWIDTH, TYPES, N, POL>>
using chi_pv_av_initiator_adapter = typename tlm::tlm2_pv_av_initiator_adapter<BUSWIDTH, TYPES, N, POL, TSOCKET_TYPE, ISOCKET_TYPE>;

} // namespace chi
