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
#include <fmt/format.h>
namespace chi {
namespace {
std::array<std::string, 3> cmd_str{"R", "W", "I"};
}
std::array<char const*, 103> opc2str = {
    "ReqLCrdReturn",
    "ReadShared",
    "ReadClean",
    "ReadOnce",
    "ReadNoSnp",
    "PCrdReturn",
    "Reserved",
    "ReadUnique",
    "CleanShared",
    "CleanInvalid",
    "MakeInvalid",
    "CleanUnique",
    "MakeUnique",
    "Evict",
    "EOBarrier",
    "ECBarrier",
    "RESERVED",
    "ReadNoSnpSep",
    "RESERVED",
    "CleanSharedPersistSep",
    "DVMOp",
    "WriteEvictFull",
    "WriteCleanPtl",
    "WriteCleanFull",
    "WriteUniquePtl",
    "WriteUniqueFull",
    "WriteBackPtl",
    "WriteBackFull",
    "WriteNoSnpPtl",
    "WriteNoSnpFull",
    "RESERVED",
    "RESERVED",
    "WriteUniqueFullStash",
    "WriteUniquePtlStash",
    "StashOnceShared",
    "StashOnceUnique",
    "ReadOnceCleanInvalid",
    "ReadOnceMakeInvalid",
    "ReadNotSharedDirty",
    "CleanSharedPersist",
    "AtomicStoreAdd",
    "AtomicStoreClr",
    "AtomicStoreEor",
    "AtomicStoreSet",
    "AtomicStoreSmax",
    "AtomicStoreSmin",
    "AtomicStoreUmax",
    "AtomicStoreUmin",
    "AtomicLoadAdd",
    "AtomicLoadClr",
    "AtomicLoadEor",
    "AtomicLoadSet",
    "AtomicLoadSmax",
    "AtomicLoadSmin",
    "AtomicLoadUmax",
    "AtomicLoadUmin",
    "AtomicSwap",
    "AtomicCompare",
    "PrefetchTgt",
    "RESERVED",
    "RESERVED",
    "RESERVED",
    "RESERVED",
    "RESERVED",
    "RESERVED",
    "MakeReadUnique",
    "WriteEvictOrEvict",
    "WriteUniqueZero",
    "WriteNoSnpZero",
    "RESERVED",
    "RESERVED",
    "StashOnceSepShared",
    "StashOnceSepUnique",
    "RESERVED",
    "RESERVED",
    "RESERVED",
    "ReadPreferUnique",
    "RESERVED",
    "RESERVED",
    "RESERVED",
    "WriteNoSnpFullCleanSh",
    "WriteNoSnpFullCleanInv",
    "WriteNoSnpFullCleanShPerSep",
    "RESERVED",
    "WriteUniqueFullCleanSh",
    "RESERVED",
    "WriteUniqueFullCleanShPerSep",
    "RESERVED",
    "WriteBackFullCleanSh",
    "WriteBackFullCleanInv",
    "WriteBackFullCleanShPerSep",
    "RESERVED",
    "WriteCleanFullCleanSh",
    "RESERVED",
    "WriteCleanFullCleanShPerSep",
    "RESERVED",
    "WriteNoSnpPtlCleanSh",
    "WriteNoSnpPtlCleanInv",
    "WriteNoSnpPtlCleanShPerSep",
    "RESERVED",
    "WriteUniquePtlCleanSh",
    "RESERVED",
    "WriteUniquePtlCleanShPerSep",
};
template <> const char* to_char<req_optype_e>(req_optype_e v) {
    auto idx = static_cast<unsigned>(v);
    if(idx < opc2str.size())
        return opc2str[idx];
    else
        return "RESERVED";
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
    case rsp_optype_e::TagMatch:
        return "TagMatch";
    case rsp_optype_e::RespSepData:
        return "RespSepData";
    case rsp_optype_e::Persist:
        return "Persist";
    case rsp_optype_e::CompPersist:
        return "CompPersist";
    case rsp_optype_e::DBIDRespOrd:
        return "DBIDRespOrd";
    case rsp_optype_e::StashDone:
        return "StashDone";
    case rsp_optype_e::CompStashDone:
        return "CompStashDone";
    case rsp_optype_e::CompCMO:
        return "CompCMO";
    case rsp_optype_e::INVALID:
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

template <> const char* to_char<rsp_resperrtype_e>(rsp_resperrtype_e v) {
    switch(v) {
    case rsp_resperrtype_e::OK:
        return "OK";
    case rsp_resperrtype_e::EXOK:
        return "EXOK";
    case rsp_resperrtype_e::DERR:
        return "DERR";
    case rsp_resperrtype_e::NDERR:
        return "NDERR";
    default:
        return "rsp_resperrtype_e";
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

std::ostream& operator<<(std::ostream& os, const tlm::tlm_generic_payload& t) {
    char const* ch = t.get_command() == tlm::TLM_READ_COMMAND ? "AR" : (t.get_command() == tlm::TLM_WRITE_COMMAND ? "AW" : "");
    os << "CMD:" << cmd_str[t.get_command()] << ", " << ch << "ADDR:0x" << std::hex << t.get_address() << ", TXLEN:0x"
       << t.get_data_length();
    if(auto ext = t.get_extension<chi::chi_ctrl_extension>()) {
        os << ", TXNID:" << ext->get_txn_id();
        os << ", OPC:" << chi::to_char(ext->req.get_opcode());
    } else if(auto ext = t.get_extension<chi::chi_snp_extension>()) {
        os << ", TXNID:" << ext->get_txn_id();
        os << ", OPC:" << chi::to_char(ext->req.get_opcode());
    }
    os << " [ptr:" << &t << "]";
    return os;
}
template <> const char* is_valid_msg<chi::chi_ctrl_extension>(chi_ctrl_extension* ext) {
    auto sz = ext->req.get_size();
    if(sz > 6)
        return "Illegal size setting, maximum is 6";
    switch(ext->req.get_opcode()) {
    case req_optype_e::ReadNoSnp:
    case req_optype_e::ReadNoSnpSep:
    case req_optype_e::WriteNoSnpPtl:
    case req_optype_e::WriteUniquePtl:
    case req_optype_e::WriteUniquePtlStash:
        break;
    default:
        if(!is_dataless(ext) && sz < 6)
            return "Coherent transactions allow only cache line size data transfers";
    }
    return nullptr;
}

} // namespace chi

#include <tlm/scc/scv/tlm_recorder.h>
#include <tlm/scc/tlm_id.h>
namespace chi {
using namespace tlm::scc::scv;
#ifdef HAS_SCV
using namespace ::scv;
#else
using namespace ::scv_tr;
#endif

struct tlm_id_ext_recording : public tlm_extensions_recording_if<chi::chi_protocol_types> {

    tlm_id_ext_recording() { recordBegin = &recordBeginTx; }

    static void recordBeginTx(scv_tr_handle& handle, chi::chi_protocol_types::tlm_payload_type& trans) {
        tlm_extension_record_registry::get().recordBeginTx(tlm::scc::tlm_id_extension::ID, handle,
                                                           trans.get_extension<tlm::scc::tlm_id_extension>(), "trans.");
    }
};

struct chi_ctrl_ext_record : public tlm_extension_record_if {

    chi_ctrl_ext_record() { recordBegin = &chi_ctrl_ext_record::record; }

    static void record(scv_tr_handle& handle, tlm::tlm_extension_base* e, std::string const& prefix) {
        if(auto ext = dynamic_cast<chi_ctrl_extension*>(e)) {
            handle.record_attribute(fmt::format("{}qos", prefix).c_str(), ext->get_qos());
            handle.record_attribute(fmt::format("{}src_id", prefix).c_str(), ext->get_src_id());
            handle.record_attribute(fmt::format("{}txn_id", prefix).c_str(), ext->get_txn_id());
            handle.record_attribute(fmt::format("{}tgt_id", prefix).c_str(), ext->req.get_tgt_id());
            handle.record_attribute(fmt::format("{}lp_id", prefix).c_str(), ext->req.get_lp_id());
            handle.record_attribute(fmt::format("{}return_txn_id", prefix).c_str(), ext->req.get_return_txn_id());
            handle.record_attribute(fmt::format("{}stash_lp_id", prefix).c_str(), ext->req.get_stash_lp_id());
            handle.record_attribute(fmt::format("{}size", prefix).c_str(), ext->req.get_size());
            handle.record_attribute(fmt::format("{}mem_attr", prefix).c_str(), ext->req.get_mem_attr());
            handle.record_attribute(fmt::format("{}req.pcrd_type", prefix).c_str(), ext->req.get_pcrd_type());
            handle.record_attribute(fmt::format("{}order", prefix).c_str(), ext->req.get_order());
            handle.record_attribute(fmt::format("{}endian", prefix).c_str(), ext->req.is_endian());
            handle.record_attribute(fmt::format("{}req.trace_tag", prefix).c_str(), ext->req.is_trace_tag());
            handle.record_attribute(fmt::format("{}return_n_id", prefix).c_str(), ext->req.get_return_n_id());
            handle.record_attribute(fmt::format("{}stash_n_id", prefix).c_str(), ext->req.get_stash_n_id());
            handle.record_attribute(fmt::format("{}opcode", prefix).c_str(), std::string(to_char(ext->req.get_opcode())));
            handle.record_attribute(fmt::format("{}stash_nnode_id_valid", prefix).c_str(), ext->req.is_stash_n_id_valid());
            handle.record_attribute(fmt::format("{}stash_lp_id_valid", prefix).c_str(), ext->req.is_stash_lp_id_valid());
            handle.record_attribute(fmt::format("{}non_secure", prefix).c_str(), ext->req.is_non_secure());
            handle.record_attribute(fmt::format("{}exp_comp_ack", prefix).c_str(), ext->req.is_exp_comp_ack());
            handle.record_attribute(fmt::format("{}allow_retry", prefix).c_str(), ext->req.is_allow_retry());
            handle.record_attribute(fmt::format("{}snp_attr", prefix).c_str(), ext->req.is_snp_attr());
            handle.record_attribute(fmt::format("{}excl", prefix).c_str(), ext->req.is_excl());
            handle.record_attribute(fmt::format("{}snoop_me", prefix).c_str(), ext->req.is_snoop_me());
            handle.record_attribute(fmt::format("{}likely_shared", prefix).c_str(), ext->req.is_likely_shared());
            handle.record_attribute(fmt::format("{}txn_rsvdc", prefix).c_str(), ext->req.get_rsvdc()); // Reserved for customer use.
            handle.record_attribute(fmt::format("{}tag_op", prefix).c_str(), ext->req.get_tag_op());
            handle.record_attribute(fmt::format("{}tag_group_id", prefix).c_str(), ext->req.get_tag_group_id());
            handle.record_attribute(fmt::format("{}mpam", prefix).c_str(), ext->req.get_mpam());
            handle.record_attribute(fmt::format("{}rsp.db_id", prefix).c_str(), ext->resp.get_db_id());
            handle.record_attribute(fmt::format("{}rsp.pcrd_type", prefix).c_str(), ext->resp.get_pcrd_type());
            handle.record_attribute(fmt::format("{}rsp.resp_err", prefix).c_str(), std::string(to_char(ext->resp.get_resp_err())));
            handle.record_attribute(fmt::format("{}rsp.fwd_state", prefix).c_str(), ext->resp.get_fwd_state());
            handle.record_attribute(fmt::format("{}rsp.data_pull", prefix).c_str(), ext->resp.get_data_pull());
            handle.record_attribute(fmt::format("{}rsp.opcode", prefix).c_str(), std::string(to_char(ext->resp.get_opcode())));
            handle.record_attribute(fmt::format("{}rsp.resp", prefix).c_str(), std::string(to_char(ext->resp.get_resp())));
            handle.record_attribute(fmt::format("{}rsp.tgt_id", prefix).c_str(), ext->resp.get_tgt_id());
            handle.record_attribute(fmt::format("{}rsp.trace_tag", prefix).c_str(), ext->resp.is_trace_tag());
            handle.record_attribute(fmt::format("{}rsp.tag_op", prefix).c_str(), ext->resp.get_tag_op());
            handle.record_attribute(fmt::format("{}rsp.tag_group_id", prefix).c_str(), ext->resp.get_tag_group_id());
        }
    }
};

struct chi_ctrl_ext_recording : public tlm_extensions_recording_if<chi::chi_protocol_types> {

    chi_ctrl_ext_recording() { recordBegin = &recordBeginTx; }

    static void recordBeginTx(scv_tr_handle& handle, chi::chi_protocol_types::tlm_payload_type& trans) {
        tlm_extension_record_registry::get().recordBeginTx(chi_ctrl_extension::ID, handle, trans.get_extension<chi_ctrl_extension>(), "trans.chi_c.");
    }
};

struct chi_data_ext_record : public tlm_extension_record_if {

    chi_data_ext_record() { recordBegin = &record; }

    static void record(scv_tr_handle& handle, tlm::tlm_extension_base* e, std::string const& prefix) {
        if(auto ext = dynamic_cast<chi_data_extension*>(e)) {
            handle.record_attribute(fmt::format("{}qos", prefix).c_str(), ext->get_qos());
            handle.record_attribute(fmt::format("{}src_id", prefix).c_str(), ext->get_src_id());
            handle.record_attribute(fmt::format("{}txn_id", prefix).c_str(), ext->get_txn_id());
            handle.record_attribute(fmt::format("{}db_id", prefix).c_str(), ext->dat.get_db_id());
            handle.record_attribute(fmt::format("{}resp_err", prefix).c_str(), std::string(to_char(ext->dat.get_resp_err())));
            handle.record_attribute(fmt::format("{}resp", prefix).c_str(), std::string(to_char(ext->dat.get_resp())));
            handle.record_attribute(fmt::format("{}fwd_state", prefix).c_str(), ext->dat.get_fwd_state());
            handle.record_attribute(fmt::format("{}data_pull", prefix).c_str(), ext->dat.get_data_pull());
            handle.record_attribute(fmt::format("{}data_source", prefix).c_str(), ext->dat.get_data_source());
            handle.record_attribute(fmt::format("{}cc_id", prefix).c_str(), ext->dat.get_cc_id());
            handle.record_attribute(fmt::format("{}data_id", prefix).c_str(), ext->dat.get_data_id());
            handle.record_attribute(fmt::format("{}poison", prefix).c_str(), ext->dat.get_poison());
            handle.record_attribute(fmt::format("{}tgt_id", prefix).c_str(), ext->dat.get_tgt_id());
            handle.record_attribute(fmt::format("{}home_node_id", prefix).c_str(), ext->dat.get_home_n_id());
            handle.record_attribute(fmt::format("{}opcode", prefix).c_str(), std::string(to_char(ext->dat.get_opcode())));
            handle.record_attribute(fmt::format("{}rsvdc", prefix).c_str(), ext->dat.get_rsvdc());
            handle.record_attribute(fmt::format("{}data_check", prefix).c_str(), ext->dat.get_data_check());
            handle.record_attribute(fmt::format("{}trace_tag", prefix).c_str(), ext->dat.is_trace_tag());
            handle.record_attribute(fmt::format("{}tag_op", prefix).c_str(), ext->dat.get_tag_op());
            handle.record_attribute(fmt::format("{}tag", prefix).c_str(), ext->dat.get_tag());
            handle.record_attribute(fmt::format("{}tu", prefix).c_str(), ext->dat.get_tu());
        }
    }
};

struct chi_data_ext_recording : public tlm_extensions_recording_if<chi::chi_protocol_types> {

    chi_data_ext_recording() { recordBegin = &recordBeginTx; }

    static void recordBeginTx(scv_tr_handle& handle, chi::chi_protocol_types::tlm_payload_type& trans) {
        tlm_extension_record_registry::get().recordBeginTx(chi_data_extension::ID, handle, trans.get_extension<chi_data_extension>(), "trans.chi_d.");
    }
};

struct chi_snp_ext_record : public tlm_extension_record_if {

    chi_snp_ext_record() { recordBegin = &record; }

    static void record(scv_tr_handle& handle, tlm::tlm_extension_base* e, std::string const& prefix) {
        if(auto ext = dynamic_cast<chi_snp_extension*>(e)) {
            handle.record_attribute(fmt::format("{}qos", prefix).c_str(), ext->get_qos());
            handle.record_attribute(fmt::format("{}src_id", prefix).c_str(), ext->get_src_id());
            handle.record_attribute(fmt::format("{}txn_id", prefix).c_str(), ext->get_txn_id());
            handle.record_attribute(fmt::format("{}fwd_txn_id", prefix).c_str(), ext->req.get_fwd_txn_id());
            handle.record_attribute(fmt::format("{}stash_lp_id", prefix).c_str(), ext->req.get_stash_lp_id());
            handle.record_attribute(fmt::format("{}vm_id_ext", prefix).c_str(), ext->req.get_vm_id_ext());
            handle.record_attribute(fmt::format("{}stash_lp_id_valid", prefix).c_str(), ext->req.is_stash_lp_id_valid());
            handle.record_attribute(fmt::format("{}opcode", prefix).c_str(), std::string(to_char(ext->req.get_opcode())));
            handle.record_attribute(fmt::format("{}fwd_n_id", prefix).c_str(), ext->req.get_fwd_n_id());
            handle.record_attribute(fmt::format("{}non_secure", prefix).c_str(), ext->req.is_non_secure());
            handle.record_attribute(fmt::format("{}do_not_goto_sd", prefix).c_str(), ext->req.is_do_not_goto_sd());
            handle.record_attribute(fmt::format("{}do_not_data_pull", prefix).c_str(), ext->req.is_do_not_data_pull());
            handle.record_attribute(fmt::format("{}ret_to_src", prefix).c_str(), ext->req.is_ret_to_src());
            handle.record_attribute(fmt::format("{}trace_tag", prefix).c_str(), ext->req.is_trace_tag());
            handle.record_attribute(fmt::format("{}rsp.db_id", prefix).c_str(), ext->resp.get_db_id());
            handle.record_attribute(fmt::format("{}rsp.pcrd_type", prefix).c_str(), ext->resp.get_pcrd_type());
            handle.record_attribute(fmt::format("{}rsp.resp_err", prefix).c_str(), std::string(to_char(ext->resp.get_resp_err())));
            handle.record_attribute(fmt::format("{}rsp.fwd_state", prefix).c_str(), ext->resp.get_fwd_state());
            handle.record_attribute(fmt::format("{}rsp.data_pull", prefix).c_str(), ext->resp.get_data_pull());
            handle.record_attribute(fmt::format("{}rsp.opcode", prefix).c_str(), std::string(to_char(ext->resp.get_opcode())));
            handle.record_attribute(fmt::format("{}rsp.resp", prefix).c_str(), std::string(to_char(ext->resp.get_resp())));
            handle.record_attribute(fmt::format("{}rsp.tgt_id", prefix).c_str(), ext->resp.get_tgt_id());
            handle.record_attribute(fmt::format("{}rsp.trace_tag", prefix).c_str(), ext->resp.is_trace_tag());
        }
    }
};

struct chi_snp_ext_recording : public tlm_extensions_recording_if<chi::chi_protocol_types> {

    chi_snp_ext_recording() { recordBegin = &recordBeginTx; }

    static void recordBeginTx(scv_tr_handle& handle, chi::chi_protocol_types::tlm_payload_type& trans) {
        tlm_extension_record_registry::get().recordBeginTx(chi_snp_extension::ID, handle, trans.get_extension<chi_snp_extension>(), "trans.chi_s.");
    }
};

struct chi_req_credit_ext_record : public tlm_extension_record_if {

    chi_req_credit_ext_record() {
        recordEnd = &record_end;
    }

    static void record_end(scv_tr_handle& handle, tlm::tlm_extension_base* e, std::string const& prefix) {
        if(auto ext = dynamic_cast<chi_credit_extension<credit_type_e::REQ>*>(e)) {
            handle.record_attribute(fmt::format("{}type", prefix).c_str(), "REQ");
            handle.record_attribute(fmt::format("{}count", prefix).c_str(), ext->count);
        }
    }
};

struct chi_resp_redit_ext_record : public tlm_extension_record_if {

    chi_resp_redit_ext_record() {
        recordEnd = &record_end;
    }

    static void record_end(scv_tr_handle& handle, tlm::tlm_extension_base* e, std::string const& prefix) {
        if(auto ext = dynamic_cast<chi_credit_extension<credit_type_e::RESP>*>(e)) {
            handle.record_attribute(fmt::format("{}type", prefix).c_str(), "RESP");
            handle.record_attribute(fmt::format("{}count", prefix).c_str(), ext->count);
        }
    }
};

struct chi_data_credit_ext_record : public tlm_extension_record_if {

    chi_data_credit_ext_record() {
        recordEnd = &record_end;
    }

    static void record_end(scv_tr_handle& handle, tlm::tlm_extension_base* e, std::string const& prefix) {
        if(auto ext = dynamic_cast<chi_credit_extension<credit_type_e::DATA>*>(e)) {
            handle.record_attribute(fmt::format("{}type", prefix).c_str(), "DATA");
            handle.record_attribute(fmt::format("{}count", prefix).c_str(), ext->count);
        }
    }
};

struct chi_credit_ext_recording : public tlm_extensions_recording_if<chi_protocol_types> {

    chi_credit_ext_recording() {
        recordEnd = &recordEndTx;
    }

    static void recordEndTx(scv_tr_handle& handle, chi_protocol_types::tlm_payload_type& trans) {
        if(auto ext = trans.get_extension<chi_credit_extension<credit_type_e::REQ>>())
            tlm_extension_record_registry::get().recordBeginTx(chi_credit_extension<credit_type_e::REQ>::ID, handle, ext, "trans.chi_req_crd.");
        else if(auto ext = trans.get_extension<chi_credit_extension<credit_type_e::RESP>>())
            tlm_extension_record_registry::get().recordBeginTx(chi_credit_extension<credit_type_e::RESP>::ID, handle, ext, "trans.chi_rsp_crd.");
        else if(auto ext = trans.get_extension<chi_credit_extension<credit_type_e::DATA>>())
            tlm_extension_record_registry::get().recordBeginTx(chi_credit_extension<credit_type_e::DATA>::ID, handle, ext, "trans.chi_dat_crd.");
    }
};

namespace scv {
using namespace tlm::scc::scv;
#if defined(__GNUG__)
__attribute__((constructor))
#endif
bool register_extensions() {
    tlm::scc::tlm_id_extension ext(nullptr);                                                        // NOLINT
    if(!tlm_extension_recording_registry<chi::chi_protocol_types>::get().is_ext_registered(ext.ID)) // NOLINT
        tlm_extension_recording_registry<chi::chi_protocol_types>::get().register_ext_rec(
            ext.ID,
            util::make_unique<tlm_id_ext_recording>()); // NOLINT
    /********************************************************************************************************************/
    chi::chi_ctrl_extension extchi_req;                                                                    // NOLINT
    if(!tlm_extension_recording_registry<chi::chi_protocol_types>::get().is_ext_registered(extchi_req.ID)) // NOLINT
        tlm_extension_recording_registry<chi::chi_protocol_types>::get().register_ext_rec(
            extchi_req.ID,
            util::make_unique<chi::chi_ctrl_ext_recording>()); // NOLINT
    if(!tlm_extension_record_registry::get().is_ext_registered(extchi_req.ID))
        tlm_extension_record_registry::get().register_ext_rec(extchi_req.ID, util::make_unique<chi::chi_ctrl_ext_record>()); // NOLINT
    /********************************************************************************************************************/
    chi::chi_data_extension extchi_data;                                                                    // NOLINT
    if(!tlm_extension_recording_registry<chi::chi_protocol_types>::get().is_ext_registered(extchi_data.ID)) // NOLINT
        tlm_extension_recording_registry<chi::chi_protocol_types>::get().register_ext_rec(
            extchi_data.ID,
            util::make_unique<chi::chi_data_ext_recording>()); // NOLINT
    if(!tlm_extension_record_registry::get().is_ext_registered(extchi_data.ID))
        tlm_extension_record_registry::get().register_ext_rec(extchi_data.ID, util::make_unique<chi::chi_data_ext_record>()); // NOLINT
    /********************************************************************************************************************/
    chi::chi_snp_extension extchi_snp;                                                                     // NOLINT
    if(!tlm_extension_recording_registry<chi::chi_protocol_types>::get().is_ext_registered(extchi_snp.ID)) // NOLINT
        tlm_extension_recording_registry<chi::chi_protocol_types>::get().register_ext_rec(
            extchi_snp.ID,
            util::make_unique<chi::chi_snp_ext_recording>()); // NOLINT
    if(!tlm_extension_record_registry::get().is_ext_registered(extchi_snp.ID))
        tlm_extension_record_registry::get().register_ext_rec(extchi_snp.ID,
                                                              util::make_unique<chi::chi_snp_ext_record>()); // NOLINT
    /********************************************************************************************************************/
    chi::chi_credit_extension<credit_type_e::REQ> extchi_req_credit;                                                                  // NOLINT
    if(!tlm_extension_recording_registry<chi::chi_protocol_types>::get().is_ext_registered(extchi_req_credit.ID)) // NOLINT
        tlm_extension_recording_registry<chi::chi_protocol_types>::get().register_ext_rec(
            extchi_req_credit.ID,
            util::make_unique<chi::chi_credit_ext_recording>()); // NOLINT
    if(!tlm_extension_record_registry::get().is_ext_registered(extchi_req_credit.ID))
        tlm_extension_record_registry::get().register_ext_rec(extchi_req_credit.ID,
                                                              util::make_unique<chi::chi_req_credit_ext_record>()); // NOLINT
    chi::chi_credit_extension<credit_type_e::REQ> extchi_resp_credit;                                                                  // NOLINT
    if(!tlm_extension_record_registry::get().is_ext_registered(extchi_resp_credit.ID))
        tlm_extension_record_registry::get().register_ext_rec(extchi_resp_credit.ID,
                                                              util::make_unique<chi::chi_resp_redit_ext_record>()); // NOLINT
    chi::chi_credit_extension<credit_type_e::REQ> extchi_data_credit;                                                                  // NOLINT
    if(!tlm_extension_record_registry::get().is_ext_registered(extchi_data_credit.ID))
        tlm_extension_record_registry::get().register_ext_rec(extchi_data_credit.ID,
                                                              util::make_unique<chi::chi_data_credit_ext_record>()); // NOLINT
    return true;                                                                                                // NOLINT
}
bool registered = register_extensions();
} // namespace scv
} // namespace chi
