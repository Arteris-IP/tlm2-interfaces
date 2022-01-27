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

#pragma once

#include "ace_recorder.h"
#include <axi/axi_tlm.h>

namespace axi {
namespace scv {

template <unsigned int BUSWIDTH = 32, typename TYPES = axi::axi_protocol_types, int N = 1,
          sc_core::sc_port_policy POL = sc_core::SC_ONE_OR_MORE_BOUND>
class ace_rec_initiator_socket : public axi::ace_initiator_socket<BUSWIDTH, TYPES, N, POL> {
    static std::string gen_name(const char* first, const char* second) {
        std::stringstream ss;
        ss << first << "_" << second;
        return ss.str();
    }

public:
    using fw_interface_type = axi::ace_fw_transport_if<TYPES>;
    using bw_interface_type = axi::ace_bw_transport_if<TYPES>;
    using port_type = sc_core::sc_port<fw_interface_type, N, POL>;
    using export_type = sc_core::sc_export<bw_interface_type>;
    using base_target_socket_type = tlm::tlm_base_target_socket_b<BUSWIDTH, fw_interface_type, bw_interface_type>;
    using base_type = tlm::tlm_base_initiator_socket_b<BUSWIDTH, fw_interface_type, bw_interface_type>;

    ace_rec_initiator_socket()
    : axi::ace_initiator_socket<BUSWIDTH, TYPES, N, POL>()
    , recorder(gen_name(this->name(), "tx").c_str()) {
        this->add_attribute(recorder.enableTracing);
        this->add_attribute(recorder.enableTimed);
    }

    explicit ace_rec_initiator_socket(const char* name)
    : axi::ace_initiator_socket<BUSWIDTH, TYPES, N, POL>(name)
    , recorder(gen_name(this->name(), "tx").c_str()) {}

    virtual ~ace_rec_initiator_socket() {}

    virtual const char* kind() const { return "axi_rec_initiator_socket"; }
    //
    // Bind initiator socket to target socket
    // - Binds the port of the initiator socket to the export of the target
    //   socket
    // - Binds the port of the target socket to the export of the initiator
    //   socket
    //
    virtual void bind(base_target_socket_type& s) {
        // initiator.port -> target.export
        (this->get_base_port())(recorder);
        recorder.fw_port(s.get_base_interface());
        // target.port -> initiator.export
        (s.get_base_port())(recorder);
        recorder.bw_port(this->get_base_interface());
    }
    //
    // Bind initiator socket to initiator socket (hierarchical bind)
    // - Binds both the export and the port
    //
    virtual void bind(base_type& s) {
        // port
        (this->get_base_port())(recorder);
        recorder.fw_port(s.get_base_port());
        // export
        (s.get_base_export())(recorder);
        recorder.bw_port(this->get_base_export());
    }

    //
    // Bind interface to socket
    // - Binds the interface to the export of this socket
    //
    virtual void bind(bw_interface_type& ifs) { (this->get_base_export())(ifs); }

    void setExtensionRecording(tlm::scc::scv::tlm_extensions_recording_if<TYPES>* extensionRecording) {
        recorder.setExtensionRecording(extensionRecording);
    }

protected:
    axi::scv::ace_recorder<TYPES> recorder;
};
} // namespace scv
} // namespace axi
