/*
 * Copyright 2020 - 2022 Arteris IP
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

#include "base.h"
#include "protocol_fsm.h"
#include <scc/report.h>
#include <systemc>
#include <tlm/scc/tlm_id.h>
#include <tlm/scc/tlm_mm.h>
#include <scc/utilities.h>

using namespace sc_core;
using namespace tlm;
namespace axi {
namespace fsm {

fsm_handle::fsm_handle():fsm{new AxiProtocolFsm()} {
    fsm->initiate();
}

fsm_handle::~fsm_handle(){
    fsm->terminate();
    delete fsm;
}

base::base(size_t transfer_width, bool coherent, protocol_time_point_e wr_start)
: transfer_width_in_bytes(transfer_width / 8)
, wr_start(wr_start)
, coherent(coherent) {
    assert(wr_start == RequestPhaseBeg || wr_start == WValidE);
    idle_fsm.clear();
    active_fsm.clear();
    sc_core::sc_spawn_options opts;
    opts.dont_initialize();
    opts.spawn_method();
    opts.set_sensitivity(&fsm_event_queue.event());
    sc_core::sc_spawn(sc_bind(&base::process_fsm_event, this), sc_core::sc_gen_unique_name("fsm_event_method"), &opts);
    fsm_clk_queue.set_avail_cb([this]() {
        if(fsm_clk_queue_hndl.valid())
            fsm_clk_queue_hndl.enable();
    });
    fsm_clk_queue.set_empty_cb([this]() {
        if(fsm_clk_queue_hndl.valid())
            fsm_clk_queue_hndl.disable();
    });
}

fsm_handle* base::find_or_create(payload_type* gp, bool ace) {
    const static std::array<std::string, 3> cmd{{"RD", "WR", "IGN"}};
    auto it = active_fsm.find(gp);
    if(!gp || it == active_fsm.end()) {
        if(gp) {
            SCCTRACE(instance_name) << "creating fsm for trans " << *gp;
        } else {
            SCCTRACE(instance_name) << "creating fsm for undefined transaction";
        }
        if(idle_fsm.empty()) {
            auto fsm_hndl = create_fsm_handle();
            setup_callbacks(fsm_hndl);
            idle_fsm.push_back(fsm_hndl);
            allocated_fsm.emplace_back(fsm_hndl);
        }
        auto fsm_hndl = idle_fsm.front();
        idle_fsm.pop_front();
        fsm_hndl->reset();
        if(gp != nullptr) {
            fsm_hndl->trans = gp;
        } else {
            fsm_hndl->trans = ace ? tlm::scc::tlm_mm<>::get().allocate<ace_extension>()
                                  : tlm::scc::tlm_mm<>::get().allocate<axi4_extension>();
        }
        active_fsm.insert(std::make_pair(fsm_hndl->trans.get(), fsm_hndl));
        fsm_hndl->start = sc_time_stamp();
        return fsm_hndl;
    } else {
        it->second->start = sc_time_stamp();
        return it->second;
    }
}

void base::process_fsm_event() {
    while(auto e = fsm_event_queue.get_next()) {
        auto entry = e.get();
        if(std::get<2>(entry))
            schedule(std::get<0>(entry), std::get<1>(entry), 0);
        else
            react(std::get<0>(entry), std::get<1>(entry));
    }
}

void base::process_fsm_clk_queue() {
    if(!fsm_clk_queue_hndl.valid())
        fsm_clk_queue_hndl = sc_process_handle(sc_get_current_process_handle());
    if(fsm_clk_queue.avail())
        while(fsm_clk_queue.avail()) {
            auto entry = fsm_clk_queue.front();
            if(std::get<2>(entry) == 0) {
                SCCTRACE(instance_name) << "processing event " << evt2str(std::get<0>(entry)) << " of trans "
                                        << *std::get<1>(entry);
                react(std::get<0>(entry), std::get<1>(entry));
            } else {
                std::get<2>(entry) -= 1;
                fsm_clk_queue.push_back(entry);
            }
            fsm_clk_queue.pop_front();
        }
    else
        // fall asleep if there is nothing to process
        fsm_clk_queue_hndl.disable();
}

void base::react(protocol_time_point_e event, axi::fsm::fsm_handle* fsm_hndl) {
    switch(event) {
    case WValidE:
        fsm_hndl->fsm->process_event(WReq());
        return;
    case WReadyE:
    case RequestPhaseBeg:
        if(is_burst(*fsm_hndl->trans) && fsm_hndl->trans->is_write() &&
           !is_dataless(fsm_hndl->trans->get_extension<axi::ace_extension>()))
            fsm_hndl->fsm->process_event(BegPartReq());
        else
            fsm_hndl->fsm->process_event(BegReq());
        return;
    case BegPartReqE:
        fsm_hndl->fsm->process_event(BegPartReq());
        return;
    case EndPartReqE:
        fsm_hndl->fsm->process_event(EndPartReq());
        return;
    case BegReqE:
        fsm_hndl->fsm->process_event(BegReq());
        return;
    case EndReqE:
        fsm_hndl->fsm->process_event(EndReq());
        return;
    case BegPartRespE:
        fsm_hndl->fsm->process_event(BegPartResp());
        return;
    case EndPartRespE:
        fsm_hndl->fsm->process_event(EndPartResp());
        return;
    case BegRespE:
        fsm_hndl->fsm->process_event(BegResp());
        return;
    case EndRespE:
        if(!coherent || fsm_hndl->is_snoop) {
            SCCTRACE(instance_name) << "freeing fsm for trans " << *fsm_hndl->trans;
            fsm_hndl->fsm->process_event(EndResp());
            active_fsm.erase(fsm_hndl->trans.get());
            fsm_hndl->trans = nullptr;
            idle_fsm.push_back(fsm_hndl);
            finish_evt.notify();
        } else {
            fsm_hndl->fsm->process_event(EndRespNoAck());
        }
        return;
    case Ack:
        SCCTRACE(instance_name) << "freeing fsm for trans " << *fsm_hndl->trans;
        fsm_hndl->fsm->process_event(AckRecv());
        active_fsm.erase(fsm_hndl->trans.get());
        fsm_hndl->trans = nullptr;
        idle_fsm.push_back(fsm_hndl);
        finish_evt.notify();
        return;
    default:
        SCCFATAL(instance_name) << "No valid protocol time point";
    }
}

tlm_sync_enum base::nb_fw(payload_type& trans, phase_type const& phase, sc_time& t) {
    SCCTRACE(instance_name) << "base::nb_fw " << phase << " of trans " << trans;
    if(phase == BEGIN_PARTIAL_REQ || phase == BEGIN_REQ) { // read/write
        auto fsm_hndl = find_or_create(&trans);
        if(!trans.is_read()) {
            protocol_time_point_e evt = axi::fsm::RequestPhaseBeg;
            if(fsm_hndl->beat_count == 0 && wr_start != RequestPhaseBeg)
                evt = wr_start;
            else
                evt = phase == BEGIN_PARTIAL_REQ ? BegPartReqE : BegReqE;
            if(t == SC_ZERO_TIME) {
                react(evt, &trans);
            } else {
                schedule(evt, &trans, t);
            }
        } else {
            if(t == SC_ZERO_TIME) {
                react(BegReqE, &trans);
            } else
                schedule(BegReqE, &trans, t);
        }
    } else if(phase == END_PARTIAL_RESP || phase == END_RESP) {
        if(t == SC_ZERO_TIME) {
            react(phase == END_RESP ? EndRespE : EndPartRespE, &trans);
        } else
            schedule(phase == END_RESP ? EndRespE : EndPartRespE, &trans, t);
    } else if(phase == END_REQ) { // snoop access resp
        if(t == SC_ZERO_TIME) {
            react(EndReqE, &trans);
        } else
            schedule(EndReqE, &trans, t);
    } else if(phase == BEGIN_PARTIAL_RESP || phase == BEGIN_RESP) { // snoop access resp
        if(t == SC_ZERO_TIME) {
            react(phase == BEGIN_RESP ? BegRespE : BegPartRespE, &trans);
        } else
            schedule(phase == BEGIN_RESP ? BegRespE : BegPartRespE, &trans, t);
    } else if(phase == axi::ACK) {
        if(t == SC_ZERO_TIME) {
            react(Ack, &trans);
        } else
            schedule(Ack, &trans, t);
    }
    return TLM_ACCEPTED;
}

tlm_sync_enum base::nb_bw(payload_type& trans, phase_type const& phase, sc_time& t) {
    SCCTRACE(instance_name) << "base::nb_bw " << phase << " of trans " << trans;
    if(phase == END_PARTIAL_REQ || phase == END_REQ) { // read/write
        if(t == SC_ZERO_TIME) {
            react(phase == END_REQ ? EndReqE : EndPartReqE, &trans);
        } else
            schedule(phase == END_REQ ? EndReqE : EndPartReqE, &trans, t);
    } else if(phase == BEGIN_PARTIAL_RESP || phase == BEGIN_RESP) { // read/write response
        if(t == SC_ZERO_TIME) {
            react(phase == BEGIN_RESP ? BegRespE : BegPartRespE, &trans);
        } else
            schedule(phase == BEGIN_RESP ? BegRespE : BegPartRespE, &trans, t);
    } else if(phase == BEGIN_REQ) { // snoop read
        auto fsm_hndl = find_or_create(&trans, true);
        fsm_hndl->is_snoop = true;
        if(t == SC_ZERO_TIME) {
            react(BegReqE, &trans);
        } else
            schedule(BegReqE, &trans, t);
    } else if(phase == END_PARTIAL_RESP || phase == END_RESP) { // snoop read response
        if(t == SC_ZERO_TIME) {
            react(phase == END_RESP ? EndRespE : EndPartRespE, &trans);
        } else
            schedule(phase == END_RESP ? EndRespE : EndPartRespE, &trans, t);
    }
    return TLM_ACCEPTED;
}

}
}
