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
 * limitations under the License.
 */

#pragma once

#include <chi/scv/chi_recorder.h>

namespace chi {
namespace scv {
/*! \brief The TLM2 transaction recorder
 *
 * This module records all TLM transaction to a SCV transaction stream for
 * further viewing and analysis.
 * The handle of the created transaction is storee in an tlm_extension so that
 * another instance of the scv_tlm2_recorder
 * e.g. further down the opath can link to it.
 * The transaction recorder is simply bound between an existing pair of
 * initiator and target sockets
 */
template <unsigned int BUSWIDTH, typename TYPES, typename BASE>
class chitlm_recorder_module : public sc_core::sc_module, public BASE {
public:
    //! The target socket of the recorder to be bound to the initiator
    typename BASE::template target_socket_type<BUSWIDTH> tsckt{"tsckt"};
    //! The initiator to be bound to the target socket
    typename BASE::template initiator_socket_type<BUSWIDTH> isckt{"isckt"};

    /*! \brief The constructor of the component
     *
     * \param name is the SystemC module name of the recorder
     * \param tr_db is a pointer to a transaction recording database. If none is
     * provided the default one is retrieved.
     *        If this database is not initialized (e.g. by not calling
     * scv_tr_db::set_default_db() ) recording is disabled.
     */
    chitlm_recorder_module(sc_core::sc_module_name name, bool recording_enabled = true,
                           SCVNS scv_tr_db* tr_db = SCVNS scv_tr_db::get_default_db())
    : sc_module(name)
    , BASE(sc_core::sc_object::name(), recording_enabled, tr_db) {
        // bind the sockets to the module
        tsckt.bind(*this);
        isckt.bind(*this);
        add_attribute(BASE::enableBlTracing);
        add_attribute(BASE::enableNbTracing);
        add_attribute(BASE::enableTimedTracing);
    }

    virtual ~chitlm_recorder_module() {}

    tlm::tlm_fw_transport_if<TYPES>* get_fw_if() { return isckt.get_base_port().operator->(); }

    tlm::tlm_bw_transport_if<TYPES>* get_bw_if() { return tsckt.get_base_port().operator->(); }

private:
    void start_of_simulation() override { BASE::initialize_streams(); }
};

template <unsigned int BUSWIDTH = 32>
using chi_trx_recorder_module =
    chitlm_recorder_module<BUSWIDTH, chi::chi_protocol_types, chi_trx_recorder<chi::chi_protocol_types>>;

} // namespace scv
} // namespace chi
