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

#ifndef _AXI_CHECKER_AXI_PROTOCOL_H_
#define _AXI_CHECKER_AXI_PROTOCOL_H_

#include <axi/axi_tlm.h>
#include "checker_if.h"
#include <tlm/scc/tlm_gp_shared.h>
#include <array>
#include <unordered_map>
#include <deque>

namespace axi {
namespace checker {

class axi_protocol: public checker_if<axi::axi_protocol_types> {
    using payload_type=axi::axi_protocol_types::tlm_payload_type;
    using phase_type=axi::axi_protocol_types::tlm_phase_type;
    constexpr static unsigned umax = std::numeric_limits<unsigned>::max();
public:
    axi_protocol(std::string const& name, unsigned bus_width_in_bytes, unsigned rd_response_timeout, unsigned wr_response_timeout)
    : name(name)
    , bw(bus_width_in_bytes)
    , rd_response_timeout(rd_response_timeout)
    , wr_response_timeout(wr_response_timeout)
    {}
    virtual ~axi_protocol() = default;
    axi_protocol(const axi_protocol &other) = delete;
    axi_protocol(axi_protocol &&other) = delete;
    axi_protocol& operator=(const axi_protocol &other) = delete;
    axi_protocol& operator=(axi_protocol &&other) = delete;
    void fw_pre(payload_type const& trans, phase_type const& phase) override;
    void fw_post(payload_type const& trans, phase_type const& phase, tlm::tlm_sync_enum rstat) override;
    void bw_pre(payload_type const& trans, phase_type const& phase) override;
    void bw_post(payload_type const& trans, phase_type const& phase, tlm::tlm_sync_enum rstat) override;
    std::string const name;
    unsigned const bw;
    unsigned const rd_response_timeout;
    unsigned const wr_response_timeout;
private:
    std::array<phase_type, 3> req_beat;
    std::array<phase_type, 3> resp_beat;
    phase_type dataless_req;
    std::array<unsigned, 3> req_id{{umax, umax, umax}};
    std::array<unsigned, 3> resp_id{{umax, umax, umax}};
    unsigned wr_req_beat_count{0};
    std::array<std::unordered_map<unsigned, std::deque<uintptr_t>>, 3> open_tx_by_id;
    std::unordered_map<unsigned, std::deque<uintptr_t>> resp_by_id;
    std::unordered_map<unsigned, unsigned> rd_resp_beat_count;
    bool check_phase_change(payload_type const& trans, const phase_type &phase);
    void request_update(payload_type const& trans);
    void response_update(payload_type const& trans);
    void check_properties(payload_type const& trans);
    void check_datawith_settings(payload_type const&trans);
};

} /* namespace checker */
} /* namespace axi */

#endif /* _AXI_CHECKER_AXI_PROTOCOL_H_ */
