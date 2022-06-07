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

#include <axi/checker/axi_protocol.h>
#include <scc/report.h>
namespace axi {
namespace checker {

axi_protocol::axi_protocol(std::string const& name) :name(name){

}

axi_protocol::~axi_protocol() {
}

bool axi_protocol::check_phase_change(tlm::tlm_command cmd, const axi_protocol::phase_type &phase) {
    // phase tests
    auto &cur_req = req_beat[cmd];
    auto &cur_resp = resp_beat[cmd];
    bool error{false};
    if (phase == tlm::BEGIN_REQ || phase == axi::BEGIN_PARTIAL_REQ) {
        error |= cur_req != tlm::UNINITIALIZED_PHASE;
        cur_req = phase;
    } else if(phase==axi::END_PARTIAL_REQ) {
        error |= cur_req != axi::BEGIN_PARTIAL_REQ;
        cur_req = tlm::UNINITIALIZED_PHASE;
    } else if(phase==tlm::END_REQ) {
        error |= cur_req != tlm::BEGIN_REQ;
        cur_req = tlm::UNINITIALIZED_PHASE;
    } else if(phase == tlm::BEGIN_RESP || phase == axi::BEGIN_PARTIAL_RESP) {
        error |= cur_resp!=tlm::UNINITIALIZED_PHASE;
        cur_resp = phase;
    } else if (phase == tlm::END_RESP) {
        error |= cur_resp != tlm::BEGIN_RESP;
        cur_resp = tlm::UNINITIALIZED_PHASE;
    } else if (phase == axi::END_PARTIAL_RESP) {
        error |= cur_resp != axi::BEGIN_PARTIAL_RESP;
        cur_resp = tlm::UNINITIALIZED_PHASE;
    } else {
        error |= true;
    }
    return error;
}

void axi_protocol::fw_pre(const axi_protocol::payload_type &trans, const axi_protocol::phase_type &phase) {
    auto cmd = trans.get_command();
    if (cmd == tlm::TLM_IGNORE_COMMAND)
        SCCERR(name) << "Detected illegal command tlm::TLM_IGNORE_COMMAND on forward path";
    if (check_phase_change(cmd, phase))
        SCCERR(name) << "Detected illegal phase " << phase.get_name() << " on forward path";
    if(cmd==tlm::TLM_WRITE_COMMAND) {

    }
}

void axi_protocol::fw_post(const axi_protocol::payload_type &trans, const axi_protocol::phase_type &phase, tlm::tlm_sync_enum rstat) {
    if(rstat == tlm::TLM_ACCEPTED) return;
    auto cmd = trans.get_command();
    if(req_beat[cmd]==tlm::BEGIN_REQ && (phase == tlm::BEGIN_RESP || phase == axi::BEGIN_PARTIAL_RESP))
        req_beat[cmd]= tlm::UNINITIALIZED_PHASE;
    if (check_phase_change(cmd, phase))
        SCCERR(name) << "Detected illegal phase " << phase.get_name() << " on return in forward path";
    if(cmd==tlm::TLM_READ_COMMAND) {

    }
}

void axi_protocol::bw_pre(const axi_protocol::payload_type &trans, const axi_protocol::phase_type &phase) {
    auto cmd = trans.get_command();
    if (cmd == tlm::TLM_IGNORE_COMMAND)
        SCCERR(name) << "Detected illegal command tlm::TLM_IGNORE_COMMAND on forward path";
    if (check_phase_change(cmd, phase))
        SCCERR(name) << "Detected illegal phase " << phase.get_name() << " on backward path";
    if(cmd==tlm::TLM_READ_COMMAND) {

    }
}

void axi_protocol::bw_post(const axi_protocol::payload_type &trans, const axi_protocol::phase_type &phase, tlm::tlm_sync_enum rstat) {
    if(rstat == tlm::TLM_ACCEPTED) return;
    auto cmd = trans.get_command();
    if (check_phase_change(cmd, phase))
        SCCERR(name) << "Detected illegal phase " << phase.get_name() << " on return in backward path";
}

} /* namespace checker */
} /* namespace axi */
