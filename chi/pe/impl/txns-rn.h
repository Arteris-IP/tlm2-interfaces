/*
 * Copyright (c) 2019 Xilinx Inc.
 * Written by Francisco Iglesias.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 *
 * [1] AMBA 5 CHI Architecture Specification, ARM IHI 0050C, ID050218
 *
 */

#ifndef TLM_MODULES_PRIV_CHI_TXNS_RN_H__
#define TLM_MODULES_PRIV_CHI_TXNS_RN_H__

#include <chi/pe/impl/cacheline.h>
#include <tlm.h>
#include <tlm/scc/tlm_gp_shared.h>
#include <tlm/scc/tlm_mm.h>

namespace chi {
namespace pe {
namespace RN {
namespace {
inline uint8_t log2n(uint8_t siz) { return ((siz > 1) ? 1 + log2n(siz >> 1) : 0); }
inline unsigned calculate_beats(chi::chi_protocol_types::tlm_payload_type& p, unsigned transfer_width_in_bytes) {
    if(transfer_width_in_bytes)
        return p.get_data_length() < transfer_width_in_bytes ? 1 : p.get_data_length() / transfer_width_in_bytes;
    else
        return 0;
}

}
template<int NODE_ID, int ICN_ID>
class ITxn {
public:
    ITxn(chi::req_optype_e opcode, size_t data_width, TxnIdPool *ids, unsigned length, CacheLine *l = nullptr, bool isSnp = false)
    : m_data_width(data_width)
    , m_l(l)
    , m_ids(ids)
    , m_txnID(0)
    , m_isSnp(isSnp)
    {
        if (!m_isSnp) {
            m_gp = tlm::scc::tlm_mm<>::get().allocate<chi::chi_ctrl_extension>(length, true);
            m_gp->set_streaming_width(length);
            m_chiattr = m_gp->get_extension<chi::chi_ctrl_extension>();
            m_chiattr->set_src_id(NODE_ID);
            if (DoAssertCheck(opcode)) {
                assert(m_l);
            }
            // Store for release in destructor
            m_txnID = m_ids->GetID();
            m_chiattr->set_txn_id(ICN_ID);
            m_chiattr->set_txn_id(m_txnID);
            m_chiattr->req.set_opcode(opcode);
            m_chiattr->req.set_size(log2n(length));
            m_chiattr->req.set_max_flit(calculate_beats(*m_gp, data_width/8) - 1);
            //
            // Transactions (except PrefetchTgt) must start with
            // AllowRetry asserted 2.11 [1]
            //
            if (opcode != chi::req_optype_e::PrefetchTgt) {
                m_chiattr->req.set_allow_retry(true);
            }
            // Takes ownership of the ptr
            m_gp->set_extension(m_chiattr);
        } else {
            assert(m_l == nullptr);
            assert(m_ids == nullptr);
        }
    }

    virtual ~ITxn() {
        if (m_ids) {
            m_ids->ReturnID(m_txnID);
        }
    }

    bool DoAssertCheck(chi::req_optype_e opcode) {
        if (opcode >= chi::req_optype_e::AtomicStoreAdd && opcode <= chi::req_optype_e::AtomicCompare) {
            return false;
        }
        switch (opcode) {
        case chi::req_optype_e::ReadOnce:
        case chi::req_optype_e::ReadOnceCleanInvalid:
        case chi::req_optype_e::ReadOnceMakeInvalid:
        case chi::req_optype_e::ReadNoSnp:
        case chi::req_optype_e::WriteUniquePtl:
        case chi::req_optype_e::WriteUniquePtlStash:
        case chi::req_optype_e::WriteUniqueFull:
        case chi::req_optype_e::WriteUniqueFullStash:
        case chi::req_optype_e::WriteUniqueFullCleanSh:
        case chi::req_optype_e::WriteUniqueFullCleanShPerSep:
        case chi::req_optype_e::WriteUniqueZero:
        case chi::req_optype_e::WriteUniquePtlCleanSh:
        case chi::req_optype_e::WriteUniquePtlCleanShPerSep:
        case chi::req_optype_e::WriteNoSnpPtl:
        case chi::req_optype_e::WriteNoSnpFull:
        case chi::req_optype_e::WriteNoSnpZero:
        case chi::req_optype_e::WriteNoSnpFullCleanSh:
        case chi::req_optype_e::WriteNoSnpFullCleanInv:
        case chi::req_optype_e::WriteNoSnpFullCleanShPerSep:
        case chi::req_optype_e::WriteNoSnpPtlCleanSh:
        case chi::req_optype_e::WriteNoSnpPtlCleanInv:
        case chi::req_optype_e::WriteNoSnpPtlCleanShPerSep:
        case chi::req_optype_e::StashOnceShared:
        case chi::req_optype_e::StashOnceUnique:
        case chi::req_optype_e::StashOnceSepShared:
        case chi::req_optype_e::StashOnceSepUnique:
        case chi::req_optype_e::DVMOp:
        case chi::req_optype_e::PrefetchTgt:
            return false;
        default:
        break;
        }
        return true;
    }

    virtual bool HandleRxRsp(tlm::tlm_generic_payload &gp) {
        return false;
    }

    virtual bool HandleRxDat(tlm::tlm_generic_payload &gp) {
        return false;
    }

    virtual bool TransmitOnTxDatChannel() {
        return false;
    }

    virtual bool TransmitOnTxRspChannel() {
        return false;
    }

    virtual void UpdateProgress() {}

    unsigned GetProgress() {
        if(m_chidata)
            return m_chidata->dat.get_data_id()*16;
        return 0;
    }

    unsigned GetNextProgress() {
        if(m_chidata)
            return m_chidata->dat.get_data_id()*16+m_data_width/8;
        return 0;
    }

    virtual bool TransmitAsAck() { return false; }

    bool RetryRequest() {
        m_gp->set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);
        return m_gotRetryAck && m_gotPCrdGrant;
    }

    void HandleRetryAck(chi::chi_ctrl_extension *chiattr) {
        //
        // Keep TgtID, Set PCrdType and deassert AllowRetry
        // 2.6.5 + 2.11.2 [1]
        //
        m_chiattr->req.set_allow_retry(false);
        m_chiattr->req.set_pcrd_type(chiattr->req.get_pcrd_type());
        m_gotRetryAck = true;
    }

    void HandlePCrdGrant(chi::chi_ctrl_extension *chiattr) {
        // Only one transaction at a time so just keep track
        // that a PCrdGrant has been is received. Most likely
        // it will arrive after the RetryAck, but it can also
        // arrive before 2.3.2 [1].
        m_gotPCrdGrant = true;
    }

    //
    // For transactions that terminate after a TXChannel, example
    // with ExpCompAck
    //
    virtual bool Done() = 0;

    tlm::tlm_generic_payload& GetGP() {
        return *m_gp;
    }

    uint8_t GetTxnID() {
        return m_txnID;
    }
    virtual uint8_t GetDBID() {
        return m_chiattr->resp.get_db_id();
    }

    bool IsSnp() {
        return m_isSnp;
    }

    bool IsWrite() {
        return m_isWrite;
    }

    sc_core::sc_event& DoneEvent() {
        return m_done;
    }

    bool GetIsWriteUniqueWithCompAck() {
        return m_isWriteUniqueWithCompAck;
    }

    void SetIsWriteUniqueWithCompAck(bool val) {
        m_isWriteUniqueWithCompAck = val;
    }
protected:
    //
    // Copy over relevant attributes for transactions that will
    // fill read in / allocate a cacheline.
    //
    void CopyCHIAttr(tlm::tlm_generic_payload &gp) {
        if (auto attr = gp.get_extension<chi::chi_ctrl_extension>()) {
            CopyCHIAttr(attr);
            m_chiattr->req.set_order(attr->req.get_order());
        }
    }
    //
    // Copy attributes when reading in / writing back a cacheline
    //
    void CopyCHIAttr(chi::chi_ctrl_extension *attr) {
        assert(attr);
        m_chiattr->req.set_size(attr->req.get_size());
        m_chiattr->req.set_non_secure(attr->req.is_non_secure());
        m_chiattr->req.set_mem_attr(attr->req.get_mem_attr());
        m_chiattr->req.set_order(attr->req.get_order());
        m_chiattr->req.set_exp_comp_ack(attr->req.is_exp_comp_ack());
        //m_chiattr->req.set_snp_attr(attr->req.is_snp_attr());
        m_chiattr->req.set_snoop_me(attr->req.is_snoop_me());
        m_chiattr->req.set_stash_n_id(attr->req.get_stash_n_id());
        m_chiattr->req.set_stash_n_id_valid(attr->req.is_stash_n_id_valid());
        m_chiattr->req.set_stash_lp_id(attr->req.get_stash_lp_id());
        m_chiattr->req.set_stash_lp_id_valid(attr->req.is_stash_lp_id_valid());
        m_chiattr->set_qos(attr->get_qos());
    }

    void SetupCompAck(chi::chi_ctrl_extension *chiattr) {
        chiattr->req.set_tgt_id(chiattr->get_src_id());
        chiattr->set_txn_id(chiattr->resp.get_db_id()); //CompAck gets db_id as TxnId
        chiattr->resp.set_opcode(chi::rsp_optype_e::CompAck);
    }

    void SetupCompAck(chi::chi_data_extension* chi_data) {
        m_chiattr->req.set_tgt_id(chi_data->dat.get_home_n_id());
        m_chiattr->set_txn_id(chi_data->dat.get_db_id()); //CompAck gets db_id as TxnId
        m_chiattr->resp.set_opcode(chi::rsp_optype_e::CompAck);
    }

    void SetupCompAck(chi::chi_snp_extension *chiattr) {
        chiattr->set_txn_id(chiattr->resp.get_db_id()); //CompAck gets db_id as TxnId
        chiattr->resp.set_opcode(chi::rsp_optype_e::CompAck);
    }


    unsigned int CopyData(tlm::tlm_generic_payload &gp, chi::chi_data_extension *chiattr) {
        //FIXME: need to calculate write
        unsigned char *data = gp.get_data_ptr();
        unsigned int len = m_data_width/8; //gp.get_data_length();
        unsigned int offset = chiattr->dat.get_data_id() * len;
        unsigned char *be = gp.get_byte_enable_ptr();
        unsigned int be_len = gp.get_byte_enable_length();
        unsigned int max_len = CACHELINE_SZ - offset;
        len = std::min(len, max_len);
        m_l->Write(offset, data, be, len);
        return len;
    }

    void ParseReadResp(chi::chi_data_extension *chiattr) {
        if (chiattr->dat.get_opcode() == chi::dat_optype_e::DataSepResp) {
            switch (chiattr->dat.get_resp()) {
            case chi::dat_resptype_e::DataSepResp_UC:
                m_l->SetValid(true);
                m_l->SetShared(false);
                m_l->SetDirty(false);
            break;
            case chi::dat_resptype_e::DataSepResp_SC:
                m_l->SetValid(true);
                m_l->SetShared(true);
                m_l->SetDirty(false);
            break;
            case chi::dat_resptype_e::DataSepResp_I:
            default:
                m_l->SetValid(false);
            break;
            }
        } else
            if (chiattr->dat.get_opcode() == chi::dat_optype_e::CompData) {
                switch (chiattr->dat.get_resp()) {
                case chi::dat_resptype_e::CompData_UC:
                    m_l->SetValid(true);
                    m_l->SetShared(false);
                    m_l->SetDirty(false);
                break;
                case chi::dat_resptype_e::CompData_SC:
                    m_l->SetValid(true);
                    m_l->SetShared(true);
                    m_l->SetDirty(false);
                break;
                case chi::dat_resptype_e::CompData_UD_PD:
                    m_l->SetValid(true);
                    m_l->SetShared(false);
                    m_l->SetDirty(true);
                break;
                case chi::dat_resptype_e::CompData_SD_PD:
                    m_l->SetValid(true);
                    m_l->SetShared(true);
                    m_l->SetDirty(true);
                break;
                case chi::dat_resptype_e::CompData_I:
                default:
                    m_l->SetValid(false);
                break;
                }
            } else {
                m_l->SetValid(false);
            }
    }
    const size_t  m_data_width;
    CacheLine *m_l;
    tlm::scc::tlm_gp_shared_ptr m_gp;
    chi::chi_ctrl_extension* m_chiattr{nullptr};
    chi::chi_data_extension* m_chidata{nullptr};
    TxnIdPool *m_ids{nullptr};
    uint8_t m_txnID;
    sc_core::sc_event m_done;
    bool m_isSnp{false};
    bool m_isWrite{false};
    bool m_gotRetryAck{false};
    bool m_gotPCrdGrant{false};
    bool m_isWriteUniqueWithCompAck{false};
};

template<int NODE_ID, int ICN_ID>
class ReadTxn: public ITxn<NODE_ID, ICN_ID> {
public:
    typedef ITxn<NODE_ID, ICN_ID> ITxn_t;

    using ITxn_t::m_gp;
    using ITxn_t::m_chiattr;
    using ITxn_t::m_l;

    ReadTxn(tlm::tlm_generic_payload &gp, chi::req_optype_e opcode, uint64_t addr, size_t data_width, CacheLine *l, TxnIdPool *ids)
    : ITxn_t(opcode, data_width, ids, CACHELINE_SZ, l)
    , m_readAddr(addr)
    , m_gotRspSepData(false)
    , m_gotReadReceipt(true)
    , m_gotDataSepRsp(false)
    , m_progress(0)
    , m_transmitCompAck(true)
    {
        m_gp->set_command(tlm::TLM_READ_COMMAND);
        m_gp->set_address(addr);
        // It is permitted to use CompAck on all read, 2.8.3
        // [1]
        this->CopyCHIAttr(gp);
        //
        // SnpAttr must be set on ReadOnce*, ReadClean,
        // ReadShared, ReadNotSharedDirty, ReadUnique and must
        // be unset on ReadNoSnp and ReadNoSnpSep.
        // 2.9.6 [1]
        //
        if (opcode == chi::req_optype_e::ReadNoSnp || opcode == chi::req_optype_e::ReadNoSnpSep) {
            m_chiattr->req.set_snp_attr(false);
            m_transmitCompAck = m_chiattr->req.is_exp_comp_ack();
        } else {
            m_chiattr->req.set_snp_attr(true);
            if(!IsNonCoherentRead())
                m_chiattr->req.set_exp_comp_ack(true);
            else
                m_transmitCompAck = m_chiattr->req.is_exp_comp_ack();
        }
        if (IsNonCoherentRead() && IsOrdered()) {
            m_gotReadReceipt = false;
        }
    }
    //
    // Data will not remain coherent if kept in a local cache at
    // the requester, 4.2.1 [1]
    //
    bool IsNonCoherentRead() {
        switch (m_chiattr->req.get_opcode()) {
        case chi::req_optype_e::ReadNoSnp:
        case chi::req_optype_e::ReadOnce:
        case chi::req_optype_e::ReadOnceCleanInvalid:
        case chi::req_optype_e::ReadOnceMakeInvalid:
            return true;
        }
        return false;
    }

    bool IsOrdered() {
        return m_chiattr->req.get_order() > 1;
    }

     bool HandleRxRsp(tlm::tlm_generic_payload &gp) override {
        auto *chiattr = gp.get_extension<chi::chi_ctrl_extension>();
        if(chiattr->resp.get_opcode() == chi::rsp_optype_e::Comp) { // response to MakeReadUnique without data
            switch (chiattr->resp.get_resp()) {
            case chi::rsp_resptype_e::Comp_UD_PD:
                m_l->SetValid(true);
                m_l->SetTag(m_gp->get_address());
                m_l->SetShared(false);
                m_l->SetDirty(true);
                break;
            case chi::rsp_resptype_e::Comp_UC:
                m_l->SetValid(true);
                m_l->SetTag(m_gp->get_address());
                m_l->SetShared(false);
                m_l->SetDirty(false);
            break;
            case chi::rsp_resptype_e::Comp_SC:
                m_l->SetValid(true);
                m_l->SetTag(m_gp->get_address());
                m_l->SetShared(true);
                m_l->SetDirty(false);
            break;
            case chi::rsp_resptype_e::Comp_I:
            default:
                m_l->SetValid(false);
            break;
            }
            m_l->ByteEnablesDisableAll();
            m_l->SetCHIAttr(m_chiattr);
            this->SetupCompAck(chiattr);
            m_gotRspSepData = true;
            m_gotDataSepRsp = true;
            return true;
        }
        bool acceptRspSepData = !m_gotRspSepData && chiattr->resp.get_opcode() == chi::rsp_optype_e::RespSepData;
        bool acceptReadReceipt = !m_gotRspSepData && chiattr->resp.get_opcode() == chi::rsp_optype_e::ReadReceipt;
        if (acceptRspSepData) {
            //
            // RspSepData means ReadReceipt
            //
            m_gotRspSepData = true;
            m_gotReadReceipt = true;
            return true;
        } else if (acceptReadReceipt) {
            m_gotReadReceipt = true;
            return true;
        }
        return false;
    }

    bool HandleRxDat(tlm::tlm_generic_payload &gp) override {
        auto *chiattr = gp.get_extension<chi::chi_ctrl_extension>();
        auto *chidata = gp.get_extension<chi::chi_data_extension>();
        bool acceptCompData = m_gotRspSepData == false && m_gotDataSepRsp == false && chidata->dat.get_opcode() == chi::dat_optype_e::CompData;
        bool acceptDataSepRsp = m_gotDataSepRsp == false && chidata->dat.get_opcode() == chi::dat_optype_e::DataSepResp;
        bool datHandled = false;
        if (acceptCompData || acceptDataSepRsp) {
            if (IsNonCoherentRead()) {
                m_progress += CopyDataNonCoherent(gp, chidata);
            } else {
                m_progress += this->CopyData(gp, chidata);
                this->ParseReadResp(chidata);
                if (m_l->IsValid()) {
                    m_l->SetTag(m_readAddr);
                    m_l->ByteEnablesEnableAll();
                    //
                    // Copy the attributes used into the line
                    //
                    m_l->SetCHIAttr(m_chiattr);
                }
            }
            if (m_progress == CACHELINE_SZ) {
                m_gotRspSepData = true;
                m_gotDataSepRsp = true;
                // All done setup CompAck
                if(m_transmitCompAck)
                    this->SetupCompAck(chidata);
                gp.set_response_status(tlm::TLM_OK_RESPONSE);
            }
            datHandled = true;
        }
        return datHandled;
    }

    bool TransmitOnTxRspChannel() override {
        //
        // 2.3.1, Read no with separate non-data and data-only
        // responses: All reads are permitted to wait for both
        // responses before signaling CompAck (required for
        // ordered ReadOnce and ReadNoSnp).
        //
        return m_transmitCompAck && m_gotRspSepData && m_gotDataSepRsp;
    }

    void UpdateProgress() override { /*FIXME: implement progress mechanism*/}

    bool TransmitAsAck() override { return m_transmitCompAck && m_gotRspSepData && m_gotDataSepRsp; }

    bool IsReadOnce() {
        return m_chiattr->req.get_opcode() == chi::req_optype_e::ReadOnce;
    }

    unsigned int CopyDataNonCoherent(tlm::tlm_generic_payload &gp, chi::chi_data_extension *chiattr) {
        // FIXME: calculate copy
        unsigned char *data = gp.get_data_ptr();
        unsigned int len = this->m_data_width/8; // gp.get_data_length();
        unsigned int offset = chiattr->dat.get_data_id() * len;
        unsigned char *be = gp.get_byte_enable_ptr();
        unsigned int be_len = gp.get_byte_enable_length();
        unsigned int max_len = CACHELINE_SZ - offset;
        if (len > max_len) {
            len = max_len;
        }
        return len;
    }

    unsigned int FetchNonCoherentData(tlm::tlm_generic_payload &gp, unsigned int pos, unsigned int line_offset) {
        unsigned char *data = gp.get_data_ptr() + pos;
        unsigned int len = gp.get_data_length() - pos;
        unsigned int max_len = CACHELINE_SZ - line_offset;
        unsigned char *be = gp.get_byte_enable_ptr();
        unsigned int be_len = gp.get_byte_enable_length();
        if (len > max_len) {
            len = max_len;
        }
        //FIXME: data handling
//        if (be_len) {
//            unsigned int i;
//            for (i = 0; i < len; i++, pos++) {
//                bool do_access = be[pos % be_len] == TLM_BYTE_ENABLED;
//                if (do_access) {
//                    data[i] = m_data[line_offset + i];
//                }
//            }
//        } else {
//            memcpy(data, &m_data[line_offset], len);
//        }
        return len;
    }

    bool Done() override {
        return m_gotRspSepData && m_gotDataSepRsp && m_gotReadReceipt &&
                (!m_transmitCompAck || m_gp->get_response_status() != tlm::TLM_INCOMPLETE_RESPONSE);
    }

    void SetExpCompAck(bool val) {
        m_chiattr->SetExpCompAck(val);
        m_transmitCompAck = val;
    }
private:
    uint64_t m_readAddr;
    bool m_gotRspSepData;
    bool m_gotReadReceipt;
    bool m_gotDataSepRsp;
    unsigned int m_progress;
    bool m_transmitCompAck;
};

template<int NODE_ID, int ICN_ID>
class DatalessTxn: public ITxn<NODE_ID, ICN_ID> {
public:
    typedef ITxn<NODE_ID, ICN_ID> ITxn_t;

    using ITxn_t::m_gp;
    using ITxn_t::m_chiattr;
    using ITxn_t::m_l;

    DatalessTxn(chi::req_optype_e opcode, uint64_t addr	/* cache aligned */, CacheLine *l, TxnIdPool *ids, tlm::tlm_generic_payload *gp = nullptr  /* Used for MakeUnique*/)
    : ITxn<NODE_ID, ICN_ID>(opcode, 0, ids, CACHELINE_SZ, l)
    , m_gotComp(false)
    , m_transmitCompAck(false)
    {
        m_gp->set_address(addr);
        if (gp || !l) {// MakeUnique allocates a line
            assert(IsMakeUnique() || IsStash() || opcode == chi::req_optype_e::PrefetchTgt);
            this->CopyCHIAttr(*gp);
        } else { // Line is already allocated
            this->CopyCHIAttr(l->GetCHIAttr());
        }
        // Below must assert ExpCompACk but not the other dataless reqs, 2.3.1 [1]
        if (opcode == chi::req_optype_e::CleanUnique || IsMakeUnique()) {
            m_chiattr->req.set_exp_comp_ack(true);
            m_transmitCompAck = true;
        }
        // SnpAttr is allowed to be set on all and must be set on CleanUnique, MakeUnique, StashOnce*, Evict, 2.9.6 [1]
        if(!IsCacheMaintenance())
            m_chiattr->req.set_snp_attr(true);
    }

    bool HandleRxRsp(tlm::tlm_generic_payload &gp) override {
        bool rspHandled = false;
        auto *chiattr = gp.get_extension<chi::chi_ctrl_extension>();
        if (!m_gotComp && chiattr->resp.get_opcode() == chi::rsp_optype_e::Comp) {
            m_gotComp = true;
            //
            // Cache maintenance ops ignores resp field
            // 4.5.1 [1]
            //
            if (!IsCacheMaintenance()) {
                ParseResp(chiattr);
            }
            if (m_transmitCompAck) {
                //
                // MakeUnique expects store to complete cacheline
                //
                if (IsMakeUnique()) {
                    m_l->ByteEnablesDisableAll();
                    //
                    // Copy the attributes used into the line
                    //
                    m_l->SetCHIAttr(m_chiattr);
                }
                // Use SrcID as target
                this->SetupCompAck(chiattr);
            } else {
                //
                // Transaction completed
                //
                this->m_done.notify();
            }
            rspHandled = true;
        }
        return rspHandled;
    }

    bool TransmitOnTxRspChannel() override {
        return m_transmitCompAck;
    }

    bool TransmitAsAck() override { return m_transmitCompAck && m_gotComp; }

    bool Done() override {
        if(m_chiattr->req.get_opcode() == chi::req_optype_e::PrefetchTgt) return true;
        if (m_transmitCompAck) {
            bool compAckDone = m_gp->get_response_status() != tlm::TLM_INCOMPLETE_RESPONSE;
            return m_gotComp && compAckDone;
        }
        return m_gotComp;
    }

private:
    inline bool IsMakeUnique() {
        return this->m_chiattr->req.get_opcode() == chi::req_optype_e::MakeUnique;
    }
    inline bool IsStash() {
        switch(this->m_chiattr->req.get_opcode()) {
        case chi::req_optype_e::StashOnceSepShared:
        case chi::req_optype_e::StashOnceSepUnique:
        case chi::req_optype_e::StashOnceShared:
        case chi::req_optype_e::StashOnceUnique:
            return true;
        }
        return false;
    }

    inline bool IsCacheMaintenance() {
        switch (this->m_chiattr->req.get_opcode()) {
        case chi::req_optype_e::CleanShared:
        case chi::req_optype_e::CleanSharedPersist:
        case chi::req_optype_e::CleanSharedPersistSep:
        case chi::req_optype_e::CleanInvalid:
        case chi::req_optype_e::MakeInvalid:
            return true;
        default:
        break;
        }
        return false;
    }

    void ParseResp(chi::chi_ctrl_extension *chiattr) {
        switch (chiattr->resp.get_resp()) {
        case chi::rsp_resptype_e::Comp_UC:
            m_l->SetValid(true);
            m_l->SetTag(m_gp->get_address());
            m_l->SetShared(false);
            m_l->SetDirty(false);
        break;
        case chi::rsp_resptype_e::Comp_SC:
            m_l->SetValid(true);
            m_l->SetTag(m_gp->get_address());
            m_l->SetShared(true);
            m_l->SetDirty(false);
        break;
        case chi::rsp_resptype_e::Comp_I:
        default:
            m_l->SetValid(false);
        break;
        }
    }

    bool m_gotComp;
    bool m_transmitCompAck;
};

template<int NODE_ID, int ICN_ID>
class WriteTxn: public ITxn<NODE_ID, ICN_ID> {
public:
    typedef ITxn<NODE_ID, ICN_ID> ITxn_t;

    using ITxn_t::m_gp;
    using ITxn_t::m_chiattr;
    using ITxn_t::m_l;
    using ITxn_t::m_isWriteUniqueWithCompAck;

    //
    // Used by NonCopyBack requests
    //
    WriteTxn(tlm::tlm_generic_payload &gp, chi::req_optype_e opcode, TxnIdPool *ids, uint64_t addr	/* cache aligned*/, size_t data_width,
            unsigned int pos, unsigned int line_offset)
    : WriteTxn(opcode, data_width, nullptr, ids, CACHELINE_SZ)
    {
        m_gp->set_address(addr);
        this->CopyCHIAttr(gp);
        m_chiattr->req.set_snp_attr(!IsWriteNoSnp());
        if (IsAtomicStore()) {
            // The request uses the correct length
            m_gp->set_data_length(gp.get_data_length());
        } else {
            switch(this->m_chiattr->req.get_opcode()){
            default:
                m_gp->set_data_length(CACHELINE_SZ);
                break;
            case chi::req_optype_e::WriteBackPtl:
            case chi::req_optype_e::WriteCleanPtl:
            case chi::req_optype_e::WriteUniquePtl:
            case chi::req_optype_e::WriteUniquePtlStash:
            case chi::req_optype_e::WriteUniquePtlCleanSh:
            case chi::req_optype_e::WriteUniquePtlCleanShPerSep:
                m_gp->set_data_length(1UL<<this->m_chiattr->req.get_size());
                break;
            case chi::req_optype_e::WriteUniqueZero:
            case chi::req_optype_e::WriteNoSnpZero:
                m_gp->set_data_length(0);
                break;
            }
        }
        CopyDataNonCached(gp, pos, line_offset);
    }

    void CopyDataNonCached(tlm::tlm_generic_payload &gp, unsigned int pos, unsigned int line_offset) {
        auto *data = gp.get_data_ptr() + pos;
        auto len = gp.get_data_length() - pos;
        auto max_len = CACHELINE_SZ - line_offset;
        auto *be = gp.get_byte_enable_ptr();
        auto be_len = gp.get_byte_enable_length();
        if (len > max_len) {
            len = max_len;
        }
        //FIXME: data handling
//        if (be_len) {
//            for (auto i = 0U; i < len; i++, pos++) {
//                bool do_access = be[pos % be_len] == TLM_BYTE_ENABLED;
//                if (do_access) {
//                    m_data[line_offset + i] = data[i];
//                    m_byteEnable[line_offset + i] = TLM_BYTE_ENABLED;
//                }
//            }
//        } else {
//            memcpy(&m_data[line_offset], data, len);
//            memset(&m_byteEnable[line_offset],
//            TLM_BYTE_ENABLED, len);
//        }
    }
    //
    // Used by CopyBack requests
    //
    WriteTxn(chi::req_optype_e opcode, size_t data_width, CacheLine *l, TxnIdPool *ids, unsigned length = CACHELINE_SZ)
    : ITxn<NODE_ID, ICN_ID>(opcode, data_width, ids, length, l)
    , m_gotDBID(false)
    , m_gotComp(false)
    , m_sentCompAck(true)
    , m_opcode(opcode)
    , m_datOpcode(GetDatOpcode())
    {
        this->m_isWrite=true;
        m_gp->set_command(tlm::TLM_WRITE_COMMAND);
        if (IsCopyBack()) {
            m_gp->set_address(m_l->GetTag());
            //
            // Move over data and byte enables
            //
            memcpy(m_gp->get_data_ptr(), m_l->GetData(), CACHELINE_SZ);
            memcpy(m_gp->get_byte_enable_ptr(), m_l->GetByteEnables(), CACHELINE_SZ);
            //
            // Copy line CHI attr for WriteBackFull, WriteBackPtl,
            // WriteCleanFull and WriteEvictFull.
            //
            this->CopyCHIAttr(m_l->GetCHIAttr());
        }
        //
        // SnpAttr must be set on WriteBack, WriteClean,
        // WriteEvictFull and WriteUnique. It must be unset on
        // WriteNoSnp. See 2.9.6 [1]
        //
        m_chiattr->req.set_snp_attr(!IsWriteNoSnp());
    }

    virtual bool HandleRxRsp(tlm::tlm_generic_payload &gp) override {
        bool rspHandled = false;
        if (!m_gotComp && this->m_chiattr->resp.get_opcode() == chi::rsp_optype_e::Comp) {
            m_gotComp = true;
            rspHandled = true;
        } else if (!m_gotDBID && this->m_chiattr->resp.get_opcode() == chi::rsp_optype_e::DBIDResp) {
            m_gotDBID = true;
            SetupWriteDat();
            if (IsCopyBack()) {
                UpdateLineStatus();
            }
            rspHandled = true;
        } else if (!m_gotDBID && !m_gotComp && this->m_chiattr->resp.get_opcode() == chi::rsp_optype_e::CompDBIDResp) {
            m_gotComp = true;
            m_gotDBID = true;
            SetupWriteDat();
            if (IsCopyBack()) {
                UpdateLineStatus();
            }
            rspHandled = true;
        }
        return rspHandled;
    }

    inline bool WriteDone() {
        if( this->m_chiattr->req.get_opcode()==chi::req_optype_e::WriteUniqueZero ||
                this->m_chiattr->req.get_opcode()==chi::req_optype_e::WriteNoSnpZero )
            return true;
        if(this->m_chidata) {
            unsigned beats = calculate_beats(this->GetGP(), this->m_data_width/8);
            return this->m_chidata->dat.get_data_id()>=beats;
        }
        return true;
    }

    bool TransmitOnTxDatChannel() override {
        return m_gotDBID && !WriteDone();
    }

    bool TransmitOnTxRspChannel() override {
        if (!m_sentCompAck && m_gotDBID && WriteDone()) {
            m_sentCompAck = true;
            return true;
        }
        return false;
    }

    void UpdateProgress() override {
        if(this->m_chidata)
            this->m_chidata->dat.set_data_id(this->m_chidata->dat.get_data_id()+this->m_data_width/128);
    }

    bool Done() override {
        return m_sentCompAck && m_gotComp && m_gotDBID && WriteDone();
    }

    bool IsWriteNoSnp() {
        return m_opcode == chi::req_optype_e::WriteNoSnpPtl ||
                m_opcode == chi::req_optype_e::WriteNoSnpFull ||
                m_opcode == chi::req_optype_e::WriteNoSnpZero;
    }

    bool IsWriteUnique() {
        switch (m_opcode) {
        case chi::req_optype_e::WriteUniquePtl:
        case chi::req_optype_e::WriteUniqueFull:
        case chi::req_optype_e::WriteUniquePtlStash:
        case chi::req_optype_e::WriteUniqueFullStash:
        case chi::req_optype_e::WriteUniquePtlCleanSh:
        case chi::req_optype_e::WriteUniqueFullCleanSh:
        case chi::req_optype_e::WriteUniquePtlCleanShPerSep:
        case chi::req_optype_e::WriteUniqueFullCleanShPerSep:
            return true;
        default:
            return false;
        }
    }

    void SetExpCompAck(bool val) {
        //
        // Only WriteUnique is allowed to have ExpCompAck
        // asserted
        //
        if (val) {
            assert(IsWriteUnique());
        }
        m_chiattr->req.set_exp_comp_ack(val);
        m_sentCompAck = !val;
        m_isWriteUniqueWithCompAck = val;
    }

private:

    bool IsAtomicStore() {
        return m_opcode >= chi::req_optype_e::AtomicStoreAdd && m_opcode < chi::req_optype_e::AtomicLoadAdd;
    }

    inline chi::dat_optype_e GetDatOpcode() {
        if (IsAtomicStore()) {
            return chi::dat_optype_e::NonCopyBackWrData;
        }
        switch (m_opcode) {
        case chi::req_optype_e::WriteNoSnpPtl:
        case chi::req_optype_e::WriteNoSnpFull:
        case chi::req_optype_e::WriteNoSnpZero:
        case chi::req_optype_e::WriteNoSnpFullCleanSh:
        case chi::req_optype_e::WriteNoSnpFullCleanInv:
        case chi::req_optype_e::WriteNoSnpFullCleanShPerSep:
        case chi::req_optype_e::WriteNoSnpPtlCleanSh:
        case chi::req_optype_e::WriteNoSnpPtlCleanInv:
        case chi::req_optype_e::WriteNoSnpPtlCleanShPerSep:
        case chi::req_optype_e::WriteUniquePtl:
        case chi::req_optype_e::WriteUniqueFull:
        case chi::req_optype_e::WriteUniqueZero:
        case chi::req_optype_e::WriteUniqueFullStash:
        case chi::req_optype_e::WriteUniqueFullCleanSh:
        case chi::req_optype_e::WriteUniqueFullCleanShPerSep:
        case chi::req_optype_e::WriteUniquePtlStash:
        case chi::req_optype_e::WriteUniquePtlCleanSh:
        case chi::req_optype_e::WriteUniquePtlCleanShPerSep:
            return chi::dat_optype_e::NonCopyBackWrData;
        case chi::req_optype_e::WriteBackPtl:
        case chi::req_optype_e::WriteBackFull:
        case chi::req_optype_e::WriteCleanFull:
        case chi::req_optype_e::WriteEvictFull:
        default:
            return chi::dat_optype_e::CopyBackWrData;
        }
    }

    inline bool IsCopyBack() {
        return GetDatOpcode() == chi::dat_optype_e::CopyBackWrData;
    }

    inline bool IsNonCopyBack() {
        return GetDatOpcode() == chi::dat_optype_e::NonCopyBackWrData;
    }

    void SetupWriteDat() {
        // Writes use SrcID instead HomeNID, 2.6.3 [1]
        m_chiattr->req.set_tgt_id(this->m_chiattr->get_src_id());
        m_chiattr->set_txn_id(this->m_chiattr->resp.get_db_id());
        if(!this->m_chidata) {
            this->m_chidata = new chi::chi_data_extension();
            m_gp->set_auto_extension(this->m_chidata);
        }
        this->m_chidata->dat.set_opcode(m_datOpcode);
        this->m_chidata->dat.set_resp(GetResp());
        this->m_chidata->set_txn_id(m_chiattr->resp.get_db_id());
    }

    chi::dat_resptype_e GetResp() {
        bool shared;
        bool dirty;
        if (IsNonCopyBack() || IsAtomicStore()) {
            //
            // WriteNoSnp* / WriteUnique* are used without
            // cacheline (so below if does not apply)
            // (4.2.3 [1])
            //
            return chi::dat_resptype_e::NonCopyBackWrData;
        }
        shared = m_l->GetShared();
        dirty = m_l->GetDirty();
        if (!m_l->IsValid()) {
            //
            // If the line got invalid by a snoop request
            // return CopyBackWrData_I and deassert byte
            // enables, 4.9.1 [1]
            //
            memset(m_gp->get_byte_enable_ptr(), TLM_BYTE_DISABLED, m_gp->get_byte_enable_length());
            return chi::dat_resptype_e::CopyBackWrData_I;
        }
        if (shared && dirty) {
            return chi::dat_resptype_e::CopyBackWrData_SD_PD;
        } else if (shared && !dirty) {
            return chi::dat_resptype_e::CopyBackWrData_SC;
        } else if (!shared && dirty) {
            return chi::dat_resptype_e::CopyBackWrData_UD_PD;
        } else { // (!shared && !dirty)
            return chi::dat_resptype_e::CopyBackWrData_UC;
        }
    }

    void UpdateLineStatus() {
        switch (m_opcode) {
        case chi::req_optype_e::WriteCleanFull:
            m_l->SetDirty(false);
        break;
        case chi::req_optype_e::WriteBackPtl:
        case chi::req_optype_e::WriteBackFull:
        case chi::req_optype_e::WriteEvictFull:
            m_l->SetValid(false);
        break;
            //
            // Non copy back don't update cache line status
            // 4.2.3 [1]
            //
        default:
        break;
        }
    }

    bool m_gotDBID;
    bool m_gotComp;
    bool m_sentCompAck;
    chi::req_optype_e m_opcode;
    chi::dat_optype_e m_datOpcode;
};

//
// Handles the non store atomics, AtomicStore is handled with a
// WriteTxn
//
template<int NODE_ID, int ICN_ID>
class AtomicTxn: public ITxn<NODE_ID, ICN_ID> {
public:
    typedef ITxn<NODE_ID, ICN_ID> ITxn_t;

    using ITxn_t::m_gp;
    using ITxn_t::m_chiattr;

    AtomicTxn(tlm::tlm_generic_payload &gp, size_t data_width, chi::chi_ctrl_extension *attr, TxnIdPool *ids)
    : ITxn<NODE_ID, ICN_ID>(attr->req.get_opcode(), data_width, ids, gp.get_data_length(), nullptr)
    , m_gotDBID(false)
    , m_gotCompData(false)
    , m_datOpcode(chi::dat_optype_e::NonCopyBackWrData)
    , m_received(0)
    , m_inboundLen(gp.get_data_length())
    {
        auto len = gp.get_data_length();
        m_gp->set_command(tlm::TLM_WRITE_COMMAND);
        uint64_t addr = gp.get_address();
        if (IsAtomicCompare()) {
            //
            // outbound is the double of inbound length so
            // double up
            //
            len = len * 2;
        }
        m_gp->set_address(addr);
        m_chiattr->req.set_snp_attr(attr->req.is_snp_attr());
        m_chiattr->req.set_snp_attr(attr->req.is_snoop_me());
        this->CopyCHIAttr(gp);
        memset(m_gp->get_data_ptr(), 0, m_gp->get_data_length());
        memset(m_gp->get_byte_enable_ptr(), TLM_BYTE_DISABLED, m_gp->get_byte_enable_length());
        if (IsAtomicCompare()) {
            //
            // Copy over CompareData + Swapdata
            //
            uint64_t addr = gp.get_address();
            unsigned int offsetSwapData;
            auto *data = gp.get_data_ptr();
            auto offsetCompareData = GetLineOffset(addr);
            //
            // If in the CompareData has been placed in
            // middle of the window place SwapData at the
            // begining, else in the middle
            //
            if (offsetCompareData % len) {
                offsetSwapData = offsetCompareData - m_inboundLen;
            } else {
                offsetSwapData = offsetCompareData + m_inboundLen;
            }
            //
            // Place CompareData
            //
            memcpy(&m_data[offsetCompareData], &data[0], m_inboundLen);
            memset(&m_byteEnable[offsetCompareData], TLM_BYTE_ENABLED, m_inboundLen);
            //
            // Place SwapData
            //
            memcpy(&m_data[offsetSwapData], &data[m_inboundLen], m_inboundLen);
            memset(&m_byteEnable[offsetSwapData],
            TLM_BYTE_ENABLED, m_inboundLen);
        } else {
            //
            // Copy over TxnData
            //
            unsigned int offset = GetLineOffset(addr);
            memcpy(&m_data[offset], gp.get_data_ptr(), len);
            memset(&m_byteEnable[offset],
            TLM_BYTE_ENABLED, m_inboundLen);
        }
    }

    bool IsAtomicCompare() {
        auto opcode = m_chiattr->req.get_opcode();
        return opcode == chi::req_optype_e::AtomicCompare;
    }

    unsigned int GetLineOffset(uint64_t addr) {
        return addr & (CACHELINE_SZ - 1);
    }

    virtual bool HandleRxRsp(tlm::tlm_generic_payload &gp) override {
        bool rspHandled = false;
        auto *chiattr = gp.get_extension<chi::chi_ctrl_extension>();
        if (!m_gotDBID && chiattr->resp.get_opcode() == chi::rsp_optype_e::DBIDResp) {
            m_gotDBID = true;
            SetupWriteDat(chiattr);
            rspHandled = true;
        }
        return rspHandled;
    }

    bool TransmitOnTxDatChannel() override {
        bool writeDone = m_gp->get_response_status() != tlm::TLM_INCOMPLETE_RESPONSE;
        return m_gotDBID && !writeDone;
    }

    bool HandleRxDat(tlm::tlm_generic_payload &gp) override {
        auto *chidata = gp.get_extension<chi::chi_data_extension>();
        bool datHandled = false;
        if (!m_gotCompData && chidata->dat.get_opcode() == chi::dat_optype_e::CompData) {
            m_received += CopyDataNonCoherent(gp, chidata);
            //
            // If all has been received
            //
            if (m_received == m_inboundLen) {
                m_gotCompData = true;
            }
            datHandled = true;
        }
        return datHandled;
    }

    bool Done() override {
        bool writeDone = m_gp->get_response_status() != tlm::TLM_INCOMPLETE_RESPONSE;
        return m_gotCompData && m_gotDBID && writeDone;
    }

    unsigned int CopyDataNonCoherent(tlm::tlm_generic_payload &gp, chi::chi_data_extension *chiattr) {
        unsigned char *data = gp.get_data_ptr();
        unsigned int len = gp.get_data_length();
        unsigned int offset = chiattr->dat.get_data_id() * len;
        unsigned char *be = gp.get_byte_enable_ptr();
        unsigned int be_len = gp.get_byte_enable_length();
        unsigned int max_len = CACHELINE_SZ - offset;
        int num_copied = 0;
        assert(len == be_len);
        if (len > max_len) {
            len = max_len;
        }
        if (be_len) {
            unsigned int i;
            for (i = 0; i < len; i++) {
                bool do_access = be[i % be_len] == TLM_BYTE_ENABLED;
                if (do_access) {
                    m_data[i] = data[i];
                    num_copied++;
                }
            }
        } else {
            memcpy(&m_data[offset], data, len);
            num_copied = len;
        }
        return num_copied;
    }

    // Tag must have been checked before calling this function
    unsigned int ReadLine(tlm::tlm_generic_payload &gp, unsigned int pos) {
        unsigned char *data = gp.get_data_ptr() + pos;
        uint64_t addr = gp.get_address() + pos;
        unsigned int len = gp.get_data_length() - pos;
        unsigned int line_offset = GetLineOffset(addr);
        unsigned int max_len = CACHELINE_SZ - line_offset;
        unsigned char *be = gp.get_byte_enable_ptr();
        unsigned int be_len = gp.get_byte_enable_length();
        if (len > max_len) {
            len = max_len;
        }
        if (be_len) {
            unsigned int i;
            for (i = 0; i < len; i++, pos++) {
                bool do_access = be[pos % be_len] == TLM_BYTE_ENABLED;
                if (do_access) {
                    data[i] = m_data[line_offset + i];
                }
            }
        } else {
            memcpy(data, &m_data[line_offset], len);
        }
        return len;
    }

private:
    void SetupWriteDat(chi::chi_ctrl_extension *chiattr) {
        // Writes use SrcID instead HomeNID, 2.6.3 [1]
        this->m_chidata->dat.set_tgt_id(chiattr->get_src_id());
        this->m_chidata->set_txn_id(chiattr->resp.get_db_id());
        this->m_chidata->dat.set_opcode(m_datOpcode);
    }

    bool m_gotDBID;
    bool m_gotCompData;

    chi::dat_optype_e m_datOpcode;

    std::array<uint8_t, CACHELINE_SZ> m_data;
    std::array<uint8_t, CACHELINE_SZ> m_byteEnable;

    unsigned int m_received;
    unsigned int m_inboundLen;
};

template<int NODE_ID, int ICN_ID>
class DVMOpTxn: public ITxn<NODE_ID, ICN_ID> {
public:
    typedef ITxn<NODE_ID, ICN_ID> ITxn_t;

    using ITxn_t::m_gp;
    using ITxn_t::m_chiattr;

    DVMOpTxn(tlm::tlm_generic_payload &gp, size_t data_width, chi::chi_ctrl_extension *chiattr, TxnIdPool *ids)
    : ITxn<NODE_ID, ICN_ID>(chi::req_optype_e::DVMOp, data_width, ids, static_cast<unsigned>(chi::dvm_e::DVMOpSize), nullptr)
    , m_gotDBID(false)
    , m_gotComp(false)
    , m_opcode(chi::req_optype_e::DVMOp)
    , m_datOpcode(chi::dat_optype_e::NonCopyBackWrData)
    {
        m_chiattr = chiattr;
        m_gp->set_address(gp.get_address());
        memcpy(m_gp->get_data_ptr(), gp.get_data_ptr(), static_cast<unsigned>(chi::dvm_e::DVMOpSize));
        memset(m_gp->get_byte_enable_ptr(), TLM_BYTE_ENABLED, static_cast<unsigned>(chi::dvm_e::DVMOpSize));
        // 8.1.4 [1]
        m_chiattr->req.set_non_secure(false);
    }

    bool HandleRxRsp(tlm::tlm_generic_payload &gp) override {
        bool rspHandled = false;
        auto *chiattr = gp.get_extension<chi::chi_ctrl_extension>();
        if (!m_gotComp && chiattr->resp.get_opcode() == chi::rsp_optype_e::Comp) {
            m_gotComp = true;
            rspHandled = true;
        } else if (!m_gotDBID && chiattr->resp.get_opcode() == chi::rsp_optype_e::DBIDResp) {
            m_gotDBID = true;
            SetupWriteDat(chiattr);
            rspHandled = true;
        }
        return rspHandled;
    }

    bool TransmitOnTxDatChannel() override {
        bool writeDone = m_gp->get_response_status() != tlm::TLM_INCOMPLETE_RESPONSE;
        return m_gotDBID && !writeDone;
    }

    bool Done() override {
        bool writeDone = m_gp->get_response_status() != tlm::TLM_INCOMPLETE_RESPONSE;
        return m_gotComp && m_gotDBID && writeDone;
    }

private:
    void SetupWriteDat(chi::chi_ctrl_extension *chiattr) {
        m_gp->set_command(tlm::TLM_WRITE_COMMAND);
        m_gp->set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);
        // Writes use SrcID instead HomeNID, 2.6.3 [1]
        this->m_chidata->dat.set_tgt_id(chiattr->get_src_id());
        this->m_chidata->set_txn_id(chiattr->resp.get_db_id());
        this->m_chidata->dat.set_opcode(m_datOpcode);
    }

    bool m_gotDBID;
    bool m_gotComp;

    chi::req_optype_e m_opcode;
    chi::dat_optype_e m_datOpcode;
};

template<int NODE_ID, int ICN_ID>
class SnpRespTxn: public ITxn<NODE_ID, ICN_ID> {
public:
    typedef ITxn<NODE_ID, ICN_ID> ITxn_t;

    using ITxn_t::m_gp;
    using ITxn_t::m_chiattr;
    using ITxn_t::m_l;
    using ITxn_t::m_txnID;
    using ITxn_t::m_ids;

    SnpRespTxn(tlm::tlm_generic_payload &gp, size_t data_width)
    : ITxn<NODE_ID, ICN_ID>(chi::req_optype_e::ReqLCrdReturn /**/, data_width, nullptr, gp.get_data_length(), nullptr, true)
    , m_dataToHomeNode(false)
    , m_dataToReqNode(false)
    , m_gotCompData(true)
    , m_transmitCompAck(false)
    , m_received(0)
    {
        //
        // Store the snoop req
        //
        m_gp=&gp;
        m_snpReqAttr = gp.get_extension<chi::chi_snp_extension>();
        assert(m_snpReqAttr);
        m_snpReqData = gp.get_extension<chi::chi_data_extension>();
        //
        // Default init as SnpResp without data
        // Always start by Replying the ICN first
        //
        m_snpReqAttr->resp.set_opcode(chi::rsp_optype_e::SnpResp);
        //
        // Take over ownership
        //
    }

    bool HandleRxDat(tlm::tlm_generic_payload &gp) override {
        this->m_chidata = gp.get_extension<chi::chi_data_extension>();
        bool datHandled = false;
        if (m_gotCompData == false && this->m_chidata->dat.get_opcode() == chi::dat_optype_e::CompData) {
            assert(m_l);
            m_received += this->CopyData(gp, this->m_chidata);
            this->ParseReadResp(this->m_chidata);
            if (m_received == CACHELINE_SZ) {
                m_gotCompData = true;
                m_transmitCompAck = true;
                // All done setup CompAck
                this->SetupCompAck(m_snpReqAttr);
            }
            datHandled = true;
        }
        return datHandled;
    }

    void UpdateProgress() override {
        m_received += this->m_data_width/8;
    }

    bool TransmitOnTxDatChannel() override {
        if(m_dataToHomeNode) {
            if (m_received == CACHELINE_SZ) {
                m_dataToHomeNode = false;
                m_dataToReqNode = false;
                m_gotCompData = true;
                return false;
            } else
                return true;
        } else if (m_dataToReqNode) {
            if (m_received == CACHELINE_SZ){
                m_dataToReqNode = false;
                return false;
            } else
                return true;
        }
        return false;
    }
    //
    // First tx on rsp channel is done directly from the snoop
    // handling functions, so a second tx is only done when
    // transmiting CompAck for stash snoops.
    //
    bool TransmitOnTxRspChannel() override {
        bool ret = m_transmitCompAck;
        m_transmitCompAck = false;
        return ret;
    }

    bool TransmitAsAck() override {
        return !m_dataToHomeNode && !m_dataToReqNode && m_gotCompData;
    }

    bool Done() override {
        return !m_dataToHomeNode && !m_dataToReqNode && m_gotCompData;
    }

    void prepare_snoop_data(chi::dat_optype_e opc, uint8_t lineState, bool passDirty) {
        if(!m_snpReqData) {
            m_snpReqData= new chi::chi_data_extension();
            m_gp->set_extension(m_snpReqData);
            m_snpReqData->cmn = m_snpReqAttr->cmn;
        }
        m_snpReqData->dat.set_opcode(opc);
        m_snpReqData->dat.set_resp(GetCompDataResp(lineState, passDirty));
        m_dataToHomeNode = true;
    }

    void SetSnpResp(uint8_t lineState, bool passDirty = false) {
        m_snpReqAttr->resp.set_opcode(chi::rsp_optype_e::SnpResp);
        m_snpReqAttr->resp.set_resp(GetResp(lineState, passDirty));
        //FIXME: not required by spec
        //m_snpReqAttr->resp.set_db_id(m_snpReqAttr->get_txn_id());
    }

    void SetSnpRespData(uint8_t lineState, bool passDirty = false) {
        m_snpReqAttr->resp.set_opcode(chi::rsp_optype_e::SnpResp);
        m_snpReqAttr->resp.set_resp(GetResp(lineState, passDirty));
        //FIXME: not required by spec
        //m_snpReqAttr->resp.set_db_id(m_snpReqAttr->get_txn_id());
        prepare_snoop_data(chi::dat_optype_e::SnpRespData, lineState, passDirty);
    }

    void SetSnpRespDataPtl(uint8_t lineState, bool passDirty = false) {
        m_snpReqAttr->resp.set_opcode(chi::rsp_optype_e::SnpResp);
        m_snpReqAttr->resp.set_resp(GetResp(lineState, passDirty));
        //FIXME: not required by spec
        //m_snpReqAttr->resp.set_db_id(m_snpReqAttr->get_txn_id());
        prepare_snoop_data(chi::dat_optype_e::SnpRespDataPtl, lineState, passDirty);
     }

    void SetSnpRespFwded(uint8_t lineState, uint8_t fwdedState, bool passDirty = false, bool fwdedPassDirty = false) {
        m_snpReqAttr->resp.set_opcode(chi::rsp_optype_e::SnpRespFwded);
        m_snpReqAttr->resp.set_resp(GetResp(lineState, passDirty));
        m_snpReqAttr->resp.set_fwd_state(static_cast<uint8_t>(GetResp(fwdedState, fwdedPassDirty)));
    }

    void SetSnpRespDataFwded(uint8_t lineState, uint8_t fwdedState, bool passDirty = false, bool fwdedPassDirty = false) {
        m_snpReqAttr->resp.set_opcode(chi::rsp_optype_e::SnpRespFwded);
        m_snpReqAttr->resp.set_resp(GetResp(lineState, passDirty));
        m_snpReqAttr->resp.set_fwd_state(static_cast<uint8_t>(GetResp(fwdedState, fwdedPassDirty)));
        prepare_snoop_data(chi::dat_optype_e::SnpRespDataFwded, lineState, passDirty);
    }

    void SetData(CacheLine *l) {
        m_gp->set_data_ptr(l->GetData());
        m_gp->set_byte_enable_ptr(l->GetByteEnables());
        m_gp->set_byte_enable_length(CACHELINE_SZ);
    }

    void SetDataPull(CacheLine *l, bool dataPull) {
        m_l = l;
        m_snpReqAttr->resp.set_data_pull(true);
        m_gotCompData = false;
    }

    bool GetDataPull() {
        return m_snpReqAttr->resp.get_data_pull();
    }

    uint8_t GetDBID() override {
        return m_snpReqAttr->resp.get_db_id();
    }

    void SetupDBID(TxnIdPool *ids) {
        //
        // Setup ID release on delete
        //
        m_ids = ids;
        //m_txnID = ids->GetID();
        //m_snpReqAttr->resp.set_db_id(m_txnID);
        auto db_id = m_snpReqAttr->get_txn_id();
        m_snpReqAttr->resp.set_db_id(db_id);
        if(m_snpReqData) m_snpReqData->dat.set_db_id(db_id);
    }

    bool GetDataToHomeNode() {
        return m_dataToHomeNode;
    }

    void SetCompData(CacheLine *l, uint8_t lineState, bool passDirty = false) {
        m_snpReqAttr->set_src_id(NODE_ID);
        m_snpReqAttr->set_txn_id(m_snpReqAttr->req.get_fwd_txn_id());
        m_snpReqAttr->resp.set_db_id(m_snpReqAttr->get_txn_id());
        if(m_snpReqData) {
            m_snpReqData->dat.set_resp(GetCompDataResp(lineState, passDirty));
            m_snpReqData->dat.set_home_n_id(m_snpReqAttr->get_src_id());
            m_snpReqData->dat.set_opcode(chi::dat_optype_e::CompData);
        }
        m_dataToReqNode = true;
    }

private:
    // Table 12-36
    static chi::rsp_resptype_e GetResp(uint8_t lineState, bool passDirty) {
        if (passDirty) {
            switch (lineState) {
            case UC:
                return chi::rsp_resptype_e::SnpRespData_UC_PD;
            case SC:
                return chi::rsp_resptype_e::SnpRespData_SC_PD;
            case INV:
            default:
                return chi::rsp_resptype_e::SnpRespData_I_PD;
            }
        } else {
            switch (lineState) {
            case UD:
                return chi::rsp_resptype_e::SnpRespData_UD;
            case UC:
                return chi::rsp_resptype_e::SnpRespData_UC;
            case SD:
                return chi::rsp_resptype_e::SnpRespData_SD;
            case SC:
                return chi::rsp_resptype_e::SnpRespData_SC;
            case INV:
            default:
                return chi::rsp_resptype_e::SnpRespData_I;
            }
        }
    }

    static chi::dat_resptype_e GetCompDataResp(uint8_t lineState, bool passDirty) {
        if (passDirty) {
            switch (lineState) {
            case UD:
                return chi::dat_resptype_e::SnpRespData_UC_PD;
            case SD:
                return chi::dat_resptype_e::SnpRespData_SC_PD;
            case INV:
            default:
                return chi::dat_resptype_e::SnpRespData_I_PD;
            }
        } else {
            switch (lineState) {
            case UD:
                return chi::dat_resptype_e::SnpRespData_UD;
            case UC:
                return chi::dat_resptype_e::SnpRespData_UC;
            case SD:
                return chi::dat_resptype_e::SnpRespData_SD;
            case SC:
                return chi::dat_resptype_e::SnpRespData_SC;
            case INV:
            default:
                return chi::dat_resptype_e::SnpRespData_I;
            }
        }
    }

    bool m_dataToHomeNode;
    bool m_dataToReqNode;
    bool m_gotCompData;
    bool m_transmitCompAck;

    chi::chi_snp_extension* m_snpReqAttr;
    chi::chi_data_extension* m_snpReqData;

    unsigned char m_dummy_data[CACHELINE_SZ];
    unsigned int m_received;
};

} /* namespace RN */
} /* namespace CHI */
} /* namespace AMBA */

#endif /* TLM_MODULES_PRIV_CHI_TXNS_RN_H__ */
