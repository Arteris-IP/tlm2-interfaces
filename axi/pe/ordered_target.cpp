#include "ordered_target.h"
namespace axi {
namespace pe {

rate_limiting_buffer::rate_limiting_buffer(const sc_core::sc_module_name &nm) {
    fw_i.bind(*this);
    add_attribute(rd_bw_limit_byte_per_sec);
    add_attribute(wr_bw_limit_byte_per_sec);
    add_attribute(rd_resp_delay);
    add_attribute(wr_resp_delay);
    SC_HAS_PROCESS(rate_limiting_buffer);
    SC_METHOD(process_req2resp_fifos);
    dont_initialize();
    sensitive << clk_i.pos();
    SC_THREAD(start_wr_resp_thread);
    SC_THREAD(start_rd_resp_thread);
}

void rate_limiting_buffer::end_of_elaboration() {
    clk_if = dynamic_cast<sc_core::sc_clock*>(clk_i.get_interface());
    if(clk_if) {
        if(rd_bw_limit_byte_per_sec.value > 0.0) {
            time_per_byte_rd = sc_core::sc_time(1.0 / rd_bw_limit_byte_per_sec.value, sc_core::SC_SEC);
        }
        if(wr_bw_limit_byte_per_sec.value > 0.0) {
            time_per_byte_wr = sc_core::sc_time(1.0 / wr_bw_limit_byte_per_sec.value, sc_core::SC_SEC);
        }
    }
}

void rate_limiting_buffer::transport(tlm::tlm_generic_payload &trans, bool lt_transport) {
    if(trans.is_write())
        wr_req2resp_fifo.push_back(std::make_tuple(&trans, wr_resp_delay.value));
    else if(trans.is_read())
        rd_req2resp_fifo.push_back(std::make_tuple(&trans, rd_resp_delay.value));
}

void rate_limiting_buffer::process_req2resp_fifos() {
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

void rate_limiting_buffer::start_rd_resp_thread() {
    auto residual_clocks = 0.0;
    while(true) {
        wait(rd_resp_fifo.data_written_event());
        while(rd_resp_fifo.avail())
            if(auto* trans = rd_resp_fifo.front()){
                if(clk_if && time_per_byte_rd.value()) {
                    auto clocks = trans->get_data_length() * time_per_byte_rd / clk_if->period() + residual_clocks;
                    auto delay = static_cast<unsigned>(clocks);
                    residual_clocks = clocks - delay;
                    while(delay) {
                        wait(clk_i.posedge_event());
                        delay--;
                    }
                }
                while(bw_o->transport(*trans)!=0) wait(clk_i.posedge_event());
                rd_resp_fifo.pop_front();
            }
    }
}

void rate_limiting_buffer::start_wr_resp_thread() {
    auto residual_clocks = 0.0;
    while(true) {
        wait(wr_resp_fifo.data_written_event());
        while(wr_resp_fifo.avail())
            if(auto* trans = wr_resp_fifo.front()){
                if(clk_if && time_per_byte_rd.value()) {
                    auto clocks = trans->get_data_length() * time_per_byte_rd / clk_if->period() + residual_clocks;
                    auto delay = static_cast<unsigned>(clocks);
                    residual_clocks = clocks - delay;
                    while(delay) {
                        wait(clk_i.posedge_event());
                        delay--;
                    }
                }
                while(bw_o->transport(*trans)!=0) wait(clk_i.posedge_event());
                wr_resp_fifo.pop_front();
            }
    }
}

}
}
