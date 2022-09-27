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

#include <axi/checker/ace_protocol.h>
#include <scc/report.h>
namespace axi {
namespace checker {

ace_protocol::~ace_protocol() = default;

void ace_protocol::fw_pre(const ace_protocol::payload_type &trans, const ace_protocol::phase_type &phase) {
    auto cmd = trans.get_command();
    if (cmd == tlm::TLM_IGNORE_COMMAND)
        SCCERR(name) << "Illegal command: tlm::TLM_IGNORE_COMMAND on forward path";
    if (check_phase_change(trans, phase))
        SCCERR(name) << "Illegal phase transition: " << phase.get_name() << " on forward path";
}

void ace_protocol::fw_post(const ace_protocol::payload_type &trans, const ace_protocol::phase_type &phase, tlm::tlm_sync_enum rstat) {
    if(rstat == tlm::TLM_ACCEPTED) return;
    auto cmd = trans.get_command();
    if(req_beat[cmd]==tlm::BEGIN_REQ && (phase == tlm::BEGIN_RESP || phase == axi::BEGIN_PARTIAL_RESP))
        req_beat[cmd]= tlm::UNINITIALIZED_PHASE;
    if (check_phase_change(trans, phase))
        SCCERR(name) << "Illegal phase transition: " << phase.get_name() << " on return in forward path";
}

void ace_protocol::bw_pre(const ace_protocol::payload_type &trans, const ace_protocol::phase_type &phase) {
    auto cmd = trans.get_command();
    if (cmd == tlm::TLM_IGNORE_COMMAND)
        SCCERR(name) << "Illegal command:  tlm::TLM_IGNORE_COMMAND on forward path";
    if (check_phase_change(trans, phase))
        SCCERR(name) << "Illegal phase transition: " << phase.get_name() << " on backward path";
}

void ace_protocol::bw_post(const ace_protocol::payload_type &trans, const ace_protocol::phase_type &phase, tlm::tlm_sync_enum rstat) {
    if(rstat == tlm::TLM_ACCEPTED) return;
    if (check_phase_change(trans, phase))
        SCCERR(name) << "Illegal phase transition: " << phase.get_name() << " on return in backward path";
}

bool ace_protocol::check_phase_change(payload_type const& trans, const ace_protocol::phase_type &phase) {
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

void ace_protocol::request_update(const payload_type &trans) {
    auto axi_id = axi::get_axi_id(trans);
    auto axi_burst_len = axi::get_burst_lenght(trans);
    auto axi_burst_size = axi::get_burst_size(trans);
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
                if(wr_req_beat_count != axi_burst_len){
                    SCCERR(name) << "Illegal AXI settings: number of transferred beats ("<<wr_req_beat_count<<") does not comply with AWLEN:0x"<<std::hex<<axi::get_burst_lenght(trans)-1;
                }
                auto mask = bw-1ULL;
                auto offset = trans.get_address() & mask;
                if(!offset){
                    if(trans.get_data_length() > (1 << axi_burst_size) * axi_burst_len) {
                        SCCERR(name) << "Illegal AXI settings: transaction data length (" << trans.get_data_length() << ") does not correspond to AxSIZE/AxLEN  setting ("
                                << axi_burst_size << "/" << axi_burst_len-1 << ") for " << trans;
                    }
                } else {
                    if((trans.get_data_length() + offset) >= (1 << axi_burst_size) * axi_burst_len) {
                        SCCERR(name) << "Illegal AXI settings: transaction data length (" << trans.get_data_length() << ") does not correspond to AxSIZE/AxLEN  setting ("
                                << axi_burst_size << "/" << axi_burst_len-1 << ") for " << trans;
                    }
                }
                wr_req_beat_count=0;
                open_tx_by_id[tlm::TLM_WRITE_COMMAND][axi_id].push_back(reinterpret_cast<uintptr_t>(&trans));
            }
            check_properties(trans);
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
            check_properties(trans);
        } else {
            SCCERR(name) << "Illegal phase: a read transaction uses a phase with id "<<req_beat[tlm::TLM_READ_COMMAND];
        }
    }
}

void ace_protocol::response_update(const payload_type &trans) {
    auto axi_id = axi::get_axi_id(trans);
    auto axi_burst_len = axi::get_burst_lenght(trans);
    auto axi_burst_size = axi::get_burst_size(trans);
    if(trans.is_write()){
        if(resp_beat[tlm::TLM_WRITE_COMMAND]==tlm::UNINITIALIZED_PHASE) {
            resp_id[tlm::TLM_WRITE_COMMAND] = umax;
        } else  if(resp_beat[tlm::TLM_WRITE_COMMAND]==tlm::BEGIN_RESP) {
            if(resp_id[tlm::TLM_WRITE_COMMAND] == umax) {
                resp_id[tlm::TLM_WRITE_COMMAND] = axi_id;
                if(open_tx_by_id[tlm::TLM_WRITE_COMMAND][axi_id].front()!=reinterpret_cast<uintptr_t>(&trans)) {
                    SCCERR(name) << "Write response ordering violation: a response with AWID:0x"<<std::hex<<axi_id<<" starts before the previous response with the same id finished";
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
                    SCCERR(name) << "Read response ordering violation: a response with ARID:0x"<<std::hex<<axi_id<<" starts before the previous response with the same id finished";
                }
                rd_resp_beat_count[axi_id]++;
            } else if(resp_id[tlm::TLM_READ_COMMAND] != axi_id){
                SCCERR(name) << "Illegal phase: a read transaction uses a phase with id "<<resp_beat[tlm::TLM_READ_COMMAND];
            }
            if(resp_beat[tlm::TLM_READ_COMMAND]==tlm::BEGIN_RESP)  {
                if(rd_resp_beat_count[axi_id] != axi_burst_len){
                    SCCERR(name) << "Illegal AXI settings: number of transferred beats ("<<wr_req_beat_count<<") does not comply with AWLEN:0x"<<std::hex<<axi::get_burst_lenght(trans)-1;
                }
                auto mask = bw-1ULL;
                auto offset = trans.get_address() & mask;
                if(!offset){
                    if(trans.get_data_length() > (1 << axi_burst_size) * axi_burst_len) {
                        SCCERR(name) << "Illegal AXI settings: transaction data length (" << trans.get_data_length() << ") does not correspond to AxSIZE/AxLEN  setting ("
                                << axi_burst_size << "/" << axi_burst_len-1 << ") for " << trans;
                    }
                } else {
                    if((trans.get_data_length() + offset) >= (1 << axi_burst_size) * axi_burst_len) {
                        SCCERR(name) << "Illegal AXI settings: transaction data length (" << trans.get_data_length() << ") does not correspond to AxSIZE/AxLEN  setting ("
                                << axi_burst_size << "/" << axi_burst_len-1 << ") for " << trans;
                    }
                }
                open_tx_by_id[tlm::TLM_READ_COMMAND][axi_id].pop_front();
                rd_resp_beat_count[axi_id]=0;
            }
        }
    }
}

constexpr unsigned comb(axi::bar_e bar, axi::domain_e domain, axi::snoop_e snoop){
    return to_int(bar)<<10|to_int(domain)<<8|to_int(snoop);
};

void ace_protocol::check_properties(const payload_type &trans) {
    if(auto* ace_ext = trans.get_extension<axi::ace_extension>()){
        if(ace_ext->get_cache()&0xc0) {
            if(!ace_ext->get_cache())
                SCCERR(name)<<"Illegal ACEL settings: active allocate bit(s) requires modifiable bit set";
        }
        auto snoop = ace_ext->get_snoop();
        auto domain = ace_ext->get_domain();
        auto bar = ace_ext->get_barrier();
        switch(comb(bar, domain, snoop)){
        // Non-snooping
        case comb(bar_e::RESPECT_BARRIER, domain_e::NON_SHAREABLE, snoop_e::READ_NO_SNOOP):
        case comb(bar_e::RESPECT_BARRIER, domain_e::SYSTEM, snoop_e::READ_NO_SNOOP):
        // Coherent
        case comb(bar_e::RESPECT_BARRIER, domain_e::INNER_SHAREABLE, snoop_e::READ_ONCE):
        case comb(bar_e::RESPECT_BARRIER, domain_e::OUTER_SHAREABLE, snoop_e::READ_ONCE):
        case comb(bar_e::RESPECT_BARRIER, domain_e::INNER_SHAREABLE, snoop_e::READ_SHARED):
        case comb(bar_e::RESPECT_BARRIER, domain_e::OUTER_SHAREABLE, snoop_e::READ_SHARED):
        case comb(bar_e::RESPECT_BARRIER, domain_e::INNER_SHAREABLE, snoop_e::READ_CLEAN):
        case comb(bar_e::RESPECT_BARRIER, domain_e::OUTER_SHAREABLE, snoop_e::READ_CLEAN):
        case comb(bar_e::RESPECT_BARRIER, domain_e::INNER_SHAREABLE, snoop_e::READ_NOT_SHARED_DIRTY):
        case comb(bar_e::RESPECT_BARRIER, domain_e::OUTER_SHAREABLE, snoop_e::READ_NOT_SHARED_DIRTY):
        case comb(bar_e::RESPECT_BARRIER, domain_e::INNER_SHAREABLE, snoop_e::READ_UNIQUE):
        case comb(bar_e::RESPECT_BARRIER, domain_e::OUTER_SHAREABLE, snoop_e::READ_UNIQUE):
        case comb(bar_e::RESPECT_BARRIER, domain_e::INNER_SHAREABLE, snoop_e::CLEAN_UNIQUE):
        case comb(bar_e::RESPECT_BARRIER, domain_e::OUTER_SHAREABLE, snoop_e::CLEAN_UNIQUE):
        case comb(bar_e::RESPECT_BARRIER, domain_e::INNER_SHAREABLE, snoop_e::MAKE_UNIQUE):
        case comb(bar_e::RESPECT_BARRIER, domain_e::OUTER_SHAREABLE, snoop_e::MAKE_UNIQUE):
        // Cache maintenance
        case comb(bar_e::RESPECT_BARRIER, domain_e::NON_SHAREABLE, snoop_e::CLEAN_SHARED):
        case comb(bar_e::RESPECT_BARRIER, domain_e::INNER_SHAREABLE, snoop_e::CLEAN_SHARED):
        case comb(bar_e::RESPECT_BARRIER, domain_e::OUTER_SHAREABLE, snoop_e::CLEAN_SHARED):
        case comb(bar_e::RESPECT_BARRIER, domain_e::NON_SHAREABLE, snoop_e::CLEAN_INVALID):
        case comb(bar_e::RESPECT_BARRIER, domain_e::INNER_SHAREABLE, snoop_e::CLEAN_INVALID):
        case comb(bar_e::RESPECT_BARRIER, domain_e::OUTER_SHAREABLE, snoop_e::CLEAN_INVALID):
        case comb(bar_e::RESPECT_BARRIER, domain_e::NON_SHAREABLE, snoop_e::MAKE_INVALID):
        case comb(bar_e::RESPECT_BARRIER, domain_e::INNER_SHAREABLE, snoop_e::MAKE_INVALID):
        case comb(bar_e::RESPECT_BARRIER, domain_e::OUTER_SHAREABLE, snoop_e::MAKE_INVALID):
        // Barrier
        case comb(bar_e::MEMORY_BARRIER, domain_e::NON_SHAREABLE, snoop_e::READ_NO_SNOOP):
        case comb(bar_e::MEMORY_BARRIER, domain_e::INNER_SHAREABLE, snoop_e::READ_ONCE):
        case comb(bar_e::MEMORY_BARRIER, domain_e::OUTER_SHAREABLE, snoop_e::READ_ONCE):
        case comb(bar_e::MEMORY_BARRIER, domain_e::SYSTEM, snoop_e::READ_NO_SNOOP):
        // Non-snooping
        case comb(bar_e::RESPECT_BARRIER, domain_e::NON_SHAREABLE, snoop_e::WRITE_NO_SNOOP):
        case comb(bar_e::RESPECT_BARRIER, domain_e::SYSTEM, snoop_e::WRITE_NO_SNOOP):
        // Coherent
        case comb(bar_e::RESPECT_BARRIER, domain_e::INNER_SHAREABLE, snoop_e::WRITE_UNIQUE):
        case comb(bar_e::RESPECT_BARRIER, domain_e::OUTER_SHAREABLE, snoop_e::WRITE_UNIQUE):
        case comb(bar_e::RESPECT_BARRIER, domain_e::INNER_SHAREABLE, snoop_e::WRITE_LINE_UNIQUE):
        case comb(bar_e::RESPECT_BARRIER, domain_e::OUTER_SHAREABLE, snoop_e::WRITE_LINE_UNIQUE):
        // Memory update
        case comb(bar_e::MEMORY_BARRIER, domain_e::INNER_SHAREABLE, snoop_e::WRITE_CLEAN):
        case comb(bar_e::MEMORY_BARRIER, domain_e::OUTER_SHAREABLE, snoop_e::WRITE_CLEAN):
        case comb(bar_e::MEMORY_BARRIER, domain_e::SYSTEM, snoop_e::WRITE_CLEAN):
        case comb(bar_e::MEMORY_BARRIER, domain_e::INNER_SHAREABLE, snoop_e::WRITE_BACK):
        case comb(bar_e::MEMORY_BARRIER, domain_e::OUTER_SHAREABLE, snoop_e::WRITE_BACK):
        case comb(bar_e::MEMORY_BARRIER, domain_e::SYSTEM, snoop_e::WRITE_BACK):
        case comb(bar_e::MEMORY_BARRIER, domain_e::INNER_SHAREABLE, snoop_e::EVICT):
        case comb(bar_e::MEMORY_BARRIER, domain_e::OUTER_SHAREABLE, snoop_e::EVICT):
        case comb(bar_e::MEMORY_BARRIER, domain_e::INNER_SHAREABLE, snoop_e::WRITE_EVICT):
        case comb(bar_e::MEMORY_BARRIER, domain_e::OUTER_SHAREABLE, snoop_e::WRITE_EVICT):
        case comb(bar_e::MEMORY_BARRIER, domain_e::SYSTEM, snoop_e::WRITE_EVICT):
        // Barrier
        case comb(bar_e::MEMORY_BARRIER, domain_e::NON_SHAREABLE, snoop_e::WRITE_NO_SNOOP):
        case comb(bar_e::MEMORY_BARRIER, domain_e::INNER_SHAREABLE, snoop_e::WRITE_UNIQUE):
        case comb(bar_e::MEMORY_BARRIER, domain_e::OUTER_SHAREABLE, snoop_e::WRITE_UNIQUE):
        case comb(bar_e::MEMORY_BARRIER, domain_e::SYSTEM, snoop_e::WRITE_NO_SNOOP):
        break;
        default:
            SCCERR(name)<<"Illegal ACE settings: According to D3.1.1 Read and write Shareable transaction types of ARM IHI 0022H  the following setting is illegal:\n"
            << "AxBAR:"<<to_char(bar)<<", AxDOMAIN:"<<to_char(domain)<<", AxSNOOP:"<<to_char(snoop);
        }
        switch(snoop) {
        default:break;
        case  snoop_e::READ_CLEAN:
        case snoop_e::READ_NOT_SHARED_DIRTY:
        case snoop_e::READ_SHARED:
        case snoop_e::READ_UNIQUE:
        case snoop_e::CLEAN_UNIQUE:
        case snoop_e::MAKE_UNIQUE:
        case snoop_e::WRITE_LINE_UNIQUE:
        case snoop_e::EVICT:
            if(ace_ext->get_domain()==domain_e::NON_SHAREABLE) {
                SCCERR(name)<<"Illegal ACE settings: According to D3.1.6 Transaction constraints of ARM IHI 0022H  the following setting is illegal:\n"
                << "AxDOMAIN:"<<to_char(ace_ext->get_domain());
            }
            /* no break */
        case snoop_e::CLEAN_SHARED:
        case snoop_e::CLEAN_INVALID:
        case snoop_e::MAKE_INVALID:
        case snoop_e::WRITE_EVICT:
            if(ace_ext->get_domain()==domain_e::SYSTEM) {
                SCCERR(name)<<"Illegal ACE settings: According to D3.1.6 Transaction constraints of ARM IHI 0022H  the following setting is illegal:\n"
                << "AxDOMAIN:"<<to_char(ace_ext->get_domain());
            }
            if(ace_ext->get_length() && (1u<<ace_ext->get_size()) != bw) {
                SCCERR(name)<<"Illegal ACE settings: According to D3.1.6 Transaction constraints of ARM IHI 0022H  the following setting is illegal:\n"
                << "AxLEN:"<<to_char(ace_ext->get_length())<<", AxSIZE:"<<to_char(ace_ext->get_size())<<" with bus with:"<<bw;
            }
            auto width = bw;
            switch(ace_ext->get_burst()) {
            case burst_e::INCR:
                width=(1+ace_ext->get_length())*(1u<<ace_ext->get_size());
                /* no break */
            case burst_e::WRAP:
                if(trans.get_address()&(width-1)){
                    SCCERR(name)<<"Illegal ACE settings: According to D3.1.6 Transaction constraints of ARM IHI 0022H  the following setting is illegal:\n"
                    << "AxBURST:"<<to_char(ace_ext->get_burst())<<", ADDR:0x"<<std::hex<<trans.get_address();
                }
                break;
            case burst_e::FIXED:
                SCCERR(name)<<"Illegal ACE settings: According to D3.1.6 Transaction constraints of ARM IHI 0022H  the following setting is illegal:\n"
                << "AxBURST:"<<to_char(ace_ext->get_burst());
            }
            if(ace_ext->get_barrier()!= bar_e::IGNORE_BARRIER) {
                SCCERR(name)<<"Illegal ACE settings: According to D3.1.6 Transaction constraints of ARM IHI 0022H  the following setting is illegal:\n"
                << "AxBAR:"<<to_char(ace_ext->get_barrier());
            }
            if(!ace_ext->is_modifiable()) {
                SCCERR(name)<<"Illegal ACE settings: According to D3.1.6 Transaction constraints of ARM IHI 0022H  the following setting is illegal:\n"
                << "AxCACHE:"<<static_cast<unsigned>(ace_ext->get_cache());
            }
        }
    }
}

} /* namespace checker */
} /* namespace axi */
