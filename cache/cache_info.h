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

namespace cache {
// cache states supported by CHI.
// invalid/valid(0), shared/unique(1), clean/dirty(2), partial&exclusive(3)
enum class state : uint8_t { IX = 0, SC = 0x1, SD = 0x5, UC = 0x3, UD = 0x7, UCE = 0xE, UDP = 0xf, RES = 0xff };

inline bool isInvalidState(state s) { return s == state::IX; }
inline bool isDirtyState(state s) { return s == state::SD || s == state::UD || s == state::UDP; }
inline bool isUniqueState(state s) { return s == state::UC || s == state::UD || s == state::UDP; }
inline bool isSharedState(state s) { return s == state::SC || s == state::SD; }
inline state setDirtyFrom(state s) { return static_cast<state>(static_cast<uint8_t>(s) | 0x4); }
inline state setCleanFrom(state s) { return static_cast<state>(static_cast<uint8_t>(s) & ~0x4); }
inline state setUniqueFrom(state s) { return static_cast<state>(static_cast<uint8_t>(s) | 0x2); }
inline state setSharedFrom(state s) { return static_cast<state>(static_cast<uint8_t>(s) & ~0x2); }

struct cache_info : public tlm::tlm_extension<cache_info> {

    tlm_extension_base* clone() const override {
        cache_info* e = new cache_info(cache_state);
        return e;
    }
    void copy_from(tlm_extension_base const& from) override {
        cache_state = static_cast<cache_info const&>(from).cache_state;
    }

    cache_info() = delete;
    cache_info(state state_param)
    : cache_state(state_param) {}
    cache_info(state state_param, bool write_back)
    : cache_state(state_param)
    , write_back(write_back) {}
    state get_state() { return cache_state; }
    bool is_write_back() { return write_back; }

private:
    state cache_state;
    bool write_back{false};
};
} // namespace cache
