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

#include "simple_target.h"
#include <axi/fsm/protocol_fsm.h>
#include <axi/fsm/types.h>
#include <scc/report.h>
#include <systemc>
#include <tuple>

using namespace sc_core;
using namespace tlm;
using namespace axi;
using namespace axi::fsm;
using namespace axi::pe;

/******************************************************************************
 * target
 ******************************************************************************/

axi_target_pe_b::axi_target_pe_b(const sc_core::sc_module_name& nm, sc_core::sc_port_b<axi::axi_bw_transport_if<axi_protocol_types>>& port,
                                 size_t transfer_width)
: sc_module(nm)
, base(transfer_width)
, socket_bw(port) {
    add_attribute(rd_data_interleaving);
    add_attribute(wr_data_accept_delay);
    add_attribute(rd_addr_accept_delay);
    add_attribute(rd_data_beat_delay);
    add_attribute(rd_resp_delay);
    add_attribute(wr_resp_delay);
    SC_METHOD(fsm_clk_method);
    dont_initialize();
    sensitive << clk_i.pos();
    SC_METHOD(process_rd_resp_queue);
    dont_initialize();
    sensitive << clk_i.pos();
    SC_THREAD(send_wr_resp_thread);
    SC_THREAD(send_rd_resp_thread);
    SC_THREAD(rd_resp_thread);
}

void axi_target_pe_b::end_of_elaboration() {
    clk_if = dynamic_cast<sc_core::sc_clock*>(clk_i.get_interface());
}

void axi::pe::axi_target_pe_b::start_of_simulation() {
    if(!rd_data_interleaving.value && !operation_cb) {
        operation_cb=[this](payload_type& trans)-> unsigned {
            if(trans.is_write()) return wr_resp_delay.value;
            rd_resp_queue.push_back(std::make_tuple(&trans, rd_resp_delay.value));
            return std::numeric_limits<unsigned>::max();
        };
    }
}

void axi_target_pe_b::b_transport(payload_type& trans, sc_time& t) {
    auto latency = operation_cb ? operation_cb(trans) : trans.is_read() ? rd_resp_delay.value : wr_resp_delay.value;
    trans.set_dmi_allowed(false);
    trans.set_response_status(tlm::TLM_OK_RESPONSE);
    auto* i = clk_i.get_interface();
    if(clk_if) {
        t += clk_if->period() * latency;
    }
}

tlm_sync_enum axi_target_pe_b::nb_transport_fw(payload_type& trans, phase_type& phase, sc_time& t) { return nb_fw(trans, phase, t); }

bool axi_target_pe_b::get_direct_mem_ptr(payload_type& trans, tlm_dmi& dmi_data) {
    trans.set_dmi_allowed(false);
    return false;
}

unsigned int axi_target_pe_b::transport_dbg(payload_type& trans) { return 0; }

fsm_handle* axi_target_pe_b::create_fsm_handle() { return new fsm_handle(); }

void axi_target_pe_b::setup_callbacks(fsm_handle* fsm_hndl) {
    fsm_hndl->fsm->cb[RequestPhaseBeg] = [this, fsm_hndl]() -> void { fsm_hndl->beat_count = 0; };
    fsm_hndl->fsm->cb[BegPartReqE] = [this, fsm_hndl]() -> void {
        if(wr_data_accept_delay.value)
            schedule(EndPartReqE, fsm_hndl->trans, wr_data_accept_delay.value-1);
        else
            schedule(EndPartReqE, fsm_hndl->trans, sc_core::SC_ZERO_TIME);
    };
    fsm_hndl->fsm->cb[EndPartReqE] = [this, fsm_hndl]() -> void {
        tlm::tlm_phase phase = axi::END_PARTIAL_REQ;
        sc_time t(clk_if?clk_if->period()-1_ps:SC_ZERO_TIME);
        auto ret = socket_bw->nb_transport_bw(*fsm_hndl->trans, phase, t);
    };
    fsm_hndl->fsm->cb[BegReqE] = [this, fsm_hndl]() -> void {
        auto latency = fsm_hndl->trans->is_read() ? rd_addr_accept_delay.value : wr_data_accept_delay.value;
        if(latency)
            schedule(EndReqE, fsm_hndl->trans, latency-1);
        else
            schedule(EndReqE, fsm_hndl->trans, sc_core::SC_ZERO_TIME);
    };
    fsm_hndl->fsm->cb[EndReqE] = [this, fsm_hndl]() -> void {
        tlm::tlm_phase phase = tlm::END_REQ;
        sc_time t(clk_if?clk_if->period()-1_ps:SC_ZERO_TIME);
        auto ret = socket_bw->nb_transport_bw(*fsm_hndl->trans, phase, t);
        fsm_hndl->trans->set_response_status(tlm::TLM_OK_RESPONSE);
        if(auto ext3 = fsm_hndl->trans->get_extension<axi3_extension>()) {
            ext3->set_resp(resp_e::OKAY);
        } else if(auto ext4 = fsm_hndl->trans->get_extension<axi4_extension>()) {
            ext4->set_resp(resp_e::OKAY);
        } else if(auto exta = fsm_hndl->trans->get_extension<ace_extension>()) {
            exta->set_resp(resp_e::OKAY);
        } else
            sc_assert(false && "No valid AXITLM extension found!");
        auto latency =
            operation_cb ? operation_cb(*fsm_hndl->trans) : fsm_hndl->trans->is_read() ? rd_resp_delay.value : wr_resp_delay.value;
        if(latency < std::numeric_limits<unsigned>::max()) {
            auto size = get_burst_lenght(fsm_hndl->trans) - 1;
            schedule(size && fsm_hndl->trans->is_read() ? BegPartRespE : BegRespE, fsm_hndl->trans, latency);
        }
    };
    fsm_hndl->fsm->cb[BegPartRespE] = [this, fsm_hndl]() -> void {
        // scheduling the response
        if(fsm_hndl->trans->is_read()){
            if(!send_rd_resp_fifo.nb_write(std::make_tuple(fsm_hndl, BegPartRespE)))
                SCCERR(SCMOD)<<"too many outstanding transactions";
        } else if(fsm_hndl->trans->is_write()){
            if(!send_wr_resp_fifo.nb_write(std::make_tuple(fsm_hndl, BegPartRespE)))
                SCCERR(SCMOD)<<"too many outstanding transactions";
        }
    };
    fsm_hndl->fsm->cb[EndPartRespE] = [this, fsm_hndl]() -> void {
        fsm_hndl->trans->is_read() ? rd_resp_ch.post() : wr_resp_ch.post();
        auto size = get_burst_lenght(fsm_hndl->trans) - 1;
        fsm_hndl->beat_count++;
        if(rd_data_beat_delay.value)
            schedule(fsm_hndl->beat_count < size ? BegPartRespE : BegRespE, fsm_hndl->trans, rd_data_beat_delay.value);
        else
            schedule(fsm_hndl->beat_count < size ? BegPartRespE : BegRespE, fsm_hndl->trans, SC_ZERO_TIME);
    };
    fsm_hndl->fsm->cb[BegRespE] = [this, fsm_hndl]() -> void {
        // scheduling the response
        if(fsm_hndl->trans->is_read()){
            if(!send_rd_resp_fifo.nb_write(std::make_tuple(fsm_hndl, BegRespE)))
                SCCERR(SCMOD)<<"too many outstanding transactions";
        } else if(fsm_hndl->trans->is_write()){
            if(!send_wr_resp_fifo.nb_write(std::make_tuple(fsm_hndl, BegRespE)))
                SCCERR(SCMOD)<<"too many outstanding transactions";
        }
    };
    fsm_hndl->fsm->cb[EndRespE] = [this, fsm_hndl]() -> void {
        fsm_hndl->trans->is_read() ? rd_resp_ch.post() : wr_resp_ch.post();
        if(!rd_data_interleaving.value && rd_resp.get_value()<rd_resp.get_capacity()){
            SCCTRACE(SCMOD)<<"finishing exclusive read response for address 0x"<<std::hex<<fsm_hndl->trans->get_address();
            rd_resp.post();
        }
    };
}

void axi::pe::axi_target_pe_b::operation_resp(payload_type& trans, unsigned clk_delay){
    auto e = axi::get_burst_lenght(trans)==0 || trans.is_write()? axi::fsm::BegRespE:BegPartRespE;
    if(clk_delay)
        schedule(e, &trans, clk_delay);
    else
        schedule(e, &trans, SC_ZERO_TIME);
}

void axi::pe::axi_target_pe_b::send_rd_resp_thread() {
    std::tuple<fsm::fsm_handle*, axi::fsm::protocol_time_point_e> entry;
    while(true) {
        // waiting for responses to send
        wait(send_rd_resp_fifo.data_written_event());
        while(send_rd_resp_fifo.nb_read(entry)) {
            // there is something to send
            auto fsm_hndl = std::get<0>(entry);
            auto tp = std::get<1>(entry);
            sc_time t;
            tlm::tlm_phase phase{tp == BegPartRespE ? axi::BEGIN_PARTIAL_RESP : tlm::tlm_phase(tlm::BEGIN_RESP)};
            // wait to get ownership of the response channel
            rd_resp_ch.wait();
            if(socket_bw->nb_transport_bw(*fsm_hndl->trans, phase, t) == tlm::TLM_UPDATED) {
                schedule(phase == tlm::END_RESP ? EndRespE : EndPartRespE, fsm_hndl->trans, 0);
            }
        }
    }
}

void axi::pe::axi_target_pe_b::send_wr_resp_thread() {
    std::tuple<fsm::fsm_handle*, axi::fsm::protocol_time_point_e> entry;
    while(true) {
        // waiting for responses to send
        wait(send_wr_resp_fifo.data_written_event());
        while(send_wr_resp_fifo.nb_read(entry)) {
            // there is something to send
            auto fsm_hndl = std::get<0>(entry);
            auto tp = std::get<1>(entry);
            sc_time t;
            tlm::tlm_phase phase{tlm::tlm_phase(tlm::BEGIN_RESP)};
            // wait to get ownership of the response channel
            wr_resp_ch.wait();
            if(socket_bw->nb_transport_bw(*fsm_hndl->trans, phase, t) == tlm::TLM_UPDATED) {
                schedule(phase == tlm::END_RESP ? EndRespE : EndPartRespE, fsm_hndl->trans, 0);
            }
        }
    }
}

void axi::pe::axi_target_pe_b::process_rd_resp_queue() {
    while (rd_resp_queue.avail()) {
        auto& entry = rd_resp_queue.front();
        if (std::get<1>(entry) == 0) {
            rd_resp_fifo.write(std::get<0>(entry));
        } else {
            std::get<1>(entry) -= 1;
            rd_resp_queue.push_back(entry);
        }
        rd_resp_queue.pop_front();
    }
}

void axi::pe::axi_target_pe_b::rd_resp_thread() {
    while(true){
        auto trans = rd_resp_fifo.read();
        rd_resp.wait();
        SCCTRACE(SCMOD)<<"starting exclusive read response for address 0x"<<std::hex<<trans->get_address();
        operation_resp(*trans, rd_data_beat_delay.value);
    }
}
