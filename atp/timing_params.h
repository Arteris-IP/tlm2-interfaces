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

#pragma once

#include <tlm>

namespace atp {

struct timing_params : public tlm::tlm_extension<timing_params> {
    tlm_extension_base* clone() const override {
        timing_params* e = new timing_params(artv, awtv, wbv, rbr, br, start_soonest);
        return e;
    }

    void copy_from(tlm_extension_base const& from) override { assert(false && "copy_from() not supported"); }

    timing_params() = delete;

    timing_params(unsigned artv, unsigned awtv, unsigned wbv, unsigned rbr, unsigned br, uint64_t start_soonest = 0)
    : artv(artv)
    , awtv(awtv)
    , wbv(wbv)
    , rbr(rbr)
    , br(br)
    , start_soonest(start_soonest) {}

    explicit timing_params(uint64_t start_soonest)
    : start_soonest(start_soonest) {}

    const unsigned artv{0};
    const unsigned awtv{0};
    const unsigned wbv{0};
    const unsigned rbr{0};
    const unsigned br{0};
    const uint64_t start_soonest{0};
};

} // namespace atp
