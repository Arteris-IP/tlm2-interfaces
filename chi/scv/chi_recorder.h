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
#include <cci_configuration>
#include <chi/chi_tlm.h>
#include <regex>
#include <string>
#include <tlm/scc/scv/tlm_recorder.h>
#include <tlm/scc/scv/tlm_recording_extension.h>
#include <tlm_utils/peq_with_cb_and_phase.h>
#include <unordered_map>
#include <vector>
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
namespace {
inline bool has_credit(chi::chi_protocol_types::tlm_payload_type& trans) {
    return trans.get_extension<chi::chi_credit_extension<chi::credit_type_e::REQ>>() ||
           trans.get_extension<chi::chi_credit_extension<chi::credit_type_e::RESP>>() ||
           trans.get_extension<chi::chi_credit_extension<chi::credit_type_e::DATA>>();
}
} // namespace

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
class chi_trx_recorder : public virtual chi::chi_fw_transport_if<TYPES>, public virtual chi::chi_bw_transport_if<TYPES> {
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
    chi_trx_recorder(const char* name, bool recording_enabled = true, SCVNS scv_tr_db* tr_db = SCVNS scv_tr_db::get_default_db())
    : enableBlTracing("enableBlTracing", recording_enabled)
    , enableNbTracing("enableNbTracing", recording_enabled)
    , m_db(tr_db)
    , fixed_basename(name) {
        register_extensions();
    }

    virtual ~chi_trx_recorder() override {
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
    void record_nb_timed(typename TYPES::tlm_payload_type& rec_parts, const typename TYPES::tlm_phase_type& begin_phase,
            const typename TYPES::tlm_phase_type& end_phase, sc_core::sc_time its, sc_core::sc_time ets,SCVNS scv_tr_handle& parent, bool is_bw);
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

    enum DIR { FW, BW, REQ=FW, WDAT=BW, SRSP, CRSP, RDAT, SNP, NONE };
    enum TYPE { READ = tlm::TLM_READ_COMMAND, WRITE = tlm::TLM_WRITE_COMMAND, OTHER = tlm::TLM_IGNORE_COMMAND, SNOOP, NO_OF_TYPES };
    //! non-blocking transaction recording stream handle
    SCVNS scv_tr_stream* nb_streamHandle{nullptr};
    //! non-blocking transaction recording stream handle with timing
    SCVNS scv_tr_stream* nb_streamHandleTimed{nullptr};
    //! transaction generator handle for non-blocking transactions
    std::array<SCVNS scv_tr_generator<std::string, std::string>*, 2> nb_trHandle{{nullptr, nullptr}};
    //! transaction generator handle for non-blocking transactions with annotated delays
    std::array<SCVNS scv_tr_generator<std::string>*, 6> nb_trTimedHandle{{nullptr, nullptr, nullptr, nullptr, nullptr, nullptr}};
    std::array<SCVNS scv_tr_generator<>*, 6> nb_trTimedCrdHandle{{nullptr, nullptr, nullptr, nullptr, nullptr, nullptr}};
    std::array<std::unordered_map<void*, SCVNS scv_tr_handle>, 6> open_nb_tx_handle_map;
    std::unordered_map<void*, SCVNS scv_tr_handle> nb_tx_handle_map;
    //! dmi transaction recording stream handle
    SCVNS scv_tr_stream* dmi_streamHandle{nullptr};
    //! transaction generator handle for DMI transactions
    SCVNS scv_tr_generator<>* dmi_trGetHandle{nullptr};
    SCVNS scv_tr_generator<sc_dt::uint64, sc_dt::uint64>* dmi_trInvalidateHandle{nullptr};

protected:
    void initialize_streams() {
        if(isRecordingBlockingTxEnabled()) {
            b_streamHandle = new SCVNS scv_tr_stream((fixed_basename + "_bl").c_str(), "[TLM][chi][b]", m_db);
            b_trHandle[tlm::TLM_READ_COMMAND] =
                new SCVNS scv_tr_generator<sc_dt::uint64, sc_dt::uint64>("read", *b_streamHandle, "start_delay", "end_delay");
            b_trHandle[tlm::TLM_WRITE_COMMAND] =
                new SCVNS scv_tr_generator<sc_dt::uint64, sc_dt::uint64>("write", *b_streamHandle, "start_delay", "end_delay");
            b_trHandle[tlm::TLM_IGNORE_COMMAND] =
                new SCVNS scv_tr_generator<sc_dt::uint64, sc_dt::uint64>("ignore", *b_streamHandle, "start_delay", "end_delay");
            if(enableTimedTracing.get_value()) {
                b_streamHandleTimed = new SCVNS scv_tr_stream((fixed_basename + "_bl_timed").c_str(), "[TLM][chi][b][timed]", m_db);
                b_trTimedHandle[tlm::TLM_READ_COMMAND] = new SCVNS scv_tr_generator<>("read", *b_streamHandleTimed);
                b_trTimedHandle[tlm::TLM_WRITE_COMMAND] = new SCVNS scv_tr_generator<>("write", *b_streamHandleTimed);
                b_trTimedHandle[tlm::TLM_IGNORE_COMMAND] = new SCVNS scv_tr_generator<>("ignore", *b_streamHandleTimed);
            }
        }
        if(isRecordingNonBlockingTxEnabled() && !nb_streamHandle) {
            nb_streamHandle = new SCVNS scv_tr_stream((fixed_basename + "_nb").c_str(), "[TLM][chi][nb]", m_db);
            nb_trHandle[FW] =
                new SCVNS scv_tr_generator<std::string, std::string>("fw", *nb_streamHandle, "tlm_phase", "tlm_phase[return_path]");
            nb_trHandle[BW] =
                new SCVNS scv_tr_generator<std::string, std::string>("bw", *nb_streamHandle, "tlm_phase", "tlm_phase[return_path]");
            if(enableTimedTracing.get_value()) {
                nb_streamHandleTimed = new SCVNS scv_tr_stream((fixed_basename + "_nb_timed").c_str(), "[TLM][chi][nb][timed]", m_db);
                nb_trTimedHandle[REQ] = new SCVNS scv_tr_generator<std::string>("REQ", *nb_streamHandleTimed, "tlm_phase");
                nb_trTimedHandle[WDAT] = new SCVNS scv_tr_generator<std::string>("WDAT", *nb_streamHandleTimed, "tlm_phase");
                nb_trTimedHandle[SRSP] = new SCVNS scv_tr_generator<std::string>("SRSP", *nb_streamHandleTimed, "tlm_phase");
                nb_trTimedHandle[CRSP] = new SCVNS scv_tr_generator<std::string>("CRSP", *nb_streamHandleTimed, "tlm_phase");
                nb_trTimedHandle[RDAT] = new SCVNS scv_tr_generator<std::string>("RDAT", *nb_streamHandleTimed, "tlm_phase");
                nb_trTimedHandle[SNP] = new SCVNS scv_tr_generator<std::string>("SNP", *nb_streamHandleTimed, "tlm_phase");
                nb_trTimedCrdHandle[REQ] = new SCVNS scv_tr_generator<>("REQ_CRD", *nb_streamHandleTimed);
                nb_trTimedCrdHandle[WDAT] = new SCVNS scv_tr_generator<>("WDAT_CRD", *nb_streamHandleTimed);
                nb_trTimedCrdHandle[SRSP] = new SCVNS scv_tr_generator<>("SRSP_CRD", *nb_streamHandleTimed);
                nb_trTimedCrdHandle[CRSP] = new SCVNS scv_tr_generator<>("CRSP_CRD", *nb_streamHandleTimed);
                nb_trTimedCrdHandle[RDAT] = new SCVNS scv_tr_generator<>("RDAT_CRD", *nb_streamHandleTimed);
                nb_trTimedCrdHandle[SNP] = new SCVNS scv_tr_generator<>("SNP_CRD", *nb_streamHandleTimed);
            }
        }
        if(m_db && enableDmiTracing.get_value() && !dmi_streamHandle) {
            dmi_streamHandle = new SCVNS scv_tr_stream((fixed_basename + "_dmi").c_str(), "[TLM][ace][dmi]", m_db);
            dmi_trGetHandle = new SCVNS scv_tr_generator<>("get", *dmi_streamHandle, "trans", "dmi_data");
            dmi_trInvalidateHandle =
                new SCVNS scv_tr_generator<sc_dt::uint64, sc_dt::uint64>("invalidate", *dmi_streamHandle, "start_addr", "end_addr");
        }
    }

    void release_handles() {
        nb_tx_handle_map.clear();
        for(auto& m:open_nb_tx_handle_map)
            m.clear();
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

template <typename TYPES> void chi_trx_recorder<TYPES>::b_transport(typename TYPES::tlm_payload_type& trans, sc_core::sc_time& delay) {
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

    for(auto& ext : tlm::scc::scv::tlm_extension_recording_registry<TYPES>::get())
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
    for(auto& ext : tlm::scc::scv::tlm_extension_recording_registry<TYPES>::get())
        if(ext)
            ext->recordEndTx(h, trans);
    // End the transaction
    b_trHandle[trans.get_command()]->end_transaction(h, delay.value(), sc_core::sc_time_stamp());
    // and now the stuff for the timed tx
    if(b_streamHandleTimed) {
        tlm::scc::scv::record(bh, trans);
        b_trTimedHandle[static_cast<unsigned>(trans.get_command())]->end_transaction(bh, sc_core::sc_time_stamp() + delay);
    }
}

template <typename TYPES> void chi_trx_recorder<TYPES>::b_snoop(typename TYPES::tlm_payload_type& trans, sc_core::sc_time& delay) {
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

    for(auto& ext : tlm::scc::scv::tlm_extension_recording_registry<TYPES>::get())
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
    for(auto& ext : tlm::scc::scv::tlm_extension_recording_registry<TYPES>::get())
        if(ext)
            ext->recordEndTx(h, trans);
    // End the transaction
    b_trHandle[trans.get_command()]->end_transaction(h, delay.value());
    // and now the stuff for the timed tx
    if(b_streamHandleTimed) {
        tlm::scc::scv::record(bh, trans);
        b_trTimedHandle[static_cast<unsigned>(trans.get_command())]->end_transaction(bh, sc_core::sc_time_stamp() + delay);
    }
}

template <typename TYPES>
tlm::tlm_sync_enum chi_trx_recorder<TYPES>::nb_transport_fw(typename TYPES::tlm_payload_type& trans, typename TYPES::tlm_phase_type& phase,
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
    for(auto& ext : tlm::scc::scv::tlm_extension_recording_registry<TYPES>::get())
        if(ext)
            ext->recordBeginTx(h, trans);
    /*************************************************************************
     * record the time of notification
     *************************************************************************/
    auto initial_delay = delay;
    auto initial_phase = phase;
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
    for(auto& ext : tlm::scc::scv::tlm_extension_recording_registry<TYPES>::get())
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
    }
    /*************************************************************************
     * do the timed notification
     *************************************************************************/
    if(nb_streamHandleTimed) {
        record_nb_timed(trans, initial_phase, phase, initial_delay, delay, h, false);
    }
    // End the transaction
    nb_trHandle[FW]->end_transaction(h, phase2string(phase));
    return status;
}

template <typename TYPES>
tlm::tlm_sync_enum chi_trx_recorder<TYPES>::nb_transport_bw(typename TYPES::tlm_payload_type& trans, typename TYPES::tlm_phase_type& phase,
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
    for(auto& ext : tlm::scc::scv::tlm_extension_recording_registry<TYPES>::get())
        if(ext)
            ext->recordBeginTx(h, trans);
    /*************************************************************************
     * record the time of notification
     *************************************************************************/
    auto initial_delay = delay;
    auto initial_phase = phase;
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
    for(auto& ext : tlm::scc::scv::tlm_extension_recording_registry<TYPES>::get())
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
    }
    /*************************************************************************
     * do the timed notification if req. finished here
     *************************************************************************/
    if(nb_streamHandleTimed) {
        record_nb_timed(trans, initial_phase, phase, initial_delay, delay, h, true);
    }
     // End the transaction
    nb_trHandle[BW]->end_transaction(h, phase2string(phase));
    return status;
}

template <typename TYPES>
void chi_trx_recorder<TYPES>::record_nb_timed(typename TYPES::tlm_payload_type& trans, const typename TYPES::tlm_phase_type& begin_phase,
        const typename TYPES::tlm_phase_type& end_phase, sc_core::sc_time begin_delay, sc_core::sc_time end_delay,  SCVNS scv_tr_handle& parent, bool is_bw) {
    SCVNS scv_tr_handle h;
    auto t = begin_delay + sc_core::sc_time_stamp();
    auto id = reinterpret_cast<uintptr_t>(&trans);
    auto crd_fiber = NONE;
    auto fiber = FW;
    chi::chi_credit_extension<chi::credit_type_e::REQ>* req_crd_ext;
    trans.get_extension(req_crd_ext);
    chi::chi_credit_extension<chi::credit_type_e::RESP>* resp_crd_ext;
    trans.get_extension(resp_crd_ext);
    chi::chi_credit_extension<chi::credit_type_e::DATA>* dat_crd_ext;
    trans.get_extension(dat_crd_ext);
    auto credits = 0u;
    if(begin_phase == tlm::BEGIN_REQ && end_phase == tlm::END_RESP) { // a dedicated CREDIT transaction
        if(is_bw){
            if(req_crd_ext){
                crd_fiber = REQ;
                credits=req_crd_ext->count;
            } else if(resp_crd_ext){
                crd_fiber = SRSP;
                credits=resp_crd_ext->count;
            } else if(dat_crd_ext){
                crd_fiber = WDAT;
                credits=dat_crd_ext->count;
            }
        } else {
            if(req_crd_ext){
                crd_fiber = SNP;
                credits=req_crd_ext->count;
            } else if(resp_crd_ext){
                crd_fiber = CRSP;
                credits=resp_crd_ext->count;
            } else if(dat_crd_ext){
                crd_fiber = RDAT;
                credits=dat_crd_ext->count;
            }
        }
        h = nb_trTimedCrdHandle[crd_fiber]->begin_transaction(sc_core::sc_time_stamp() + begin_delay);
        h.add_relation(rel_str(tlm::scc::scv::PARENT_CHILD), parent);
        h.record_attribute("credits", credits);
        nb_trTimedCrdHandle[crd_fiber]->end_transaction(h, sc_core::sc_time_stamp() + end_delay);
        return;
    }
    bool accepted = begin_phase == end_phase;
    if(is_bw) {
        if(begin_phase == tlm::BEGIN_REQ) {
            fiber = SNP;
            if(req_crd_ext) {
                crd_fiber = SNP;
                credits=req_crd_ext->count;
           }
        } else if(begin_phase == tlm::END_REQ) {
            auto it = open_nb_tx_handle_map[REQ].find(&trans);
            if(it != open_nb_tx_handle_map[REQ].end())
                nb_trTimedHandle[fiber]->end_transaction(it->second, sc_core::sc_time_stamp() + begin_delay);
            open_nb_tx_handle_map[REQ].erase(it);
            return;
        } else if(begin_phase == tlm::BEGIN_RESP) {
            fiber = CRSP;
            if(resp_crd_ext) {
                crd_fiber =CRSP;
                credits=resp_crd_ext->count;
           }
        } else if(begin_phase == tlm::END_RESP) {
            auto it = open_nb_tx_handle_map[SRSP].find(&trans);
            if(it != open_nb_tx_handle_map[SRSP].end())
                nb_trTimedHandle[fiber]->end_transaction(it->second, sc_core::sc_time_stamp() + begin_delay);
            open_nb_tx_handle_map[SRSP].erase(it);
            return;
        } else if(begin_phase == chi::BEGIN_DATA || begin_phase==chi::BEGIN_PARTIAL_DATA) {
            fiber = RDAT;
            if(dat_crd_ext) {
                crd_fiber = RDAT;
                credits=dat_crd_ext->count;
            }
        } else if(begin_phase == chi::END_DATA || begin_phase==chi::END_PARTIAL_DATA) {
            auto it = open_nb_tx_handle_map[WDAT].find(&trans);
            if(it != open_nb_tx_handle_map[WDAT].end())
                nb_trTimedHandle[fiber]->end_transaction(it->second, sc_core::sc_time_stamp() + begin_delay);
            open_nb_tx_handle_map[WDAT].erase(it);
            return;
        } else if(begin_phase == ACK) {
            fiber = CRSP;
            if(dat_crd_ext) {
                crd_fiber = CRSP;
                credits=dat_crd_ext->count;
            }
        }
    } else {
        if(begin_phase == tlm::BEGIN_REQ) {
            fiber = REQ;
            if(req_crd_ext) {
                crd_fiber = REQ;
                credits=req_crd_ext->count;
            }
        } else if(begin_phase == tlm::END_REQ) {
            auto it = open_nb_tx_handle_map[SNP].find(&trans);
            if(it != open_nb_tx_handle_map[SNP].end())
                nb_trTimedHandle[fiber]->end_transaction(it->second, sc_core::sc_time_stamp() + begin_delay);
            open_nb_tx_handle_map[SNP].erase(it);
            return;
      } else if(begin_phase == tlm::BEGIN_RESP) {
            fiber = SRSP;
            if(resp_crd_ext) {
                crd_fiber =SRSP;
                credits=resp_crd_ext->count;
           }
       } else if(begin_phase == tlm::END_RESP) {
           auto it = open_nb_tx_handle_map[CRSP].find(&trans);
           if(it != open_nb_tx_handle_map[CRSP].end()){
               nb_trTimedHandle[fiber]->end_transaction(it->second, sc_core::sc_time_stamp() + begin_delay);
               open_nb_tx_handle_map[CRSP].erase(it);
           }
           return;
       } else if(begin_phase == chi::BEGIN_DATA || begin_phase==chi::BEGIN_PARTIAL_DATA) {
            fiber = WDAT;
            if(dat_crd_ext) {
                crd_fiber = WDAT;
                credits=dat_crd_ext->count;
            }
       } else if(begin_phase == chi::END_DATA || begin_phase==chi::END_PARTIAL_DATA) {
           auto it = open_nb_tx_handle_map[RDAT].find(&trans);
           if(it != open_nb_tx_handle_map[RDAT].end())
               nb_trTimedHandle[fiber]->end_transaction(it->second, sc_core::sc_time_stamp() + begin_delay);
           open_nb_tx_handle_map[RDAT].erase(it);
           return;
       } else if(begin_phase == ACK) {
           fiber = SRSP;
           if(dat_crd_ext) {
               crd_fiber = SRSP;
               credits=dat_crd_ext->count;
           }
        }
    }
    if(crd_fiber != NONE) {
        h = nb_trTimedCrdHandle[crd_fiber]->begin_transaction(sc_core::sc_time_stamp() + begin_delay);
        h.add_relation(rel_str(tlm::scc::scv::PARENT_CHILD), parent);
        h.record_attribute("credits", credits);
        nb_trTimedCrdHandle[crd_fiber]->end_transaction(h, sc_core::sc_time_stamp() + end_delay);
    }
    h = nb_trTimedHandle[fiber]->begin_transaction(phase2string(begin_phase), sc_core::sc_time_stamp() + begin_delay);
    h.add_relation(rel_str(tlm::scc::scv::PARENT_CHILD), parent);
    if(begin_phase!=tlm::BEGIN_REQ) {
        auto it = nb_tx_handle_map.find(&trans);
        if(it!=nb_tx_handle_map.end())
            h.add_relation(rel_str(tlm::scc::scv::PREDECESSOR_SUCCESSOR), it->second);
    }
    tlm::scc::scv::record(h, trans);
    for(auto& ext : tlm::scc::scv::tlm_extension_recording_registry<TYPES>::get())
        if(ext)
            ext->recordBeginTx(h, trans);
    nb_tx_handle_map[&trans]=h;
    if(accepted && begin_phase != chi::ACK) {
        open_nb_tx_handle_map[fiber][&trans]=h;
    } else
        nb_trTimedHandle[fiber]->end_transaction(h, sc_core::sc_time_stamp() + end_delay);
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
template <typename TYPES> void chi_trx_recorder<TYPES>::invalidate_direct_mem_ptr(sc_dt::uint64 start_addr, sc_dt::uint64 end_addr) {
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
