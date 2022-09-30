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

//! TLM2.0 components modeling AXI/ACE
namespace axi {
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

std::ostream& operator<<(std::ostream& os, tlm::tlm_generic_payload const& t);

/**
 * the burst type enumeration class
 */
enum class burst_e : uint8_t {
    FIXED = 0, /** fixed address burst */
    INCR = 1,  /** incrementing burst */
    WRAP = 2   /** wrapping burst */
};
/**
 * the lock type enumeration class
 */
enum class lock_e : uint8_t { NORMAL = 0x0, EXLUSIVE = 0x1, LOCKED = 0x2 };
/**
 * the domain type enumeration class
 */
enum class domain_e : uint8_t { NON_SHAREABLE = 0x0, INNER_SHAREABLE = 0x1, OUTER_SHAREABLE = 0x2, SYSTEM = 0x3 };
/**
 * the barrier type enumeration class
 */
enum class bar_e : uint8_t {
    RESPECT_BARRIER = 0x0,          //! Normal access, respecting barriers
    MEMORY_BARRIER = 0x1,           //! Memory barrier
    IGNORE_BARRIER = 0x2,           //! Normal access, ignoring barriers
    SYNCHRONISATION_BARRIER = 0x3   //!Synchronization barrier
};

/**
 * the snoop type enumeration class. Since the interpretation depends of other setting there are double defined entries
 */
enum class snoop_e : uint8_t {
    // clang-format off
	// non-snooping (domain==0 || domain==3, bar==0)
    READ_NO_SNOOP 			= 0x10,
    // Coherent (domain==1 || domain==2, bar==0)
    READ_ONCE 				= 0x0,
    READ_SHARED 			= 0x1,
    READ_CLEAN 				= 0x2,
    READ_NOT_SHARED_DIRTY 	= 0x3,
    READ_ONCE_CLEAN_INVALID	= 0x4, // ACE5
    READ_ONCE_MAKE_INVALID	= 0x5, // ACE5
    READ_UNIQUE 			= 0x7,
    CLEAN_UNIQUE 			= 0xb,
    MAKE_UNIQUE 			= 0xc,
    // Cache maintenance (domain==0 || domain==1 || domain==2, bar==0)
    CLEAN_SHARED 			= 0x8,
    CLEAN_INVALID 			= 0x9,
    CLEAN_SHARED_PERSIST 	= 0xa, // ACE5
    MAKE_INVALID 			= 0xd,
    // DVM (domain==1 || domain==2, bar==0)
    DVM_COMPLETE 			= 0xe,
    DVM_MESSAGE 			= 0xf,
    // Barrier (bar==1)
    BARRIER                 = 0x40,
    // non-snooping (domain==0 || domain==3, bar==0)
    WRITE_NO_SNOOP 			= 0x30,
    // Coherent (domain==1 || domain==2, bar==0)
    WRITE_UNIQUE 			= 0x20,
    WRITE_LINE_UNIQUE 		= 0x21,
    // Memory update (domain==0 || domain==1 || domain==2, bar==0)
    WRITE_CLEAN 			= 0x22,
    WRITE_BACK 				= 0x23,
    // (domain==1 || domain==2)
    EVICT 					= 0x24,
    WRITE_EVICT 			= 0x25,
    CMO_ON_WRITE 			= 0x26, // ACE5Lite
    // Cache Stash Transactions, ACE5Lite
    WRITE_UNIQUE_PTL_STASH 	= 0x28,
    WRITE_UNIQUE_FULL_STASH = 0x29,
    STASH_ONCE_SHARED 		= 0x2c,
    STASH_ONCE_UNIQUE 		= 0x2d,
    STASH_TRANSLATION 		= 0x2e
    // clang-format on
};

enum class atop_low_e { ADD = 0x0, CLR = 0x1, EOR = 0x2, SET = 0x3, SMAX = 0x4, SMIN = 0x5, UMAX = 0x6, UMIN = 0x7 };

enum class atop_enc_e {
    NonAtomic = 0x00,
    AtomicStore = 0x10,
    AtomicLoad = 0x20,
    AtomicSwap = 0x30,
    AtomicCompare = 0x31,
};
/**
 * the response type enumeration class
 */
enum class resp_e : uint8_t { OKAY = 0x0, EXOKAY = 0x1, SLVERR = 0x2, DECERR = 0x3 };
/**
 * the variable part of all requests containing AxID and AxUSER
 */
struct common {
    //! the constructor
    common() = default;
    //! reset all data member to their default
    void reset();
    /**
     * @brief copy assignment operator
     * @param o
     * @return reference to self
     */
    common& operator=(const common& o) {
        id = o.id;
        user = o.user;
        return *this;
    }
    /**
     * Since AXI has up to three channels per transaction the id and user fields are denoted by this enum
     */
    enum class id_type { CTRL, DATA, RESP };
    //! set the id value of a particular channel
    //! @param value of the AxID of the ADDR, DATA, and RESP channel
    void set_id(unsigned int value);
    //! get the id value of a particular channel
    //! @return the AxID
    unsigned int get_id() const;
    //! set the user value of a particular channel
    //! @param chnl AxUSER value of the ADDR, DATA, and RESP channel
    //! @param value of the user field
    void set_user(id_type chnl, unsigned int value);
    //! get the user value of a particular channel
    //! @return AxUSER value
    unsigned int get_user(id_type chnl) const;

protected:
    unsigned id{0};
    std::array<unsigned, 3> user{{0, 0, 0}};
};
/**
 * The request part of AXI3, AXI4 and ACE transactions. All member data are stored in this
 * class to allow easy reuse of free'd memory and prevent memory fragmentation.
 * The request class holds the data in a similar representation than in the HW implementation. Derived
 * classes are not expected to add data member, they only add interpretation of bits and fields as
 * member functions
 */
struct request {
    //! the default constructor
    request() = default;
    //! reset all data member to their default
    void reset();
    /**
     * @brief set the AxLEN value of the transaction, the value denotes the burst length - 1
     * @param the len value
     */
    void set_length(uint8_t);
    /**
     * @brief get the AxLEN value of the transaction, the value denotes the burst length - 1
     * @return the AxLEN value
     */
    uint8_t get_length() const;
    /**
     * @brief get the AxSIZE value of the transaction, the length is 2^size. It needs to be less than 10 (512 bit width)
     * @param size value of AxSIZE
     */
    void set_size(uint8_t);
    /**
     * @brief set the AxSIZE value of the transaction
     * @return AxSIZE value
     */
    uint8_t get_size() const;
    /**
     * @brief set the AxBURST value, @see burst_e
     * @param the burst
     */
    void set_burst(burst_e);
    /**
     * @brief get the AxBURST value, @see burst_e
     * @return
     */
    burst_e get_burst() const;
    /**
     * @brief set the AxPROT value as POD, only values from 0...7 are allowed
     * @param  the prot value
     */
    void set_prot(uint8_t);
    /**
     * @brief set the AxPROT value as POD, only values from 0...7 are allowed
     * @return the prot value
     */
    uint8_t get_prot() const;
    /**
     * @brief set the privileged bit of the AxPROT (AxPROT[0])
     * @param the privileged value
     */
    void set_privileged(bool = true);
    /**
     * @brief get the privileged bit of the AxPROT (AxPROT[0])
     * @return the privileged value
     */
    bool is_privileged() const;
    /**
     * @brief set the non-secure bit of the AxPROT (AxPROT[1])
     * @param the non-secure bit
     */
    void set_non_secure(bool = true);
    /**
     * @brief set the non-secure bit of the AxPROT (AxPROT[1])
     * @return the non-secure bit
     */
    bool is_non_secure() const;
    /**
     * @brief set the instruction bit of the AxPROT (AxPROT[2])
     * @param the instruction bit
     */
    void set_instruction(bool = true);
    /**
     * @brief set the instruction bit of the AxPROT (AxPROT[2])
     * @return the instruction bit
     */
    bool is_instruction() const;
    /**
     * @brief set the AxCACHE value as POD, only value from 0..15 are allowed
     * @param the cache value
     */
    void set_cache(uint8_t);
    /**
     * @brief get the AxCACHE value as POD
     * @return the cache value
     */
    uint8_t get_cache() const;
    /**
     * @brief set the AxQOS (quality of service) value
     * @param the qos value
     */
    void set_qos(uint8_t);
    /**
     * @brief get the AxQOS (quality of service) value
     * @return the qos value
     */
    uint8_t get_qos() const;
    /**
     * @brief set the AxREGION value
     * @param the region value
     */
    void set_region(uint8_t);
    /**
     * @brief get the AxREGION value
     * @return the region value
     */
    uint8_t get_region() const;
    /**
     * @brief set the raw AWATOP value
     * @param the atop value
     */
    void set_atop(uint8_t);
    /**
     * @brief get the raw AWATOP value
     * return the unique value
     */
    uint8_t get_atop() const;
    /**
     * @brief set the raw AWSTASHNID value
     * @param the atop value
     */
    void set_stash_nid(uint8_t);
    /**
     * @brief get the raw AWSTASHNID value
     * return the unique value
     */
    uint8_t get_stash_nid() const;
    /**
     * @brief check if AWSTASHNID is valid
     * return the valid value
     */
    bool is_stash_nid_en() const;
    /**
     * @brief set the raw AWSTASHLPID value
     * @param the atop value
     */
    void set_stash_lpid(uint8_t);
    /**
     * @brief get the raw AWSTASHLPID value
     * return the unique value
     */
    uint8_t get_stash_lpid() const;
    /**
     * @brief check if AWSTASHLPID is valid
     * return the valid value
     */
    bool is_stash_lpid_en() const;

protected:
    /**
     * equal operator
     * @param o
     * @return reference to self
     */
    request& operator=(const request& o) {
        length = o.length;
        size = o.size;
        burst = o.burst;
        prot = o.prot;
        qos = o.qos;
        region = o.region;
        domain = o.domain;
        snoop = o.snoop;
        barrier = o.barrier;
        lock = o.lock;
        cache = o.cache;
        unique = o.unique;
        atop = o.atop;
        return *this;
    }

    enum { // bit masks
        BUFFERABLE = 1,
        CACHEABLE = 2,
        RA = 4,
        WA = 8,
        EXCL = 1,
        LOCKED = 2,
        PRIVILEGED = 1,
        SECURE = 2,
        INSTRUCTION = 4
    };
    bool unique{false};
    // sums up to sizeof(bool) +11 bytes +  sizeof(response)= 16bytes
    uint8_t length{0};
    uint8_t size{0};
    burst_e burst{burst_e::FIXED};
    uint8_t prot{0};
    uint8_t qos{0};
    uint8_t region{0};
    domain_e domain{domain_e::NON_SHAREABLE};
    snoop_e snoop{snoop_e::READ_NO_SNOOP};
    bar_e barrier{bar_e::RESPECT_BARRIER};
    lock_e lock{lock_e::NORMAL};
    uint8_t cache{0};
    uint8_t atop{0};
    uint16_t stash_nid{std::numeric_limits<uint16_t>::max()};
    uint8_t stash_lpid{std::numeric_limits<uint8_t>::max()};
};
/**
 * The AXI3 specific interpretation of request data members
 */
struct axi3 : public request {
    /**
     * equal operator
     * @param o
     * @return reference to self
     */
    axi3& operator=(const axi3& o) {
        request::operator=(o);
        return *this;
    }
    /**
     * @brief get the exclusive bit of AxLOCK (AxLOCK[0])
     * @param the exclusive bit
     */
    void set_exclusive(bool = true);
    /**
     * @brief get the exclusive bit of AxLOCK (AxLOCK[0])
     * return the exclusive bit
     */
    bool is_exclusive() const;
    /**
     * @brief get the locked bit of AxLOCK (AxLOCK[1])
     * @param the locked bit
     */
    void set_locked(bool = true);
    /**
     * @brief get the locked bit of AxLOCK (AxLOCK[1])
     * return the locked bit
     */
    bool is_locked() const;
    /**
     * @brief set the bufferable bit of AxCACHE (AxCACHE[0])
     * @param the bufferable bit
     */
    void set_bufferable(bool = true);
    /**
     * @brief get the bufferable bit of AxCACHE (AxCACHE[0])
     * return the bufferable bit
     */
    bool is_bufferable() const;
    /**
     * @brief set the cacheable bit of AxCACHE (AxCACHE[1])
     * @param the cacheable bit
     */
    void set_cacheable(bool = true);
    /**
     * @brief get the cacheable bit of AxCACHE (AxCACHE[1])
     * return the cacheable bit
     */
    bool is_cacheable() const;
    /**
     * @brief set the write_allocate bit of AxCACHE (AxCACHE[2])
     * @param the write_allocate bit
     */
    void set_write_allocate(bool = true);
    /**
     * @brief get the write_allocate bit of AxCACHE (AxCACHE[2])
     * return the write_allocate bit
     */
    bool is_write_allocate() const;
    /**
     * @brief set the read_allocate bit of AxCACHE (AxCACHE[3])
     * @param the read_allocate bit
     */
    void set_read_allocate(bool = true);
    /**
     * @brief get the read_allocate bit of AxCACHE (AxCACHE[3])
     * return the read_allocate bit
     */
    bool is_read_allocate() const;
};
/**
 * The AXI4 specific interpretation of request data members
 */
struct axi4 : public request {
    /**
     * equal operator
     * @param o
     * @return reference to self
     */
    axi4& operator=(const axi4& o) {
        request::operator=(o);
        return *this;
    }
    /**
     * @brief get the exclusive bit of AxLOCK (AxLOCK[0])
     * @param the exclusive bit
     */
    void set_exclusive(bool = true);
    /**
     * @brief get the exclusive bit of AxLOCK (AxLOCK[0])
     * return the exclusive bit
     */
    bool is_exclusive() const;
    /**
     * @brief set the bufferable bit of AxCACHE (AxCACHE[0])
     * @param the bufferable bit
     */
    void set_bufferable(bool = true);
    /**
     * @brief get the bufferable bit of AxCACHE (AxCACHE[0])
     * return the bufferable bit
     */
    bool is_bufferable() const;
    /**
     * @brief set the modifiable bit of AxCACHE (AxCACHE[1])
     * @param the modifiable bit
     */
    void set_modifiable(bool = true);
    /**
     * @brief get the modifiable bit of AxCACHE (AxCACHE[1])
     * return the modifiable bit
     */
    bool is_modifiable() const;
    /**
     * @brief set the read allocate/write other allocate bit of AxCACHE (AxCACHE[2])
     * @param the read_other_allocate bit
     */
    void set_read_other_allocate(bool = true);
    /**
     * @brief get the read allocate/write other allocate bit of AxCACHE (AxCACHE[2])
     * return the read_other_allocate bit
     */
    bool is_read_other_allocate() const;
    /**
     * @brief set the write allocate/read other allocate bit of AxCACHE (AxCACHE[3])
     * @param the write_other_allocate bit
     */
    void set_write_other_allocate(bool = true);
    /**
     * @brief get the write allocate/read other allocate bit of AxCACHE (AxCACHE[3])
     * return the write_other_allocate bit
     */
    bool is_write_other_allocate() const;
};
/**
 * The ACE specific interpretation of request data members extending the AXI4 one
 */
struct ace : public axi4 {
    /**
     * equal operator
     * @param o
     * @return reference to self
     */
    ace& operator=(const ace& o) {
        request::operator=(o);
        return *this;
    }
    /**
     * @brief set the AxDOMAIN value
     * @param the domain value
     */
    void set_domain(domain_e);
    /**
     * @brief get the AxDOMAIN value
     * return the domain value
     */
    domain_e get_domain() const;
    /**
     * @brief set the AxSNOOP value
     * @param the snoop value
     */
    void set_snoop(snoop_e);
    /**
     * @brief get the AxSNOOP value
     * return the snoop value
     */
    snoop_e get_snoop() const;
    /**
     * @brief set the AxBAR value
     * @param the barrier value
     */
    void set_barrier(bar_e);
    /**
     * @brief get the AxBAR value
     * return the barrier value
     */
    bar_e get_barrier() const;
    /**
     * @brief set the AxUNIQUE value
     * @param the unique value
     */
    void set_unique(bool);
    /**
     * @brief get the AxUNIQUE value
     * return the unique value
     */
    bool get_unique() const;
};
/**
 * the response status of an AXI transaction
 */
struct response {
    response() = default;
    /**
     * @brief assignment operator
     * @param o
     * @return reference to self
     */
    response& operator=(const response& o) {
        resp = o.resp;
        return *this;
    }
    /**
     * @brief reset all data member to their default
     */
    void reset();
    /**
     * @brief converts the response status of a generic payload to a @see resp_e type
     * @param the tlm gp response status
     * @return the axi response status
     */
    static resp_e from_tlm_response_status(tlm::tlm_response_status);
    /**
     * @brief converts a @see resp_e type to a response status of a generic payload
     * @param the axi response status
     * @return the tlm gp response status
     */
    static tlm::tlm_response_status to_tlm_response_status(resp_e);
    /**
     * @brief set the response status as POD
     * @param the status
     */
    void set_resp(resp_e);
    /**
     * @brief get the response status as POD
     * @return the status
     */
    resp_e get_resp() const;
    /**
     * @brief check if the response status is OKAY
     * @return true if the status was ok
     */
    bool is_okay() const;
    /**
     * @brief set the response status to OKAY
     */
    void set_okay();
    /**
     * @brief check if the response status is EXOKAY
     * @return true if the status was exok
     */
    bool is_exokay() const;
    /**
     * @brief set the response status to EXOKAY
     */
    void set_exokay();
    /**
     * @brief check if the response status is SLVERR
     * @return true if the status was slverr
     */
    bool is_slverr() const;
    /**
     * @brief set the response status to SLVERR
     */
    void set_slverr();
    /**
     * @brief check if the response status is DECERR
     * @return true if the status was decerr
     */
    bool is_decerr() const;
    /**
     * @brief set the response status to DECERR
     */
    void set_decerr();

protected:
    enum { // bit masks for CRESP
        DATATRANSFER = 1,
        SNOOPEERROR = 2,
        PASSDIRTY = 4,
        ISSHARED = 8,
        WASUNIQUE = 16
    };
    uint8_t resp{static_cast<uint8_t>(resp_e::OKAY)};
};
/**
 * the response status of an ACE and SNOOP transaction extending the AXI one
 */
struct ace_response : public response {
    /**
     * @brief set the coherent response status
     * @param the status
     */
    void set_cresp(uint8_t);
    /**
     * @brief get the coherent response status
     * @return the status
     */
    uint8_t get_cresp() const;
    /**
     * @brief check the response status bit PassDirty (CRESP[2])
     * @return true if the status was pass dirty
     */
    bool is_pass_dirty() const;
    /**
     * @brief set the response status bit PassDirty
     * @param the pass dirty status
     */
    void set_pass_dirty(bool = true);
    /**
     * @brief check the response status bit IsShared (CRESP[3])
     * @return true if the status was shared
     */
    bool is_shared() const;
    /**
     * @brief set the response status bit IsShared
     * @param the shared status
     */
    void set_shared(bool = true);
    /**
     * @brief check the response status bit DataTransfer
     * @return true if there is data to be transfered
     */
    // CRRESP[0]
    bool is_snoop_data_transfer() const;
    /**
     * @brief set the response status bit DataTransfer
     * @param true if there is data to be transfered false otherwise
     */
    void set_snoop_data_transfer(bool = true);
    /**
     * @brief check the response status bit Error
     * @return true if the was an error
     */
    // CRRESP[1]
    bool is_snoop_error() const;
    /**
     * @brief set the response status bit Error
     * @param true if there is an error false otherwise
     */
    void set_snoop_error(bool = true);
    /**
     * @brief check the response status bit WasUnique
     * @return true if the access was unique
     */
    // CRRESP[4]
    bool is_snoop_was_unique() const;
    /**
     * @brief set the response status bit WasUnique
     * @param the unique flag
     */
    void set_snoop_was_unique(bool = true);
};
/**
 * the template class forming an AXI extension as a combination of common, a request class and a response class
 */
template <typename REQ, typename RESP = response>
struct axi_extension : public common, // 2x 4byte
                       public REQ,    // 11byte + 4byte
                       public RESP    // 1byte
{
    /**
     * @brief the default constructor
     */
    axi_extension() = default;
    /**
     * @brief the copy constructor
     * @param the extension to copy from
     */
    axi_extension(const axi_extension<REQ, RESP>*);
    /**
     * the virtual destructor
     */
    virtual ~axi_extension() {}
    /**
     * @brief reset all data member to their default
     */
    void reset();
    /**
     * @brief reset the common and response part, reset response using the given reset value
     * @param the response reset value
     */
    void reset(const REQ*);
    /**
     * @brief add a read response to the response array
     * @param the response
     */
    void add_to_response_array(response&);
    /**
     * @brief return the read response array for constant instances
     * @return the const-qualified response array
     */
    const std::vector<response>& get_response_array() const;
    /**
     * @brief return the read response array
     * @return the response array
     */
    std::vector<response>& get_response_array();
    /**
     * @brief set the flag indicating the all read responses are collected
     * @param the flag value
     */
    void set_response_array_complete(bool = true);
    /**
     * get the flag indicating the all read responses are collected
     * @return the flag value
     */
    bool is_response_array_complete();

private:
    std::vector<response> response_arr{};
    bool response_array_complete{false};
};
/**
 * the AXI3 tlm extension class
 */
struct axi3_extension : public tlm::tlm_extension<axi3_extension>, public axi_extension<axi3> {
    /**
     * @brief the default constructor
     */
    axi3_extension() = default;
    /**
     * @brief the copy constructor
     * @param the extension to copy from
     */
    axi3_extension(const axi3_extension* o)
    : axi_extension<axi3>(o) {}
    /**
     * @brief the clone function to create deep copies of
     * @return pointer to heap-allocated extension
     */
    tlm::tlm_extension_base* clone() const override;
    /**
     * @brief deep copy all values from ext
     * @param ext
     */
    void copy_from(tlm::tlm_extension_base const& ext) override;
};
/**
 * the AXI4 tlm extension class
 */
struct axi4_extension : public tlm::tlm_extension<axi4_extension>, public axi_extension<axi4> {
    /**
     * @brief the default constructor
     */
    axi4_extension() = default;
    /**
     * @brief the copy constructor
     * @param the extension to copy from
     */
    axi4_extension(const axi4_extension* o)
    : axi_extension<axi4>(o) {}
    /**
     * @brief the clone function to create deep copies of
     * @return pointer to heap-allocated extension
     */
    tlm::tlm_extension_base* clone() const override;
    /**
     * @brief deep copy all values from ext
     * @param ext
     */
    void copy_from(tlm::tlm_extension_base const& ext) override;
};
/**
 * the ACE tlm extension class
 */
struct ace_extension : public tlm::tlm_extension<ace_extension>, public axi_extension<ace, ace_response> {
    /**
     * @brief the default constructor
     */
    ace_extension() = default;
    /**
     * @brief the copy constructor
     * @param the extension to copy from
     */
    ace_extension(const ace_extension* o)
    : axi_extension<ace, ace_response>(o) {}
    /**
     * @brief the clone function to create deep copies of
     * @return pointer to heap-allocated extension
     */
    tlm::tlm_extension_base* clone() const override;
    /**
     * @brief deep copy all values from ext
     * @param ext
     */
    void copy_from(tlm::tlm_extension_base const& ext) override;
};
//! aliases for payload and phase types
using axi_payload = tlm::tlm_generic_payload;
using axi_phase = tlm::tlm_phase;
/**
 * @brief The AXI protocol traits class.
 * Since the protocoll defines additional non-ignorable phases a dedicated protocol traits class has to be defined.
 */
struct axi_protocol_types {
    typedef axi_payload tlm_payload_type;
    typedef axi_phase tlm_phase_type;
};
/**
 * definition of the additional protocol phases
 */
DECLARE_EXTENDED_PHASE(BEGIN_PARTIAL_REQ);
DECLARE_EXTENDED_PHASE(END_PARTIAL_REQ);
DECLARE_EXTENDED_PHASE(BEGIN_PARTIAL_RESP);
DECLARE_EXTENDED_PHASE(END_PARTIAL_RESP);
DECLARE_EXTENDED_PHASE(ACK);
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
//! alias declaration for the forward interface
template <typename TYPES = axi_protocol_types> using axi_fw_transport_if = tlm::tlm_fw_transport_if<TYPES>;
//! alias declaration for the backward interface:
template <typename TYPES = axi_protocol_types> using axi_bw_transport_if = tlm::tlm_bw_transport_if<TYPES>;
//! alias declaration for the ACE forward interface
template <typename TYPES = axi_protocol_types> using ace_fw_transport_if = tlm::tlm_fw_transport_if<TYPES>;
/**
 *  The ACE backward interface which combines the TLM2.0 backward interface and the @see bw_blocking_transport_if
 */
template <typename TYPES = axi_protocol_types>
class ace_bw_transport_if : public tlm::tlm_bw_transport_if<TYPES>,
                            public virtual bw_blocking_transport_if<typename TYPES::tlm_payload_type> {};
/**
 * AXI initiator socket class using payloads carrying AXI3 or AXi4 extensions
 */
template <unsigned int BUSWIDTH = 32, typename TYPES = axi_protocol_types, int N = 1,
          sc_core::sc_port_policy POL = sc_core::SC_ONE_OR_MORE_BOUND>
struct axi_initiator_socket
: public tlm::tlm_base_initiator_socket<BUSWIDTH, axi_fw_transport_if<TYPES>, axi_bw_transport_if<TYPES>, N, POL> {
    //! base type alias
    using base_type =
        tlm::tlm_base_initiator_socket<BUSWIDTH, axi_fw_transport_if<TYPES>, axi_bw_transport_if<TYPES>, N, POL>;
    /**
     * @brief default constructor using a generated instance name
     */
    axi_initiator_socket()
    : base_type() {}
    /**
     * @brief constructor with instance name
     * @param name
     */
    explicit axi_initiator_socket(const char* name)
    : base_type(name) {}
    /**
     * @brief get the kind of this sc_object
     * @return the kind string
     */
    const char* kind() const override { return "axi_initiator_socket"; }
    // not the right version but we assume TLM is always bundled with SystemC
#if SYSTEMC_VERSION >= 20181013 // ((TLM_VERSION_MAJOR > 2) || (TLM_VERSION==2 && TLM_VERSION_MINOR>0) ||(TLM_VERSION==2
                                // && TLM_VERSION_MINOR>0 && TLM_VERSION_PATCH>4))
    sc_core::sc_type_index get_protocol_types() const override { return typeid(TYPES); }
#endif
};
/**
 * AXI target socket class using payloads carrying AXI3 or AXi4 extensions
 */
template <unsigned int BUSWIDTH = 32, typename TYPES = axi_protocol_types, int N = 1,
          sc_core::sc_port_policy POL = sc_core::SC_ONE_OR_MORE_BOUND>
struct axi_target_socket
: public tlm::tlm_base_target_socket<BUSWIDTH, axi_fw_transport_if<TYPES>, axi_bw_transport_if<TYPES>, N, POL> {
    //! base type alias
    using base_type =
        tlm::tlm_base_target_socket<BUSWIDTH, axi_fw_transport_if<TYPES>, axi_bw_transport_if<TYPES>, N, POL>;
    /**
     * @brief default constructor using a generated instance name
     */
    axi_target_socket()
    : base_type() {}
    /**
     * @brief constructor with instance name
     * @param name
     */
    explicit axi_target_socket(const char* name)
    : base_type(name) {}
    /**
     * @brief get the kind of this sc_object
     * @return the kind string
     */
    const char* kind() const override { return "axi_target_socket"; }
    // not the right version but we assume TLM is always bundled with SystemC
#if SYSTEMC_VERSION >= 20181013 // ((TLM_VERSION_MAJOR > 2) || (TLM_VERSION==2 && TLM_VERSION_MINOR>0) ||(TLM_VERSION==2
                                // && TLM_VERSION_MINOR>0 && TLM_VERSION_PATCH>4))
    sc_core::sc_type_index get_protocol_types() const override { return typeid(TYPES); }
#endif
};
/**
 * ACE initiator socket class using payloads carrying AXI3 or AXi4 extensions
 */
template <unsigned int BUSWIDTH = 32, typename TYPES = axi_protocol_types, int N = 1,
          sc_core::sc_port_policy POL = sc_core::SC_ONE_OR_MORE_BOUND>
struct ace_initiator_socket
: public tlm::tlm_base_initiator_socket<BUSWIDTH, ace_fw_transport_if<TYPES>, ace_bw_transport_if<TYPES>, N, POL> {
    //! base type alias
    using base_type =
        tlm::tlm_base_initiator_socket<BUSWIDTH, ace_fw_transport_if<TYPES>, ace_bw_transport_if<TYPES>, N, POL>;
    /**
     * @brief default constructor using a generated instance name
     */
    ace_initiator_socket()
    : base_type() {}
    /**
     * @brief constructor with instance name
     * @param name
     */
    explicit ace_initiator_socket(const char* name)
    : base_type(name) {}
    /**
     * @brief get the kind of this sc_object
     * @return the kind string
     */
    const char* kind() const override { return "axi_initiator_socket"; }
#if SYSTEMC_VERSION >= 20181013 // not the right version but we assume TLM is always bundled with SystemC
    /**
     * @brief get the type of protocol
     * @return the kind typeid
     */
    sc_core::sc_type_index get_protocol_types() const override { return typeid(TYPES); }
#endif
};
/**
 * ACE target socket class using payloads carrying AXI3 or AXi4 extensions
 */
template <unsigned int BUSWIDTH = 32, typename TYPES = axi_protocol_types, int N = 1,
          sc_core::sc_port_policy POL = sc_core::SC_ONE_OR_MORE_BOUND>
struct ace_target_socket
: public tlm::tlm_base_target_socket<BUSWIDTH, ace_fw_transport_if<TYPES>, ace_bw_transport_if<TYPES>, N, POL> {
    //! base type alias
    using base_type =
        tlm::tlm_base_target_socket<BUSWIDTH, ace_fw_transport_if<TYPES>, ace_bw_transport_if<TYPES>, N, POL>;
    /**
     * @brief default constructor using a generated instance name
     */
    ace_target_socket()
    : base_type() {}
    /**
     * @brief constructor with instance name
     * @param name
     */
    explicit ace_target_socket(const char* name)
    : base_type(name) {}
    /**
     * @brief get the kind of this sc_object
     * @return the kind string
     */
    const char* kind() const override { return "axi_target_socket"; }
    // not the right version but we assume TLM is always bundled with SystemC
#if SYSTEMC_VERSION >= 20181013 // not the right version but we assume TLM is always bundled with SystemC
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

template <typename EXT> bool is_valid(EXT& ext) { return is_valid(&ext); }

template <typename EXT> bool is_valid(EXT* ext) { return is_valid_msg(ext) == nullptr; }

template <typename EXT> char const* is_valid_msg(EXT& ext) { return is_valid_msg(&ext); }

template <typename EXT> char const* is_valid_msg(EXT* ext);

inline bool is_dataless(axi::ace_extension const* ext) {
    if(!ext)
        return false;
    auto snp = ext->get_snoop();
    return snp == snoop_e::CLEAN_UNIQUE || snp == snoop_e::MAKE_UNIQUE || snp == snoop_e::CLEAN_SHARED ||
           snp == snoop_e::CLEAN_INVALID || snp == snoop_e::MAKE_INVALID || snp == snoop_e::EVICT ||
           snp == snoop_e::STASH_ONCE_SHARED || snp == snoop_e::STASH_ONCE_UNIQUE;
}

inline unsigned get_axi_id(axi::axi_protocol_types::tlm_payload_type const& trans) {
    if(auto e = trans.get_extension<axi::ace_extension>())
        return e->get_id();
    if(auto e = trans.get_extension<axi::axi4_extension>())
        return e->get_id();
    if(auto e = trans.get_extension<axi::axi3_extension>())
        return e->get_id();
    sc_assert(false && "transaction is not an axi or ace transaction");
    return std::numeric_limits<unsigned>::max();
}

inline unsigned get_axi_id(axi::axi_protocol_types::tlm_payload_type const* trans) { return get_axi_id(*trans); }

/**
 * determine if the transaction is an AXI burst
 * @param trans
 * @return true if the access is a burst
 *
 * @todo The calculation needs to be checked and probably fixed
 */
inline bool is_burst(const axi::axi_protocol_types::tlm_payload_type& trans) {
    if(auto e = trans.get_extension<axi::ace_extension>())
        return e->get_length() > 0;
    if(auto e = trans.get_extension<axi::axi4_extension>())
        return e->get_length() > 0;
    if(auto e = trans.get_extension<axi::axi3_extension>())
        return e->get_length() > 0;
    sc_assert(false && "transaction is not an axi or ace transaction");
    return false;
}
/**
 * determine if the transaction is an AXI burst
 * @param trans
 * @return the number of beats
 */
inline bool is_burst(const axi::axi_protocol_types::tlm_payload_type* trans) { return is_burst(*trans); }
/**
 * get length of a burst
 * @param r
 * @return the number of beats
 */
inline unsigned get_burst_lenght(const request& r) { return r.get_length() + 1; }
/**
 * get length of a burst
 * @param r
 * @return the number of beats
 */
inline unsigned get_burst_lenght(const request* r) { return r->get_length() + 1; }
/**
 * get length of a burst
 * @param trans
 * @return the number of beats
 */
inline unsigned get_burst_lenght(const axi::axi_protocol_types::tlm_payload_type& trans) {
    if(auto e = trans.get_extension<axi::ace_extension>())
        return e->get_length() + 1;
    if(auto e = trans.get_extension<axi::axi4_extension>())
        return e->get_length() + 1;
    if(auto e = trans.get_extension<axi::axi3_extension>())
        return e->get_length() + 1;
    sc_assert(false && "transaction is not an axi or ace transaction");
    return 0;
}
/**
 * get length of a burst
 * @param trans
 * @return the number of beats
 */
inline unsigned get_burst_lenght(const axi::axi_protocol_types::tlm_payload_type* trans) {
    return get_burst_lenght(*trans);
}
/**
 * get size of a burst in bytes which is 2^AxBURST
 * @param r
 * @return the burst size in bytes
 */
inline unsigned get_burst_size(const request& r) { return 1 << r.get_size(); }
/**
 * get size of a burst in bytes which is 2^AxBURST
 * @param r
 * @return the burst size in bytes
 */
inline unsigned get_burst_size(const request* r) { return 1 << r->get_size(); }
/**
 * get size of a burst in bytes which is 2^AxBURST
 * @param trans
 * @return the burst size in bytes
 */
inline unsigned get_burst_size(const axi::axi_protocol_types::tlm_payload_type& trans) {
    if(auto e = trans.get_extension<axi::ace_extension>())
        return 1 << e->get_size();
    if(auto e = trans.get_extension<axi::axi4_extension>())
        return 1 << e->get_size();
    if(auto e = trans.get_extension<axi::axi3_extension>())
        return 1 << e->get_size();
    sc_assert(false && "transaction is not an axi or ace transaction");
    return 0;
}
/**
 * get size of a burst in bytes which is 2^AxBURST
 * @param trans
 * @return the burst size in bytes
 */
inline unsigned get_burst_size(const axi::axi_protocol_types::tlm_payload_type* trans) {
    return get_burst_size(*trans);
}

inline burst_e get_burst_type(const axi::axi_protocol_types::tlm_payload_type& trans) {
    if(auto e = trans.get_extension<axi::ace_extension>())
        return e->get_burst();
    if(auto e = trans.get_extension<axi::axi4_extension>())
        return e->get_burst();
    if(auto e = trans.get_extension<axi::axi3_extension>())
        return e->get_burst();
    sc_assert(false && "transaction is not an axi or ace transaction");
    return burst_e::FIXED;
}
inline burst_e get_burst_type(const axi::axi_protocol_types::tlm_payload_type* trans) { return get_burst_type(*trans); }

inline unsigned get_cache(const axi::axi_protocol_types::tlm_payload_type& trans) {
    if(auto e = trans.get_extension<axi::ace_extension>())
        return e->get_cache();
    if(auto e = trans.get_extension<axi::axi4_extension>())
        return e->get_cache();
    if(auto e = trans.get_extension<axi::axi3_extension>())
        return e->get_cache();
    sc_assert(false && "transaction is not an axi or ace transaction");
    return 0;
}
inline unsigned get_cache(const axi::axi_protocol_types::tlm_payload_type* trans) { return get_cache(*trans); }

/*****************************************************************************
 * Implementation details
 *****************************************************************************/
template <> struct enable_for_enum<burst_e> { static const bool enable = true; };
template <> struct enable_for_enum<lock_e> { static const bool enable = true; };
template <> struct enable_for_enum<domain_e> { static const bool enable = true; };
template <> struct enable_for_enum<bar_e> { static const bool enable = true; };
template <> struct enable_for_enum<snoop_e> { static const bool enable = true; };
template <> struct enable_for_enum<resp_e> { static const bool enable = true; };

template <> inline burst_e into<burst_e>(typename std::underlying_type<burst_e>::type t) {
    assert(t <= static_cast<std::underlying_type<burst_e>::type>(burst_e::WRAP));
    return static_cast<burst_e>(t);
}

template <> inline lock_e into<lock_e>(typename std::underlying_type<lock_e>::type t) {
    assert(t <= static_cast<std::underlying_type<lock_e>::type>(lock_e::LOCKED));
    return static_cast<lock_e>(t);
}

template <> inline domain_e into<domain_e>(typename std::underlying_type<domain_e>::type t) {
    assert(t <= static_cast<std::underlying_type<domain_e>::type>(domain_e::SYSTEM));
    return static_cast<domain_e>(t);
}

template <> inline bar_e into<bar_e>(typename std::underlying_type<bar_e>::type t) {
    assert(t <= static_cast<std::underlying_type<bar_e>::type>(bar_e::SYNCHRONISATION_BARRIER));
    return static_cast<bar_e>(t);
}

template <> inline snoop_e into<snoop_e>(typename std::underlying_type<snoop_e>::type t) {
    assert(t <= static_cast<std::underlying_type<snoop_e>::type>(snoop_e::WRITE_NO_SNOOP));
    return static_cast<snoop_e>(t);
}

template <> inline resp_e into<resp_e>(typename std::underlying_type<resp_e>::type t) {
    assert(t <= static_cast<std::underlying_type<resp_e>::type>(resp_e::DECERR));
    return static_cast<resp_e>(t);
}

inline void common::set_id(unsigned int id) { this->id = id; }

inline unsigned int common::get_id() const { return id; }

inline void common::set_user(id_type chnl, unsigned int user) { this->user[static_cast<size_t>(chnl)] = user; }

inline unsigned int common::get_user(id_type chnl) const { return user[static_cast<size_t>(chnl)]; }

inline void axi3::set_exclusive(bool exclusive) {
    uint8_t t = to_int(lock);
    if(exclusive)
        t |= EXCL;
    else
        t &= ~EXCL;
    lock = into<lock_e>(t);
}

inline bool axi3::is_exclusive() const {
    uint8_t t = to_int(lock);
    return (t & EXCL) != 0;
}

inline void axi3::set_locked(bool locked) {
    uint8_t t = to_int(lock);
    if(locked)
        t |= LOCKED;
    else
        t &= ~LOCKED;
    lock = into<lock_e>(t);
}

inline bool axi3::is_locked() const {
    uint8_t t = to_int(lock);
    return (t & LOCKED) != 0;
}

inline void axi3::set_bufferable(bool bufferable) {
    if(bufferable)
        cache |= BUFFERABLE;
    else
        cache &= ~BUFFERABLE;
}

inline bool axi3::is_bufferable() const { return (cache & BUFFERABLE) != 0; }

inline void axi3::set_cacheable(bool cacheable) {
    if(cacheable)
        cache |= CACHEABLE;
    else
        cache &= ~CACHEABLE;
}

inline bool axi3::is_cacheable() const { return (cache & CACHEABLE) != 0; }

inline void axi3::set_write_allocate(bool wa) {
    if(wa)
        cache |= WA;
    else
        cache &= ~WA;
}

inline bool axi3::is_write_allocate() const { return (cache & WA) != 0; }

inline void axi3::set_read_allocate(bool ra) {
    if(ra)
        cache |= RA;
    else
        cache &= ~RA;
}

inline bool axi3::is_read_allocate() const { return (cache & RA) != 0; }

inline void axi4::set_exclusive(bool exclusive) { lock = exclusive ? lock_e::EXLUSIVE : lock_e::NORMAL; }

inline bool axi4::is_exclusive() const { return lock == lock_e::EXLUSIVE; }

inline void axi4::set_bufferable(bool bufferable) {
    if(bufferable)
        cache |= BUFFERABLE;
    else
        cache &= ~BUFFERABLE;
}

inline bool axi4::is_bufferable() const { return (cache & BUFFERABLE) != 0; }

inline void axi4::set_modifiable(bool cacheable) {
    if(cacheable)
        cache |= CACHEABLE;
    else
        cache &= ~CACHEABLE;
}

inline bool axi4::is_modifiable() const { return (cache & CACHEABLE) != 0; }

inline void axi4::set_read_other_allocate(bool roa) {
    if(roa)
        cache |= WA;
    else
        cache &= ~WA;
}

inline bool axi4::is_read_other_allocate() const { return (cache & WA) != 0; }

inline void axi4::set_write_other_allocate(bool woa) {
    if(woa)
        cache |= RA;
    else
        cache &= ~RA;
}

inline bool axi4::is_write_other_allocate() const { return (cache & RA) != 0; }

inline void request::reset() {
    length = 0;
    size = 0;
    burst = burst_e::FIXED;
    prot = 0;
    qos = 0;
    region = 0;
    domain = domain_e::NON_SHAREABLE;
    snoop = snoop_e::READ_NO_SNOOP;
    barrier = bar_e::RESPECT_BARRIER;
    lock = lock_e::NORMAL;
    cache = 0;
    unique = false;
    atop = 0;
}

inline void request::set_length(uint8_t length) { this->length = length; }

inline uint8_t request::get_length() const { return length; }

inline void request::set_size(uint8_t size) {
    assert(size < 10);
    this->size = size;
}

inline uint8_t request::get_size() const { return size; }

inline void request::set_burst(burst_e burst) { this->burst = burst; }

inline burst_e request::get_burst() const { return burst; }

inline void request::set_prot(uint8_t prot) {
    assert(prot < 8);
    this->prot = prot;
}

inline uint8_t request::get_prot() const { return prot; }

inline void request::set_privileged(bool privileged) {
    prot &= ~PRIVILEGED;
    if(privileged)
        prot |= PRIVILEGED;
}

inline bool request::is_privileged() const { return (prot & PRIVILEGED) != 0; }

inline void request::set_non_secure(bool non_sec) {
    prot &= ~SECURE;
    if(non_sec)
        prot |= SECURE;
}

inline bool request::is_non_secure() const { return (prot & SECURE) != 0; }

inline void request::set_instruction(bool instr) {
    prot &= ~INSTRUCTION;
    if(instr)
        prot |= INSTRUCTION;
}

inline bool request::is_instruction() const { return (prot & INSTRUCTION) != 0; }

inline void request::set_qos(uint8_t qos) { this->qos = qos; }

inline uint8_t request::get_qos() const { return qos; }

inline void request::set_region(uint8_t region) { this->region = region; }

inline uint8_t request::get_region() const { return region; }

inline void request::set_cache(uint8_t cache) {
    assert(cache < 16);
    this->cache = cache;
}

inline uint8_t request::get_cache() const { return cache; }

inline void ace::set_domain(domain_e domain) { this->domain = domain; }

inline domain_e ace::get_domain() const { return domain; }

inline void ace::set_snoop(snoop_e snoop) { this->snoop = snoop; }

inline snoop_e ace::get_snoop() const { return snoop; }

inline void ace::set_barrier(bar_e bar) { this->barrier = bar; }

inline bar_e ace::get_barrier() const { return barrier; }

inline void ace::set_unique(bool uniq) { this->unique = uniq; }

inline bool ace::get_unique() const { return unique; }

inline void request::set_atop(uint8_t atop) { this->atop = atop; }

inline uint8_t request::get_atop() const { return atop; }

inline void request::set_stash_nid(uint8_t stash_nid) { this->stash_nid = stash_nid; }

inline uint8_t request::get_stash_nid() const { return stash_nid < 0x800 ? stash_nid : 0; }

inline bool request::is_stash_nid_en() const { return stash_nid < 0x800; }

inline void request::set_stash_lpid(uint8_t stash_lpid) { this->stash_lpid = stash_lpid; }

inline uint8_t request::get_stash_lpid() const { return stash_lpid < 0x20 ? stash_lpid : 0; }

inline bool request::is_stash_lpid_en() const { return stash_lpid < 0x20; }

inline void response::reset() { resp = static_cast<uint8_t>(resp_e::OKAY); }

inline resp_e response::from_tlm_response_status(tlm::tlm_response_status st) {
    switch(st) {
    case tlm::TLM_OK_RESPONSE:
        return resp_e::OKAY;
    case tlm::TLM_INCOMPLETE_RESPONSE:
    case tlm::TLM_ADDRESS_ERROR_RESPONSE:
        return resp_e::DECERR;
    default:
        return resp_e::SLVERR;
    }
}

inline tlm::tlm_response_status response::to_tlm_response_status(resp_e resp) {
    switch(to_int(resp) & 0x3) {
    case to_int(resp_e::OKAY):
        return tlm::TLM_OK_RESPONSE;
    case to_int(resp_e::EXOKAY):
        return tlm::TLM_OK_RESPONSE;
    case to_int(resp_e::DECERR):
        return tlm::TLM_ADDRESS_ERROR_RESPONSE;
    default:
        return tlm::TLM_GENERIC_ERROR_RESPONSE;
    }
}

inline void response::set_resp(resp_e resp) { this->resp = static_cast<uint8_t>(resp); }

inline resp_e response::get_resp() const { // @suppress("No return")
    switch(resp & 0x3) {
    case 0:
        return resp_e::OKAY;
    case 1:
        return resp_e::EXOKAY;
    case 2:
        return resp_e::SLVERR;
    default:
        return resp_e::DECERR;
    }
}

inline bool response::is_okay() const { return resp == static_cast<uint8_t>(resp_e::OKAY); }

inline void response::set_okay() { resp = static_cast<uint8_t>(resp_e::OKAY); }

inline bool response::is_exokay() const { return resp == static_cast<uint8_t>(resp_e::EXOKAY); }

inline void response::set_exokay() { resp = static_cast<uint8_t>(resp_e::EXOKAY); }

inline bool response::is_slverr() const { return resp == static_cast<uint8_t>(resp_e::SLVERR); }

inline void response::set_slverr() { resp = static_cast<uint8_t>(resp_e::SLVERR); }

inline bool response::is_decerr() const { return resp == static_cast<uint8_t>(resp_e::DECERR); }

inline void response::set_decerr() { resp = static_cast<uint8_t>(resp_e::DECERR); }

inline void ace_response::set_cresp(uint8_t resp) { this->resp = resp & 0x1f; }

inline uint8_t ace_response::get_cresp() const { return resp; }

inline bool ace_response::is_pass_dirty() const { return (resp & PASSDIRTY) != 0; }

inline void ace_response::set_pass_dirty(bool dirty) {
    if(dirty)
        resp |= PASSDIRTY;
    else
        resp &= ~PASSDIRTY;
}

inline bool ace_response::is_shared() const { return (resp & ISSHARED) != 0; }

inline void ace_response::set_shared(bool shared) {
    if(shared)
        resp |= ISSHARED;
    else
        resp &= ~ISSHARED;
}

inline bool ace_response::is_snoop_data_transfer() const { return (resp & DATATRANSFER) != 0; }

inline void ace_response::set_snoop_data_transfer(bool snoop_data) {
    if(snoop_data)
        resp |= DATATRANSFER;
    else
        resp &= ~DATATRANSFER;
}

inline bool ace_response::is_snoop_error() const { return (resp & SNOOPEERROR) != 0; }

inline void ace_response::set_snoop_error(bool err) {
    if(err)
        resp |= SNOOPEERROR;
    else
        resp &= ~SNOOPEERROR;
}

inline bool ace_response::is_snoop_was_unique() const { return (resp & WASUNIQUE) != 0; }

inline void ace_response::set_snoop_was_unique(bool uniq) {
    if(uniq)
        resp |= WASUNIQUE;
    else
        resp &= ~WASUNIQUE;
}

template <typename REQ, typename RESP> axi_extension<REQ, RESP>::axi_extension(const axi_extension<REQ, RESP>* ext) {
    static_cast<common&>(*this) = *ext;
    static_cast<REQ&>(*this) = *ext;
    static_cast<response&>(*this) = *ext;
}

template <typename REQ, typename RESP> void axi_extension<REQ, RESP>::reset() {
    common::reset();
    REQ::reset();
    response::reset();
}

template <typename REQ, typename RESP> void axi_extension<REQ, RESP>::reset(const REQ* control) {
    common::reset();
    static_cast<REQ&>(*this) = *control;
    response::reset();
}

template <typename REQ, typename RESP> inline void axi_extension<REQ, RESP>::add_to_response_array(response& arr) {
    response_arr.push_back(arr);
}

template <typename REQ, typename RESP>
inline const std::vector<response>& axi_extension<REQ, RESP>::get_response_array() const {
    return response_arr;
}

template <typename REQ, typename RESP> inline std::vector<response>& axi_extension<REQ, RESP>::get_response_array() {
    return response_arr;
}

template <typename REQ, typename RESP>
inline void axi_extension<REQ, RESP>::set_response_array_complete(bool complete) {
    response_array_complete = complete;
}

template <typename REQ, typename RESP> inline bool axi_extension<REQ, RESP>::is_response_array_complete() {
    return response_array_complete;
}

inline tlm::tlm_extension_base* axi3_extension::clone() const { return new axi3_extension(this); }

inline void axi3_extension::copy_from(const tlm::tlm_extension_base& bext) {
    auto const* ext = dynamic_cast<const axi3_extension*>(&bext);
    assert(ext);
    static_cast<common&>(*this) = *ext;
    static_cast<axi_extension<axi3>&>(*this) = *ext;
    static_cast<response&>(*this) = *ext;
}

inline tlm::tlm_extension_base* axi4_extension::clone() const { return new axi4_extension(this); }

inline void axi4_extension::copy_from(const tlm::tlm_extension_base& bext) {
    auto const* ext = dynamic_cast<const axi4_extension*>(&bext);
    assert(ext);
    static_cast<common&>(*this) = *ext;
    static_cast<axi_extension<axi4>&>(*this) = *ext;
    static_cast<response&>(*this) = *ext;
}

inline tlm::tlm_extension_base* ace_extension::clone() const { return new ace_extension(this); }

inline void ace_extension::copy_from(const tlm::tlm_extension_base& bext) {
    auto const* ext = dynamic_cast<const ace_extension*>(&bext);
    assert(ext);
    static_cast<common&>(*this) = *ext;
    static_cast<axi_extension<ace, ace_response>&>(*this) = *ext;
    static_cast<response&>(*this) = *ext;
}

} // namespace axi
