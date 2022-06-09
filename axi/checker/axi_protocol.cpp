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

axi_protocol::axi_protocol(std::string const& name, unsigned bus_width_in_bytes) :name(name), bw(bus_width_in_bytes){

}

axi_protocol::~axi_protocol() {
}

void axi_protocol::fw_pre(const axi_protocol::payload_type &trans, const axi_protocol::phase_type &phase) {
    auto cmd = trans.get_command();
    if (cmd == tlm::TLM_IGNORE_COMMAND)
        SCCERR(name) << "Illegal command: tlm::TLM_IGNORE_COMMAND on forward path";
    if (check_phase_change(trans, phase))
        SCCERR(name) << "Illegal phase transition: " << phase.get_name() << " on forward path";
    if(cmd==tlm::TLM_WRITE_COMMAND) {

    }
}

void axi_protocol::fw_post(const axi_protocol::payload_type &trans, const axi_protocol::phase_type &phase, tlm::tlm_sync_enum rstat) {
    if(rstat == tlm::TLM_ACCEPTED) return;
    auto cmd = trans.get_command();
    if(req_beat[cmd]==tlm::BEGIN_REQ && (phase == tlm::BEGIN_RESP || phase == axi::BEGIN_PARTIAL_RESP))
        req_beat[cmd]= tlm::UNINITIALIZED_PHASE;
    if (check_phase_change(trans, phase))
        SCCERR(name) << "Illegal phase transition: " << phase.get_name() << " on return in forward path";
    if(cmd==tlm::TLM_READ_COMMAND) {

    }
}

void axi_protocol::bw_pre(const axi_protocol::payload_type &trans, const axi_protocol::phase_type &phase) {
    auto cmd = trans.get_command();
    if (cmd == tlm::TLM_IGNORE_COMMAND)
        SCCERR(name) << "Illegal command:  tlm::TLM_IGNORE_COMMAND on forward path";
    if (check_phase_change(trans, phase))
        SCCERR(name) << "Illegal phase transition: " << phase.get_name() << " on backward path";
    if(cmd==tlm::TLM_READ_COMMAND) {

    }
}

void axi_protocol::bw_post(const axi_protocol::payload_type &trans, const axi_protocol::phase_type &phase, tlm::tlm_sync_enum rstat) {
    if(rstat == tlm::TLM_ACCEPTED) return;
    auto cmd = trans.get_command();
    if (check_phase_change(trans, phase))
        SCCERR(name) << "Illegal phase transition: " << phase.get_name() << " on return in backward path";
}

bool axi_protocol::check_phase_change(payload_type const& trans, const axi_protocol::phase_type &phase) {
    // phase tests
    auto cur_req = req_beat[trans.get_command()];
    auto cur_resp = resp_beat[trans.get_command()];
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
    if(req_beat[trans.get_command()] != cur_req){
        req_beat[trans.get_command()] = cur_req;
        request_update(trans);
    }
    if(resp_beat[trans.get_command()] != cur_resp){
        resp_beat[trans.get_command()] = cur_resp;
        response_update(trans);
    }
    return error;
}

void axi_protocol::request_update(const payload_type &trans) {
    auto axi_id = axi::get_axi_id(trans);
    if(trans.is_write()){
        if(req_beat[tlm::TLM_WRITE_COMMAND]==tlm::UNINITIALIZED_PHASE) {
            req_id[tlm::TLM_WRITE_COMMAND] = umax;
        } else {
            if(req_id[tlm::TLM_WRITE_COMMAND] == umax) {
                req_id[tlm::TLM_WRITE_COMMAND] = axi_id;
                wr_req_beat_count++;
            } else if(req_id[tlm::TLM_WRITE_COMMAND] != axi_id){
                SCCERR(name) << "Illegal ordering: a transaction with AWID:0x"<<std::hex<<axi_id<<" starts while a transaction with AWID:0x"<<req_id[tlm::TLM_WRITE_COMMAND]<<" is active";
            }
            if(req_beat[tlm::TLM_WRITE_COMMAND]==tlm::BEGIN_REQ) {
                if(wr_req_beat_count !=axi::get_burst_lenght(trans)){
                    SCCERR(name) << "Illegal AXI settings: number of transferred beats ("<<wr_req_beat_count<<") does not comply with AWLEN:0x"<<std::hex<<axi::get_burst_lenght(trans)-1;
                }
                wr_req_beat_count=0;
                open_tx_by_id[tlm::TLM_WRITE_COMMAND][axi_id].push_back(reinterpret_cast<uintptr_t>(&trans));
            }
        }
    } else if(trans.is_read()) {
        if(req_beat[tlm::TLM_READ_COMMAND]==tlm::UNINITIALIZED_PHASE) {
            req_id[tlm::TLM_READ_COMMAND] = umax;
        } else  if(req_beat[tlm::TLM_READ_COMMAND]==tlm::BEGIN_REQ) {
            if(req_id[tlm::TLM_READ_COMMAND] == umax) {
                req_id[tlm::TLM_READ_COMMAND] = axi_id;
                open_tx_by_id[tlm::TLM_READ_COMMAND][axi_id].push_back(reinterpret_cast<uintptr_t>(&trans));
            } else if(req_id[tlm::TLM_READ_COMMAND] != axi_id){
                SCCERR(name) << "Illegal phase: a read transaction uses a phase with id "<<req_beat[tlm::TLM_READ_COMMAND];
            }
        } else {
            SCCERR(name) << "Illegal phase: a read transaction uses a phase with id "<<req_beat[tlm::TLM_READ_COMMAND];
        }
    }
}

void axi_protocol::response_update(const payload_type &trans) {
    auto axi_id = axi::get_axi_id(trans);
    if(trans.is_write()){
        if(resp_beat[tlm::TLM_WRITE_COMMAND]==tlm::UNINITIALIZED_PHASE) {
            resp_id[tlm::TLM_WRITE_COMMAND] = umax;
        } else  if(resp_beat[tlm::TLM_WRITE_COMMAND]==tlm::BEGIN_RESP) {
            if(resp_id[tlm::TLM_WRITE_COMMAND] == umax) {
                resp_id[tlm::TLM_WRITE_COMMAND] = axi_id;
                if(open_tx_by_id[tlm::TLM_WRITE_COMMAND][axi_id].front()!=reinterpret_cast<uintptr_t>(&trans)) {
                    SCCERR(name) << "Illegal write response: a response with AWID:0x"<<std::hex<<axi_id<<" starts before the previous response with the same id finished";
                }
            } else if(resp_id[tlm::TLM_WRITE_COMMAND] != axi_id){
                SCCERR(name) << "Illegal phase: a read transaction uses a phase with id "<<resp_beat[tlm::TLM_WRITE_COMMAND];
            }
            open_tx_by_id[tlm::TLM_WRITE_COMMAND][axi_id].pop_front();
        } else {
            SCCERR(name) << "Illegal phase: a read transaction uses a phase with id "<<resp_beat[tlm::TLM_WRITE_COMMAND];
        }
    } else if(trans.is_read()) {
        if(resp_beat[tlm::TLM_READ_COMMAND]==tlm::UNINITIALIZED_PHASE) {
            resp_id[tlm::TLM_READ_COMMAND] = umax;
        } else {
            if(resp_id[tlm::TLM_READ_COMMAND] == umax) {
                resp_id[tlm::TLM_READ_COMMAND] = axi_id;
                if(open_tx_by_id[tlm::TLM_READ_COMMAND][axi_id].front()!=reinterpret_cast<uintptr_t>(&trans)) {
                    SCCERR(name) << "Illegal read response: a response with ARID:0x"<<std::hex<<axi_id<<" starts before the previous response with the same id finished";
                }
                rd_resp_beat_count[axi_id]++;
            } else if(resp_id[tlm::TLM_READ_COMMAND] != axi_id){
                SCCERR(name) << "Illegal phase: a read transaction uses a phase with id "<<resp_beat[tlm::TLM_READ_COMMAND];
            }
            if(resp_beat[tlm::TLM_READ_COMMAND]==tlm::BEGIN_RESP)  {
                if(rd_resp_beat_count[axi_id] !=axi::get_burst_lenght(trans)){
                    SCCERR(name) << "Illegal AXI settings: number of transferred beats ("<<wr_req_beat_count<<") does not comply with AWLEN:0x"<<std::hex<<axi::get_burst_lenght(trans)-1;
                }
                open_tx_by_id[tlm::TLM_READ_COMMAND][axi_id].pop_front();
                rd_resp_beat_count[axi_id]=0;
            }
        }
    }
}

} /* namespace checker */
} /* namespace axi */
