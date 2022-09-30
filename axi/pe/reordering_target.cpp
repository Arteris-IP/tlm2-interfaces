/*
 * Copyright 2020-2022 Arteris IP
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

#include "reordering_target.h"

axi::pe::tx_reorderer::tx_reorderer(const sc_core::sc_module_name &nm): sc_core::sc_module(nm) {
    add_attribute(min_latency);
    add_attribute(max_latency);
    add_attribute(prioritize_by_latency);
    add_attribute(prioritize_by_qos);
    SC_HAS_PROCESS(tx_reorderer);
    fw_i(*this);
    SC_METHOD(clock_cb);
    sensitive<<clk_i.pos();
    dont_initialize();
}

void axi::pe::tx_reorderer::transport(tlm::tlm_generic_payload &payload, bool lt_transport) {
    if(auto ext = payload.get_extension<axi::axi4_extension>()) {
        auto id = ext->get_id();
        reorder_buffer[payload.get_command()][id].emplace_back(payload);
    } else
        SCCFATAL(SCMOD)<<"Transaction is not a AXI4 transaction";
}

void axi::pe::tx_reorderer::clock_cb() {
    for(unsigned cmd=tlm::TLM_READ_COMMAND; cmd<tlm::TLM_IGNORE_COMMAND; ++cmd) {
        if(!reorder_buffer[cmd].empty()) {
            std::vector<unsigned> r1{}, r2{};
            r1.reserve(reorder_buffer[cmd].size());
            r2.reserve(reorder_buffer[cmd].size());
            auto lat_sum = 0U;
            auto max_qos=0U;
            for(auto& e:reorder_buffer[cmd]) {
                if(e.second.size()){
                    e.second.front().age++;
                    if(e.second.front().age>max_latency.value) {
                        r1.push_back(e.first);
                    } else if(e.second.front().age>min_latency.value) {
                        r2.push_back(e.first);
                        lat_sum+=e.second.front().age;
                        max_qos=std::max<unsigned>(max_qos, e.second.front().trans->get_extension<axi::axi4_extension>()->get_qos());
                    }
                }
            }
            if(r1.size()) {
                //std::random_shuffle(r1.begin(), r1.end(), [](unsigned l) -> unsigned { return scc::MT19937::uniform(0, l);});
                for (auto i = r1.begin() + 1; i != r1.end(); ++i) {
                    auto j = r1.begin() + scc::MT19937::uniform(0, i - r1.begin());
                    if (i != j) std::iter_swap(i, j);
                }

                for(auto& e:r1) {
                    auto& deq = reorder_buffer[cmd][e];
                    auto notok = bw_o->transport(*deq.front().trans);
                    if(notok==0) deq.pop_front();
                }
            } else if(r2.size()>window_size.value) {
                if(prioritize_by_qos.value) {
                    std::vector<unsigned> res;
                    auto& buf = reorder_buffer[cmd];
                    std::copy_if(std::begin(r2), std::end(r2), std::back_inserter(res), [max_qos, &buf](unsigned e){
                        return buf[e].front().trans->get_extension<axi::axi4_extension>()->get_qos() >= max_qos;
                    });
                    if(res.size()==1){
                        auto& deq = reorder_buffer[cmd][res.front()];
                        auto notok = bw_o->transport(*deq.front().trans);
                        if(notok==0) deq.pop_front();
                        return;
                    } else {
                        r2=res;
                    }
                }
                if(prioritize_by_latency.value){
                    auto rnd = scc::MT19937::uniform(0, lat_sum);
                    auto part_sum = 0U;
                    for(auto& e:r2){
                        auto& deq = reorder_buffer[cmd][e];
                        part_sum+=deq.front().age;
                        if(part_sum>=rnd) {
                            auto notok = bw_o->transport(*deq.front().trans);
                            if(notok==0) deq.pop_front();
                            return;
                        }
                    }
                } else {
                    auto rnd = scc::MT19937::uniform(0, r2.size()-1);
                    auto& deq = reorder_buffer[cmd][r2[rnd]];
                    auto notok = bw_o->transport(*deq.front().trans);
                    if(notok==0) deq.pop_front();
                }
            }
        }
    }
}

