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
#include <axi/pe/axi_initiator.h>
#include <scc/report.h>
#include <tlm/scc/tlm_gp_shared.h>

using namespace sc_core;
using sem_lock = scc::ordered_semaphore::lock;

namespace axi {
namespace pe {

namespace {
uint8_t log2n(uint8_t siz) { return ((siz > 1) ? 1 + log2n(siz >> 1) : 0); }

} // anonymous namespace

SC_HAS_PROCESS(axi_initiator_b);

axi_initiator_b::axi_initiator_b(sc_core::sc_module_name nm,
        sc_core::sc_port_b<axi::axi_fw_transport_if<axi_protocol_types>>& port,
        size_t transfer_width, flavor_e flavor)
: sc_module(nm)
, socket_fw(port)
, transfer_width_in_bytes(transfer_width / 8)
, flavor(flavor) {
    fw_i.bind(*this);
    SC_METHOD(clk_counter);
    sensitive << clk_i.pos();

    if(flavor == flavor_e::AXI)
        for(auto i = 0u; i < 16; i++)
            sc_core::sc_spawn([this]() { snoop_thread(); });
}

axi_initiator_b::~axi_initiator_b() {
    for(auto& e : tx_state_by_tx)
        delete e.second;
}

void axi_initiator_b::end_of_elaboration() {
    clk_if = dynamic_cast<sc_core::sc_clock*>(clk_i.get_interface());
    for(auto i = 0U; i < outstanding_snoops.get_value(); ++i) {
        sc_spawn(sc_bind(&axi_initiator_b::snoop_thread, this));
    }
}

void axi_initiator_b::b_snoop(payload_type& trans, sc_core::sc_time& t) {
    if(bw_o.get_interface()) {
        auto latency = bw_o->transport(trans);
        if(latency < std::numeric_limits<unsigned>::max())
            t += latency * clk_period;
    }
}

tlm::tlm_sync_enum axi_initiator_b::nb_transport_bw(payload_type& trans, phase_type& phase, sc_core::sc_time& t) {
    SCCTRACE(SCMOD) << __FUNCTION__ << " received with phase " << phase << " with delay = " << t <<  " with trans " << trans;
    if(phase == tlm::BEGIN_REQ) {   // snoop
        snp_peq.notify(trans, t);
    } else if(phase == END_PARTIAL_RESP || phase == tlm::END_RESP) {  //snoop
        auto it = snp_state_by_id.find(&trans);
        sc_assert(it != snp_state_by_id.end());
        it->second->peq.notify(std::make_tuple(&trans, phase), t);
    } else {  // read/write
        auto it = tx_state_by_tx.find(&trans);
        sc_assert(it != tx_state_by_tx.end());
        auto txs = it->second;
        txs->peq.notify(std::make_tuple(&trans, phase), t);
    }
    return tlm::TLM_ACCEPTED;
}

void axi_initiator_b::invalidate_direct_mem_ptr(sc_dt::uint64 start_range, sc_dt::uint64 end_range) {}

tlm::tlm_phase axi_initiator_b::send(payload_type& trans, axi_initiator_b::tx_state* txs, tlm::tlm_phase phase) {
    sc_core::sc_time delay;
    SCCTRACE(SCMOD) << "Send " << phase << " of " << trans;
    tlm::tlm_sync_enum ret = socket_fw->nb_transport_fw(trans, phase, delay);
    if(ret == tlm::TLM_UPDATED) {
        wait(delay);
        SCCTRACE(SCMOD) << "Received " << phase << " for " << trans;
        return phase;
    } else {
        auto waiting = txs->peq.has_next();
        auto entry = txs->peq.get();
        if(waiting)
            SCCFATAL(SCMOD) << "there is a waiting " << std::get<0>(entry) << " with phase " << std::get<1>(entry);
        sc_assert(!txs->peq.has_next());
        sc_assert(std::get<0>(entry) == &trans);
        SCCTRACE(SCMOD) << "in send() Received " << std::get<1>(entry) << " for " << trans;
        return std::get<1>(entry);
    }
}

void axi_initiator_b::transport(payload_type& trans, bool blocking) {
    auto axi_id = get_axi_id(trans);
    if(flavor == flavor_e::AXI) {
        if(!trans.get_extension<axi::axi4_extension>() && !trans.get_extension<axi::axi3_extension>()) {
            auto ace = trans.set_extension<axi::ace_extension>(nullptr);
            sc_assert(ace && "No valid extension found in transaction");
            auto axi4 = new axi::axi4_extension();
            *static_cast<axi::axi4*>(axi4) = *static_cast<axi::axi4*>(ace);
            *static_cast<axi::common*>(axi4) = *static_cast<axi::common*>(ace);
            trans.set_extension(axi4);
            delete ace;
        }
    } else {
        sc_assert(trans.get_extension<axi::ace_extension>() && "No ACE extension found in transaction");
    }
    SCCTRACE(SCMOD) << "got transport req for " << trans;
    if(blocking) {
        sc_time t;
        socket_fw->b_transport(trans, t);
    } else {
        auto it = tx_state_by_tx.find(&trans);
        if(it == tx_state_by_tx.end()) {
            bool success;
            std::tie(it, success) = tx_state_by_tx.insert(std::make_pair(&trans, new tx_state()));
        }
        if(trans.is_read()) rd_waiting++;
        else wr_waiting++;
        auto& txs = it->second;
        auto timing_e = trans.set_auto_extension<atp::timing_params>(nullptr);

        if(enable_id_serializing.get_value()) {
            if(!id_mtx[axi_id]) {
                id_mtx[axi_id] = new scc::ordered_semaphore(1);
            }
            id_mtx[axi_id]->wait(); // wait until running tx with same id is over
        }
        txs->active_tx = &trans;
        auto burst_length = 0;
        if(auto e = trans.get_extension<axi::ace_extension>()) {
            burst_length = is_dataless(e) ? 1 : e->get_length() + 1;
        } else if(auto e = trans.get_extension<axi::axi4_extension>()) {
            burst_length = e->get_length() + 1;
        } else if(auto e = trans.get_extension<axi::axi3_extension>()) {
            burst_length = e->get_length() + 1;
        }
        SCCTRACE(SCMOD) << "start transport " << trans;
        tlm::tlm_phase next_phase{tlm::UNINITIALIZED_PHASE};
        if(!trans.is_read()) { // data less via write channel
            if(!data_interleaving.get_value()) { // Note that AXI4 does not allow write data interleaving, and ncore3 only supports AXI4.
                sem_lock lck(wr_chnl);
                wr_waiting--;
                wr_outstanding++;
                /// Timing
                for(unsigned i = 1; i < (timing_e ? timing_e->awtv : awtv.get_value()); ++i) {
                    wait(clk_i.posedge_event());
                }
                SCCTRACE(SCMOD) << "starting " << burst_length << " write beats of " << trans;
                for(unsigned i = 0; i < burst_length - 1; ++i) {
                    if(protocol_cb[axi::fsm::BegPartReqE])
                        protocol_cb[axi::fsm::BegPartReqE](trans, false);
                    auto res = send(trans, txs, axi::BEGIN_PARTIAL_REQ);
                    if(axi::END_PARTIAL_REQ != res)
                        SCCFATAL(SCMOD) << "target responded with " << res << " for the " << i << "th beat of "
                        << burst_length << " beats  in transaction " << trans;
                    for(unsigned i = 0; i < (timing_e ? timing_e->wbv : wbv.get_value()); ++i)
                        wait(clk_i.posedge_event());
                    if(protocol_cb[axi::fsm::EndPartReqE])
                        protocol_cb[axi::fsm::EndPartReqE](trans, false);
                }
                auto res = send(trans, txs, tlm::BEGIN_REQ);
                if(res == axi::BEGIN_PARTIAL_RESP || res == tlm::BEGIN_RESP)
                    next_phase = res;
                else if(res != tlm::END_REQ)
                    SCCERR(SCMOD) << "target did not repsond with END_REQ to a BEGIN_REQ";
                wait(clk_i.posedge_event());
            } else { // AXI3 allows data interleaving and there may be support for AXI3 in Symphony
                SCCTRACE(SCMOD) << "starting " << burst_length << " write beats of " << trans;
                for(unsigned i = 0; i < burst_length - 1; ++i) {
                    sem_lock lck(wr_chnl);
                    if(i==0){
                        wr_waiting--;
                        wr_outstanding++;
                        /// Timing
                        for(unsigned i = 1; i < (timing_e ? timing_e->awtv : awtv.get_value()); ++i)
                            wait(clk_i.posedge_event());
                    }
                    auto res = send(trans, txs, axi::BEGIN_PARTIAL_REQ);
                    sc_assert(axi::END_PARTIAL_REQ == res);
                    for(unsigned i = 1; i < (timing_e ? timing_e->wbv : wbv.get_value()); ++i)
                        wait(clk_i.posedge_event());
                }
                sem_lock lck(wr_chnl);
                if(burst_length==1){
                    wr_waiting--;
                    wr_outstanding++;
                }
                if(protocol_cb[axi::fsm::BegReqE])
                    protocol_cb[axi::fsm::BegReqE](trans, false);
                auto res = send(trans, txs, tlm::BEGIN_REQ);
                if(res == axi::BEGIN_PARTIAL_RESP || res == tlm::BEGIN_RESP)
                    next_phase = res;
                else if(res != tlm::END_REQ)
                    SCCERR(SCMOD) << "target did not repsond with END_REQ to a BEGIN_REQ";
                wait(clk_i.posedge_event());
                if(protocol_cb[axi::fsm::EndReqE])
                    protocol_cb[axi::fsm::EndReqE](trans, false);
            }
        } else {
            sem_lock lck(rd_chnl);
            rd_waiting--;
            rd_outstanding++;
            /// Timing
            for(unsigned i = 1; i < (timing_e ? timing_e->artv : artv.get_value()); ++i)
                wait(clk_i.posedge_event());
            SCCTRACE(SCMOD) << "starting address phase of " << trans;
            if(protocol_cb[axi::fsm::BegPartReqE])
                protocol_cb[axi::fsm::BegPartReqE](trans, false);
            auto res = send(trans, txs, tlm::BEGIN_REQ);
            if(res == axi::BEGIN_PARTIAL_RESP || res == tlm::BEGIN_RESP)
                next_phase = res;
            else if(res != tlm::END_REQ)
                SCCERR(SCMOD) << "target did not repsond with END_REQ to a BEGIN_REQ";
            wait(clk_i.posedge_event());
            if(protocol_cb[axi::fsm::EndReqE])
                protocol_cb[axi::fsm::EndReqE](trans, false);
        }
        auto finished = false;
        if(!trans.is_read() || !trans.get_data_length())
            burst_length = 1;
        const auto exp_burst_length = burst_length;
        do {
            // waiting for response
            auto entry = next_phase == tlm::UNINITIALIZED_PHASE ? txs->peq.get() : std::make_tuple(&trans, next_phase);
            next_phase = tlm::UNINITIALIZED_PHASE;
            // Handle optional CRESP response
            if(std::get<0>(entry) == &trans && std::get<1>(entry) == tlm::BEGIN_RESP) {
                if(protocol_cb[axi::fsm::BegRespE])
                    protocol_cb[axi::fsm::BegRespE](trans, false);
                SCCTRACE(SCMOD) << "received last beat of " << trans;
                auto delay_in_cycles = timing_e ? (trans.is_read() ? timing_e->rbr : timing_e->br) : br.get_value();
                for(unsigned i = 0; i < delay_in_cycles; ++i)
                    wait(clk_i.posedge_event());
                burst_length--;
                tlm::tlm_phase phase = tlm::END_RESP;
                sc_time delay = clk_if ? clk_if->period() - 1_ps : SC_ZERO_TIME;
                socket_fw->nb_transport_fw(trans, phase, delay);
                if(burst_length)
                    SCCWARN(SCMOD) << "got wrong number of burst beats, expected " << exp_burst_length << ", got "
                    << exp_burst_length - burst_length;
                wait(clk_i.posedge_event());
                if(protocol_cb[axi::fsm::EndRespE])
                    protocol_cb[axi::fsm::EndRespE](trans, false);
                finished = true;
            } else if(std::get<0>(entry) == &trans &&
                    std::get<1>(entry) == axi::BEGIN_PARTIAL_RESP) { // RDAT without CRESP case
                SCCTRACE(SCMOD) << "received beat = "<< burst_length<<" with trans " << trans;
                auto delay_in_cycles = timing_e ? timing_e->rbr : rbr.get_value();
                for(unsigned i = 0; i < delay_in_cycles; ++i)
                    wait(clk_i.posedge_event());
                burst_length--;
                if(protocol_cb[axi::fsm::BegPartRespE])
                    protocol_cb[axi::fsm::BegPartRespE](trans, false);
                tlm::tlm_phase phase = axi::END_PARTIAL_RESP;
                sc_time delay = clk_if ? clk_if->period() - 1_ps : SC_ZERO_TIME;
                auto res = socket_fw->nb_transport_fw(trans, phase, delay);
                if(res == tlm::TLM_UPDATED) {
                    next_phase = phase;
                    wait(delay);
                }
                if(protocol_cb[axi::fsm::EndPartRespE])
                    protocol_cb[axi::fsm::EndPartRespE](trans, false);
          }
        } while(!finished);
        if(flavor == flavor_e::ACE) {
            if(trans.is_read() && rla.get_value() != std::numeric_limits<unsigned>::max()) {
                for(unsigned i = 0; i < rla.get_value(); ++i)
                    wait(clk_i.posedge_event());
                tlm::tlm_phase phase = axi::ACK;
                sc_time delay = SC_ZERO_TIME;
                socket_fw->nb_transport_fw(trans, phase, delay);
                wait(clk_i.posedge_event());

            } else if(trans.is_write() && ba.get_value() != std::numeric_limits<unsigned>::max()) {
                for(unsigned i = 0; i < ba.get_value(); ++i)
                    wait(clk_i.posedge_event());
                tlm::tlm_phase phase = axi::ACK;
                sc_time delay = SC_ZERO_TIME;
                socket_fw->nb_transport_fw(trans, phase, delay);
                wait(clk_i.posedge_event());
            }
        }
        if(trans.is_read()) rd_outstanding--;
        else wr_outstanding--;
        SCCTRACE(SCMOD) << "finished non-blocking protocol";
        if(enable_id_serializing.get_value()) {
            id_mtx[axi_id]->post();
        }
        txs->active_tx = nullptr;
        any_tx_finished.notify(SC_ZERO_TIME);
    }
    SCCTRACE(SCMOD) << "finished transport req for " << trans;
}

// This process handles the SNOOP request received
void axi_initiator_b::snoop_thread() {
    tlm::scc::tlm_gp_shared_ptr trans{nullptr};
    while(true) {
        while(!(trans = snp_peq.get_next_transaction())) {
            wait(snp_peq.get_event());
        }
        snoops_in_flight++;
        SCCDEBUG(SCMOD) << "start snoop #" << snoops_in_flight;
        auto req_ext = trans->get_extension<ace_extension>();
        sc_assert(req_ext != nullptr);

        auto it = snp_state_by_id.find(&trans);
        if(it == snp_state_by_id.end()) {
            bool success;
            std::tie(it, success) = snp_state_by_id.insert(std::make_pair(trans.get(), new tx_state()));
        }

        sc_time delay = clk_if ? clk_if->period() - 1_ps : SC_ZERO_TIME;
        tlm::tlm_phase phase = tlm::END_REQ;
        // here delay is not used in nb_fw of following module
        // therefore one cycle delay between BEG_REQ and END_REQ should be explicitly called here??
        if(protocol_cb[axi::fsm::BegReqE])
            protocol_cb[axi::fsm::BegReqE](*trans, true);
        socket_fw->nb_transport_fw(*trans, phase, delay);
        auto cycles = 0U;
        if(bw_o.get_interface())
            cycles = bw_o->transport(*trans);
        if(protocol_cb[axi::fsm::EndReqE])
            protocol_cb[axi::fsm::EndReqE](*trans, true);
        if(cycles < std::numeric_limits<unsigned>::max()) {
            // we handle the snoop access ourselfs
            for(size_t i = 0; i <= cycles; ++i)
                wait(clk_i.posedge_event());
            snoop_resp(*trans);
        }
        // finish snoop response, should release tlm gp_shared_ptr
        SCCTRACE(SCMOD)<<" finish snoop response, release gp_shared_ptr";
        snoops_in_flight--;
        trans=nullptr;
    }
}

void axi_initiator_b::snoop_resp(payload_type& trans, bool sync) {
    auto it = snp_state_by_id.find(&trans);
    sc_assert(it != snp_state_by_id.end());
    auto& txs = it->second;
    auto data_len=trans.get_data_length();
    auto burst_length = data_len/transfer_width_in_bytes;
    if(burst_length<1)
        burst_length=1;
    tlm::tlm_phase next_phase{tlm::UNINITIALIZED_PHASE};
    auto delay_in_cycles = wbv.get_value();
    sem_lock lck(sresp_chnl);
    /*
     * here according to spec, ccresp should first be checked to see whether there is data transfer( decided by TC)
     * if there is data to transfer, start cache data transfer, otherwise only crresp
     * */
    SCCTRACE(SCMOD) << "starting snoop resp with " << burst_length << " beats of " << trans;
    for(unsigned i = 0; i < burst_length - 1; ++i) {
        if(protocol_cb[axi::fsm::BegPartRespE])
            protocol_cb[axi::fsm::BegPartRespE](trans, true);
        auto res = send(trans, txs, axi::BEGIN_PARTIAL_RESP);
        sc_assert(axi::END_PARTIAL_RESP == res);
        wait(clk_i.posedge_event());
        if(protocol_cb[axi::fsm::EndPartRespE])
            protocol_cb[axi::fsm::EndPartRespE](trans, true);
        for(unsigned i = 1; i < delay_in_cycles; ++i)
            wait(clk_i.posedge_event());
    }
    if(protocol_cb[axi::fsm::BegRespE])
        protocol_cb[axi::fsm::BegRespE](trans, true);
    auto res = send(trans, txs, tlm::BEGIN_RESP);
    if(res != tlm::END_RESP)
        SCCERR(SCMOD) << "target did not respond with END_RESP to a BEGIN_RESP";
    wait(clk_i.posedge_event());
    if(protocol_cb[axi::fsm::EndRespE])
        protocol_cb[axi::fsm::EndRespE](trans, true);
}
} // namespace pe
} // namespace axi
