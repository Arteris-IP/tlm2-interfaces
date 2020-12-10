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

#pragma once

#include <array>
#include <axi/axi_tlm.h>
#include <regex>
#include <scv.h>
#include <scv4tlm/tlm_recorder.h>
#include <scv4tlm/tlm_recording_extension.h>
#include <string>
#include <tlm_utils/peq_with_cb_and_phase.h>
#include <unordered_map>
#include <vector>

namespace scv4axi {

bool register_extensions();

/*! \brief The TLM2 transaction recorder
 *
 * This module records all TLM transaction to a SCV transaction stream for
 * further viewing and analysis.
 * The handle of the created transaction is storee in an tlm_extension so that
 * another instance of the scv_axi_recorder
 * e.g. further down the path can link to it.
 */
template <typename TYPES = axi::axi_protocol_types>
class axi_recorder : public virtual axi::axi_fw_transport_if<TYPES>, public virtual axi::axi_bw_transport_if<TYPES> {
public:
    template <unsigned int BUSWIDTH = 32, int N = 1, sc_core::sc_port_policy POL = sc_core::SC_ONE_OR_MORE_BOUND>
    using initiator_socket_type = axi::axi_initiator_socket<BUSWIDTH, TYPES, N, POL>;

    template <unsigned int BUSWIDTH = 32, int N = 1, sc_core::sc_port_policy POL = sc_core::SC_ONE_OR_MORE_BOUND>
    using target_socket_type = axi::axi_target_socket<BUSWIDTH, TYPES, N, POL>;

    using recording_types = scv4tlm::impl::tlm_recording_types<TYPES>;
    using mm = tlm::tlm_mm<recording_types>;
    using tlm_recording_payload = scv4tlm::impl::tlm_recording_payload<TYPES>;

    SC_HAS_PROCESS(axi_recorder<TYPES>); // NOLINT

    //! \brief the attribute to selectively enable/disable recording
    sc_core::sc_attribute<bool> enableTracing;

    //! \brief the attribute to selectively enable/disable timed recording
    sc_core::sc_attribute<bool> enableTimed;

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
    axi_recorder(const char* name, bool recording_enabled = true, scv_tr_db* tr_db = scv_tr_db::get_default_db())
    : enableTracing("enableTracing", recording_enabled)
    , enableTimed("enableTimed", recording_enabled)
    , b_timed_peq(this, &axi_recorder::btx_cb)
    , nb_timed_peq(this, &axi_recorder::nbtx_cb)
    , m_db(tr_db)
    , fixed_basename(name) {
        register_extensions();
    }

    virtual ~axi_recorder() override {
        delete b_streamHandle;
        delete b_streamHandleTimed;
        for(size_t i = 0; i < b_trTimedHandle.size(); ++i)
            delete b_trTimedHandle[i];
        for(size_t i = 0; i < nb_streamHandle.size(); ++i)
            delete nb_streamHandle[i];
        for(size_t i = 0; i < nb_streamHandleTimed.size(); ++i)
            delete nb_streamHandleTimed[i];
        for(size_t i = 0; i < nb_fw_trHandle.size(); ++i)
            delete nb_fw_trHandle[i];
        for(size_t i = 0; i < nb_txReqHandle.size(); ++i)
            delete nb_txReqHandle[i];
        for(size_t i = 0; i < nb_bw_trHandle.size(); ++i)
            delete nb_bw_trHandle[i];
        for(size_t i = 0; i < nb_txRespHandle.size(); ++i)
            delete nb_txRespHandle[i];
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
    const bool isRecordingEnabled() const { return m_db != NULL && enableTracing.value; }

private:
    //! event queue to hold time points of blocking transactions
    tlm_utils::peq_with_cb_and_phase<axi_recorder, recording_types> b_timed_peq;
    //! event queue to hold time points of non-blocking transactions
    tlm_utils::peq_with_cb_and_phase<axi_recorder, recording_types> nb_timed_peq;
    /*! \brief The thread processing the blocking accesses with their annotated
     * times
     *  to generate the timed view of blocking tx
     */
    void btx_cb(tlm_recording_payload& rec_parts, const typename TYPES::tlm_phase_type& phase);
    /*! \brief The thread processing the non-blocking requests with their
     * annotated times
     * to generate the timed view of non-blocking tx
     */
    void nbtx_cb(tlm_recording_payload& rec_parts, const typename TYPES::tlm_phase_type& phase);
    //! transaction recording database
    scv_tr_db* m_db{nullptr};
    //! blocking transaction recording stream handle
    scv_tr_stream* b_streamHandle{nullptr};
    //! transaction generator handle for blocking transactions
    std::array<scv_tr_generator<sc_dt::uint64, sc_dt::uint64>*, 3> b_trHandle{{nullptr, nullptr, nullptr}};
    //! timed blocking transaction recording stream handle
    scv_tr_stream* b_streamHandleTimed{nullptr};
    //! transaction generator handle for blocking transactions with annotated
    //! delays
    std::array<scv_tr_generator<tlm::tlm_command, tlm::tlm_response_status>*, 3> b_trTimedHandle{{nullptr, nullptr, nullptr}};
    std::unordered_map<uint64, scv_tr_handle> btx_handle_map;

    enum DIR { FW, BW };
    //! non-blocking transaction recording stream handle
    std::array<scv_tr_stream*, 2> nb_streamHandle{{nullptr, nullptr}};
    //! non-blocking transaction recording stream handle
    std::array<scv_tr_stream*, 2> nb_streamHandleTimed{{nullptr, nullptr}};
    //! transaction generator handle for forward non-blocking transactions
    std::array<scv_tr_generator<std::string, tlm::tlm_sync_enum>*, 3> nb_fw_trHandle{{nullptr, nullptr, nullptr}};
    //! transaction generator handle for forward non-blocking transactions with
    //! annotated delays
    std::array<scv_tr_generator<>*, 4> nb_txReqHandle{{nullptr, nullptr, nullptr, nullptr}};
    std::unordered_map<uint64, scv_tr_handle> nbtx_req_handle_map;
    //! transaction generator handle for backward non-blocking transactions
    std::array<scv_tr_generator<std::string, tlm::tlm_sync_enum>*, 4> nb_bw_trHandle{{nullptr, nullptr, nullptr, nullptr}};
    //! transaction generator handle for backward non-blocking transactions with
    //! annotated delays
    std::array<scv_tr_generator<>*, 3> nb_txRespHandle{{nullptr, nullptr, nullptr}};
    std::unordered_map<uint64, scv_tr_handle> nbtx_last_req_handle_map;
    std::unordered_map<uint64, scv_tr_handle> nbtx_resp_handle_map;
    std::unordered_map<uint64, scv_tr_handle> nbtx_last_resp_handle_map;
    //! dmi transaction recording stream handle
    scv_tr_stream* dmi_streamHandle{nullptr};
    //! transaction generator handle for DMI transactions
    scv_tr_generator<scv4tlm::tlm_gp_data, scv4tlm::tlm_dmi_data>* dmi_trGetHandle{nullptr};
    scv_tr_generator<sc_dt::uint64, sc_dt::uint64>* dmi_trInvalidateHandle{nullptr};

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

template <typename TYPES> void axi_recorder<TYPES>::b_transport(typename TYPES::tlm_payload_type& trans, sc_core::sc_time& delay) {
    tlm_recording_payload* req{nullptr};
    if(!isRecordingEnabled()) {
        get_fw_if()->b_transport(trans, delay);
        return;
    }
    if(b_streamHandle == NULL) {
        b_streamHandle = new scv_tr_stream((fixed_basename + "_bl").c_str(), "TRANSACTOR", m_db);
        b_trHandle[tlm::TLM_READ_COMMAND] =
            new scv_tr_generator<sc_dt::uint64, sc_dt::uint64>("read", *b_streamHandle, "start_delay", "end_delay");
        b_trHandle[tlm::TLM_WRITE_COMMAND] =
            new scv_tr_generator<sc_dt::uint64, sc_dt::uint64>("write", *b_streamHandle, "start_delay", "end_delay");
        b_trHandle[tlm::TLM_IGNORE_COMMAND] =
            new scv_tr_generator<sc_dt::uint64, sc_dt::uint64>("ignore", *b_streamHandle, "start_delay", "end_delay");
    }
    // Get a handle for the new transaction
    scv_tr_handle h = b_trHandle[trans.get_command()]->begin_transaction(delay.value(), sc_time_stamp());
    scv4tlm::tlm_gp_data tgd(trans);

    /*************************************************************************
     * do the timed notification
     *************************************************************************/
    if(enableTimed.value) {
        req = mm::get().allocate();
        req->acquire();
        (*req) = trans;
        req->parent = h;
        req->id = h.get_id();
        b_timed_peq.notify(*req, tlm::BEGIN_REQ, delay);
    }

    for(auto& ext : scv4tlm::tlm_extension_recording_registry<TYPES>::inst().get())
        if(ext)
            ext->recordBeginTx(h, trans);
    scv4tlm::tlm_recording_extension* preExt = NULL;

    trans.get_extension(preExt);
    if(preExt == NULL) { // we are the first recording this transaction
        preExt = new scv4tlm::tlm_recording_extension(h, this);
        trans.set_extension(preExt);
    } else {
        h.add_relation(scv4tlm::rel_str(scv4tlm::PREDECESSOR_SUCCESSOR), preExt->txHandle);
    }
    scv_tr_handle preTx(preExt->txHandle);
    preExt->txHandle = h;
    if(trans.get_command() == tlm::TLM_WRITE_COMMAND && tgd.data_length < 8)
        h.record_attribute("trans.data_value", tgd.get_data_value());
    get_fw_if()->b_transport(trans, delay);
    trans.get_extension(preExt);
    if(preExt->get_creator() == this) {
        // clean-up the extension if this is the original creator
        delete trans.set_extension(static_cast<scv4tlm::tlm_recording_extension*>(nullptr));
    } else {
        preExt->txHandle = preTx;
    }

    tgd.response_status = trans.get_response_status();
    h.record_attribute("trans", tgd);
    if(trans.get_command() == tlm::TLM_READ_COMMAND && tgd.data_length < 8)
        h.record_attribute("trans.data_value", tgd.get_data_value());
    for(auto& ext : scv4tlm::tlm_extension_recording_registry<TYPES>::inst().get())
        if(ext)
            ext->recordEndTx(h, trans);
    // End the transaction
    b_trHandle[trans.get_command()]->end_transaction(h, delay.value(), sc_time_stamp());
    // and now the stuff for the timed tx
    if(enableTimed.value) {
        b_timed_peq.notify(*req, tlm::END_RESP, delay);
    }
}

template <typename TYPES> void axi_recorder<TYPES>::btx_cb(tlm_recording_payload& rec_parts, const typename TYPES::tlm_phase_type& phase) {
    scv_tr_handle h;
    if(b_trTimedHandle[0] == NULL) {
        b_streamHandleTimed = new scv_tr_stream((fixed_basename + "_bl_timed").c_str(), "TRANSACTOR", m_db);
        b_trTimedHandle[0] = new scv_tr_generator<tlm::tlm_command, tlm::tlm_response_status>("read", *b_streamHandleTimed);
        b_trTimedHandle[1] = new scv_tr_generator<tlm::tlm_command, tlm::tlm_response_status>("write", *b_streamHandleTimed);
        b_trTimedHandle[2] = new scv_tr_generator<tlm::tlm_command, tlm::tlm_response_status>("ignore", *b_streamHandleTimed);
    }
    // Now process outstanding recordings
    switch(phase) {
    case tlm::BEGIN_REQ: {
        scv4tlm::tlm_gp_data tgd(rec_parts);
        h = b_trTimedHandle[rec_parts.get_command()]->begin_transaction(rec_parts.get_command());
        h.record_attribute("trans", tgd);
        h.add_relation(scv4tlm::rel_str(scv4tlm::PARENT_CHILD), rec_parts.parent);
        btx_handle_map[rec_parts.id] = h;
    } break;
    case tlm::END_RESP: {
        auto it = btx_handle_map.find(rec_parts.id);
        sc_assert(it != btx_handle_map.end());
        h = it->second;
        btx_handle_map.erase(it);
        h.end_transaction(h, rec_parts.get_response_status());
        rec_parts.release();
    } break;
    default:
        sc_assert(!"phase not supported!");
    }
    return;
}

template <typename TYPES>
tlm::tlm_sync_enum axi_recorder<TYPES>::nb_transport_fw(typename TYPES::tlm_payload_type& trans, typename TYPES::tlm_phase_type& phase,
                                                        sc_core::sc_time& delay) {
    if(!isRecordingEnabled())
        return get_fw_if()->nb_transport_fw(trans, phase, delay);
    // initialize stream and generator if not yet done
    if(nb_streamHandle[FW] == NULL) {
        nb_streamHandle[FW] = new scv_tr_stream((fixed_basename + "_nb_fw").c_str(), "TRANSACTOR", m_db);
        nb_fw_trHandle[tlm::TLM_READ_COMMAND] =
            new scv_tr_generator<std::string, tlm::tlm_sync_enum>("read", *nb_streamHandle[FW], "tlm_phase", "tlm_sync");
        nb_fw_trHandle[tlm::TLM_WRITE_COMMAND] =
            new scv_tr_generator<std::string, tlm::tlm_sync_enum>("write", *nb_streamHandle[FW], "tlm_phase", "tlm_sync");
        nb_fw_trHandle[tlm::TLM_IGNORE_COMMAND] =
            new scv_tr_generator<std::string, tlm::tlm_sync_enum>("ignore", *nb_streamHandle[FW], "tlm_phase", "tlm_sync");
    }
   /*************************************************************************
     * prepare recording
     *************************************************************************/
    // Get a handle for the new transaction
    scv_tr_handle h = nb_fw_trHandle[trans.get_command()]->begin_transaction(phase2string(phase));
    scv4tlm::tlm_recording_extension* preExt = NULL;
    trans.get_extension(preExt);
    if((phase == axi::BEGIN_PARTIAL_REQ || phase == tlm::BEGIN_REQ) && preExt == NULL) { // we are the first recording this transaction
        preExt = new scv4tlm::tlm_recording_extension(h, this);
        trans.set_extension(preExt);
    } else if(preExt != NULL) {
        // link handle if we have a predecessor
        h.add_relation(scv4tlm::rel_str(scv4tlm::PREDECESSOR_SUCCESSOR), preExt->txHandle);
    } else {
        sc_assert(preExt != NULL && "ERROR on forward path in phase other than tlm::BEGIN_REQ");
    }
    // update the extension
    preExt->txHandle = h;
    h.record_attribute("delay", delay.to_string());
    for(auto& ext : scv4tlm::tlm_extension_recording_registry<TYPES>::inst().get())
        if(ext)
            ext->recordBeginTx(h, trans);
    scv4tlm::tlm_gp_data tgd(trans);
    /*************************************************************************
     * do the timed notification
     *************************************************************************/
    if(enableTimed.value) {
        tlm_recording_payload* req = mm::get().allocate();
        req->acquire();
        (*req) = trans;
        req->parent = h;
        nb_timed_peq.notify(*req, phase, delay);
    }
    /*************************************************************************
     * do the access
     *************************************************************************/
    tlm::tlm_sync_enum status = get_fw_if()->nb_transport_fw(trans, phase, delay);
    /*************************************************************************
     * handle recording
     *************************************************************************/
    tgd.response_status = trans.get_response_status();
    h.record_attribute("trans.uid", reinterpret_cast<uintptr_t>(&trans));
    h.record_attribute("trans", tgd);
    if(tgd.data_length < 8) {
        uint64_t buf = 0;
        // FIXME: this is endianess dependent
        for(size_t i = 0; i < tgd.data_length; i++)
            buf += (*tgd.data) << i * 8;
        h.record_attribute("trans.data_value", buf);
    }
    for(auto& ext : scv4tlm::tlm_extension_recording_registry<TYPES>::inst().get())
        if(ext)
            ext->recordEndTx(h, trans);
    h.record_attribute("tlm_phase[return_path]", phase2string(phase));
    h.record_attribute("delay[return_path]", delay.to_string());
    // get the extension and free the memory if it was mine
    if(status == tlm::TLM_COMPLETED || (status == tlm::TLM_ACCEPTED && phase == tlm::END_RESP)) {
        // the transaction is finished
        trans.get_extension(preExt);
        if(preExt && preExt->get_creator() == this) {
            // clean-up the extension if this is the original creator
            delete trans.set_extension(static_cast<scv4tlm::tlm_recording_extension*>(nullptr));
        }
        /*************************************************************************
         * do the timed notification if req. finished here
         *************************************************************************/
        if(enableTimed.value) {
            tlm_recording_payload* req = mm::get().allocate();
            req->acquire();
            (*req) = trans;
            req->parent = h;
            nb_timed_peq.notify(*req, (status == tlm::TLM_COMPLETED && phase == tlm::BEGIN_REQ) ? tlm::END_RESP : phase, delay);
        }
    } else if(enableTimed.value && status == tlm::TLM_UPDATED) {
        tlm_recording_payload* req = mm::get().allocate();
        req->acquire();
        (*req) = trans;
        req->parent = h;
        nb_timed_peq.notify(*req, phase, delay);
    }
    // End the transaction
    nb_fw_trHandle[trans.get_command()]->end_transaction(h, status);
    return status;
}

template <typename TYPES>
tlm::tlm_sync_enum axi_recorder<TYPES>::nb_transport_bw(typename TYPES::tlm_payload_type& trans, typename TYPES::tlm_phase_type& phase,
                                                        sc_core::sc_time& delay) {
    if(!isRecordingEnabled())
        return get_bw_if()->nb_transport_bw(trans, phase, delay);
    if(nb_streamHandle[BW] == NULL) {
        nb_streamHandle[BW] = new scv_tr_stream((fixed_basename + "_nb_bw").c_str(), "TRANSACTOR", m_db);
        nb_bw_trHandle[0] = new scv_tr_generator<std::string, tlm::tlm_sync_enum>("read", *nb_streamHandle[BW], "tlm_phase", "tlm_sync");
        nb_bw_trHandle[1] = new scv_tr_generator<std::string, tlm::tlm_sync_enum>("write", *nb_streamHandle[BW], "tlm_phase", "tlm_sync");
        nb_bw_trHandle[2] = new scv_tr_generator<std::string, tlm::tlm_sync_enum>("ignore", *nb_streamHandle[BW], "tlm_phase", "tlm_sync");
    }
    /*************************************************************************
     * prepare recording
     *************************************************************************/
    // Get a handle for the new transaction
    scv_tr_handle h = nb_bw_trHandle[trans.get_command()]->begin_transaction(phase2string(phase));
    scv4tlm::tlm_recording_extension* preExt = NULL;
    trans.get_extension(preExt);
    if(phase == tlm::BEGIN_REQ && preExt == NULL) { // we are the first recording this transaction
        preExt = new scv4tlm::tlm_recording_extension(h, this);
        trans.set_extension(preExt);
    } else if(preExt != NULL) {
        // link handle if we have a predecessor
        h.add_relation(scv4tlm::rel_str(scv4tlm::PREDECESSOR_SUCCESSOR), preExt->txHandle);
    } else {
        sc_assert(preExt != NULL && "ERROR on backward path in phase other than tlm::BEGIN_REQ");
    }
    // and set the extension handle to this transaction
    preExt->txHandle = h;
    h.record_attribute("delay", delay.to_string());
    for(auto& ext : scv4tlm::tlm_extension_recording_registry<TYPES>::inst().get())
        if(ext)
            ext->recordBeginTx(h, trans);
    scv4tlm::tlm_gp_data tgd(trans);
    /*************************************************************************
     * do the timed notification
     *************************************************************************/
    if(enableTimed.value) {
        tlm_recording_payload* req = mm::get().allocate();
        req->acquire();
        (*req) = trans;
        req->parent = h;
        nb_timed_peq.notify(*req, phase, delay);
    }
    /*************************************************************************
     * do the access
     *************************************************************************/
    tlm::tlm_sync_enum status = get_bw_if()->nb_transport_bw(trans, phase, delay);
    /*************************************************************************
     * handle recording
     *************************************************************************/
    tgd.response_status = trans.get_response_status();
    h.record_attribute("trans.uid", reinterpret_cast<uintptr_t>(&trans));
    h.record_attribute("trans", tgd);
    if(tgd.data_length < 8) {
        uint64_t buf = 0;
        // FIXME: this is endianess dependent
        for(size_t i = 0; i < tgd.data_length; i++)
            buf += (*tgd.data) << i * 8;
        h.record_attribute("trans.data_value", buf);
    }
    for(auto& ext : scv4tlm::tlm_extension_recording_registry<TYPES>::inst().get())
        if(ext)
            ext->recordEndTx(h, trans);
    h.record_attribute("tlm_phase[return_path]", phase2string(phase));
    h.record_attribute("delay[return_path]", delay.to_string());
    // get the extension and free the memory if it was mine
    if(status == tlm::TLM_COMPLETED || (status == tlm::TLM_UPDATED && phase == tlm::END_RESP)) {
        // the transaction is finished
        trans.get_extension(preExt);
        if(preExt->get_creator() == this) {
            // clean-up the extension if this is the original creator
            delete trans.set_extension(static_cast<scv4tlm::tlm_recording_extension*>(nullptr));
        }
        /*************************************************************************
         * do the timed notification if req. finished here
         *************************************************************************/
        if(enableTimed.value) {
        tlm_recording_payload* req = mm::get().allocate();
        req->acquire();
        (*req) = trans;
        req->parent = h;
        nb_timed_peq.notify(*req, (status == tlm::TLM_COMPLETED && phase == tlm::BEGIN_REQ) ? tlm::END_RESP : phase, delay);
        }
    } else if(enableTimed.value && status == tlm::TLM_UPDATED) {
        tlm_recording_payload* req = mm::get().allocate();
        req->acquire();
        (*req) = trans;
        req->parent = h;
        nb_timed_peq.notify(*req, phase, delay);
    }
    // End the transaction
    nb_bw_trHandle[trans.get_command()]->end_transaction(h, status);
    return status;
}

template <typename TYPES> void axi_recorder<TYPES>::nbtx_cb(tlm_recording_payload& rec_parts, const typename TYPES::tlm_phase_type& phase) {
    scv_tr_handle h;
    if(nb_streamHandleTimed[FW] == NULL) {
        nb_streamHandleTimed[FW] = new scv_tr_stream((fixed_basename + "_nb_req_timed").c_str(), "TRANSACTOR", m_db);
        nb_txReqHandle[0] = new scv_tr_generator<>("read", *nb_streamHandleTimed[FW]);
        nb_txReqHandle[1] = new scv_tr_generator<>("write", *nb_streamHandleTimed[FW]);
        nb_txReqHandle[2] = new scv_tr_generator<>("ignore", *nb_streamHandleTimed[FW]);
    }
    if(nb_streamHandleTimed[BW] == NULL) {
        nb_streamHandleTimed[BW] = new scv_tr_stream((fixed_basename + "_nb_resp_timed").c_str(), "TRANSACTOR", m_db);
        nb_txRespHandle[0] = new scv_tr_generator<>("read", *nb_streamHandleTimed[BW]);
        nb_txRespHandle[1] = new scv_tr_generator<>("write", *nb_streamHandleTimed[BW]);
        nb_txRespHandle[2] = new scv_tr_generator<>("ignore", *nb_streamHandleTimed[BW]);
    }
    scv4tlm::tlm_gp_data tgd(rec_parts);
    // Now process outstanding recordings
    if(phase == tlm::BEGIN_REQ || phase == axi::BEGIN_PARTIAL_REQ) {
        h = nb_txReqHandle[rec_parts.get_command()]->begin_transaction();
        h.record_attribute("trans", tgd);
        h.add_relation(scv4tlm::rel_str(scv4tlm::PARENT_CHILD), rec_parts.parent);
        nbtx_req_handle_map[rec_parts.id] = h;
    } else if(phase == tlm::END_REQ || phase == axi::END_PARTIAL_REQ) {
        auto it = nbtx_req_handle_map.find(rec_parts.id);
        if(it != nbtx_req_handle_map.end()) {
            h = it->second;
            nbtx_req_handle_map.erase(it);
            h.end_transaction();
            nbtx_last_req_handle_map[rec_parts.id] = h;
        }
    } else if(phase == tlm::BEGIN_RESP || phase == axi::BEGIN_PARTIAL_RESP) {
        auto it = nbtx_req_handle_map.find(rec_parts.id);
        if(it != nbtx_req_handle_map.end()) {
            h = it->second;
            nbtx_req_handle_map.erase(it);
            h.end_transaction();
            nbtx_last_req_handle_map[rec_parts.id] = h;
        }
        h = nb_txRespHandle[rec_parts.get_command()]->begin_transaction();
        h.record_attribute("trans", tgd);
        h.add_relation(scv4tlm::rel_str(scv4tlm::PARENT_CHILD), rec_parts.parent);
        nbtx_resp_handle_map[rec_parts.id] = h;
        it = nbtx_last_req_handle_map.find(rec_parts.id);
        if(it != nbtx_last_req_handle_map.end()) {
            scv_tr_handle& pred = it->second;
            h.add_relation(scv4tlm::rel_str(scv4tlm::PREDECESSOR_SUCCESSOR), pred);
            nbtx_last_req_handle_map.erase(it);
        } else {
            it = nbtx_last_resp_handle_map.find(rec_parts.id);
            if(it != nbtx_last_resp_handle_map.end()) {
                scv_tr_handle& pred = it->second;
                h.add_relation(scv4tlm::rel_str(scv4tlm::PREDECESSOR_SUCCESSOR), pred);
                nbtx_last_resp_handle_map.erase(it);
            }
        }
    } else if(phase == tlm::END_RESP || phase == axi::END_PARTIAL_RESP) {
        auto it = nbtx_resp_handle_map.find(rec_parts.id);
        if(it != nbtx_resp_handle_map.end()) {
            h = it->second;
            nbtx_resp_handle_map.erase(it);
            h.end_transaction();
            if(phase == axi::END_PARTIAL_REQ) {
                nbtx_last_resp_handle_map[rec_parts.id] = h;
            }
        }
    } else
        sc_assert(!"phase not supported!");
    rec_parts.release();
    return;
}

template <typename TYPES> bool axi_recorder<TYPES>::get_direct_mem_ptr(typename TYPES::tlm_payload_type& trans, tlm::tlm_dmi& dmi_data) {
    if(!isRecordingEnabled()) {
        return get_fw_if()->get_direct_mem_ptr(trans, dmi_data);
    }
    if(!dmi_streamHandle)
        dmi_streamHandle = new scv_tr_stream((fixed_basename + "_dmi").c_str(), "TRANSACTOR", m_db);
    if(!dmi_trGetHandle)
        dmi_trGetHandle =
            new scv_tr_generator<scv4tlm::tlm_gp_data, scv4tlm::tlm_dmi_data>("get_dmi_ptr", *dmi_streamHandle, "trans", "dmi_data");
    scv_tr_handle h = dmi_trGetHandle->begin_transaction(scv4tlm::tlm_gp_data(trans));
    bool status = get_fw_if()->get_direct_mem_ptr(trans, dmi_data);
    dmi_trGetHandle->end_transaction(h, scv4tlm::tlm_dmi_data(dmi_data));
    return status;
}
/*! \brief The direct memory interface backward function
 *
 * This type of transaction is just forwarded and not recorded.
 * \param start_addr is the start address of the memory area being invalid
 * \param end_addr is the end address of the memory area being invalid
 */
template <typename TYPES> void axi_recorder<TYPES>::invalidate_direct_mem_ptr(sc_dt::uint64 start_addr, sc_dt::uint64 end_addr) {
    if(!isRecordingEnabled()) {
        get_bw_if()->invalidate_direct_mem_ptr(start_addr, end_addr);
        return;
    }
    if(!dmi_streamHandle)
        dmi_streamHandle = new scv_tr_stream((fixed_basename + "_dmi").c_str(), "TRANSACTOR", m_db);
    if(!dmi_trInvalidateHandle)
        dmi_trInvalidateHandle =
            new scv_tr_generator<sc_dt::uint64, sc_dt::uint64>("invalidate_dmi_ptr", *dmi_streamHandle, "start_delay", "end_delay");

    scv_tr_handle h = dmi_trInvalidateHandle->begin_transaction(start_addr);
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
template <typename TYPES> unsigned int axi_recorder<TYPES>::transport_dbg(typename TYPES::tlm_payload_type& trans) {
    unsigned int count = get_fw_if()->transport_dbg(trans);
    return count;
}
} // namespace scv4axi
