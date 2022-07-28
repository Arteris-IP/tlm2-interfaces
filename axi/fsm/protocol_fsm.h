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

#include "types.h"
#include <array>
#include <boost/mpl/list.hpp>
#include <boost/statechart/custom_reaction.hpp>
#include <boost/statechart/event.hpp>
#include <boost/statechart/state.hpp>
#include <boost/statechart/state_machine.hpp>
#include <boost/statechart/transition.hpp>
#include <functional>
#include <iostream>

namespace mpl = boost::mpl;
namespace bsc = boost::statechart;

namespace axi {
namespace fsm {
// Event Declarations
struct WReq : bsc::event<WReq> {};
struct BegPartReq : bsc::event<BegPartReq> {};
struct EndPartReq : bsc::event<EndPartReq> {};
struct BegReq : bsc::event<BegReq> {};
struct EndReq : bsc::event<EndReq> {};
struct BegPartResp : bsc::event<BegPartResp> {};
struct EndPartResp : bsc::event<EndPartResp> {};
struct BegResp : bsc::event<BegResp> {};
struct EndResp : bsc::event<EndResp> {};
struct EndRespNoAck : bsc::event<EndRespNoAck> {};
struct AckRecv : bsc::event<AckRecv> {};
// State forward Declarations
struct Idle; // forward declaration
struct ATrans;
struct PartialRequest;
struct WriteIdle;
struct Request;
struct WaitForResponse;
struct PartialResponse;
struct ReadIdle;
struct Response;
struct WaitAck;
/**
 * State Machine Base Class Declaration
 */
struct AxiProtocolFsm : bsc::state_machine<AxiProtocolFsm, Idle> {
    void InvokeResponsePhaseBeg(const BegResp&) {
        if(cb.at(axi::fsm::ResponsePhaseBeg))
            cb.at(axi::fsm::ResponsePhaseBeg)();
    };
    void InvokeResponsePhaseBeg(const BegPartResp&) {
        if(cb.at(axi::fsm::ResponsePhaseBeg))
            cb.at(axi::fsm::ResponsePhaseBeg)();
    };
    void terminate(){
        for(auto& f:cb) f=nullptr;
        bsc::state_machine<AxiProtocolFsm, Idle>::terminate();
    }
    axi::fsm::protocol_cb cb;
};
//! the idle state
struct Idle : bsc::state<Idle, AxiProtocolFsm> { // @suppress("Class has a virtual method and non-virtual destructor")
    Idle(my_context ctx)
    : my_base(ctx) {}
    ~Idle() {
        if(context<AxiProtocolFsm>().cb.at(axi::fsm::RequestPhaseBeg))
            context<AxiProtocolFsm>().cb.at(axi::fsm::RequestPhaseBeg)();
    }
    typedef mpl::list<bsc::transition<BegPartReq, PartialRequest>, bsc::transition<BegReq, Request>,
                      bsc::transition<WReq, ATrans>>
        reactions;
};
//! special state to map AWREADY/WDATA of SNPS to AXI protocol
struct ATrans
: bsc::state<ATrans, AxiProtocolFsm> { // @suppress("Class has a virtual method and non-virtual destructor")
    ATrans(my_context ctx)
    : my_base(ctx) {
        if(context<AxiProtocolFsm>().cb.at(axi::fsm::WValidE))
            context<AxiProtocolFsm>().cb.at(axi::fsm::WValidE)();
    }
    ~ATrans() {
        if(context<AxiProtocolFsm>().cb.at(axi::fsm::WReadyE))
            context<AxiProtocolFsm>().cb.at(axi::fsm::WReadyE)();
    }
    typedef mpl::list<bsc::transition<BegPartReq, PartialRequest>, bsc::transition<BegReq, Request>> reactions;
};
//! a burst beat
struct PartialRequest
: bsc::state<PartialRequest, AxiProtocolFsm> { // @suppress("Class has a virtual method and non-virtual destructor")
    PartialRequest(my_context ctx)
    : my_base(ctx) {
        if(context<AxiProtocolFsm>().cb.at(axi::fsm::BegPartReqE))
            context<AxiProtocolFsm>().cb.at(axi::fsm::BegPartReqE)();
    }
    ~PartialRequest() {
        if(context<AxiProtocolFsm>().cb.at(axi::fsm::EndPartReqE))
            context<AxiProtocolFsm>().cb.at(axi::fsm::EndPartReqE)();
    }
    typedef bsc::transition<EndPartReq, WriteIdle> reactions;
};
//! the phase between 2 burst beats, should keep the link locked
struct WriteIdle
: bsc::simple_state<WriteIdle, AxiProtocolFsm> { // @suppress("Class has a virtual method and non-virtual destructor")
    typedef mpl::list<bsc::transition<BegPartReq, PartialRequest>, bsc::transition<BegReq, Request>> reactions;
};
//! the request, either the last beat of a write or the address phase of a read
struct Request
: bsc::state<Request, AxiProtocolFsm> { // @suppress("Class has a virtual method and non-virtual destructor")
    Request(my_context ctx)
    : my_base(ctx) {
        if(context<AxiProtocolFsm>().cb.at(axi::fsm::BegReqE))
            context<AxiProtocolFsm>().cb.at(axi::fsm::BegReqE)();
    }
    ~Request() {
        if(context<AxiProtocolFsm>().cb.at(axi::fsm::EndReqE))
            context<AxiProtocolFsm>().cb.at(axi::fsm::EndReqE)();
    }
    typedef mpl::list<
        bsc::transition<EndReq, WaitForResponse>,
        bsc::transition<BegResp, Response, AxiProtocolFsm, &AxiProtocolFsm::InvokeResponsePhaseBeg>,
        bsc::transition<BegPartResp, PartialResponse, AxiProtocolFsm, &AxiProtocolFsm::InvokeResponsePhaseBeg>>
        reactions;
};
//! the operation state where the target can do it's stuff
struct WaitForResponse
: bsc::state<WaitForResponse, AxiProtocolFsm> { // @suppress("Class has a virtual method and non-virtual destructor")
    WaitForResponse(my_context ctx)
    : my_base(ctx) {}
    ~WaitForResponse() {
        if(context<AxiProtocolFsm>().cb.at(axi::fsm::ResponsePhaseBeg))
            context<AxiProtocolFsm>().cb.at(axi::fsm::ResponsePhaseBeg)();
    }
    typedef mpl::list<bsc::transition<BegPartResp, PartialResponse>, bsc::transition<BegResp, Response>> reactions;
};
//! the beat of a burst response
struct PartialResponse
: bsc::state<PartialResponse, AxiProtocolFsm> { // @suppress("Class has a virtual method and non-virtual destructor")
    PartialResponse(my_context ctx)
    : my_base(ctx) {
        if(context<AxiProtocolFsm>().cb.at(axi::fsm::BegPartRespE))
            context<AxiProtocolFsm>().cb.at(axi::fsm::BegPartRespE)();
    }
    ~PartialResponse() {
        if(context<AxiProtocolFsm>().cb.at(axi::fsm::EndPartRespE))
            context<AxiProtocolFsm>().cb.at(axi::fsm::EndPartRespE)();
    }
    typedef bsc::transition<EndPartResp, ReadIdle> reactions;
};
//! the phase between 2 read burst response beats
struct ReadIdle
: bsc::simple_state<ReadIdle, AxiProtocolFsm> { // @suppress("Class has a virtual method and non-virtual destructor")
    typedef mpl::list<bsc::transition<BegPartResp, PartialResponse>, bsc::transition<BegResp, Response>> reactions;
};
//! the write response or the last read response (beat)
struct Response
: bsc::state<Response, AxiProtocolFsm> { // @suppress("Class has a virtual method and non-virtual destructor")
    Response(my_context ctx)
    : my_base(ctx) {
        if(context<AxiProtocolFsm>().cb.at(axi::fsm::BegRespE))
            context<AxiProtocolFsm>().cb.at(axi::fsm::BegRespE)();
    }
    ~Response() {
        if(context<AxiProtocolFsm>().cb.at(axi::fsm::EndRespE))
            context<AxiProtocolFsm>().cb.at(axi::fsm::EndRespE)();
    }
    typedef mpl::list<bsc::transition<EndResp, Idle>, bsc::transition<EndRespNoAck, WaitAck>> reactions;
};
//! waiting for ack in case of ACE access
struct WaitAck
: bsc::state<WaitAck, AxiProtocolFsm> { // @suppress("Class has a virtual method and non-virtual destructor")
    WaitAck(my_context ctx)
    : my_base(ctx) {}
    ~WaitAck() {
        if(context<AxiProtocolFsm>().cb.at(axi::fsm::Ack))
            context<AxiProtocolFsm>().cb.at(axi::fsm::Ack)();
    }
    typedef bsc::transition<AckRecv, Idle> reactions;
};
} // namespace fsm
} // namespace axi
