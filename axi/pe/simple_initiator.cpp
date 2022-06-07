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
SC_HAS_PROCESS(simple_initiator_b);

simple_initiator_b::simple_initiator_b(const sc_core::sc_module_name& nm,
                                       sc_core::sc_port_b<axi::axi_fw_transport_if<axi_protocol_types>>& port,
                                       size_t transfer_width, bool ack_phase)
: sc_module(nm)
, base(transfer_width, ack_phase)
, socket_fw(port) {
    add_attribute(wr_data_beat_delay);
    add_attribute(rd_data_accept_delay);
    add_attribute(wr_resp_accept_delay);
    add_attribute(ack_resp_delay);
    drv_i.bind(*this);
    SC_METHOD(fsm_clk_method);
    dont_initialize();
    sensitive << clk_i.pos();
    SC_METHOD(process_snoop_resp);
    sensitive << clk_i.pos();
    snp_resp_queue.set_avail_cb([this]() {
        if(snp_resp_queue_hndl.valid())
            snp_resp_queue_hndl.enable();
    });
    snp_resp_queue.set_empty_cb([this]() {
        if(snp_resp_queue_hndl.valid())
            snp_resp_queue_hndl.disable();
    });
}

void simple_initiator_b::end_of_elaboration() { clk_if = dynamic_cast<sc_core::sc_clock*>(clk_i.get_interface()); }

// bool simple_initiator_b::operation(bool write, uint64_t addr, unsigned len, const uint8_t* data, bool blocking) {
void simple_initiator_b::transport(payload_type& trans, bool blocking) {
    //    auto ext = trans.get_extension<axi::axi4_extension>();
    //    sc_assert(ext!=nullptr);
    SCCTRACE(SCMOD) << "got transport req for trans " << trans;
    if(blocking) {
        sc_time t;
        socket_fw->b_transport(trans, t);
    } else {
        fsm_handle* fsm = find_or_create(&trans);
        if(trans.is_read()) {
            if(rd.trywait() < 0) {
                rd.wait();
                wait(clk_i.posedge_event());
            }
        } else {
            if(wr.trywait() < 0) {
                wr.wait();
                wait(clk_i.posedge_event());
            }
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
        auto& f = protocol_cb[RequestPhaseBeg];
        if(f)
            f(*fsm_hndl->trans, fsm_hndl->is_snoop);
    };
    fsm_hndl->fsm->cb[BegPartReqE] = [this, fsm_hndl]() -> void {
        sc_time t;
        tlm::tlm_phase phase = axi::BEGIN_PARTIAL_REQ;
        auto ret = socket_fw->nb_transport_fw(*fsm_hndl->trans, phase, t);
        if(ret == tlm::TLM_UPDATED) {
            schedule(EndPartReqE, fsm_hndl->trans, t, true);
        }
        auto& f = protocol_cb[BegPartReqE];
        if(f)
            f(*fsm_hndl->trans, fsm_hndl->is_snoop);
    };
    fsm_hndl->fsm->cb[EndPartReqE] = [this, fsm_hndl]() -> void {
        fsm_hndl->beat_count++;
        if(fsm_hndl->beat_count < (get_burst_lenght(*fsm_hndl->trans) - 1))
            if(::scc::get_value(wr_data_beat_delay) > 0)
                schedule(BegPartReqE, fsm_hndl->trans, ::scc::get_value(wr_data_beat_delay) - 1);
            else
                schedule(BegPartReqE, fsm_hndl->trans, SC_ZERO_TIME);
        else if(::scc::get_value(wr_data_beat_delay) > 0)
            schedule(BegReqE, fsm_hndl->trans, ::scc::get_value(wr_data_beat_delay) - 1);
        else
            schedule(BegReqE, fsm_hndl->trans, 0);
        auto& f = protocol_cb[EndPartReqE];
        if(f)
            f(*fsm_hndl->trans, fsm_hndl->is_snoop);
    };
    fsm_hndl->fsm->cb[BegReqE] = [this, fsm_hndl]() -> void {
        if(fsm_hndl->is_snoop) {
            schedule(EndReqE, fsm_hndl->trans, SC_ZERO_TIME);
        } else {
            sc_time t;
            tlm::tlm_phase phase = tlm::BEGIN_REQ;
            auto ret = socket_fw->nb_transport_fw(*fsm_hndl->trans, phase, t);
            if(ret == tlm::TLM_UPDATED) {
                schedule(EndReqE, fsm_hndl->trans, t, true);
            }
        }
        auto& f = protocol_cb[BegReqE];
        if(f)
            f(*fsm_hndl->trans, fsm_hndl->is_snoop);
    };
    fsm_hndl->fsm->cb[EndReqE] = [this, fsm_hndl]() -> void {
        if(fsm_hndl->is_snoop) {
            tlm::tlm_phase phase = tlm::END_REQ;
            sc_time t(clk_if ? ::scc::time_to_next_posedge(clk_if) - 1_ps : SC_ZERO_TIME);
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
            if(latency < std::numeric_limits<unsigned>::max()) {
                auto length = transfer_width_in_bytes/fsm_hndl->trans->get_data_length();
                auto evt = ext->is_snoop_data_transfer() && length > 1 ? BegPartRespE : BegRespE;
                snp_resp_queue.push_back(std::make_tuple(evt, fsm_hndl->trans.get(), latency));
            }
        } else {
            // auto ext = fsm_hndl->trans->get_extension<axi::axi4_extension>();
            if(fsm_hndl->trans->is_write())
                wr.post();
            else
                rd.post();
        }
        auto& f = protocol_cb[BegReqE];
        if(f)
            f(*fsm_hndl->trans, fsm_hndl->is_snoop);
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
                schedule(EndPartRespE, fsm_hndl->trans, SC_ZERO_TIME);
        }
        auto& f = protocol_cb[BegPartRespE];
        if(f)
            f(*fsm_hndl->trans, fsm_hndl->is_snoop);
    };
    fsm_hndl->fsm->cb[EndPartRespE] = [this, fsm_hndl]() -> void {
        if(fsm_hndl->is_snoop) {
            auto size = transfer_width_in_bytes/fsm_hndl->trans->get_data_length();
            if(size<1)
                size=1;
            fsm_hndl->beat_count++;
            schedule(fsm_hndl->beat_count < size ? BegPartRespE : BegRespE, fsm_hndl->trans, 0);
        } else {
            sc_time t(clk_if ? ::scc::time_to_next_posedge(clk_if) - 1_ps : SC_ZERO_TIME);
            tlm::tlm_phase phase = axi::END_PARTIAL_RESP;
            auto ret = socket_fw->nb_transport_fw(*fsm_hndl->trans, phase, t);
            fsm_hndl->beat_count++;
        }
        auto& f = protocol_cb[EndPartRespE];
        if(f)
            f(*fsm_hndl->trans, fsm_hndl->is_snoop);
    };
    fsm_hndl->fsm->cb[BegRespE] = [this, fsm_hndl]() -> void {
        if(fsm_hndl->is_snoop) {
            tlm::tlm_phase phase = tlm::BEGIN_RESP;
            sc_time t;
            auto ret = socket_fw->nb_transport_fw(*fsm_hndl->trans, phase, t);
        } else {
            auto del = fsm_hndl->trans->is_read() ? ::scc::get_value(rd_data_accept_delay)
                                                  : ::scc::get_value(wr_resp_accept_delay);
            if(del)
                schedule(EndRespE, fsm_hndl->trans, del);
            else
                schedule(EndRespE, fsm_hndl->trans, SC_ZERO_TIME);
        }
        auto& f = protocol_cb[BegRespE];
        if(f)
            f(*fsm_hndl->trans, fsm_hndl->is_snoop);
    };
    fsm_hndl->fsm->cb[EndRespE] = [this, fsm_hndl]() -> void {
        if(fsm_hndl->is_snoop) {
            snp.post();
        } else {
            sc_time t(clk_if ? ::scc::time_to_next_posedge(clk_if) - 1_ps : SC_ZERO_TIME);
            tlm::tlm_phase phase = tlm::END_RESP;
            auto ret = socket_fw->nb_transport_fw(*fsm_hndl->trans, phase, t);
            if(coherent) {
                schedule(Ack, fsm_hndl->trans, ::scc::get_value(ack_resp_delay));
            } else
                fsm_hndl->finish.notify();
        }
        auto& f = protocol_cb[EndRespE];
        if(f)
            f(*fsm_hndl->trans, fsm_hndl->is_snoop);
    };
    fsm_hndl->fsm->cb[Ack] = [this, fsm_hndl]() -> void {
        sc_time t;
        tlm::tlm_phase phase = axi::ACK;
        auto ret = socket_fw->nb_transport_fw(*fsm_hndl->trans, phase, t);
        fsm_hndl->finish.notify();
        auto& f = protocol_cb[Ack];
        if(f)
            f(*fsm_hndl->trans, fsm_hndl->is_snoop);
    };
}

tlm_sync_enum simple_initiator_b::nb_transport_bw(payload_type& trans, phase_type& phase, sc_time& t) {
    auto ret = TLM_ACCEPTED;
    SCCTRACE(SCMOD) << "nb_transport_bw " << phase << " of trans " << trans;
    if(phase == END_PARTIAL_REQ || phase == END_REQ) { // read/write
        schedule(phase == END_REQ ? EndReqE : EndPartReqE, &trans, t, false);
    } else if(phase == BEGIN_PARTIAL_RESP || phase == BEGIN_RESP) { // read/write response
        schedule(phase == BEGIN_RESP ? BegRespE : BegPartRespE, &trans, t, false);
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
        auto latency = drv_o->transport(trans);
        if(latency < std::numeric_limits<unsigned>::max())
            t += latency * clk_period;
    } else if(snoop_cb) {
        auto latency = (*snoop_cb)(trans);
        if(latency < std::numeric_limits<unsigned>::max())
            t += latency * clk_period;
    }
}

void simple_initiator_b::process_snoop_resp() {
    if(!snp_resp_queue_hndl.valid())
        snp_resp_queue_hndl = sc_process_handle(sc_get_current_process_handle());
    if(snp_resp_queue.avail())
        while(snp_resp_queue.avail()) {
            auto entry = snp_resp_queue.front();
            if(std::get<2>(entry) == 0) {
                if(snp.trywait() < 0)
                    snp_resp_queue.push_back(entry);
                else {
                    auto gp = std::get<1>(entry);
                    SCCTRACE(instance_name)
                        << "processing event " << evt2str(std::get<0>(entry)) << " of trans " << *gp;
                    react(std::get<0>(entry), std::get<1>(entry));
                }
            } else {
                std::get<2>(entry) -= 1;
                snp_resp_queue.push_back(entry);
            }
            snp_resp_queue.pop_front();
        }
    else
        // fall asleep if there is nothing to process
        snp_resp_queue_hndl.disable();
}

void simple_initiator_b::snoop_resp(payload_type& trans, bool sync) {
    auto fsm_hndl = active_fsm[&trans];
    sc_assert(fsm_hndl != nullptr);
    auto ext = fsm_hndl->trans->get_extension<ace_extension>();
    auto size = ext->get_length();
    protocol_time_point_e e = ext->is_snoop_data_transfer() && size > 0 ? BegPartRespE : BegRespE;
    if(sync)
        schedule(e, fsm_hndl->trans, 0);
    else
        react(e, fsm_hndl->trans);
}

void simple_initiator_b::invalidate_direct_mem_ptr(sc_dt::uint64 start_range, sc_dt::uint64 end_range) {}
