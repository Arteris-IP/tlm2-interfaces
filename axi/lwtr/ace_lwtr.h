/*
 * Copyright 2023 Arteris IP
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

#include <tlm/scc/lwtr/tlm2_lwtr.h>
#include <cci_configuration>
#include <axi/checker/axi_protocol.h>
#include <tlm/scc/tlm_mm.h>
#include <array>
#include <axi/axi_tlm.h>
#include <regex>
#include <string>
#include <scc/peq.h>
#include <unordered_map>
#include <vector>

//! @brief LWTR components for AXI/ACE
namespace axi {
namespace lwtr {

using tx_db = ::lwtr::tx_db;
using tx_fiber = ::lwtr::tx_fiber;
template <typename BEGIN = ::lwtr::no_data, typename END = ::lwtr::no_data>
using tx_generator = ::lwtr::tx_generator<BEGIN, END>;
using tx_handle = ::lwtr::tx_handle;
using mm = tlm::scc::tlm_mm<>;
using namespace tlm::scc::lwtr;

struct nb_ace_rec_entry: public nb_rec_entry {
	bool snoop;
	nb_ace_rec_entry(tlm::scc::tlm_gp_shared_ptr tr,tlm::tlm_phase const ph, uintptr_t const id, tx_handle parent, bool snoop)
	: nb_rec_entry{tr, ph, id, parent}
		, snoop(snoop) {}
};
extern bool registered;

/*! \brief The TLM2 transaction recorder
 *
 * This module records all TLM transaction to a LWTR transaction fiber for
 * further viewing and analysis.
 * The handle of the created transaction is stored in an tlm_extension so that
 * another instance of the ace_lwtr
 * e.g. further down the path can link to it.
 */
template <typename TYPES = axi::axi_protocol_types>
class ace_lwtr : public virtual axi::ace_fw_transport_if<TYPES>, public virtual axi::ace_bw_transport_if<TYPES> {
public:
	//! \brief the attribute to selectively enable/disable recording of blocking protocol tx
	cci::cci_param<bool> enableBlTracing;

	//! \brief the attribute to selectively enable/disable recording of non-blocking protocol tx
	cci::cci_param<bool> enableNbTracing;

	//! \brief the attribute to selectively enable/disable timed recording
	cci::cci_param<bool> enableTimedTracing{"enableTimedTracing", true};

	//! \brief the attribute to selectively enable/disable DMI recording
	cci::cci_param<bool> enableDmiTracing{"enableDmiTracing", false};

	/*! \brief The constructor of the component
	 *
	 * \param name is the SystemC module name of the recorder
	 * \param tr_db is a pointer to a transaction recording database. If none is
	 * provided the default one is retrieved.
	 *        If this database is not initialized (e.g. by not calling
	 * lwtr::set_default_db() ) recording is disabled.
	 */
	ace_lwtr(char const* full_name, unsigned bus_width, bool recording_enabled = true, tx_db* tr_db = tx_db::get_default_db())
	: enableBlTracing("enableBlTracing", recording_enabled)
	, enableNbTracing("enableNbTracing", recording_enabled)
	, full_name(full_name)
	, nb_timed_peq()
	, bus_width(bus_width)
	, m_db(tr_db)
	{
		sc_core::sc_spawn_options opts;
		opts.spawn_method();
		opts.dont_initialize();
		opts.set_sensitivity(&nb_timed_peq.event());
		sc_core::sc_spawn([this](){
			nbtx_cb();
		}, nullptr, &opts);
		initialize_streams();
	}

	virtual ~ace_lwtr() override {
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
			delete checker;
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

protected:
	//! \brief the port where fw accesses are forwarded to
	sc_core::sc_port<axi::ace_fw_transport_if<TYPES>> fw_port{"fw_port"};

	//! \brief the port where bw accesses are forwarded to
	sc_core::sc_port<axi::ace_bw_transport_if<TYPES>> bw_port{"bw_port"};

private:
	std::string const full_name;
	//! event queue to hold time points of non-blocking transactions
	::scc::peq<nb_ace_rec_entry> nb_timed_peq;
	/*! \brief The thread processing the non-blocking requests with their
	 * annotated times
	 * to generate the timed view of non-blocking tx
	 */
	void nbtx_cb();
	const unsigned bus_width{0};
	//! transaction recording database
	tx_db* m_db{nullptr};
	//! the relationship name handles
	::lwtr::tx_relation_handle pred_succ_hndl{0}, par_chld_hndl{0};
	//! blocking transaction recording stream handle
	tx_fiber* b_streamHandle{nullptr};
	//! transaction generator handle for blocking transactions
	std::array<tx_generator<sc_core::sc_time, sc_core::sc_time>*, 3> b_trHandle{{nullptr, nullptr, nullptr}};
	//! timed blocking transaction recording stream handle
	tx_fiber* b_streamHandleTimed{nullptr};
	//! transaction generator handle for blocking transactions with annotated
	//! delays
	std::array<tx_generator<>*, 3> b_trTimedHandle{{nullptr, nullptr, nullptr}};

    enum DIR { FW, BW, REQ = FW, RESP = BW, ACK };
	//! non-blocking transaction recording stream handle
	tx_fiber* nb_streamHandle{nullptr};
	//! non-blocking transaction recording stream handle with timing
	tx_fiber* nb_streamHandleTimed{nullptr};
	//! transaction generator handle for non-blocking transactions
	std::array<tx_generator<std::string, std::string>*, 3> nb_trHandle{{nullptr, nullptr, nullptr}};
	//! transaction generator handle for non-blocking transactions with annotated delays
	std::array<tx_generator<>*, 3> nb_trTimedHandle{{nullptr, nullptr, nullptr}};
	std::unordered_map<uint64_t, tx_handle> nbtx_req_handle_map;
	std::unordered_map<uint64_t, tx_handle> nbtx_last_req_handle_map;
	std::unordered_map<uint64_t, tx_handle> nbtx_resp_handle_map;
	std::unordered_map<uint64_t, tx_handle> nbtx_last_resp_handle_map;
	//! dmi transaction recording stream handle
	tx_fiber* dmi_streamHandle{nullptr};
	//! transaction generator handle for DMI transactions
	tx_generator<>* dmi_trGetHandle{nullptr};
	tx_generator<sc_dt::uint64, sc_dt::uint64>* dmi_trInvalidateHandle{nullptr};

protected:
	void initialize_streams() {
		if(m_db) {
			pred_succ_hndl = m_db->create_relation("PREDECESSOR_SUCCESSOR");
			par_chld_hndl = m_db->create_relation("PARENT_CHILD");
		}
		if(isRecordingBlockingTxEnabled() && !b_streamHandle) {
			b_streamHandle = new tx_fiber((full_name + "_bl").c_str(), "[TLM][ace][b]", m_db);
			b_trHandle[tlm::TLM_READ_COMMAND] = new tx_generator<sc_core::sc_time, sc_core::sc_time>(
					"read", *b_streamHandle, "start_delay", "end_delay");
			b_trHandle[tlm::TLM_WRITE_COMMAND] = new tx_generator<sc_core::sc_time, sc_core::sc_time>(
					"write", *b_streamHandle, "start_delay", "end_delay");
			b_trHandle[tlm::TLM_IGNORE_COMMAND] = new tx_generator<sc_core::sc_time, sc_core::sc_time>(
					"ignore", *b_streamHandle, "start_delay", "end_delay");
			if(enableTimedTracing.get_value()) {
				b_streamHandleTimed =
						new tx_fiber((full_name + "_bl_timed").c_str(), "[TLM][ace][b][timed]", m_db);
				b_trTimedHandle[tlm::TLM_READ_COMMAND] = new tx_generator<>("read", *b_streamHandleTimed);
				b_trTimedHandle[tlm::TLM_WRITE_COMMAND] = new tx_generator<>("write", *b_streamHandleTimed);
				b_trTimedHandle[tlm::TLM_IGNORE_COMMAND] = new tx_generator<>("ignore", *b_streamHandleTimed);
			}
		}
		if(isRecordingNonBlockingTxEnabled() && !nb_streamHandle) {
			nb_streamHandle = new tx_fiber((full_name + "_nb").c_str(), "[TLM][ace][nb]", m_db);
			nb_trHandle[FW] = new tx_generator<std::string, std::string>("fw", *nb_streamHandle, "tlm_phase",
					"tlm_phase[return_path]");
			nb_trHandle[BW] = new tx_generator<std::string, std::string>("bw", *nb_streamHandle, "tlm_phase",
					"tlm_phase[return_path]");
			if(enableTimedTracing.get_value()) {
				nb_streamHandleTimed =
						new tx_fiber((full_name + "_nb_timed").c_str(), "[TLM][ace][nb][timed]", m_db);
				nb_trTimedHandle[FW] = new tx_generator<>("request", *nb_streamHandleTimed);
				nb_trTimedHandle[BW] = new tx_generator<>("response", *nb_streamHandleTimed);
                nb_trTimedHandle[ACK] = new tx_generator<>("ack", *nb_streamHandleTimed);
			}
		}
		if(m_db && enableDmiTracing.get_value() && !dmi_streamHandle) {
			dmi_streamHandle = new tx_fiber((full_name + "_dmi").c_str(), "[TLM][ace][dmi]", m_db);
			dmi_trGetHandle = new tx_generator<>("get", *dmi_streamHandle);
			dmi_trInvalidateHandle = new tx_generator<sc_dt::uint64, sc_dt::uint64>(
					"invalidate", *dmi_streamHandle, "start_addr", "end_addr");
		}
	}

private:
	axi::checker::checker_if<TYPES>* checker{nullptr};
};

template <unsigned BUSWIDTH=32, typename TYPES = axi::axi_protocol_types, int N = 1,
        sc_core::sc_port_policy POL = sc_core::SC_ONE_OR_MORE_BOUND>
class ace_lwtr_recorder : public sc_core::sc_module, public ace_lwtr<TYPES> {
public:
	axi::ace_target_socket<BUSWIDTH, TYPES, N, POL> ts{"ts"};
	axi::ace_initiator_socket<BUSWIDTH, TYPES, N, POL> is{"is"};

	ace_lwtr_recorder(sc_core::sc_module_name nm, bool recording_enabled = true, tx_db* tr_db = tx_db::get_default_db())
	: sc_core::sc_module(nm)
	, ace_lwtr<TYPES>(name(), BUSWIDTH, recording_enabled, tr_db)
	{
 		is(*this);
 		ts(*this);
 		this->bw_port(ts.get_base_port());
 		this->fw_port(is.get_base_port());
	}
};
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// implementations of functions
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

template <typename TYPES>
void ace_lwtr<TYPES>::b_transport(typename TYPES::tlm_payload_type& trans, sc_core::sc_time& delay) {
	if(!isRecordingBlockingTxEnabled()) {
		fw_port->b_transport(trans, delay);
		return;
	}
	// Get a handle for the new transaction
	tx_handle h = b_trHandle[trans.get_command()]->begin_tx(delay);
	tx_handle htim;
	/*************************************************************************
	 * do the timed notification
	 *************************************************************************/
	if(b_streamHandleTimed)
		htim = b_trTimedHandle[trans.get_command()]->begin_tx_delayed(sc_core::sc_time_stamp()+delay, par_chld_hndl, h);

	if(registered)
		for(auto& extensionRecording : lwtr4tlm2_extension_registry<TYPES>::inst().get())
			if(extensionRecording) {
				extensionRecording->recordBeginTx(h, trans);
				if(htim.is_valid())
					extensionRecording->recordBeginTx(htim, trans);
			}
	link_pred_ext* preExt = nullptr;

	trans.get_extension(preExt);
	if(preExt == nullptr) { // we are the first recording this transaction
		preExt = new link_pred_ext(h, this);
		if(trans.has_mm())
			trans.set_auto_extension(preExt);
		else
			trans.set_extension(preExt);
	} else {
		h.add_relation(pred_succ_hndl, preExt->txHandle);
	}
	tx_handle preTx(preExt->txHandle);
	preExt->txHandle = h;
	fw_port->b_transport(trans, delay);
	trans.get_extension(preExt);
	if(preExt->creator == this) {
		// clean-up the extension if this is the original creator
		trans.set_extension(static_cast<link_pred_ext*>(nullptr));
		if(!trans.has_mm()) {
			delete preExt;
		}
	} else {
		preExt->txHandle = preTx;
	}

	h.record_attribute("trans", trans);
	if(registered)
		for(auto& extensionRecording : lwtr4tlm2_extension_registry<TYPES>::inst().get())
			if(extensionRecording) {
				extensionRecording->recordEndTx(h, trans);
				if(htim.is_active())
					extensionRecording->recordEndTx(htim, trans);
			}
	// End the transaction
	h.end_tx(delay);
	// and now the stuff for the timed tx
	if(htim.is_valid()) {
		htim.record_attribute("trans", trans);
		htim.end_tx_delayed(sc_core::sc_time_stamp()+delay);
	}
}

template <typename TYPES>
void ace_lwtr<TYPES>::b_snoop(typename TYPES::tlm_payload_type& trans, sc_core::sc_time& delay) {
    if(!b_streamHandleTimed) {
    	bw_port->b_snoop(trans, delay);
        return;
    }
    // Get a handle for the new transaction
    tx_handle h = b_trHandle[trans.get_command()]->begin_tx(delay);
    tx_handle htim;
    /*************************************************************************
     * do the timed notification
     *************************************************************************/
	if(b_streamHandleTimed)
		htim = b_trTimedHandle[trans.get_command()]->begin_tx_delayed(sc_core::sc_time_stamp()+delay, par_chld_hndl, h);

	if(registered)
		for(auto& extensionRecording : lwtr4tlm2_extension_registry<TYPES>::inst().get())
			if(extensionRecording) {
				extensionRecording->recordBeginTx(h, trans);
				if(htim.is_valid())
					extensionRecording->recordBeginTx(htim, trans);
			}
	link_pred_ext* preExt = nullptr;

    trans.get_extension(preExt);
    if(preExt == nullptr) { // we are the first recording this transaction
        preExt = new link_pred_ext(h, this);
        trans.set_extension(preExt);
    } else {
        h.add_relation(pred_succ_hndl, preExt->txHandle);
    }
    tx_handle preTx(preExt->txHandle);
    preExt->txHandle = h;
    bw_port->b_snoop(trans, delay);
    trans.get_extension(preExt);
    if(preExt->creator == this) {
        // clean-up the extension if this is the original creator
        trans.set_extension(static_cast<link_pred_ext*>(nullptr));
		if(!trans.has_mm())
			delete preExt;
    } else {
        preExt->txHandle = preTx;
    }

	h.record_attribute("trans", trans);
	if(registered)
		for(auto& extensionRecording : lwtr4tlm2_extension_registry<TYPES>::inst().get())
			if(extensionRecording) {
				extensionRecording->recordEndTx(h, trans);
				if(htim.is_active())
					extensionRecording->recordEndTx(htim, trans);
			}
    // End the transaction
    h.end_tx(delay);
    // and now the stuff for the timed tx
	if(htim.is_valid()) {
		htim.record_attribute("trans", trans);
		htim.end_tx_delayed(sc_core::sc_time_stamp()+delay);
	}
}

template <typename TYPES>
tlm::tlm_sync_enum ace_lwtr<TYPES>::nb_transport_fw(typename TYPES::tlm_payload_type& trans,
		typename TYPES::tlm_phase_type& phase,
		sc_core::sc_time& delay) {
	if(!isRecordingNonBlockingTxEnabled()){
			return fw_port->nb_transport_fw(trans, phase, delay);
	}
	/*************************************************************************
	 * prepare recording
	 *************************************************************************/
	// Get a handle for the new transaction
	tx_handle h = nb_trHandle[FW]->begin_tx(phase.get_name());
	link_pred_ext* preExt = nullptr;
	trans.get_extension(preExt);
	if((phase == axi::BEGIN_PARTIAL_REQ || phase == tlm::BEGIN_REQ) && preExt == nullptr) { // we are the first recording this transaction
		preExt = new link_pred_ext(h, this);
		if(trans.has_mm())
			trans.set_auto_extension(preExt);
		else
			trans.set_extension(preExt);
	} else if(preExt != nullptr) {
		// link handle if we have a predecessor
		h.add_relation(pred_succ_hndl, preExt->txHandle);
	} else {
		sc_assert(preExt != nullptr && "ERROR on forward path in phase other than tlm::BEGIN_REQ");
	}
	// update the extension
	preExt->txHandle = h;
	h.record_attribute("delay", delay);
	if(registered)
		for(auto& extensionRecording : lwtr4tlm2_extension_registry<TYPES>::inst().get())
			if(extensionRecording)
				extensionRecording->recordBeginTx(h, trans);
	/*************************************************************************
	 * do the timed notification
	 *************************************************************************/
	if(nb_streamHandleTimed) {
		nb_ace_rec_entry rec(mm::get().allocate(), phase, reinterpret_cast<uint64_t>(&trans), h, (phase == tlm::BEGIN_RESP || phase == axi::BEGIN_PARTIAL_RESP));
		rec.tr->deep_copy_from(trans);
		nb_timed_peq.notify(rec, delay);
	}
	/*************************************************************************
	 * do the access
	 *************************************************************************/
	if(checker) checker->fw_pre(trans, phase);
	tlm::tlm_sync_enum status = fw_port->nb_transport_fw(trans, phase, delay);
	if(checker) checker->fw_post(trans, phase, status);
	/*************************************************************************
	 * handle recording
	 *************************************************************************/
	h.record_attribute("tlm_sync", status);
	h.record_attribute("delay[return_path]", delay);
	h.record_attribute("trans", trans);
	if(registered)
		for(auto& extensionRecording : lwtr4tlm2_extension_registry<TYPES>::inst().get())
			if(extensionRecording)
				extensionRecording->recordEndTx(h, trans);
	// get the extension and free the memory if it was mine
    if(status == tlm::TLM_COMPLETED || (phase == axi::ACK)) {
		trans.get_extension(preExt);
		if(preExt && preExt->creator == this) {
			trans.set_extension(static_cast<link_pred_ext*>(nullptr));
			if(!trans.has_mm()) {
				delete preExt;
			}
		}
		/*************************************************************************
		 * do the timed notification if req. finished here
		 *************************************************************************/
		if(nb_streamHandleTimed) {
			nb_ace_rec_entry rec {mm::get().allocate(), phase, reinterpret_cast<uint64_t>(&trans), h, (phase == tlm::BEGIN_RESP || phase == axi::BEGIN_PARTIAL_RESP)};
			rec.tr->deep_copy_from(trans);
			nb_timed_peq.notify(rec, delay);
		}
	} else if(nb_streamHandleTimed && status == tlm::TLM_UPDATED) {
		nb_ace_rec_entry rec {mm::get().allocate(), phase, reinterpret_cast<uint64_t>(&trans), h, (phase == tlm::BEGIN_RESP || phase == axi::BEGIN_PARTIAL_RESP)};
		rec.tr->deep_copy_from(trans);
		nb_timed_peq.notify(rec, delay);
	}
	// End the transaction
	nb_trHandle[FW]->end_tx(h, phase.get_name());
	return status;
}

template <typename TYPES>
tlm::tlm_sync_enum ace_lwtr<TYPES>::nb_transport_bw(typename TYPES::tlm_payload_type& trans,
		typename TYPES::tlm_phase_type& phase,
		sc_core::sc_time& delay) {
	if(!isRecordingNonBlockingTxEnabled()){
		if(checker) {
			checker->bw_pre(trans, phase);
			tlm::tlm_sync_enum status = bw_port->nb_transport_bw(trans, phase, delay);
			checker->bw_post(trans, phase, status);
			return status;
		} else
			return bw_port->nb_transport_bw(trans, phase, delay);
	}
	/*************************************************************************
	 * prepare recording
	 *************************************************************************/
	link_pred_ext* preExt = nullptr;
	trans.get_extension(preExt);
	// Get a handle for the new transaction
	tx_handle h = nb_trHandle[BW]->begin_tx(phase.get_name());
	// link handle if we have a predecessor and that's not ourself
	if(phase == tlm::BEGIN_REQ && preExt == nullptr) { // we are the first recording this transaction
		preExt = new link_pred_ext(h, this);
		trans.set_extension(preExt);
		preExt->txHandle = h;
	} else if(preExt) {
		// link handle if we have a predecessor
		h.add_relation(pred_succ_hndl, preExt->txHandle);
		// and set the extension handle to this transaction
		preExt->txHandle = h;
	} else {
		sc_assert(preExt != nullptr && "ERROR on backward path in phase other than tlm::BEGIN_REQ");
	}
	// and set the extension handle to this transaction
	h.record_attribute("delay", delay);
	for(auto& extensionRecording : lwtr4tlm2_extension_registry<TYPES>::inst().get())
		if(extensionRecording)
			extensionRecording->recordBeginTx(h, trans);
	/*************************************************************************
	 * do the timed notification
	 *************************************************************************/
	if(nb_streamHandleTimed) {
		nb_ace_rec_entry rec {mm::get().allocate(), phase, reinterpret_cast<uint64_t>(&trans), h, phase == tlm::BEGIN_REQ};
		rec.tr->deep_copy_from(trans);
		nb_timed_peq.notify(rec, delay);
	}
	/*************************************************************************
	 * do the access
	 *************************************************************************/
	if(checker) checker->bw_pre(trans, phase);
	tlm::tlm_sync_enum status = bw_port->nb_transport_bw(trans, phase, delay);
	if(checker) checker->bw_post(trans, phase, status);
	/*************************************************************************
	 * handle recording
	 *************************************************************************/
	h.record_attribute("tlm_sync", status);
	h.record_attribute("delay[return_path]", delay);
	h.record_attribute("trans", trans);
	if(registered)
		for(auto& extensionRecording : lwtr4tlm2_extension_registry<TYPES>::inst().get())
			if(extensionRecording)
				extensionRecording->recordEndTx(h, trans);
	// End the transaction
	nb_trHandle[BW]->end_tx(h, phase.get_name());
	// get the extension and free the memory if it was mine
	if(status == tlm::TLM_COMPLETED || (status == tlm::TLM_UPDATED && phase == axi::ACK)) {
		// the transaction is finished
		if(preExt && preExt->creator == this) {
			// clean-up the extension if this is the original creator
			trans.set_extension(static_cast<link_pred_ext*>(nullptr));
			if(!trans.has_mm()) {
				delete preExt;
			}
		}
		/*************************************************************************
		 * do the timed notification if req. finished here
		 *************************************************************************/
		if(nb_streamHandleTimed) {
			nb_ace_rec_entry rec {mm::get().allocate(), phase, reinterpret_cast<uint64_t>(&trans), h, phase == tlm::BEGIN_REQ};
			rec.tr->deep_copy_from(trans);
			nb_timed_peq.notify(rec, delay);
		}
	} else if(nb_streamHandleTimed && status == tlm::TLM_UPDATED) {
		nb_ace_rec_entry rec {mm::get().allocate(), phase, reinterpret_cast<uint64_t>(&trans), h, phase == tlm::BEGIN_REQ};
		rec.tr->deep_copy_from(trans);
		nb_timed_peq.notify(rec, delay);
	}
	return status;
}

template <typename TYPES>
void ace_lwtr<TYPES>::nbtx_cb() {
	auto opt = nb_timed_peq.get_next();
	if(opt) {
		auto& e = opt.get();
		tx_handle h;
		// Now process outstanding recordings
		if(e.ph == tlm::BEGIN_REQ || e.ph == axi::BEGIN_PARTIAL_REQ) {
			h = nb_trTimedHandle[REQ]->begin_tx(par_chld_hndl, e.parent);
			nbtx_req_handle_map[e.id] = h;
		} else if(e.ph == tlm::END_REQ || e.ph == axi::END_PARTIAL_REQ) {
			auto it = nbtx_req_handle_map.find(e.id);
			if(it != nbtx_req_handle_map.end()) {
				h = it->second;
				nbtx_req_handle_map.erase(it);
				h.record_attribute("trans", *e.tr);
				h.end_tx();
				nbtx_last_req_handle_map[e.id] = h;
			}
		} else if(e.ph == tlm::BEGIN_RESP || e.ph == axi::BEGIN_PARTIAL_RESP) {
			auto it = nbtx_req_handle_map.find(e.id);
			if(it != nbtx_req_handle_map.end()) {
				h = it->second;
				nbtx_req_handle_map.erase(it);
				h.record_attribute("trans", *e.tr);
				h.end_tx();
				nbtx_last_req_handle_map[e.id] = h;
			}
			h = nb_trTimedHandle[RESP]->begin_tx(par_chld_hndl, e.parent);
			nbtx_resp_handle_map[e.id] = h;
			it = nbtx_last_req_handle_map.find(e.id);
			if(it != nbtx_last_req_handle_map.end()) {
				tx_handle& pred = it->second;
				h.add_relation(pred_succ_hndl, pred);
				nbtx_last_req_handle_map.erase(it);
			} else {
				it = nbtx_last_resp_handle_map.find(e.id);
				if(it != nbtx_last_resp_handle_map.end()) {
					tx_handle& pred = it->second;
					h.add_relation(pred_succ_hndl, pred);
					nbtx_last_resp_handle_map.erase(it);
				}
			}
		} else if(e.ph == tlm::END_RESP || e.ph == axi::END_PARTIAL_RESP) {
			auto it = nbtx_resp_handle_map.find(e.id);
			if(it != nbtx_resp_handle_map.end()) {
				h = it->second;
				nbtx_resp_handle_map.erase(it);
				h.record_attribute("trans", *e.tr);
				h.end_tx();
				if(e.ph == axi::END_PARTIAL_RESP) {
					nbtx_last_resp_handle_map[e.id] = h;
				}
			}
	    } else if(e.ph == axi::ACK) {
			h = nb_trTimedHandle[ACK]->begin_tx(par_chld_hndl, e.parent);
			h.record_attribute("trans", *e.tr);
	        h.end_tx();
		} else
			sc_assert(!"phase not supported!");
	}
	return;
}

template <typename TYPES>
bool ace_lwtr<TYPES>::get_direct_mem_ptr(typename TYPES::tlm_payload_type& trans, tlm::tlm_dmi& dmi_data) {
	if(!(m_db && enableDmiTracing.get_value()))
		return fw_port->get_direct_mem_ptr(trans, dmi_data);
	tx_handle h = dmi_trGetHandle->begin_tx();
	bool status = fw_port->get_direct_mem_ptr(trans, dmi_data);
	h.record_attribute("trans", trans);
	h.record_attribute("dmi_data", dmi_data);
	h.end_tx();
	return status;
}
/*! \brief The direct memory interface backward function
 *
 * This type of transaction is just forwarded and not recorded.
 * \param start_addr is the start address of the memory area being invalid
 * \param end_addr is the end address of the memory area being invalid
 */
template <typename TYPES>
void ace_lwtr<TYPES>::invalidate_direct_mem_ptr(sc_dt::uint64 start_addr, sc_dt::uint64 end_addr) {
	if(!(m_db && enableDmiTracing.get_value())) {
		bw_port->invalidate_direct_mem_ptr(start_addr, end_addr);
		return;
	}
	tx_handle h = dmi_trInvalidateHandle->begin_tx(start_addr);
	bw_port->invalidate_direct_mem_ptr(start_addr, end_addr);
	dmi_trInvalidateHandle->end_tx(h, end_addr);
	return;
}
/*! \brief The debug transportfunction
 *
 * This type of transaction is just forwarded and not recorded.
 * \param trans is the generic payload of the transaction
 * \return the sync state of the transaction
 */
template <typename TYPES> unsigned int ace_lwtr<TYPES>::transport_dbg(typename TYPES::tlm_payload_type& trans) {
	return fw_port->transport_dbg(trans);
}
} // namespace lwtr
} // namespace axi
