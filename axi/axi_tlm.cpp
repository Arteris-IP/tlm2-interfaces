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
 * limitations under the License.
 */

#include "axi_tlm.h"
#include <tlm/scc/scv/tlm_extension_recording_registry.h>

namespace axi {

namespace {
std::array<std::string, 3> cmd_str{"R", "W", "I"};
}

template <> const char* to_char<snoop_e>(snoop_e v) {
    switch(v) {
    case snoop_e::READ_NO_SNOOP:
        return "READ_NO_SNOOP"; //= 0x10
    case snoop_e::READ_ONCE:
        return "READ_ONCE"; //= 0
    case snoop_e::READ_SHARED:
        return "READ_SHARED"; // = 1,
    case snoop_e::READ_CLEAN:
        return "READ_CLEAN"; // = 2,
    case snoop_e::READ_NOT_SHARED_DIRTY:
        return "READ_NOT_SHARED_DIRTY"; // = 0x3,
    case snoop_e::READ_UNIQUE:
        return "READ_UNIQUE"; // = 0x7,
    case snoop_e::CLEAN_SHARED:
        return "CLEAN_SHARED"; // = 0x8,
    case snoop_e::CLEAN_INVALID:
        return "CLEAN_INVALID"; // = 0x9,
    case snoop_e::CLEAN_SHARED_PERSIST:
        return "CLEAN_SHARED_PERSIST"; // = 0xa,
    case snoop_e::CLEAN_UNIQUE:
        return "CLEAN_UNIQUE"; // = 0xb,
    case snoop_e::MAKE_UNIQUE:
        return "MAKE_UNIQUE"; // = 0xc,
    case snoop_e::MAKE_INVALID:
        return "MAKE_INVALID"; // = 0xd,
    case snoop_e::DVM_COMPLETE:
        return "DVM_COMPLETE"; // = 0xe,
    case snoop_e::DVM_MESSAGE:
        return "DVM_MESSAGE"; // = 0xf
    case snoop_e::BARRIER:
        return "BARRIER"; // = 0x40,
    case snoop_e::WRITE_NO_SNOOP:
        return "WRITE_NO_SNOOP"; //= 0x30
    case snoop_e::WRITE_UNIQUE:
        return "WRITE_UNIQUE"; //= 0x20
    case snoop_e::WRITE_LINE_UNIQUE:
        return "WRITE_LINE_UNIQUE"; // = 0x21,
    case snoop_e::WRITE_CLEAN:
        return "WRITE_CLEAN"; // = 0x22,
    case snoop_e::WRITE_BACK:
        return "WRITE_BACK"; // = 0x23,
    case snoop_e::EVICT:
        return "EVICT"; // = 0x24,
    case snoop_e::WRITE_EVICT:
        return "WRITE_EVICT"; // = 0x25,
    case snoop_e::CMO_ON_WRITE:
        return "CMO_ON_WRITE"; // = 0x26,
    case snoop_e::WRITE_UNIQUE_PTL_STASH:
        return "WRITE_UNIQUE_PTL_STASH"; // = 0x28,
    case snoop_e::WRITE_UNIQUE_FULL_STASH:
        return "WRITE_UNIQUE_FULL_STASH"; // = 0x29,
    case snoop_e::STASH_ONCE_SHARED:
        return "STASH_ONCE_SHARED"; // = 0x2c,
    case snoop_e::STASH_ONCE_UNIQUE:
        return "STASH_ONCE_UNIQUE"; // = 0x2d,
    case snoop_e::STASH_TRANSLATION:
        return "STASH_TRANSLATION"; // = 0x2e,
    default:
        return "reserved";
    };
}

template <> const char* to_char<burst_e>(burst_e v) {
    switch(v) {
    case burst_e::FIXED:
        return "FIXED";
    case burst_e::INCR:
        return "INCR";
    case burst_e::WRAP:
        return "WRAP";
    default:
        return "UNKNOWN";
    }
}

template <> const char* to_char<lock_e>(lock_e v) {
    switch(v) {
    case lock_e::NORMAL:
        return "NORMAL";
    case lock_e::EXLUSIVE:
        return "EXLUSIVE";
    case lock_e::LOCKED:
        return "LOCKED";
    default:
        return "UNKNOWN";
    }
}

template <> const char* to_char<domain_e>(domain_e v) {
    switch(v) {
    case domain_e::NON_SHAREABLE:
        return "NON_SHAREABLE";
    case domain_e::INNER_SHAREABLE:
        return "INNER_SHAREABLE";
    case domain_e::OUTER_SHAREABLE:
        return "OUTER_SHAREABLE";
    case domain_e::SYSTEM:
        return "SYSTEM";
    default:
        return "UNKNOWN";
    }
}

template <> const char* to_char<bar_e>(bar_e v) {
    switch(v) {
    case bar_e::RESPECT_BARRIER:
        return "RESPECT_BARRIER";
    case bar_e::MEMORY_BARRIER:
        return "MEMORY_BARRIER";
    case bar_e::IGNORE_BARRIER:
        return "IGNORE_BARRIER";
    case bar_e::SYNCHRONISATION_BARRIER:
        return "SYNCHRONISATION_BARRIER";
    default:
        return "UNKNOWN";
    }
}

template <> const char* to_char<resp_e>(resp_e v) {
    switch(v) {
    case resp_e::OKAY:
        return "OKAY";
    case resp_e::EXOKAY:
        return "EXOKAY";
    case resp_e::SLVERR:
        return "SLVERR";
    case resp_e::DECERR:
        return "DECERR";
    default:
        return "OTHER";
    }
}

std::ostream& operator<<(std::ostream& os, const tlm::tlm_generic_payload& t) {
    char const* ch =
        t.get_command() == tlm::TLM_READ_COMMAND ? "AR" : (t.get_command() == tlm::TLM_WRITE_COMMAND ? "AW" : "");
    os << "CMD:" << cmd_str[t.get_command()] << ", " << ch << "ADDR:0x" << std::hex << t.get_address() << ", TXLEN:0x"
       << t.get_data_length();
    if(auto e = t.get_extension<axi::ace_extension>()) {
        os << ", " << ch << "ID:" << e->get_id() << ", " << ch << "LEN:0x" << std::hex
           << static_cast<unsigned>(e->get_length()) << ", " << ch << "SIZE:0x" << static_cast<unsigned>(e->get_size())
           << std::dec << ", " << ch << "BURST:" << to_char(e->get_burst()) << ", " << ch
           << "PROT:" << static_cast<unsigned>(e->get_prot()) << ", " << ch
           << "CACHE:" << static_cast<unsigned>(e->get_cache()) << ", " << ch
           << "QOS:" << static_cast<unsigned>(e->get_qos()) << ", " << ch
           << "REGION:" << static_cast<unsigned>(e->get_region()) << ", " << ch << "SNOOP:0x" << std::hex
           << (static_cast<unsigned>(e->get_snoop()) & 0xf) << std::dec << ", " << ch
           << "DOMAIN:" << to_char(e->get_domain()) << ", " << ch << "BAR:" << to_char(e->get_barrier()) << ", " << ch
           << "UNIQUE:" << e->get_unique();
    } else if(auto e = t.get_extension<axi::axi4_extension>()) {
        os << ", " << ch << "ID:" << e->get_id() << ", " << ch << "LEN:0x" << std::hex
           << static_cast<unsigned>(e->get_length()) << ", " << ch << "SIZE:0x" << static_cast<unsigned>(e->get_size())
           << std::dec << ", " << ch << "BURST:" << to_char(e->get_burst()) << ", " << ch
           << "PROT:" << static_cast<unsigned>(e->get_prot()) << ", " << ch
           << "CACHE:" << static_cast<unsigned>(e->get_cache()) << ", " << ch
           << "QOS:" << static_cast<unsigned>(e->get_qos()) << ", " << ch
           << "REGION:" << static_cast<unsigned>(e->get_region());
    } else if(auto e = t.get_extension<axi::axi3_extension>()) {
        os << ", " << ch << "ID:" << e->get_id() << ", " << ch << "LEN:0x" << std::hex
           << static_cast<unsigned>(e->get_length()) << ", " << ch << "SIZE:0x" << static_cast<unsigned>(e->get_size())
           << std::dec << ", " << ch << "BURST:" << to_char(e->get_burst()) << ", " << ch
           << "PROT:" << static_cast<unsigned>(e->get_prot()) << ", " << ch
           << "CACHE:" << static_cast<unsigned>(e->get_cache()) << ", " << ch
           << "QOS:" << static_cast<unsigned>(e->get_qos()) << ", " << ch
           << "REGION:" << static_cast<unsigned>(e->get_region());
    }
    os << " [ptr:" << &t << "]";
    return os;
}

namespace {
const std::array<std::array<bool, 4>, 16> rd_valid{{
    {true, true, true, true},     // ReadNoSnoop/ReadOnce
    {false, true, true, false},   // ReadShared
    {false, true, true, false},   // ReadClean
    {false, true, true, false},   // ReadNotSharedDirty
    {false, true, true, false},   // ReadOnceCleanInvalid (ACE5)
    {false, true, true, false},   // ReadOnceMakeInvalid (ACE5)
    {false, false, false, false}, //
    {false, true, true, false},   // ReadUnique
    {true, true, true, false},    // CleanShared
    {true, true, true, false},    // CleanInvalid
    {false, true, true, false},   // CleanSharedPersist (ACE5)
    {false, true, true, false},   // CleanUnique
    {false, true, true, false},   // MakeUnique
    {true, true, true, false},    // MakeInvalid
    {false, true, true, false},   // DVM Complete
    {false, true, true, false}    // DVM Message
}};
const std::array<std::array<bool, 4>, 16> wr_valid{{
    {true, true, true, true},     // WriteNoSnoop/WriteUnique
    {false, true, true, false},   // WriteLineUnique
    {true, true, true, false},    // WriteClean
    {true, true, true, false},    // WriteBack
    {false, true, true, false},   // Evict
    {true, true, true, false},    // WriteEvict
    {false, false, false, false}, // CmoOnWrite (ACE5L)
    {false, false, false, false}, //
    {true, true, true, false},    // WriteUniquePtlStash (ACE5L)
    {true, true, true, false},    // CleanInvalid
    {false, true, true, false},   // WriteUniqueFullStash (ACE5L)
    {false, false, false, false}, //
    {true, true, true, false},    // StashOnceShared (ACE5L)
    {false, true, true, false},   // StashOnceUnique (ACE5L)
    {false, true, true, false},   // StashTranslation (ACE5L)
    {false, false, false, false}  //
}};
} // namespace
template <> char const* is_valid_msg<axi::ace_extension>(axi::ace_extension* ext) {
    auto offset = to_int(ext->get_snoop());
    if(offset < 32) { // a read access
        if(!rd_valid[offset & 0xf][to_int(ext->get_domain())])
            return "illegal read snoop value";
    } else {
        if(!wr_valid[offset & 0xf][to_int(ext->get_domain())])
            return "illegal write snoop value";
    }
    if(ext->get_cache()<2 && ext->get_domain() != axi::domain_e::SYSTEM)
        return "illegal combination of AxCACHE and SYSTEM domain";
    if(ext->get_cache()>3 && ext->get_domain() == axi::domain_e::SYSTEM)
        return "illegal combination of AxCACHE and SYSTEM domain";
    // check table ED3-7 and D3-8 of IHI0022H
    switch(ext->get_snoop()) {
    case snoop_e::READ_NO_SNOOP:
    case snoop_e::WRITE_NO_SNOOP: // non coherent access to coherent domain
        if(ext->get_domain() != domain_e::NON_SHAREABLE && ext->get_domain() != domain_e::SYSTEM) {
            return "illegal domain for no non-coherent access";
        }
        break;
    case snoop_e::READ_ONCE:
    case snoop_e::READ_SHARED:
    case snoop_e::READ_CLEAN:
    case snoop_e::READ_NOT_SHARED_DIRTY:
    case snoop_e::READ_UNIQUE:
    case snoop_e::MAKE_UNIQUE:
    case snoop_e::DVM_COMPLETE:
    case snoop_e::DVM_MESSAGE:
    case snoop_e::WRITE_UNIQUE:
    case snoop_e::WRITE_LINE_UNIQUE:
    case snoop_e::EVICT:
        if(ext->get_domain() != domain_e::INNER_SHAREABLE && ext->get_domain() != domain_e::OUTER_SHAREABLE) {
            return "illegal domain for coherent access";
        }
        break;
    case snoop_e::CLEAN_SHARED:
    case snoop_e::CLEAN_INVALID:
    case snoop_e::MAKE_INVALID:
    case snoop_e::WRITE_CLEAN:
    case snoop_e::WRITE_BACK:
    case snoop_e::WRITE_EVICT:
        if(ext->get_domain() == domain_e::SYSTEM) {
            return "illegal domain for coherent access";
        }
        break;
    default:
        break;
    }
    if((ext->get_barrier() == bar_e::MEMORY_BARRIER || ext->get_barrier() == bar_e::SYNCHRONISATION_BARRIER) &&
       (offset & 0xf) != 0)
        return "illegal barrier/snoop value combination";
    switch(ext->get_cache()) {
    case 4:
    case 5:
    case 8:
    case 9:
    case 12:
    case 13:
        return "illegal cache value";
    default:
        break;
    }
    if((ext->get_cache() & 2) == 0 && ext->get_domain() != domain_e::NON_SHAREABLE &&
       ext->get_domain() != domain_e::SYSTEM)
        return "illegal domain for no non-cachable access";
    return nullptr;
}

template <> char const* is_valid_msg<axi::axi4_extension>(axi::axi4_extension* ext) {
    switch(ext->get_cache()) {
    case 4:
    case 5:
    case 8:
    case 9:
    case 12:
    case 13:
        return "illegal cache value";
    }
    return nullptr;
}

template <> char const* is_valid_msg<axi::axi3_extension>(axi::axi3_extension* ext) {
    switch(ext->get_cache()) {
    case 4:
    case 5:
    case 8:
    case 9:
    case 12:
    case 13:
        return "illegal cache value";
    }
    return nullptr;
}
} // namespace axi
