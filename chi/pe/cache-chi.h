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
 * References:
 *
 * [1] AMBA 5 CHI Architecture Specification, ARM IHI 0050C, ID050218
 *
 */

#ifndef _CHI_PE_CACHE_CHI_H_
#define _CHI_PE_CACHE_CHI_H_

#include <chi/pe/impl/txnidpool.h>
#include <tlm>
#include <tlm_utils/simple_initiator_socket.h>
#include <tlm_utils/simple_target_socket.h>
#include "impl/cacheline.h"
#include "impl/txns-rn.h"
#include <scc/ordered_semaphore.h>
#include <scc/report.h>
#include <deque>
#include <memory>

namespace chi {
namespace pe {

template<int NODE_ID, int CACHE_SZ, int ICN_ID = 20>
class cache_chi: public sc_core::sc_module {
private:
    enum {
        NUM_CACHELINES = CACHE_SZ / CACHELINE_SZ
    };

    using CacheLine   = RN::CacheLine ;
    using ITxn        = RN::ITxn<NODE_ID, ICN_ID> ;
    using ReadTxn     = RN::ReadTxn<NODE_ID, ICN_ID> ;
    using DatalessTxn = RN::DatalessTxn<NODE_ID, ICN_ID> ;
    using WriteTxn    = RN::WriteTxn<NODE_ID, ICN_ID> ;
    using AtomicTxn   = RN::AtomicTxn<NODE_ID, ICN_ID> ;
    using DVMOpTxn    = RN::DVMOpTxn<NODE_ID, ICN_ID> ;
    using SnpRespTxn  = RN::SnpRespTxn<NODE_ID, ICN_ID> ;

    struct ITransmitter {
        virtual ~ITransmitter() = default;
        virtual void ProcessSnp(ITxn *t) = 0;
        virtual void Process(ITxn *t) = 0;
    };

    using send_flit_fct = void(chi::chi_payload&, tlm::tlm_phase const&, tlm::tlm_phase const&);
    class TxChannel: public sc_core::sc_module {
    public:
        enum channel_type {REQ, RESP, DAT};

        sc_core::sc_in<bool> clk_i{"clk_i"};

        SC_HAS_PROCESS(TxChannel);

        TxChannel(sc_core::sc_module_name name, channel_type ch_type, std::function<send_flit_fct> send_flit)
        : sc_core::sc_module(name)
        , ch_type(ch_type)
        , send_flit(send_flit)
        , m_transmitter(NULL) {
            SC_THREAD(tx_thread);
        }

        void ProcessAndWait(ITxn &t) {
            Process(&t);
            wait(t.DoneEvent());
        }

        void Process(ITxn *t) {
            m_txList.push_back(t);
            m_listEvent.notify();
        }

        void SetTransmitter(ITransmitter *transmitter) {
            m_transmitter = transmitter;
        }

    private:
        void tx_thread() {
            while (true) {
                if (m_txList.empty())
                    wait(m_listEvent);
                auto* t = m_txList.front();
                m_txList.pop_front();
                sc_core::sc_time delay(sc_core::SC_ZERO_TIME);
                tlm::tlm_phase beg_ph{tlm::BEGIN_REQ};
                tlm::tlm_phase end_ph{tlm::END_REQ};
                if(t->TransmitAsAck()) {
                    beg_ph = chi::ACK;
                    end_ph = chi::ACK;
                } else if(ch_type==DAT) {
                    if(t->GetNextProgress()>=t->GetGP().get_data_length()) {
                        beg_ph = chi::BEGIN_DATA;
                        end_ph = chi::END_DATA;
                    } else {
                        beg_ph = chi::BEGIN_PARTIAL_DATA;
                        end_ph = chi::END_PARTIAL_DATA;
                    }
                } else if(ch_type==RESP && t->IsSnp()) {
                    beg_ph = tlm::BEGIN_RESP;
                    end_ph = tlm::END_RESP;
                }
                send_flit(t->GetGP(), beg_ph, end_ph);
                if(ch_type==DAT)
                    SCCTRACE(SCMOD) << "TDAT flit sent " << ", addr: 0x" << std::hex << t->GetGP().get_address();
                if (m_transmitter && t->IsSnp()) {
                    if(ch_type==channel_type::DAT)
                        t->UpdateProgress();
                    m_transmitter->ProcessSnp(t);
                } else
                    if (m_transmitter && ((t->IsWrite() && !t->Done()) || t->GetIsWriteUniqueWithCompAck())) {
                        t->UpdateProgress();
                        m_transmitter->Process(t);
                    } else if (t->Done()) {
                        t->DoneEvent().notify();
                    }
                wait(clk_i.posedge_event());
            }
        }
        channel_type const ch_type;
        std::function<send_flit_fct> send_flit;
        std::deque<ITxn*> m_txList;
        sc_core::sc_event m_listEvent, responseEvent;
        ITransmitter *m_transmitter;
    };

    class Transmitter: public ITransmitter {
    public:
        Transmitter(TxChannel &txRspChannel, TxChannel &txDatChannel)
        : m_txRspChannel(txRspChannel)
        , m_txDatChannel(txDatChannel)
        { }

        void Process(ITxn *t) {
            if (t->TransmitOnTxDatChannel())
                m_txDatChannel.Process(t);
            if (t->TransmitOnTxRspChannel())
                m_txRspChannel.Process(t);
            if (t->Done())
                t->DoneEvent().notify();
        }

        void ProcessSnp(ITxn *t) {
            if (t->TransmitOnTxDatChannel())
                m_txDatChannel.Process(t);
            else if (t->TransmitOnTxRspChannel()) {
                m_txRspChannel.Process(t);
            } else if (t->Done()) {
                delete t;
            }
            // Else the txn is waiting on the rx channels
        }

    private:
        TxChannel &m_txRspChannel;
        TxChannel &m_txDatChannel;
    };

    class ICache {
    public:
        ICache(size_t data_width, TxChannel &txReqChannel, TxChannel &txRspChannel, TxChannel &txDatChannel, TxnIdPool *ids, ITxn **txn)
        : m_data_width(data_width)
        , m_txReqChannel(txReqChannel)
        , m_txRspChannel(txRspChannel)
        , m_txDatChannel(txDatChannel)
        , m_txn_id_pool(ids)
        , m_txn(txn)
        , m_randomize(false)
        , m_seed(0) {
        }

        virtual ~ICache() = default;

        virtual void HandleLoad(tlm::tlm_generic_payload &gp) = 0;
        virtual void HandleStore(tlm::tlm_generic_payload &gp) = 0;

        inline static unsigned int get_line_offset(uint64_t addr) {
            return addr & (CACHELINE_SZ - 1);
        }

        inline static uint64_t align_address(uint64_t addr) {
            return addr & ~(CACHELINE_SZ - 1);
        }

        inline static int64_t get_tag(uint64_t addr) {
            return align_address(addr);
        }

        inline static uint64_t get_index(uint64_t tag) {
            return (tag % CACHE_SZ) / CACHELINE_SZ;
        }

        CacheLine* get_line(uint64_t addr) {
            uint64_t tag = get_tag(addr);
            unsigned int index = get_index(tag);

            return &m_cacheline[index];
        }

        // Tag must have been checked before calling this function
        unsigned int ReadLine(tlm::tlm_generic_payload &gp, unsigned int pos) {
            unsigned char *data = gp.get_data_ptr() + pos;
            uint64_t addr = gp.get_address() + pos;
            unsigned int len = gp.get_data_length() - pos;
            unsigned int line_offset = get_line_offset(addr);
            unsigned int max_len = CACHELINE_SZ - line_offset;
            unsigned char *be = gp.get_byte_enable_ptr();
            unsigned int be_len = gp.get_byte_enable_length();
            CacheLine *l = get_line(addr);
            uint8_t *lineData = l->GetData();

            if (len > max_len) {
                len = max_len;
            }

            if (be_len) {
                unsigned int i;

                for (i = 0; i < len; i++, pos++) {
                    bool do_access = be[pos % be_len] == TLM_BYTE_ENABLED;

                    if (do_access) {
                        data[i] = lineData[line_offset + i];
                    }
                }
            } else {
                memcpy(data, &lineData[line_offset], len);
            }

            return len;
        }

        // Tag must have been checked before calling this function
        unsigned int WriteLine(tlm::tlm_generic_payload &gp, unsigned int pos) {
            unsigned char *data = gp.get_data_ptr() + pos;
            uint64_t addr = gp.get_address() + pos;
            unsigned int len = gp.get_data_length() - pos;
            unsigned int line_offset = get_line_offset(addr);
            unsigned int max_len = CACHELINE_SZ - line_offset;
            unsigned char *be = gp.get_byte_enable_ptr();
            unsigned int be_len = gp.get_byte_enable_length();
            CacheLine *l = get_line(addr);

            if (len > max_len) {
                len = max_len;
            }

            l->Write(line_offset, data, be, len, pos);

            if (l->GetFillGrade() != CacheLine::Empty) {
                l->SetDirty(true);
            }

            return len;
        }

        bool InCache(uint64_t addr, bool nonSecure, bool requireFullState = false) {
            uint64_t tag = get_tag(addr);
            CacheLine *l = get_line(addr);

            if (!l->IsValid()) {
                return false;
            }

            if (requireFullState && l->GetFillGrade() != CacheLine::Full) {
                return false;
            }

            return l->GetTag() == tag && nonSecure == l->GetNonSecure();
        }

        bool IsUnique(uint64_t addr) {
            CacheLine *l = get_line(addr);

            return l->GetShared() == false;
        }

        unsigned int ToWrite(tlm::tlm_generic_payload &gp, unsigned int pos) {
            uint64_t addr = gp.get_address() + pos;
            unsigned int len = gp.get_data_length() - pos;
            unsigned int line_offset = this->get_line_offset(addr);
            unsigned int max_len = CACHELINE_SZ - line_offset;

            if (len > max_len) {
                len = max_len;
            }

            return len;
        }

        inline void process_txn(ITxn &t) {
            m_txn[t.GetTxnID()] = &t;
            m_txReqChannel.ProcessAndWait(t);
            m_txn[t.GetTxnID()] = nullptr;
        }

        void DVMOperation(tlm::tlm_generic_payload &gp, chi::chi_ctrl_extension *chiattr) {
            DVMOpTxn t(gp, m_data_width, chiattr, m_txn_id_pool);
            assert(m_txn[t.GetTxnID()] == NULL);
            process_txn(t);
        }

        void AtomicNonStore(tlm::tlm_generic_payload &gp, chi::chi_ctrl_extension *chiattr) {
            AtomicTxn t(gp, m_data_width, chiattr, m_txn_id_pool);
            assert(m_txn[t.GetTxnID()] == NULL);
            process_txn(t);
            t.ReadLine(gp, 0);
        }

        void AtomicStore(tlm::tlm_generic_payload &gp, chi::chi_ctrl_extension *chiattr) {
            uint64_t addr = gp.get_address();
            WriteTxn t(gp, chiattr->req.get_opcode(), m_txn_id_pool, addr, m_data_width, 0, get_line_offset(addr));
            assert(m_txn[t.GetTxnID()] == NULL);
            process_txn(t);
        }

        unsigned int WriteNoSnp(tlm::tlm_generic_payload &gp, chi::req_optype_e opcode, uint64_t addr, unsigned int pos) {
            WriteTxn t(gp, opcode, m_txn_id_pool, get_tag(addr), m_data_width, pos, get_line_offset(addr));
            unsigned int len = t.GetGP().get_data_length();
            assert(m_txn[t.GetTxnID()] == NULL);
            process_txn(t);
            if (len > CACHELINE_SZ || len==0) {
                len = CACHELINE_SZ;
            }
            return len;
        }

        unsigned int WriteUnique(tlm::tlm_generic_payload &gp, chi::req_optype_e opcode, uint64_t addr, unsigned int pos = 0) {
            WriteTxn t(gp, opcode, m_txn_id_pool, get_tag(addr), m_data_width, pos, get_line_offset(addr));
            unsigned int len = gp.get_data_length() - pos;
            t.SetExpCompAck(GetRandomBool());
            assert(m_txn[t.GetTxnID()] == NULL);
            process_txn(t);
            if (len > CACHELINE_SZ) {
                len = CACHELINE_SZ;
            }
            return len;
        }

        void WriteBack(CacheLine *l, chi::req_optype_e opcode, tlm::tlm_generic_payload &gp) {
            WriteTxn t(opcode, m_data_width, l, m_txn_id_pool);
            assert(l && l->IsValid() && l->GetDirty());
            assert(m_txn[t.GetTxnID()] == NULL);
            process_txn(t);
        }

        void WriteClean(CacheLine *l, chi::req_optype_e opcode, tlm::tlm_generic_payload &gp) {
            WriteTxn t(opcode, m_data_width, l, m_txn_id_pool);
            assert(l && l->IsValid() && l->GetDirty());
            assert(m_txn[t.GetTxnID()] == NULL);
            process_txn(t);
        }

        void WriteEvictFull(CacheLine *l, tlm::tlm_generic_payload &gp) {
            WriteTxn t(chi::req_optype_e::WriteEvictFull, m_data_width, l, m_txn_id_pool);
            assert(l && l->IsValid() && !l->GetDirty());
            assert(m_txn[t.GetTxnID()] == NULL);
            process_txn(t);
        }

        void Evict(CacheLine *l, tlm::tlm_generic_payload &gp) {
            DatalessTxn t(chi::req_optype_e::Evict, l->GetTag(), l, m_txn_id_pool);
            assert(l && l->IsValid());
            //
            // Special case for evict, invalidate (silent cache
            // state transition 4.6 [1]) before issuing is required
            // (4.7.2 [1]).
            //
            l->SetValid(false);
            process_txn(t);
        }

        void Stash(tlm::tlm_generic_payload &gp, chi::req_optype_e opc) {
            DatalessTxn t(opc, ICache::align_address(gp.get_address()), nullptr, m_txn_id_pool, &gp);
            process_txn(t);
        }

       void InvalidateCacheLine(CacheLine *l, tlm::tlm_generic_payload &gp) {
            //
            // Sometimes do a WriteCleanFull first
            //
            if (l->IsDirtyFull() && GetRandomBool()) {
                WriteClean(l, chi::req_optype_e::WriteCleanFull, gp);
            }

            //
            // Line might have been invalidated by a snoop here if
            // we issued a WriteCleanFull so check if it is valid
            //
            if (l->IsValid()) {
                if (l->IsDirtyPartial()) {
                    WriteBack(l, chi::req_optype_e::WriteBackPtl, gp);
                } else
                    if ((l->IsDirty())) {
                        WriteBack(l, chi::req_optype_e::WriteBackFull, gp);
                    } else {
                        //
                        // Line is clean, if line is UC do a
                        // WriteEvictFull sometimes
                        //
                        if (l->IsUniqueCleanFull() && GetRandomBool()) {
                            WriteEvictFull(l, gp);
                        } else {
                            Evict(l, gp);
                        }
                    }
            }
        }

        unsigned int ReadNoSnp(tlm::tlm_generic_payload &gp, uint64_t addr, unsigned int pos) {
            ReadTxn t(gp, chi::req_optype_e::ReadNoSnp, get_tag(addr), m_data_width, // 2.10.2 + 4.2.1
                    NULL, m_txn_id_pool);
            process_txn(t);
            return t.FetchNonCoherentData(gp, pos, get_line_offset(addr));
        }

        unsigned int ReadOnce(tlm::tlm_generic_payload &gp, chi::req_optype_e opcode, uint64_t addr, unsigned int pos) {
            CacheLine *l = get_line(addr);
            ReadTxn t(gp, opcode, get_tag(addr), m_data_width, // 2.10.2 + 4.2.1
                    NULL, m_txn_id_pool);
            //
            // Line might be in the cache but not with fillgrade
            // full
            //
            if (l->IsValid()) {
                InvalidateCacheLine(l, gp);
            }
            process_txn(t);
            return t.FetchNonCoherentData(gp, pos, get_line_offset(addr));
        }

        void ReadShared(tlm::tlm_generic_payload &gp, uint64_t addr) {
            CacheLine *l = get_line(addr);
            ReadTxn t(gp, chi::req_optype_e::ReadShared, get_tag(addr), m_data_width, // 2.10.2 + 4.2.1
                    l, m_txn_id_pool);
            if (l->IsValid()) {
                InvalidateCacheLine(l, gp);
            }
            process_txn(t);
        }

        void ReadClean(tlm::tlm_generic_payload &gp, uint64_t addr) {
            CacheLine *l = get_line(addr);
            ReadTxn t(gp, chi::req_optype_e::ReadClean, get_tag(addr), m_data_width, // 2.10.2 + 4.2.1
                    l, m_txn_id_pool);
            if (l->IsValid()) {
                InvalidateCacheLine(l, gp);
            }
            process_txn(t);
        }

        void ReadNotSharedDirty(tlm::tlm_generic_payload &gp, uint64_t addr) {
            CacheLine *l = get_line(addr);
            ReadTxn t(gp, chi::req_optype_e::ReadNotSharedDirty, get_tag(addr), m_data_width, // 2.10.2 + 4.2.1
                    l, m_txn_id_pool);
            if (l->IsValid()) {
                InvalidateCacheLine(l, gp);
            }
            process_txn(t);
        }

        void ReadUnique(tlm::tlm_generic_payload &gp, chi::req_optype_e opc, uint64_t addr) {
            CacheLine *l = get_line(addr);
            ReadTxn t(gp, opc, get_tag(addr), m_data_width, // 2.10.2 + 4.2.1
                    l, m_txn_id_pool);
            if (l->IsValid()) {
                InvalidateCacheLine(l, gp);
            }
            process_txn(t);
        }

        void MakeUnique(tlm::tlm_generic_payload &gp, uint64_t addr) {
            CacheLine *l = get_line(addr);
            DatalessTxn t(chi::req_optype_e::MakeUnique, get_tag(addr), l, m_txn_id_pool, &gp);
            if (l->IsValid()) {
                InvalidateCacheLine(l, gp);
            }
            process_txn(t);
        }

        void MakeInvalid(tlm::tlm_generic_payload &gp, uint64_t addr) {
            CacheLine *l = get_line(addr);
            DatalessTxn t(chi::req_optype_e::MakeInvalid, get_tag(addr), l, m_txn_id_pool);
            if (l->IsValid()) {
                InvalidateCacheLine(l, gp);
            }
            process_txn(t);
        }

        void CleanUnique(tlm::tlm_generic_payload &gp, uint64_t addr) {
            CacheLine *l = get_line(addr);
            DatalessTxn t(chi::req_optype_e::CleanUnique, l->GetTag(), l, m_txn_id_pool);
            process_txn(t);
        }

        void CleanInvalid(tlm::tlm_generic_payload &gp, uint64_t addr) {
            CacheLine *l = get_line(addr);
            DatalessTxn t(chi::req_optype_e::CleanInvalid, l->GetTag(), l, m_txn_id_pool);
            process_txn(t);
        }

        void CleanShared(tlm::tlm_generic_payload &gp, chi::req_optype_e opc, uint64_t addr) {
            CacheLine *l = get_line(addr);
            DatalessTxn t(opc, l->GetTag(), l, m_txn_id_pool);
            process_txn(t);
        }

        void PrefetchTgt(tlm::tlm_generic_payload &gp, uint64_t addr) {
            DatalessTxn t(chi::req_optype_e::PrefetchTgt, addr, nullptr, m_txn_id_pool, &gp);
            process_txn(t);
        }

        //
        // See 4.7.6 [1]
        //
        bool HandleSnp(tlm::tlm_generic_payload &gp, chi::chi_snp_extension *chiattr) {
            bool ret = true;
            bool nonSecure = chiattr->req.is_non_secure();
            uint64_t addr = gp.get_address();
            //CacheLine *l = get_line(gp.get_address());

            if (chiattr->req.get_opcode() == chi::snp_optype_e::SnpDVMOp) {
                uint8_t txnID = chiattr->get_txn_id();

                //
                // Check if the other packet to this DVM
                // request has been received, if so output a
                // response, else just mark that it was
                // received.
                //
                if (m_receivedDVM[txnID]) {
                    SnpRespTxn *t = new SnpRespTxn(gp, m_data_width);

                    m_receivedDVM[txnID] = false;

                    t->SetSnpResp(INV);
                    m_txRspChannel.Process(t);
                } else {
                    m_receivedDVM[txnID] = true;
                }
            } else
                if (InCache(addr, nonSecure)) {
                    switch (chiattr->req.get_opcode()) {
                    case chi::snp_optype_e::SnpOnce:
                        HandleSnpOnce(gp, chiattr);
                        break;
                    case chi::snp_optype_e::SnpClean:
                    case chi::snp_optype_e::SnpShared:
                    case chi::snp_optype_e::SnpNotSharedDirty:
                        HandleSnpClean(gp, chiattr);
                        break;
                    case chi::snp_optype_e::SnpUnique:
                        HandleSnpUnique(gp, chiattr);
                        break;
                    case chi::snp_optype_e::SnpCleanShared:
                        HandleSnpCleanShared(gp, chiattr);
                        break;
                    case chi::snp_optype_e::SnpCleanInvalid:
                        HandleSnpCleanInvalid(gp, chiattr);
                        break;
                    case chi::snp_optype_e::SnpMakeInvalid:
                        HandleSnpMakeInvalid(gp, chiattr);
                        break;
                    case chi::snp_optype_e::SnpUniqueStash:
                        HandleSnpUniqueStash(gp, chiattr);
                        break;
                    case chi::snp_optype_e::SnpMakeInvalidStash:
                        HandleSnpMakeInvalidStash(gp, chiattr);
                        break;
                    case chi::snp_optype_e::SnpStashUnique:
                        HandleSnpStashUnique(gp, chiattr);
                        break;
                    case chi::snp_optype_e::SnpStashShared:
                        HandleSnpStashShared(gp, chiattr);
                        break;
                    case chi::snp_optype_e::SnpOnceFwd:
                        HandleSnpOnceFwd(gp, chiattr);
                        break;
                    case chi::snp_optype_e::SnpCleanFwd:
                        HandleSnpCleanFwd(gp, chiattr);
                        break;
                    case chi::snp_optype_e::SnpNotSharedDirtyFwd:
                        HandleSnpNotSharedDirtyFwd(gp, chiattr);
                        break;
                    case chi::snp_optype_e::SnpSharedFwd:
                        HandleSnpSharedFwd(gp, chiattr);
                        break;
                    case chi::snp_optype_e::SnpUniqueFwd:
                        HandleSnpUniqueFwd(gp, chiattr);
                        break;
                    default:
                        ret = false;
                        break;
                    }

                    if (!get_line(gp.get_address())->IsValid()) {
                        m_monitor.Reset(gp.get_address());
                    }

                } else {
                    switch (chiattr->req.get_opcode()) {
//                    case chi::snp_optype_e::SnpOnce:
//                        HandleSnpOnce(gp, chiattr);
//                        break;
//                    case chi::snp_optype_e::SnpClean:
//                    case chi::snp_optype_e::SnpShared:
//                    case chi::snp_optype_e::SnpNotSharedDirty:
//                        HandleSnpClean(gp, chiattr);
//                        break;
//                    case chi::snp_optype_e::SnpUnique:
//                        HandleSnpUnique(gp, chiattr);
//                        break;
//                    case chi::snp_optype_e::SnpCleanShared:
//                        HandleSnpCleanShared(gp, chiattr);
//                        break;
//                    case chi::snp_optype_e::SnpCleanInvalid:
//                        HandleSnpCleanInvalid(gp, chiattr);
//                        break;
//                    case chi::snp_optype_e::SnpMakeInvalid:
//                        HandleSnpMakeInvalid(gp, chiattr);
//                        break;
                    case chi::snp_optype_e::SnpUniqueStash:
                        HandleSnpUniqueStash(gp, chiattr);
                        break;
                    case chi::snp_optype_e::SnpMakeInvalidStash:
                        HandleSnpMakeInvalidStash(gp, chiattr);
                        break;
                    case chi::snp_optype_e::SnpStashUnique:
                        HandleSnpStashUnique(gp, chiattr);
                        break;
                    case chi::snp_optype_e::SnpStashShared:
                        HandleSnpStashShared(gp, chiattr);
                        break;
//                    case chi::snp_optype_e::SnpOnceFwd:
//                        HandleSnpOnceFwd(gp, chiattr);
//                        break;
//                    case chi::snp_optype_e::SnpCleanFwd:
//                        HandleSnpCleanFwd(gp, chiattr);
//                        break;
//                    case chi::snp_optype_e::SnpNotSharedDirtyFwd:
//                        HandleSnpNotSharedDirtyFwd(gp, chiattr);
//                        break;
//                    case chi::snp_optype_e::SnpSharedFwd:
//                        HandleSnpSharedFwd(gp, chiattr);
//                        break;
//                    case chi::snp_optype_e::SnpUniqueFwd:
//                        HandleSnpUniqueFwd(gp, chiattr);
//                        break;
                    default:
                        SnpRespTxn *t = new SnpRespTxn(gp, m_data_width);
                        t->SetSnpResp(INV);
                        m_txRspChannel.Process(t);
                        break;
                    }


                }

            return ret;
        }

        // Table 4-16 [1]
        void HandleSnpOnce(tlm::tlm_generic_payload &gp, chi::chi_snp_extension *chiattr) {
            CacheLine *l = get_line(gp.get_address());
            SnpRespTxn *t = new SnpRespTxn(gp, m_data_width);

            switch (l->GetStatus()) {
            case UC:
                // SnpRespData_UC;
                t->SetSnpRespData(UC);
                break;
            case UCE:
                // SnpResp_UC;
                t->SetSnpResp(UC);
                break;
            case UD:
                // SnpRespData_UD;
                t->SetSnpRespData(UD);
                break;
            case UDP:
                // SnpRespDataPtl_UD;
                t->SetSnpRespDataPtl(UD);
                break;
            case SC:
                if (chiattr->req.is_ret_to_src()) {
                    // SnpRespData_SC;
                    t->SetSnpRespData(SC);
                } else {
                    // SnpResp_SC;
                    t->SetSnpResp(SC);
                }
                break;
            case SD:
                // SnpRespData_SD;
                t->SetSnpRespData(SD);
                break;
            case INV:
            default:
                // SnpResp_I;
                t->SetSnpResp(INV);
                break;
            }

            if (t->GetDataToHomeNode()) {
                t->SetData(l);
                m_txDatChannel.Process(t);
            } else {
                m_txRspChannel.Process(t);
            }
        }

        // Table 4-17 [1]
        void HandleSnpClean(tlm::tlm_generic_payload &gp, chi::chi_snp_extension *chiattr) {
            CacheLine *l = get_line(gp.get_address());
            SnpRespTxn *t = new SnpRespTxn(gp, m_data_width);

            switch (l->GetStatus()) {
            case UC:
                // Go to SC
                l->SetShared(true);

                // SnpRespData_SC;
                t->SetSnpRespData(SC);
                break;
            case UCE:
                // Go to I
                l->SetValid(false);

                // SnpResp_I;
                t->SetSnpResp(INV);
                break;
            case UD:
                // Go to SC
                l->SetShared(true);

                // SnpRespData_SC_PD;
                t->SetSnpRespData(SC, true);
                break;
            case UDP:
                // Go to I
                l->SetValid(false);

                // SnpRespDataPtl_I_PD;
                t->SetSnpRespDataPtl(INV, true);
                break;
            case SC:
                // Stay in SC
                if (chiattr->req.is_ret_to_src()) {
                    // SnpRespData_SC;
                    t->SetSnpRespData(SC);
                } else {
                    // SnpResp_SC;
                    t->SetSnpResp(SC);
                }
                break;
            case SD:
                // Go to SC;
                l->SetDirty(false);

                // SnpRespData_SC_PD;
                t->SetSnpRespData(SC, true);
                break;
            case INV:
            default:
                // SnpResp_I;
                t->SetSnpResp(INV);
                break;
            }

            if (t->GetDataToHomeNode()) {
                t->SetData(l);
                m_txDatChannel.Process(t);
            } else {
                m_txRspChannel.Process(t);
            }
        }

        // Table 4-18 [1]
        void HandleSnpUnique(tlm::tlm_generic_payload &gp, chi::chi_snp_extension *chiattr) {
            CacheLine *l = get_line(gp.get_address());
            SnpRespTxn *t = new SnpRespTxn(gp, m_data_width);

            switch (l->GetStatus()) {
            case UC:
                // SnpRespData_I;
                t->SetSnpRespData(INV);
                break;
            case UCE:
                // SnpResp_I;
                t->SetSnpResp(INV);
                break;
            case UD:
                // SnpRespData_I_PD;
                t->SetSnpRespData(INV, true);
                break;
            case UDP:
                // SnpRespDataPtl_I_PD;
                t->SetSnpRespDataPtl(INV, true);
                break;
            case SC:
                // Stay in SC
                if (chiattr->req.is_ret_to_src()) {
                    // SnpRespData_I;
                    t->SetSnpRespData(INV);
                } else {
                    // SnpResp_I;
                    t->SetSnpResp(INV);
                }
                break;
            case SD:
                // SnpRespData_I_PD;
                t->SetSnpRespData(INV, true);
                break;
            case INV:
            default:
                // SnpResp_I;
                t->SetSnpResp(INV);
                break;
            }

            //
            // Invalidate line
            //
            l->SetValid(false);

            if (t->GetDataToHomeNode()) {
                t->SetData(l);
                m_txDatChannel.Process(t);
            } else {
                m_txRspChannel.Process(t);
            }
        }

        // Table 4-19 [1]
        void HandleSnpCleanShared(tlm::tlm_generic_payload &gp, chi::chi_snp_extension *chiattr) {
            CacheLine *l = get_line(gp.get_address());
            SnpRespTxn *t = new SnpRespTxn(gp, m_data_width);

            switch (l->GetStatus()) {
            case UC:
                // SnpResp_UC;
                t->SetSnpResp(UC);
                break;
            case UCE:
                // Go to I
                l->SetValid(false);

                // SnpResp_I;
                t->SetSnpResp(INV);
                break;
            case UD:
                // Go to UC
                l->SetDirty(false);

                // SnpRespData_UC_PD;
                t->SetSnpRespData(UC, true);
                break;
            case UDP:
                // Go to I
                l->SetValid(false);

                // SnpRespDataPtl_I_PD;
                t->SetSnpRespDataPtl(INV, true);
                break;
            case SC:
                // Stay in SC

                // SnpResp_SC;
                t->SetSnpResp(SC);
                break;
            case SD:
                // Go to SC
                l->SetDirty(false);

                // SnpRespData_SC_PD;
                t->SetSnpRespData(SC, true);
                break;
            case INV:
            default:
                // SnpResp_I;
                t->SetSnpResp(INV);
                break;
            }

            if (t->GetDataToHomeNode()) {
                t->SetData(l);
                m_txDatChannel.Process(t);
            } else {
                m_txRspChannel.Process(t);
            }
        }

        // Table 4-19 [1]
        void HandleSnpCleanInvalid(tlm::tlm_generic_payload &gp, chi::chi_snp_extension *chiattr) {
            CacheLine *l = get_line(gp.get_address());
            SnpRespTxn* t = new SnpRespTxn(gp, m_data_width);

            switch (l->GetStatus()) {
            case UC:
                // SnpResp_I;
                t->SetSnpResp(INV);
                break;
            case UCE:
                // SnpResp_I;
                t->SetSnpResp(INV);
                break;
            case UD:
                // SnpRespData_I_PD;
                t->SetSnpRespData(INV, true);
                break;
            case UDP:
                // SnpRespDataPtl_I_PD;
                t->SetSnpRespDataPtl(INV, true);
                break;
            case SC:
                // SnpResp_I;
                t->SetSnpResp(INV);
                break;
            case SD:
                // SnpRespData_I_PD;
                t->SetSnpRespData(INV, true);
                break;
            case INV:
            default:
                // SnpResp_I;
                t->SetSnpResp(INV);
                break;
            }

            //
            // Invalidate line
            //
            l->SetValid(false);

            if (t->GetDataToHomeNode()) {
                t->SetData(l);
                m_txDatChannel.Process(t);
            } else {
                m_txRspChannel.Process(t);
            }
        }

        // Table 4-19 [1]
        void HandleSnpMakeInvalid(tlm::tlm_generic_payload &gp, chi::chi_snp_extension *chiattr) {
            CacheLine *l = get_line(gp.get_address());
            SnpRespTxn *t = new SnpRespTxn(gp, m_data_width);

            //
            // Invalidate line
            //
            l->SetValid(false);

            t->SetSnpResp(INV);

            m_txRspChannel.Process(t);
        }

        // Table 4-20
        void HandleSnpUniqueStash(tlm::tlm_generic_payload &gp, chi::chi_snp_extension *chiattr) {
            CacheLine *l = get_line(gp.get_address());
            SnpRespTxn *t = new SnpRespTxn(gp, m_data_width);

            switch (l->GetStatus()) {
            case UC:
                // SnpResp_I;
                t->SetSnpResp(INV);
                if (!chiattr->req.is_do_not_data_pull()) {
                    t->SetDataPull(l, true);
                }
                break;
            case UCE:
                // SnpResp_I;
                t->SetSnpResp(INV);
                if (!chiattr->req.is_do_not_data_pull()) {
                    t->SetDataPull(l, true);
                }
                break;
            case UD:
                // SnpRespData_I_PD;
                t->SetSnpRespData(INV, true);
                if (!chiattr->req.is_do_not_data_pull()) {
                    t->SetDataPull(l, true);
                }
                break;
            case UDP:
                // SnpRespDataPtl_I_PD;
                t->SetSnpRespDataPtl(INV, true);
                if (!chiattr->req.is_do_not_data_pull()) {
                    t->SetDataPull(l, true);
                }
                break;
            case SC:
                // SnpResp_I;
                t->SetSnpResp(INV);
                if (!chiattr->req.is_do_not_data_pull()) {
                    t->SetDataPull(l, true);
                }
                break;
            case SD:
                // SnpRespData_I_PD;
                t->SetSnpRespData(INV, true);
                if (!chiattr->req.is_do_not_data_pull()) {
                    t->SetDataPull(l, true);
                }
                break;
            case INV:
            default:
                // SnpResp_I;
                t->SetSnpResp(INV);
                if (!chiattr->req.is_do_not_data_pull()) {
                    t->SetDataPull(l, true);
                }
                break;
            }

            //
            // Invalidate line
            //
            l->SetValid(false);

            if (t->GetDataPull()) {
                t->SetupDBID(m_txn_id_pool);
                m_txn[t->GetDBID()] = t;
            }

            if (t->GetDataToHomeNode()) {
                t->SetData(l);
                m_txDatChannel.Process(t);
            } else {
                m_txRspChannel.Process(t);
            }
        }

        void HandleSnpMakeInvalidStash(tlm::tlm_generic_payload &gp, chi::chi_snp_extension *chiattr) {
            CacheLine *l = get_line(gp.get_address());
            SnpRespTxn *t = new SnpRespTxn(gp, m_data_width);

            //
            // Invalidate line
            //
            l->SetValid(false);

            t->SetSnpResp(INV);
            if (!chiattr->req.is_do_not_data_pull()) {
                l->SetTag(get_tag(gp.get_address()));
                t->SetDataPull(l, true);
                t->SetupDBID(m_txn_id_pool);
                m_txn[t->GetDBID()] = t;

            }

            m_txRspChannel.Process(t);
        }

        void HandleSnpStashUnique(tlm::tlm_generic_payload &gp, chi::chi_snp_extension *chiattr) {
            CacheLine *l = get_line(gp.get_address());
            SnpRespTxn *t = new SnpRespTxn(gp, m_data_width);

            switch (l->GetStatus()) {
            case UC:
                // SnpResp_UC;
                t->SetSnpResp(UC);
                break;
            case UCE:
                // SnpResp_UC;
                t->SetSnpResp(UC);
                if (!chiattr->req.is_do_not_data_pull()) {
                    t->SetDataPull(l, true);
                }
                break;
            case UD:
                // SnpResp_UD;
                t->SetSnpResp(UD);
                break;
            case UDP:
                // SnpResp_UD;
                t->SetSnpResp(UD);
                break;
            case SC:
                // SnpResp_SC_Read;
                t->SetSnpResp(SC);
                if (!chiattr->req.is_do_not_data_pull()) {
                    t->SetDataPull(l, true);
                }
                break;
            case SD:
                // SnpResp_SD_Read;
                t->SetSnpResp(SD);
                if (!chiattr->req.is_do_not_data_pull()) {
                    t->SetDataPull(l, true);
                }
                break;
            case INV:
            default:
                // SnpResp_I;
                t->SetSnpResp(INV);
                if (!chiattr->req.is_do_not_data_pull()) {
                    t->SetDataPull(l, true);
                }
                break;
            }

            if (t->GetDataPull()) {
                t->SetupDBID(m_txn_id_pool);
                m_txn[t->GetDBID()] = t;
            }

            m_txRspChannel.Process(t);
        }

        void HandleSnpStashShared(tlm::tlm_generic_payload &gp, chi::chi_snp_extension *chiattr) {
            CacheLine *l = get_line(gp.get_address());
            SnpRespTxn *t = new SnpRespTxn(gp, m_data_width);

            switch (l->GetStatus()) {
            case UC:
                // SnpResp_UC;
                t->SetSnpResp(UC);
                break;
            case UCE:
                // SnpResp_UC;
                t->SetSnpResp(UC);
                if (!chiattr->req.is_do_not_data_pull()) {
                    t->SetDataPull(l, true);
                }
                break;
            case UD:
                // SnpResp_UD;
                t->SetSnpResp(UD);
                break;
            case UDP:
                // SnpResp_UD;
                t->SetSnpResp(UD);
                break;
            case SC:
                // SnpResp_SC_Read;
                t->SetSnpResp(SC);
                break;
            case SD:
                // SnpResp_SD_Read;
                t->SetSnpResp(SD);
                break;
            case INV:
            default:
                // SnpResp_I;
                t->SetSnpResp(INV);
                if (!chiattr->req.is_do_not_data_pull()) {
                    t->SetDataPull(l, true);
                }
                break;
            }
            m_txRspChannel.Process(t);
        }

        // Table 4-23
        void HandleSnpOnceFwd(tlm::tlm_generic_payload &gp, chi::chi_snp_extension *chiattr) {
            CacheLine *l = get_line(gp.get_address());
            SnpRespTxn *t = new SnpRespTxn(gp, m_data_width);
            switch (l->GetStatus()) {
            case UC:
                // CompData_I
                t->SetCompData(l, INV);
                // SnpResp_UC_Fwded_I
                t->SetSnpRespFwded(UC, INV);
                break;
            case UCE:
                // SnpResp_UC
                t->SetSnpResp(UC);
                break;
            case UD:
                // CompData_I
                t->SetCompData(l, INV);
                // SnpResp_UD_Fwded_I
                t->SetSnpRespFwded(UD, INV);
                break;
            case UDP:
                // SnpRespDataPtl_UD
                t->SetSnpRespDataPtl(UD);
                break;
            case SC:
                // CompData_I
                t->SetCompData(l, INV);
                // SnpResp_SC_Fwded_I
                t->SetSnpRespFwded(SC, INV);
                break;
            case SD:
                // CompData_I
                t->SetCompData(l, INV);
                // SnpResp_SD_Fwded_I
                t->SetSnpRespFwded(SD, INV);
                break;
            case INV:
            default:
                // SnpResp_I;
                t->SetSnpResp(INV);
                break;
            }
            if (t->GetDataToHomeNode()) {
                t->SetData(l);
                m_txDatChannel.Process(t);
            } else {
                m_txRspChannel.Process(t);
            }
        }

        // Table 4-24
        void HandleSnpCleanFwd(tlm::tlm_generic_payload &gp, chi::chi_snp_extension *chiattr) {
            CacheLine *l = get_line(gp.get_address());
            SnpRespTxn *t = new SnpRespTxn(gp, m_data_width);
            switch (l->GetStatus()) {
            case UC:
                // Go to SC
                l->SetShared(true);
                // CompData_SC
                t->SetCompData(l, SC);
                if (chiattr->req.is_ret_to_src()) {
                    // SnpRespData_SC_Fwded_SC
                    t->SetSnpRespDataFwded(SC, SC);
                } else {
                    // SnpResp_SC_Fwded_SC
                    t->SetSnpRespFwded(SC, SC);
                }
                break;
            case UCE:
                // Go to I
                l->SetValid(false);
                // SnpResp_INV
                t->SetSnpResp(INV);
                break;
            case UD:
                // Go to SC
                l->SetShared(true);
                l->SetDirty(false);
                // CompData_SC
                t->SetCompData(l, SC);
                // SnpRespData_SC_PD_Fwded_SC
                t->SetSnpRespDataFwded(SC, SC, true);

                break;
            case UDP:
                // Go to I
                l->SetValid(false);
                // SnpRespDataPtl_I_PD
                t->SetSnpRespDataPtl(INV, true);
                break;
            case SC:
                // Stay in SC
                // CompData_I
                t->SetCompData(l, SC);
                if (chiattr->req.is_ret_to_src()) {
                    // SnpRespData_SC_Fwded_SC
                    t->SetSnpRespDataFwded(SC, SC);
                } else {
                    // SnpResp_SC_Fwded_SC
                    t->SetSnpRespFwded(SC, SC);
                }
                break;
            case SD:
                // Go to SC
                l->SetDirty(false);
                // CompData_SC
                t->SetCompData(l, SC);
                // SnpRespData_SC_PD_Fwded_SC
                t->SetSnpRespDataFwded(SC, SC, true);
                break;
            case INV:
            default:
                // SnpResp_I;
                t->SetSnpResp(INV);
                break;
            }
            if (t->GetDataToHomeNode()) {
                t->SetData(l);
                m_txDatChannel.Process(t);
            } else {
                m_txRspChannel.Process(t);
            }
        }

        // Table 4-25
        void HandleSnpNotSharedDirtyFwd(tlm::tlm_generic_payload &gp, chi::chi_snp_extension *chiattr) {
            CacheLine *l = get_line(gp.get_address());
            SnpRespTxn *t = new SnpRespTxn(gp, m_data_width);
            switch (l->GetStatus()) {
            case UC:
                // Go to SC
                l->SetShared(true);
                // CompData_SC
                t->SetCompData(l, SC);
                if (chiattr->req.is_ret_to_src()) {
                    // SnpRespData_SC_Fwded_SC
                    t->SetSnpRespDataFwded(SC, SC);
                } else {
                    // SnpResp_SC_Fwded_SC
                    t->SetSnpRespFwded(SC, SC);
                }
                break;
            case UCE:
                // Go to I
                l->SetValid(false);
                // SnpResp_INV
                t->SetSnpResp(INV);
                break;
            case UD:
                // Go to SC
                l->SetShared(true);
                l->SetDirty(false);
                // CompData_SC
                t->SetCompData(l, SC);
                // SnpRespData_SC_PD_Fwded_SC
                t->SetSnpRespDataFwded(SC, SC, true);
                break;
            case UDP:
                // Go to I
                l->SetValid(false);
                // SnpRespDataPtl_I_PD
                t->SetSnpRespDataPtl(INV, true);
                break;
            case SC:
                // Stay in SC
                // CompData_SC
                t->SetCompData(l, SC);
                if (chiattr->req.is_ret_to_src()) {
                    // SnpRespData_SC_Fwded_SC
                    t->SetSnpRespDataFwded(SC, SC);
                } else {
                    // SnpResp_SC_Fwded_SC
                    t->SetSnpRespFwded(SC, SC);
                }
                break;
            case SD:
                // Go to SC
                l->SetDirty(false);
                // CompData_SC
                t->SetCompData(l, SC);
                // SnpRespData_SC_PD_Fwded_SC
                t->SetSnpRespDataFwded(SC, SC, true);
                break;
            case INV:
            default:
                // SnpResp_I;
                t->SetSnpResp(INV);
                break;
            }
            if (t->GetDataToHomeNode()) {
                t->SetData(l);
                m_txDatChannel.Process(t);
            } else {
                m_txRspChannel.Process(t);
            }
        }

        // Table 4-26
        void HandleSnpSharedFwd(tlm::tlm_generic_payload &gp, chi::chi_snp_extension *chiattr) {
            CacheLine *l = get_line(gp.get_address());
            SnpRespTxn *t = new SnpRespTxn(gp, m_data_width);
            switch (l->GetStatus()) {
            case UC:
                // Go to SC
                l->SetShared(true);
                // CompData_SC
                t->SetCompData(l, SC);
                if (chiattr->req.is_ret_to_src()) {
                    // SnpRespData_SC_Fwded_SC
                    t->SetSnpRespDataFwded(SC, SC);
                } else {
                    // SnpResp_SC_Fwded_SC
                    t->SetSnpRespFwded(SC, SC);
                }
                break;
            case UCE:
                // Go to I
                l->SetValid(false);
                // SnpResp_INV
                t->SetSnpResp(INV);
                break;
            case UD:
                // Go to SC
                l->SetShared(true);
                l->SetDirty(false);
                // CompData_SC
                t->SetCompData(l, SC);
                // SnpRespData_SC_PD_Fwded_SC
                t->SetSnpRespDataFwded(SC, SC, true);
                break;
            case UDP:
                // Go to I
                l->SetValid(false);
                // SnpRespDataPtl_I_PD
                t->SetSnpRespDataPtl(INV, true);
                break;
            case SC:
                // Stay in SC
                // CompData_SC
                t->SetCompData(l, SC);
                if (chiattr->req.is_ret_to_src()) {
                    // SnpRespData_SC_Fwded_SC
                    t->SetSnpRespDataFwded(SC, SC);
                } else {
                    // SnpResp_SC_Fwded_SC
                    t->SetSnpRespFwded(SC, SC);
                }
                break;
            case SD:
                // Go to SC
                l->SetDirty(false);
                // CompData_SC
                t->SetCompData(l, SC);
                // SnpRespData_SC_PD_Fwded_SC
                t->SetSnpRespDataFwded(SC, SC, true);
                break;
            case INV:
            default:
                // SnpResp_I;
                t->SetSnpResp(INV);
                break;
            }
            if (t->GetDataToHomeNode()) {
                t->SetData(l);
                m_txDatChannel.Process(t);
            } else {
                m_txRspChannel.Process(t);
            }
        }

        // Table 4-27
        void HandleSnpUniqueFwd(tlm::tlm_generic_payload &gp, chi::chi_snp_extension *chiattr) {
            CacheLine *l = get_line(gp.get_address());
            SnpRespTxn *t = new SnpRespTxn(gp, m_data_width);
            switch (l->GetStatus()) {
            case UC:
                // CompData_UC
                t->SetCompData(l, UC);
                // SnpResp_I_Fwded_UC
                t->SetSnpRespFwded(INV, UC);
                break;
                // SnpResp_INV
                t->SetSnpResp(INV);
                break;
            case UDP:
                // SnpRespDataPtl_I_PD
                t->SetSnpRespDataPtl(INV, true);
                break;
            case SC:
                // CompData_UC
                t->SetCompData(l, UC);
                // SnpResp_I_Fwded_UC
                t->SetSnpRespFwded(INV, UC);
                break;
            case UD:
            case SD:
                // CompData_UD_PD
                t->SetCompData(l, UD, true);
                // SnpResp_I_Fwded_UD_PD
                t->SetSnpRespFwded(INV, UD, false, true);
                break;
            case UCE:
            case INV:
            default:
                // SnpResp_I;
                t->SetSnpResp(INV);
                break;
            }
            l->SetValid(false);
            if (t->GetDataToHomeNode()) {
                t->SetData(l);
                m_txDatChannel.Process(t);
            } else {
                m_txRspChannel.Process(t);
            }
        }

        void HandleSnpDVMOp(tlm::tlm_generic_payload &gp, chi::chi_snp_extension *chiattr) {
        }

        CacheLine* GetCacheLine() {
            return m_cacheline;
        }

        bool GetRandomBool() {
            if (m_randomize == false) {
                return false;
            }
            return rand_r(&m_seed) % 2;
        }

        int GetRandomInt(int modulo) {
            if (m_randomize == false) {
                return 0;
            }
            return rand_r(&m_seed) % modulo;
        }

        void SetRandomize(bool val) {
            m_randomize = val;
        }

        void SetSeed(unsigned int seed) {
            m_seed = seed;
        }

        unsigned int GetSeed() {
            return m_seed;
        }

        bool AllBytesEnabled(tlm::tlm_generic_payload &gp) {
            unsigned char *be = gp.get_byte_enable_ptr();
            unsigned int be_len = gp.get_byte_enable_length();
            unsigned int i;
            if (be_len < CACHELINE_SZ) {
                return false;
            }
            if (be_len) {
                for (i = 0; i < be_len; i++) {
                    if (be[i] != TLM_BYTE_ENABLED) {
                        break;
                    }
                }
                if (i != be_len) {
                    return false;
                }
            }
            return true;
        }

        bool IsExclusive(tlm::tlm_generic_payload &trans) {
            chi::chi_ctrl_extension *chiattr;
            trans.get_extension(chiattr);
            if (chiattr) {
                return chiattr->req.is_excl();
            }
            return false;
        }

        bool HasExclusiveOkay(tlm::tlm_generic_payload &trans) {
            if (auto *chiattr = trans.get_extension<chi::chi_ctrl_extension>()) {
                return chiattr->resp.get_resp_err() == chi::rsp_resperrtype_e::EXOK;
            }
            return false;
        }

        bool ClearExclusiveOkay(tlm::tlm_generic_payload &trans) {
            if (auto *chiattr = trans.get_extension<chi::chi_ctrl_extension>()) {
                chiattr->resp.set_resp_err(chi::rsp_resperrtype_e::OK);
            }
            return false;
        }

        void SetExclusiveOkay(tlm::tlm_generic_payload &trans) {
            if (auto *chiattr = trans.get_extension<chi::chi_ctrl_extension>()) {
                chiattr->resp.set_resp_err(chi::rsp_resperrtype_e::EXOK);
            }
        }

        //
        // LP exclusive monitor Section 6.2.1 [1]
        //
        class LPExclusiveMonitor {
        public:
            void Set(uint64_t addr) {
                if (!InList(addr)) {
                    m_addr.push_back(addr);
                }
            }

            void Reset(uint64_t addr) {
                auto it = std::find(std::begin(m_addr), std::end(m_addr), addr);
                if (it!=std::end(m_addr)) {
                    m_addr.erase(it);
                }
            }

            bool IsSet(uint64_t addr) {
                return InList(addr);
            }
        private:

            bool InList(uint64_t addr) {
                for (auto it = m_addr.begin(); it != m_addr.end(); it++) {
                    if ((*it) == addr) {
                        return true;
                    }
                }
                return false;
            }

            std::deque<uint64_t> m_addr;
        };

    protected:
        std::array<CacheLine, NUM_CACHELINES> m_cacheline{};
        size_t m_data_width;
        TxChannel &m_txReqChannel;
        TxChannel &m_txRspChannel;
        TxChannel &m_txDatChannel;

        TxnIdPool *m_txn_id_pool;
        ITxn **m_txn;

        bool m_randomize;
        unsigned int m_seed;

        LPExclusiveMonitor m_monitor;
        std::array<bool, TxnIdPool::NumIDs> m_receivedDVM;
    };

    class CacheWriteBack: public ICache {
    public:
        CacheWriteBack(size_t data_width, TxChannel &txReqChannel, TxChannel &txRspChannel, TxChannel &txDatChannel, TxnIdPool *ids, ITxn **txn)
        : ICache(data_width, txReqChannel, txRspChannel, txDatChannel, ids, txn)
        { }

        bool GetNonSecure(tlm::tlm_generic_payload &gp) {
            chi::chi_ctrl_extension *attr;
            gp.get_extension(attr);
            if (attr) {
                return attr->req.is_non_secure();
            }
            //
            // Default to be non secure access
            //
            return true;
        }

        void HandleLoad(tlm::tlm_generic_payload &gp) override {
            static const std::array<chi::req_optype_e, 7> random_opcodes = {
                    chi::req_optype_e::ReadShared,
                    chi::req_optype_e::ReadClean,
                    chi::req_optype_e::ReadUnique,
                    chi::req_optype_e::ReadOnce,
                    chi::req_optype_e::ReadOnceCleanInvalid,
                    chi::req_optype_e::ReadOnceMakeInvalid,
                    chi::req_optype_e::ReadNotSharedDirty
            };
            bool nonSecure = this->GetNonSecure(gp);
            uint64_t addr = gp.get_address();
            unsigned int len = gp.get_data_length();
            unsigned int pos = 0;
            bool exclusive = this->IsExclusive(gp);
            bool exclusive_failed = false;
            while (pos < len) {
                if (this->InCache(addr, nonSecure, true)) {
                    unsigned int n = this->ReadLine(gp, pos);
                    pos += n;
                    addr += n;
                } else {
                    auto opc = random_opcodes[this->GetRandomInt(random_opcodes.size())];
                    if(exclusive)
                        opc = random_opcodes[0];
                    if(auto* chireq = gp.get_extension<chi::chi_ctrl_extension>())
                        opc=chireq->req.get_opcode();
                    unsigned int n=0;
                    //
                    // Randomize between different reads if not
                    // exclusive. Exclusive loads always use
                    // ReadShared here.
                    //
                    switch(opc) {
                    case chi::req_optype_e::ReadShared:
                        this->ReadShared(gp, addr);
                        n=64;
                        break;
                    case chi::req_optype_e::ReadClean:
                        this->ReadClean(gp, addr);
                        n=64;
                        break;
                    case chi::req_optype_e::ReadUnique:
                    case chi::req_optype_e::MakeReadUnique:
                    case chi::req_optype_e::ReadPreferUnique:
                        this->ReadUnique(gp, opc, addr);
                        n=64;
                        break;
                    case chi::req_optype_e::ReadOnce:
                    case chi::req_optype_e::ReadOnceCleanInvalid:
                    case chi::req_optype_e::ReadOnceMakeInvalid:
                        n = this->ReadOnce(gp, opc, addr, pos);
                        break;
                    case chi::req_optype_e::ReadNotSharedDirty:
                        this->ReadNotSharedDirty(gp, addr);
                        n=64;
                        break;
                    case chi::req_optype_e::ReadNoSnp:
                    case chi::req_optype_e::ReadNoSnpSep: // only HN->SN
                        n = this->ReadNoSnp(gp, addr, pos);
                        break;
                    case chi::req_optype_e::CleanUnique:
                        this->CleanUnique(gp, addr);
                        n=64;
                        break;
                    case chi::req_optype_e::MakeUnique:
                        this->MakeUnique(gp, addr);
                        n=64;
                        break;
                    case chi::req_optype_e::MakeInvalid:
                        this->MakeInvalid(gp, addr);
                        n=64;
                        break;
                    case chi::req_optype_e::PrefetchTgt:
                        this->PrefetchTgt(gp, addr);
                        n=64;
                        break;
                    default:
                        assert(false && "Unhandled opcode value");
                    }
                    pos += n;
                    addr += n;
                    if (exclusive) {
                        if (!this->HasExclusiveOkay(gp)) {
                            exclusive_failed = true;
                        }
                        this->m_monitor.Set(addr);
                        this->ClearExclusiveOkay(gp);
                    }
                }
            }
            if (exclusive && !exclusive_failed) {
                this->SetExclusiveOkay(gp);
            }
            gp.set_response_status(tlm::TLM_OK_RESPONSE);
        }

        void HandleStore(tlm::tlm_generic_payload &gp) override {
            static const std::array<chi::req_optype_e, 6> random_opcodes_ix = {
                    chi::req_optype_e::WriteUniquePtl,
                    chi::req_optype_e::WriteUniqueFull,
                    chi::req_optype_e::WriteUniqueZero,
//                    chi::req_optype_e::WriteUniquePtlStash,
//                    chi::req_optype_e::WriteUniqueFullStash,
                    chi::req_optype_e::WriteNoSnpPtl,
                    chi::req_optype_e::WriteNoSnpFull,
                    chi::req_optype_e::WriteNoSnpZero
            };
            bool nonSecure = this->GetNonSecure(gp);
            uint64_t addr = gp.get_address();
            unsigned int len = gp.get_data_length();
            unsigned int pos = 0;
            bool exclusive = this->IsExclusive(gp);
            while (pos < len) {
                if (exclusive && !this->m_monitor.IsSet(addr)) {
                    break;
                }
                if (this->InCache(addr, nonSecure)) {
                    if(auto* chireq = gp.get_extension<chi::chi_ctrl_extension>()) {
                        auto opc=chireq->req.get_opcode();
                        CacheLine *l = this->get_line(addr);
                        unsigned int n=0;
                        switch(opc) {
                        case chi::req_optype_e::WriteBackPtl:
                        case chi::req_optype_e::WriteBackFull:
                            if(l->IsDirty()) this->WriteBack(l, opc, gp);
                            n=CACHELINE_SZ;
                            break;
                        case chi::req_optype_e::WriteCleanPtl:
                        case chi::req_optype_e::WriteCleanFull:
                            if(l->IsDirty()) this->WriteClean(l, opc, gp);
                            n=CACHELINE_SZ;
                           break;
                        case chi::req_optype_e::WriteEvictFull:
                            this->WriteEvictFull(l, gp);
                            n=CACHELINE_SZ;
                            break;
                        case chi::req_optype_e::WriteEvictOrEvict:
                            /*
                                WriteEvictOrEvict WriteBack of Clean data to the next-level cache.
                                ? Merging of WriteEvictFull and Evict into one request.
                                ? Data might not be sent if the Completer does not accept data.
                                ? If data is sent then the Data size is a cache line length.
                                ? LikelyShared value indicates the initial state of the cache line when the request is
                                sent:

                                ?
                                Not permitted to assert Exclusive in the request.

                                LikelyShared Initial state
                                0 UC
                                1 SC
                             */
                            this->Evict(l, gp);
                            n=CACHELINE_SZ;
                            break;
                        case chi::req_optype_e::CleanShared:
                        case chi::req_optype_e::CleanSharedPersist:
                        case chi::req_optype_e::CleanSharedPersistSep:
                            if(l->IsClean()) this->CleanShared(gp, opc, addr);
                            n=CACHELINE_SZ;
                            break;
                        case chi::req_optype_e::CleanUnique:
                            this->CleanUnique(gp, addr);
                            n=CACHELINE_SZ;
                            break;
                        case chi::req_optype_e::CleanInvalid:
                            this->CleanInvalid(gp, addr);
                            n=CACHELINE_SZ;
                            break;
                        case chi::req_optype_e::Evict:
                            this->Evict(l, gp);
                            break;
                        case chi::req_optype_e::WriteUniquePtl:
                        case chi::req_optype_e::WriteUniqueFull:
                        case chi::req_optype_e::WriteUniqueZero:
                        case chi::req_optype_e::WriteUniquePtlStash:
                        case chi::req_optype_e::WriteUniqueFullStash:
                        case chi::req_optype_e::WriteUniqueFullCleanSh:
                        case chi::req_optype_e::WriteUniqueFullCleanShPerSep:
                        case chi::req_optype_e::WriteUniquePtlCleanSh:
                        case chi::req_optype_e::WriteUniquePtlCleanShPerSep:
                            // Unique is only allowed in I state so w e ignore it
                            n = CACHELINE_SZ;
                            break;
                        case chi::req_optype_e::WriteNoSnpPtl:
                        case chi::req_optype_e::WriteNoSnpFull:
                        case chi::req_optype_e::WriteNoSnpZero:
                        case chi::req_optype_e::WriteNoSnpFullCleanSh:
                        case chi::req_optype_e::WriteNoSnpFullCleanInv:
                        case chi::req_optype_e::WriteNoSnpFullCleanShPerSep:
                        case chi::req_optype_e::WriteNoSnpPtlCleanSh:
                        case chi::req_optype_e::WriteNoSnpPtlCleanInv:
                        case chi::req_optype_e::WriteNoSnpPtlCleanShPerSep:
                            // NoSnp is only allowed in I state so w e ignore it
                            n = CACHELINE_SZ;
                            break;
                      default:
                            assert(false && "Unhandled opcode value");
                        }
                        pos += n;
                        addr += n;
                    } else if (this->IsUnique(addr)) {
                        unsigned int n = this->WriteLine(gp, pos);
                        if (exclusive) {
                            //
                            // Exclusive sequence
                            // for the line done
                            //
                            this->m_monitor.Reset(addr);
                        }
                        pos += n;
                        addr += n;
                    } else {
                        this->CleanUnique(gp, addr);
                        if (exclusive) {
                            // exclusive failed
                            if (!this->HasExclusiveOkay(gp)) {
                                this->m_monitor.Reset(addr);
                                break;
                            }
                            this->ClearExclusiveOkay(gp);
                        }
                    }
                } else {
                    //
                    // Make sure to use an operation that
                    // puts the line in unique state if
                    // the exclusive store has been done
                    // without a paired exclusive load,
                    // 6.3.3 [1]
                    //
                    if (exclusive) {
                        unsigned int n = this->ToWrite(gp, pos);
                        if (n == CACHELINE_SZ) {
                            this->MakeUnique(gp, addr);
                        } else {
                            this->ReadUnique(gp, chi::req_optype_e::ReadUnique, addr);
                        }
                    } else {
                        auto opc = random_opcodes_ix[this->GetRandomInt(random_opcodes_ix.size())];
                        if(auto* chireq = gp.get_extension<chi::chi_ctrl_extension>())
                            opc=chireq->req.get_opcode();
                        unsigned int n=0;
                        switch(opc) {
                        case chi::req_optype_e::WriteUniquePtl:
                        case chi::req_optype_e::WriteUniqueFull:
                        case chi::req_optype_e::WriteUniqueZero:
                        case chi::req_optype_e::WriteUniquePtlStash:
                        case chi::req_optype_e::WriteUniqueFullStash:
                        case chi::req_optype_e::WriteUniqueFullCleanSh:
                        case chi::req_optype_e::WriteUniqueFullCleanShPerSep:
                        case chi::req_optype_e::WriteUniquePtlCleanSh:
                        case chi::req_optype_e::WriteUniquePtlCleanShPerSep:
                            n = this->WriteUnique(gp, opc, addr, pos);
                            break;
                        case chi::req_optype_e::WriteNoSnpPtl:
                        case chi::req_optype_e::WriteNoSnpFull:
                        case chi::req_optype_e::WriteNoSnpZero:
                        case chi::req_optype_e::WriteNoSnpFullCleanSh:
                        case chi::req_optype_e::WriteNoSnpFullCleanInv:
                        case chi::req_optype_e::WriteNoSnpFullCleanShPerSep:
                        case chi::req_optype_e::WriteNoSnpPtlCleanSh:
                        case chi::req_optype_e::WriteNoSnpPtlCleanInv:
                        case chi::req_optype_e::WriteNoSnpPtlCleanShPerSep:
                           n = this->WriteNoSnp(gp, opc, addr, pos);
                            break;
                        case chi::req_optype_e::CleanShared:
                        case chi::req_optype_e::CleanSharedPersist:
                        case chi::req_optype_e::CleanSharedPersistSep:
                            this->CleanShared(gp, opc, addr);
                            n=CACHELINE_SZ;
                            break;
                        case chi::req_optype_e::CleanInvalid:
                            this->CleanInvalid(gp, addr);
                            n=CACHELINE_SZ;
                            break;
                        case chi::req_optype_e::CleanUnique:
                            this->CleanUnique(gp, addr);
                            n=CACHELINE_SZ;
                            break;
                        case chi::req_optype_e::Evict:
                        case chi::req_optype_e::WriteEvictFull:
                        case chi::req_optype_e::WriteCleanPtl:
                        case chi::req_optype_e::WriteCleanFull:
                        case chi::req_optype_e::WriteCleanFullCleanSh:
                        case chi::req_optype_e::WriteCleanFullCleanShPerSep:
                        case chi::req_optype_e::WriteBackPtl:
                        case chi::req_optype_e::WriteBackFull:
                        case chi::req_optype_e::WriteEvictOrEvict:
                        case chi::req_optype_e::WriteBackFullCleanSh:
                        case chi::req_optype_e::WriteBackFullCleanInv:
                        case chi::req_optype_e::WriteBackFullCleanShPerSep:
                            n=CACHELINE_SZ;
                            break;
                        case chi::req_optype_e::StashOnceSepShared:
                        case chi::req_optype_e::StashOnceSepUnique:
                        case chi::req_optype_e::StashOnceShared:
                        case chi::req_optype_e::StashOnceUnique:
                            this->Stash(gp, opc);
                            n=CACHELINE_SZ;
                            break;
                       default:
                            SCCFATAL("CacheWriteBack")<<"Unhandled opcode value: "<<to_char(opc);
                       }
                        pos += n;
                        addr += n;
                    }
                }
            }
            if (exclusive && pos == len) {
                this->SetExclusiveOkay(gp);
            }
            gp.set_response_status(tlm::TLM_OK_RESPONSE);
        }
    };

    bool IsPCrdGrant(chi::chi_ctrl_extension *attr) {
        return attr->resp.get_opcode() == chi::rsp_optype_e::PCrdGrant;
    }

    bool IsRetryAck(chi::chi_ctrl_extension *attr) {
        return attr->resp.get_opcode() == chi::rsp_optype_e::RetryAck;
    }

    void WriteNoSnp(tlm::tlm_generic_payload &gp) {
        uint64_t addr = gp.get_address();
        unsigned int len = gp.get_data_length();
        unsigned int pos = 0;
        while (pos < len) {
            unsigned int n;
            if (m_cache->AllBytesEnabled(gp)) {
                n = m_cache->WriteNoSnp(gp, chi::req_optype_e::WriteUniqueFull, addr, pos);
            } else {
                n = m_cache->WriteNoSnp(gp, chi::req_optype_e::WriteUniquePtl, addr, pos);
            }
            pos += n;
            addr += n;
        }
        gp.set_response_status(tlm::TLM_OK_RESPONSE);
    }

    void ReadNoSnp(tlm::tlm_generic_payload &gp) {
        uint64_t addr = gp.get_address();
        unsigned int len = gp.get_data_length();
        unsigned int pos = 0;
        while (pos < len) {
            unsigned int n = m_cache->ReadNoSnp(gp, addr, pos);
            pos += n;
            addr += n;
        }
        gp.set_response_status(tlm::TLM_OK_RESPONSE);
    }

    class NonShareableRegion {
    public:
        NonShareableRegion(uint64_t start, unsigned int len)
        : m_nonshareable_start(start), m_nonshareable_len(len)
        { }

        bool InRegion(uint64_t addr) {
            uint64_t m_nonshareable_end = m_nonshareable_start + m_nonshareable_len;
            return addr >= m_nonshareable_start && addr < m_nonshareable_end;
        }

    private:
        const uint64_t m_nonshareable_start;
        const unsigned int m_nonshareable_len;
    };

    bool InNonShareableRegion(tlm::tlm_generic_payload &gp) {
        typename std::vector<NonShareableRegion>::iterator it;
        uint64_t addr = gp.get_address();
        for (it = m_regions.begin(); it != m_regions.end(); it++) {
            if ((*it).InRegion(addr)) {
                return true;
            }
        }
        return false;
    }

    void AddNonShareableRegion(uint64_t start, unsigned int len) {
        NonShareableRegion region(start, len);
        m_regions.push_back(region);
    }

    bool IsAtomic(tlm::tlm_generic_payload &gp) {
        chi::chi_ctrl_extension *attr;
        gp.get_extension(attr);
        if (attr) {
            auto opcode = attr->req.get_opcode();
            return opcode >= chi::req_optype_e::AtomicStore && opcode <= chi::req_optype_e::AtomicCompare;
        }
        return false;
    }

    bool IsDVMOp(tlm::tlm_generic_payload &gp) {
        chi::chi_ctrl_extension *attr;
        gp.get_extension(attr);
        if (attr) {
            return attr->req.get_opcode() == chi::req_optype_e::DVMOp;
        }
        return false;
    }

    void HandleAtomic(tlm::tlm_generic_payload &gp) {
        chi::chi_ctrl_extension *attr;
        gp.get_extension(attr);
        if (attr) {
            if (attr->req.get_opcode() < chi::req_optype_e::AtomicLoad) {
                m_cache->AtomicStore(gp, attr);
            } else {
                m_cache->AtomicNonStore(gp, attr);
            }
        }
        gp.set_response_status(tlm::TLM_OK_RESPONSE);
    }

    void HandleDVMOperation(tlm::tlm_generic_payload &gp) {
        chi::chi_ctrl_extension *attr;
        gp.get_extension(attr);
        if (attr) {
            m_cache->DVMOperation(gp, attr);
        }
        gp.set_response_status(tlm::TLM_OK_RESPONSE);
    }


    TxChannel m_txReqChannel;
    TxChannel m_txRspChannel;
    TxChannel m_txDatChannel;

    Transmitter m_transmitter;

    const size_t data_width;
    TxnIdPool m_ids;
    std::unique_ptr<ICache> m_cache;
    std::array<ITxn *, TxnIdPool::NumIDs> m_txn{nullptr};

    scc::ordered_semaphore_t<1> m_mutex;

    std::vector<NonShareableRegion> m_regions;

    public:

    sc_core::sc_in<bool> clk_i{"clk_i"};

    cache_chi(sc_core::sc_module_name name, std::function<send_flit_fct> send_flit, size_t data_width)
    : sc_core::sc_module(name)
    , m_txReqChannel("TxReqChannel", TxChannel::REQ, send_flit)
    , m_txRspChannel("TxRspChannel", TxChannel::RESP, send_flit)
    , m_txDatChannel("TxDatChannel", TxChannel::DAT, send_flit)
    , m_transmitter(m_txRspChannel, m_txDatChannel)
    , data_width(data_width)
    {
        m_txRspChannel.SetTransmitter(&m_transmitter);
        m_txDatChannel.SetTransmitter(&m_transmitter);
        m_cache.reset(new CacheWriteBack(data_width, m_txReqChannel, m_txRspChannel, m_txDatChannel, &m_ids, m_txn.data()));
        m_txReqChannel.clk_i(clk_i);
        m_txRspChannel.clk_i(clk_i);
        m_txDatChannel.clk_i(clk_i);
    }

    ~cache_chi() = default;

    void transport(tlm::tlm_generic_payload &trans, sc_core::sc_time &delay) {
        scc::ordered_semaphore::lock l(m_mutex);
        wait(delay);
        delay = sc_core::SC_ZERO_TIME;
        if (InNonShareableRegion(trans)) {
            if (trans.is_write()) {
                SCCTRACE(SCMOD) << "starting non-sharable write tx to addr=0x"<<std::hex<<trans.get_address();
                WriteNoSnp(trans);
            } else if (trans.is_read()) {
                SCCTRACE(SCMOD) << "starting non-sharable read tx to addr=0x"<<std::hex<<trans.get_address();
                ReadNoSnp(trans);
            }
        } else if (IsAtomic(trans)) {
            SCCTRACE(SCMOD) << "starting atomic tx to addr=0x"<<std::hex<<trans.get_address();
            HandleAtomic(trans);
        } else if (IsDVMOp(trans)) {
            SCCTRACE(SCMOD) << "starting DVMOp tx to addr=0x"<<std::hex<<trans.get_address();
            HandleDVMOperation(trans);
        } else {
            if (trans.is_write()) {
                SCCTRACE(SCMOD) << "starting sharable write tx to addr=0x"<<std::hex<<trans.get_address();
                m_cache->HandleStore(trans);
            } else if (trans.is_read()) {
                SCCTRACE(SCMOD) << "starting sharable read tx to addr=0x"<<std::hex<<trans.get_address();
                m_cache->HandleLoad(trans);
            }
        }
    }

    void handle_rxrsp(tlm::tlm_generic_payload &trans, sc_core::sc_time &delay) {
        if (auto  *chiattr = trans.get_extension<chi::chi_ctrl_extension>()) {
            ITxn *t = m_txn[chiattr->get_txn_id()];
            if (t) {
                bool ret = true;
                if (IsRetryAck(chiattr)) {
                    t->HandleRetryAck(chiattr);
                    //
                    // If got both RetryAck and
                    // PCrdGrant
                    //
                    if (t->RetryRequest()) {
                        m_txReqChannel.Process(t);
                    }
                } else if (IsPCrdGrant(chiattr)) {
                    t->HandlePCrdGrant(chiattr);
                    //
                    // If got both RetryAck and
                    // PCrdGrant
                    //
                    if (t->RetryRequest()) {
                        m_txReqChannel.Process(t);
                    }
                } else {
                    ret = t->HandleRxRsp(trans);
                    m_transmitter.Process(t);
                }
                if (ret) {
                    trans.set_response_status(tlm::TLM_OK_RESPONSE);
                }
            }
        }
    }

    void handle_rxdat(tlm::tlm_generic_payload &trans, sc_core::sc_time &delay) {
        // if (trans.is_read()) {
            if (auto *chiattr= trans.get_extension<chi::chi_data_extension>()) {
                ITxn *t = m_txn[chiattr->get_txn_id()];
                if (t) {
                    bool ret = t->HandleRxDat(trans);
                    if (t->IsSnp() && t->Done()) {
                        //
                        // Snps (data pull) only receives once
                        //
                        m_txn[chiattr->get_txn_id()] = nullptr;
                        m_transmitter.ProcessSnp(t);
                    } else {
                        m_transmitter.Process(t);
                    }
                    //                    if (ret) {
                    //                        trans.set_response_status(tlm::TLM_OK_RESPONSE);
                    //                    }
                }
            }
        //}
    }

    void handle_rxsnp(tlm::tlm_generic_payload &trans, sc_core::sc_time &delay) {
        //trans.set_response_status(tlm::TLM_GENERIC_ERROR_RESPONSE);
        //if (trans.get_command() == tlm::TLM_IGNORE_COMMAND) {
            auto *chiattr = trans.get_extension<chi::chi_snp_extension>();
            if (chiattr) {
                bool ret = m_cache->HandleSnp(trans, chiattr);
                if (ret) {
                    trans.set_response_status(tlm::TLM_OK_RESPONSE);
                }
            }
        //}
    }

    void RandomizeTransactions(bool val) {
        m_cache->SetRandomize(val);
    }

    void SetSeed(unsigned int seed) {
        m_cache->SetSeed(seed);
    }
    unsigned int GetSeed() {
        return m_cache->GetSeed();
    }

    void CreateNonShareableRegion(uint64_t start, unsigned int len) {
        AddNonShareableRegion(start, len);
    }
};
}
}

#endif // _CHI_PE_CACHE_CHI_H_
