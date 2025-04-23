#include "replay_target.h"
#include <fstream>
#include <yaml-cpp/exceptions.h>
#include <yaml-cpp/node/parse.h>
#include <yaml-cpp/yaml.h>

namespace axi {
namespace pe {

replay_buffer::replay_buffer(const sc_core::sc_module_name& nm)
: sc_core::sc_module(nm) {
    fw_i.bind(*this);
#if SYSTEMC_VERSION < 20250221
    SC_HAS_PROCESS(replay_buffer);
#endif
    SC_METHOD(process_req2resp_fifos);
    dont_initialize();
    sensitive << clk_i.pos();
    SC_THREAD(start_wr_resp_thread);
    SC_THREAD(start_rd_resp_thread);
}

void replay_buffer::end_of_elaboration() { clk_if = dynamic_cast<sc_core::sc_clock*>(clk_i.get_interface()); }

void replay_buffer::start_of_simulation() {
    if(replay_file_name.get_value().length()) {
        std::ifstream ifs(replay_file_name.get_value());
        if(ifs.is_open()) {
            std::string line;
            std::getline(ifs, line);
            while(std::getline(ifs, line)) {
                auto token = util::split(line, ',');
                auto addr = strtoull(token[1].c_str(), nullptr, 10);
                auto id = strtoull(token[2].c_str(), nullptr, 10);
                auto start_cycle = strtoull(token[3].c_str(), nullptr, 10);
                auto req_resp_lat = strtoull(token[4].c_str(), nullptr, 10);
                if(token[0] == "READ") {
                    if(id >= rd_sequence.size())
                        rd_sequence.resize(id + 1);
                    rd_sequence[id].emplace_back(addr, req_resp_lat);
                } else if(token[0] == "WRITE") {
                    if(id >= wr_sequence.size())
                        wr_sequence.resize(id + 1);
                    wr_sequence[id].emplace_back(addr, req_resp_lat);
                }
            }
        }
        SCCINFO(SCMOD) << "SEQ file name of replay target = " << replay_file_name.get_value();
    }
}

void replay_buffer::transport(tlm::tlm_generic_payload& trans, bool lt_transport) {
    auto id = axi::get_axi_id(trans);
    auto addr = trans.get_address();
    auto cycle = clk_if ? sc_core::sc_time_stamp() / clk_if->period() : 0;
    if(trans.is_write()) {
        if(id < wr_sequence.size() && wr_sequence[id].size()) {
            auto it = std::find_if(std::begin(wr_sequence[id]), std::end(wr_sequence[id]),
                                   [addr](entry_t const& e) { return std::get<0>(e) == addr; });
            if(it != std::end(wr_sequence[id])) {
                wr_req2resp_fifo.push_back(std::make_tuple(&trans, std::get<1>(*it)));
                wr_sequence[id].erase(it);
                return;
            }
        }
        if(replay_file_name.get_value().length()) {
            SCCWARN(SCMOD) << "No transaction in write sequence buffer for " << trans;
        }
        wr_req2resp_fifo.push_back(std::make_tuple(&trans, 0));
    } else if(trans.is_read()) {
        if(id < rd_sequence.size() && rd_sequence[id].size()) {
            auto it = std::find_if(std::begin(rd_sequence[id]), std::end(rd_sequence[id]),
                                   [addr](entry_t const& e) { return std::get<0>(e) == addr; });
            if(it != std::end(rd_sequence[id])) {
                rd_req2resp_fifo.push_back(std::make_tuple(&trans, std::get<1>(*it)));
                rd_sequence[id].erase(it);
                return;
            }
        }
        if(replay_file_name.get_value().length()) {
            SCCWARN(SCMOD) << "No transaction in read sequence buffer for " << trans;
        }
        rd_req2resp_fifo.push_back(std::make_tuple(&trans, 0));
    }
}

void replay_buffer::process_req2resp_fifos() {
    while(rd_req2resp_fifo.avail()) {
        auto& entry = rd_req2resp_fifo.front();
        if(std::get<1>(entry) == 0) {
            rd_resp_fifo.push_back(std::get<0>(entry));
        } else {
            std::get<1>(entry) -= 1;
            rd_req2resp_fifo.push_back(entry);
        }
        rd_req2resp_fifo.pop_front();
    }
    while(wr_req2resp_fifo.avail()) {
        auto& entry = wr_req2resp_fifo.front();
        if(std::get<1>(entry) == 0) {
            wr_resp_fifo.push_back(std::get<0>(entry));
        } else {
            std::get<1>(entry) -= 1;
            wr_req2resp_fifo.push_back(entry);
        }
        wr_req2resp_fifo.pop_front();
    }
}

void replay_buffer::start_rd_resp_thread() {
    auto residual_clocks = 0.0;
    while(true) {
        wait(rd_resp_fifo.data_written_event());
        while(rd_resp_fifo.avail())
            if(auto* trans = rd_resp_fifo.front()) {
                if(clk_if) {
                    if(time_per_byte_total.value()) {
                        scc::ordered_semaphore::lock lck(total_arb);
                        auto clocks = trans->get_data_length() * time_per_byte_total / clk_if->period() + total_residual_clocks;
                        auto delay = static_cast<unsigned>(clocks);
                        total_residual_clocks = clocks - delay;
                        wait(delay * clk_if->period());
                        wait(clk_i.posedge_event());
                    } else if(time_per_byte_rd.value()) {
                        auto clocks = trans->get_data_length() * time_per_byte_rd / clk_if->period() + residual_clocks;
                        auto delay = static_cast<unsigned>(clocks);
                        residual_clocks = clocks - delay;
                        wait(delay * clk_if->period());
                        wait(clk_i.posedge_event());
                    }
                }
                while(bw_o->transport(*trans) != 0)
                    wait(clk_i.posedge_event());
                rd_resp_fifo.pop_front();
            }
    }
}

void replay_buffer::start_wr_resp_thread() {
    auto residual_clocks = 0.0;
    while(true) {
        wait(wr_resp_fifo.data_written_event());
        while(wr_resp_fifo.avail())
            if(auto* trans = wr_resp_fifo.front()) {
                if(clk_if) {
                    if(time_per_byte_total.value()) {
                        scc::ordered_semaphore::lock lck(total_arb);
                        auto clocks = trans->get_data_length() * time_per_byte_total / clk_if->period() + total_residual_clocks;
                        auto delay = static_cast<unsigned>(clocks);
                        total_residual_clocks = clocks - delay;
                        wait(delay * clk_if->period());
                        wait(clk_i.posedge_event());
                    } else if(time_per_byte_rd.value()) {
                        auto clocks = trans->get_data_length() * time_per_byte_rd / clk_if->period() + residual_clocks;
                        auto delay = static_cast<unsigned>(clocks);
                        residual_clocks = clocks - delay;
                        wait(delay * clk_if->period());
                        wait(clk_i.posedge_event());
                    }
                }
                while(bw_o->transport(*trans) != 0)
                    wait(clk_i.posedge_event());
                wr_resp_fifo.pop_front();
            }
    }
}
} // namespace pe
} // namespace axi
