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

#include "chi_tlm.h"
#include <iostream>
#include <tlm/scc/scv/tlm_extension_recording_registry.h>

namespace chi {

template <> const char* to_char<req_optype_e>(req_optype_e v) {
    switch(v) {
    case req_optype_e::ReqLCrdReturn:
        return "ReqLCrdReturn";
    case req_optype_e::ReadShared:
        return "ReadShared";
    case req_optype_e::ReadClean:
        return "ReadClean";
    case req_optype_e::ReadOnce:
        return "ReadOnce";
    case req_optype_e::ReadNoSnp:
        return "ReadNoSnp";
    case req_optype_e::PCrdReturn:
        return "PCrdReturn";
    case req_optype_e::Reserved:
        return "Reserved";
    case req_optype_e::ReadUnique:
        return "ReadUnique";
    case req_optype_e::CleanShared:
        return "CleanShared";
    case req_optype_e::CleanInvalid:
        return "CleanInvalid";
    case req_optype_e::MakeInvalid:
        return "MakeInvalid";
    case req_optype_e::CleanUnique:
        return "CleanUnique";
    case req_optype_e::MakeUnique:
        return "MakeUnique";
    case req_optype_e::Evict:
        return "Evict";
    case req_optype_e::ReadNoSnpSep:
        return "ReadNoSnpSep";
    case req_optype_e::DVMOp:
        return "DVMOp";
    case req_optype_e::WriteEvictFull:
        return "WriteEvictFull";
    case req_optype_e::WriteCleanFull:
        return "WriteCleanFull";
    case req_optype_e::WriteCleanPtl:
        return "WriteCleanPtl";
    case req_optype_e::WriteUniquePtl:
        return "WriteUniquePtl";
    case req_optype_e::WriteUniqueFull:
        return "WriteUniqueFull";
    // mahi: discuss with suresh sir
    // Reserved(EOBarrier)=0x0E
    // Reserved(ECBarrier)=0x0F
    // Reserved=0x10
    // Reserved=0x12-0x13
    // Reserved (WriteCleanPtl)=0x16
    case req_optype_e::WriteBackPtl:
        return "WriteBackPtl";
    case req_optype_e::WriteBackFull:
        return "WriteBackFull";
    case req_optype_e::WriteNoSnpPtl:
        return "WriteNoSnpPtl";
    case req_optype_e::WriteNoSnpFull:
        return "WriteNoSnpFull";
    // RESERVED    0x1E-0x1F
    case req_optype_e::WriteUniqueFullStash:
        return "WriteUniqueFullStash";
    case req_optype_e::WriteUniquePtlStash:
        return "WriteUniquePtlStash";
    case req_optype_e::StashOnceShared:
        return "StashOnceShared";
    case req_optype_e::StashOnceUnique:
        return "StashOnceUnique";
    case req_optype_e::ReadOnceCleanInvalid:
        return "ReadOnceCleanInvalid";
    case req_optype_e::ReadOnceMakeInvalid:
        return "ReadOnceMakeInvalid";
    case req_optype_e::ReadNotSharedDirty:
        return "ReadNotSharedDirty";
    case req_optype_e::CleanSharedPersist:
        return "CleanSharedPersist";
    case req_optype_e::AtomicLoadAdd:
        return "AtomicLoadAdd";
    case req_optype_e::AtomicLoadClr:
        return "AtomicLoadClr";
    case req_optype_e::AtomicLoadEor:
        return "AtomicLoadEor";
    case req_optype_e::AtomicLoadSet:
        return "AtomicLoadSet";
    case req_optype_e::AtomicLoadSmax:
        return "AtomicLoadSmax";
    case req_optype_e::AtomicLoadSmin:
        return "AtomicLoadSmin";
    case req_optype_e::AtomicLoadUmax:
        return "AtomicLoadUmax";
    case req_optype_e::AtomicLoadUmin:
        return "AtomicLoadUmin";
    case req_optype_e::AtomicStoreAdd:
        return "AtomicStoreAdd";
    case req_optype_e::AtomicStoreClr:
        return "AtomicStoreClr";
    case req_optype_e::AtomicStoreEor:
        return "AtomicStoreEor";
    case req_optype_e::AtomicStoreSet:
        return "AtomicStoreSet";
    case req_optype_e::AtomicStoreSmax:
        return "AtomicStoreSmax";
    case req_optype_e::AtomicStoreSmin:
        return "AtomicStoreSmin";
    case req_optype_e::AtomicStoreUmax:
        return "AtomicStoreUmax";
    case req_optype_e::AtomicStoreUmin:
        return "AtomicStoreUmin";
    case req_optype_e::AtomicSwap:
        return "AtomicSwap";
    case req_optype_e::AtomicCompare:
        return "AtomicCompare";
    case req_optype_e::PrefetchTgt:
        return "PrefetchTgt";
    default:
        return "UNKNOWN";
    }
}

template <> const char* to_char<dat_optype_e>(dat_optype_e v) {
    switch(v) {
    case dat_optype_e::DataLCrdReturn:
        return "DataLCrdReturn";
    case dat_optype_e::SnpRespData:
        return "SnpRespData";
    case dat_optype_e::CopyBackWrData:
        return "CopyBackWrData";
    case dat_optype_e::NonCopyBackWrData:
        return "NonCopyBackWrData";
    case dat_optype_e::CompData:
        return "CompData";
    case dat_optype_e::SnpRespDataPtl:
        return "SnpRespDataPtl";
    case dat_optype_e::SnpRespDataFwded:
        return "SnpRespDataFwded";
    case dat_optype_e::WriteDataCancel:
        return "WriteDataCancel";
    case dat_optype_e::DataSepResp:
        return "DataSepResp";
    case dat_optype_e::NCBWrDataCompAck:
        return "NCBWrDataCompAck";
    default:
        return "UNKNOWN_dat_optype_e";
    }
}

template <> const char* to_char<rsp_optype_e>(rsp_optype_e v) {
    switch(v) {
    case rsp_optype_e::RespLCrdReturn:
        return "RespLCrdReturn";
    case rsp_optype_e::SnpResp:
        return "SnpResp";
    case rsp_optype_e::CompAck:
        return "CompAck";
    case rsp_optype_e::RetryAck:
        return "RetryAck";
    case rsp_optype_e::Comp:
        return "Comp";
    case rsp_optype_e::CompDBIDResp:
        return "CompDBIDResp";
    case rsp_optype_e::DBIDResp:
        return "DBIDResp";
    case rsp_optype_e::PCrdGrant:
        return "PCrdGrant";
    case rsp_optype_e::ReadReceipt:
        return "ReadReceipt";
    case rsp_optype_e::SnpRespFwded:
        return "SnpRespFwded";
    case rsp_optype_e::RespSepData:
        return "RespSepData";
    case rsp_optype_e::Invalid:
        return "---";
    default:
        return "UNKNOWN_rsp_optype_e";
    }
}

template <> const char* to_char<snp_optype_e>(snp_optype_e v) {
    switch(v) {
    case snp_optype_e::SnpLCrdReturn:
        return "SnpLCrdReturn";
    case snp_optype_e::SnpShared:
        return "SnpShared";
    case snp_optype_e::SnpClean:
        return "SnpClean";
    case snp_optype_e::SnpOnce:
        return "SnpOnce";
    case snp_optype_e::SnpNotSharedDirty:
        return "SnpNotSharedDirty";
    case snp_optype_e::SnpUniqueStash:
        return "SnpUniqueStash";
    case snp_optype_e::SnpMakeInvalidStash:
        return "SnpMakeInvalidStash";
    case snp_optype_e::SnpUnique:
        return "SnpUnique";
    case snp_optype_e::SnpCleanShared:
        return "SnpCleanShared";
    case snp_optype_e::SnpCleanInvalid:
        return "SnpCleanInvalid";
    case snp_optype_e::SnpMakeInvalid:
        return "SnpMakeInvalid";
    case snp_optype_e::SnpStashUnique:
        return "SnpStashUnique";
    case snp_optype_e::SnpStashShared:
        return "SnpStashShared";
    case snp_optype_e::SnpDVMOp:
        return "SnpDVMOp";
    case snp_optype_e::SnpSharedFwd:
        return "SnpSharedFwd";
    case snp_optype_e::SnpCleanFwd:
        return "SnpCleanFwd";
    case snp_optype_e::SnpOnceFwd:
        return "SnpOnceFwd";
    case snp_optype_e::SnpNotSharedDirtyFwd:
        return "SnpNotSharedDirtyFwd";
    case snp_optype_e::SnpUniqueFwd:
        return "SnpUniqueFwd";
    default:
        return "UNKNOWN_snp_optype_e";
    }
}

template <> const char* to_char<dat_resptype_e>(dat_resptype_e v) {
    switch(v) {
    // case CompData_I: return "CompData_I";
    // case DataSepResp_I: return "DataSepResp_I";
    // case RespSepData_I: return "RespSepData_I";
    // case CompData_UC: return "CompData_UC";
    // case DataSepResp_UC: return "DataSepResp_UC";
    // case RespSepData_UC: return "RespSepData_UC";
    // case CompData_SC: return "CompData_SC";
    // case DataSepResp_SC: return "DataSepResp_SC";
    // case RespSepData_SC: return "RespSepData_SC";
    case dat_resptype_e::CompData_UD_PD:
        return "CompData_UD_PD";
    case dat_resptype_e::CompData_SD_PD:
        return "CompData_SD_PD";
    case dat_resptype_e::Comp_I:
        return "Comp_I";
    case dat_resptype_e::Comp_UC:
        return "Comp_UC";
    case dat_resptype_e::Comp_SC:
        return "Comp_SC";
    // case CopyBackWrData_I: return "CopyBackWrData_I";
    // case CopyBackWrData_UC: return "CopyBackWrData_UC";
    // case CopyBackWrData_SC: return "CopyBackWrData_SC";
    // case CopyBackWrData_UD_PD: return "CopyBackWrData_UD_PD";
    // case CopyBackWrData_SD_PD: return "CopyBackWrData_SD_PD";
    // case NonCopyBackWrData: return "NonCopyBackWrData";
    // case NCBWrDataCompAck: return "NCBWrDataCompAck";
    default:
        return "UNKNOWN_dat_resptype_e";
    }
}

template <> const char* to_char<rsp_resptype_e>(rsp_resptype_e v) {
    switch(v) {
    case rsp_resptype_e::SnpResp_I:
        return "SnpResp_I";
    case rsp_resptype_e::SnpResp_SC:
        return "SnpResp_SC";
    case rsp_resptype_e::SnpResp_UC:
        return "SnpResp_UC";
    // case SnpResp_UD: return "SnpResp_UD";
    case rsp_resptype_e::SnpResp_SD:
        return "SnpResp_SD";
    default:
        return "UNKNOWN_rsp_resptype_e";
    }
}

template <> const char* to_char<credit_type_e>(credit_type_e v) {
    switch(v) {
    case credit_type_e::LINK:
        return "LINK";
    case credit_type_e::REQ:
        return "REQ";
    case credit_type_e::RESP:
        return "RESP";
    case credit_type_e::DATA:
        return "DATA";
    default:
        return "credit_type_e";
    }
}

template <> bool is_valid<chi::chi_ctrl_extension>(chi_ctrl_extension* ext) {
    auto sz = ext->req.get_size();
    if(sz > 6)
        return false;
    return true;
}

} // namespace chi

#include <tlm/scc/scv/tlm_recorder.h>
#include <tlm/scc/tlm_id.h>
namespace chi {
using namespace tlm::scc::scv;

class tlm_id_ext_recording : public tlm_extensions_recording_if<chi_protocol_types> {

    void recordBeginTx(SCVNS scv_tr_handle& handle, chi_protocol_types::tlm_payload_type& trans) override {
        if(auto ext = trans.get_extension<tlm::scc::tlm_id_extension>()) {
            handle.record_attribute("trans.uid", ext->id);
        }
    }

    void recordEndTx(SCVNS scv_tr_handle& handle, tlm::tlm_base_protocol_types::tlm_payload_type& trans) override {
    }
};

class chi_ctrl_ext_recording : public tlm_extensions_recording_if<chi_protocol_types> {

    void recordBeginTx(SCVNS scv_tr_handle& handle, chi_protocol_types::tlm_payload_type& trans) override {
        auto ext = trans.get_extension<chi_ctrl_extension>();
        if(ext) {
            handle.record_attribute("trans.chi_c.qos", ext->get_qos());
            handle.record_attribute("trans.chi_c.src_id", ext->get_src_id());
            handle.record_attribute("trans.chi_c.txn_id", ext->get_txn_id());
            handle.record_attribute("trans.chi_c.tgt_id", ext->req.get_tgt_id());
            handle.record_attribute("trans.chi_c.lp_id", ext->req.get_lp_id());
            handle.record_attribute("trans.chi_c.return_txn_id", ext->req.get_return_txn_id());
            handle.record_attribute("trans.chi_c.stash_lp_id", ext->req.get_stash_lp_id());
            handle.record_attribute("trans.chi_c.size", ext->req.get_size());
            handle.record_attribute("trans.chi_c.mem_attr", ext->req.get_mem_attr());
            handle.record_attribute("trans.chi_c.req.pcrd_type", ext->req.get_pcrd_type());
            handle.record_attribute("trans.chi_c.order", ext->req.get_order());
            handle.record_attribute("trans.chi_c.endian", ext->req.is_endian());
            handle.record_attribute("trans.chi_c.req.trace_tag", ext->req.is_trace_tag());
            handle.record_attribute("trans.chi_c.return_n_id", ext->req.get_return_n_id());
            handle.record_attribute("trans.chi_c.stash_n_id", ext->req.get_stash_n_id());
            handle.record_attribute("trans.chi_c.opcode", std::string(to_char(ext->req.get_opcode())));
            handle.record_attribute("trans.chi_c.stash_nnode_id_valid", ext->req.is_stash_n_id_valid());
            handle.record_attribute("trans.chi_c.stash_lp_id_valid", ext->req.is_stash_lp_id_valid());
            handle.record_attribute("trans.chi_c.non_secure", ext->req.is_non_secure());
            handle.record_attribute("trans.chi_c.exp_comp_ack", ext->req.is_exp_comp_ack());
            handle.record_attribute("trans.chi_c.allow_retry", ext->req.is_allow_retry());
            handle.record_attribute("trans.chi_c.snp_attr", ext->req.is_snp_attr());
            handle.record_attribute("trans.chi_c.excl", ext->req.is_excl());
            handle.record_attribute("trans.chi_c.snoop_me", ext->req.is_snoop_me());
            handle.record_attribute("trans.chi_c.likely_shared", ext->req.is_likely_shared());
            handle.record_attribute("trans.chi_c.txn_rsvdc", ext->req.get_rsvdc()); // Reserved for customer use.
            handle.record_attribute("trans.chi_c.rsp.db_id", ext->resp.get_db_id());
            handle.record_attribute("trans.chi_c.rsp.pcrd_type", ext->resp.get_pcrd_type());
            handle.record_attribute("trans.chi_c.rsp.resp_err", ext->resp.get_resp_err());
            handle.record_attribute("trans.chi_c.rsp.fwd_state", ext->resp.get_fwd_state());
            handle.record_attribute("trans.chi_c.rsp.data_pull", ext->resp.get_data_pull());
            handle.record_attribute("trans.chi_c.rsp.opcode", std::string(to_char(ext->resp.get_opcode())));
            handle.record_attribute("trans.chi_c.rsp.resp", std::string(to_char(ext->resp.get_resp())));
            handle.record_attribute("trans.chi_c.rsp.tgt_id", ext->resp.get_tgt_id());
            handle.record_attribute("trans.chi_c.rsp.trace_tag", ext->resp.is_trace_tag());
        }
    }

    void recordEndTx(SCVNS scv_tr_handle& handle, chi_protocol_types::tlm_payload_type& trans) override {}
};

class chi_data_ext_recording : public tlm_extensions_recording_if<chi_protocol_types> {

    void recordBeginTx(SCVNS scv_tr_handle& handle, chi_protocol_types::tlm_payload_type& trans) override {
        auto ext = trans.get_extension<chi_data_extension>();
        if(ext) {
            handle.record_attribute("trans.chi_d.qos", ext->get_qos());
            handle.record_attribute("trans.chi_d.src_id", ext->get_src_id());
            handle.record_attribute("trans.chi_d.txn_id", ext->get_txn_id());
            handle.record_attribute("trans.chi_d.db_id", ext->dat.get_db_id());
            handle.record_attribute("trans.chi_d.resp_err", ext->dat.get_resp_err());
            handle.record_attribute("trans.chi_d.resp", std::string(to_char(ext->dat.get_resp())));
            handle.record_attribute("trans.chi_d.fwd_state", ext->dat.get_fwd_state());
            handle.record_attribute("trans.chi_d.data_pull", ext->dat.get_data_pull());
            handle.record_attribute("trans.chi_d.data_source", ext->dat.get_data_source());
            handle.record_attribute("trans.chi_d.cc_id", ext->dat.get_cc_id());
            handle.record_attribute("trans.chi_d.data_id", ext->dat.get_data_id());
            handle.record_attribute("trans.chi_d.poison", ext->dat.get_poison());
            handle.record_attribute("trans.chi_d.tgt_id", ext->dat.get_tgt_id());
            handle.record_attribute("trans.chi_d.home_node_id", ext->dat.get_home_n_id());
            handle.record_attribute("trans.chi_d.opcode", std::string(to_char(ext->dat.get_opcode())));
            handle.record_attribute("trans.chi_d.rsvdc", ext->dat.get_rsvdc());
            handle.record_attribute("trans.chi_d.data_check", ext->dat.get_data_check());
            handle.record_attribute("trans.chi_d.trace_tag", ext->dat.is_trace_tag());
        }
    }

    void recordEndTx(SCVNS scv_tr_handle& handle, chi_protocol_types::tlm_payload_type& trans) override {}
};

class chi_snp_ext_recording : public tlm_extensions_recording_if<chi_protocol_types> {

    void recordBeginTx(SCVNS scv_tr_handle& handle, chi_protocol_types::tlm_payload_type& trans) override {
        auto ext = trans.get_extension<chi_snp_extension>();
        if(ext) {
            handle.record_attribute("trans.chi_s.qos", ext->get_qos());
            handle.record_attribute("trans.chi_s.src_id", ext->get_src_id());
            handle.record_attribute("trans.chi_s.txn_id", ext->get_txn_id());
            handle.record_attribute("trans.chi_s.fwd_txn_id", ext->req.get_fwd_txn_id());
            handle.record_attribute("trans.chi_s.stash_lp_id", ext->req.get_stash_lp_id());
            handle.record_attribute("trans.chi_s.vm_id_ext", ext->req.get_vm_id_ext());
            handle.record_attribute("trans.chi_s.stash_lp_id_valid", ext->req.is_stash_lp_id_valid());
            handle.record_attribute("trans.chi_s.opcode", std::string(to_char(ext->req.get_opcode())));
            handle.record_attribute("trans.chi_s.fwd_n_id", ext->req.get_fwd_n_id());
            handle.record_attribute("trans.chi_s.non_secure", ext->req.is_non_secure());
            handle.record_attribute("trans.chi_s.do_not_goto_sd", ext->req.is_do_not_goto_sd());
            handle.record_attribute("trans.chi_s.do_not_data_pull", ext->req.is_do_not_data_pull());
            handle.record_attribute("trans.chi_s.ret_to_src", ext->req.is_ret_to_src());
            handle.record_attribute("trans.chi_s.trace_tag", ext->req.is_trace_tag());
            handle.record_attribute("trans.chi_s.rsp.db_id", ext->resp.get_db_id());
            handle.record_attribute("trans.chi_s.rsp.pcrd_type", ext->resp.get_pcrd_type());
            handle.record_attribute("trans.chi_s.rsp.resp_err", ext->resp.get_resp_err());
            handle.record_attribute("trans.chi_s.rsp.fwd_state", ext->resp.get_fwd_state());
            handle.record_attribute("trans.chi_s.rsp.data_pull", ext->resp.get_data_pull());
            handle.record_attribute("trans.chi_s.rsp.opcode", std::string(to_char(ext->resp.get_opcode())));
            handle.record_attribute("trans.chi_s.rsp.resp", std::string(to_char(ext->resp.get_resp())));
            handle.record_attribute("trans.chi_s.rsp.tgt_id", ext->resp.get_tgt_id());
            handle.record_attribute("trans.chi_s.rsp.trace_tag", ext->resp.is_trace_tag());
        }
    }

    void recordEndTx(SCVNS scv_tr_handle& handle, chi_protocol_types::tlm_payload_type& trans) override {}
};

class chi_credit_ext_recording : public tlm_extensions_recording_if<chi_protocol_types> {

    void recordBeginTx(SCVNS scv_tr_handle& handle, chi_protocol_types::tlm_payload_type& trans) override {
        auto ext = trans.get_extension<chi_credit_extension>();
        if(ext) {
            handle.record_attribute("trans.chi_credit.type", std::string(to_char(ext->type)));
            handle.record_attribute("trans.chi_credit.count", ext->count);
        }
    }

    void recordEndTx(SCVNS scv_tr_handle& handle, chi_protocol_types::tlm_payload_type& trans) override {}
};
namespace scv {
using namespace tlm::scc::scv;
#if defined(__GNUG__)
__attribute__((constructor))
#endif
bool register_extensions() {
    tlm::scc::tlm_id_extension ext(nullptr); // NOLINT
    tlm_extension_recording_registry<chi::chi_protocol_types>::inst().register_ext_rec(
        ext.ID, new tlm_id_ext_recording()); // NOLINT
    chi::chi_ctrl_extension extchi_req; // NOLINT
    tlm_extension_recording_registry<chi::chi_protocol_types>::inst().register_ext_rec(
        extchi_req.ID,
        new chi::chi_ctrl_ext_recording()); // NOLINT
    chi::chi_data_extension extchi_data;    // NOLINT
    tlm_extension_recording_registry<chi::chi_protocol_types>::inst().register_ext_rec(
        extchi_data.ID,
        new chi::chi_data_ext_recording()); // NOLINT
    chi::chi_snp_extension extchi_snp;      // NOLINT
    tlm_extension_recording_registry<chi::chi_protocol_types>::inst().register_ext_rec(
        extchi_snp.ID,
        new chi::chi_snp_ext_recording());   // NOLINT
    chi::chi_credit_extension extchi_credit; // NOLINT
    tlm_extension_recording_registry<chi::chi_protocol_types>::inst().register_ext_rec(
        extchi_credit.ID,
        new chi::chi_credit_ext_recording()); // NOLINT
    return true;                              // NOLINT
}
bool registered = register_extensions();
} // namespace scv
} // namespace chi
