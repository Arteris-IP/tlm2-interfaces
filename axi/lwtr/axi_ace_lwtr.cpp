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
#include <tlm/scc/tlm_id.h>
#include <tlm/scc/scv/tlm_extension_recording_registry.h>
#include <tlm/scc/lwtr/lwtr4tlm2_extension_registry.h>

namespace lwtr {
template <class Archive> void record(Archive &ar, tlm::scc::tlm_id_extension const& e) {ar & field("uid", e.id);}
template <class Archive> void record(Archive &ar, axi::axi3_extension const& e) {
    ar & field("id", e.get_id());
    ar & field("user[CTRL]", e.get_user(axi::common::id_type::CTRL));
    ar & field("user[DATA]", e.get_user(axi::common::id_type::DATA));
    ar & field("user[RESP]", e.get_user(axi::common::id_type::RESP));
    ar & field("length", e.get_length());
    ar & field("size", e.get_size());
    ar & field("burst", to_char(e.get_burst()));
    ar & field("prot", e.get_prot());
    ar & field("exclusive", e.is_exclusive() ? "true" : "false");
    ar & field("cache", e.get_cache());
    ar & field("cache_bufferable", e.is_bufferable());
    ar & field("cache_cacheable", e.is_cacheable());
    ar & field("cache_read_alloc", e.is_read_allocate());
    ar & field("cache_write_alloc", e.is_write_allocate());
    ar & field("qos", e.get_qos());
    ar & field("region", e.get_region());
}

template <class Archive> void record(Archive &ar, axi::axi4_extension const& e) {
    ar & field("id", e.get_id());
    ar & field("user[CTRL]", e.get_user(axi::common::id_type::CTRL));
    ar & field("user[DATA]", e.get_user(axi::common::id_type::DATA));
    ar & field("user[RESP]", e.get_user(axi::common::id_type::RESP));
    ar & field("length", e.get_length());
    ar & field("size", e.get_size());
    ar & field("burst", to_char(e.get_burst()));
    ar & field("prot", e.get_prot());
    ar & field("exclusive", e.is_exclusive() ? "true" : "false");
    ar & field("cache", e.get_cache());
    ar & field("cache_bufferable", e.is_bufferable());
    ar & field("cache_modifiable", e.is_modifiable());
    ar & field("cache_allocate", e.is_allocate());
    ar & field("cache_other_alloc", e.is_other_allocate());
    ar & field("qos", e.get_qos());
    ar & field("region", e.get_region());
}

template <class Archive> void record(Archive &ar, axi::ace_extension const& e) {
    ar & field("id", e.get_id());
    ar & field("user[CTRL]", e.get_user(axi::common::id_type::CTRL));
    ar & field("user[DATA]", e.get_user(axi::common::id_type::DATA));
    ar & field("user[RESP]", e.get_user(axi::common::id_type::RESP));
    ar & field("length", e.get_length());
    ar & field("size", e.get_size());
    ar & field("burst", to_char(e.get_burst()));
    ar & field("prot", e.get_prot());
    ar & field("exclusive", e.is_exclusive() ? "true" : "false");
    ar & field("cache", e.get_cache());
    ar & field("cache_bufferable", e.is_bufferable());
    ar & field("cache_modifiable", e.is_modifiable());
    ar & field("cache_write_alloc", e.is_write_allocate());
    ar & field("cache_read_alloc", e.is_read_allocate());
    ar & field("qos", e.get_qos());
    ar & field("region", e.get_region());
    ar & field("domain", to_char(e.get_domain()));
    ar & field("snoop", to_char(e.get_snoop()));
    ar & field("barrier", to_char(e.get_barrier()));
    ar & field("unique", e.get_unique());
}

}

namespace axi {
namespace lwtr {
namespace {
const std::array<std::string, 3> cmd2char{{"tlm::TLM_READ_COMMAND", "tlm::TLM_WRITE_COMMAND", "tlm::TLM_IGNORE_COMMAND"}};
const std::array<std::string, 7> resp2char{
	{"tlm::TLM_OK_RESPONSE", "tlm::TLM_INCOMPLETE_RESPONSE", "tlm::TLM_GENERIC_ERROR_RESPONSE", "tlm::TLM_ADDRESS_ERROR_RESPONSE", "tlm::TLM_COMMAND_ERROR_RESPONSE", "tlm::TLM_BURST_ERROR_RESPONSE", "tlm::TLM_BYTE_ENABLE_ERROR_RESPONSE"}};
const std::array<std::string, 3> gp_option2char{{"tlm::TLM_MIN_PAYLOAD", "tlm::TLM_FULL_PAYLOAD", "tlm::TLM_FULL_PAYLOAD_ACCEPTED"}};
const std::array<std::string, 5> phase2char{{"tlm::UNINITIALIZED_PHASE", "tlm::BEGIN_REQ", "tlm::END_REQ", "tlm::BEGIN_RESP", "tlm::END_RESP"}};
const std::array<std::string, 4> dmi2char{
    {"tlm::DMI_ACCESS_NONE", "tlm::DMI_ACCESS_READ", "tlm::DMI_ACCESS_WRITE", "tlm::DMI_ACCESS_READ_WRITE"}};
const std::array<std::string, 3> sync2char{{"tlm::TLM_ACCEPTED", "tlm::TLM_UPDATED", "tlm::TLM_COMPLETED"}};
} // namespace

class tlm_id_ext_recording : public tlm::scc::lwtr::lwtr4tlm2_extension_registry_if<axi_protocol_types> {
    void recordBeginTx(::lwtr::tx_handle& handle, tlm::tlm_base_protocol_types::tlm_payload_type& trans) override {
        if(auto* ext = trans.get_extension<tlm::scc::tlm_id_extension>())
            handle.record_attribute("trans", *ext);
    }
    void recordEndTx(::lwtr::tx_handle& handle, tlm::tlm_base_protocol_types::tlm_payload_type& trans) override {
    }
};

class axi3_ext_recording : public tlm::scc::lwtr::lwtr4tlm2_extension_registry_if<axi_protocol_types> {
    void recordBeginTx(::lwtr::tx_handle& handle, axi_protocol_types::tlm_payload_type& trans) override {
        if(auto* ext = trans.get_extension<axi3_extension>())
            handle.record_attribute("trans.axi3", *ext);
    }
    void recordEndTx(::lwtr::tx_handle& handle, axi_protocol_types::tlm_payload_type& trans) override {
        if(auto* ext = trans.get_extension<axi3_extension>())
        	handle.record_attribute("trans.axi3.resp", to_char(ext->get_resp()));
   }
};

class axi4_ext_recording : public tlm::scc::lwtr::lwtr4tlm2_extension_registry_if<axi_protocol_types> {
    void recordBeginTx(::lwtr::tx_handle& handle, axi_protocol_types::tlm_payload_type& trans) override {
        if(auto* ext = trans.get_extension<axi4_extension>())
            handle.record_attribute("trans.axi4", *ext);
    }
    void recordEndTx(::lwtr::tx_handle& handle, axi_protocol_types::tlm_payload_type& trans) override {
        if(auto* ext = trans.get_extension<axi4_extension>())
        	handle.record_attribute("trans.axi4.resp", to_char(ext->get_resp()));
   }
};

class ace_ext_recording : public tlm::scc::lwtr::lwtr4tlm2_extension_registry_if<axi_protocol_types> {
    void recordBeginTx(::lwtr::tx_handle& handle, axi_protocol_types::tlm_payload_type& trans) override {
        if(auto* ext = trans.get_extension<ace_extension>())
            handle.record_attribute("trans.ace", *ext);
    }
    void recordEndTx(::lwtr::tx_handle& handle, axi_protocol_types::tlm_payload_type& trans) override {
        if(auto* ext = trans.get_extension<ace_extension>()) {
            handle.record_attribute("trans.ace.resp", to_char(ext->get_resp()));
            handle.record_attribute("trans.ace.cresp_PassDirty", ext->is_pass_dirty());
            handle.record_attribute("trans.ace.cresp_IsShared", ext->is_shared());
            handle.record_attribute("trans.ace.cresp_SnoopDataTransfer", ext->is_snoop_data_transfer());
            handle.record_attribute("trans.ace.cresp_SnoopError", ext->is_snoop_error());
            handle.record_attribute("trans.ace.cresp_SnoopWasUnique", ext->is_snoop_was_unique());
        }
   }
};

//using namespace tlm::scc::scv;
#if defined(__GNUG__)
__attribute__((constructor))
#endif
bool register_extensions() {
    tlm::scc::tlm_id_extension ext(nullptr); // NOLINT
    tlm::scc::lwtr::lwtr4tlm2_extension_registry<axi::axi_protocol_types>::inst().register_ext_rec(
        ext.ID, new axi::lwtr::tlm_id_ext_recording()) ; // NOLINT
    axi::axi3_extension ext3; // NOLINT
    tlm::scc::lwtr::lwtr4tlm2_extension_registry<axi::axi_protocol_types>::inst().register_ext_rec(
        ext3.ID, new axi::lwtr::axi3_ext_recording()); // NOLINT
    axi::axi4_extension ext4;                    // NOLINT
    tlm::scc::lwtr::lwtr4tlm2_extension_registry<axi::axi_protocol_types>::inst().register_ext_rec(
        ext4.ID, new axi::lwtr::axi4_ext_recording()); // NOLINT
    axi::ace_extension extace;                   // NOLINT
    tlm::scc::lwtr::lwtr4tlm2_extension_registry<axi::axi_protocol_types>::inst().register_ext_rec(
    	extace.ID, new axi::lwtr::ace_ext_recording()); // NOLINT
    return true;                                  // NOLINT
}
bool registered = register_extensions();

} // namespace lwtr
} // namespace axi
