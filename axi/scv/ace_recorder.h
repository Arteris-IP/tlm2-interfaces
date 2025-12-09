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
#include <axi/axi_tlm.h>
#include <axi/checker/checker_if.h>
#include <string>
#include <tlm/scc/scv/tlm_recorder.h>
#include <tlm/scc/scv/tlm_recording_extension.h>
#include <tlm_utils/peq_with_cb_and_phase.h>
#include <unordered_map>
#include <cci_configuration>

//! SCV components for AXI/ACE
namespace axi {
namespace scv {

bool register_extensions();

/*! \brief The TLM2 transaction recorder
 *
 * This module records all TLM transaction to a SCV transaction stream for
 * further viewing and analysis.
 * The handle of the created transaction is storee in an tlm_extension so that
 * another instance of the ace_recorder
 * e.g. further down the path can link to it.
 */
template <typename TYPES = axi::axi_protocol_types>
class ace_recorder : public virtual axi::ace_fw_transport_if<TYPES>, public virtual axi::ace_bw_transport_if<TYPES> {
public:
    template <unsigned int BUSWIDTH = 32, int N = 1, sc_core::sc_port_policy POL = sc_core::SC_ONE_OR_MORE_BOUND>
    using initiator_socket_type = axi::ace_initiator_socket<BUSWIDTH, TYPES, N, POL>;

    template <unsigned int BUSWIDTH = 32, int N = 1, sc_core::sc_port_policy POL = sc_core::SC_ONE_OR_MORE_BOUND>
    using target_socket_type = axi::ace_target_socket<BUSWIDTH, TYPES, N, POL>;

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

    //! \brief the attribute to  enable/disable protocol checking
    cci::cci_param<bool> enableProtocolChecker{"enableProtocolChecker", false};

    cci::cci_param<unsigned> rd_response_timeout{"rd_response_timeout", 0};

    cci::cci_param<unsigned> wr_response_timeout{"wr_response_timeout", 0};

    //! \brief the port where fw accesses are forwarded to
    virtual axi::ace_fw_transport_if<TYPES>* get_fw_if() = 0;

    //! \brief the port where bw accesses are forwarded to
    virtual axi::ace_bw_transport_if<TYPES>* get_bw_if() = 0;

    /*! \brief The constructor of the component
     *
     * \param name is the SystemC module name of the recorder
     * \param tr_db is a pointer to a transaction recording database. If none is
     * provided the default one is retrieved.
     *        If this database is not initialized (e.g. by not calling
     * scv_tr_db::set_default_db() ) recording is disabled.
     */
    ace_recorder(const char* name, unsigned bus_width, bool recording_enabled = true,
                 SCVNS scv_tr_db* tr_db = SCVNS scv_tr_db::get_default_db())
    : enableBlTracing("enableBlTracing", recording_enabled)
    , enableNbTracing("enableNbTracing", recording_enabled)
    , m_db(tr_db)
    , fixed_basename(name) {
        register_extensions();
    }

    virtual ~ace_recorder() override {
        nbtx_req_handle_map.clear();
        nbtx_last_req_handle_map.clear();
        nbtx_resp_handle_map.clear();
        nbtx_last_resp_handle_map.clear();
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
    /*! \brief The thread processing the non-blocking requests with their
     * annotated times
     * to generate the timed view of non-blocking tx
     */
    void record_nb_tx(typename TYPES::tlm_payload_type&, const typename TYPES::tlm_phase_type&, sc_core::sc_time, SCVNS scv_tr_handle, bool);
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

    enum DIR { FW, BW, REQ = FW, RESP = BW, ACK };
    //! non-blocking transaction recording stream handle
    SCVNS scv_tr_stream* nb_streamHandle{nullptr};
    //! non-blocking transaction recording stream handle with timing
    SCVNS scv_tr_stream* nb_streamHandleTimed{nullptr};
    //! transaction generator handle for non-blocking transactions
    std::array<SCVNS scv_tr_generator<std::string, std::string>*, 2> nb_trHandle{{nullptr, nullptr}};
    //! transaction generator handle for non-blocking transactions with annotated delays
    std::array<SCVNS scv_tr_generator<>*, 3> nb_trTimedHandle{{nullptr, nullptr, nullptr}};
    std::unordered_map<uint64_t, SCVNS scv_tr_handle> nbtx_req_handle_map;
    std::unordered_map<uint64_t, SCVNS scv_tr_handle> nbtx_last_req_handle_map;
    std::unordered_map<uint64_t, SCVNS scv_tr_handle> nbtx_resp_handle_map;
    std::unordered_map<uint64_t, SCVNS scv_tr_handle> nbtx_last_resp_handle_map;
    //! dmi transaction recording stream handle
    SCVNS scv_tr_stream* dmi_streamHandle{nullptr};
    //! transaction generator handle for DMI transactions
    SCVNS scv_tr_generator<>* dmi_trGetHandle{nullptr};
    SCVNS scv_tr_generator<sc_dt::uint64, sc_dt::uint64>* dmi_trInvalidateHandle{nullptr};

protected:
    void initialize_streams() {
        if(isRecordingBlockingTxEnabled()) {
            b_streamHandle = new SCVNS scv_tr_stream((fixed_basename + "_bl").c_str(), "[TLM][ace][b]", m_db);
            b_trHandle[tlm::TLM_READ_COMMAND] =
                new SCVNS scv_tr_generator<sc_dt::uint64, sc_dt::uint64>("read", *b_streamHandle, "start_delay", "end_delay");
            b_trHandle[tlm::TLM_WRITE_COMMAND] =
                new SCVNS scv_tr_generator<sc_dt::uint64, sc_dt::uint64>("write", *b_streamHandle, "start_delay", "end_delay");
            b_trHandle[tlm::TLM_IGNORE_COMMAND] =
                new SCVNS scv_tr_generator<sc_dt::uint64, sc_dt::uint64>("ignore", *b_streamHandle, "start_delay", "end_delay");
            if(enableTimedTracing.get_value()) {
                b_streamHandleTimed = new SCVNS scv_tr_stream((fixed_basename + "_bl_timed").c_str(), "[TLM][ace][b][timed]", m_db);
                b_trTimedHandle[tlm::TLM_READ_COMMAND] = new SCVNS scv_tr_generator<>("read", *b_streamHandleTimed);
                b_trTimedHandle[tlm::TLM_WRITE_COMMAND] = new SCVNS scv_tr_generator<>("write", *b_streamHandleTimed);
                b_trTimedHandle[tlm::TLM_IGNORE_COMMAND] = new SCVNS scv_tr_generator<>("ignore", *b_streamHandleTimed);
            }
        }
        if(isRecordingNonBlockingTxEnabled() && !nb_streamHandle) {
            nb_streamHandle = new SCVNS scv_tr_stream((fixed_basename + "_nb").c_str(), "[TLM][ace][nb]", m_db);
            nb_trHandle[FW] =
                new SCVNS scv_tr_generator<std::string, std::string>("fw", *nb_streamHandle, "tlm_phase", "tlm_phase[return_path]");
            nb_trHandle[BW] =
                new SCVNS scv_tr_generator<std::string, std::string>("bw", *nb_streamHandle, "tlm_phase", "tlm_phase[return_path]");
            if(enableTimedTracing.get_value()) {
                nb_streamHandleTimed = new SCVNS scv_tr_stream((fixed_basename + "_nb_timed").c_str(), "[TLM][ace][nb][timed]", m_db);
                nb_trTimedHandle[FW] = new SCVNS scv_tr_generator<>("request", *nb_streamHandleTimed);
                nb_trTimedHandle[BW] = new SCVNS scv_tr_generator<>("response", *nb_streamHandleTimed);
                nb_trTimedHandle[ACK] = new SCVNS scv_tr_generator<>("ack", *nb_streamHandleTimed);
            }
        }
        if(m_db && enableDmiTracing.get_value() && !dmi_streamHandle) {
            dmi_streamHandle = new SCVNS scv_tr_stream((fixed_basename + "_dmi").c_str(), "[TLM][ace][dmi]", m_db);
            dmi_trGetHandle = new SCVNS scv_tr_generator<>("get", *dmi_streamHandle);
            dmi_trInvalidateHandle =
                new SCVNS scv_tr_generator<sc_dt::uint64, sc_dt::uint64>("invalidate", *dmi_streamHandle, "start_addr", "end_addr");
        }
        //        if(enableProtocolChecker.get_value()) {
        //            checker=new axi::checker::ace_protocol(fixed_basename, bus_width/8, rd_response_timeout.get_value(),
        //            wr_response_timeout.get_value());
        //        }
    }

private:
    const std::string fixed_basename;
    axi::checker::checker_if<TYPES>* checker{nullptr};
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// implementations of functions
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

template <typename TYPES> void ace_recorder<TYPES>::b_transport(typename TYPES::tlm_payload_type& trans, sc_core::sc_time& delay) {
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
        bh = b_trTimedHandle[trans.get_command()]->begin_transaction(sc_core::sc_time_stamp()+delay);
        bh.add_relation(tlm::scc::scv::rel_str(tlm::scc::scv::PARENT_CHILD), h);
    }

    auto addr = trans.get_address();
    for(auto& ext : tlm::scc::scv::tlm_extension_recording_registry<TYPES>::inst().get())
        if(ext)
            ext->recordBeginTx(h, trans);
    tlm::scc::scv::tlm_recording_extension* preExt = nullptr;

    trans.get_extension(preExt);
    if(preExt == nullptr) { // we are the first recording this transaction
        preExt = new tlm::scc::scv::tlm_recording_extension(h, this);
        if(trans.has_mm())
            trans.set_auto_extension(preExt);
        else
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
        if(trans.has_mm())
            trans.set_auto_extension(static_cast<tlm::scc::scv::tlm_recording_extension*>(nullptr));
        else
            delete trans.set_extension(static_cast<tlm::scc::scv::tlm_recording_extension*>(nullptr));
    } else {
        preExt->txHandle = preTx;
    }

    trans.set_address(addr);
    tlm::scc::scv::record(h, trans);
    for(auto& ext : tlm::scc::scv::tlm_extension_recording_registry<TYPES>::inst().get())
        if(ext)
            ext->recordEndTx(h, trans);
    // End the transaction
    b_trHandle[trans.get_command()]->end_transaction(h, delay.value(), sc_core::sc_time_stamp());
    // and now the stuff for the timed tx
    if(bh.is_valid()) {
        tlm::scc::scv::record(bh, trans);
        bh.end_transaction(sc_core::sc_time_stamp()+delay);
    }
}

template <typename TYPES> void ace_recorder<TYPES>::b_snoop(typename TYPES::tlm_payload_type& trans, sc_core::sc_time& delay) {
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
        bh = b_trTimedHandle[trans.get_command()]->begin_transaction(sc_core::sc_time_stamp()+delay);
        bh.add_relation(tlm::scc::scv::rel_str(tlm::scc::scv::PARENT_CHILD), h);
    }

    for(auto& ext : tlm::scc::scv::tlm_extension_recording_registry<TYPES>::inst().get())
        if(ext)
            ext->recordBeginTx(h, trans);
    tlm::scc::scv::tlm_recording_extension* preExt = NULL;

    trans.get_extension(preExt);
    if(preExt == NULL) { // we are the first recording this transaction
        preExt = new tlm::scc::scv::tlm_recording_extension(h, this);
        if(trans.has_mm())
            trans.set_auto_extension(preExt);
        else
            trans.set_extension(preExt);
    } else {
        h.add_relation(tlm::scc::scv::rel_str(tlm::scc::scv::PREDECESSOR_SUCCESSOR), preExt->txHandle);
    }
    SCVNS scv_tr_handle preTx(preExt->txHandle);
    preExt->txHandle = h;
    get_bw_if()->b_snoop(trans, delay);
    trans.get_extension(preExt);
    if(preExt->get_creator() == this) {
        // clean-up the extension if this is the original creator
        if(trans.has_mm())
            trans.set_auto_extension(static_cast<tlm::scc::scv::tlm_recording_extension*>(nullptr));
        else
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
    if(bh.is_valid()) {
        tlm::scc::scv::record(bh, trans);
        bh.end_transaction(sc_core::sc_time_stamp()+delay);
    }
}

template <typename TYPES>
tlm::tlm_sync_enum ace_recorder<TYPES>::nb_transport_fw(typename TYPES::tlm_payload_type& trans, typename TYPES::tlm_phase_type& phase,
                                                        sc_core::sc_time& delay) {
    if(!isRecordingNonBlockingTxEnabled()) {
        if(checker) {
            checker->fw_pre(trans, phase);
            tlm::tlm_sync_enum status = get_fw_if()->nb_transport_fw(trans, phase, delay);
            checker->fw_post(trans, phase, status);
            return status;
        } else
            return get_fw_if()->nb_transport_fw(trans, phase, delay);
    }
    /*************************************************************************
     * prepare recording
     *************************************************************************/
    // Get a handle for the new transaction
    SCVNS scv_tr_handle h = nb_trHandle[FW]->begin_transaction(phase.get_name());
    tlm::scc::scv::tlm_recording_extension* preExt = nullptr;
    trans.get_extension(preExt);
    if((phase == axi::BEGIN_PARTIAL_REQ || phase == tlm::BEGIN_REQ) && preExt == nullptr) { // we are the first recording this transaction
        preExt = new tlm::scc::scv::tlm_recording_extension(h, this);
        if(trans.has_mm())
            trans.set_auto_extension(preExt);
        else
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
    if(nb_streamHandleTimed) {
        record_nb_tx(trans, phase, sc_core::sc_time_stamp()+delay, h, phase == tlm::BEGIN_RESP || phase == axi::BEGIN_PARTIAL_RESP);
    }
    /*************************************************************************
     * do the access
     *************************************************************************/
    if(checker)
        checker->fw_pre(trans, phase);
    tlm::tlm_sync_enum status = get_fw_if()->nb_transport_fw(trans, phase, delay);
    if(checker)
        checker->fw_post(trans, phase, status);
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
    if(status == tlm::TLM_COMPLETED || (phase == axi::ACK)) {
        // the transaction is finished
        trans.get_extension(preExt);
        if(preExt && preExt->get_creator() == this) {
            // clean-up the extension if this is the original creator
            if(trans.has_mm())
                trans.set_auto_extension(static_cast<tlm::scc::scv::tlm_recording_extension*>(nullptr));
            else
                delete trans.set_extension(static_cast<tlm::scc::scv::tlm_recording_extension*>(nullptr));
        }
        /*************************************************************************
         * do the timed notification if req. finished here
         *************************************************************************/
        if(nb_streamHandleTimed) {
            record_nb_tx(trans, (status == tlm::TLM_COMPLETED && phase == tlm::BEGIN_REQ) ? tlm::END_RESP : phase, sc_core::sc_time_stamp()+delay, h, false);
        }
    } else if(nb_streamHandleTimed && status == tlm::TLM_UPDATED) {
        record_nb_tx(trans, phase, sc_core::sc_time_stamp()+delay, h, false);
    }
    // End the transaction
    nb_trHandle[FW]->end_transaction(h, phase.get_name());
    return status;
}

template <typename TYPES>
tlm::tlm_sync_enum ace_recorder<TYPES>::nb_transport_bw(typename TYPES::tlm_payload_type& trans, typename TYPES::tlm_phase_type& phase,
                                                        sc_core::sc_time& delay) {
    if(!isRecordingNonBlockingTxEnabled()) {
        if(checker) {
            checker->bw_pre(trans, phase);
            tlm::tlm_sync_enum status = get_bw_if()->nb_transport_bw(trans, phase, delay);
            checker->bw_post(trans, phase, status);
            return status;
        } else
            return get_bw_if()->nb_transport_bw(trans, phase, delay);
    }
    /*************************************************************************
     * prepare recording
     *************************************************************************/
    // Get a handle for the new transaction
    SCVNS scv_tr_handle h = nb_trHandle[BW]->begin_transaction(phase.get_name());
    tlm::scc::scv::tlm_recording_extension* preExt = nullptr;
    trans.get_extension(preExt);
    if(phase == tlm::BEGIN_REQ && preExt == nullptr) { // we are the first recording this transaction
        preExt = new tlm::scc::scv::tlm_recording_extension(h, this);
        if(trans.has_mm())
            trans.set_auto_extension(preExt);
        else
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
    if(nb_streamHandleTimed) {
        record_nb_tx(trans, phase, sc_core::sc_time_stamp()+delay, h, phase == tlm::BEGIN_REQ);
    }
    /*************************************************************************
     * do the access
     *************************************************************************/
    if(checker)
        checker->bw_pre(trans, phase);
    tlm::tlm_sync_enum status = get_bw_if()->nb_transport_bw(trans, phase, delay);
    if(checker)
        checker->bw_post(trans, phase, status);
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
    if(status == tlm::TLM_COMPLETED || (status == tlm::TLM_UPDATED && phase == axi::ACK)) {
        // the transaction is finished
        trans.get_extension(preExt);
        if(preExt->get_creator() == this) {
            // clean-up the extension if this is the original creator
            if(trans.has_mm())
                trans.set_auto_extension(static_cast<tlm::scc::scv::tlm_recording_extension*>(nullptr));
            else
                delete trans.set_extension(static_cast<tlm::scc::scv::tlm_recording_extension*>(nullptr));
        }
        /*************************************************************************
         * do the timed notification if req. finished here
         *************************************************************************/
        if(nb_streamHandleTimed) {
            record_nb_tx(trans, (status == tlm::TLM_COMPLETED && phase == tlm::BEGIN_REQ) ? tlm::END_RESP : phase, sc_core::sc_time_stamp()+delay, h, false);
        }
    } else if(nb_streamHandleTimed && status == tlm::TLM_UPDATED) {
        record_nb_tx(trans, phase, sc_core::sc_time_stamp()+delay, h, false);
    }
    // End the transaction
    nb_trHandle[BW]->end_transaction(h, phase.get_name());
    return status;
}

template <typename TYPES> void ace_recorder<TYPES>::record_nb_tx(typename TYPES::tlm_payload_type & trans, const typename TYPES::tlm_phase_type& phase, sc_core::sc_time delay, SCVNS scv_tr_handle parent, bool is_snoop) {
    SCVNS scv_tr_handle h;
    // Now process outstanding recordings
    auto t = sc_core::sc_time_stamp()+delay;
    auto id = reinterpret_cast<uintptr_t>(&trans);
    if(phase == tlm::BEGIN_REQ || phase == axi::BEGIN_PARTIAL_REQ) {
        h = nb_trTimedHandle[REQ]->begin_transaction(t);
        tlm::scc::scv::record(h, trans);
        h.add_relation(tlm::scc::scv::rel_str(tlm::scc::scv::PARENT_CHILD), parent);
        nbtx_req_handle_map[id] = h;
    } else if(phase == tlm::END_REQ || phase == axi::END_PARTIAL_REQ) {
        auto it = nbtx_req_handle_map.find(id);
        sc_assert(it != nbtx_req_handle_map.end());
        h = it->second;
        nbtx_req_handle_map.erase(it);
        h.end_transaction(t);
        nbtx_last_req_handle_map[id] = h;
    } else if(phase == tlm::BEGIN_RESP || phase == axi::BEGIN_PARTIAL_RESP) {
        auto it = nbtx_req_handle_map.find(id);
        if(it != nbtx_req_handle_map.end()) {
            h = it->second;
            nbtx_req_handle_map.erase(it);
            h.end_transaction(t);
            nbtx_last_req_handle_map[id] = h;
        }
        h = nb_trTimedHandle[RESP]->begin_transaction(t);
        tlm::scc::scv::record(h, trans);
        h.add_relation(tlm::scc::scv::rel_str(tlm::scc::scv::PARENT_CHILD), parent);
        nbtx_resp_handle_map[id] = h;
        it = nbtx_last_req_handle_map.find(id);
        if(it != nbtx_last_req_handle_map.end()) {
            SCVNS scv_tr_handle& pred = it->second;
            h.add_relation(tlm::scc::scv::rel_str(tlm::scc::scv::PREDECESSOR_SUCCESSOR), pred);
            nbtx_last_req_handle_map.erase(it);
        } else {
            it = nbtx_last_resp_handle_map.find(id);
            if(it != nbtx_last_resp_handle_map.end()) {
                SCVNS scv_tr_handle& pred = it->second;
                h.add_relation(tlm::scc::scv::rel_str(tlm::scc::scv::PREDECESSOR_SUCCESSOR), pred);
                nbtx_last_resp_handle_map.erase(it);
            }
        }
    } else if(phase == tlm::END_RESP || phase == axi::END_PARTIAL_RESP) {
        auto it = nbtx_resp_handle_map.find(id);
        if(it != nbtx_resp_handle_map.end()) {
            h = it->second;
            nbtx_resp_handle_map.erase(it);
            h.end_transaction(t);
            if(phase == axi::END_PARTIAL_RESP) {
                nbtx_last_resp_handle_map[id] = h;
            }
        }
    } else if(phase == axi::ACK) {
        h = nb_trTimedHandle[ACK]->begin_transaction(t);
        tlm::scc::scv::record(h, trans);
        h.add_relation(tlm::scc::scv::rel_str(tlm::scc::scv::PARENT_CHILD), parent);
        h.end_transaction(t);
    } else
        sc_assert(!"phase not supported!");
    return;
}

template <typename TYPES> bool ace_recorder<TYPES>::get_direct_mem_ptr(typename TYPES::tlm_payload_type& trans, tlm::tlm_dmi& dmi_data) {
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
template <typename TYPES> void ace_recorder<TYPES>::invalidate_direct_mem_ptr(sc_dt::uint64 start_addr, sc_dt::uint64 end_addr) {
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
/*! \brief The debug transportfunction
 *
 * This type of transaction is just forwarded and not recorded.
 * \param trans is the generic payload of the transaction
 * \return the sync state of the transaction
 */
template <typename TYPES> unsigned int ace_recorder<TYPES>::transport_dbg(typename TYPES::tlm_payload_type& trans) {
    unsigned int count = get_fw_if()->transport_dbg(trans);
    return count;
}
} // namespace scv
} // namespace axi
