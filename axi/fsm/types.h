/*
 * Copyright 2020 -2022 Arteris IP
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

#include <array>
#include <axi/axi_tlm.h>
#include <functional>
#include <tlm/scc/tlm_gp_shared.h>

namespace axi {
namespace fsm {

// fsm event enumerators
enum protocol_time_point_e {
    RequestPhaseBeg = 0, // only for triggering the request phase
    WValidE,
    WReadyE,
    BegPartReqE,
    EndPartReqE,
    BegReqE,
    EndReqE,
    ResponsePhaseBeg,
    BegPartRespE,
    EndPartRespE,
    BegRespE,
    EndRespE,
    Ack,
    CB_CNT
};

inline const char* evt2str(unsigned evt) {
    static const char* lut[] = {
        "RequestPhaseBeg",  "WValidE",      "WReadyE",      "BegPartReqE", "EndPartReqE", "BegReqE", "EndReqE",
        "ResponsePhaseBeg", "BegPartRespE", "EndPartRespE", "BegRespE",    "EndRespE",    "Ack"};
    return lut[evt];
}
//! alias for the callback function
using protocol_cb = std::array<std::function<void(void)>, CB_CNT>;
//! forward declaration of the FSM itself
struct AxiProtocolFsm;
/**
 * a handle class holding the FSM and associated data
 */
struct fsm_handle {
    //! pointer to the FSM
    AxiProtocolFsm* const fsm;
    //! pointer to the associated AXITLM payload
    tlm::scc::tlm_gp_shared_ptr trans{nullptr};
    //! beat count of this transaction
    size_t beat_count = 0;
    //! indicator if this is a snoop access
    bool is_snoop = false;
    //! event indicating the end of the transaction
    sc_core::sc_event finish;
    //! additional data being used in the various adapters, @todo refactor to remove this forward definitions
    tlm::scc::tlm_gp_shared_ptr gp{};
    union {
        uint64_t i64;
        struct {
            uint32_t i0;
            uint32_t i1;
        } i32;
        void* p;
    } aux;
    sc_core::sc_time start;
    /**
     * reset all data members to their default
     */
    void reset() {
        trans = nullptr;
        beat_count = 0;
        is_snoop = false;
        aux.i64 = 0;
        start = sc_core::SC_ZERO_TIME;
    }

    fsm_handle();

    ~fsm_handle();
};

} // namespace fsm
} // namespace axi
