/*
 * Copyright 2020, 2021 Arteris IP
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

#ifndef SC_INCLUDE_DYNAMIC_PROCESSES
#define SC_INCLUDE_DYNAMIC_PROCESSES
#endif

#include <array>
#include <chi/chi_tlm.h>
#include <regex>
#include <string>
#include <tlm/scc/scv/tlm_recorder.h>
#include <tlm/scc/scv/tlm_recording_extension.h>
#include <tlm_utils/peq_with_cb_and_phase.h>
#include <unordered_map>
#include <vector>
#include <cci_configuration>
#ifdef HAS_SCV
#include <scv.h>
#else
#include <scv-tr.h>
#ifndef SCVNS
#define SCVNS ::scv_tr::
#endif
#endif

//! SCV components for CHI
namespace chi {
namespace scv {

bool register_extensions();

/*! \brief The TLM2 transaction recorder
 *
 * This module records all TLM transaction to a SCV transaction stream for
 * further viewing and analysis.
 * The handle of the created transaction is storee in an tlm_extension so that
 * another instance of the chi_recorder
 * e.g. further down the path can link to it.
 */
template <typename TYPES = chi::chi_protocol_types>
class chi_trx_recorder : public virtual chi::chi_fw_transport_if<TYPES>,
                         public virtual chi::chi_bw_transport_if<TYPES> {
public:
    template <unsigned int BUSWIDTH = 32, int N = 1, sc_core::sc_port_policy POL = sc_core::SC_ONE_OR_MORE_BOUND>
    using initiator_socket_type = chi::chi_initiator_socket<BUSWIDTH, TYPES, N, POL>;

    template <unsigned int BUSWIDTH = 32, int N = 1, sc_core::sc_port_policy POL = sc_core::SC_ONE_OR_MORE_BOUND>
    using target_socket_type = chi::chi_target_socket<BUSWIDTH, TYPES, N, POL>;

    //! \brief the attribute to selectively enable/disable recording of blocking protocol tx
    cci::cci_param<bool> enableBlTracing;

    //! \brief the attribute to selectively enable/disable recording of non-blocking protocol tx
    cci::cci_param<bool> enableNbTracing;

    //! \brief the attribute to selectively enable/disable timed recording
    cci::cci_param<bool> enableTimedTracing{"enableTimedTracing", true};

    //! \brief the attribute to selectively enable/disable DMI recording
    cci::cci_param<bool> enableDmiTracing{"enableDmiTracing", false};

    //! \brief the attribute to selectively enable/disable transport dbg recording
    cci::cci_param<bool> enableTrDbgTracing{"enableTrDbgTracing", false};

    //! \brief the port where fw accesses are forwarded to
    virtual tlm::tlm_fw_transport_if<TYPES>* get_fw_if() = 0;

    //! \brief the port where bw accesses are forwarded to
    virtual tlm::tlm_bw_transport_if<TYPES>* get_bw_if() = 0;

    /*! \brief The constructor of the component
     *
     * \param name is the SystemC module name of the recorder
     * \param tr_db is a pointer to a transaction recording database. If none is
     * provided the default one is retrieved.
     *        If this database is not initialized (e.g. by not calling
     * scv_tr_db::set_default_db() ) recording is disabled.
     */
    chi_trx_recorder(const char* name, bool recording_enabled = true,
                     SCVNS scv_tr_db* tr_db = SCVNS scv_tr_db::get_default_db())
    : enableBlTracing("enableBlTracing", recording_enabled)
    , enableNbTracing("enableNbTracing", recording_enabled)
    , m_db(tr_db)
    , fixed_basename(name) {
        register_extensions();
    }

    virtual ~chi_trx_recorder() override {
        btx_handle_map.clear();
        nbtx_req_handle_map.clear();
        nbtx_last_req_handle_map.clear();
        nbtx_resp_handle_map.clear();
        nbtx_last_resp_handle_map.clear();
        nbtx_data_handle_map.clear();
        nbtx_last_data_handle_map.clear();
        nbtx_ack_handle_map.clear();

        delete b_streamHandle;
        for(auto* p : b_trHandle)
            delete p; // NOLINT
        delete b_streamHandleTimed;
        for(auto* p : b_trTimedHandle)
            delete p; // NOLINT
        delete nb_streamHandle;
        for(auto* p : nb_trHandle)
            delete p; // NOLINT
        delete nb_streamHandleTimed;
        for(auto* p : nb_trTimedHandle)
            delete p; // NOLINT
        delete dmi_streamHandle;
        delete dmi_trGetHandle;
        delete dmi_trInvalidateHandle;
    }

    // TLM-2.0 interface methods for initiator and target sockets, surrounded with
    // Tx Recording
    /*! \brief The non-blocking forward transport function
     *
     * This type of transaction is forwarded and recorded to a transaction stream
     * named "nb_fw" with current timestamps.
     * \param trans is the generic payload of the transaction
     * \param phase is the current phase of the transaction
     * \param delay is the annotated delay
     * \return the sync state of the transaction
     */
    tlm::tlm_sync_enum nb_transport_fw(typename TYPES::tlm_payload_type& trans, typename TYPES::tlm_phase_type& phase,
                                       sc_core::sc_time& delay) override;
    /*! \brief The non-blocking backward transport function
     *
     * This type of transaction is forwarded and recorded to a transaction stream
     * named "nb_bw" with current timestamps.
     * \param trans is the generic payload of the transaction
     * \param phase is the current phase of the transaction
     * \param delay is the annotated delay
     * \return the sync state of the transaction
     */
    tlm::tlm_sync_enum nb_transport_bw(typename TYPES::tlm_payload_type& trans, typename TYPES::tlm_phase_type& phase,
                                       sc_core::sc_time& delay) override;
    /*! \brief The blocking transport function
     *
     * This type of transaction is forwarded and recorded to a transaction stream
     * named "b_tx" with current timestamps. Additionally a "b_tx_timed"
     * is been created recording the transactions at their annotated delay
     * \param trans is the generic payload of the transaction
     * \param delay is the annotated delay
     */
    void b_transport(typename TYPES::tlm_payload_type& trans, sc_core::sc_time& delay) override;
    /*! \brief The blocking snoop function
     *
     * This type of transaction is forwarded and recorded to a transaction stream
     * named "b_tx" with current timestamps. Additionally a "b_tx_timed"
     * is been created recording the transactions at their annotated delay
     * \param trans is the generic payload of the transaction
     * \param delay is the annotated delay
     */
    void b_snoop(typename TYPES::tlm_payload_type& trans, sc_core::sc_time& delay) override;
    /*! \brief The direct memory interface forward function
     *
     * This type of transaction is just forwarded and not recorded.
     * \param trans is the generic payload of the transaction
     * \param dmi_data is the structure holding the dmi information
     * \return if the dmi structure is valid
     */
    bool get_direct_mem_ptr(typename TYPES::tlm_payload_type& trans, tlm::tlm_dmi& dmi_data) override;
    /*! \brief The direct memory interface backward function
     *
     * This type of transaction is just forwarded and not recorded.
     * \param start_addr is the start address of the memory area being invalid
     * \param end_addr is the end address of the memory area being invalid
     */
    void invalidate_direct_mem_ptr(sc_dt::uint64 start_addr, sc_dt::uint64 end_addr) override;
    /*! \brief The debug transportfunction
     *
     * This type of transaction is just forwarded and not recorded.
     * \param trans is the generic payload of the transaction
     * \return the sync state of the transaction
     */
    unsigned int transport_dbg(typename TYPES::tlm_payload_type& trans) override;
    /*! \brief get the current state of transaction recording
     *
     * \return if true transaction recording is enabled otherwise transaction
     * recording is bypassed
     */
    inline bool isRecordingBlockingTxEnabled() const { return m_db && enableBlTracing.get_value(); }
    /*! \brief get the current state of transaction recording
     *
     * \return if true transaction recording is enabled otherwise transaction
     * recording is bypassed
     */
    inline bool isRecordingNonBlockingTxEnabled() const { return m_db && enableNbTracing.get_value(); }

private:
    void record_nb_timed(typename TYPES::tlm_payload_type& rec_parts, const typename TYPES::tlm_phase_type& phase, sc_core::sc_time const& ts, SCVNS scv_tr_handle parent, bool is_credit);
    //! transaction recording database
    SCVNS scv_tr_db* m_db{nullptr};
    //! blocking transaction recording stream handle
    SCVNS scv_tr_stream* b_streamHandle{nullptr};
    //! transaction generator handle for blocking transactions
    std::array<SCVNS scv_tr_generator<sc_dt::uint64, sc_dt::uint64>*, 3> b_trHandle{{nullptr, nullptr, nullptr}};
    //! timed blocking transaction recording stream handle
    SCVNS scv_tr_stream* b_streamHandleTimed{nullptr};
    //! transaction generator handle for blocking transactions with annotated
    //! delays
    std::array<SCVNS scv_tr_generator<>*, 3> b_trTimedHandle{{nullptr, nullptr, nullptr}};
    std::unordered_map<uint64_t, SCVNS scv_tr_handle> btx_handle_map;

    enum DIR { FW, BW, DATA, ACK, CREDIT, REQ = FW, RESP = BW };
    enum TYPE {
        READ = tlm::TLM_READ_COMMAND,
        WRITE = tlm::TLM_WRITE_COMMAND,
        OTHER = tlm::TLM_IGNORE_COMMAND,
        SNOOP,
        NO_OF_TYPES
    };
    //! non-blocking transaction recording stream handle
    SCVNS scv_tr_stream* nb_streamHandle{nullptr};
    //! non-blocking transaction recording stream handle with timing
    SCVNS scv_tr_stream* nb_streamHandleTimed{nullptr};
    //! transaction generator handle for non-blocking transactions
    std::array<SCVNS scv_tr_generator<std::string, std::string>*, 2> nb_trHandle{{nullptr, nullptr}};
    //! transaction generator handle for non-blocking transactions with annotated delays
    std::array<SCVNS scv_tr_generator<>*, 5> nb_trTimedHandle{{nullptr, nullptr, nullptr, nullptr, nullptr}};
    std::unordered_map<uint64_t, SCVNS scv_tr_handle> nbtx_req_handle_map;
    std::unordered_map<uint64_t, SCVNS scv_tr_handle> nbtx_last_req_handle_map;
    std::unordered_map<uint64_t, SCVNS scv_tr_handle> nbtx_resp_handle_map;
    std::unordered_map<uint64_t, SCVNS scv_tr_handle> nbtx_last_resp_handle_map;
    std::unordered_map<uint64_t, SCVNS scv_tr_handle> nbtx_data_handle_map;
    std::unordered_map<uint64_t, SCVNS scv_tr_handle> nbtx_last_data_handle_map;
    std::unordered_map<uint64_t, SCVNS scv_tr_handle> nbtx_ack_handle_map;
    //! dmi transaction recording stream handle
    SCVNS scv_tr_stream* dmi_streamHandle{nullptr};
    //! transaction generator handle for DMI transactions
    SCVNS scv_tr_generator<>* dmi_trGetHandle{nullptr};
    SCVNS scv_tr_generator<sc_dt::uint64, sc_dt::uint64>* dmi_trInvalidateHandle{nullptr};

protected:
    void initialize_streams() {
        if(isRecordingBlockingTxEnabled()) {
            b_streamHandle = new SCVNS scv_tr_stream((fixed_basename + "_bl").c_str(), "[TLM][chi][b]", m_db);
            b_trHandle[tlm::TLM_READ_COMMAND] = new SCVNS scv_tr_generator<sc_dt::uint64, sc_dt::uint64>(
                "read", *b_streamHandle, "start_delay", "end_delay");
            b_trHandle[tlm::TLM_WRITE_COMMAND] = new SCVNS scv_tr_generator<sc_dt::uint64, sc_dt::uint64>(
                "write", *b_streamHandle, "start_delay", "end_delay");
            b_trHandle[tlm::TLM_IGNORE_COMMAND] = new SCVNS scv_tr_generator<sc_dt::uint64, sc_dt::uint64>(
                "ignore", *b_streamHandle, "start_delay", "end_delay");
            if(enableTimedTracing.get_value()) {
                b_streamHandleTimed =
                    new SCVNS scv_tr_stream((fixed_basename + "_bl_timed").c_str(), "[TLM][chi][b][timed]", m_db);
                b_trTimedHandle[tlm::TLM_READ_COMMAND] = new SCVNS scv_tr_generator<>("read", *b_streamHandleTimed);
                b_trTimedHandle[tlm::TLM_WRITE_COMMAND] = new SCVNS scv_tr_generator<>("write", *b_streamHandleTimed);
                b_trTimedHandle[tlm::TLM_IGNORE_COMMAND] = new SCVNS scv_tr_generator<>("ignore", *b_streamHandleTimed);
            }
        }
        if(isRecordingNonBlockingTxEnabled() && !nb_streamHandle) {
            nb_streamHandle = new SCVNS scv_tr_stream((fixed_basename + "_nb").c_str(), "[TLM][chi][nb]", m_db);
            nb_trHandle[FW] = new SCVNS scv_tr_generator<std::string, std::string>("fw", *nb_streamHandle, "tlm_phase",
                                                                                   "tlm_phase[return_path]");
            nb_trHandle[BW] = new SCVNS scv_tr_generator<std::string, std::string>("bw", *nb_streamHandle, "tlm_phase",
                                                                                   "tlm_phase[return_path]");
            if(enableTimedTracing.get_value()) {
                nb_streamHandleTimed =
                    new SCVNS scv_tr_stream((fixed_basename + "_nb_timed").c_str(), "[TLM][chi][nb][timed]", m_db);
                nb_trTimedHandle[REQ] = new SCVNS scv_tr_generator<>("request", *nb_streamHandleTimed);
                nb_trTimedHandle[RESP] = new SCVNS scv_tr_generator<>("response", *nb_streamHandleTimed);
                nb_trTimedHandle[DATA] = new SCVNS scv_tr_generator<>("data", *nb_streamHandleTimed);
                nb_trTimedHandle[ACK] = new SCVNS scv_tr_generator<>("ack", *nb_streamHandleTimed);
                nb_trTimedHandle[CREDIT] = new SCVNS scv_tr_generator<>("link", *nb_streamHandleTimed);
            }
        }
        if(m_db && enableDmiTracing.get_value() && !dmi_streamHandle) {
            dmi_streamHandle = new SCVNS scv_tr_stream((fixed_basename + "_dmi").c_str(), "[TLM][ace][dmi]", m_db);
            dmi_trGetHandle = new SCVNS scv_tr_generator<>("get", *dmi_streamHandle, "trans", "dmi_data");
            dmi_trInvalidateHandle = new SCVNS scv_tr_generator<sc_dt::uint64, sc_dt::uint64>(
                "invalidate", *dmi_streamHandle, "start_addr", "end_addr");
        }
    }

private:
    const std::string fixed_basename;

    inline std::string phase2string(const tlm::tlm_phase& p) {
        std::stringstream ss;
        ss << p;
        return ss.str();
    }
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// implementations of functions
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

template <typename TYPES>
void chi_trx_recorder<TYPES>::b_transport(typename TYPES::tlm_payload_type& trans, sc_core::sc_time& delay) {
    if(!isRecordingBlockingTxEnabled()) {
        get_fw_if()->b_transport(trans, delay);
        return;
    }
    // Get a handle for the new transaction
    SCVNS scv_tr_handle h = b_trHandle[trans.get_command()]->begin_transaction(delay.value(), sc_core::sc_time_stamp());

    /*************************************************************************
     * do the timed notification
     *************************************************************************/
    SCVNS scv_tr_handle bh;
    if(b_streamHandleTimed) {
        bh = b_trTimedHandle[static_cast<unsigned>(trans.get_command())]->begin_transaction(sc_core::sc_time_stamp() + delay);
        bh.add_relation(rel_str(tlm::scc::scv::PARENT_CHILD), h);
    }

    for(auto& ext : tlm::scc::scv::tlm_extension_recording_registry<TYPES>::inst().get())
        if(ext)
            ext->recordBeginTx(h, trans);
    tlm::scc::scv::tlm_recording_extension* preExt = nullptr;

    trans.get_extension(preExt);
    if(preExt == nullptr) { // we are the first recording this transaction
        preExt = new tlm::scc::scv::tlm_recording_extension(h, this);
        trans.set_extension(preExt);
    } else {
        h.add_relation(tlm::scc::scv::rel_str(tlm::scc::scv::PREDECESSOR_SUCCESSOR), preExt->txHandle);
    }
    SCVNS scv_tr_handle preTx(preExt->txHandle);
    preExt->txHandle = h;
    get_fw_if()->b_transport(trans, delay);
    trans.get_extension(preExt);
    if(preExt->get_creator() == this) {
        // clean-up the extension if this is the original creator
        delete trans.set_extension(static_cast<tlm::scc::scv::tlm_recording_extension*>(nullptr));
    } else {
        preExt->txHandle = preTx;
    }
    tlm::scc::scv::record(h, trans);
    for(auto& ext : tlm::scc::scv::tlm_extension_recording_registry<TYPES>::inst().get())
        if(ext)
            ext->recordEndTx(h, trans);
    // End the transaction
    b_trHandle[trans.get_command()]->end_transaction(h, delay.value(), sc_core::sc_time_stamp());
    // and now the stuff for the timed tx
    if(b_streamHandleTimed) {
        tlm::scc::scv::record(bh, trans);
        b_trTimedHandle[static_cast<unsigned>(trans.get_command())]->end_transaction(bh, sc_core::sc_time_stamp()+ delay);
    }
}

template <typename TYPES>
void chi_trx_recorder<TYPES>::b_snoop(typename TYPES::tlm_payload_type& trans, sc_core::sc_time& delay) {
    if(!b_streamHandleTimed) {
        get_fw_if()->b_transport(trans, delay);
        return;
    }
    // Get a handle for the new transaction
    SCVNS scv_tr_handle h = b_trHandle[trans.get_command()]->begin_transaction(delay.value(), sc_core::sc_time_stamp());
    /*************************************************************************
     * do the timed notification
     *************************************************************************/
    SCVNS scv_tr_handle bh;
    if(b_streamHandleTimed) {
        bh = b_trTimedHandle[static_cast<unsigned>(trans.get_command())]->begin_transaction(sc_core::sc_time_stamp() + delay);
        bh.add_relation(rel_str(tlm::scc::scv::PARENT_CHILD), h);
    }

    for(auto& ext : tlm::scc::scv::tlm_extension_recording_registry<TYPES>::inst().get())
        if(ext)
            ext->recordBeginTx(h, trans);
    tlm::scc::scv::tlm_recording_extension* preExt = NULL;

    trans.get_extension(preExt);
    if(preExt == NULL) { // we are the first recording this transaction
        preExt = new tlm::scc::scv::tlm_recording_extension(h, this);
        trans.set_extension(preExt);
    } else {
        h.add_relation(tlm::scc::scv::rel_str(tlm::scc::scv::PREDECESSOR_SUCCESSOR), preExt->txHandle);
    }
    SCVNS scv_tr_handle preTx(preExt->txHandle);
    preExt->txHandle = h;
    get_fw_if()->b_transport(trans, delay);
    trans.get_extension(preExt);
    if(preExt->get_creator() == this) {
        // clean-up the extension if this is the original creator
        delete trans.set_extension(static_cast<tlm::scc::scv::tlm_recording_extension*>(nullptr));
    } else {
        preExt->txHandle = preTx;
    }

    tlm::scc::scv::record(h, trans);
    for(auto& ext : tlm::scc::scv::tlm_extension_recording_registry<TYPES>::inst().get())
        if(ext)
            ext->recordEndTx(h, trans);
    // End the transaction
    b_trHandle[trans.get_command()]->end_transaction(h, delay.value());
    // and now the stuff for the timed tx
    if(b_streamHandleTimed) {
        tlm::scc::scv::record(bh, trans);
        b_trTimedHandle[static_cast<unsigned>(trans.get_command())]->end_transaction(bh, sc_core::sc_time_stamp()+ delay);
    }
}

template <typename TYPES>
tlm::tlm_sync_enum chi_trx_recorder<TYPES>::nb_transport_fw(typename TYPES::tlm_payload_type& trans,
                                                            typename TYPES::tlm_phase_type& phase,
                                                            sc_core::sc_time& delay) {
    if(!nb_trHandle[FW])
        return get_fw_if()->nb_transport_fw(trans, phase, delay);
    /*************************************************************************
     * prepare recording
     *************************************************************************/
    // Get a handle for the new transaction
    bool is_snoop = (trans.template get_extension<chi::chi_snp_extension>() != nullptr);
    SCVNS scv_tr_handle h = nb_trHandle[FW]->begin_transaction(phase2string(phase));
    tlm::scc::scv::tlm_recording_extension* preExt = nullptr;
    trans.get_extension(preExt);
    if(phase == tlm::BEGIN_REQ && preExt == nullptr) { // we are the first recording this transaction
        preExt = new tlm::scc::scv::tlm_recording_extension(h, this);
        trans.set_extension(preExt);
    } else if(preExt != nullptr) {
        // link handle if we have a predecessor
        h.add_relation(tlm::scc::scv::rel_str(tlm::scc::scv::PREDECESSOR_SUCCESSOR), preExt->txHandle);
    } else {
        sc_assert(preExt != nullptr && "ERROR on forward path in phase other than tlm::BEGIN_REQ");
    }
    // update the extension
    preExt->txHandle = h;
    h.record_attribute("delay", delay.to_string());
    for(auto& ext : tlm::scc::scv::tlm_extension_recording_registry<TYPES>::inst().get())
        if(ext)
            ext->recordBeginTx(h, trans);
    /*************************************************************************
     * do the timed notification
     *************************************************************************/
    chi::chi_credit_extension* chi_ext{nullptr} ;
    trans.get_extension(chi_ext);
    if(nb_streamHandleTimed) {
        record_nb_timed(trans, phase, delay, h, (phase == tlm::BEGIN_REQ && chi_ext != nullptr));
    }
    /*************************************************************************
     * do the access
     *************************************************************************/
    tlm::tlm_sync_enum status = get_fw_if()->nb_transport_fw(trans, phase, delay);
    /*************************************************************************
     * handle recording
     *************************************************************************/
    tlm::scc::scv::record(h, status);
    h.record_attribute("delay[return_path]", delay.to_string());
    tlm::scc::scv::record(h, trans);
    for(auto& ext : tlm::scc::scv::tlm_extension_recording_registry<TYPES>::inst().get())
        if(ext)
            ext->recordEndTx(h, trans);
    // get the extension and free the memory if it was mine
    if(status == tlm::TLM_COMPLETED) {
        // the transaction is finished
        trans.get_extension(preExt);
        if(preExt && preExt->get_creator() == this) {
            // clean-up the extension if this is the original creator
            delete trans.set_extension(static_cast<tlm::scc::scv::tlm_recording_extension*>(nullptr));
        }
        /*************************************************************************
         * do the timed notification if req. finished here
         *************************************************************************/
        if(nb_streamHandleTimed) {
             record_nb_timed(trans, (status == tlm::TLM_COMPLETED && phase == tlm::BEGIN_REQ) ? tlm::END_RESP : phase,delay, h, (phase == tlm::BEGIN_REQ && chi_ext != nullptr));
        }
    } else if(nb_streamHandleTimed && status == tlm::TLM_UPDATED) {
        record_nb_timed(trans, phase, delay, h, (phase == tlm::BEGIN_REQ && chi_ext != nullptr));
    }
    // End the transaction
    nb_trHandle[FW]->end_transaction(h, phase2string(phase));
    return status;
}

template <typename TYPES>
tlm::tlm_sync_enum chi_trx_recorder<TYPES>::nb_transport_bw(typename TYPES::tlm_payload_type& trans,
                                                            typename TYPES::tlm_phase_type& phase,
                                                            sc_core::sc_time& delay) {
    if(!nb_trHandle[BW])
        return get_bw_if()->nb_transport_bw(trans, phase, delay);
    /*************************************************************************
     * prepare recording
     *************************************************************************/
    // Get a handle for the new transaction
    bool is_snoop = (trans.template get_extension<chi::chi_snp_extension>() != nullptr);
    SCVNS scv_tr_handle h = nb_trHandle[BW]->begin_transaction(phase2string(phase));
    tlm::scc::scv::tlm_recording_extension* preExt = nullptr;
    trans.get_extension(preExt);
    if(phase == tlm::BEGIN_REQ && preExt == nullptr) { // we are the first recording this transaction
        preExt = new tlm::scc::scv::tlm_recording_extension(h, this);
        trans.set_extension(preExt);
    } else if(preExt != nullptr) {
        // link handle if we have a predecessor
        h.add_relation(tlm::scc::scv::rel_str(tlm::scc::scv::PREDECESSOR_SUCCESSOR), preExt->txHandle);
    } else {
        sc_assert(preExt != nullptr && "ERROR on backward path in phase other than tlm::BEGIN_REQ");
    }
    // and set the extension handle to this transaction
    preExt->txHandle = h;
    h.record_attribute("delay", delay.to_string());
    for(auto& ext : tlm::scc::scv::tlm_extension_recording_registry<TYPES>::inst().get())
        if(ext)
            ext->recordBeginTx(h, trans);
    /*************************************************************************
     * do the timed notification
     *************************************************************************/
    chi::chi_credit_extension* chi_ext ;
    trans.get_extension(chi_ext);
    if(nb_streamHandleTimed) {
        record_nb_timed(trans, phase, delay, h, (phase == tlm::BEGIN_REQ && chi_ext != nullptr));
    }
    /*************************************************************************
     * do the access
     *************************************************************************/
    tlm::tlm_sync_enum status = get_bw_if()->nb_transport_bw(trans, phase, delay);
    /*************************************************************************
     * handle recording
     *************************************************************************/
    tlm::scc::scv::record(h, status);
    h.record_attribute("delay[return_path]", delay.to_string());
    tlm::scc::scv::record(h, trans);
    for(auto& ext : tlm::scc::scv::tlm_extension_recording_registry<TYPES>::inst().get())
        if(ext)
            ext->recordEndTx(h, trans);
    // get the extension and free the memory if it was mine
    if(status == tlm::TLM_COMPLETED) {
        // the transaction is finished
        trans.get_extension(preExt);
        if(preExt->get_creator() == this) {
            // clean-up the extension if this is the original creator
            delete trans.set_extension(static_cast<tlm::scc::scv::tlm_recording_extension*>(nullptr));
        }
        /*************************************************************************
         * do the timed notification if req. finished here
         *************************************************************************/
        if(nb_streamHandleTimed) {
            record_nb_timed(trans, (status == tlm::TLM_COMPLETED && phase == tlm::BEGIN_REQ) ? tlm::END_RESP : phase, delay, h, (phase == tlm::BEGIN_REQ && chi_ext != nullptr));
        }
    } else if(nb_streamHandleTimed && status == tlm::TLM_UPDATED) {
        record_nb_timed(trans, phase, delay, h, (phase == tlm::BEGIN_REQ && chi_ext != nullptr));

    }
    // End the transaction
    nb_trHandle[BW]->end_transaction(h, phase2string(phase));
    return status;
}

template <typename TYPES>
void chi_trx_recorder<TYPES>::record_nb_timed(typename TYPES::tlm_payload_type& trans, const typename TYPES::tlm_phase_type& phase, sc_core::sc_time const& delay, SCVNS scv_tr_handle parent, bool is_credit) {
    SCVNS scv_tr_handle h;
    std::unordered_map<uint64_t, SCVNS scv_tr_handle>::iterator it;
    auto t = delay+sc_core::sc_time_stamp();
    auto id = reinterpret_cast<uintptr_t>(&trans);
    // Now process outstanding recordings
    if(is_credit) {
        if(phase == tlm::BEGIN_REQ) {
            h = nb_trTimedHandle[CREDIT]->begin_transaction(t);
            h.add_relation(tlm::scc::scv::rel_str(tlm::scc::scv::PARENT_CHILD), parent);
            nbtx_req_handle_map[id] = h;
        } else {
            it = nbtx_req_handle_map.find(id);
            if(it != nbtx_req_handle_map.end()) {
                it->second.end_transaction(t);
                nbtx_req_handle_map.erase(it);
            }
        }
    } else if(phase == tlm::BEGIN_REQ) {
        h = nb_trTimedHandle[REQ]->begin_transaction(t);
        tlm::scc::scv::record(h, trans);
        h.add_relation(tlm::scc::scv::rel_str(tlm::scc::scv::PARENT_CHILD), parent);
        nbtx_req_handle_map[id] = h;
        nbtx_last_req_handle_map[id] = h;
    } else if(phase == tlm::END_REQ) {
        it = nbtx_req_handle_map.find(id);
        if(it != nbtx_req_handle_map.end()) {
            it->second.end_transaction(t);
            nbtx_req_handle_map.erase(it);
        }
    } else if(phase == tlm::BEGIN_RESP) {
        it = nbtx_req_handle_map.find(id);
        if(it != nbtx_req_handle_map.end()) {
            it->second.end_transaction(t);
            nbtx_req_handle_map.erase(it);
        }
        h = nb_trTimedHandle[RESP]->begin_transaction();
        tlm::scc::scv::record(h, trans);
        h.add_relation(tlm::scc::scv::rel_str(tlm::scc::scv::PARENT_CHILD), parent);
        nbtx_resp_handle_map[id] = h;
        it = nbtx_last_req_handle_map.find(id);
        if(it != nbtx_last_req_handle_map.end()) {
            SCVNS scv_tr_handle pred = it->second;
            nbtx_last_req_handle_map.erase(it);
            h.add_relation(tlm::scc::scv::rel_str(tlm::scc::scv::PREDECESSOR_SUCCESSOR), pred);
        } else {
            it = nbtx_last_resp_handle_map.find(id);
            if(it != nbtx_last_resp_handle_map.end()) {
                SCVNS scv_tr_handle pred = it->second;
                nbtx_last_resp_handle_map.erase(it);
                h.add_relation(tlm::scc::scv::rel_str(tlm::scc::scv::PREDECESSOR_SUCCESSOR), pred);
            }
        }
    } else if(phase == tlm::END_RESP) {
        it = nbtx_resp_handle_map.find(id);
        if(it != nbtx_resp_handle_map.end()) {
            h = it->second;
            nbtx_resp_handle_map.erase(it);
            h.end_transaction(t);
        } else {
            it = nbtx_req_handle_map.find(id);
            if(it != nbtx_req_handle_map.end()) {
                h = it->second;
                nbtx_req_handle_map.erase(it);
                h.end_transaction(t);
            }
        }
    } else if(phase == chi::BEGIN_DATA || phase == chi::BEGIN_PARTIAL_DATA) {
        it = nbtx_req_handle_map.find(id);
        if(it != nbtx_req_handle_map.end()) {
            h = it->second;
            h.end_transaction(t);
            nbtx_last_req_handle_map[id] = h;
        }
        h = nb_trTimedHandle[DATA]->begin_transaction();
        tlm::scc::scv::record(h, trans);
        h.add_relation(tlm::scc::scv::rel_str(tlm::scc::scv::PARENT_CHILD), parent);
        nbtx_data_handle_map[id] = h;
        it = nbtx_last_data_handle_map.find(id);
        if(it != nbtx_last_data_handle_map.end()) {
            SCVNS scv_tr_handle pred = it->second;
            nbtx_last_data_handle_map.erase(it);
            h.add_relation(tlm::scc::scv::rel_str(tlm::scc::scv::PREDECESSOR_SUCCESSOR), pred);
        }
    } else if(phase == chi::END_DATA || phase == chi::END_PARTIAL_DATA) {
        it = nbtx_data_handle_map.find(id);
        if(it != nbtx_data_handle_map.end()) {
            h = it->second;
            nbtx_data_handle_map.erase(it);
            h.end_transaction(t);
        }
    } else if(phase == chi::ACK) {
        it = nbtx_ack_handle_map.find(id);
        if(it == nbtx_ack_handle_map.end()) {
            auto h = nb_trTimedHandle[ACK]->begin_transaction(t);
            nbtx_ack_handle_map[id] = h;
            h.add_relation(tlm::scc::scv::rel_str(tlm::scc::scv::PARENT_CHILD), parent);
        } else {
            it->second.end_transaction(t);
            nbtx_ack_handle_map.erase(it);
        }
    } else
        sc_assert(!"phase not supported!");
}

template <typename TYPES>
bool chi_trx_recorder<TYPES>::get_direct_mem_ptr(typename TYPES::tlm_payload_type& trans, tlm::tlm_dmi& dmi_data) {
    if(!(m_db && enableDmiTracing.get_value()))
        return get_fw_if()->get_direct_mem_ptr(trans, dmi_data);
    else if(!dmi_streamHandle)
        initialize_streams();
    SCVNS scv_tr_handle h = dmi_trGetHandle->begin_transaction();
    bool status = get_fw_if()->get_direct_mem_ptr(trans, dmi_data);
    tlm::scc::scv::record(h, trans);
    tlm::scc::scv::record(h, dmi_data);
    h.end_transaction();
    return status;
}
/*! \brief The direct memory interface backward function
 *
 * This type of transaction is just forwarded and not recorded.
 * \param start_addr is the start address of the memory area being invalid
 * \param end_addr is the end address of the memory area being invalid
 */
template <typename TYPES>
void chi_trx_recorder<TYPES>::invalidate_direct_mem_ptr(sc_dt::uint64 start_addr, sc_dt::uint64 end_addr) {
    if(!(m_db && enableDmiTracing.get_value())) {
        get_bw_if()->invalidate_direct_mem_ptr(start_addr, end_addr);
        return;
    } else if(!dmi_streamHandle)
        initialize_streams();
    SCVNS scv_tr_handle h = dmi_trInvalidateHandle->begin_transaction(start_addr);
    get_bw_if()->invalidate_direct_mem_ptr(start_addr, end_addr);
    dmi_trInvalidateHandle->end_transaction(h, end_addr);
    return;
}
/*! \brief The debug transport function
 *
 * This type of transaction is just forwarded and not recorded.
 * \param trans is the generic payload of the transaction
 * \return the sync state of the transaction
 */
template <typename TYPES> unsigned int chi_trx_recorder<TYPES>::transport_dbg(typename TYPES::tlm_payload_type& trans) {
    unsigned int count = get_fw_if()->transport_dbg(trans);
    return count;
}
} // namespace scv
} // namespace chi
