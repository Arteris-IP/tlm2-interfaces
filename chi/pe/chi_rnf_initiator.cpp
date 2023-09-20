/*
 * Copyright 2021 Arteris IP
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

#ifndef SC_INCLUDE_DYNAMIC_PROCESSES
#define SC_INCLUDE_DYNAMIC_PROCESSES
#endif
#include <atp/timing_params.h>
#include <axi/axi_tlm.h>
#include <cache/cache_info.h>
#include <chi/pe/chi_rnf_initiator.h>
#include <scc/report.h>
#include <util/strprintf.h>

using namespace sc_core;
using namespace chi;
using namespace chi::pe;
using sem_lock = scc::ordered_semaphore::lock;

namespace {
uint8_t log2n(uint8_t siz) { return ((siz > 1) ? 1 + log2n(siz >> 1) : 0); }
inline uintptr_t to_id(tlm::tlm_generic_payload& t) { return reinterpret_cast<uintptr_t>(&t); }
inline uintptr_t to_id(tlm::tlm_generic_payload* t) { return reinterpret_cast<uintptr_t>(t); }

} // anonymous namespace

SC_HAS_PROCESS(chi_rnf_initiator_b);

chi::pe::chi_rnf_initiator_b::chi_rnf_initiator_b(sc_core::sc_module_name nm,
        sc_core::sc_port_b<chi::chi_fw_transport_if<chi_protocol_types>>& port,
        size_t transfer_width)
: sc_module(nm)
, socket_fw(port)
, transfer_width_in_bytes(transfer_width / 8)
, cache("cache", [this](chi::chi_payload& gp, tlm::tlm_phase const& beg_ph, tlm::tlm_phase const& end_ph){
    this->send_flit(gp, beg_ph, end_ph);
}, transfer_width)
{
    add_attribute(home_node_id);
    add_attribute(src_id);
    add_attribute(data_interleaving);
    add_attribute(strict_income_order);
    add_attribute(use_legacy_mapping);

    SC_METHOD(clk_counter);
    sensitive << clk_i.pos();
    cache.clk_i(clk_i);
}

chi::pe::chi_rnf_initiator_b::~chi_rnf_initiator_b() {
}

void chi::pe::chi_rnf_initiator_b::b_snoop(payload_type& trans, sc_core::sc_time& t) {
    SCCWARN(SCMOD)<<"blocking snoop is not (yet) supported)";
}

tlm::tlm_sync_enum chi::pe::chi_rnf_initiator_b::nb_transport_bw(payload_type& trans, phase_type& phase,
        sc_core::sc_time& t) {
    if(phase == tlm::BEGIN_REQ) {
        if(auto credit_ext = trans.get_extension<chi_credit_extension>()) {
            if(credit_ext->type == credit_type_e::REQ) {
                SCCTRACEALL(SCMOD) << "Received " << credit_ext->count << " req "
                        << (credit_ext->count == 1 ? "credit" : "credits");
                for(auto i = 0U; i < credit_ext->count; ++i)
                    req_credits.post();
            }
            phase = tlm::END_RESP;
            trans.set_response_status(tlm::TLM_OK_RESPONSE);
            if(clk_if)
                t += clk_if->period() - 1_ps;
            return tlm::TLM_COMPLETED;
        } else if(auto snp_ext = trans.get_extension<chi_snp_extension>()) {
            SCCTRACEALL(SCMOD) << "Received snoop req "<<to_char(snp_ext->req.get_opcode()) << " for addr=0x"<<std::hex<<trans.get_address();;
            cache.handle_rxsnp(trans, t);
            phase = tlm::END_REQ;
            if(clk_if)
                t += clk_if->period()-1_ps;
            return tlm::TLM_UPDATED;
        } else {
            SCCFATAL(SCMOD) << "Illegal transaction received from HN";
        }
    } else if(phase == tlm::BEGIN_RESP) {
        SCCTRACE(SCMOD) << "RSP flit received. " << ", addr: 0x" << std::hex << trans.get_address();
        cache.handle_rxrsp(trans, t);
        phase = tlm::END_RESP;
        if(clk_if)
            t += clk_if->period()-1_ps;
       return tlm::TLM_UPDATED;
    } else if(phase == chi::BEGIN_PARTIAL_DATA || phase == chi::BEGIN_DATA) {
        SCCTRACE(SCMOD) << "RDAT flit received. " << ", addr: 0x" << std::hex << trans.get_address();
        cache.handle_rxdat(trans, t);
        phase = phase == chi::BEGIN_PARTIAL_DATA? chi::END_PARTIAL_DATA:chi::END_DATA;
        if(clk_if)
            t += clk_if->period()-1_ps;
        return tlm::TLM_UPDATED;
    } else {
        bw_flits.emplace_back(&trans, phase, t);
        bw_flits_evt.notify(SC_ZERO_TIME);
    }
    return tlm::TLM_ACCEPTED;
}

void chi::pe::chi_rnf_initiator_b::invalidate_direct_mem_ptr(sc_dt::uint64 start_range, sc_dt::uint64 end_range) {}

void chi::pe::chi_rnf_initiator_b::transport(payload_type& trans, bool blocking) {
    static const std::array<char const*, 3> str_table = {"read", "write", "ignore"};
    if(blocking) {
        SCCTRACE(SCMOD) << "got transport req";
        sc_time t;
        socket_fw->b_transport(trans, t);
    } else {
        if(auto req_ext = trans.get_extension<chi_ctrl_extension>()) {
            SCCTRACE(SCMOD) << "got "<< str_table[trans.get_command()]<<" req with txnid="<<req_ext->get_txn_id();
        } else {
            auto id = axi::get_axi_id(trans);
            SCCTRACE(SCMOD) << "got "<< str_table[trans.get_command()]<<" req with id="<<id;
        }
        sc_core::sc_time d= sc_core::SC_ZERO_TIME;
        cache.transport(trans, d);
    }
}

inline unsigned calculate_beats(chi::chi_protocol_types::tlm_payload_type& p, unsigned transfer_width_in_bytes) {
    // sc_assert(p.get_data_length() > 0);
    return p.get_data_length() < transfer_width_in_bytes ? 1 : p.get_data_length() / transfer_width_in_bytes;
}

void chi::pe::chi_rnf_initiator_b::send_flit(chi::pe::chi_rnf_initiator_b::payload_type& gp, tlm::tlm_phase const& beg_ph, tlm::tlm_phase const& end_ph){
    sc_core::sc_time d;
    tlm::tlm_phase p = beg_ph;
    auto res = socket_fw->nb_transport_fw(gp, p, d);
    if(res==tlm::TLM_ACCEPTED || (end_ph != tlm::UNINITIALIZED_PHASE && p != end_ph)) {
        auto found=false;
        auto gp_ptr = &gp;
        do {
            wait(bw_flits_evt);
            auto it = std::find_if(std::begin(bw_flits), std::end(bw_flits), [gp_ptr, end_ph](std::tuple<payload_type*, tlm::tlm_phase, sc_core::sc_time> const& e) {
                return std::get<0>(e)==gp_ptr && std::get<1>(e)==end_ph;
            });
            found = it!=std::end(bw_flits);
            if(found && std::get<2>(*it)>sc_core::SC_ZERO_TIME)
                wait(std::get<2>(*it));
        } while(!found);
    }
}
