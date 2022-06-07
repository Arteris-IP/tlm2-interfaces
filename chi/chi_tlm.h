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

#pragma once

#include <array>
#include <cstdint>
#include <tlm>
#include <type_traits>

//! TLM2.0 components modeling CHI
namespace chi {
/**
 * helper function to allow SFINAE
 */
template <typename Enum> struct enable_for_enum { static const bool value = false; };
/**
 * helper function to convert integer into class enums
 * @param t
 * @return
 */
template <typename E> inline E into(typename std::underlying_type<E>::type t);
/**
 * helper function to convert class enums into integer
 * @param t
 * @return
 */
template <
    typename E, typename ULT = typename std::underlying_type<E>::type,
    typename X = typename std::enable_if<std::is_enum<E>::value && !std::is_convertible<E, ULT>::value, bool>::type>
inline constexpr ULT to_int(E t) {
    return static_cast<typename std::underlying_type<E>::type>(t);
}
/**
 * helper function to convert class enums into char string
 * @param t
 * @return
 */
template <typename E> const char* to_char(E t);
/**
 *
 * @param os
 * @param e
 * @return
 */
template <typename E, typename std::enable_if<enable_for_enum<E>::value, bool>::type>
inline std::ostream& operator<<(std::ostream& os, E e) {
    os << to_char(e);
    return os;
}

//  REQ channel request type enumeration class
enum class req_optype_e : uint8_t {
    // Table 12-14 REQ channel opcodes and Page No:318
    ReqLCrdReturn = 0x00,
    ReadShared = 0x01,
    ReadClean = 0x02,
    ReadOnce = 0x03,
    ReadNoSnp = 0x04,
    PCrdReturn = 0x05,
    Reserved = 0x06,
    ReadUnique = 0x07,
    CleanShared = 0x08,
    CleanInvalid = 0x09,
    MakeInvalid = 0x0A,
    CleanUnique = 0x0B,
    MakeUnique = 0x0C,
    Evict = 0x0D,
    EOBarrier = 0x0E,
    ECBarrier = 0x0F,
    ReadNoSnpSep = 0x11,
    DVMOp = 0x14,
    WriteEvictFull = 0x15,
    WriteCleanFull = 0x17,
    WriteUniquePtl = 0x18,
    WriteUniqueFull = 0x19,
    // mahi: discuss with suresh sir
    // Reserved=0x10
    // Reserved=0x12-0x13
    // Reserved (WriteCleanPtl)=0x16    // As per CHI issue-C
    WriteCleanPtl = 0x16, // As per CHI issue-A
    WriteBackPtl = 0x1A,
    WriteBackFull = 0x1B,
    WriteNoSnpPtl = 0x1C,
    WriteNoSnpFull = 0x1D,
    // RESERVED    0x1E-0x1F
    WriteUniqueFullStash = 0x20,
    WriteUniquePtlStash = 0x21,
    StashOnceShared = 0x22,
    StashOnceUnique = 0x23,
    ReadOnceCleanInvalid = 0x24,
    ReadOnceMakeInvalid = 0x25,
    ReadNotSharedDirty = 0x26,
    CleanSharedPersist = 0x27,
    AtomicStoreAdd = 0x28,
    AtomicStoreClr = 0x29,
    AtomicStoreEor = 0x2A,
    AtomicStoreSet = 0x2B,
    AtomicStoreSmax = 0x2C,
    AtomicStoreSmin = 0x2D,
    AtomicStoreUmax = 0x2E,
    AtomicStoreUmin = 0x2F,
    AtomicLoadAdd = 0x30,
    AtomicLoadClr = 0x31,
    AtomicLoadEor = 0x32,
    AtomicLoadSet = 0x33,
    AtomicLoadSmax = 0x34,
    AtomicLoadSmin = 0x35,
    AtomicLoadUmax = 0x36,
    AtomicLoadUmin = 0x37,
    AtomicSwap = 0x38,
    AtomicCompare = 0x39,
    PrefetchTgt = 0x3A,
    // RESERVED        = 0x3B to 0x3F
    ILLEGAL = 0xFF
};

// the Snp_Req channel request type enumeration class acc. Table 12-17 SNP channel opcodes and Page No:321
enum class snp_optype_e : uint8_t {
    SnpLCrdReturn = 0x00,
    SnpShared = 0x01,
    SnpClean = 0x02,
    SnpOnce = 0x03,
    SnpNotSharedDirty = 0x04,
    SnpUniqueStash = 0x05,
    SnpMakeInvalidStash = 0x06,
    SnpUnique = 0x07,
    SnpCleanShared = 0x08,
    SnpCleanInvalid = 0x09,
    SnpMakeInvalid = 0x0A,
    SnpStashUnique = 0x0B,
    SnpStashShared = 0x0c,
    SnpDVMOp = 0x0D,
    SnpSharedFwd = 0x11,
    SnpCleanFwd = 0x12,
    SnpOnceFwd = 0x13,
    SnpNotSharedDirtyFwd = 0x14,
    SnpUniqueFwd = 0x17,
    // Reserved           = 0x0E-0x0F
    // Reserved           = 0x10
    // Reserved           = 0x15-0x16
    // Reserved           = 0x18-0x1F
};

// the Dat type enumeration class acc. Table 12-18 DAT channel opcodes and Page No:322
enum class dat_optype_e : uint8_t {
    DataLCrdReturn = 0x0,
    SnpRespData = 0x1,
    CopyBackWrData = 0x2,
    NonCopyBackWrData = 0x3,
    CompData = 0x4,
    SnpRespDataPtl = 0x5,
    SnpRespDataFwded = 0x6,
    WriteDataCancel = 0x7,
    DataSepResp = 0xB,
    NCBWrDataCompAck = 0xC,
    // Reserved        = 0x8-0xA
    // Reserved        = 0xD-0xF
    // Table 4-9 Permitted Non-forward type data opcode
    SnpRespData_I = 0x1,
    SnpRespData_UC = 0x1,
    SnpRespData_UD = 0x1,
    SnpRespData_SC = 0x1,
    SnpRespData_SD = 0x1,
    SnpRespData_I_PD = 0x1,
    SnpRespData_UC_PD = 0x1,
    SnpRespData_SC_PD = 0x1,
    SnpRespDataPtl_I_PD = 0x5,
    SnpRespDataPtl_UD = 0x5,
    // Table 4-10 Permitted Forward data opcode
    SnpRespData_I_Fwded_SC = 0x6,
    SnpRespData_I_Fwded_SD_PD = 0x6,
    SnpRespData_SC_Fwded_SC = 0x6,
    SnpRespData_SC_Fwded_SD_PD = 0x6,
    SnpRespData_SD_Fwded_SC = 0x6,
    SnpRespData_I_PD_Fwded_I = 0x6,
    SnpRespData_I_PD_Fwded_SC = 0x6,
    SnpRespData_SC_PD_Fwded_I = 0x6,
    SnpRespData_SC_PD_Fwded_SC = 0x6

};

// the dat response 'resp' field type enumeration class acc. sec 4.5 Response types
enum class dat_resptype_e : uint8_t {
    // Read and Atomic Completion
    CompData_I = 0b000, // Copy of cacheline cannot be kept
    DataSepResp_I = 0b000,
    RespSepData_I = 0b000, // Not applicable
    CompData_UC = 0b010,   // Final state of cacheline can be UC or UCE or SC or I
    DataSepResp_UC = 0b010,
    RespSepData_UC = 0b010,
    CompData_SC = 0b001,
    DataSepResp_SC = 0b001,
    RespSepData_SC = 0b001,
    CompData_UD_PD = 0b110,
    CompData_SD_PD = 0b111,
    // Table 4-9 Permitted Non-forward type snoop responses with data(Dat opcode =0x1)
    SnpRespData_I = 0b000,
    SnpRespData_UC = 0b010,
    SnpRespData_UD = 0b010,
    SnpRespData_SC = 0b001,
    SnpRespData_SD = 0b011,
    SnpRespData_I_PD = 0b100,
    SnpRespData_UC_PD = 0b110,
    SnpRespData_SC_PD = 0b101,
    SnpRespDataPtl_I_PD = 0b100,
    SnpRespDataPtl_UD = 0b010,

    // Dataless transactions
    Comp_I = 0b000,  // final state must be I
    Comp_UC = 0b010, // final state can be UC, UCE,SC or I
    Comp_SC = 0b001, // final state SC or I
    // Write and Atomic completion
    CopyBackWrData_I = 0b000,  // if cache state when data sent is I
    CopyBackWrData_UC = 0b010, // Cache line = UC
    CopyBackWrData_SC = 0b001,
    CopyBackWrData_UD_PD = 0b110, // cacheline state UD or UDP
    CopyBackWrData_SD_PD = 0b111,
    NonCopyBackWrData = 0b000,
    NCBWrDataCompAck = 0b000
};

// response type enumeration class according to Table 12-16 RSP channel opcodes and Page No :320
enum class rsp_optype_e : uint8_t {
    RespLCrdReturn = 0x0,
    SnpResp = 0x1,
    CompAck = 0x2,
    RetryAck = 0x3,
    Comp = 0x4,
    CompDBIDResp = 0x5,
    DBIDResp = 0x6,
    PCrdGrant = 0x7,
    ReadReceipt = 0x8,
    SnpRespFwded = 0x9,
    RespSepData = 0xB,
    // Reserved     = 0xA,
    // Reserved     = 0xC-0xF
    Invalid = 0x10
};

enum class rsp_resptype_e : uint8_t {
    // Table 4-7 Permitted Non-forward type snoop responses without data
    SnpResp_I = 0b000,
    SnpResp_SC = 0b001,
    SnpResp_UC = 0b010,
    SnpResp_UD = 0b010,
    SnpResp_SD = 0b011,
    // Table 4-5 Permitted Dataless transaction completion
    Comp_I = 0b000,  // final state must be I
    Comp_UC = 0b010, // final state can be UC, UCE,SC or I
    Comp_SC = 0b001, // final state SC or I
};

enum class credit_type_e : uint8_t {
    LINK,
    REQ,  // RN->HN: snoop,  HN->RN: req
    RESP, // RN->HN: cresp,  HN->RN: sresp
    DATA, // RN->HN: rxdata, HN->RN: txdata
};
/**
 * common : This structure contains common fields for all the 4 structures of 'request', 'snp_request',
 *            'data' packet as well as 'response'
 */
struct common {
public:
    void reset();

    //! the constructori
    common() = default;
    //! reset all data member to their default
    /**
     * @brief copy assignment operator
     * @param o
     * @return reference to self
     */
    common& operator=(const common& o) {
        qos = o.qos;
        txn_id = o.txn_id;
        src_id = o.src_id;
        return *this;
    }

    //! set the transition id value of REQ,WDAT,RDAT and CRSP a particular channel
    //! @param the TxnID of the channel
    // A transaction request includes a TxnID that is used to identify the transaction from a given Requester
    void set_txn_id(unsigned int);

    //! get the transition id value of a particular channel
    //! @return the TxnID
    unsigned int get_txn_id() const;

    //! set the source id value of REQ,WDAT,RDAT and CRSP a particular channel
    //! @param the TxnID of the channel
    void set_src_id(unsigned int);

    //! get the source id value of a particular channel
    //! @return the SrcID
    unsigned int get_src_id() const;

    //! set the chi qos value of a particular channel
    //! @param chi qos value of the REQ,WDAT,RDAT and CRSP channel
    void set_qos(uint8_t qos);

    //! get the chi qos value of a particular channel
    //! @return chi qos value
    unsigned int get_qos() const;

private:
    uint32_t qos{0};
    uint16_t src_id{0};
    uint16_t txn_id{0};
};

/**
 * request : This structure to be used in extension of payload for providing transaction
 *  request on REQ channel (TXREQ channel for RN node)
 */
struct request {
    // request() { }
    // A transaction request includes a TgtID that identifies the target node
    void set_tgt_id(uint8_t);
    uint8_t get_tgt_id() const;
    /*Logical Processor ID. Used in conjunction with the SrcID field to uniquely
    identify the logical processor that generated the request. */
    void set_lp_id(uint8_t);
    uint8_t get_lp_id() const;
    /*Return Transaction ID. The unique transaction ID that conveys the value of
    TxnID in the data response from the Slave*/
    void set_return_txn_id(uint8_t);
    uint8_t get_return_txn_id() const;
    // Stash Logical Processor ID. The ID of the logical processor at the Stash target.
    void set_stash_lp_id(uint8_t);
    uint8_t get_stash_lp_id() const;
    /*Data size. Specifies the size of the data associated with the transaction. This
      determines the number of data packets within the transaction */
    void set_size(uint8_t);
    uint8_t get_size() const;
    /*Max number of flits in current transaction. The number is determined based on data length and bus width
     * The number is required to identify the last flit in data transaction. */
    void set_max_flit(uint8_t data_id);
    uint8_t get_max_flit() const;
    /* Memory attribute. Determines the memory attributes associated with the
    transaction. */
    void set_mem_attr(uint8_t);
    uint8_t get_mem_attr() const;
    /* Protocol Credit Type. Indicates the type of Protocol Credit being used by a
       request that has the AllowRetry field deasserted */
    void set_pcrd_type(uint8_t);
    uint8_t get_pcrd_type() const;
    // Endianness. Indicates the endianness of Data in the Data packet.
    void set_endian(bool);
    bool is_endian() const;
    /*Order requirement. Determines the ordering requirement for this request with
      respect to other transactions from the same agent.*/
    void set_order(uint8_t);
    uint8_t get_order() const;
    /*Trace Tag. Provides additional support for the debugging, tracing, and
     performance measurement of systems. */
    void set_trace_tag(bool tg = true);
    bool is_trace_tag() const;
    void set_opcode(chi::req_optype_e op);
    chi::req_optype_e get_opcode() const;
    // Return Node ID. The node ID that the response with Data is to be sent to.
    void set_return_n_id(uint16_t);
    uint16_t get_return_n_id() const;
    // Stash Node ID is the node ID of the Stash target.
    void set_stash_n_id(uint16_t);
    uint16_t get_stash_n_id() const;
    /* Stash Node ID Valid. Indicates that the StashNID field has a valid Stash target
    value. */
    void set_stash_n_id_valid(bool = true);
    bool is_stash_n_id_valid() const;
    /*Stash Logical Processor ID. The ID of the logical processor at the Stash target.*/
    void set_stash_lp_id_valid(bool = true);
    bool is_stash_lp_id_valid() const;
    /*Secure and Non-secure transactions are defined to support Secure and
    Non-secure operating states.*/
    void set_non_secure(bool = true);
    bool is_non_secure() const;
    /* Expect CompAck. Indicates that the transaction will include a Completion
       Acknowledge message*/
    void set_exp_comp_ack(bool = true);
    bool is_exp_comp_ack() const;
    /* Allow Retry determines if the target is permitted to give a Retry response.*/
    void set_allow_retry(bool = true);
    bool is_allow_retry() const;
    /* Snoop attribute specifies the snoop attributes associated with the transaction.*/
    void set_snp_attr(bool = true);
    bool is_snp_attr() const;
    /*Exclusive access. Indicates that the corresponding transaction is an Exclusive
     access transaction.*/
    void set_excl(bool = true);
    bool is_excl() const;
    /* Snoop Me. Indicates that Home must determine whether to send a snoop to the
    Requester.*/
    void set_snoop_me(bool = true);
    bool is_snoop_me() const;
    /*The LikelyShared attribute is a cache allocation hint. When asserted this attribute indicates
    that the requested data is likely to be shared by other Request Nodes within the system */
    void set_likely_shared(bool = true);
    bool is_likely_shared() const;
    /* Reserved for customer use. Any value is valid in a Protocol flit. Propagation of this field through the
     interconnect is IMPLEMENTATION DEFINED.*/
    void set_rsvdc(uint32_t);
    uint32_t get_rsvdc() const; // Reserved for customer use.

private:
    uint8_t tgt_id{0}, lp_id{0}, return_txn_id{0}, stash_lp_id{0}, size{0}, max_flit{0}, mem_attr{0}, pcrd_type{0},
        order{0};
    bool endian{false}, trace_tag{false};
    uint16_t return_n_id{0}, stash_n_id{0};
    req_optype_e opcode{req_optype_e::ReqLCrdReturn};
    bool stash_n_id_valid{false}, stash_lp_id_valid{false}, ns{false}, exp_comp_ack{false}, allow_retry{false},
        snp_attr{false}, excl{false}, snoop_me{false}, likely_shared{false};
    uint32_t rsvdc{0};
};

/**
 * snp_request : This structure to be used in extension of payload for providing snoop
 *  request on SNP channel
 */
struct snp_request {
    /*Forward Transaction ID. The transaction ID used in the Request by the original
    Requester.*/
    void set_fwd_txn_id(uint8_t);
    uint8_t get_fwd_txn_id() const;
    /*Stash Logical Processor ID. The ID of the logical processor at the Stash target.*/
    void set_stash_lp_id(uint8_t);
    uint8_t get_stash_lp_id() const;
    /* Stash Node ID Valid. Indicates that the StashNID field has a valid Stash target
    value. */
    void set_stash_lp_id_valid(bool = true);
    bool is_stash_lp_id_valid() const;
    /* Virtual Machine ID Extension use in DVM Operation types*/
    void set_vm_id_ext(uint8_t);
    uint8_t get_vm_id_ext() const;
    /* Set the opcode of Snoop Request */
    void set_opcode(snp_optype_e opcode);
    snp_optype_e get_opcode() const;
    /*Forward Node ID is Node ID of the original Requester */
    void set_fwd_n_id(uint16_t);
    uint16_t get_fwd_n_id() const;
    /*Secure and Non-secure transactions are defined to support Secure and
    Non-secure operating states.*/
    void set_non_secure(bool = true); // NS bit
    bool is_non_secure() const;       // NS bit
    /* Do Not Go To SD state controls Snoopee use of SD state.It specifies that
    the Snoopee must not transition to SD state as a result of the Snoop request.*/
    void set_do_not_goto_sd(bool = true);
    bool is_do_not_goto_sd() const;
    /*Do Not Data Pull. Instructs the Snoopee that it is not permitted to use the
    Data Pull feature associated with Stash requests*/
    void set_do_not_data_pull(bool = true);
    bool is_do_not_data_pull() const;
    /*Return to Source. Instructs the receiver of the snoop to return Data with
    the Snoop response.*/
    void set_ret_to_src(bool);
    bool is_ret_to_src() const;
    /*Trace Tag. Provides additional support for the debugging, tracing, and
      performance measurement of systems*/
    void set_trace_tag(bool = true);
    bool is_trace_tag() const;

private:
    uint8_t fwd_txn_id{0}, stash_lp_id{0}, vm_id_ext{0};
    bool stash_lp_id_valid{false};
    snp_optype_e opcode;
    uint16_t fwd_n_id{0};
    bool ns{false}, do_not_goto_sd{false}, do_not_data_pull{false}, ret_to_src{false}, trace_tag{false};
};

/**
 * data : This structure to be used in extension of payload for providing data on WDAT/RDAT channels
 *        (to be used in Request of Write & Read operations on WDAT and RDAT channels respectively)
 *        ( or in Reponse of Snoop operation on WDAT channel)
 */
struct data {
    /* Data Buffer ID. The ID provided to be used as the TxnID in the response to this message*/
    void set_db_id(uint8_t);
    uint8_t get_db_id() const;
    /* Set the opcode of Data packet */
    void set_opcode(dat_optype_e opcode);
    dat_optype_e get_opcode() const;
    /*Response Error status. Indicates the error status associated with a data transfer*/
    void set_resp_err(uint8_t);
    uint8_t get_resp_err() const;
    /*Response status. Indicates the cache line state associated with a data transfer*/
    void set_resp(dat_resptype_e);
    dat_resptype_e get_resp() const;
    /*Forward State. Indicates the cache line state associated with a data transfer
    to the Requester from the receiver of the snoop*/
    void set_fwd_state(uint8_t);
    uint8_t get_fwd_state() const;
    /*Data Pull. Indicates the inclusion of an implied Read request in the Data
     response*/
    void set_data_pull(uint8_t);
    uint8_t get_data_pull() const;
    /* Data Source. The value indicates the source of the data in a read Data
    response*/
    void set_data_source(uint8_t);
    uint8_t get_data_source() const;
    /* Critical Chunk Identifier. Replicates the address offset of the original
    transaction reques*/
    void set_cc_id(uint8_t);
    uint8_t get_cc_id() const;
    /*Data Identifier. Provides the address offset of the data provided in the packet*/
    void set_data_id(uint8_t);
    uint8_t get_data_id() const;
    /*Poison. Indicates that a set of data bytes has previously been corrupted*/
    void set_poison(uint8_t);
    uint8_t get_poison() const;
    /*A transaction request includes a TgtID that identifies the target node*/
    void set_tgt_id(uint16_t);
    uint16_t get_tgt_id() const;
    /*Home Node ID. The Node ID of the target of the CompAck response to be
    sent from the Requester*/
    void set_home_n_id(uint16_t);
    uint16_t get_home_n_id() const;
    /*Reserved for customer use. Any value is valid in a Protocol flit. Propagation of this field through the
    interconnect is IMPLEMENTATION DEFINED*/
    void set_rsvdc(uint32_t);
    uint32_t get_rsvdc() const;
    /*Data Check. Detects data errors in the DAT packet*/
    void set_data_check(uint64_t);
    uint64_t get_data_check() const;
    /*Trace Tag. Provides additional support for the debugging, tracing, and
    performance measurement of systems*/
    void set_trace_tag(bool);
    bool is_trace_tag() const;

private:
    uint8_t db_id{0}, resp_err{0};
    dat_resptype_e resp{dat_resptype_e::CompData_I};
    uint8_t fwd_state{0}, data_pull{0}, data_source{0}, cc_id{0}, data_id{0}, poison{0};
    uint16_t tgt_id{0}, home_n_id{0};
    dat_optype_e opcode{dat_optype_e::DataLCrdReturn};
    uint32_t rsvdc{0};
    uint64_t data_check{0};
    bool trace_tag{false};
};

/**
 * response : This structure to be used as response for both Transaction request as well as Snoop request
 * (used for both CRSP channel Completor response and SRSP channel Snoop response)
 */
struct response {
    /* Data Buffer ID. The ID provided to be used as the TxnID in the response to
    this message*/
    void set_db_id(uint8_t);
    uint8_t get_db_id() const;
    /*The PCrdType field indicates the credit type associated with the request*/
    void set_pcrd_type(uint8_t);
    uint8_t get_pcrd_type() const;
    /* Set the opcode of response packet */
    void set_opcode(rsp_optype_e opcode);
    rsp_optype_e get_opcode() const;
    /*Response Error status. Indicates the error status associated with request*/
    void set_resp_err(uint8_t);
    uint8_t get_resp_err() const;
    /*Response status. Indicates the response associated with a request*/
    void set_resp(rsp_resptype_e);
    rsp_resptype_e get_resp() const;
    /*Forward State. Indicates the cache line state associated with a data transfer
    to the Requester from the receiver of the snoop*/
    void set_fwd_state(uint8_t);
    uint8_t get_fwd_state() const;
    /*Data Pull. Indicates the inclusion of an implied Read request in the Data response*/
    void set_data_pull(bool);
    bool get_data_pull() const;

    /*A transaction request includes a TgtID that identifies the target node*/
    void set_tgt_id(uint16_t);
    uint16_t get_tgt_id() const;

    /*Trace Tag. Provides additional support for the debugging, tracing, and performance measurement of systems*/
    void set_trace_tag(bool);
    bool is_trace_tag() const;

private:
    uint8_t db_id{0}, pcrd_type{0}, resp_err{0}, fwd_state{0};
    bool data_pull{false};
    rsp_optype_e opcode{rsp_optype_e::Invalid};
    rsp_resptype_e resp{rsp_resptype_e::SnpResp_I};
    uint16_t tgt_id{0};
    bool trace_tag{false};
};

struct credit {
    credit() = default;
    credit(short unsigned count, chi::credit_type_e type)
    : count(count)
    , type(type) {}
    unsigned short count{1};
    credit_type_e type{credit_type_e::LINK};
};

struct chi_ctrl_extension : public tlm::tlm_extension<chi_ctrl_extension> {
    /**
     * @brief the default constructor
     */
    chi_ctrl_extension() = default;
    /**
     * @brief the copy constructor
     * @param the extension to copy from
     */
    chi_ctrl_extension(const chi_ctrl_extension& o) = default;
    /**
     * @brief the clone function to create deep copies of
     * @return pointer to heap-allocated extension
     */
    tlm::tlm_extension_base* clone() const { return new chi_ctrl_extension(*this); }
    /**
     * @brief deep copy all values from ext
     * @param ext
     */
    void copy_from(tlm::tlm_extension_base const& ext) {
        *this = static_cast<const chi_ctrl_extension&>(ext);
    } // use assignment operator

    void set_txn_id(unsigned int id) { cmn.set_txn_id(id); }

    unsigned int get_txn_id() const { return cmn.get_txn_id(); }

    void set_src_id(unsigned int id) { cmn.set_src_id(id); }

    unsigned int get_src_id() const { return cmn.get_src_id(); }

    void set_qos(uint8_t qos) { cmn.set_qos(qos); }

    unsigned int get_qos() const { return cmn.get_qos(); }

    common cmn;
    request req;
    response resp;
};

struct chi_snp_extension : public tlm::tlm_extension<chi_snp_extension> {
    /**
     * @brief the default constructor
     */
    chi_snp_extension() = default;
    /**
     * @brief the copy constructor
     * @param the extension to copy from
    chi_snp_req_extension(const chi_snp_req_extension* o) : chi_extension<snp_request>(o){}
     */
    /**
     * @brief the clone function to create deep copies of
     * @return pointer to heap-allocated extension
     */
    tlm::tlm_extension_base* clone() const { return new chi_snp_extension(*this); };
    /**
     * @brief deep copy all values from ext
     * @param ext
     */
    void copy_from(tlm::tlm_extension_base const& ext) { *this = static_cast<const chi_snp_extension&>(ext); }

    void set_txn_id(unsigned int id) { cmn.set_txn_id(id); }

    unsigned int get_txn_id() const { return cmn.get_txn_id(); }

    void set_src_id(unsigned int id) { cmn.set_src_id(id); }

    unsigned int get_src_id() const { return cmn.get_src_id(); }

    void set_qos(uint8_t qos) { cmn.set_qos(qos); }

    unsigned int get_qos() const { return cmn.get_qos(); }

    common cmn;
    snp_request req;
    response resp;
};

struct chi_data_extension : public tlm::tlm_extension<chi_data_extension> {
    /**
     * @brief the default constructor
     */
    chi_data_extension() = default;
    /**
     * @brief the copy constructor
     * @param the extension to copy from
    chi_data_extension(const chi_data_extension* o) {}
     */
    /**
     * @brief the clone function to create deep copies of
     * @return pointer to heap-allocated extension
     */
    tlm::tlm_extension_base* clone() const { return new chi_data_extension(*this); }
    /**
     * @brief deep copy all values from ext
     * @param ext
     */
    void copy_from(tlm::tlm_extension_base const& ext) {
        *this = static_cast<const chi_data_extension&>(ext);
    } // use assignment operator

    void set_txn_id(unsigned int id) { cmn.set_txn_id(id); }

    unsigned int get_txn_id() const { return cmn.get_txn_id(); }

    void set_src_id(unsigned int id) { cmn.set_src_id(id); }

    unsigned int get_src_id() const { return cmn.get_src_id(); }

    void set_qos(uint8_t qos) { cmn.set_qos(qos); }

    unsigned int get_qos() const { return cmn.get_qos(); }

    common cmn{};
    data dat{};
};

struct chi_credit_extension : public tlm::tlm_extension<chi_credit_extension>, public credit {
    /**
     * @brief the default constructor
     */
    chi_credit_extension() = default;

    chi_credit_extension(credit_type_e type, unsigned short count = 1)
    : credit(count, type) {}
    /**
     * @brief the copy constructor
     * @param the extension to copy from
    chi_credit_extension(const chi_credit_extension* o) : chi_extension<response>(o){}
     */
    /**
     * @brief the clone function to create deep copies of
     * @return pointer to heap-allocated extension
     */
    tlm::tlm_extension_base* clone() const { return new chi_credit_extension(*this); };
    /**
     * @brief deep copy all values from ext
     * @param ext
     */
    void copy_from(tlm::tlm_extension_base const& ext) {
        *this = static_cast<const chi_credit_extension&>(ext);
    } // use assignment operator
};

//! aliases for payload and phase types
using chi_payload = tlm::tlm_generic_payload;
using chi_phase = tlm::tlm_phase;
/**
 * @brief The AXI protocol traits class.
 * Since the protocoll defines additional non-ignorable phases a dedicated protocol traits class has to be defined.
 */
struct chi_protocol_types {
    typedef chi_payload tlm_payload_type;
    typedef chi_phase tlm_phase_type;
};

/**
 * definition of the additional protocol phases
 */
DECLARE_EXTENDED_PHASE(BEGIN_PARTIAL_DATA);
DECLARE_EXTENDED_PHASE(END_PARTIAL_DATA);
DECLARE_EXTENDED_PHASE(BEGIN_DATA);
DECLARE_EXTENDED_PHASE(END_DATA);
DECLARE_EXTENDED_PHASE(ACK);

//! alias declaration for the forward interface
template <typename TYPES = chi::chi_protocol_types> using chi_fw_transport_if = tlm::tlm_fw_transport_if<TYPES>;
////! alias declaration for the backward interface:
// template <typename TYPES = chi::chi_protocol_types> using chi_bw_transport_if = tlm::tlm_bw_transport_if<TYPES>;

/**
 * interface definition for the blocking backward interface. This is need to allow snoop accesses in blocking mode
 */
template <typename TRANS = tlm::tlm_generic_payload>
class bw_blocking_transport_if : public virtual sc_core::sc_interface {
public:
    /**
     * @brief snoop access to a snooped master
     * @param trans the payload
     * @param t annotated delay
     */
    virtual void b_snoop(TRANS& trans, sc_core::sc_time& t) = 0;
};

/**
 *  The CHI backward interface which combines the TLM2.0 backward interface and the @see bw_blocking_transport_if
 */
template <typename TYPES = chi::chi_protocol_types>
class chi_bw_transport_if : public tlm::tlm_bw_transport_if<TYPES>,
                            public virtual bw_blocking_transport_if<typename TYPES::tlm_payload_type> {};

/**
 * CHI initiator socket class using payloads carrying CHI transaction request and response (RN to HN request and HN to
 * RN response)
 */
template <unsigned int BUSWIDTH = 32, typename TYPES = chi_protocol_types, int N = 1,
          sc_core::sc_port_policy POL = sc_core::SC_ONE_OR_MORE_BOUND>
struct chi_initiator_socket
: public tlm::tlm_base_initiator_socket<BUSWIDTH, chi_fw_transport_if<TYPES>, chi_bw_transport_if<TYPES>, N, POL> {
    //! base type alias
    using base_type =
        tlm::tlm_base_initiator_socket<BUSWIDTH, chi_fw_transport_if<TYPES>, chi_bw_transport_if<TYPES>, N, POL>;
    /**
     * @brief default constructor using a generated instance name
     */
    chi_initiator_socket()
    : base_type() {}
    /**
     * @brief constructor with instance name
     * @param name
     */
    explicit chi_initiator_socket(const char* name)
    : base_type(name) {}
    /**
     * @brief get the kind of this sc_object
     * @return the kind string
     */
    const char* kind() const override { return "chi_trx_initiator_socket"; }
#if SYSTEMC_VERSION >= 20181013 // not the right version but we assume TLM is always bundled with SystemC
    /**
     * @brief get the type of protocol
     * @return the kind typeid
     */
    sc_core::sc_type_index get_protocol_types() const override { return typeid(TYPES); }
#endif
};

/**
 * CHI target socket class using payloads carrying CHI transaction request and response (RN to HN request and HN to RN
 * response)
 */
template <unsigned int BUSWIDTH = 32, typename TYPES = chi_protocol_types, int N = 1,
          sc_core::sc_port_policy POL = sc_core::SC_ONE_OR_MORE_BOUND>
struct chi_target_socket
: public tlm::tlm_base_target_socket<BUSWIDTH, chi_fw_transport_if<TYPES>, chi_bw_transport_if<TYPES>, N, POL> {
    //! base type alias
    using base_type =
        tlm::tlm_base_target_socket<BUSWIDTH, chi_fw_transport_if<TYPES>, chi_bw_transport_if<TYPES>, N, POL>;
    /**
     * @brief default constructor using a generated instance name
     */
    chi_target_socket()
    : base_type() {}
    /**
     * @brief constructor with instance name
     * @param name
     */
    explicit chi_target_socket(const char* name)
    : base_type(name) {}
    /**
     * @brief get the kind of this sc_object
     * @return the kind string
     */
    const char* kind() const override { return "chi_trx_target_socket"; }
#if SYSTEMC_VERSION >= 20181013
    /**
     * @brief get the type of protocol
     * @return the kind typeid
     */
    sc_core::sc_type_index get_protocol_types() const override { return typeid(TYPES); }
#endif
};
/*****************************************************************************
 * free function easing handling of transactions and extensions
 *****************************************************************************/

template <typename EXT> inline bool is_valid(EXT& ext) { return is_valid(&ext); }

template <typename EXT> bool is_valid(EXT* ext);

inline bool is_dataless(const chi::chi_ctrl_extension* req_e) {
    auto opcode = req_e->req.get_opcode();
    if(opcode == chi::req_optype_e::CleanShared || opcode == chi::req_optype_e::CleanInvalid ||
       opcode == chi::req_optype_e::MakeInvalid || opcode == chi::req_optype_e::CleanUnique ||
       opcode == chi::req_optype_e::MakeUnique || opcode == chi::req_optype_e::CleanSharedPersist ||
       opcode == chi::req_optype_e::Evict) {
        return true;
    }
    return false;
}

/*****************************************************************************
 * Implementation details
 *****************************************************************************/
template <> struct enable_for_enum<req_optype_e> { static const bool enable = true; };
template <> struct enable_for_enum<snp_optype_e> { static const bool enable = true; };
template <> struct enable_for_enum<dat_optype_e> { static const bool enable = true; };
template <> struct enable_for_enum<dat_resptype_e> { static const bool enable = true; };
template <> struct enable_for_enum<rsp_optype_e> { static const bool enable = true; };
template <> struct enable_for_enum<rsp_resptype_e> { static const bool enable = true; };

template <> inline req_optype_e into<req_optype_e>(typename std::underlying_type<req_optype_e>::type t) {
    assert(t <= static_cast<std::underlying_type<req_optype_e>::type>(req_optype_e::PrefetchTgt));
    return static_cast<req_optype_e>(t);
}

template <> inline snp_optype_e into<snp_optype_e>(typename std::underlying_type<snp_optype_e>::type t) {
    assert(t <= static_cast<std::underlying_type<snp_optype_e>::type>(snp_optype_e::SnpUniqueFwd));
    return static_cast<snp_optype_e>(t);
}

template <> inline dat_optype_e into<dat_optype_e>(typename std::underlying_type<dat_optype_e>::type t) {
    assert(t <= static_cast<std::underlying_type<dat_optype_e>::type>(dat_optype_e::NCBWrDataCompAck));
    return static_cast<dat_optype_e>(t);
}

template <> inline dat_resptype_e into<dat_resptype_e>(typename std::underlying_type<dat_resptype_e>::type t) {
    assert(t <= static_cast<std::underlying_type<dat_resptype_e>::type>(dat_resptype_e::CopyBackWrData_SD_PD));
    return static_cast<dat_resptype_e>(t);
}

template <> inline rsp_optype_e into<rsp_optype_e>(typename std::underlying_type<rsp_optype_e>::type t) {
    assert(t >= static_cast<typename std::underlying_type<rsp_optype_e>::type>(rsp_optype_e::RespLCrdReturn) &&
           t <= static_cast<std::underlying_type<rsp_optype_e>::type>(rsp_optype_e::RespSepData));
    return static_cast<rsp_optype_e>(t);
}

template <> inline rsp_resptype_e into<rsp_resptype_e>(typename std::underlying_type<rsp_resptype_e>::type t) {
    assert(t <= static_cast<std::underlying_type<rsp_resptype_e>::type>(rsp_resptype_e::SnpResp_SD));
    return static_cast<rsp_resptype_e>(t);
}

// [1]  Implementation of 'request' structure member functions

//! set the transition id value of REQ,WDAT,RDAT and CRSP a particular channel
//! @param the TxnID of the channel
// A transaction request includes a TxnID that is used to identify the transaction from a given Requester
inline void common::set_txn_id(unsigned int txn_id) {
    sc_assert(txn_id <= 1024); // TxnID field is defined to accommodate up to 1024 outstanding transactions.
    this->txn_id = txn_id;
}

//! get the transition id value of a particular channel
//! @return the TxnID
inline unsigned int common::get_txn_id() const { return txn_id; }

//! set the source id value of REQ,WDAT,RDAT and CRSP a particular channel
//! @param the TxnID of the channel
inline void common::set_src_id(unsigned int src_id) { this->src_id = src_id; }

//! get the source id value of a particular channel
//! @return the SrcID
inline unsigned int common::get_src_id() const { return src_id; }

//! set the chi qos value of a particular channel
//! @param chi qos value of the REQ,WDAT,RDAT and CRSP channel
inline void common::set_qos(uint8_t qos) { this->qos = qos; }

//! get the chi qos value of a particular channel
//! @return chi qos value
inline unsigned int common::get_qos() const { return qos; }

// [1]  End of Implementation of 'common'

// [2]  Implementation of 'request' structure member functions

// A transaction request includes a TgtID that identifies the target node
inline void request::set_tgt_id(uint8_t id) { tgt_id = id; }
inline uint8_t request::get_tgt_id() const { return tgt_id; }

/*Logical Processor ID. Used in conjunction with the SrcID field to uniquely
identify the logical processor that generated the request. */
inline void request::set_lp_id(uint8_t id) { lp_id = id; }
inline uint8_t request::get_lp_id() const { return lp_id; }

/*Return Transaction ID. The unique transaction ID that conveys the value of
TxnID in the data response from the Slave*/
inline void request::set_return_txn_id(uint8_t id) { return_txn_id = id; }
inline uint8_t request::get_return_txn_id() const { return return_txn_id; }

// Stash Logical Processor ID. The ID of the logical processor at the Stash target.
inline void request::set_stash_lp_id(uint8_t id) { stash_lp_id = id; }
inline uint8_t request::get_stash_lp_id() const { return stash_lp_id; }

/*Data size. Specifies the size of the data associated with the transaction. This
  determines the number of data packets within the transaction */
inline void request::set_size(uint8_t sz) {
    assert(sz <= 8);
    size = sz;
}
inline uint8_t request::get_size() const { return size; }

/*Max number of flits in current transaction. The number determined based on data length and bus width*/
inline void request::set_max_flit(uint8_t data_id) { max_flit = data_id; }
inline uint8_t request::get_max_flit() const { return max_flit; }

/* Memory attribute. Determines the memory attributes associated with the
transaction. */
inline void request::set_mem_attr(uint8_t mem_attr) {
    assert(mem_attr < 16);
    this->mem_attr = mem_attr;
}
inline uint8_t request::get_mem_attr() const { return mem_attr; }

/* Protocol Credit Type. Indicates the type of Protocol Credit being used by a
   request that has the AllowRetry field deasserted */
inline void request::set_pcrd_type(uint8_t pcrd_type) { this->pcrd_type = pcrd_type; }
inline uint8_t request::get_pcrd_type() const { return pcrd_type; }

// Endianness. Indicates the endianness of Data in the Data packet.
inline void request::set_endian(bool endian) { this->endian = endian; }
inline bool request::is_endian() const { return this->endian; }

/*Order requirement. Determines the ordering requirement for this request with
  respect to other transactions from the same agent.*/
inline void request::set_order(uint8_t order) { this->order = order; }
inline uint8_t request::get_order() const { return this->order; }

/*Trace Tag. Provides additional support for the debugging, tracing, and
 performance measurement of systems. */
inline void request::set_trace_tag(bool trace_tag) { this->trace_tag = trace_tag; }
inline bool request::is_trace_tag() const { return trace_tag; }

inline void request::set_opcode(req_optype_e opcode) {
    // Check opcode validity
    assert(to_int(opcode) <= 0x3F);
    this->opcode = opcode;
}
inline req_optype_e request::get_opcode() const { return opcode; }

// Return Node ID. The node ID that the response with Data is to be sent to.
inline void request::set_return_n_id(uint16_t return_n_id) { this->return_n_id = return_n_id; }
inline uint16_t request::get_return_n_id() const { return return_n_id; }

// Stash Node ID is the node ID of the Stash target.
inline void request::set_stash_n_id(uint16_t stash_n_id) { this->stash_n_id = stash_n_id; }
inline uint16_t request::get_stash_n_id() const { return stash_n_id; }

/* Stash Node ID Valid. Indicates that the StashNID field has a valid Stash target
value. */
inline void request::set_stash_n_id_valid(bool stash_n_id_valid) { this->stash_n_id_valid = stash_n_id_valid; }
inline bool request::is_stash_n_id_valid() const { return stash_n_id_valid; }

/*Stash Logical Processor ID. The ID of the logical processor at the Stash target.*/
inline void request::set_stash_lp_id_valid(bool stash_lp_id_valid) { this->stash_lp_id_valid = stash_lp_id_valid; }
inline bool request::is_stash_lp_id_valid() const { return stash_lp_id_valid; }

/*Secure and Non-secure transactions are defined to support Secure and
Non-secure operating states.*/
inline void request::set_non_secure(bool ns) { this->ns = ns; }
inline bool request::is_non_secure() const { return ns; }

/* Expect CompAck. Indicates that the transaction will include a Completion
   Acknowledge message*/
inline void request::set_exp_comp_ack(bool exp_comp_ack) { this->exp_comp_ack = exp_comp_ack; }
inline bool request::is_exp_comp_ack() const { return exp_comp_ack; }

/* Allow Retry determines if the target is permitted to give a Retry response.*/
inline void request::set_allow_retry(bool allow_retry) { this->allow_retry = allow_retry; }
inline bool request::is_allow_retry() const { return allow_retry; }

/* Snoop attribute specifies the snoop attributes associated with the transaction.*/
inline void request::set_snp_attr(bool snp_attr) { this->snp_attr = snp_attr; }
inline bool request::is_snp_attr() const { return snp_attr; }

/*Exclusive access. Indicates that the corresponding transaction is an Exclusive
 access transaction.*/
inline void request::set_excl(bool excl) { this->excl = excl; }
inline bool request::is_excl() const { return this->excl; }

/* Snoop Me. Indicates that Home must determine whether to send a snoop to the
Requester.*/
inline void request::set_snoop_me(bool snoop_me) { this->snoop_me = snoop_me; }
inline bool request::is_snoop_me() const { return snoop_me; }

/*The LikelyShared attribute is a cache allocation hint. When asserted this attribute indicates
that the requested data is likely to be shared by other Request Nodes within the system */
inline void request::set_likely_shared(bool likely_shared) { this->likely_shared = likely_shared; }
inline bool request::is_likely_shared() const { return likely_shared; }

/* Reserved for customer use. Any value is valid in a Protocol flit. Propagation of this field through the interconnect
 is IMPLEMENTATION DEFINED.*/
inline void request::set_rsvdc(uint32_t rsvdc) { this->rsvdc = rsvdc; }
inline uint32_t request::get_rsvdc() const { return rsvdc; }

//===== End of [2] request

//==== [3] Starting of snp_request

inline void snp_request::set_fwd_txn_id(uint8_t fwd_txn_id) { this->fwd_txn_id = fwd_txn_id; }
inline uint8_t snp_request::get_fwd_txn_id() const { return fwd_txn_id; }

/*Stash Logical Processor ID. The ID of the logical processor at the Stash target.*/
inline void snp_request::set_stash_lp_id(uint8_t stash_lp_id) { this->stash_lp_id = stash_lp_id; }
inline uint8_t snp_request::get_stash_lp_id() const { return stash_lp_id; }

/* Stash Node ID Valid. Indicates that the StashNID field has a valid Stash target value. */
inline void snp_request::set_stash_lp_id_valid(bool stash_lp_id_valid) { this->stash_lp_id_valid = stash_lp_id_valid; }
inline bool snp_request::is_stash_lp_id_valid() const { return stash_lp_id_valid; }

/* Virtual Machine ID Extension use in DVM Operation types*/
inline void snp_request::set_vm_id_ext(uint8_t vm_id_ext) { this->vm_id_ext = vm_id_ext; }
inline uint8_t snp_request::get_vm_id_ext() const { return vm_id_ext; }

/*Forward Node ID is Node ID of the original Requester */
inline void snp_request::set_fwd_n_id(uint16_t fwd_n_id) { this->fwd_n_id = fwd_n_id; }
inline uint16_t snp_request::get_fwd_n_id() const { return fwd_n_id; }

inline void snp_request::set_opcode(snp_optype_e opcode) {
    // Check opcode validity
    this->opcode = opcode;
}
inline snp_optype_e snp_request::get_opcode() const { return opcode; }

/*Secure and Non-secure transactions are defined to support Secure and
Non-secure operating states.*/
inline void snp_request::set_non_secure(bool non_secure) // NS bit
{
    this->ns = non_secure;
}
inline bool snp_request::is_non_secure() const // NS bit
{
    return ns;
}

/* Do Not Go To SD state controls Snoopee use of SD state.It specifies that
the Snoopee must not transition to SD state as a result of the Snoop request.*/
inline void snp_request::set_do_not_goto_sd(bool do_not_goto_sd) { this->do_not_goto_sd = do_not_goto_sd; }
inline bool snp_request::is_do_not_goto_sd() const { return do_not_goto_sd; }

/*Do Not Data Pull. Instructs the Snoopee that it is not permitted to use the
Data Pull feature associated with Stash requests*/
inline void snp_request::set_do_not_data_pull(bool do_not_data_pull) { this->do_not_data_pull = do_not_data_pull; }
inline bool snp_request::is_do_not_data_pull() const { return do_not_data_pull; }

/*Return to Source. Instructs the receiver of the snoop to return Data with
the Snoop response.*/
inline void snp_request::set_ret_to_src(bool ret_to_src) { this->ret_to_src = ret_to_src; }
inline bool snp_request::is_ret_to_src() const { return ret_to_src; }

/*Trace Tag. Provides additional support for the debugging, tracing, and
performance measurement of systems*/
inline void snp_request::set_trace_tag(bool trace_tag) { this->trace_tag = trace_tag; }
inline bool snp_request::is_trace_tag() const { return trace_tag; }

//===== End of [3] snp_request

//==== [4] Starting of structure 'data' member functions

/* Data Buffer ID. The ID provided to be used as the TxnID in the response to
this message*/
inline void data::set_db_id(uint8_t db_id) { this->db_id = db_id; }
inline uint8_t data::get_db_id() const { return db_id; }

/*Response Error status. Indicates the error status associated with a data
  transfer*/
inline void data::set_resp_err(uint8_t resp_err) { this->resp_err = resp_err; }
inline uint8_t data::get_resp_err() const { return resp_err; }

/*Response status. Indicates the cache line state associated with a data transfer*/
inline void data::set_resp(dat_resptype_e resp) { this->resp = resp; }
inline dat_resptype_e data::get_resp() const { return resp; }
/*Forward State. Indicates the cache line state associated with a data transfer
  to the Requester from the receiver of the snoop*/
inline void data::set_fwd_state(uint8_t fwd_state) { this->fwd_state = fwd_state; }
inline uint8_t data::get_fwd_state() const { return fwd_state; }

/*Data Pull. Indicates the inclusion of an implied Read request in the Data
  response*/
inline void data::set_data_pull(uint8_t data_pull) { this->data_pull = data_pull; }
inline uint8_t data::get_data_pull() const { return data_pull; }
/* Data Source. The value indicates the source of the data in a read Data
   response*/
inline void chi::data::set_data_source(uint8_t data_source) { this->data_source = data_source; }
inline uint8_t data::get_data_source() const { return data_source; }

/* Critical Chunk Identifier. Replicates the address offset of the original
   transaction reques*/
inline void data::set_cc_id(uint8_t cc_id) { this->cc_id = cc_id; }
inline uint8_t data::get_cc_id() const { return cc_id; }
/*Data Identifier. Provides the address offset of the data provided in the packet*/

inline void data::set_data_id(uint8_t data_id) { this->data_id = data_id; }
inline uint8_t data::get_data_id() const { return data_id; }

/*Poison. Indicates that a set of data bytes has previously been corrupted*/
inline void data::set_poison(uint8_t poison) { this->poison = poison; }
inline uint8_t data::get_poison() const { return poison; }

/*A transaction request includes a TgtID that identifies the target node*/
inline void data::set_tgt_id(uint16_t tgt_id) { this->tgt_id = tgt_id; }
inline uint16_t data::get_tgt_id() const { return tgt_id; }

/*Home Node ID. The Node ID of the target of the CompAck response to be
  sent from the Requester*/
inline void data::set_home_n_id(uint16_t home_n_id) { this->home_n_id = home_n_id; }
inline uint16_t data::get_home_n_id() const { return home_n_id; }

/*Reserved for customer use. Any value is valid in a Protocol flit. Propagation of this field through the interconnect
  is IMPLEMENTATION DEFINED*/
inline void data::set_rsvdc(uint32_t rsvdc) { this->rsvdc = rsvdc; }
inline uint32_t data::get_rsvdc() const { return rsvdc; }

/*Data Check. Detects data errors in the DAT packet*/
inline void data::set_data_check(uint64_t data_check) { this->data_check = data_check; }
inline uint64_t data::get_data_check() const { return data_check; }

inline void data::set_opcode(dat_optype_e opcode) { this->opcode = opcode; }
inline dat_optype_e data::get_opcode() const { return opcode; }

/*Trace Tag. Provides additional support for the debugging, tracing, and
  performance measurement of systems*/
inline void data::set_trace_tag(bool trace_tag) { this->trace_tag = trace_tag; }
inline bool data::is_trace_tag() const { return trace_tag; }

//===== End of [4] data

//==== [5] Starting of structure 'response' member functions

/* Data Buffer ID. The ID provided to be used as the TxnID in the response to
        this message*/
inline void response::set_db_id(uint8_t db_id) { this->db_id = db_id; }
inline uint8_t response::get_db_id() const { return db_id; }

/*The PCrdType field indicates the credit type associated with the request*/
inline void response::set_pcrd_type(uint8_t pcrd_type) { this->pcrd_type = pcrd_type; }
inline uint8_t response::get_pcrd_type() const { return pcrd_type; }

/*Response Error status. Indicates the error status associated with a data
transfer*/
inline void response::set_resp_err(uint8_t resp_err) { this->resp_err = resp_err; }
inline uint8_t response::get_resp_err() const { return resp_err; }

/*Response status. Indicates the cache line state associated with a data transfer*/
inline void response::set_resp(rsp_resptype_e resp) { this->resp = resp; }
inline rsp_resptype_e response::get_resp() const { return resp; }
/*Forward State. Indicates the cache line state associated with a data transfer
to the Requester from the receiver of the snoop*/
inline void response::set_fwd_state(uint8_t fwd_state) { this->fwd_state = fwd_state; }
inline uint8_t response::get_fwd_state() const { return fwd_state; }

/*Data Pull. Indicates the inclusion of an implied Read request in the Data
response*/
inline void response::set_data_pull(bool data_pull) { this->data_pull = data_pull; }
inline bool response::get_data_pull() const { return data_pull; }

/*A transaction request includes a TgtID that identifies the target node*/
inline void response::set_tgt_id(uint16_t tgt_id) { this->tgt_id = tgt_id; }
inline uint16_t response::get_tgt_id() const { return tgt_id; }
inline void response::set_opcode(rsp_optype_e opcode) { this->opcode = opcode; }
inline rsp_optype_e response::get_opcode() const { return opcode; }
/*Trace Tag. Provides additional support for the debugging, tracing, and
performance measurement of systems*/
inline void response::set_trace_tag(bool trace_tag) { this->trace_tag = trace_tag; }
inline bool response::is_trace_tag() const { return trace_tag; }

//===== End of [5] response

} // end of namespace chi
