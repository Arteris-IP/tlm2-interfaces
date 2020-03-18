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

#include "chi_tlm.h"
#include <iostream>

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
    case req_optype_e::AtomicLoad:
        return "AtomicLoad";
    case req_optype_e::AtomicStore:
        return "AtomicStore";
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
} // namespace chi
#ifdef WITH_SCV
#include <scv4tlm/tlm2_recorder.h>
namespace chi {
using namespace scv4tlm;

class chi_req_ext_recording : public tlm2_extensions_recording_if<chi_protocol_types> {

    void recordBeginTx(scv_tr_handle& handle, chi_protocol_types::tlm_payload_type& trans) {
        auto ext = trans.get_extension<chi_req_extension>();
        if(ext) {
            handle.record_attribute("trans.chi_req.qos", ext->get_qos());
            handle.record_attribute("trans.chi_req.src_id", ext->get_src_id());
            handle.record_attribute("trans.chi_req.txn_id", ext->get_txn_id());
            handle.record_attribute("trans.chi_req.tgt_id", ext->get_tgt_id());
            handle.record_attribute("trans.chi_req.lp_id", ext->get_lp_id());
            handle.record_attribute("trans.chi_req.stash_lp_id", ext->get_stash_lp_id());
            handle.record_attribute("trans.chi_req.size", ext->get_size());
            handle.record_attribute("trans.chi_req.mem_attr", ext->get_mem_attr());
            handle.record_attribute("trans.chi_req.pcrd_type", ext->get_pcrd_type());
            handle.record_attribute("trans.chi_req.endian", ext->is_endian());
            handle.record_attribute("trans.chi_req.order", ext->get_order());
            handle.record_attribute("trans.chi_req.trace_tag", ext->is_trace_tag());
            handle.record_attribute("trans.chi_req.opcode", std::string(to_char(ext->get_opcode())));
            handle.record_attribute("trans.chi_req.stash_node_id", ext->get_stash_n_id());
            handle.record_attribute("trans.chi_req.stash_nnode_id_valid", ext->is_stash_n_id_valid());
            handle.record_attribute("trans.chi_req.stash_lp_id_valid", ext->is_stash_lp_id_valid());
            handle.record_attribute("trans.chi_req.non_secure", ext->is_non_secure());
            handle.record_attribute("trans.chi_req.exp_comp_ack", ext->is_exp_comp_ack());
            handle.record_attribute("trans.chi_req.allow_retry", ext->is_allow_retry());
            handle.record_attribute("trans.chi_req.snp_attr", ext->is_snp_attr());
            handle.record_attribute("trans.chi_req.excl", ext->is_excl());
            handle.record_attribute("trans.chi_req.snoop_me", ext->is_snoop_me());
            handle.record_attribute("trans.chi_req.likely_shared", ext->is_likely_shared());
            handle.record_attribute("trans.chi_req.txn_rsvdc", ext->get_rsvdc()); // Reserved for customer use.
            handle.record_attribute("trans.chi_req.return_txn_id", ext->get_return_txn_id());
            handle.record_attribute("trans.chi_req.return_node_id", ext->get_return_n_id());
        }
    }

    void recordEndTx(scv_tr_handle& handle, chi_protocol_types::tlm_payload_type& trans) {}
};

class chi_data_ext_recording : public tlm2_extensions_recording_if<chi_protocol_types> {

    void recordBeginTx(scv_tr_handle& handle, chi_protocol_types::tlm_payload_type& trans) {
        auto ext = trans.get_extension<chi_data_extension>();
        if(ext) {
            handle.record_attribute("trans.chi_data.db_id", ext->get_db_id());
            handle.record_attribute("trans.chi_data.resp_err", ext->get_resp_err());
            handle.record_attribute("trans.chi_data.resp", std::string(to_char(ext->get_resp())));
            handle.record_attribute("trans.chi_data.fwd_state", ext->get_fwd_state());
            handle.record_attribute("trans.chi_data.data_pull", ext->get_data_pull());
            handle.record_attribute("trans.chi_data.data_source", ext->get_data_source());
            handle.record_attribute("trans.chi_data.cc_id", ext->get_cc_id());
            handle.record_attribute("trans.chi_data.data_id", ext->get_data_id());
            handle.record_attribute("trans.chi_data.poison", ext->get_poison());
            handle.record_attribute("trans.chi_data.tgt_id", ext->get_tgt_id());
            handle.record_attribute("trans.chi_data.home_node_id", ext->get_home_n_id());
            handle.record_attribute("trans.chi_data.opcode", std::string(to_char(ext->get_opcode())));
            handle.record_attribute("trans.chi_data.rsvdc", ext->get_rsvdc());
            handle.record_attribute("trans.chi_data.data_check", ext->get_data_check());
            handle.record_attribute("trans.chi_data.trace_tag", ext->is_trace_tag());
        }
    }

    void recordEndTx(scv_tr_handle& handle, chi_protocol_types::tlm_payload_type& trans) {}
};

class chi_resp_ext_recording : public tlm2_extensions_recording_if<chi_protocol_types> {

    void recordBeginTx(scv_tr_handle& handle, chi_protocol_types::tlm_payload_type& trans) {
        auto ext = trans.get_extension<chi_rsp_extension>();
        if(ext) {
            handle.record_attribute("trans.chi_rsp.db_id", ext->get_db_id());
            handle.record_attribute("trans.chi_rsp.pcrd_type", ext->get_pcrd_type());
            handle.record_attribute("trans.chi_rsp.resp_err", ext->get_resp_err());
            handle.record_attribute("trans.chi_rsp.fwd_state", ext->get_fwd_state());
            handle.record_attribute("trans.chi_rsp.data_pull", ext->get_data_pull());
            handle.record_attribute("trans.chi_req.opcode", std::string(to_char(ext->get_opcode())));
            handle.record_attribute("trans.chi_req.resp", std::string(to_char(ext->get_resp())));
            handle.record_attribute("trans.chi_data.tgt_id", ext->get_tgt_id());
            handle.record_attribute("trans.chi_data.trace_tag", ext->is_trace_tag());
        }
    }

    void recordEndTx(scv_tr_handle& handle, chi_protocol_types::tlm_payload_type& trans) {}
};

class chi_snp_ext_recording : public tlm2_extensions_recording_if<chi_protocol_types> {

    void recordBeginTx(scv_tr_handle& handle, chi_protocol_types::tlm_payload_type& trans) {
        auto ext = trans.get_extension<chi_snp_req_extension>();
        if(ext) {
            handle.record_attribute("trans.chi_snp.txn_id", ext->get_fwd_txn_id());
            handle.record_attribute("trans.chi_snp.stash_lp_id", ext->get_stash_lp_id());
            handle.record_attribute("trans.chi_rsp.vm_id_ext", ext->get_vm_id_ext());
            handle.record_attribute("trans.chi_req.opcode", std::string(to_char(ext->get_opcode())));
            handle.record_attribute("trans.chi_rsp.fwd_n_id", ext->get_fwd_n_id());
            handle.record_attribute("trans.chi_rsp.non_secure", ext->is_non_secure());
            handle.record_attribute("trans.chi_rsp.do_not_goto_sd", ext->is_do_not_goto_sd());
            handle.record_attribute("trans.chi_rsp.do_not_data_pull", ext->is_do_not_data_pull());
            handle.record_attribute("trans.chi_rsp.ret_to_src", ext->is_ret_to_src());
            handle.record_attribute("trans.chi_data.trace_tag", ext->is_trace_tag());
        }
    }

    void recordEndTx(scv_tr_handle& handle, chi_protocol_types::tlm_payload_type& trans) {}
};

class chi_credit_ext_recording : public tlm2_extensions_recording_if<chi_protocol_types> {

    void recordBeginTx(scv_tr_handle& handle, chi_protocol_types::tlm_payload_type& trans) {
        auto ext = trans.get_extension<chi_credit_extension>();
        if(ext) {
            handle.record_attribute("trans.chi_credit.lcredits", ext->get_lcredits());
        }
    }

    void recordEndTx(scv_tr_handle& handle, chi_protocol_types::tlm_payload_type& trans) {}
};
} // namespace chi
namespace scv4chi {
__attribute__((constructor)) bool register_extensions() {
    chi::chi_req_extension extchi_req;
    scv4tlm::tlm2_extension_recording_registry<chi::chi_protocol_types>::inst().register_ext_rec(extchi_req.ID,
                                                                                                 new chi::chi_req_ext_recording());
    chi::chi_data_extension extchi_data;
    scv4tlm::tlm2_extension_recording_registry<chi::chi_protocol_types>::inst().register_ext_rec(extchi_data.ID,
                                                                                                 new chi::chi_data_ext_recording());
    chi::chi_rsp_extension extchi_rsp;
    scv4tlm::tlm2_extension_recording_registry<chi::chi_protocol_types>::inst().register_ext_rec(extchi_rsp.ID,
                                                                                                 new chi::chi_resp_ext_recording());
    chi::chi_snp_req_extension extchi_snp;
    scv4tlm::tlm2_extension_recording_registry<chi::chi_protocol_types>::inst().register_ext_rec(extchi_snp.ID,
                                                                                                 new chi::chi_snp_ext_recording());
    chi::chi_credit_extension extchi_credit;
    scv4tlm::tlm2_extension_recording_registry<chi::chi_protocol_types>::inst().register_ext_rec(extchi_credit.ID,
                                                                                                 new chi::chi_credit_ext_recording());
    return true;
}
bool registered = register_extensions();
} // namespace scv4chi
#endif
