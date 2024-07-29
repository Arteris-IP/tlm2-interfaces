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

namespace axi {
namespace scv {

class tlm_id_ext_recording : public tlm::scc::scv::tlm_extensions_recording_if<axi_protocol_types> {

    void recordBeginTx(SCVNS scv_tr_handle& handle, axi_protocol_types::tlm_payload_type& trans) override {
        if(auto ext = trans.get_extension<tlm::scc::tlm_id_extension>()) {
            handle.record_attribute("trans.uid", ext->id);
        }
    }

    void recordEndTx(SCVNS scv_tr_handle& handle, tlm::tlm_base_protocol_types::tlm_payload_type& trans) override {
    }
};

class axi3_ext_recording : public tlm::scc::scv::tlm_extensions_recording_if<axi_protocol_types> {

    void recordBeginTx(SCVNS scv_tr_handle& handle, axi_protocol_types::tlm_payload_type& trans) override {
        if(auto ext3 = trans.get_extension<axi3_extension>()) { // CTRL, DATA, RESP
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

    void recordEndTx(SCVNS scv_tr_handle& handle, axi_protocol_types::tlm_payload_type& trans) override {
        if(auto ext3 = trans.get_extension<axi3_extension>()) {
            handle.record_attribute("trans.axi3.resp", std::string(to_char(ext3->get_resp())));
        }
    }
};
class axi4_ext_recording : public tlm::scc::scv::tlm_extensions_recording_if<axi_protocol_types> {

    void recordBeginTx(SCVNS scv_tr_handle& handle, axi_protocol_types::tlm_payload_type& trans) override {
        if(auto ext4 = trans.get_extension<axi4_extension>()) {
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
            handle.record_attribute("trans.axi4.cache_allocate", ext4->is_allocate());
            handle.record_attribute("trans.axi4.cache_other_alloc", ext4->is_other_allocate());
            handle.record_attribute("trans.axi4.qos", ext4->get_qos());
            handle.record_attribute("trans.axi4.region", ext4->get_region());
        }
    }

    void recordEndTx(SCVNS scv_tr_handle& handle, axi_protocol_types::tlm_payload_type& trans) override {
        if(auto ext4 = trans.get_extension<axi4_extension>()) {
            handle.record_attribute("trans.axi4.resp", std::string(to_char(ext4->get_resp())));
        }
    }
};
class ace_ext_recording : public tlm::scc::scv::tlm_extensions_recording_if<axi_protocol_types> {

    void recordBeginTx(SCVNS scv_tr_handle& handle, axi_protocol_types::tlm_payload_type& trans) override {
        if(auto ext4 = trans.get_extension<ace_extension>()) {
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
            handle.record_attribute("trans.ace.cache_allocate", ext4->is_allocate());
            handle.record_attribute("trans.ace.cache_other_alloc", ext4->is_other_allocate());
            handle.record_attribute("trans.ace.qos", ext4->get_qos());
            handle.record_attribute("trans.ace.region", ext4->get_region());
            handle.record_attribute("trans.ace.domain", std::string(to_char(ext4->get_domain())));
            handle.record_attribute("trans.ace.snoop", std::string(to_char(ext4->get_snoop())));
            handle.record_attribute("trans.ace.barrier", std::string(to_char(ext4->get_barrier())));
            handle.record_attribute("trans.ace.unique", ext4->get_unique());
        }
    }

    void recordEndTx(SCVNS scv_tr_handle& handle, axi_protocol_types::tlm_payload_type& trans) override {
        if(auto ext4 = trans.get_extension<ace_extension>()) {
            handle.record_attribute("trans.ace.resp", std::string(to_char(ext4->get_resp())));
            handle.record_attribute("trans.ace.cresp_PassDirty", ext4->is_pass_dirty());
            handle.record_attribute("trans.ace.cresp_IsShared", ext4->is_shared());
            handle.record_attribute("trans.ace.cresp_SnoopDataTransfer", ext4->is_snoop_data_transfer());
            handle.record_attribute("trans.ace.cresp_SnoopError", ext4->is_snoop_error());
            handle.record_attribute("trans.ace.cresp_SnoopWasUnique", ext4->is_snoop_was_unique());
        }
    }
};

#if defined(__GNUG__)
__attribute__((constructor))
#endif
bool register_extensions() {
    tlm::scc::tlm_id_extension ext(nullptr); // NOLINT
    tlm::scc::scv::tlm_extension_recording_registry<axi::axi_protocol_types>::inst().register_ext_rec(
        ext.ID, new tlm_id_ext_recording()); // NOLINT
    axi::axi3_extension ext3; // NOLINT
    tlm::scc::scv::tlm_extension_recording_registry<axi::axi_protocol_types>::inst().register_ext_rec(
        ext3.ID, new axi::scv::axi3_ext_recording()); // NOLINT
    axi::axi4_extension ext4;                    // NOLINT
    tlm::scc::scv::tlm_extension_recording_registry<axi::axi_protocol_types>::inst().register_ext_rec(
        ext4.ID, new axi::scv::axi4_ext_recording()); // NOLINT
    axi::ace_extension extace;                   // NOLINT
    tlm::scc::scv::tlm_extension_recording_registry<axi::axi_protocol_types>::inst().register_ext_rec(
        extace.ID, new axi::scv::ace_ext_recording()); // NOLINT
    return true;                                  // NOLINT
}
bool registered = register_extensions();
} // namespace scv
} // namespace axi
