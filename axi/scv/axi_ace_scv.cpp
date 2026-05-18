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

#include <axi/axi_tlm.h>
#include <tlm/scc/scv/tlm_extension_recording_registry.h>
#include <tlm/scc/scv/tlm_recorder.h>
#include <tlm/scc/tlm_id.h>
#include <fmt/format.h>

namespace axi {
namespace scv {

using namespace tlm::scc::scv;

struct tlm_id_ext_recording : public tlm_extensions_recording_if<axi::axi_protocol_types> {

    tlm_id_ext_recording() { recordBegin = &recordBeginTx; }

    static void recordBeginTx(SCVNS scv_tr_handle& handle, axi_protocol_types::tlm_payload_type& trans) {
        tlm_extension_record_registry::get().recordBeginTx(tlm::scc::tlm_id_extension::ID, handle,
                                                           trans.get_extension<tlm::scc::tlm_id_extension>(), "trans.");
    }
};

struct axi3_ext_record : public tlm_extension_record_if {

    axi3_ext_record() {
        recordBegin = &recordBeginTx;
        recordEnd = recordEndTx;
    }

    static void recordBeginTx(SCVNS scv_tr_handle& handle, tlm::tlm_extension_base* e, std::string const& prefix) {
        if(auto ext = dynamic_cast<axi3_extension*>(e)) {
            handle.record_attribute(fmt::format("{}id", prefix).c_str(), ext->get_id());
            handle.record_attribute(fmt::format("{}user[CTRL]", prefix).c_str(), ext->get_user(common::id_type::CTRL));
            handle.record_attribute(fmt::format("{}user[DATA]", prefix).c_str(), ext->get_user(common::id_type::DATA));
            handle.record_attribute(fmt::format("{}user[RESP]", prefix).c_str(), ext->get_user(common::id_type::RESP));
            handle.record_attribute(fmt::format("{}length", prefix).c_str(), ext->get_length());
            handle.record_attribute(fmt::format("{}size", prefix).c_str(), ext->get_size());
            handle.record_attribute(fmt::format("{}burst", prefix).c_str(), std::string(to_char(ext->get_burst())));
            handle.record_attribute(fmt::format("{}prot", prefix).c_str(), ext->get_prot());
            handle.record_attribute(fmt::format("{}exclusive", prefix).c_str(), std::string(ext->is_exclusive() ? "true" : "false"));
            handle.record_attribute(fmt::format("{}cache", prefix).c_str(), ext->get_cache());
            handle.record_attribute(fmt::format("{}cache_bufferable", prefix).c_str(), ext->is_bufferable());
            handle.record_attribute(fmt::format("{}cache_cacheable", prefix).c_str(), ext->is_cacheable());
            handle.record_attribute(fmt::format("{}cache_read_alloc", prefix).c_str(), ext->is_read_allocate());
            handle.record_attribute(fmt::format("{}cache_write_alloc", prefix).c_str(), ext->is_write_allocate());
            handle.record_attribute(fmt::format("{}qos", prefix).c_str(), ext->get_qos());
            handle.record_attribute(fmt::format("{}region", prefix).c_str(), ext->get_region());
        }
    }
    static void recordEndTx(SCVNS scv_tr_handle& handle, tlm::tlm_extension_base* e, std::string const& prefix) {
        if(auto ext = dynamic_cast<axi3_extension*>(e)) {
            handle.record_attribute(fmt::format("{}resp", prefix).c_str(), std::string(to_char(ext->get_resp())));
        }
    }
};

struct axi3_ext_recording : public tlm_extensions_recording_if<axi_protocol_types> {

    axi3_ext_recording() {
        recordBegin = &recordBeginTx;
        recordEnd = &recordEndTx;
    }

    static void recordBeginTx(SCVNS scv_tr_handle& handle, axi_protocol_types::tlm_payload_type& trans) {
        tlm_extension_record_registry::get().recordBeginTx(axi3_extension::ID, handle, trans.get_extension<axi3_extension>(), "trans.axi3.");
    }

    static void recordEndTx(SCVNS scv_tr_handle& handle, axi_protocol_types::tlm_payload_type& trans) {
        tlm_extension_record_registry::get().recordEndTx(axi3_extension::ID, handle, trans.get_extension<axi3_extension>(), "trans.axi3.");
    }
};

struct axi4_ext_record : public tlm_extension_record_if {

    axi4_ext_record() {
        recordBegin = &recordBeginTx;
        recordEnd = &recordEndTx;
    }

    static void recordBeginTx(SCVNS scv_tr_handle& handle, tlm::tlm_extension_base* e, std::string const& prefix) {
        if(auto ext = dynamic_cast<axi4_extension*>(e)) {
            handle.record_attribute(fmt::format("{}id", prefix).c_str(), ext->get_id());
            handle.record_attribute(fmt::format("{}user[CTRL]", prefix).c_str(), ext->get_user(common::id_type::CTRL));
            handle.record_attribute(fmt::format("{}user[DATA]", prefix).c_str(), ext->get_user(common::id_type::DATA));
            handle.record_attribute(fmt::format("{}user[RESP]", prefix).c_str(), ext->get_user(common::id_type::RESP));
            handle.record_attribute(fmt::format("{}length", prefix).c_str(), ext->get_length());
            handle.record_attribute(fmt::format("{}size", prefix).c_str(), ext->get_size());
            handle.record_attribute(fmt::format("{}burst", prefix).c_str(), std::string(to_char(ext->get_burst())));
            handle.record_attribute(fmt::format("{}prot", prefix).c_str(), ext->get_prot());
            handle.record_attribute(fmt::format("{}exclusive", prefix).c_str(), std::string(ext->is_exclusive() ? "true" : "false"));
            handle.record_attribute(fmt::format("{}cache", prefix).c_str(), ext->get_cache());
            handle.record_attribute(fmt::format("{}cache_bufferable", prefix).c_str(), ext->is_bufferable());
            handle.record_attribute(fmt::format("{}cache_modifiable", prefix).c_str(), ext->is_modifiable());
            handle.record_attribute(fmt::format("{}cache_allocate", prefix).c_str(), ext->is_allocate());
            handle.record_attribute(fmt::format("{}cache_other_alloc", prefix).c_str(), ext->is_other_allocate());
            handle.record_attribute(fmt::format("{}qos", prefix).c_str(), ext->get_qos());
            handle.record_attribute(fmt::format("{}region", prefix).c_str(), ext->get_region());
        }
    }

    static void recordEndTx(SCVNS scv_tr_handle& handle, tlm::tlm_extension_base* e, std::string const& prefix) {
        if(auto ext = dynamic_cast<axi4_extension*>(e)) {
            handle.record_attribute(fmt::format("{}resp", prefix).c_str(), std::string(to_char(ext->get_resp())));
        }
    }
};

struct axi4_ext_recording : public tlm_extensions_recording_if<axi_protocol_types> {

    axi4_ext_recording() {
        recordBegin = &recordBeginTx;
        recordEnd = &recordEndTx;
    }

    static void recordBeginTx(SCVNS scv_tr_handle& handle, axi_protocol_types::tlm_payload_type& trans) {
        tlm_extension_record_registry::get().recordBeginTx(axi4_extension::ID, handle, trans.get_extension<axi4_extension>(), "trans.axi4.");
    }

    static void recordEndTx(SCVNS scv_tr_handle& handle, axi_protocol_types::tlm_payload_type& trans) {
        tlm_extension_record_registry::get().recordEndTx(axi4_extension::ID, handle, trans.get_extension<axi4_extension>(), "trans.axi4.");
    }
};

struct ace_ext_record : public tlm_extension_record_if {

    ace_ext_record() {
        recordBegin = &recordBeginTx;
        recordEnd = &recordEndTx;
    }

    static void recordBeginTx(SCVNS scv_tr_handle& handle, tlm::tlm_extension_base* e, std::string const& prefix) {
        if(auto ext = dynamic_cast<ace_extension*>(e)) {
            handle.record_attribute(fmt::format("{}id", prefix).c_str(), ext->get_id());
            handle.record_attribute(fmt::format("{}user[CTRL]", prefix).c_str(), ext->get_user(common::id_type::CTRL));
            handle.record_attribute(fmt::format("{}user[DATA]", prefix).c_str(), ext->get_user(common::id_type::DATA));
            handle.record_attribute(fmt::format("{}length", prefix).c_str(), ext->get_length());
            handle.record_attribute(fmt::format("{}size", prefix).c_str(), ext->get_size());
            handle.record_attribute(fmt::format("{}burst", prefix).c_str(), std::string(to_char(ext->get_burst())));
            handle.record_attribute(fmt::format("{}prot", prefix).c_str(), ext->get_prot());
            handle.record_attribute(fmt::format("{}exclusive", prefix).c_str(), std::string(ext->is_exclusive() ? "true" : "false"));
            handle.record_attribute(fmt::format("{}cache", prefix).c_str(), ext->get_cache());
            handle.record_attribute(fmt::format("{}cache_bufferable", prefix).c_str(), ext->is_bufferable());
            handle.record_attribute(fmt::format("{}cache_modifiable", prefix).c_str(), ext->is_modifiable());
            handle.record_attribute(fmt::format("{}cache_allocate", prefix).c_str(), ext->is_allocate());
            handle.record_attribute(fmt::format("{}cache_other_alloc", prefix).c_str(), ext->is_other_allocate());
            handle.record_attribute(fmt::format("{}qos", prefix).c_str(), ext->get_qos());
            handle.record_attribute(fmt::format("{}region", prefix).c_str(), ext->get_region());
            handle.record_attribute(fmt::format("{}domain", prefix).c_str(), std::string(to_char(ext->get_domain())));
            handle.record_attribute(fmt::format("{}snoop", prefix).c_str(), std::string(to_char(ext->get_snoop())));
            handle.record_attribute(fmt::format("{}barrier", prefix).c_str(), std::string(to_char(ext->get_barrier())));
            handle.record_attribute(fmt::format("{}unique", prefix).c_str(), ext->get_unique());
        }
    }

    static void recordEndTx(SCVNS scv_tr_handle& handle, tlm::tlm_extension_base* e, std::string const& prefix) {
        if(auto ext = dynamic_cast<ace_extension*>(e)) {
            handle.record_attribute(fmt::format("{}resp", prefix).c_str(), std::string(to_char(ext->get_resp())));
            handle.record_attribute(fmt::format("{}cresp", prefix).c_str(), static_cast<unsigned>(ext->get_cresp()));
            handle.record_attribute(fmt::format("{}cresp_PassDirty", prefix).c_str(), ext->is_pass_dirty());
            handle.record_attribute(fmt::format("{}cresp_IsShared", prefix).c_str(), ext->is_shared());
            handle.record_attribute(fmt::format("{}cresp_SnoopDataTransfer", prefix).c_str(), ext->is_snoop_data_transfer());
            handle.record_attribute(fmt::format("{}cresp_SnoopError", prefix).c_str(), ext->is_snoop_error());
            handle.record_attribute(fmt::format("{}cresp_SnoopWasUnique", prefix).c_str(), ext->is_snoop_was_unique());
        }
    }
};

struct ace_ext_recording : public tlm_extensions_recording_if<axi_protocol_types> {

    ace_ext_recording() {
        recordBegin = &recordBeginTx;
        recordEnd = &recordEndTx;
    }

    static void recordBeginTx(SCVNS scv_tr_handle& handle, axi_protocol_types::tlm_payload_type& trans) {
        tlm_extension_record_registry::get().recordBeginTx(ace_extension::ID, handle, trans.get_extension<ace_extension>(), "trans.ace.");
    }

    static void recordEndTx(SCVNS scv_tr_handle& handle, axi_protocol_types::tlm_payload_type& trans) {
        tlm_extension_record_registry::get().recordEndTx(ace_extension::ID, handle, trans.get_extension<ace_extension>(), "trans.ace.");
    }
};

#if defined(__GNUG__)
__attribute__((constructor))
#endif
bool register_extensions() {
    tlm::scc::tlm_id_extension ext(nullptr);                                                   // NOLINT
    if(!tlm_extension_recording_registry<axi_protocol_types>::get().is_ext_registered(ext.ID)) // NOLINT
        tlm_extension_recording_registry<axi_protocol_types>::get().register_ext_rec(ext.ID,
                                                                                     util::make_unique<tlm_id_ext_recording>()); // NOLINT
    /********************************************************************************************************************/ axi::
    axi3_extension ext3;                                                                    // NOLINT
    if(!tlm_extension_recording_registry<axi_protocol_types>::get().is_ext_registered(ext3.ID)) // NOLINT
        tlm_extension_recording_registry<axi::axi_protocol_types>::get().register_ext_rec(
            ext3.ID,
            util::make_unique<axi::scv::axi3_ext_recording>()); // NOLINT
    if(!tlm_extension_record_registry::get().is_ext_registered(ext3.ID))
        tlm_extension_record_registry::get().register_ext_rec(ext3.ID,
                                                              util::make_unique<axi3_ext_record>()); // NOLINT
    /********************************************************************************************************************/ axi::
    axi4_extension ext4;                                                                    // NOLINT
    if(!tlm_extension_recording_registry<axi_protocol_types>::get().is_ext_registered(ext4.ID)) // NOLINT
        tlm_extension_recording_registry<axi::axi_protocol_types>::get().register_ext_rec(
            ext4.ID,
            util::make_unique<axi::scv::axi4_ext_recording>()); // NOLINT
    if(!tlm_extension_record_registry::get().is_ext_registered(ext4.ID))
        tlm_extension_record_registry::get().register_ext_rec(ext4.ID,
                                                              util::make_unique<axi4_ext_record>()); // NOLINT
    /********************************************************************************************************************/ axi::
    ace_extension extace;                                                                   // NOLINT
    if(!tlm_extension_recording_registry<axi_protocol_types>::get().is_ext_registered(extace.ID)) // NOLINT
        tlm_extension_recording_registry<axi::axi_protocol_types>::get().register_ext_rec(
            extace.ID,
            util::make_unique<axi::scv::ace_ext_recording>()); // NOLINT
    if(!tlm_extension_record_registry::get().is_ext_registered(extace.ID))
        tlm_extension_record_registry::get().register_ext_rec(extace.ID,
                                                              util::make_unique<ace_ext_record>()); // NOLINT
    return true;                                                                                    // NOLINT
}
bool registered = register_extensions();
} // namespace scv
} // namespace axi
