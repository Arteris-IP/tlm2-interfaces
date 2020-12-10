/*******************************************************************************
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
 *******************************************************************************/

#include "axi_tlm.h"

namespace axi {

std::string to_read_string(snoop_e snoop) {
    switch(snoop) {
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
    default:
        return "reserved";
    };
}

std::string to_write_string(snoop_e snoop) {
    switch(snoop) {
    case snoop_e::WRITE_UNIQUE:
        return "WRITE_UNIQUE"; //= 0
    case snoop_e::WRITE_LINE_UNIQUE:
        return "WRITE_LINE_UNIQUE"; // = 1,
    case snoop_e::WRITE_CLEAN:
        return "WRITE_CLEAN"; // = 2,
    case snoop_e::WRITE_BACK:
        return "WRITE_BACK"; // = 0x3,
    case snoop_e::EVICT:
        return "EVICT"; // = 0x4,
    case snoop_e::WRITE_EVICT:
        return "WRITE_EVICT"; // = 0x5,
    case snoop_e::WRITE_UNIQUE_PTL_STASH:
        return "WRITE_UNIQUE_PTL_STASH"; // = 0x8,
    case snoop_e::WRITE_UNIQUE_FULL_STASH:
        return "WRITE_UNIQUE_FULL_STASH"; // = 0x9,
    case snoop_e::STASH_ONCE_SHARED:
        return "STASH_ONCE_SHARED"; // = 0xc,
    case snoop_e::STASH_ONCE_UNIQUE:
        return "STASH_ONCE_UNIQUE"; // = 0xd,
    case snoop_e::STASH_TRANSLATION:
        return "STASH_TRANSLATION"; // = 0xe,
    default:
        return "reserved";
    };
}

template <> const char* to_char<snoop_e>(snoop_e v) {
    switch(to_int(v)) {
    case 0:
        return "READ_ONCE/WRITE_UNIQUE";
    case 1:
        return "READ_SHARED/WRITE_LINE_UNIQUE";
    case 2:
        return "READ_CLEAN/WRITE_CLEAN";
    case 3:
        return "READ_NOT_SHARED_DIRTY/WRITE_BACK";
    case 4:
        return "EVICT";
    case 5:
        return "WRITE_EVICT";
    case 6:
        return "READ_ONCE/CMO_ON_WRITE";
    case 7:
        return "READ_UNIQUE";
    case 8:
        return "CLEAN_SHARED/WRITE_UNIQUE_PTL_STASH";
    case 9:
        return "CLEAN_INVALID/WRITE_UNIQUE_FULL_STASH";
    case 11:
        return "CLEAN_UNIQUE";
    case 12:
        return "MAKE_UNIQUE/STASH_ONCE_SHARED";
    case 13:
        return "MAKE_INVALID/STASH_ONCE_UNIQUE";
    case 14:
        return "DVM_COMPLETE/STASH_TRANSLATION";
    case 15:
        return "DVM_MESSAGE";
    case 0xc0:
        return "STASH_ONCE_SHARED";
    default:
        return "reserved";
    }
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
        return "UNKNOWN";
    }
}
} // namespace axi
#ifdef WITH_SCV
#include <scv4tlm/tlm_recorder.h>
namespace axi {

using namespace scv4tlm;

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
            handle.record_attribute("trans.axi4.cache.bufferable", ext3->is_bufferable());
            handle.record_attribute("trans.axi4.cache.cacheable", ext3->is_cacheable());
            handle.record_attribute("trans.axi4.cache.read_alloc", ext3->is_read_allocate());
            handle.record_attribute("trans.axi4.cache.write_alloc", ext3->is_write_allocate());
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
            handle.record_attribute("trans.axi4.cache.bufferable", ext4->is_bufferable());
            handle.record_attribute("trans.axi4.cache.modifiable", ext4->is_modifiable());
            handle.record_attribute("trans.axi4.cache.write_other_alloc", ext4->is_write_other_allocate());
            handle.record_attribute("trans.axi4.cache.read_other_alloc", ext4->is_read_other_allocate());
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
            handle.record_attribute("trans.ace.cache.bufferable", ext4->is_bufferable());
            handle.record_attribute("trans.ace.cache.modifiable", ext4->is_modifiable());
            handle.record_attribute("trans.ace.cache.write_other_alloc", ext4->is_write_other_allocate());
            handle.record_attribute("trans.ace.cache.read_other_alloc", ext4->is_read_other_allocate());
            handle.record_attribute("trans.ace.qos", ext4->get_qos());
            handle.record_attribute("trans.ace.region", ext4->get_region());
            handle.record_attribute("trans.ace.domain", std::string(to_char(ext4->get_domain())));
            handle.record_attribute("trans.ace.snoop",
                                    trans.is_write() ? to_write_string(ext4->get_snoop()) : to_read_string(ext4->get_snoop()));
            handle.record_attribute("trans.ace.barrier", std::string(to_char(ext4->get_barrier())));
            handle.record_attribute("trans.ace.unique", ext4->get_unique());
        }
    }

    void recordEndTx(scv_tr_handle& handle, axi_protocol_types::tlm_payload_type& trans) override {
        auto ext4 = trans.get_extension<ace_extension>();
        if(ext4) {
            handle.record_attribute("trans.ace.resp", std::string(to_char(ext4->get_resp())));
            handle.record_attribute("trans.ace.resp.PassDirty", ext4->is_pass_dirty());
            handle.record_attribute("trans.ace.resp.IsShared", ext4->is_shared());
            handle.record_attribute("trans.ace.resp.snoopDataTransfer", ext4->is_snoop_data_transfer());
            handle.record_attribute("trans.ace.resp.snoopError", ext4->is_snoop_error());
            handle.record_attribute("trans.ace.resp.snoopWasUnique", ext4->is_snoop_was_unique());
        }
    }
};
} // namespace axi
namespace scv4axi {
__attribute__((constructor)) bool register_extensions() {
    axi::axi3_extension ext3; // NOLINT
    scv4tlm::tlm_extension_recording_registry<axi::axi_protocol_types>::inst().register_ext_rec(ext3.ID, new axi::axi3_ext_recording()); // NOLINT
    axi::axi4_extension ext4; // NOLINT
    scv4tlm::tlm_extension_recording_registry<axi::axi_protocol_types>::inst().register_ext_rec(ext4.ID, new axi::axi4_ext_recording()); // NOLINT
    axi::ace_extension extace; // NOLINT
    scv4tlm::tlm_extension_recording_registry<axi::axi_protocol_types>::inst().register_ext_rec(extace.ID, new axi::ace_ext_recording()); // NOLINT
    return true; // NOLINT
}
bool registered = register_extensions();
} // namespace scv4axi
#endif
