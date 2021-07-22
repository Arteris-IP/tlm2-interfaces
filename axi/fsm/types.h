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

#pragma once

#include <array>
#include <axi/axi_tlm.h>
#include <functional>

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
    static const char* lut[] = {"RequestPhaseBeg", "WValidE",          "WReadyE",      "BegPartReqE",  "EndPartReqE", "BegReqE",
                                "EndReqE",         "ResponsePhaseBeg", "BegPartRespE", "EndPartRespE", "BegRespE",    "EndRespE",  "Ack"};
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
    AxiProtocolFsm* fsm = nullptr;
    //! pointer to the associated AXITLM payload
    axi::axi_protocol_types::tlm_payload_type* trans = nullptr;
    //! beat count of this transaction
    size_t beat_count = 0;
    //! indicator if this is a snoop access
    bool is_snoop = false;
    //! event indicating the end of the transaction
    sc_core::sc_event finish;
    //! additional data being used in the various adapters, @todo refactor to remove this forward definitions
    union {
        uint64_t progress;
        tlm::tlm_generic_payload* gp;
        void* pp;
    } p;
    sc_core::sc_time start;
    /**
     * reset all data members to their default
     */
    void reset() {
        trans = nullptr;
        beat_count = 0;
        is_snoop = false;
        p.progress = 0;
        start=sc_core::SC_ZERO_TIME;
    }
};

} // namespace fsm
} // namespace axi
