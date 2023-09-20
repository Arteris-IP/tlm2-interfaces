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

#include <chi/chi_tlm.h>
#include <tlm/scc/tlm_id.h>
#include <tlm/scc/scv/tlm_extension_recording_registry.h>
#include <tlm/scc/lwtr/lwtr4tlm2_extension_registry.h>

namespace lwtr {
using namespace chi;

template <class Archive> void record(Archive &ar, tlm::scc::tlm_id_extension const& e) {ar & field("uid", e.id);}

template <class Archive> void record(Archive &ar, chi::chi_ctrl_extension const& e) {
    ar & field("qos", e.get_qos());
    ar & field("src_id", e.get_src_id());
    ar & field("txn_id", e.get_txn_id());
    ar & field("tgt_id", e.req.get_tgt_id());
    ar & field("lp_id", e.req.get_lp_id());
    ar & field("return_txn_id", e.req.get_return_txn_id());
    ar & field("stash_lp_id", e.req.get_stash_lp_id());
    ar & field("size", e.req.get_size());
    ar & field("mem_attr", e.req.get_mem_attr());
    ar & field("req.pcrd_type", e.req.get_pcrd_type());
    ar & field("order", e.req.get_order());
    ar & field("endian", e.req.is_endian());
    ar & field("req.trace_tag", e.req.is_trace_tag());
    ar & field("return_n_id", e.req.get_return_n_id());
    ar & field("stash_n_id", e.req.get_stash_n_id());
    ar & field("opcode", to_char(e.req.get_opcode()));
    ar & field("stash_nnode_id_valid", e.req.is_stash_n_id_valid());
    ar & field("stash_lp_id_valid", e.req.is_stash_lp_id_valid());
    ar & field("non_secure", e.req.is_non_secure());
    ar & field("exp_comp_ack", e.req.is_exp_comp_ack());
    ar & field("allow_retry", e.req.is_allow_retry());
    ar & field("snp_attr", e.req.is_snp_attr());
    ar & field("excl", e.req.is_excl());
    ar & field("snoop_me", e.req.is_snoop_me());
    ar & field("likely_shared", e.req.is_likely_shared());
    ar & field("txn_rsvdc", e.req.get_rsvdc()); // Reserved for customer use.
    ar & field("tag_op", e.req.get_tag_op());
    ar & field("tag_group_id", e.req.get_tag_group_id());
    ar & field("mpam", e.req.get_mpam());
    ar & field("rsp.db_id", e.resp.get_db_id());
    ar & field("rsp.pcrd_type", e.resp.get_pcrd_type());
    ar & field("rsp.resp_err", to_char(e.resp.get_resp_err()));
    ar & field("rsp.fwd_state", e.resp.get_fwd_state());
    ar & field("rsp.data_pull", e.resp.get_data_pull());
    ar & field("rsp.opcode", to_char(e.resp.get_opcode()));
    ar & field("rsp.resp", to_char(e.resp.get_resp()));
    ar & field("rsp.tgt_id", e.resp.get_tgt_id());
    ar & field("rsp.trace_tag", e.resp.is_trace_tag());
    ar & field("rsp.tag_op", e.resp.get_tag_op());
    ar & field("rsp.tag_group_id", e.resp.get_tag_group_id());

}

template <class Archive> void record(Archive &ar, chi::chi_data_extension const& e) {
    ar & field("qos", e.get_qos());
    ar & field("src_id", e.get_src_id());
    ar & field("txn_id", e.get_txn_id());
    ar & field("db_id", e.dat.get_db_id());
    ar & field("resp_err", to_char(e.dat.get_resp_err()));
    ar & field("resp", to_char(e.dat.get_resp()));
    ar & field("fwd_state", e.dat.get_fwd_state());
    ar & field("data_pull", e.dat.get_data_pull());
    ar & field("data_source", e.dat.get_data_source());
    ar & field("cc_id", e.dat.get_cc_id());
    ar & field("data_id", e.dat.get_data_id());
    ar & field("poison", e.dat.get_poison());
    ar & field("tgt_id", e.dat.get_tgt_id());
    ar & field("home_node_id", e.dat.get_home_n_id());
    ar & field("opcode", to_char(e.dat.get_opcode()));
    ar & field("rsvdc", e.dat.get_rsvdc());
    ar & field("data_check", e.dat.get_data_check());
    ar & field("trace_tag", e.dat.is_trace_tag());
    ar & field("tag_op", e.dat.get_tag_op());
    ar & field("tag", e.dat.get_tag());
    ar & field("tu", e.dat.get_tu());
}

template <class Archive> void record(Archive &ar, chi::chi_snp_extension const& e) {
    ar & field("qos", e.get_qos());
    ar & field("src_id", e.get_src_id());
    ar & field("txn_id", e.get_txn_id());
    ar & field("fwd_txn_id", e.req.get_fwd_txn_id());
    ar & field("stash_lp_id", e.req.get_stash_lp_id());
    ar & field("vm_id_ext", e.req.get_vm_id_ext());
    ar & field("stash_lp_id_valid", e.req.is_stash_lp_id_valid());
    ar & field("opcode", to_char(e.req.get_opcode()));
    ar & field("fwd_n_id", e.req.get_fwd_n_id());
    ar & field("non_secure", e.req.is_non_secure());
    ar & field("do_not_goto_sd", e.req.is_do_not_goto_sd());
    ar & field("do_not_data_pull", e.req.is_do_not_data_pull());
    ar & field("ret_to_src", e.req.is_ret_to_src());
    ar & field("trace_tag", e.req.is_trace_tag());
    ar & field("rsp.db_id", e.resp.get_db_id());
    ar & field("rsp.pcrd_type", e.resp.get_pcrd_type());
    ar & field("rsp.resp_err", to_char(e.resp.get_resp_err()));
    ar & field("rsp.fwd_state", e.resp.get_fwd_state());
    ar & field("rsp.data_pull", e.resp.get_data_pull());
    ar & field("rsp.opcode", to_char(e.resp.get_opcode()));
    ar & field("rsp.resp", to_char(e.resp.get_resp()));
    ar & field("rsp.tgt_id", e.resp.get_tgt_id());
    ar & field("rsp.trace_tag", e.resp.is_trace_tag());
}

template <class Archive> void record(Archive &ar, chi::chi_credit_extension const& e) {
    ar & field("type", to_char(e.type));
    ar & field("count", e.count);
}
}

namespace chi {
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

class tlm_id_ext_recording : public tlm::scc::lwtr::lwtr4tlm2_extension_registry_if<chi_protocol_types> {
    void recordBeginTx(::lwtr::tx_handle& handle, tlm::tlm_base_protocol_types::tlm_payload_type& trans) override {
        if(auto* ext = trans.get_extension<tlm::scc::tlm_id_extension>())
            handle.record_attribute("trans", *ext);
    }
    void recordEndTx(::lwtr::tx_handle& handle, tlm::tlm_base_protocol_types::tlm_payload_type& trans) override { }
};

class chi_ctrl_ext_recording : public tlm::scc::lwtr::lwtr4tlm2_extension_registry_if<chi_protocol_types> {
    void recordBeginTx(::lwtr::tx_handle& handle, chi_protocol_types::tlm_payload_type& trans) override {
        if(auto* ext = trans.get_extension<chi::chi_ctrl_extension>())
            handle.record_attribute("trans.chi_c", *ext);
    }
    void recordEndTx(::lwtr::tx_handle& handle, chi_protocol_types::tlm_payload_type& trans) override { }
};
class chi_data_ext_recording : public tlm::scc::lwtr::lwtr4tlm2_extension_registry_if<chi_protocol_types> {
    void recordBeginTx(::lwtr::tx_handle& handle, chi_protocol_types::tlm_payload_type& trans) override {
        if(auto* ext = trans.get_extension<chi::chi_data_extension>())
            handle.record_attribute("trans.chi_c", *ext);
    }
    void recordEndTx(::lwtr::tx_handle& handle, chi_protocol_types::tlm_payload_type& trans) override { }
};
class chi_snp_ext_recording : public tlm::scc::lwtr::lwtr4tlm2_extension_registry_if<chi_protocol_types> {
    void recordBeginTx(::lwtr::tx_handle& handle, chi_protocol_types::tlm_payload_type& trans) override {
        if(auto* ext = trans.get_extension<chi::chi_snp_extension>())
            handle.record_attribute("trans.chi_s", *ext);
    }
    void recordEndTx(::lwtr::tx_handle& handle, chi_protocol_types::tlm_payload_type& trans) override { }
};
class chi_link_ext_recording : public tlm::scc::lwtr::lwtr4tlm2_extension_registry_if<chi_protocol_types> {
    void recordBeginTx(::lwtr::tx_handle& handle, chi_protocol_types::tlm_payload_type& trans) override {
        if(auto* ext = trans.get_extension<chi::chi_credit_extension>())
            handle.record_attribute("trans.chi_credit", *ext);
    }
    void recordEndTx(::lwtr::tx_handle& handle, chi_protocol_types::tlm_payload_type& trans) override { }
};

//using namespace tlm::scc::scv;
#if defined(__GNUG__)
__attribute__((constructor))
#endif
bool register_extensions() {
    tlm::scc::tlm_id_extension ext(nullptr); // NOLINT
    tlm::scc::lwtr::lwtr4tlm2_extension_registry<chi::chi_protocol_types>::inst().register_ext_rec(
        ext.ID, new chi::lwtr::tlm_id_ext_recording()) ; // NOLINT
    chi::chi_ctrl_extension chi_c; // NOLINT
    tlm::scc::lwtr::lwtr4tlm2_extension_registry<chi::chi_protocol_types>::inst().register_ext_rec(
    		chi_c.ID, new chi::lwtr::chi_ctrl_ext_recording()); // NOLINT
    chi::chi_data_extension chi_d; // NOLINT
    tlm::scc::lwtr::lwtr4tlm2_extension_registry<chi::chi_protocol_types>::inst().register_ext_rec(
    		chi_d.ID, new chi::lwtr::chi_data_ext_recording()); // NOLINT
    chi::chi_snp_extension chi_s; // NOLINT
    tlm::scc::lwtr::lwtr4tlm2_extension_registry<chi::chi_protocol_types>::inst().register_ext_rec(
    		chi_s.ID, new chi::lwtr::chi_snp_ext_recording()); // NOLINT
    chi::chi_credit_extension chi_l; // NOLINT
    tlm::scc::lwtr::lwtr4tlm2_extension_registry<chi::chi_protocol_types>::inst().register_ext_rec(
    		chi_l.ID, new chi::lwtr::chi_link_ext_recording()); // NOLINT
    return true;                                  // NOLINT
}
bool registered = register_extensions();

} // namespace lwtr
} // namespace axi
