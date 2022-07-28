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

#pragma once

#include "types.h"
#include <axi/axi_tlm.h>
#include <deque>
#include <scc/fifo_w_cb.h>
#include <scc/peq.h>
#include <scc/report.h>
#include <tlm/scc/tlm_gp_shared.h>
#include <unordered_map>

namespace axi {
namespace fsm {
//! forward declaration to reduce dependencies
struct fsm_handle;

inline std::ostream& operator<<(std::ostream& os,
                                const std::tuple<axi::fsm::fsm_handle*, axi::fsm::protocol_time_point_e>&) {
    return os;
}

using axi::operator<<;

/**
 * @brief base class of all AXITLM based adapters and interfaces.
 */
struct base {
    //! aliases used in the class
    using payload_type = axi::axi_protocol_types::tlm_payload_type;
    using phase_type = axi::axi_protocol_types::tlm_phase_type;
    /**
     * @brief the constructor
     * @param transfer_width the bus width in bytes
     * @param wr_start the phase to start a write access
     */
    base(size_t transfer_width, bool coherent = false,
         axi::fsm::protocol_time_point_e wr_start = axi::fsm::RequestPhaseBeg);
    /**
     * @brief the destructor
     */
    virtual ~base() {}
    /**
     * @brief triggers the FSM based on TLM phases in the forward path. Should be called from np_transport_fw
     * of the respective derived class
     * @param trans the tlm generic payload
     * @param phase the protocol phase
     * @param t the annotated time for LT environments
     * @return the sync enum denoting if the transaction has been updated or just accepted
     */
    tlm::tlm_sync_enum nb_fw(payload_type& trans, phase_type const& phase, sc_core::sc_time& t);
    /**
     * @brief triggers the FSM based on TLM phases in the backward path. Should be called from np_transport_bw
     * of the respective derived class
     * @param trans the tlm generic payload
     * @param phase the protocol phase
     * @param t the annotated time for LT environments
     * @return the sync enum denoting if the transaction has been updated or just accepted
     */
    tlm::tlm_sync_enum nb_bw(payload_type& trans, phase_type const& phase, sc_core::sc_time& t);
    /**
     * @brief retrieve the FSM handle based on the transaction passed. If non exist one will be created
     * @param gp the pointer to the payload
     * @param ace flag indicating if the ACE protocaol has to be supported
     * @return handle to the FSM
     */
    axi::fsm::fsm_handle* find_or_create(payload_type* gp = nullptr, bool ace = false);
    /**
     * @brief function to create a fsm_handle. Needs to be implemented by the derived class
     * @return
     */
    virtual axi::fsm::fsm_handle* create_fsm_handle() = 0;
    /**
     * @brief this function is called to add the callbacks to the fsm handle during creation.
     * Needs to be implemented by the derived classes describing reactions upon entering and leaving a state
     * @param the handle of the active fsm
     */
    virtual void setup_callbacks(axi::fsm::fsm_handle*) = 0;
    /**
     * @brief processes the fsm_event_queue and triggers FSM aligned
     */
    void process_fsm_event();
    /**
     * @brief processes the fsm_clk_queue and triggers the FSM accordingly. Should be registered as rising-edge clock
     * callback
     */
    void process_fsm_clk_queue();
    /**
     * @brief processes the fsm_sched_queue and propagates events to fsm_clk_queue. Should be registered as falling-edge
     * clock callback
     */
    inline void schedule(axi::fsm::protocol_time_point_e e, tlm::scc::tlm_gp_shared_ptr& gp, unsigned cycles) {
        schedule(e, gp.get(), cycles);
    }
    void schedule(axi::fsm::protocol_time_point_e e, payload_type* gp, unsigned cycles) {
        SCCTRACE(instance_name) << "pushing sync event " << evt2str(e) << " for transaction " << *gp
                                << " (sync:" << cycles << ")";
        fsm_clk_queue.push_back(std::make_tuple(e, gp, cycles));
    }
    /**
     * @brief processes the fsm_sched_queue and propagates events to fsm_clk_queue. Should be registered as falling-edge
     * clock callback
     */
    inline void schedule(axi::fsm::protocol_time_point_e e, tlm::scc::tlm_gp_shared_ptr& gp, sc_core::sc_time delay,
                         bool syncronize = false) {
        schedule(e, gp.get(), delay, syncronize);
    }
    void schedule(axi::fsm::protocol_time_point_e e, payload_type* gp, sc_core::sc_time delay,
                  bool syncronize = false) {
        SCCTRACE(instance_name) << "pushing event " << evt2str(e) << " for transaction " << *gp << " (delay " << delay
                                << ")";
        fsm_event_queue.notify(std::make_tuple(e, gp, syncronize), delay);
    }
    /**
     * @brief triggers the FSM with event and given transaction
     * @param event the event triggering the FSM
     * @param trans the AXITLM transaction the event belongs to
     */
    inline void react(axi::fsm::protocol_time_point_e event, tlm::scc::tlm_gp_shared_ptr& trans) {
        react(event, trans.get());
    }

    inline void react(axi::fsm::protocol_time_point_e event, payload_type* trans) {
        SCCTRACE(instance_name) << "reacting on event " << evt2str(static_cast<unsigned>(event)) << " for trans "
                                << *trans;
        auto fsm_hndl = active_fsm[trans];
        if(!fsm_hndl) {
            SCCFATAL(instance_name) << "No valid FSM found for trans " << std::hex << trans;
            throw std::runtime_error("No valid FSM found for trans");
        }
        react(event, fsm_hndl);
    }

    void react(axi::fsm::protocol_time_point_e, axi::fsm::fsm_handle*);

    ::scc::peq<std::tuple<axi::fsm::protocol_time_point_e, payload_type*, bool>> fsm_event_queue;

    ::scc::fifo_w_cb<std::tuple<axi::fsm::protocol_time_point_e, payload_type*, unsigned>> fsm_clk_queue;

    sc_core::sc_process_handle fsm_clk_queue_hndl;

    size_t transfer_width_in_bytes;

    const axi::fsm::protocol_time_point_e wr_start;

    const bool coherent;

    std::unordered_map<payload_type*, axi::fsm::fsm_handle*> active_fsm;

    std::deque<axi::fsm::fsm_handle*> idle_fsm;

    std::vector<std::unique_ptr<axi::fsm::fsm_handle>> allocated_fsm;

    std::string instance_name;

    sc_core::sc_event finish_evt;
};
} // namespace fsm
} // namespace axi
