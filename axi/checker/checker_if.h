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
 * limitations under the License.
 */

#ifndef _AXI_SCV_CHECKER_IF_H_
#define _AXI_SCV_CHECKER_IF_H_
#include <tlm>

namespace axi {
namespace checker {
template <typename TYPES>
struct checker_if {
    virtual void fw_pre(typename TYPES::tlm_payload_type const& trans, typename TYPES::tlm_phase_type const& phase) = 0;
    virtual void fw_post(typename TYPES::tlm_payload_type const& trans, typename TYPES::tlm_phase_type const& phase, tlm::tlm_sync_enum rstat) = 0;
    virtual void bw_pre(typename TYPES::tlm_payload_type const& trans, typename TYPES::tlm_phase_type const& phase) = 0;
    virtual void bw_post(typename TYPES::tlm_payload_type const& trans, typename TYPES::tlm_phase_type const& phase, tlm::tlm_sync_enum rstat) = 0;
    virtual ~checker_if() = default;
};
}
}
#endif /* _AXI_SCV_CHECKER_IF_H_ */
