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

#ifndef SC_INCLUDE_DYNAMIC_PROCESSES
#define SC_INCLUDE_DYNAMIC_PROCESSES
#endif

#include <axi/fsm/protocol_fsm.h>
#include <axi/fsm/types.h>
#include <axi/pe/ace_target_pe.h>
#include <scc/mt19937_rng.h>
#include <scc/report.h>
#include <scc/utilities.h>
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
struct ace_target_pe::bw_intor_impl : public tlm::scc::pe::intor_bw_nb {
    ace_target_pe* const that;
    bw_intor_impl(ace_target_pe* that)
    : that(that) {}
    unsigned transport(tlm::tlm_generic_payload& payload) override {
        if((payload.is_read() && that->rd_resp_fifo.num_free())) {
            that->rd_resp_fifo.write(&payload);
            return 0;
        } else if((payload.is_write() && that->wr_resp_fifo.num_free())) {
            that->wr_resp_fifo.write(&payload);
            return 0;
        }
        return std::numeric_limits<unsigned>::max();
    }
    virtual ~bw_intor_impl() = default;
};

#if SYSTEMC_VERSION < 20250221
SC_HAS_PROCESS(ace_target_pe);
#endif
ace_target_pe::ace_target_pe(const sc_core::sc_module_name& nm, size_t transfer_width)
: sc_module(nm)
, base(transfer_width, true) // coherent true
, bw_intor(new bw_intor_impl(this)) {
    isckt_axi.bind(*this);
    instance_name = name();

    bw_i.bind(*bw_intor);

    SC_METHOD(fsm_clk_method);
    dont_initialize();
    sensitive << clk_i.pos();
}

ace_target_pe::~ace_target_pe() = default;

void ace_target_pe::end_of_elaboration() { clk_if = dynamic_cast<sc_core::sc_clock*>(clk_i.get_interface()); }

void ace_target_pe::start_of_simulation() {
    if(!socket_bw)
        SCCFATAL(SCMOD) << "No backward interface registered!";
}

inline unsigned get_cci_randomized_value(cci::cci_param<int> const& p) {
    if(p.get_value() < 0)
        return ::scc::MT19937::uniform(0, -p.get_value());
    return p.get_value();
}

void ace_target_pe::b_transport(payload_type& trans, sc_time& t) {
    auto latency = operation_cb      ? operation_cb(trans)
                   : trans.is_read() ? get_cci_randomized_value(rd_resp_delay)
                                     : get_cci_randomized_value(wr_resp_delay);
    trans.set_dmi_allowed(false);
    trans.set_response_status(tlm::TLM_OK_RESPONSE);
    if(clk_if)
        t += clk_if->period() * latency;
}

tlm_sync_enum ace_target_pe::nb_transport_fw(payload_type& trans, phase_type& phase, sc_time& t) {
    SCCTRACE(SCMOD) << "in nb_transport_fw receives pahse " << phase;
    auto ret = TLM_ACCEPTED;
    if(phase == END_REQ) { // snoop
        schedule(phase == END_REQ ? EndReqE : EndPartReqE, &trans, t, false);
    } else if(phase == BEGIN_PARTIAL_RESP || phase == BEGIN_RESP) { // snoop response
        schedule(phase == BEGIN_RESP ? BegRespE : BegPartRespE, &trans, t, false);
    } else { //  forward read/Write
        if(phase == axi::ACK)
            return tlm::TLM_COMPLETED;
        SCCTRACE(SCMOD) << " forward via axi_i_sckt, in nb_transport_fw () with phase " << phase;
        return isckt_axi->nb_transport_fw(trans, phase, t);
    }
    return ret;
}

bool ace_target_pe::get_direct_mem_ptr(payload_type& trans, tlm_dmi& dmi_data) {
    trans.set_dmi_allowed(false);
    return false;
}

unsigned int ace_target_pe::transport_dbg(payload_type& trans) { return 0; }

fsm_handle* ace_target_pe::create_fsm_handle() { return new fsm_handle(); }

void ace_target_pe::setup_callbacks(fsm_handle* fsm_hndl) {
    fsm_hndl->fsm->cb[RequestPhaseBeg] = [this, fsm_hndl]() -> void {
        fsm_hndl->beat_count = 0;
        outstanding_cnt[fsm_hndl->trans->get_command()]++;
    };
    fsm_hndl->fsm->cb[BegPartReqE] = [this, fsm_hndl]() -> void {
        //  for snoop, state will not receive this event
    };
    fsm_hndl->fsm->cb[EndPartReqE] = [this, fsm_hndl]() -> void {
        //  for snoop, state will not receive this event
    };
    fsm_hndl->fsm->cb[BegReqE] = [this, fsm_hndl]() -> void {
        SCCTRACE(SCMOD) << "in BegReq of setup_cb";
        if(fsm_hndl->is_snoop) {
            sc_time t;
            tlm::tlm_phase phase = tlm::BEGIN_REQ;
            auto ret = socket_bw->nb_transport_bw(*fsm_hndl->trans, phase, t);
        }
    };
    fsm_hndl->fsm->cb[EndReqE] = [this, fsm_hndl]() -> void { SCCTRACE(SCMOD) << " EndReqE in setup_cb"; };
    fsm_hndl->fsm->cb[BegPartRespE] = [this, fsm_hndl]() -> void {
        SCCTRACE(SCMOD) << "in BegPartRespE of setup_cb,  ";
        sc_time t(clk_if ? ::scc::time_to_next_posedge(clk_if) - 1_ps : SC_ZERO_TIME);
        schedule(EndPartRespE, fsm_hndl->trans, t);
    };
    fsm_hndl->fsm->cb[EndPartRespE] = [this, fsm_hndl]() -> void {
        SCCTRACE(SCMOD) << "in EndPartRespE of setup_cb";
        // sc_time t(clk_if ? ::scc::time_to_next_posedge(clk_if) - 1_ps : SC_ZERO_TIME);
        sc_time t(SC_ZERO_TIME);
        tlm::tlm_phase phase = axi::END_PARTIAL_RESP;
        auto ret = socket_bw->nb_transport_bw(*fsm_hndl->trans, phase, t);
        fsm_hndl->beat_count++;
    };
    fsm_hndl->fsm->cb[BegRespE] = [this, fsm_hndl]() -> void {
        SCCTRACE(SCMOD) << "in BegRespE of setup_cb";
        sc_time t(clk_if ? ::scc::time_to_next_posedge(clk_if) - 1_ps : SC_ZERO_TIME);
        tlm::tlm_phase phase = tlm::END_RESP;
        auto ret = socket_bw->nb_transport_bw(*fsm_hndl->trans, phase, t);
        t = ::scc::time_to_next_posedge(clk_if);
        /* here *3 because after send() of intiator ,there is one cycle wait
         * target here need to wait long cycles so that gp_shared_ptr can be released
         */
        schedule(EndRespE, fsm_hndl->trans, 3 * t);
    };
    fsm_hndl->fsm->cb[EndRespE] = [this, fsm_hndl]() -> void {
        /*
        sc_time t(clk_if ? ::scc::time_to_next_posedge(clk_if) - 1_ps : SC_ZERO_TIME);
        tlm::tlm_phase phase = tlm::END_RESP;
        auto ret = socket_bw->nb_transport_bw(*fsm_hndl->trans, phase, t);
        */
        SCCTRACE(SCMOD) << "notifying finish ";
        fsm_hndl->finish.notify();
    };
    /*TBD threre is  ack for snoop_trans
     * */
}

void ace_target_pe::snoop(payload_type& trans) {
    SCCTRACE(SCMOD) << "got transport snoop trans ";
    bool ace = true;
    fsm_handle* fsm = find_or_create(&trans, true);
    fsm->is_snoop = true;
    react(RequestPhaseBeg, fsm->trans); //
    SCCTRACE(SCMOD) << "started non-blocking protocol";
    sc_core::wait(fsm->finish);
    SCCTRACE(SCMOD) << "finished non-blocking protocol";
}
