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
 * limitations under the License.axi_util.cpp
 */

#ifndef SC_INCLUDE_DYNAMIC_PROCESSES
#define SC_INCLUDE_DYNAMIC_PROCESSES
#endif

#include <axi/pe/axi_target_pe.h>
#include <axi/fsm/protocol_fsm.h>
#include <axi/fsm/types.h>
#include <scc/report.h>
#include <scc/utilities.h>
#include <systemc>
#include <tuple>

using namespace sc_core;
using namespace tlm;
using namespace axi;
using namespace axi::fsm;
using namespace axi::pe;

/******************************************************************************
 * target
 ******************************************************************************/
struct axi_target_pe::bw_intor_impl : public tlm::scc::pe::intor_bw_nb {
	axi_target_pe* const that;
	bw_intor_impl(axi_target_pe* that)
	: that(that) {}
	unsigned transport(tlm::tlm_generic_payload& payload) override {
		if((payload.is_read() && that->rd_resp_fifo.num_free())){
			that->rd_resp_fifo.write(&payload);
			return 0;
		} else if((payload.is_write() && that->wr_resp_fifo.num_free())){
			that->wr_resp_fifo.write(&payload);
			return 0;
		}
		return std::numeric_limits<unsigned>::max();
	}
};

SC_HAS_PROCESS(axi_target_pe);

axi_target_pe::axi_target_pe(const sc_core::sc_module_name& nm, size_t transfer_width, flavor_e flavor)
: sc_module(nm)
, base(transfer_width, (flavor != flavor_e::AXI)) // based on flavor, set the coherent flag of base
, bw_intor(new bw_intor_impl(this)) {
	instance_name = name();

	bw_i.bind(*bw_intor);

	SC_METHOD(fsm_clk_method);
	dont_initialize();
	sensitive << clk_i.pos();
	SC_METHOD(process_req2resp_fifos);
	dont_initialize();
	sensitive << clk_i.pos();
	SC_THREAD(start_wr_resp_thread);
	SC_THREAD(start_rd_resp_thread);
	SC_THREAD(send_wr_resp_beat_thread);
	SC_THREAD(send_rd_resp_beat_thread);
}

axi_target_pe::~axi_target_pe() = default;

void axi_target_pe::end_of_elaboration() {
	clk_if = dynamic_cast<sc_core::sc_clock*>(clk_i.get_interface());
}

void axi_target_pe::start_of_simulation() {
	if (!socket_bw)
		SCCFATAL(SCMOD) << "No backward interface registered!";
}

void axi_target_pe::b_transport(payload_type& trans, sc_time& t) {
	auto latency = operation_cb ? operation_cb(trans) : trans.is_read() ? get_cci_randomized_value(rd_resp_delay) : get_cci_randomized_value(wr_resp_delay);
	trans.set_dmi_allowed(false);
	trans.set_response_status(tlm::TLM_OK_RESPONSE);
	if(clk_if)
		t += clk_if->period() * latency;
}

tlm_sync_enum axi_target_pe::nb_transport_fw(payload_type& trans, phase_type& phase, sc_time& t) {
	fw_peq.notify(trans, phase, t);
	return tlm::TLM_ACCEPTED;
}

bool axi_target_pe::get_direct_mem_ptr(payload_type& trans, tlm_dmi& dmi_data) {
	trans.set_dmi_allowed(false);
	return false;
}

unsigned int axi_target_pe::transport_dbg(payload_type& trans) { return 0; }

fsm_handle* axi_target_pe::create_fsm_handle() { return new fsm_handle(); }

void axi_target_pe::setup_callbacks(fsm_handle* fsm_hndl) {
	fsm_hndl->fsm->cb[RequestPhaseBeg] = [this, fsm_hndl]() -> void {
		fsm_hndl->beat_count = 0;
		outstanding_cnt[fsm_hndl->trans->get_command()]++;
	};
	fsm_hndl->fsm->cb[BegPartReqE] = [this, fsm_hndl]() -> void {
		if(!fsm_hndl->beat_count && max_outstanding_tx.get_value() &&
				outstanding_cnt[fsm_hndl->trans->get_command()] > max_outstanding_tx.get_value()) {
			stalled_tx[fsm_hndl->trans->get_command()] = fsm_hndl->trans.get();
			stalled_tp[fsm_hndl->trans->get_command()] = EndPartReqE;
		} else { // accepted, schedule response
            if(!fsm_hndl->beat_count)
                getOutStandingTx(fsm_hndl->trans->get_command())++;
			if(auto delay = get_cci_randomized_value(wr_data_accept_delay))
				schedule(EndPartReqE, fsm_hndl->trans, delay - 1);
			else
				schedule(EndPartReqE, fsm_hndl->trans, sc_core::SC_ZERO_TIME);
		}
	};
	fsm_hndl->fsm->cb[EndPartReqE] = [this, fsm_hndl]() -> void {
		tlm::tlm_phase phase = axi::END_PARTIAL_REQ;
		sc_time t(clk_if ? ::scc::time_to_next_posedge(clk_if) - 1_ps : SC_ZERO_TIME);
		auto ret = socket_bw->nb_transport_bw(*fsm_hndl->trans, phase, t);
		fsm_hndl->beat_count++;
	};
	fsm_hndl->fsm->cb[BegReqE] = [this, fsm_hndl]() -> void {
		if(!fsm_hndl->beat_count && max_outstanding_tx.get_value() &&
				outstanding_cnt[fsm_hndl->trans->get_command()] > max_outstanding_tx.get_value()) {
			stalled_tx[fsm_hndl->trans->get_command()] = fsm_hndl->trans.get();
			stalled_tp[fsm_hndl->trans->get_command()] = EndReqE;
		} else { // accepted, schedule response
			if(!fsm_hndl->beat_count)
				getOutStandingTx(fsm_hndl->trans->get_command())++;
			auto latency = fsm_hndl->trans->is_read() ? get_cci_randomized_value(rd_addr_accept_delay) : get_cci_randomized_value(wr_data_accept_delay);
			if(latency)
				schedule(EndReqE, fsm_hndl->trans, latency - 1);
			else
				schedule(EndReqE, fsm_hndl->trans, sc_core::SC_ZERO_TIME);
		}
	};
	fsm_hndl->fsm->cb[EndReqE] = [this, fsm_hndl]() -> void {
		tlm::tlm_phase phase = tlm::END_REQ;
		sc_time t(clk_if ? ::scc::time_to_next_posedge(clk_if) - 1_ps : SC_ZERO_TIME);
		auto ret = socket_bw->nb_transport_bw(*fsm_hndl->trans, phase, t);
		fsm_hndl->trans->set_response_status(tlm::TLM_OK_RESPONSE);
		//it is better to move the set_resp in testcase
		if(auto ext3 = fsm_hndl->trans->get_extension<axi3_extension>()) {
			ext3->set_resp(resp_e::OKAY);
		} else if(auto ext4 = fsm_hndl->trans->get_extension<axi4_extension>()) {
			ext4->set_resp(resp_e::OKAY);
		} else if(auto exta = fsm_hndl->trans->get_extension<ace_extension>()) {
			exta->set_resp(resp_e::OKAY);
		} else
			sc_assert(false && "No valid AXITLM extension found!");
		if(fw_o.get_interface())
			fw_o->transport(*(fsm_hndl->trans));
		else {
			auto latency = operation_cb ? operation_cb(*fsm_hndl->trans)
					: fsm_hndl->trans->is_read() ? get_cci_randomized_value(rd_resp_delay) : get_cci_randomized_value(wr_resp_delay);
			if(latency < std::numeric_limits<unsigned>::max()) {
				if(fsm_hndl->trans->is_write())
					wr_req2resp_fifo.push_back(std::make_tuple(fsm_hndl->trans.get(), latency));
				else if(fsm_hndl->trans->is_read())
					rd_req2resp_fifo.push_back(std::make_tuple(fsm_hndl->trans.get(), latency));
			}
		}
	};
	fsm_hndl->fsm->cb[BegPartRespE] = [this, fsm_hndl]() -> void {
		// scheduling the response
		if(fsm_hndl->trans->is_read()) {
			if(!rd_resp_beat_fifo.nb_write(std::make_tuple(fsm_hndl, BegPartRespE)))
				SCCERR(SCMOD) << "too many outstanding transactions";
		} else if(fsm_hndl->trans->is_write()) {
			if(!wr_resp_beat_fifo.nb_write(std::make_tuple(fsm_hndl, BegPartRespE)))
				SCCERR(SCMOD) << "too many outstanding transactions";
		}
	};
	fsm_hndl->fsm->cb[EndPartRespE] = [this, fsm_hndl]() -> void {
		fsm_hndl->trans->is_read() ? rd_resp_ch.post() : wr_resp_ch.post();
		auto size = get_burst_length(*fsm_hndl->trans) - 1;
		fsm_hndl->beat_count++;
		SCCTRACE(SCMOD)<< " in EndPartialResp with beat_count = " << fsm_hndl->beat_count << " expected size = " << size;
		if(rd_data_beat_delay.get_value())
			schedule(fsm_hndl->beat_count < size ? BegPartRespE : BegRespE, fsm_hndl->trans, get_cci_randomized_value(rd_data_beat_delay));
		else
			schedule(fsm_hndl->beat_count < size ? BegPartRespE : BegRespE, fsm_hndl->trans, SC_ZERO_TIME, true);
	};
	fsm_hndl->fsm->cb[BegRespE] = [this, fsm_hndl]() -> void {
		// scheduling the response
		if(fsm_hndl->trans->is_read()) {
			if(!rd_resp_beat_fifo.nb_write(std::make_tuple(fsm_hndl, BegRespE)))
				SCCERR(SCMOD) << "too many outstanding transactions";
		} else if(fsm_hndl->trans->is_write()) {
			if(!wr_resp_beat_fifo.nb_write(std::make_tuple(fsm_hndl, BegRespE)))
				SCCERR(SCMOD) << "too many outstanding transactions";
		}
	};
	fsm_hndl->fsm->cb[EndRespE] = [this, fsm_hndl]() -> void {
		fsm_hndl->trans->is_read() ? rd_resp_ch.post()   : wr_resp_ch.post();
		if(rd_resp.get_value() < rd_resp.get_capacity()) {
			SCCTRACE(SCMOD) << "finishing exclusive read response for trans " << *fsm_hndl->trans;
			rd_resp.post();
		}
		auto cmd = fsm_hndl->trans->get_command();
		outstanding_cnt[cmd]--;
		getOutStandingTx(cmd)--;
		if(cmd == tlm::TLM_READ_COMMAND)
			active_rdresp_id.erase(axi::get_axi_id(fsm_hndl->trans.get()));
		if(stalled_tx[cmd]) {
			auto* trans = stalled_tx[cmd];
			auto latency = trans->is_read() ? get_cci_randomized_value(rd_addr_accept_delay) : get_cci_randomized_value(wr_data_accept_delay);
			if(latency)
				schedule(stalled_tp[cmd], trans, latency - 1);
			else
				schedule(stalled_tp[cmd], trans, sc_core::SC_ZERO_TIME);
			stalled_tx[cmd] = nullptr;
			stalled_tp[cmd] = CB_CNT;
		}
	};
}

void axi::pe::axi_target_pe::operation_resp(payload_type& trans, unsigned clk_delay) {
	if(trans.is_write())
		wr_req2resp_fifo.push_back(std::make_tuple(&trans, clk_delay));
	else if(trans.is_read())
		rd_req2resp_fifo.push_back(std::make_tuple(&trans, clk_delay));
}

void axi::pe::axi_target_pe::process_req2resp_fifos() {
    while(!rd_req2resp_fifo.empty()) {
		auto& entry = rd_req2resp_fifo.front();
		if(std::get<1>(entry) == 0) {
			if(!rd_resp_fifo.nb_write(std::get<0>(entry)))
				rd_req2resp_fifo.push_back(entry);
			rd_req2resp_fifo.pop_front();
		} else {
			std::get<1>(entry) -= 1;
			rd_req2resp_fifo.push_back(entry);
			rd_req2resp_fifo.pop_front();
		}
	}
    while(!wr_req2resp_fifo.empty()) {
		auto& entry = wr_req2resp_fifo.front();
		if(std::get<1>(entry) == 0) {
			if(!wr_resp_fifo.nb_write(std::get<0>(entry)))
				wr_req2resp_fifo.push_back(entry);
			wr_req2resp_fifo.pop_front();
		} else {
			std::get<1>(entry) -= 1;
			wr_req2resp_fifo.push_back(entry);
			wr_req2resp_fifo.pop_front();
		}
	}
}

void axi::pe::axi_target_pe::start_rd_resp_thread() {
	auto residual_clocks = 0.0;
	while(true) {
		auto* trans = rd_resp_fifo.read();
		if(!rd_data_interleaving.get_value() || rd_data_beat_delay.get_value() == 0) {
			while(!rd_resp.get_value())
				wait(clk_i.posedge_event());
			rd_resp.wait();
		}
		SCCTRACE(SCMOD) << __FUNCTION__ << " starting exclusive read response for trans " << *trans;
		auto e = axi::get_burst_length(trans) == 1 || trans->is_write() ? axi::fsm::BegRespE : BegPartRespE;
		auto id = axi::get_axi_id(trans);
		while(active_rdresp_id.size() && active_rdresp_id.find(id) != active_rdresp_id.end()) {
			wait(clk_i.posedge_event());
		}
		active_rdresp_id.insert(id);
		if(auto delay = get_cci_randomized_value(rd_data_beat_delay))
			schedule(e, trans, delay-1U);
		else
			schedule(e, trans, SC_ZERO_TIME);
	}
}

void axi::pe::axi_target_pe::start_wr_resp_thread() {
	auto residual_clocks = 0.0;
	while(true) {
		auto* trans = wr_resp_fifo.read();
		schedule(axi::fsm::BegRespE, trans, SC_ZERO_TIME);
	}
}

void axi::pe::axi_target_pe::send_rd_resp_beat_thread() {
	std::tuple<fsm::fsm_handle*, axi::fsm::protocol_time_point_e> entry;
	while(true) {
		// waiting for responses to send, which is notifed in Begin_Partial_Resp
		wait(rd_resp_beat_fifo.data_written_event());
		while(rd_resp_beat_fifo.nb_read(entry)) {
			// there is something to send
			auto fsm_hndl = std::get<0>(entry);
			auto tp = std::get<1>(entry);
			sc_time t;
			tlm::tlm_phase phase{axi::BEGIN_PARTIAL_RESP};
			if(tp == BegRespE)
			    phase= tlm::BEGIN_RESP;
			// wait to get ownership of the response channel
			while(!rd_resp_ch.get_value())
				wait(clk_i.posedge_event());
			rd_resp_ch.wait();
			SCCTRACE(SCMOD) << __FUNCTION__ << " starting exclusive read response for trans " << *fsm_hndl->trans;
			if(socket_bw->nb_transport_bw(*fsm_hndl->trans, phase, t) == tlm::TLM_UPDATED) {
				schedule(phase == tlm::END_RESP ? EndRespE : EndPartRespE, fsm_hndl->trans, 0);
			}
		}
	}
}

void axi::pe::axi_target_pe::send_wr_resp_beat_thread() {
	std::tuple<fsm::fsm_handle*, axi::fsm::protocol_time_point_e> entry;
	while(true) {
		// waiting for responses to send
		wait(wr_resp_beat_fifo.data_written_event());
		while(wr_resp_beat_fifo.nb_read(entry)) {
			// there is something to send
			auto fsm_hndl = std::get<0>(entry);
			sc_time t;
			tlm::tlm_phase phase{tlm::tlm_phase(tlm::BEGIN_RESP)};
			// wait to get ownership of the response channel
			wr_resp_ch.wait();
			if(socket_bw->nb_transport_bw(*fsm_hndl->trans, phase, t) == tlm::TLM_UPDATED) {
				schedule(phase == tlm::END_RESP ? EndRespE : EndPartRespE, fsm_hndl->trans, 0);
			}
		}
	}
}
