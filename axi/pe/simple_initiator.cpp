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

#define SC_INCLUDE_DYNAMIC_PROCESSES

#include "simple_initiator.h"
#include <axi/fsm/protocol_fsm.h>
#include <axi/fsm/types.h>
#include <scc/report.h>
#include <systemc>
#include <tlm/scc/tlm_id.h>

using namespace sc_core;
using namespace tlm;
using namespace axi;
using namespace axi::fsm;
using namespace axi::pe;

/******************************************************************************
 * initiator
 ******************************************************************************/

simple_initiator_b::simple_initiator_b(const sc_core::sc_module_name& nm,
                                       sc_core::sc_port_b<axi::axi_fw_transport_if<axi_protocol_types>>& port, size_t transfer_width)
: sc_module(nm)
, base(transfer_width)
, socket_fw(port) {
    add_attribute(wr_data_beat_delay);
    add_attribute(rd_data_accept_delay);
    add_attribute(wr_resp_accept_delay);
    drv_i.bind(*this);

    SC_METHOD(fsm_clk_method);
    dont_initialize();
    sensitive << clk_i.pos();
}

// bool simple_initiator_b::operation(bool write, uint64_t addr, unsigned len, const uint8_t* data, bool blocking) {
void simple_initiator_b::transport(payload_type& trans, bool blocking) {
    //    auto ext = trans.get_extension<axi::axi4_extension>();
    //    sc_assert(ext!=nullptr);
    SCCTRACE(SCMOD) << "got transport req for trans "<<std::hex<<&trans<<" to address 0x"<<trans.get_address();
    if(blocking) {
        sc_time t;
        socket_fw->b_transport(trans, t);
    } else {
        fsm_handle* fsm = find_or_create(&trans);
        if(trans.is_read()) {
            rd.wait();
        } else {
            wr.wait();
        }
        react(RequestPhaseBeg, fsm->trans);
        SCCTRACE(SCMOD) << "started non-blocking protocol";
        sc_core::wait(fsm->finish);
        SCCTRACE(SCMOD) << "finished non-blocking protocol";
    }
}

axi::fsm::fsm_handle* axi::pe::simple_initiator_b::create_fsm_handle() { return new fsm_handle(); }

void axi::pe::simple_initiator_b::setup_callbacks(axi::fsm::fsm_handle* fsm_hndl) {
    fsm_hndl->fsm->cb[RequestPhaseBeg] = [this, fsm_hndl]() -> void {
        fsm_hndl->beat_count = 0;
        if(protocol_cb[RequestPhaseBeg])
            protocol_cb[RequestPhaseBeg](*fsm_hndl->trans, fsm_hndl->is_snoop);
    };
    fsm_hndl->fsm->cb[BegPartReqE] = [this, fsm_hndl]() -> void {
        sc_time t;
        tlm::tlm_phase phase = axi::BEGIN_PARTIAL_REQ;
        auto ret = socket_fw->nb_transport_fw(*fsm_hndl->trans, phase, t);
        if(ret == tlm::TLM_UPDATED) {
            schedule(EndPartReqE, fsm_hndl->trans, t, true);
        }
        if(protocol_cb[BegPartReqE])
            protocol_cb[BegPartReqE](*fsm_hndl->trans, fsm_hndl->is_snoop);
    };
    fsm_hndl->fsm->cb[EndPartReqE] = [this, fsm_hndl]() -> void {
        fsm_hndl->beat_count++;
        if(fsm_hndl->beat_count < (get_burst_lenght(fsm_hndl->trans) - 1))
            if(::scc::get_value(wr_data_beat_delay) > 0)
                schedule(BegPartReqE, fsm_hndl->trans, ::scc::get_value(wr_data_beat_delay) - 1);
            else
                schedule_imm(BegPartReqE, fsm_hndl->trans);
        else if(::scc::get_value(wr_data_beat_delay) > 0)
            schedule(BegReqE, fsm_hndl->trans, ::scc::get_value(wr_data_beat_delay) - 1);
        else
            schedule_imm(BegReqE, fsm_hndl->trans);
        if(protocol_cb[EndPartReqE])
            protocol_cb[EndPartReqE](*fsm_hndl->trans, fsm_hndl->is_snoop);
    };
    fsm_hndl->fsm->cb[BegReqE] = [this, fsm_hndl]() -> void {
        if(fsm_hndl->is_snoop) {
            schedule(EndReqE, fsm_hndl->trans);
        } else {
            sc_time t;
            tlm::tlm_phase phase = tlm::BEGIN_REQ;
            auto ret = socket_fw->nb_transport_fw(*fsm_hndl->trans, phase, t);
            if(ret == tlm::TLM_UPDATED) {
                schedule(EndReqE, fsm_hndl->trans, t, true);
            }
        }
        if(protocol_cb[BegReqE])
            protocol_cb[BegReqE](*fsm_hndl->trans, fsm_hndl->is_snoop);
    };
    fsm_hndl->fsm->cb[EndReqE] = [this, fsm_hndl]() -> void {
        if(fsm_hndl->is_snoop) {
            tlm::tlm_phase phase = tlm::END_REQ;
            sc_time t;
            auto ret = socket_fw->nb_transport_fw(*fsm_hndl->trans, phase, t);
            auto ext = fsm_hndl->trans->get_extension<ace_extension>();
            sc_assert(ext && "No ACE extension found for snoop access");
            fsm_hndl->beat_count = 0;
            fsm_hndl->trans->set_response_status(tlm::TLM_OK_RESPONSE);
            auto latency = snoop_latency;
            if(snoop_cb)
                latency = (*snoop_cb)(*fsm_hndl->trans);
            else {
                ext->set_snoop_data_transfer(false);
                ext->set_snoop_error(false);
                ext->set_pass_dirty(false);
                ext->set_shared(false);
                ext->set_snoop_was_unique(false);
            }
            if(latency < std::numeric_limits<unsigned>::max())
                schedule(ext->is_snoop_data_transfer() && ext->get_length() > 0 ? BegPartRespE : BegRespE, fsm_hndl->trans, latency);
        } else {
            // auto ext = fsm_hndl->trans->get_extension<axi::axi4_extension>();
            if(fsm_hndl->trans->is_write())
                wr.post();
            else
                rd.post();
        }
        if(protocol_cb[EndReqE])
            protocol_cb[EndReqE](*fsm_hndl->trans, fsm_hndl->is_snoop);
    };
    fsm_hndl->fsm->cb[BegPartRespE] = [this, fsm_hndl]() -> void {
        if(fsm_hndl->is_snoop) {
            tlm::tlm_phase phase = axi::BEGIN_PARTIAL_RESP;
            sc_time t;
            auto ret = socket_fw->nb_transport_fw(*fsm_hndl->trans, phase, t);
        } else {
            if(::scc::get_value(rd_data_accept_delay))
                schedule(EndPartRespE, fsm_hndl->trans, ::scc::get_value(rd_data_accept_delay));
            else
                schedule_imm(EndPartRespE, fsm_hndl->trans);
        }
        if(protocol_cb[BegPartRespE])
            protocol_cb[BegPartRespE](*fsm_hndl->trans, fsm_hndl->is_snoop);
    };
    fsm_hndl->fsm->cb[EndPartRespE] = [this, fsm_hndl]() -> void {
        if(fsm_hndl->is_snoop) {
            auto size = get_burst_lenght(fsm_hndl->trans);
            fsm_hndl->beat_count++;
            schedule(fsm_hndl->beat_count < size ? BegPartRespE : BegRespE, fsm_hndl->trans, SC_ZERO_TIME);
        } else {
            sc_time t;
            tlm::tlm_phase phase = axi::END_PARTIAL_RESP;
            auto ret = socket_fw->nb_transport_fw(*fsm_hndl->trans, phase, t);
            fsm_hndl->beat_count++;
        }
        if(protocol_cb[EndPartRespE])
            protocol_cb[EndPartRespE](*fsm_hndl->trans, fsm_hndl->is_snoop);
    };
    fsm_hndl->fsm->cb[BegRespE] = [this, fsm_hndl]() -> void {
        if(fsm_hndl->is_snoop) {
            tlm::tlm_phase phase = tlm::BEGIN_RESP;
            sc_time t;
            auto ret = socket_fw->nb_transport_fw(*fsm_hndl->trans, phase, t);
        } else {
            auto del = fsm_hndl->trans->is_read() ? ::scc::get_value(rd_data_accept_delay) : ::scc::get_value(wr_resp_accept_delay);
            if(del)
                schedule(EndRespE, fsm_hndl->trans, del);
            else
                schedule_imm(EndRespE, fsm_hndl->trans);
        }
        if(protocol_cb[BegRespE])
            protocol_cb[BegRespE](*fsm_hndl->trans, fsm_hndl->is_snoop);
    };
    fsm_hndl->fsm->cb[EndRespE] = [this, fsm_hndl]() -> void {
        if(!fsm_hndl->is_snoop) {
            sc_time t;
            tlm::tlm_phase phase = tlm::END_RESP;
            auto ret = socket_fw->nb_transport_fw(*fsm_hndl->trans, phase, t);
            fsm_hndl->finish.notify();
        }
        if(protocol_cb[EndRespE])
            protocol_cb[EndRespE](*fsm_hndl->trans, fsm_hndl->is_snoop);
    };
}

tlm_sync_enum simple_initiator_b::nb_transport_bw(payload_type& trans, phase_type& phase, sc_time& t) {
    auto ret = TLM_ACCEPTED;
    SCCTRACE(SCMOD) << "nb_transport_bw " << phase << " of trans " << std::hex << &trans << std::dec;
    if(phase == END_PARTIAL_REQ || phase == END_REQ) { // read/write
        schedule(phase == END_REQ ? EndReqE : EndPartReqE, &trans, t, true);
    } else if(phase == BEGIN_PARTIAL_RESP || phase == BEGIN_RESP) { // read/write response
        schedule(phase == BEGIN_RESP ? BegRespE : BegPartRespE, &trans, t, true);
    } else if(phase == BEGIN_REQ) { // snoop read
        auto fsm_hndl = find_or_create(&trans, true);
        fsm_hndl->is_snoop = true;
        schedule(BegReqE, &trans, t);
    } else if(phase == END_PARTIAL_RESP || phase == END_RESP) { // snoop read response
        schedule(phase == END_RESP ? EndRespE : EndPartRespE, &trans, t);
    }
    return ret;
}

void simple_initiator_b::b_snoop(payload_type& trans, sc_time& t) {
    if(drv_o.get_interface()) {
        auto latency =drv_o->transport(trans);
        if(latency < std::numeric_limits<unsigned>::max())
            t += latency * clk_period;
    } else if(snoop_cb) {
        auto latency = (*snoop_cb)(trans);
        if(latency < std::numeric_limits<unsigned>::max())
            t += latency * clk_period;
    }
}

void simple_initiator_b::snoop_resp(payload_type& trans, bool sync) {
    auto fsm_hndl = active_fsm[&trans];
    sc_assert(fsm_hndl != nullptr);
    auto ext = fsm_hndl->trans->get_extension<ace_extension>();
    auto size = ext->get_length();
    protocol_time_point_e e = ext->is_snoop_data_transfer() && size > 0 ? BegPartRespE : BegRespE;
    if(sync)
        schedule(e, fsm_hndl->trans);
    else
        react(e, fsm_hndl->trans);
}

void simple_initiator_b::invalidate_direct_mem_ptr(sc_dt::uint64 start_range, sc_dt::uint64 end_range) {}

struct my_data {
    int x;
    long y;
};
