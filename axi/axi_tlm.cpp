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

namespace axi {

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

namespace {
    const std::array<std::array<bool, 4>, 16> rd_valid{{
            {true, true, true, true},       // ReadNoSnoop/ReadOnce
            {false, true, true, false},     // ReadShared
            {false, true, true, false},     // ReadClean
            {false, true, true, false},     // ReadNotSharedDirty
            {false, true, true, false},     // ReadOnceCleanInvalid (ACE5)
            {false, true, true, false},     // ReadOnceMakeInvalid (ACE5)
            {false, false, false, false},   //
            {false, true, true, false},     // ReadUnique
            {true, true, true, false},      // CleanShared
            {true, true, true, false},      // CleanInvalid
            {false, true, true, false},     // CleanSharedPersist (ACE5)
            {false, true, true, false},     // CleanUnique
            {false, true, true, false},     // MakeUnique
            {true, true, true, false},      // MakeInvalid
            {false, true, true, false},     // DVM Complete
            {false, true, true, false}      // DVM Message
    }};
    const std::array<std::array<bool, 4>, 16> wr_valid{{
            {true, true, true, true},       // WriteNoSnoop/WriteUnique
            {false, true, true, false},     // WriteLineUnique
            {true, true, true, false},      // WriteClean
            {true, true, true, false},      // WriteBack
            {false, true, true, false},     // Evict
            {true, true, true, false},      // WriteEvict
            {false, false, false, false},   // CmoOnWrite (ACE5L)
            {false, false, false, false},   //
            {true, true, true, false},      // WriteUniquePtlStash (ACE5L)
            {true, true, true, false},      // CleanInvalid
            {false, true, true, false},     // WriteUniqueFullStash (ACE5L)
            {false, false, false, false},   //
            {true, true, true, false},      // StashOnceShared (ACE5L)
            {false, true, true, false},     // StashOnceUnique (ACE5L)
            {false, true, true, false},     // StashTranslation (ACE5L)
            {false, false, false, false}    //
    }};
}
template<>
char const*  is_valid_msg<axi::ace_extension>(axi::ace_extension* ext){
    auto offset = to_int(ext->get_snoop());
    if(offset<32){ // a read access
        if(!rd_valid[offset&0xf][to_int(ext->get_domain())])
            return "illegal read snoop value";
    } else {
        if(!wr_valid[offset&0xf][to_int(ext->get_domain())])
        return "illegal write snoop value";
    }
    if((ext->get_snoop()==snoop_e::READ_NO_SNOOP || ext->get_snoop()==snoop_e::WRITE_NO_SNOOP) &&
            ext->get_domain()!=domain_e::NON_SHAREABLE && ext->get_domain()!=domain_e::SYSTEM){
        return "illegal domain for no snoop access";
    }
    if((ext->get_barrier()==bar_e::MEMORY_BARRIER || ext->get_barrier()==bar_e::SYNCHRONISATION_BARRIER) && (offset&0xf)!=0)
        return "illegal barrier/snoop value combination";
    switch(ext->get_cache()){
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

template<>
char const* is_valid_msg<axi::axi4_extension>(axi::axi4_extension* ext){
    switch(ext->get_cache()){
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

template<>
char const*  is_valid_msg<axi::axi3_extension>(axi::axi3_extension* ext){
    switch(ext->get_cache()){
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
#include <tlm/scc/scv/tlm_recorder.h>
namespace axi {

using namespace tlm::scc::scv;

class axi3_ext_recording : public tlm_extensions_recording_if<axi_protocol_types> {

    void recordBeginTx(scv_tr_handle& handle, axi_protocol_types::tlm_payload_type& trans) override {
        auto ext3 = trans.get_extension<axi3_extension>();
        if(ext3) { // CTRL, DATA, RESP
            handle.record_attribute("trans.axi3.id", ext3->get_id());
            handle.record_attribute("trans.axi3.user[CTRL]", ext3->get_user(common::id_type::CTRL));
            handle.record_attribute("trans.axi3.user[DATA]", ext3->get_user(common::id_type::DATA));
            handle.record_attribute("trans.axi3.user[RESP]", ext3->get_user(common::id_type::RESP));
            handle.record_attribute("trans.axi3.length", ext3->get_length());
            handle.record_attribute("trans.axi3.size", ext3->get_size());
            handle.record_attribute("trans.axi3.burst", std::string(to_char(ext3->get_burst())));
            handle.record_attribute("trans.axi3.prot", ext3->get_prot());
            handle.record_attribute("trans.axi3.exclusive", std::string(ext3->is_exclusive() ? "true" : "false"));
            handle.record_attribute("trans.axi3.cache", ext3->get_cache());
            handle.record_attribute("trans.axi4.cache_bufferable", ext3->is_bufferable());
            handle.record_attribute("trans.axi4.cache_cacheable", ext3->is_cacheable());
            handle.record_attribute("trans.axi4.cache_read_alloc", ext3->is_read_allocate());
            handle.record_attribute("trans.axi4.cache_write_alloc", ext3->is_write_allocate());
            handle.record_attribute("trans.axi3.qos", ext3->get_qos());
            handle.record_attribute("trans.axi3.region", ext3->get_region());
        }
    }

    void recordEndTx(scv_tr_handle& handle, axi_protocol_types::tlm_payload_type& trans) override {
        auto ext3 = trans.get_extension<axi3_extension>();
        if(ext3) {
            handle.record_attribute("trans.axi3.resp", std::string(to_char(ext3->get_resp())));
        }
    }
};
class axi4_ext_recording : public tlm_extensions_recording_if<axi_protocol_types> {

    void recordBeginTx(scv_tr_handle& handle, axi_protocol_types::tlm_payload_type& trans) override {
        auto ext4 = trans.get_extension<axi4_extension>();
        if(ext4) {
            handle.record_attribute("trans.axi4.id", ext4->get_id());
            handle.record_attribute("trans.axi4.user[CTRL]", ext4->get_user(common::id_type::CTRL));
            handle.record_attribute("trans.axi4.user[DATA]", ext4->get_user(common::id_type::DATA));
            handle.record_attribute("trans.axi4.user[RESP]", ext4->get_user(common::id_type::RESP));
            handle.record_attribute("trans.axi4.length", ext4->get_length());
            handle.record_attribute("trans.axi4.size", ext4->get_size());
            handle.record_attribute("trans.axi4.burst", std::string(to_char(ext4->get_burst())));
            handle.record_attribute("trans.axi4.prot", ext4->get_prot());
            handle.record_attribute("trans.axi4.exclusive", std::string(ext4->is_exclusive() ? "true" : "false"));
            handle.record_attribute("trans.axi4.cache", ext4->get_cache());
            handle.record_attribute("trans.axi4.cache_bufferable", ext4->is_bufferable());
            handle.record_attribute("trans.axi4.cache_modifiable", ext4->is_modifiable());
            handle.record_attribute("trans.axi4.cache_write_other_alloc", ext4->is_write_other_allocate());
            handle.record_attribute("trans.axi4.cache_read_other_alloc", ext4->is_read_other_allocate());
            handle.record_attribute("trans.axi4.qos", ext4->get_qos());
            handle.record_attribute("trans.axi4.region", ext4->get_region());
        }
    }

    void recordEndTx(scv_tr_handle& handle, axi_protocol_types::tlm_payload_type& trans) override {
        auto ext4 = trans.get_extension<axi4_extension>();
        if(ext4) {
            handle.record_attribute("trans.axi4.resp", std::string(to_char(ext4->get_resp())));
        }
    }
};
class ace_ext_recording : public tlm_extensions_recording_if<axi_protocol_types> {

    void recordBeginTx(scv_tr_handle& handle, axi_protocol_types::tlm_payload_type& trans) override {
        auto ext4 = trans.get_extension<ace_extension>();
        if(ext4) {
            handle.record_attribute("trans.ace.id", ext4->get_id());
            handle.record_attribute("trans.ace.user[CTRL]", ext4->get_user(common::id_type::CTRL));
            handle.record_attribute("trans.ace.user[DATA]", ext4->get_user(common::id_type::DATA));
            handle.record_attribute("trans.ace.length", ext4->get_length());
            handle.record_attribute("trans.ace.size", ext4->get_size());
            handle.record_attribute("trans.ace.burst", std::string(to_char(ext4->get_burst())));
            handle.record_attribute("trans.ace.prot", ext4->get_prot());
            handle.record_attribute("trans.ace.exclusive", std::string(ext4->is_exclusive() ? "true" : "false"));
            handle.record_attribute("trans.ace.cache", ext4->get_cache());
            handle.record_attribute("trans.ace.cache_bufferable", ext4->is_bufferable());
            handle.record_attribute("trans.ace.cache_modifiable", ext4->is_modifiable());
            handle.record_attribute("trans.ace.cache_write_other_alloc", ext4->is_write_other_allocate());
            handle.record_attribute("trans.ace.cache_read_other_alloc", ext4->is_read_other_allocate());
            handle.record_attribute("trans.ace.qos", ext4->get_qos());
            handle.record_attribute("trans.ace.region", ext4->get_region());
            handle.record_attribute("trans.ace.domain", std::string(to_char(ext4->get_domain())));
            handle.record_attribute("trans.ace.snoop", std::string(to_char(ext4->get_snoop())));
            handle.record_attribute("trans.ace.barrier", std::string(to_char(ext4->get_barrier())));
            handle.record_attribute("trans.ace.unique", ext4->get_unique());
        }
    }

    void recordEndTx(scv_tr_handle& handle, axi_protocol_types::tlm_payload_type& trans) override {
        auto ext4 = trans.get_extension<ace_extension>();
        if(ext4) {
            handle.record_attribute("trans.ace.resp", std::string(to_char(ext4->get_resp())));
            handle.record_attribute("trans.ace.cresp_PassDirty", ext4->is_pass_dirty());
            handle.record_attribute("trans.ace.cresp_IsShared", ext4->is_shared());
            handle.record_attribute("trans.ace.cresp_SnoopDataTransfer", ext4->is_snoop_data_transfer());
            handle.record_attribute("trans.ace.cresp_SnoopError", ext4->is_snoop_error());
            handle.record_attribute("trans.ace.cresp_SnoopWasUnique", ext4->is_snoop_was_unique());
        }
    }
};
namespace scv {
using namespace tlm::scc::scv;
__attribute__((constructor)) bool register_extensions() {
    axi::axi3_extension ext3; // NOLINT
    tlm_extension_recording_registry<axi::axi_protocol_types>::inst().register_ext_rec(ext3.ID, new axi::axi3_ext_recording()); // NOLINT
    axi::axi4_extension ext4; // NOLINT
    tlm_extension_recording_registry<axi::axi_protocol_types>::inst().register_ext_rec(ext4.ID, new axi::axi4_ext_recording()); // NOLINT
    axi::ace_extension extace; // NOLINT
    tlm_extension_recording_registry<axi::axi_protocol_types>::inst().register_ext_rec(extace.ID, new axi::ace_ext_recording()); // NOLINT
    return true; // NOLINT
}
bool registered = register_extensions();
} // namespace scv
} // namespace axi
