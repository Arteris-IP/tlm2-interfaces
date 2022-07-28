/*
 * Copyright 2021 Arteris IP
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

#define SC_INCLUDE_DYNAMIC_PROCESSES
#include <atp/timing_params.h>
#include <axi/axi_tlm.h>
#include <cache/cache_info.h>
#include <chi/pe/chi_rn_initiator.h>
#include <scc/report.h>
#include <util/strprintf.h>

using namespace sc_core;
using namespace chi;
using namespace chi::pe;
using sem_lock = scc::ordered_semaphore::lock;

namespace {
uint8_t log2n(uint8_t siz) { return ((siz > 1) ? 1 + log2n(siz >> 1) : 0); }
inline uintptr_t to_id(tlm::tlm_generic_payload& t) { return reinterpret_cast<uintptr_t>(&t); }
inline uintptr_t to_id(tlm::tlm_generic_payload* t) { return reinterpret_cast<uintptr_t>(t); }
void convert_axi4ace_to_chi(tlm::tlm_generic_payload& gp, char const* name, bool legacy_mapping = false) {
    if(gp.get_data_length() > 64) {
        SCCWARN(__FUNCTION__) << "Data length of " << gp.get_data_length()
                              << " is not supported by CHI, shortening payload";
        gp.set_data_length(64);
    }
    auto ace_ext = gp.set_extension<axi::ace_extension>(nullptr);
    auto axi4_ext = gp.set_extension<axi::axi4_extension>(nullptr);

    // Check that either the AXI4 or ACE extension is set
    sc_assert(ace_ext != nullptr || axi4_ext != nullptr);

    bool is_ace = (ace_ext != nullptr);

    auto* chi_req_ext = new chi::chi_ctrl_extension;

    //---------- Map the fields in AXI4/ACE extension to CHI request extension
    // 1. Set SRC ID and TGT ID based on Address of transaction??  XXX: Currently hardcoded

    // Map AXI4/ACE ID to CHI TXN ID
    chi_req_ext->set_txn_id(is_ace ? ace_ext->get_id() : axi4_ext->get_id());
    chi_req_ext->set_qos(is_ace ? ace_ext->get_qos() : axi4_ext->get_qos());
    SCCTRACE(name) << "chi_ctrl_extension set TxnID=0x" << std::hex << chi_req_ext->get_txn_id();

    // XXX: Can gp.get_data_length() be mapped to CHI req 'size' field?
    sc_assert(((gp.get_data_length() & (gp.get_data_length() - 1)) == 0) &&
              "CHI data size is not a power of 2: Byte transfer: 0->1, 1->2, 2->4, 3->8, .. 6->64, 7->reserved");
    uint8_t chi_size = log2n(gp.get_data_length());
    SCCDEBUG(name) << "convert_axi4ace_to_chi: data length = " << gp.get_data_length()
                   << "; Converted data length to chi_size = " << static_cast<unsigned>(chi_size);

    chi_req_ext->req.set_size(chi_size);

    uint8_t mem_attr = 0; // CHI request field MemAttr

    // Set opcode based on snoop, bar and domain
    if(!is_ace) {
        // Non coherent
        sc_assert(gp.is_read() || gp.is_write()); // Read/Write, no cache maintenance
        chi_req_ext->req.set_opcode(gp.is_read() ? chi::req_optype_e::ReadNoSnp : chi::req_optype_e::WriteNoSnpFull);
    } else {

        auto axi_gp_cmd = gp.get_command();
        auto axi_snp = ace_ext->get_snoop();
        auto axi_domain = ace_ext->get_domain();
        auto axi_bar = ace_ext->get_barrier();
        auto axi_atomic = ace_ext->get_atop();
        // AN-573. If something is cacheable, then set, if something is not cacheable, then don't set snpattr.
        auto cacheable = ace_ext->is_modifiable();
        SCCDEBUG(name) << "AXI/ACE: snoop = " << axi::to_char(axi_snp) << ", barrier = " << axi::to_char(axi_bar)
                       << ", domain = " << axi::to_char(axi_domain);
        if(axi_bar == axi::bar_e::MEMORY_BARRIER) {
            sc_assert(axi_snp == axi::snoop_e::BARRIER);
            SCCERR(name) << "Barrier transaction has no mapping in CHI";
        }
        chi::req_optype_e opcode{chi::req_optype_e::ReqLCrdReturn};

        if(axi_atomic) {
            SCCDEBUG(name) << "AWATOP value: " << std::hex << static_cast<unsigned>(axi_atomic);
            auto atomic_opcode = (axi_atomic >> 4) & 3;
            auto atomic_subcode = axi_atomic & 7;

            if(atomic_opcode == 1) {
                const std::array<chi::req_optype_e, 8> atomic_store_opcodes = {
                    chi::req_optype_e::AtomicStoreAdd,  chi::req_optype_e::AtomicStoreClr,
                    chi::req_optype_e::AtomicStoreEor,  chi::req_optype_e::AtomicStoreSet,
                    chi::req_optype_e::AtomicStoreSmax, chi::req_optype_e::AtomicStoreSmin,
                    chi::req_optype_e::AtomicStoreUmax, chi::req_optype_e::AtomicStoreUmin};
                opcode = atomic_store_opcodes[atomic_subcode];
            } else if(atomic_opcode == 2) {
                const std::array<chi::req_optype_e, 8> atomic_load_opcodes = {
                    chi::req_optype_e::AtomicLoadAdd,  chi::req_optype_e::AtomicLoadClr,
                    chi::req_optype_e::AtomicLoadEor,  chi::req_optype_e::AtomicLoadSet,
                    chi::req_optype_e::AtomicLoadSmax, chi::req_optype_e::AtomicLoadSmin,
                    chi::req_optype_e::AtomicLoadUmax, chi::req_optype_e::AtomicLoadUmin};
                opcode = atomic_load_opcodes[atomic_subcode];
            } else if(axi_atomic == 0x30)
                opcode = chi::req_optype_e::AtomicSwap;
            else if(axi_atomic == 0x31)
                opcode = chi::req_optype_e::AtomicCompare;
            else
                SCCERR(name) << "Can't handle AXI AWATOP value: " << axi_atomic;

            chi_req_ext->req.set_opcode(opcode);
            chi_req_ext->req.set_snp_attr(axi_snp != axi::snoop_e::READ_NO_SNOOP);
            chi_req_ext->req.set_snoop_me(axi_snp != axi::snoop_e::READ_NO_SNOOP);
        } else if(gp.is_read()) {
            switch(axi_snp) {
            case axi::snoop_e::READ_NO_SNOOP:
                sc_assert(axi_domain == axi::domain_e::NON_SHAREABLE || axi_domain == axi::domain_e::SYSTEM);
                opcode = chi::req_optype_e::ReadNoSnp;
                break;
            case axi::snoop_e::READ_ONCE:
                sc_assert(axi_domain == axi::domain_e::INNER_SHAREABLE || axi_domain == axi::domain_e::OUTER_SHAREABLE);
                opcode = chi::req_optype_e::ReadOnce;
                chi_req_ext->req.set_snp_attr(cacheable);
                break;
            case axi::snoop_e::READ_SHARED:
                opcode = chi::req_optype_e::ReadShared;
                break;
            case axi::snoop_e::READ_CLEAN:
                opcode = chi::req_optype_e::ReadClean;
                break;
            case axi::snoop_e::READ_NOT_SHARED_DIRTY:
                opcode = chi::req_optype_e::ReadNotSharedDirty;
                break;
            case axi::snoop_e::READ_UNIQUE:
                opcode = chi::req_optype_e::ReadUnique;
                break;
            case axi::snoop_e::CLEAN_SHARED:
                opcode = chi::req_optype_e::CleanShared;
                gp.set_command(tlm::TLM_IGNORE_COMMAND);
                gp.set_data_length(0);
                break;
            case axi::snoop_e::CLEAN_INVALID:
                opcode = chi::req_optype_e::CleanInvalid;
                gp.set_command(tlm::TLM_IGNORE_COMMAND);
                gp.set_data_length(0);
                break;
            case axi::snoop_e::CLEAN_SHARED_PERSIST:
                opcode = chi::req_optype_e::CleanSharedPersist;
                gp.set_command(tlm::TLM_IGNORE_COMMAND);
                gp.set_data_length(0);
                break;
            case axi::snoop_e::CLEAN_UNIQUE:
                opcode = chi::req_optype_e::CleanUnique;
                gp.set_command(tlm::TLM_IGNORE_COMMAND);
                gp.set_data_length(0);
                break;
            case axi::snoop_e::MAKE_UNIQUE:
                opcode = chi::req_optype_e::MakeUnique;
                gp.set_command(tlm::TLM_IGNORE_COMMAND);
                gp.set_data_length(0);
                break;
            case axi::snoop_e::MAKE_INVALID:
                opcode = chi::req_optype_e::MakeInvalid;
                gp.set_command(tlm::TLM_IGNORE_COMMAND);
                gp.set_data_length(0);
                break;
            default:
                SCCWARN(name) << "unexpected read type";
                break;
            }
            chi_req_ext->req.set_opcode(opcode);

            if(axi_snp != axi::snoop_e::READ_NO_SNOOP) {
                chi_req_ext->req.set_snp_attr(cacheable);
            }
            if(opcode == chi::req_optype_e::StashOnceUnique || opcode == chi::req_optype_e::StashOnceShared) {
                gp.set_data_length(0);
                gp.set_command(tlm::TLM_IGNORE_COMMAND);
                if(ace_ext->is_stash_nid_en()) {
                    chi_req_ext->req.set_stash_n_id(ace_ext->get_stash_nid());
                    chi_req_ext->req.set_stash_n_id_valid(true);
                }
                if(ace_ext->is_stash_lpid_en()) {
                    chi_req_ext->req.set_stash_lp_id(ace_ext->get_stash_lpid());
                    chi_req_ext->req.set_stash_lp_id_valid(true);
                }
            }
        } else if(gp.is_write()) {
            switch(axi_snp) {
            case axi::snoop_e::WRITE_NO_SNOOP:
                sc_assert(axi_domain == axi::domain_e::NON_SHAREABLE || axi_domain == axi::domain_e::SYSTEM);
                opcode = chi::req_optype_e::WriteNoSnpFull;
                if(gp.get_data_length() < 64)
                    opcode = chi::req_optype_e::WriteNoSnpPtl;
                break;
            case axi::snoop_e::WRITE_UNIQUE:
                sc_assert(axi_domain == axi::domain_e::INNER_SHAREABLE || axi_domain == axi::domain_e::OUTER_SHAREABLE);
                opcode = chi::req_optype_e::WriteUniquePtl;
                chi_req_ext->req.set_snp_attr(cacheable);
                break;
            case axi::snoop_e::WRITE_LINE_UNIQUE:
                opcode = chi::req_optype_e::WriteUniqueFull;
                break;
            case axi::snoop_e::WRITE_CLEAN: {
                bool ptl = false;
                for(auto i = 0; i < gp.get_byte_enable_length(); ++i) {
                    if(gp.get_byte_enable_ptr()[i] == 0) {
                        ptl = true;
                        break;
                    }
                }
                if(ptl)
                    opcode = chi::req_optype_e::WriteCleanPtl;
                else
                    opcode = chi::req_optype_e::WriteCleanFull;
                break;
            }
            case axi::snoop_e::WRITE_BACK:
                opcode =
                    gp.get_data_length() == 64 ? chi::req_optype_e::WriteBackFull : chi::req_optype_e::WriteBackPtl;
                break;
            case axi::snoop_e::EVICT:
                opcode = chi::req_optype_e::Evict;
                break;
            case axi::snoop_e::WRITE_EVICT:
                opcode = chi::req_optype_e::WriteEvictFull;
                break;
            case axi::snoop_e::WRITE_UNIQUE_PTL_STASH:
                opcode = chi::req_optype_e::WriteUniquePtlStash;
                break;
            case axi::snoop_e::WRITE_UNIQUE_FULL_STASH:
                opcode = chi::req_optype_e::WriteUniqueFullStash;
                break;
            case axi::snoop_e::STASH_ONCE_UNIQUE:
                opcode = chi::req_optype_e::StashOnceUnique;
                gp.set_command(tlm::TLM_IGNORE_COMMAND);
                gp.set_data_length(0);
                break;
            case axi::snoop_e::STASH_ONCE_SHARED:
                opcode = chi::req_optype_e::StashOnceShared;
                gp.set_command(tlm::TLM_IGNORE_COMMAND);
                gp.set_data_length(0);
                break;
            default:
                SCCWARN(name) << "unexpected snoop type " << axi::to_char(axi_snp) << " during write";
                break;
            }
            chi_req_ext->req.set_opcode(opcode);

            if(axi_snp != axi::snoop_e::WRITE_NO_SNOOP) {
                chi_req_ext->req.set_snp_attr(cacheable);
            }
            if(opcode == chi::req_optype_e::WriteUniquePtlStash || opcode == chi::req_optype_e::WriteUniqueFullStash ||
               opcode == chi::req_optype_e::StashOnceUnique || opcode == chi::req_optype_e::StashOnceShared) {
                if(ace_ext->is_stash_nid_en()) {
                    chi_req_ext->req.set_stash_n_id(ace_ext->get_stash_nid());
                    chi_req_ext->req.set_stash_n_id_valid(true);
                }
                if(ace_ext->is_stash_lpid_en()) {
                    chi_req_ext->req.set_stash_lp_id(ace_ext->get_stash_lpid());
                    chi_req_ext->req.set_stash_lp_id_valid(true);
                }
            }
        } else {
            // Must be Cache maintenance. XXX: To do
            SCCERR(name) << "Not yet implemented !!! ";
        }

        if(legacy_mapping) {
            /// AxCACHE mapped on Memory Attributes as specified here:
            /// https://confluence.arteris.com/display/ENGR/Model+Verification Device Storage Type (ST) Allocate
            /// Allocate (AC) Cacheable Cacheable (CA) EWA !Visible (VZ) From Table 8 Ncore3SysArch.pdf ST => Device
            /// (bit 1) CA => Cacheable (bit 2) AC => Allocate (bit 3) VZ => !EWA  (bit 0) AC,CA,ST,EWA AxCACHE[3:0] ST
            /// CA  AC  VZ  CH  Order[1:0]  Transactions Used       MemAttr[3:0] 0000 Read/Write 1   0   0   1   0   11
            /// CmdRdNC, CmdWrNC        0010 0001 Read/Write 1   0   0   0   0   11          CmdRdNC, CmdWrNC 0011 0010
            /// Read/Write 0   0   0   1   0   10          CmdRdNC, CmdWrNC        0000 0011 Read/Write 0   0   0   0 0
            /// 10          CmdRdNC, CmdWrNC        0001 0110 Read       0   1   1   0   1   10          ReadNITC 1101
            /// 0110 Write      0   1   0   0   1   10          WriteUnique             0101
            /// 0111 Read       0   1   1   0   1   10          ReadNITC                1101
            /// 0110 Write      0   1   0   0   1   10          WriteUnique             0101
            /// 1010 Read       0   1   0   0   1   10          ReadNITC                0101
            /// 1010 Write      0   1   1   0   1   10          WriteUnique             1101
            /// 1011 Read       0   1   0   0   1   10          ReadNITC                0101
            /// 1011 Write      0   1   1   0   1   10          WriteUnique             1101
            /// 1110 Read/Write 0   1   1   0   1   10          ReadNITC/WriteUnique    1101
            /// 1111 Read/Write 0   1   1   0   1   10          ReadNITC/WriteUnique    1101
            //
            //      Cache Read Write
            //       0000 0010               Read/Write
            //       0001 0011               Read/Write
            //       0010 0000               Read/Write
            //       0011 0001               Read/Write
            //       0110 1101 0101          Read Write
            //       0111 1101 0101          Read Write
            //       1010 0101 1101          Read Write
            //       1011 0101 1101          Read Write
            //       1110 1101               Read/Write
            //       1111 1101               Read/Write

            switch(ace_ext->get_cache()) {
            case 0b0000:
                mem_attr = 0b0010;
                break;
            case 0b0001:
                mem_attr = 0b0011;
                break;
            case 0b0010:
                mem_attr = 0b0000;
                break;
            case 0b0011:
                mem_attr = 0b0001;
                break;
            case 0b0110:
            case 0b0111:
                mem_attr = gp.is_read() ? 0b1101 : 0b0101;
                break;
            case 0b1010:
            case 0b1011:
                mem_attr = gp.is_read() ? 0b0101 : 0b1101;
                break;
            case 0b1110:
            case 0b1111:
                mem_attr = 0b1101;
                break;
            default:
                SCCERR(name) << "Unexpected AxCACHE type";
                break;
            }
        } else {
            auto allocate = (ace_ext->is_read_other_allocate() && axi_gp_cmd == tlm::TLM_WRITE_COMMAND) ||
                            (ace_ext->is_write_other_allocate() && axi_gp_cmd == tlm::TLM_READ_COMMAND);
            auto cachable = ace_ext->is_modifiable();
            auto ewa = ace_ext->is_bufferable();
            auto device = ace_ext->get_cache() < 2;
            mem_attr = (allocate ? 8 : 0) + (cachable ? 4 : 0) + (device ? 2 : 0) + (ewa ? 1 : 0);
            // ReadNoSnp., ReadNoSnpSep., ReadOnce*., WriteNoSnp., WriteNoSnp, CMO., WriteNoSnpZero., WriteUnique.,
            // WriteUnique, CMO., WriteUniqueZero., Atomic.
            if(!device)
                switch(opcode) {
                case chi::req_optype_e::ReadNoSnp:
                case chi::req_optype_e::ReadNoSnpSep:
                case chi::req_optype_e::ReadOnce:
                case chi::req_optype_e::ReadOnceCleanInvalid:
                case chi::req_optype_e::ReadOnceMakeInvalid:
                case chi::req_optype_e::WriteNoSnpPtl:
                case chi::req_optype_e::WriteNoSnpFull:
                case chi::req_optype_e::WriteUniquePtl:
                case chi::req_optype_e::WriteUniqueFull:
                case chi::req_optype_e::AtomicStoreAdd:
                case chi::req_optype_e::AtomicStoreClr:
                case chi::req_optype_e::AtomicStoreEor:
                case chi::req_optype_e::AtomicStoreSet:
                case chi::req_optype_e::AtomicStoreSmax:
                case chi::req_optype_e::AtomicStoreSmin:
                case chi::req_optype_e::AtomicStoreUmax:
                case chi::req_optype_e::AtomicStoreUmin:
                case chi::req_optype_e::AtomicLoadAdd:
                case chi::req_optype_e::AtomicLoadClr:
                case chi::req_optype_e::AtomicLoadEor:
                case chi::req_optype_e::AtomicLoadSet:
                case chi::req_optype_e::AtomicLoadSmax:
                case chi::req_optype_e::AtomicLoadSmin:
                case chi::req_optype_e::AtomicLoadUmax:
                case chi::req_optype_e::AtomicLoadUmin:
                case chi::req_optype_e::AtomicSwap:
                case chi::req_optype_e::AtomicCompare:
                    chi_req_ext->req.set_order(0b10);
                    break;
                default:
                    break;
                }
        }
    }
    chi_req_ext->req.set_mem_attr(mem_attr);

    if(!chi::is_valid(chi_req_ext))
        SCCFATAL(__FUNCTION__) << "Conversion created an invalid chi request, pls. check the AXI/ACE settings";

    if(gp.has_mm())
        gp.set_auto_extension(chi_req_ext);
    else {
        gp.set_extension(chi_req_ext);
    }
    delete ace_ext;
    ace_ext = nullptr;
    delete axi4_ext;
    axi4_ext = nullptr;
    gp.set_extension(ace_ext);
    gp.set_extension(axi4_ext);
}

void setExpCompAck(chi::chi_ctrl_extension* const req_e) {
    switch(req_e->req.get_opcode()) {
    // Ref : Sec 2.8.3 pg 97
    case chi::req_optype_e::ReadNoSnpSep: // Ref: Pg 143
                                          // Ref : Table 2.7 pg 55
    case chi::req_optype_e::Evict:
    case chi::req_optype_e::StashOnceUnique:
    case chi::req_optype_e::StashOnceShared:
    case chi::req_optype_e::CleanShared:
    case chi::req_optype_e::CleanSharedPersist:
    case chi::req_optype_e::CleanInvalid:
    case chi::req_optype_e::MakeInvalid:
        // write case
    case chi::req_optype_e::WriteNoSnpFull:
    case chi::req_optype_e::WriteNoSnpPtl:
    case chi::req_optype_e::WriteUniquePtl:
    case chi::req_optype_e::WriteUniqueFull:
    case chi::req_optype_e::WriteUniqueFullStash:
    case chi::req_optype_e::WriteBackFull:
    case chi::req_optype_e::WriteBackPtl:
    case chi::req_optype_e::WriteCleanFull:
    case chi::req_optype_e::WriteCleanPtl:
        // Atomics
    case chi::req_optype_e::AtomicStoreAdd:
    case chi::req_optype_e::AtomicStoreClr:
    case chi::req_optype_e::AtomicStoreEor:
    case chi::req_optype_e::AtomicStoreSet:
    case chi::req_optype_e::AtomicStoreSmax:
    case chi::req_optype_e::AtomicStoreSmin:
    case chi::req_optype_e::AtomicStoreUmax:
    case chi::req_optype_e::AtomicStoreUmin:
    case chi::req_optype_e::AtomicLoadAdd:
    case chi::req_optype_e::AtomicLoadClr:
    case chi::req_optype_e::AtomicLoadEor:
    case chi::req_optype_e::AtomicLoadSet:
    case chi::req_optype_e::AtomicLoadSmax:
    case chi::req_optype_e::AtomicLoadSmin:
    case chi::req_optype_e::AtomicLoadUmax:
    case chi::req_optype_e::AtomicLoadUmin:
    case chi::req_optype_e::AtomicCompare:
    case chi::req_optype_e::AtomicSwap:

        // case chi::req_optype_e::ReadNoSnp:  //XXX: Temporary for testing
        // case chi::req_optype_e::ReadOnce:  //XXX: Temporary for testing
        req_e->req.set_exp_comp_ack(false);
        break;

        // Ref : pg 55
    case chi::req_optype_e::ReadNoSnp:
    case chi::req_optype_e::ReadOnce:
    case chi::req_optype_e::CleanUnique:
    case chi::req_optype_e::MakeUnique:
        req_e->req.set_exp_comp_ack(true);
        break;
    default: // XXX: Should default ExpCompAck be true or false?
        req_e->req.set_exp_comp_ack(true);
        break;
    }

    // XXX: For Ordered Read, set ExpCompAck. Check once again, not clear
    if((req_e->req.get_opcode() == chi::req_optype_e::ReadNoSnp ||
        req_e->req.get_opcode() == chi::req_optype_e::ReadOnce) &&
       (req_e->req.get_order() == 0b10 || req_e->req.get_order() == 0b11)) {
        req_e->req.set_exp_comp_ack(true);
    }

    // Ref pg 101: Ordered write => set ExpCompAck (XXX: Check if its true for all Writes)
    if((req_e->req.get_opcode() >= chi::req_optype_e::WriteEvictFull &&
        req_e->req.get_opcode() <= chi::req_optype_e::WriteUniquePtlStash) &&
       (req_e->req.get_order() == 0b10 || req_e->req.get_order() == 0b11)) {
        req_e->req.set_exp_comp_ack(true);
    }
}

bool make_rsp_from_req(tlm::tlm_generic_payload& gp, chi::rsp_optype_e rsp_opcode) {
    if(auto* ctrl_e = gp.get_extension<chi::chi_ctrl_extension>()) {
        ctrl_e->resp.set_opcode(rsp_opcode);
        if(rsp_opcode == chi::rsp_optype_e::CompAck) {
            if(is_dataless(ctrl_e) || gp.is_write()) {
                ctrl_e->resp.set_tgt_id(ctrl_e->req.get_tgt_id());
                ctrl_e->resp.set_trace_tag(ctrl_e->req.is_trace_tag()); // XXX ??
                return true;
            } else {
                auto dat_e = gp.get_extension<chi::chi_data_extension>();
                ctrl_e->set_src_id(dat_e->get_src_id());
                ctrl_e->set_qos(dat_e->get_qos());
                ctrl_e->set_txn_id(dat_e->dat.get_db_id());
                ctrl_e->resp.set_tgt_id(dat_e->dat.get_tgt_id());
                ctrl_e->resp.set_trace_tag(dat_e->dat.is_trace_tag()); // XXX ??
                return true;
            }
        }
    } else if(auto* snp_e = gp.get_extension<chi::chi_snp_extension>()) {
        snp_e->resp.set_opcode(rsp_opcode);
        if(rsp_opcode == chi::rsp_optype_e::CompAck) {
            auto dat_e = gp.get_extension<chi::chi_data_extension>();
            snp_e->set_src_id(dat_e->get_src_id());
            snp_e->set_qos(dat_e->get_qos());
            snp_e->set_txn_id(dat_e->dat.get_db_id());
            snp_e->resp.set_tgt_id(dat_e->dat.get_tgt_id());
            snp_e->resp.set_trace_tag(dat_e->dat.is_trace_tag()); // XXX ??
            return true;
        }
    }
    return false;
}

} // anonymous namespace

SC_HAS_PROCESS(chi_rn_initiator_b);

chi::pe::chi_rn_initiator_b::chi_rn_initiator_b(sc_core::sc_module_name nm,
                                                sc_core::sc_port_b<chi::chi_fw_transport_if<chi_protocol_types>>& port,
                                                size_t transfer_width)
: sc_module(nm)
, socket_fw(port)
, transfer_width_in_bytes(transfer_width / 8) {
    add_attribute(home_node_id);
    add_attribute(src_id);
    add_attribute(data_interleaving);
    add_attribute(strict_income_order);
    add_attribute(use_legacy_mapping);

    SC_METHOD(clk_counter);
    sensitive << clk_i.pos();

    SC_THREAD(snoop_dispatch);
}

chi::pe::chi_rn_initiator_b::~chi_rn_initiator_b() {
    if(tx_state_by_trans.size())
        SCCERR(SCMOD) << "is still waiting for unfinished transactions";
    for(auto& e : tx_state_by_trans)
        delete e.second;
}

void chi::pe::chi_rn_initiator_b::b_snoop(payload_type& trans, sc_core::sc_time& t) {
    if(snoop_cb) {
        auto latency = (*snoop_cb)(trans);
        if(latency < std::numeric_limits<unsigned>::max())
            t += latency * (clk_if ? clk_if->period() : clk_period);
    }
}

void chi::pe::chi_rn_initiator_b::snoop_resp(payload_type& trans, bool sync) {
    auto req_ext = trans.get_extension<chi_snp_extension>();
    sc_assert(req_ext != nullptr);
    auto it = tx_state_by_trans.find(to_id(trans));
    sc_assert(it != tx_state_by_trans.end());
    auto* txs = it->second;
    handle_snoop_response(trans, txs);
}

tlm::tlm_sync_enum chi::pe::chi_rn_initiator_b::nb_transport_bw(payload_type& trans, phase_type& phase,
                                                                sc_core::sc_time& t) {
    if(auto snp_ext = trans.get_extension<chi_snp_extension>()) {
        if(phase == tlm::BEGIN_REQ) {
            if(trans.has_mm())
                trans.acquire();
            snp_peq.notify(trans, t);
        } else {
            auto it = tx_state_by_trans.find(to_id(trans));
            sc_assert(it != tx_state_by_trans.end());
            it->second->peq.notify(std::make_tuple(&trans, phase), t);
        }
    } else {
        if(phase == tlm::BEGIN_REQ) {
            if(auto credit_ext = trans.get_extension<chi_credit_extension>()) {
                if(credit_ext->type == credit_type_e::REQ) {
                    SCCTRACEALL(SCMOD) << "Received " << credit_ext->count << " req "
                                       << (credit_ext->count == 1 ? "credit" : "credits");
                    for(auto i = 0U; i < credit_ext->count; ++i)
                        req_credits.post();
                }
                phase = tlm::END_RESP;
                trans.set_response_status(tlm::TLM_OK_RESPONSE);
                if(clk_if)
                    t += clk_if->period() - 1_ps;
                return tlm::TLM_COMPLETED;
            } else {
                SCCFATAL(SCMOD) << "Illegal transaction received from HN";
            }
        } else {
            auto it = tx_state_by_trans.find(to_id(trans));
            sc_assert(it != tx_state_by_trans.end());
            it->second->peq.notify(std::make_tuple(&trans, phase), t);
        }
    }
    return tlm::TLM_ACCEPTED;
}

void chi::pe::chi_rn_initiator_b::invalidate_direct_mem_ptr(sc_dt::uint64 start_range, sc_dt::uint64 end_range) {}

void chi::pe::chi_rn_initiator_b::update_data_extension(chi::chi_data_extension* data_ext, payload_type& trans) {
    auto req_e = trans.get_extension<chi::chi_ctrl_extension>();
    sc_assert(req_e != nullptr);
    switch(req_e->req.get_opcode()) {
    case chi::req_optype_e::WriteNoSnpFull:
    case chi::req_optype_e::WriteNoSnpPtl:
    case chi::req_optype_e::WriteUniquePtl:
    case chi::req_optype_e::WriteUniqueFull:
    case chi::req_optype_e::WriteUniquePtlStash:
    case chi::req_optype_e::WriteUniqueFullStash:
        data_ext->dat.set_opcode(chi::dat_optype_e::NonCopyBackWrData);
        break;
    case chi::req_optype_e::WriteBackFull:
    case chi::req_optype_e::WriteBackPtl:
    case chi::req_optype_e::WriteCleanFull:
    case chi::req_optype_e::WriteCleanPtl:
    case chi::req_optype_e::WriteEvictFull:
        data_ext->dat.set_opcode(chi::dat_optype_e::CopyBackWrData);
        break;
    case chi::req_optype_e::AtomicStoreAdd:
    case chi::req_optype_e::AtomicStoreClr:
    case chi::req_optype_e::AtomicStoreEor:
    case chi::req_optype_e::AtomicStoreSet:
    case chi::req_optype_e::AtomicStoreSmax:
    case chi::req_optype_e::AtomicStoreSmin:
    case chi::req_optype_e::AtomicStoreUmax:
    case chi::req_optype_e::AtomicStoreUmin:
        data_ext->dat.set_opcode(chi::dat_optype_e::NonCopyBackWrData);
        break;
    case chi::req_optype_e::AtomicLoadAdd:
    case chi::req_optype_e::AtomicLoadClr:
    case chi::req_optype_e::AtomicLoadEor:
    case chi::req_optype_e::AtomicLoadSet:
    case chi::req_optype_e::AtomicLoadSmax:
    case chi::req_optype_e::AtomicLoadSmin:
    case chi::req_optype_e::AtomicLoadUmax:
    case chi::req_optype_e::AtomicLoadUmin:
    case chi::req_optype_e::AtomicSwap:
    case chi::req_optype_e::AtomicCompare:
        data_ext->dat.set_opcode(chi::dat_optype_e::NonCopyBackWrData);
        break;
    default:
        SCCWARN(SCMOD) << " Unable to match req_opcode with data_opcode in write transaction ";
    }
    if(data_ext->dat.get_opcode() == chi::dat_optype_e::NonCopyBackWrData) {
        data_ext->dat.set_resp(chi::dat_resptype_e::NonCopyBackWrData);
    } else if(data_ext->dat.get_opcode() == chi::dat_optype_e::NCBWrDataCompAck) {
        data_ext->dat.set_resp(chi::dat_resptype_e::NCBWrDataCompAck);
    } else if(data_ext->dat.get_opcode() == chi::dat_optype_e::CopyBackWrData) {
        auto cache_ext = trans.get_extension<::cache::cache_info>();
        sc_assert(cache_ext != nullptr);
        auto cache_state = cache_ext->get_state();
        if(cache_state == ::cache::state::IX) {
            data_ext->dat.set_resp(chi::dat_resptype_e::CopyBackWrData_I);
        } else if(cache_state == ::cache::state::UC) {
            data_ext->dat.set_resp(chi::dat_resptype_e::CopyBackWrData_UC);
        } else if(cache_state == ::cache::state::SC) {
            data_ext->dat.set_resp(chi::dat_resptype_e::CopyBackWrData_SC);
        } else if(cache_state == ::cache::state::UD) {
            data_ext->dat.set_resp(chi::dat_resptype_e::CopyBackWrData_UD_PD);
        } else if(cache_state == ::cache::state::SD) {
            data_ext->dat.set_resp(chi::dat_resptype_e::CopyBackWrData_SD_PD);
        } else
            SCCWARN(SCMOD) << " Unable to match cache state with resptype ";
    } else {
        SCCWARN(SCMOD) << "Unable to match resptype with  WriteData Responses";
    }

    auto db_id = req_e->resp.get_db_id();
    data_ext->set_txn_id(db_id);
    data_ext->set_src_id(req_e->resp.get_tgt_id());
    data_ext->dat.set_tgt_id(req_e->get_src_id());
}

void chi::pe::chi_rn_initiator_b::create_data_ext(payload_type& trans) {
    auto data_ext = new chi::chi_data_extension;
    update_data_extension(data_ext, trans);
    trans.set_auto_extension<chi::chi_data_extension>(data_ext);
}

void chi::pe::chi_rn_initiator_b::send_packet(tlm::tlm_phase phase, payload_type& trans,
                                              chi::pe::chi_rn_initiator_b::tx_state* txs) {
    sc_core::sc_time delay = sc_core::SC_ZERO_TIME;
    tlm::tlm_sync_enum ret = socket_fw->nb_transport_fw(trans, phase, delay);
    if(ret == tlm::TLM_UPDATED) {
        if(phase == chi::END_PARTIAL_DATA || phase == chi::END_DATA) {
            if(delay.value())
                wait(delay);
        }
    } else {
        auto entry = txs->peq.get();
        sc_assert(std::get<0>(entry) == &trans &&
                  (std::get<1>(entry) == chi::END_PARTIAL_DATA || std::get<1>(entry) == chi::END_DATA));
    }
    auto timing_e = trans.get_extension<atp::timing_params>();
    auto delay_in_cycles = (timing_e && timing_e->wbv) ? timing_e->wbv : 1;
    while(delay_in_cycles) {
        delay_in_cycles--;
        wait(clk_i.posedge_event());
    }
}

void chi::pe::chi_rn_initiator_b::send_wdata(payload_type& trans, chi::pe::chi_rn_initiator_b::tx_state* txs) {
    sc_core::sc_time delay;
    tlm::tlm_phase phase;
    auto data_ext = trans.get_extension<chi::chi_data_extension>();
    if(data_ext == nullptr) {
        create_data_ext(trans);
        data_ext = trans.get_extension<chi::chi_data_extension>();
    }

    auto beat_cnt = calculate_beats(trans);
    SCCDEBUG(SCMOD) << "Starting transaction on channel WDAT : (opcode, cmd, addr, len) = ("
                    << to_char(data_ext->dat.get_opcode()) << ", " << trans.get_command() << ", " << std::hex
                    << trans.get_address() << ", " << trans.get_data_length() << ")";

    if(!data_interleaving.value) {
        sem_lock lck(wdat_chnl);
        for(auto i = 0U; i < beat_cnt; ++i) {
            if(i < beat_cnt - 1)
                phase = chi::BEGIN_PARTIAL_DATA;
            else
                phase = chi::BEGIN_DATA;
            send_packet(phase, trans, txs);
            SCCTRACE(SCMOD) << "WDAT flit with txnid " << data_ext->cmn.get_txn_id() << " sent. Beat count: " << i
                            << ", addr: 0x" << std::hex << trans.get_address() << ", last=" << (i == (beat_cnt - 1));
        }
    } else { // data packet interleaving allowed
        for(auto i = 0U; i < beat_cnt; ++i) {
            {
                sem_lock lck(wdat_chnl);
                if(i < beat_cnt - 1)
                    phase = chi::BEGIN_PARTIAL_DATA;
                else
                    phase = chi::BEGIN_DATA;
                send_packet(phase, trans, txs);
                SCCTRACE(SCMOD) << "WDAT flit with txnid " << data_ext->cmn.get_txn_id() << " sent. Beat count: " << i
                                << ", addr: 0x" << std::hex << trans.get_address()
                                << ", last=" << (i == (beat_cnt - 1));
            }
            wait(SC_ZERO_TIME); // yield execution to allow others to lock
        }
    }
}

void chi::pe::chi_rn_initiator_b::send_comp_ack(payload_type& trans, tx_state*& txs) {
    if(make_rsp_from_req(trans, chi::rsp_optype_e::CompAck)) {
        sem_lock lck(sresp_chnl);
        SCCDEBUG(SCMOD) << "Send the CompAck response on SRSP channel, addr: 0x" << std::hex << trans.get_address();
        tlm::tlm_phase phase = chi::ACK;
        auto delay = SC_ZERO_TIME;
        auto ret = socket_fw->nb_transport_fw(trans, phase, delay);
        if(ret == tlm::TLM_UPDATED && phase == chi::ACK) {
            if(delay.value())
                wait(delay);
        } else {
            auto entry = txs->peq.get();
            sc_assert(std::get<0>(entry) == &trans && std::get<1>(entry) == tlm::END_RESP);
        }
        wait(clk_i.posedge_event()); // sync to clock before releasing resource
    }
}

void chi::pe::chi_rn_initiator_b::exec_read_write_protocol(const unsigned int txn_id, payload_type& trans,
                                                           chi::pe::chi_rn_initiator_b::tx_state*& txs) {
    // TODO: in write case CAIU does not send BEGIN_RESP;
    sc_core::sc_time delay;
    auto not_finish = 0b11U; // bit0: data is ongoing, bit1: ctrl resp. is ongoing
    auto exp_beat_cnt = calculate_beats(trans);
    auto beat_cnt = 0U;
    while(not_finish) {
        // waiting for response
        auto entry = txs->peq.get();
        sc_assert(std::get<0>(entry) == &trans);
        auto phase = std::get<1>(entry);
        if(phase == tlm::BEGIN_RESP) {
            cresp_response(trans);
            not_finish &= 0x1; // clear bit1
            auto resp_ext = trans.get_extension<chi::chi_ctrl_extension>();
            if(trans.is_write() && (resp_ext->resp.get_opcode() == chi::rsp_optype_e::DBIDResp ||
                                    resp_ext->resp.get_opcode() == chi::rsp_optype_e::CompDBIDResp)) {
                send_wdata(trans, txs);
                not_finish &= 0x2; // clear bit0
            } else if(chi::is_dataless(resp_ext) && resp_ext->resp.get_opcode() == chi::rsp_optype_e::Comp &&
                      (resp_ext->resp.get_resp() == chi::rsp_resptype_e::Comp_I ||
                       resp_ext->resp.get_resp() == chi::rsp_resptype_e::Comp_UC ||
                       resp_ext->resp.get_resp() ==
                           chi::rsp_resptype_e::Comp_SC)) { // Response to dataless makeUnique request
                not_finish &= 0x2;                          // clear bit0
            }
        } else if(trans.is_read() && (phase == chi::BEGIN_PARTIAL_DATA || phase == chi::BEGIN_DATA)) {
            SCCTRACE(SCMOD) << "RDAT flit received. Beat count: " << beat_cnt << ", addr: 0x" << std::hex
                            << trans.get_address();
            not_finish &= 0x1; // clear bit1
            if(phase == chi::BEGIN_PARTIAL_DATA)
                phase = chi::END_PARTIAL_DATA;
            else
                phase = chi::END_DATA;
            delay = clk_if ? clk_if->period() - 1_ps : SC_ZERO_TIME;
            socket_fw->nb_transport_fw(trans, phase, delay);
            beat_cnt++;
            if(phase == chi::END_DATA) {
                not_finish &= 0x2; // clear bit0
                if(beat_cnt != exp_beat_cnt)
                    SCCERR(SCMOD) << "Wrong beat count, expected " << exp_beat_cnt << ", got " << beat_cnt;
            }

        } else {
            SCCFATAL(SCMOD) << "Illegal protocol state (maybe just not implemented?)";
        }
    }
}

void chi::pe::chi_rn_initiator_b::cresp_response(payload_type& trans) {
    auto resp_ext = trans.get_extension<chi::chi_ctrl_extension>();
    sc_assert(resp_ext != nullptr);
    auto id = (unsigned)(resp_ext->get_txn_id());
    SCCDEBUG(SCMOD) << "got cresp: src_id=" << (unsigned)resp_ext->get_src_id()
                    << ", tgt_id=" << (unsigned)resp_ext->resp.get_tgt_id() << ", "
                    << "txnid=0x" << std::hex << id << ", " << to_char(resp_ext->resp.get_opcode())
                    << ", db_id=" << (unsigned)resp_ext->resp.get_db_id() << ", addr=0x" << std::hex
                    << trans.get_address() << ")";
    tlm::tlm_phase phase = tlm::END_RESP;
    sc_core::sc_time delay = clk_if ? clk_if->period() - 1_ps : SC_ZERO_TIME;
    socket_fw->nb_transport_fw(trans, phase, delay);
    wait(clk_i.posedge_event());
}

void chi::pe::chi_rn_initiator_b::exec_atomic_protocol(const unsigned int txn_id, payload_type& trans,
                                                       chi::pe::chi_rn_initiator_b::tx_state*& txs) {
    sc_core::sc_time delay;
    // waiting for response
    auto entry = txs->peq.get();
    sc_assert(std::get<0>(entry) == &trans);
    auto phase = std::get<1>(entry);
    if(phase == tlm::BEGIN_RESP) {
        cresp_response(trans);
        auto resp_ext = trans.get_extension<chi::chi_ctrl_extension>();
        if(resp_ext->resp.get_opcode() == chi::rsp_optype_e::DBIDResp) {
            SCCERR(SCMOD) << "CRESP illegal response opcode: " << to_char(resp_ext->resp.get_opcode());
        }
    } else {
        SCCERR(SCMOD) << "Illegal protocol state (maybe just not implemented?) " << phase;
    }

    auto not_finish = 0b11U; // bit0: sending data ongoing, bit1: receiving data ongoing
    auto exp_beat_cnt = calculate_beats(trans);
    auto input_beat_cnt = 0U;
    auto output_beat_cnt = 0U;

    sem_lock lck(wdat_chnl);
    while(not_finish) {
        /// send NonCopyBackWrData_I data to HN packet by packet.
        /// While send data process is still ongoing HN can reply with CompDataI (old data)
        if(output_beat_cnt < exp_beat_cnt) {
            ;
            if(auto data_ext = trans.get_extension<chi::chi_data_extension>()) {
                update_data_extension(data_ext, trans);
            } else {
                create_data_ext(trans);
            }
            output_beat_cnt++;
            SCCDEBUG(SCMOD) << "Atomic send data (txn_id,opcode,cmd,addr,len) = (" << txn_id << ","
                            << to_char(trans.get_extension<chi::chi_data_extension>()->dat.get_opcode()) << ", "
                            << trans.get_command() << ",0x" << std::hex << trans.get_address() << ","
                            << trans.get_data_length() << "), beat=" << output_beat_cnt << "/" << exp_beat_cnt;
            if(output_beat_cnt < exp_beat_cnt)
                phase = chi::BEGIN_PARTIAL_DATA;
            else
                phase = chi::BEGIN_DATA;
            send_packet(phase, trans, txs);
            if(output_beat_cnt == exp_beat_cnt) {
                wait(clk_i.posedge_event()); // sync to clock before releasing resource
                not_finish &= 0x2;           // clear bit0
            }
        }
        /// HN sends CompData_I to the Requester. The data received with Comp is the old copy of the data.
        if(input_beat_cnt < exp_beat_cnt && txs->peq.has_next()) {

            // waiting for response
            auto entry = txs->peq.get();
            sc_assert(std::get<0>(entry) == &trans);
            phase = std::get<1>(entry);

            if(phase == chi::BEGIN_PARTIAL_DATA || phase == chi::BEGIN_DATA) {
                auto data_ext = trans.get_extension<chi::chi_data_extension>();
                sc_assert(data_ext);
                input_beat_cnt++;
                SCCDEBUG(SCMOD) << "Atomic received data (txn_id,opcode,cmd,addr,len)=(" << txn_id << ","
                                << to_char(data_ext->dat.get_opcode()) << "," << trans.get_command() << ",0x"
                                << std::hex << trans.get_address() << "," << trans.get_data_length()
                                << "), beat=" << input_beat_cnt << "/" << exp_beat_cnt;
                if(phase == chi::BEGIN_PARTIAL_DATA)
                    phase = chi::END_PARTIAL_DATA;
                else
                    phase = chi::END_DATA;
                delay = clk_if ? clk_if->period() - 1_ps : SC_ZERO_TIME;
                socket_fw->nb_transport_fw(trans, phase, delay);
                if(phase == chi::END_DATA) {
                    not_finish &= 0x1; // clear bit1
                    if(input_beat_cnt != exp_beat_cnt)
                        SCCERR(SCMOD) << "Wrong beat count, expected " << exp_beat_cnt << ", got " << input_beat_cnt;
                }
            } else {
                SCCERR(SCMOD) << "Illegal protocol state: " << phase;
            }
        } else if(output_beat_cnt == exp_beat_cnt)
            wait(txs->peq.event());
    }
}

void chi::pe::chi_rn_initiator_b::transport(payload_type& trans, bool blocking) {
    SCCTRACE(SCMOD) << "got transport req";
    if(blocking) {
        sc_time t;
        socket_fw->b_transport(trans, t);
    } else {
        auto req_ext = trans.get_extension<chi_ctrl_extension>();
        if(!req_ext) {
            convert_axi4ace_to_chi(trans, name(), use_legacy_mapping.value);
            req_ext = trans.get_extension<chi_ctrl_extension>();
            sc_assert(req_ext != nullptr);
        }
        req_ext->set_src_id(src_id.value);
        req_ext->req.set_tgt_id(home_node_id.value);
        req_ext->req.set_max_flit(calculate_beats(trans) - 1);
        tx_waiting++;
        auto it = tx_state_by_trans.find(to_id(trans));
        if(it == tx_state_by_trans.end()) {
            bool success;
            std::tie(it, success) =
                tx_state_by_trans.insert({to_id(trans), new tx_state(util::strprintf("peq_%d", ++peq_cnt))});
        }
        auto& txs = it->second;
        auto const txn_id = req_ext->get_txn_id();
        if(strict_income_order.value) strict_order_sem.wait();
        sem_lock txnlck(active_tx_by_id[txn_id]); // wait until running tx with same id is over
        if(strict_income_order.value) strict_order_sem.post();
        tx_outstanding++;
        tx_waiting--;
        // Check if Link-credits are available for sending this transactionand wait if not
        req_credits.wait();
        SCCTRACE(SCMOD) << "starting transaction with txn_id=" << txn_id;
        setExpCompAck(req_ext);

        /// Timing
        auto timing_e = trans.get_extension<atp::timing_params>();
        if(timing_e != nullptr) { // TPU as it has been defined in TPU
            auto delay_in_cycles = trans.is_read() ? timing_e->artv : timing_e->awtv;
            auto current_count = get_clk_cnt();
            if(current_count - m_prev_clk_cnt < delay_in_cycles) {
                unsigned delta_cycles = delay_in_cycles - (current_count - m_prev_clk_cnt);
                while(delta_cycles) {
                    delta_cycles--;
                    wait(clk_i.posedge_event());
                }
            }
        } // no timing info in case of STL
        {
            sem_lock lck(req_chnl);
            m_prev_clk_cnt = get_clk_cnt();
            tlm::tlm_phase phase = tlm::BEGIN_REQ;
            sc_core::sc_time delay;
            SCCTRACE(SCMOD) << "Send REQ, addr: 0x" << std::hex << trans.get_address() << ", TxnID: 0x" << std::hex
                            << txn_id;
            tlm::tlm_sync_enum ret = socket_fw->nb_transport_fw(trans, phase, delay);
            if(ret == tlm::TLM_UPDATED) {
                sc_assert(phase == tlm::END_REQ);
                wait(delay);
            } else {
                auto entry = txs->peq.get();
                sc_assert(std::get<0>(entry) == &trans && std::get<1>(entry) == tlm::END_REQ);
            }
            auto credit_ext = trans.get_extension<chi_credit_extension>();
            wait(clk_i.posedge_event()); // sync to clock before releasing resource
            if(credit_ext) {
                if(credit_ext->type == credit_type_e::REQ) {
                    SCCTRACEALL(SCMOD) << "Received " << credit_ext->count << " req "
                                       << (credit_ext->count == 1 ? "credit" : "credits");
                    for(auto i = 0U; i < credit_ext->count; ++i)
                        req_credits.post();
                }
            }
        }

        if((req_optype_e::AtomicLoadAdd <= req_ext->req.get_opcode()) &&
           (req_ext->req.get_opcode() <= req_optype_e::AtomicCompare))
            exec_atomic_protocol(txn_id, trans, txs);
        else {
            exec_read_write_protocol(txn_id, trans, txs);
            bool is_atomic = req_ext->req.get_opcode() >= req_optype_e::AtomicStoreAdd &&
                             req_ext->req.get_opcode() <= req_optype_e::AtomicCompare;
            bool compack_allowed = true;
            switch(req_ext->req.get_opcode()) {
            case req_optype_e::WriteUniqueFullStash:
            case req_optype_e::WriteUniquePtlStash:
            case req_optype_e::StashOnceShared:
            case req_optype_e::StashOnceUnique:
            case req_optype_e::WriteBackPtl:
            case req_optype_e::WriteBackFull:
            case req_optype_e::WriteCleanFull:
            case req_optype_e::WriteEvictFull:
                compack_allowed = false;
                break;
            default:
                break;
            }
            if(!is_atomic && compack_allowed && req_ext->req.is_exp_comp_ack())
                send_comp_ack(trans, txs);
        }

        trans.set_response_status(tlm::TLM_OK_RESPONSE);
        wait(clk_i.posedge_event()); // sync to clock
        tx_state_by_trans.erase(to_id(trans));
        SCCTRACE(SCMOD) << "finished non-blocking protocol";
        any_tx_finished.notify(SC_ZERO_TIME);
        tx_outstanding--;
    }
}

void chi::pe::chi_rn_initiator_b::handle_snoop_response(payload_type& trans,
                                                        chi::pe::chi_rn_initiator_b::tx_state* txs) {
    auto ext = trans.get_extension<chi_data_extension>();
    tlm::tlm_phase phase;
    sc_time delay;
    if(!ext) {
        // dataless response or stash
        auto snp_ext = trans.get_extension<chi_snp_extension>();
        sc_assert(snp_ext != nullptr);

        snp_ext->set_src_id(src_id.value);
        snp_ext->resp.set_tgt_id(snp_ext->get_src_id());
        snp_ext->resp.set_db_id(snp_ext->get_txn_id());

        phase = tlm::BEGIN_RESP; // SRSP channel
        delay = SC_ZERO_TIME;
        auto not_finish =
            snp_ext->resp.get_data_pull() ? 0b11U : 0b10U; // bit0: data is ongoing, bit1: ctrl resp. is ongoing
        {
            sem_lock lock(sresp_chnl);
            auto ret = socket_fw->nb_transport_fw(trans, phase, delay);
            if(ret == tlm::TLM_UPDATED) {
                sc_assert(phase == tlm::END_RESP); // SRSP channel
                wait(delay);
                not_finish &= 0x1; // clear bit1
            }
            wait(clk_i.posedge_event()); // sync to clock before releasing resource
        }
        if(snp_ext->resp.get_data_pull() && trans.get_data_length() != 64) {
            delete[] trans.get_data_ptr();
            trans.set_data_ptr(new uint8_t[64]);
            trans.set_data_length(64);
        }
        auto exp_beat_cnt = calculate_beats(trans);
        auto beat_cnt = 0U;
        while(not_finish) {
            // waiting for response
            auto entry = txs->peq.get();
            sc_assert(std::get<0>(entry) == &trans);
            auto phase = std::get<1>(entry);
            if(phase == tlm::END_RESP) {
                not_finish &= 0x1; // clear bit1
            } else if(snp_ext->resp.get_data_pull() && (phase == chi::BEGIN_PARTIAL_DATA || phase == chi::BEGIN_DATA)) {
                SCCTRACE(SCMOD) << "RDAT packet received with phase " << phase << ". Beat count: " << beat_cnt
                                << ", addr: 0x" << std::hex << trans.get_address();
                not_finish &= 0x1; // clear bit1
                if(phase == chi::BEGIN_PARTIAL_DATA)
                    phase = chi::END_PARTIAL_DATA;
                else
                    phase = chi::END_DATA;
                delay = clk_if ? clk_if->period() - 1_ps : SC_ZERO_TIME;
                socket_fw->nb_transport_fw(trans, phase, delay);
                beat_cnt++;
                if(phase == chi::END_DATA) {
                    not_finish &= 0x2; // clear bit0
                    if(beat_cnt != exp_beat_cnt)
                        SCCERR(SCMOD) << "Wrong beat count, expected " << exp_beat_cnt << ", got " << beat_cnt;
                    (*snoop_cb)(trans);
                }

            } else {
                SCCFATAL(SCMOD) << "Illegal protocol state (maybe just not implemented?)";
            }
        }
        wait(clk_i.posedge_event()); // sync to clock before releasing resource
        if(trans.get_data_length())
            send_comp_ack(trans, txs);
    } else {
        ext->set_src_id(src_id.value);
        send_wdata(trans, txs);
    }
}

// This process handles the SNOOP request received from ICN/HN and dispatches them to the snoop_handler threads
void chi::pe::chi_rn_initiator_b::snoop_dispatch() {
    sc_core::sc_spawn_options opts;
    opts.set_stack_size(0x10000);
    payload_type* trans{nullptr};
    while(true) {
        while(!(trans = snp_peq.get_next_transaction())) {
            wait(snp_peq.get_event());
        }
        if(thread_avail == 0 && thread_active < 32) {
            sc_core::sc_spawn(
                [this]() {
                    payload_type* trans{nullptr};
                    thread_avail++;
                    thread_active++;
                    while(true) {
                        while(!(trans = snp_dispatch_que.get_next_transaction()))
                            wait(snp_dispatch_que.get_event());
                        sc_assert(thread_avail > 0);
                        thread_avail--;
                        this->snoop_handler(trans);
                        thread_avail++;
                    }
                },
                nullptr, &opts);
        }
        snp_dispatch_que.notify(*trans);
    }
}

void chi::pe::chi_rn_initiator_b::snoop_handler(payload_type* trans) {
    auto req_ext = trans->get_extension<chi_snp_extension>();
    sc_assert(req_ext != nullptr);
    auto const txn_id = req_ext->get_txn_id();

    SCCDEBUG(SCMOD) << "Received SNOOP request: (src_id, txn_id, opcode, command, address) = " << req_ext->get_src_id()
                    << ", " << txn_id << ", " << to_char(req_ext->req.get_opcode()) << ", "
                    << (trans->is_read() ? "READ" : "WRITE") << ", " << std::hex << trans->get_address() << ")";

    auto it = tx_state_by_trans.find(to_id(trans));
    if(it == tx_state_by_trans.end()) {
        bool success;
        std::tie(it, success) = tx_state_by_trans.insert({to_id(trans), new tx_state("snp_peq")});
    }
    auto* txs = it->second;

    sc_time delay = clk_if ? clk_if->period() - 1_ps : SC_ZERO_TIME;
    tlm::tlm_phase phase = tlm::END_REQ;
    socket_fw->nb_transport_fw(*trans, phase, delay);
    auto cycles = 0U;
    if(snoop_cb)
        cycles = (*snoop_cb)(*trans);
    if(cycles < std::numeric_limits<unsigned>::max()) {
        // we handle the snoop access ourselfs
        for(size_t i = 0; i < cycles + 1; ++i)
            wait(clk_i.posedge_event());
        handle_snoop_response(*trans, txs);
    }
    tx_state_by_trans.erase(to_id(trans));
    if(trans->has_mm())
        trans->release();
}
